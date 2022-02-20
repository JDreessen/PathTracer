//
// Created by jedre on 19.02.2022.
//

#ifndef PATHTRACER_VULKANUTILS_HPP
#define PATHTRACER_VULKANUTILS_HPP

#include "vulkan/vulkan_raii.hpp"

// vulkan error handling
void check_vk_result(VkResult err);

struct QueueFamilyIndices {
    uint32_t graphics, compute, transfer;
};

namespace vk::utils {
    // check if list of extensions contains specific extension by name
    bool contains(const std::vector<vk::ExtensionProperties> &extensionProperties, const std::string &extensionName);
}

#endif //PATHTRACER_VULKANUTILS_HPP
