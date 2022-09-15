#include "Utils/Utils.h"
#include "UtilsVulkan.h"
#include "Utils/Bitmap.h"

#include "StandAlone/ResourceLimits.h"

#define VK_NO_PROTOTYPES
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/version.h>

using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

#include <cstdio>
#include <cstdlib>

#include <array>

// ============================ debug capabilities ====================================

void CHECK(bool check, const char *fileName, int lineNumber)
{
	if (!check)
	{
		printf("CHECK() failed at %s:%i\n", fileName, lineNumber);
		assert(false);
		exit(EXIT_FAILURE);
	}
}

// tracking all possible errors and warnings that may be produced by the validation layer.
// prints all messages coming into the system console
static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT Severity,
	VkDebugUtilsMessageTypeFlagsEXT Type,
	const VkDebugUtilsMessengerCallbackDataEXT *CallbackData,
	void *UserData)
{
	printf("Validation layer: %s\n", CallbackData->pMessage);
	return VK_FALSE;
}

// more elaborate and provides information about an object that's causing an error or a warning.
// Some performance warnings are silenced to make the debug output more readable
static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugReportCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objectType,
	uint64_t object,
	size_t location,
	int32_t messageCode,
	const char *pLayerPrefix,
	const char *pMessage,
	void *UserData)
{
	// https://github.com/zeux/niagara/blob/master/src/device.cpp   [ignoring performance warnings]
	// This silences warnings like "For optimal performance image layout should be VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL instead of GENERAL."
	if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		return VK_FALSE;

	printf("Debug callback (%s): %s\n", pLayerPrefix, pMessage);
	return VK_FALSE;
}

// associate these callbacks with a Vulkan instance, we should create two more
// objects, messenger and reportCallback; be destroyed at the end of the application
bool setupDebugCallbacks(VkInstance instance, VkDebugUtilsMessengerEXT *messenger, VkDebugReportCallbackEXT *reportCallback)
{
	{
		const VkDebugUtilsMessengerCreateInfoEXT ci = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = &VulkanDebugCallback,
			.pUserData = nullptr};

		VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &ci, nullptr, messenger));
	}
	{
		const VkDebugReportCallbackCreateInfoEXT ci = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
			.pNext = nullptr,
			.flags =
				VK_DEBUG_REPORT_WARNING_BIT_EXT |
				VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
				VK_DEBUG_REPORT_ERROR_BIT_EXT |
				VK_DEBUG_REPORT_DEBUG_BIT_EXT,
			.pfnCallback = &VulkanDebugReportCallback,
			.pUserData = nullptr};

		VK_CHECK(vkCreateDebugReportCallbackEXT(instance, &ci, nullptr, reportCallback));
	}

	return true;
}

// add symbolic names to Vulkan objects. This is useful for debugging Vulkan applications
// in situations where the validation layer reports object handles
// bool setVkObjectName(VulkanRenderDevice &vkDev, void *object, VkObjectType objType, const char *name)
// {
// 	VkDebugUtilsObjectNameInfoEXT nameInfo = {
// 		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
// 		.pNext = nullptr,
// 		.objectType = objType,
// 		.objectHandle = (uint64_t)object,
// 		.pObjectName = name};

// 	return (vkSetDebugUtilsObjectNameEXT(vkDev.device, &nameInfo) == VK_SUCCESS);
// }

// ============================ instances & devices ====================================

// Using the Vulkan instance, we can acquire a list of physical devices with the
// required properties
void createInstance(VkInstance *instance)
{
	// allow us to enable debugging output for every Vulkan call.
	// The only layer we will be using is the debugging layer
	const std::vector<const char *> ValidationLayers =
		{
			"VK_LAYER_KHRONOS_validation"};

	const std::vector<const char *> exts =
	{
		"VK_KHR_surface",
#if defined(_WIN32)
		"VK_KHR_win32_surface",
#endif
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME
		/* for indexed textures */
		,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
	};

	const VkApplicationInfo appinfo =
		{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = nullptr,
			.pApplicationName = "Vulkan",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "No Engine",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = VK_API_VERSION_1_1};

	const VkInstanceCreateInfo createInfo =
		{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.pApplicationInfo = &appinfo,
			.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size()),
			.ppEnabledLayerNames = ValidationLayers.data(),
			.enabledExtensionCount = static_cast<uint32_t>(exts.size()),
			.ppEnabledExtensionNames = exts.data()};

	VK_CHECK(vkCreateInstance(&createInfo, nullptr, instance));

	// ask the Volk library to retrieve all the Vulkan API function pointers
	// for all the extensions that are available for the created VkInstance
	volkLoadInstance(*instance);
}

// Once we have a Vulkan instance ready and the graphics queue index set up with the
// selected physical device, we can create a logical representation of a GPU. Vulkan treats
// all devices as a collection of queues and memory heaps. To use a device for rendering,
// we need to specify a queue that can execute graphics-related commands, and a physical
// device that has such a queue.

// device features (for example, geometry shader support)
VkResult createDevice(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures deviceFeatures,
					  uint32_t graphicsFamily, VkDevice *device)
{
	const std::vector<const char *> extensions =
		{
			//  allows us to present rendered frames on the screen.
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		};

	// only use a single graphics queue that has maximum priority
	const float queuePriority = 1.0f;

	const VkDeviceQueueCreateInfo qci =
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queueFamilyIndex = graphicsFamily,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority};

	// To create something in Vulkan, we should fill in a ...CreateInfo structure
	// and pass all the required object properties to an appropriate vkCreate...() function.

	const VkDeviceCreateInfo ci =
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &qci,
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = nullptr,
			.enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
			.ppEnabledExtensionNames = extensions.data(),
			.pEnabledFeatures = &deviceFeatures};

	return vkCreateDevice(physicalDevice, &ci, nullptr, device);
}

