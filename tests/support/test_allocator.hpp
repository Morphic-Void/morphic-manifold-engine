//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)

#pragma once

#ifndef MORPHIC_TEST_ALLOCATOR_HPP_INCLUDED
#define MORPHIC_TEST_ALLOCATOR_HPP_INCLUDED

#include <cstddef>

#include "platform/platform_defines.hpp"

namespace tests
{

struct TAllocatorFixture
{
    bool reject_allocation{ false };
};

[[nodiscard]] void* MV_STD_ABI_CALL allocate_test_memory(
    void* state, std::size_t alignment, std::size_t bytes) noexcept;
[[nodiscard]] bool MV_STD_ABI_CALL deallocate_test_memory(
    void* state, std::size_t alignment, void* pointer) noexcept;

}   //  namespace tests

#endif  //  MORPHIC_TEST_ALLOCATOR_HPP_INCLUDED
