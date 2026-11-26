#ifndef KAI_PLATFORM_HPP
#define KAI_PLATFORM_HPP

#include "kaisou/platform/window.hpp"
#include "kaisou/platform/input.hpp"
#include "kaisou/core/time.hpp"

#include <memory>
#include <exception>

namespace kaisou::platform
{
    class Platform final
    {
    public:
        using LiveResizeCallback = void (*)(void *context, const InputSnapshot &input);

        explicit Platform(const WindowConfig &config);
        ~Platform();

        Platform(const Platform &) = delete;
        Platform &operator=(const Platform &) = delete;

        Platform(Platform &&) = delete;
        Platform &operator=(Platform &&) = delete;

        void PollEvents();

        // 只能在主线程、回调执行之外绑定
        void BindLiveResizeCallback(LiveResizeCallback callback, void *context);
        void UnbindLiveResizeCallback() noexcept;

        static void WaitFor(core::Duration duration);

        [[nodiscard]]
        bool ShouldQuit() const noexcept;

        [[nodiscard]]
        const Window &MainWindow() const noexcept;

        [[nodiscard]]
        const InputSnapshot &Input() const noexcept;

    private:
        struct EventWatcher;

        void HandleRedrawEvent(const SDL_Event &event) noexcept;

        std::unique_ptr<Window> window_;
        InputSystem input_;
        bool quitRequested_ = false;

        LiveResizeCallback liveResizeCallback_ = nullptr;
        void *liveResizeContext_ = nullptr;
        std::exception_ptr liveResizeException_;
        bool eventWatchRegistered_ = false;
        bool liveResizeExecuting_ = false;
    };
}

#endif
