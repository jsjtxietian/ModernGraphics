#pragma once

#include "RendererBase.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>

using glm::mat4;
using glm::vec3;
using glm::vec4;

// contains a CPU-accessible list of 3D lines defined by two
// points and a color. For each frame, the user can call the line() method to draw a
// new line that should be rendered in the current frame. To render these lines into the
// framebuffer, we pre-allocate a reasonably large GPU buffer to store line geometry data,
// which we will update every frame
class VulkanCanvas : public RendererBase
{
public:
    explicit VulkanCanvas(VulkanRenderDevice &vkDev, VulkanImage depth);
    virtual ~VulkanCanvas();

    virtual void fillCommandBuffer(VkCommandBuffer commandBuffer, size_t currentImage) override;

    void clear();
    void line(const vec3 &p1, const vec3 &p2, const vec4 &c);
    void plane3d(const vec3 &orig, const vec3 &v1, const vec3 &v2,
                 int n1, int n2, float s1, float s2,
                 const vec4 &color, const vec4 &outlineColor);
    void updateBuffer(VulkanRenderDevice &vkDev, size_t currentImage);
    void updateUniformBuffer(VulkanRenderDevice &vkDev, const glm::mat4 &modelViewProj, float time, uint32_t currentImage);

private:
    struct VertexData
    {
        vec3 position;
        vec4 color;
    };

    struct UniformBuffer
    {
        glm::mat4 mvp;
        float time;
    };

    bool createDescriptorSet(VulkanRenderDevice &vkDev);

    std::vector<VertexData> lines_;

    // 7. Storage Buffer with index and vertex data
    std::vector<VkBuffer> storageBuffer_;
    std::vector<VkDeviceMemory> storageBufferMemory_;

    static constexpr unsigned kMaxLinesCount = 65536;
    static constexpr unsigned kMaxLinesDataSize = kMaxLinesCount * sizeof(VulkanCanvas::VertexData) * 2;
};
