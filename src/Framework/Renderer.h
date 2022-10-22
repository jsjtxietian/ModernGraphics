#pragma once

#include "VulkanApp.h"

// consists of a function to fill command buffers and a function to
// update all the current auxiliary buffers containing uniforms or geometry data
struct Renderer
{
    // The Renderer class contains an empty public constructor that stores a reference to
    // VulkanRenderContext and the default size of the output framebuffer
    Renderer(VulkanRenderContext &c)
        : processingWidth(c.vkDev.framebufferWidth), processingHeight(c.vkDev.framebufferHeight), ctx_(c)
    {
    }
    // A pure virtual fillCommandBuffer() method is overridden
    // in subclasses to record rendering commands. Each frame can be rendered to a
    // different framebuffer, so we will pass the image index as a parameter. A frame can
    // be rendered to an onscreen framebuffer. In this case, we pass null handles as the
    // output framebuffer and render pass:
    virtual void fillCommandBuffer(VkCommandBuffer cmdBuffer, size_t currentImage, VkFramebuffer fb = VK_NULL_HANDLE, VkRenderPass rp = VK_NULL_HANDLE) = 0;
    virtual void updateBuffers(size_t currentImage) {}

    inline void updateUniformBuffer(uint32_t currentImage, const uint32_t offset, const uint32_t size, const void *data)
    {
        uploadBufferData(ctx_.vkDev, uniforms_[currentImage].memory, offset, data, size);
    }

    // The initPipeline() function creates a pipeline layout and then
    // immediately uses this layout to create the Vulkan pipeline itself. Just like with any of
    // the Vulkan objects, the pipeline handle is stored in the ctx_.resources object
    void initPipeline(const std::vector<const char *> &shaders, const PipelineInfo &pInfo, uint32_t vtxConstSize = 0, uint32_t fragConstSize = 0)
    {
        pipelineLayout_ = ctx_.resources.addPipelineLayout(descriptorSetLayout_, vtxConstSize, fragConstSize);
        graphicsPipeline_ = ctx_.resources.addPipeline(renderPass_.handle, pipelineLayout_, shaders, pInfo);
    }

    // Each renderer defines a dedicated render pass that is compatible with the set of
    // input textures. The initRenderPass() function contains the logic that's used
    // in most of the renderer classes. The input pipeline parameters can be changed
    // if offscreen rendering is performed (non-empty list of output textures). If we
    // pass in a valid renderPass object, then it is directly assigned to the internal
    // renderPass_ field:
    PipelineInfo initRenderPass(const PipelineInfo &pInfo, const std::vector<VulkanTexture> &outputs,
                                RenderPass renderPass = RenderPass(),
                                RenderPass fallbackPass = RenderPass())
    {
        PipelineInfo outInfo = pInfo;
        // If the output list is empty, which means we are rendering to the screen, and the
        // renderPass parameter is not valid, then we take fallbackPass as the rendering
        // pass. Usually, it is taken from one of the class fields of VulkanRenderContext
        // – screenRenderPass or screenRenderPass_NoDepth – depending on
        // whether we need depth buffering or not. We may need to modify the input pipeline
        // description, so we will declare a new PipelineInfo variable. If we are rendering
        // to an offscreen buffer, we must store the buffer dimensions in the rendering area. The
        // output pipeline information structure also contains the actual rendering area's size:
        if (!outputs.empty()) // offscreen rendering
        {
            printf("Creating framebuffer (outputs = %d). Output0: %dx%d; Output1: %dx%d\n",
                   (int)outputs.size(), outputs[0].width, outputs[0].height,
                   (outputs.size() > 1 ? outputs[1].width : 0), (outputs.size() > 1 ? outputs[1].height : 0));
            fflush(stdout);

            processingWidth = outputs[0].width;
            processingHeight = outputs[0].height;

            outInfo.width = processingWidth;
            outInfo.height = processingHeight;

            // If no external renderpass is provided, we allocate a new one that's compatible with
            // the output framebuffer. If we have only one depth attachment, then we must use
            // a special rendering pass. The isDepthFormat() function is a one-liner that
            // compares the VkFormat parameter with one of the predefined Vulkan depth
            // buffer formats; see the UtilsVulkan.h file for details. To render to the screen
            // framebuffer, we will use one of the renderpasses from our parameters:
            renderPass_ = (renderPass.handle != VK_NULL_HANDLE) ? renderPass : ((isDepthFormat(outputs[0].format) && (outputs.size() == 1)) ? ctx_.resources.addDepthRenderPass(outputs) : ctx_.resources.addRenderPass(outputs, RenderPassCreateInfo(), true));
            framebuffer_ = ctx_.resources.addFramebuffer(renderPass_, outputs);
        }
        else
        {
            renderPass_ = (renderPass.handle != VK_NULL_HANDLE) ? renderPass : fallbackPass;
        }
        return outInfo;
    }

