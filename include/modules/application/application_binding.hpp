
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    application_binding.hpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    10 Aug 26
//
//  Application module function identities and ABI signatures.

#pragma once

#ifndef APPLICATION_BINDING_HPP_INCLUDED
#define APPLICATION_BINDING_HPP_INCLUDED

#include "platform/threading/thread_lifetime.hpp"
#include "system/system_type_registration.hpp"

namespace application
{

//==============================================================================
//  Function identity declarations
//==============================================================================

struct CApplicationThreadFunction;

using FApplicationThread = platform::threading::FThreadEntry;

}   //  namespace application

MV_REGISTER_SYSTEM_TYPE(application::CApplicationThreadFunction, system_type_ids::application_thread_function);

#endif  //  #ifndef APPLICATION_BINDING_HPP_INCLUDED
