#ifndef KAI_VULKAN_QUEUE_TOPOLOGY_HPP
#define KAI_VULKAN_QUEUE_TOPOLOGY_HPP

#include "kaisou/rhi/vulkan/capabilities/capability_merge.hpp"

#include <vector>

namespace kaisou::rhi::vulkan
{
    struct QueueFamilyRequest
    {
        core::u32 familyIndex = VK_QUEUE_FAMILY_IGNORED;
        core::u32 queueCount = 0;
    };

    // 当前角色使用该 Family 的 [0, queueCount)
    struct QueueRoleMapping
    {
        DeviceQueueRole role = DeviceQueueRole::Graphics;
        core::u32 familyIndex = VK_QUEUE_FAMILY_IGNORED;
        core::u32 queueCount = 0;
    };

    struct QueueTopology
    {
        std::vector<QueueFamilyRequest> families;
        std::vector<QueueRoleMapping> roles;
    };

    // 输入必须是合并后的请求；失败时 topology 为空，成功时发布完整配置。
    [[nodiscard]]
    bool BuildQueueTopology(
        core::Span<const MergedQueueRequest> requests,
        const DeviceCapabilities &capabilities, QueueTopology &topology);
}

#endif