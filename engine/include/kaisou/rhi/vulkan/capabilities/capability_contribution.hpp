#ifndef KAI_VULKAN_CAPABILITIES_CAPABILITY_CONTRIBUTION_HPP
#define KAI_VULKAN_CAPABILITIES_CAPABILITY_CONTRIBUTION_HPP

#include "kaisou/rhi/vulkan/capabilities/device_capabilities.hpp"

#include <string>
#include <vector>

namespace kaisou::rhi::vulkan
{
    struct CapabilityContribution
    {
        // 自行持有来源名称，供合并后的需求追溯与失败报告使用
        std::string source;

        DeviceRequirements requirements;
        DevicePreferences preferences;

        // Instance 扩展与 Device 扩展具有不同的查询、创建阶段
        std::vector<std::string> requiredInstanceExtensions;
        std::vector<std::string> preferredInstanceExtensions;
    };

    [[nodiscard]]
    CapabilityContribution MakeVulkanBaselineContribution();

    // windowExtensions 来自已有 RequiredWindowExtensions()
    [[nodiscard]]
    CapabilityContribution MakePresentationContribution(core::Span<const std::string> windowExtensions);

    // 这里只贡献诊断所需的扩展；Validation Layer 仍由 InstanceConfig 控制
    [[nodiscard]]
    CapabilityContribution MakeDiagnosticsContribution(bool enableValidation, bool enableGpuDebug);
}

#endif
