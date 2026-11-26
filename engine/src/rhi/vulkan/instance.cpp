#include "kaisou/rhi/vulkan/instance.hpp"

#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"
#include "kaisou/rhi/vulkan/presentation/window_surface.hpp"

#include <VkBootstrap.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

namespace kaisou::rhi::vulkan
{
    namespace
    {
        std::vector<std::string> CollectExtensions(const InstanceConfig &config)
        {
            auto extensions = RequiredWindowExtensions();

            const auto append = [&extensions](const std::string &extension)
            {
                if (std::ranges::find(extensions, extension) == extensions.end())
                    extensions.push_back(extension);
            };
            for (const auto &extension : config.additionalExtensions)
                append(extension);
                
            // TODO(M2-CAP-04): Consume merged instance-extension contributions instead of appending presentation requirements here.
            append(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            append(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);

            return extensions;
        }

        VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            const VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            const VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
            void *)
        {
            const char *message =
                callbackData != nullptr && callbackData->pMessage != nullptr
                    ? callbackData->pMessage
                    : "Vulkan validation message unavailable";

            const char *severityName =
                severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
                    ? "error"
                    : "warning"; // 当前仅error和warning

            std::fprintf(
                stderr,
                "[Vulkan][%s][0x%x] %s\n",
                severityName,
                static_cast<unsigned int>(type),
                message);

            try
            {
                if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                    core::Log::Error(
                        "Vulkan.Validation",
                        "{}",
                        message);
                else
                    core::Log::Warn(
                        "Vulkan.Validation",
                        "{}",
                        message);
            }
            catch (...)
            {
                // C++ 异常不能越过 Vulkan C 回调边界
            }

            return VK_FALSE;
        }
    }

    Instance::Instance(const InstanceConfig &config)
    {
        const VkResult initializeResult = volkInitialize();
        if (initializeResult != VK_SUCCESS)
            KAISOU_FATAL("Failed to initialize Vulkan loader through Volk");

        auto systemInfoResult = vkb::SystemInfo::get_system_info(vkGetInstanceProcAddr);
        if (!systemInfoResult)
        {
            const std::string message = "Failed to query Vulkan system information: " + systemInfoResult.error().message();
            KAISOU_FATAL(message);
        }

        auto extensions = CollectExtensions(config);
        const auto &systemInfo = systemInfoResult.value();
        if (config.enableValidation ||
            (KAISOU_ENABLE_GPU_DEBUG && systemInfo.is_extension_available(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)))
        {
            if (std::ranges::find(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == extensions.end())
                extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        else if (KAISOU_ENABLE_GPU_DEBUG)
        {
            KAISOU_LOG_WARN("Vulkan", "VK_EXT_debug_utils unavailable; debug annotations disabled");
        }

        for (const auto &extension : extensions)
        {
            if (!systemInfo.is_extension_available(extension.c_str()))
            {
                const std::string message = "Required Vulkan instance extension is unavailable: " + extension;
                KAISOU_FATAL(message);
            }
        }

        for (const auto &extension : extensions)
            KAISOU_LOG_INFO("Vulkan", "Requesting Vulkan instance extension: {}", extension);

        vkb::InstanceBuilder builder{
            vkGetInstanceProcAddr};

        builder
            .set_app_name(config.applicationName)
            .set_app_version(config.applicationVersion)
            .set_engine_name("Kaisou")
            .set_engine_version(
                KAISOU_ENGINE_VERSION_MAJOR,
                KAISOU_ENGINE_VERSION_MINOR,
                KAISOU_ENGINE_VERSION_PATCH)
            .require_api_version(1, 4, 0)
            .enable_extensions(extensions);

        if (config.enableValidation)
        {
            builder
                .enable_validation_layers()
                .set_debug_callback(DebugCallback)
                .set_debug_messenger_severity(
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                .set_debug_messenger_type(
                    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT);
        }

        auto instanceResult = builder.build();
        if (!instanceResult)
        {
            const std::string message = instanceResult.error().message();
            KAISOU_FATAL(message);
        }

        const auto builtInstance = std::move(instanceResult).value();

        instance_ = builtInstance.instance;
        debugMessenger_ = builtInstance.debug_messenger;
        validationEnabled_ = config.enableValidation;
        debugUtilsEnabled_ =
            std::ranges::find(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != extensions.end();

        volkLoadInstance(instance_);
    }

    Instance::~Instance()
    {
        if (debugMessenger_ != VK_NULL_HANDLE)
            vkDestroyDebugUtilsMessengerEXT(
                instance_,
                debugMessenger_,
                nullptr);

        if (instance_ != VK_NULL_HANDLE)
            vkDestroyInstance(
                instance_,
                nullptr);
    }

    VkInstance Instance::Handle() const noexcept
    {
        return instance_;
    }

    bool Instance::ValidationEnabled() const noexcept
    {
        return validationEnabled_;
    }
}
