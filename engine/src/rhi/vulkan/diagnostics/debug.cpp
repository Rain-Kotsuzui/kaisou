#include "kaisou/rhi/vulkan/diagnostics/debug.hpp"
#include "kaisou/rhi/vulkan/device.hpp"

namespace kaisou::rhi::vulkan
{
    VkResult SetDebugName(
        const Device &device, const VkObjectType objectType,
        const std::uint64_t objectHandle, const char *name) noexcept
    {
        if (!KAISOU_ENABLE_GPU_DEBUG || !device.DebugUtilsEnabled() || !vkSetDebugUtilsObjectNameEXT)
            return VK_SUCCESS;

        VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        info.objectType = objectType;
        info.objectHandle = objectHandle;
        info.pObjectName = name;

        return vkSetDebugUtilsObjectNameEXT(device.Handle(), &info);
    }

    DebugLabel::DebugLabel(
        const Device &device, const VkCommandBuffer commandBuffer, const char *name) noexcept
        : commandBuffer_(commandBuffer),
          active_(KAISOU_ENABLE_GPU_DEBUG && device.DebugUtilsEnabled() &&
                  vkCmdBeginDebugUtilsLabelEXT && vkCmdEndDebugUtilsLabelEXT)
    {
        if (!active_)
            return;

        VkDebugUtilsLabelEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
        info.pLabelName = name;

        vkCmdBeginDebugUtilsLabelEXT(commandBuffer_, &info);
    }

    DebugLabel::~DebugLabel() noexcept
    {
        if (active_)
            vkCmdEndDebugUtilsLabelEXT(commandBuffer_);
    }
}
