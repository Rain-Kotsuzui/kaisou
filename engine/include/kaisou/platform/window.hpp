#ifndef KAI_WINDOW_HPP
#define KAI_WINDOW_HPP

#include <string>

struct SDL_Window;
union SDL_Event;

namespace kaisou::platform
{
    struct WindowConfig
    {
        std::string title = "Kaisou";
        int width = 1280;
        int height = 720;
    };
    struct WindowState
    {
        // 窗口尺寸
        int width = 0;
        int height = 0;

        // 像素尺寸
        int pixelWidth = 0;
        int pixelHeight = 0;

        // 显示缩放
        float displayScale = 1.0f;
        bool minimized = false;

        bool operator==(const WindowState &) const noexcept = default;
    };

    class Window final
    {
    public:
        ~Window();

        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;

        Window(Window &&) = delete;
        Window &operator=(Window &&) = delete;

        [[nodiscard]]
        bool ShouldClose() const noexcept;

        [[nodiscard]]
        const WindowState &State() const noexcept;
        [[nodiscard]]
        bool StateChanged() const noexcept;

        [[nodiscard]]
        bool IsDrawable() const noexcept;
        [[nodiscard]]
        SDL_Window *NativeHandle() const noexcept;

    private:
        friend class Platform;

        explicit Window(const WindowConfig &config);

        // 返回这个事件是否要求刷新窗口状态
        bool HandleEvent(const SDL_Event &event);

        bool RefreshState();

        SDL_Window *window_ = nullptr;
        bool shouldClose_ = false;

        WindowState state_;
        bool stateChanged_ = false;
    };
}

#endif