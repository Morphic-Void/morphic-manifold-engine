
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   primitives.cpp
//  Author: Ritchie Brannan
//  Date:   4 May 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Base level platform threading support primitives.

#include <atomic>       //  std::atomic
#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::uint8_t, std::uint32_t, std::uint64_t

#include "platform/threading/primitives.hpp"

//==============================================================================
//  Platform support defines
//==============================================================================

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #define THREADING_PLATFORM_WINDOWS 1
#endif

#if (defined(__APPLE__) && defined(__MACH__))
    #define THREADING_PLATFORM_APPLE 1
#endif

#if defined(__linux__) && !defined(__ANDROID__)
    #define THREADING_PLATFORM_LINUX_ONLY 1
#endif

#if defined(__ANDROID__)
    #define THREADING_PLATFORM_ANDROID_ONLY 1
#endif

#if defined(__linux__) || defined(__ANDROID__)
    #define THREADING_PLATFORM_LINUX_ANDROID 1
#endif

#if defined(THREADING_PLATFORM_APPLE) || defined(THREADING_PLATFORM_LINUX_ANDROID)
    #define THREADING_PLATFORM_PLATFORM_HAS_PTHREADS 1
#endif

//==============================================================================
//  Platform includes
//==============================================================================

#if defined(THREADING_PLATFORM_WINDOWS)
    #include <windows.h>
#endif

#if defined(THREADING_PLATFORM_APPLE)
    #include <stddef.h>
    #include <pthread.h>
    #include <sys/sysctl.h>
#endif

#if defined(THREADING_PLATFORM_LINUX_ONLY)
    #include <limits.h>
    #include <linux/futex.h>
#endif

#if defined(THREADING_PLATFORM_LINUX_ANDROID)
    #include <pthread.h>
    #include <sched.h>
    #include <sys/syscall.h>
    #include <unistd.h>
#endif

//==============================================================================
//  Implementation
//==============================================================================

