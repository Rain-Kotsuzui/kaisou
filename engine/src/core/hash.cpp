#include "kaisou/core/hash.hpp"

#include <fmt/format.h>
#include <xxhash.h>

namespace kaisou::core
{
    namespace
    {
        const void* DataOrEmpty(
            const void* data,
            const std::size_t size)
        {
            static constexpr char Empty = '\0';

            return size == 0
                ? static_cast<const void*>(&Empty)
                : data;
        }
    }

    ContentHash ContentHasher::HashBytes(
        const Span<const std::byte> data)
    {
        return ContentHash{
            XXH3_64bits(
                DataOrEmpty(
                    data.data(),
                    data.size()),
                data.size())
        };
    }

    ContentHash ContentHasher::HashString(
        const std::string_view text)
    {
        return ContentHash{
            XXH3_64bits(
                DataOrEmpty(
                    text.data(),
                    text.size()),
                text.size())
        };
    }

    std::expected<ContentHash, FileFailure>
    ContentHasher::HashFile(
        const std::filesystem::path& path)
    {
        auto data =
            FileSystem::ReadBinary(path);

        if (!data)
        {
            return std::unexpected(
                data.error());
        }

        return HashBytes(*data);
    }

    std::string ContentHasher::ToHex(
        const ContentHash hash)
    {
        return fmt::format(
            "{:016x}",
            hash.Value());
    }
}