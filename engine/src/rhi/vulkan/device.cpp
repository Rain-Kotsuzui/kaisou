
#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"

#include "kaisou/rhi/vulkan/instance.hpp"
#include "kaisou/rhi/vulkan/presentation/window_surface.hpp"
#include "kaisou/rhi/vulkan/device.hpp"
#include "kaisou/rhi/vulkan/diagnostics/debug.hpp"
#include "kaisou/core/types.hpp"

#include <VkBootstrap.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <array>
#include <cstdio>

namespace kaisou::rhi::vulkan
{
    namespace
    {
        vkb::Instance MakeBootstrapInstance(const Instance &instance)
        {
            vkb::Instance bootstrapInstance;

            bootstrapInstance.instance = instance.Handle();
            bootstrapInstance.instance_version = VK_API_VERSION_1_4;
            bootstrapInstance.api_version = VK_API_VERSION_1_4;
            bootstrapInstance.fp_vkGetInstanceProcAddr = vkGetInstanceProcAddr;
            bootstrapInstance.fp_vkGetDeviceProcAddr = vkGetDeviceProcAddr;

            return bootstrapInstance;
        }

        DeviceQueue GetRequiredQueue(const vkb::Device &device, const vkb::QueueType type, const std::string_view name)
        {
            auto queueResult = device.get_queue_and_index(type);

            if (!queueResult)
            {
                const std::string message =
                    "Failed to get " +
                    std::string{name} +
                    " queue: " +
                    queueResult.error().message();

                KAISOU_FATAL(message);
            }

            const auto [handle, familyIndex] = std::move(queueResult).value();

            return DeviceQueue{handle, familyIndex};
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
        const Instance &instance,
        const WindowSurface &surface,
        const DeviceConfig &config)
    {
        try
        {
            const auto bootstrapInstance = MakeBootstrapInstance(instance);

            VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
            VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR maintenanceFeatures{
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR};
            // TODO(M2-CAP-03): Move subsystem feature requirements into unified device capability negotiation.
            features12.timelineSemaphore = config.requireTimelineSemaphore
                                               ? VK_TRUE
                                               : VK_FALSE;

            features13.dynamicRendering = VK_TRUE;
            features13.synchronization2 = VK_TRUE;
            maintenanceFeatures.swapchainMaintenance1 = VK_TRUE;

            vkb::PhysicalDeviceSelector selector{bootstrapInstance, surface.Handle()};

            selector
                .set_minimum_version(1, 4)
                .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                .allow_any_gpu_device_type(config.allowNonDiscreteFallback)
                .require_present(true)
                .set_required_features_12(features12)
                .set_required_features_13(features13)
                .add_required_extension(VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)
                .add_required_extension_features(maintenanceFeatures);

            auto physicalResult = selector.select();

            if (!physicalResult)
            {
                const std::string message = physicalResult.error().message();
                KAISOU_FATAL(message);
            }

            const auto physicalDevice = std::move(physicalResult).value();

            vkb::DeviceBuilder deviceBuilder{physicalDevice};
            auto deviceResult = deviceBuilder.build();

            if (!deviceResult)
            {
                const std::string message = deviceResult.error().message();
                KAISOU_FATAL(message);
            }

            const auto builtDevice = std::move(deviceResult).value();

            physicalDevice_ = physicalDevice.physical_device;
            device_ = builtDevice.device;
            properties_ = physicalDevice.properties;
            volkLoadDevice(device_);
            debugUtilsEnabled_ = instance.DebugUtilsEnabled();

            // TODO：当前每个 Family 仅使用 Queue[0]，Queue 拓扑和多 Queue 调度后续实现。
            graphicsQueue_ = GetRequiredQueue(builtDevice, vkb::QueueType::graphics, "graphics");
            presentQueue_ = GetRequiredQueue(builtDevice, vkb::QueueType::present, "present");
            computeQueue_ = GetRequiredQueue(builtDevice, vkb::QueueType::compute, "compute");
            transferQueue_ = GetRequiredQueue(builtDevice, vkb::QueueType::transfer, "transfer");

#if KAISOU_ENABLE_GPU_DEBUG
            const auto nameObject = [this](
                                        const VkObjectType objectType, const std::uint64_t objectHandle, const char *name)
            {
                const VkResult result = SetDebugName(*this, objectType, objectHandle, name);
                if (result != VK_SUCCESS)
                    KAISOU_LOG_WARN("Vulkan", "Naming {} failed with VkResult {}", name, static_cast<int>(result));
            };

            nameObject(VK_OBJECT_TYPE_DEVICE, reinterpret_cast<std::uint64_t>(device_), "Kaisou.Device");

            const std::array<const DeviceQueue *, 4> queues{
                &graphicsQueue_, &presentQueue_, &computeQueue_, &transferQueue_};

            for (std::size_t index = 0; index < queues.size(); ++index)
            {
                const auto &queue = *queues[index];
                const auto previous = core::Span{queues}.first(index);

                if (std::ranges::find(previous, queue.handle, &DeviceQueue::handle) != previous.end())
                    continue;

                char name[128];
                (void)std::snprintf(
                    name, sizeof(name), "Queue[family=%u]%s%s%s%s",
                    static_cast<unsigned int>(queue.familyIndex),
                    queue.handle == graphicsQueue_.handle ? " [Graphics]" : "",
                    queue.handle == presentQueue_.handle ? " [Present]" : "",
                    queue.handle == computeQueue_.handle ? " [Compute]" : "",
                    queue.handle == transferQueue_.handle ? " [Transfer]" : "");

                nameObject(VK_OBJECT_TYPE_QUEUE, reinterpret_cast<std::uint64_t>(queue.handle), name);
            }
#endif

            KAISOU_LOG_INFO(
                "Vulkan",
                "Selected GPU: {} ({}, Vulkan {}.{}.{})",
                properties_.deviceName,
                DeviceTypeName(properties_.deviceType),
                VK_API_VERSION_MAJOR(properties_.apiVersion),
                VK_API_VERSION_MINOR(properties_.apiVersion),
                VK_API_VERSION_PATCH(properties_.apiVersion));

            KAISOU_LOG_INFO(
                "Vulkan",
                "Queue families: graphics {}, present {}, "
                "compute {}, transfer {}",
                graphicsQueue_.familyIndex,
                presentQueue_.familyIndex,
                computeQueue_.familyIndex,
                transferQueue_.familyIndex);
            KAISOU_LOG_INFO(
                "Vulkan",
                "Device initialized with VK_KHR_swapchain_maintenance1, swapchainMaintenance1 true");
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

    const VkPhysicalDeviceProperties &Device::Properties() const noexcept
    {
        return properties_;
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
