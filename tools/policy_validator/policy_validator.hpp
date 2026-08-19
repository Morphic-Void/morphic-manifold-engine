//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    policy_validator.hpp
//  Primary implementation: OpenAI tools
//  Reviewed and accepted by: Ritchie Brannan
//  Date:    19 Aug 26

#pragma once

#ifndef MORPHIC_POLICY_VALIDATOR_HPP_INCLUDED
#define MORPHIC_POLICY_VALIDATOR_HPP_INCLUDED

#include <filesystem>
#include <string>

namespace morphic::policy
{

struct SOptions
{
    std::filesystem::path repository_root;
    std::string project{ "Repository" };
    std::string configuration;
    std::string platform;
    bool write_report{ true };
};

[[nodiscard]] int run(const SOptions& options);
[[nodiscard]] std::filesystem::path discover_repository_root(
    const std::filesystem::path& start);

}   //  namespace morphic::policy

#endif  //  MORPHIC_POLICY_VALIDATOR_HPP_INCLUDED
