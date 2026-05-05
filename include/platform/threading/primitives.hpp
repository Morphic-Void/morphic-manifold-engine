
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   primitives.hpp
//  Author: Ritchie Brannan
//  Date:   4 May 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Base level platform threading support primitives.

#pragma once

#ifndef THREADING_PRIMITIVES_HPP_INCLUDED
#define THREADING_PRIMITIVES_HPP_INCLUDED

#include <atomic>       //  std::atomic
#include <cstdint>      //  std::uint8_t, std::uint32_t, std::uint64_t

namespace platform::threading
{

//==============================================================================
//  Native supported thread count query
//==============================================================================

//  Returns a non-zero logical execution-context count hint.
//
//  Suitable for startup sizing and thread-system policy decisions.
//  This is not a physical core count, NUMA topology query, scheduling
//  guarantee, or dynamic topology monitor.
//
//  Platforms may reflect process/thread availability where this is cheap and
//  mechanical. Failure or unavailable platform support returns 1.
std::uint32_t query_hardware_thread_count() noexcept;

//==============================================================================
//  Native thread identifier query
//==============================================================================

struct CPlatformThreadId
{
    std::uint64_t value;
};

//  Returns a best-effort native OS thread id for the calling thread.
//
//  The returned value is a diagnostic witness only.
//  It is not a thread handle, engine thread id, role id, module id, persistent
//  process-independent identity, or array index. It may be reused after the
//  native thread exits.
//
//  Failure or unavailable platform support returns {0}.
CPlatformThreadId query_current_thread_id() noexcept;

inline bool is_valid(CPlatformThreadId id) noexcept { return id.value != 0u; }
inline bool are_equal(CPlatformThreadId lhs, CPlatformThreadId rhs) noexcept { return lhs.value == rhs.value; }
inline bool operator==(CPlatformThreadId lhs, CPlatformThreadId rhs) noexcept { return are_equal(lhs, rhs); }
inline bool operator!=(CPlatformThreadId lhs, CPlatformThreadId rhs) noexcept { return !are_equal(lhs, rhs); }

//==============================================================================
//  Atomic wait word primitives
//==============================================================================

//  Blocks the calling thread while the atomic word still equals expected.
//
//  This is a single low-level wait attempt, not a complete predicate wait.
//  The function may return even if the word still equals expected, and it may
//  also return because the word no longer equals expected before the thread
//  was parked.
//
//  Callers must re-check their controlling predicate after every return.
//  Use this directly when building custom atomic state-machine loops, such as
//  CAS-based counters or semaphores.
//
//  The wait word must remain alive for the duration of any wait and must be
//  naturally aligned. Invalid pointers, misalignment, and waiting on destroyed
//  storage are caller misuse.
void wait_while_equal(const std::atomic<std::uint32_t>* word, const std::uint32_t expected) noexcept;

//  Wakes one current waiter blocked on word, if one exists.
//
//  Wakeups are not remembered. This does not modify the atomic word and does
//  not provide acquire/release synchronization.
void wake_one_waiter(const std::atomic<std::uint32_t>* word) noexcept;

//  Wakes all current waiters blocked on word, if any exist.
//
//  Wakeups are not remembered. This does not modify the atomic word and does
//  not provide acquire/release synchronization.
void wake_all_waiters(const std::atomic<std::uint32_t>* word) noexcept;

//==============================================================================
//  Native exclusive locking
//==============================================================================

struct CExclusiveLock
{
    alignas(16) std::uint8_t storage[64] = {};
    bool is_valid = false;

    CExclusiveLock() noexcept = default;
    ~CExclusiveLock() noexcept = default;

    CExclusiveLock(const CExclusiveLock&) = delete;
    CExclusiveLock& operator=(const CExclusiveLock&) = delete;

    CExclusiveLock(CExclusiveLock&&) = delete;
    CExclusiveLock& operator=(CExclusiveLock&&) = delete;
};

//  Creates an in-process, exclusive, non-recursive native lock.
//
//  Returns false if native initialisation fails or the lock is already live.
//  A live lock must not be copied, moved, relocated, or bytewise cloned.
bool initialise_exclusive_lock(CExclusiveLock* const lock) noexcept;

//  Destroys a live native lock and returns it to the invalid state.
//
//  Destroying an acquired lock is misuse.
void destroy_exclusive_lock(CExclusiveLock* const lock) noexcept;

//  Blocks until the lock is acquired.
//
//  Recursive acquisition is misuse.
void acquire_exclusive_lock(CExclusiveLock* const lock) noexcept;

//  Releases a lock acquired by the calling thread.
//
//  Releasing from a non-owner thread is misuse.
void release_exclusive_lock(CExclusiveLock* const lock) noexcept;

// Returns whether the wrapper currently contains a live native lock.
inline bool is_valid_exclusive_lock(const CExclusiveLock* const lock) noexcept { return (lock != nullptr) && lock->is_valid; }

}   //  namespace platform::threading

#endif  //  #ifndef THREADING_PRIMITIVES_HPP_INCLUDED
