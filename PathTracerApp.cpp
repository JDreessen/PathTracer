//
// Created by jedre on 19.02.2022.
//

#include "PathTracerApp.hpp"
#include <iostream>

PathTracerApp::PathTracerApp()
        : window(), windowWidth(), windowHeight(), vkInstance(VK_NULL_HANDLE), physicalDevice(VK_NULL_HANDLE),
          device(VK_NULL_HANDLE), glfwSurface(VK_NULL_HANDLE), surfaceFormat(), surface(VK_NULL_HANDLE),
          queueFamilyIndices{~0u, ~0u, ~0u}  // max uint32_t
        , swapchain(VK_NULL_HANDLE), swapchainImages(), swapchainImageViews(), waitForFrameFences(),
          commandPool(VK_NULL_HANDLE), semaphoreImageAvailable(VK_NULL_HANDLE), semaphoreRenderFinished(VK_NULL_HANDLE),
          graphicsQueue(VK_NULL_HANDLE), computeQueue(VK_NULL_HANDLE), transferQueue(VK_NULL_HANDLE),
          descriptorSetLayoutRT(VK_NULL_HANDLE), pipelineLayoutRT(VK_NULL_HANDLE), pipelineRT(VK_NULL_HANDLE),
          descriptorPoolRT(VK_NULL_HANDLE), descriptorSetRT(VK_NULL_HANDLE), shaderBindingTable(), scene() {}

void PathTracerApp::run() {
    initSettings();
    initGLFW();
    initVulkan();
    initDevicesAndQueues();
    initSurface();
    initSwapchain();
    initSyncObjects();
    vk::utils::Initialize(&physicalDevice, &device, &commandPool, &transferQueue);
    initOffscreenImage();
    initCommandPoolAndBuffers();

    createScene();
    createRaytracingPipeline();
    createShaderBindingTable();
    createDescriptorSet();

    fillCommandBuffers();

    mainLoop();
}

PathTracerApp &PathTracerApp::instance() {
    static PathTracerApp _instance;
    return _instance;
}

// free glfw and vulkan resources
PathTracerApp::~PathTracerApp() {
    device.waitIdle();
    glfwDestroyWindow(window);
    glfwTerminate();
}

