#pragma once

#define VK_NO_PROTOTYPES
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <limits>

#include <imgui/imgui.h>

#include "Scene/Camera.h"
#include "Utils/Utils.h"
#include "Utils/UtilsMath.h"
#include "Utils/UtilsFPS.h"
#include "Vulkan/UtilsVulkan.h"

#include "VulkanResources.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

struct Resolution
{
    uint32_t width = 0;
    uint32_t height = 0;
};

GLFWwindow *initVulkanApp(int width, int height, Resolution *resolution = nullptr);
bool drawFrame(VulkanRenderDevice &vkDev, const std::function<void(uint32_t)> &updateBuffersFunc, const std::function<void(VkCommandBuffer, uint32_t)> &composeFrameFunc);

struct Renderer;

// Since individual Renderer class creation is rather expensive, we must define a
// wrapper structure that takes a reference to the Renderer class. Thanks to C++11's
// move semantics, an std::vector of RenderItem instances can be filled with
// emplace_back(), without it triggering copy constructors or reinitialization:
struct RenderItem
{
    Renderer &renderer_;
    bool enabled_ = true;
    bool useDepth_ = true;
    explicit RenderItem(Renderer &r, bool useDepth = true)
        : renderer_(r), useDepth_(useDepth)
    {
    }
};

// The VulkanRenderContext class holds all basic Vulkan objects (instance and device),
// along with a list of on-screen renderers.
// This class will be used later in VulkanApp to compose a frame.
// VulkanContextCreator helps initialize both the instance and logical Vulkan device.
// The resource management system is also initialized here:
struct VulkanRenderContext
{
    VulkanInstance vk;
    VulkanRenderDevice vkDev;
    VulkanContextCreator ctxCreator;
    VulkanResources resources;

    VulkanRenderContext(void *window, uint32_t screenWidth, uint32_t screenHeight,
                        const VulkanContextFeatures &ctxFeatures = VulkanContextFeatures())
        : ctxCreator(vk, vkDev, window, screenWidth, screenHeight, ctxFeatures),
          resources(vkDev),

          depthTexture(resources.addDepthTexture(vkDev.framebufferWidth, vkDev.framebufferHeight, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)),

          screenRenderPass(resources.addFullScreenPass()),
          screenRenderPass_NoDepth(resources.addFullScreenPass(false)),

          finalRenderPass(resources.addFullScreenPass(true, RenderPassCreateInfo{.clearColor_ = false, .clearDepth_ = false, .flags_ = eRenderPassBit_Last})),
          clearRenderPass(resources.addFullScreenPass(true, RenderPassCreateInfo{.clearColor_ = true, .clearDepth_ = true, .flags_ = eRenderPassBit_First})),

          swapchainFramebuffers(resources.addFramebuffers(screenRenderPass.handle, depthTexture.image.imageView)),
          swapchainFramebuffers_NoDepth(resources.addFramebuffers(screenRenderPass_NoDepth.handle))
    {
    }

    // iterates over all the enabled renderers and updates their internal buffers:
    void updateBuffers(uint32_t imageIndex);
    void composeFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    // For Chapter 8 & 9
    inline PipelineInfo pipelineParametersForOutputs(const std::vector<VulkanTexture> &outputs) const
    {
        return PipelineInfo{
            .width = outputs.empty() ? vkDev.framebufferWidth : outputs[0].width,
            .height = outputs.empty() ? vkDev.framebufferHeight : outputs[0].height,
            .useBlending = false};
    }

    // this class contains a list of on-screen renderers, declared as a dynamic
    // array. Along with composite subsystems, a list of renderpass and framebuffer
    // handles are declared for use in Renderer instances. All framebuffers share a single
    // depth buffer. The render passes for on-screen rendering are also declared here:
    std::vector<RenderItem> onScreenRenderers_;

    VulkanTexture depthTexture;

    // Framebuffers and renderpass for on-screen rendering
    RenderPass screenRenderPass;
    RenderPass screenRenderPass_NoDepth;

