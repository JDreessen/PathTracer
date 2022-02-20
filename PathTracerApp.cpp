//
// Created by jedre on 19.02.2022.
//

#include <iostream>
#include "PathTracerApp.hpp"
#include "GLFW/glfw3.h"

PathTracerApp::PathTracerApp()
        : window()
        , windowWidth()
        , windowHeight()
        , vkInstance(VK_NULL_HANDLE)
        , physicalDevice(VK_NULL_HANDLE)
        , device(VK_NULL_HANDLE)
        , glfwSurface(VK_NULL_HANDLE)
        , surface(VK_NULL_HANDLE)
        , queueFamilyIndices{~0u, ~0u, ~0u}
        , swapchain(VK_NULL_HANDLE) {}

void PathTracerApp::run() {
    initSettings();
    initGLFW();
    initVulkan();
    initDevicesAndQueues();
    initSurface();
    initSwapchain();

    mainLoop();
}

PathTracerApp &PathTracerApp::instance() {
    static PathTracerApp _instance;
    return _instance;
}

// free glfw and vulkan resources
PathTracerApp::~PathTracerApp() {
    glfwDestroyWindow(window);
    glfwTerminate();
}

void PathTracerApp::mainLoop() {

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glfwSetKeyCallback(window, [](GLFWwindow *callbackWindow, int key, int scancode, int action, int mods) {
            if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(callbackWindow, 1);
        });
    }
}

void PathTracerApp::initSettings() {
    name = "PathTracer";
    windowWidth = 1280;
    windowHeight = 720;
}

void PathTracerApp::initGLFW() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(windowWidth,
                              windowHeight,
                              "Vulkan window",
                              nullptr,
                              nullptr);

    //glfwSetKeyCallback(window, keyCallback());
}

//TODO: glfw key callback
//GLFWkeyfun PathTracerApp::keyCallback() {}

void PathTracerApp::initVulkan() {
    uint32_t glfwExtensionsCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);

    std::vector<const char*> instanceExtensions;
    for (uint32_t i = 0; i < glfwExtensionsCount; i++)
        instanceExtensions.emplace_back(glfwExtensions[i]);
    instanceExtensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    vk::ApplicationInfo applicationInfo = vk::ApplicationInfo(name.c_str(),
                                                              VK_MAKE_VERSION(1, 0, 0),
                                                              "Vulkan",
                                                              VK_MAKE_VERSION(1, 0, 0),
                                                              VK_API_VERSION_1_2);

    vk::InstanceCreateInfo instanceCreateInfo = vk::InstanceCreateInfo({},
                                                                       &applicationInfo,
                                                                       {},
                                                                       {},
                                                                       instanceExtensions.size(),
                                                                       instanceExtensions.data());
    vkInstance = std::move(vk::raii::Instance(context, instanceCreateInfo));
}

void PathTracerApp::initDevicesAndQueues() {
    // grab first available GPU
    //TODO: test all available GPUs for extension support
    physicalDevice = std::move(vk::raii::PhysicalDevices(vkInstance).front());

    // grab available extensions of GPU
    std::vector<vk::ExtensionProperties> extensionProperties = physicalDevice.enumerateDeviceExtensionProperties();

    //
    auto requiredExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                               VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                               VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME};

    // make sure GPU supports necessary extensions
    for (auto &ex : requiredExtensions) {
        if (!vk::utils::contains(extensionProperties, ex)) {
            throw std::runtime_error(std::string("GPU does not support ") + ex);
        }
    }

    // find queue family indices

    // graphics, compute and transfer queue family indices
    std::vector<vk::QueueFamilyProperties> queueFamilies = physicalDevice.getQueueFamilyProperties();

    // stolen from rtxON tutorial
    // any graphics; compute without graphics; transfer without graphics or compute queue family
    // ensures queueFlags are in different queueFamilies if possible
    const vk::QueueFlagBits askingFlags[3] = { vk::QueueFlagBits::eGraphics, vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eTransfer };
    for (size_t i = 0; i < 3; ++i) {
        const vk::QueueFlagBits flag = askingFlags[i];
        uint32_t& queueIdx = queueFamilyIndices[i];

        switch (flag) {
            // select a compute queue family different from the graphics queue family if possible
            case vk::QueueFlagBits::eCompute:
                for (uint32_t j = 0; j < queueFamilies.size(); ++j) {
                    if ((queueFamilies[j].queueFlags & vk::QueueFlagBits::eCompute) &&
                        !(queueFamilies[j].queueFlags & vk::QueueFlagBits::eGraphics)) {
                        queueIdx = j;
                        break;
                    }
                }
                break;
            // select a transfer queue family different from the graphics and compute queue families if possible
            case vk::QueueFlagBits::eTransfer:
                for (uint32_t j = 0; j < queueFamilies.size(); ++j) {
                    if ((queueFamilies[j].queueFlags & vk::QueueFlagBits::eTransfer) &&
                        !(queueFamilies[j].queueFlags & vk::QueueFlagBits::eGraphics) &&
                        !(queueFamilies[j].queueFlags & vk::QueueFlagBits::eCompute)) {
                        queueIdx = j;
                        break;
                    }
                }
                break;
            default: // ignore other cases
                break;
        }
        // select any available matching queue family otherwise
        if (queueIdx == ~0u) {
            for (uint32_t j = 0; j < queueFamilies.size(); ++j) {
                if (queueFamilies[j].queueFlags & flag) {
                    queueIdx = j;
                    break;
                }
            }
        }
    }
    //std::cout << "queue family indices: " << queueFamilyIndices[0] << " " << queueFamilyIndices[1] << " " << queueFamilyIndices[2];

    // Create logical device from GPU with required extensions and all available extra features
    auto supportedFeatures =
            physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceDescriptorIndexingFeaturesEXT>();

    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo(vk::DeviceQueueCreateFlags(),
                                                    queueFamilyIndices[0],
                                                    1,
                                                    &queuePriority);
    vk::DeviceCreateInfo deviceCreateInfo(vk::DeviceCreateFlags(),
                                          deviceQueueCreateInfo,
                                          {},
                                          requiredExtensions,
                                          &supportedFeatures.get<vk::PhysicalDeviceFeatures2>().features);
    deviceCreateInfo.pNext = &supportedFeatures.get<vk::PhysicalDeviceFeatures2>();
    device = physicalDevice.createDevice(deviceCreateInfo);
}

void PathTracerApp::initSurface() {
    VkResult err = glfwCreateWindowSurface(static_cast<VkInstance>(*vkInstance),
                                           window,
                                           nullptr,
                                           &glfwSurface);
    check_vk_result(err);
    surface = vk::raii::SurfaceKHR(vkInstance, glfwSurface);

    //TODO: setup present queue
    if (!physicalDevice.getSurfaceSupportKHR(queueFamilyIndices[0], *surface))
        throw std::runtime_error("Graphics queue family has no surface support");
    auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(*surface);

}

void PathTracerApp::initSwapchain() {
    swapchain = std::move(vk::raii::SwapchainKHR(device, {}));
}