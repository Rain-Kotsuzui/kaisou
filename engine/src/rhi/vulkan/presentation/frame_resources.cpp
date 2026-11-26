#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"
#include "kaisou/rhi/vulkan/device.hpp"
#include "kaisou/rhi/vulkan/presentation/frame_resources.hpp"
#include "kaisou/rhi/vulkan/diagnostics/debug.hpp"

#include <cstdio>
#include <string>
#include <limits>

namespace kaisou::rhi::vulkan
{
    FrameResources::FrameResources(
        const Device &device,
        const std::size_t swapchainImageCount,
        const std::uint32_t framesInFlight)
        : device_(device.Handle())
    {
        if (framesInFlight == 0)
            KAISOU_FATAL("Frames in flight must be greater than zero");

        if (swapchainImageCount == 0)
            KAISOU_FATAL("Swapchain image count must be greater than zero");
        try
        {
            frameSlots_.resize(framesInFlight);
            presentImages_.resize(swapchainImageCount);

            const auto fail = [this](const char *operation, const VkResult result)
            {
                Destroy();
                const std::string message =
                    std::string{operation} + " failed with VkResult " + std::to_string(static_cast<int>(result));
                KAISOU_FATAL(message);
            };

            VkCommandPoolCreateInfo commandPoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            commandPoolInfo.queueFamilyIndex = device.GraphicsQueue().familyIndex;

            VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // 第一次使用无上一轮完成信号，直接已发出信号

            const VkFenceCreateInfo presentFenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            const VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

            for (FrameSlot &slot : frameSlots_)
            {
                VkResult result = vkCreateCommandPool(device_, &commandPoolInfo, nullptr, &slot.commandPool);
                if (result != VK_SUCCESS)
                    fail("vkCreateCommandPool", result);

                VkCommandBufferAllocateInfo commandBufferInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                commandBufferInfo.commandPool = slot.commandPool;
                commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                commandBufferInfo.commandBufferCount = 1;

                result = vkAllocateCommandBuffers(device_, &commandBufferInfo, &slot.commandBuffer);
                if (result != VK_SUCCESS)
                    fail("vkAllocateCommandBuffers", result);

                result = vkCreateFence(device_, &fenceInfo, nullptr, &slot.submitFence);
                if (result != VK_SUCCESS)
                    fail("vkCreateFence", result);

                result = vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &slot.acquireSemaphore);
                if (result != VK_SUCCESS)
                    fail("vkCreateSemaphore for acquire", result);
            }

            for (PresentImageResources &image : presentImages_)
            {
                VkResult result = vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &image.renderFinishedSemaphore);
                if (result != VK_SUCCESS)
                    fail("vkCreateSemaphore for present", result);

                result = vkCreateFence(device_, &presentFenceInfo, nullptr, &image.presentFence);
                if (result != VK_SUCCESS)
                    fail("vkCreateFence for present", result);
            }

