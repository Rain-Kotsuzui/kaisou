#ifndef KAI_INPUT_HPP
#define KAI_INPUT_HPP

#include "kaisou/core/types.hpp"

#include <array>
#include <cstddef>

struct SDL_Window;
union SDL_Event;

namespace kaisou::platform
{
    enum class Key
    {
        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        Digit0, Digit1, Digit2, Digit3, Digit4,
        Digit5, Digit6, Digit7, Digit8, Digit9,

        F1, F2, F3, F4, F5, F6,
        F7, F8, F9, F10, F11, F12,

        Escape, Space, Enter, Tab, Backspace,
        Left, Right, Up, Down,
        LeftShift, RightShift,
        LeftControl, RightControl,
        LeftAlt, RightAlt,

        Count
    };

    enum class MouseButton
    {
        Left,
        Right,
        Middle,
        X1,
        X2,

        Count
    };

    struct ButtonState
    {
        bool down = false;
        bool pressed = false;
        bool released = false;
    };

    struct InputPosition
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    class InputSnapshot final
    {
    public:
        [[nodiscard]]
        ButtonState GetKey(Key key) const
        {
            return keys_.at(static_cast<std::size_t>(key));
        }

        [[nodiscard]]
        ButtonState GetMouseButton(MouseButton button) const
        {
            return buttons_.at(static_cast<std::size_t>(button));
        }

        [[nodiscard]]
        InputPosition MousePosition() const noexcept
        {
            return mousePosition_;
        }

        [[nodiscard]]
        InputPosition MouseDelta() const noexcept
        {
            return mouseDelta_;
        }

        [[nodiscard]]
        InputPosition WheelDelta() const noexcept
        {
            return wheelDelta_;
        }

        [[nodiscard]]
        bool HasFocus() const noexcept
        {
            return focused_;
        }

    private:
        friend class InputSystem;

        std::array<ButtonState,
            static_cast<std::size_t>(Key::Count)> keys_{};

        std::array<ButtonState,
            static_cast<std::size_t>(MouseButton::Count)> buttons_{};

        InputPosition mousePosition_;
        InputPosition mouseDelta_;
        InputPosition wheelDelta_;

        bool focused_ = false;
    };

    class InputSystem final
    {
    public:
        [[nodiscard]]
        const InputSnapshot& Snapshot() const noexcept
        {
            return snapshot_;
        }

    private:
        friend class Platform;
        [[nodiscard]] InputSnapshot LiveResizeSnapshot() const noexcept;

        void Initialize(SDL_Window* window);
        void BeginFrame() noexcept;
        void HandleEvent(const SDL_Event& event);
        void SetFocus(bool focused) noexcept;

        InputSnapshot snapshot_;
        kaisou::core::u32 windowID_ = 0;
    };
}

#endif
