
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   host.cpp
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

#include <cstdint>      //  std::int32_t, std::uint8_t, std::uint64_t
#include <utility>      //  std::move

#include "assets/asset_repository.hpp"
#include "containers/containers.hpp"
#include "host/host.hpp"
#include "host/host_worker_thread.hpp"
#include "image/codec/tga.hpp"
#include "modules/application/application_thread.hpp"
#include "platform/system/performance_counter.hpp"
#include "system/async_state.hpp"
#include "system/erased_owner.hpp"
#include "system/transported_types.hpp"
#include "threading/CThreadPackage.hpp"

#include "debug/macros.hpp"
#include "debug/service.hpp"

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

MV_REGISTER_SYSTEM_TYPE(SHostTgaFileLoadState, type_ids::host_tga_file_load_state);
MV_REGISTER_SYSTEM_TYPE(SHostTgaDecodeState, type_ids::host_tga_decode_state);
MV_REGISTER_SYSTEM_TYPE(SHostTgaEncodeState, type_ids::host_tga_encode_state);
MV_REGISTER_SYSTEM_TYPE(SHostTgaFileSaveState, type_ids::host_tga_file_save_state);

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

    static const threading::ThreadConfig thread_configs[3]{
        {thread_ids::bg_file_io, platform::threading::EThreadPriority::Background, host_worker_thread_entry_point()},
        {thread_ids::bg_conditioning, platform::threading::EThreadPriority::Background, host_worker_thread_entry_point()},
        {thread_ids::application, platform::threading::EThreadPriority::Normal, application::application_thread_entry_point()} };

    enum class EWorkerThreadID : std::uint8_t { bg_file_io = 0u, bg_conditioning, application };

    int32_t thread_slots[3]{ -1, -1, -1 };

    TUnorderedCollection<threading::CThreadPackage> thread_packages;
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
            threading::CThreadPackage& package = *thread_packages.get_object(thread_slot);
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

        threading::CThreadPackage& application_package =
            *thread_packages.get_object(thread_slots[static_cast<std::uint8_t>(EWorkerThreadID::application)]);
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
                threading::CThreadPackage& inbound_package = *thread_packages.get_object(inbound_slot);
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
                            threading::CThreadPackage& outbound_package = *thread_packages.get_object(outbound_slot);
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
                            threading::CThreadPackage& outbound_package = *thread_packages.get_object(outbound_slot);
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
                            threading::CThreadPackage& outbound_package = *thread_packages.get_object(outbound_slot);
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
                            threading::CThreadPackage& outbound_package = *thread_packages.get_object(outbound_slot);
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
            threading::CThreadPackage& worker_package = *thread_packages.get_object(controller_slot);
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