// The createDevice() function expects a reference to a physical graphics-capable
// device. The following function finds such a device
VkResult findSuitablePhysicalDevice(VkInstance instance, std::function<bool(VkPhysicalDevice)> selector,
									VkPhysicalDevice *physicalDevice)
{
	uint32_t deviceCount = 0;
	VK_CHECK_RET(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));

	if (!deviceCount)
		return VK_ERROR_INITIALIZATION_FAILED;

	std::vector<VkPhysicalDevice> devices(deviceCount);
	VK_CHECK_RET(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

	for (const auto &device : devices)
	{
		if (selector(device))
		{
			*physicalDevice = device;
			return VK_SUCCESS;
		}
	}

	return VK_ERROR_INITIALIZATION_FAILED;
}

// Once we have a physical device reference, we will get a list of its queues. Here, we
// must check for the one with our desired capability flags
uint32_t findQueueFamilies(VkPhysicalDevice device, VkQueueFlags desiredFlags)
{
	uint32_t familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);

	std::vector<VkQueueFamilyProperties> families(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

	for (uint32_t i = 0; i != families.size(); i++)
		if (families[i].queueCount > 0 && families[i].queueFlags & desiredFlags)
			return i;

	return 0;
}

bool isDeviceSuitable(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	const bool isDiscreteGPU = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
	const bool isIntegratedGPU = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
	const bool isGPU = isDiscreteGPU || isIntegratedGPU;

	return isGPU && deviceFeatures.geometryShader;
}

// ============================ swapchain ====================================
// Normally, each frame is rendered as an offscreen image. Once the rendering process is
// complete, the offscreen image should be made visible. An object that holds a collection of
// available offscreen images – or, more specifically, a queue of rendered images waiting to
// be presented to the screen – is called a swap chain. In OpenGL, presenting an offscreen
// buffer to the visible area of a window is performed using system-dependent functions,
// namely wglSwapBuffers() on Windows, and automatically on macOS. Using Vulkan,
// we need to select a sequencing algorithm for the swap chain images. Also, the
// operation that presents an image to the display is no different from any other operation,
// such as rendering a collection of triangles. The Vulkan API object model treats each
// graphics device as a collection of command queues where rendering, computation, or
// transfer operations can be enqueued.

SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapchainSupportDetails details;
	// Query the basic capabilities of a surface
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	// Get the number of available surface formats. Allocate the storage to hold them
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	// Retrieve the supported presentation modes in a similar way
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

// choosing the required surface format.
// use a hardcoded value here for the RGBA 8-bit per channel format with the sRGB color space
VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
{
	return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
}

// select presentation mode. The preferred presentation mode is
// VK_PRESENT_MODE_MAILBOX_KHR, which specifies that the Vulkan presentation
// system should wait for the next vertical blanking period to update the current
// image. Visual tearing will not be observed in this case. However, it's not guaranteed
// that this presentation mode will be supported. In this situation, we can always fall
// back to VK_PRESENT_MODE_FIFO_KHR. The differences between all possible
// presentation modes are described in the Vulkan specification at
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPresentModeKHR.html
VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes)
{
	for (const auto mode : availablePresentModes)
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			return mode;

	// FIFO will always be supported
	return VK_PRESENT_MODE_FIFO_KHR;
}

// choose the number of images in the swap chain object. It is based on
// the surface capabilities we retrieved earlier. Instead of
// using minImageCount directly, we will request one additional image to make sure
// we are not waiting on the GPU to complete any operations
uint32_t chooseSwapImageCount(const VkSurfaceCapabilitiesKHR &capabilities)
{
	const uint32_t imageCount = capabilities.minImageCount + 1;
	const bool imageCountExceeded = capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount;
	return imageCountExceeded ? capabilities.maxImageCount : imageCount;
}

VkResult createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice,
						 VkSurfaceKHR surface, uint32_t graphicsFamily,
						 uint32_t width, uint32_t height,
						 VkSwapchainKHR *swapchain, bool supportScreenshots)
{
	auto swapchainSupport = querySwapchainSupport(physicalDevice, surface);
	auto surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
	auto presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);

	// initial example will not use a depth buffer, so only VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
	// will be used. The VK_IMAGE_USAGE_TRANSFER_DST_BIT flag specifies that the
	// image can be used as the destination of a transfer command
	const VkSwapchainCreateInfoKHR ci =
		{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.flags = 0,
			.surface = surface,
			.minImageCount = chooseSwapImageCount(swapchainSupport.capabilities),
			.imageFormat = surfaceFormat.format,
			.imageColorSpace = surfaceFormat.colorSpace,
			.imageExtent = {.width = width, .height = height},
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | (supportScreenshots ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0u),
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 1,
			.pQueueFamilyIndices = &graphicsFamily,
			.preTransform = swapchainSupport.capabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = presentMode,
			.clipped = VK_TRUE,
			.oldSwapchain = VK_NULL_HANDLE};

	return vkCreateSwapchainKHR(device, &ci, nullptr, swapchain);
}

// Once the swapchain object has been created, we should retrieve the actual images from the swapchain
size_t createSwapchainImages(
	VkDevice device, VkSwapchainKHR swapchain,
	std::vector<VkImage> &swapchainImages,
	std::vector<VkImageView> &swapchainImageViews)
{
	uint32_t imageCount = 0;
	VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));

	swapchainImages.resize(imageCount);
	swapchainImageViews.resize(imageCount);

	VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data()));

	for (unsigned i = 0; i < imageCount; i++)
		if (!createImageView(device, swapchainImages[i], VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, &swapchainImageViews[i]))
			exit(0);

	return static_cast<size_t>(imageCount);
}

// creates an image view for us
bool createImageView(VkDevice device, VkImage image, VkFormat format,
					 VkImageAspectFlags aspectFlags, VkImageView *imageView,
					 VkImageViewType viewType,
					 uint32_t layerCount, uint32_t mipLevels)
{
	const VkImageViewCreateInfo viewInfo =
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.image = image,
			.viewType = viewType,
			.format = format,
			.subresourceRange =
				{
					.aspectMask = aspectFlags,
					.baseMipLevel = 0,
					.levelCount = mipLevels,
					.baseArrayLayer = 0,
					.layerCount = layerCount}};

	return (vkCreateImageView(device, &viewInfo, nullptr, imageView) == VK_SUCCESS);
}

