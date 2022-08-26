#pragma once

#include <vector>
#include <functional>

#define VK_NO_PROTOTYPES
#include <volk/volk.h>
#include "glslang_c_interface.h"

#define VK_CHECK(value) CHECK(value == VK_SUCCESS, __FILE__, __LINE__);
#define VK_CHECK_RET(value)               \
	if (value != VK_SUCCESS)              \
	{                                     \
		CHECK(false, __FILE__, __LINE__); \
		return value;                     \
	}

struct VulkanInstance final
{
	VkInstance instance;
	VkSurfaceKHR surface;
	VkDebugUtilsMessengerEXT messenger;
	VkDebugReportCallbackEXT reportCallback;
};

struct VulkanRenderDevice final
{
	uint32_t framebufferWidth;
	uint32_t framebufferHeight;

	VkDevice device;
	VkQueue graphicsQueue;
	VkPhysicalDevice physicalDevice;

	uint32_t graphicsFamily;

	VkSwapchainKHR swapchain;
	// ensure that the rendering process waits for the swap chain image to become available
	VkSemaphore semaphore;
	// ensure that the presentation process waits for rendering to have completed
	VkSemaphore renderSemaphore;

	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;

	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
};

struct VulkanImage final
{
	VkImage image = nullptr;
	VkDeviceMemory imageMemory = nullptr;
	VkImageView imageView = nullptr;
};

struct SwapchainSupportDetails final
{
	VkSurfaceCapabilitiesKHR capabilities = {};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

// Aggregate structure for passing around the texture data
struct VulkanTexture final
{
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	VkFormat format;

	VulkanImage image;
	VkSampler sampler;

	// Offscreen buffers require VK_IMAGE_LAYOUT_GENERAL && static textures have VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	VkImageLayout desiredLayout;
};

struct ShaderModule final
{
	std::vector<unsigned int> SPIRV;
	VkShaderModule shaderModule = nullptr;
};

struct RenderPassCreateInfo final
{
	bool clearColor_ = false;
	bool clearDepth_ = false;
	uint8_t flags_ = 0;
};

enum eRenderPassBit : uint8_t
{
	// clear the attachment
	eRenderPassBit_First = 0x01,
	// transition to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	eRenderPassBit_Last = 0x02,
	// transition to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	eRenderPassBit_Offscreen = 0x04,
	// keep VK_IMAGE_LAYOUT_*_ATTACHMENT_OPTIMAL
	eRenderPassBit_OffscreenInternal = 0x08,
};

void CHECK(bool check, const char *fileName, int lineNumber);
bool setupDebugCallbacks(VkInstance instance, VkDebugUtilsMessengerEXT *messenger, VkDebugReportCallbackEXT *reportCallback);

size_t compileShaderFile(const char *file, ShaderModule &shaderModule);
VkResult createShaderModule(VkDevice device, ShaderModule *shader, const char *fileName);

void createInstance(VkInstance *instance);
VkResult createDevice(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures deviceFeatures, uint32_t graphicsFamily, VkDevice *device);
VkResult findSuitablePhysicalDevice(VkInstance instance, std::function<bool(VkPhysicalDevice)> selector, VkPhysicalDevice *physicalDevice);
uint32_t findQueueFamilies(VkPhysicalDevice device, VkQueueFlags desiredFlags);

SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
uint32_t chooseSwapImageCount(const VkSurfaceCapabilitiesKHR &capabilities);
VkResult createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t graphicsFamily, uint32_t width, uint32_t height, VkSwapchainKHR *swapchain, bool supportScreenshots = false);
size_t createSwapchainImages(VkDevice device, VkSwapchainKHR swapchain, std::vector<VkImage> &swapchainImages, std::vector<VkImageView> &swapchainImageViews);
bool createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView *imageView, VkImageViewType viewType, uint32_t layerCount, uint32_t mipLevels);

VkResult createSemaphore(VkDevice device, VkSemaphore *outSemaphore);

bool initVulkanRenderDevice(VulkanInstance &vk, VulkanRenderDevice &vkDev, uint32_t width, uint32_t height, std::function<bool(VkPhysicalDevice)> selector, VkPhysicalDeviceFeatures deviceFeatures);
void destroyVulkanRenderDevice(VulkanRenderDevice &vkDev);
void destroyVulkanInstance(VulkanInstance &vk);

bool createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory);
VkCommandBuffer beginSingleTimeCommands(VulkanRenderDevice &vkDev);
void endSingleTimeCommands(VulkanRenderDevice &vkDev, VkCommandBuffer commandBuffer);
void copyBuffer(VulkanRenderDevice &vkDev, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

bool createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory, VkImageCreateFlags flags = 0, uint32_t mipLevels = 1);
bool createTextureImageFromData(VulkanRenderDevice &vkDev,
								VkImage &textureImage, VkDeviceMemory &textureImageMemory,
								void *imageData, uint32_t texWidth, uint32_t texHeight,
								VkFormat texFormat,
								uint32_t layerCount = 1, VkImageCreateFlags flags = 0);
bool updateTextureImage(VulkanRenderDevice &vkDev, VkImage &textureImage, VkDeviceMemory &textureImageMemory, uint32_t texWidth, uint32_t texHeight, VkFormat texFormat, uint32_t layerCount, const void *imageData, VkImageLayout sourceImageLayout = VK_IMAGE_LAYOUT_UNDEFINED);
void transitionImageLayout(VulkanRenderDevice &vkDev, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layerCount = 1, uint32_t mipLevels = 1);

size_t allocateVertexBuffer(VulkanRenderDevice &vkDev, VkBuffer *storageBuffer, VkDeviceMemory *storageBufferMemory, size_t vertexDataSize, const void *vertexData, size_t indexDataSize, const void *indexData);
bool createTexturedVertexBuffer(VulkanRenderDevice &vkDev, const char *filename, VkBuffer *storageBuffer, VkDeviceMemory *storageBufferMemory, size_t *vertexBufferSize, size_t *indexBufferSize);

bool createDescriptorPool(VulkanRenderDevice &vkDev, uint32_t uniformBufferCount, uint32_t storageBufferCount, uint32_t samplerCount, VkDescriptorPool *descriptorPool);

bool createPipelineLayout(VkDevice device, VkDescriptorSetLayout dsLayout, VkPipelineLayout *pipelineLayout);
bool createColorAndDepthRenderPass(VulkanRenderDevice &device, bool useDepth, VkRenderPass *renderPass, const RenderPassCreateInfo &ci, VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM);
bool createGraphicsPipeline(
	VulkanRenderDevice &vkDev,
	VkRenderPass renderPass, VkPipelineLayout pipelineLayout,
	const std::vector<const char *> &shaderFiles,
	VkPipeline *pipeline,
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST /* defaults to triangles*/,
	bool useDepth = true,
	bool useBlending = true,
	bool dynamicScissorState = false,
	int32_t customWidth = -1,
	int32_t customHeight = -1,
	uint32_t numPatchControlPoints = 0);

inline VkPipelineShaderStageCreateInfo shaderStageInfo(VkShaderStageFlagBits shaderStage, ShaderModule &module, const char *entryPoint)
{
	return VkPipelineShaderStageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = shaderStage,
		.module = module.shaderModule,
		.pName = entryPoint,
		.pSpecializationInfo = nullptr};
}