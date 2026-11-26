#ifndef KAI_VULKAN_SWAPCHAIN_HPP
#define KAI_VULKAN_SWAPCHAIN_HPP

#include "kaisou/rhi/vulkan/vulkan.hpp"
#include "kaisou/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace kaisou::platform
{
    class Window;
}

namespace kaisou::rhi::vulkan
{
    class Device;
    class WindowSurface;

    enum class PresentPolicy
    {
        VSync,               // FIFO，不撕裂
        LowLatencyNoTearing, // MAILBOX，不支持时回退 FIFO
        LowestLatency        // IMMEDIATE，允许撕裂
    };

    struct SwapchainConfig
    {
        PresentPolicy presentPolicy = PresentPolicy::VSync;
        std::uint32_t desiredImageCount = 3; // 三缓冲
    };

    class Swapchain final
    {
    public:
        Swapchain(
            const Device &device, const WindowSurface &surface,
            const platform::Window &window, const SwapchainConfig &config = {},
            VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);

        ~Swapchain();

        Swapchain(const Swapchain &) = delete;
        Swapchain &operator=(const Swapchain &) = delete;
        Swapchain(Swapchain &&) = delete;
        Swapchain &operator=(Swapchain &&) = delete;

        [[nodiscard]] VkSwapchainKHR Handle() const noexcept { return swapchain_; }
        [[nodiscard]] VkFormat ImageFormat() const noexcept { return imageFormat_; }
        [[nodiscard]] VkColorSpaceKHR ColorSpace() const noexcept { return colorSpace_; }
        [[nodiscard]] VkExtent2D Extent() const noexcept { return extent_; }
        [[nodiscard]] VkPresentModeKHR PresentMode() const noexcept { return presentMode_; }

        [[nodiscard]] core::Span<const VkImage> Images() const noexcept { return images_; }
        [[nodiscard]] core::Span<const VkImageView> ImageViews() const noexcept { return imageViews_; }

        [[nodiscard]] VkResult SetDebugNames(const Device &device, core::u64 generation) noexcept;

    private:
        void Create(
            const Device &device, const WindowSurface &surface,
            const platform::Window &window, const SwapchainConfig &config,
            VkSwapchainKHR oldSwapchain);

        void Destroy() noexcept;

        VkDevice device_ = VK_NULL_HANDLE;
        VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
        VkFormat imageFormat_ = VK_FORMAT_UNDEFINED;
        VkColorSpaceKHR colorSpace_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        VkExtent2D extent_{};
        VkPresentModeKHR presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
        std::vector<VkImage> images_;
        std::vector<VkImageView> imageViews_;
    };
}

#endif
