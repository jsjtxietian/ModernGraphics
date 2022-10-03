#pragma once

#define VK_NO_PROTOTYPES
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <limits>

#include <imgui/imgui.h>

#include "Scene/Camera.h"
#include "Utils/Utils.h"
#include "Utils/UtilsMath.h"
#include "Vulkan/UtilsVulkan.h"
#include "Utils/UtilsFPS.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

struct Resolution
{
    uint32_t width = 0;
    uint32_t height = 0;
};

GLFWwindow *initVulkanApp(int width, int height, Resolution *resolution = nullptr);
bool drawFrame(VulkanRenderDevice &vkDev, const std::function<void(uint32_t)> &updateBuffersFunc,
               const std::function<void(VkCommandBuffer, uint32_t)> &composeFrameFunc);