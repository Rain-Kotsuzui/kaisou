#ifndef KAI_VULKAN_GPU_TRACE_HPP
#define KAI_VULKAN_GPU_TRACE_HPP

#include "kaisou/rhi/vulkan/vulkan.hpp"

#if KAISOU_ENABLE_PROFILING

#include <tracy/TracyVulkan.hpp>

namespace kaisou::rhi::vulkan
{
    class Device;

    class GpuTraceContext final
    {
    public:
        explicit GpuTraceContext(const Device &device);
        ~GpuTraceContext() noexcept;

        GpuTraceContext(const GpuTraceContext &) = delete;
        GpuTraceContext &operator=(const GpuTraceContext &) = delete;
        GpuTraceContext(GpuTraceContext &&) = delete;
        GpuTraceContext &operator=(GpuTraceContext &&) = delete;

        [[nodiscard]] TracyVkCtx Handle() const noexcept { return context_; }

        void Collect(VkCommandBuffer commandBuffer) const noexcept;
        void FinishCollection() const noexcept;

    private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkCommandPool commandPool_ = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
        TracyVkCtx context_ = nullptr;
    };
}

#endif

#endif