    void beginRenderPass(VkRenderPass rp, VkFramebuffer fb, VkCommandBuffer commandBuffer, size_t currentImage)
    {
        // declare some buffer clearing values and the output area:
        const VkClearValue clearValues[2] = {
            VkClearValue{.color = {1.0f, 1.0f, 1.0f, 1.0f}},
            VkClearValue{.depthStencil = {1.0f, 0}}};

        const VkRect2D rect{
            .offset = {0, 0},
            .extent = {.width = processingWidth, .height = processingHeight}};

        // To avoid calling the Vulkan API directly, we will pass a complete set of parameters
        // to the VulkanRenderContext::beginRenderPass() function Some arithmetic is required to calculate the
        // number of clear values. If we don't need to clear the color, depth, or both buffers, we
        // will change the offset in the clearValues array:
        ctx_.beginRenderPass(commandBuffer, rp, currentImage, rect,
                             fb,
                             (renderPass_.info.clearColor_ ? 1u : 0u) + (renderPass_.info.clearDepth_ ? 1u : 0u),
                             renderPass_.info.clearColor_ ? &clearValues[0] : (renderPass_.info.clearDepth_ ? &clearValues[1] : nullptr));

        // After starting a renderpass, we must bind our local graphics pipeline and descriptor
        // set for this frame:
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSets_[currentImage], 0, nullptr);
    }

    // cached instances of the framebuffer
    // and renderpass, as well as the output framebuffer dimensions
    VkFramebuffer framebuffer_ = nullptr;
    RenderPass renderPass_;

    uint32_t processingWidth;
    uint32_t processingHeight;

    // Updating individual textures (9 is the binding in our Chapter7-Chapter9 IBL scene shaders)
    void updateTexture(uint32_t textureIndex, VulkanTexture newTexture, uint32_t bindingIndex = 9)
    {
        for (auto ds : descriptorSets_)
            updateTextureInDescriptorSetArray(ctx_.vkDev, ds, newTexture, textureIndex, bindingIndex);
    }

protected:
    // use the VulkanRendererContext reference to cleanly manage Vulkan objects. Each
    // renderer contains a list of descriptor sets, along with a pool and a layout for all the
    // sets. The pipeline layout and the pipeline itself are also present in every renderer. An
    // array of uniform buffers, one for each of the frames in a swapchain, is the last field
    // of our Renderer:
    VulkanRenderContext &ctx_;

    // Descriptor set (layout + pool + sets) -> uses uniform buffers, textures, framebuffers
    VkDescriptorSetLayout descriptorSetLayout_ = nullptr;
    VkDescriptorPool descriptorPool_ = nullptr;
    std::vector<VkDescriptorSet> descriptorSets_;

    // 4. Pipeline & render pass (using DescriptorSets & pipeline state options)
    VkPipelineLayout pipelineLayout_ = nullptr;
    VkPipeline graphicsPipeline_ = nullptr;

    std::vector<VulkanBuffer> uniforms_;
};
