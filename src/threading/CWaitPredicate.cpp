
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   CWaitPredicate.cpp
//  Author: Ritchie Brannan
//  Date:   10 May 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.

#include "threading/CWaitPredicate.hpp"
#include "platform/platform_defines.hpp"
#include "debug/debug.hpp"

#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
#include "platform/threading/wait_word.hpp"
#endif

namespace threading
{

//==============================================================================
//  Constructor and destructor
//==============================================================================

CWaitPredicate::CWaitPredicate() noexcept
    : CWaitPredicate(0u)
{
}

CWaitPredicate::CWaitPredicate(const std::uint32_t initial_value) noexcept
    : m_word(initial_value)
{
#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
    m_valid = true;
#else
    m_valid = m_fallback_gate.is_valid();
#endif
    m_has_control = false;
}

CWaitPredicate::~CWaitPredicate() noexcept
{
    MV_HARD_ASSERT(!m_has_control);
}

//==============================================================================
//  Status
//==============================================================================

bool CWaitPredicate::is_valid() const noexcept
{
    return m_valid;
}

bool CWaitPredicate::has_control() const noexcept
{
    return m_has_control;
}

//==============================================================================
//  Control
//==============================================================================

bool CWaitPredicate::acquire_control() noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid && !m_has_control))
    {
#if !MV_PLATFORM_HAS_NATIVE_WAIT_WORD
        if (!MV_FAIL_SAFE_ASSERT(!m_fallback_gate.has_control()))
        {
            return false;
        }
        if (!m_fallback_gate.acquire_control())
        {
            return false;
        }
#endif
        m_has_control = true;
        return true;
    }
    return false;
}

void CWaitPredicate::release_control() noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid && m_has_control))
    {
#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
        platform::threading::wake_all_waiters(m_word);
#else
        if (MV_FAIL_SAFE_ASSERT(m_fallback_gate.has_control()))
        {
            m_fallback_gate.release_control();
        }
#endif
        m_has_control = false;
    }
}

//==============================================================================
//  Word access
//==============================================================================

std::uint32_t CWaitPredicate::get() const noexcept
{
    return m_word.load(std::memory_order_acquire);
}

void CWaitPredicate::set(const std::uint32_t value) noexcept
{
    m_word.store(value, std::memory_order_release);
}

void CWaitPredicate::increment() noexcept
{
    m_word.fetch_add(1u, std::memory_order_acq_rel);
}

void CWaitPredicate::decrement() noexcept
{
    m_word.fetch_sub(1u, std::memory_order_acq_rel);
}

//==============================================================================
//  Predicate waits
//==============================================================================

void CWaitPredicate::wait_until_equal(CParkingTicket& ticket, const std::uint32_t value) noexcept
{
#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
    (void)ticket;
#endif
    if (MV_FAIL_SAFE_ASSERT(m_valid))
    {
        for (;;)
        {
            const std::uint32_t seen = m_word.load(std::memory_order_acquire);

            if (seen == value)
            {
                break;
            }

#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
            platform::threading::wait_while_equal(m_word, seen);
#else
            m_fallback_gate.park(ticket);
#endif
        }
    }
}

std::uint32_t CWaitPredicate::wait_until_not_equal(CParkingTicket& ticket, const std::uint32_t value) noexcept
{
#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
    (void)ticket;
#endif
    if (MV_FAIL_SAFE_ASSERT(m_valid))
    {
        for (;;)
        {
            const std::uint32_t seen = m_word.load(std::memory_order_acquire);

            if (seen != value)
            {
                return seen;
            }

#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
            platform::threading::wait_while_equal(m_word, value);
#else
            m_fallback_gate.park(ticket);
#endif
        }
    }
    return m_word.load(std::memory_order_acquire);
}

//==============================================================================
//  Waiter notification
//==============================================================================

void CWaitPredicate::wake_one_waiter() noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid && m_has_control))
    {
#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
        platform::threading::wake_one_waiter(m_word);
#else
        m_fallback_gate.cycle_phase();
#endif
    }
}

void CWaitPredicate::wake_all_waiters() noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid && m_has_control))
    {
#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
        platform::threading::wake_all_waiters(m_word);
#else
        m_fallback_gate.cycle_phase();
#endif
    }
}

}   //  namespace threading
