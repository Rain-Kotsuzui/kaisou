#ifndef KAI_VULKAN_GPU_PROFILER_HPP
#define KAI_VULKAN_GPU_PROFILER_HPP

#include "kaisou/core/types.hpp"
#include "kaisou/rhi/vulkan/vulkan.hpp"

#include <vector>

namespace kaisou::rhi::vulkan
{
    class Device;

    class GpuProfiler final
    {
    public:
        GpuProfiler(const Device &device, core::u32 framesInFlight);
        ~GpuProfiler();

        GpuProfiler(const GpuProfiler &) = delete;
        GpuProfiler &operator=(const GpuProfiler &) = delete;
        GpuProfiler(GpuProfiler &&) = delete;
        GpuProfiler &operator=(GpuProfiler &&) = delete;

        [[nodiscard]] bool IsEnabled() const noexcept { return queryPool_ != VK_NULL_HANDLE; }

        [[nodiscard]] VkResult Collect(core::u32 frameSlot);
        void BeginFrame(VkCommandBuffer commandBuffer, core::u32 frameSlot) const noexcept;
        void EndFrame(VkCommandBuffer commandBuffer, core::u32 frameSlot) const noexcept;
        void MarkSubmitted(core::u32 frameSlot) noexcept;

    private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkQueryPool queryPool_ = VK_NULL_HANDLE;
        core::u64 timestampMask_ = 0;
        double timestampPeriod_ = 0.0;
        std::vector<core::u8> pending_;
        core::u32 collectedSamples_ = 0;
        double durationSumMs_ = 0.0;
        double maxDurationMs_ = 0.0;
    };
}

#endif