// ================ Tracking and cleaning up Vulkan objects ==============

VkResult createSemaphore(VkDevice device, VkSemaphore *outSemaphore)
{
	const VkSemaphoreCreateInfo ci =
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

	return vkCreateSemaphore(device, &ci, nullptr, outSemaphore);
}

bool initVulkanRenderDevice(VulkanInstance &vk, VulkanRenderDevice &vkDev, uint32_t width, uint32_t height, std::function<bool(VkPhysicalDevice)> selector, VkPhysicalDeviceFeatures deviceFeatures)
{
	vkDev.framebufferWidth = width;
	vkDev.framebufferHeight = height;

	VK_CHECK(findSuitablePhysicalDevice(vk.instance, selector, &vkDev.physicalDevice));
	vkDev.graphicsFamily = findQueueFamilies(vkDev.physicalDevice, VK_QUEUE_GRAPHICS_BIT);
	VK_CHECK(createDevice(vkDev.physicalDevice, deviceFeatures, vkDev.graphicsFamily, &vkDev.device));

	vkGetDeviceQueue(vkDev.device, vkDev.graphicsFamily, 0, &vkDev.graphicsQueue);
	if (vkDev.graphicsQueue == nullptr)
		exit(EXIT_FAILURE);

	VkBool32 presentSupported = 0;
	vkGetPhysicalDeviceSurfaceSupportKHR(vkDev.physicalDevice, vkDev.graphicsFamily, vk.surface, &presentSupported);
	if (!presentSupported)
		exit(EXIT_FAILURE);

	VK_CHECK(createSwapchain(vkDev.device, vkDev.physicalDevice, vk.surface, vkDev.graphicsFamily, width, height, &vkDev.swapchain));
	const size_t imageCount = createSwapchainImages(vkDev.device, vkDev.swapchain, vkDev.swapchainImages, vkDev.swapchainImageViews);
	vkDev.commandBuffers.resize(imageCount);

	VK_CHECK(createSemaphore(vkDev.device, &vkDev.semaphore));
	VK_CHECK(createSemaphore(vkDev.device, &vkDev.renderSemaphore));

	// command pool is necessary to allocate command buffers
	const VkCommandPoolCreateInfo cpi =
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = 0,
			.queueFamilyIndex = vkDev.graphicsFamily};

	VK_CHECK(vkCreateCommandPool(vkDev.device, &cpi, nullptr, &vkDev.commandPool));

	// allocate one command buffer per swap chain image:
	const VkCommandBufferAllocateInfo ai =
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = vkDev.commandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = static_cast<uint32_t>(vkDev.swapchainImages.size()),
		};

	VK_CHECK(vkAllocateCommandBuffers(vkDev.device, &ai, &vkDev.commandBuffers[0]));
	return true;
}

void destroyVulkanRenderDevice(VulkanRenderDevice &vkDev)
{
	for (size_t i = 0; i < vkDev.swapchainImages.size(); i++)
		vkDestroyImageView(vkDev.device, vkDev.swapchainImageViews[i], nullptr);

	vkDestroySwapchainKHR(vkDev.device, vkDev.swapchain, nullptr);
	vkDestroyCommandPool(vkDev.device, vkDev.commandPool, nullptr);
	vkDestroySemaphore(vkDev.device, vkDev.semaphore, nullptr);
	vkDestroySemaphore(vkDev.device, vkDev.renderSemaphore, nullptr);
	vkDestroyDevice(vkDev.device, nullptr);
}

void destroyVulkanInstance(VulkanInstance &vk)
{
	vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
	vkDestroyDebugReportCallbackEXT(vk.instance, vk.reportCallback, nullptr);
	vkDestroyDebugUtilsMessengerEXT(vk.instance, vk.messenger, nullptr);
	vkDestroyInstance(vk.instance, nullptr);
}

// ============================ Buffers ====================================

// Uploading data into GPU buffers is an operation that is executed, just like any other
// Vulkan operation, using command buffers. This means we need to have a command
// queue that's capable of performing transfer operations.
// transfer geometry and image data to Vulkan buffers, as well as to convert data into different formats

// selects an appropriate heap type on the GPU, based on the required properties and a filter
uint32_t findMemoryType(VkPhysicalDevice device, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(device, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	return 0xFFFFFFFF;
}

// create a buffer object and an associated device memory region. We will use this function
// to create uniform, shader storage, and other types of buffers. The exact buffer usage is
// specified by the usage parameter. The access permissions for the memory block are specified by properties flags:
bool createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
				  VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
				  VkBuffer &buffer, VkDeviceMemory &bufferMemory)
{
	const VkBufferCreateInfo bufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr};

	VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer));

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	const VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)};

	VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory));

	vkBindBufferMemory(device, buffer, bufferMemory, 0);

	return true;
}

// upload some data into a GPU buffer
void copyBuffer(VulkanRenderDevice &vkDev, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(vkDev);

	const VkBufferCopy copyRegion = {
		.srcOffset = 0,
		.dstOffset = 0,
		.size = size};

	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(vkDev, commandBuffer);
}

// creates a temporary command buffer that contains transfer commands
VkCommandBuffer beginSingleTimeCommands(VulkanRenderDevice &vkDev)
{
	VkCommandBuffer commandBuffer;

	const VkCommandBufferAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = vkDev.commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1};

	vkAllocateCommandBuffers(vkDev.device, &allocInfo, &commandBuffer);

	const VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr};

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

// submits the command buffer to the graphics queue and waits for the entire operation to complete
void endSingleTimeCommands(VulkanRenderDevice &vkDev, VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	const VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 0,
		.pWaitSemaphores = nullptr,
		.pWaitDstStageMask = nullptr,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer,
		.signalSemaphoreCount = 0,
		.pSignalSemaphores = nullptr};

	vkQueueSubmit(vkDev.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(vkDev.graphicsQueue);

	vkFreeCommandBuffers(vkDev.device, vkDev.commandPool, 1, &commandBuffer);
}

// ============================ texture ====================================
// several functions for creating, destroying, and modifying texture objects on the GPU using the Vulkan API

