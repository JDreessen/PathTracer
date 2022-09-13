//
// Created by JDreessen on 19.02.2022.
//

#include "PathTracerApp.hpp"
#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION

#include "lib/tinyobjloader/tiny_obj_loader.h"

PathTracerApp::PathTracerApp()
        : window(), settings(), vkInstance(VK_NULL_HANDLE), physicalDevice(VK_NULL_HANDLE),
          device(VK_NULL_HANDLE), glfwSurface(VK_NULL_HANDLE), surfaceFormat(), surface(VK_NULL_HANDLE),
          queueFamilyIndices{~0u, ~0u, ~0u}  // max uint32_t
        , swapchain(VK_NULL_HANDLE), swapchainImages(), swapchainImageViews(), waitForFrameFences(),
          graphicsPool(VK_NULL_HANDLE), computePool(VK_NULL_HANDLE), computeCommandBuffer(VK_NULL_HANDLE),
          semaphoreImageAvailable(VK_NULL_HANDLE), semaphoreRenderFinished(VK_NULL_HANDLE),
          graphicsQueue(VK_NULL_HANDLE), computeQueue(VK_NULL_HANDLE), transferQueue(VK_NULL_HANDLE),
          descriptorSetLayouts{}, pipelineLayout(VK_NULL_HANDLE), pipelineRT(VK_NULL_HANDLE),
          descriptorPoolRayGen(VK_NULL_HANDLE), descriptorPoolCHit(VK_NULL_HANDLE), descriptorSets{},
          shaderBindingTable(), scene(), frameData(),
          cameraDelta{0, 0, 0}, frameDataBuffer() {}

void PathTracerApp::run() {
    initSettings();
    initGLFW();
    initVulkan();
    initDevicesAndQueues();
    initSurface();
    initSwapchain();
    initSyncObjects();
    vk::utils::Initialize(&physicalDevice, &device, &graphicsPool, &transferQueue);
    initImages();
    initCommandPoolAndBuffers();

    createScene();
    createRaytracingPipeline();
    createShaderBindingTable();
    createDescriptorSets();

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
    double currentTime, previousTime = 0, deltaTime;
    while (!glfwWindowShouldClose(window)) {
        currentTime = glfwGetTime();
        deltaTime = currentTime - previousTime;
        previousTime = currentTime;

        drawFrame(static_cast<float>(deltaTime));

        glfwPollEvents();
    }
}

//void PathTracerApp::initSettings(std::string appName = "PathTracer", uint32_t width = 800, uint32_t height = 600, std::string modelName = "cornell_box", uint32_t maxRecursionDepth = 31) {
void PathTracerApp::initSettings() {
    settings.name = "PathTracer";
    settings.windowWidth = 1280;
    settings.windowHeight = 720;
    settings.modelName = "cornell_box";
    settings.maxRecursionDepth = 16;

    camera = Camera({275, 275, 1}, {0, 0, 1}, {0, 1, 0}, 0.1f, 1000.0f, 90.0f);

    frameData.cameraPos = {275, 275, 1, 1};
    frameData.cameraDir = {0, 0, 1, 1};
    frameData.cameraUp = {0, 0, 1, 1};
    frameData.cameraSide = {1, 0, 0, 1};
    frameData.cameraNearFarFOV = {0, 1000, 90, 1};
    frameData.frameID = {0, 0, 0, 1};
}

