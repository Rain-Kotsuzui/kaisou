#ifndef KAI_VULKAN_CAPABILITIES_DEVICE_CAPABILITY_QUERY_HPP
#define KAI_VULKAN_CAPABILITIES_DEVICE_CAPABILITY_QUERY_HPP

#include "kaisou/rhi/vulkan/capabilities/device_capabilities.hpp"

namespace kaisou::rhi::vulkan
{
    [[nodiscard]]
    DeviceCapabilities QueryDeviceCapabilities(
        VkPhysicalDevice physicalDevice, VkSurfaceKHR surface = VK_NULL_HANDLE);

    struct MergedCapabilityRequests;
    // 只检查 API、Extension、Feature 和 Limit，不修改请求或支持快照。
    [[nodiscard]]
    bool CheckDeviceCapabilityRequests(
        const MergedCapabilityRequests &requests, const DeviceCapabilities &capabilities);

}

#endif