
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

#include "host/host.hpp"
#include "containers/containers.hpp"
#include "image/codec/tga.hpp"
#include "platform/filesystem/file.hpp"
#include "platform/module/binding.hpp"
#include "platform/path/native_path.hpp"
#include "platform/system/performance_counter.hpp"
#include "platform/system/process_priority.hpp"
#include "platform/threading/platform_threading.hpp"
#include "system/system_ids.hpp"
#include "system/TStaticLookup.hpp"
#include "threading/threading.hpp"
#include "types/typeless_traits.hpp"
#include "types/typeless_pod.hpp"
#include "types/typeless.hpp"

#include "debug/debug.hpp"

namespace type_ids
{   //  will be moved into system/system_ids.hpp

static constexpr std::size_t msg_id_unrecognised_msg = encode_id(100u);

static constexpr std::size_t msg_id_file_load_request = encode_id(101u);
static constexpr std::size_t msg_id_file_save_request = encode_id(102u);
static constexpr std::size_t msg_id_tga_load_request = encode_id(103u);
static constexpr std::size_t msg_id_tga_save_request = encode_id(104u);
static constexpr std::size_t msg_id_tga_encode_request = encode_id(105u);
static constexpr std::size_t msg_id_tga_decode_request = encode_id(106u);

static constexpr std::size_t msg_id_file_load_result = encode_id(107u);
static constexpr std::size_t msg_id_file_save_result = encode_id(108u);
static constexpr std::size_t msg_id_tga_load_result = encode_id(109u);
static constexpr std::size_t msg_id_tga_save_result = encode_id(110u);

static constexpr std::size_t msg_id_file_load_result_owning = encode_id(111u);
static constexpr std::size_t msg_id_tga_encode_result_owning = encode_id(112u);
static constexpr std::size_t msg_id_tga_decode_result_owning = encode_id(113u);

};

struct UnrecognisedMsg { std::size_t msg_id; };

struct FileLoadRequest { const char* file; };
struct FileSaveRequest { const char* file; CByteConstView* view; };
struct TgaLoadRequest { const char* file; bool vflip; };
struct TgaSaveRequest { const char* file; image::codec::tga::EncodeOptions* options; CByteRectConstView* view; };
struct TgaEncodeRequest { CByteRectConstView* view; image::codec::tga::EncodeOptions* options; };
struct TgaDecodeRequest { CByteConstView* view; bool vflip; };

struct FileLoadResult { CByteConstView* view; };
struct FileSaveResult { bool success; };
struct TgaLoadResult { CByteRectConstView* view; image::codec::tga::decoded_image_desc desc; };
struct TgaSaveResult { bool success; };

struct FileLoadResultOwning { std::size_t async_slot; CByteBuffer buffer; };
struct TgaEncodeResultOwning { std::size_t async_slot; CByteBuffer buffer; };
struct TgaDecodeResultOwning { std::size_t async_slot; CByteRectBuffer buffer; image::codec::tga::decoded_image_desc desc; };

MV_DECLARE_TYPELESS(UnrecognisedMsg, type_ids::msg_id_unrecognised_msg);

MV_DECLARE_TYPELESS(FileLoadRequest, type_ids::msg_id_file_load_request);
MV_DECLARE_TYPELESS(FileSaveRequest, type_ids::msg_id_file_save_request);
MV_DECLARE_TYPELESS(TgaSaveRequest, type_ids::msg_id_tga_save_request);
MV_DECLARE_TYPELESS(TgaLoadRequest, type_ids::msg_id_tga_load_request);
MV_DECLARE_TYPELESS(TgaEncodeRequest, type_ids::msg_id_tga_encode_request);
MV_DECLARE_TYPELESS(TgaDecodeRequest, type_ids::msg_id_tga_decode_request);

MV_DECLARE_TYPELESS(FileLoadResult, type_ids::msg_id_file_load_result);
MV_DECLARE_TYPELESS(FileSaveResult, type_ids::msg_id_file_save_result);
MV_DECLARE_TYPELESS(TgaLoadResult, type_ids::msg_id_tga_load_result);
MV_DECLARE_TYPELESS(TgaSaveResult, type_ids::msg_id_tga_save_result);