void PathTracerApp::initGLFW() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(static_cast<int>(settings.windowWidth),
                              static_cast<int>(settings.windowHeight),
                              settings.name.c_str(),
                              nullptr,
                              nullptr);
    glfwSetKeyCallback(window, [](GLFWwindow *callbackWindow, int key, int scancode, int action, int mods) {
        PathTracerApp::instance().keyCallback(callbackWindow, key, scancode, action, mods);
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

    vk::ApplicationInfo applicationInfo = vk::ApplicationInfo(settings.name.c_str(),
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

    pipelineProperties = props.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
    settings.maxRecursionDepth = std::min(pipelineProperties.maxRayRecursionDepth, settings.maxRecursionDepth);
    std::cout << "Max ray recursion depth: " << settings.maxRecursionDepth << std::endl;
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
    settings.windowWidth = std::clamp(settings.windowWidth, surfaceCapabilities.minImageExtent.width,
                                      surfaceCapabilities.maxImageExtent.width);
    settings.windowHeight = std::clamp(settings.windowHeight, surfaceCapabilities.minImageExtent.height,
                                       surfaceCapabilities.maxImageExtent.height);

    // since we only care about rendering static images, present mode doesn't matter so FIFO is fine
    vk::SwapchainCreateInfoKHR swapchainCreateInfo({},
                                                   *surface,
                                                   surfaceCapabilities.minImageCount,
                                                   surfaceFormat.format,
                                                   surfaceFormat.colorSpace,
                                                   {settings.windowWidth, settings.windowHeight},
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
    swapchain = device.createSwapchainKHR(swapchainCreateInfo);

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
    for (const auto &e: swapchainImages)
        waitForFrameFences.emplace_back(device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));

    graphicsPool = device.createCommandPool(vk::CommandPoolCreateInfo());

    semaphoreImageAvailable = device.createSemaphore(vk::SemaphoreCreateInfo());
    semaphoreRenderFinished = device.createSemaphore(vk::SemaphoreCreateInfo());
}

void PathTracerApp::initImages() {
    resultImage = vk::utils::Image(vk::ImageType::e2D,
                                   surfaceFormat.format,
                                   {settings.windowWidth, settings.windowHeight, 1},
                                   vk::ImageTiling::eOptimal,
                                   vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
                                   vk::MemoryPropertyFlagBits::eDeviceLocal);

    resultImage.createImageView(vk::ImageViewType::e2D,
                                surfaceFormat.format,
                                {vk::ImageAspectFlagBits::eColor,
                                 0,
                                 1,
                                 0,
                                 1});
}

void PathTracerApp::initCommandPoolAndBuffers() {
    graphicsPool = device.createCommandPool(
            vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                      queueFamilyIndices[vk::utils::QueueFamilyIndex::graphics]));
    computePool = device.createCommandPool(
            vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                      queueFamilyIndices[vk::utils::QueueFamilyIndex::compute]));

    commandBuffers = device.allocateCommandBuffers({*graphicsPool,
                                                    vk::CommandBufferLevel::ePrimary,
                                                    static_cast<uint32_t>(swapchainImages.size()) + 1});
    computeCommandBuffer = std::move(device.allocateCommandBuffers({*computePool,
                                                                    vk::CommandBufferLevel::ePrimary,
                                                                    1}).front());
}