// A Vulkan image is another type of buffer that's designed to store a 1D, 2D, or 3D image, or even an array of these images.
// An image is just a region in memory. Its internal structure, such as the number of layers for a cube map or the number
// of mipmap levels is has, is specified in the VkImageView object.
// The createImage() function is similar to createBuffer(); vkBindImageMemory() is used instead of vkBindBufferMemory()
bool createImage(VkDevice device, VkPhysicalDevice physicalDevice,
				 uint32_t width, uint32_t height, VkFormat format,
				 VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
				 VkImage &image, VkDeviceMemory &imageMemory, VkImageCreateFlags flags, uint32_t mipLevels)
{
	const VkImageCreateInfo imageInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = flags,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = VkExtent3D{.width = width, .height = height, .depth = 1},
		.mipLevels = mipLevels,
		.arrayLayers = (uint32_t)((flags == VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) ? 6 : 1),
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = tiling,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

	VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &image));

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	const VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)};

	VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory));

	vkBindImageMemory(device, image, imageMemory, 0);
	return true;
}

// create a sampler that allows our fragment shaders to fetch texels from the image
bool createTextureSampler(VkDevice device, VkSampler *sampler, VkFilter minFilter, VkFilter maxFilter, VkSamplerAddressMode addressMode)
{
	const VkSamplerCreateInfo samplerInfo = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = addressMode, // VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = addressMode, // VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = addressMode, // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_FALSE,
		.maxAnisotropy = 1,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE};

	return (vkCreateSampler(device, &samplerInfo, nullptr, sampler) == VK_SUCCESS);
};

// upload the data to an image
void copyBufferToImage(VulkanRenderDevice &vkDev, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(vkDev);

	const VkBufferImageCopy region = {
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = VkImageSubresourceLayers{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = layerCount},
		.imageOffset = VkOffset3D{.x = 0, .y = 0, .z = 0},
		.imageExtent = VkExtent3D{.width = width, .height = height, .depth = 1}};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endSingleTimeCommands(vkDev, commandBuffer);
}

void destroyVulkanImage(VkDevice device, VulkanImage &image)
{
	vkDestroyImageView(device, image.imageView, nullptr);
	vkDestroyImage(device, image.image, nullptr);
	vkFreeMemory(device, image.imageMemory, nullptr);
}

void destroyVulkanTexture(VkDevice device, VulkanTexture &texture)
{
	destroyVulkanImage(device, texture.image);
	vkDestroySampler(device, texture.sampler, nullptr);
}

// The GPU may need to reorganize texture data internally for faster access. This
// reorganization happens when we insert a pipeline barrier operation into the graphics
// command queue. The following lengthy function handles the necessary format
// transitions for 2D textures and depth buffers. This function is necessary if you want
// to resolve all the validation layer warnings for swap chain images.
// This will also serve as a good starting point for cleaning up validation layer warnings:
void transitionImageLayout(VulkanRenderDevice &vkDev, VkImage image, VkFormat format,
						   VkImageLayout oldLayout, VkImageLayout newLayout,
						   uint32_t layerCount, uint32_t mipLevels)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(vkDev);

	transitionImageLayoutCmd(commandBuffer, image, format, oldLayout, newLayout, layerCount, mipLevels);

	endSingleTimeCommands(vkDev, commandBuffer);
}

//  presenting all the use cases for the image transitions that are necessary
void transitionImageLayoutCmd(VkCommandBuffer commandBuffer, VkImage image, VkFormat format,
							  VkImageLayout oldLayout, VkImageLayout newLayout,
							  uint32_t layerCount, uint32_t mipLevels)
{
	VkImageMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = 0,
		.dstAccessMask = 0,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = VkImageSubresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = mipLevels,
			.baseArrayLayer = 0,
			.layerCount = layerCount}};

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
		(format == VK_FORMAT_D16_UNORM) ||
		(format == VK_FORMAT_X8_D24_UNORM_PACK32) ||
		(format == VK_FORMAT_D32_SFLOAT) ||
		(format == VK_FORMAT_S8_UINT) ||
		(format == VK_FORMAT_D16_UNORM_S8_UINT) ||
		(format == VK_FORMAT_D24_UNORM_S8_UINT))
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (hasStencilComponent(format))
		{
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	/* Convert back from read-only to updateable */
	else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	/* Convert from updateable texture to shader read-only */
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	/* Convert depth texture from undefined state to depth-stencil buffer */
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}

	/* Wait for render pass to complete */
	else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = 0; // VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = 0;
		/*
				sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		///		destinationStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
				destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		*/
		sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	/* Convert back from read-only to color attachment */
	else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	/* Convert from updateable texture to shader read-only */
	else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	/* Convert back from read-only to depth attachment */
	else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	}
	/* Convert from updateable depth texture to shader read-only */
	else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier);
}

// accepts the required format features and tiling options
// and returns the first suitable format that satisfies these requirements
VkFormat findSupportedFormat(VkPhysicalDevice device, const std::vector<VkFormat> &candidates,
							 VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(device, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}

	printf("failed to find supported format!\n");
	exit(0);
}