MV_DECLARE_TYPELESS(FileLoadResultOwning, type_ids::msg_id_file_load_result_owning);
MV_DECLARE_TYPELESS(TgaEncodeResultOwning, type_ids::msg_id_tga_encode_result_owning);
MV_DECLARE_TYPELESS(TgaDecodeResultOwning, type_ids::msg_id_tga_decode_result_owning);

struct ThreadConfig
{
    std::size_t thread_system_id{ 0u };
    const char* thread_name{ nullptr };
    platform::threading::EThreadPriority thread_priority{ platform::threading::EThreadPriority::Normal };
    platform::threading::FThreadEntry thread_entry{ nullptr };
};

struct WorkerThreadPackage
{
    WorkerThreadPackage() noexcept = default;
    ~WorkerThreadPackage() noexcept = default;
    std::int32_t provisioning_slot{ -1 };
    std::int32_t controller_slot{ -1 };
    platform::threading::CThread thread;
    bool thread_created{ false };
    ThreadConfig thread_config;
    threading::CThreadControlState control_state;
    threading::CWaitPredicate wait_predicate;           //  use wait_predicate for simple workers
    threading::CCountingSemaphore counting_semaphore;   //  use counting_semaphore for multi-thread jobs
    threading::CParkingTicket parking_ticket;
    threading::transports::TQueueBundle<threading::CPodThreadMsg> host_to_worker_msgs;
    threading::transports::TQueueBundle<threading::CPodThreadMsg> worker_to_host_msgs;
    threading::transports::TOwningBundle<memory::CTypeless> worker_owned_to_host_owned;
};

struct WorkerThreadProvisioning
{
    WorkerThreadProvisioning(WorkerThreadPackage& package) noexcept;
    ~WorkerThreadProvisioning() noexcept = default;
    ThreadConfig& thread_config;
    threading::CThreadControlState& control_state;
    threading::CWaitPredicate& wait_predicate;
    threading::CParkingTicket& parking_ticket;
    threading::transports::TQueueConsumerEndpoint<threading::CPodThreadMsg>& inbound_msgs;
    threading::transports::TQueueProducerEndpoint<threading::CPodThreadMsg>& outbound_msgs;
    threading::transports::TOwningProducerEndpoint<memory::CTypeless>& outbound_msgs_owning;
};

inline WorkerThreadProvisioning::WorkerThreadProvisioning(WorkerThreadPackage& package) noexcept :
    thread_config{ package.thread_config },
    control_state{ package.control_state },
    wait_predicate{ package.wait_predicate },
    parking_ticket{ package.parking_ticket },
    inbound_msgs{ package.host_to_worker_msgs.consumer },
    outbound_msgs{ package.worker_to_host_msgs.producer },
    outbound_msgs_owning{ package.worker_owned_to_host_owned.producer }
{
}

struct WorkerThreadController
{
    WorkerThreadController(WorkerThreadPackage& package) noexcept;
    ~WorkerThreadController() noexcept = default;
    platform::threading::CThread& thread;
    bool& thread_created;
    ThreadConfig& thread_config;
    threading::CThreadControlState& control_state;
    threading::CWaitPredicate& wait_predicate;
    threading::transports::TQueueProducerEndpoint<threading::CPodThreadMsg>& outbound_msgs;
    threading::transports::TQueueConsumerEndpoint<threading::CPodThreadMsg>& inbound_msgs;
    threading::transports::TOwningConsumerEndpoint<memory::CTypeless>& inbound_msgs_owning;
};

inline WorkerThreadController::WorkerThreadController(WorkerThreadPackage& package) noexcept :
    thread{ package.thread },
    thread_created{ package.thread_created },
    thread_config{ package.thread_config },
    control_state{ package.control_state },
    wait_predicate{ package.wait_predicate },
    outbound_msgs{ package.host_to_worker_msgs.producer },
    inbound_msgs{ package.worker_to_host_msgs.consumer },
    inbound_msgs_owning{ package.worker_owned_to_host_owned.consumer }
{
}

