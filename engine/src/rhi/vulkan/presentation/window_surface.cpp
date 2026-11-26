
#include "kaisou/platform/window.hpp"

#include "kaisou/core/assert.hpp"

#include "kaisou/rhi/vulkan/vulkan.hpp"
#include "kaisou/rhi/vulkan/instance.hpp"
#include "kaisou/rhi/vulkan/presentation/window_surface.hpp"

#include <SDL3/SDL_vulkan.h>

namespace kaisou::rhi::vulkan
{
    std::vector<std::string> RequiredWindowExtensions()
    {
        Uint32 extensionCount = 0;

        const char *const *extensionNames =
            SDL_Vulkan_GetInstanceExtensions(
                &extensionCount);

        if (extensionNames == nullptr)
            KAISOU_FATAL(SDL_GetError());

        if (extensionCount == 0)
            KAISOU_FATAL("SDL reported no required Vulkan instance extensions");

        std::vector<std::string> extensions;
        extensions.reserve(extensionCount);

        for (Uint32 index = 0; index < extensionCount; ++index)
        {
            if (extensionNames[index] == nullptr)
                KAISOU_FATAL("SDL returned a null Vulkan extension name");

            extensions.emplace_back(
                extensionNames[index]);
        }

        return extensions;
    }

    WindowSurface::WindowSurface(
        const Instance &instance,
        const platform::Window &window)
        : instance_(instance.Handle())
    {
        if (!SDL_Vulkan_CreateSurface(
                window.NativeHandle(),
                instance_,
                nullptr,
                &surface_))
            KAISOU_FATAL(SDL_GetError());
    }

    WindowSurface::~WindowSurface()
    {
        if (surface_ != VK_NULL_HANDLE)
            SDL_Vulkan_DestroySurface(
                instance_,
                surface_,
                nullptr);
    }

    VkSurfaceKHR WindowSurface::Handle() const noexcept
    {
        return surface_;
    }
}
