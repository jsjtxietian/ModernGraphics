#pragma once

#include "RendererBase.h"

// create another empty rendering pass that
// transitions the swapchain image to the VK_IMAGE_LAYOUT_PRESENT_SRC_KHR format
class VulkanFinish : public RendererBase
{
public:
	VulkanFinish(VulkanRenderDevice &vkDev, VulkanImage depthTexture);

	virtual void fillCommandBuffer(VkCommandBuffer commandBuffer, size_t currentImage) override;
};