static std::uint32_t worker_thread(void* user_data) noexcept
{
    //  Initialise the provisioning
    WorkerThreadProvisioning provisioning = *reinterpret_cast<WorkerThreadProvisioning*>(user_data);
    provisioning.control_state.mark_starting();

    debug_utils::debug_output("Worker (%s): Starting\n", provisioning.thread_config.thread_name);

    //  Apply standard configuration
    std::size_t thread_system_id = provisioning.thread_config.thread_system_id; // This needs to be stored in TLS for the real implementation
    (void)platform::threading::set_current_thread_name(provisioning.thread_config.thread_name);
    (void)platform::threading::set_current_thread_priority(provisioning.thread_config.thread_priority);

    std::uint32_t epoch = 0u;
    while (!provisioning.control_state.is_exit_requested())
    {

        debug_utils::debug_output("Worker (%s): Waiting epoch=%u\n",  provisioning.thread_config.thread_name, epoch);

        provisioning.control_state.mark_waiting();
        epoch = provisioning.wait_predicate.wait_until_not_equal(provisioning.parking_ticket, epoch);

        debug_utils::debug_output("Worker (%s): Running epoch=%u\n", provisioning.thread_config.thread_name, epoch);

        provisioning.control_state.mark_running();
        provisioning.control_state.advance_heartbeat();
        while (!provisioning.control_state.is_exit_requested())
        {   //  drain incoming messages

            threading::CPodThreadMsg inbound_msg;
            if (!provisioning.inbound_msgs.read(inbound_msg))
            {
                break;
            }

            debug_utils::debug_output("Worker (%s): Message received\n", provisioning.thread_config.thread_name);

            switch (inbound_msg.payload.query_type_id())
            {
                case (k_type_id_v<FileLoadRequest>):
                {
                    debug_utils::debug_output("Worker (%s): File load request\n", provisioning.thread_config.thread_name);

                    FileLoadRequest request;
                    (void)inbound_msg.payload.copy_to(request);
                    memory::CTypeless outbound_msg = create_typeless<FileLoadResultOwning>();
                    FileLoadResultOwning& result = *typeless_cast<FileLoadResultOwning>(outbound_msg);
                    result.async_slot = inbound_msg.async_slot;
                    result.buffer = platform::filesystem::loadFile(request.file);
                    (void)provisioning.outbound_msgs_owning.post(std::move(outbound_msg));
                    break;
                }
                case (k_type_id_v<FileSaveRequest>):
                {
                    debug_utils::debug_output("Worker (%s): File save request\n", provisioning.thread_config.thread_name);

                    FileSaveRequest request;
                    (void)inbound_msg.payload.copy_to(request);
                    FileSaveResult result;
                    result.success = platform::filesystem::saveFile(request.file, *request.view);
                    threading::CPodThreadMsg outbound_msg;
                    outbound_msg.async_slot = inbound_msg.async_slot;
                    outbound_msg.payload.assign(result);
                    (void)provisioning.outbound_msgs.post(outbound_msg);
                    break;
                }
                case (k_type_id_v<TgaEncodeRequest>):
                {
                    debug_utils::debug_output("Worker (%s): TGA encode request\n", provisioning.thread_config.thread_name);

                    TgaEncodeRequest request;
                    (void)inbound_msg.payload.copy_to(request);
                    memory::CTypeless outbound_msg = create_typeless<TgaEncodeResultOwning>();
                    TgaEncodeResultOwning& result = *typeless_cast<TgaEncodeResultOwning>(outbound_msg);
                    result.async_slot = inbound_msg.async_slot;
                    result.buffer = image::codec::tga::encode(*request.view, *request.options);
                    (void)provisioning.outbound_msgs_owning.post(std::move(outbound_msg));
                    break;
                }
                case (k_type_id_v<TgaDecodeRequest>):
                {
                    debug_utils::debug_output("Worker (%s): TGA decode request\n", provisioning.thread_config.thread_name);

                    TgaDecodeRequest request;
                    (void)inbound_msg.payload.copy_to(request);
                    memory::CTypeless outbound_msg = create_typeless<TgaDecodeResultOwning>();
                    TgaDecodeResultOwning& result = *typeless_cast<TgaDecodeResultOwning>(outbound_msg);
                    result.async_slot = inbound_msg.async_slot;
                    result.buffer = image::codec::tga::decode(*request.view, result.desc, request.vflip);
                    (void)provisioning.outbound_msgs_owning.post(std::move(outbound_msg));
                    break;
                }
                default:
                {
                    debug_utils::debug_output("Worker (%s): Unrecognised message type (%d)\n", provisioning.thread_config.thread_name, inbound_msg.payload.query_type_id());

                    UnrecognisedMsg unrecognised;
                    unrecognised.msg_id = inbound_msg.payload.query_type_id();
                    threading::CPodThreadMsg outbound_msg;
                    outbound_msg.async_slot = inbound_msg.async_slot;
                    outbound_msg.payload.assign(unrecognised);
                    (void)provisioning.outbound_msgs.post(outbound_msg);
                    break;
                }
            }
            provisioning.control_state.advance_heartbeat();
        }
    }
    provisioning.control_state.mark_exited();

    debug_utils::debug_output("Worker (%s): Exited\n", provisioning.thread_config.thread_name);

    return 0u;
}

