#pragma once

#include "RendererBase.h"

class ImGuiRenderer : public RendererBase
{
public:
    explicit ImGuiRenderer(VulkanRenderDevice &vkDev);
    explicit ImGuiRenderer(VulkanRenderDevice &vkDev, const std::vector<VulkanTexture> &textures);
    virtual ~ImGuiRenderer();

    virtual void fillCommandBuffer(VkCommandBuffer commandBuffer, size_t currentImage) override;
    void updateBuffers(VulkanRenderDevice &vkDev, uint32_t currentImage, const ImDrawData *imguiDrawData);

private:
    // store a pointer to ImDrawData that holds all the ImGui-rendered widgets
    const ImDrawData *drawData = nullptr;
    
    bool createDescriptorSet(VulkanRenderDevice &vkDev);

    /* Descriptor set with multiple textures (for offscreen buffer display etc.) */
    bool createMultiDescriptorSet(VulkanRenderDevice &vkDev);

    std::vector<VulkanTexture> extTextures_;

    // storage buffer with index and vertex data
    // allocate one geometry buffer for each swapchain image to avoid any kind of synchronization
    VkDeviceSize bufferSize_;
    std::vector<VkBuffer> storageBuffer_;
    std::vector<VkDeviceMemory> storageBufferMemory_;

    VkSampler fontSampler_;
    VulkanImage font_;
};
