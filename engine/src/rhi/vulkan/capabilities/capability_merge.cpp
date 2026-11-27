#include "kaisou/rhi/vulkan/capabilities/capability_merge.hpp"

#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"

#include <type_traits>
#include <fmt/format.h>
#include <algorithm>
#include <utility>

namespace kaisou::rhi::vulkan
{
    namespace
    {
        FeatureRequirement MergeRequirement(const FeatureRequirement left, const FeatureRequirement right)
        {
            if (left == FeatureRequirement::Required || right == FeatureRequirement::Required)
                return FeatureRequirement::Required;

            if (left == FeatureRequirement::Preferred || right == FeatureRequirement::Preferred)
                return FeatureRequirement::Preferred;

            return FeatureRequirement::Disabled;
        }

        template <typename Record, typename Key>
        Record &GetOrAppend(std::vector<Record> &records, const Key &key, Key Record::*keyMember)
        {
            auto record = std::ranges::find(records, key, keyMember);
            if (record != records.end())
                return *record;

            Record entry;
            entry.*keyMember = key;
            records.push_back(std::move(entry));
            return records.back();
        }

        template <typename Value>
        Value MergeMaximum(const Value left, const Value right)
        {
            return std::max(left, right);
        }

        bool MergeLogicalOr(const bool left, const bool right) noexcept
        {
            return left || right;
        }

        template <auto Merge, typename Record, typename Value>
        void MergeSource(
            std::vector<Record> &sources,
            const std::string &source,
            Value Record::*valueMember,
            const Value value)
        {
            // 当前来源成员的默认值必须是合并中性值：Disabled、0 或 false
            auto &origin = GetOrAppend(sources, source, &Record::source);
            auto &current = origin.*valueMember;
            current = Merge(current, value);
        }

        void AppendApiVersion(
            MergedCapabilityRequests &merged,
            const core::u32 minimumApiVersion,
            const std::string &source)
        {
            if (VK_API_VERSION_VARIANT(minimumApiVersion) != 0 ||
                VK_API_VERSION_MAJOR(minimumApiVersion) == 0)
                KAISOU_FATAL("Invalid Vulkan API version in capability contribution");

            merged.minimumApiVersion = std::max(merged.minimumApiVersion, minimumApiVersion);

            MergeSource<MergeMaximum<core::u32>>(
                merged.apiVersionSources, source, &ApiVersionRequestSource::minimumApiVersion, minimumApiVersion);
        }

        void AppendExtension(
            std::vector<MergedExtensionRequest> &requests,
            const std::string &name,
            const std::string &source,
            const FeatureRequirement requirement)
        {
            if (name.empty() || name.find('\0') != std::string::npos)
                KAISOU_FATAL("Invalid Vulkan extension request name");

            auto &request = GetOrAppend(requests, name, &MergedExtensionRequest::name);
            request.requirement = MergeRequirement(request.requirement, requirement);
            MergeSource<MergeRequirement>(
                request.sources, source, &CapabilityRequestSource::requirement, requirement);
        }

        void AppendFeature(
            std::vector<MergedFeatureRequest> &requests,
            const DeviceFeature feature,
            const std::string &source,
            const FeatureRequirement requirement)
        {
            // 新增执行能力时必须同步扩展这里及后续查询、启用映射
            switch (feature)
            {
            case DeviceFeature::TimelineSemaphore:
            case DeviceFeature::DynamicRendering:
            case DeviceFeature::Synchronization2:
            case DeviceFeature::SwapchainMaintenance1:
                break;
            default:
                KAISOU_FATAL("Invalid DeviceFeature in capability contribution");
            }

            auto &request = GetOrAppend(requests, feature, &MergedFeatureRequest::feature);
            request.requirement = MergeRequirement(request.requirement, requirement);
            MergeSource<MergeRequirement>(
                request.sources, source, &CapabilityRequestSource::requirement, requirement);
        }

