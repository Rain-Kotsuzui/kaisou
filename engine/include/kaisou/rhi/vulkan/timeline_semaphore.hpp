#ifndef KAI_VULKAN_TIMELINE_SEMAPHORE_HPP
#define KAI_VULKAN_TIMELINE_SEMAPHORE_HPP

#include "kaisou/core/types.hpp"
#include "kaisou/rhi/vulkan/vulkan.hpp"

namespace kaisou::rhi::vulkan
{
    class Device;

    class TimelineSemaphore final
    {
    public:
        explicit TimelineSemaphore(const Device &device, core::u64 initialValue = 0);
        ~TimelineSemaphore();

        TimelineSemaphore(const TimelineSemaphore &) = delete;
        TimelineSemaphore &operator=(const TimelineSemaphore &) = delete;
        TimelineSemaphore(TimelineSemaphore &&) = delete;
        TimelineSemaphore &operator=(TimelineSemaphore &&) = delete;

        [[nodiscard]] VkSemaphore Handle() const noexcept { return semaphore_; }

        [[nodiscard]] VkResult GetValue(core::u64 &value) const noexcept;
        [[nodiscard]] VkResult Signal(core::u64 value) const noexcept;

        // 等待计数 >= value；超时单位为纳秒，0 表示不阻塞检查
        [[nodiscard]] VkResult Wait(core::u64 value, core::u64 timeoutNanoseconds) const noexcept;

    private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkSemaphore semaphore_ = VK_NULL_HANDLE;
    };
}

#endif