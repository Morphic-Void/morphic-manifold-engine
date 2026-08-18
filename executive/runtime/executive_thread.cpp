
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    executive_thread.cpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    7 Aug 26
//
//  Executive thread and the current TGA flow test.

#include <cstdint>      //  std::int32_t, std::uint32_t, std::uint64_t
#include <cstring>      //  std::strcmp
#include <utility>      //  std::move

#include "executive/runtime/executive_thread.hpp"
#include "executive/module/types/local_type_ids.hpp"
#include "module/module_binding_context.hpp"

#include "debug/macros.hpp"
#include "image/codec/tga.hpp"
#include "platform/system/performance_counter.hpp"
#include "system/async_state.hpp"
#include "system/system_id_registry.hpp"
#include "system/transported_types.hpp"
#include "threading/CThreadPackage.hpp"

namespace executive
{

struct SExecutiveTgaLoadState { std::uint32_t reserved; };
struct SExecutiveTgaSaveState { CAssetId source; };

class CExecutiveThread
{
public:
    static std::uint32_t MV_STD_ABI_CALL entry_point(void* const user_data) noexcept;

private:
    CExecutiveThread(const CExecutiveThread&) noexcept = delete;
    CExecutiveThread& operator=(const CExecutiveThread&) noexcept = delete;
    CExecutiveThread(CExecutiveThread&&) noexcept = delete;
    CExecutiveThread& operator=(CExecutiveThread&&) noexcept = delete;

    explicit CExecutiveThread(threading::CThreadResources& resources) noexcept : m_context{ resources } {}
    ~CExecutiveThread() noexcept = default;

    std::uint32_t main() noexcept;
    [[nodiscard]] bool startup() noexcept;
    [[nodiscard]] bool initialise() noexcept;
    void operate() noexcept;
    void shutdown() noexcept;
    void fail(const std::uint32_t code) noexcept;

