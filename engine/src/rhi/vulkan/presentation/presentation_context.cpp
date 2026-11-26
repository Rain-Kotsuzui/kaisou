#include "kaisou/rhi/vulkan/presentation/presentation_context.hpp"

#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"
#include "kaisou/core/profile.hpp"
#include "kaisou/platform/window.hpp"
#include "kaisou/rhi/vulkan/device.hpp"
#include "kaisou/rhi/vulkan/diagnostics/debug.hpp"

#include <string>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace kaisou::rhi::vulkan
{
    namespace
    {
        void RequireSuccess(const VkResult result, const char *operation)
        {
            if (result == VK_SUCCESS)
                return;

            const std::string message =
                std::string{operation} + " failed with VkResult " + std::to_string(static_cast<int>(result));

            KAISOU_FATAL(message);
        }
    }
    PresentationContext::SwapchainGeneration::SwapchainGeneration(
        const Device &device, const WindowSurface &surface,
        const platform::Window &window, const SwapchainConfig &config,
        const std::uint32_t framesInFlight, const VkSwapchainKHR oldSwapchain)
        : swapchain(device, surface, window, config, oldSwapchain),
          resources(device, swapchain.Images().size(), framesInFlight)
#if KAISOU_ENABLE_PROFILING
          ,
          gpuProfiler(device, framesInFlight)
#endif
    {
    }
#if KAISOU_ENABLE_PROFILING
    void PresentationContext::EnsureGpuTraceContext(const SwapchainGeneration &generation)
    {
        if (gpuTraceContext_ || !generation.gpuProfiler.IsEnabled())
            return;

        gpuTraceContext_ = std::make_unique<GpuTraceContext>(device_);
        KAISOU_LOG_INFO("Vulkan", "Tracy Graphics GPU context initialized");
    }
#endif

    PresentationContext::PresentationContext(
        const Device &device, const WindowSurface &surface,
        const platform::Window &window, const PresentationConfig &config)
        : device_(device),
          surface_(surface),
          swapchainConfig_(config.swapchain),
          framesInFlight_(config.framesInFlight),
          enableFrameStatistics_(config.enableFrameStatistics)
    {
        KAISOU_VERIFY(framesInFlight_ > 0, "Frames in flight must be greater than zero");
        const auto &state = window.State();
        observedPixelWidth_ = state.pixelWidth;
        observedPixelHeight_ = state.pixelHeight;
        wasDrawable_ = window.IsDrawable();

        if (window.IsDrawable())
        {
            KAISOU_PROFILE_SCOPE("Create Swapchain generation");
            activeGeneration_ = std::make_unique<SwapchainGeneration>(
                device_, surface_, window, swapchainConfig_, framesInFlight_);
#if KAISOU_ENABLE_PROFILING
            EnsureGpuTraceContext(*activeGeneration_);
#endif
            ++swapchainCreations_;
#if KAISOU_ENABLE_GPU_DEBUG
            const VkResult nameResult = activeGeneration_->swapchain.SetDebugNames(device_, swapchainCreations_);
            if (nameResult != VK_SUCCESS)
                KAISOU_LOG_WARN(
                    "Vulkan", "Swapchain generation {} naming failed with VkResult {}",
                    swapchainCreations_, static_cast<int>(nameResult));
            const VkResult frameNameResult = activeGeneration_->resources.SetDebugNames(device_, swapchainCreations_);
            if (frameNameResult != VK_SUCCESS)
                KAISOU_LOG_WARN(
                    "Vulkan", "Frame resources generation {} naming failed with VkResult {}",
                    swapchainCreations_, static_cast<int>(frameNameResult));
#endif
        }
        else
        {
            rebuildRequired_ = true;
        }

        statistics_.reportStart = core::Clock::now();

        KAISOU_LOG_INFO(
            "Vulkan", "Presentation context initialized, frame statistics {}", enableFrameStatistics_);
    }

    PresentationContext::~PresentationContext() noexcept
    {
        const VkResult result = WaitForResources();

        if (result != VK_SUCCESS)
        {
            std::fprintf(stderr, "Renderer shutdown wait failed with VkResult %d\n", static_cast<int>(result));
            std::fflush(stderr);

            if (result != VK_ERROR_DEVICE_LOST)
                std::abort();
            return;
        }

#if KAISOU_ENABLE_PROFILING
        if (gpuTraceContext_)
            gpuTraceContext_->FinishCollection();
#endif
        // 析构保护
        try
        {
            KAISOU_LOG_INFO("Renderer", "GPU submissions and pending presentation fences completed");
        }
        catch (...)
        {
            std::fprintf(stderr, "Renderer shutdown completed; logging failed\n");
        }
    }
    VkResult PresentationContext::WaitForResources() noexcept
    {
        VkResult result;

        {
            KAISOU_PROFILE_SCOPE("Wait Device idle");
            result = vkDeviceWaitIdle(device_.Handle());
        }

        if (result != VK_SUCCESS)
            return result;

        {
            KAISOU_PROFILE_SCOPE("Wait pending Presents");

            if (activeGeneration_)
            {
                result = activeGeneration_->resources.WaitForPresents();
                if (result != VK_SUCCESS)
                    return result;
            }

            for (auto &generation : retiredGenerations_)
            {
                if (!generation)
                    continue;

                result = generation->resources.WaitForPresents();
                if (result != VK_SUCCESS)
                    return result;
            }
        }

        return VK_SUCCESS;
    }
    void PresentationContext::CollectRetiredGenerations()
    {
        KAISOU_PROFILE_SCOPE("Collect retired generations");

        std::size_t pendingCount = 0;

        for (auto &generation : retiredGenerations_)
        {
            if (!generation)
                continue;

            const VkResult result = generation->resources.QueryCompletion();

            if (result == VK_NOT_READY)
            {
                ++pendingCount;
                continue;
            }

            RequireSuccess(result, "Query retired generation completion");
#if KAISOU_ENABLE_PROFILING
            for (core::u32 frameSlot = 0; frameSlot < generation->resources.FrameSlotCount(); ++frameSlot)
                RequireSuccess(generation->gpuProfiler.Collect(frameSlot), "Collect retired GPU timestamps");
#endif
            generation.reset();
        }

        if (pendingCount < retiredGenerations_.size())
            retirementPressureReported_ = false;

        KAISOU_PROFILE_VALUE("Retired Swapchain generations", pendingCount);
    }

    bool PresentationContext::RebuildSwapchain(const platform::Window &window)
    {
        KAISOU_PROFILE_SCOPE("Rebuild Swapchain");

        VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE;

        if (activeGeneration_)
        {
            auto freeSlot = retiredGenerations_.end();

            for (auto slot = retiredGenerations_.begin(); slot != retiredGenerations_.end(); ++slot)
            {
                if (!*slot)
                {
                    freeSlot = slot;
                    break;
                }
            }

            if (freeSlot == retiredGenerations_.end())
            {
                if (!retirementPressureReported_)
                {
                    retirementPressureReported_ = true;
                    KAISOU_LOG_WARN("Renderer", "Retired generation limit reached; postponing Swapchain creation");
                }

                return false;
            }

            // 先保存旧代所有权，确保新代创建抛出时仍能完整清理旧代
            *freeSlot = std::move(activeGeneration_);
            oldSwapchain = (*freeSlot)->swapchain.Handle();
        }

        std::size_t retiredCount = 0;
        for (const auto &generation : retiredGenerations_)
            if (generation)
                ++retiredCount;

        if (retiredCount > retiredPeak_)
            retiredPeak_ = retiredCount;

        {
            KAISOU_PROFILE_SCOPE("Create Swapchain generation");

            activeGeneration_ = std::make_unique<SwapchainGeneration>(
                device_, surface_, window, swapchainConfig_, framesInFlight_, oldSwapchain);
        }

#if KAISOU_ENABLE_PROFILING
        EnsureGpuTraceContext(*activeGeneration_);
#endif
        ++swapchainCreations_;
#if KAISOU_ENABLE_GPU_DEBUG
        const VkResult nameResult = activeGeneration_->swapchain.SetDebugNames(device_, swapchainCreations_);
        if (nameResult != VK_SUCCESS)
            KAISOU_LOG_WARN(
                "Vulkan", "Swapchain generation {} naming failed with VkResult {}",
                swapchainCreations_, static_cast<int>(nameResult));
        const VkResult frameNameResult = activeGeneration_->resources.SetDebugNames(device_, swapchainCreations_);
        if (frameNameResult != VK_SUCCESS)
            KAISOU_LOG_WARN(
                "Vulkan", "Frame resources generation {} naming failed with VkResult {}",
                swapchainCreations_, static_cast<int>(frameNameResult));
#endif

        rebuildRequired_ = false;

        const auto &generation = *activeGeneration_;
        const VkExtent2D extent = generation.swapchain.Extent();

        KAISOU_LOG_INFO(
            "Renderer",
            "Swapchain generation {}: {} x {}, {} images, {} frame slots, retired {}, peak {}",
            swapchainCreations_, extent.width, extent.height,
            generation.swapchain.Images().size(), generation.resources.FrameSlotCount(),
            retiredCount, retiredPeak_);

        return true;
    }

    void PresentationContext::RequestPresentPolicy(const PresentPolicy policy) noexcept
    {
        if (swapchainConfig_.presentPolicy == policy)
            return;

        swapchainConfig_.presentPolicy = policy;
        rebuildRequired_ = true;
    }
    PresentFrameResult PresentationContext::RequestRebuild()
    {
        if (!rebuildRequired_)
            KAISOU_LOG_INFO("Renderer", "Swapchain rebuild requested");

        rebuildRequired_ = true;
        return PresentFrameResult::NeedsRebuild;
    }
    PresentFrameResult PresentationContext::Render(
        const platform::Window &window, RecordFunction &record, void *context)
    {
        KAISOU_PROFILE_FUNCTION();

        CollectRetiredGenerations();

        const auto &state = window.State();
        const bool drawable = window.IsDrawable();
        const bool pixelSizeChanged =
            state.pixelWidth != observedPixelWidth_ || state.pixelHeight != observedPixelHeight_;

        observedPixelWidth_ = state.pixelWidth;
        observedPixelHeight_ = state.pixelHeight;

        if (!drawable)
        {
            wasDrawable_ = false;
            rebuildRequired_ = true;
            return PresentFrameResult::Skipped;
        }

        if (!wasDrawable_ || pixelSizeChanged || !activeGeneration_ || activeGeneration_->acquireInvalid)
            (void)RequestRebuild();

        wasDrawable_ = true;

        if (rebuildRequired_ && !RebuildSwapchain(window))
        {
            if (!activeGeneration_ || activeGeneration_->acquireInvalid)
                return PresentFrameResult::Skipped;
        }

        auto &generation = *activeGeneration_;
        auto &swapchain = generation.swapchain;
        auto &resources = generation.resources;
        auto &slot = resources.Slot(generation.frameSlot);

        {
            KAISOU_PROFILE_SCOPE("Query frame readiness");

            VkResult result = vkGetFenceStatus(device_.Handle(), slot.submitFence);
            if (result == VK_NOT_READY)
                return PresentFrameResult::Skipped;

            RequireSuccess(result, "Query submit completion");
#if KAISOU_ENABLE_PROFILING
            RequireSuccess(generation.gpuProfiler.Collect(generation.frameSlot), "Collect GPU frame timestamps");
#endif
            result = resources.QueryPresentCompletion();
            if (result == VK_NOT_READY)
                return PresentFrameResult::Skipped;

            RequireSuccess(result, "Query Present completion");
        }

        std::uint32_t imageIndex = 0;
        VkResult acquireResult;

        {
            KAISOU_PROFILE_SCOPE("Acquire Swapchain image");
            // 暂时没有图像就返回，不阻塞 SDL 重绘回调。
            acquireResult = vkAcquireNextImageKHR(
                device_.Handle(),
                swapchain.Handle(),
                0,
                slot.acquireSemaphore,
                VK_NULL_HANDLE,
                &imageIndex);
        }
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            generation.acquireInvalid = true;
            return RequestRebuild();
        }

        if (acquireResult == VK_TIMEOUT || acquireResult == VK_NOT_READY)
            return PresentFrameResult::Skipped;

        // SUBOPTIMAL 仍然获取到了图像，必须完成其同步与呈现流程。
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
            RequireSuccess(acquireResult, "vkAcquireNextImageKHR");

        KAISOU_VERIFY(imageIndex < swapchain.Images().size(), "Acquired Swapchain image index out of range");
        {
            KAISOU_PROFILE_SCOPE("Prepare Present fence");
            RequireSuccess(resources.PreparePresent(imageIndex), "PreparePresent");
        }
        {
            KAISOU_PROFILE_SCOPE("Record presentation frame");

            // 此槽位的上一轮提交已经完成，可以整体重置命令池。
            RequireSuccess(vkResetCommandPool(device_.Handle(), slot.commandPool, 0), "vkResetCommandPool");
            VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            RequireSuccess(vkBeginCommandBuffer(slot.commandBuffer, &beginInfo), "vkBeginCommandBuffer");
#if KAISOU_ENABLE_PROFILING
            const TracyVkCtx traceContext = gpuTraceContext_ ? gpuTraceContext_->Handle() : nullptr;
            // 读取已完成的旧查询，并把查询重置录制到本次实际提交的命令中
            if (gpuTraceContext_)
                gpuTraceContext_->Collect(slot.commandBuffer);

            generation.gpuProfiler.BeginFrame(slot.commandBuffer, generation.frameSlot);
#endif
            {
#if KAISOU_ENABLE_PROFILING
                TracyVkNamedZone(traceContext, frameZone, slot.commandBuffer, "Frame", traceContext != nullptr);
#endif
#if KAISOU_ENABLE_GPU_DEBUG
                const DebugLabel frameLabel(device_, slot.commandBuffer, "Frame");
#endif

                const PresentationFrame frame{
                    slot.commandBuffer,
                    swapchain.Images()[imageIndex],
                    swapchain.ImageViews()[imageIndex],
                    swapchain.Extent(),
                    swapchain.ImageFormat(),
#if KAISOU_ENABLE_PROFILING
                    traceContext,
#endif
                };

                record(context, frame);
            }

#if KAISOU_ENABLE_PROFILING
            generation.gpuProfiler.EndFrame(slot.commandBuffer, generation.frameSlot);
#endif
            RequireSuccess(vkEndCommandBuffer(slot.commandBuffer), "vkEndCommandBuffer");
        }

        const VkSemaphore renderFinished = resources.RenderFinishedSemaphore(imageIndex);

        VkSemaphoreSubmitInfo acquireWait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        acquireWait.semaphore = slot.acquireSemaphore;
        acquireWait.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        VkCommandBufferSubmitInfo commandSubmit{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
        commandSubmit.commandBuffer = slot.commandBuffer;

        VkSemaphoreSubmitInfo renderSignal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        renderSignal.semaphore = renderFinished;
        renderSignal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        VkSubmitInfo2 submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &acquireWait;
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandSubmit;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &renderSignal;

        {
            KAISOU_PROFILE_SCOPE("Submit presentation frame");

            // 已成功获取并录制，确定要提交后才重置 Fence。
            RequireSuccess(vkResetFences(device_.Handle(), 1, &slot.submitFence), "vkResetFences");

            // 提交失败直接终止，不能继续等待已被重置却无人发出信号的 Fence。
            // TODO(M3-RGE-08): Replace the single Graphics submission with Queue Scheduler submission batches.
            RequireSuccess(
                vkQueueSubmit2(device_.GraphicsQueue().handle, 1, &submitInfo, slot.submitFence),
                "vkQueueSubmit2");
#if KAISOU_ENABLE_PROFILING
            generation.gpuProfiler.MarkSubmitted(generation.frameSlot);
#endif
            if (enableFrameStatistics_)
                ++statistics_.submissions;
        }

        const VkSwapchainKHR swapchainHandle = swapchain.Handle();
        const VkFence presentFence = resources.PresentFence(imageIndex);

        VkSwapchainPresentFenceInfoKHR presentFenceInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR};
        presentFenceInfo.swapchainCount = 1;
        presentFenceInfo.pFences = &presentFence;

        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.pNext = &presentFenceInfo;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinished;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchainHandle;
        presentInfo.pImageIndices = &imageIndex;

        VkResult presentResult;

        {
            KAISOU_PROFILE_SCOPE("Present Swapchain image");
            presentResult = vkQueuePresentKHR(device_.PresentQueue().handle, &presentInfo);
            resources.RecordPresentResult(imageIndex, presentResult);
        }

        // 只有真正提交的帧才推进槽位索引。
        generation.frameSlot =
            (generation.frameSlot + 1) % static_cast<std::uint32_t>(resources.FrameSlotCount());
        if (enableFrameStatistics_)
            RecordPresentation(presentResult);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            generation.acquireInvalid = true;
            return RequestRebuild();
        }

        if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR)
            RequireSuccess(presentResult, "vkQueuePresentKHR");

        if (!firstPresented_)
        {
            KAISOU_LOG_INFO("Renderer", "First frame submitted and presentation accepted, image {}", imageIndex);
            firstPresented_ = true;
        }

        if (acquireResult == VK_SUBOPTIMAL_KHR || presentResult == VK_SUBOPTIMAL_KHR)
            return RequestRebuild();

        return PresentFrameResult::Presented;
    }
    void PresentationContext::RecordPresentation(const VkResult result)
    {
        const auto now = core::Clock::now();
        auto &stats = statistics_;

        if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
        {
            ++stats.acceptedPresents;

            if (stats.lastAcceptedPresent)
            {
                const double intervalMs = core::Milliseconds(now - *stats.lastAcceptedPresent).count();

                ++stats.intervalSamples;
                stats.intervalSumMs += intervalMs;

                if (intervalMs > stats.maxIntervalMs)
                    stats.maxIntervalMs = intervalMs;

                KAISOU_PROFILE_VALUE("Present accepted interval (ms)", intervalMs);
            }

            stats.lastAcceptedPresent = now;
        }

        const double reportSeconds = core::Seconds(now - stats.reportStart).count();
        if (reportSeconds < 1.0)
            return;

        const double averageMs =
            stats.intervalSamples > 0 ? stats.intervalSumMs / static_cast<double>(stats.intervalSamples) : 0.0;

        KAISOU_LOG_INFO(
            "Renderer",
            "Frame stats: window {:.2f} s, submitted {}, accepted {}, accepted FPS {:.1f}, "
            "interval samples {}, avg {:.2f} ms, max {:.2f} ms",
            reportSeconds,
            stats.submissions,
            stats.acceptedPresents,
            static_cast<double>(stats.acceptedPresents) / reportSeconds,
            stats.intervalSamples,
            averageMs,
            stats.maxIntervalMs);

        stats.reportStart = now;
        stats.submissions = 0;
        stats.acceptedPresents = 0;
        stats.intervalSamples = 0;
        stats.intervalSumMs = 0.0;
        stats.maxIntervalMs = 0.0;
    }
}
