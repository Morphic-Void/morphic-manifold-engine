
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   host.hpp
//  Author: Ritchie Brannan
//  Date:   15 May 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  The main host service thread for the engine.
//
//  This is currently only a test/sketch/prototype
//  to validate the existing codebase features.
//
//  The code is placeholder and not final.

#include <atomic>       //  std::atomic
#include <cstdint>      //  std::int32_t, std::uint32_t
#include <thread>       //  std::this_thread::yield
#include <utility>      //  std::move

#include "assets/asset_repository.hpp"
#include "host/host.hpp"
#include "containers/containers.hpp"
#include "image/codec/tga.hpp"
#include "platform/filesystem/file.hpp"
#include "platform/module/binding.hpp"
#include "platform/path/native_path.hpp"
#include "platform/system/performance_counter.hpp"
#include "platform/system/process_priority.hpp"
#include "platform/threading/platform_threading.hpp"
#include "system/erased_owner.hpp"
#include "system/erased_pod.hpp"
#include "system/erased_owner_transport.hpp"
#include "system/async_state.hpp"
#include "system/system_ids.hpp"
#include "system/system_context.hpp"
#include "system/TStaticLookup.hpp"
#include "system/transported_types.hpp"
#include "threading/threading.hpp"

#include "debug/macros.hpp"
#include "debug/service.hpp"

struct SApplicationTgaLoadState { std::uint32_t reserved; };
struct SApplicationTgaSaveState { CAssetId source; };

struct SHostTgaFileLoadState
{
    std::int32_t application_slot;
    const char* file;
    bool vflip;
};

struct SHostTgaDecodeState
{
    std::int32_t application_slot;
    CAssetId loaded_file;
};

struct SHostTgaEncodeState
{
    std::int32_t application_slot;
    CAssetId source;
    const char* file;
    image::codec::tga::EncodeOptions options;
};

struct SHostTgaFileSaveState
{
    std::int32_t application_slot;
    CAssetId encoded_file;
};

MV_REGISTER_SYSTEM_TYPE(SApplicationTgaLoadState, type_ids::application_tga_load_state);
MV_REGISTER_SYSTEM_TYPE(SApplicationTgaSaveState, type_ids::application_tga_save_state);
MV_REGISTER_SYSTEM_TYPE(SHostTgaFileLoadState, type_ids::host_tga_file_load_state);
MV_REGISTER_SYSTEM_TYPE(SHostTgaDecodeState, type_ids::host_tga_decode_state);
MV_REGISTER_SYSTEM_TYPE(SHostTgaEncodeState, type_ids::host_tga_encode_state);
MV_REGISTER_SYSTEM_TYPE(SHostTgaFileSaveState, type_ids::host_tga_file_save_state);

struct ThreadConfig
{
    thread_ids::id_type thread_id{};
    platform::threading::EThreadPriority priority{ platform::threading::EThreadPriority::Normal };
    platform::threading::FThreadEntry entry_point{ nullptr };
};

class CThreadResources
{
public:
    CThreadResources(const ThreadConfig& thread_config) noexcept : config{ thread_config } {}
    ~CThreadResources() noexcept = default;

    //  Thread resourcing
    bool created{ false };
    platform::threading::CThread thread;
    threading::CParkingTicket parking_ticket;
    threading::CWaitPredicate wait_predicate;           //  use wait_predicate for simple workers
    threading::CCountingSemaphore counting_semaphore;   //  use counting_semaphore for multi-thread jobs
    threading::transports::TQueue<threading::CPodThreadMsg> host_to_worker_msgs;
    threading::transports::TQueue<threading::CPodThreadMsg> worker_to_host_msgs;
    threading::transports::CErasedOwnerTransport worker_owned_to_host_owned;
    threading::CThreadControlState control_state;
    ThreadConfig config;
};

class CThreadContext
{
public:
    CThreadContext(CThreadResources& resources) noexcept : m_resources{ resources } {};
    ~CThreadContext() noexcept = default;

