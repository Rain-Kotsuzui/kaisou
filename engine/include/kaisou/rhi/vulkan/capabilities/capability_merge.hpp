#ifndef KAI_VULKAN_CAPABILITIES_CAPABILITY_MERGE_HPP
#define KAI_VULKAN_CAPABILITIES_CAPABILITY_MERGE_HPP

#include "kaisou/rhi/vulkan/capabilities/capability_contribution.hpp"

#include <string>
#include <vector>

namespace kaisou::rhi::vulkan
{
    // 来源记录：自行持有来源名称及该来源的请求值
    struct CapabilityRequestSource
    {
        std::string source;
        FeatureRequirement requirement = FeatureRequirement::Disabled;
    };

    struct ApiVersionRequestSource
    {
        std::string source;
        core::u32 minimumApiVersion = 0;
    };

    struct QueueRequirementSource
    {
        std::string source;
        core::u32 minimumCount = 0;
    };

    struct QueuePreferenceSource
    {
        std::string source;
        bool preferDedicatedFamily = false;
    };
    struct LimitRequestSource
    {
        std::string source;
        core::u64 minimum = 0;
    };
    // 单项合并结果：同一扩展、Feature 或队列角色只保留一项
    struct MergedExtensionRequest
    {
        std::string name;
        FeatureRequirement requirement = FeatureRequirement::Disabled;

        // 同一来源只记录一次，保留该来源的最强请求
        std::vector<CapabilityRequestSource> sources;
    };

    struct MergedFeatureRequest
    {
        DeviceFeature feature = DeviceFeature::TimelineSemaphore;
        FeatureRequirement requirement = FeatureRequirement::Disabled;

        // 复用扩展请求的来源表达，记录每个来源的最强请求
        std::vector<CapabilityRequestSource> sources;
    };

    struct MergedQueueRequest
    {
        DeviceQueueRole role = DeviceQueueRole::Graphics;

        // 0 表示没有 Required 数量请求，只有偏好不能建立必需角色
        core::u32 minimumCount = 0;
        bool preferDedicatedFamily = false;

        std::vector<QueueRequirementSource> requiredSources;
        std::vector<QueuePreferenceSource> preferredSources;
    };

    struct MergedLimitRequest
    {
        DeviceLimit limit = DeviceLimit::ImageDimension2D;

        core::u64 requiredMinimum = 0;
        core::u64 preferredMinimum = 0;

        std::vector<LimitRequestSource> requiredSources;
        std::vector<LimitRequestSource> preferredSources;
    };

    // 聚合结果：保存合并后的请求，不代表 GPU 支持或实际启用
    struct MergedCapabilityRequests
    {
        // 合并结果不能降低项目的 Vulkan 1.4 硬基线
        core::u32 minimumApiVersion = VK_API_VERSION_1_4;
        std::vector<ApiVersionRequestSource> apiVersionSources;

        std::vector<MergedExtensionRequest> instanceExtensions;
        std::vector<MergedExtensionRequest> deviceExtensions;
        std::vector<MergedFeatureRequest> deviceFeatures;
        std::vector<MergedQueueRequest> deviceQueues;
        std::vector<MergedLimitRequest> deviceLimits;
    };
    struct CapabilityPolicy
    {
        std::string source = "UserPolicy";

        std::vector<std::string> forbiddenInstanceExtensions;
        std::vector<std::string> forbiddenDeviceExtensions;
        std::vector<DeviceFeature> forbiddenDeviceFeatures;
    };
    // 合并入口：借用贡献列表，返回自行持有数据的请求结果
    [[nodiscard]]
    MergedCapabilityRequests MergeCapabilityRequests(core::Span<const CapabilityContribution> contributions);
    // 应用禁止策略及当前能力依赖规则，必须在发布请求引用或生成创建信息之前执行
    void ApplyCapabilityPolicy(MergedCapabilityRequests &requests, const CapabilityPolicy &policy);
    // 原地处理 Instance 扩展支持情况，随后重新处理下游依赖
    void ResolveInstanceExtensionRequests(
        MergedCapabilityRequests &requests, core::Span<const VkExtensionProperties> availableExtensions);
    // 最终 GPU 选定后请求，随后重新处理依赖
    void ResolveDeviceCapabilityRequests(
        MergedCapabilityRequests &requests, const DeviceCapabilities &capabilities);

}

#endif
