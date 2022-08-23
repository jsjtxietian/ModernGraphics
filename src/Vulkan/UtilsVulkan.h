#pragma once

#include <vector>

#define VK_NO_PROTOTYPES
#include <volk/volk.h>
#include "glslang_c_interface.h"


struct ShaderModule final
{
	std::vector<unsigned int> SPIRV;
	VkShaderModule shaderModule = nullptr;
};

size_t compileShaderFile(const char* file, ShaderModule& shaderModule);