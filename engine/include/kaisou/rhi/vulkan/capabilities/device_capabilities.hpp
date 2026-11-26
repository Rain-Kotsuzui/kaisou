#ifndef KAI_VULKAN_CAPABILITIES_DEVICE_CAPABILITIES_HPP
#define KAI_VULKAN_CAPABILITIES_DEVICE_CAPABILITIES_HPP

#include "kaisou/rhi/vulkan/capabilities/feature_requirement.hpp"
#include "kaisou/rhi/vulkan/vulkan.hpp"

#include <string>
#include <vector>

namespace kaisou::rhi::vulkan
{
    enum class DeviceFeature : core::u8
    {
        TimelineSemaphore,
        DynamicRendering,
        Synchronization2,
        SwapchainMaintenance1
    };

    enum class DeviceQueueRole : core::u8
    {
        Graphics,
        Present,
        Compute,
        Transfer
    };

    enum class DeviceLimit : core::u8
    {
        ImageDimension2D,
        StorageBufferRange,
        PushConstantsSize,
        ComputeWorkGroupInvocations
    };

    struct DeviceQueueRequirement
    {
        DeviceQueueRole role = DeviceQueueRole::Graphics;
        core::u32 minimumCount = 1;
    };

    struct DeviceQueuePreference
    {
        DeviceQueueRole role = DeviceQueueRole::Graphics;

        bool preferDedicatedFamily = false;
    };

    struct DeviceLimitRequirement
    {
        DeviceLimit limit = DeviceLimit::ImageDimension2D;

        core::u64 minimum = 0;
    };

    struct DeviceRequirements
    {
        core::u32 minimumApiVersion = VK_API_VERSION_1_4;

        std::vector<std::string> extensions;
        std::vector<DeviceFeature> features;
        std::vector<DeviceQueueRequirement> queues;
        std::vector<DeviceLimitRequirement> limits;
    };

    struct DevicePreferences
    {
        std::vector<std::string> extensions;
        std::vector<DeviceFeature> features;
        std::vector<DeviceQueuePreference> queues;
        std::vector<DeviceLimitRequirement> limits;
    };

    struct QueueFamilyCapabilities
    {
        core::u32 familyIndex = VK_QUEUE_FAMILY_IGNORED;
        VkQueueFamilyProperties properties{};

        bool supportsPresent = false;
    };

    struct DeviceCapabilities
    {
        VkPhysicalDeviceProperties properties{};

        std::vector<std::string> extensions;

        std::vector<DeviceFeature> features;

        std::vector<QueueFamilyCapabilities> queueFamilies;

        bool presentSupportQueried = false;
    };

    struct EnabledDeviceFeatures
    {
        core::u32 apiVersion = 0;

        std::vector<std::string> extensions;
        std::vector<DeviceFeature> features;
    };

    [[nodiscard]]
    FeatureDecision ResolveDeviceFeatureRequirement(
        FeatureRequirement requirement, DeviceFeature feature, const DeviceCapabilities &capabilities) noexcept;
}

#endif
