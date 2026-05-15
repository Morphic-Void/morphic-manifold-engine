
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   CPodThreadMsg.hpp
//  Author: Ritchie Brannan
//  Date:   14 May 2026
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Fixed-size POD typeless message for thread communication.
//
//  CPodThreadMsg is a non-owning, trivially-copyable message container
//  intended for queue-based communication between engine threads.
//
//  The stored payload is type-erased using TTypelessPod and identified by
//  the project type-id registry. Concrete message payloads are copied into
//  and out of the erased storage; no typed pointer into the erased payload is
//  exposed.
//
//  The message payload is shaped as three 64-bit cells. This supports common
//  thread-message forms such as task slot references, generation counters,
//  flags, result codes, small handles, and non-owning pointer-sized values.
//
//  IMPORTANT SEMANTIC NOTE
//  -----------------------
//  CPodThreadMsg does not own any memory referenced by values stored in its
//  payload. Pointer values carried by a message are views only. Their validity
//  must be guaranteed by the surrounding thread, host, or provisioning
//  protocol.
//
//  Ownership transfer must use an explicit owning transport or host-owned
//  storage path, not CPodThreadMsg.

#pragma once

#ifndef CPOD_THREAD_MSG_HPP_INCLUDED
#define CPOD_THREAD_MSG_HPP_INCLUDED

#include <cstdint>      //  std::uint64_t, std::uintptr_t

#include "types/typeless_pod.hpp"

namespace threading
{

//==============================================================================
//  PodThreadMsgPayload
//  Fixed payload shape for CPodThreadMsg
//==============================================================================

struct PodThreadMsgPayload
{
    std::uint64_t a = 0u;
    std::uint64_t b = 0u;
    std::uint64_t c = 0u;
};

//==============================================================================
//  CPodThreadMsg
//  Fixed-size non-owning POD typeless thread message
//==============================================================================

struct CPodThreadMsg
{
    std::int32_t async_slot = 0;
    TTypelessPodFor<PodThreadMsgPayload> payload;
};

static_assert((sizeof(std::uintptr_t) <= sizeof(std::uint64_t)), "CPodThreadMsg requires pointer-sized values to fit in std::uint64_t.");

}   //  namespace threading

#endif  //  #ifndef CPOD_THREAD_MSG_HPP_INCLUDED
