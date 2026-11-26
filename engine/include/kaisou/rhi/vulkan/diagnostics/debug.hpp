#ifndef KAI_VULKAN_DEBUG_HPP
#define KAI_VULKAN_DEBUG_HPP

#include "kaisou/rhi/vulkan/vulkan.hpp"

#include <cstdint>
#include <type_traits>
namespace kaisou::rhi::vulkan
{
    class Device;

    [[nodiscard]] VkResult SetDebugName(
        const Device &device, VkObjectType objectType,
        std::uint64_t objectHandle, const char *name) noexcept;
    template <typename Handle>
    [[nodiscard]] VkResult SetDebugName(
        const Device &device, VkObjectType objectType, Handle objectHandle, const char *name) noexcept
    {
        if constexpr (std::is_pointer_v<Handle>)
            return SetDebugName(device, objectType, reinterpret_cast<std::uint64_t>(objectHandle), name);
        else
            return SetDebugName(device, objectType, static_cast<std::uint64_t>(objectHandle), name);
    }
    // 借用正在录制的 CommandBuffer，配对 Begin/End Label
    class DebugLabel final
    {
    public:
        DebugLabel(const Device &device, VkCommandBuffer commandBuffer, const char *name) noexcept;
        ~DebugLabel() noexcept;

        DebugLabel(const DebugLabel &) = delete;
        DebugLabel &operator=(const DebugLabel &) = delete;
        DebugLabel(DebugLabel &&) = delete;
        DebugLabel &operator=(DebugLabel &&) = delete;

    private:
        VkCommandBuffer commandBuffer_;
        bool active_ = false;
    };
}

#endif