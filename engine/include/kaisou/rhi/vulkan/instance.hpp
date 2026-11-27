#ifndef KAI_VULKAN_INSTANCE_HPP
#define KAI_VULKAN_INSTANCE_HPP

#include "kaisou/rhi/vulkan/vulkan.hpp"

#include <cstdint>
#include <string>

#ifndef KAISOU_ENABLE_VALIDATION
#error "KAISOU_ENABLE_VALIDATION must be defined by CMake"
#endif

namespace kaisou::rhi::vulkan
{
    struct MergedCapabilityRequests;
    struct EnabledInstanceFeatures
    {
        bool validationEnabled = false;
        bool debugUtilsEnabled = false;
    };
    struct InstanceConfig
    {
        std::string applicationName = "Kaisou";
        std::uint32_t applicationVersion =
            VK_MAKE_API_VERSION(0, 0, 1, 0);
        bool enableValidation =
            KAISOU_ENABLE_VALIDATION != 0;
    };

    class Instance final
    {
    public:
        explicit Instance(MergedCapabilityRequests &requests, const InstanceConfig &config = {});

        ~Instance();

        Instance(const Instance &) = delete;
        Instance &operator=(const Instance &) = delete;
        Instance(Instance &&) = delete;
        Instance &operator=(Instance &&) = delete;

        [[nodiscard]]
        VkInstance Handle() const noexcept;
        [[nodiscard]]
        bool ValidationEnabled() const noexcept;
        [[nodiscard]]
        bool DebugUtilsEnabled() const noexcept
        {
            return enabledFeatures_.debugUtilsEnabled;
        }

        [[nodiscard]]
        const EnabledInstanceFeatures &EnabledFeatures() const noexcept
        {
            return enabledFeatures_;
        }

    private:
        VkInstance instance_ = VK_NULL_HANDLE;

        VkDebugUtilsMessengerEXT debugMessenger_ =
            VK_NULL_HANDLE;
        EnabledInstanceFeatures enabledFeatures_;
    };
}

#endif
