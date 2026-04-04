
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   TSequentialTransport.hpp
//  Author: Ritchie Brannan
//  Date:   04 Apr 26

// currently at sketch only stage

#include <atomic>       //  std::atomic
#include <cstdint>      //  std::uint32_t
#include <cstring>      //  std::memcpy
#include <type_traits>  //  std::is_const_v, std::is_trivially_copyable_v
#include <utility>      //  std::move

#include "memory/memory_allocation.hpp"
#include "memory/memory_primitives.hpp"

//==============================================================================
//  TSequentialTransport<T>
//  Single Producer, Single Consumer (SPSC) message transport
//==============================================================================

template<typename T>
class TSequentialTransport
{
private:
    static_assert(!std::is_const_v<T>, "TSequentialTransport<T> requires non-const T.");
    static_assert(std::is_trivially_copyable_v<T>, "TSequentialTransport<T> requires trivially copyable T.");

public:
    TSequentialTransport() noexcept = default;
    TSequentialTransport(const TSequentialTransport&) = delete;
    TSequentialTransport& operator=(const TSequentialTransport&) = delete;
    TSequentialTransport(TSequentialTransport&&) noexcept = delete;
    TSequentialTransport& operator=(TSequentialTransport&&) noexcept = delete;
    ~TSequentialTransport() noexcept { (void)deallocate(true); };

    //  Initialisation and destruction
    bool initialise(const std::uint32_t initial_capacity, const std::uint32_t maximum_capacity, const bool allow_discard) noexcept;
    bool deallocate(const bool force = false) noexcept;

    //  User interface
    bool producer_send(const T& packet) noexcept;
    bool consumer_read(T& packet) noexcept;

    //  Constants
    static constexpr std::uint32_t k_max_elements = static_cast<std::uint32_t>(memory::t_max_elements<T>());

private:

    class Handshake
    {
    public:
        Handshake() noexcept = default;
        explicit Handshake(const std::uint32_t handshake) noexcept : m_handshake(handshake) {}

        //  Raw access (ready for the atomic exchange)
        std::uint32_t raw() const noexcept { return m_handshake; }

        //  Interrogation
        bool originator_is_producer() const noexcept { return (m_handshake & 0x80000000u) != 0u; }
        bool originator_is_consumer() const noexcept { return (m_handshake & 0x80000000u) == 0u; }
        bool ack_parity() const noexcept { return (m_handshake & 0x40000000u) != 0u; }
        std::uint32_t buffer_index() const noexcept { return (m_handshake >> 28) & 0x00000003u; }
        std::uint32_t consumer_index() const noexcept { return m_handshake & 0x0fffffffu; }

        //  Configuration
        void set_originator_is_producer() noexcept { m_handshake |= 0x80000000u; }
        void set_originator_is_consumer() noexcept { m_handshake &= 0x7fffffffu; }
        void set_ack_parity(const bool ack_parity) noexcept { m_handshake &= 0xbfffffff; if (ack_parity) m_handshake |= 0x40000000u; }
        void set_buffer_index(const std::uint32_t v) noexcept { m_handshake = (m_handshake & 0xcfffffffu) | ((v & 0x00000003u) << 28); }
        void set_consumer_index(const std::uint32_t v) noexcept { m_handshake = (m_handshake & 0xf0000000u) | (v & 0x0fffffffu); }

    private:
        std::uint32_t m_handshake = 0u;
    };

    struct BufferState
    {
        memory::TMemoryToken<T> buffer;
        std::uint32_t capacity = 0u;
        std::uint32_t size = 0u;
    };

    //  Transport buffers
    BufferState m_buffer_states[3];

    //  Producer state
    bool m_producer_ack_parity = false;
    std::uint32_t m_producer_buffer_index = 0u;

    //  Consumer state
    bool m_consumer_ack_parity = false;
    std::uint32_t m_consumer_buffer_index = 1u;
    std::uint32_t m_consumer_read_index = 0u;

