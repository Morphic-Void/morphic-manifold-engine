//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)

#include "tests/environment/test_paths.hpp"

#include <filesystem>
#include <system_error>

namespace test_environment
{
namespace
{
std::filesystem::path s_repository_root;
std::filesystem::path s_binary_directory;
std::string s_repository_root_text;

bool is_repository_root(const std::filesystem::path& candidate)
{
    std::error_code error;
    return std::filesystem::is_regular_file(
        candidate / "test_data" / "input" / "files" / "test_input.tga",
        error);
}

std::filesystem::path find_repository_root(std::filesystem::path candidate)
{
    std::error_code error;
    candidate = std::filesystem::absolute(candidate, error);
    if (error)
    {
        return {};
    }

    for (;;)
    {
        if (is_repository_root(candidate))
        {
            return candidate.lexically_normal();
        }
        const std::filesystem::path parent = candidate.parent_path();
        if (parent.empty() || parent == candidate)
        {
            return {};
        }
        candidate = parent;
    }
}
}

bool initialise_paths(const char* const executable_argument) noexcept
{
    try
    {
        std::error_code error;
        const std::filesystem::path current = std::filesystem::current_path(error);
        if (error)
        {
            return false;
        }

        if ((executable_argument != nullptr) && (executable_argument[0] != 0))
        {
            std::filesystem::path executable(executable_argument);
            executable = std::filesystem::absolute(executable, error);
            if (!error)
            {
                s_binary_directory = executable.parent_path().lexically_normal();
                s_repository_root = find_repository_root(s_binary_directory);
            }
        }

        if (s_repository_root.empty())
        {
            s_repository_root = find_repository_root(current);
        }
        if (s_binary_directory.empty())
        {
            s_binary_directory = current.lexically_normal();
        }
        if (s_repository_root.empty())
        {
            return false;
        }

        s_repository_root_text = s_repository_root.string();
        std::filesystem::current_path(s_repository_root, error);
        if (error)
        {
            return false;
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

const std::string& repository_root() noexcept
{
    return s_repository_root_text;
}

std::string repository_path(const char* const relative_path)
{
    if ((relative_path == nullptr) || s_repository_root.empty())
    {
        return {};
    }
    return (s_repository_root / std::filesystem::path(relative_path)).lexically_normal().string();
}

std::string binary_path(const char* const filename)
{
    if ((filename == nullptr) || s_binary_directory.empty())
    {
        return {};
    }
    return (s_binary_directory / std::filesystem::path(filename)).lexically_normal().string();
}

}   //  namespace test_environment
