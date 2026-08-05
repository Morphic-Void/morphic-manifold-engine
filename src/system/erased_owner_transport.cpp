
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   erased_owner_transport.cpp
//  Author: Ritchie Brannan
//  Date:   26 Jul 26

#include <utility>      //  std::move

#include "debug/macros.hpp"
#include "system/erased_owner_transport.hpp"

namespace threading::transports
{

CErasedOwnerTransport::CErasedOwnerTransport(
    memory::CMemoryContext* const transport_context,
    memory::CMemoryContext* const recipient_context) noexcept
    : m_transport(transport_context)
    , m_recipient_context(recipient_context)
{
}

bool CErasedOwnerTransport::is_valid() const noexcept
{
    return attribution_is_valid() && m_transport.is_valid();
}

bool CErasedOwnerTransport::posting_is_valid() const noexcept
{
    return attribution_is_valid() && m_transport.posting_is_valid();
}

bool CErasedOwnerTransport::post(CErasedOwner&& owner) noexcept
{
    if (!posting_is_valid() || (writable_count() == 0u) || !owner.is_ready())
    {
        return false;
    }

    memory::CMemoryContext* const transport_context = memory_context();
    if (!owner.can_reattribute_to(transport_context))
    {
        return false;
    }

    memory::CMemoryContext* const source_context = owner.memory_context();
    const bool reattributed = owner.reattribute(transport_context);
    MV_CRITICAL_ASSERT(reattributed);
    if (!reattributed)
    {
        return false;
    }

    const bool posted = m_transport.post(std::move(owner));
    MV_CRITICAL_ASSERT(posted);
    if (!posted)
    {
        const bool restored = owner.reattribute(source_context);
        MV_CRITICAL_ASSERT(restored);
    }
    return posted;
}

bool CErasedOwnerTransport::reading_is_valid() const noexcept
{
    return attribution_is_valid() && m_transport.reading_is_valid();
}

bool CErasedOwnerTransport::read(CErasedOwner& owner) noexcept
{
    if (!reading_is_valid() || !m_transport.read(owner))
    {
        return false;
    }
    if (m_recipient_context != nullptr)
    {
        if (!owner.reattribute(m_recipient_context))
        {
            MV_CRITICAL_ASSERT(false);
        }
    }
    return true;
}

bool CErasedOwnerTransport::initialise(const std::uint32_t capacity) noexcept
{
    return attribution_is_valid() && m_transport.initialise(capacity);
}

bool CErasedOwnerTransport::attribution_is_valid() const noexcept
{
    memory::CMemoryContext* const transport_context = memory_context();
    return (transport_context != nullptr) &&
        ((m_recipient_context == nullptr) ||
            transport_context->is_compatible_with(*m_recipient_context));
}

}   //  namespace threading::transports
