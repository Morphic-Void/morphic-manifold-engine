
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   platform_threading.hpp
//  Author: Ritchie Brannan
//  Date:   7 May 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Convenience include for low-level platform threading primitives.
//
//  This header aggregates the small native/platform threading wrappers. Include
//  individual primitive headers when a narrower dependency is preferred.

#pragma once

#ifndef PLATFORM_THREADING_HPP_INCLUDED
#define PLATFORM_THREADING_HPP_INCLUDED

#include "platform/platform_defines.hpp"
#include "platform/threading/exclusive_lock.hpp"
#include "platform/threading/hw_thread_count.hpp"
#include "platform/threading/native_thread_id.hpp"
#include "platform/threading/processor_relax.hpp"
#include "platform/threading/thread_lifetime.hpp"

#if defined(MV_PLATFORM_HAS_NATIVE_WAIT_WORD)
#include "platform/threading/counting_semaphore.hpp"
#include "platform/threading/wait_word.hpp"
#endif

#endif  //  #ifndef PLATFORM_THREADING_HPP_INCLUDED
