
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   host_worker_thread.cpp
//  Author: Ritchie Brannan
//  Drafting and refactoring assistance: OpenAI tools
//  Date:   7 Aug 26
//
//  Host-owned file and image conditioning worker thread.

#include <cstdint>      //  std::uint32_t

#include "host/host_worker_thread.hpp"

#include "debug/macros.hpp"
#include "image/codec/tga.hpp"
#include "platform/filesystem/file.hpp"
#include "system/erased_owner.hpp"
#include "system/transported_types.hpp"
#include "threading/CThreadPackage.hpp"

namespace host
{

class CHostWorkerThread
{
public:
    static std::uint32_t entry_point(void* user_data) noexcept;

private:
    CHostWorkerThread(const CHostWorkerThread&) noexcept = delete;
    CHostWorkerThread& operator=(const CHostWorkerThread&) noexcept = delete;
    CHostWorkerThread(CHostWorkerThread&&) noexcept = delete;
    CHostWorkerThread& operator=(CHostWorkerThread&&) noexcept = delete;

    explicit CHostWorkerThread(threading::CThreadResources& resources) noexcept : m_context{ resources } {}
    ~CHostWorkerThread() noexcept = default;

    std::uint32_t main() noexcept;

    threading::CThreadContext m_context;
};

std::uint32_t CHostWorkerThread::entry_point(void* user_data) noexcept
{
    if (user_data == nullptr)
    {
        MV_ERROR("CHostWorkerThread entry received a null user_data pointer");
        return ~0u;
    }

    threading::CThreadResources& resources = *static_cast<threading::CThreadResources*>(user_data);
    CHostWorkerThread thread(resources);
    return thread.main();
}

std::uint32_t CHostWorkerThread::main() noexcept
{
    m_context.startup();

    MV_INFO("Worker starting");

    std::uint32_t epoch = 0u;
    while (!m_context.exit_requested())
    {
        m_context.advance_heartbeat();
        threading::CPodThreadMsg inbound_msg;
        if (m_context.read(inbound_msg))
        {
            MV_TRACE("Worker message received");

            switch (inbound_msg.query_payload_type_id())
            {
                case (k_type_id_v<FileLoadRequest>):
                {
                    MV_DETAIL("Worker file load request");

                    FileLoadRequest request;
                    (void)inbound_msg.copy_payload_to(request);
                    CErasedOwner outbound_msg = CErasedOwner::create<OwningFileLoadResult>();
                    if (OwningFileLoadResult* const result = outbound_msg.payload<OwningFileLoadResult>())
                    {
                        result->async_slot = inbound_msg.query_async_slot();
                        result->buffer = platform::filesystem::loadFile(request.file);
                        (void)m_context.pass_ownership(outbound_msg);
                    }
                    break;
                }
                case (k_type_id_v<FileSaveRequest>):
                {
                    MV_DETAIL("Worker file save request");

                    FileSaveRequest request;
                    (void)inbound_msg.copy_payload_to(request);
                    FileSaveResult result;
                    result.success = platform::filesystem::saveFile(request.file, request.view);
                    threading::CPodThreadMsg outbound_msg;
                    outbound_msg.set_async_slot(inbound_msg.query_async_slot());
                    outbound_msg.assign_payload(result);
                    (void)m_context.post(outbound_msg);
                    break;
                }
                case (k_type_id_v<TgaEncodeRequest>):
                {
                    MV_DETAIL("Worker TGA encode request");

                    TgaEncodeRequest request;
                    (void)inbound_msg.copy_payload_to(request);
                    CErasedOwner outbound_msg = CErasedOwner::create<OwningTgaEncodeResult>();
                    if (OwningTgaEncodeResult* const result = outbound_msg.payload<OwningTgaEncodeResult>())
                    {
                        result->async_slot = inbound_msg.query_async_slot();
                        result->buffer = image::codec::tga::encode(request.view, request.options);
                        (void)m_context.pass_ownership(outbound_msg);
                    }
                    break;
                }
                case (k_type_id_v<TgaDecodeRequest>):
                {
                    MV_DETAIL("Worker TGA decode request");

                    TgaDecodeRequest request;
                    (void)inbound_msg.copy_payload_to(request);
                    CErasedOwner outbound_msg = CErasedOwner::create<OwningTgaDecodeResult>();
                    if (OwningTgaDecodeResult* const result = outbound_msg.payload<OwningTgaDecodeResult>())
                    {
                        result->async_slot = inbound_msg.query_async_slot();
                        result->buffer = image::codec::tga::decode(request.view, result->desc, request.vflip);
                        (void)m_context.pass_ownership(outbound_msg);
                    }
                    break;
                }
                default:
                {
                    MV_DETAIL("Worker unrecognised message type {}", inbound_msg.query_payload_type_id());

                    UnrecognisedMsg unrecognised;
                    unrecognised.msg_id = inbound_msg.query_payload_type_id();
                    threading::CPodThreadMsg outbound_msg;
                    outbound_msg.set_async_slot(inbound_msg.query_async_slot());
                    outbound_msg.assign_payload(unrecognised);
                    (void)m_context.post(outbound_msg);
                    break;
                }
            }
        }
        else
        {
            epoch = m_context.wait_for_new_epoch(epoch);
        }
    }
    m_context.mark_exited();

    MV_INFO("Worker exited");

    return 0u;
}

platform::threading::FThreadEntry host_worker_thread_entry_point() noexcept
{
    return &CHostWorkerThread::entry_point;
}

}   //  namespace host