// find the requested depth format
VkFormat findDepthFormat(VkPhysicalDevice device)
{
	return findSupportedFormat(
		device,
		{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

// check if it has a suitable stencil component
bool hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

bool createDepthResources(VulkanRenderDevice &vkDev, uint32_t width,
						  uint32_t height, VulkanImage &depth)
{
	VkFormat depthFormat = findDepthFormat(vkDev.physicalDevice);

	if (!createImage(vkDev.device, vkDev.physicalDevice, width, height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth.image, depth.imageMemory))
		return false;

	if (!createImageView(vkDev.device, depth.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, &depth.imageView))
		return false;

	transitionImageLayout(vkDev, depth.image, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	return true;
}

// load a 2D texture from an image file to a Vulkan image.
// This function uses a staging buffer in a similar way to the vertex buffer creation function
bool createTextureImage(VulkanRenderDevice &vkDev, const char *filename, VkImage &textureImage, VkDeviceMemory &textureImageMemory, uint32_t *outTexWidth, uint32_t *outTexHeight)
{
	int texWidth, texHeight, texChannels;
	stbi_uc *pixels = stbi_load(filename, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels)
	{
		printf("Failed to load [%s] texture\n", filename);
		fflush(stdout);
		return false;
	}

	bool result = createTextureImageFromData(vkDev, textureImage, textureImageMemory,
											 pixels, texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM);

	stbi_image_free(pixels);

	if (outTexWidth && outTexHeight)
	{
		*outTexWidth = (uint32_t)texWidth;
		*outTexHeight = (uint32_t)texHeight;
	}

	return result;
}

bool createTextureImageFromData(VulkanRenderDevice &vkDev,
								VkImage &textureImage, VkDeviceMemory &textureImageMemory,
								void *imageData, uint32_t texWidth, uint32_t texHeight,
								VkFormat texFormat,
								uint32_t layerCount, VkImageCreateFlags flags)
{
	createImage(vkDev.device, vkDev.physicalDevice, texWidth, texHeight, texFormat,
				VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory, flags);

	return updateTextureImage(vkDev, textureImage, textureImageMemory,
							  texWidth, texHeight, texFormat,
							  layerCount, imageData);
}

bool updateTextureImage(VulkanRenderDevice &vkDev, VkImage &textureImage, VkDeviceMemory &textureImageMemory,
						uint32_t texWidth, uint32_t texHeight, VkFormat texFormat,
						uint32_t layerCount, const void *imageData, VkImageLayout sourceImageLayout)
{
	uint32_t bytesPerPixel = bytesPerTexFormat(texFormat);

	VkDeviceSize layerSize = texWidth * texHeight * bytesPerPixel;
	VkDeviceSize imageSize = layerSize * layerCount;

	// A staging buffer is necessary to upload texture data into the GPU via memory
	// mapping. This buffer should be declared as VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(vkDev.device, vkDev.physicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	uploadBufferData(vkDev, stagingBufferMemory, 0, imageData, imageSize);

	// The actual image is located in the device memory and can't be accessed directly from the host
	transitionImageLayout(vkDev, textureImage, texFormat, sourceImageLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layerCount);
	copyBufferToImage(vkDev, stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), layerCount);
	transitionImageLayout(vkDev, textureImage, texFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, layerCount);

	vkDestroyBuffer(vkDev.device, stagingBuffer, nullptr);
	vkFreeMemory(vkDev.device, stagingBufferMemory, nullptr);

	return true;
}

bool createUniformBuffer(VulkanRenderDevice &vkDev, VkBuffer &buffer,
						 VkDeviceMemory &bufferMemory, VkDeviceSize bufferSize)
{
	return createBuffer(vkDev.device, vkDev.physicalDevice, bufferSize,
						VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						buffer, bufferMemory);
}

void uploadBufferData(VulkanRenderDevice &vkDev, const VkDeviceMemory &bufferMemory, VkDeviceSize deviceOffset, const void *data, const size_t dataSize)
{
	void *mappedData = nullptr;
	vkMapMemory(vkDev.device, bufferMemory, deviceOffset, dataSize, 0, &mappedData);
	memcpy(mappedData, data, dataSize);
	vkUnmapMemory(vkDev.device, bufferMemory);
}

bool createColorAndDepthFramebuffers(VulkanRenderDevice &vkDev, VkRenderPass renderPass, VkImageView depthImageView, std::vector<VkFramebuffer> &swapchainFramebuffers)
{
	swapchainFramebuffers.resize(vkDev.swapchainImageViews.size());

	for (size_t i = 0; i < vkDev.swapchainImages.size(); i++)
	{
		std::array<VkImageView, 2> attachments = {
			vkDev.swapchainImageViews[i],
			depthImageView};

		const VkFramebufferCreateInfo framebufferInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = renderPass,
			.attachmentCount = static_cast<uint32_t>((depthImageView == VK_NULL_HANDLE) ? 1 : 2),
			.pAttachments = attachments.data(),
			.width = vkDev.framebufferWidth,
			.height = vkDev.framebufferHeight,
			.layers = 1};

		VK_CHECK(vkCreateFramebuffer(vkDev.device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]));
	}

	return true;
}

uint32_t bytesPerTexFormat(VkFormat fmt)
{
	switch (fmt)
	{
	case VK_FORMAT_R8_SINT:
	case VK_FORMAT_R8_UNORM:
		return 1;
	case VK_FORMAT_R16_SFLOAT:
		return 2;
	case VK_FORMAT_R16G16_SFLOAT:
		return 4;
	case VK_FORMAT_R16G16_SNORM:
		return 4;
	case VK_FORMAT_B8G8R8A8_UNORM:
		return 4;
	case VK_FORMAT_R8G8B8A8_UNORM:
		return 4;
	case VK_FORMAT_R16G16B16A16_SFLOAT:
		return 4 * sizeof(uint16_t);
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		return 4 * sizeof(float);
	default:
		break;
	}
	return 0;
}

// ============================ mesh geometry data ====================================

// loads a mesh via Assimp from a file into a Vulkan shader storage buffer.
// The loading part is identical to OpenGL
bool createTexturedVertexBuffer(VulkanRenderDevice &vkDev, const char *filename,
								VkBuffer *storageBuffer, VkDeviceMemory *storageBufferMemory,
								size_t *vertexBufferSize, size_t *indexBufferSize)
{
	const aiScene *scene = aiImportFile(filename, aiProcess_Triangulate);

	if (!scene || !scene->HasMeshes())
	{
		printf("Unable to load %s\n", filename);
		exit(255);
	}

	const aiMesh *mesh = scene->mMeshes[0];
	struct VertexData
	{
		vec3 pos;
		vec2 tc;
	};

	std::vector<VertexData> vertices;
	for (unsigned i = 0; i != mesh->mNumVertices; i++)
	{
		const aiVector3D v = mesh->mVertices[i];
		const aiVector3D t = mesh->mTextureCoords[0][i];
		vertices.push_back({.pos = vec3(v.x, v.z, v.y), .tc = vec2(t.x, t.y)});
	}

	std::vector<unsigned int> indices;
	for (unsigned i = 0; i != mesh->mNumFaces; i++)
	{
		for (unsigned j = 0; j != 3; j++)
			indices.push_back(mesh->mFaces[i].mIndices[j]);
	}
	aiReleaseImport(scene);

	// need a staging buffer to upload the data into the GPU memory
	*vertexBufferSize = sizeof(VertexData) * vertices.size();
	*indexBufferSize = sizeof(unsigned int) * indices.size();

	allocateVertexBuffer(vkDev, storageBuffer, storageBufferMemory, *vertexBufferSize, vertices.data(), *indexBufferSize, indices.data());

	return true;
}

size_t allocateVertexBuffer(VulkanRenderDevice &vkDev, VkBuffer *storageBuffer,
							VkDeviceMemory *storageBufferMemory, size_t vertexDataSize, const void *vertexData,
							size_t indexDataSize, const void *indexData)
{
	VkDeviceSize bufferSize = vertexDataSize + indexDataSize;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(vkDev.device, vkDev.physicalDevice, bufferSize,
				 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				 stagingBuffer, stagingBufferMemory);

	void *data;
	vkMapMemory(vkDev.device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertexData, vertexDataSize);
	memcpy((unsigned char *)data + vertexDataSize, indexData, indexDataSize);
	vkUnmapMemory(vkDev.device, stagingBufferMemory);

	createBuffer(vkDev.device, vkDev.physicalDevice, bufferSize,
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, *storageBuffer, *storageBufferMemory);

	copyBuffer(vkDev, stagingBuffer, *storageBuffer, bufferSize);

	vkDestroyBuffer(vkDev.device, stagingBuffer, nullptr);
	vkFreeMemory(vkDev.device, stagingBufferMemory, nullptr);

	return bufferSize;
}

// ============================ descriptor sets ====================================

// A descriptor set object is an object that holds a set of descriptors. Think of each descriptor
// as a handle or a pointer to a resource. We can think of a descriptor set as everything that
// is "external" to the graphics pipeline or as a resource set. Also, the descriptor set is the
// only way to specify which textures and buffers can be used by the shader modules in the
// pipeline. The Vulkan API does not allow you to bind individual resources in shaders; they
// must be grouped into sets, and only a limited number of descriptor sets can be bound to
// a given pipeline. This design decision was mostly due to the limitations of some legacy
// hardware, which must be able to run Vulkan applications.

// Descriptor sets cannot be created directly. They must come from a descriptor pool, which
// is similar to the command pool we allocated

// a function to create a descriptor pool. The allocated descriptor
// pool must contain enough items for each texture sample and buffer that's used. We
// must also multiply these numbers by the number of swap chain images since,
// we will allocate one descriptor set to each swap chain image
bool createDescriptorPool(VulkanRenderDevice &vkDev, uint32_t uniformBufferCount, uint32_t storageBufferCount, uint32_t samplerCount, VkDescriptorPool *descriptorPool)
{
	const uint32_t imageCount = static_cast<uint32_t>(vkDev.swapchainImages.size());

	std::vector<VkDescriptorPoolSize> poolSizes;

	if (uniformBufferCount)
		poolSizes.push_back(VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = imageCount * uniformBufferCount});

	if (storageBufferCount)
		poolSizes.push_back(VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = imageCount * storageBufferCount});

	if (samplerCount)
		poolSizes.push_back(VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = imageCount * samplerCount});

	const VkDescriptorPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.maxSets = static_cast<uint32_t>(imageCount),
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.empty() ? nullptr : poolSizes.data()};

	return (vkCreateDescriptorPool(vkDev.device, &poolInfo, nullptr, descriptorPool) == VK_SUCCESS);
}

