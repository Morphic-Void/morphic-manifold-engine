
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   erased_owner.cpp
//  Author: Ritchie Brannan
//  Date:   26 Jul 26
//
//  Closed-world destruction and lifetime implementation for CErasedOwner.

#include <utility>      //  std::move

#include "system/erased_owner.hpp"
#include "system/transported_types.hpp"

CErasedOwner::CErasedOwner(CErasedOwner&& other) noexcept
    : m_storage(std::move(other.m_storage))
    , m_type_id(other.m_type_id)
    , m_hazards(other.m_hazards)
{
    other.make_canonical_empty();
}

CErasedOwner& CErasedOwner::operator=(CErasedOwner&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        m_storage = std::move(other.m_storage);
        m_type_id = other.m_type_id;
        m_hazards = other.m_hazards;
        other.make_canonical_empty();
    }
    return *this;
}

CErasedOwner::~CErasedOwner() noexcept
{
    destroy();
}

void CErasedOwner::destroy() noexcept
{
    if (!is_empty())
    {
        void* const payload = m_storage.data();
        MV_CRITICAL_ASSERT(payload != nullptr);
        if (payload != nullptr)
        {
            switch (m_type_id)
            {
                case k_type_id_v<OwningFileLoadResult>:
                    static_cast<OwningFileLoadResult*>(payload)->~OwningFileLoadResult();
                    break;

                case k_type_id_v<OwningTgaEncodeResult>:
                    static_cast<OwningTgaEncodeResult*>(payload)->~OwningTgaEncodeResult();
                    break;

                case k_type_id_v<OwningTgaDecodeResult>:
                    static_cast<OwningTgaDecodeResult*>(payload)->~OwningTgaDecodeResult();
                    break;

                default:
                    MV_CRITICAL_ASSERT(false);
                    break;
            }
        }
    }

    m_storage.deallocate();
    make_canonical_empty();
}

void CErasedOwner::add_hazard(const mount_point_ids::id_type mount_point_id) noexcept
{
    m_hazards |= hazard_bit(mount_point_id);
}

void CErasedOwner::remove_hazard(const mount_point_ids::id_type mount_point_id) noexcept
{
    m_hazards &= ~hazard_bit(mount_point_id);
}

bool CErasedOwner::has_hazard(const mount_point_ids::id_type mount_point_id) const noexcept
{
    const std::uint32_t bit = hazard_bit(mount_point_id);
    return (bit != 0u) && ((m_hazards & bit) != 0u);
}

bool CErasedOwner::can_reattribute_to(memory::CMemoryContext* target) const noexcept
{
    target = (target != nullptr) ? target : memory::get_ambient_memory_context();
    if (target == nullptr)
    {
        return false;
    }
    if (is_empty())
    {
        return true;
    }

    memory::CMemoryContext* source = nullptr;
    return is_ready() && memory_source_context(source) &&
        m_storage.can_reattribute_to(target) &&
        payload_can_reattribute_to(target);
}

bool CErasedOwner::reattribute(memory::CMemoryContext* target) noexcept
{
    target = (target != nullptr) ? target : memory::get_ambient_memory_context();
    if ((target == nullptr) || !can_reattribute_to(target))
    {
        return false;
    }
    if (is_empty())
    {
        return true;
    }

    memory::CMemoryContext* source = nullptr;
    if (!memory_source_context(source))
    {
        return false;
    }
    if ((source != nullptr) && (source != target) &&
        !memory::reattribute(
            *source, *target, memory_allocation_count(), memory_allocation_size()))
    {
        return false;
    }

    m_storage.unsafe_replace_context_without_accounting(source, target);
    unsafe_replace_payload_memory_context_without_accounting(source, target);
    return true;
}

std::uint32_t CErasedOwner::hazard_bit(const mount_point_ids::id_type mount_point_id) noexcept
{
    const mount_point_ids::index_type index = mount_point_ids::decode_id(mount_point_id);
    const bool valid =
        mount_point_ids::is_valid_index(index)
        && (index.raw_value() < mount_point_ids::k_count);
    MV_ASSERT(valid);
    return valid ? (std::uint32_t{ 1u } << static_cast<std::uint32_t>(index.raw_value())) : 0u;
}

