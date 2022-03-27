//
// Created by jedre on 19.02.2022.
//

#ifndef PATHTRACER_VULKANUTILS_HPP
#define PATHTRACER_VULKANUTILS_HPP

#include "vulkan/vulkan_raii.hpp"

// vulkan error handling
void check_vk_result(vk::Result err);
void check_vk_result(VkResult err);

namespace vk::utils {

    namespace context {
        static vk::raii::PhysicalDevice*            physicalDevice;
        static vk::raii::Device*                    device;
        static vk::raii::CommandPool*               commandPool;
        static vk::raii::Queue*                     transferQueue;
        static vk::PhysicalDeviceMemoryProperties   physicalMemoryProperties;
    } // namespace context

    enum QueueFamilyIndex {graphics, compute, transfer};

    class Buffer {
    public:
        Buffer();
        Buffer(BufferCreateInfo bufferCreateInfo, MemoryPropertyFlags memoryProperties);

        void* map(vk::DeviceSize size = UINT64_MAX, vk::DeviceSize offset = 0) const;
        void unmap() const;

        void uploadData(const void* data, vk::DeviceSize size, vk::DeviceSize offset = 0) const;

        // getters
        const vk::raii::Buffer& getBuffer() const;
        const vk::DeviceSize& getSize() const;
        vk::DeviceAddress getAddress() const;

    private:
        vk::raii::Buffer buffer;
        vk::raii::DeviceMemory memory;
        vk::DeviceSize size;
    };

    class Image {
    public:
        Image();

        Image(vk::ImageType imageType,
                          vk::Format format,
                          Extent3D extent,
                          vk::ImageTiling tiling,
                          vk::ImageUsageFlags usage,
                          vk::MemoryPropertyFlags memoryProperties);

        void load(const std::string& filename);
        void createImageView(vk::ImageViewType imageViewType, vk::Format format, vk::ImageSubresourceRange imageSubresourceRange);
        void createSampler(vk::Filter magFilter, vk::Filter minFilter, vk::SamplerMipmapMode mipmapMode, vk::SamplerAddressMode addressMode);

        Format getFormat() const;
        const vk::raii::Image& getImage() const;
        const vk::raii::DeviceMemory& getDeviceMemory() const;
        const vk::raii::ImageView& getImageView() const;
        const raii::Sampler& getSampler() const;
    private:
        vk::Format format;
        vk::raii::Image image;
        vk::raii::DeviceMemory memory;
        vk::raii::ImageView imageView;
        vk::raii::Sampler sampler;
    };

    class Shader {
    public:
        Shader(const std::string& filename, vk::ShaderStageFlagBits stage);
        vk::PipelineShaderStageCreateInfo getShaderStage();
    private:
        vk::raii::ShaderModule module;
        vk::ShaderStageFlagBits stage;
    };

    struct RTAccelerationStructure {
        vk::utils::Buffer buffer;
        vk::raii::AccelerationStructureKHR accelerationStructure{VK_NULL_HANDLE};
        vk::utils::Buffer instancesBuffer;
        vk::DeviceAddress getAddress() const;
    };

    struct RTScene {
        std::vector<RTAccelerationStructure> bottomLevelAS;
        RTAccelerationStructure topLevelAS;
    };

    void Initialize(vk::raii::PhysicalDevice* physicalDevice,
                    vk::raii::Device* device,
                    vk::raii::CommandPool* commandPool,
                    vk::raii::Queue* transferQueue);

    // check if list of extensions contains specific extension by name
    bool contains(const std::vector<vk::ExtensionProperties> &extensionProperties, const std::string &extensionName);

    void imageBarrier(const vk::raii::CommandBuffer& commandBuffer,
                      VkImage image,
                      const vk::ImageSubresourceRange& subresourceRange,
                      const vk::AccessFlags& srcAccessMask,
                      const vk::AccessFlags& dstAccessMask,
                      const ImageLayout& oldLayout,
                      const ImageLayout& newLayout);

    uint32_t getMemoryType(vk::MemoryRequirements memoryRequirements, vk::MemoryPropertyFlags memoryProperties);

} // namespace vk::utils

#endif //PATHTRACER_VULKANUTILS_HPP