        MergedQueueRequest &GetOrAppendQueueRequest(
            std::vector<MergedQueueRequest> &requests, const DeviceQueueRole role)
        {
            switch (role)
            {
            case DeviceQueueRole::Graphics:
            case DeviceQueueRole::Present:
            case DeviceQueueRole::Compute:
            case DeviceQueueRole::Transfer:
                break;
            default:
                KAISOU_FATAL("Invalid DeviceQueueRole in capability contribution");
            }

            return GetOrAppend(requests, role, &MergedQueueRequest::role);
        }

        void AppendQueueRequirement(
            std::vector<MergedQueueRequest> &requests,
            const DeviceQueueRequirement &requirement,
            const std::string &source)
        {
            if (requirement.minimumCount == 0)
                KAISOU_FATAL("Required queue minimumCount must be greater than zero");

            auto &request = GetOrAppendQueueRequest(requests, requirement.role);
            request.minimumCount = std::max(request.minimumCount, requirement.minimumCount);

            MergeSource<MergeMaximum<core::u32>>(
                request.requiredSources, source, &QueueRequirementSource::minimumCount, requirement.minimumCount);
        }

        void AppendQueuePreference(
            std::vector<MergedQueueRequest> &requests,
            const DeviceQueuePreference &preference,
            const std::string &source)
        {
            auto &request = GetOrAppendQueueRequest(requests, preference.role);
            request.preferDedicatedFamily = request.preferDedicatedFamily || preference.preferDedicatedFamily;

            MergeSource<MergeLogicalOr>(
                request.preferredSources, source,
                &QueuePreferenceSource::preferDedicatedFamily, preference.preferDedicatedFamily);
        }
        void AppendLimit(
            std::vector<MergedLimitRequest> &requests,
            const DeviceLimitRequirement &limitRequest,
            const std::string &source,
            const FeatureRequirement requirement)
        {
            if (requirement != FeatureRequirement::Required && requirement != FeatureRequirement::Preferred)
                KAISOU_FATAL("Limit contribution must be Required or Preferred");

            // 当前只包含最低容量要求，新增其他语义的 Limit 时必须重新确定合并规则
            switch (limitRequest.limit)
            {
            case DeviceLimit::ImageDimension2D:
            case DeviceLimit::StorageBufferRange:
            case DeviceLimit::PushConstantsSize:
            case DeviceLimit::ComputeWorkGroupInvocations:
                break;
            default:
                KAISOU_FATAL("Invalid DeviceLimit in capability contribution");
            }

            auto &request = GetOrAppend(requests, limitRequest.limit, &MergedLimitRequest::limit);
            const bool required = requirement == FeatureRequirement::Required;

            auto &minimum = required ? request.requiredMinimum : request.preferredMinimum;
            auto &sources = required ? request.requiredSources : request.preferredSources;

            minimum = std::max(minimum, limitRequest.minimum);
            MergeSource<MergeMaximum<core::u64>>(
                sources, source, &LimitRequestSource::minimum, limitRequest.minimum);
        }
        template <auto Append, typename Record, typename Input, typename... Args>
        void AppendRequests(
            std::vector<Record> &requests,
            const std::vector<Input> &inputs,
            const std::string &source,
            const Args &...args)
        {
            for (const auto &input : inputs)
                Append(requests, input, source, args...);
        }
        template <typename Request, typename Key>
        bool HandleUnavailableRequest(
            const Request &request, const Key &key, const std::string &reason, const char *scope)
        {
            const auto decision = ResolveFeatureRequirement(request.requirement, false);
            if (decision == FeatureDecision::Skip)
                return false;

            const auto label = [&]
            {
                if constexpr (std::is_enum_v<Key>)
                    return std::to_string(static_cast<core::u32>(key));
                else
                    return key;
            }();

            auto message = fmt::format("{} '{}' unavailable: {}", scope, label, reason);
            for (const auto &origin : request.sources)
                message += fmt::format("; requested by '{}'", origin.source);

            if (decision == FeatureDecision::Reject)
                KAISOU_FATAL("Required " + message);

            KAISOU_LOG_WARN("Vulkan", "Preferred {}; request removed", message);
            return true;
        }

