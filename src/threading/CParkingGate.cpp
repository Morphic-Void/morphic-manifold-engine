
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   CParkingGate.cpp
//  Author: Ritchie Brannan
//  Date:   18 May 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.

#include "threading/CParkingGate.hpp"
#include "platform/threading/processor_relax.hpp"
#include "debug/debug.hpp"

namespace threading
{

//==============================================================================
//  Constructor and destructor
//==============================================================================

CParkingGate::CParkingGate() noexcept : m_state(0u)
{
    m_valid = m_gates[0].is_valid() && m_gates[1].is_valid();
}

CParkingGate::~CParkingGate() noexcept
{
    MV_HARD_ASSERT(!has_control());
}

//==============================================================================
//  Status
//==============================================================================

bool CParkingGate::is_valid() const noexcept
{
    return m_valid;
}

bool CParkingGate::has_control() const noexcept
{
    return (m_state.load(std::memory_order_acquire) & k_control_mask) != 0u;
}

//==============================================================================
//  Control
//==============================================================================

bool CParkingGate::acquire_control() noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid))
    {
        const std::uint32_t state = m_state.load(std::memory_order_acquire);
        if (MV_FAIL_SAFE_ASSERT((state & k_control_mask) == 0u))
        {   //  the gate is not controlled already so acquire it
            const std::uint32_t closed_phase = 0u;
            const std::uint32_t next_state = ((state + k_generation_step) & k_generation_mask) | k_control_mask | closed_phase;
            m_gates[closed_phase].acquire();
            m_state.store(next_state, std::memory_order_release);
            return true;
        }
    }
    return false;
}

void CParkingGate::release_control() noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid))
    {
        const std::uint32_t state = m_state.load(std::memory_order_acquire);
        if (MV_FAIL_SAFE_ASSERT((state & k_control_mask) != 0u))
        {   //  the gate is controlled, so release it
            const std::uint32_t closed_phase = state & k_phase_mask;
            const std::uint32_t next_state = state & k_generation_mask;
            m_state.store(next_state, std::memory_order_release);
            m_gates[closed_phase].release();
        }
    }
}

//==============================================================================
//  Parking
//==============================================================================

void CParkingGate::park(CParkingTicket& ticket) noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid))
    {
        const std::uint32_t gate_state = m_state.load(std::memory_order_acquire);
        const std::uint32_t ticket_state = ticket.get_state();
        const bool ticket_was_controlled = (ticket_state & k_control_mask) != 0u;
        if (((gate_state & k_control_mask) == 0u) || (ticket_was_controlled && (((ticket_state ^ gate_state) & k_generation_mask) != 0u)))
        {   //  gate is uncontrolled or the ticket is invalid for parking (controlled by a previous generation), update the ticket without parking
            ticket.set_state(gate_state);
        }
        else
        {   //  the gate is controlled and the ticket is valid for parking, pass through the parking gate and update the ticket
            const std::uint32_t ticket_phase = (ticket_was_controlled ? ticket_state: gate_state) & k_phase_mask;
            platform::threading::processor_relax();
            m_gates[ticket_phase].acquire();
            m_gates[ticket_phase].release();
            ticket.set_state((gate_state & k_validation_mask) | (ticket_phase ^ k_phase_mask));
        }
    }
}

//==============================================================================
//  Phase control
//==============================================================================

void CParkingGate::cycle_phase() noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid))
    {
        std::uint32_t state = m_state.load(std::memory_order_acquire);
        if (MV_FAIL_SAFE_ASSERT((state & k_control_mask) != 0u))
        {   //  the gate is controlled so safe to cycle
            const std::uint32_t closed_phase = state & k_phase_mask;
            const std::uint32_t open_phase = closed_phase ^ k_phase_mask;
            state ^= k_phase_mask;
            m_gates[open_phase].acquire();
            m_state.store(state, std::memory_order_release);
            m_gates[closed_phase].release();
        }
    }
}

}   //  namespace threading