    threading::CThreadContext m_context;
    platform::system::CPerfCounter m_perf_counter;
    CASyncStates m_async_states;
    bool m_operation_complete{ false };
    std::uint32_t m_failure_code{ 0u };
};

std::uint32_t MV_STD_ABI_CALL CExecutiveThread::entry_point(void* const user_data) noexcept
{
    if (user_data == nullptr)
    {
        MV_ERROR("CExecutiveThread entry received a null user_data pointer");
        return ~0u;
    }

    threading::CThreadResources& resources = *static_cast<threading::CThreadResources*>(user_data);
    if (!modules::is_thread_context_ready(user_data))
    {
        resources.control_state.mark_failed(~0u);
        MV_ERROR("CExecutiveThread entry detected incomplete module context installation");
        return ~0u;
    }

    CExecutiveThread thread(resources);
    return thread.main();
}

std::uint32_t CExecutiveThread::main() noexcept
{
    if (startup())
    {
        operate();
    }
    shutdown();
    return m_failure_code;
}

bool CExecutiveThread::startup() noexcept
{
    m_context.startup();
    MV_INFO("Executive: Starting");

    if (!initialise())
    {
        return false;
    }
    m_context.mark_running();
    MV_INFO("Executive: Running");
    return true;
}

bool CExecutiveThread::initialise() noexcept
{
    const char* const registry_name = system_id_registry::lookup_type_name(system_type_ids::file_load_request);
    if ((registry_name == nullptr) || (std::strcmp(registry_name, "file_load_request") != 0))
    {
        fail(1u);
        return false;
    }

    MV_REPORT("Executive system registry authority: %s", registry_name);

    if (!m_perf_counter.update() ||
        !m_context.perf_count_conversion().is_valid() ||
        !m_async_states.initialise(1u))
    {
        fail(1u);
        return false;
    }

    const std::int32_t tga_slot = m_async_states.acquire<SExecutiveTgaLoadState>();
    if (tga_slot < 0)
    {
        fail(1u);
        return false;
    }

    CErasedOwner tga_load_owner = CErasedOwner::create<TgaLoadRequest>();
    TgaLoadRequest* const tga_load_request = tga_load_owner.payload<TgaLoadRequest>();
    if ((tga_load_request == nullptr) || !tga_load_request->file.set("test_data/input/files/test_input.tga"))
    {
        (void)m_async_states.release(tga_slot);
        fail(1u);
        return false;
    }
    tga_load_request->vflip = false;
    threading::CErasedOwnerMsg initial_msg;
    initial_msg.set_message_type<TgaLoadRequest>();
    initial_msg.set_async_slot(tga_slot);
    initial_msg.set_owner(std::move(tga_load_owner));
    if (!m_context.post(std::move(initial_msg)))
    {
        (void)m_async_states.release(tga_slot);
        fail(1u);
        return false;
    }
    return true;
}

void CExecutiveThread::operate() noexcept
{
    const std::uint64_t ticks_per_second = m_context.perf_count_conversion().query_ticks_per_second();

    while (!m_context.exit_requested() && !m_operation_complete)
    {
        const std::uint64_t tick_delta = m_perf_counter.query_delta();
        if (tick_delta >= ticks_per_second)
        {
            m_context.advance_heartbeat();
            m_perf_counter.update();

            MV_TRACE("Executive: Heartbeat");
        }

        while (!m_context.exit_requested() && !m_operation_complete)
        {
            threading::CErasedPodMsg inbound_msg;
            if (!m_context.read(inbound_msg))
            {
                break;
            }

            MV_TRACE("Executive: Message received");

            switch (inbound_msg.query_message_type_id().raw_value())
            {
                case k_type_id_v<TgaLoadResult>.raw_value():
                {
                    MV_DETAIL("Executive: TGA load result");

                    TgaLoadResult tga_load_result;
                    (void)inbound_msg.copy_payload_to(tga_load_result);
                    const std::int32_t async_slot = inbound_msg.query_async_slot();
                    if (m_async_states.payload<SExecutiveTgaLoadState>(async_slot) == nullptr)
                    {
                        fail(2u);
                        break;
                    }
                    if (!tga_load_result.success || !tga_load_result.asset)
                    {
                        (void)m_async_states.release(async_slot);
                        fail(2u);
                        break;
                    }

                    SExecutiveTgaSaveState* const save_state = m_async_states.redefine<SExecutiveTgaSaveState>(async_slot);
                    save_state->source = tga_load_result.asset;

                    CErasedOwner tga_save_owner = CErasedOwner::create<TgaSaveRequest>();
                    TgaSaveRequest* const tga_save_request = tga_save_owner.payload<TgaSaveRequest>();
                    if ((tga_save_request == nullptr) || !tga_save_request->file.set("test_data/output/files/test_output.tga"))
                    {
                        (void)m_async_states.release(async_slot);
                        fail(3u);
                        break;
                    }
                    tga_save_request->source = save_state->source;
                    tga_save_request->options.src =
                        (tga_load_result.desc == image::codec::tga::decoded_image_desc::Gray) ?
                        image::codec::tga::image_encode_src::Gray :
                        image::codec::tga::image_encode_src::AutoTrue32;
                    tga_save_request->options.allow_clut = true;
                    tga_save_request->options.allow_rle = true;
                    tga_save_request->options.vflip = false;

                    threading::CErasedOwnerMsg outbound_msg;
                    outbound_msg.set_message_type<TgaSaveRequest>();
                    outbound_msg.set_async_slot(async_slot);
                    outbound_msg.set_owner(std::move(tga_save_owner));
                    if (!m_context.post(std::move(outbound_msg)))
                    {
                        (void)m_async_states.release(async_slot);
                        fail(3u);
                    }
                    break;
                }
                case k_type_id_v<TgaSaveResult>.raw_value():
                {
                    MV_DETAIL("Executive: TGA save result");

                    TgaSaveResult tga_save_result;
                    (void)inbound_msg.copy_payload_to(tga_save_result);
                    const std::int32_t async_slot = inbound_msg.query_async_slot();
                    if (m_async_states.payload<SExecutiveTgaSaveState>(async_slot) == nullptr)
                    {
                        fail(4u);
                        break;
                    }

                    (void)m_async_states.release(async_slot);
                    if (!tga_save_result.success)
                    {
                        fail(4u);
                    }
                    else
                    {
                        m_operation_complete = true;
                    }
                    break;
                }
                default:
                {
                    system_type_id unrecognised_id;
                    if (!inbound_msg.query_message_type_id().try_system_type_id(unrecognised_id))
                    {
                        MV_DETAIL("Executive: Unrecognised LOCAL message type");
                        break;
                    }
                    MV_DETAIL("Executive: Unrecognised message type {}", unrecognised_id);

                    UnrecognisedMsg unrecognised;
                    unrecognised.msg_id = unrecognised_id;
                    threading::CErasedPodMsg outbound_msg;
                    outbound_msg.set_async_slot(inbound_msg.query_async_slot());
                    outbound_msg.assign_payload(unrecognised);
                    (void)m_context.post(outbound_msg);
                    break;
                }
            }
        }
    }
}

void CExecutiveThread::shutdown() noexcept
{
    if (m_failure_code == 0u)
    {
        m_context.mark_exiting();
    }
    m_async_states.deallocate();

    if (m_failure_code == 0u)
    {
        m_context.mark_exited();
        MV_INFO("Executive: Exited");
    }
    else
    {
        MV_WARNING("Executive: TGA flow failed");
    }
}

void CExecutiveThread::fail(const std::uint32_t code) noexcept
{
    if (m_failure_code == 0u)
    {
        m_failure_code = code;
        m_context.mark_failed(code);
    }
    m_operation_complete = true;
}

FExecutiveThread executive_thread_entry_point() noexcept
{
    return &CExecutiveThread::entry_point;
}

}   //  namespace executive
