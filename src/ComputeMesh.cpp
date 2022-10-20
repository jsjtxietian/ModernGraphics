#if 0

// run a compute shader to create triangulated geometry of a
// three-dimensional (3D) torus knot shape with different P and Q parameters.

#include <imgui/imgui.h>

#include "Framework/VulkanApp.h"
#include "Vulkan/VulkanClear.h"
#include "Vulkan/ComputedVB.h"
#include "Vulkan/ComputedImage.h"
#include "Vulkan/VulkanFinish.h"
#include "Vulkan/ModelRenderer.h"
#include "Vulkan/VulkanImGui.h"

struct MouseState
{
    glm::vec2 pos = glm::vec2(0.0f);
    bool pressedLeft = false;
} mouseState;

GLFWwindow *window;

const uint32_t kScreenWidth = 1600;
const uint32_t kScreenHeight = 900;

// We store a queue of P-Q pairs that defines the order of morphing. The queue always
// has at least two elements that define the current and the next torus knot. We also
// store a morphCoef floating-point value that is the 0...1 morphing factor between
// these two pairs in the queue. The mesh is regenerated every frame and the morphing
// coefficient is increased until it reaches 1.0. At this point, we will either stop
// morphing or, in case there are more than two elements in the queue, remove the top
// element from it, reset morphCoef back to 0, and repeat. The animationSpeed
// value defines how fast one torus knot mesh morphs into another:

/// should contain at least two elements
std::deque<std::pair<uint32_t, uint32_t>> morphQueue = {{5, 8}, {5, 8}};

/// morphing between two torus knots 0..1
float morphCoef = 0.0f;
float animationSpeed = 1.0f;

bool useColoredMesh = false;

VulkanInstance vk;
VulkanRenderDevice vkDev;

std::unique_ptr<VulkanClear> clear;
std::unique_ptr<ComputedVertexBuffer> meshGen;
std::unique_ptr<ModelRenderer> mesh;
std::unique_ptr<ModelRenderer> meshColor;
std::unique_ptr<ComputedImage> imgGen;
std::unique_ptr<ImGuiRenderer> imgui;
std::unique_ptr<VulkanFinish> finish;

// Two global constants define the tessellation level of a torus knot.
const uint32_t numU = 1024;
const uint32_t numV = 1024;

// Note two sets of P and Q parameters here:
struct MeshUniformBuffer
{
    float time;
    uint32_t numU;
    uint32_t numV;
    float minU, maxU;
    float minV, maxV;
    uint32_t p1, p2;
    uint32_t q1, q2;
    float morph;
} ubo;

// Regardless of the P and Q parameter values, we have a single order in
// which we should traverse vertices to produce torus knot triangles. The
// generateIndices() function prepares index buffer data for this purpose:
void generateIndices(uint32_t *indices)
{
    for (uint32_t j = 0; j < numV - 1; j++)
    {
        for (uint32_t i = 0; i < numU - 1; i++)
        {
            uint32_t ofs = (j * (numU - 1) + i) * 6;

            uint32_t i1 = (j + 0) * numU + (i + 0);
            uint32_t i2 = (j + 0) * numU + (i + 1);
            uint32_t i3 = (j + 1) * numU + (i + 1);
            uint32_t i4 = (j + 1) * numU + (i + 0);

            indices[ofs + 0] = i1;
            indices[ofs + 1] = i2;
            indices[ofs + 2] = i4;

            indices[ofs + 3] = i2;
            indices[ofs + 4] = i3;
            indices[ofs + 5] = i4;
        }
    }
}

