#include "ShaderProcessor.h"
#include "VulkanResources.h"

VulkanShaderProcessor::VulkanShaderProcessor(VulkanRenderContext &ctx,
											 const PipelineInfo &pInfo,
											 const DescriptorSetInfo &dsInfo,
											 const std::vector<const char *> &shaders,
											 const std::vector<VulkanTexture> &outputs,
											 uint32_t indexBufferSize,
											 RenderPass screenRenderPass)
	: Renderer(ctx), indexBufferSize(indexBufferSize)
{
	// Fullscreen processors need only a single descriptor set, which is created using the
	// dsInfo structure passed via parameters. At the end of initialization, a graphics
	// pipeline with a suitable render pass is created:
	descriptorSetLayout_ = ctx.resources.addDescriptorSetLayout(dsInfo);

	descriptorSets_.resize(1);
	descriptorSets_[0] = ctx.resources.addDescriptorSet(ctx.resources.addDescriptorPool(dsInfo), descriptorSetLayout_);
	ctx.resources.updateDescriptorSet(descriptorSets_[0], dsInfo);

	initPipeline(shaders, initRenderPass(pInfo, outputs, screenRenderPass, ctx.screenRenderPass_NoDepth));
}

void VulkanShaderProcessor::fillCommandBuffer(VkCommandBuffer cmdBuffer, size_t currentImage, VkFramebuffer fb, VkRenderPass rp)
{
	beginRenderPass((rp != VK_NULL_HANDLE) ? rp : renderPass_.handle, (fb != VK_NULL_HANDLE) ? fb : framebuffer_, cmdBuffer, 0);

	// uses the stored size of an index buffer
	vkCmdDraw(cmdBuffer, static_cast<uint32_t>((indexBufferSize) / sizeof(uint32_t)), 1, 0, 0);
	vkCmdEndRenderPass(cmdBuffer);
}
