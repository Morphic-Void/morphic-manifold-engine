
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   TRingTransport.hpp
//  Author: Ritchie Brannan
//  Date:   01 Apr 26

#include <atomic>
#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::uint8_t
#include <cstring>      //  std::memcpy, std::memset
#include <utility>      //  std::move

//==============================================================================
//  TRingTransport<T>
//  Single Producer, Single Consumer (SPSC) message transport
//==============================================================================

template<typename T>
class TRingTransport
{
private:
    static_assert(!std::is_const_v<T>, "TRingTransport<T> requires non-const T.");
    static_assert(std::is_trivially_copyable_v<T>, "TRingTransport<T> requires trivially copyable T.");

public:
    TRingTransport() noexcept = default;
    TRingTransport(const TRingTransport&) = delete;
    TRingTransport& operator=(const TRingTransport&) = delete;
    TRingTransport(TRingTransport&&) noexcept = delete;
    TRingTransport& operator=(TRingTransport&&) noexcept = delete;
    ~TRingTransport() noexcept { (void)deallocate(true); };

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
//  TRingTransport<T> out of class function bodies
//==============================================================================

template<typename T>
inline bool TRingTransport<T>::initialise(const std::uint32_t initial_capacity, const std::uint32_t maximum_capacity, const bool allow_discard) noexcept
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
inline bool TRingTransport<T>::deallocate(const bool force) noexcept
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
inline bool TRingTransport<T>::producer_send(const T& packet) noexcept
{
    (void)packet;
    return false;
}

template<typename T>
inline bool TRingTransport<T>::consumer_read(T& packet) noexcept
{
    BufferState& buffer_state = m_buffer_states[m_consumer_buffer_index];
    if (m_consumer_read_index == buffer_state.size)
    {   //  the buffer is exhausted, see if there is another
        Handshake handshake;
        handshake.originator_is_consumer();
        handshake.set_ack_parity();
        handshake.set_buffer_index(m_consumer_buffer_index);
    }
    packet = buffer_state.buffer.data[m_consumer_read_index++];
    return true;
}