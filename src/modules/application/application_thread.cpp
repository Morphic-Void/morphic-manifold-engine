
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   application_thread.cpp
//  Author: Ritchie Brannan
//  Drafting and refactoring assistance: OpenAI tools
//  Date:   7 Aug 26
//
//  Application thread and the current TGA flow test.

#include <cstdint>      //  std::int32_t, std::uint32_t, std::uint64_t

#include "modules/application/application_thread.hpp"

#include "debug/macros.hpp"
#include "image/codec/tga.hpp"
#include "platform/system/performance_counter.hpp"
#include "system/async_state.hpp"
#include "system/transported_types.hpp"
#include "threading/CThreadPackage.hpp"

namespace application
{

struct SApplicationTgaLoadState { std::uint32_t reserved; };
struct SApplicationTgaSaveState { CAssetId source; };

}   //  namespace application

MV_REGISTER_SYSTEM_TYPE(application::SApplicationTgaLoadState, type_ids::application_tga_load_state);
MV_REGISTER_SYSTEM_TYPE(application::SApplicationTgaSaveState, type_ids::application_tga_save_state);

namespace application
{

class CApplicationThread
{
public:
    static std::uint32_t entry_point(void* user_data) noexcept;

private:
    CApplicationThread(const CApplicationThread&) noexcept = delete;
    CApplicationThread& operator=(const CApplicationThread&) noexcept = delete;
    CApplicationThread(CApplicationThread&&) noexcept = delete;
    CApplicationThread& operator=(CApplicationThread&&) noexcept = delete;

    explicit CApplicationThread(threading::CThreadResources& resources) noexcept : m_context{ resources } {}
    ~CApplicationThread() noexcept = default;

    std::uint32_t main() noexcept;

    threading::CThreadContext m_context;
};

std::uint32_t CApplicationThread::entry_point(void* user_data) noexcept
{
    if (user_data == nullptr)
    {
        MV_ERROR("CApplicationThread entry received a null user_data pointer");
        return ~0u;
    }

    threading::CThreadResources& resources = *static_cast<threading::CThreadResources*>(user_data);
    CApplicationThread thread(resources);
    return thread.main();
}

std::uint32_t CApplicationThread::main() noexcept
{
    m_context.startup();

    MV_INFO("Application: Starting");

    platform::system::CPerfCounter perf_counter;
    platform::system::CPerfCountConversion perf_count_converter;
    perf_counter.update();
    perf_count_converter.init();
    const std::uint64_t ticks_per_second = perf_count_converter.query_ticks_per_second();

    CASyncStates async_states;
    if (!async_states.initialise(1u))
    {
        m_context.mark_failed(1u);
        return 1u;
    }

    const std::int32_t tga_slot = async_states.acquire<SApplicationTgaLoadState>();
    if (tga_slot < 0)
    {
        async_states.deallocate();
        m_context.mark_failed(1u);
        return 1u;
    }

    TgaLoadRequest tga_load_request;
    tga_load_request.file = "test_files/test_input.tga";
    tga_load_request.vflip = false;
    threading::CPodThreadMsg initial_msg;
    initial_msg.set_async_slot(tga_slot);
    initial_msg.assign_payload(tga_load_request);
    if (!m_context.post(initial_msg))
    {
        (void)async_states.release(tga_slot);
        async_states.deallocate();
        m_context.mark_failed(1u);
        return 1u;
    }

    m_context.mark_running();

    MV_INFO("Application: Running");

    bool test_complete = false;
    bool test_failed = false;
    const auto fail_test = [this, &test_complete, &test_failed](const std::uint32_t code) noexcept
    {
        m_context.mark_failed(code);
        test_complete = true;
        test_failed = true;
    };
    while (!m_context.exit_requested() && !test_complete)
    {
        const std::uint64_t tick_delta = perf_counter.query_delta();
        if (tick_delta >= ticks_per_second)
        {
            m_context.advance_heartbeat();
            perf_counter.update();

            MV_TRACE("Application: Heartbeat");
        }

        while (!m_context.exit_requested() && !test_complete)
        {
            threading::CPodThreadMsg inbound_msg;
            if (!m_context.read(inbound_msg))
            {
                break;
            }

            MV_TRACE("Application: Message received");

            switch (inbound_msg.query_payload_type_id())
            {
                case (k_type_id_v<TgaLoadResult>):
                {
                    MV_DETAIL("Application: TGA load result");

                    TgaLoadResult tga_load_result;
                    (void)inbound_msg.copy_payload_to(tga_load_result);
                    const std::int32_t async_slot = inbound_msg.query_async_slot();
                    if (async_states.payload<SApplicationTgaLoadState>(async_slot) == nullptr)
                    {
                        fail_test(2u);
                        break;
                    }
                    if (!tga_load_result.success || !tga_load_result.asset)
                    {
                        (void)async_states.release(async_slot);
                        fail_test(2u);
                        break;
                    }

                    SApplicationTgaSaveState* const save_state = async_states.redefine<SApplicationTgaSaveState>(async_slot);
                    save_state->source = tga_load_result.asset;

                    TgaSaveRequest tga_save_request;
                    tga_save_request.file = "test_files/test_output.tga";
                    tga_save_request.source = save_state->source;
                    tga_save_request.options.src =
                        (tga_load_result.desc == image::codec::tga::decoded_image_desc::Gray) ?
                        image::codec::tga::image_encode_src::Gray :
                        image::codec::tga::image_encode_src::AutoTrue32;
                    tga_save_request.options.allow_clut = true;
                    tga_save_request.options.allow_rle = true;
                    tga_save_request.options.vflip = false;

                    threading::CPodThreadMsg outbound_msg;
                    outbound_msg.set_async_slot(async_slot);
                    outbound_msg.assign_payload(tga_save_request);
                    if (!m_context.post(outbound_msg))
                    {
                        (void)async_states.release(async_slot);
                        fail_test(3u);
                    }
                    break;
                }
                case (k_type_id_v<TgaSaveResult>):
                {
                    MV_DETAIL("Application: TGA save result");

                    TgaSaveResult tga_save_result;
                    (void)inbound_msg.copy_payload_to(tga_save_result);
                    const std::int32_t async_slot = inbound_msg.query_async_slot();
                    if (async_states.payload<SApplicationTgaSaveState>(async_slot) == nullptr)
                    {
                        fail_test(4u);
                        break;
                    }

                    (void)async_states.release(async_slot);
                    if (!tga_save_result.success)
                    {
                        fail_test(4u);
                    }
                    else
                    {
                        test_complete = true;
                    }
                    break;
                }
                default:
                {
                    MV_DETAIL("Application: Unrecognised message type {}", inbound_msg.query_payload_type_id());

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
    }
    async_states.deallocate();
    if (!test_failed)
    {
        m_context.mark_exited();
        MV_INFO("Application: Exited");
    }
    else
    {
        MV_WARNING("Application: TGA flow failed");
    }

    return 0u;
}

platform::threading::FThreadEntry application_thread_entry_point() noexcept
{
    return &CApplicationThread::entry_point;
}

}   //  namespace application