// ============================ shader ====================================

VkShaderStageFlagBits glslangShaderStageToVulkan(glslang_stage_t sh)
{
	switch (sh)
	{
	case GLSLANG_STAGE_VERTEX:
		return VK_SHADER_STAGE_VERTEX_BIT;
	case GLSLANG_STAGE_FRAGMENT:
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	case GLSLANG_STAGE_GEOMETRY:
		return VK_SHADER_STAGE_GEOMETRY_BIT;
	case GLSLANG_STAGE_TESSCONTROL:
		return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	case GLSLANG_STAGE_TESSEVALUATION:
		return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	case GLSLANG_STAGE_COMPUTE:
		return VK_SHADER_STAGE_COMPUTE_BIT;
	}

	return VK_SHADER_STAGE_VERTEX_BIT;
}

glslang_stage_t glslangShaderStageFromFileName(const char *fileName)
{
	if (endsWith(fileName, ".vert"))
		return GLSLANG_STAGE_VERTEX;

	if (endsWith(fileName, ".frag"))
		return GLSLANG_STAGE_FRAGMENT;

	if (endsWith(fileName, ".geom"))
		return GLSLANG_STAGE_GEOMETRY;

	if (endsWith(fileName, ".comp"))
		return GLSLANG_STAGE_COMPUTE;

	if (endsWith(fileName, ".tesc"))
		return GLSLANG_STAGE_TESSCONTROL;

	if (endsWith(fileName, ".tese"))
		return GLSLANG_STAGE_TESSEVALUATION;

	return GLSLANG_STAGE_VERTEX;
}

static_assert(sizeof(TBuiltInResource) == sizeof(glslang_resource_t));