        template <typename Request, typename Key, typename Supports>
        void ResolveSupportedRequests(
            std::vector<Request> &requests, Key Request::*keyMember,
            const Supports &supports, const std::string &reason, const char *scope)
        {
            std::erase_if(requests, [&](const Request &request)
                          {
                if (request.requirement == FeatureRequirement::Disabled)
                    return true;

                if (supports(request))
                    return false;

                return HandleUnavailableRequest(request, request.*keyMember, reason, scope); });
        }

        template <typename Request, typename Key>
        void ApplyForbiddenRequests(
            std::vector<Request> &requests,
            const std::vector<Key> &forbidden,
            Key Request::*keyMember,
            const CapabilityPolicy &policy,
            const char *scope)
        {
            std::erase_if(requests, [&](const Request &request)
                          {
                const auto &key = request.*keyMember;

                if (request.requirement == FeatureRequirement::Disabled ||
                    std::ranges::find(forbidden, key) == forbidden.end())
                    return false;

                return HandleUnavailableRequest(
                    request, key, fmt::format("forbidden by policy '{}'", policy.source), scope); });
        }
        template <typename Request, typename Key>
        void ApplyDependency(
            std::vector<Request> &requests,
            Key Request::*keyMember,
            const Key &key,
            const std::vector<MergedExtensionRequest> &extensions,
            const char *extensionName,
            const char *scope)
        {
            const auto dependency = std::ranges::find(extensions, extensionName, &MergedExtensionRequest::name);
            const auto dependencyRequirement =
                dependency == extensions.end() ? FeatureRequirement::Disabled : dependency->requirement;

            std::erase_if(requests, [&](const Request &request)
                          {
                if (request.*keyMember != key || request.requirement == FeatureRequirement::Disabled)
                    return false;

                const bool available =
                    dependencyRequirement == FeatureRequirement::Required ||
                    (dependencyRequirement == FeatureRequirement::Preferred &&
                     request.requirement == FeatureRequirement::Preferred);

                if (available)
                    return false;

                return HandleUnavailableRequest(
                    request, request.*keyMember,
                    fmt::format("dependency '{}' is missing or weaker than the request", extensionName), scope); });
        }
        void ApplyCapabilityDependencies(MergedCapabilityRequests &requests)
        {
            ApplyDependency(
                requests.instanceExtensions, &MergedExtensionRequest::name,
                std::string{VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME},
                requests.instanceExtensions, VK_KHR_SURFACE_EXTENSION_NAME, "Instance extension");

            ApplyDependency(
                requests.instanceExtensions, &MergedExtensionRequest::name,
                std::string{VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME},
                requests.instanceExtensions, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, "Instance extension");

            ApplyDependency(
                requests.deviceExtensions, &MergedExtensionRequest::name,
                std::string{VK_KHR_SWAPCHAIN_EXTENSION_NAME},
                requests.instanceExtensions, VK_KHR_SURFACE_EXTENSION_NAME, "Device extension");

            ApplyDependency(
                requests.deviceExtensions, &MergedExtensionRequest::name,
                std::string{VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME},
                requests.deviceExtensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME, "Device extension");

            ApplyDependency(
                requests.deviceExtensions, &MergedExtensionRequest::name,
                std::string{VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME},
                requests.instanceExtensions, VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME, "Device extension");

            ApplyDependency(
                requests.deviceFeatures, &MergedFeatureRequest::feature,
                DeviceFeature::SwapchainMaintenance1,
                requests.deviceExtensions, VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME, "Device feature");
        }

    }

