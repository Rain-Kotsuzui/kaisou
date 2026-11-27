#include "kaisou/rhi/vulkan/diagnostics/gpu_profiler.hpp"
#include "kaisou/rhi/vulkan/device.hpp"
#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"
#include "kaisou/core/profile.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string>

namespace kaisou::rhi::vulkan
{
    GpuProfiler::GpuProfiler(const Device &device, const core::u32 framesInFlight)
        : device_(device.Handle())
    {
        KAISOU_VERIFY(
            framesInFlight > 0 && framesInFlight <= std::numeric_limits<core::u32>::max() / 2,
            "GPU profiler requires a nonzero frame slot count with two queries per slot");

        const auto &graphicsQueue = device.GraphicsQueue();
        if (!graphicsQueue.IsValid())
        {
            KAISOU_LOG_WARN("Vulkan", "Graphics Queue unavailable; GPU frame timing disabled");
            return;
        }

        const auto &capabilities = device.Capabilities();
        const auto &families = capabilities.queueFamilies;
        const auto family =
            std::ranges::find(families, graphicsQueue.familyIndex, &QueueFamilyCapabilities::familyIndex);

        if (family == families.end())
            KAISOU_FATAL("Graphics Queue family is missing from selected device capabilities");

        const core::u32 validBits = family->properties.timestampValidBits;
        timestampPeriod_ = static_cast<double>(capabilities.properties.limits.timestampPeriod);
        if (validBits == 0)
        {
            KAISOU_LOG_WARN("Vulkan", "Graphics Queue timestamps unavailable; GPU frame timing disabled");
            return;
        }

        timestampMask_ = validBits == 64
                             ? std::numeric_limits<core::u64>::max()
                             : (core::u64{1} << validBits) - 1;
        pending_.resize(framesInFlight, 0);

        // 分配和日志放在获取裸 Vulkan 资源之前，避免构造异常留下查询池。
        KAISOU_LOG_INFO(
            "Vulkan", "GPU frame timestamps: valid bits {}, period {:.6f} ns/tick, frame slots {}",
            validBits, timestampPeriod_, framesInFlight);

        VkQueryPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        createInfo.queryCount = framesInFlight * 2;

        const VkResult result = vkCreateQueryPool(device_, &createInfo, nullptr, &queryPool_);
        if (result != VK_SUCCESS)
        {
            const std::string message =
                "vkCreateQueryPool failed with VkResult " + std::to_string(static_cast<int>(result));
            KAISOU_FATAL(message);
        }
    }

    GpuProfiler::~GpuProfiler()
    {
        if (queryPool_ != VK_NULL_HANDLE)
            vkDestroyQueryPool(device_, queryPool_, nullptr);
    }

    void GpuProfiler::BeginFrame(const VkCommandBuffer commandBuffer, const core::u32 frameSlot) const noexcept
    {
        if (!IsEnabled())
            return;

        KAISOU_ASSERT(pending_[frameSlot] == 0, "Collect previous GPU timestamps before reusing this frame slot");

        const core::u32 firstQuery = frameSlot * 2;
        vkCmdResetQueryPool(commandBuffer, queryPool_, firstQuery, 2);
        vkCmdWriteTimestamp2(commandBuffer, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, queryPool_, firstQuery);
    }

    void GpuProfiler::EndFrame(const VkCommandBuffer commandBuffer, const core::u32 frameSlot) const noexcept
    {
        if (!IsEnabled())
            return;

        vkCmdWriteTimestamp2(commandBuffer, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, queryPool_, frameSlot * 2 + 1);
    }

    void GpuProfiler::MarkSubmitted(const core::u32 frameSlot) noexcept
    {
        if (IsEnabled())
            pending_[frameSlot] = 1;
    }

    VkResult GpuProfiler::Collect(const core::u32 frameSlot)
    {
        if (!IsEnabled() || pending_[frameSlot] == 0)
            return VK_SUCCESS;

        std::array<core::u64, 2> timestamps{};
        const VkResult result = vkGetQueryPoolResults(
            device_, queryPool_, frameSlot * 2, 2, sizeof(timestamps), timestamps.data(),
            sizeof(core::u64), VK_QUERY_RESULT_64_BIT);

        if (result != VK_SUCCESS)
            return result;

        const core::u64 elapsedTicks = (timestamps[1] - timestamps[0]) & timestampMask_;
        const double durationMs = static_cast<double>(elapsedTicks) * timestampPeriod_ / 1'000'000.0;
        pending_[frameSlot] = 0;

        KAISOU_PROFILE_VALUE("GPU Frame (ms)", durationMs);

        ++collectedSamples_;
        durationSumMs_ += durationMs;
        if (durationMs > maxDurationMs_)
            maxDurationMs_ = durationMs;

        if (collectedSamples_ == 120)
        {
            KAISOU_LOG_INFO(
                "Vulkan", "GPU frame timing: collected samples {}, avg {:.6f} ms, max {:.6f} ms",
                collectedSamples_, durationSumMs_ / static_cast<double>(collectedSamples_), maxDurationMs_);
            collectedSamples_ = 0;
            durationSumMs_ = 0.0;
            maxDurationMs_ = 0.0;
        }

        return VK_SUCCESS;
    }
}
