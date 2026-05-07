
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   platform_defines.hpp
//  Author: Ritchie Brannan
//  Date:   7 May 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Platform selection and capability defines for low-level platform
//  implementation files.
//
//  This header classifies the target platform and derives coarse
//  platform capabilities.
// 
//  This header does NOT declare platform APIs or provide feature
//  wrappers and these should not be added to this file.
//
//  Apple targets require target and availability classification
//  headers so that unsupported Apple deployment baseline targets
//  can be rejected at compile time.
//
//  Supported targets
//  -----------------
//  Primary:
//  - Windows.
//  - Linux desktop.
//
//  Secondary:
//  - Android is classified, but does not currently provide native
//    wait-word support.
//
//  Tertiary:
//  - macOS 14.4 or later.
//  - Other Apple targets are intentionally unsupported for now.
//
//  Unsupported Apple targets
//  -------------------------
//  iOS, iPadOS, tvOS, watchOS, visionOS, simulators, and older macOS
//  deployment targets are rejected until deliberately enabled.
//
//  Capability defines
//  ------------------
//  MV_PLATFORM_HAS_PTHREADS
//      Target has pthread support available to platform implementation code.
//
//  MV_PLATFORM_HAS_NATIVE_WAIT_WORD
//      Target has a native 32-bit wait-by-address / wake-by-address primitive
//      suitable for platform::threading wait-word support.

#pragma once

#ifndef PLATFORM_DEFINES_HPP_INCLUDED
#define PLATFORM_DEFINES_HPP_INCLUDED

//==============================================================================
//  Platforms
//==============================================================================

#if defined(_WIN32) || defined(_WIN64)
#define MV_PLATFORM_WINDOWS
#endif

#if defined(__APPLE__) && defined(__MACH__)
#define MV_PLATFORM_APPLE
#endif

#if defined(__linux__) && !defined(__ANDROID__)
#define MV_PLATFORM_LINUX_ONLY
#endif

#if defined(__ANDROID__)
#define MV_PLATFORM_ANDROID_ONLY
#endif

#if defined(MV_PLATFORM_LINUX_ONLY) || defined(MV_PLATFORM_ANDROID_ONLY)
#define MV_PLATFORM_LINUX_ANDROID
#endif

//==============================================================================
//  Supported platforms
//==============================================================================

#if defined(MV_PLATFORM_ANDROID_ONLY)

#if defined(__ANDROID_API__) && (__ANDROID_API__ >= 32)
#define MV_PLATFORM_ANDROID_SUPPORTED
#else
#error "Unsupported Android target. Requires Android API 32 or later."
#endif

#endif  //  #if defined(MV_PLATFORM_ANDROID_ONLY)

#if defined(MV_PLATFORM_APPLE)

#include <Availability.h>
#include <TargetConditionals.h>

#if defined(TARGET_OS_OSX) && TARGET_OS_OSX

#define MV_PLATFORM_APPLE_MACOS

#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && (__MAC_OS_X_VERSION_MIN_REQUIRED >= 140400)
#define MV_PLATFORM_APPLE_SUPPORTED
#else
#error "Unsupported macOS target. Requires macOS 14.4 or later."
#endif

#else

#error "Unsupported Apple target. Only macOS 14.4 or later is currently supported."

#endif  //  #if defined(TARGET_OS_OSX) && TARGET_OS_OSX

#endif  //  #if defined(MV_PLATFORM_APPLE)

//==============================================================================
//  Platform capabilities
//==============================================================================

#if defined(MV_PLATFORM_APPLE) || defined(MV_PLATFORM_LINUX_ANDROID)
#define MV_PLATFORM_HAS_PTHREADS
#endif

#if defined(MV_PLATFORM_WINDOWS) || \
    defined(MV_PLATFORM_LINUX_ONLY) || \
    defined(MV_PLATFORM_APPLE_SUPPORTED)
#define MV_PLATFORM_HAS_NATIVE_WAIT_WORD
#endif

#endif  //  #ifndef PLATFORM_DEFINES_HPP_INCLUDED
