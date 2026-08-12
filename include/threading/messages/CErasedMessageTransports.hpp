
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    CErasedMessageTransports.hpp
//  Authors: Ritchie Brannan / OpenAI tools
//  Date:    8 Aug 26
//
//  Concrete transports for erased POD and owning thread messages.

#pragma once

#ifndef CERASED_MESSAGE_TRANSPORTS_HPP_INCLUDED
#define CERASED_MESSAGE_TRANSPORTS_HPP_INCLUDED

#include <cstdint>      //  std::uint32_t
#include <utility>      //  std::move

#include "system/erased_transport_admission.hpp"
#include "threading/messages/CErasedOwnerMsg.hpp"
#include "threading/messages/CErasedPodMsg.hpp"
#include "threading/transports/TOwningTransport.hpp"
#include "threading/transports/TQueueTransport.hpp"

namespace threading::transports
{

class CErasedPodMsgTransport
{
public:
    CErasedPodMsgTransport() noexcept = default;
    explicit CErasedPodMsgTransport(
        const module_ids::id_type destination_module_id) noexcept
        : m_destination_module_id(destination_module_id)
    {
    }
    CErasedPodMsgTransport(const CErasedPodMsgTransport&) = delete;
    CErasedPodMsgTransport& operator=(const CErasedPodMsgTransport&) = delete;
    CErasedPodMsgTransport(CErasedPodMsgTransport&&) = delete;
    CErasedPodMsgTransport& operator=(CErasedPodMsgTransport&&) = delete;
    ~CErasedPodMsgTransport() noexcept = default;

    [[nodiscard]] bool posting_is_valid() const noexcept
    {
        return module_ids::is_valid_id(m_destination_module_id) &&
            m_transport.posting_is_valid();
    }
    [[nodiscard]] bool posting_is_ready() const noexcept
    {
        return module_ids::is_valid_id(m_destination_module_id) &&
            m_transport.posting_is_ready();
    }
    [[nodiscard]] bool posting_poisoned() const noexcept
    {
        return m_transport.posting_poisoned();
    }
    [[nodiscard]] bool post(const threading::CErasedPodMsg& msg) noexcept
    {
        return erased_transport_admission::is_admissible(
            type_id{ msg.query_message_type_id() }, m_destination_module_id) &&
            m_transport.post(msg);
    }
    [[nodiscard]] bool post_would_reallocate(const std::uint32_t count) const noexcept
    {
        return m_transport.post_would_reallocate(count);
    }

    [[nodiscard]] bool reading_is_valid() const noexcept
    {
        return module_ids::is_valid_id(m_destination_module_id) &&
            m_transport.reading_is_valid();
    }
    [[nodiscard]] bool reading_is_ready() const noexcept
    {
        return module_ids::is_valid_id(m_destination_module_id) &&
            m_transport.reading_is_ready();
    }
    [[nodiscard]] bool read(threading::CErasedPodMsg& msg) noexcept
    {
        return m_transport.read(msg);
    }
    [[nodiscard]] std::uint32_t current_readable_count() const noexcept
    {
        return m_transport.current_readable_count();
    }
    [[nodiscard]] std::uint32_t refresh_readable_count() noexcept
    {
        return m_transport.refresh_readable_count();
    }

    [[nodiscard]] bool initialise_fixed(
        const std::uint32_t capacity,
        const bool allow_discard = false) noexcept
    {
        return module_ids::is_valid_id(m_destination_module_id) &&
            m_transport.initialise_fixed(capacity, allow_discard);
    }
    [[nodiscard]] bool initialise_growable(
        const std::uint32_t capacity,
        const std::uint32_t max_capacity = 0u) noexcept
    {
        return module_ids::is_valid_id(m_destination_module_id) &&
            m_transport.initialise_growable(capacity, max_capacity);
    }
    void deallocate() noexcept { m_transport.deallocate(); }

