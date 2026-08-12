
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    application_thread.cpp
//  Authors: Ritchie Brannan / OpenAI tools
//  Date:    7 Aug 26
//
//  Application thread and the current TGA flow test.

#include <cstdint>      //  std::int32_t, std::uint32_t, std::uint64_t
#include <cstring>      //  std::strcmp

#include "modules/application/application_thread.hpp"
#include "modules/module_binding_context.hpp"

#include "debug/macros.hpp"
#include "debug/event_arguments.hpp"
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

MV_REGISTER_SYSTEM_TYPE(application::SApplicationTgaLoadState, system_type_ids::application_tga_load_state);
MV_REGISTER_SYSTEM_TYPE(application::SApplicationTgaSaveState, system_type_ids::application_tga_save_state);

namespace application
{

class CApplicationThread
{
public:
    static std::uint32_t MV_STD_ABI_CALL entry_point(void* user_data) noexcept;

private:
    CApplicationThread(const CApplicationThread&) noexcept = delete;
    CApplicationThread& operator=(const CApplicationThread&) noexcept = delete;
    CApplicationThread(CApplicationThread&&) noexcept = delete;
    CApplicationThread& operator=(CApplicationThread&&) noexcept = delete;

    explicit CApplicationThread(threading::CThreadResources& resources) noexcept : m_context{ resources } {}
    ~CApplicationThread() noexcept = default;

    std::uint32_t main() noexcept;
    [[nodiscard]] bool startup() noexcept;
    [[nodiscard]] bool initialise() noexcept;
    void operate() noexcept;
    void shutdown() noexcept;
    void fail(std::uint32_t code) noexcept;

    threading::CThreadContext m_context;
    platform::system::CPerfCounter m_perf_counter;
    CASyncStates m_async_states;
    bool m_operation_complete{ false };
    std::uint32_t m_failure_code{ 0u };
};

std::uint32_t MV_STD_ABI_CALL CApplicationThread::entry_point(void* user_data) noexcept
{
    if (user_data == nullptr)
    {
        MV_ERROR("CApplicationThread entry received a null user_data pointer");
        return ~0u;
    }

    threading::CThreadResources& resources = *static_cast<threading::CThreadResources*>(user_data);
    if (!modules::is_thread_context_ready(user_data))
    {
        resources.control_state.mark_failed(~0u);
        MV_ERROR("CApplicationThread entry detected incomplete module context installation");
        return ~0u;
    }

    CApplicationThread thread(resources);
    return thread.main();
}

std::uint32_t CApplicationThread::main() noexcept
{
    if (startup())
    {
        operate();
    }
    shutdown();
    return m_failure_code;
}

bool CApplicationThread::startup() noexcept
{
    m_context.startup();
    MV_INFO("Application: Starting");

    if (!initialise())
    {
        return false;
    }
    m_context.mark_running();
    MV_INFO("Application: Running");
    return true;
}

bool CApplicationThread::initialise() noexcept
{
    char registry_name[64]{};
    std::size_t registry_name_size = 0u;
    constexpr char registry_format[]{ "{}" };
    const debug_system::SEventArguments registry_arguments =
        debug_system::encode_event_arguments(system_type_ids::file_load_request);
    if ((debug_system::format_event_text(
            registry_name, sizeof(registry_name),
            registry_format, (sizeof(registry_format) - 1u),
            registry_arguments.parameter_count,
            registry_arguments.parameter_types,
            registry_arguments.parameters,
            registry_name_size) != debug_system::EEventFormatResult::success) ||
        (std::strcmp(registry_name, "file_load_request") != 0))
    {
        fail(1u);
        return false;
    }
    MV_REPORT("Application system registry authority: %s", registry_name);

    if (!m_perf_counter.update() ||
        !m_context.perf_count_conversion().is_valid() ||
        !m_async_states.initialise(1u))
    {
        fail(1u);
        return false;
    }

    const std::int32_t tga_slot = m_async_states.acquire<SApplicationTgaLoadState>();
    if (tga_slot < 0)
    {
        fail(1u);
        return false;
    }

    TgaLoadRequest tga_load_request;
    tga_load_request.file = "test_files/test_input.tga";
    tga_load_request.vflip = false;
    threading::CErasedPodMsg initial_msg;
    initial_msg.set_async_slot(tga_slot);
    initial_msg.assign_payload(tga_load_request);
    if (!m_context.post(initial_msg))
    {
        (void)m_async_states.release(tga_slot);
        fail(1u);
        return false;
    }
    return true;
}

void CApplicationThread::operate() noexcept
{
    const std::uint64_t ticks_per_second = m_context.perf_count_conversion().query_ticks_per_second();

    while (!m_context.exit_requested() && !m_operation_complete)
    {
        const std::uint64_t tick_delta = m_perf_counter.query_delta();
        if (tick_delta >= ticks_per_second)
        {
            m_context.advance_heartbeat();
            m_perf_counter.update();

            MV_TRACE("Application: Heartbeat");
        }

        while (!m_context.exit_requested() && !m_operation_complete)
        {
            threading::CErasedPodMsg inbound_msg;
            if (!m_context.read(inbound_msg))
            {
                break;
            }

            MV_TRACE("Application: Message received");

            switch (inbound_msg.query_message_type_id())
            {
                case (k_system_type_id_v<TgaLoadResult>):
                {
                    MV_DETAIL("Application: TGA load result");

                    TgaLoadResult tga_load_result;
                    (void)inbound_msg.copy_payload_to(tga_load_result);
                    const std::int32_t async_slot = inbound_msg.query_async_slot();
                    if (m_async_states.payload<SApplicationTgaLoadState>(async_slot) == nullptr)
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

                    SApplicationTgaSaveState* const save_state = m_async_states.redefine<SApplicationTgaSaveState>(async_slot);
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

                    threading::CErasedPodMsg outbound_msg;
                    outbound_msg.set_async_slot(async_slot);
                    outbound_msg.assign_payload(tga_save_request);
                    if (!m_context.post(outbound_msg))
                    {
                        (void)m_async_states.release(async_slot);
                        fail(3u);
                    }
                    break;
                }
                case (k_system_type_id_v<TgaSaveResult>):
                {
                    MV_DETAIL("Application: TGA save result");

                    TgaSaveResult tga_save_result;
                    (void)inbound_msg.copy_payload_to(tga_save_result);
                    const std::int32_t async_slot = inbound_msg.query_async_slot();
                    if (m_async_states.payload<SApplicationTgaSaveState>(async_slot) == nullptr)
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
                    MV_DETAIL("Application: Unrecognised message type {}", inbound_msg.query_message_type_id());

                    UnrecognisedMsg unrecognised;
                    unrecognised.msg_id = inbound_msg.query_message_type_id();
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

void CApplicationThread::shutdown() noexcept
{
    if (m_failure_code == 0u)
    {
        m_context.mark_exiting();
    }
    m_async_states.deallocate();

    if (m_failure_code == 0u)
    {
        m_context.mark_exited();
        MV_INFO("Application: Exited");
    }
    else
    {
        MV_WARNING("Application: TGA flow failed");
    }
}

void CApplicationThread::fail(const std::uint32_t code) noexcept
{
    if (m_failure_code == 0u)
    {
        m_failure_code = code;
        m_context.mark_failed(code);
    }
    m_operation_complete = true;
}

FApplicationThread application_thread_entry_point() noexcept
{
    return &CApplicationThread::entry_point;
}

}   //  namespace application