// compile a shader from its source code for a specified Vulkan pipeline stage
// then, save the binary SPIR-V result in the ShaderModule structure
static size_t compileShader(glslang_stage_t stage, const char *shaderSource, ShaderModule &shaderModule)
{
	const glslang_input_t input =
		{
			.language = GLSLANG_SOURCE_GLSL,
			.stage = stage,
			.client = GLSLANG_CLIENT_VULKAN,
			.client_version = GLSLANG_TARGET_VULKAN_1_1,
			.target_language = GLSLANG_TARGET_SPV,
			.target_language_version = GLSLANG_TARGET_SPV_1_3,
			.code = shaderSource,
			.default_version = 100,
			.default_profile = GLSLANG_NO_PROFILE,
			.force_default_version_and_profile = false,
			.forward_compatible = false,
			.messages = GLSLANG_MSG_DEFAULT_BIT,
			.resource = (const glslang_resource_t *)&glslang::DefaultTBuiltInResource,
		};

	glslang_shader_t *shader = glslang_shader_create(&input);

	// This function returns true if all the extensions, pragmas,
	// and version strings mentioned in the shader source code are valid
	if (!glslang_shader_preprocess(shader, &input))
	{
		fprintf(stderr, "GLSL preprocessing failed\n");
		fprintf(stderr, "\n%s", glslang_shader_get_info_log(shader));
		fprintf(stderr, "\n%s", glslang_shader_get_info_debug_log(shader));
		printShaderSource(input.code);
		return 0;
	}

	// the shader gets parsed in an internal parse tree representation inside the compiler
	if (!glslang_shader_parse(shader, &input))
	{
		fprintf(stderr, "GLSL parsing failed\n");
		fprintf(stderr, "\n%s", glslang_shader_get_info_log(shader));
		fprintf(stderr, "\n%s", glslang_shader_get_info_debug_log(shader));
		printShaderSource(glslang_shader_get_preprocessed_code(shader));
		return 0;
	}

	// link the shader to a program and proceed with the binary code generation stage
	glslang_program_t *program = glslang_program_create();
	glslang_program_add_shader(program, shader);

	if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
	{
		fprintf(stderr, "GLSL linking failed\n");
		fprintf(stderr, "\n%s", glslang_program_get_info_log(program));
		fprintf(stderr, "\n%s", glslang_program_get_info_debug_log(program));
		return 0;
	}

	// Generate some binary SPIR-V code and store it inside the shaderModule output variable
	glslang_program_SPIRV_generate(program, stage);

	shaderModule.SPIRV.resize(glslang_program_SPIRV_get_size(program));
	glslang_program_SPIRV_get(program, shaderModule.SPIRV.data());

	{
		const char *spirv_messages =
			glslang_program_SPIRV_get_messages(program);

		if (spirv_messages)
			fprintf(stderr, "%s", spirv_messages);
	}

	glslang_program_delete(program);
	glslang_shader_delete(shader);

	return shaderModule.SPIRV.size();
}

size_t compileShaderFile(const char *file, ShaderModule &shaderModule)
{
	if (auto shaderSource = readShaderFile(file); !shaderSource.empty())
		return compileShader(glslangShaderStageFromFileName(file), shaderSource.c_str(), shaderModule);

	return 0;
}

// compile a shader that's been loaded from a file using
// glslang and upload the resulting SPIR-V binary to Vulkan
VkResult createShaderModule(VkDevice device, ShaderModule *shader, const char *fileName)
{
	if (compileShaderFile(fileName, *shader) < 1)
		return VK_NOT_READY;

	const VkShaderModuleCreateInfo createInfo =
		{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = shader->SPIRV.size() * sizeof(unsigned int),
			.pCode = shader->SPIRV.data(),
		};

	return vkCreateShaderModule(device, &createInfo, nullptr, &shader->shaderModule);
}

// ============================ pipeline ====================================

// create a Vulkan pipeline layout
bool createPipelineLayout(VkDevice device, VkDescriptorSetLayout dsLayout, VkPipelineLayout *pipelineLayout)
{
	const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = 1,
		.pSetLayouts = &dsLayout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr};

	return (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, pipelineLayout) == VK_SUCCESS);
}

bool createPipelineLayoutWithConstants(VkDevice device, VkDescriptorSetLayout dsLayout,
									   VkPipelineLayout *pipelineLayout, uint32_t vtxConstSize,
									   uint32_t fragConstSize)
{
	const VkPushConstantRange ranges[] =
		{
			{
				VK_SHADER_STAGE_VERTEX_BIT, // stageFlags
				0,							// offset
				vtxConstSize				// size
			},

			{
				VK_SHADER_STAGE_FRAGMENT_BIT, // stageFlags
				vtxConstSize,				  // offset
				fragConstSize				  // size
			}};

	uint32_t constSize = (vtxConstSize > 0) + (fragConstSize > 0);

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = 1,
		.pSetLayouts = &dsLayout,
		.pushConstantRangeCount = constSize,
		.pPushConstantRanges = (constSize == 0) ? nullptr : (vtxConstSize > 0 ? ranges : &ranges[1])};

	return (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, pipelineLayout) == VK_SUCCESS);
}

