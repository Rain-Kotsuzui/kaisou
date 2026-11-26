#include "kaisou/rhi/vulkan/capabilities/device_capabilities.hpp"

#include <algorithm>

namespace kaisou::rhi::vulkan
{
    FeatureDecision ResolveDeviceFeatureRequirement(
        const FeatureRequirement requirement, const DeviceFeature feature,
        const DeviceCapabilities &capabilities) noexcept
    {
        const bool supported =
            std::ranges::find(capabilities.features, feature) != capabilities.features.end();

        return ResolveFeatureRequirement(requirement, supported);
    }
}
