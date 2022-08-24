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

struct SwapchainSupportDetails final
{
	VkSurfaceCapabilitiesKHR capabilities = {};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct ShaderModule final
{
	std::vector<unsigned int> SPIRV;
	VkShaderModule shaderModule = nullptr;
};

void CHECK(bool check, const char *fileName, int lineNumber);

size_t compileShaderFile(const char *file, ShaderModule &shaderModule);

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