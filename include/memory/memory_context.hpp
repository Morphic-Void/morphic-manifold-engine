
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   memory_context.hpp
//  Author: Ritchie Brannan
//  Date:   12 Jul 26
//
//  Allocation routing, attribution accounting, and ambient memory context.

#pragma once

#ifndef MEMORY_CONTEXT_HPP_INCLUDED
#define MEMORY_CONTEXT_HPP_INCLUDED

#include <atomic>       //  std::atomic
#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::uint32_t, std::uint64_t, std::uintptr_t
#include <limits>       //  std::numeric_limits

#include "memory_policies.hpp"
#include "bit_utils/bit_ops.hpp"
#include "system/system_context.hpp"
#include "platform/platform_defines.hpp"
#include "debug/macros.hpp"

namespace memory
{

class CMemoryContext;

//==============================================================================
//  Ambient memory context
//==============================================================================

[[nodiscard]] CMemoryContext* get_ambient_memory_context() noexcept;
CMemoryContext* set_module_memory_context(CMemoryContext* context = nullptr) noexcept;
CMemoryContext* set_thread_memory_context(CMemoryContext* context = nullptr) noexcept;

//==============================================================================
//  Allocator callback interface
//==============================================================================

using FAllocate = void* (MV_STD_ABI_CALL*)(void* const context, const std::size_t align, const std::size_t bytes) noexcept;
using FDeallocate = bool (MV_STD_ABI_CALL*)(void* const context, const std::size_t align, void* const ptr) noexcept;

//==============================================================================
//  CMemoryAllocator
//==============================================================================

class CMemoryAllocator
{
public:
    CMemoryAllocator(void* context, FAllocate allocate, FDeallocate deallocate,
        system_ids::id_type system_id = system_context::get_ambient_system_id()) noexcept;

    CMemoryAllocator(const CMemoryAllocator&) = delete;
    CMemoryAllocator& operator=(const CMemoryAllocator&) = delete;
    CMemoryAllocator(CMemoryAllocator&&) = delete;
    CMemoryAllocator& operator=(CMemoryAllocator&&) = delete;
    ~CMemoryAllocator() noexcept = default;

    [[nodiscard]] bool is_usable() const noexcept { return (m_allocate != nullptr) && (m_deallocate != nullptr); }
    [[nodiscard]] system_ids::id_type get_system_id() const noexcept { return m_system_id; }

private:
    [[nodiscard]] void* allocate(std::size_t conditioned_alignment, std::size_t conditioned_bytes) noexcept;
    [[nodiscard]] bool deallocate(std::size_t conditioned_alignment, void* ptr) noexcept;

    friend class CMemoryContext;

    system_ids::id_type m_system_id{};
    void*       m_context{ nullptr };
    FAllocate   m_allocate{ nullptr };
    FDeallocate m_deallocate{ nullptr };
};

//==============================================================================
//  CMemoryContext
//==============================================================================

class CMemoryContext
{
public:
    explicit CMemoryContext(CMemoryAllocator& allocator,
        system_ids::id_type system_id = system_context::get_ambient_system_id()) noexcept;

    CMemoryContext(const CMemoryContext&) = delete;
    CMemoryContext& operator=(const CMemoryContext&) = delete;
    CMemoryContext(CMemoryContext&&) = delete;
    CMemoryContext& operator=(CMemoryContext&&) = delete;
    ~CMemoryContext() noexcept;

    [[nodiscard]] bool is_usable() const noexcept { return m_allocator.is_usable(); }
    [[nodiscard]] bool is_compatible_with(const CMemoryContext& other) const noexcept { return &m_allocator == &other.m_allocator; }
    [[nodiscard]] bool belongs_to_module(module_ids::id_type module_id) const noexcept;
    [[nodiscard]] bool is_attribution_empty() const noexcept;
    [[nodiscard]] const CMemoryAllocator& get_allocator() const noexcept { return m_allocator; }
    [[nodiscard]] system_ids::id_type get_system_id() const noexcept { return m_system_id; }

    [[nodiscard]] std::uint32_t get_live_allocation_count() const noexcept;
    [[nodiscard]] std::uint64_t get_live_allocated_bytes() const noexcept;
    [[nodiscard]] std::size_t condition_alignment(std::size_t requested_alignment) const noexcept;
    [[nodiscard]] std::size_t condition_bytes(std::size_t conditioned_alignment, std::size_t requested_bytes) const noexcept;

    [[nodiscard]] void* allocate(std::size_t requested_alignment, std::size_t requested_bytes) noexcept;
    void deallocate(std::size_t requested_alignment, std::size_t requested_bytes, void* ptr) noexcept;

private:
    [[nodiscard]] static bool validate(std::size_t align, std::size_t bytes) noexcept;
    [[nodiscard]] static bool validate(std::size_t align, std::size_t bytes, const void* ptr) noexcept;
    [[nodiscard]] bool add(std::size_t allocation_count, std::uint64_t bytes) noexcept;
    [[nodiscard]] bool sub(std::size_t allocation_count, std::uint64_t bytes) noexcept;

