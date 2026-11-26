#ifndef KAI_VULKAN_PRESENTATION_CONTEXT_HPP
#define KAI_VULKAN_PRESENTATION_CONTEXT_HPP

#include "kaisou/rhi/vulkan/presentation/swapchain.hpp"
#include "kaisou/rhi/vulkan/presentation/frame_resources.hpp"
#include "kaisou/core/time.hpp"
#include "kaisou/rhi/vulkan/diagnostics/gpu_profiler.hpp"
#include "kaisou/rhi/vulkan/diagnostics/gpu_trace.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace kaisou::rhi::vulkan
{
    struct PresentationConfig
    {
        SwapchainConfig swapchain;
        std::uint32_t framesInFlight = 2;
        bool enableFrameStatistics = false;
    };

    enum class PresentFrameResult
    {
        Presented,
        Skipped,
        NeedsRebuild
    };

    struct PresentationFrame
    {
        VkCommandBuffer commandBuffer;
        VkImage image;
        VkImageView imageView;
        VkExtent2D extent;
        VkFormat format;
#if KAISOU_ENABLE_PROFILING
            TracyVkCtx gpuTraceContext = nullptr;
#endif
    };

    class PresentationContext final
    {
    public:
        using RecordFunction = void(void *context, const PresentationFrame &frame) noexcept;

        PresentationContext(const Device &device, const WindowSurface &surface,
                            const platform::Window &window, const PresentationConfig &config = {});
        ~PresentationContext() noexcept;

        PresentationContext(const PresentationContext &) = delete;
        PresentationContext &operator=(const PresentationContext &) = delete;
        PresentationContext(PresentationContext &&) = delete;
        PresentationContext &operator=(PresentationContext &&) = delete;

        [[nodiscard]] PresentFrameResult Render(const platform::Window &window, RecordFunction &record, void *context);
        void RequestPresentPolicy(PresentPolicy policy) noexcept;

    private:
        struct SwapchainGeneration final
        {
            SwapchainGeneration(const Device &device, const WindowSurface &surface, const platform::Window &window,
                                const SwapchainConfig &config, std::uint32_t framesInFlight,
                                VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);

            Swapchain swapchain;
            FrameResources resources;
            std::uint32_t frameSlot = 0;
            bool acquireInvalid = false;

#if KAISOU_ENABLE_PROFILING
            GpuProfiler gpuProfiler;
#endif
        };

        struct FrameStatistics
        {
            core::TimePoint reportStart{};
            std::optional<core::TimePoint> lastAcceptedPresent;
            std::uint64_t submissions = 0;
            std::uint64_t acceptedPresents = 0;
            std::uint64_t intervalSamples = 0;
            double intervalSumMs = 0.0;
            double maxIntervalMs = 0.0;
        };

        void RecordPresentation(VkResult result);
        PresentFrameResult RequestRebuild();
        [[nodiscard]] VkResult WaitForResources() noexcept;
        [[nodiscard]] bool RebuildSwapchain(const platform::Window &window);
        void CollectRetiredGenerations();
#if KAISOU_ENABLE_PROFILING
        void EnsureGpuTraceContext(const SwapchainGeneration &generation);
#endif

        const Device &device_;
        const WindowSurface &surface_;
#if KAISOU_ENABLE_PROFILING
        std::unique_ptr<GpuTraceContext> gpuTraceContext_;
#endif
        std::unique_ptr<SwapchainGeneration> activeGeneration_;
        std::array<std::unique_ptr<SwapchainGeneration>, 2> retiredGenerations_{};
        std::uint64_t swapchainCreations_ = 0;
        std::size_t retiredPeak_ = 0;
        bool retirementPressureReported_ = false;
        SwapchainConfig swapchainConfig_;
        std::uint32_t framesInFlight_ = 2;
        bool enableFrameStatistics_ = false;
        FrameStatistics statistics_;
        int observedPixelWidth_ = 0;
        int observedPixelHeight_ = 0;
        bool rebuildRequired_ = false;
        bool wasDrawable_ = false;
        bool firstPresented_ = false;
    };
}

#endif
