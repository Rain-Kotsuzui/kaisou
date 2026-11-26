#ifndef KAI_SAMPLES_M1_TRIANGLE_PIPELINE_HPP
#define KAI_SAMPLES_M1_TRIANGLE_PIPELINE_HPP

#include "kaisou/rhi/vulkan/vulkan.hpp"

#include <filesystem>

namespace kaisou::rhi::vulkan
{
    class Device;
}

namespace kaisou::samples::m1
{
    // M1 归档示例固定绘制状态；正式 Graphics Pipeline Description 在 M2-PIPE-01 独立实现。
    class TrianglePipeline final
    {
    public:
        TrianglePipeline(
            const rhi::vulkan::Device &device, VkFormat colorFormat,
            const std::filesystem::path &shaderDirectory);
        ~TrianglePipeline() noexcept;

        TrianglePipeline(const TrianglePipeline &) = delete;
        TrianglePipeline &operator=(const TrianglePipeline &) = delete;
        TrianglePipeline(TrianglePipeline &&) = delete;
        TrianglePipeline &operator=(TrianglePipeline &&) = delete;

        [[nodiscard]] VkPipeline Handle() const noexcept { return pipeline_; }
        [[nodiscard]] VkFormat ColorFormat() const noexcept { return colorFormat_; }

    private:
        void Destroy() noexcept;

        // 借用 Device；它必须比本对象活得更久。
        VkDevice device_ = VK_NULL_HANDLE;

        // 本对象拥有这两个 Vulkan 资源。
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkFormat colorFormat_ = VK_FORMAT_UNDEFINED;
    };
}

#endif
