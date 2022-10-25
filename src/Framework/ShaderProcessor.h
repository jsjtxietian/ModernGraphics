#pragma once

#include "Renderer.h"

/*
   @brief Shader (post)processor for fullscreen effects

   Multiple input textures, single output [color + depth]. Possibly, can be extented to multiple outputs (allocate appropriate framebuffer)
*/
struct VulkanShaderProcessor : public Renderer
{
	VulkanShaderProcessor(VulkanRenderContext &ctx,
						  const PipelineInfo &pInfo,
						  const DescriptorSetInfo &dsInfo,
						  const std::vector<const char *> &shaders,
						  const std::vector<VulkanTexture> &outputs,
						  uint32_t indexBufferSize = 6 * 4,
						  RenderPass screenRenderPass = RenderPass());

	// a generic fillCommandBuffer() method that calls a custom shader
	void fillCommandBuffer(VkCommandBuffer cmdBuffer, size_t currentImage, VkFramebuffer fb = VK_NULL_HANDLE, VkRenderPass rp = VK_NULL_HANDLE) override;

private:
	// a default value of 24, which is the number of bytes to store six 32-bit integers
	// This is the size of an index array for two triangles
	// that form a single quadrangle. The reason this class exposes this parameter to
	// the user is simple: rendering a complex 3D mesh instead of a single quadrangle
	// is performed in the same way.
	uint32_t indexBufferSize;
};

struct QuadProcessor : public VulkanShaderProcessor
{
	// The vertex shader is always the same, but the
	// fragment shader specified as a parameter contains custom postprocessing per-pixel
	// logic. If there are no offscreen render targets, we use a screen render pass
	QuadProcessor(VulkanRenderContext &ctx,
				  const DescriptorSetInfo &dsInfo,
				  const std::vector<VulkanTexture> &outputs,
				  const char *shaderFile) : VulkanShaderProcessor(ctx, ctx.pipelineParametersForOutputs(outputs), dsInfo,
																  std::vector<const char *>{"data/shaders/08/VK02_Quad.vert", shaderFile},
																  outputs, 6 * 4, outputs.empty() ? ctx.screenRenderPass : RenderPass())
	{
	}
};

struct BufferProcessor : public VulkanShaderProcessor
{
	BufferProcessor(VulkanRenderContext &ctx, const DescriptorSetInfo &dsInfo,
					const std::vector<VulkanTexture> &outputs, const std::vector<const char *> &shaderFiles,
					uint32_t indexBufferSize = 6 * 4, RenderPass renderPass = RenderPass()) : VulkanShaderProcessor(ctx, ctx.pipelineParametersForOutputs(outputs), dsInfo,
																													shaderFiles, outputs, indexBufferSize, outputs.empty() ? ctx.screenRenderPass : renderPass)
	{
	}
};

// for single mesh rendering into an offscreen framebuffer
struct OffscreenMeshRenderer : public BufferProcessor
{
	OffscreenMeshRenderer(
		VulkanRenderContext &ctx,
		VulkanBuffer uniformBuffer,
		const std::pair<BufferAttachment, BufferAttachment> &meshBuffer,
		const std::vector<TextureAttachment> &usedTextures,
		const std::vector<VulkanTexture> &outputs,
		const std::vector<const char *> &shaderFiles,
		bool firstPass = false)
		:

		  BufferProcessor(ctx,
						  DescriptorSetInfo{
							  .buffers = {
								  uniformBufferAttachment(uniformBuffer, 0, 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
								  meshBuffer.first,
								  meshBuffer.second,
							  },
							  .textures = usedTextures},
						  outputs, shaderFiles, meshBuffer.first.size, ctx.resources.addRenderPass(outputs, RenderPassCreateInfo{.clearColor_ = firstPass, .clearDepth_ = firstPass, .flags_ = (uint8_t)((firstPass ? eRenderPassBit_First : eRenderPassBit_OffscreenInternal) | eRenderPassBit_Offscreen)}))
	{
	}
};