    //  Thread facing functions
    void startup() noexcept;
    void mark_waiting() noexcept { m_resources.control_state.mark_waiting(); }
    void mark_running() noexcept { m_resources.control_state.mark_running(); }
    void mark_exiting() noexcept { m_resources.control_state.mark_exiting(); }
    void mark_exited() noexcept { m_resources.control_state.mark_exited(); }
    void mark_failed(const std::uint32_t code) noexcept { return m_resources.control_state.mark_failed(code); }
    void advance_heartbeat() noexcept { m_resources.control_state.advance_heartbeat(); }
    bool exit_requested() const noexcept { return m_resources.control_state.exit_requested(); }
    std::uint32_t wait_for_new_epoch(const uint32_t epoch) noexcept;
    bool read(threading::CPodThreadMsg& msg) noexcept { return m_resources.host_to_worker_msgs.read(msg); }
    bool post(const threading::CPodThreadMsg& msg) noexcept { return m_resources.worker_to_host_msgs.post(msg); }
    bool pass_ownership(CErasedOwner& obj) noexcept { return m_resources.worker_owned_to_host_owned.post(std::move(obj)); }

private:
    CThreadResources& m_resources;
};

inline void CThreadContext::startup() noexcept
{
    m_resources.control_state.mark_startup();

    const thread_ids::id_type thread_id = m_resources.config.thread_id;
    (void)system_context::set_ambient_thread_id(thread_id);

    const char* const thread_name = system_id_registry::lookup_thread_name(thread_id);
    MV_ASSERT(thread_name != nullptr);
    if (thread_name != nullptr)
    {
        (void)platform::threading::set_current_thread_name(thread_name);
    }
    (void)platform::threading::set_current_thread_priority(m_resources.config.priority);
}

std::uint32_t CThreadContext::wait_for_new_epoch(const uint32_t epoch) noexcept
{
    MV_TRACE("Waiting epoch={}", epoch);

    mark_waiting();
    std::uint32_t new_epoch = m_resources.wait_predicate.wait_until_not_equal(m_resources.parking_ticket, epoch);
    mark_running();

    MV_TRACE("Running epoch={}", new_epoch);

    return new_epoch;
}

class CThreadPackage
{
public:
    CThreadPackage(const ThreadConfig& thread_config) noexcept : m_resources(thread_config) {}
    ~CThreadPackage() noexcept = default;

    //  Thread owner facing functions
    bool startup() noexcept;
    bool shutdown() noexcept;
    bool read(threading::CPodThreadMsg& msg) noexcept;
    bool post(const threading::CPodThreadMsg& msg) noexcept;
    bool take_ownership(CErasedOwner& obj) noexcept;
    threading::EThreadRunState query_state() const noexcept;

private:
    CThreadResources m_resources;
};

inline bool CThreadPackage::startup() noexcept
{
    if (m_resources.host_to_worker_msgs.initialise_growable(0u))
    {
        if (m_resources.worker_to_host_msgs.initialise_growable(0u))
        {
            if (m_resources.worker_owned_to_host_owned.initialise(0u))
            {
                if (m_resources.wait_predicate.acquire_control())
                {
                    m_resources.control_state.mark_pending();
                    m_resources.created = m_resources.thread.create(m_resources.config.entry_point, &m_resources);
                    if (m_resources.created)
                    {
                        while (m_resources.control_state.is_starting())
                        {
                            std::this_thread::yield();
                        }
                        if (m_resources.control_state.is_ready())
                        {
                            return true;
                        }
                        (void)m_resources.thread.join_and_close();
                        m_resources.created = false;
                    }
                    m_resources.control_state.mark_empty();
                    m_resources.wait_predicate.release_control();
                }
                m_resources.worker_owned_to_host_owned.deallocate();
            }
            m_resources.worker_to_host_msgs.deallocate();
        }
        m_resources.host_to_worker_msgs.deallocate();
    }
    return false;
}

inline bool CThreadPackage::shutdown() noexcept
{
    if (m_resources.created && m_resources.wait_predicate.has_control())
    {
        m_resources.control_state.request_exit();
        m_resources.wait_predicate.release_control();
        while (!m_resources.control_state.is_done())
        {
            std::this_thread::yield();
        }
        m_resources.created = m_resources.thread.join_and_close();
        if (!m_resources.created)
        {
            m_resources.worker_owned_to_host_owned.deallocate();
            m_resources.worker_to_host_msgs.deallocate();
            m_resources.host_to_worker_msgs.deallocate();
        }
    }
    return m_resources.created;
}

inline bool CThreadPackage::read(threading::CPodThreadMsg& msg) noexcept
{
    return m_resources.worker_to_host_msgs.read(msg);
}

