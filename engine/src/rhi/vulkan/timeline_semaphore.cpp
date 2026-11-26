#include "kaisou/core/assert.hpp"
#include "kaisou/rhi/vulkan/device.hpp"
#include "kaisou/rhi/vulkan/timeline_semaphore.hpp"

#include <string>

namespace kaisou::rhi::vulkan
{
    TimelineSemaphore::TimelineSemaphore(const Device &device, const core::u64 initialValue)
        : device_(device.Handle())
    {
        VkSemaphoreTypeCreateInfo typeInfo{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        typeInfo.initialValue = initialValue;

        VkSemaphoreCreateInfo createInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        createInfo.pNext = &typeInfo;

        const VkResult result = vkCreateSemaphore(device_, &createInfo, nullptr, &semaphore_);

        if (result != VK_SUCCESS)
        {
            const std::string message =
                "vkCreateSemaphore for timeline failed with VkResult " + std::to_string(static_cast<int>(result));
            KAISOU_FATAL(message);
        }
    }

    TimelineSemaphore::~TimelineSemaphore()
    {
        // 必须先确认引用该信号量的 GPU 提交已完成
        vkDestroySemaphore(device_, semaphore_, nullptr);
    }

    VkResult TimelineSemaphore::GetValue(core::u64 &value) const noexcept
    {
        return vkGetSemaphoreCounterValue(device_, semaphore_, &value);
    }

    VkResult TimelineSemaphore::Signal(const core::u64 value) const noexcept
    {
        VkSemaphoreSignalInfo signalInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
        signalInfo.semaphore = semaphore_;
        signalInfo.value = value;

        return vkSignalSemaphore(device_, &signalInfo);
    }

    VkResult TimelineSemaphore::Wait(const core::u64 value, const core::u64 timeoutNanoseconds) const noexcept
    {
        VkSemaphoreWaitInfo waitInfo{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &semaphore_;
        waitInfo.pValues = &value;

        return vkWaitSemaphores(device_, &waitInfo, timeoutNanoseconds);
    }
}