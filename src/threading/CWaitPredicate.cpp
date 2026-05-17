
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

CWaitPredicate::CWaitPredicate() noexcept : m_word(k_control_released_word)
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
    MV_HARD_ASSERT(is_control_released());

#if !MV_PLATFORM_HAS_NATIVE_WAIT_WORD
    MV_HARD_ASSERT(!m_fallback_gate.has_control());
#endif
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

bool CWaitPredicate::is_control_released() const noexcept
{
    return m_word.load(std::memory_order_acquire) == k_control_released_word;
}

//==============================================================================
//  Control
//==============================================================================

bool CWaitPredicate::acquire_control(const std::uint32_t initial_value) noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid && !m_has_control && (initial_value != k_control_released_word)))
    {
        if (MV_FAIL_SAFE_ASSERT(is_control_released()))
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
            m_word.store(initial_value, std::memory_order_release);
            m_has_control = true;
            return true;
        }
    }
    return false;
}

void CWaitPredicate::release_control() noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid && m_has_control))
    {
        const std::uint32_t previous = m_word.exchange(k_control_released_word, std::memory_order_acq_rel);

        MV_FAIL_SAFE_ASSERT(previous != k_control_released_word);

#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
        wake_all_waiters_unchecked();
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
//  Word observation
//==============================================================================

std::uint32_t CWaitPredicate::get_word() const noexcept
{
    return m_word.load(std::memory_order_acquire);
}

//==============================================================================
//  Predicate waits
//==============================================================================

bool CWaitPredicate::wait_until_equal(CParkingTicket& ticket, const std::uint32_t value) noexcept
{
#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
    (void)ticket;
#endif
    if (MV_FAIL_SAFE_ASSERT(m_valid && (value != k_control_released_word)))
    {
        for (;;)
        {
            const std::uint32_t seen = m_word.load(std::memory_order_acquire);

            if (seen == value)
            {
                return true;
            }

            if (seen == k_control_released_word)
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
    return false;
}

std::uint32_t CWaitPredicate::wait_until_not_equal(CParkingTicket& ticket, const std::uint32_t value) noexcept
{
#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
    (void)ticket;
#endif
    if (MV_FAIL_SAFE_ASSERT(m_valid && (value != k_control_released_word)))
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
    return k_control_released_word;
}

//==============================================================================
//  Word modification and waiter notification
//==============================================================================

bool CWaitPredicate::set_and_wake_one(const std::uint32_t value) noexcept
{
    if (set(value))
    {
        wake_one_waiter_unchecked();
        return true;
    }
    return false;
}

bool CWaitPredicate::set_and_wake_all(const std::uint32_t value) noexcept
{
    if (set(value))
    {
        wake_all_waiters_unchecked();
        return true;
    }
    return false;
}

bool CWaitPredicate::increment_and_wake_one() noexcept
{
    if (increment())
    {
        wake_one_waiter_unchecked();
        return true;
    }
    return false;
}

bool CWaitPredicate::increment_and_wake_all() noexcept
{
    if (increment())
    {
        wake_all_waiters_unchecked();
        return true;
    }
    return false;
}

bool CWaitPredicate::decrement_and_wake_one() noexcept
{
    if (decrement())
    {
        wake_one_waiter_unchecked();
        return true;
    }
    return false;
}

bool CWaitPredicate::decrement_and_wake_all() noexcept
{
    if (decrement())
    {
        wake_all_waiters_unchecked();
        return true;
    }
    return false;
}

//==============================================================================
//  Word modification without waiter notification
//==============================================================================

bool CWaitPredicate::set(const std::uint32_t value) noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid && m_has_control && (value != k_control_released_word)))
    {
        m_word.store(value, std::memory_order_release);
        return true;
    }
    return false;
}

bool CWaitPredicate::increment() noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid && m_has_control))
    {
        std::uint32_t seen = m_word.load(std::memory_order_relaxed);

        for (;;)
        {
            if (seen == k_control_released_word)
            {
                return false;
            }

            const std::uint32_t wanted = next_predicate_word(seen);

            if (m_word.compare_exchange_weak(
                seen, wanted, std::memory_order_release, std::memory_order_relaxed))
            {
                return true;
            }

            //  On failure, seen has been updated with the current m_word.
        }
    }
    return false;
}

bool CWaitPredicate::decrement() noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid && m_has_control))
    {
        std::uint32_t seen = m_word.load(std::memory_order_relaxed);

        for (;;)
        {
            if (seen == k_control_released_word)
            {
                return false;
            }

            const std::uint32_t wanted = previous_predicate_word(seen);

            if (m_word.compare_exchange_weak(
                seen, wanted, std::memory_order_release, std::memory_order_relaxed))
            {
                return true;
            }

            //  On failure, seen has been updated with the current m_word.
        }
    }
    return false;
}

//==============================================================================
//  Waiter notification without word modification
//==============================================================================

bool CWaitPredicate::wake_one_waiter() noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid && m_has_control))
    {
        wake_one_waiter_unchecked();
        return true;
    }
    return false;
}

bool CWaitPredicate::wake_all_waiters() noexcept
{
    if (MV_FAIL_SAFE_ASSERT(m_valid && m_has_control))
    {
        wake_all_waiters_unchecked();
        return true;
    }
    return false;
}

//==============================================================================
//  Internal helpers
//==============================================================================

std::uint32_t CWaitPredicate::next_predicate_word(const std::uint32_t value) noexcept
{
    if (value >= k_max_predicate_word)
    {
        return 0u;
    }
    return value + 1u;
}

std::uint32_t CWaitPredicate::previous_predicate_word(const std::uint32_t value) noexcept
{
    if (value == 0u)
    {
        return k_max_predicate_word;
    }
    return value - 1u;
}

void CWaitPredicate::wake_one_waiter_unchecked() noexcept
{
#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
    platform::threading::wake_one_waiter(m_word);
#else
    m_fallback_gate.cycle_phase();
#endif
}

void CWaitPredicate::wake_all_waiters_unchecked() noexcept
{
#if MV_PLATFORM_HAS_NATIVE_WAIT_WORD
    platform::threading::wake_all_waiters(m_word);
#else
    m_fallback_gate.cycle_phase();
#endif
}

}   //  namespace threading
