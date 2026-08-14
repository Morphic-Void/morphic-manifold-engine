
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    erased_owner.hpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    26 Jul 26
//
//  Move-only ownership for one registered, type-erased SYSTEM payload.
//  The stored identity uses the category-bearing erased-carrier representation;
//  construction and destruction remain closed over SYSTEM payload definitions.

#pragma once

#ifndef ERASED_OWNER_HPP_INCLUDED
#define ERASED_OWNER_HPP_INCLUDED

#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::uint32_t
#include <new>          //  ::new
#include <type_traits>  //  std::is_nothrow_*_v
#include <utility>      //  std::move

#include "debug/macros.hpp"
#include "memory/memory_token.hpp"
#include "system/system_type_registration.hpp"

class CErasedOwner
{
public:
    CErasedOwner() noexcept = default;
    CErasedOwner(const CErasedOwner&) = delete;
    CErasedOwner& operator=(const CErasedOwner&) = delete;
    CErasedOwner(CErasedOwner&& other) noexcept;
    CErasedOwner& operator=(CErasedOwner&& other) noexcept;
    ~CErasedOwner() noexcept;

    [[nodiscard]] bool is_empty() const noexcept { return !m_type_id.is_valid(); }
    [[nodiscard]] bool is_ready() const noexcept { return !is_empty() && (m_storage.data() != nullptr); }
    [[nodiscard]] explicit operator bool() const noexcept { return is_ready(); }

    [[nodiscard]] type_id query_type_id() const noexcept { return m_type_id; }

    template<typename T>
    [[nodiscard]] static CErasedOwner create(memory::CMemoryContext* const context = nullptr) noexcept;

    template<typename T>
    [[nodiscard]] T* payload() noexcept;

    template<typename T>
    [[nodiscard]] const T* payload() const noexcept;

    void destroy() noexcept;

    void add_hazard(mount_point_ids::id_type mount_point_id) noexcept;
    void remove_hazard(mount_point_ids::id_type mount_point_id) noexcept;
    [[nodiscard]] bool has_hazard(mount_point_ids::id_type mount_point_id) const noexcept;
    [[nodiscard]] bool has_any_hazard() const noexcept { return m_hazards != 0u; }
    [[nodiscard]] std::uint32_t hazard_mask() const noexcept { return m_hazards; }

    [[nodiscard]] memory::CMemoryContext* memory_context() const noexcept { return m_storage.context(); }
    [[nodiscard]] bool can_reattribute_to(memory::CMemoryContext* const context = nullptr) const noexcept;
    [[nodiscard]] bool reattribute(memory::CMemoryContext* const context = nullptr) noexcept;

private:
    [[nodiscard]] static std::uint32_t hazard_bit(mount_point_ids::id_type mount_point_id) noexcept;
    [[nodiscard]] bool memory_source_context(memory::CMemoryContext*& source) const noexcept;
    [[nodiscard]] std::uint32_t memory_allocation_count() const noexcept;
    [[nodiscard]] std::uint64_t memory_allocation_size() const noexcept;
    [[nodiscard]] std::uint32_t payload_memory_allocation_count() const noexcept;
    [[nodiscard]] std::uint64_t payload_memory_allocation_size() const noexcept;
    [[nodiscard]] bool payload_can_reattribute_to(memory::CMemoryContext* const context) const noexcept;
    void unsafe_replace_payload_memory_context_without_accounting(
        memory::CMemoryContext* expected_source, memory::CMemoryContext* target) noexcept;
    void make_canonical_empty() noexcept;

    memory::CMemoryToken m_storage;
    type_id              m_type_id{ type_ids::undefined };
    std::uint32_t        m_hazards{ 0u };
};

static_assert((mount_point_ids::k_count <= 32u), "CErasedOwner hazard storage cannot represent all registered mounting points");
static_assert(((sizeof(void*) != 8u) || (sizeof(CErasedOwner) == 32u)), "CErasedOwner must occupy 32 bytes on a 64-bit target");
static_assert(((sizeof(void*) != 4u) || (sizeof(CErasedOwner) == 24u)), "CErasedOwner must occupy 24 bytes on a 32-bit target");

//==============================================================================
//  Typed payload implementation
//==============================================================================

template<typename T>
inline CErasedOwner CErasedOwner::create(memory::CMemoryContext* const context) noexcept
{
    static_assert(k_is_erased_owner_payload_v<T>, "CErasedOwner may only create explicitly registered erased-owner payloads.");
    static_assert(system_type_ids::is_valid_id(k_system_type_id_v<T>), "CErasedOwner payload registration must provide a valid type id.");
    static_assert(std::is_nothrow_default_constructible_v<T>, "CErasedOwner payloads must be nothrow default constructible.");
    static_assert(std::is_nothrow_move_constructible_v<T>, "CErasedOwner payloads must be nothrow move constructible.");
    static_assert(std::is_nothrow_move_assignable_v<T>, "CErasedOwner payloads must be nothrow move assignable.");
    static_assert(std::is_nothrow_destructible_v<T>, "CErasedOwner payloads must be nothrow destructible.");
    static_assert((sizeof(T) <= 0xffffu), "CErasedOwner payload size exceeds the memory-token stride field.");

    CErasedOwner owner;
    memory::CMemoryToken storage{ sizeof(T), alignof(T), context };
    const bool allocated = storage.allocate(1u, false);
    MV_ASSERT(allocated);
    if (allocated)
    {
        ::new (storage.data()) T();
        owner.m_storage = std::move(storage);
        owner.m_type_id = type_id{ k_system_type_id_v<T> };
    }
    return owner;
}

template<typename T>
inline T* CErasedOwner::payload() noexcept
{
    static_assert(k_is_erased_owner_payload_v<T>, "CErasedOwner may only access explicitly registered erased-owner payloads.");
    return (m_type_id == type_id{ k_system_type_id_v<T> }) ? static_cast<T*>(m_storage.data()) : nullptr;
}

template<typename T>
inline const T* CErasedOwner::payload() const noexcept
{
    static_assert(k_is_erased_owner_payload_v<T>, "CErasedOwner may only access explicitly registered erased-owner payloads.");
    return (m_type_id == type_id{ k_system_type_id_v<T> }) ? static_cast<const T*>(m_storage.data()) : nullptr;
}

#endif  //  #ifndef ERASED_OWNER_HPP_INCLUDED