    friend bool reattribute(CMemoryContext& from, CMemoryContext& to,
        std::size_t allocation_count, std::uint64_t bytes) noexcept;

    CMemoryAllocator&          m_allocator;
    system_ids::id_type        m_system_id{};
    std::atomic<std::uint32_t> m_live_allocations{ 0u };
    std::atomic<std::uint64_t> m_live_allocated_bytes{ 0u };
};

[[nodiscard]] bool reattribute(CMemoryContext& from, CMemoryContext& to,
    std::size_t allocation_count, std::uint64_t bytes) noexcept;

//==============================================================================
//  CMemoryAllocator implementation
//==============================================================================

inline CMemoryAllocator::CMemoryAllocator(
    void* const context,
    const FAllocate allocate,
    const FDeallocate deallocate,
    const system_ids::id_type system_id) noexcept
    : m_system_id(system_id)
    , m_context(context)
    , m_allocate(allocate)
    , m_deallocate(deallocate)
{
}

inline void* CMemoryAllocator::allocate(
    const std::size_t conditioned_alignment,
    const std::size_t conditioned_bytes) noexcept
{
    if ((m_allocate == nullptr) ||
        (memory::condition_alignment(conditioned_alignment) != conditioned_alignment) ||
        (memory::condition_bytes(conditioned_alignment, conditioned_bytes) != conditioned_bytes))
    {
        MV_ERROR("CMemoryAllocator::allocate received an invalid conditioned allocation request");
        return nullptr;
    }

    return m_allocate(m_context, conditioned_alignment, conditioned_bytes);
}

inline bool CMemoryAllocator::deallocate(const std::size_t conditioned_alignment, void* const ptr) noexcept
{
    if ((m_deallocate == nullptr) ||
        (memory::condition_alignment(conditioned_alignment) != conditioned_alignment) ||
        (ptr == nullptr))
    {
        MV_ERROR("CMemoryAllocator::deallocate received an invalid conditioned deallocation request");
        return false;
    }

    if (!m_deallocate(m_context, conditioned_alignment, ptr))
    {
        MV_ERROR("CMemoryAllocator::deallocate failed");
        return false;
    }
    return true;
}

//==============================================================================
//  CMemoryContext implementation
//==============================================================================

inline CMemoryContext::CMemoryContext(CMemoryAllocator& allocator, const system_ids::id_type system_id) noexcept
    : m_allocator(allocator)
    , m_system_id(system_id)
{
}

inline CMemoryContext::~CMemoryContext() noexcept
{
    if (!is_attribution_empty())
    {
        MV_ERROR("CMemoryContext was destroyed with live allocations still recorded");
    }
}

inline std::uint32_t CMemoryContext::get_live_allocation_count() const noexcept
{
    return m_live_allocations.load(std::memory_order_relaxed);
}

inline std::uint64_t CMemoryContext::get_live_allocated_bytes() const noexcept
{
    return m_live_allocated_bytes.load(std::memory_order_relaxed);
}

inline bool CMemoryContext::belongs_to_module(const module_ids::id_type module_id) const noexcept
{
    return module_ids::is_valid_id(module_id) && system_ids::is_valid_id(m_system_id) &&
        (system_ids::get_module_id(m_system_id) == module_id);
}

inline bool CMemoryContext::is_attribution_empty() const noexcept
{
    return (get_live_allocation_count() == 0u) && (get_live_allocated_bytes() == 0u);
}

inline std::size_t CMemoryContext::condition_alignment(const std::size_t requested_alignment) const noexcept
{
    return memory::condition_alignment(requested_alignment);
}

inline std::size_t CMemoryContext::condition_bytes(
    const std::size_t conditioned_alignment,
    const std::size_t requested_bytes) const noexcept
{
    return memory::condition_bytes(conditioned_alignment, requested_bytes);
}

inline bool CMemoryContext::validate(const std::size_t align, const std::size_t bytes) noexcept
{
    return bit_ops::is_pow2(align) && memory::in_non_empty_range(bytes, memory::k_byte_size_ceiling);
}

inline bool CMemoryContext::validate(const std::size_t align, const std::size_t bytes, const void* const ptr) noexcept
{
    return validate(align, bytes) && (ptr != nullptr) && ((reinterpret_cast<std::uintptr_t>(ptr) & (align - 1u)) == 0u);
}

inline bool CMemoryContext::add(const std::size_t allocation_count, const std::uint64_t bytes) noexcept
{
    if ((allocation_count == 0u) || (allocation_count > std::numeric_limits<std::uint32_t>::max()) || (bytes == 0u))
    {
        return false;
    }

    const std::uint32_t count = static_cast<std::uint32_t>(allocation_count);
    std::uint32_t allocations = m_live_allocations.load(std::memory_order_relaxed);
    while (count <= (std::numeric_limits<std::uint32_t>::max() - allocations))
    {
        if (m_live_allocations.compare_exchange_weak(allocations, (allocations + count), std::memory_order_relaxed))
        {
            std::uint64_t allocated_bytes = m_live_allocated_bytes.load(std::memory_order_relaxed);
            while (bytes <= (std::numeric_limits<std::uint64_t>::max() - allocated_bytes))
            {
                if (m_live_allocated_bytes.compare_exchange_weak(allocated_bytes, (allocated_bytes + bytes), std::memory_order_relaxed))
                {
                    return true;
                }
            }
            m_live_allocations.fetch_sub(count, std::memory_order_relaxed);
            return false;
        }
    }
    return false;
}

inline bool CMemoryContext::sub(const std::size_t allocation_count, const std::uint64_t bytes) noexcept
{
    if ((allocation_count == 0u) || (allocation_count > std::numeric_limits<std::uint32_t>::max()) || (bytes == 0u))
    {
        return false;
    }

    const std::uint32_t count = static_cast<std::uint32_t>(allocation_count);
    std::uint32_t allocations = m_live_allocations.load(std::memory_order_relaxed);
    while (count <= allocations)
    {
        if (m_live_allocations.compare_exchange_weak(allocations, (allocations - count), std::memory_order_relaxed))
        {
            std::uint64_t allocated_bytes = m_live_allocated_bytes.load(std::memory_order_relaxed);
            while (bytes <= allocated_bytes)
            {
                if (m_live_allocated_bytes.compare_exchange_weak(allocated_bytes, (allocated_bytes - bytes), std::memory_order_relaxed))
                {
                    return true;
                }
            }
            m_live_allocations.fetch_add(count, std::memory_order_relaxed);
            return false;
        }
    }
    return false;
}

inline void* CMemoryContext::allocate(
    const std::size_t requested_alignment,
    const std::size_t requested_bytes) noexcept
{
    const std::size_t conditioned_alignment = condition_alignment(requested_alignment);
    const std::size_t conditioned_bytes = condition_bytes(conditioned_alignment, requested_bytes);
    if (!validate(conditioned_alignment, conditioned_bytes))
    {
        MV_ERROR("CMemoryContext::allocate received an invalid allocation request");
        return nullptr;
    }

    if (!add(1u, conditioned_bytes))
    {
        MV_ERROR("CMemoryContext::allocate failed to add allocation accounting");
        return nullptr;
    }

    void* const ptr = m_allocator.allocate(conditioned_alignment, conditioned_bytes);
    if (ptr == nullptr)
    {
        if (!sub(1u, conditioned_bytes))
        {
            MV_ERROR("CMemoryContext::allocate failed to roll back allocation accounting");
        }
        return nullptr;
    }

    if ((reinterpret_cast<std::uintptr_t>(ptr) & (conditioned_alignment - 1u)) != 0u)
    {
        MV_ERROR("CMemoryContext::allocate returned a misaligned pointer");
        (void)m_allocator.deallocate(conditioned_alignment, ptr);
        if (!sub(1u, conditioned_bytes))
        {
            MV_ERROR("CMemoryContext::allocate failed to roll back allocation accounting after misalignment");
        }
        return nullptr;
    }
    return ptr;
}

inline void CMemoryContext::deallocate(
    const std::size_t requested_alignment,
    const std::size_t requested_bytes,
    void* const ptr) noexcept
{
    const std::size_t conditioned_alignment = condition_alignment(requested_alignment);
    const std::size_t conditioned_bytes = condition_bytes(conditioned_alignment, requested_bytes);
    if (!validate(conditioned_alignment, conditioned_bytes, ptr))
    {
        MV_ERROR("CMemoryContext::deallocate received an invalid deallocation request");
        return;
    }

    if (!sub(1u, conditioned_bytes))
    {
        MV_ERROR("CMemoryContext::deallocate failed to subtract allocation accounting");
        return;
    }
    if (!m_allocator.deallocate(conditioned_alignment, ptr))
    {
        if (!add(1u, conditioned_bytes))
        {
            MV_ERROR("CMemoryContext::deallocate failed to restore allocation accounting after allocator failure");
        }
    }
}

inline bool reattribute(
    CMemoryContext& from,
    CMemoryContext& to,
    const std::size_t allocation_count,
    const std::uint64_t bytes) noexcept
{
    if (&from == &to)
    {
        return true;
    }
    if (!from.is_compatible_with(to) ||
        (allocation_count == 0u) || (bytes == 0u))
    {
        MV_ERROR("memory::reattribute received an invalid attribution transfer request");
        return false;
    }
    if (!to.add(allocation_count, bytes))
    {
        return false;
    }
    if (!from.sub(allocation_count, bytes))
    {
        MV_ERROR("memory::reattribute failed to remove accounting from the source context");
        MV_ASSERT(to.sub(allocation_count, bytes));
        return false;
    }
    return true;
}

}   //  namespace memory

#endif  //  #ifndef MEMORY_CONTEXT_HPP_INCLUDED
