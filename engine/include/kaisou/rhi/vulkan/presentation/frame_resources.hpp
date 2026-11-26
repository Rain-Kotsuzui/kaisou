#ifndef KAI_VULKAN_FRAME_RESOURCES_HPP
#define KAI_VULKAN_FRAME_RESOURCES_HPP

#include "kaisou/rhi/vulkan/vulkan.hpp"
#include "kaisou/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace kaisou::rhi::vulkan
{
    class Device;

    struct FrameSlot
    {
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkFence submitFence = VK_NULL_HANDLE;
        VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
    };

    class FrameResources final
    {
    public:
        FrameResources(
            const Device &device,
            std::size_t swapchainImageCount,
            std::uint32_t framesInFlight = 2);

        ~FrameResources();

        FrameResources(const FrameResources &) = delete;
        FrameResources &operator=(const FrameResources &) = delete;
        FrameResources(FrameResources &&) = delete;
        FrameResources &operator=(FrameResources &&) = delete;

        [[nodiscard]] std::size_t FrameSlotCount() const noexcept { return frameSlots_.size(); }

        [[nodiscard]] FrameSlot &Slot(std::uint32_t frameSlot) noexcept;
        [[nodiscard]] const FrameSlot &Slot(std::uint32_t frameSlot) const noexcept;
        [[nodiscard]] VkSemaphore RenderFinishedSemaphore(std::uint32_t imageIndex) const noexcept;

        [[nodiscard]] VkFence PresentFence(std::uint32_t imageIndex) const noexcept;
        [[nodiscard]] VkResult PreparePresent(std::uint32_t imageIndex) noexcept;
        void RecordPresentResult(std::uint32_t imageIndex, VkResult result) noexcept;
        [[nodiscard]] VkResult WaitForPresents() noexcept;

        // VK_SUCCESS：完成；VK_NOT_READY：尚未完成
        [[nodiscard]] VkResult QueryPresentCompletion() noexcept;
        [[nodiscard]] VkResult QueryCompletion() noexcept;

        [[nodiscard]] VkResult SetDebugNames(const Device &device, core::u64 generation) noexcept;
    private:
        struct PresentImageResources
        {
            VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
            VkFence presentFence = VK_NULL_HANDLE;
            bool presentPending = false;
        };
        [[nodiscard]] VkResult WaitForPresent(std::uint32_t imageIndex) noexcept;
        void Destroy() noexcept;

        VkDevice device_ = VK_NULL_HANDLE;
        std::vector<FrameSlot> frameSlots_;
        std::vector<PresentImageResources> presentImages_;
    };
}

#endif
