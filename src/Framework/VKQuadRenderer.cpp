#include "VKQuadRenderer.h"

static constexpr int MAX_QUADS = 256;

// The main function for us is, of course, the quad routine, which adds a new quadrangle
// to the buffer. A quadrangle is defined by four corner points whose coordinates are
// calculated from parameters. Each of the vertices is marked with a texture index that
// is passed to the fragment shader. Since Vulkan does not support quadrangles as a
// rendering primitive, we will split the quadrangle into two adjacent triangles. Each
// triangle is specified by three vertices:
void QuadRenderer::quad(float x1, float y1, float x2, float y2, int texIdx)
{
	VertexData v1{{x1, y1, 0}, {0, 0}, texIdx};
	VertexData v2{{x2, y1, 0}, {1, 0}, texIdx};
	VertexData v3{{x2, y2, 0}, {1, 1}, texIdx};
	VertexData v4{{x1, y2, 0}, {0, 1}, texIdx};

	quads_.push_back(v1);
	quads_.push_back(v2);
	quads_.push_back(v3);
	quads_.push_back(v1);
	quads_.push_back(v3);
	quads_.push_back(v4);
}

void QuadRenderer::fillCommandBuffer(VkCommandBuffer commandBuffer, size_t currentImage, VkFramebuffer fb, VkRenderPass rp)
{
	// If the quads list is empty, no commands need to be issued:
	if (quads_.empty())
		return;

	beginRenderPass((rp != VK_NULL_HANDLE) ? rp : renderPass_.handle, (fb != VK_NULL_HANDLE) ? fb : framebuffer_, commandBuffer, currentImage);

	vkCmdDraw(commandBuffer, static_cast<uint32_t>(quads_.size()), 1, 0, 0);
	vkCmdEndRenderPass(commandBuffer);
}

// If the quad geometry or amount changes, the quad geometry buffer implicitly
// reuploads to the GPU using the overridden uploadBuffers() function:
void QuadRenderer::updateBuffers(size_t currentImage)
{
	if (!quads_.empty())
		uploadBufferData(ctx_.vkDev, storages_[currentImage].memory, 0, quads_.data(), quads_.size() * sizeof(VertexData));
}

QuadRenderer::QuadRenderer(VulkanRenderContext &ctx,
						   const std::vector<VulkanTexture> &textures,
						   const std::vector<VulkanTexture> &outputs,
						   RenderPass screenRenderPass) : Renderer(ctx)
{
	// The initialization begins by creating an appropriate framebuffer and a render pass.
	// The QuadRenderer class does not use the depth buffer, so a depthless framebuffer
	// is passed as a parameter:
	const PipelineInfo pInfo = initRenderPass(PipelineInfo{}, outputs, screenRenderPass, ctx.screenRenderPass_NoDepth);

	// The vertex buffer must contain six vertices for each item on the screen since we are
	// using two triangles to represent a single quadrangle. MAX_QUADS is just an integer
	// constant containing the largest number of quadrangles in a buffer. The number of
	// geometry buffers and descriptor sets equals the number of swapchain images:
	uint32_t vertexBufferSize = MAX_QUADS * 6 * sizeof(VertexData);

	const size_t imgCount = ctx.vkDev.swapchainImages.size();
	descriptorSets_.resize(imgCount);
	storages_.resize(imgCount);

	// Each of the descriptor sets for this renderer contains a reference to all the textures
	// and uses a single geometry buffer. Here, we will only fill in a helper structure that is
	// then passed to the construction routine:
	DescriptorSetInfo dsInfo = {
		.buffers = {
			storageBufferAttachment(VulkanBuffer{}, 0, vertexBufferSize, VK_SHADER_STAGE_VERTEX_BIT)},
		.textureArrays = {fsTextureArrayAttachment(textures)}};

	// Once we have a structure that describes the layout of the descriptor set, we will
	// call the VulkanResources object to create Vulkan's descriptor set layout and a
	// descriptor pool
	descriptorSetLayout_ = ctx.resources.addDescriptorSetLayout(dsInfo);
	descriptorPool_ = ctx.resources.addDescriptorPool(dsInfo, imgCount);

	// For each of the swapchain images, we allocate a GPU geometry buffer and put it in
	// the first slot of the descriptor set:
	for (size_t i = 0; i < imgCount; i++)
	{
		storages_[i] = ctx.resources.addStorageBuffer(vertexBufferSize);
		dsInfo.buffers[0].buffer = storages_[i];
		descriptorSets_[i] = ctx.resources.addDescriptorSet(descriptorPool_, descriptorSetLayout_);
		ctx.resources.updateDescriptorSet(descriptorSets_[i], dsInfo);
	}

	// At the end of the constructor, we should create a pipeline using the following shaders:
	initPipeline({"data/shaders/VK_QuadRenderer.vert", "data/shaders/VK_QuadRenderer.frag"}, pInfo);
}
