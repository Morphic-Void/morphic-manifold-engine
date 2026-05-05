
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   predicate_wait_helpers.hpp
//  Author: Ritchie Brannan
//  Date:   5 May 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Platform-agnostic predicate wait helpers over platform wait primitives.
//
//  Provides completed equality/inequality predicate loops.
//  The raw wait primitive remains platform::threading::wait_while_equal().

#pragma once

#ifndef PREDICATE_WAIT_HELPERS_HPP_INCLUDED
#define PREDICATE_WAIT_HELPERS_HPP_INCLUDED

#include <atomic>       //  std::atomic
#include <cstdint>      //  std::uint32_t

#include "platform/threading/primitives.hpp"

namespace threading
{

//  Wait until word is observed to equal value.
//
//  This waits for a specific visible value, not just for progress.
//  word must remain alive and stable for the duration of the wait.
inline void wait_until_equal(const std::atomic<std::uint32_t>* const word, const std::uint32_t value) noexcept
{
    for (;;)
    {
        const std::uint32_t current = word->load(std::memory_order_acquire);

        if (current == value)
        {
            break;
        }

        platform::threading::wait_while_equal(word, current);
    }
}

//  Wait until word is observed to differ from value.
//  Returns the observed non-equal value.
//
//  word must remain alive and stable for the duration of the wait.
inline std::uint32_t wait_until_not_equal(const std::atomic<std::uint32_t>* const word, const std::uint32_t value) noexcept
{
    for (;;)
    {
        const std::uint32_t current = word->load(std::memory_order_acquire);

        if (current != value)
        {
            return current;
        }

        platform::threading::wait_while_equal(word, value);
    }
}

}   //  namespace threading

#endif  //  #ifndef PREDICATE_WAIT_HELPERS_HPP_INCLUDED