// This allocates all the necessary buffers, uploads indices data into the GPU, loads compute shaders for
// texture and mesh generation, and creates two model renderers, one for a textured mesh
// and another for a colored one.
void initMesh()
{
    // allocate storage for our generated indices data. To make things
    // simpler, we do not use triangle strips, so it is always 6 indices for each quad defined
    // by the UV mapping
    std::vector<uint32_t> indicesGen((numU - 1) * (numV - 1) * 6);

    generateIndices(indicesGen.data());

    // Compute all the necessary sizes for our GPU buffer. 12 floats are necessary to store
    // three vec4 components per vertex. The actual data structure is defined only in
    // GLSL and can be found in data/shaders/mesh_common.inc:
    uint32_t vertexBufferSize = 12 * sizeof(float) * numU * numV;
    uint32_t indexBufferSize = sizeof(uint32_t) * (numU - 1) * (numV - 1) * 6;
    uint32_t bufferSize = vertexBufferSize + indexBufferSize;

    // Load both compute shaders. The grid size for texture generation is fixed at
    // 1024x1024. The grid size for the mesh can be tweaked using numU and numV:
    imgGen = std::make_unique<ComputedImage>(vkDev, "data/shaders/compute_texture.comp", 1024, 1024, false);
    meshGen = std::make_unique<ComputedVertexBuffer>(vkDev, "data/shaders/compute_mesh.comp",
                                                     indexBufferSize, sizeof(MeshUniformBuffer), 12 * sizeof(float), numU * numV);

    // Use a staging buffer to upload indices data into the GPU memory:
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(vkDev.device, vkDev.physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    {
        void *data;
        vkMapMemory(vkDev.device, stagingBufferMemory, 0, bufferSize, 0, &data);
        // pregenerated index data
        memcpy((void *)((uint8_t *)data + vertexBufferSize), indicesGen.data(), indexBufferSize);
        vkUnmapMemory(vkDev.device, stagingBufferMemory);
    }

    copyBuffer(vkDev, stagingBuffer, meshGen->computedBuffer, bufferSize);

    vkDestroyBuffer(vkDev.device, stagingBuffer, nullptr);
    vkFreeMemory(vkDev.device, stagingBufferMemory, nullptr);

    meshGen->fillComputeCommandBuffer();
    meshGen->submit();

    // essentially making these two processes
    // completely serial and inefficient. While this is acceptable for the purpose of showing
    // a single feature in a standalone demo app,
    vkDeviceWaitIdle(vkDev.device);

    std::vector<const char *> shaders = {"data/shaders/VK04_render.vert", "data/shaders/VK04_render.frag"};
    std::vector<const char *> shadersColor = {"data/shaders/VK04_render.vert", "data/shaders/VK04_render_color.frag"};

    // The first one will draw the generated mesh geometry textured with an image
    // generated by a compute shader.
    mesh = std::make_unique<ModelRenderer>(
        vkDev, true,
        meshGen->computedBuffer, meshGen->computedMemory,
        vertexBufferSize, indexBufferSize,
        imgGen->computed, imgGen->computedImageSampler,
        shaders, (uint32_t)sizeof(mat4),
        true);

    // apply only a solid color with some simple lighting.
    meshColor = std::make_unique<ModelRenderer>(
        vkDev, true,
        meshGen->computedBuffer, meshGen->computedMemory,
        vertexBufferSize, indexBufferSize,
        imgGen->computed, imgGen->computedImageSampler,
        shadersColor, (uint32_t)sizeof(mat4),
        true, mesh->getDepthTexture(), false);
}

void renderGUI(uint32_t imageIndex)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)width, (float)height);
    ImGui::NewFrame();

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBackground;

    // Each torus knot is specified by a pair of coprime integers p and q.
    // https://en.wikipedia.org/wiki/Torus_knot
    static const std::vector<std::pair<uint32_t, uint32_t>> PQ = {
        {2, 3}, {2, 5}, {2, 7}, {3, 4}, {2, 9}, {3, 5}, {5, 8}};

    ImGui::Begin("Torus Knot params", nullptr);
    {
        ImGui::Checkbox("Use colored mesh", &useColoredMesh);
        ImGui::SliderFloat("Animation speed", &animationSpeed, 0.0f, 2.0f);

        for (size_t i = 0; i != PQ.size(); i++)
        {
            std::string title = std::to_string(PQ[i].first) + ", " + std::to_string(PQ[i].second);
            if (ImGui::Button(title.c_str()))
            {
                if (PQ[i] != morphQueue.back())
                    morphQueue.push_back(PQ[i]);
            }
        }
    }
    ImGui::End();
    ImGui::Render();

    imgui->updateBuffers(vkDev, imageIndex, ImGui::GetDrawData());
}

void updateBuffers(uint32_t imageIndex)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    const float ratio = width / (float)height;

    const mat4 m1 = glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, -18.f));
    const mat4 p = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);
    const mat4 mtx = p * m1;

    if (useColoredMesh)
        meshColor->updateUniformBuffer(vkDev, imageIndex, glm::value_ptr(mtx), sizeof(mat4));
    else
        mesh->updateUniformBuffer(vkDev, imageIndex, glm::value_ptr(mtx), sizeof(mat4));

    renderGUI(imageIndex);
}

void composeFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    clear->fillCommandBuffer(commandBuffer, imageIndex);

    if (useColoredMesh)
        meshColor->fillCommandBuffer(commandBuffer, imageIndex);
    else
        mesh->fillCommandBuffer(commandBuffer, imageIndex);

    imgui->fillCommandBuffer(commandBuffer, imageIndex);
    finish->fillCommandBuffer(commandBuffer, imageIndex);
}

float easing(float x)
{
    return (x < 0.5)
               ? (4 * x * x * (3 * x - 1))
               : (4 * (x - 1) * (x - 1) * (3 * (x - 1) + 1) + 1);
}

int main()
{
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();

    window = initVulkanApp(kScreenWidth, kScreenHeight);

    glfwSetCursorPosCallback(
        window,
        [](auto *window, double x, double y)
        {
            ImGui::GetIO().MousePos = ImVec2((float)x, (float)y);
        });

    glfwSetMouseButtonCallback(
        window,
        [](auto *window, int button, int action, int mods)
        {
            auto &io = ImGui::GetIO();
            const int idx = button == GLFW_MOUSE_BUTTON_LEFT ? 0 : button == GLFW_MOUSE_BUTTON_RIGHT ? 2
                                                                                                     : 1;
            io.MouseDown[idx] = action == GLFW_PRESS;

            if (button == GLFW_MOUSE_BUTTON_LEFT)
                mouseState.pressedLeft = action == GLFW_PRESS;
        });

    glfwSetKeyCallback(
        window,
        [](GLFWwindow *window, int key, int scancode, int action, int mods)
        {
            const bool pressed = action != GLFW_RELEASE;
            if (key == GLFW_KEY_ESCAPE && pressed)
                glfwSetWindowShouldClose(window, GLFW_TRUE);
        });

    createInstance(&vk.instance);

    BL_CHECK(setupDebugCallbacks(vk.instance, &vk.messenger, &vk.reportCallback));
    VK_CHECK(glfwCreateWindowSurface(vk.instance, window, nullptr, &vk.surface));
    BL_CHECK(initVulkanRenderDeviceWithCompute(vk, vkDev, kScreenWidth, kScreenHeight, VkPhysicalDeviceFeatures{}));

    initMesh();

    clear = std::make_unique<VulkanClear>(vkDev, mesh->getDepthTexture());
    finish = std::make_unique<VulkanFinish>(vkDev, mesh->getDepthTexture());
    imgui = std::make_unique<ImGuiRenderer>(vkDev);

    double lastTime = glfwGetTime();

    do
    {
        auto iter = morphQueue.begin();

        ubo.time = (float)glfwGetTime();
        ubo.morph = easing(morphCoef);
        ubo.p1 = iter->first;
        ubo.q1 = iter->second;
        ubo.p2 = (iter + 1)->first;
        ubo.q2 = (iter + 1)->second;

        ubo.numU = numU;
        ubo.numV = numV;
        ubo.minU = -1.0f;
        ubo.maxU = +1.0f;
        ubo.minV = -1.0f;
        ubo.maxV = +1.0f;

        meshGen->uploadUniformBuffer(sizeof(MeshUniformBuffer), &ubo);

        meshGen->fillComputeCommandBuffer(nullptr, 0, meshGen->computedVertexCount / 2, 1, 1);
        meshGen->submit();
        vkDeviceWaitIdle(vkDev.device);

        imgGen->fillComputeCommandBuffer(&ubo.time, sizeof(float), imgGen->computedWidth / 16, imgGen->computedHeight / 16, 1);
        imgGen->submit();
        vkDeviceWaitIdle(vkDev.device);

        drawFrame(vkDev, updateBuffers, composeFrame);

        const double newTime = glfwGetTime();
        const float deltaSeconds = static_cast<float>(newTime - lastTime);
        lastTime = newTime;
        morphCoef += animationSpeed * deltaSeconds;
        if (morphCoef > 1.0)
        {
            morphCoef = 1.0f;
            if (morphQueue.size() > 2)
            {
                morphCoef = 0.0f;
                morphQueue.pop_front();
            }
        }

        glfwPollEvents();
    } while (!glfwWindowShouldClose(window));

    clear = nullptr;
    finish = nullptr;

    imgui = nullptr;
    meshGen = nullptr;
    imgGen = nullptr;
    mesh = nullptr;
    meshColor->freeTextureSampler(); // release sampler handle
    meshColor = nullptr;

    destroyVulkanRenderDevice(vkDev);
    destroyVulkanInstance(vk);

    ImGui::DestroyContext();

    glfwTerminate();
    glslang_finalize_process();

    return 0;
}

#endif
