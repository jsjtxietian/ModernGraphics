#pragma once

#include "RendererBase.h"

// initializes and starts an empty rendering pass whose only purpose
// is to clear the color and depth buffers
class VulkanClear: public RendererBase
{
public:
	VulkanClear(VulkanRenderDevice& vkDev, VulkanImage depthTexture);

	virtual void fillCommandBuffer(VkCommandBuffer commandBuffer, size_t currentImage) override;

private:
	bool shouldClearDepth;
};
