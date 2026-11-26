#ifndef KAI_VULKAN_CAPABILITIES_FEATURE_REQUIREMENT_HPP
#define KAI_VULKAN_CAPABILITIES_FEATURE_REQUIREMENT_HPP

#include "kaisou/core/types.hpp"

namespace kaisou::rhi::vulkan
{
    enum class FeatureRequirement : core::u8
    {
        Disabled,
        Preferred,
        Required
    };

    enum class FeatureDecision : core::u8
    {
        Skip,

        Enable,

        Fallback,
        Reject
    };

    [[nodiscard]]
    FeatureDecision ResolveFeatureRequirement(FeatureRequirement requirement, bool supported) noexcept;
}

#endif
