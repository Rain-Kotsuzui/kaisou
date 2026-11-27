#include "kaisou/rhi/vulkan/capabilities/queue_topology.hpp"

#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"

#include <algorithm>
#include <utility>

namespace kaisou::rhi::vulkan
{
    namespace
    {
        bool SupportsQueueRole(const QueueFamilyCapabilities &family, const DeviceQueueRole role)
        {
            const auto flags = family.properties.queueFlags;

            switch (role)
            {
            case DeviceQueueRole::Graphics:
                return (flags & VK_QUEUE_GRAPHICS_BIT) != 0;

            case DeviceQueueRole::Present:
                return family.supportsPresent;

            case DeviceQueueRole::Compute:
                return (flags & VK_QUEUE_COMPUTE_BIT) != 0;

            case DeviceQueueRole::Transfer:
                return (flags & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) != 0;
            }

            KAISOU_FATAL("Invalid DeviceQueueRole in queue topology");
        }

        bool IsDedicatedFamily(const QueueFamilyCapabilities &family, const DeviceQueueRole role)
        {
            const auto flags = family.properties.queueFlags;

            switch (role)
            {
            case DeviceQueueRole::Compute:
                return (flags & VK_QUEUE_GRAPHICS_BIT) == 0;

            case DeviceQueueRole::Transfer:
                return (flags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == 0;

            case DeviceQueueRole::Graphics:
            case DeviceQueueRole::Present:
                return false;
            }

            KAISOU_FATAL("Invalid DeviceQueueRole in dedicated-family preference");
        }

        const QueueFamilyCapabilities *SelectQueueFamily(
            const MergedQueueRequest &request, const DeviceCapabilities &capabilities,
            const QueueTopology &topology, const MergedQueueRequest *presentRequest)
        {
            const auto suitable = [&](const QueueFamilyCapabilities &family)
            {
                if (request.role == DeviceQueueRole::Present && !capabilities.presentSupportQueried)
                    return false;

                return family.properties.queueCount >= request.minimumCount &&
                       SupportsQueueRole(family, request.role);
            };

            const auto findFamily = [&](const auto &predicate) -> const QueueFamilyCapabilities *
            {
                const auto found = std::ranges::find_if(capabilities.queueFamilies, predicate);
                return found == capabilities.queueFamilies.end() ? nullptr : &*found;
            };

            if (request.preferDedicatedFamily)
            {
                const auto *dedicated = findFamily([&](const QueueFamilyCapabilities &family)
                {
                    return suitable(family) && IsDedicatedFamily(family, request.role);
                });

                if (dedicated != nullptr)
                    return dedicated;

                KAISOU_LOG_WARN(
                    "Vulkan", "GPU '{}': role {} dedicated-family preference unavailable; try compatible families",
                    capabilities.properties.deviceName, static_cast<core::u32>(request.role));

                for (const auto &source : request.preferredSources)
                {
                    if (source.preferDedicatedFamily)
                        KAISOU_LOG_INFO("Vulkan", "  Preference source: {}", source.source);
                }
            }

            if (request.role == DeviceQueueRole::Graphics && presentRequest != nullptr &&
                presentRequest->minimumCount != 0 && capabilities.presentSupportQueried)
            {
                const auto *shared = findFamily([&](const QueueFamilyCapabilities &family)
                {
                    return suitable(family) && family.supportsPresent &&
                           family.properties.queueCount >= presentRequest->minimumCount;
                });

                if (shared != nullptr)
                    return shared;
            }

            for (const auto &existing : topology.families)
            {
                const auto *shared = findFamily([&](const QueueFamilyCapabilities &family)
                {
                    return family.familyIndex == existing.familyIndex && suitable(family);
                });

                if (shared != nullptr)
                    return shared;
            }

            return findFamily(suitable);
        }
    }

    bool BuildQueueTopology(
        const core::Span<const MergedQueueRequest> requests,
        const DeviceCapabilities &capabilities, QueueTopology &topology)
    {
        topology = {};
        QueueTopology candidate;
        bool accepted = true;
        bool hasRequiredRoles = false;

        const auto present = std::ranges::find(requests, DeviceQueueRole::Present, &MergedQueueRequest::role);
        const auto *presentRequest = present == requests.end() ? nullptr : &*present;

        constexpr DeviceQueueRole roleOrder[]{
            DeviceQueueRole::Graphics,
            DeviceQueueRole::Present,
            DeviceQueueRole::Compute,
            DeviceQueueRole::Transfer
        };

        for (const auto role : roleOrder)
        {
            const auto request = std::ranges::find(requests, role, &MergedQueueRequest::role);
            if (request == requests.end() || request->minimumCount == 0)
                continue;

            hasRequiredRoles = true;
            const auto *family = SelectQueueFamily(*request, capabilities, candidate, presentRequest);

            if (family == nullptr)
            {
                const char *reason =
                    role == DeviceQueueRole::Present && !capabilities.presentSupportQueried
                        ? "Present support was not queried for a Surface"
                        : "no compatible single family has enough queues";

                KAISOU_LOG_WARN(
                    "Vulkan", "GPU '{}': Required queue role {} needs {} queue(s); {}; reject candidate",
                    capabilities.properties.deviceName, static_cast<core::u32>(role),
                    request->minimumCount, reason);

                for (const auto &source : request->requiredSources)
                {
                    KAISOU_LOG_INFO(
                        "Vulkan", "  Queue request source: {} (minimum {})",
                        source.source, source.minimumCount);
                }

                accepted = false;
                continue;
            }

            auto existing =
                std::ranges::find(candidate.families, family->familyIndex, &QueueFamilyRequest::familyIndex);

            if (existing == candidate.families.end())
                candidate.families.push_back({family->familyIndex, request->minimumCount});
            else
                existing->queueCount = std::max(existing->queueCount, request->minimumCount);

            candidate.roles.push_back({role, family->familyIndex, request->minimumCount});
        }

        if (!hasRequiredRoles)
        {
            KAISOU_LOG_WARN("Vulkan", "No Required queue roles were requested; no device queue topology generated");
            accepted = false;
        }

        if (!accepted)
            return false;

        topology = std::move(candidate);
        return true;
    }
}