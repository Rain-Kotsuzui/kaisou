#ifndef KAI_VULKAN_DEVICE_HPP
#define KAI_VULKAN_DEVICE_HPP

#include "kaisou/rhi/vulkan/vulkan.hpp"
#include "kaisou/rhi/vulkan/capabilities/device_capabilities.hpp"
#include "kaisou/rhi/vulkan/capabilities/queue_topology.hpp"

#include <vector>
#include <cstdint>

namespace kaisou::rhi::vulkan
{
    class Instance;
    struct MergedCapabilityRequests;

    struct DeviceConfig
    {
        // 优先独显，允许集显、虚拟 GPU 回退。
        bool allowNonDiscreteFallback = true;
    };

    struct DeviceQueue
    {
        VkQueue handle = VK_NULL_HANDLE;

        std::uint32_t familyIndex = VK_QUEUE_FAMILY_IGNORED;
        std::uint32_t queueIndex = 0;

        [[nodiscard]]
        bool IsValid() const noexcept
        {
            return handle != VK_NULL_HANDLE &&
                   familyIndex != VK_QUEUE_FAMILY_IGNORED;
        }
    };

    class Device final
    {
    public:
        Device(
            const Instance &instance, MergedCapabilityRequests &requests,
            VkSurfaceKHR surface = VK_NULL_HANDLE, const DeviceConfig &config = {});

        ~Device();

        Device(const Device &) = delete;
        Device &operator=(const Device &) = delete;
        Device(Device &&) = delete;
        Device &operator=(Device &&) = delete;

        [[nodiscard]]
        VkPhysicalDevice PhysicalHandle() const noexcept;
        [[nodiscard]]
        VkDevice Handle() const noexcept;
        [[nodiscard]]
        const DeviceCapabilities &Capabilities() const noexcept;
        [[nodiscard]]
        const VkPhysicalDeviceProperties &Properties() const noexcept;

        [[nodiscard]]
        const DeviceQueue &GraphicsQueue() const noexcept;
        [[nodiscard]]
        const DeviceQueue &PresentQueue() const noexcept;
        [[nodiscard]]
        const DeviceQueue &ComputeQueue() const noexcept;
        [[nodiscard]]
        const DeviceQueue &TransferQueue() const noexcept;
        [[nodiscard]]
        core::Span<const DeviceQueue> QueuePool(DeviceQueueRole role) const noexcept;

        [[nodiscard]] bool DebugUtilsEnabled() const noexcept { return enabledFeatures_.debugUtilsEnabled; }
        [[nodiscard]]
        const EnabledDeviceFeatures &EnabledFeatures() const noexcept
        {
            return enabledFeatures_;
        }

    private:
        void Destroy() noexcept;

        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;

        DeviceCapabilities capabilities_;
        EnabledDeviceFeatures enabledFeatures_;
        std::vector<DeviceQueue> queues_;
        std::vector<QueueRoleMapping> queueRoles_;
        
        DeviceQueue graphicsQueue_;
        DeviceQueue presentQueue_;
        DeviceQueue computeQueue_;
        DeviceQueue transferQueue_;
    };
}

#endif