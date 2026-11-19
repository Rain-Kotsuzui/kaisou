#ifndef KAI_HASH_HPP
#define KAI_HASH_HPP

#include "kaisou/core/filesystem.hpp"
#include "kaisou/core/types.hpp"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace kaisou::core
{
    class ContentHash final
    {
    public:
        ContentHash() = delete;

        explicit constexpr ContentHash(
            const u64 value) noexcept
            : value_(value)
        {
        }

        [[nodiscard]]
        constexpr u64 Value() const noexcept
        {
            return value_;
        }

        constexpr bool operator==(
            const ContentHash &) const noexcept = default;

    private:
        u64 value_;
    };

    class ContentHasher final
    {
    public:
        [[nodiscard]]
        static ContentHash HashBytes(
            Span<const std::byte> data);

        [[nodiscard]]
        static ContentHash HashString(
            std::string_view text);

        [[nodiscard]]
        static std::expected<ContentHash, FileFailure>
        HashFile(
            const std::filesystem::path &path);

        [[nodiscard]]
        static std::string ToHex(
            ContentHash hash);

    private:
        ContentHasher() = delete;
    };
}

#endif