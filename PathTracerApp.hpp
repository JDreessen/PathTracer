//
// Created by JDreessen on 19.02.2022.
//

#ifndef PATHTRACER_PATHTRACERAPP_HPP
#define PATHTRACER_PATHTRACERAPP_HPP

#include <memory>

#define GLFW_INCLUDE_VULKAN

#include "GLFW/glfw3.h"

#define GLM_ENABLE_EXPERIMENTAL

#include "glm/glm.hpp"
#include "glm/gtx/hash.hpp"
#include <vulkan/vulkan_raii.hpp>
#include "VulkanUtils.hpp"
#include "shaderStructs.hpp"

class PathTracerApp {
public:
    void run(); // run application

    static PathTracerApp &instance();

    ~PathTracerApp();

    // Singleton stuff
    PathTracerApp(const PathTracerApp &) = delete;

    PathTracerApp &operator=(const PathTracerApp &) = delete;

private:
    PathTracerApp();

    void mainLoop();

    void initSettings();                // Initialize application settings
    void initGLFW();                    // Create glfw window
    void initVulkan();                  // Initialize vulkan instance
    void initDevicesAndQueues();        // Create vulkan devices, queue families and queues
    void initSurface();                 // Create vulkan window using glfw
    void initSwapchain();               //
    void initSyncObjects();             //
    void initImages();                  // Initialize resultImage for displaying and previousImage for blending
    void initCommandPoolAndBuffers();   //
    void fillCommandBuffers();          //

    void fillCommandBuffer(const vk::raii::CommandBuffer &commandBuffer);

    void keyCallback(GLFWwindow *callbackWindow, int key, int scancode, int action, int mods);

    void drawFrame(float dt);

    void update(float dt);

    void createAS(const vk::AccelerationStructureTypeKHR &type,
                  const vk::AccelerationStructureGeometryKHR &geometry,
                  vk::utils::RTAccelerationStructure &_as);

    void createScene();

    void createRaytracingPipeline();

    void createShaderBindingTable();

    void createDescriptorSets();

    std::string name;
    uint32_t windowWidth{}, windowHeight{};
    GLFWwindow *window;

    vk::raii::Context context;
    vk::raii::Instance vkInstance;
    vk::raii::PhysicalDevice physicalDevice;
    vk::raii::Device device;

    VkSurfaceKHR glfwSurface;
    vk::SurfaceFormatKHR surfaceFormat;
    vk::raii::SurfaceKHR surface;

    vk::raii::SwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<vk::raii::ImageView> swapchainImageViews;
    std::vector<uint32_t> queueFamilyIndices;
    std::vector<vk::raii::Fence> waitForFrameFences;
    vk::utils::Image resultImage;
    vk::raii::CommandPool commandPool;
    std::vector<vk::raii::CommandBuffer> commandBuffers;
    vk::raii::Semaphore semaphoreImageAvailable;
    vk::raii::Semaphore semaphoreRenderFinished;
    vk::raii::Queue graphicsQueue;
    vk::raii::Queue computeQueue;
    vk::raii::Queue transferQueue;

    // RayTracing pipeline stuff
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR pipelineProperties;
    std::vector<vk::raii::DescriptorSetLayout> descriptorSetLayouts;
    vk::raii::PipelineLayout pipelineLayout;
    vk::raii::Pipeline pipelineRT;
    vk::raii::DescriptorPool descriptorPoolRayGen;
    vk::raii::DescriptorPool descriptorPoolCHit;
    std::vector<vk::raii::DescriptorSet> descriptorSets;
    vk::utils::Buffer shaderBindingTable;

    // Scene data
    FrameData frameData; // Camera position and frame index
    vk::utils::Buffer frameDataBuffer;
    glm::vec3 cameraDelta;
    vk::utils::RTScene scene;
};

#endif //PATHTRACER_PATHTRACERAPP_HPP
