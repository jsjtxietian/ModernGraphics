#include "VulkanApp.h"

Resolution detectResolution(int width, int height)
{
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const int code = glfwGetError(nullptr);

    if (code != 0)
    {
        printf("Monitor: %p; error = %x / %d\n", monitor, code, code);
        exit(255);
    }

    const GLFWvidmode *info = glfwGetVideoMode(monitor);

    const uint32_t windowW = width > 0 ? width : (uint32_t)(info->width * width / -100);
    const uint32_t windowH = height > 0 ? height : (uint32_t)(info->height * height / -100);

    return Resolution{.width = windowW, .height = windowH};
}

GLFWwindow *initVulkanApp(int width, int height, Resolution *resolution)
{
    glslang_initialize_process();

    volkInitialize();

    if (!glfwInit())
        exit(EXIT_FAILURE);

    if (!glfwVulkanSupported())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    if (resolution)
    {
        *resolution = detectResolution(width, height);
        width = resolution->width;
        height = resolution->height;
    }

    GLFWwindow *result = glfwCreateWindow(width, height, "VulkanApp", nullptr, nullptr);
    if (!result)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    return result;
}

bool drawFrame(VulkanRenderDevice &vkDev, const std::function<void(uint32_t)> &updateBuffersFunc, const std::function<void(VkCommandBuffer, uint32_t)> &composeFrameFunc)
{
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(vkDev.device, vkDev.swapchain, 0, vkDev.semaphore, VK_NULL_HANDLE, &imageIndex);
    VK_CHECK(vkResetCommandPool(vkDev.device, vkDev.commandPool, 0));

    if (result != VK_SUCCESS)
        return false;

    updateBuffersFunc(imageIndex);

    VkCommandBuffer commandBuffer = vkDev.commandBuffers[imageIndex];

    const VkCommandBufferBeginInfo bi =
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            .pInheritanceInfo = nullptr};

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &bi));

    composeFrameFunc(commandBuffer, imageIndex);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; // or even VERTEX_SHADER_STAGE

    const VkSubmitInfo si =
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &vkDev.semaphore,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &vkDev.commandBuffers[imageIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &vkDev.renderSemaphore};

    VK_CHECK(vkQueueSubmit(vkDev.graphicsQueue, 1, &si, nullptr));

    const VkPresentInfoKHR pi =
        {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &vkDev.renderSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &vkDev.swapchain,
            .pImageIndices = &imageIndex};

    VK_CHECK(vkQueuePresentKHR(vkDev.graphicsQueue, &pi));
    VK_CHECK(vkDeviceWaitIdle(vkDev.device));

    return true;
}
