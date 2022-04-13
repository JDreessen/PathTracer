//
// Created by JDreessen on 19.02.2022.
//

#include "VulkanUtils.hpp"
#include <iostream>
#include <fstream>

void check_vk_result(vk::Result err) {
    if (err != vk::Result::eSuccess) {
        std::cerr << "Vulkan error " << vk::to_string(err);
        // abort if critical
        if (static_cast<VkResult>(err) < 0) abort();
    }
}

void check_vk_result(VkResult err) { check_vk_result(static_cast<vk::Result>(err)); }

namespace vk::utils {
    void Initialize(vk::raii::PhysicalDevice *physicalDevice,
                    vk::raii::Device *device,
                    vk::raii::CommandPool *commandPool,
                    vk::raii::Queue *transferQueue) {
        context::physicalDevice = physicalDevice;
        context::device = device;
        context::commandPool = commandPool;
        context::transferQueue = transferQueue;

        context::physicalMemoryProperties = physicalDevice->getMemoryProperties();
    }

    // check whether ExtensionProperties vector contains specific extension
    bool contains(const std::vector<vk::ExtensionProperties> &extensionProperties, const std::string &extensionName) {
        auto propertyIterator = std::find_if(extensionProperties.begin(),
                                             extensionProperties.end(),
                                             [&extensionName](const vk::ExtensionProperties &ep) {
                                                 return extensionName == ep.extensionName;
                                             });
        return (propertyIterator != extensionProperties.end());
    }

