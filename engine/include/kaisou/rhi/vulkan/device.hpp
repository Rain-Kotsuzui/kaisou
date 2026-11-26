#ifndef KAI_VULKAN_DEVICE_HPP
#define KAI_VULKAN_DEVICE_HPP

#include "kaisou/rhi/vulkan/vulkan.hpp"

#include <cstdint>

namespace kaisou::rhi::vulkan
{
    class Instance;
    class WindowSurface;

    struct DeviceConfig
    {
        // 优先独显，允许集显、虚拟 GPU 回退。
        bool allowNonDiscreteFallback = true;

        // Timeline Semaphore 作为必需能力
        bool requireTimelineSemaphore = true;
    };

    struct DeviceQueue
    {
        VkQueue handle = VK_NULL_HANDLE;

        std::uint32_t familyIndex = VK_QUEUE_FAMILY_IGNORED;

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
            const Instance &instance,
            const WindowSurface &surface,
            const DeviceConfig &config = {});

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
        const VkPhysicalDeviceProperties &Properties() const noexcept;

        [[nodiscard]]
        const DeviceQueue &GraphicsQueue() const noexcept;
        [[nodiscard]]
        const DeviceQueue &PresentQueue() const noexcept;
        [[nodiscard]]
        const DeviceQueue &ComputeQueue() const noexcept;
        [[nodiscard]]
        const DeviceQueue &TransferQueue() const noexcept;

        [[nodiscard]] bool DebugUtilsEnabled() const noexcept { return debugUtilsEnabled_; }

    private:
        void Destroy() noexcept;

        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;

        VkPhysicalDeviceProperties properties_{};

        DeviceQueue graphicsQueue_;
        DeviceQueue presentQueue_;
        DeviceQueue computeQueue_;
        DeviceQueue transferQueue_;

        // TODO(M2-CAP-06): Merge this inherited extension state into the unified capability report.
        bool debugUtilsEnabled_ = false;
    };
}

#endif