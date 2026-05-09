//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   wait_word.hpp
//  Author: Ritchie Brannan
//  Date:   7 May 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Low-level atomic wait/wake primitives for 32-bit wait words.
//
//  This header is available only when the platform provides native wait-word
//  support.
//
//  The primitive functions expose platform wait-on-address style blocking over
//  an externally owned atomic word. Wakeups are not remembered. A single wait
//  may return spuriously or after the observed word has changed.
//
//  The predicate helpers build simple acquire-load wait loops over those
//  primitives. wait_until_equal() waits until a specific value is observed.
//  wait_until_not_equal() waits until a different value is observed and returns
//  that value.
//
//  The wait and wake primitives do not modify the atomic word or provide
//  synchronization by themselves. Ordering is provided by the caller's atomic
//  operations and by the acquire loads used by the predicate helpers.
//
//  The wait word must remain alive and stable for the duration of any wait.

#pragma once

#ifndef WAIT_WORD_HPP_INCLUDED
#define WAIT_WORD_HPP_INCLUDED

#include <atomic>       //  std::atomic
#include <cstdint>      //  std::uint32_t

namespace platform::threading
{

//==============================================================================
//  Atomic wait word primitives
//==============================================================================

//  Blocks the calling thread while word equals expected.
void wait_while_equal(const std::atomic<std::uint32_t>& word, const std::uint32_t expected) noexcept;

//  Wakes one current waiter blocked on word, if one exists.
void wake_one_waiter(const std::atomic<std::uint32_t>& word) noexcept;

//  Wakes all current waiters blocked on word, if any exist.
void wake_all_waiters(const std::atomic<std::uint32_t>& word) noexcept;

//==============================================================================
//  Atomic wait word predicates
//==============================================================================

//  Wait until word is seen to equal value.
//  This waits for a specific visible value, not just for progress.
void wait_until_equal(const std::atomic<std::uint32_t>& word, const std::uint32_t value) noexcept;

//  Wait until word is seen to differ from value.
//  Returns the seen non-equal value.
std::uint32_t wait_until_not_equal(const std::atomic<std::uint32_t>& word, const std::uint32_t value) noexcept;

}   //  namespace platform::threading

#endif  //  #ifndef WAIT_WORD_HPP_INCLUDED
