#include "ComputeBase.h"

// allocate I/O buffers. Those are shared between the compute and graphics queues
ComputeBase::ComputeBase(VulkanRenderDevice &vkDev, const char *shaderName, uint32_t inputSize, uint32_t outputSize)
	: vkDev(vkDev)
{
	createSharedBuffer(vkDev, inputSize,
					   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					   inBuffer, inBufferMemory);

	// allocate both buffers as host-visible. If the output buffer is
	// needed only for rendering purposes, host visibility and coherence can be disabled:
	createSharedBuffer(vkDev, outputSize,
					   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					   outBuffer, outBufferMemory);
	// uses a single compute shader to process the input buffer and write the output buffer
	ShaderModule s;
	createShaderModule(vkDev.device, &s, shaderName);

	createComputeDescriptorSetLayout(vkDev.device, &dsLayout);
	createPipelineLayout(vkDev.device, dsLayout, &pipelineLayout);
	createComputePipeline(vkDev.device, s.shaderModule, pipelineLayout, &pipeline);
	createComputeDescriptorSet(vkDev.device, dsLayout);

	vkDestroyShaderModule(vkDev.device, s.shaderModule, nullptr);
}

ComputeBase::~ComputeBase()
{
	vkDestroyBuffer(vkDev.device, inBuffer, nullptr);
	vkFreeMemory(vkDev.device, inBufferMemory, nullptr);

	vkDestroyBuffer(vkDev.device, outBuffer, nullptr);
	vkFreeMemory(vkDev.device, outBufferMemory, nullptr);

	vkDestroyPipelineLayout(vkDev.device, pipelineLayout, nullptr);
	vkDestroyPipeline(vkDev.device, pipeline, nullptr);

	vkDestroyDescriptorSetLayout(vkDev.device, dsLayout, nullptr);
	vkDestroyDescriptorPool(vkDev.device, descriptorPool, nullptr);
}

bool ComputeBase::createComputeDescriptorSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout)
{
	// Descriptor pool, only have two buffers 
	VkDescriptorPoolSize descriptorPoolSize = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, 0, 0, 1, 1, &descriptorPoolSize};

	VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, 0, &descriptorPool));

	// Descriptor set
	// only need one set for the computation
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		0, descriptorPool, 1, &descriptorSetLayout};

	VK_CHECK(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

	// Finally, update descriptor set with concrete buffer pointers
	// The I/O buffer handles are bound to the descriptor set.
	VkDescriptorBufferInfo inBufferInfo = {inBuffer, 0, VK_WHOLE_SIZE};

	VkDescriptorBufferInfo outBufferInfo = {outBuffer, 0, VK_WHOLE_SIZE};

	VkWriteDescriptorSet writeDescriptorSet[2] = {
		{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &inBufferInfo, 0},
		{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descriptorSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &outBufferInfo, 0}};

	vkUpdateDescriptorSets(device, 2, writeDescriptorSet, 0, 0);

	return true;
}