void PathTracerApp::mainLoop() {
    glfwSetTime(0);
    double currentTime, previousTime = 0, deltaTime = 0;
    while (!glfwWindowShouldClose(window)) {
        currentTime = glfwGetTime();
        deltaTime = currentTime - previousTime;
        previousTime = currentTime;

        drawFrame(static_cast<float>(deltaTime));

        glfwPollEvents();
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

    window = glfwCreateWindow(static_cast<int>(windowWidth),
                              static_cast<int>(windowHeight),
                              "Vulkan window",
                              nullptr,
                              nullptr);

    glfwSetKeyCallback(window, [](GLFWwindow *callbackWindow, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(callbackWindow, 1);
    });
}

void PathTracerApp::initVulkan() {
    uint32_t glfwExtensionsCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);

    std::vector<const char *> instanceExtensions;
    for (uint32_t i = 0; i < glfwExtensionsCount; i++)
        instanceExtensions.emplace_back(glfwExtensions[i]);
    instanceExtensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    std::vector<const char *> instanceLayers;

#ifndef NDEBUG
    instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    vk::ApplicationInfo applicationInfo = vk::ApplicationInfo(name.c_str(),
                                                              VK_MAKE_VERSION(1, 0, 0),
                                                              "Vulkan",
                                                              VK_MAKE_VERSION(1, 0, 0),
                                                              VK_API_VERSION_1_2);

    vk::InstanceCreateInfo instanceCreateInfo = vk::InstanceCreateInfo({},
                                                                       &applicationInfo,
                                                                       instanceLayers,
                                                                       instanceExtensions);
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
                               VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
                               VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                               VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                               VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
                               VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                               VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                               VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME};

    // make sure GPU supports necessary extensions
    for (auto &ex: requiredExtensions) {
        if (!vk::utils::contains(extensionProperties, ex)) {
            throw std::runtime_error(std::string("Vulkan Error: GPU does not support ") + ex);
        }
    }

    // graphics, compute and transfer queue family indices
    std::vector<vk::QueueFamilyProperties> queueFamilies = physicalDevice.getQueueFamilyProperties();

    // stolen from rtxON tutorial
    // any graphics; compute without graphics; transfer without graphics or compute queue family are preferred
    // this ensures queueFlags are in different queueFamilies if possible
    const std::array<vk::QueueFlagBits, 3> askingFlags = {vk::QueueFlagBits::eGraphics,
                                                          vk::QueueFlagBits::eCompute,
                                                          vk::QueueFlagBits::eTransfer};
    for (size_t i = 0; i < 3; ++i) {
        const vk::QueueFlagBits flag = askingFlags[i];
        uint32_t & queueFamilyIndex = queueFamilyIndices[i];

        if (flag == vk::QueueFlagBits::eCompute) {
            // select a compute queue family different from the graphics queue family if possible
            for (uint32_t j = 0; j < queueFamilies.size(); ++j) {
                if ((queueFamilies[j].queueFlags & vk::QueueFlagBits::eCompute) &&
                    !(queueFamilies[j].queueFlags & vk::QueueFlagBits::eGraphics)) {
                    queueFamilyIndex = j;
                    break;
                }
            }
        } else if (flag == vk::QueueFlagBits::eTransfer) {
            // select a transfer queue family different from the graphics and compute queue families if possible
            for (uint32_t j = 0; j < queueFamilies.size(); ++j) {
                if ((queueFamilies[j].queueFlags & vk::QueueFlagBits::eTransfer) &&
                    !(queueFamilies[j].queueFlags & vk::QueueFlagBits::eGraphics) &&
                    !(queueFamilies[j].queueFlags & vk::QueueFlagBits::eCompute)) {
                    queueFamilyIndex = j;
                    break;
                }
            }
        }
        // select any available matching queue family otherwise
        if (queueFamilyIndex == ~0u) {
            for (uint32_t j = 0; j < queueFamilies.size(); ++j) {
                if (queueFamilies[j].queueFlags & flag) {
                    queueFamilyIndex = j;
                    break;
                }
            }
        }
    }

    // Create logical device from GPU with required extensions and all available extra features
    auto supportedFeatures =
            physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2,
                    vk::PhysicalDeviceDescriptorIndexingFeaturesEXT,
                    vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
                    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR,
                    vk::PhysicalDeviceBufferDeviceAddressFeatures>();

    float queuePriority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo(vk::DeviceQueueCreateFlags(),
                                                    queueFamilyIndices[vk::utils::QueueFamilyIndex::graphics],
                                                    1,
                                                    &queuePriority);
    deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);

    if (queueFamilyIndices[vk::utils::QueueFamilyIndex::compute] !=
        queueFamilyIndices[vk::utils::QueueFamilyIndex::graphics])
        deviceQueueCreateInfos.push_back(
                deviceQueueCreateInfo.setQueueFamilyIndex(queueFamilyIndices[vk::utils::QueueFamilyIndex::compute]));
    if (queueFamilyIndices[vk::utils::QueueFamilyIndex::transfer] !=
        queueFamilyIndices[vk::utils::QueueFamilyIndex::graphics])
        deviceQueueCreateInfos.push_back(
                deviceQueueCreateInfo.setQueueFamilyIndex(queueFamilyIndices[vk::utils::QueueFamilyIndex::transfer]));

    vk::DeviceCreateInfo deviceCreateInfo(vk::DeviceCreateFlags(),
                                          deviceQueueCreateInfos,
                                          {},
                                          requiredExtensions,
                                          &supportedFeatures.get<vk::PhysicalDeviceFeatures2>().features);
    deviceCreateInfo.pNext = &supportedFeatures.get<vk::PhysicalDeviceDescriptorIndexingFeaturesEXT>();
    device = physicalDevice.createDevice(deviceCreateInfo);

    graphicsQueue = device.getQueue(queueFamilyIndices[vk::utils::QueueFamilyIndex::graphics], 0);
    computeQueue = device.getQueue(queueFamilyIndices[vk::utils::QueueFamilyIndex::compute], 0);
    transferQueue = device.getQueue(queueFamilyIndices[vk::utils::QueueFamilyIndex::transfer], 0);

    auto props = physicalDevice.getProperties2<vk::PhysicalDeviceProperties2,
            vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    pipelinePropertiesRT = props.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
}