void PathTracerApp::fillCommandBuffers() {
    for (size_t i = 0; i < commandBuffers.size() - 1; ++i) {
        const vk::raii::CommandBuffer &commandBuffer = commandBuffers[i];
        commandBuffer.begin({});

        commandBuffer.pushConstants<uint32_t>(*pipelineLayout, vk::ShaderStageFlagBits::eClosestHitKHR, 0,
                                              {settings.maxRecursionDepth});

        vk::utils::imageBarrier(commandBuffer,
                                *resultImage.getImage(),
                                {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
                                {},
                                vk::AccessFlagBits::eShaderWrite,
                                vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eGeneral);

        fillCommandBuffer(commandBuffer);

        vk::utils::imageBarrier(commandBuffer,
                                swapchainImages[i],
                                {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
                                {},
                                vk::AccessFlagBits::eTransferWrite,
                                vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eTransferDstOptimal);

        vk::utils::imageBarrier(commandBuffer,
                                *resultImage.getImage(),
                                {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
                                vk::AccessFlagBits::eShaderWrite,
                                vk::AccessFlagBits::eTransferRead,
                                vk::ImageLayout::eGeneral,
                                vk::ImageLayout::eTransferSrcOptimal);

        vk::ImageCopy copyRegion({vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                 {0, 0, 0},
                                 {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                 {0, 0, 0},
                                 {settings.windowWidth, settings.windowHeight, 1});

        commandBuffer.copyImage(*resultImage.getImage(),
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

void PathTracerApp::fillCommandBuffer(const vk::raii::CommandBuffer &commandBuffer) {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *pipelineRT);

    std::vector<vk::DescriptorSet> sets{};
    std::for_each(descriptorSets.begin(), descriptorSets.end(),
                  [&sets](const auto &e) { sets.push_back(*e); });

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *pipelineLayout, 0, sets, {});

    // our shader binding table layout:
    // |[ raygen ]|[miss]|[closest hit]|
    // | 0        | 1    | 2           |

    uint32_t sbtChunkSize =
            (pipelineProperties.shaderGroupHandleSize + (pipelineProperties.shaderGroupBaseAlignment - 1)) &
            (~(pipelineProperties.shaderGroupBaseAlignment - 1));

    std::array strideAddresses{
            vk::StridedDeviceAddressRegionKHR(shaderBindingTable.getAddress() + 0u * sbtChunkSize, sbtChunkSize,
                                              sbtChunkSize),
            vk::StridedDeviceAddressRegionKHR(shaderBindingTable.getAddress() + 1u * sbtChunkSize, sbtChunkSize,
                                              sbtChunkSize),
            vk::StridedDeviceAddressRegionKHR(shaderBindingTable.getAddress() + 2u * sbtChunkSize, sbtChunkSize,
                                              sbtChunkSize),
            vk::StridedDeviceAddressRegionKHR(0u, 0u, 0u)
    };
    commandBuffer.traceRaysKHR(strideAddresses[0],
                               strideAddresses[1],
                               strideAddresses[2],
                               strideAddresses[3],
                               settings.windowWidth, settings.windowHeight, 1);
}

// create either a bottom level acceleration structure containing geometries or
// a top level acceleration structure containing all bottom level instances
void PathTracerApp::createAS(const vk::AccelerationStructureTypeKHR &type,
                             const vk::AccelerationStructureGeometryKHR &_geometry,
                             vk::utils::RTAccelerationStructure &_as) {
    vk::AccelerationStructureGeometryKHR geometry;
    std::size_t geometryCount = 0;

    if (type == vk::AccelerationStructureTypeKHR::eBottomLevel) {
        geometry = _geometry;
        geometryCount = geometry.geometry.triangles.maxVertex;
    } else if (type == vk::AccelerationStructureTypeKHR::eTopLevel) {
        vk::TransformMatrixKHR transform({{
                                                  {{1., 0., 0., 0.}},
                                                  {{0., 1., 0., 0.}},
                                                  {{0., 0., 1., 0.}}
                                          }});

        std::vector<vk::AccelerationStructureInstanceKHR> instances;
        for (const auto &bottomLevelAS: scene.bottomLevelAS)
            instances.emplace_back(transform, 0, 0xff, 0,
                                   vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable,
                                   bottomLevelAS.getAddress());
        geometryCount = instances.size();

        _as.instancesBuffer = {
                {{}, sizeof(instances[0]) * instances.size(), vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                                              vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                                                              vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR},
                vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent};

        _as.instancesBuffer.uploadData(instances.data(), sizeof(instances[0]) * instances.size());

        vk::AccelerationStructureGeometryInstancesDataKHR instancesData(VK_FALSE,
                                                                        _as.instancesBuffer.getAddress());
        geometry = {vk::GeometryTypeKHR::eInstances, {instancesData}, {}};
    }

    vk::AccelerationStructureBuildGeometryInfoKHR geometryInfo(type,
                                                               {},
                                                               vk::BuildAccelerationStructureModeKHR::eBuild,
                                                               {},
                                                               {},
                                                               geometry);

    vk::AccelerationStructureBuildSizesInfoKHR sizeInfo = device.getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice, geometryInfo, geometryCount);

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
             type});
    geometryInfo.dstAccelerationStructure = *_as.accelerationStructure;

    vk::utils::Buffer scratchBuffer({
                                            {},
                                            sizeInfo.buildScratchSize,
                                            vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                            vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                                            vk::BufferUsageFlagBits::eStorageBuffer},
                                    vk::MemoryPropertyFlagBits::eDeviceLocal);
    geometryInfo.scratchData.deviceAddress = scratchBuffer.getAddress();

    commandBuffers[0].begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo(geometryCount, 0, 0, 0);
    commandBuffers[0].buildAccelerationStructuresKHR(geometryInfo, &buildRangeInfo);

    commandBuffers[0].end();
    graphicsQueue.submit(vk::SubmitInfo(VK_NULL_HANDLE, VK_NULL_HANDLE, *commandBuffers[0], VK_NULL_HANDLE));
    graphicsQueue.waitIdle();
}

// load scene data from obj file into acceleration structure
void PathTracerApp::createScene() {
    frameDataBuffer = {{{}, sizeof(frameData), vk::BufferUsageFlagBits::eUniformBuffer},
                       vk::MemoryPropertyFlagBits::eHostVisible};
    frameDataBuffer.uploadData(&frameData, sizeof(frameData));

    std::string fileName = "../models/" + settings.modelName + ".obj";
    tinyobj::ObjReaderConfig readerConfig;
    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(fileName, readerConfig)) {
        if (!reader.Error().empty())
            std::cerr << "TinyObjReader: " << reader.Error();
        exit(1);
    }

    if (!reader.Warning().empty())
        std::cout << "TinyObjReader: " << reader.Warning();

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    auto &materials = reader.GetMaterials();

    std::unordered_map<glm::vec4, uint32_t> uniqueVertices{};

    for (const auto &shape: shapes) {
        std::vector<glm::vec4> vertices{};
        std::vector<uint32_t> indices{};
        std::vector<Material> newMaterials{};
        for (const auto &index: shape.mesh.indices) {
            glm::vec4 vertex{
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2],
                    1
            };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }
        for (const auto &index: shape.mesh.material_ids) {
            Material material{};
            material.emittance = {materials[index].ambient[0],
                                  materials[index].ambient[1],
                                  materials[index].ambient[2],
                                  0.f};
            material.reflectance = {materials[index].diffuse[0],
                                    materials[index].diffuse[1],
                                    materials[index].diffuse[2],
                                    materials[index].shininess};
            newMaterials.push_back(material);
        }
        scene.vertexBuffers.emplace_back(
                vk::BufferCreateInfo({}, sizeof(glm::vec4) * vertices.size(), vk::BufferUsageFlagBits::eStorageBuffer |
                                                                              vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                                                                              vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                                                              vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR),
                vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent);

        scene.vertexBuffers.back().uploadData(vertices.data(), scene.vertexBuffers.back().getSize());

        scene.indexBuffers.emplace_back(
                vk::BufferCreateInfo({}, sizeof(uint32_t) * indices.size(), vk::BufferUsageFlagBits::eStorageBuffer |
                                                                            vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                                                                            vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                                                            vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR),
                vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent);

        scene.indexBuffers.back().uploadData(indices.data(), scene.indexBuffers.back().getSize());

        scene.materialBuffers.emplace_back(
                vk::BufferCreateInfo({}, sizeof(Material) * newMaterials.size(),
                                     vk::BufferUsageFlagBits::eStorageBuffer |
                                     vk::BufferUsageFlagBits::eShaderDeviceAddress),
                vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent);

        scene.materialBuffers.back().uploadData(newMaterials.data(), scene.materialBuffers.back().getSize());

        vk::AccelerationStructureGeometryKHR geometry(vk::GeometryTypeKHR::eTriangles,
                                                      {{
                                                               vk::Format::eR32G32B32A32Sfloat,
                                                               scene.vertexBuffers.back().getAddress(),
                                                               sizeof(glm::vec4),
                                                               static_cast<uint32_t>(indices.size()),
                                                               vk::IndexType::eUint32,
                                                               scene.indexBuffers.back().getAddress()}},
                                                      vk::GeometryFlagBitsKHR::eOpaque);

        scene.bottomLevelAS.emplace_back();

        createAS(vk::AccelerationStructureTypeKHR::eBottomLevel,
                 geometry,
                 scene.bottomLevelAS.back());
    }
    createAS(vk::AccelerationStructureTypeKHR::eTopLevel, {}, scene.topLevelAS);
}

// create raytracing pipeline with shaders and associated data
void PathTracerApp::createRaytracingPipeline() {
    // acceleration structure and resulting image layout bindings
    std::vector<vk::DescriptorSetLayoutBinding> bindingsRayGen{
            {0, vk::DescriptorType::eAccelerationStructureKHR, 1,
                                                                  vk::ShaderStageFlagBits::eRaygenKHR |
                                                                  vk::ShaderStageFlagBits::eClosestHitKHR},
            {1, vk::DescriptorType::eStorageImage,             1, vk::ShaderStageFlagBits::eRaygenKHR},
            {2, vk::DescriptorType::eUniformBuffer,            1, vk::ShaderStageFlagBits::eRaygenKHR}
    };
    std::vector<vk::DescriptorSetLayoutBinding> bindingVertexBuffer{
            {0, vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(scene.vertexBuffers.size()),
             vk::ShaderStageFlagBits::eClosestHitKHR}};
    std::vector<vk::DescriptorSetLayoutBinding> bindingIndexBuffer{
            {0, vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(scene.indexBuffers.size()),
             vk::ShaderStageFlagBits::eClosestHitKHR}};
    std::vector<vk::DescriptorSetLayoutBinding> bindingMaterialBuffer{
            {0, vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(scene.materialBuffers.size()),
             vk::ShaderStageFlagBits::eClosestHitKHR}};

    descriptorSetLayouts.push_back(device.createDescriptorSetLayout({{}, bindingsRayGen}));
    descriptorSetLayouts.push_back(device.createDescriptorSetLayout({{}, bindingVertexBuffer}));
    descriptorSetLayouts.push_back(device.createDescriptorSetLayout({{}, bindingIndexBuffer}));
    descriptorSetLayouts.push_back(device.createDescriptorSetLayout({{}, bindingMaterialBuffer}));

    std::vector<vk::DescriptorSetLayout> layouts{};
    std::for_each(descriptorSetLayouts.begin(), descriptorSetLayouts.end(),
                  [&layouts](const auto &e) { layouts.push_back(*e); });

    vk::PushConstantRange pushConstantRange(vk::ShaderStageFlagBits::eClosestHitKHR, 0, sizeof(uint32_t));

    pipelineLayout = device.createPipelineLayout({{}, layouts, pushConstantRange});

    vk::utils::Shader rayGenShader("../shaderBin/rayGen.bin", vk::ShaderStageFlagBits::eRaygenKHR);
    vk::utils::Shader rayMissShader("../shaderBin/rayMiss.bin", vk::ShaderStageFlagBits::eMissKHR);
    vk::utils::Shader rayChitShader("../shaderBin/rayChit.bin", vk::ShaderStageFlagBits::eClosestHitKHR);

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
                                                    {{}, shaderStages, shaderGroups, settings.maxRecursionDepth, {}, {},
                                                     {}, *pipelineLayout});
}