// create one render pass that uses color and depth buffers. This
// render pass will clear the color and depth attachments up-front using the values
// provided later. The RenderPassCreateInfo structure is used to simplify the
// creation process
bool createColorAndDepthRenderPass(VulkanRenderDevice &vkDev, bool useDepth,
								   VkRenderPass *renderPass, const RenderPassCreateInfo &ci, VkFormat colorFormat)
{
	const bool offscreenInt = ci.flags_ & eRenderPassBit_OffscreenInternal;
	const bool first = ci.flags_ & eRenderPassBit_First;
	const bool last = ci.flags_ & eRenderPassBit_Last;

	VkAttachmentDescription colorAttachment = {
		.flags = 0,
		.format = colorFormat,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = offscreenInt ? VK_ATTACHMENT_LOAD_OP_LOAD : (ci.clearColor_ ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD),
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = first ? VK_IMAGE_LAYOUT_UNDEFINED : (offscreenInt ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
		.finalLayout = last ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	const VkAttachmentReference colorAttachmentRef = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkAttachmentDescription depthAttachment = {
		.flags = 0,
		.format = useDepth ? findDepthFormat(vkDev.physicalDevice) : VK_FORMAT_D32_SFLOAT,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		// If the loadOp field is changed to VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		// the framebuffer will not be cleared at the beginning of the render pass. This can be
		// desirable if the frame has been composed with the output of another rendering pass:
		.loadOp = offscreenInt ? VK_ATTACHMENT_LOAD_OP_LOAD : (ci.clearDepth_ ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD),
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = ci.clearDepth_ ? VK_IMAGE_LAYOUT_UNDEFINED : (offscreenInt ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
		.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	// The depth attachment is handled in a similar way
	const VkAttachmentReference depthAttachmentRef = {
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	// The subpasses in a render pass automatically take care of image layout transitions.
	// This render pass also specifies one subpass dependency, which instructs Vulkan to
	// prevent the transition from happening until it is actually necessary and allowed.
	// This dependency only makes sense for color buffer writes. In case of an offscreen
	// render pass, we use subpass dependencies for layout transitions.
	if (ci.flags_ & eRenderPassBit_Offscreen)
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	std::vector<VkSubpassDependency> dependencies = {
		/* VkSubpassDependency */ {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = 0}};

	// add two explicit dependencies which ensure all rendering operations are
	// completed before this render pass and before the color attachment can be used in
	// subsequent passes
	if (ci.flags_ & eRenderPassBit_Offscreen)
	{
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// Use subpass dependencies for layout transitions
		dependencies.resize(2);

		dependencies[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT};

		dependencies[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT};
	}

	// The rendering pass consists of a single subpass that only uses color and depth buffers
	const VkSubpassDescription subpass = {
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentRef,
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = useDepth ? &depthAttachmentRef : nullptr,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr};

	std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

	const VkRenderPassCreateInfo renderPassInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.attachmentCount = static_cast<uint32_t>(useDepth ? 2 : 1),
		.pAttachments = attachments.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = static_cast<uint32_t>(dependencies.size()),
		.pDependencies = dependencies.data()};

	return (vkCreateRenderPass(vkDev.device, &renderPassInfo, nullptr, renderPass) == VK_SUCCESS);
}

bool createGraphicsPipeline(
	VulkanRenderDevice &vkDev,
	VkRenderPass renderPass, VkPipelineLayout pipelineLayout,
	const std::vector<const char *> &shaderFiles,
	VkPipeline *pipeline,
	VkPrimitiveTopology topology,
	bool useDepth,
	bool useBlending,
	bool dynamicScissorState,
	int32_t customWidth,
	int32_t customHeight,
	uint32_t numPatchControlPoints)
{
	std::vector<ShaderModule> shaderModules;
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	shaderStages.resize(shaderFiles.size());
	shaderModules.resize(shaderFiles.size());

	for (size_t i = 0; i < shaderFiles.size(); i++)
	{
		const char *file = shaderFiles[i];
		VK_CHECK(createShaderModule(vkDev.device, &shaderModules[i], file));

		VkShaderStageFlagBits stage = glslangShaderStageToVulkan(glslangShaderStageFromFileName(file));

		shaderStages[i] = shaderStageInfo(stage, shaderModules[i], "main");
	}

	const VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

	// we are using programmable vertex pulling, the only thing we need to specify
	// for the input assembly is the primitive topology type. We must disable the primitive
	// restart capabilities that we won't be using
	const VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		/* The only difference from createGraphicsPipeline() */
		.topology = topology,
		.primitiveRestartEnable = VK_FALSE};

	const VkViewport viewport = {
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(customWidth > 0 ? customWidth : vkDev.framebufferWidth),
		.height = static_cast<float>(customHeight > 0 ? customHeight : vkDev.framebufferHeight),
		.minDepth = 0.0f,
		.maxDepth = 1.0f};

	// scissor covers the entire viewport:
	const VkRect2D scissor = {
		.offset = {0, 0},
		.extent = {customWidth > 0 ? customWidth : vkDev.framebufferWidth, customHeight > 0 ? customHeight : vkDev.framebufferHeight}};

	// combine the viewport and scissor declarations in the required viewport state
	const VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor};

	// We will not be using backface culling yet
	const VkPipelineRasterizationStateCreateInfo rasterizer = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.lineWidth = 1.0f};

	// Multisampling should be disabled
	const VkPipelineMultisampleStateCreateInfo multisampling = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 1.0f};

	// All the blending operations should be disabled as well. A color mask is required if
	// we want to see any pixels that have been rendered
	const VkPipelineColorBlendAttachmentState colorBlendAttachment = {
		.blendEnable = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = useBlending ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

	// Disable any logic operations
	const VkPipelineColorBlendStateCreateInfo colorBlending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
		.blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}};

	// Enable depth test using the VK_COMPARE_OP_LESS operator
	const VkPipelineDepthStencilStateCreateInfo depthStencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = static_cast<VkBool32>(useDepth ? VK_TRUE : VK_FALSE),
		.depthWriteEnable = static_cast<VkBool32>(useDepth ? VK_TRUE : VK_FALSE),
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f};

	VkDynamicState dynamicStateElt = VK_DYNAMIC_STATE_SCISSOR;

	const VkPipelineDynamicStateCreateInfo dynamicState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.dynamicStateCount = 1,
		.pDynamicStates = &dynamicStateElt};

	const VkPipelineTessellationStateCreateInfo tessellationState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.patchControlPoints = numPatchControlPoints};

	// bring all the previously defined rendering states and attachments
	// into the VkGraphicsPipelineCreateInfo structure. Then, we can call
	// the vkCreateGraphicsPipelines() function to create an actual Vulkan
	// graphics pipeline
	const VkGraphicsPipelineCreateInfo pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = static_cast<uint32_t>(shaderStages.size()),
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pTessellationState = (topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST) ? &tessellationState : nullptr,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = useDepth ? &depthStencil : nullptr,
		.pColorBlendState = &colorBlending,
		// we can specify the pDynamicState field of the VkGraphicsPipelineCreateInfo structure. This
		// is an array of state identifiers that can change. The most commonly used values are VK_
		// DYNAMIC_STATE_SCISSOR and VK_DYNAMIC_STATE_VIEWPORT. When a graphics
		// pipeline is created with these options enabled, we can use the vkCmdSetScissor()
		// and vkCmdSetViewport() functions to record frame-dependent values into Vulkan
		// command buffers.
		.pDynamicState = dynamicScissorState ? &dynamicState : nullptr,
		.layout = pipelineLayout,
		.renderPass = renderPass,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1};

	VK_CHECK(vkCreateGraphicsPipelines(vkDev.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, pipeline));

	for (auto m : shaderModules)
		vkDestroyShaderModule(vkDev.device, m.shaderModule, nullptr);

	return true;
}