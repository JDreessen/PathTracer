//
// Created by jedre on 19.02.2022.
//

#ifndef PATHTRACER_PATHTRACERAPP_HPP
#define PATHTRACER_PATHTRACERAPP_HPP

#include <memory>
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vulkan/vulkan_raii.hpp>
#include "VulkanUtils.hpp"

class PathTracerApp {
public:
    void run(); // run application

    static PathTracerApp& instance();
    ~PathTracerApp();

    // Singleton stuff
    PathTracerApp(const PathTracerApp&) = delete;
    PathTracerApp& operator=(const PathTracerApp&) = delete;
private:
    PathTracerApp();

    void mainLoop();
    void initSettings();                // Initialize application settings
    void initGLFW();                    // Create glfw window and declare callbacks
    void initVulkan();                  // Initialize vulkan instance
    void initDevicesAndQueues();        // Create vulkan devices, queue families and queues
    void initSurface();                 // Create vulkan window using glfw
    void initSwapchain();               //
    void initSyncObjects();             //
    void initCommandPoolAndBuffers();   //

    //GLFWkeyfun keyCallback();

    std::string name;
    uint32_t windowWidth{}, windowHeight{};
    GLFWwindow* window;

    vk::raii::Context context;
    vk::raii::Instance vkInstance;
    vk::raii::PhysicalDevice physicalDevice;
    vk::raii::Device device;
    VkSurfaceKHR glfwSurface;
    vk::SurfaceFormatKHR surfaceFormat;
    vk::raii::SurfaceKHR surface;
    vk::raii::SwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages; // try converting elements to vk::raii::Image if problems occur
    std::vector<vk::raii::ImageView> swapchainImageViews;
    std::array<uint32_t, 3> queueFamilyIndices;
    std::vector<vk::raii::Fence> waitForFrameFences;
    vk::raii::CommandPool commandPool;
    std::vector<vk::raii::CommandBuffer> commandBuffers;
    vk::raii::Semaphore semaphoreImageAvailable;
    vk::raii::Semaphore semaphoreRenderFinished;
    //vk::raii::Queue graphicsQueue;
    //vk::raii::Queue computeQueue;
    //vk::raii::Queue transferQueue;
};

#endif //PATHTRACER_PATHTRACERAPP_HPP
