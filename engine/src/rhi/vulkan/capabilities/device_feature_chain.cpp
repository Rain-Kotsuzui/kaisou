#include "kaisou/rhi/vulkan/capabilities/device_feature_chain.hpp"

#include "kaisou/core/assert.hpp"

#include <algorithm>

namespace kaisou::rhi::vulkan
{
    DeviceFeatureChain::DeviceFeatureChain() noexcept
    {
        Reset(false);
    }

    void DeviceFeatureChain::Reset(const bool includeSwapchainMaintenance) noexcept
    {
        features2_ = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        features12_ = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        features13_ = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        maintenance_ = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR};

        features2_.pNext = &features12_;
        features12_.pNext = &features13_;
        features13_.pNext = includeSwapchainMaintenance ? &maintenance_ : nullptr;
    }

    template <typename Self>
    auto &DeviceFeatureChain::Value(Self &self, const DeviceFeature feature) noexcept
    {
        switch (feature)
        {
        case DeviceFeature::TimelineSemaphore:
            return self.features12_.timelineSemaphore;
        case DeviceFeature::DynamicRendering:
            return self.features13_.dynamicRendering;
        case DeviceFeature::Synchronization2:
            return self.features13_.synchronization2;
        case DeviceFeature::SwapchainMaintenance1:
            return self.maintenance_.swapchainMaintenance1;
        default:
            KAISOU_FATAL("Invalid DeviceFeature in feature chain");
        }
    }

    void DeviceFeatureChain::Query(
        const VkPhysicalDevice physicalDevice, const bool querySwapchainMaintenance) noexcept
    {
        Reset(querySwapchainMaintenance);
        vkGetPhysicalDeviceFeatures2(physicalDevice, &features2_);
    }

    bool DeviceFeatureChain::Supports(const DeviceFeature feature) const noexcept
    {
        return Value(*this, feature) == VK_TRUE;
    }

    void DeviceFeatureChain::ConfigureEnablement(const core::Span<const DeviceFeature> features) noexcept
    {
        const bool includeMaintenance =
            std::ranges::find(features, DeviceFeature::SwapchainMaintenance1) != features.end();

        Reset(includeMaintenance);

        for (const auto feature : features)
            Value(*this, feature) = VK_TRUE;
    }

    const VkPhysicalDeviceFeatures2 &DeviceFeatureChain::Head() const noexcept
    {
        return features2_;
    }
}