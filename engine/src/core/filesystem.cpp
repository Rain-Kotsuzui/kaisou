#include "kaisou/core/filesystem.hpp"

#include <fstream>
#include <utility>

namespace kaisou::core
{
    namespace
    {
        using Result =
            std::expected<void, FileFailure>;

        Result CreateParentDirectory(
            const std::filesystem::path& path)
        {
            const auto parent =
                path.parent_path();

            if (parent.empty())
                return {};

            std::error_code error;

            std::filesystem::create_directories(
                parent,
                error);

            if (error)
            {
                return std::unexpected(
                    FileFailure{
                        FileError::CreateDirectoryFailed,
                        path
                    });
            }

            return {};
        }
    }

    std::filesystem::path FileSystem::Normalize(
        std::filesystem::path path)
    {
        path = path.lexically_normal();
        path.make_preferred();
        return path;
    }

    std::expected<std::string, FileFailure>
    FileSystem::ReadText(
        const std::filesystem::path& path)
    {
        std::ifstream file(
            path,
            std::ios::binary |
            std::ios::ate);

        if (!file)
        {
            return std::unexpected(
                FileFailure{
                    FileError::OpenFailed,
                    path
                });
        }

        const auto end = file.tellg();

        if (end < 0)
        {
            return std::unexpected(
                FileFailure{
                    FileError::ReadFailed,
                    path
                });
        }

        std::string data(
            static_cast<std::size_t>(end),
            '\0');

        file.seekg(0);

        if (!data.empty() &&
            !file.read(
                data.data(),
                static_cast<std::streamsize>(
                    data.size())))
        {
            return std::unexpected(
                FileFailure{
                    FileError::ReadFailed,
                    path
                });
        }

        return data;
    }

    std::expected<FileSystem::BinaryData, FileFailure>
    FileSystem::ReadBinary(
        const std::filesystem::path& path)
    {
        std::ifstream file(
            path,
            std::ios::binary |
            std::ios::ate);

        if (!file)
        {
            return std::unexpected(
                FileFailure{
                    FileError::OpenFailed,
                    path
                });
        }

        const auto end = file.tellg();

        if (end < 0)
        {
            return std::unexpected(
                FileFailure{
                    FileError::ReadFailed,
                    path
                });
        }

        BinaryData data(
            static_cast<std::size_t>(end));

        file.seekg(0);

        if (!data.empty() &&
            !file.read(
                reinterpret_cast<char*>(
                    data.data()),
                static_cast<std::streamsize>(
                    data.size())))
        {
            return std::unexpected(
                FileFailure{
                    FileError::ReadFailed,
                    path
                });
        }

        return data;
    }

    std::expected<void, FileFailure>
    FileSystem::WriteText(
        const std::filesystem::path& path,
        const std::string_view text)
    {
        if (auto result =
                CreateParentDirectory(path);
            !result)
        {
            return result;
        }

        std::ofstream file(
            path,
            std::ios::binary |
            std::ios::trunc);

        if (!file)
        {
            return std::unexpected(
                FileFailure{
                    FileError::OpenFailed,
                    path
                });
        }

        file.write(
            text.data(),
            static_cast<std::streamsize>(
                text.size()));

        if (!file)
        {
            return std::unexpected(
                FileFailure{
                    FileError::WriteFailed,
                    path
                });
        }

        return {};
    }

    std::expected<void, FileFailure>
    FileSystem::WriteBinary(
        const std::filesystem::path& path,
        const Span<const std::byte> data)
    {
        if (auto result =
                CreateParentDirectory(path);
            !result)
        {
            return result;
        }

        std::ofstream file(
            path,
            std::ios::binary |
            std::ios::trunc);

        if (!file)
        {
            return std::unexpected(
                FileFailure{
                    FileError::OpenFailed,
                    path
                });
        }

        if (!data.empty())
        {
            file.write(
                reinterpret_cast<const char*>(
                    data.data()),
                static_cast<std::streamsize>(
                    data.size()));
        }

        if (!file)
        {
            return std::unexpected(
                FileFailure{
                    FileError::WriteFailed,
                    path
                });
        }

        return {};
    }
}