namespace platform::threading
{

namespace internal
{

//==============================================================================
//  Native supported thread count query implementation helpers
//==============================================================================

#if defined(THREADING_PLATFORM_WINDOWS)

std::uint32_t query_hardware_thread_count_windows() noexcept
{
    DWORD dwCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

    if (dwCount == 0u)
    {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        dwCount = info.dwNumberOfProcessors;
    }

    return (dwCount != 0u) ? static_cast<std::uint32_t>(dwCount) : 1u;
}

#elif defined(THREADING_PLATFORM_APPLE)

std::uint32_t query_hardware_thread_count_macos() noexcept
{
    static const char* const k_query_names[3] = { "hw.activecpu", "hw.logicalcpu", "hw.ncpu" };

    for (int i = 0; i < 3; ++i)
    {   //  only supporting 32-bit return values
        std::uint32_t count = 0u;
        std::size_t size = sizeof(count);
        if ((sysctlbyname(k_query_names[i], &count, &size, nullptr, 0) == 0) && (size == sizeof(count)) && (count != 0u))
        {
            return count;
        }
    }

    return 1u;
}

#elif defined(THREADING_PLATFORM_LINUX_ANDROID)

std::uint32_t count_cpu_set(const cpu_set_t& set) noexcept
{
    std::uint32_t count = 0u;

    for (int i = 0; i < CPU_SETSIZE; ++i)
    {
        if (CPU_ISSET(i, &set))
        {
            ++count;
        }
    }

    return count;
}

std::uint32_t query_hardware_thread_count_linux_like() noexcept
{
    cpu_set_t set;
    CPU_ZERO(&set);

    if (sched_getaffinity(0, sizeof(set), &set) == 0)
    {
        const std::uint32_t affinity_count = count_cpu_set(set);
        if (affinity_count != 0u)
        {
            return affinity_count;
        }
    }

    const long online_count = sysconf(_SC_NPROCESSORS_ONLN);

    return (online_count > 0) ? static_cast<std::uint32_t>(online_count) : 1u;
}

#endif

//==============================================================================
//  Native thread identifier query implementation helpers
//==============================================================================

#if defined(THREADING_PLATFORM_WINDOWS)

CPlatformThreadId query_current_thread_id_windows() noexcept
{
    const DWORD id = GetCurrentThreadId();

    return CPlatformThreadId{ static_cast<std::uint64_t>(id) };
}

#elif defined(THREADING_PLATFORM_APPLE)

CPlatformThreadId query_current_thread_id_macos() noexcept
{
    std::uint64_t id = 0u;

    const int result = pthread_threadid_np(nullptr, &id);

    if ((result == 0) && (id != 0u))
    {
        return CPlatformThreadId{ id };
    }

    return CPlatformThreadId{ 0u };
}

#elif defined(THREADING_PLATFORM_LINUX_ANDROID)

CPlatformThreadId query_current_thread_id_linux_like() noexcept
{
    const long id = ::syscall(SYS_gettid);

    if (id > 0)
    {
        return CPlatformThreadId{ static_cast<std::uint64_t>(id) };
    }

    return CPlatformThreadId{ 0u };
}

#endif

//==============================================================================
//  Atomic wait word implementation helpers
//==============================================================================

#if defined(THREADING_PLATFORM_WINDOWS) || defined(THREADING_PLATFORM_LINUX_ONLY)

static_assert((sizeof(std::atomic<std::uint32_t>) == sizeof(std::uint32_t)),
    "std::atomic<std::uint32_t> must have the same storage size as std::uint32_t.");

static_assert((alignof(std::atomic<std::uint32_t>) >= alignof(std::uint32_t)),
    "std::atomic<std::uint32_t> must be naturally aligned for 32-bit waits.");

static_assert((std::atomic<std::uint32_t>::is_always_lock_free),
    "std::atomic<std::uint32_t> must be always lock-free.");

#endif

#if defined(THREADING_PLATFORM_WINDOWS)

void* wait_address_from_word(const std::atomic<std::uint32_t>* const word) noexcept
{
    const void* const address = static_cast<const void*>(word);

    return const_cast<void*>(address);
}

#elif defined(THREADING_PLATFORM_LINUX_ONLY)

const std::uint32_t* futex_address_from_word(const std::atomic<std::uint32_t>* const word) noexcept
{
    return reinterpret_cast<const std::uint32_t*>(word);
}

long futex_wait_private(const std::uint32_t* const address, const std::uint32_t expected) noexcept
{
    return ::syscall(
        SYS_futex, const_cast<std::uint32_t*>(address),
        FUTEX_WAIT_PRIVATE, expected,
        nullptr, nullptr, 0);
}

long futex_wake_private(const std::uint32_t* const address, const int waiter_count) noexcept
{
    return ::syscall(
        SYS_futex, const_cast<std::uint32_t*>(address),
        FUTEX_WAKE_PRIVATE, waiter_count,
        nullptr, nullptr, 0);
}

#elif defined(THREADING_PLATFORM_APPLE)

#error "threading::platform atomic wait word primitives are not implemented for macOS in phase 1."

#elif defined(THREADING_PLATFORM_ANDROID_ONLY)

#error "threading::platform atomic wait word primitives are not implemented for Android in phase 1."

#else

#error "threading::platform atomic wait word primitives are not implemented for this platform."

#endif

//==============================================================================
//  Native exclusive locking implementation helpers
//==============================================================================

#if defined(THREADING_PLATFORM_WINDOWS)

using native_exclusive_lock_type = SRWLOCK;

static_assert((sizeof(native_exclusive_lock_type) <= sizeof(CExclusiveLock::storage)),
    "CExclusiveLock storage is too small for SRWLOCK.");

static_assert((alignof(CExclusiveLock) >= alignof(native_exclusive_lock_type)),
    "CExclusiveLock alignment is too small for SRWLOCK.");

native_exclusive_lock_type* native_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    return reinterpret_cast<native_exclusive_lock_type*>(lock->storage);
}

#elif defined(THREADING_PLATFORM_PLATFORM_HAS_PTHREADS)

using native_exclusive_lock_type = pthread_mutex_t;

static_assert((sizeof(native_exclusive_lock_type) <= sizeof(CExclusiveLock::storage)),
    "CExclusiveLock storage is too small for pthread_mutex_t.");

static_assert((alignof(CExclusiveLock) >= alignof(native_exclusive_lock_type)),
    "CExclusiveLock alignment is too small for pthread_mutex_t.");

native_exclusive_lock_type* native_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    return reinterpret_cast<native_exclusive_lock_type*>(lock->storage);
}

#endif

}   //  namespace internal

static_assert((offsetof(CExclusiveLock, storage) == 0u),
    "CExclusiveLock native storage must be the first member.");

static void clear_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    if (lock == nullptr)
    {
        return;
    }

    for (std::uint8_t& byte : lock->storage)
    {
        byte = 0u;
    }

    lock->is_valid = false;
}

//==============================================================================
//  Native supported thread count query implementation
//==============================================================================

std::uint32_t query_hardware_thread_count() noexcept
{
#if defined(THREADING_PLATFORM_WINDOWS)

    return internal::query_hardware_thread_count_windows();

#elif defined(THREADING_PLATFORM_APPLE)

    return internal::query_hardware_thread_count_macos();

#elif defined(THREADING_PLATFORM_LINUX_ANDROID)

    return internal::query_hardware_thread_count_linux_like();

#else

    return 1u;

#endif
}

//==============================================================================
//  Native thread identifier query implementation
//==============================================================================

