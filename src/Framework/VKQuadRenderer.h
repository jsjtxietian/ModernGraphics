#pragma once

#include "Renderer.h"

// A companion for LineCanvas to render texture quadrangles
struct QuadRenderer : public Renderer
{
	// The constructor of this class takes a list of textures, which can be mapped to
	// quadrangles later, and an array of output textures. If the list of output textures is
	// empty, this class renders directly to the screen. A render pass that's compatible with
	// the output textures can be passed from the outside context as an optional parameter:
	QuadRenderer(VulkanRenderContext &ctx, const std::vector<VulkanTexture> &textures,
				 const std::vector<VulkanTexture> &outputs = {},
				 RenderPass screenRenderPass = RenderPass());

	void fillCommandBuffer(VkCommandBuffer cmdBuffer, size_t currentImage, VkFramebuffer fb = VK_NULL_HANDLE, VkRenderPass rp = VK_NULL_HANDLE) override;
	void updateBuffers(size_t currentImage) override;

	void quad(float x1, float y1, float x2, float y2, int texIdx);
	void clear() { quads_.clear(); }

private:
	// a structured buffer of vertex data.
	// Note that no index buffer is being used, so there are additional possibilities for
	// optimizations. At the end, we store a list of buffers for storing geometry data on our
	// GPU – one buffer per swapchain image
	struct VertexData
	{
		glm::vec3 pos;
		glm::vec2 tc;
		int texIdx;
	};

	std::vector<VertexData> quads_;
	std::vector<VulkanBuffer> storages_;
};
