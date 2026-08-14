
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    host.cpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    15 May 26
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
#include <limits>       //  std::numeric_limits
#include <utility>      //  std::move

#include "assets/asset_repository.hpp"
#include "containers/containers.hpp"
#include "host/host.hpp"
#include "host/host_context.hpp"
#include "host/host_local_type_registry.hpp"
#include "host/host_worker_thread.hpp"
#include "host/system_id_definitions.hpp"
#include "image/codec/tga.hpp"
#include "modules/application/application_binding.hpp"
#include "platform/path/native_path.hpp"
#include "platform/system/performance_counter.hpp"
#include "system/async_state.hpp"
#include "system/erased_owner.hpp"
#include "system/transported_types.hpp"
#include "threading/CThreadPackage.hpp"

#include "debug/macros.hpp"
#include "debug/service.hpp"

namespace host
{

CHost::~CHost() noexcept
{
    shutdown();
}

void CHost::initialise_debug_service() noexcept
{
    m_debug_service_owner = TInstance<debug_system::CDebugServiceState>::create();
    if (m_debug_service_owner)
    {
        m_debug_service = m_debug_service_owner.operator->();
        const bool debug_logs_opened =
            m_debug_service->configure_log_paths(
                "morphic_debug.log",
                "morphic_debug_direct.log") &&
            m_debug_service->open_logs();
        if (debug_logs_opened)
        {
            m_debug_service_installed = debug_system::install_service(m_debug_service);
            if (m_debug_service_installed)
            {
                m_debug_service_started = m_debug_service->start();
            }
        }
    }
}

bool CHost::start_threads() noexcept
{
    const threading::ThreadConfig thread_configs[k_thread_count]{
        {thread_ids::bg_file_io, module_ids::executable, platform::threading::EThreadPriority::Background, host_worker_thread_entry_point()},
        {thread_ids::bg_conditioning, module_ids::executable, platform::threading::EThreadPriority::Background, host_worker_thread_entry_point()},
        {thread_ids::application, module_ids::application, platform::threading::EThreadPriority::Normal, m_application_thread, &CBoundModule::prepare_thread, &m_application_module} };

    for (std::size_t thread_index = 0u; thread_index < k_thread_count; ++thread_index)
    {
        const std::int32_t thread_slot = m_thread_packages.emplace(thread_configs[thread_index], m_perf_count_conversion);
        if (thread_slot < 0)
        {
            return false;
        }
        m_thread_slots[thread_index] = thread_slot;
        threading::CThreadPackage& package = *m_thread_packages.get_object(thread_slot);
        if (!package.startup())
        {
            return false;
        }
    }
    return true;
}

bool CHost::bind_application_module() noexcept
{
    constexpr modules::SAdvertisedIdentity advertised_host_identity{
        module_ids::executable, { modules::k_binding_abi_major, 0u },
        modules::k_binding_abi_major, modules::k_binding_abi_major };
    constexpr std::uint32_t expected_module_major = modules::k_binding_abi_major;

    const platform::path::NativePath module_path = platform::path::makeNativePath("MorphicApplication.dll");
    if (!module_path.is_ready() ||
        !m_application_module.bind(module_path, module_ids::application, advertised_host_identity) ||
        !m_application_module.install(system_registry_view(), module_ids::application, host_memory_context(), m_debug_service))
    {
        MV_ERROR("Host failed to bind and install the application module");
        return false;
    }

    application::FApplicationThread application_thread = nullptr;
    if (!validate_application_module_compatibility(advertised_host_identity, expected_module_major, application_thread))
    {
        MV_ERROR("Host failed the application module compatibility checks");
        return false;
    }

    m_application_thread = application_thread;
    return true;
}

bool CHost::validate_application_module_compatibility(
    const modules::SAdvertisedIdentity& advertised_host_identity,
    const std::uint32_t expected_module_major,
    application::FApplicationThread& application_thread) noexcept
{
    application_thread = nullptr;

    const modules::SAdvertisedIdentity& module_identity = m_application_module.advertised_module_identity();
    const std::uint32_t functional_major = m_application_module.negotiated_functional_major();
    MV_INFO("Application module version {}.{} supports functional majors [{},{}]",
        module_identity.version.major,
        module_identity.version.minor,
        module_identity.minimum_functional_major,
        module_identity.maximum_functional_major);

    if ((module_identity.version.major != expected_module_major) ||
        (functional_major != modules::highest_common_functional_major(advertised_host_identity, module_identity)))
    {
        return false;
    }

    // The next representable major cannot belong to the negotiated range.
    if (functional_major != std::numeric_limits<std::uint32_t>::max())
    {
        modules::SCoreFunctions unsupported_core;
        const modules::EBindingResult unsupported_result = m_application_module.populate_core_functions((functional_major + 1u), unsupported_core);

        // Rejection must be explicit and must not expose callable functions.
        if ((unsupported_result != modules::EBindingResult::unsupported_version) || !unsupported_core.is_empty())
        {
            return false;
        }
    }

    modules::FModuleFunction raw_application_thread = nullptr;
    modules::FModuleFunction unknown_thread = nullptr;
    if (!m_application_module.query_function(system_type_ids::application_thread_function, raw_application_thread) ||
        m_application_module.query_function(system_type_ids::undefined, unknown_thread) || (unknown_thread != nullptr))
    {
        return false;
    }

    application_thread = reinterpret_cast<application::FApplicationThread>(raw_application_thread);
    return application_thread != nullptr;
}

bool CHost::initialise_runtime() noexcept
{
    return
        m_perf_count_conversion.init() &&
        m_thread_packages.initialise() &&
        m_assets.initialise() &&
        m_async_states.initialise() &&
        bind_application_module() &&
        start_threads();
}

threading::CThreadPackage* CHost::thread_package(const EWorkerThreadID id) noexcept
{
    const std::size_t index = static_cast<std::size_t>(id);
    if ((index >= k_thread_count) || (m_thread_slots[index] < 0))
    {
        return nullptr;
    }
    return m_thread_packages.get_object(m_thread_slots[index]);
}

int CHost::execute() noexcept
{
    initialise_debug_service();
    MV_INFO("Host: Starting");

    const bool initialised = initialise_runtime();
    if (initialised)
    {
        run();
    }
    shutdown();
    return initialised ? 0 : 1;
}

void CHost::run() noexcept
{
    TUnorderedCollection<threading::CThreadPackage>& thread_packages = m_thread_packages;
    CAssetRepository& assets = m_assets;
    CASyncStates& async_states = m_async_states;
    debug_system::CDebugServiceState* const debug_service = m_debug_service;

    platform::system::CPerfCounter perf_counter;
    perf_counter.update();
    const std::uint64_t ticks_per_second = m_perf_count_conversion.query_ticks_per_second();

    threading::CThreadPackage* const application = thread_package(EWorkerThreadID::application);
    MV_CRITICAL_ASSERT(application != nullptr);
    if (application == nullptr)
    {
        return;
    }
    threading::CThreadPackage& application_package = *application;

    const auto post_tga_load_result = [&application_package](
        const std::int32_t application_slot, const CAssetId asset,
        const image::codec::tga::decoded_image_desc desc, const bool success) noexcept
    {
        TgaLoadResult result;
        result.asset = asset;
        result.desc = desc;
        result.success = success;
        threading::CErasedPodMsg outbound_msg;
        outbound_msg.set_async_slot(application_slot);
        outbound_msg.assign_payload(result);
        return application_package.post(outbound_msg);
    };

    const auto post_tga_save_result = [&application_package](
        const std::int32_t application_slot, const bool success) noexcept
    {
        TgaSaveResult result;
        result.success = success;
        threading::CErasedPodMsg outbound_msg;
        outbound_msg.set_async_slot(application_slot);
        outbound_msg.assign_payload(result);
        return application_package.post(outbound_msg);
    };

    while ((application_package.query_state() != threading::EThreadRunState::Exited) &&
        (application_package.query_state() != threading::EThreadRunState::Failed) &&
        ((debug_service == nullptr) || (debug_service->read_shutdown_request() == debug_system::EShutdownReason::none)))
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
            threading::CErasedPodMsg inbound_msg;
            while (inbound_package.read(inbound_msg))
            {
                MV_TRACE("Host: Recieved a message");

                switch (inbound_msg.query_message_type_id().raw_value())
                {
                    case k_type_id_v<FileSaveResult>.raw_value():
                    {
                        FileSaveResult result;
                        (void)inbound_msg.copy_payload_to(result);
                        MV_DETAIL("Host: Recieved a file save result success={}", result.success ? 1u : 0u);
                        const std::int32_t async_slot = inbound_msg.query_async_slot();
                        const SHostTgaFileSaveState* const state = async_states.payload<SHostTgaFileSaveState>(async_slot);
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
                    case k_type_id_v<TgaLoadRequest>.raw_value():
                    {
                        MV_DETAIL("Host: Recieved a TGA load request");

                        TgaLoadRequest tga_load_request;
                        (void)inbound_msg.copy_payload_to(tga_load_request);
                        const std::int32_t application_slot = inbound_msg.query_async_slot();
                        const std::int32_t async_slot = async_states.acquire<SHostTgaFileLoadState>();
                        SHostTgaFileLoadState* const state = async_states.payload<SHostTgaFileLoadState>(async_slot);
                        if (state == nullptr)
                        {
                            MV_CRITICAL_EVENT("Host: Failed to acquire TGA load state");
                            (void)post_tga_load_result(application_slot, CAssetId{}, image::codec::tga::decoded_image_desc::RGBA, false);
                            break;
                        }
                        state->application_slot = application_slot;
                        state->file = tga_load_request.file;
                        state->vflip = tga_load_request.vflip;

                        threading::CThreadPackage& outbound_package = *thread_package(EWorkerThreadID::bg_file_io);
                        FileLoadRequest file_load_request;
                        file_load_request.file = state->file;
                        threading::CErasedPodMsg outbound_msg;
                        outbound_msg.set_async_slot(async_slot);
                        outbound_msg.assign_payload(file_load_request);
                        if (!outbound_package.post(outbound_msg))
                        {
                            (void)async_states.release(async_slot);
                            (void)post_tga_load_result(application_slot, CAssetId{}, image::codec::tga::decoded_image_desc::RGBA, false);
                        }
                        break;
                    }
                    case k_type_id_v<TgaSaveRequest>.raw_value():
                    {
                        MV_DETAIL("Host: Recieved a TGA save request");

                        TgaSaveRequest tga_save_request;
                        (void)inbound_msg.copy_payload_to(tga_save_request);
                        const CAssetRecord* const source_record = assets.resolve(tga_save_request.source);
                        const DecodedTga* const source = (source_record != nullptr) ? source_record->payload<DecodedTga>() : nullptr;
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
                        threading::CThreadPackage& outbound_package = *thread_package(EWorkerThreadID::bg_conditioning);
                        TgaEncodeRequest tga_encode_request;
                        tga_encode_request.view = source_view;
                        tga_encode_request.options = state->options;
                        threading::CErasedPodMsg outbound_msg;
                        outbound_msg.set_async_slot(async_slot);
                        outbound_msg.assign_payload(tga_encode_request);
                        if (!outbound_package.post(outbound_msg))
                        {
                            (void)async_states.release(async_slot);
                            (void)post_tga_save_result(application_slot, false);
                        }
                        break;
                    }
                    case k_type_id_v<UnrecognisedMsg>.raw_value():
                    {
                        UnrecognisedMsg unrecognised;
                        (void)inbound_msg.copy_payload_to(unrecognised);

                        MV_DETAIL("Host: Recieved an unrecognised message notification {}", unrecognised.msg_id);
                        break;
                    }
                    default:
                    {
                        system_type_id unrecognised_id;
                        if (inbound_msg.query_message_type_id().try_system_type_id(
                                unrecognised_id))
                        {
                            MV_DETAIL("Host: Recieved an unrecognised message type {}",
                                unrecognised_id);
                        }
                        else
                        {
                            MV_DETAIL("Host: Recieved an unrecognised LOCAL message type");
                        }
                        break;
                    }
                }
            }
            threading::CErasedOwnerMsg inbound_owned_msg;
            while (inbound_package.read(inbound_owned_msg))
            {
                MV_TRACE("Host: Recieved an owning message");

                const std::int32_t async_slot = inbound_owned_msg.query_async_slot();
                CErasedOwner content = inbound_owned_msg.take_owner();
                switch (inbound_owned_msg.query_message_type_id().raw_value())
                {
                    case k_type_id_v<FileLoadResult>.raw_value():
                    {
                        MV_DETAIL("Host: Took ownership of a loaded file buffer");

                        LoadedFile* const result = content.payload<LoadedFile>();
                        const SHostTgaFileLoadState* const load_state = async_states.payload<SHostTgaFileLoadState>(async_slot);
                        if (load_state == nullptr)
                        {
                            MV_CRITICAL_EVENT("Host: File load result has invalid async slot {}", async_slot);
                            break;
                        }

                        const std::int32_t application_slot = load_state->application_slot;
                        const bool vflip = load_state->vflip;
                        if ((result == nullptr) || !result->buffer.is_ready())
                        {
                            (void)post_tga_load_result(application_slot, CAssetId{}, image::codec::tga::decoded_image_desc::RGBA, false);
                            (void)async_states.release(async_slot);
                            break;
                        }

                        const CAssetId loaded_file = assets.insert(std::move(content));
                        const CAssetRecord* const loaded_record = assets.resolve(loaded_file);
                        const LoadedFile* const loaded = (loaded_record != nullptr) ? loaded_record->payload<LoadedFile>() : nullptr;
                        if (loaded == nullptr)
                        {
                            (void)post_tga_load_result(application_slot, CAssetId{}, image::codec::tga::decoded_image_desc::RGBA, false);
                            (void)async_states.release(async_slot);
                            break;
                        }

                        SHostTgaDecodeState* const decode_state = async_states.redefine<SHostTgaDecodeState>(async_slot);
                        decode_state->application_slot = application_slot;
                        decode_state->loaded_file = loaded_file;

                        const CByteConstView loaded_view = loaded->buffer.const_view();
                        threading::CThreadPackage& outbound_package = *thread_package(EWorkerThreadID::bg_conditioning);
                        TgaDecodeRequest tga_decode_request;
                        tga_decode_request.view = loaded_view;
                        tga_decode_request.vflip = vflip;
                        threading::CErasedPodMsg outbound_msg;
                        outbound_msg.set_async_slot(async_slot);
                        outbound_msg.assign_payload(tga_decode_request);
                        if (!outbound_package.post(outbound_msg))
                        {
                            (void)post_tga_load_result(application_slot, CAssetId{}, image::codec::tga::decoded_image_desc::RGBA, false);
                            (void)async_states.release(async_slot);
                        }
                        break;
                    }
                    case k_type_id_v<TgaEncodeResult>.raw_value():
                    {
                        MV_DETAIL("Host: Took ownership of an encoded TGA file buffer");

                        EncodedTga* const result = content.payload<EncodedTga>();
                        const SHostTgaEncodeState* const encode_state = async_states.payload<SHostTgaEncodeState>(async_slot);
                        if (encode_state == nullptr)
                        {
                            MV_CRITICAL_EVENT("Host: TGA encode result has invalid async slot {}", async_slot);
                            break;
                        }

                        const std::int32_t application_slot = encode_state->application_slot;
                        const char* const file = encode_state->file;
                        if ((result == nullptr) || !result->buffer.is_ready())
                        {
                            (void)post_tga_save_result(application_slot, false);
                            (void)async_states.release(async_slot);
                            break;
                        }

                        const CAssetId encoded_file = assets.insert(std::move(content));
                        const CAssetRecord* const encoded_record = assets.resolve(encoded_file);
                        const EncodedTga* const encoded = (encoded_record != nullptr) ? encoded_record->payload<EncodedTga>() : nullptr;
                        if (encoded == nullptr)
                        {
                            (void)post_tga_save_result(application_slot, false);
                            (void)async_states.release(async_slot);
                            break;
                        }

                        SHostTgaFileSaveState* const save_state = async_states.redefine<SHostTgaFileSaveState>(async_slot);
                        save_state->application_slot = application_slot;
                        save_state->encoded_file = encoded_file;

                        const CByteConstView encoded_view = encoded->buffer.const_view();
                        threading::CThreadPackage& outbound_package = *thread_package(EWorkerThreadID::bg_file_io);
                        FileSaveRequest file_save_request;
                        file_save_request.file = file;
                        file_save_request.view = encoded_view;
                        threading::CErasedPodMsg outbound_msg;
                        outbound_msg.set_async_slot(async_slot);
                        outbound_msg.assign_payload(file_save_request);
                        if (!outbound_package.post(outbound_msg))
                        {
                            (void)post_tga_save_result(application_slot, false);
                            (void)async_states.release(async_slot);
                        }
                        break;
                    }
                    case k_type_id_v<TgaDecodeResult>.raw_value():
                    {
                        MV_DETAIL("Host: Took ownership of a decoded TGA image buffer");

                        DecodedTga* const result = content.payload<DecodedTga>();
                        const SHostTgaDecodeState* const decode_state = async_states.payload<SHostTgaDecodeState>(async_slot);
                        if (decode_state == nullptr)
                        {
                            MV_CRITICAL_EVENT("Host: TGA decode result has invalid async slot {}", async_slot);
                            break;
                        }

                        const std::int32_t application_slot = decode_state->application_slot;
                        const image::codec::tga::decoded_image_desc desc = (result != nullptr) ? result->desc : image::codec::tga::decoded_image_desc::RGBA;
                        if ((result == nullptr) || !result->buffer.is_ready())
                        {
                            (void)post_tga_load_result(application_slot, CAssetId{}, desc, false);
                            (void)async_states.release(async_slot);
                            break;
                        }

                        const CAssetId decoded_image = assets.insert(std::move(content));
                        const bool stored = assets.resolve(decoded_image) != nullptr;
                        (void)post_tga_load_result(application_slot, decoded_image, desc, stored);
                        (void)async_states.release(async_slot);
                        break;
                    }
                    default:
                    {
                        system_type_id unrecognised_id;
                        if (inbound_owned_msg.query_message_type_id().try_system_type_id(
                                unrecognised_id))
                        {
                            MV_CRITICAL_EVENT(
                                "Host: Recieved an unknown owning message type {}",
                                unrecognised_id);
                        }
                        else
                        {
                            MV_CRITICAL_EVENT(
                                "Host: Recieved an unknown LOCAL owning message type");
                        }
                        break;
                    }
                }
            }
        }
    }
}

