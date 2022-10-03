#include "MultiMeshRenderer.h"

bool MultiMeshRenderer::createDescriptorSet(VulkanRenderDevice &vkDev)
{
    const std::array<VkDescriptorSetLayoutBinding, 5> bindings = {
        descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
        /* vertices [part of this.storageBuffer] */
        descriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
        /* indices [part of this.storageBuffer] */
        descriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
        /* draw data [this.drawDataBuffer] */
        descriptorSetLayoutBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
        /* material data [this.materialBuffer] */
        descriptorSetLayoutBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)};

    const VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()};

    VK_CHECK(vkCreateDescriptorSetLayout(vkDev.device, &layoutInfo, nullptr, &descriptorSetLayout_));

    std::vector<VkDescriptorSetLayout> layouts(vkDev.swapchainImages.size(), descriptorSetLayout_);

    const VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptorPool_,
        .descriptorSetCount = static_cast<uint32_t>(vkDev.swapchainImages.size()),
        .pSetLayouts = layouts.data()};

    descriptorSets_.resize(vkDev.swapchainImages.size());

    VK_CHECK(vkAllocateDescriptorSets(vkDev.device, &allocInfo, descriptorSets_.data()));

    for (size_t i = 0; i < vkDev.swapchainImages.size(); i++)
    {
        VkDescriptorSet ds = descriptorSets_[i];

        const VkDescriptorBufferInfo bufferInfo = {uniformBuffers_[i], 0, sizeof(mat4)};
        const VkDescriptorBufferInfo bufferInfo2 = {storageBuffer_, 0, maxVertexBufferSize_};
        const VkDescriptorBufferInfo bufferInfo3 = {storageBuffer_, maxVertexBufferSize_, maxIndexBufferSize_};
        const VkDescriptorBufferInfo bufferInfo4 = {drawDataBuffers_[i], 0, maxDrawDataSize_};
        const VkDescriptorBufferInfo bufferInfo5 = {materialBuffer_, 0, maxMaterialSize_};

        const std::array<VkWriteDescriptorSet, 5> descriptorWrites = {
            bufferWriteDescriptorSet(ds, &bufferInfo, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER),
            bufferWriteDescriptorSet(ds, &bufferInfo2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
            bufferWriteDescriptorSet(ds, &bufferInfo3, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
            bufferWriteDescriptorSet(ds, &bufferInfo4, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
            bufferWriteDescriptorSet(ds, &bufferInfo5, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
            //			imageWriteDescriptorSet( ds, &imageInfo,   3)
        };

        vkUpdateDescriptorSets(vkDev.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    return true;
}

void MultiMeshRenderer::fillCommandBuffer(VkCommandBuffer commandBuffer, size_t currentImage)
{
    beginRenderPass(commandBuffer, currentImage);
    /* For CountKHR (Vulkan 1.1) we may use indirect rendering with GPU-based object counter */
    /// vkCmdDrawIndirectCountKHR(commandBuffer, indirectBuffers_[currentImage], 0, countBuffers_[currentImage], 0, maxShapes_, sizeof(VkDrawIndirectCommand));
    /* For Vulkan 1.0 vkCmdDrawIndirect is enough */
    vkCmdDrawIndirect(commandBuffer, indirectBuffers_[currentImage], 0, maxShapes_, sizeof(VkDrawIndirectCommand));

    vkCmdEndRenderPass(commandBuffer);
}

void MultiMeshRenderer::updateUniformBuffer(VulkanRenderDevice &vkDev, size_t currentImage, const mat4 &m)
{
    uploadBufferData(vkDev, uniformBuffersMemory_[currentImage], 0, glm::value_ptr(m), sizeof(mat4));
}

// The geometry data is uploaded into the GPU in two parts. Technically, this can be
// simplified to a single upload operation if the indices and vertices are stored in a
// single contiguous buffer:
void MultiMeshRenderer::updateGeometryBuffers(VulkanRenderDevice &vkDev, uint32_t vertexCount, uint32_t indexCount, const void *vertices, const void *indices)
{
    uploadBufferData(vkDev, storageBufferMemory_, 0, vertices, vertexCount);
    uploadBufferData(vkDev, storageBufferMemory_, maxVertexBufferSize_, indices, indexCount);
}

void MultiMeshRenderer::updateDrawDataBuffer(VulkanRenderDevice &vkDev, size_t currentImage, uint32_t drawDataSize, const void *drawData)
{
    uploadBufferData(vkDev, drawDataBuffersMemory_[currentImage], 0, drawData, drawDataSize);
}

void MultiMeshRenderer::updateMaterialBuffer(VulkanRenderDevice &vkDev, uint32_t materialSize, const void *materialData)
{
}

void MultiMeshRenderer::updateCountBuffer(VulkanRenderDevice &vkDev, size_t currentImage, uint32_t itemCount)
{
    uploadBufferData(vkDev, countBuffersMemory_[currentImage], 0, &itemCount, sizeof(uint32_t));
}

// A buffer required for indirect rendering is filled according to the loaded instances
// list. After mapping a region of GPU-visible memory into a CPU-accessible pointer,
// we iterate the drawdata array. For each loaded mesh instance, we fetch the vertex
// count and vertex offset within the data buffer:
void MultiMeshRenderer::updateIndirectBuffers(VulkanRenderDevice &vkDev, size_t currentImage, bool *visibility)
{
    VkDrawIndirectCommand *data = nullptr;
    vkMapMemory(vkDev.device, indirectBuffersMemory_[currentImage], 0, 2 * sizeof(VkDrawIndirectCommand), 0, (void **)&data);

    for (uint32_t i = 0; i < maxShapes_; i++)
    {
        const uint32_t j = shapes[i].meshIndex;
        const uint32_t lod = shapes[i].LOD;
        data[i] = {
            .vertexCount = meshData_.meshes_[j].getLODIndicesCount(lod),
            .instanceCount = visibility ? (visibility[i] ? 1u : 0u) : 1u,
            .firstVertex = 0,
            .firstInstance = i};
    }
    vkUnmapMemory(vkDev.device, indirectBuffersMemory_[currentImage]);
}

// loads a list of transformations for each mesh
void MultiMeshRenderer::loadDrawData(const char *drawDataFile)
{
    FILE *f = fopen(drawDataFile, "rb");

    if (!f)
    {
        printf("Unable to open draw data file. Run MeshConvert first\n");
        exit(255);
    }

    // After determining the size of the input file, we should calculate the number of
    // shapes in this file:
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    maxShapes_ = static_cast<uint32_t>(fsize / sizeof(DrawData));

    printf("Reading draw data items: %d\n", (int)maxShapes_);
    fflush(stdout);

    shapes.resize(maxShapes_);

    if (fread(shapes.data(), sizeof(DrawData), maxShapes_, f) != maxShapes_)
    {
        printf("Unable to read draw data\n");
        exit(255);
    }

    fclose(f);
}

MultiMeshRenderer::MultiMeshRenderer(
    VulkanRenderDevice &vkDev,
    const char *meshFile,
    const char *drawDataFile,
    const char *materialFile,
    const char *vertShaderFile,
    const char *fragShaderFile) : vkDev(vkDev),
                                  RendererBase(vkDev, VulkanImage())
{
    if (!createColorAndDepthRenderPass(vkDev, false, &renderPass_, RenderPassCreateInfo()))
    {
        printf("Failed to create render pass\n");
        exit(EXIT_FAILURE);
    }

    // we start the rendering pass by saving the frame buffer dimensions:
    framebufferWidth_ = vkDev.framebufferWidth;
    framebufferHeight_ = vkDev.framebufferHeight;

    createDepthResources(vkDev, framebufferWidth_, framebufferHeight_, depthTexture_);

    // To proceed with the GPU buffer allocation, we need to read the mesh and instance data
    loadDrawData(drawDataFile);

    MeshFileHeader header = loadMeshData(meshFile, meshData_);

    const uint32_t indirectDataSize = maxShapes_ * sizeof(VkDrawIndirectCommand);
    maxDrawDataSize_ = maxShapes_ * sizeof(DrawData);
    maxMaterialSize_ = 1024;

    countBuffers_.resize(vkDev.swapchainImages.size());
    countBuffersMemory_.resize(vkDev.swapchainImages.size());

    // We require a copy of the instance and indirect rendering data for each of the swapchain images:
    drawDataBuffers_.resize(vkDev.swapchainImages.size());
    drawDataBuffersMemory_.resize(vkDev.swapchainImages.size());

    indirectBuffers_.resize(vkDev.swapchainImages.size());
    indirectBuffersMemory_.resize(vkDev.swapchainImages.size());

    // just allocate the buffer for the material data and avoid using it for now:
    if (!createBuffer(vkDev.device, vkDev.physicalDevice, maxMaterialSize_,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      materialBuffer_, materialBufferMemory_))
    {
        printf("Cannot create material buffer\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    // To allocate a descriptor set, we need to save the sizes of the index and vertex buffers:
    maxVertexBufferSize_ = header.vertexDataSize;
    maxIndexBufferSize_ = header.indexDataSize;

    // Now, we want to store arbitrary arrays of mesh vertices and face indices,
    // so this forces us to support arbitrary offsets of GPU sub-buffers. Our descriptor set for
    // MultiMeshRenderer has two logical storage buffers for index and vertex data.
    // we pad the vertex data with zeros so that its size has the necessary alignment properties:

    VkPhysicalDeviceProperties devProps;
    vkGetPhysicalDeviceProperties(vkDev.physicalDevice, &devProps);
    const uint32_t offsetAlignment = static_cast<uint32_t>(devProps.limits.minStorageBufferOffsetAlignment);
    if ((maxVertexBufferSize_ & (offsetAlignment - 1)) != 0)
    {
        int floats = (offsetAlignment - (maxVertexBufferSize_ & (offsetAlignment - 1))) / sizeof(float);
        // add the necessary zeros to the end of the vertex data array
        for (int ii = 0; ii < floats; ii++)
            meshData_.vertexData_.push_back(0);
        maxVertexBufferSize_ = (maxVertexBufferSize_ + offsetAlignment) & ~(offsetAlignment - 1);
    }

    // Once we have calculated the index and vertex buffer size, we allocate the GPU buffer itself
    if (!createBuffer(vkDev.device, vkDev.physicalDevice, maxVertexBufferSize_ + maxIndexBufferSize_,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      storageBuffer_, storageBufferMemory_))
    {
        printf("Cannot create vertex/index buffer\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    // only update the geometry data once, during the initialization stage
    updateGeometryBuffers(vkDev, header.vertexDataSize, header.indexDataSize,
                          meshData_.vertexData_.data(), meshData_.indexData_.data());

    // One swapchain image corresponds to one instance buffer or indirect draw data buffer:
    for (size_t i = 0; i < vkDev.swapchainImages.size(); i++)
    {
        // If the instances are not changed by the CPU, for example, for
        // static geometry, such buffers could be allocated on the GPU for better performance.
        // However, this requires you to allocate a staging buffer.
        if (!createBuffer(vkDev.device, vkDev.physicalDevice, indirectDataSize,
                          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,                                        // | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, /* for debugging we make it host-visible */
                          indirectBuffers_[i], indirectBuffersMemory_[i]))
        {
            printf("Cannot create indirect buffer\n");
            fflush(stdout);
            exit(EXIT_FAILURE);
        }

        // fill the indirect draw buffers with the required values
        // we do not update this buffer during runtime.
        // However, it might be necessary to do so if we want to set the
        // LOD for the meshes when the camera position changes.
        updateIndirectBuffers(vkDev, i);

        // the instance data also stays immutable after initialization:
        if (!createBuffer(vkDev.device, vkDev.physicalDevice, maxDrawDataSize_,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, /* for debugging we make it host-visible */
                          drawDataBuffers_[i], drawDataBuffersMemory_[i]))
        {
            printf("Cannot create draw data buffer\n");
            fflush(stdout);
            exit(EXIT_FAILURE);
        }

        // upload local instance data to the GPU buffer:
        updateDrawDataBuffer(vkDev, i, maxDrawDataSize_, shapes.data());

        if (!createBuffer(vkDev.device, vkDev.physicalDevice, sizeof(uint32_t),
                          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, /* for debugging we make it host-visible */
                          countBuffers_[i], countBuffersMemory_[i]))
        {
            printf("Cannot create count buffer\n");
            fflush(stdout);
            exit(EXIT_FAILURE);
        }

        updateCountBuffer(vkDev, i, maxShapes_);
    }

    if (!createUniformBuffers(vkDev, sizeof(mat4)) ||
        !createColorAndDepthFramebuffers(vkDev, renderPass_, VK_NULL_HANDLE, swapchainFramebuffers_) ||
        !createDescriptorPool(vkDev, 1, 4, 0, &descriptorPool_) ||
        !createDescriptorSet(vkDev) ||
        !createPipelineLayout(vkDev.device, descriptorSetLayout_, &pipelineLayout_) ||
        !createGraphicsPipeline(vkDev, renderPass_, pipelineLayout_, {vertShaderFile, fragShaderFile}, &graphicsPipeline_))
    {
        printf("Failed to create pipeline\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
}

MultiMeshRenderer::~MultiMeshRenderer()
{
    VkDevice device = vkDev.device;

    vkDestroyBuffer(device, storageBuffer_, nullptr);
    vkFreeMemory(device, storageBufferMemory_, nullptr);

    for (size_t i = 0; i < swapchainFramebuffers_.size(); i++)
    {
        vkDestroyBuffer(device, drawDataBuffers_[i], nullptr);
        vkFreeMemory(device, drawDataBuffersMemory_[i], nullptr);

        vkDestroyBuffer(device, countBuffers_[i], nullptr);
        vkFreeMemory(device, countBuffersMemory_[i], nullptr);

        vkDestroyBuffer(device, indirectBuffers_[i], nullptr);
        vkFreeMemory(device, indirectBuffersMemory_[i], nullptr);
    }

    vkDestroyBuffer(device, materialBuffer_, nullptr);
    vkFreeMemory(device, materialBufferMemory_, nullptr);

    destroyVulkanImage(device, depthTexture_);
}
