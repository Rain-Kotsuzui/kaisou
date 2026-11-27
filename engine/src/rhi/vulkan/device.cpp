#include "kaisou/rhi/vulkan/device.hpp"
#include "kaisou/rhi/vulkan/instance.hpp"
#include "kaisou/rhi/vulkan/enumerate.hpp"
#include "kaisou/rhi/vulkan/diagnostics/debug.hpp"
#include "kaisou/rhi/vulkan/capabilities/capability_merge.hpp"
#include "kaisou/rhi/vulkan/capabilities/device_capability_query.hpp"
#include "kaisou/rhi/vulkan/capabilities/device_feature_chain.hpp"
#include "kaisou/rhi/vulkan/capabilities/queue_topology.hpp"

#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <utility>
#include <vector>
#include <fmt/format.h>

namespace kaisou::rhi::vulkan
{
    namespace
    {
        DeviceQueue GetPrimaryQueue(const core::Span<const DeviceQueue> pool)
        {
            return pool.empty() ? DeviceQueue{} : pool.front();
        }
        const char *DeviceTypeName(
            const VkPhysicalDeviceType type)
        {
            switch (type)
            {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                return "Integrated GPU";

            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                return "Discrete GPU";

            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                return "Virtual GPU";

            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                return "CPU";

            default:
                return "Other";
            }
        }
    }