void PathTracerApp::initSurface() {
    VkResult err = glfwCreateWindowSurface(static_cast<VkInstance>(*vkInstance),
                                           window,
                                           nullptr,
                                           &glfwSurface);
    check_vk_result(err);
    surface = vk::raii::SurfaceKHR(vkInstance, glfwSurface);

    if (!physicalDevice.getSurfaceSupportKHR(queueFamilyIndices[vk::utils::QueueFamilyIndex::graphics], *surface))
        throw std::runtime_error("Graphics queue family has no surface support");

    auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
    assert(!surfaceFormats.empty());

    // choose surface format
    // eB8G8R8A8Unorm format in sRGB color space is preferred as it looks fine and is widely supported
    // fallback to first available surfaceFormat
    if (auto it = std::find(surfaceFormats.begin(), surfaceFormats.end(),
                            vk::SurfaceFormatKHR(vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear)); it !=
                                                                                                                  surfaceFormats.end()) {
        surfaceFormat = vk::SurfaceFormatKHR(*it);
    } else surfaceFormat = vk::SurfaceFormatKHR(surfaceFormats[0]);
}

void PathTracerApp::initSwapchain() {
    // get surface capabilities
    auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);

    // make sure window size is within limits
    windowWidth = std::clamp(windowWidth, surfaceCapabilities.minImageExtent.width,
                             surfaceCapabilities.maxImageExtent.width);
    windowHeight = std::clamp(windowHeight, surfaceCapabilities.minImageExtent.height,
                              surfaceCapabilities.maxImageExtent.height);

    // since we only care about rendering static images, present mode doesn't matter so FIFO is fine
    vk::SwapchainCreateInfoKHR swapchainCreateInfo({},
                                                   *surface,
                                                   surfaceCapabilities.minImageCount,
                                                   surfaceFormat.format,
                                                   surfaceFormat.colorSpace,
                                                   {windowWidth, windowHeight},
                                                   1,
                                                   vk::ImageUsageFlagBits::eColorAttachment |
                                                   vk::ImageUsageFlagBits::eTransferDst,
                                                   vk::SharingMode::eConcurrent, // simpler, but less performant
                                                   queueFamilyIndices,
                                                   vk::SurfaceTransformFlagBitsKHR::eIdentity,
                                                   vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                                   vk::PresentModeKHR::eFifo,
                                                   VK_TRUE,
                                                   VK_NULL_HANDLE); // required for rebuilding swapchain to enable resizing window
    swapchain = std::move(vk::raii::SwapchainKHR(device, swapchainCreateInfo));

    // get swapchain images and create corresponding image views
    swapchainImages = swapchain.getImages();
    vk::ImageViewCreateInfo imageViewCreateInfo({},
                                                {},
                                                vk::ImageViewType::e2D, surfaceFormat.format,
                                                {},
                                                {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    for (const auto &swapchainImage: swapchainImages) {
        imageViewCreateInfo.image = swapchainImage;
        swapchainImageViews.emplace_back(device, imageViewCreateInfo);
    }
}

void PathTracerApp::initSyncObjects() {
    for (size_t i = 0; i < swapchainImages.size(); ++i)
        waitForFrameFences.emplace_back(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));

    commandPool = device.createCommandPool(vk::CommandPoolCreateInfo());

    semaphoreImageAvailable = device.createSemaphore(vk::SemaphoreCreateInfo());
    semaphoreRenderFinished = device.createSemaphore(vk::SemaphoreCreateInfo());
}