    // Two special render passes for clearing and finalizing the frame are used,
    // just as VulkanClear and VulkanFinish classes.
    RenderPass clearRenderPass;
    RenderPass finalRenderPass;

    std::vector<VkFramebuffer> swapchainFramebuffers;
    std::vector<VkFramebuffer> swapchainFramebuffers_NoDepth;

    // All the renderers in our framework use custom rendering passes. Starting a new
    // rendering pass can be implemented with the following routine
    void beginRenderPass(VkCommandBuffer cmdBuffer, VkRenderPass pass, size_t currentImage, const VkRect2D area,
                         VkFramebuffer fb = VK_NULL_HANDLE,
                         uint32_t clearValueCount = 0, const VkClearValue *clearValues = nullptr)
    {
        // If an external framebuffer is unspecified, we use our local full screen framebuffer.
        // Optional clearing values are also passed as parameters
        const VkRenderPassBeginInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = pass,
            .framebuffer = (fb != VK_NULL_HANDLE) ? fb : swapchainFramebuffers[currentImage],
            .renderArea = area,
            .clearValueCount = clearValueCount,
            .pClearValues = clearValues};

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
};

struct VulkanApp
{
    VulkanApp(int screenWidth, int screenHeight, const VulkanContextFeatures &ctxFeatures = VulkanContextFeatures())
        : window_(initVulkanApp(screenWidth, screenHeight, &resolution_)),
          ctx_(window_, resolution_.width, resolution_.height, ctxFeatures),
          onScreenRenderers_(ctx_.onScreenRenderers_)
    {
        glfwSetWindowUserPointer(window_, this);
        assignCallbacks();
    }

    ~VulkanApp()
    {
        glslang_finalize_process();
        glfwTerminate();
    }

    virtual void drawUI() {}
    virtual void draw3D() = 0;

    void mainLoop();

    // Check if none of the ImGui widgets were touched so our app can process mouse events
    inline bool shouldHandleMouse() const { return !ImGui::GetIO().WantCaptureMouse; }

    virtual void handleKey(int key, bool pressed) = 0;
    virtual void handleMouseClick(int button, bool pressed)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT)
            mouseState_.pressedLeft = pressed;
    }

    virtual void handleMouseMove(float mx, float my)
    {
        mouseState_.pos.x = mx;
        mouseState_.pos.y = my;
    }

    //  The update() routine performs whatever actions necessary to calculate the new application state:
    virtual void update(float deltaSeconds) = 0;

    inline float getFPS() const { return fpsCounter_.getFPS(); }

protected:
    struct MouseState
    {
        glm::vec2 pos = glm::vec2(0.0f);
        bool pressedLeft = false;
    } mouseState_;

    Resolution resolution_;
    GLFWwindow *window_ = nullptr;

    VulkanRenderContext ctx_;
    std::vector<RenderItem> &onScreenRenderers_;
    FramesPerSecondCounter fpsCounter_;

private:
    void assignCallbacks();

    void updateBuffers(uint32_t imageIndex);
};

struct CameraApp : public VulkanApp
{
    CameraApp(int screenWidth, int screenHeight,
              const VulkanContextFeatures &ctxFeatures = VulkanContextFeatures())
        : VulkanApp(screenWidth, screenHeight, ctxFeatures),

          positioner(glm::vec3(0.0f, 5.0f, 10.0f), vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, -1.0f, 0.0f)),
          camera(positioner)
    {
    }

    // sends mouse event parameters to the 3D camera positioner:
    virtual void update(float deltaSeconds) override
    {
        positioner.update(deltaSeconds, mouseState_.pos, shouldHandleMouse() ? mouseState_.pressedLeft : false);
    }

    // The default camera projection calculator uses the screen aspect ratio
    glm::mat4 getDefaultProjection() const
    {
        const float ratio = ctx_.vkDev.framebufferWidth / (float)ctx_.vkDev.framebufferHeight;
        return glm::perspective(45.0f, ratio, 0.1f, 1000.0f);
    }

    virtual void handleKey(int key, bool pressed) override;

protected:
    CameraPositioner_FirstPerson positioner;
    Camera camera;
};
