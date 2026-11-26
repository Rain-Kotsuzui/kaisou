#ifndef KAI_SAMPLES_M1_CLEAR_RENDERER_HPP
#define KAI_SAMPLES_M1_CLEAR_RENDERER_HPP

#include "kaisou/rhi/vulkan/instance.hpp"
#include "kaisou/rhi/vulkan/presentation/window_surface.hpp"
#include "kaisou/rhi/vulkan/device.hpp"
#include "kaisou/rhi/vulkan/presentation/presentation_context.hpp"
#include "kaisou/core/time.hpp"
#include "kaisou/samples/m1/triangle_pipeline.hpp"

#include <filesystem>
#include <memory>
#include <vector>
#include <array>
#include <cstdint>

namespace kaisou::platform
{
    class Window;
}

namespace kaisou::samples::m1
{
    // 临时演示的配置容器；下列后端配置可迁入正式 Renderer，清屏测试配置随演示替换。
    struct ClearRendererConfig
    {
        // 【可复用】实例、设备、交换链配置及在途帧数，不依赖清屏逻辑。
        rhi::vulkan::InstanceConfig instance;
        rhi::vulkan::DeviceConfig device;
        rhi::vulkan::SwapchainConfig swapchain;

        std::uint32_t framesInFlight = 2;
        // 【临时测试】固定清屏颜色与拖动期间观察出帧的颜色动画。
        std::array<float, 4> clearColor{0.50f, 0.00f, 0.0f, 1.0f};
        bool animateClearColor = false;
        // 【可复用诊断】控制 PresentationContext 的呈现统计；默认关闭，正式 Renderer 可继续使用。
        bool enableFrameStatistics = false;

        // M1 演示默认关闭，由用户手动开启。
        bool showTriangle = false;

        // 相对路径基于进程工作目录；也可由应用传入绝对路径。
        std::filesystem::path shaderDirectory{"assets/shaders/m1"};
    };

    // 【临时兼容名称】实际结果类型属于后端，正式 Renderer 可直接使用 PresentFrameResult。
    using ClearFrameResult = rhi::vulkan::PresentFrameResult;

    // 本类整体是临时清屏演示；可复用的实现位于 rhi/vulkan，下面仅组合和调用这些对象。
    // M1 归档示例；正式 Render Graph 在 M3-RGE-03 独立实现。
    class ClearRenderer final
    {
    public:
        explicit ClearRenderer(const platform::Window &window, const ClearRendererConfig &config = {});
        ~ClearRenderer() noexcept = default;

        ClearRenderer(const ClearRenderer &) = delete;
        ClearRenderer &operator=(const ClearRenderer &) = delete;
        ClearRenderer(ClearRenderer &&) = delete;
        ClearRenderer &operator=(ClearRenderer &&) = delete;

        // 呈现流程可复用；RecordFrame 和 this 上下文属于本示例。
        [[nodiscard]] ClearFrameResult Render(const platform::Window &window)
        {
            return presentation_.Render(window, RecordFrame, this);
        }

        void SetTriangleEnabled(bool enabled) noexcept { showTriangle_ = enabled; }
        [[nodiscard]] bool TriangleEnabled() const noexcept { return showTriangle_; }

        // 【可复用调用方式】转发显示策略请求，实际状态管理位于 PresentationContext。
        void RequestPresentPolicy(rhi::vulkan::PresentPolicy policy) noexcept
        {
            presentation_.RequestPresentPolicy(policy);
        }

    private:
        // 【临时测试】清屏专用录制回调；固定附件、Barrier 和颜色动画均不作为通用渲染实现保留。
        // 借用呈现帧，录制清屏和可选三角形，不自行提交。
        static void RecordFrame(void *context, const rhi::vulkan::PresentationFrame &frame) noexcept;
        [[nodiscard]] const TrianglePipeline &EnsureTrianglePipeline(VkFormat format) noexcept;

        // 【可复用后端组装】这些类型及依赖顺序可由正式 Renderer/设备拥有者沿用。
        // 按逆序析构：PresentationContext 等待并释放资源，然后才销毁 Device、Surface、Instance。
        rhi::vulkan::Instance instance_;
        rhi::vulkan::WindowSurface surface_;
        rhi::vulkan::Device device_;

        std::filesystem::path shaderDirectory_;

        // 保留实际出现过的格式对应的管线，避免提前销毁 GPU 仍在使用的旧管线。
        // M1 示例保留同步查找；正式共享 Pipeline Cache 在 M2-PIPE-05 独立实现。
        std::vector<std::unique_ptr<TrianglePipeline>> trianglePipelines_;

        // 必须声明在管线拥有者之后：逆序析构时，先等待 GPU，再释放管线。
        rhi::vulkan::PresentationContext presentation_;

        std::array<float, 4> clearColor_;
        bool animateClearColor_ = false;
        bool showTriangle_ = false;
        core::TimePoint animationStart_{};
    };
}

#endif
