#ifndef KAI_TYPES_HPP
#define KAI_TYPES_HPP

#include <cstdint>
#include <limits>
#include <span>

namespace kaisou::core
{
    using i8 = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;

    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using f32 = float;
    using f64 = double;

    template <
        typename Type,
        std::size_t Extent = std::dynamic_extent>
    using Span = std::span<Type, Extent>;

    template <typename Tag>
    class Handle final
    {
    public:
        static constexpr u32 InvalidIndex = std::numeric_limits<u32>::max();

        constexpr Handle() noexcept = default;

        explicit constexpr Handle(
            const u32 index,
            const u32 generation = 0) noexcept
            : index_(index),
              generation_(generation)
        {
        }

        [[nodiscard]]
        constexpr bool IsValid() const noexcept
        {
            return index_ != InvalidIndex;
        }

        [[nodiscard]]
        explicit constexpr operator bool() const noexcept
        {
            return IsValid();
        }

        [[nodiscard]]
        constexpr u32 Index() const noexcept
        {
            return index_;
        }

        [[nodiscard]]
        constexpr u32 Generation() const noexcept
        {
            return generation_;
        }

        constexpr bool operator==(
            const Handle&) const noexcept = default;

    private:
        u32 index_ = InvalidIndex;
        u32 generation_ = 0;
    };
}

#endif