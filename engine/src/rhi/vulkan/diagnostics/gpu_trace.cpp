#include "kaisou/rhi/vulkan/diagnostics/gpu_trace.hpp"

#if KAISOU_ENABLE_PROFILING

#include "kaisou/core/assert.hpp"
#include "kaisou/rhi/vulkan/device.hpp"

namespace kaisou::rhi::vulkan
{
    GpuTraceContext::GpuTraceContext(const Device &device)
        : device_(device.Handle())
    {
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        // Tracy 初始化会反复 Begin 同一个 Command Buffer
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = device.GraphicsQueue().familyIndex;

        KAISOU_VERIFY(vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) == VK_SUCCESS,
                      "Create Tracy initialization command pool failed");

        VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocateInfo.commandPool = commandPool_;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;

        KAISOU_VERIFY(vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer_) == VK_SUCCESS,
                      "Allocate Tracy initialization command buffer failed");

        // Tracy 内部录制、提交并等待该队列；只在首次初始化执行
        context_ = TracyVkContext(device.PhysicalHandle(), device_, device.GraphicsQueue().handle, commandBuffer_);

        constexpr char contextName[] = "Graphics";
        TracyVkContextName(context_, contextName, sizeof(contextName) - 1);
    }

    GpuTraceContext::~GpuTraceContext() noexcept
    {
        // 必须先完成 GPU 使用；析构自身不额外等待设备。
        TracyVkDestroy(context_);
        vkDestroyCommandPool(device_, commandPool_, nullptr);
    }

    void GpuTraceContext::Collect(const VkCommandBuffer commandBuffer) const noexcept
    {
        TracyVkCollect(context_, commandBuffer);
    }

    void GpuTraceContext::FinishCollection() const noexcept
    {
        KAISOU_VERIFY(vkResetCommandPool(device_, commandPool_, 0) == VK_SUCCESS,
                      "Reset Tracy shutdown command pool failed");

        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        KAISOU_VERIFY(vkBeginCommandBuffer(commandBuffer_, &beginInfo) == VK_SUCCESS,
                      "Begin Tracy shutdown command buffer failed");

        // 单次 Collect 只处理查询环中的一段连续范围；两次覆盖可能的环绕。
        TracyVkCollect(context_, commandBuffer_);
        TracyVkCollect(context_, commandBuffer_);

        KAISOU_VERIFY(vkEndCommandBuffer(commandBuffer_) == VK_SUCCESS,
                      "End Tracy shutdown command buffer failed");
    }
}

#endif
