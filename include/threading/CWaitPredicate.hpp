
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   CWaitPredicate.hpp
//  Author: Ritchie Brannan
//  Date:   10 May 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Engine-facing 32-bit wait predicate.
//
//  CWaitPredicate owns an atomic 32-bit word and provides wait-until predicate
//  operations over that word. The native backend uses platform wait-word
//  support where available. The fallback backend parks through CParkingGate.
//
//  State changes and waiter notification are explicit: set() stores the word,
//  while wake_one_waiter() and wake_all_waiters() notify waiters.
//
//  Construction does not acquire control. acquire_control() must be called by
//  the controller side before controlled use. release_control() releases the
//  fallback parking gate and wakes native waiters, but does not change the
//  predicate word.

#pragma once

#ifndef CWAIT_PREDICATE_HPP_INCLUDED
#define CWAIT_PREDICATE_HPP_INCLUDED

#include <atomic>       //  std::atomic
#include <cstdint>      //  std::uint32_t

#include "threading/CParkingGate.hpp"

namespace threading
{

//==============================================================================
//  CWaitPredicate
//==============================================================================

class CWaitPredicate
{
public:

    //  Constructor and destructor
    CWaitPredicate() noexcept;
    explicit CWaitPredicate(std::uint32_t initial_value) noexcept;
    ~CWaitPredicate() noexcept;

    //  Deleted lifetime
    CWaitPredicate(const CWaitPredicate&) = delete;
    CWaitPredicate& operator=(const CWaitPredicate&) = delete;
    CWaitPredicate(CWaitPredicate&&) = delete;
    CWaitPredicate& operator=(CWaitPredicate&&) = delete;

    //  Status
    bool is_valid() const noexcept;
    bool has_control() const noexcept;

    //  Control
    bool acquire_control() noexcept;
    void release_control() noexcept;

    //  Word access
    std::uint32_t get() const noexcept;
    void set(const std::uint32_t value) noexcept;
    void increment() noexcept;
    void decrement() noexcept;

    //  Predicate waits
    void wait_until_equal(CParkingTicket& ticket, const std::uint32_t value) noexcept;
    std::uint32_t wait_until_not_equal(CParkingTicket& ticket, const std::uint32_t value) noexcept;

    //  Waiter notification
    void wake_one_waiter() noexcept;
    void wake_all_waiters() noexcept;

private:

    std::atomic<std::uint32_t> m_word{ 0u };

    CParkingGate m_fallback_gate;

    bool m_valid = false;
    bool m_has_control = false;
};

}   //  namespace threading

#endif  //  #ifndef CWAIT_PREDICATE_HPP_INCLUDED