    Device::Device(
        const Instance &instance, MergedCapabilityRequests &requests,
        const VkSurfaceKHR surface, const DeviceConfig &config)
    {
        try
        {
            const auto physicalDevices = EnumerateProperties<VkPhysicalDevice>(
                "vkEnumeratePhysicalDevices", vkEnumeratePhysicalDevices, instance.Handle());

            VkPhysicalDevice selectedPhysicalDevice = VK_NULL_HANDLE;
            DeviceCapabilities selectedCapabilities;
            QueueTopology selectedTopology;

            for (const auto physicalDevice : physicalDevices)
            {
                auto capabilities = QueryDeviceCapabilities(physicalDevice, surface);
                const bool discrete =
                    capabilities.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

                if (!discrete && !config.allowNonDiscreteFallback)
                {
                    KAISOU_LOG_INFO(
                        "Vulkan", "Skipping GPU '{}': non-discrete fallback disabled",
                        capabilities.properties.deviceName);
                    continue;
                }

                if (!CheckDeviceCapabilityRequests(requests, capabilities))
                    continue;

                // Vulkan requires this extension when the device advertises it.
                constexpr const char *portabilitySubset = "VK_KHR_portability_subset";
                const bool portabilitySupported =
                    std::ranges::find(capabilities.extensions, portabilitySubset) != capabilities.extensions.end();

                const bool portabilityRequested = std::ranges::any_of(
                    requests.deviceExtensions, [&](const MergedExtensionRequest &request)
                    { return request.name == portabilitySubset &&
                             request.requirement != FeatureRequirement::Disabled; });

                if (portabilitySupported && !portabilityRequested)
                {
                    KAISOU_LOG_WARN(
                        "Vulkan", "GPU '{}': portability_subset requires an explicit request; reject candidate",
                        capabilities.properties.deviceName);
                    continue;
                }

                QueueTopology topology;
                if (!BuildQueueTopology(requests.deviceQueues, capabilities, topology))
                    continue;

                if (selectedPhysicalDevice == VK_NULL_HANDLE || discrete)
                {
                    selectedPhysicalDevice = physicalDevice;
                    selectedCapabilities = std::move(capabilities);
                    selectedTopology = std::move(topology);
                }

                if (discrete)
                    break;
            }

            if (selectedPhysicalDevice == VK_NULL_HANDLE)
                KAISOU_FATAL("No physical device satisfies capability, queue and device-selection requirements");

            ResolveDeviceCapabilityRequests(requests, selectedCapabilities);

            EnabledDeviceFeatures enablement;
            enablement.apiVersion = std::max(requests.minimumApiVersion, VK_API_VERSION_1_4);
            enablement.debugUtilsEnabled = instance.EnabledFeatures().debugUtilsEnabled;

            enablement.extensions.reserve(requests.deviceExtensions.size());
            for (const auto &request : requests.deviceExtensions)
                enablement.extensions.push_back(request.name);

            enablement.features.reserve(requests.deviceFeatures.size());
            for (const auto &request : requests.deviceFeatures)
                enablement.features.push_back(request.feature);

            std::vector<const char *> extensionNames;
            extensionNames.reserve(enablement.extensions.size());
            for (const auto &extension : enablement.extensions)
                extensionNames.push_back(extension.c_str());

            std::vector<std::vector<float>> priorities;
            priorities.reserve(selectedTopology.families.size());
            for (const auto &family : selectedTopology.families)
                priorities.emplace_back(family.queueCount, 1.0f);

            std::vector<VkDeviceQueueCreateInfo> queueInfos;
            queueInfos.reserve(selectedTopology.families.size());

            for (std::size_t index = 0; index < selectedTopology.families.size(); ++index)
            {
                const auto &family = selectedTopology.families[index];

                VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
                queueInfo.queueFamilyIndex = family.familyIndex;
                queueInfo.queueCount = family.queueCount;
                queueInfo.pQueuePriorities = priorities[index].data();
                queueInfos.push_back(queueInfo);
            }

            DeviceFeatureChain featureChain;
            featureChain.ConfigureEnablement(enablement.features);

            VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            createInfo.pNext = &featureChain.Head();
            createInfo.queueCreateInfoCount = static_cast<core::u32>(queueInfos.size());
            createInfo.pQueueCreateInfos = queueInfos.data();
            createInfo.enabledExtensionCount = static_cast<core::u32>(extensionNames.size());
            createInfo.ppEnabledExtensionNames = extensionNames.empty() ? nullptr : extensionNames.data();
            createInfo.pEnabledFeatures = nullptr;

            VkDevice createdDevice = VK_NULL_HANDLE;
            const VkResult result = vkCreateDevice(selectedPhysicalDevice, &createInfo, nullptr, &createdDevice);

            if (result != VK_SUCCESS)
                KAISOU_FATAL(fmt::format("vkCreateDevice failed: VkResult {}", static_cast<int>(result)));

            // Publish ownership immediately after successful creation.
            device_ = createdDevice;
            physicalDevice_ = selectedPhysicalDevice;
            capabilities_ = std::move(selectedCapabilities);
            volkLoadDevice(device_);

            enabledFeatures_ = std::move(enablement);

            queueRoles_ = std::move(selectedTopology.roles);

            std::size_t queueCount = 0;
            for (const auto &family : selectedTopology.families)
                queueCount += family.queueCount;

            queues_.reserve(queueCount);
            for (const auto &family : selectedTopology.families)
            {
                for (core::u32 queueIndex = 0; queueIndex < family.queueCount; ++queueIndex)
                {
                    DeviceQueue queue{VK_NULL_HANDLE, family.familyIndex, queueIndex};
                    vkGetDeviceQueue(device_, queue.familyIndex, queue.queueIndex, &queue.handle);
                    queues_.push_back(queue);
                }
            }

            graphicsQueue_ = GetPrimaryQueue(QueuePool(DeviceQueueRole::Graphics));
            presentQueue_ = GetPrimaryQueue(QueuePool(DeviceQueueRole::Present));
            computeQueue_ = GetPrimaryQueue(QueuePool(DeviceQueueRole::Compute));
            transferQueue_ = GetPrimaryQueue(QueuePool(DeviceQueueRole::Transfer));

#if KAISOU_ENABLE_GPU_DEBUG
            const auto nameObject = [this](
                                        const VkObjectType objectType, const std::uint64_t objectHandle, const char *name)
            {
                const VkResult result = SetDebugName(*this, objectType, objectHandle, name);
                if (result != VK_SUCCESS)
                    KAISOU_LOG_WARN("Vulkan", "Naming {} failed with VkResult {}", name, static_cast<int>(result));
            };

            nameObject(VK_OBJECT_TYPE_DEVICE, reinterpret_cast<std::uint64_t>(device_), "Kaisou.Device");

            for (std::size_t index = 0; index < queues_.size(); ++index)
            {
                const auto &queue = queues_[index];
                const auto previous = core::Span<const DeviceQueue>{queues_}.first(index);

                if (!queue.IsValid() ||
                    std::ranges::find(previous, queue.handle, &DeviceQueue::handle) != previous.end())
                    continue;

                char name[128];
                (void)std::snprintf(
                    name, sizeof(name), "Queue[family=%u,index=%u]%s%s%s%s",
                    static_cast<unsigned int>(queue.familyIndex),
                    static_cast<unsigned int>(queue.queueIndex),
                    queue.handle == graphicsQueue_.handle ? " [Graphics]" : "",
                    queue.handle == presentQueue_.handle ? " [Present]" : "",
                    queue.handle == computeQueue_.handle ? " [Compute]" : "",
                    queue.handle == transferQueue_.handle ? " [Transfer]" : "");

                nameObject(VK_OBJECT_TYPE_QUEUE, reinterpret_cast<std::uint64_t>(queue.handle), name);
            }
#endif

            const auto &properties = capabilities_.properties;

            KAISOU_LOG_INFO(
                "Vulkan", "Selected GPU: {} ({}, Vulkan {}.{}.{})",
                properties.deviceName,
                DeviceTypeName(properties.deviceType),
                VK_API_VERSION_MAJOR(properties.apiVersion),
                VK_API_VERSION_MINOR(properties.apiVersion),
                VK_API_VERSION_PATCH(properties.apiVersion));

            KAISOU_LOG_INFO(
                "Vulkan", "Effective Vulkan API contract: {}.{}.{}",
                VK_API_VERSION_MAJOR(enabledFeatures_.apiVersion),
                VK_API_VERSION_MINOR(enabledFeatures_.apiVersion),
                VK_API_VERSION_PATCH(enabledFeatures_.apiVersion));

            for (const auto &extension : enabledFeatures_.extensions)
                KAISOU_LOG_INFO("Vulkan", "Enabled device extension: {}", extension);

            for (const auto feature : enabledFeatures_.features)
                KAISOU_LOG_INFO("Vulkan", "Enabled device feature: {}", static_cast<core::u32>(feature));

            for (const auto &family : selectedTopology.families)
                KAISOU_LOG_INFO(
                    "Vulkan", "Created queue family {}: {} queue(s)", family.familyIndex, family.queueCount);
            
                    for (const auto &mapping : queueRoles_)
                KAISOU_LOG_INFO(
                    "Vulkan", "Queue role {}: family {}, Primary index 0, count {}",
                    static_cast<core::u32>(mapping.role), mapping.familyIndex, mapping.queueCount);
        }
        catch (...)
        {
            Destroy();
            throw;
        }
    }