    //  Staged state
    std::uint32_t m_staged_buffer_index = 2u;

    //  Configuration
    std::uint32_t m_current_capacity = 0u;
    std::uint32_t m_maximum_capacity = 0u;
    bool m_allow_discard = false;

    //  Atomic handshake
    std::atomic<std::uint32_t> m_handshake{ 0 };
};

//==============================================================================
//  TSequentialTransport<T> out of class function bodies
//==============================================================================

template<typename T>
inline bool TSequentialTransport<T>::initialise(const std::uint32_t initial_capacity, const std::uint32_t maximum_capacity, const bool allow_discard) noexcept
{
    if (m_current_capacity != 0u)
    {   //  re-initialisation is not allowed
        return false;
    }
    if ((initial_capacity > k_max_elements) || (maximum_capacity > k_max_elements))
    {
        return false;
    }
    const std::uint32_t current_capacity = std::max(initial_capacity, std::uint32_t{ 32 });
    memory::TMemoryToken<T> buffers[3];
    if (!buffers[0].allocate(current_capacity, false) || !buffers[1].allocate(current_capacity, false) || !buffers[2].allocate(current_capacity, false))
    {   //  buffer allocation failed
        return false;
    }
    deallocate();
    m_current_capacity = current_capacity;
    m_maximum_capacity = std::max(maximum_capacity, m_current_capacity);
    m_allow_discard = allow_discard;
    for (int i = 0; i < 3; ++i)
    {
        m_buffer_states[i].buffer = std::move(buffers[i]);
        m_buffer_states[i].capacity = m_current_capacity;
        m_buffer_states[i].size = 0u;
    }
    return true;
}

template<typename T>
inline bool TSequentialTransport<T>::deallocate(const bool force) noexcept
{
    (void)force;

    //  need to check that nothing is in flight before destruction
    //  assert if there is and force is false

    for (int i = 0; i < 3; ++i)
    {
        m_buffer_states[i].buffer.deallocate();
        m_buffer_states[i].capacity = 0u;
        m_buffer_states[i].size = 0u;
    }
    m_producer_ack_parity = false;
    m_producer_buffer_index = 0u;
    m_consumer_ack_parity = false;
    m_consumer_buffer_index = 1u;
    m_consumer_read_index = 0u;
    m_staged_buffer_index = 2u;
    m_current_capacity = 0u;
    m_maximum_capacity = 0u;
    m_handshake.store(0u, std::memory_order_release);
    return true;
}

template<typename T>
inline bool TSequentialTransport<T>::producer_send(const T& packet) noexcept
{
    (void)packet;
    return false;
}

template<typename T>
inline bool TSequentialTransport<T>::consumer_read(T& packet) noexcept
{
    BufferState* buffer_state = &m_buffer_states[m_consumer_buffer_index];
    if (m_consumer_read_index == buffer_state->size)
    {   //  the current buffer is exhausted, check if another is available
        Handshake handshake;
        handshake.originator_is_consumer();
        handshake.set_ack_parity(m_consumer_ack_parity);
        handshake.set_buffer_index(m_consumer_buffer_index);
        handshake = m_handshake.exchange(handshake, std::memory_order_acq_rel);
        if (!handshake.originator_is_producer())
        {   //  no new buffer is available
            return false;
        }
        m_consumer_buffer_index = handshake.buffer_index();
        if (m_consumer_ack_parity != handshake.ack_parity())
        {
            m_consumer_ack_parity = !m_consumer_ack_parity;
            m_consumer_read_index = 0u;
        }
        else
        {
            m_consumer_read_index = handshake.consumer_index();
        }
        buffer_state = &m_buffer_states[m_consumer_buffer_index];
        if (m_consumer_read_index == buffer_state->size)
        {   //  the new buffer contains no new packets
            return false;
        }
    }
    packet = buffer_state->buffer.data[m_consumer_read_index++];
    return true;
}