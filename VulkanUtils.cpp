//
// Created by jedre on 19.02.2022.
//

#include "VulkanUtils.hpp"
#include <iostream>

void check_vk_result(VkResult err) {
    if (err != VK_SUCCESS) {
        std::cerr << "Vulkan error " << vk::to_string(static_cast<vk::Result>(err));
        if (err < 0)
            abort();
    }
}

namespace vk::utils {
    // check whether ExtensionProperties vector contains specific extension
    bool contains(const std::vector<vk::ExtensionProperties> &extensionProperties, const std::string &extensionName) {
        auto propertyIterator = std::find_if(extensionProperties.begin(),
                                             extensionProperties.end(),
                                             [&extensionName](const vk::ExtensionProperties &ep) {
                                                 return extensionName == ep.extensionName;
                                             });
        return (propertyIterator != extensionProperties.end());
    }
}