    Device::~Device()
    {
        Destroy();
    }

    void Device::Destroy() noexcept
    {
        if (device_ == VK_NULL_HANDLE)
            return;
        (void)vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);

        device_ = VK_NULL_HANDLE;
    }

    VkPhysicalDevice Device::PhysicalHandle() const noexcept
    {
        return physicalDevice_;
    }

    VkDevice Device::Handle() const noexcept
    {
        return device_;
    }

    const DeviceCapabilities &Device::Capabilities() const noexcept
    {
        return capabilities_;
    }

    const VkPhysicalDeviceProperties &Device::Properties() const noexcept
    {
        return capabilities_.properties;
    }
    core::Span<const DeviceQueue> Device::QueuePool(const DeviceQueueRole role) const noexcept
    {
        const auto mapping = std::ranges::find(queueRoles_, role, &QueueRoleMapping::role);
        if (mapping == queueRoles_.end())
            return {};

        const auto first = std::ranges::find(queues_, mapping->familyIndex, &DeviceQueue::familyIndex);
        if (first == queues_.end())
            KAISOU_FATAL("Queue Pool family is missing from Device queues");

        const auto offset = static_cast<std::size_t>(first - queues_.begin());
        const auto pool = core::Span<const DeviceQueue>{queues_}.subspan(offset);

        if (mapping->queueCount == 0 || mapping->queueCount > pool.size() ||
            pool[mapping->queueCount - 1].familyIndex != mapping->familyIndex)
            KAISOU_FATAL("Queue Pool range does not match its role mapping");

        return pool.first(mapping->queueCount);
    }

    const DeviceQueue &Device::GraphicsQueue() const noexcept
    {
        return graphicsQueue_;
    }

    const DeviceQueue &Device::PresentQueue() const noexcept
    {
        return presentQueue_;
    }

    const DeviceQueue &Device::ComputeQueue() const noexcept
    {
        return computeQueue_;
    }

    const DeviceQueue &Device::TransferQueue() const noexcept
    {
        return transferQueue_;
    }
}
