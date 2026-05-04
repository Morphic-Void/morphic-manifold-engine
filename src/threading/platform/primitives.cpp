
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
//  Layer 0 threading primitives.

#include <cstdint>      //  std::uint8_t, std::uint32_t, std::uint64_t
#include <cstddef>      //  std::size_t

#include "threading/platform/primitives.hpp"

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#elif defined(__APPLE__) && defined(__MACH__)
    #include <stddef.h>
    #include <pthread.h>
    #include <sys/sysctl.h>
#elif defined(__linux__) || defined(__ANDROID__)
    #include <pthread.h>
    #include <sched.h>
    #include <sys/syscall.h>
    #include <unistd.h>
#endif

#if (defined(__APPLE__) && defined(__MACH__)) || defined(__linux__) || defined(__ANDROID__)
    #define THREADING_PLATFORM_HAS_PTHREADS 1
#endif

namespace threading::platform
{

namespace internal
{

//==============================================================================
//  Native supported thread count query implementation helpers
//==============================================================================

#if defined(_WIN32)

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

#elif defined(__APPLE__) && defined(__MACH__)

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

#elif defined(__linux__) || defined(__ANDROID__)

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

#if defined(_WIN32)

CPlatformThreadId query_current_thread_id_windows() noexcept
{
    const DWORD id = GetCurrentThreadId();

    return CPlatformThreadId{ static_cast<std::uint64_t>(id) };
}

#elif defined(__APPLE__) && defined(__MACH__)

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

#elif defined(__linux__) || defined(__ANDROID__)

CPlatformThreadId query_current_thread_id_linux_like() noexcept
{
    const long id = syscall(SYS_gettid);

    if (id > 0)
    {
        return CPlatformThreadId{ static_cast<std::uint64_t>(id) };
    }

    return CPlatformThreadId{ 0u };
}

#endif

//==============================================================================
//  Native exclusive locking implementation helpers
//==============================================================================

#if defined(_WIN32)

using native_exclusive_lock_type = SRWLOCK;

static_assert((sizeof(native_exclusive_lock_type) <= sizeof(CExclusiveLock::storage)),
    "CExclusiveLock storage is too small for SRWLOCK.");

static_assert((alignof(CExclusiveLock) >= alignof(native_exclusive_lock_type)),
    "CExclusiveLock alignment is too small for SRWLOCK.");

native_exclusive_lock_type* native_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    return reinterpret_cast<native_exclusive_lock_type*>(lock->storage);
}

#elif defined(THREADING_PLATFORM_HAS_PTHREADS)

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
#if defined(_WIN32)

    return internal::query_hardware_thread_count_windows();

#elif defined(__APPLE__) && defined(__MACH__)

    return internal::query_hardware_thread_count_macos();

#elif defined(__linux__) || defined(__ANDROID__)

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
#if defined(_WIN32)

    return internal::query_current_thread_id_windows();

#elif defined(__APPLE__) && defined(__MACH__)

    return internal::query_current_thread_id_macos();

#elif defined(__linux__) || defined(__ANDROID__)

    return internal::query_current_thread_id_linux_like();

#else

    return CPlatformThreadId{ 0u };

#endif
}

//==============================================================================
//  Native exclusive locking implementation
//==============================================================================

bool initialise_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    if ((lock == nullptr) || lock->is_valid)
    {
        return false;
    }

    clear_exclusive_lock(lock);

#if defined(_WIN32)

    InitializeSRWLock(internal::native_exclusive_lock(lock));
    lock->is_valid = true;
    return true;

#elif defined(THREADING_PLATFORM_HAS_PTHREADS)

    const int result = pthread_mutex_init(internal::native_exclusive_lock(lock), nullptr);

    if (result != 0)
    {
        clear_exclusive_lock(lock);
        return false;
    }

    lock->is_valid = true;
    return true;

#else

    return false;

#endif
}

void destroy_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    if ((lock == nullptr) || !lock->is_valid)
    {
        return;
    }

#if defined(_WIN32)

    //  SRWLOCK has no explicit destruction operation.

#elif defined(THREADING_PLATFORM_HAS_PTHREADS)

    //  Failure indicates misuse under this primitive's contract.
    (void)pthread_mutex_destroy(internal::native_exclusive_lock(lock));

#endif

    clear_exclusive_lock(lock);
}

void acquire_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    if ((lock == nullptr) || !lock->is_valid)
    {
        return;
    }

#if defined(_WIN32)

    AcquireSRWLockExclusive(internal::native_exclusive_lock(lock));

#elif defined(THREADING_PLATFORM_HAS_PTHREADS)

    //  Failure indicates misuse or platform failure under this contract.
    (void)pthread_mutex_lock(internal::native_exclusive_lock(lock));

#endif
}

void release_exclusive_lock(CExclusiveLock* const lock) noexcept
{
    if ((lock == nullptr) || !lock->is_valid)
    {
        return;
    }

#if defined(_WIN32)

    ReleaseSRWLockExclusive(internal::native_exclusive_lock(lock));

#elif defined(THREADING_PLATFORM_HAS_PTHREADS)

    //  Failure indicates misuse under this contract.
    (void)pthread_mutex_unlock(internal::native_exclusive_lock(lock));

#endif
}

}   //  namespace threading::platform
