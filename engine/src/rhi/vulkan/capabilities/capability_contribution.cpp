#include "kaisou/rhi/vulkan/capabilities/capability_contribution.hpp"

namespace kaisou::rhi::vulkan
{
    CapabilityContribution MakeVulkanBaselineContribution()
    {
        CapabilityContribution contribution;
        contribution.source = "Vulkan.Baseline";
        contribution.requirements.minimumApiVersion = VK_API_VERSION_1_4;
        contribution.requirements.features = {
            DeviceFeature::TimelineSemaphore,
            DeviceFeature::DynamicRendering,
            DeviceFeature::Synchronization2
        };

        return contribution;
    }

    CapabilityContribution MakePresentationContribution(const core::Span<const std::string> windowExtensions)
    {
        CapabilityContribution contribution;
        contribution.source = "Vulkan.Presentation";

        contribution.requiredInstanceExtensions.reserve(windowExtensions.size() + 2);
        for (const auto &extension : windowExtensions)
            contribution.requiredInstanceExtensions.push_back(extension);

        contribution.requiredInstanceExtensions.emplace_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
        contribution.requiredInstanceExtensions.emplace_back(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);

        contribution.requirements.extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME
        };
        contribution.requirements.features = {
            DeviceFeature::SwapchainMaintenance1
        };
        contribution.requirements.queues = {
            {DeviceQueueRole::Graphics, 1},
            {DeviceQueueRole::Present, 1}
        };

        return contribution;
    }

    CapabilityContribution MakeDiagnosticsContribution(const bool enableValidation, const bool enableGpuDebug)
    {
        CapabilityContribution contribution;
        contribution.source = "Vulkan.Diagnostics";

        if (enableValidation)
            contribution.requiredInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        else if (enableGpuDebug)
            contribution.preferredInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return contribution;
    }
}