// create the shader binding table structure containing the shader groups and their respective shaders
void PathTracerApp::createShaderBindingTable() {
    uint32_t sbtChunkSize =
            (pipelineProperties.shaderGroupHandleSize + (pipelineProperties.shaderGroupBaseAlignment - 1)) &
            (~(pipelineProperties.shaderGroupBaseAlignment - 1));

    const uint32_t numGroups = 3;
    const uint32_t shaderBindingTableSize = pipelineProperties.shaderGroupHandleSize * numGroups;
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
        shaderGroupHandlesAligned[i * sbtChunkSize] = shaderGroupHandles[i *
                                                                         pipelineProperties.shaderGroupHandleSize];

    shaderBindingTable.uploadData(shaderGroupHandlesAligned.data(), shaderBindingTableSizeAligned);
}

// create descriptor sets containing data to be transmitted between CPU and GPU
void PathTracerApp::createDescriptorSets() {
    // Not sure if two pools are necessary
    std::vector<vk::DescriptorPoolSize> poolSizesRayGen{
            {vk::DescriptorType::eAccelerationStructureKHR, 1},
            {vk::DescriptorType::eStorageImage,             1},
            {vk::DescriptorType::eUniformBuffer,            1}
    };
    std::vector<vk::DescriptorPoolSize> poolSizesCHit{
            {vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(scene.bottomLevelAS.size())}};
    // Validation layers want the freeDescriptorSet flag to be set for destroying pools when exiting
    descriptorPoolRayGen = device.createDescriptorPool(
            {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, poolSizesRayGen});
    descriptorPoolCHit = device.createDescriptorPool(
            {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 4, poolSizesCHit});

    std::vector<vk::DescriptorSetLayout> layoutsCHit{*descriptorSetLayouts[1], *descriptorSetLayouts[2],
                                                     *descriptorSetLayouts[3]};

    descriptorSets.push_back(
            std::move(device.allocateDescriptorSets({*descriptorPoolRayGen, *descriptorSetLayouts[0]}).front()));
    auto setsCHit = device.allocateDescriptorSets({*descriptorPoolCHit, layoutsCHit});
    for (auto &set: setsCHit)
        descriptorSets.push_back(std::move(set));

    // set 0, binding 0: top acceleration structure
    vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructure(1,
                                                                                   &*scene.topLevelAS.accelerationStructure);
    vk::WriteDescriptorSet accelerationStructureWrite(*descriptorSets[0], 0, 0, 1,
                                                      vk::DescriptorType::eAccelerationStructureKHR);
    accelerationStructureWrite.pNext = &descriptorAccelerationStructure;

    // set 0, binding 1: result image
    vk::DescriptorImageInfo descriptorOutputImageInfo(VK_NULL_HANDLE, *resultImage.getImageView(),
                                                      vk::ImageLayout::eGeneral);
    vk::WriteDescriptorSet resultImageWrite(*descriptorSets[0], 1, 0, 1, vk::DescriptorType::eStorageImage,
                                            &descriptorOutputImageInfo);

    // set 0, binding 2: instance data buffer
    vk::DescriptorBufferInfo descriptorFrameDataBufferInfo(*frameDataBuffer.getBuffer(), 0, frameDataBuffer.getSize());
    vk::WriteDescriptorSet frameDataWrite(*descriptorSets[0], 2, 0, vk::DescriptorType::eUniformBuffer, {},
                                          descriptorFrameDataBufferInfo);

    // set 1, binding 0: vertex buffers for each instance (shape)
    std::vector<vk::DescriptorBufferInfo> descriptorVertexBufferInfos{};
    for (const auto &buffer: scene.vertexBuffers)
        descriptorVertexBufferInfos.emplace_back(*buffer.getBuffer(), 0, buffer.getSize());
    vk::WriteDescriptorSet vertexWrite(*descriptorSets[1], 0, 0, vk::DescriptorType::eStorageBuffer, {},
                                       descriptorVertexBufferInfos);

    // set 2, binding 0: index buffers for each instance (shape)
    std::vector<vk::DescriptorBufferInfo> descriptorIndexBufferInfos{};
    for (const auto &buffer: scene.indexBuffers)
        descriptorIndexBufferInfos.emplace_back(*buffer.getBuffer(), 0, buffer.getSize());
    vk::WriteDescriptorSet indexWrite(*descriptorSets[2], 0, 0, vk::DescriptorType::eStorageBuffer, {},
                                      descriptorIndexBufferInfos);

    // set 3, binding 0: material buffers for each instance (shape)
    std::vector<vk::DescriptorBufferInfo> descriptorMaterialBufferInfos{};
    for (const auto &buffer: scene.materialBuffers)
        descriptorMaterialBufferInfos.emplace_back(*buffer.getBuffer(), 0, buffer.getSize());
    vk::WriteDescriptorSet materialWrite(*descriptorSets[3], 0, 0, vk::DescriptorType::eStorageBuffer, {},
                                         descriptorMaterialBufferInfos);

    std::vector<vk::WriteDescriptorSet> descriptorWrites{accelerationStructureWrite,
                                                         resultImageWrite, frameDataWrite,
                                                         vertexWrite, indexWrite, materialWrite};

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

    update(dt);

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
    glfwSetWindowTitle(window,std::to_string(frameData.frameID.x++).c_str());
}

