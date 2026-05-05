//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   memory_allocation.cpp
//  Author: Ritchie Brannan
//  Date:   15 Apr 26
//
//  The memory substrate (C++17, noexcept).
//
//  Purpose
//  -------
//  Provides the shared foundational memory layer.
//
//  Scope
//  -----
//  This file is strictly mechanical.
//
//  Design Constraints
//  ------------------
//  - Requires C++17 or greater.
//  - No exceptions (allocation uses nothrow new[]).
//  - 32-bit element domain cap:
//      memory::k_max_elements = 0x80000000u
//
//  Higher-level structures build semantic behavior on top of the
//  primitives defined here.

#include <atomic>       //  std::atomic
#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::uint32_t
#include <mutex>        //  std::mutex, std::lock_guard
#include <new>          //  std::align_val_t, std::nothrow, aligned operator new[]/delete[]
#include <thread>       //  std::this_thread::yield

#include "memory/memory_allocation.hpp"
#include "system/system_ids.hpp"
#include "debug/debug.hpp"

namespace memory
{

//==============================================================================
//  Allocation configuration
//==============================================================================

static std::atomic<IAllocator*> s_allocator{ nullptr };
static std::atomic<bool>        s_allocation_is_enabled{ true };

//  Reconfiguration gate:
//  - true  : allocator configuration is being changed
//  - false : normal allocation/deallocation traffic may enter
static std::atomic<bool> s_allocator_gate_is_closed{ false };

//  Number of threads currently inside the protected allocation/deallocation
//  dispatch region. This is used to establish a quiescent point before
//  allocator replacement.
static std::atomic<std::uint32_t> s_active_calls{ 0u };

//  Number of live allocations plus in-flight allocation attempts that have
//  reserved ownership within the current allocator regime. Allocator
//  replacement is rejected while this is non-zero.
static std::atomic<std::uint32_t> s_live_allocations{ 0u };

//==============================================================================
//  Allocation call guard
//==============================================================================

namespace allocation
{

class CCallGuard
{
public:
    CCallGuard() noexcept
        : m_is_active(false)
    {
        for (;;)
        {
            while (s_allocator_gate_is_closed.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            s_active_calls.fetch_add(1u, std::memory_order_acq_rel);

            if (!s_allocator_gate_is_closed.load(std::memory_order_acquire))
            {
                m_is_active = true;
                break;
            }

            s_active_calls.fetch_sub(1u, std::memory_order_acq_rel);
        }
    }

    ~CCallGuard() noexcept
    {
        if (m_is_active)
        {
            s_active_calls.fetch_sub(1u, std::memory_order_acq_rel);
        }
    }

    CCallGuard(const CCallGuard&) = delete;
    CCallGuard& operator=(const CCallGuard&) = delete;

private:
    bool m_is_active;
};

}   //  namespace allocation

bool set_allocator(IAllocator* allocator) noexcept
{
    static std::mutex s_mutex;
    std::lock_guard<std::mutex> lock(s_mutex);

    bool success = false;

    s_allocator_gate_is_closed.store(true, std::memory_order_release);

    while (s_active_calls.load(std::memory_order_acquire) != 0u)
    {
        std::this_thread::yield();
    }

    if (s_live_allocations.load(std::memory_order_acquire) == 0u)
    {
        s_allocator.store(allocator, std::memory_order_release);

        success = true;
    }

    s_allocator_gate_is_closed.store(false, std::memory_order_release);

    return success;
}

bool enable_allocation(const bool enable) noexcept
{
    return s_allocation_is_enabled.exchange(enable, std::memory_order_relaxed);
}

//==============================================================================
//  Byte memory allocation and deallocation
//==============================================================================

void* byte_allocate(const std::size_t bytes, const std::size_t align) noexcept
{
    if (bytes == 0u)
    {   //  zero size allocations are rejected
        return nullptr;
    }

    allocation::CCallGuard call_guard;

    if (!s_allocation_is_enabled.load(std::memory_order_relaxed))
    {
        return nullptr;
    }

    IAllocator* allocator = s_allocator.load(std::memory_order_acquire);

    s_live_allocations.fetch_add(1u, std::memory_order_acq_rel);

    void* ptr = nullptr;

    if (MV_FAIL_SAFE_ASSERT(allocator != nullptr))
    {
        ptr = allocator->byte_allocate(bytes, align);
    }

    if (ptr == nullptr)
    {
        s_live_allocations.fetch_sub(1u, std::memory_order_acq_rel);
    }

    return ptr;
}

void byte_deallocate(void* const ptr, const std::size_t align) noexcept
{
    if (ptr == nullptr)
    {   //  note: the null check is not strictly required
        return;
    }

    allocation::CCallGuard call_guard;

    IAllocator* allocator = s_allocator.load(std::memory_order_acquire);

    if (MV_FAIL_SAFE_ASSERT(allocator != nullptr))
    {
        allocator->byte_deallocate(ptr, align);
    }

    s_live_allocations.fetch_sub(1u, std::memory_order_acq_rel);
}

//==============================================================================
//  Default system allocator
//==============================================================================

class CSystemAllocator : public IAllocator
{
public:
    CSystemAllocator() = default;
    ~CSystemAllocator() = default;

    virtual void* byte_allocate(const std::size_t bytes, const std::size_t align) noexcept override final
    {
        return ::operator new[](bytes, alignment_policy(align), std::nothrow);
    }

    virtual void byte_deallocate(void* const ptr, const std::size_t align) noexcept override final
    {
        if (ptr != nullptr)
        {
            ::operator delete[](ptr, alignment_policy(align));
        }
    }
};

bool install_system_allocator(const std::size_t system_id) noexcept
{
    static CSystemAllocator allocator;
    bool success = false;
    if ((system_ids::get_module_id(system_id) == module_ids::executable) &&
        (system_ids::get_thread_id(system_id) == thread_ids::host))
    {
        success = set_allocator(&allocator);
    }
    MV_HARD_ASSERT(success);
    return success;
}

}   //  namespace memory