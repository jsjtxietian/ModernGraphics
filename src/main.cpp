#define VK_NO_PROTOTYPES
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <array>
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

// use the descriptor pool to create the required descriptor set for
// our demo application. However, the descriptor set must have a fixed layout that
// describes the number and usage type of all the texture samples and buffers. This
// layout is also a Vulkan object
bool createDescriptorSet()
{
    // declare a list of buffer and sampler descriptions. Each entry in this list
    // defines which shader unit this entity is bound to, the exact data type of this entity,
    // and which shader stage (or multiple stages) can access this item
    const std::array<VkDescriptorSetLayoutBinding, 4> bindings = {
        descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
        descriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
        descriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
        descriptorSetLayoutBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)};

    const VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()};

    VK_CHECK(vkCreateDescriptorSetLayout(vkDev.device, &layoutInfo, nullptr, &vkState.descriptorSetLayout));

    // allocate a number of descriptor set layouts, one for each swap chain
    // image, just like we did with the uniform and command buffers
    std::vector<VkDescriptorSetLayout> layouts(vkDev.swapchainImages.size(), vkState.descriptorSetLayout);

    // Once allocated the descriptor sets with the specified layout, we must update
    // these descriptor sets with concrete buffer and texture handles. This operation can
    // be viewed as an analogue of texture and buffer binding in OpenGL. The crucial
    // difference is that we do not do this at every frame since binding is prebaked into the
    // pipeline. The minor downside of this approach is that we cannot simply change the
    // texture from frame to frame.

    const VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = vkState.descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(vkDev.swapchainImages.size()),
        .pSetLayouts = layouts.data()};

    vkState.descriptorSets.resize(vkDev.swapchainImages.size());

    VK_CHECK(vkAllocateDescriptorSets(vkDev.device, &allocInfo, vkState.descriptorSets.data()));

    // use one uniform buffer, one index buffer, one vertex buffer, and one texture:
    for (size_t i = 0; i < vkDev.swapchainImages.size(); i++)
    {
        const VkDescriptorBufferInfo bufferInfo = {
            .buffer = vkState.uniformBuffers[i],
            .offset = 0,
            .range = sizeof(UniformBuffer)};
        const VkDescriptorBufferInfo bufferInfo2 = {
            .buffer = vkState.storageBuffer,
            .offset = 0,
            .range = vertexBufferSize};
        const VkDescriptorBufferInfo bufferInfo3 = {
            .buffer = vkState.storageBuffer,
            .offset = vertexBufferSize,
            .range = indexBufferSize};
        const VkDescriptorImageInfo imageInfo = {
            .sampler = vkState.textureSampler,
            .imageView = vkState.texture.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        // The VkWriteDescriptorSet operation array contains all the "bindings" for the
        // buffers we declared previously
        const std::array<VkWriteDescriptorSet, 4> descriptorWrites = {
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = vkState.descriptorSets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &bufferInfo},
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = vkState.descriptorSets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &bufferInfo2},
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = vkState.descriptorSets[i],
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &bufferInfo3},
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = vkState.descriptorSets[i],
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &imageInfo},
        };

        // update the descriptor by applying the necessary descriptor write operations
        vkUpdateDescriptorSets(vkDev.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    return true;
}
