#pragma once

#include "UtilsVulkan.h"

// each layer is an object that contains a Vulkan pipeline object,
// framebuffers, a Vulkan rendering pass, all descriptor sets, and all kinds of buffers
// necessary for rendering. This object provides an interface that can fill Vulkan command
// buffers for the current frame and update GPU buffers with CPU data, for example,
// per-frame uniforms such as the camera transformation.
class RendererBase
{
public:
    explicit RendererBase(const VulkanRenderDevice &vkDev, VulkanImage depthTexture)
        : device_(vkDev.device), framebufferWidth_(vkDev.framebufferWidth),
          framebufferHeight_(vkDev.framebufferHeight), depthTexture_(depthTexture)
    {
    }
    virtual ~RendererBase();
    // injects a stream of Vulkan commands into the passed command buffer
    virtual void fillCommandBuffer(VkCommandBuffer commandBuffer, size_t currentImage) = 0;

    // gives access to the internally managed depth buffer, which can be shared between layers
    inline VulkanImage getDepthTexture() const { return depthTexture_; }

protected:
    // The first one emits the vkCmdBeginRenderPass,
    // vkCmdBindPipeline, and vkCmdBindDescriptorSet commands to begin rendering.
    void beginRenderPass(VkCommandBuffer commandBuffer, size_t currentImage);
    // allocates a list of GPU buffers that contain uniform
    // data, with one buffer per swapchain image
    bool createUniformBuffers(VulkanRenderDevice &vkDev, size_t uniformDataSize);

    VkDevice device_ = nullptr;

    uint32_t framebufferWidth_ = 0;
    uint32_t framebufferHeight_ = 0;

    // Depth buffer
    VulkanImage depthTexture_;
    
    // maintain one descriptor set per swapchain image
    // Descriptor set (layout + pool + sets) -> uses uniform buffers, textures, framebuffers
    VkDescriptorSetLayout descriptorSetLayout_ = nullptr;
    VkDescriptorPool descriptorPool_ = nullptr;
    std::vector<VkDescriptorSet> descriptorSets_;

    // Framebuffers (one for each command buffer)
    std::vector<VkFramebuffer> swapchainFramebuffers_;

    // 4. Pipeline & render pass (using DescriptorSets & pipeline state options)
    VkRenderPass renderPass_ = nullptr;
    VkPipelineLayout pipelineLayout_ = nullptr;
    VkPipeline graphicsPipeline_ = nullptr;

    // 5. Uniform buffer
    // Each swapchain image has an associated uniform buffer
    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VkDeviceMemory> uniformBuffersMemory_;
};
