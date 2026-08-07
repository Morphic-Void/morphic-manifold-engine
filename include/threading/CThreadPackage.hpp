
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   CThreadPackage.hpp
//  Author: Ritchie Brannan
//  Drafting and refactoring assistance: OpenAI tools
//  Date:   7 Aug 26
//
//  Common resources and owner/thread-facing interfaces for an engine thread.
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.

#pragma once

#ifndef CTHREADPACKAGE_HPP_INCLUDED
#define CTHREADPACKAGE_HPP_INCLUDED

#include <cstdint>      //  std::uint32_t
#include <utility>      //  std::move

#include "platform/threading/thread_lifetime.hpp"
#include "platform/threading/thread_priority.hpp"
#include "system/erased_owner.hpp"
#include "system/erased_owner_transport.hpp"
#include "system/system_ids.hpp"
#include "threading/CParkingGate.hpp"
#include "threading/CThreadControlState.hpp"
#include "threading/CWaitPredicate.hpp"
#include "threading/messages/CPodThreadMsg.hpp"
#include "threading/transports/TQueueTransport.hpp"

namespace threading
{

struct ThreadConfig
{
    thread_ids::id_type thread_id{};
    platform::threading::EThreadPriority priority{ platform::threading::EThreadPriority::Normal };
    platform::threading::FThreadEntry entry_point{ nullptr };
};

class CThreadResources
{
public:
    explicit CThreadResources(const ThreadConfig& thread_config) noexcept : config{ thread_config } {}
    ~CThreadResources() noexcept = default;

    //  Common resources available to the thread package and context.
    bool created{ false };
    platform::threading::CThread thread;
    CParkingTicket parking_ticket;
    CWaitPredicate wait_predicate;
    transports::TQueue<CPodThreadMsg> host_to_worker_msgs;
    transports::TQueue<CPodThreadMsg> worker_to_host_msgs;
    transports::CErasedOwnerTransport worker_owned_to_host_owned;
    CThreadControlState control_state;
    ThreadConfig config;
};

class CThreadContext
{
public:
    explicit CThreadContext(CThreadResources& resources) noexcept : m_resources{ resources } {}
    ~CThreadContext() noexcept = default;

    void startup() noexcept;
    void mark_waiting() noexcept;
    void mark_running() noexcept;
    void mark_exiting() noexcept;
    void mark_exited() noexcept;
    void mark_failed(std::uint32_t code) noexcept;
    void advance_heartbeat() noexcept;

    [[nodiscard]] bool exit_requested() const noexcept;
    std::uint32_t wait_for_new_epoch(std::uint32_t epoch) noexcept;

    bool read(CPodThreadMsg& msg) noexcept;
    bool post(const CPodThreadMsg& msg) noexcept;
    bool pass_ownership(CErasedOwner& obj) noexcept;

private:
    CThreadResources& m_resources;
};

class CThreadPackage
{
public:
    explicit CThreadPackage(const ThreadConfig& thread_config) noexcept : m_resources{ thread_config } {}
    ~CThreadPackage() noexcept = default;

    bool startup() noexcept;
    bool shutdown() noexcept;
    bool read(CPodThreadMsg& msg) noexcept;
    bool post(const CPodThreadMsg& msg) noexcept;
    bool take_ownership(CErasedOwner& obj) noexcept;

    [[nodiscard]] EThreadRunState query_state() const noexcept;

private:
    CThreadResources m_resources;
};

//==============================================================================
//  CThreadContext inline implementation
//==============================================================================

inline void CThreadContext::mark_waiting() noexcept
{
    m_resources.control_state.mark_waiting();
}

inline void CThreadContext::mark_running() noexcept
{
    m_resources.control_state.mark_running();
}

inline void CThreadContext::mark_exiting() noexcept
{
    m_resources.control_state.mark_exiting();
}

inline void CThreadContext::mark_exited() noexcept
{
    m_resources.control_state.mark_exited();
}

inline void CThreadContext::mark_failed(const std::uint32_t code) noexcept
{
    m_resources.control_state.mark_failed(code);
}

inline void CThreadContext::advance_heartbeat() noexcept
{
    m_resources.control_state.advance_heartbeat();
}

inline bool CThreadContext::exit_requested() const noexcept
{
    return m_resources.control_state.exit_requested();
}

inline bool CThreadContext::read(CPodThreadMsg& msg) noexcept
{
    return m_resources.host_to_worker_msgs.read(msg);
}

inline bool CThreadContext::post(const CPodThreadMsg& msg) noexcept
{
    return m_resources.worker_to_host_msgs.post(msg);
}

inline bool CThreadContext::pass_ownership(CErasedOwner& obj) noexcept
{
    return m_resources.worker_owned_to_host_owned.post(std::move(obj));
}

//==============================================================================
//  CThreadPackage inline implementation
//==============================================================================

inline bool CThreadPackage::read(CPodThreadMsg& msg) noexcept
{
    return m_resources.worker_to_host_msgs.read(msg);
}

inline bool CThreadPackage::post(const CPodThreadMsg& msg) noexcept
{
    const bool success = m_resources.host_to_worker_msgs.post(msg);
    if (success)
    {
        m_resources.wait_predicate.poke_epoch_and_wake_one();
    }
    return success;
}

inline bool CThreadPackage::take_ownership(CErasedOwner& obj) noexcept
{
    return m_resources.worker_owned_to_host_owned.read(obj);
}

inline EThreadRunState CThreadPackage::query_state() const noexcept
{
    return m_resources.control_state.query_state();
}

}   //  namespace threading

#endif  //  #ifndef CTHREADPACKAGE_HPP_INCLUDED
