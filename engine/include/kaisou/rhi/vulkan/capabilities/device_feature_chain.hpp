#ifndef KAI_VULKAN_CAPABILITIES_DEVICE_FEATURE_CHAIN_HPP
#define KAI_VULKAN_CAPABILITIES_DEVICE_FEATURE_CHAIN_HPP

#include "kaisou/rhi/vulkan/capabilities/device_capabilities.hpp"

namespace kaisou::rhi::vulkan
{
    class DeviceFeatureChain final
    {
    public:
        DeviceFeatureChain() noexcept;

        DeviceFeatureChain(const DeviceFeatureChain &) = delete;
        DeviceFeatureChain &operator=(const DeviceFeatureChain &) = delete;
        DeviceFeatureChain(DeviceFeatureChain &&) = delete;
        DeviceFeatureChain &operator=(DeviceFeatureChain &&) = delete;

        // 只有候选设备支持 KHR maintenance 扩展时才查询扩展节点
        void Query(VkPhysicalDevice physicalDevice, bool querySwapchainMaintenance) noexcept;

        // 仅用于读取 Query 的结果
        [[nodiscard]] bool Supports(DeviceFeature feature) const noexcept;

        // 输入必须已经完成支持判定；这里只准备创建数据，不代表实际启用
        void ConfigureEnablement(core::Span<const DeviceFeature> features) noexcept;

        // 借用期间不得重置或销毁拥有者
        [[nodiscard]] const VkPhysicalDeviceFeatures2 &Head() const noexcept;

    private:
        void Reset(bool includeSwapchainMaintenance) noexcept;

        template <typename Self>
        static auto &Value(Self &self, DeviceFeature feature) noexcept;

        VkPhysicalDeviceFeatures2 features2_{};
        VkPhysicalDeviceVulkan12Features features12_{};
        VkPhysicalDeviceVulkan13Features features13_{};
        VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR maintenance_{};
    };
}

#endif