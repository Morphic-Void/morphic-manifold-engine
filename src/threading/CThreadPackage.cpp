
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   CThreadPackage.cpp
//  Author: Ritchie Brannan
//  Drafting and refactoring assistance: OpenAI tools
//  Date:   7 Aug 26
//
//  Common resources and owner/thread-facing interfaces for an engine thread.

#include <thread>       //  std::this_thread::yield

#include "threading/CThreadPackage.hpp"

#include "debug/macros.hpp"
#include "platform/threading/thread_naming.hpp"
#include "system/system_context.hpp"

namespace threading
{

void CThreadContext::startup() noexcept
{
    m_resources.control_state.mark_startup();

    const thread_ids::id_type thread_id = m_resources.config.thread_id;
    (void)system_context::set_ambient_thread_id(thread_id);

    const char* const thread_name = system_id_registry::lookup_thread_name(thread_id);
    MV_ASSERT(thread_name != nullptr);
    if (thread_name != nullptr)
    {
        (void)platform::threading::set_current_thread_name(thread_name);
    }

    (void)platform::threading::set_current_thread_priority(m_resources.config.priority);
}

std::uint32_t CThreadContext::wait_for_new_epoch(const std::uint32_t epoch) noexcept
{
    MV_TRACE("Waiting epoch={}", epoch);

    mark_waiting();
    const std::uint32_t new_epoch = m_resources.wait_predicate.wait_until_not_equal(m_resources.parking_ticket, epoch);
    mark_running();

    MV_TRACE("Running epoch={}", new_epoch);

    return new_epoch;
}

bool CThreadPackage::startup() noexcept
{
    if (m_resources.host_to_worker_msgs.initialise_growable(0u))
    {
        if (m_resources.worker_to_host_msgs.initialise_growable(0u))
        {
            if (m_resources.worker_owned_to_host_owned.initialise(0u))
            {
                if (m_resources.wait_predicate.acquire_control())
                {
                    m_resources.control_state.mark_pending();
                    m_resources.created = m_resources.thread.create(m_resources.config.entry_point, &m_resources);
                    if (m_resources.created)
                    {
                        while (m_resources.control_state.is_starting())
                        {
                            std::this_thread::yield();
                        }
                        if (m_resources.control_state.is_ready())
                        {
                            return true;
                        }
                        (void)m_resources.thread.join_and_close();
                        m_resources.created = false;
                    }
                    m_resources.control_state.mark_empty();
                    m_resources.wait_predicate.release_control();
                }
                m_resources.worker_owned_to_host_owned.deallocate();
            }
            m_resources.worker_to_host_msgs.deallocate();
        }
        m_resources.host_to_worker_msgs.deallocate();
    }
    return false;
}

bool CThreadPackage::shutdown() noexcept
{
    if (m_resources.created && m_resources.wait_predicate.has_control())
    {
        m_resources.control_state.request_exit();
        m_resources.wait_predicate.release_control();
        while (!m_resources.control_state.is_done())
        {
            std::this_thread::yield();
        }
        m_resources.created = m_resources.thread.join_and_close();
        if (!m_resources.created)
        {
            m_resources.worker_owned_to_host_owned.deallocate();
            m_resources.worker_to_host_msgs.deallocate();
            m_resources.host_to_worker_msgs.deallocate();
        }
    }
    return m_resources.created;
}

}   //  namespace threading
