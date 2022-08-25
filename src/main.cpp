#define VK_NO_PROTOTYPES
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>

#include "Utils/Utils.h"
#include "Vulkan/UtilsVulkan.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>
using glm::mat4;
using glm::vec3;
using glm::vec4;

const uint32_t kScreenWidth = 1280;
const uint32_t kScreenHeight = 720;

GLFWwindow *window;

struct UniformBuffer
{
    mat4 mvp;
};

static constexpr VkClearColorValue clearValueColor = {1.0f, 1.0f, 1.0f, 1.0f};

size_t vertexBufferSize;
size_t indexBufferSize;

VulkanInstance vk;
VulkanRenderDevice vkDev;

struct VulkanState
{
    // 1. Descriptor set (layout + pool + sets) -> uses uniform buffers, textures, framebuffers
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    // 2.
    std::vector<VkFramebuffer> swapchainFramebuffers;

    // 3. Pipeline & render pass (using DescriptorSets & pipeline state options)
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    // 4. Uniform buffer
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    // 5. Storage Buffer with index and vertex data
    VkBuffer storageBuffer;
    VkDeviceMemory storageBufferMemory;

    // 6. Depth buffer
    VulkanImage depthTexture;

    VkSampler textureSampler;
    VulkanImage texture;
} vkState;

// prepare a command buffer that will begin a new render pass, clear the color and
// depth attachments, bind pipelines and descriptor sets, and render a mesh
bool fillCommandBuffers(size_t i)
{
    const VkCommandBufferBeginInfo bi =
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            .pInheritanceInfo = nullptr};

    // clear the framebuffer and VkRect2D to hold its dimensions
    const std::array<VkClearValue, 2> clearValues =
        {
            VkClearValue{.color = clearValueColor},
            VkClearValue{.depthStencil = {1.0f, 0}}};

    const VkRect2D screenRect = {
        .offset = {0, 0},
        .extent = {.width = kScreenWidth, .height = kScreenHeight}};

    // Each command buffer corresponds to a separate image in the swap chain.
    // fill in the current one
    VK_CHECK(vkBeginCommandBuffer(vkDev.commandBuffers[i], &bi));

    const VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = vkState.renderPass,
        .framebuffer = vkState.swapchainFramebuffers[i],
        .renderArea = screenRect,
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data()};

    vkCmdBeginRenderPass(vkDev.commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the pipeline and descriptor sets.
    vkCmdBindPipeline(vkDev.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, vkState.graphicsPipeline);

    vkCmdBindDescriptorSets(vkDev.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, vkState.pipelineLayout, 0, 1, &vkState.descriptorSets[i], 0, nullptr);
    vkCmdDraw(vkDev.commandBuffers[i], static_cast<uint32_t>(indexBufferSize / (sizeof(unsigned int))), 1, 0, 0);

    vkCmdEndRenderPass(vkDev.commandBuffers[i]);

    VK_CHECK(vkEndCommandBuffer(vkDev.commandBuffers[i]));

    return true;
}

// creates a buffer that will store the UniformBuffer structure
bool createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(UniformBuffer);

    vkState.uniformBuffers.resize(vkDev.swapchainImages.size());
    vkState.uniformBuffersMemory.resize(vkDev.swapchainImages.size());

    for (size_t i = 0; i < vkDev.swapchainImages.size(); i++)
    {
        if (!createBuffer(vkDev.device, vkDev.physicalDevice, bufferSize,
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          vkState.uniformBuffers[i], vkState.uniformBuffersMemory[i]))
        {
            printf("Fail: buffers\n");
            return false;
        }
    }

    return true;
}

// called every frame to update our data in the buffer
void updateUniformBuffer(uint32_t currentImage, const void *uboData, size_t uboSize)
{
    void *data = nullptr;
    vkMapMemory(vkDev.device, vkState.uniformBuffersMemory[currentImage], 0, uboSize, 0, &data);
    memcpy(data, uboData, uboSize);
    vkUnmapMemory(vkDev.device, vkState.uniformBuffersMemory[currentImage]);
}