inline bool CThreadPackage::post(const threading::CPodThreadMsg& msg) noexcept
{
    bool success = m_resources.host_to_worker_msgs.post(msg);
    if (success)
    {
        m_resources.wait_predicate.poke_epoch_and_wake_one();
    }
    return success;
}

inline bool CThreadPackage::take_ownership(CErasedOwner& msg) noexcept
{
    return m_resources.worker_owned_to_host_owned.read(msg);
}

inline threading::EThreadRunState CThreadPackage::query_state() const noexcept
{
    return m_resources.control_state.query_state();
}

class CHostWorkerThread
{
public:

    //  Platform entry binding
    using FThreadEntry = platform::threading::FThreadEntry;
    static FThreadEntry get_entry_point() noexcept { return &entry_point; }

private:

    //  Deleted lifetime
    CHostWorkerThread(const CHostWorkerThread&) noexcept = delete;
    CHostWorkerThread& operator=(const CHostWorkerThread&) noexcept = delete;
    CHostWorkerThread(CHostWorkerThread&&) noexcept = delete;
    CHostWorkerThread& operator=(CHostWorkerThread&&) noexcept = delete;

    //  Workspace lifetime
    explicit CHostWorkerThread(CThreadResources& resources) noexcept : m_context{ resources } {}
    ~CHostWorkerThread() noexcept = default;

    //  Thread entry point
    static std::uint32_t entry_point(void* user_data) noexcept;

private:

    //  Workspace execution
    std::uint32_t main() noexcept;

    //  Non-owning thread context
    CThreadContext m_context;
};

std::uint32_t CHostWorkerThread::entry_point(void* user_data) noexcept
{
    if (user_data == nullptr)
    {
        MV_ERROR("CHostWorkerThread entry received a null user_data pointer");
        return ~0u;
    }

    CThreadResources& resources = *static_cast<CThreadResources*>(user_data);
    CHostWorkerThread thread(resources);
    return thread.main();
}

