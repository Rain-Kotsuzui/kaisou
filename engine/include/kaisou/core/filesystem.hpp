#ifndef KAI_FILESYSTEM_HPP
#define KAI_FILESYSTEM_HPP

#include "kaisou/core/types.hpp"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace kaisou::core
{
    enum class FileError
    {
        OpenFailed,
        ReadFailed,
        WriteFailed,
        CreateDirectoryFailed
    };

    struct FileFailure
    {
        FileError error;
        std::filesystem::path path;
    };

    class FileSystem final
    {
    public:
        using BinaryData =
            std::vector<std::byte>;

        [[nodiscard]]
        static std::filesystem::path Normalize(
            std::filesystem::path path);

        [[nodiscard]]
        static std::expected<std::string, FileFailure>
        ReadText(
            const std::filesystem::path& path);

        [[nodiscard]]
        static std::expected<BinaryData, FileFailure>
        ReadBinary(
            const std::filesystem::path& path);

        [[nodiscard]]
        static std::expected<void, FileFailure>
        WriteText(
            const std::filesystem::path& path,
            std::string_view text);

        [[nodiscard]]
        static std::expected<void, FileFailure>
        WriteBinary(
            const std::filesystem::path& path,
            Span<const std::byte> data);

    private:
        FileSystem() = delete;
    };
}

#endif