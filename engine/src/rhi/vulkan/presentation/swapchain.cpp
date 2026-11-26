#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"
#include "kaisou/platform/window.hpp"
#include "kaisou/rhi/vulkan/device.hpp"
#include "kaisou/rhi/vulkan/presentation/swapchain.hpp"
#include "kaisou/rhi/vulkan/presentation/window_surface.hpp"
#include "kaisou/rhi/vulkan/diagnostics/debug.hpp"

#include <VkBootstrap.h>

#include <cstdio>
#include <string>
#include <utility>

namespace kaisou::rhi::vulkan
{
    namespace
    {
        const char *FormatName(const VkFormat format)
        {
            switch (format)
            {
            case VK_FORMAT_B8G8R8A8_SRGB:
                return "B8G8R8A8_SRGB";
            case VK_FORMAT_R8G8B8A8_SRGB:
                return "R8G8B8A8_SRGB";
            case VK_FORMAT_B8G8R8A8_UNORM:
                return "B8G8R8A8_UNORM";
            case VK_FORMAT_R8G8B8A8_UNORM:
                return "R8G8B8A8_UNORM";
            default:
                return "Other";
            }
        }

        const char *PresentModeName(const VkPresentModeKHR mode)
        {
            switch (mode)
            {
            case VK_PRESENT_MODE_FIFO_KHR:
                return "FIFO";
            case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
                return "FIFO_RELAXED";
            case VK_PRESENT_MODE_MAILBOX_KHR:
                return "MAILBOX";
            case VK_PRESENT_MODE_IMMEDIATE_KHR:
                return "IMMEDIATE";
            default:
                return "Other";
            }
        }
    }

    Swapchain::Swapchain(
        const Device &device, const WindowSurface &surface,
        const platform::Window &window, const SwapchainConfig &config,
        const VkSwapchainKHR oldSwapchain)
        : device_(device.Handle())
    {
        try
        {
            Create(device, surface, window, config, oldSwapchain);
        }
        catch (...)
        {
            Destroy();
            throw;
        }
    }
    void Swapchain::Create(
        const Device &device, const WindowSurface &surface,
        const platform::Window &window, const SwapchainConfig &config,
        const VkSwapchainKHR oldSwapchain)
    {
        const auto &windowState = window.State();

        if (!window.IsDrawable())
        {
            KAISOU_LOG_INFO("Vulkan", "窗口不可绘制，不创建交换链");
            return;
        }

        vkb::SwapchainBuilder builder{
            device.PhysicalHandle(),
            device.Handle(),
            surface.Handle(),
            device.GraphicsQueue().familyIndex,
            device.PresentQueue().familyIndex};

        builder
            .set_desired_extent(
                static_cast<std::uint32_t>(windowState.pixelWidth),
                static_cast<std::uint32_t>(windowState.pixelHeight))
            .set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}) // 优先使用 BGRA
            .add_fallback_format({VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .set_desired_min_image_count(config.desiredImageCount)
            .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .set_clipped(true)
            .set_old_swapchain(oldSwapchain);

        switch (config.presentPolicy)
        {
        case PresentPolicy::VSync:
            builder.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR);
            break;

        case PresentPolicy::LowLatencyNoTearing:
            builder
                .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
                .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR);
            break;

        case PresentPolicy::LowestLatency:
            builder
                .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
                .add_fallback_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
                .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR);
            break;

        default:
            KAISOU_FATAL("Unsupported Swapchain present policy");
        }

        auto swapchainResult = builder.build();

        if (!swapchainResult)
        {
            const std::string message = "Failed to create Swapchain: " + swapchainResult.error().message();
            KAISOU_FATAL(message);
        }

        auto builtSwapchain = std::move(swapchainResult).value();
        swapchain_ = builtSwapchain.swapchain;
        imageFormat_ = builtSwapchain.image_format;
        colorSpace_ = builtSwapchain.color_space;
        extent_ = builtSwapchain.extent;
        presentMode_ = builtSwapchain.present_mode;

        auto imagesResult = builtSwapchain.get_images();
        if (!imagesResult)
        {
            const std::string message = "Failed to get Swapchain images: " + imagesResult.error().message();
            Destroy();
            KAISOU_FATAL(message);
        }

        images_ = std::move(imagesResult).value();
        if (images_.empty())
        {
            Destroy();
            KAISOU_FATAL("Swapchain returned no images");
        }

        imageViews_.resize(images_.size(), VK_NULL_HANDLE);

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = imageFormat_;
        viewInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY};
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        for (std::size_t index = 0; index < images_.size(); ++index)
        {
            viewInfo.image = images_[index];
            const VkResult result = vkCreateImageView(device_, &viewInfo, nullptr, &imageViews_[index]);

            if (result != VK_SUCCESS)
            {
                const std::string message =
                    "Failed to create Swapchain ImageView: VkResult " + std::to_string(static_cast<int>(result));

                Destroy();
                KAISOU_FATAL(message);
            }
        }

        KAISOU_LOG_INFO(
            "Vulkan",
            "Swapchain: {} images, {} x {}, format {} ({}), color space {}, present mode {}",
            images_.size(),
            extent_.width,
            extent_.height,
            FormatName(imageFormat_),
            static_cast<int>(imageFormat_),
            static_cast<int>(colorSpace_),
            PresentModeName(presentMode_));

    }

    VkResult Swapchain::SetDebugNames(const Device &device, const core::u64 generation) noexcept
    {
#if KAISOU_ENABLE_GPU_DEBUG
        const auto generationNumber = static_cast<unsigned long long>(generation);
        char name[128];

        (void)std::snprintf(name, sizeof(name), "Swapchain[generation=%llu]", generationNumber);
        VkResult result = SetDebugName(device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, swapchain_, name);
        if (result != VK_SUCCESS)
            return result;

        for (std::size_t index = 0; index < images_.size(); ++index)
        {
            (void)std::snprintf(
                name, sizeof(name), "Swapchain[generation=%llu].Image[%zu]", generationNumber, index);
            result = SetDebugName(device, VK_OBJECT_TYPE_IMAGE, images_[index], name);
            if (result != VK_SUCCESS)
                return result;

            (void)std::snprintf(
                name, sizeof(name), "Swapchain[generation=%llu].ImageView[%zu]", generationNumber, index);
            result = SetDebugName(device, VK_OBJECT_TYPE_IMAGE_VIEW, imageViews_[index], name);
            if (result != VK_SUCCESS)
                return result;
        }
#else
        (void)device;
        (void)generation;
#endif
        return VK_SUCCESS;
    }

    Swapchain::~Swapchain()
    {
        Destroy();
    }

    void Swapchain::Destroy() noexcept
    {
        for (const VkImageView imageView : imageViews_)
            if (imageView != VK_NULL_HANDLE)
                vkDestroyImageView(device_, imageView, nullptr);

        imageViews_.clear();
        images_.clear();

        if (swapchain_ != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(device_, swapchain_, nullptr);

        swapchain_ = VK_NULL_HANDLE;
        imageFormat_ = VK_FORMAT_UNDEFINED;
        colorSpace_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        extent_ = {};
        presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
    }

}