void PathTracerApp::update(const float dt) {
    const float movementSpeed = 300;
    // add camera movemen
    //TODO: remove cameraDir in calculaion
    frameData.cameraPos += dt * glm::vec4(movementSpeed * cameraDelta, 0) * frameData.cameraDir;

    // reset image on camera movement
    if (cameraDelta != glm::vec3(0))
        frameData.frameID.x = 0;

    frameDataBuffer.uploadData(&frameData, sizeof(frameData));
}

void PathTracerApp::keyCallback(GLFWwindow *callbackWindow, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(callbackWindow, 1);
        else if (key == GLFW_KEY_A) cameraDelta.x = -0.1;
        else if (key == GLFW_KEY_D) cameraDelta.x = 0.1;
        else if (key == GLFW_KEY_W) cameraDelta.y = -0.1;
        else if (key == GLFW_KEY_S) cameraDelta.y = 0.1;
        else if (key == GLFW_KEY_Q) cameraDelta.z = -0.1;
        else if (key == GLFW_KEY_E) cameraDelta.z = 0.1;
    } else if (action == GLFW_RELEASE) {
        if (key == GLFW_KEY_A || key == GLFW_KEY_D) cameraDelta.x = 0;
        else if (key == GLFW_KEY_W || key == GLFW_KEY_S) cameraDelta.y = 0;
        else if (key == GLFW_KEY_Q || key == GLFW_KEY_E) cameraDelta.z = 0;
        else if (key == GLFW_KEY_P) exportImage();
    }
}

