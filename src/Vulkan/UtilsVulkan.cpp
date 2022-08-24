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

void CHECK(bool check, const char *fileName, int lineNumber)
{
	if (!check)
	{
		printf("CHECK() failed at %s:%i\n", fileName, lineNumber);
		assert(false);
		exit(EXIT_FAILURE);
	}
}

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
