#ifndef KAI_WINDOW_SURFACE_HPP
#define KAI_WINDOW_SURFACE_HPP

#include "kaisou/rhi/vulkan/vulkan.hpp"

#include <string>
#include <vector>

namespace kaisou::platform
{
    class Window;
}

namespace kaisou::rhi::vulkan
{
    class Instance;
    
    [[nodiscard]]
    std::vector<std::string> RequiredWindowExtensions();

    class WindowSurface final
    {
    public:
        WindowSurface(
            const Instance &instance,
            const platform::Window &window);

        ~WindowSurface();

        WindowSurface(const WindowSurface &) = delete;
        WindowSurface &operator=(const WindowSurface &) = delete;

        WindowSurface(WindowSurface &&) = delete;
        WindowSurface &operator=(WindowSurface &&) = delete;

        [[nodiscard]]
        VkSurfaceKHR Handle() const noexcept;

    private:
        VkInstance instance_ = VK_NULL_HANDLE;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    };
}

#endif
