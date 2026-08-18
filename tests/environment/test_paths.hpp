//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)

#pragma once

#ifndef MORPHIC_TEST_PATHS_HPP_INCLUDED
#define MORPHIC_TEST_PATHS_HPP_INCLUDED

#include <string>

namespace test_environment
{

[[nodiscard]] bool initialise_paths(const char* executable_argument) noexcept;
[[nodiscard]] const std::string& repository_root() noexcept;
[[nodiscard]] std::string repository_path(const char* relative_path);
[[nodiscard]] std::string binary_path(const char* filename);

}   //  namespace test_environment

#endif  //  MORPHIC_TEST_PATHS_HPP_INCLUDED