static std::uint32_t app_thread(void* user_data) noexcept
{
    //  Initialise the provisioning
    WorkerThreadProvisioning provisioning = *reinterpret_cast<WorkerThreadProvisioning*>(user_data);
    provisioning.control_state.mark_starting();

    debug_utils::debug_output("Application: Starting\n");

    //  Apply standard configuration
    std::size_t thread_system_id = provisioning.thread_config.thread_system_id; // This needs to be stored in TLS for the real implementation
    (void)platform::threading::set_current_thread_name(provisioning.thread_config.thread_name);
    (void)platform::threading::set_current_thread_priority(provisioning.thread_config.thread_priority);

    platform::system::CPerfCounter perf_counter;
    platform::system::CPerfCountConversion perf_count_converter;
    perf_counter.update();
    perf_count_converter.init();
    std::uint64_t ticks_per_second = perf_count_converter.query_ticks_per_second();

    struct AsyncTgaLoad
    {
        bool complete = false;
        bool success = false;
        CByteRectConstView view;
        image::codec::tga::decoded_image_desc desc = image::codec::tga::decoded_image_desc::RGBA;
    };

    struct AsyncTgaSave
    {
        bool complete = false;
        bool success = false;
    };

    AsyncTgaLoad tga_load;
    AsyncTgaSave tga_save;

    provisioning.control_state.mark_running();

    {   //  kick off the test
        TgaLoadRequest tga_load_request;
        tga_load_request.file = "d:/test_input.tga";
        tga_load_request.vflip = false;
        threading::CPodThreadMsg outbound_msg;
        outbound_msg.async_slot = 0;
        outbound_msg.payload.assign(tga_load_request);
        (void)provisioning.outbound_msgs.post(outbound_msg);
    }

    enum class ETgaTestStates : std::uint32_t { no_state = 0u, waiting_for_tga_load, waiting_for_tga_save, done };
    ETgaTestStates tga_state = ETgaTestStates::waiting_for_tga_load;

    image::codec::tga::EncodeOptions tga_encode_options;

    provisioning.control_state.mark_running();

    debug_utils::debug_output("Application: Running\n");

    while (!provisioning.control_state.is_exit_requested())
    {
        std::uint64_t tick_delta = perf_counter.query_delta();
        if (tick_delta >= ticks_per_second)
        {
            provisioning.control_state.advance_heartbeat();
            perf_counter.update();

            debug_utils::debug_output("Application: Heartbeat\n");
        }

        bool state_updated = false;

        while (!provisioning.control_state.is_exit_requested())
        {   //  drain incoming messages

            threading::CPodThreadMsg inbound_msg;
            if (!provisioning.inbound_msgs.read(inbound_msg))
            {
                break;
            }

            debug_utils::debug_output("Application: Message received\n");

            switch (inbound_msg.payload.query_type_id())
            {
                case (k_type_id_v<TgaLoadResult>):
                {
                    debug_utils::debug_output("Application: TGA load result\n");

                    TgaLoadResult tga_load_result;
                    (void)inbound_msg.payload.copy_to(tga_load_result);
                    tga_load.view = *tga_load_result.view;
                    tga_load.desc = tga_load_result.desc;
                    tga_load.success = !tga_load_result.view->is_empty();
                    tga_load.complete = true;
                    state_updated = true;
                    break;
                }
                case (k_type_id_v<TgaSaveResult>):
                {
                    debug_utils::debug_output("Application: TGA save result\n");

                    TgaSaveResult tga_save_result;
                    (void)inbound_msg.payload.copy_to(tga_save_result);
                    tga_save.success = tga_save_result.success;
                    tga_save.complete = true;
                    state_updated = true;
                    break;
                }
                default:
                {
                    debug_utils::debug_output("Application: Unrecognised message type (%d)\n", inbound_msg.payload.query_type_id());

                    UnrecognisedMsg unrecognised;
                    unrecognised.msg_id = inbound_msg.payload.query_type_id();
                    threading::CPodThreadMsg outbound_msg;
                    outbound_msg.async_slot = inbound_msg.async_slot;
                    outbound_msg.payload.assign(unrecognised);
                    (void)provisioning.outbound_msgs.post(outbound_msg);
                    break;
                }
            }
        }

        if (state_updated)
        {
            if (tga_state == ETgaTestStates::waiting_for_tga_load)
            {
                if (tga_load.complete)
                {
                    if (!tga_load.success)
                    {
                        provisioning.control_state.mark_failed(1u);
                        break;
                    }

                    tga_encode_options.src = (tga_load.desc == image::codec::tga::decoded_image_desc::Gray) ?
                        image::codec::tga::image_encode_src::Gray :
                        image::codec::tga::image_encode_src::AutoTrue32;

                    TgaSaveRequest tga_save_request;
                    tga_save_request.file = "d:/test_output.tga";
                    tga_save_request.view = &tga_load.view;
                    tga_save_request.options = &tga_encode_options;

                    threading::CPodThreadMsg outbound_msg;
                    outbound_msg.async_slot = 0;
                    outbound_msg.payload.assign(tga_save_request);
                    (void)provisioning.outbound_msgs.post(outbound_msg);

                    tga_state = ETgaTestStates::waiting_for_tga_save;
                }
            }
            else if (tga_state == ETgaTestStates::waiting_for_tga_save)
            {
                if (tga_save.complete)
                {
                    if (!tga_save.success)
                    {
                        provisioning.control_state.mark_failed(2u);
                        break;
                    }

                    tga_state = ETgaTestStates::done;
                    break;
                }
                break;  //  test force exit
            }
        }
    }
    provisioning.control_state.mark_exited();

    debug_utils::debug_output("Application: Exited\n");

    return 0u;
}

