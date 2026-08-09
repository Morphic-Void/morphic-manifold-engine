
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

#include "threading/messages/CErasedOwnerMsg.hpp"
#include "threading/messages/CErasedPodMsg.hpp"
#include "threading/transports/TOwningTransport.hpp"
#include "threading/transports/TQueueTransport.hpp"

namespace threading::transports
{

using CErasedPodMsgTransport = TQueue<threading::CErasedPodMsg>;

class CErasedOwnerMsgTransport
{
public:
    CErasedOwnerMsgTransport() noexcept = default;
    explicit CErasedOwnerMsgTransport(
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

private:
    [[nodiscard]] bool attribution_is_valid() const noexcept;

    TOwning<threading::CErasedOwnerMsg> m_transport;
    memory::CMemoryContext* const m_recipient_context{ nullptr };
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