    [[nodiscard]] memory::CMemoryContext* memory_context() const noexcept
    {
        return m_transport.memory_context();
    }
    [[nodiscard]] module_ids::id_type destination_module_id() const noexcept
    {
        return m_destination_module_id;
    }

private:
    TQueue<threading::CErasedPodMsg> m_transport;
    module_ids::id_type m_destination_module_id{};
};

class CErasedOwnerMsgTransport
{
public:
    CErasedOwnerMsgTransport() noexcept = default;
    explicit CErasedOwnerMsgTransport(
        module_ids::id_type destination_module_id,
        memory::CMemoryContext* transport_context,
        memory::CMemoryContext* recipient_context = nullptr) noexcept;
    CErasedOwnerMsgTransport(const CErasedOwnerMsgTransport&) = delete;
    CErasedOwnerMsgTransport& operator=(const CErasedOwnerMsgTransport&) = delete;
    CErasedOwnerMsgTransport(CErasedOwnerMsgTransport&&) = delete;
    CErasedOwnerMsgTransport& operator=(CErasedOwnerMsgTransport&&) = delete;
    ~CErasedOwnerMsgTransport() noexcept = default;

    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool is_ready() const noexcept { return m_transport.is_ready(); }

    [[nodiscard]] bool posting_is_valid() const noexcept;
    [[nodiscard]] bool post(threading::CErasedOwnerMsg&& msg) noexcept;
    [[nodiscard]] std::uint32_t writable_count() const noexcept { return m_transport.writable_count(); }

    [[nodiscard]] bool reading_is_valid() const noexcept;
    [[nodiscard]] bool read(threading::CErasedOwnerMsg& msg) noexcept;
    [[nodiscard]] std::uint32_t readable_count() const noexcept { return m_transport.readable_count(); }

    [[nodiscard]] bool initialise(std::uint32_t capacity) noexcept;
    void deallocate() noexcept { m_transport.deallocate(); }

    [[nodiscard]] memory::CMemoryContext* memory_context() const noexcept
    {
        return m_transport.memory_context();
    }
    [[nodiscard]] memory::CMemoryContext* recipient_memory_context() const noexcept
    {
        return m_recipient_context;
    }
    [[nodiscard]] module_ids::id_type destination_module_id() const noexcept
    {
        return m_destination_module_id;
    }

private:
    [[nodiscard]] bool attribution_is_valid() const noexcept;

    TOwning<threading::CErasedOwnerMsg> m_transport;
    memory::CMemoryContext* const m_recipient_context{ nullptr };
    module_ids::id_type m_destination_module_id{};
};

class CErasedOwnerMsgProducerEndpoint
{
public:
    explicit CErasedOwnerMsgProducerEndpoint(CErasedOwnerMsgTransport& transport) noexcept
        : m_transport(transport)
    {
    }
    ~CErasedOwnerMsgProducerEndpoint() noexcept = default;

    [[nodiscard]] bool is_valid() const noexcept { return m_transport.posting_is_valid(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_transport.is_ready(); }
    [[nodiscard]] bool post(threading::CErasedOwnerMsg&& msg) noexcept
    {
        return m_transport.post(std::move(msg));
    }
    [[nodiscard]] std::uint32_t writable_count() const noexcept { return m_transport.writable_count(); }

private:
    CErasedOwnerMsgTransport& m_transport;
};

class CErasedOwnerMsgConsumerEndpoint
{
public:
    explicit CErasedOwnerMsgConsumerEndpoint(CErasedOwnerMsgTransport& transport) noexcept
        : m_transport(transport)
    {
    }
    ~CErasedOwnerMsgConsumerEndpoint() noexcept = default;

    [[nodiscard]] bool is_valid() const noexcept { return m_transport.reading_is_valid(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_transport.is_ready(); }
    [[nodiscard]] bool read(threading::CErasedOwnerMsg& msg) noexcept { return m_transport.read(msg); }
    [[nodiscard]] std::uint32_t readable_count() const noexcept { return m_transport.readable_count(); }

private:
    CErasedOwnerMsgTransport& m_transport;
};

}   //  namespace threading::transports

#endif  //  #ifndef CERASED_MESSAGE_TRANSPORTS_HPP_INCLUDED
