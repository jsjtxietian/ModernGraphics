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

using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

#include <cstdio>
#include <cstdlib>

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
						 VkSwapchainKHR *swapchain, bool supportScreenshots = false)
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
					 VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
					 uint32_t layerCount = 1, uint32_t mipLevels = 1)
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

// ============================ shader ====================================
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