void PathTracerApp::exportImage() {
    std::ofstream file(
            std::string("../screenshots/") + settings.modelName + '-' + std::to_string(frameData.frameID.x) + ".ppm",
            std::ios::binary);
    if (!file) {
        std::cerr << "Could not open file for writing" << std::endl;
        return;
    }

    vk::utils::Image screenshot(vk::ImageType::e2D, vk::Format::eR8G8B8A8Unorm,
                                {settings.windowWidth, settings.windowHeight, 1}, vk::ImageTiling::eLinear,
                                vk::ImageUsageFlagBits::eTransferDst,
                                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    computeCommandBuffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    vk::utils::imageBarrier(computeCommandBuffer,
                            *resultImage.getImage(),
                            {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
                            {},
                            vk::AccessFlagBits::eShaderWrite,
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eTransferSrcOptimal);

    vk::utils::imageBarrier(computeCommandBuffer,
                            *screenshot.getImage(),
                            {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
                            {},
                            vk::AccessFlagBits::eShaderWrite,
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eTransferDstOptimal);

    vk::ImageCopy copyRegion({vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                             {0, 0, 0},
                             {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                             {0, 0, 0},
                             {settings.windowWidth, settings.windowHeight, 1});

    computeCommandBuffer.copyImage(*resultImage.getImage(),
                                   vk::ImageLayout::eTransferSrcOptimal,
                                   *screenshot.getImage(),
                                   vk::ImageLayout::eTransferDstOptimal, copyRegion);
    computeCommandBuffer.end();

    computeQueue.submit(vk::SubmitInfo(VK_NULL_HANDLE, VK_NULL_HANDLE, *computeCommandBuffer, VK_NULL_HANDLE));
    computeQueue.waitIdle();

    const void *data = screenshot.getDeviceMemory().mapMemory(0, VK_WHOLE_SIZE);
    file << "P6\n" << settings.windowWidth << " " << settings.windowHeight << "\n255\n";

    for (int i = 0; i < settings.windowWidth * settings.windowHeight * 4; i += 4) {
        file << static_cast<const uint8_t *>(data)[i + 2];
        file << static_cast<const uint8_t *>(data)[i + 1];
        file << static_cast<const uint8_t *>(data)[i + 0];
    }
    //file.write(reinterpret_cast<const char *>(data), windowWidth * windowHeight * 3);

    screenshot.getDeviceMemory().unmapMemory();
}
