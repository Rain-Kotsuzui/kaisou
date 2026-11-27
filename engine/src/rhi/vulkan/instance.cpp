#include "kaisou/rhi/vulkan/instance.hpp"

#include "kaisou/core/assert.hpp"
#include "kaisou/core/log.hpp"
#include "kaisou/rhi/vulkan/capabilities/capability_merge.hpp"
#include "kaisou/rhi/vulkan/enumerate.hpp"

#include <VkBootstrap.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include <fmt/format.h>

namespace kaisou::rhi::vulkan
{
    namespace
    {

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

    Instance::Instance(MergedCapabilityRequests &requests, const InstanceConfig &config)
    {
        const VkResult initializeResult = volkInitialize();
        if (initializeResult != VK_SUCCESS)
            KAISOU_FATAL("Failed to initialize Vulkan loader through Volk");

        auto systemInfoResult = vkb::SystemInfo::get_system_info(vkGetInstanceProcAddr);
        if (!systemInfoResult)
            KAISOU_FATAL("Failed to query Vulkan system information: " + systemInfoResult.error().message());

        const auto &systemInfo = systemInfoResult.value();
        const auto apiVersion = std::max(requests.minimumApiVersion, VK_API_VERSION_1_4);

        if (systemInfo.instance_api_version < apiVersion)
        {
            KAISOU_FATAL(fmt::format(
                "Requested Vulkan API version {}.{}.{} is unavailable",
                VK_API_VERSION_MAJOR(apiVersion), VK_API_VERSION_MINOR(apiVersion), VK_API_VERSION_PATCH(apiVersion)));
        }

        const char *validationLayer = "VK_LAYER_KHRONOS_validation";
        if (config.enableValidation && !systemInfo.is_layer_available(validationLayer))
            KAISOU_FATAL("Required Vulkan validation layer is unavailable");

        auto availableExtensions = EnumerateProperties<VkExtensionProperties>(
            "vkEnumerateInstanceExtensionProperties", vkEnumerateInstanceExtensionProperties, nullptr);

        if (config.enableValidation)
        {
            const auto layerExtensions = EnumerateProperties<VkExtensionProperties>(
                "vkEnumerateInstanceExtensionProperties", vkEnumerateInstanceExtensionProperties, validationLayer);

            availableExtensions.insert(
                availableExtensions.end(), layerExtensions.begin(), layerExtensions.end());
        }

        ResolveInstanceExtensionRequests(requests, availableExtensions);

        const bool debugUtilsEnabled =
            std::ranges::find(
                requests.instanceExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                &MergedExtensionRequest::name) != requests.instanceExtensions.end();

        if (config.enableValidation && !debugUtilsEnabled)
            KAISOU_FATAL("Validation requires VK_EXT_debug_utils in finalized requests");

        const bool portabilityEnabled =
            std::ranges::find(
                requests.instanceExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
                &MergedExtensionRequest::name) != requests.instanceExtensions.end();

        std::vector<const char *> extensions;
        extensions.reserve(requests.instanceExtensions.size());
        for (const auto &request : requests.instanceExtensions)
        {
            extensions.push_back(request.name.c_str());
            KAISOU_LOG_INFO("Vulkan", "Requesting Vulkan instance extension: {}", request.name);
        }

        VkApplicationInfo applicationInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        applicationInfo.pApplicationName = config.applicationName.c_str();
        applicationInfo.applicationVersion = config.applicationVersion;
        applicationInfo.pEngineName = "Kaisou";
        applicationInfo.engineVersion = VK_MAKE_API_VERSION(
            0, KAISOU_ENGINE_VERSION_MAJOR, KAISOU_ENGINE_VERSION_MINOR, KAISOU_ENGINE_VERSION_PATCH);
        applicationInfo.apiVersion = apiVersion;

        VkDebugUtilsMessengerCreateInfoEXT debugInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debugInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = DebugCallback;

        VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        createInfo.pNext = config.enableValidation ? &debugInfo : nullptr;
        createInfo.flags = portabilityEnabled ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR : 0;
        createInfo.pApplicationInfo = &applicationInfo;
        createInfo.enabledExtensionCount = static_cast<core::u32>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
        createInfo.enabledLayerCount = config.enableValidation ? 1 : 0;
        createInfo.ppEnabledLayerNames = config.enableValidation ? &validationLayer : nullptr;

        VkInstance createdInstance = VK_NULL_HANDLE;
        const VkResult createResult = vkCreateInstance(&createInfo, nullptr, &createdInstance);
        if (createResult != VK_SUCCESS)
            KAISOU_FATAL(fmt::format("Failed to create Vulkan instance: VkResult {}", static_cast<int>(createResult)));

        volkLoadInstance(createdInstance);

        VkDebugUtilsMessengerEXT createdMessenger = VK_NULL_HANDLE;
        if (config.enableValidation)
        {
            const VkResult messengerResult =
                vkCreateDebugUtilsMessengerEXT(createdInstance, &debugInfo, nullptr, &createdMessenger);

            if (messengerResult != VK_SUCCESS)
            {
                vkDestroyInstance(createdInstance, nullptr);
                KAISOU_FATAL(fmt::format(
                    "Failed to create Vulkan debug messenger: VkResult {}", static_cast<int>(messengerResult)));
            }
        }

        instance_ = createdInstance;
        debugMessenger_ = createdMessenger;
        enabledFeatures_ = {config.enableValidation, debugUtilsEnabled};
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
        return enabledFeatures_.validationEnabled;
    }
}