bool CErasedOwner::memory_source_context(memory::CMemoryContext*& source) const noexcept
{
    source = m_storage.owns_storage() ? m_storage.context() : nullptr;
    if (!is_ready() || (source == nullptr))
    {
        return false;
    }

    memory::CMemoryContext* payload_source = nullptr;
    switch (m_type_id)
    {
        case k_type_id_v<OwningFileLoadResult>:
            payload_source =
                static_cast<const OwningFileLoadResult*>(m_storage.data())->buffer.memory_source_context();
            break;

        case k_type_id_v<OwningTgaEncodeResult>:
            payload_source =
                static_cast<const OwningTgaEncodeResult*>(m_storage.data())->buffer.memory_source_context();
            break;

        case k_type_id_v<OwningTgaDecodeResult>:
            payload_source =
                static_cast<const OwningTgaDecodeResult*>(m_storage.data())->buffer.memory_source_context();
            break;

        default:
            MV_CRITICAL_ASSERT(false);
            return false;
    }
    return (payload_source == nullptr) || (payload_source == source);
}

std::uint32_t CErasedOwner::memory_allocation_count() const noexcept
{
    return is_empty()
        ? 0u
        : (m_storage.memory_allocation_count() + payload_memory_allocation_count());
}

std::uint64_t CErasedOwner::memory_allocation_size() const noexcept
{
    return is_empty()
        ? 0u
        : (m_storage.memory_allocation_size() + payload_memory_allocation_size());
}

std::uint32_t CErasedOwner::payload_memory_allocation_count() const noexcept
{
    switch (m_type_id)
    {
        case k_type_id_v<OwningFileLoadResult>:
            return static_cast<const OwningFileLoadResult*>(m_storage.data())->buffer.memory_allocation_count();

        case k_type_id_v<OwningTgaEncodeResult>:
            return static_cast<const OwningTgaEncodeResult*>(m_storage.data())->buffer.memory_allocation_count();

        case k_type_id_v<OwningTgaDecodeResult>:
            return static_cast<const OwningTgaDecodeResult*>(m_storage.data())->buffer.memory_allocation_count();

        default:
            MV_CRITICAL_ASSERT(false);
            return 0u;
    }
}

std::uint64_t CErasedOwner::payload_memory_allocation_size() const noexcept
{
    switch (m_type_id)
    {
        case k_type_id_v<OwningFileLoadResult>:
            return static_cast<const OwningFileLoadResult*>(m_storage.data())->buffer.memory_allocation_size();

        case k_type_id_v<OwningTgaEncodeResult>:
            return static_cast<const OwningTgaEncodeResult*>(m_storage.data())->buffer.memory_allocation_size();

        case k_type_id_v<OwningTgaDecodeResult>:
            return static_cast<const OwningTgaDecodeResult*>(m_storage.data())->buffer.memory_allocation_size();

        default:
            MV_CRITICAL_ASSERT(false);
            return 0u;
    }
}

bool CErasedOwner::payload_can_reattribute_to(memory::CMemoryContext* const target) const noexcept
{
    switch (m_type_id)
    {
        case k_type_id_v<OwningFileLoadResult>:
            return static_cast<const OwningFileLoadResult*>(m_storage.data())->buffer.can_reattribute_to(target);

        case k_type_id_v<OwningTgaEncodeResult>:
            return static_cast<const OwningTgaEncodeResult*>(m_storage.data())->buffer.can_reattribute_to(target);

        case k_type_id_v<OwningTgaDecodeResult>:
            return static_cast<const OwningTgaDecodeResult*>(m_storage.data())->buffer.can_reattribute_to(target);

        default:
            MV_CRITICAL_ASSERT(false);
            return false;
    }
}

void CErasedOwner::unsafe_replace_payload_memory_context_without_accounting(
    memory::CMemoryContext* const expected_source,
    memory::CMemoryContext* const target) noexcept
{
    switch (m_type_id)
    {
        case k_type_id_v<OwningFileLoadResult>:
            static_cast<OwningFileLoadResult*>(m_storage.data())->
                buffer.unsafe_replace_memory_context_without_accounting(expected_source, target);
            break;

        case k_type_id_v<OwningTgaEncodeResult>:
            static_cast<OwningTgaEncodeResult*>(m_storage.data())->
                buffer.unsafe_replace_memory_context_without_accounting(expected_source, target);
            break;

        case k_type_id_v<OwningTgaDecodeResult>:
            static_cast<OwningTgaDecodeResult*>(m_storage.data())->
                buffer.unsafe_replace_memory_context_without_accounting(expected_source, target);
            break;

        default:
            MV_CRITICAL_ASSERT(false);
            break;
    }
}

void CErasedOwner::make_canonical_empty() noexcept
{
    m_storage = memory::CMemoryToken{};
    m_type_id = type_ids::id_type{ 0u };
    m_hazards = 0u;
}
