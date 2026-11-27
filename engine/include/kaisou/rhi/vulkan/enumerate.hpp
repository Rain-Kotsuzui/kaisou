#ifndef KAI_VULKAN_ENUMERATE_HPP
#define KAI_VULKAN_ENUMERATE_HPP

#include "kaisou/core/assert.hpp"
#include "kaisou/core/types.hpp"
#include "kaisou/rhi/vulkan/vulkan.hpp"

#include <vector>
#include <fmt/format.h>

namespace kaisou::rhi::vulkan
{
    template <typename Property, typename Query, typename... Args>
    [[nodiscard]]
    std::vector<Property> EnumerateProperties(const char *operation, const Query query, const Args &...args)
    {
        std::vector<Property> properties;
        core::u32 count = 0;
        VkResult result = VK_SUCCESS;

        do
        {
            result = query(args..., &count, nullptr);
            if (result != VK_SUCCESS)
                break;

            properties.resize(count);
            if (count == 0)
                break;

            result = query(args..., &count, properties.data());
        } while (result == VK_INCOMPLETE);

        if (result != VK_SUCCESS)
            KAISOU_FATAL(fmt::format("{} failed: VkResult {}", operation, static_cast<int>(result)));

        properties.resize(count);
        return properties;
    }
}

#endif