void PathTracerApp::initOffscreenImage() {
    offscreenImage = vk::utils::Image(vk::ImageType::e2D,
                          surfaceFormat.format,
                          {windowWidth, windowHeight, 1},
                          vk::ImageTiling::eOptimal,
                          vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
                          vk::MemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal));

    offscreenImage.createImageView(vk::ImageViewType::e2D,
                                   surfaceFormat.format,
                                   {vk::ImageAspectFlagBits::eColor,
                                    0,
                                    1,
                                    0,
                                    1});
}

void PathTracerApp::initCommandPoolAndBuffers() {
    commandPool = device.createCommandPool(
            vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                      queueFamilyIndices[vk::utils::QueueFamilyIndex::graphics]));

    commandBuffers = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(*commandPool,
                                                                                 vk::CommandBufferLevel::ePrimary,
                                                                                 swapchainImages.size()));
}

void PathTracerApp::fillCommandBuffers() {
    for (size_t i = 0; i < commandBuffers.size(); ++i) {
        const vk::raii::CommandBuffer &commandBuffer = commandBuffers[i];
        commandBuffer.begin({});

        vk::utils::imageBarrier(commandBuffer,
                                *offscreenImage.getImage(),
                                {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
                                {},
                                vk::AccessFlagBits::eShaderWrite,
                                vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eGeneral);

        fillCommandBuffer(commandBuffer, i);

        vk::utils::imageBarrier(commandBuffer,
                                swapchainImages[i],
                                {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
                                {},
                                vk::AccessFlagBits::eTransferWrite,
                                vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eTransferDstOptimal);

        vk::utils::imageBarrier(commandBuffer,
                                *offscreenImage.getImage(),
                                {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
                                vk::AccessFlagBits::eShaderWrite,
                                vk::AccessFlagBits::eTransferRead,
                                vk::ImageLayout::eGeneral,
                                vk::ImageLayout::eTransferSrcOptimal);

        vk::ImageCopy copyRegion({vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                 {0, 0, 0},
                                 {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                 {0, 0, 0},
                                 {windowWidth, windowHeight, 1});

        commandBuffer.copyImage(*offscreenImage.getImage(),
                                vk::ImageLayout::eTransferSrcOptimal,
                                swapchainImages[i],
                                vk::ImageLayout::eTransferDstOptimal,
                                copyRegion);

        vk::utils::imageBarrier(commandBuffer, swapchainImages[i],
                                {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
                                vk::AccessFlagBits::eTransferWrite,
                                {},
                                vk::ImageLayout::eTransferDstOptimal,
                                vk::ImageLayout::ePresentSrcKHR);

        commandBuffer.end();
    }
}

void PathTracerApp::fillCommandBuffer(const vk::raii::CommandBuffer &commandBuffer, size_t i) {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *pipelineRT);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *pipelineLayoutRT, 0, *descriptorSetRT, {});

    // our shader binding table layout:
    // |[ raygen ]|[closest hit]|[miss]|
    // | 0        | 1           | 2    |

    uint32_t sbtChunkSize =
            (pipelinePropertiesRT.shaderGroupHandleSize + (pipelinePropertiesRT.shaderGroupBaseAlignment - 1)) &
            (~(pipelinePropertiesRT.shaderGroupBaseAlignment - 1));
    //std::cout << pipelinePropertiesRT.shaderGroupBaseAlignment << ", "<< pipelinePropertiesRT.shaderGroupHandleAlignment << std::endl;
    std::array strideAddresses{
            vk::StridedDeviceAddressRegionKHR(shaderBindingTable.getAddress() + 0u * sbtChunkSize, sbtChunkSize, sbtChunkSize),
            vk::StridedDeviceAddressRegionKHR(shaderBindingTable.getAddress() + 1u * sbtChunkSize, sbtChunkSize, sbtChunkSize),
            vk::StridedDeviceAddressRegionKHR(shaderBindingTable.getAddress() + 2u * sbtChunkSize, sbtChunkSize, sbtChunkSize),
            vk::StridedDeviceAddressRegionKHR(0u, 0u, 0u)
    };
    commandBuffer.traceRaysKHR(strideAddresses[0],
                               strideAddresses[1],
                               strideAddresses[2],
                               strideAddresses[3],
                               windowWidth, windowHeight, 1);
}

void PathTracerApp::createAS(const vk::AccelerationStructureTypeKHR &type,
                             const vk::AccelerationStructureGeometryKHR &_geometry,
                             vk::utils::RTAccelerationStructure &_as) {
    vk::AccelerationStructureGeometryKHR geometry;
    if (type == vk::AccelerationStructureTypeKHR::eTopLevel) {
        vk::TransformMatrixKHR transform({{
                                                  {{1., 0., 0., 0.}},
                                                  {{0., 1., 0., 0.}},
                                                  {{0., 0., 1., 0.}}
                                          }});

        _as.instancesBuffer = {
                {{}, sizeof(vk::AccelerationStructureInstanceKHR), vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                                                   vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                                                                   vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR},
                vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent};

        vk::AccelerationStructureInstanceKHR instance(transform, 0, 0xff, 0,
                                                      vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable,
                                                      scene.bottomLevelAS[0].getAddress());

        _as.instancesBuffer.uploadData(&instance, sizeof(instance));

        vk::AccelerationStructureGeometryInstancesDataKHR instancesData(VK_FALSE,
                                                                        _as.instancesBuffer.getAddress());
        geometry = {vk::GeometryTypeKHR::eInstances, {instancesData}, {}};
    } else {
        geometry = _geometry;
    }

    vk::AccelerationStructureBuildGeometryInfoKHR geometryInfo(type,
                                                               {},
                                                               vk::BuildAccelerationStructureModeKHR::eBuild,
                                                               {},
                                                               {},
                                                               geometry);

    vk::AccelerationStructureBuildSizesInfoKHR sizeInfo = device.getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice, geometryInfo, 1);

    _as.buffer = {{
                          {},
                          sizeInfo.accelerationStructureSize,
                          vk::BufferUsageFlagBits::eShaderDeviceAddress |
                          vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                          vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                          vk::SharingMode::eExclusive},
                  vk::MemoryPropertyFlagBits::eDeviceLocal};

    _as.accelerationStructure = device.createAccelerationStructureKHR(
            {{},
             *_as.buffer.getBuffer(),
             {},
             sizeInfo.accelerationStructureSize,
             type,
             {}});
    geometryInfo.dstAccelerationStructure = *_as.accelerationStructure;

    vk::utils::Buffer scratchBuffer({
                                            {},
                                            sizeInfo.buildScratchSize,
                                            vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                            vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                                            vk::BufferUsageFlagBits::eStorageBuffer},
                                    vk::MemoryPropertyFlagBits::eDeviceLocal);
    geometryInfo.scratchData.deviceAddress = scratchBuffer.getAddress();

    std::vector<vk::raii::CommandBuffer> commandBuffers_(
            device.allocateCommandBuffers({*commandPool, vk::CommandBufferLevel::ePrimary, 1}));

    commandBuffers_[0].begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo(1, 0, 0, 0);
    commandBuffers_[0].buildAccelerationStructuresKHR(geometryInfo, &buildRangeInfo);

    commandBuffers_[0].end();
    graphicsQueue.submit(vk::SubmitInfo(VK_NULL_HANDLE, VK_NULL_HANDLE, *commandBuffers_[0], VK_NULL_HANDLE));
    graphicsQueue.waitIdle();

}

void PathTracerApp::createScene() {
    const float vertices[] = {
            0.25f, 0.25, 0.0f,
            0.75f, 0.25f, 0.0f,
            0.50f, 0.75f, 0.0f
    };

    const uint32_t indices[3] = {0, 1, 2};

    vk::utils::Buffer vertexBuffer({{}, vk::DeviceSize(sizeof(vertices)), vk::BufferUsageFlagBits::eVertexBuffer |
                                                                          vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                                                                          vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                                                          vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR},
                                   vk::MemoryPropertyFlagBits::eHostVisible |
                                   vk::MemoryPropertyFlagBits::eHostCoherent);

    vertexBuffer.uploadData(vertices, vertexBuffer.getSize());

    vk::utils::Buffer indexBuffer({{}, sizeof(indices), vk::BufferUsageFlagBits::eIndexBuffer |
                                                        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                                                        vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                                        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR},
                                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    indexBuffer.uploadData(indices, indexBuffer.getSize());

    vk::AccelerationStructureGeometryKHR geometry(vk::GeometryTypeKHR::eTriangles,
                                                  {{
                                                           vk::Format::eR32G32B32Sfloat,
                                                           vertexBuffer.getAddress(),
                                                           sizeof(float[3]),
                                                           3,
                                                           vk::IndexType::eUint32,
                                                           indexBuffer.getAddress()}}, // may need to be 3x4 unit matrix
                                                  vk::GeometryFlagBitsKHR::eOpaque);

    scene.bottomLevelAS.resize(1);

    createAS(vk::AccelerationStructureTypeKHR::eBottomLevel,
             geometry,
             scene.bottomLevelAS[0]);

    createAS(vk::AccelerationStructureTypeKHR::eTopLevel, {}, scene.topLevelAS);
}

void PathTracerApp::createRaytracingPipeline() {
    // acceleration structure and resulting image layout bindings
    std::vector<vk::DescriptorSetLayoutBinding> bindings({
                                                                 {0, vk::DescriptorType::eAccelerationStructureKHR, 1, vk::ShaderStageFlagBits::eRaygenKHR},
                                                                 {1, vk::DescriptorType::eStorageImage,             1, vk::ShaderStageFlagBits::eRaygenKHR}
                                                         });

    descriptorSetLayoutRT = device.createDescriptorSetLayout({{}, bindings});
    pipelineLayoutRT = device.createPipelineLayout({{}, *descriptorSetLayoutRT});

    vk::utils::Shader rayGenShader("../shaders_/rayGen.bin", vk::ShaderStageFlagBits::eRaygenKHR);
    vk::utils::Shader rayMissShader("../shaders_/rayMiss.bin", vk::ShaderStageFlagBits::eMissKHR);
    vk::utils::Shader rayChitShader("../shaders_/rayChit.bin", vk::ShaderStageFlagBits::eClosestHitKHR);

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages{
            rayGenShader.getShaderStage(),
            rayMissShader.getShaderStage(),
            rayChitShader.getShaderStage()

    };
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups = {
            {vk::RayTracingShaderGroupTypeKHR::eGeneral, 0, VK_SHADER_UNUSED_KHR,           VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
            {vk::RayTracingShaderGroupTypeKHR::eGeneral, 1, VK_SHADER_UNUSED_KHR,           VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
            {vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup, VK_SHADER_UNUSED_KHR, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR}
    };

    pipelineRT = device.createRayTracingPipelineKHR(VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                    {{}, shaderStages, shaderGroups, 1, {}, {}, {}, *pipelineLayoutRT});
}

void PathTracerApp::createShaderBindingTable() {
    uint32_t sbtChunkSize =
            (pipelinePropertiesRT.shaderGroupHandleSize + (pipelinePropertiesRT.shaderGroupBaseAlignment - 1)) &
            (~(pipelinePropertiesRT.shaderGroupBaseAlignment - 1));

    const uint32_t numGroups = 3;
    const uint32_t shaderBindingTableSize = pipelinePropertiesRT.shaderGroupHandleSize * numGroups;
    const uint32_t shaderBindingTableSizeAligned = sbtChunkSize * numGroups;

    shaderBindingTable = {{{}, shaderBindingTableSizeAligned,
                           {vk::BufferUsageFlagBits::eTransferSrc |
                            vk::BufferUsageFlagBits::eShaderBindingTableKHR |
                            vk::BufferUsageFlagBits::eShaderDeviceAddress}},
                          vk::MemoryPropertyFlagBits::eHostVisible};

    auto shaderGroupHandles = pipelineRT.getRayTracingShaderGroupHandlesKHR<uint8_t>(0, numGroups,
                                                                                     shaderBindingTableSize);
    std::vector<uint8_t> shaderGroupHandlesAligned(shaderBindingTableSizeAligned);
    for (int i = 0; i < numGroups; ++i)
        shaderGroupHandlesAligned[i * sbtChunkSize] = shaderGroupHandles[i * pipelinePropertiesRT.shaderGroupHandleSize];

    shaderBindingTable.uploadData(shaderGroupHandlesAligned.data(), shaderBindingTableSizeAligned);
}

void PathTracerApp::createDescriptorSet() {
    std::vector<vk::DescriptorPoolSize> poolSizes({
                                                          {vk::DescriptorType::eAccelerationStructureKHR, 1},
                                                          {vk::DescriptorType::eStorageImage,             1}
                                                  });

    //flag vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet ??
    descriptorPoolRT = device.createDescriptorPool({vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, poolSizes});

    descriptorSetRT = std::move(device.allocateDescriptorSets({*descriptorPoolRT, *descriptorSetLayoutRT}).front());

    vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructure(1,
                                                                                   &*scene.topLevelAS.accelerationStructure);

    vk::WriteDescriptorSet accelerationStructureWrite(*descriptorSetRT, 0, 0, 1,
                                                      vk::DescriptorType::eAccelerationStructureKHR);
    accelerationStructureWrite.pNext = &descriptorAccelerationStructure;

    vk::DescriptorImageInfo descriptorOutputImageInfo(VK_NULL_HANDLE, *offscreenImage.getImageView(),
                                                      vk::ImageLayout::eGeneral);

    vk::WriteDescriptorSet resultImageWrite(*descriptorSetRT, 1, 0, 1, vk::DescriptorType::eStorageImage,
                                            &descriptorOutputImageInfo);

    std::vector<vk::WriteDescriptorSet> descriptorWrites({accelerationStructureWrite, resultImageWrite});

    device.updateDescriptorSets(descriptorWrites, VK_NULL_HANDLE);
}

void PathTracerApp::drawFrame(const float dt) {
    auto result = swapchain.acquireNextImage(UINT64_MAX, *semaphoreImageAvailable);
    check_vk_result(result.first);

    uint32_t imageIndex = result.second;

    const vk::Fence &fence = *waitForFrameFences[imageIndex];
    auto error = device.waitForFences(fence, VK_TRUE, UINT64_MAX);
    check_vk_result(error);

    device.resetFences(fence);

    vk::PipelineStageFlags waitStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    graphicsQueue.submit(vk::SubmitInfo(*semaphoreImageAvailable,
                                        waitStageMask,
                                        *commandBuffers[imageIndex],
                                        *semaphoreRenderFinished),
                         fence);

    error = graphicsQueue.presentKHR(vk::PresentInfoKHR(*semaphoreRenderFinished,
                                                        *swapchain,
                                                        imageIndex,
                                                        {}));
    check_vk_result(error);
}