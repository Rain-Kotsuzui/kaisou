#include "kaisou/rhi/vulkan/capabilities/device_capability_query.hpp"
#include "kaisou/rhi/vulkan/capabilities/device_feature_chain.hpp"
#include "kaisou/rhi/vulkan/enumerate.hpp"
#include "kaisou/rhi/vulkan/capabilities/capability_merge.hpp"

#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"

#include <string_view>

#include <algorithm>

namespace kaisou::rhi::vulkan
{
    namespace
    {
        core::u64 DeviceLimitValue(const DeviceCapabilities &capabilities, const DeviceLimit limit)
        {
            const auto &limits = capabilities.properties.limits;

            switch (limit)
            {
            case DeviceLimit::ImageDimension2D:
                return limits.maxImageDimension2D;

            case DeviceLimit::StorageBufferRange:
                return limits.maxStorageBufferRange;

            case DeviceLimit::PushConstantsSize:
                return limits.maxPushConstantsSize;

            case DeviceLimit::ComputeWorkGroupInvocations:
                return limits.maxComputeWorkGroupInvocations;
            }

            KAISOU_FATAL("Invalid DeviceLimit in capability check");
        }

        template <typename Source>
        bool CheckDecision(
            const DeviceCapabilities &capabilities, const char *scope, const std::string_view name,
            const FeatureDecision decision, const std::vector<Source> &sources)
        {
            if (decision != FeatureDecision::Reject && decision != FeatureDecision::Fallback)
                return true;

            const bool rejected = decision == FeatureDecision::Reject;

            KAISOU_LOG_WARN(
                "Vulkan", "GPU '{}': {} '{}' unavailable; {}",
                capabilities.properties.deviceName, scope, name,
                rejected ? "reject candidate (Required)" : "fallback (Preferred)");

            for (const auto &source : sources)
                KAISOU_LOG_INFO("Vulkan", "  Request source: {}", source.source);

            return !rejected;
        }
    }

    DeviceCapabilities QueryDeviceCapabilities(
        const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface)
    {
        DeviceCapabilities capabilities;
        vkGetPhysicalDeviceProperties(physicalDevice, &capabilities.properties);

        if (capabilities.properties.apiVersion < VK_API_VERSION_1_4)
            return capabilities;

        const auto extensions = EnumerateProperties<VkExtensionProperties>(
            "vkEnumerateDeviceExtensionProperties", vkEnumerateDeviceExtensionProperties, physicalDevice, nullptr);

        capabilities.extensions.reserve(extensions.size());
        for (const auto &extension : extensions)
            capabilities.extensions.emplace_back(extension.extensionName);

        const bool maintenanceSupported =
            std::ranges::find(
                capabilities.extensions, VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME) !=
            capabilities.extensions.end();

        DeviceFeatureChain featureChain;
        featureChain.Query(physicalDevice, maintenanceSupported);

        constexpr DeviceFeature featureCatalog[]{
            DeviceFeature::TimelineSemaphore,
            DeviceFeature::DynamicRendering,
            DeviceFeature::Synchronization2,
            DeviceFeature::SwapchainMaintenance1};

        for (const auto feature : featureCatalog)
        {
            if (featureChain.Supports(feature))
                capabilities.features.push_back(feature);
        }

        core::u32 familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);

        std::vector<VkQueueFamilyProperties> familyProperties(familyCount);
        if (familyCount != 0)
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, familyProperties.data());

        familyProperties.resize(familyCount);
        capabilities.queueFamilies.reserve(familyCount);

        for (core::u32 familyIndex = 0; familyIndex < familyCount; ++familyIndex)
        {
            QueueFamilyCapabilities family;
            family.familyIndex = familyIndex;
            family.properties = familyProperties[familyIndex];

            if (surface != VK_NULL_HANDLE)
            {
                VkBool32 supported = VK_FALSE;
                const VkResult result =
                    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, surface, &supported);

                if (result != VK_SUCCESS)
                {
                    KAISOU_FATAL(fmt::format(
                        "vkGetPhysicalDeviceSurfaceSupportKHR failed for family {}: VkResult {}",
                        familyIndex, static_cast<int>(result)));
                }

                family.supportsPresent = supported == VK_TRUE;
            }

            capabilities.queueFamilies.push_back(family);
        }

        capabilities.presentSupportQueried = surface != VK_NULL_HANDLE;
        return capabilities;
    }
    bool CheckDeviceCapabilityRequests(
        const MergedCapabilityRequests &requests, const DeviceCapabilities &capabilities)
    {
        const auto minimumApiVersion = std::max(requests.minimumApiVersion, VK_API_VERSION_1_4);
        const auto actualApiVersion = capabilities.properties.apiVersion;

        if (actualApiVersion < minimumApiVersion)
        {
            KAISOU_LOG_WARN(
                "Vulkan",
                "GPU '{}': Vulkan {}.{}.{} is below required {}.{}.{}; reject candidate",
                capabilities.properties.deviceName,
                VK_API_VERSION_MAJOR(actualApiVersion),
                VK_API_VERSION_MINOR(actualApiVersion),
                VK_API_VERSION_PATCH(actualApiVersion),
                VK_API_VERSION_MAJOR(minimumApiVersion),
                VK_API_VERSION_MINOR(minimumApiVersion),
                VK_API_VERSION_PATCH(minimumApiVersion));

            for (const auto &source : requests.apiVersionSources)
            {
                KAISOU_LOG_INFO(
                    "Vulkan", "  API request source: {} ({}.{}.{})",
                    source.source,
                    VK_API_VERSION_MAJOR(source.minimumApiVersion),
                    VK_API_VERSION_MINOR(source.minimumApiVersion),
                    VK_API_VERSION_PATCH(source.minimumApiVersion));
            }

            return false;
        }

        bool accepted = true;

        for (const auto &request : requests.deviceExtensions)
        {
            const bool supported =
                std::ranges::find(capabilities.extensions, request.name) != capabilities.extensions.end();

            const auto decision = ResolveFeatureRequirement(request.requirement, supported);

            if (!CheckDecision(capabilities, "Device extension", request.name, decision, request.sources))
                accepted = false;
        }

        for (const auto &request : requests.deviceFeatures)
        {
            const auto decision =
                ResolveDeviceFeatureRequirement(request.requirement, request.feature, capabilities);

            const auto name = fmt::format("Feature {}", static_cast<core::u32>(request.feature));

            if (!CheckDecision(capabilities, "Device feature", name, decision, request.sources))
                accepted = false;
        }

        for (const auto &request : requests.deviceLimits)
        {
            const auto actual = DeviceLimitValue(capabilities, request.limit);

            if (!request.requiredSources.empty())
            {
                const auto decision =
                    ResolveFeatureRequirement(FeatureRequirement::Required, actual >= request.requiredMinimum);

                const auto name = fmt::format(
                    "Limit {} (available {}, minimum {})",
                    static_cast<core::u32>(request.limit), actual, request.requiredMinimum);

                if (!CheckDecision(capabilities, "Device limit", name, decision, request.requiredSources))
                    accepted = false;
            }

            if (!request.preferredSources.empty())
            {
                const auto decision =
                    ResolveFeatureRequirement(FeatureRequirement::Preferred, actual >= request.preferredMinimum);

                const auto name = fmt::format(
                    "Limit {} (available {}, minimum {})",
                    static_cast<core::u32>(request.limit), actual, request.preferredMinimum);

                if (!CheckDecision(capabilities, "Device limit", name, decision, request.preferredSources))
                    accepted = false;
            }
        }

        return accepted;
    }
}