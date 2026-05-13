
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   run_threading_test.cpp
//  Author: Ritchie Brannan
//  Date:   13 May 26

#include <atomic>       //  std::atomic
#include <cstdint>      //  std::uint32_t

#include "tests/threading_test.hpp"
#include "containers/ByteBuffers.hpp"
#include "containers/TPodVector.hpp"
#include "image/codec/tga.hpp"
#include "platform/threading/thread_lifetime.hpp"
#include "platform/threading/thread_naming.hpp"
#include "platform/threading/thread_priority.hpp"
#include "threading/CCountingSemaphore.hpp"
#include "threading/transports/bundles/TOwningBundle.hpp"
#include "types/typeless.hpp"

//  need a slot manager for pending tasks

struct FileLoadRequestMsg
{
};

struct FileSaveRequestMsg
{
};

struct TgaFileLoadRequestMsg
{
};

struct TgaFileSaveRequestMsg
{
};

struct FileLoadResultMsg
{
};

struct FileSaveResultMsg
{
};

struct TgaFileLoadResultMsg
{
};

struct TgaFileSaveResultMsg
{
};

struct ThreadMsg
{
    int dummy = 0;
};

static const std::size_t thread_msg_type_id{ 0u };

MV_DECLARE_TYPELESS(ThreadMsg, thread_msg_type_id);

struct ThreadPkg
{
    const char* thread_name;
    platform::threading::CThread thread;
    platform::threading::EThreadPriority priority;
    std::atomic<std::uint32_t> exit_request;
    std::atomic<std::uint32_t> heartbeat;
    std::atomic<std::uint32_t> exited;
    threading::CCountingSemaphore semaphore;
    threading::transports::TOwningBundle<ThreadMsg> owned_to_host;
    threading::transports::TOwningBundle<ThreadMsg> owned_to_user;
};

static std::uint32_t bg_thread_file_handling(void* user_data) noexcept
{
    return 0u;
}

static std::uint32_t bg_thread_data_conditioning(void* user_data) noexcept
{
    return 0u;
}

int run_threading_test()
{
    platform::threading::CThread owner_thread_bg_file_handling;
    platform::threading::CThread owner_thread_bg_data_conditioning;

    bool created_bg_thread_file_handling = owner_thread_bg_file_handling.create(&bg_thread_file_handling, nullptr);
    bool created_bg_thread_data_conditioning = owner_thread_bg_data_conditioning.create(&bg_thread_data_conditioning, nullptr);

    if (created_bg_thread_file_handling && created_bg_thread_data_conditioning)
    {
    }

    bool closed_bg_thread_data_conditioning = created_bg_thread_data_conditioning ? owner_thread_bg_data_conditioning.join_and_close() : false;
    bool closed_bg_thread_file_handling = created_bg_thread_file_handling ? owner_thread_bg_file_handling.join_and_close() : false;

    return 0;
}
