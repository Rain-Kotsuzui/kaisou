#include "kaisou/rhi/vulkan/capabilities/feature_requirement.hpp"

#include "kaisou/core/assert.hpp"

namespace kaisou::rhi::vulkan
{
    FeatureDecision ResolveFeatureRequirement(const FeatureRequirement requirement, const bool supported) noexcept
    {
        switch (requirement)
        {
        case FeatureRequirement::Disabled:
            return FeatureDecision::Skip;

        case FeatureRequirement::Preferred:
            return supported ? FeatureDecision::Enable : FeatureDecision::Fallback;

        case FeatureRequirement::Required:
            return supported ? FeatureDecision::Enable : FeatureDecision::Reject;
        }

        KAISOU_FATAL("Invalid FeatureRequirement");
    }
}
