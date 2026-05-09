
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   wait_word.cpp
//  Author: Ritchie Brannan
//  Date:   7 May 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Low-level atomic wait/wake primitive implementation.

#include <atomic>       //  std::atomic
#include <cstdint>      //  std::uint32_t

#include "platform/threading/wait_word.hpp"
#include "platform/platform_defines.hpp"

#if MV_PLATFORM_WINDOWS
#include "platform/windows_include.hpp"
#endif

#if MV_PLATFORM_LINUX || MV_PLATFORM_ANDROID
#include <limits.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#if MV_PLATFORM_MAC_OS
#include <os/os_sync_wait_on_address.h>
#endif

namespace platform::threading
{

//==============================================================================
//  Internals
//==============================================================================

namespace internal
{

static_assert((sizeof(std::atomic<std::uint32_t>) == sizeof(std::uint32_t)),
    "std::atomic<std::uint32_t> must have the same storage size as std::uint32_t.");

static_assert((alignof(std::atomic<std::uint32_t>) >= alignof(std::uint32_t)),
    "std::atomic<std::uint32_t> must be naturally aligned for 32-bit waits.");

static_assert((std::atomic<std::uint32_t>::is_always_lock_free),
    "std::atomic<std::uint32_t> must be always lock-free.");

#if MV_PLATFORM_WINDOWS || MV_PLATFORM_MAC_OS

void* wait_address_from_word(const std::atomic<std::uint32_t>& word) noexcept
{
    const void* const address = static_cast<const void*>(&word);

    return const_cast<void*>(address);
}

#endif

#if MV_PLATFORM_LINUX || MV_PLATFORM_ANDROID

const std::uint32_t* futex_address_from_word(const std::atomic<std::uint32_t>& word) noexcept
{
    return reinterpret_cast<const std::uint32_t*>(&word);
}

long futex_wait_private(const std::uint32_t* const address, const std::uint32_t value) noexcept
{
    return ::syscall(
        SYS_futex,
        const_cast<std::uint32_t*>(address),
        FUTEX_WAIT_PRIVATE, value,
        nullptr, nullptr, 0);
}

long futex_wake_private(const std::uint32_t* const address, const int waiter_count) noexcept
{
    return ::syscall(
        SYS_futex,
        const_cast<std::uint32_t*>(address),
        FUTEX_WAKE_PRIVATE, waiter_count,
        nullptr, nullptr, 0);
}

#endif

}   //  namespace internal

//==============================================================================
//  Atomic wait word primitive implementations
//==============================================================================

#if MV_PLATFORM_WINDOWS

void wait_while_equal(const std::atomic<std::uint32_t>& word, const std::uint32_t expected) noexcept
{
    //  WaitOnAddress performs the compare-and-park operation. Failure,
    //  mismatch, and spurious return are normal at this abstraction level.
    (void)WaitOnAddress(
        internal::wait_address_from_word(word),
        static_cast<void*>(const_cast<std::uint32_t*>(&expected)),
        sizeof(expected), INFINITE);
}

void wake_one_waiter(const std::atomic<std::uint32_t>& word) noexcept
{
    WakeByAddressSingle(internal::wait_address_from_word(word));
}

void wake_all_waiters(const std::atomic<std::uint32_t>& word) noexcept
{
    WakeByAddressAll(internal::wait_address_from_word(word));
}

#elif MV_PLATFORM_LINUX || MV_PLATFORM_ANDROID

void wait_while_equal(const std::atomic<std::uint32_t>& word, const std::uint32_t expected) noexcept
{
    const std::uint32_t* const address = internal::futex_address_from_word(word);

    (void)internal::futex_wait_private(address, expected);
}

void wake_one_waiter(const std::atomic<std::uint32_t>& word) noexcept
{
    const std::uint32_t* const address = internal::futex_address_from_word(word);

    (void)internal::futex_wake_private(address, 1);
}

void wake_all_waiters(const std::atomic<std::uint32_t>& word) noexcept
{
    const std::uint32_t* const address = internal::futex_address_from_word(word);

    (void)internal::futex_wake_private(address, INT_MAX);
}

#if MV_PLATFORM_ANDROID
#if defined(_MSC_VER)
#pragma message("warning: Android wait-word support is experimental and currently uses the Linux futex implementation path.")
#else
#warning "Android wait-word support is experimental and currently uses the Linux futex implementation path."
#endif
#endif

#elif MV_PLATFORM_MAC_OS

void wait_while_equal(const std::atomic<std::uint32_t>& word, const std::uint32_t expected) noexcept
{
    //  The wait may return for wake, mismatch, interruption, or error.
    //  Callers must re-check their controlling predicate.
    (void)os_sync_wait_on_address(
        internal::wait_address_from_word(word),
        static_cast<std::uint64_t>(expected),
        sizeof(std::uint32_t),
        OS_SYNC_WAIT_ON_ADDRESS_NONE);
}

void wake_one_waiter(const std::atomic<std::uint32_t>& word) noexcept
{
    (void)os_sync_wake_by_address_any(
        internal::wait_address_from_word(word),
        sizeof(std::uint32_t),
        OS_SYNC_WAKE_BY_ADDRESS_NONE);
}

void wake_all_waiters(const std::atomic<std::uint32_t>& word) noexcept
{
    (void)os_sync_wake_by_address_all(
        internal::wait_address_from_word(word),
        sizeof(std::uint32_t),
        OS_SYNC_WAKE_BY_ADDRESS_NONE);
}

#else

void wait_while_equal(const std::atomic<std::uint32_t>& word, const std::uint32_t expected) noexcept
{
    (void)word;
    (void)expected;
}

void wake_one_waiter(const std::atomic<std::uint32_t>& word) noexcept
{
    (void)word;
}

void wake_all_waiters(const std::atomic<std::uint32_t>& word) noexcept
{
    (void)word;
}

#error "platform::threading wait-word is not implemented for this platform."

#endif

//==============================================================================
//  Atomic wait word predicate implementations
//==============================================================================

void wait_until_equal(const std::atomic<std::uint32_t>& word, const std::uint32_t value) noexcept
{
    for (;;)
    {
        const std::uint32_t seen = word.load(std::memory_order_acquire);

        if (seen == value)
        {
            break;
        }

        platform::threading::wait_while_equal(word, seen);
    }
}

std::uint32_t wait_until_not_equal(const std::atomic<std::uint32_t>& word, const std::uint32_t value) noexcept
{
    for (;;)
    {
        const std::uint32_t seen = word.load(std::memory_order_acquire);

        if (seen != value)
        {
            return seen;
        }

        platform::threading::wait_while_equal(word, value);
    }
}

}   //  namespace platform::threading
