#include "kaisou/samples/m1/clear_renderer.hpp"

#include "kaisou/core/log.hpp"
#include "kaisou/rhi/vulkan/diagnostics/debug.hpp"
#include "kaisou/core/assert.hpp"
#include "kaisou/core/profile.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <cmath>

namespace kaisou::samples::m1
{
    // 【可复用组装】Instance → Surface → Device → PresentationContext 的依赖顺序。
    // 【临时测试】构造 ClearRenderer 并保存颜色/动画状态；正式 Renderer 可沿用后端组装，替换演示状态。
    ClearRenderer::ClearRenderer(const platform::Window &window, const ClearRendererConfig &config)
        : instance_(config.instance),
          surface_(instance_, window),
          device_(instance_, surface_, config.device),
          shaderDirectory_(config.shaderDirectory),
          presentation_(device_, surface_, window,
                        {config.swapchain, config.framesInFlight, config.enableFrameStatistics}),
          clearColor_(config.clearColor),
          animateClearColor_(config.animateClearColor),
          showTriangle_(config.showTriangle),
          animationStart_(core::Clock::now())
    {
        // 【临时测试日志】描述清屏演示的配置；正式 Renderer 使用自己的初始化日志。
        KAISOU_LOG_INFO("Renderer", "Clear diagnostics: animation {}, frame statistics {}",
                        animateClearColor_, config.enableFrameStatistics);
        KAISOU_LOG_INFO("Renderer", "Clear renderer initialized, validation {}, color ({}, {}, {}, {})",
                        instance_.ValidationEnabled(), clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3]);
    }
    const TrianglePipeline &ClearRenderer::EnsureTrianglePipeline(const VkFormat format) noexcept
    {
        for (const auto &pipeline : trianglePipelines_)
        {
            if (pipeline->ColorFormat() == format)
                return *pipeline;
        }

        // M1 示例保留同步创建；正式异步 Pipeline 系统在 M2-PIPE-07 独立实现。
        // 当前只在首次开启、或首次遇到新附件格式时同步创建。
        try
        {
            KAISOU_PROFILE_SCOPE("Create triangle pipeline");
            auto pipeline = std::make_unique<TrianglePipeline>(device_, format, shaderDirectory_);
            trianglePipelines_.push_back(std::move(pipeline));
            return *trianglePipelines_.back();
        }
        catch (const std::exception &error)
        {
            std::fprintf(stderr, "Triangle pipeline initialization failed: %s\n", error.what());
            KAISOU_FATAL("Unable to initialize the triangle pipeline");
        }
        catch (...)
        {
            KAISOU_FATAL("Unknown failure while initializing the triangle pipeline");
        }
    }
    // 【临时测试实现】本函数整体随清屏演示替换。
    // 可复用的是后端提供的录制回调契约：借用 frame，只记录命令，最终将图像转换到呈现布局。
    void ClearRenderer::RecordFrame(void *context, const rhi::vulkan::PresentationFrame &frame) noexcept
    {
        auto &renderer = *static_cast<ClearRenderer *>(context);

        const TrianglePipeline *triangle =
            renderer.showTriangle_ ? &renderer.EnsureTrianglePipeline(frame.format) : nullptr;

        // 【临时固定 Barrier】从 UNDEFINED 丢弃内容转为颜色附件，适用于本清屏示例。
        // 正式 Render Graph 按真实资源访问推导 Stage/Access/Layout，不能原样套用此固定状态。
        VkImageMemoryBarrier2 toAttachment{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        toAttachment.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        toAttachment.srcAccessMask = VK_ACCESS_2_NONE;
        toAttachment.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        toAttachment.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        // 清屏不读取上一帧内容，允许每次从 UNDEFINED 转换并丢弃旧内容。
        toAttachment.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toAttachment.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toAttachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toAttachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toAttachment.image = frame.image;
        toAttachment.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toAttachment.subresourceRange.levelCount = 1;
        toAttachment.subresourceRange.layerCount = 1;

        VkDependencyInfo attachmentDependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        attachmentDependency.imageMemoryBarrierCount = 1;
        attachmentDependency.pImageMemoryBarriers = &toAttachment;
        vkCmdPipelineBarrier2(frame.commandBuffer, &attachmentDependency);

        // 【临时固定附件】单颜色附件、CLEAR/STORE；动态渲染 API 可复用，附件组织由正式执行器决定。
        VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        colorAttachment.imageView = frame.imageView;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        // 【临时测试颜色】固定颜色与四秒动画用于观察出帧，不属于正式场景/材质逻辑。
        auto color = renderer.clearColor_;
        if (renderer.animateClearColor_)
        {
            const double elapsed = core::Seconds(core::Clock::now() - renderer.animationStart_).count();
            const double phase = std::fmod(elapsed, 4.0);
            const float blend = static_cast<float>(phase < 2.0 ? phase * 0.5 : (4.0 - phase) * 0.5);
            // 诊断动画：四秒完成一次红 → 蓝 → 红，保留配置的 Alpha。
            color[0] = 0.5f * (1.0f - blend);
            color[1] = 0.0f;
            color[2] = 0.5f * blend;
        }
        for (std::size_t index = 0; index < color.size(); ++index)
            colorAttachment.clearValue.color.float32[index] = color[index];

        VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
        renderingInfo.renderArea.extent = frame.extent;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        // M1 示例保留固定附件；正式动态渲染执行流程在 M3-RGE-04 独立实现。
        {
#if KAISOU_ENABLE_PROFILING
            TracyVkNamedZone(frame.gpuTraceContext, clearZone, frame.commandBuffer, "Clear",
                             frame.gpuTraceContext != nullptr);
#endif
#if KAISOU_ENABLE_GPU_DEBUG
            const rhi::vulkan::DebugLabel clearLabel(renderer.device_, frame.commandBuffer, "Clear");
#endif
            vkCmdBeginRendering(frame.commandBuffer, &renderingInfo);
        }

        if (triangle != nullptr)
        {
#if KAISOU_ENABLE_PROFILING
            TracyVkNamedZone(frame.gpuTraceContext, triangleZone, frame.commandBuffer, "Triangle",
                             frame.gpuTraceContext != nullptr);
#endif
#if KAISOU_ENABLE_GPU_DEBUG
            const rhi::vulkan::DebugLabel triangleLabel(renderer.device_, frame.commandBuffer, "Triangle");
#endif
            // 使用居中的正方形视口，避免窗口宽高比改变时拉伸三角形。
            const float width = static_cast<float>(frame.extent.width);
            const float height = static_cast<float>(frame.extent.height);
            const float side = std::min(width, height);

            VkViewport viewport{};
            viewport.x = (width - side) * 0.5f;
            viewport.y = (height - side) * 0.5f;
            viewport.width = side;
            viewport.height = side;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor{};
            scissor.extent = frame.extent;

            vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, triangle->Handle());
            vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);
            vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);
        }

        vkCmdEndRendering(frame.commandBuffer);

        // 【临时固定 Barrier】这里假设最后一次访问是颜色附件写入。
        // 【可复用约束】交给 Present 前必须处于 PRESENT_SRC_KHR；正式执行器按实际末次访问生成转换。
        VkImageMemoryBarrier2 toPresent{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        toPresent.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        toPresent.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        toPresent.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
        toPresent.dstAccessMask = VK_ACCESS_2_NONE;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.image = frame.image;
        toPresent.subresourceRange = toAttachment.subresourceRange;

        VkDependencyInfo presentDependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        presentDependency.imageMemoryBarrierCount = 1;
        presentDependency.pImageMemoryBarriers = &toPresent;
        vkCmdPipelineBarrier2(frame.commandBuffer, &presentDependency);
    }
}
