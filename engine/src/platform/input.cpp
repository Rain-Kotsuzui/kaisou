#include "kaisou/platform/input.hpp"

#include <SDL3/SDL.h>

namespace kaisou::platform
{
    namespace
    {
        void SetButton(ButtonState& state, const bool down) noexcept
        {
            if (state.down == down)
            {
                return;
            }

            state.down = down;

            if (down)
            {
                state.pressed = true;
            }
            else
            {
                state.released = true;
            }
        }

        template <std::size_t Size>
        void ClearTransitions(
            std::array<ButtonState, Size>& states) noexcept
        {
            for (auto& state : states)
            {
                state.pressed = false;
                state.released = false;
            }
        }

        template <std::size_t Size>
        void ReleaseButtons(
            std::array<ButtonState, Size>& states) noexcept
        {
            for (auto& state : states)
            {
                SetButton(state, false);
            }
        }

        Key ToKey(const SDL_Scancode code) noexcept
        {
            const int value = static_cast<int>(code);

            if (code >= SDL_SCANCODE_A && code <= SDL_SCANCODE_Z)
            {
                return static_cast<Key>(
                    static_cast<int>(Key::A) +
                    value - static_cast<int>(SDL_SCANCODE_A));
            }

            if (code >= SDL_SCANCODE_1 && code <= SDL_SCANCODE_9)
            {
                return static_cast<Key>(
                    static_cast<int>(Key::Digit1) +
                    value - static_cast<int>(SDL_SCANCODE_1));
            }

            if (code >= SDL_SCANCODE_F1 && code <= SDL_SCANCODE_F12)
            {
                return static_cast<Key>(
                    static_cast<int>(Key::F1) +
                    value - static_cast<int>(SDL_SCANCODE_F1));
            }

            switch (code)
            {
            case SDL_SCANCODE_0: return Key::Digit0;
            case SDL_SCANCODE_ESCAPE: return Key::Escape;
            case SDL_SCANCODE_SPACE: return Key::Space;
            case SDL_SCANCODE_RETURN: return Key::Enter;
            case SDL_SCANCODE_TAB: return Key::Tab;
            case SDL_SCANCODE_BACKSPACE: return Key::Backspace;
            case SDL_SCANCODE_LEFT: return Key::Left;
            case SDL_SCANCODE_RIGHT: return Key::Right;
            case SDL_SCANCODE_UP: return Key::Up;
            case SDL_SCANCODE_DOWN: return Key::Down;
            case SDL_SCANCODE_LSHIFT: return Key::LeftShift;
            case SDL_SCANCODE_RSHIFT: return Key::RightShift;
            case SDL_SCANCODE_LCTRL: return Key::LeftControl;
            case SDL_SCANCODE_RCTRL: return Key::RightControl;
            case SDL_SCANCODE_LALT: return Key::LeftAlt;
            case SDL_SCANCODE_RALT: return Key::RightAlt;
            default: return Key::Count;
            }
        }

        MouseButton ToMouseButton(const Uint8 button) noexcept
        {
            switch (button)
            {
            case SDL_BUTTON_LEFT: return MouseButton::Left;
            case SDL_BUTTON_RIGHT: return MouseButton::Right;
            case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
            case SDL_BUTTON_X1: return MouseButton::X1;
            case SDL_BUTTON_X2: return MouseButton::X2;
            default: return MouseButton::Count;
            }
        }
    }

    void InputSystem::Initialize(SDL_Window* window)
    {
        snapshot_ = {};
        windowID_ = SDL_GetWindowID(window);

        const auto flags = SDL_GetWindowFlags(window);

        snapshot_.focused_ =
            (flags & SDL_WINDOW_INPUT_FOCUS) != 0;

        if ((flags & SDL_WINDOW_MOUSE_FOCUS) != 0)
        {
            SDL_GetMouseState(
                &snapshot_.mousePosition_.x,
                &snapshot_.mousePosition_.y);
        }
    }

    void InputSystem::BeginFrame() noexcept
    {
        ClearTransitions(snapshot_.keys_);
        ClearTransitions(snapshot_.buttons_);

        snapshot_.mouseDelta_ = {};
        snapshot_.wheelDelta_ = {};
    }

    void InputSystem::SetFocus(const bool focused) noexcept
    {
        snapshot_.focused_ = focused;

        if (!focused)
        {
            ReleaseButtons(snapshot_.keys_);
            ReleaseButtons(snapshot_.buttons_);

            snapshot_.mouseDelta_ = {};
            snapshot_.wheelDelta_ = {};
        }
    }

    void InputSystem::HandleEvent(const SDL_Event& event)
    {
        switch (event.type)
        {
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (event.window.windowID == windowID_)
            {
                SetFocus(
                    event.type == SDL_EVENT_WINDOW_FOCUS_GAINED);
            }
            break;

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (event.key.windowID == windowID_ &&
                snapshot_.focused_ &&
                !event.key.repeat)
            {
                const auto key = ToKey(event.key.scancode);

                if (key != Key::Count)
                {
                    SetButton(
                        snapshot_.keys_[static_cast<std::size_t>(key)],
                        event.type == SDL_EVENT_KEY_DOWN);
                }
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.windowID == windowID_ &&
                snapshot_.focused_)
            {
                const auto button =
                    ToMouseButton(event.button.button);

                if (button != MouseButton::Count)
                {
                    SetButton(
                        snapshot_.buttons_[
                            static_cast<std::size_t>(button)],
                        event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                }

                snapshot_.mousePosition_ = {
                    event.button.x,
                    event.button.y
                };
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (event.motion.windowID == windowID_ &&
                snapshot_.focused_)
            {
                snapshot_.mousePosition_ = {
                    event.motion.x,
                    event.motion.y
                };

                snapshot_.mouseDelta_.x += event.motion.xrel;
                snapshot_.mouseDelta_.y += event.motion.yrel;
            }
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            if (event.wheel.windowID == windowID_ &&
                snapshot_.focused_)
            {
                const float direction =
                    event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                    ? -1.0f
                    : 1.0f;

                snapshot_.wheelDelta_.x +=
                    event.wheel.x * direction;

                snapshot_.wheelDelta_.y +=
                    event.wheel.y * direction;

                snapshot_.mousePosition_ = {
                    event.wheel.mouse_x,
                    event.wheel.mouse_y
                };
            }
            break;

        default:
            break;
        }
    }
}