    uint32_t getMemoryType(vk::MemoryRequirements memoryRequirements, vk::MemoryPropertyFlags memoryProperties) {
        uint32_t result = 0;
        for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < VK_MAX_MEMORY_TYPES; ++memoryTypeIndex) {
            if (memoryRequirements.memoryTypeBits & (1 << memoryTypeIndex)) {
                if ((context::physicalMemoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & memoryProperties) ==
                    memoryProperties) {
                    result = memoryTypeIndex;
                    break;
                }
            }
        }
        return result;
    }

    void imageBarrier(const vk::raii::CommandBuffer &commandBuffer,
                      VkImage image,
                      const vk::ImageSubresourceRange &subresourceRange,
                      const vk::AccessFlags &srcAccessMask,
                      const vk::AccessFlags &dstAccessMask,
                      const ImageLayout &oldLayout,
                      const ImageLayout &newLayout) {
        vk::ImageMemoryBarrier imageMemoryBarrier(srcAccessMask,
                                                  dstAccessMask,
                                                  oldLayout,
                                                  newLayout,
                                                  VK_QUEUE_FAMILY_IGNORED,
                                                  VK_QUEUE_FAMILY_IGNORED,
                                                  image,
                                                  subresourceRange);

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                                      vk::PipelineStageFlagBits::eAllCommands,
                                      {},
                                      nullptr,
                                      nullptr,
                                      imageMemoryBarrier);
    }

    Buffer::Buffer() : buffer(VK_NULL_HANDLE), memory(VK_NULL_HANDLE), size() {}

    vk::utils::Buffer::Buffer(vk::BufferCreateInfo bufferCreateInfo, vk::MemoryPropertyFlags memoryProperties) : size(
            bufferCreateInfo.size),
                                                                                                                 buffer(context::device->createBuffer(
                                                                                                                         bufferCreateInfo)),
                                                                                                                 memory(VK_NULL_HANDLE) {
        vk::MemoryRequirements memoryRequirements = buffer.getMemoryRequirements();

        vk::MemoryAllocateInfo allocateInfo(memoryRequirements.size,
                                            getMemoryType(memoryRequirements, memoryProperties));

        vk::MemoryAllocateFlagsInfo memoryAllocateFlagsInfo(vk::MemoryAllocateFlagBits::eDeviceAddress);
        allocateInfo.pNext = &memoryAllocateFlagsInfo;

        memory = std::move(context::device->allocateMemory(
                allocateInfo));

        buffer.bindMemory(*memory, 0);
    }

    // map memory region to void pointer
    void *vk::utils::Buffer::map(vk::DeviceSize size, vk::DeviceSize offset) const {
        size = std::min(this->size, size); // we can't map more than is allocated
        return memory.mapMemory(offset, size);
    }

    // unmap mapped memory region
    void vk::utils::Buffer::unmap() const { memory.unmapMemory(); }

    // copy data to memory region
    void vk::utils::Buffer::uploadData(const void *data, vk::DeviceSize size, vk::DeviceSize offset) const {
        void *mem = this->map(size, offset);
        memcpy(mem, data, size);
        this->unmap();
    }

    const vk::raii::Buffer &Buffer::getBuffer() const { return buffer; }

    const vk::DeviceSize &Buffer::getSize() const { return size; }

    vk::DeviceAddress Buffer::getAddress() const {
        return context::device->getBufferAddress({*buffer});
    }

    Image::Image()
            : format(), image(VK_NULL_HANDLE), imageView(VK_NULL_HANDLE), memory(VK_NULL_HANDLE),
              sampler(VK_NULL_HANDLE) {}

    Image::Image(vk::ImageType imageType, vk::Format format, vk::Extent3D extent, vk::ImageTiling tiling,
                 vk::ImageUsageFlags usage, vk::MemoryPropertyFlags memoryProperties) : format(format),
                                                                                        memory(VK_NULL_HANDLE),
                                                                                        image(VK_NULL_HANDLE),
                                                                                        imageView(VK_NULL_HANDLE),
                                                                                        sampler(VK_NULL_HANDLE) {
        image = context::device->createImage({{},
                                              imageType,
                                              this->format,
                                              extent,
                                              1,
                                              1,
                                              vk::SampleCountFlagBits::e1,
                                              tiling,
                                              usage,
                                              vk::SharingMode::eExclusive});

        vk::MemoryRequirements memoryRequirements = image.getMemoryRequirements();

        vk::MemoryAllocateInfo allocateInfo(memoryRequirements.size,
                                            getMemoryType(memoryRequirements, memoryProperties));

        vk::MemoryAllocateFlagsInfo memoryAllocateFlagsInfo(vk::MemoryAllocateFlagBits::eDeviceAddress);
        allocateInfo.pNext = &memoryAllocateFlagsInfo;

        memory = std::move(context::device->allocateMemory(
                allocateInfo));

        image.bindMemory(*memory, 0);
    }

    void Image::createImageView(vk::ImageViewType imageViewType, vk::Format format,
                                vk::ImageSubresourceRange imageSubresourceRange) {
        imageView = context::device->createImageView(vk::ImageViewCreateInfo({},
                                                                             *image,
                                                                             imageViewType,
                                                                             format,
                                                                             {vk::ComponentSwizzle::eR,
                                                                              vk::ComponentSwizzle::eG,
                                                                              vk::ComponentSwizzle::eB,
                                                                              vk::ComponentSwizzle::eA},
                                                                             imageSubresourceRange));
    }

    void Image::createSampler(vk::Filter magFilter, vk::Filter minFilter, vk::SamplerMipmapMode mipmapMode,
                              vk::SamplerAddressMode addressMode) {
        sampler = context::device->createSampler(vk::SamplerCreateInfo({},
                                                                       magFilter,
                                                                       minFilter,
                                                                       mipmapMode,
                                                                       addressMode,
                                                                       addressMode,
                                                                       addressMode,
                                                                       0,
                                                                       VK_FALSE,
                                                                       1,
                                                                       VK_FALSE,
                                                                       vk::CompareOp::eAlways,
                                                                       0,
                                                                       0,
                                                                       vk::BorderColor::eIntOpaqueBlack,
                                                                       VK_FALSE));
    }

    Format Image::getFormat() const { return format; }

    const vk::raii::Image &Image::getImage() const { return image; }

    const vk::raii::DeviceMemory &Image::getDeviceMemory() const { return memory; }

    const vk::raii::ImageView &Image::getImageView() const { return imageView; }

    const vk::raii::Sampler &Image::getSampler() const { return sampler; }

    vk::DeviceAddress RTAccelerationStructure::getAddress() const {
        return context::device->getAccelerationStructureAddressKHR({*accelerationStructure});
    }

    // Load SPIR-V shader stage binary from file
    Shader::Shader(const std::string &fileName, vk::ShaderStageFlagBits stage) : module(VK_NULL_HANDLE), stage(stage) {
        // open shader binary file
        std::ifstream file(fileName, std::ios::in | std::ios::binary | std::ios::ate);
        if (!file)
            throw std::runtime_error("Could not open shader binary " + fileName);

        // read 1-Byte ifstream into 4-Byte SPIR-V data vector
        uint32_t *memblock;
        auto size = file.tellg();
        memblock = new uint32_t[size / 4 + (size % 4 == 0 ? 0 : 1)];
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char *>(memblock), size);
        file.close();

        std::vector<uint32_t> shaderCode(memblock, memblock + (size / 4 + (size % 4 == 0 ? 0 : 1)));

        module = context::device->createShaderModule({{}, shaderCode});
        delete[](memblock);
    }

    vk::PipelineShaderStageCreateInfo Shader::getShaderStage() {
        return {{}, stage, *module, "main"};
    }
}