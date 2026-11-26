#include "kaisou/samples/m1/triangle_pipeline.hpp"

#include "kaisou/core/assert.hpp"
#include "kaisou/core/filesystem.hpp"
#include "kaisou/core/types.hpp"
#include "kaisou/rhi/vulkan/device.hpp"
#include "kaisou/rhi/vulkan/diagnostics/debug.hpp"

#include <fmt/format.h>

#include <cstring>
#include <vector>

namespace kaisou::samples::m1
{
    namespace
    {
        std::vector<core::u32> ReadSpirv(const std::filesystem::path &path)
        {
            const auto binary = core::FileSystem::ReadBinary(path);
            if (!binary)
            {
                KAISOU_FATAL(fmt::format(
                    "Failed to read SPIR-V '{}', FileError {}",
                    path.string(), static_cast<int>(binary.error().error)));
            }

            // SPIR-V 包含五个头部 word，完整二进制大小必须是四字节的整数倍。
            constexpr std::size_t headerSize = 5 * sizeof(core::u32);
            if (binary->size() < headerSize || binary->size() % sizeof(core::u32) != 0)
                KAISOU_FATAL(fmt::format("Invalid SPIR-V byte size in '{}'", path.string()));

            // 使用 u32 存储保证 pCode 的对齐；直接转换 byte 指针不能提供这个保证。
            std::vector<core::u32> words(binary->size() / sizeof(core::u32));
            std::memcpy(words.data(), binary->data(), binary->size());

            constexpr core::u32 spirvMagic = 0x07230203u;
            if (words.front() != spirvMagic)
                KAISOU_FATAL(fmt::format("Invalid SPIR-V magic in '{}'", path.string()));

            return words;
        }
    }

    TrianglePipeline::TrianglePipeline(
        const rhi::vulkan::Device &device, const VkFormat colorFormat,
        const std::filesystem::path &shaderDirectory)
        : device_(device.Handle()), colorFormat_(colorFormat)
    {
        KAISOU_VERIFY(device_ != VK_NULL_HANDLE, "TrianglePipeline requires a valid Device");
        KAISOU_VERIFY(colorFormat_ != VK_FORMAT_UNDEFINED, "TrianglePipeline requires a color format");

        // 文件读取和 CPU 分配先完成，再开始创建 Vulkan 对象。
        const auto vertexCode = ReadSpirv(shaderDirectory / "triangle.vert.spv");
        const auto fragmentCode = ReadSpirv(shaderDirectory / "triangle.frag.spv");

        VkShaderModule vertexModule = VK_NULL_HANDLE;
        VkShaderModule fragmentModule = VK_NULL_HANDLE;

        const auto destroyModules = [&]() noexcept
        {
            if (fragmentModule != VK_NULL_HANDLE)
                vkDestroyShaderModule(device_, fragmentModule, nullptr);
            if (vertexModule != VK_NULL_HANDLE)
                vkDestroyShaderModule(device_, vertexModule, nullptr);

            fragmentModule = VK_NULL_HANDLE;
            vertexModule = VK_NULL_HANDLE;
        };

        const auto checkResult = [&](const VkResult result, const char *operation)
        {
            if (result == VK_SUCCESS)
                return;

            // Fatal 会终止进程，不进行正常栈展开，因此必须先释放已创建资源。
            destroyModules();
            Destroy();
            KAISOU_FATAL(fmt::format("{} failed with VkResult {}", operation, static_cast<int>(result)));
        };

        try
        {
            VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            moduleInfo.codeSize = vertexCode.size() * sizeof(core::u32);
            moduleInfo.pCode = vertexCode.data();
            checkResult(
                vkCreateShaderModule(device_, &moduleInfo, nullptr, &vertexModule),
                "vkCreateShaderModule(vertex)");

            moduleInfo.codeSize = fragmentCode.size() * sizeof(core::u32);
            moduleInfo.pCode = fragmentCode.data();
            checkResult(
                vkCreateShaderModule(device_, &moduleInfo, nullptr, &fragmentModule),
                "vkCreateShaderModule(fragment)");

            // Shader 没有 Descriptor 或 Push Constant，因此创建空布局。
            // TODO(M2-PIPE-03): Create Pipeline Layout from Shader Reflection metadata.
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            checkResult(
                vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &layout_),
                "vkCreatePipelineLayout");

            VkPipelineShaderStageCreateInfo stages[2]{};
            stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stages[0].module = vertexModule;
            stages[0].pName = "VSMain";

            stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[1].module = fragmentModule;
            stages[1].pName = "PSMain";

            // 顶点位置由 SV_VertexID 生成，没有 Vertex Buffer 输入。
            VkPipelineVertexInputStateCreateInfo vertexInput{
                VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

            VkPipelineInputAssemblyStateCreateInfo inputAssembly{
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            // 数量固定为一；具体值由后续 vkCmdSetViewport/vkCmdSetScissor 设置。
            VkPipelineViewportStateCreateInfo viewportState{
                VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;

            VkPipelineRasterizationStateCreateInfo rasterization{
                VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            rasterization.polygonMode = VK_POLYGON_MODE_FILL;
            rasterization.cullMode = VK_CULL_MODE_NONE;
            rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterization.lineWidth = 1.0f;

            VkPipelineMultisampleStateCreateInfo multisample{
                VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.blendEnable = VK_FALSE;
            colorBlendAttachment.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            VkPipelineColorBlendStateCreateInfo colorBlend{
                VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            colorBlend.attachmentCount = 1;
            colorBlend.pAttachments = &colorBlendAttachment;

            const VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamicState{
                VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            dynamicState.dynamicStateCount = 2;
            dynamicState.pDynamicStates = dynamicStates;

            // Dynamic Rendering 的管线仍须声明附件格式。
            VkPipelineRenderingCreateInfo renderingInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachmentFormats = &colorFormat_;

            VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            pipelineInfo.pNext = &renderingInfo;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = stages;
            pipelineInfo.pVertexInputState = &vertexInput;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterization;
            pipelineInfo.pMultisampleState = &multisample;
            pipelineInfo.pColorBlendState = &colorBlend;
            pipelineInfo.pDynamicState = &dynamicState;
            pipelineInfo.layout = layout_;
            pipelineInfo.renderPass = VK_NULL_HANDLE;

            // TODO(M2-PIPE-05): Reuse pipelines through the shared in-memory Pipeline Cache.
            checkResult(
                vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_),
                "vkCreateGraphicsPipelines");

#if KAISOU_ENABLE_GPU_DEBUG
            (void)rhi::vulkan::SetDebugName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, layout_, "Triangle.Layout");
            (void)rhi::vulkan::SetDebugName(device, VK_OBJECT_TYPE_PIPELINE, pipeline_, "Triangle.Pipeline");
#endif
        }
        catch (...)
        {
            // 构造函数抛出异常时，不会调用 TrianglePipeline 自身的析构函数。
            destroyModules();
            Destroy();
            throw;
        }

        // Pipeline 创建完成后，不再需要保留创建时使用的 Shader Modules。
        destroyModules();
    }

    TrianglePipeline::~TrianglePipeline() noexcept
    {
        // 调用方必须先确认使用此 Pipeline 的 GPU 工作已完成。
        Destroy();
    }

    void TrianglePipeline::Destroy() noexcept
    {
        if (pipeline_ != VK_NULL_HANDLE)
            vkDestroyPipeline(device_, pipeline_, nullptr);
        if (layout_ != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device_, layout_, nullptr);

        pipeline_ = VK_NULL_HANDLE;
        layout_ = VK_NULL_HANDLE;
    }
}
