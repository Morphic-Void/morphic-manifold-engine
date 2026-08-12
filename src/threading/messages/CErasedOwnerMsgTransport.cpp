
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   CErasedOwnerMsgTransport.cpp
//  Authors: Ritchie Brannan / OpenAI tools
//  Date:   8 Aug 26

#include <utility>      //  std::move

#include "debug/macros.hpp"
#include "threading/messages/CErasedMessageTransports.hpp"

namespace threading::transports
{

CErasedOwnerMsgTransport::CErasedOwnerMsgTransport(
    const module_ids::id_type destination_module_id,
    memory::CMemoryContext* const transport_context,
    memory::CMemoryContext* const recipient_context) noexcept
    : m_transport(transport_context)
    , m_recipient_context(recipient_context)
    , m_destination_module_id(destination_module_id)
{
}

bool CErasedOwnerMsgTransport::is_valid() const noexcept
{
    return attribution_is_valid() && m_transport.is_valid();
}

bool CErasedOwnerMsgTransport::posting_is_valid() const noexcept
{
    return attribution_is_valid() && m_transport.posting_is_valid();
}

bool CErasedOwnerMsgTransport::post(threading::CErasedOwnerMsg&& msg) noexcept
{
    if (!posting_is_valid() || (writable_count() == 0u) || !msg.has_message_type() ||
        !erased_transport_admission::is_admissible(
            type_id{ msg.query_message_type_id() }, m_destination_module_id))
    {
        return false;
    }

    CErasedOwner& owner = msg.owner();
    memory::CMemoryContext* source_context = nullptr;
    if (owner.is_ready())
    {
        if (!erased_transport_admission::is_admissible(
            type_id{ owner.query_type_id() }, m_destination_module_id))
        {
            return false;
        }

        memory::CMemoryContext* const transport_context = memory_context();
        if (!owner.can_reattribute_to(transport_context))
        {
            return false;
        }

        source_context = owner.memory_context();
        const bool reattributed = owner.reattribute(transport_context);
        MV_CRITICAL_ASSERT(reattributed);
        if (!reattributed)
        {
            return false;
        }
    }

    const bool posted = m_transport.post(std::move(msg));
    MV_CRITICAL_ASSERT(posted);
    if (!posted && owner.is_ready())
    {
        const bool restored = owner.reattribute(source_context);
        MV_CRITICAL_ASSERT(restored);
    }
    return posted;
}

bool CErasedOwnerMsgTransport::reading_is_valid() const noexcept
{
    return attribution_is_valid() && m_transport.reading_is_valid();
}

bool CErasedOwnerMsgTransport::read(threading::CErasedOwnerMsg& msg) noexcept
{
    if (!reading_is_valid() || !m_transport.read(msg))
    {
        return false;
    }
    if ((m_recipient_context != nullptr) && msg.has_owner())
    {
        if (!msg.owner().reattribute(m_recipient_context))
        {
            MV_CRITICAL_ASSERT(false);
        }
    }
    return true;
}

bool CErasedOwnerMsgTransport::initialise(const std::uint32_t capacity) noexcept
{
    return attribution_is_valid() && m_transport.initialise(capacity);
}

bool CErasedOwnerMsgTransport::attribution_is_valid() const noexcept
{
    memory::CMemoryContext* const transport_context = memory_context();
    memory::CMemoryContext* const destination_context =
        (m_recipient_context != nullptr) ? m_recipient_context : transport_context;
    return module_ids::is_valid_id(m_destination_module_id) &&
        (transport_context != nullptr) &&
        (destination_context != nullptr) &&
        system_ids::is_valid_id(destination_context->get_system_id()) &&
        (system_ids::get_module_id(destination_context->get_system_id()) ==
            m_destination_module_id) &&
        ((m_recipient_context == nullptr) ||
            transport_context->is_compatible_with(*m_recipient_context));
}

}   //  namespace threading::transports