int host()
{
    debug_utils::debug_output("Host: Starting\n");

    ThreadConfig thread_configs[3]{
        {system_ids::bg_file_io, "bg_file_io", platform::threading::EThreadPriority::Background, &worker_thread},
        {system_ids::bg_conditioning, "bg_conditioning", platform::threading::EThreadPriority::Background, &worker_thread},
        {system_ids::application, "application", platform::threading::EThreadPriority::Normal, &app_thread} };

    enum class EWorkerThreadID : std::uint8_t { bg_file_io = 0u, bg_conditioning, application };

    struct WorkerThreadSlots { std::int32_t package_slot = -1; std::int32_t provisioning_slot = -1; std::int32_t controller_slot = -1; };
    WorkerThreadSlots worker_thread_slots[3]{};

    TUnorderedCollection<WorkerThreadPackage> worker_thread_packages;
    TUnorderedCollection<WorkerThreadProvisioning> worker_thread_provisioning;
    TUnorderedCollection<WorkerThreadController> worker_thread_controllers;

    bool initialised = true;
    if (initialised) initialised = worker_thread_packages.initialise();
    if (initialised) initialised = worker_thread_provisioning.initialise();
    if (initialised) initialised = worker_thread_controllers.initialise();
    if (initialised)
    {
        for (std::int32_t thread_index = 0; thread_index <= 2; ++thread_index)
        {
            int32_t package_slot = worker_thread_packages.emplace();
            if (package_slot < 0)
            {
                initialised = false;
                break;
            }
            WorkerThreadPackage& package = *worker_thread_packages.get_object(package_slot);
            worker_thread_slots[thread_index].package_slot = package_slot;
            package.thread_config = thread_configs[thread_index];
            package.control_state.mark_pending_start();
            if (!package.wait_predicate.acquire_control())
            {
                initialised = false;
                break;
            }
            if (!package.host_to_worker_msgs.transport.initialise_growable(0u))
            {
                initialised = false;
                break;
            }
            if (!package.worker_to_host_msgs.transport.initialise_growable(0u))
            {
                initialised = false;
                break;
            }
            if (!package.worker_owned_to_host_owned.transport.initialise(0u))
            {
                initialised = false;
                break;
            }
            package.provisioning_slot = worker_thread_provisioning.emplace(package);
            if (package.provisioning_slot < 0)
            {
                initialised = false;
                break;
            }
            worker_thread_slots[thread_index].provisioning_slot = package.provisioning_slot;
            package.controller_slot = worker_thread_controllers.emplace(package);
            if (package.controller_slot < 0)
            {
                initialised = false;
                break;
            }
            worker_thread_slots[thread_index].controller_slot = package.controller_slot;
            package.thread_created = package.thread.create(package.thread_config.thread_entry, worker_thread_provisioning.get_object(package.provisioning_slot));
            if (!package.thread_created)
            {
                initialised = false;
                break;
            }
            while (!package.control_state.query_ready())
            {
                std::this_thread::yield();
            }
        }
    }

    struct AsyncTgaSave
    {
        const char* file = nullptr;
        CByteBuffer buffer;
        CByteConstView view;
        CByteRectBuffer rect_buffer;
        CByteRectConstView rect_view;
        image::codec::tga::EncodeOptions options{};
    };

    struct AsyncTgaLoad
    {
        const char* file = nullptr;
        CByteBuffer buffer;
        CByteConstView view;
        CByteRectBuffer rect_buffer;
        CByteRectConstView rect_view;
        bool vflip = false;
        image::codec::tga::decoded_image_desc desc = image::codec::tga::decoded_image_desc::RGBA;
    };

    AsyncTgaLoad async_tga_load;
    AsyncTgaSave async_tga_save;

    if (initialised)
    {
        WorkerThreadController& application_controller = *worker_thread_controllers.get_object(worker_thread_slots[static_cast<std::uint8_t>(EWorkerThreadID::application)].controller_slot);
        while (application_controller.control_state.query_state() != threading::EThreadRunState::Exited)
        {
            for (int32_t inbound_slot = worker_thread_controllers.first_live(); inbound_slot >= 0; inbound_slot = worker_thread_controllers.next_live(inbound_slot))
            {
                WorkerThreadController& inbound_controller = *worker_thread_controllers.get_object(inbound_slot);
                threading::CPodThreadMsg inbound_msg;
                while (inbound_controller.inbound_msgs.read(inbound_msg))
                {
                    debug_utils::debug_output("Host: Message received\n");

                    switch (inbound_msg.payload.query_type_id())
                    {
                        case (k_type_id_v<FileSaveResult>):
                        {
                            debug_utils::debug_output("Host: File save result\n");

                            FileSaveResult result;
                            (void)inbound_msg.payload.copy_to(result);
                            const std::int32_t outbound_slot = worker_thread_slots[static_cast<std::uint8_t>(EWorkerThreadID::application)].controller_slot;
                            WorkerThreadController& outbound_controller = *worker_thread_controllers.get_object(outbound_slot);
                            TgaSaveResult forward;
                            forward.success = result.success;
                            threading::CPodThreadMsg outbound_msg;
                            outbound_msg.async_slot = 0;
                            outbound_msg.payload.assign(forward);
                            (void)outbound_controller.outbound_msgs.post(outbound_msg);
                            outbound_controller.wait_predicate.poke_epoch_and_wake_one();
                            break;
                        }
                        case (k_type_id_v<TgaLoadRequest>):
                        {
                            debug_utils::debug_output("Host: TGA load request\n");

                            TgaLoadRequest tga_load_request;
                            (void)inbound_msg.payload.copy_to(tga_load_request);
                            async_tga_load.file = tga_load_request.file;
                            async_tga_load.vflip = tga_load_request.vflip;
                            const std::int32_t outbound_slot = worker_thread_slots[static_cast<std::uint8_t>(EWorkerThreadID::bg_file_io)].controller_slot;
                            WorkerThreadController& outbound_controller = *worker_thread_controllers.get_object(outbound_slot);
                            FileLoadRequest file_load_request;
                            file_load_request.file = async_tga_load.file;
                            threading::CPodThreadMsg outbound_msg;
                            outbound_msg.async_slot = 0;
                            outbound_msg.payload.assign(file_load_request);
                            (void)outbound_controller.outbound_msgs.post(outbound_msg);
                            outbound_controller.wait_predicate.poke_epoch_and_wake_one();

                            break;
                        }
                        case (k_type_id_v<TgaSaveRequest>):
                        {
                            debug_utils::debug_output("Host: TGA save request\n");

                            TgaSaveRequest tga_save_request;
                            (void)inbound_msg.payload.copy_to(tga_save_request);
                            async_tga_save.file = tga_save_request.file;
                            async_tga_save.options = *tga_save_request.options;
                            async_tga_save.rect_view = *tga_save_request.view;
                            const std::int32_t outbound_slot = worker_thread_slots[static_cast<std::uint8_t>(EWorkerThreadID::bg_conditioning)].controller_slot;
                            WorkerThreadController& outbound_controller = *worker_thread_controllers.get_object(outbound_slot);
                            TgaEncodeRequest tga_encode_request;
                            tga_encode_request.view = &async_tga_save.rect_view;
                            tga_encode_request.options = &async_tga_save.options;
                            threading::CPodThreadMsg outbound_msg;
                            outbound_msg.async_slot = 0;
                            outbound_msg.payload.assign(tga_encode_request);
                            (void)outbound_controller.outbound_msgs.post(outbound_msg);
                            outbound_controller.wait_predicate.poke_epoch_and_wake_one();
                            break;
                        }
                        case (k_type_id_v<UnrecognisedMsg>):
                        {
                            UnrecognisedMsg unrecognised;
                            (void)inbound_msg.payload.copy_to(unrecognised);

                            debug_utils::debug_output("Host notification: Unrecognised message type (%d)\n", unrecognised.msg_id);
                            break;
                        }
                        default:
                        {
                            debug_utils::debug_output("Host: Unrecognised message type (%d)\n", inbound_msg.payload.query_type_id());
                            break;
                        }
                    }
                }
                memory::CTypeless inbound_msg_owning;
                while (inbound_controller.inbound_msgs_owning.read(inbound_msg_owning))
                {
                    debug_utils::debug_output("Host: Owning message received\n");

                    switch (inbound_msg_owning.query_type_id())
                    {
                        case (k_type_id_v<FileLoadResultOwning>):
                        {   //  for this test we know that this is in response to our own attempt to load the tga file
                            debug_utils::debug_output("Host: Owning file load result\n");

                            FileLoadResultOwning& result = *typeless_cast<FileLoadResultOwning>(inbound_msg_owning);
                            async_tga_load.buffer = std::move(result.buffer);
                            async_tga_load.view = async_tga_load.buffer.const_view();
                            const std::int32_t outbound_slot = worker_thread_slots[static_cast<std::uint8_t>(EWorkerThreadID::bg_conditioning)].controller_slot;
                            WorkerThreadController& outbound_controller = *worker_thread_controllers.get_object(outbound_slot);
                            TgaDecodeRequest tga_decode_request;
                            tga_decode_request.view = &async_tga_load.view;
                            tga_decode_request.vflip = async_tga_load.vflip;
                            threading::CPodThreadMsg outbound_msg;
                            outbound_msg.async_slot = 0;
                            outbound_msg.payload.assign(tga_decode_request);
                            (void)outbound_controller.outbound_msgs.post(outbound_msg);
                            outbound_controller.wait_predicate.poke_epoch_and_wake_one();
                            break;
                        }
                        case (k_type_id_v<TgaEncodeResultOwning>):
                        {   //  for this test we know that this is in response to our own attempt to encode the tga file
                            debug_utils::debug_output("Host: Owning TGA encode result\n");

                            TgaEncodeResultOwning& result = *typeless_cast<TgaEncodeResultOwning>(inbound_msg_owning);
                            async_tga_save.buffer = std::move(result.buffer);
                            async_tga_save.view = async_tga_save.buffer.const_view();
                            const std::int32_t outbound_slot = worker_thread_slots[static_cast<std::uint8_t>(EWorkerThreadID::bg_file_io)].controller_slot;
                            WorkerThreadController& outbound_controller = *worker_thread_controllers.get_object(outbound_slot);
                            FileSaveRequest file_save_request;
                            file_save_request.file = async_tga_save.file;
                            file_save_request.view = &async_tga_save.view;
                            threading::CPodThreadMsg outbound_msg;
                            outbound_msg.async_slot = 0;
                            outbound_msg.payload.assign(file_save_request);
                            (void)outbound_controller.outbound_msgs.post(outbound_msg);
                            outbound_controller.wait_predicate.poke_epoch_and_wake_one();
                            break;
                        }
                        case (k_type_id_v<TgaDecodeResultOwning>):
                        {   //  for this test we know that this is in response to our own attempt to decode the tga file
                            debug_utils::debug_output("Host: Owning TGA decode result\n");

                            TgaDecodeResultOwning& result = *typeless_cast<TgaDecodeResultOwning>(inbound_msg_owning);
                            async_tga_load.rect_buffer = std::move(result.buffer);
                            async_tga_load.rect_view = async_tga_load.rect_buffer.const_view();
                            async_tga_load.desc = result.desc;
                            const std::int32_t outbound_slot = worker_thread_slots[static_cast<std::uint8_t>(EWorkerThreadID::application)].controller_slot;
                            WorkerThreadController& outbound_controller = *worker_thread_controllers.get_object(outbound_slot);
                            TgaLoadResult tga_load_result;
                            tga_load_result.view = &async_tga_load.rect_view;
                            tga_load_result.desc = async_tga_load.desc;
                            threading::CPodThreadMsg outbound_msg;
                            outbound_msg.async_slot = 0;
                            outbound_msg.payload.assign(tga_load_result);
                            (void)outbound_controller.outbound_msgs.post(outbound_msg);
                            outbound_controller.wait_predicate.poke_epoch_and_wake_one();
                            break;
                        }
                        default:
                        {
                            debug_utils::debug_output("Host: Unrecognised owning message type (%d)\n", inbound_msg_owning.query_type_id());
                            break;
                        }
                    }
                }
            }
        }
    }

    for (std::int32_t thread_index = 2; thread_index >= 0; --thread_index)
    {
        const std::int32_t controller_slot = worker_thread_slots[thread_index].controller_slot;
        WorkerThreadController& worker_controller = *worker_thread_controllers.get_object(controller_slot);

        for (;;)
        {   //  testing the workers re-entering a waiting state
            threading::EThreadRunState state = worker_controller.control_state.query_state();
            if ((state == threading::EThreadRunState::Exited) || (state == threading::EThreadRunState::Waiting))
            {
                break;
            }
            std::this_thread::yield();
        }

        worker_controller.control_state.request_exit();
        worker_controller.wait_predicate.release_control();
        if (worker_controller.thread_created)
        {
            worker_controller.thread_created = worker_controller.thread.join_and_close();
        }
    }
    return 0;
}