CPlatformThreadId query_current_thread_id() noexcept
{
#if defined(THREADING_PLATFORM_WINDOWS)

    return internal::query_current_thread_id_windows();

#elif defined(THREADING_PLATFORM_APPLE)

    return internal::query_current_thread_id_macos();

#elif defined(THREADING_PLATFORM_LINUX_ANDROID)

    return internal::query_current_thread_id_linux_like();

#else

    return CPlatformThreadId{ 0u };

#endif
}

//==============================================================================
//  Atomic wait word implementation
//==============================================================================

#if defined(THREADING_PLATFORM_WINDOWS)

void wait_while_equal(const std::atomic<std::uint32_t>* const word, const std::uint32_t expected) noexcept
{
    //  WaitOnAddress performs the compare-and-park operation. Failure,
    //  mismatch, and spurious return are normal at this abstraction level.
    (void)WaitOnAddress(
        internal::wait_address_from_word(word),
        static_cast<void*>(const_cast<std::uint32_t*>(&expected)),
        sizeof(expected), INFINITE);
}

void wake_one_waiter(const std::atomic<std::uint32_t>* const word) noexcept
{
    WakeByAddressSingle(internal::wait_address_from_word(word));
}

void wake_all_waiters(const std::atomic<std::uint32_t>* const word) noexcept
{
    WakeByAddressAll(internal::wait_address_from_word(word));
}

#elif defined(THREADING_PLATFORM_LINUX_ONLY)

void wait_while_equal(const std::atomic<std::uint32_t>* const word, const std::uint32_t expected) noexcept
{
    const std::uint32_t* const address = internal::futex_address_from_word(word);

    (void)internal::futex_wait_private(address, expected);
}

void wake_one_waiter(const std::atomic<std::uint32_t>* const word) noexcept
{
    const std::uint32_t* const address = internal::futex_address_from_word(word);

    (void)internal::futex_wake_private(address, 1);
}

void wake_all_waiters(const std::atomic<std::uint32_t>* const word) noexcept
{
    const std::uint32_t* const address = internal::futex_address_from_word(word);

    (void)internal::futex_wake_private(address, INT_MAX);
}

#elif defined(THREADING_PLATFORM_APPLE)

#error "threading::platform atomic wait word primitives are not implemented for macOS in phase 1."

#elif defined(THREADING_PLATFORM_ANDROID_ONLY)

#error "threading::platform atomic wait word primitives are not implemented for Android in phase 1."

#else

#error "threading::platform atomic wait word primitives are not implemented for this platform."

#endif

//==============================================================================
//  Native exclusive locking implementation
//==============================================================================

#if defined(THREADING_PLATFORM_WINDOWS)

bool initialise_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    if ((lock == nullptr) || lock->is_valid)
    {
        return false;
    }
    clear_exclusive_lock(lock);
    InitializeSRWLock(internal::native_exclusive_lock(lock));
    lock->is_valid = true;
    return true;
}

void destroy_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    if ((lock == nullptr) || !lock->is_valid)
    {
        return;
    }

    //  SRWLOCK has no explicit destruction operation.

    clear_exclusive_lock(lock);
}

void acquire_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    if ((lock == nullptr) || !lock->is_valid)
    {
        return;
    }
    AcquireSRWLockExclusive(internal::native_exclusive_lock(lock));
}

void release_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    if ((lock == nullptr) || !lock->is_valid)
    {
        return;
    }
    ReleaseSRWLockExclusive(internal::native_exclusive_lock(lock));
}

#elif defined(THREADING_PLATFORM_PLATFORM_HAS_PTHREADS)

bool initialise_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    if ((lock == nullptr) || lock->is_valid)
    {
        return false;
    }
    clear_exclusive_lock(lock);
    const int result = ::pthread_mutex_init(internal::native_exclusive_lock(lock), nullptr);
    if (result != 0)
    {
        clear_exclusive_lock(lock);
        return false;
    }
    lock->is_valid = true;
    return true;
}

void destroy_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    if ((lock == nullptr) || !lock->is_valid)
    {
        return;
    }

    //  Failure indicates misuse under this primitive's contract.
    (void)pthread_mutex_destroy(internal::native_exclusive_lock(lock));

    clear_exclusive_lock(lock);
}

void acquire_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    if ((lock == nullptr) || !lock->is_valid)
    {
        return;
    }

    //  Failure indicates misuse or platform failure under this contract.
    (void)pthread_mutex_lock(internal::native_exclusive_lock(lock));
}

void release_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    if ((lock == nullptr) || !lock->is_valid)
    {
        return;
    }

    //  Failure indicates misuse under this contract.
    (void)pthread_mutex_unlock(internal::native_exclusive_lock(lock));
}

#endif

}   //  namespace platform::threading