void CHost::shutdown_threads() noexcept
{
    for (std::size_t thread_index = k_thread_count; thread_index > 0u; --thread_index)
    {
        std::int32_t& controller_slot = m_thread_slots[thread_index - 1u];
        if (controller_slot >= 0)
        {
            threading::CThreadPackage* const worker_package = m_thread_packages.get_object(controller_slot);
            if (worker_package != nullptr)
            {
                (void)worker_package->shutdown();
            }
            controller_slot = -1;
        }
    }
}

void CHost::shutdown_debug_service() noexcept
{
    if (m_debug_service_started)
    {
        if (!m_debug_service->stop())
        {
            MV_ERROR("Host failed to stop the debug service cleanly");
        }
        m_debug_service_started = false;
    }
    if (m_debug_service_installed)
    {
        if (!debug_system::uninstall_service(m_debug_service))
        {
            MV_ERROR("Host failed to uninstall the debug service cleanly");
        }
        m_debug_service_installed = false;
    }
    m_debug_service = nullptr;
    m_debug_service_owner.reset();
}

void CHost::shutdown() noexcept
{
    shutdown_threads();
    m_application_thread = nullptr;
    m_application_module.unbind();
    m_async_states.deallocate();
    m_assets.deallocate();
    m_thread_packages.deallocate();
    shutdown_debug_service();
}

int host() noexcept
{
    CHost runtime;
    return runtime.execute();
}

}   //  namespace host