    MergedCapabilityRequests MergeCapabilityRequests(
        const core::Span<const CapabilityContribution> contributions)
    {
        MergedCapabilityRequests merged;

        for (const auto &contribution : contributions)
        {
            if (contribution.source.empty())
                KAISOU_FATAL("Capability contribution source must not be empty");
            AppendApiVersion(merged, contribution.requirements.minimumApiVersion, contribution.source);

            AppendRequests<AppendExtension>(
                merged.instanceExtensions, contribution.requiredInstanceExtensions,
                contribution.source, FeatureRequirement::Required);
            AppendRequests<AppendExtension>(
                merged.instanceExtensions, contribution.preferredInstanceExtensions,
                contribution.source, FeatureRequirement::Preferred);

            AppendRequests<AppendExtension>(
                merged.deviceExtensions, contribution.requirements.extensions,
                contribution.source, FeatureRequirement::Required);
            AppendRequests<AppendExtension>(
                merged.deviceExtensions, contribution.preferences.extensions,
                contribution.source, FeatureRequirement::Preferred);

            AppendRequests<AppendFeature>(
                merged.deviceFeatures, contribution.requirements.features,
                contribution.source, FeatureRequirement::Required);
            AppendRequests<AppendFeature>(
                merged.deviceFeatures, contribution.preferences.features,
                contribution.source, FeatureRequirement::Preferred);

            AppendRequests<AppendQueueRequirement>(
                merged.deviceQueues, contribution.requirements.queues, contribution.source);
            AppendRequests<AppendQueuePreference>(
                merged.deviceQueues, contribution.preferences.queues, contribution.source);

            AppendRequests<AppendLimit>(
                merged.deviceLimits, contribution.requirements.limits,
                contribution.source, FeatureRequirement::Required);
            AppendRequests<AppendLimit>(
                merged.deviceLimits, contribution.preferences.limits,
                contribution.source, FeatureRequirement::Preferred);
        }

        return merged;
    }
    void ApplyCapabilityPolicy(MergedCapabilityRequests &requests, const CapabilityPolicy &policy)
    {
        ApplyForbiddenRequests(
            requests.instanceExtensions, policy.forbiddenInstanceExtensions,
            &MergedExtensionRequest::name, policy, "Instance extension");

        ApplyForbiddenRequests(
            requests.deviceExtensions, policy.forbiddenDeviceExtensions,
            &MergedExtensionRequest::name, policy, "Device extension");

        ApplyForbiddenRequests(
            requests.deviceFeatures, policy.forbiddenDeviceFeatures,
            &MergedFeatureRequest::feature, policy, "Device feature");
        ApplyCapabilityDependencies(requests);
    }
    void ResolveInstanceExtensionRequests(
        MergedCapabilityRequests &requests, const core::Span<const VkExtensionProperties> availableExtensions)
    {
        ResolveSupportedRequests(
            requests.instanceExtensions, &MergedExtensionRequest::name,
            [&](const MergedExtensionRequest &request)
            {
                return std::ranges::any_of(availableExtensions, [&](const VkExtensionProperties &extension)
                                           { return request.name == extension.extensionName; });
            },
            "not supported by the Vulkan runtime", "Instance extension");

        ApplyCapabilityDependencies(requests);
    }
    void ResolveDeviceCapabilityRequests(
        MergedCapabilityRequests &requests, const DeviceCapabilities &capabilities)
    {
        const auto reason =
            fmt::format("not supported by selected GPU '{}'", capabilities.properties.deviceName);

        ResolveSupportedRequests(
            requests.deviceExtensions, &MergedExtensionRequest::name,
            [&](const MergedExtensionRequest &request)
            {
                return std::ranges::find(capabilities.extensions, request.name) != capabilities.extensions.end();
            },
            reason, "Device extension");

        ResolveSupportedRequests(
            requests.deviceFeatures, &MergedFeatureRequest::feature,
            [&](const MergedFeatureRequest &request)
            {
                return ResolveDeviceFeatureRequirement(request.requirement, request.feature, capabilities) ==
                       FeatureDecision::Enable;
            },
            reason, "Device feature");

        ApplyCapabilityDependencies(requests);
    }
}
