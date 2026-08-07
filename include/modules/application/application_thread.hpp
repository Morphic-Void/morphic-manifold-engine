
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   application_thread.hpp
//  Author: Ritchie Brannan
//  Drafting and refactoring assistance: OpenAI tools
//  Date:   7 Aug 26
//
//  Entry point for the application thread.

#pragma once

#ifndef APPLICATION_THREAD_HPP_INCLUDED
#define APPLICATION_THREAD_HPP_INCLUDED

#include "platform/threading/thread_lifetime.hpp"

namespace application
{

platform::threading::FThreadEntry application_thread_entry_point() noexcept;

}   //  namespace application

#endif  //  #ifndef APPLICATION_THREAD_HPP_INCLUDED