std::uint32_t CHostWorkerThread::main() noexcept
{
    //  Standard thread startup
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
                    MV_DETAIL("Worker unrecognised message type {}",
                        inbound_msg.query_payload_type_id());

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

class CApplicationThread
{
public:

    //  Platform entry binding
    using FThreadEntry = platform::threading::FThreadEntry;
    static FThreadEntry get_entry_point() noexcept { return &entry_point; }

private:

    //  Deleted lifetime
    CApplicationThread(const CApplicationThread&) noexcept = delete;
    CApplicationThread& operator=(const CApplicationThread&) noexcept = delete;
    CApplicationThread(CApplicationThread&&) noexcept = delete;
    CApplicationThread& operator=(CApplicationThread&&) noexcept = delete;

    //  Workspace lifetime
    explicit CApplicationThread(CThreadResources& resources) noexcept : m_context{ resources } {}
    ~CApplicationThread() noexcept = default;

    //  Thread entry point
    static std::uint32_t entry_point(void* user_data) noexcept;

private:

    //  Workspace execution
    std::uint32_t main() noexcept;

    //  Non-owning thread context
    CThreadContext m_context;
};

std::uint32_t CApplicationThread::entry_point(void* user_data) noexcept
{
    if (user_data == nullptr)
    {
        MV_ERROR("CApplicationThread entry received a null user_data pointer");
        return ~0u;
    }

    CThreadResources& resources = *static_cast<CThreadResources*>(user_data);
    CApplicationThread thread(resources);
    return thread.main();
}

std::uint32_t CApplicationThread::main() noexcept
{
    //  Standard thread startup
    m_context.startup();

    MV_INFO("Application: Starting");

    platform::system::CPerfCounter perf_counter;
    platform::system::CPerfCountConversion perf_count_converter;
    perf_counter.update();
    perf_count_converter.init();
    std::uint64_t ticks_per_second = perf_count_converter.query_ticks_per_second();

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
        std::uint64_t tick_delta = perf_counter.query_delta();
        if (tick_delta >= ticks_per_second)
        {
            m_context.advance_heartbeat();
            perf_counter.update();

            MV_TRACE("Application: Heartbeat");
        }

        while (!m_context.exit_requested() && !test_complete)
        {   //  drain incoming messages

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

                    SApplicationTgaSaveState* const save_state =
                        async_states.redefine<SApplicationTgaSaveState>(async_slot);
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
                    MV_DETAIL("Application: Unrecognised message type {}",
                        inbound_msg.query_payload_type_id());

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

namespace host
{

int host()
{
    MV_INFO("Host: Starting");

    TInstance<debug_system::CDebugServiceState> debug_service_owner =
        TInstance<debug_system::CDebugServiceState>::create();
    debug_system::CDebugServiceState* debug_service = nullptr;
    bool debug_service_installed = false;
    bool debug_service_started = false;
    if (debug_service_owner)
    {
        debug_service = debug_service_owner.operator->();
        const bool debug_logs_opened =
            debug_service->configure_log_paths(
                "manifold_debug.log",
                "manifold_debug_direct.log") &&
            debug_service->open_logs();
        if (debug_logs_opened)
        {
            debug_service_installed =
                debug_system::install_service(debug_service);
            if (debug_service_installed)
            {
                debug_service_started = debug_service->start();
            }
        }
    }

    static const ThreadConfig thread_configs[3]{
        {thread_ids::bg_file_io, platform::threading::EThreadPriority::Background, CHostWorkerThread::get_entry_point()},
        {thread_ids::bg_conditioning, platform::threading::EThreadPriority::Background, CHostWorkerThread::get_entry_point()},
        {thread_ids::application, platform::threading::EThreadPriority::Normal, CApplicationThread::get_entry_point()} };

    enum class EWorkerThreadID : std::uint8_t { bg_file_io = 0u, bg_conditioning, application };

    int32_t thread_slots[3]{ -1, -1, -1 };

    TUnorderedCollection<CThreadPackage> thread_packages;
    CAssetRepository assets;
    CASyncStates async_states;

    bool initialised = true;
    if (initialised) initialised = thread_packages.initialise();
    if (initialised) initialised = assets.initialise();
    if (initialised) initialised = async_states.initialise();
    if (initialised)
    {
        for (std::int32_t thread_index = 0; thread_index <= 2; ++thread_index)
        {
            int32_t thread_slot = thread_packages.emplace(thread_configs[thread_index]);
            if (thread_slot < 0)
            {
                initialised = false;
                break;
            }
            thread_slots[thread_index] = thread_slot;
            CThreadPackage& package = *thread_packages.get_object(thread_slot);
            initialised = package.startup();
            if (!initialised)
            {
                break;
            }
        }
    }

    if (initialised)
    {
        platform::system::CPerfCounter perf_counter;
        platform::system::CPerfCountConversion perf_count_converter;
        perf_counter.update();
        perf_count_converter.init();
        std::uint64_t ticks_per_second = perf_count_converter.query_ticks_per_second();

        CThreadPackage& application_package = *thread_packages.get_object(thread_slots[static_cast<std::uint8_t>(EWorkerThreadID::application)]);
        const auto post_tga_load_result = [&application_package](
            const std::int32_t application_slot, const CAssetId asset,
            const image::codec::tga::decoded_image_desc desc, const bool success) noexcept
        {
            TgaLoadResult result;
            result.asset = asset;
            result.desc = desc;
            result.success = success;
            threading::CPodThreadMsg outbound_msg;
            outbound_msg.set_async_slot(application_slot);
            outbound_msg.assign_payload(result);
            return application_package.post(outbound_msg);
        };
        const auto post_tga_save_result = [&application_package](
            const std::int32_t application_slot, const bool success) noexcept
        {
            TgaSaveResult result;
            result.success = success;
            threading::CPodThreadMsg outbound_msg;
            outbound_msg.set_async_slot(application_slot);
            outbound_msg.assign_payload(result);
            return application_package.post(outbound_msg);
        };

        while ((application_package.query_state() != threading::EThreadRunState::Exited) &&
            (application_package.query_state() != threading::EThreadRunState::Failed) &&
            ((debug_service == nullptr) ||
                (debug_service->read_shutdown_request() ==
                    debug_system::EShutdownReason::none)))
        {
            std::uint64_t tick_delta = perf_counter.query_delta();
            if ((tick_delta * 500u) >= ticks_per_second)
            {
                perf_counter.update();

                MV_TRACE("Host: Service OS Pump");
            }

            for (int32_t inbound_slot = thread_packages.first_live(); inbound_slot >= 0; inbound_slot = thread_packages.next_live(inbound_slot))
            {
                CThreadPackage& inbound_package = *thread_packages.get_object(inbound_slot);
                threading::CPodThreadMsg inbound_msg;
                while (inbound_package.read(inbound_msg))
                {
                    MV_TRACE("Host: Recieved a message");

                    switch (inbound_msg.query_payload_type_id())
                    {
                        case (k_type_id_v<FileSaveResult>):
                        {
                            FileSaveResult result;
                            (void)inbound_msg.copy_payload_to(result);
                            MV_DETAIL("Host: Recieved a file save result success={}",
                                result.success ? 1u : 0u);
                            const std::int32_t async_slot = inbound_msg.query_async_slot();
                            const SHostTgaFileSaveState* const state =
                                async_states.payload<SHostTgaFileSaveState>(async_slot);
                            if (state == nullptr)
                            {
                                MV_CRITICAL_EVENT("Host: File save result has invalid async slot {}", async_slot);
                                break;
                            }

                            const std::int32_t application_slot = state->application_slot;
                            (void)post_tga_save_result(application_slot, result.success);
                            (void)async_states.release(async_slot);
                            break;
                        }
                        case (k_type_id_v<TgaLoadRequest>):
                        {
                            MV_DETAIL("Host: Recieved a TGA load request");

                            TgaLoadRequest tga_load_request;
                            (void)inbound_msg.copy_payload_to(tga_load_request);
                            const std::int32_t application_slot = inbound_msg.query_async_slot();
                            const std::int32_t async_slot = async_states.acquire<SHostTgaFileLoadState>();
                            SHostTgaFileLoadState* const state =
                                async_states.payload<SHostTgaFileLoadState>(async_slot);
                            if (state == nullptr)
                            {
                                MV_CRITICAL_EVENT("Host: Failed to acquire TGA load state");
                                (void)post_tga_load_result(
                                    application_slot, CAssetId{},
                                    image::codec::tga::decoded_image_desc::RGBA, false);
                                break;
                            }
                            state->application_slot = application_slot;
                            state->file = tga_load_request.file;
                            state->vflip = tga_load_request.vflip;

                            const std::int32_t outbound_slot = thread_slots[static_cast<std::uint8_t>(EWorkerThreadID::bg_file_io)];
                            CThreadPackage& outbound_package = *thread_packages.get_object(outbound_slot);
                            FileLoadRequest file_load_request;
                            file_load_request.file = state->file;
                            threading::CPodThreadMsg outbound_msg;
                            outbound_msg.set_async_slot(async_slot);
                            outbound_msg.assign_payload(file_load_request);
                            if (!outbound_package.post(outbound_msg))
                            {
                                (void)async_states.release(async_slot);
                                (void)post_tga_load_result(
                                    application_slot, CAssetId{},
                                    image::codec::tga::decoded_image_desc::RGBA, false);
                            }
                            break;
                        }
                        case (k_type_id_v<TgaSaveRequest>):
                        {
                            MV_DETAIL("Host: Recieved a TGA save request");

                            TgaSaveRequest tga_save_request;
                            (void)inbound_msg.copy_payload_to(tga_save_request);
                            const CAssetRecord* const source_record = assets.resolve(tga_save_request.source);
                            const OwningTgaDecodeResult* const source =
                                (source_record != nullptr) ? source_record->payload<OwningTgaDecodeResult>() : nullptr;
                            if ((source == nullptr) || !source->buffer.is_ready())
                            {
                                MV_CRITICAL_EVENT("Host: TGA save request has invalid source asset");
                                (void)post_tga_save_result(inbound_msg.query_async_slot(), false);
                                break;
                            }

                            const std::int32_t application_slot = inbound_msg.query_async_slot();
                            const std::int32_t async_slot = async_states.acquire<SHostTgaEncodeState>();
                            SHostTgaEncodeState* const state = async_states.payload<SHostTgaEncodeState>(async_slot);
                            if (state == nullptr)
                            {
                                MV_CRITICAL_EVENT("Host: Failed to acquire TGA encode state");
                                (void)post_tga_save_result(application_slot, false);
                                break;
                            }
                            state->application_slot = application_slot;
                            state->source = tga_save_request.source;
                            state->file = tga_save_request.file;
                            state->options = tga_save_request.options;

                            const CByteRectConstView source_view = source->buffer.const_view();
                            const std::int32_t outbound_slot = thread_slots[static_cast<std::uint8_t>(EWorkerThreadID::bg_conditioning)];
                            CThreadPackage& outbound_package = *thread_packages.get_object(outbound_slot);
                            TgaEncodeRequest tga_encode_request;
                            tga_encode_request.view = source_view;
                            tga_encode_request.options = state->options;
                            threading::CPodThreadMsg outbound_msg;
                            outbound_msg.set_async_slot(async_slot);
                            outbound_msg.assign_payload(tga_encode_request);
                            if (!outbound_package.post(outbound_msg))
                            {
                                (void)async_states.release(async_slot);
                                (void)post_tga_save_result(application_slot, false);
                            }
                            break;
                        }
                        case (k_type_id_v<UnrecognisedMsg>):
                        {
                            UnrecognisedMsg unrecognised;
                            (void)inbound_msg.copy_payload_to(unrecognised);

                            MV_DETAIL("Host: Recieved an unrecognised message notification {}",
                                unrecognised.msg_id);
                            break;
                        }
                        default:
                        {
                            MV_DETAIL("Host: Recieved an unrecognised message type {}",
                                inbound_msg.query_payload_type_id());
                            break;
                        }
                    }
                }
                CErasedOwner inbound_msg_owning;
                while (inbound_package.take_ownership(inbound_msg_owning))
                {
                    MV_TRACE("Host: Recieved object ownership");

                    switch (inbound_msg_owning.query_type_id())
                    {
                        case (k_type_id_v<OwningFileLoadResult>):
                        {
                            MV_DETAIL("Host: Took ownership of a loaded file buffer");

                            OwningFileLoadResult* const result =
                                inbound_msg_owning.payload<OwningFileLoadResult>();
                            const std::int32_t async_slot = result->async_slot;
                            const SHostTgaFileLoadState* const load_state =
                                async_states.payload<SHostTgaFileLoadState>(async_slot);
                            if (load_state == nullptr)
                            {
                                MV_CRITICAL_EVENT("Host: File load result has invalid async slot {}", async_slot);
                                break;
                            }

                            const std::int32_t application_slot = load_state->application_slot;
                            const bool vflip = load_state->vflip;
                            if (!result->buffer.is_ready())
                            {
                                (void)post_tga_load_result(
                                    application_slot, CAssetId{},
                                    image::codec::tga::decoded_image_desc::RGBA, false);
                                (void)async_states.release(async_slot);
                                break;
                            }

                            const CAssetId loaded_file = assets.insert(std::move(inbound_msg_owning));
                            const CAssetRecord* const loaded_record = assets.resolve(loaded_file);
                            const OwningFileLoadResult* const loaded =
                                (loaded_record != nullptr) ? loaded_record->payload<OwningFileLoadResult>() : nullptr;
                            if (loaded == nullptr)
                            {
                                (void)post_tga_load_result(
                                    application_slot, CAssetId{},
                                    image::codec::tga::decoded_image_desc::RGBA, false);
                                (void)async_states.release(async_slot);
                                break;
                            }

                            SHostTgaDecodeState* const decode_state =
                                async_states.redefine<SHostTgaDecodeState>(async_slot);
                            decode_state->application_slot = application_slot;
                            decode_state->loaded_file = loaded_file;

                            const CByteConstView loaded_view = loaded->buffer.const_view();
                            const std::int32_t outbound_slot = thread_slots[static_cast<std::uint8_t>(EWorkerThreadID::bg_conditioning)];
                            CThreadPackage& outbound_package = *thread_packages.get_object(outbound_slot);
                            TgaDecodeRequest tga_decode_request;
                            tga_decode_request.view = loaded_view;
                            tga_decode_request.vflip = vflip;
                            threading::CPodThreadMsg outbound_msg;
                            outbound_msg.set_async_slot(async_slot);
                            outbound_msg.assign_payload(tga_decode_request);
                            if (!outbound_package.post(outbound_msg))
                            {
                                (void)post_tga_load_result(
                                    application_slot, CAssetId{},
                                    image::codec::tga::decoded_image_desc::RGBA, false);
                                (void)async_states.release(async_slot);
                            }
                            break;
                        }
                        case (k_type_id_v<OwningTgaEncodeResult>):
                        {
                            MV_DETAIL("Host: Took ownership of an encoded TGA file buffer");

                            OwningTgaEncodeResult* const result =
                                inbound_msg_owning.payload<OwningTgaEncodeResult>();
                            const std::int32_t async_slot = result->async_slot;
                            const SHostTgaEncodeState* const encode_state =
                                async_states.payload<SHostTgaEncodeState>(async_slot);
                            if (encode_state == nullptr)
                            {
                                MV_CRITICAL_EVENT("Host: TGA encode result has invalid async slot {}", async_slot);
                                break;
                            }

                            const std::int32_t application_slot = encode_state->application_slot;
                            const char* const file = encode_state->file;
                            if (!result->buffer.is_ready())
                            {
                                (void)post_tga_save_result(application_slot, false);
                                (void)async_states.release(async_slot);
                                break;
                            }

                            const CAssetId encoded_file = assets.insert(std::move(inbound_msg_owning));
                            const CAssetRecord* const encoded_record = assets.resolve(encoded_file);
                            const OwningTgaEncodeResult* const encoded =
                                (encoded_record != nullptr) ? encoded_record->payload<OwningTgaEncodeResult>() : nullptr;
                            if (encoded == nullptr)
                            {
                                (void)post_tga_save_result(application_slot, false);
                                (void)async_states.release(async_slot);
                                break;
                            }

                            SHostTgaFileSaveState* const save_state =
                                async_states.redefine<SHostTgaFileSaveState>(async_slot);
                            save_state->application_slot = application_slot;
                            save_state->encoded_file = encoded_file;

                            const CByteConstView encoded_view = encoded->buffer.const_view();
                            const std::int32_t outbound_slot = thread_slots[static_cast<std::uint8_t>(EWorkerThreadID::bg_file_io)];
                            CThreadPackage& outbound_package = *thread_packages.get_object(outbound_slot);
                            FileSaveRequest file_save_request;
                            file_save_request.file = file;
                            file_save_request.view = encoded_view;
                            threading::CPodThreadMsg outbound_msg;
                            outbound_msg.set_async_slot(async_slot);
                            outbound_msg.assign_payload(file_save_request);
                            if (!outbound_package.post(outbound_msg))
                            {
                                (void)post_tga_save_result(application_slot, false);
                                (void)async_states.release(async_slot);
                            }
                            break;
                        }
                        case (k_type_id_v<OwningTgaDecodeResult>):
                        {
                            MV_DETAIL("Host: Took ownership of a decoded TGA image buffer");

                            OwningTgaDecodeResult* const result =
                                inbound_msg_owning.payload<OwningTgaDecodeResult>();
                            const std::int32_t async_slot = result->async_slot;
                            const SHostTgaDecodeState* const decode_state =
                                async_states.payload<SHostTgaDecodeState>(async_slot);
                            if (decode_state == nullptr)
                            {
                                MV_CRITICAL_EVENT("Host: TGA decode result has invalid async slot {}", async_slot);
                                break;
                            }

                            const std::int32_t application_slot = decode_state->application_slot;
                            const image::codec::tga::decoded_image_desc desc = result->desc;
                            if (!result->buffer.is_ready())
                            {
                                (void)post_tga_load_result(application_slot, CAssetId{}, desc, false);
                                (void)async_states.release(async_slot);
                                break;
                            }

                            const CAssetId decoded_image = assets.insert(std::move(inbound_msg_owning));
                            const bool stored = assets.resolve(decoded_image) != nullptr;
                            (void)post_tga_load_result(application_slot, decoded_image, desc, stored);
                            (void)async_states.release(async_slot);
                            break;
                        }
                        default:
                        {
                            MV_CRITICAL_EVENT("Host: Took ownership of an unknown object {}", inbound_msg_owning.query_type_id());
                            break;
                        }
                    }
                }
            }
        }
    }

    for (std::int32_t thread_index = 2; thread_index >= 0; --thread_index)
    {
        const std::int32_t controller_slot = thread_slots[thread_index];
        if (controller_slot >= 0)
        {
            CThreadPackage& worker_package = *thread_packages.get_object(controller_slot);
            (void)worker_package.shutdown();
        }
    }

    async_states.deallocate();
    assets.deallocate();

    if (debug_service_started)
    {
        if (!debug_service->stop())
        {
            MV_ERROR("Host failed to stop the debug service cleanly");
        }
    }
    if (debug_service_installed)
    {
        if (!debug_system::uninstall_service(debug_service))
        {
            MV_ERROR("Host failed to uninstall the debug service cleanly");
        }
    }
    return 0;
}

}   //  namespace host