            KAISOU_LOG_INFO(
                "Vulkan", "Frame resources: {} frame slots, {} Swapchain images, {} Present fences",
                frameSlots_.size(), presentImages_.size(), presentImages_.size());
        }
        catch (...)
        {
            Destroy();
            throw;
        }
    }


    VkResult FrameResources::SetDebugNames(const Device &device, const core::u64 generation) noexcept
    {
#if KAISOU_ENABLE_GPU_DEBUG
        const auto generationNumber = static_cast<unsigned long long>(generation);
        VkResult result = VK_SUCCESS;

        const auto nameObject = [&device, generationNumber, &result](
                                    VkObjectType objectType, auto handle, const char *group, std::size_t index, const char *resource)
        {
            if (result != VK_SUCCESS)
                return;

            char name[128];
            (void)std::snprintf(
                name, sizeof(name), "Swapchain[generation=%llu].%s[%zu].%s",
                generationNumber, group, index, resource);
            result = SetDebugName(device, objectType, handle, name);
        };

        for (std::size_t index = 0; index < frameSlots_.size(); ++index)
        {
            const auto &slot = frameSlots_[index];
            nameObject(VK_OBJECT_TYPE_COMMAND_POOL, slot.commandPool, "FrameSlot", index, "CommandPool");
            nameObject(VK_OBJECT_TYPE_COMMAND_BUFFER, slot.commandBuffer, "FrameSlot", index, "CommandBuffer");
            nameObject(VK_OBJECT_TYPE_FENCE, slot.submitFence, "FrameSlot", index, "SubmitFence");
            nameObject(VK_OBJECT_TYPE_SEMAPHORE, slot.acquireSemaphore, "FrameSlot", index, "AcquireSemaphore");
        }

        for (std::size_t index = 0; index < presentImages_.size(); ++index)
        {
            const auto &image = presentImages_[index];
            nameObject(
                VK_OBJECT_TYPE_SEMAPHORE, image.renderFinishedSemaphore,
                "PresentImage", index, "RenderFinishedSemaphore");
            nameObject(VK_OBJECT_TYPE_FENCE, image.presentFence, "PresentImage", index, "PresentFence");
        }

        return result;
#else
        (void)device;
        (void)generation;
        return VK_SUCCESS;
#endif
    }

    FrameResources::~FrameResources()
    {
        // 先确认 GPU 提交和 pending Present 完成
        Destroy();
    }

    FrameSlot &FrameResources::Slot(const std::uint32_t frameSlot) noexcept
    {
        KAISOU_ASSERT(frameSlot < frameSlots_.size(), "Frame slot index out of range");
        return frameSlots_[frameSlot];
    }

    const FrameSlot &FrameResources::Slot(const std::uint32_t frameSlot) const noexcept
    {
        KAISOU_ASSERT(frameSlot < frameSlots_.size(), "Frame slot index out of range");
        return frameSlots_[frameSlot];
    }

    VkSemaphore FrameResources::RenderFinishedSemaphore(const std::uint32_t imageIndex) const noexcept
    {
        KAISOU_ASSERT(imageIndex < presentImages_.size(), "Swapchain image index out of range");
        return presentImages_[imageIndex].renderFinishedSemaphore;
    }
    VkFence FrameResources::PresentFence(const std::uint32_t imageIndex) const noexcept
    {
        KAISOU_ASSERT(imageIndex < presentImages_.size(), "Swapchain image index out of range");
        return presentImages_[imageIndex].presentFence;
    }

    VkResult FrameResources::WaitForPresent(const std::uint32_t imageIndex) noexcept
    {
        KAISOU_ASSERT(imageIndex < presentImages_.size(), "Swapchain image index out of range");
        auto &image = presentImages_[imageIndex];

        if (!image.presentPending)
            return VK_SUCCESS;

        const VkResult result = vkWaitForFences(
            device_, 1, &image.presentFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max());

        if (result == VK_SUCCESS)
            image.presentPending = false;
        return result;
    }

    VkResult FrameResources::PreparePresent(const std::uint32_t imageIndex) noexcept
    {
        const VkResult result = WaitForPresent(imageIndex);
        if (result != VK_SUCCESS)
            return result;

        return vkResetFences(device_, 1, &presentImages_[imageIndex].presentFence);
    }

    void FrameResources::RecordPresentResult(const std::uint32_t imageIndex, const VkResult result) noexcept
    {
        KAISOU_ASSERT(imageIndex < presentImages_.size(), "Swapchain image index out of range");
        auto &image = presentImages_[imageIndex];
        KAISOU_ASSERT(!image.presentPending, "Previous Present must be completed before reuse");

        switch (result)
        {
        case VK_SUCCESS:
        case VK_SUBOPTIMAL_KHR:
        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_ERROR_SURFACE_LOST_KHR:
        case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
            image.presentPending = true;
            break;

        default:
            // 内存不足未入队；Device Lost 不进入正常等待流程。
            break;
        }
    }

    VkResult FrameResources::WaitForPresents() noexcept
    {
        for (std::size_t index = 0; index < presentImages_.size(); ++index)
        {
            const VkResult result = WaitForPresent(static_cast<std::uint32_t>(index));
            if (result != VK_SUCCESS)
                return result;
        }

        return VK_SUCCESS;
    }

    VkResult FrameResources::QueryPresentCompletion() noexcept
    {
        for (auto &image : presentImages_)
        {
            if (!image.presentPending)
                continue;

            const VkResult result = vkGetFenceStatus(device_, image.presentFence);
            if (result != VK_SUCCESS)
                return result;

            image.presentPending = false;
        }

        return VK_SUCCESS;
    }

    VkResult FrameResources::QueryCompletion() noexcept
    {
        for (const auto &slot : frameSlots_)
        {
            const VkResult result = vkGetFenceStatus(device_, slot.submitFence);
            if (result != VK_SUCCESS)
                return result;
        }

        return QueryPresentCompletion();
    }

    void FrameResources::Destroy() noexcept
    {
        if (device_ == VK_NULL_HANDLE)
            return;

        for (PresentImageResources &image : presentImages_)
        {
            if (image.presentFence != VK_NULL_HANDLE)
                vkDestroyFence(device_, image.presentFence, nullptr);
            if (image.renderFinishedSemaphore != VK_NULL_HANDLE)
                vkDestroySemaphore(device_, image.renderFinishedSemaphore, nullptr);

            image = {};
        }

        for (FrameSlot &slot : frameSlots_)
        {
            if (slot.acquireSemaphore != VK_NULL_HANDLE)
                vkDestroySemaphore(device_, slot.acquireSemaphore, nullptr);
            if (slot.submitFence != VK_NULL_HANDLE)
                vkDestroyFence(device_, slot.submitFence, nullptr);
            if (slot.commandPool != VK_NULL_HANDLE)
                vkDestroyCommandPool(device_, slot.commandPool, nullptr);

            slot = {};
        }

        presentImages_.clear();
        frameSlots_.clear();
    }
}
