//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   DebugService_test_suite.cpp
//  Author: OpenAI Codex
//  Date:   28 Jul 26

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <type_traits>

#include "containers/TInstance.hpp"
#include "debug/debug.hpp"
#include "debug/service.hpp"
#include "platform/filesystem/internal/file_utils.hpp"
#include "platform/path/native_path.hpp"
#include "system/system_context.hpp"
#include "tests/DebugService_test_suite.hpp"

namespace debug_service_tests
{

struct TTestContext
{
    void expect(const bool condition, const char* const expression, const int line)
    {
        if (condition)
        {
            ++passed;
        }
        else
        {
            ++failed;
            std::cerr << "DebugService test failure at line " << line
                << ": " << expression << '\n';
        }
    }

    int passed{ 0 };
    int failed{ 0 };
};

#define TEST_EXPECT(ctx, expression) (ctx).expect(!!(expression), #expression, __LINE__)

void compile_public_macro_interface(const bool execute)
{
    if (!execute)
    {
        return;
    }

    const bool condition = true;
    MV_ASSERT(condition);
    MV_ASSERT_MSG(condition, "assert message {}", std::uint32_t{ 1u });
    MV_DEBUG_ASSERT(condition);
    MV_DEBUG_ASSERT_MSG(condition, "debug assert message");
    MV_DEBUG_ONLY((void)condition);
    MV_CRITICAL_ASSERT(condition);
    MV_CRITICAL_ASSERT_MSG(condition, "critical assert message");
    MV_FATAL_ASSERT(condition);
    MV_FATAL_ASSERT_MSG(condition, "fatal assert message");
    MV_INFO("info {}", std::uint32_t{ 2u });
    MV_DETAIL("detail");
    MV_TRACE("trace");
    MV_REPORT("report %u", 3u);
    MV_WARNING("warning");
    MV_ERROR("error");
    MV_CRITICAL_EVENT("critical event");
    MV_FATAL_EVENT("fatal event");
    MV_PANIC("panic");
}

bool file_contains(const char* const path, const char* const expected)
{
    const platform::path::NativePath native_path =
        platform::path::makeNativePath(path);
    std::FILE* const stream = platform::filesystem::openFile(
        native_path, platform::filesystem::EOpenMode::BinaryRead);
    if (stream == nullptr)
    {
        return false;
    }

    char buffer[4096]{};
    const std::size_t read = std::fread(buffer, 1u, sizeof(buffer) - 1u, stream);
    std::fclose(stream);
    buffer[read] = 0;
    return std::strstr(buffer, expected) != nullptr;
}

debug_system::EEventArgumentType argument_type(
    const debug_system::SEventArguments& arguments,
    const std::size_t index)
{
    return static_cast<debug_system::EEventArgumentType>(
        (arguments.type_tags >> static_cast<std::uint32_t>(index * 4u)) &
        0x0fu);
}

debug_system::EEventFormatResult format_event(
    char* const destination,
    const std::size_t destination_capacity,
    const char* const format,
    const debug_system::SEventArguments& arguments,
    std::size_t& out_size)
{
    return debug_system::format_event_text(
        destination,
        destination_capacity,
        format,
        std::strlen(format),
        arguments,
        out_size);
}

void test_exclusive_lock_try_acquire(TTestContext& ctx)
{
    platform::threading::CExclusiveLock lock;
    TEST_EXPECT(ctx, lock.is_valid());
    TEST_EXPECT(ctx, lock.try_acquire());
    lock.release();
    TEST_EXPECT(ctx, lock.try_acquire());
    lock.release();
}

void test_argument_encoding(TTestContext& ctx)
{
    static_assert(
        sizeof(debug_system::SEvent) ==
        debug_system::k_event_transport_element_size);
    static_assert(debug_system::is_valid_event_metadata(
        debug_system::EEventLevel::info,
        debug_system::EEventType::event));
    static_assert(debug_system::is_valid_event_metadata(
        debug_system::EEventLevel::assert,
        debug_system::EEventType::condition));
    static_assert(debug_system::is_valid_event_metadata(
        debug_system::EEventLevel::critical,
        debug_system::EEventType::condition));
    static_assert(debug_system::is_valid_event_metadata(
        debug_system::EEventLevel::critical,
        debug_system::EEventType::event));
    static_assert(!debug_system::is_valid_event_metadata(
        debug_system::EEventLevel::info,
        debug_system::EEventType::condition));
    static_assert(!debug_system::is_valid_event_metadata(
        debug_system::EEventLevel::warning,
        debug_system::EEventType::condition));
    static_assert(
        std::is_trivially_copyable_v<debug_system::SEventArguments>);
    static_assert(
        std::is_trivially_copyable_v<debug_system::CInlineText16>);
    static_assert(debug_system::is_supported_event_argument_v<bool>);
    static_assert(debug_system::is_supported_event_argument_v<std::int32_t>);
    static_assert(debug_system::is_supported_event_argument_v<std::uint32_t>);
    static_assert(debug_system::is_supported_event_argument_v<std::int64_t>);
    static_assert(debug_system::is_supported_event_argument_v<std::uint64_t>);
    static_assert(debug_system::is_supported_event_argument_v<float>);
    static_assert(debug_system::is_supported_event_argument_v<double>);
    static_assert(
        debug_system::is_supported_event_argument_v<
            debug_system::CInlineText16>);
    static_assert(!debug_system::is_supported_event_argument_v<const char*>);

    const debug_system::SEventArguments empty =
        debug_system::encode_event_arguments();
    TEST_EXPECT(ctx, empty.type_tags == 0u);
    TEST_EXPECT(ctx, empty.parameter_count == 0u);
    TEST_EXPECT(ctx, empty.payload_size == 0u);
    TEST_EXPECT(ctx, empty.reserved == 0u);

    const debug_system::SEventArguments encoded =
        debug_system::encode_event_arguments(
            false,
            true,
            std::int32_t{ -12 },
            std::uint32_t{ 34u },
            std::int64_t{ -56 },
            std::uint64_t{ 78u },
            1.25f,
            2.5);

    TEST_EXPECT(ctx, encoded.parameter_count == 8u);
    TEST_EXPECT(ctx, encoded.payload_size == 36u);
    TEST_EXPECT(ctx,
        argument_type(encoded, 0u) ==
        debug_system::EEventArgumentType::false_value);
    TEST_EXPECT(ctx,
        argument_type(encoded, 1u) ==
        debug_system::EEventArgumentType::true_value);
    TEST_EXPECT(ctx,
        argument_type(encoded, 2u) ==
        debug_system::EEventArgumentType::int32);
    TEST_EXPECT(ctx,
        argument_type(encoded, 3u) ==
        debug_system::EEventArgumentType::uint32);
    TEST_EXPECT(ctx,
        argument_type(encoded, 4u) ==
        debug_system::EEventArgumentType::int64);
    TEST_EXPECT(ctx,
        argument_type(encoded, 5u) ==
        debug_system::EEventArgumentType::uint64);
    TEST_EXPECT(ctx,
        argument_type(encoded, 6u) ==
        debug_system::EEventArgumentType::float32);
    TEST_EXPECT(ctx,
        argument_type(encoded, 7u) ==
        debug_system::EEventArgumentType::float64);

    const debug_system::SEventArguments full_payload =
        debug_system::encode_event_arguments(
            std::uint64_t{ 0u },
            std::uint64_t{ 1u },
            std::uint64_t{ 2u },
            std::uint64_t{ 3u },
            std::uint64_t{ 4u },
            std::uint64_t{ 5u },
            std::uint64_t{ 6u },
            std::uint64_t{ 7u });
    TEST_EXPECT(ctx, full_payload.parameter_count == 8u);
    TEST_EXPECT(ctx, full_payload.payload_size == 64u);

    const debug_system::SEventArguments inline_text =
        debug_system::encode_event_arguments(
            debug_system::CInlineText16{ "fifteen-chars!!" });
    TEST_EXPECT(ctx, inline_text.parameter_count == 1u);
    TEST_EXPECT(ctx, inline_text.payload_size == 16u);
    TEST_EXPECT(ctx,
        argument_type(inline_text, 0u) ==
        debug_system::EEventArgumentType::inline_text);
}

void test_argument_formatting(TTestContext& ctx)
{
    char output[256]{};
    std::size_t output_size = 0u;

    const debug_system::SEventArguments values =
        debug_system::encode_event_arguments(
            false,
            true,
            std::int32_t{ -12 },
            std::uint32_t{ 34u },
            std::int64_t{ -56 },
            std::uint64_t{ 78u },
            1.25f,
            2.5);
    TEST_EXPECT(ctx,
        format_event(
            output,
            sizeof(output),
            "{} {} {} {} {} {} {} {}",
            values,
            output_size) ==
        debug_system::EEventFormatResult::success);
    TEST_EXPECT(ctx,
        std::strcmp(
            output,
            "false true -12 34 -56 78 1.25 2.5") == 0);
    TEST_EXPECT(ctx, output_size == std::strlen(output));

    const debug_system::SEventArguments inline_text =
        debug_system::encode_event_arguments(
            debug_system::CInlineText16{ "small text" });
    TEST_EXPECT(ctx,
        format_event(
            output,
            sizeof(output),
            "{{inline: {}}}",
            inline_text,
            output_size) ==
        debug_system::EEventFormatResult::success);
    TEST_EXPECT(ctx, std::strcmp(output, "{inline: small text}") == 0);

    TEST_EXPECT(ctx,
        format_event(
            output,
            sizeof(output),
            "{} {}",
            debug_system::encode_event_arguments(std::uint32_t{ 1u }),
            output_size) ==
        debug_system::EEventFormatResult::argument_mismatch);
    TEST_EXPECT(ctx,
        format_event(
            output,
            sizeof(output),
            "{broken",
            debug_system::encode_event_arguments(),
            output_size) ==
        debug_system::EEventFormatResult::malformed_format);

    constexpr char embedded_null[]{ 'a', 0, 'b', 0 };
    TEST_EXPECT(ctx,
        debug_system::format_event_text(
            output,
            sizeof(output),
            embedded_null,
            3u,
            debug_system::encode_event_arguments(),
            output_size) ==
        debug_system::EEventFormatResult::malformed_format);

    TEST_EXPECT(ctx,
        format_event(
            output,
            4u,
            "value {}",
            debug_system::encode_event_arguments(std::uint32_t{ 1u }),
            output_size) ==
        debug_system::EEventFormatResult::output_too_small);

    debug_system::SEventArguments malformed =
        debug_system::encode_event_arguments(std::uint32_t{ 1u });
    malformed.payload_size = 3u;
    TEST_EXPECT(ctx,
        format_event(
            output,
            sizeof(output),
            "{}",
            malformed,
            output_size) ==
        debug_system::EEventFormatResult::invalid_descriptor);

    debug_system::SEventArguments external_reference{};
    external_reference.type_tags = static_cast<std::uint32_t>(
        debug_system::EEventArgumentType::external_string_reference);
    external_reference.parameter_count = 1u;
    external_reference.payload_size = 8u;
    TEST_EXPECT(ctx,
        format_event(
            output,
            sizeof(output),
            "{}",
            external_reference,
            output_size) ==
        debug_system::EEventFormatResult::unsupported_argument);
}

void test_system_id_name_registry(TTestContext& ctx)
{
    TEST_EXPECT(ctx, system_id_registry::type_count() == type_ids::k_count);
    TEST_EXPECT(ctx,
        system_id_registry::mount_point_count() ==
        mount_point_ids::k_count);
    TEST_EXPECT(ctx, system_id_registry::thread_count() == thread_ids::k_count);
    TEST_EXPECT(ctx, system_id_registry::module_count() == module_ids::k_count);
    TEST_EXPECT(ctx, system_id_registry::validate_type_registrations());
    TEST_EXPECT(ctx, system_id_registry::validate_mount_point_registrations());
    TEST_EXPECT(ctx, system_id_registry::validate_thread_registrations());
    TEST_EXPECT(ctx, system_id_registry::validate_module_registrations());
    TEST_EXPECT(ctx, system_id_registry::validate_all());

    const system_id_registry::STypeRegistration* const type_registration =
        system_id_registry::find_type(type_ids::file_load_request);
    TEST_EXPECT(ctx, type_registration != nullptr);
    if (type_registration != nullptr)
    {
        TEST_EXPECT(ctx,
            type_registration->index == type_ids::file_load_request_index);
    }
    const char* const type_name =
        system_id_registry::lookup_type_name(type_ids::file_load_request);
    TEST_EXPECT(ctx, type_name != nullptr);
    if (type_name != nullptr)
    {
        TEST_EXPECT(ctx, std::strcmp(type_name, "file_load_request") == 0);
    }

    const system_id_registry::SMountPointRegistration* const mount_registration =
        system_id_registry::find_mount_point(mount_point_ids::render);
    TEST_EXPECT(ctx, mount_registration != nullptr);
    if (mount_registration != nullptr)
    {
        TEST_EXPECT(ctx,
            mount_registration->index == mount_point_ids::render_index);
    }
    const char* const mount_name =
        system_id_registry::lookup_mount_point_name(mount_point_ids::render);
    TEST_EXPECT(ctx, mount_name != nullptr);
    if (mount_name != nullptr)
    {
        TEST_EXPECT(ctx, std::strcmp(mount_name, "render") == 0);
    }

    const system_id_registry::SModuleRegistration* const module_registration =
        system_id_registry::find_module(module_ids::application);
    TEST_EXPECT(ctx, module_registration != nullptr);
    if (module_registration != nullptr)
    {
        TEST_EXPECT(ctx,
            module_registration->index == module_ids::application_index);
        TEST_EXPECT(ctx,
            module_registration->mount_point_id == mount_point_ids::application);
    }
    const char* const module_name =
        system_id_registry::lookup_module_name(module_ids::application);
    TEST_EXPECT(ctx, module_name != nullptr);
    if (module_name != nullptr)
    {
        TEST_EXPECT(ctx, std::strcmp(module_name, "application") == 0);
    }

    const system_id_registry::SThreadRegistration* const thread_registration =
        system_id_registry::find_thread(thread_ids::bg_conditioning);
    TEST_EXPECT(ctx, thread_registration != nullptr);
    if (thread_registration != nullptr)
    {
        TEST_EXPECT(ctx,
            thread_registration->index == thread_ids::bg_conditioning_index);
    }
    const char* const thread_name =
        system_id_registry::lookup_thread_name(thread_ids::bg_conditioning);
    TEST_EXPECT(ctx, thread_name != nullptr);
    if (thread_name != nullptr)
    {
        TEST_EXPECT(ctx, std::strcmp(thread_name, "bg_conditioning") == 0);
    }

    char system_name[64]{};
    std::size_t system_name_size = 0u;
    TEST_EXPECT(ctx,
        system_id_registry::format_system_name(
            system_ids::application,
            system_name,
            sizeof(system_name),
            system_name_size));
    TEST_EXPECT(ctx, std::strcmp(system_name, "application:application") == 0);
    TEST_EXPECT(ctx, system_name_size == std::strlen(system_name));

    char small_system_name[8]{};
    std::size_t small_system_name_size = 123u;
    TEST_EXPECT(ctx,
        !system_id_registry::format_system_name(
            system_ids::application,
            small_system_name,
            sizeof(small_system_name),
            small_system_name_size));
    TEST_EXPECT(ctx, small_system_name[0] == 0);
    TEST_EXPECT(ctx, small_system_name_size == 0u);

    TEST_EXPECT(ctx, system_id_registry::lookup_type_name(type_ids::id_type{}) == nullptr);
    TEST_EXPECT(ctx, system_id_registry::lookup_mount_point_name({}) == nullptr);
    TEST_EXPECT(ctx, system_id_registry::lookup_module_name({}) == nullptr);
    TEST_EXPECT(ctx, system_id_registry::lookup_thread_name({}) == nullptr);

    const module_ids::id_type unregistered_module = module_ids::make_id(
        mount_point_ids::render,
        module_ids::application_index);
    const system_ids::id_type unregistered_system =
        system_ids::make_system_id(unregistered_module, thread_ids::host);
    TEST_EXPECT(ctx, module_ids::is_valid_id(unregistered_module));
    TEST_EXPECT(ctx, system_ids::is_valid_id(unregistered_system));
    TEST_EXPECT(ctx,
        system_id_registry::find_module(unregistered_module) == nullptr);
    TEST_EXPECT(ctx,
        system_id_registry::lookup_module_name(unregistered_module) == nullptr);
    TEST_EXPECT(ctx,
        !system_id_registry::format_system_name(
            unregistered_system,
            system_name,
            sizeof(system_name),
            system_name_size));
    TEST_EXPECT(ctx, system_name[0] == 0);
    TEST_EXPECT(ctx, system_name_size == 0u);
}

void test_provisioning_and_shared_words(TTestContext& ctx)
{
    TInstance<debug_system::CDebugServiceState> owner =
        TInstance<debug_system::CDebugServiceState>::create();
    TEST_EXPECT(ctx, owner.is_ready());

    debug_system::CDebugServiceState* const service = owner.operator->();
    TEST_EXPECT(ctx, debug_system::get_service() == nullptr);
    TEST_EXPECT(ctx, !debug_system::install_service(nullptr));
    TEST_EXPECT(ctx, debug_system::install_service(service));
    TEST_EXPECT(ctx, debug_system::get_service() == service);
    TEST_EXPECT(ctx, !debug_system::install_service(service));
    TEST_EXPECT(ctx, !debug_system::uninstall_service(nullptr));

    TEST_EXPECT(ctx, service->allocate_incident_id() == 1u);
    TEST_EXPECT(ctx, service->allocate_incident_id() == 2u);

    service->publish_configuration(0x55aa55aau);
    TEST_EXPECT(ctx, service->read_configuration() == 0x55aa55aau);

    int filtered_argument_evaluations = 0;
    service->publish_configuration(0u);
    MV_INFO("filtered {}", ++filtered_argument_evaluations);
    TEST_EXPECT(ctx, filtered_argument_evaluations == 0);
    TEST_EXPECT(ctx, !service->informational_event_enabled(debug_system::EEventLevel::info));
    service->publish_configuration(1u << debug_system::k_information_level_shift);
    TEST_EXPECT(ctx, service->informational_event_enabled(debug_system::EEventLevel::info));
    TEST_EXPECT(ctx, !service->informational_event_enabled(debug_system::EEventLevel::detail));
    service->publish_configuration(debug_system::k_information_level_mask);
    TEST_EXPECT(ctx, service->informational_event_enabled(debug_system::EEventLevel::trace));
    TEST_EXPECT(ctx, !service->critical_shutdown_enabled());
    service->publish_configuration(debug_system::k_information_level_mask | debug_system::k_critical_shutdown_enabled);
    TEST_EXPECT(ctx, service->critical_shutdown_enabled());

    debug_system::EBreakpointOverride breakpoint_override =
        debug_system::EBreakpointOverride::inherit;
    service->publish_configuration(debug_system::k_breakpoints_enabled);
    TEST_EXPECT(ctx, service->breakpoint_enabled(breakpoint_override, true));
    TEST_EXPECT(ctx, !service->breakpoint_enabled(breakpoint_override, false));
    breakpoint_override = debug_system::EBreakpointOverride::enabled;
    TEST_EXPECT(ctx, service->breakpoint_enabled(breakpoint_override, false));
    breakpoint_override = debug_system::EBreakpointOverride::disabled;
    TEST_EXPECT(ctx, !service->breakpoint_enabled(breakpoint_override, true));
    service->publish_configuration(0u);
    breakpoint_override = debug_system::EBreakpointOverride::enabled;
    TEST_EXPECT(ctx, !service->breakpoint_enabled(breakpoint_override, true));
    TEST_EXPECT(ctx, !service->breakpoint_pause_requested());
    debug_system::SBreakpointContext breakpoint_context;
    TEST_EXPECT(ctx,
        !service->read_breakpoint_context(breakpoint_context));

    TEST_EXPECT(ctx,
        service->read_shutdown_request() ==
        debug_system::EShutdownReason::none);
    const debug_system::SEventUsagePoint invalid_usage_point{};
    service->publish_configuration(debug_system::k_critical_shutdown_enabled);
    TEST_EXPECT(ctx,
        !(service->process_event<
            debug_system::EEventLevel::critical,
            debug_system::EEventType::event>(
            invalid_usage_point,
            breakpoint_override,
            true,
            debug_system::EShutdownReason::critical_incident,
            0u,
            nullptr,
            0u)));
    TEST_EXPECT(ctx,
        service->read_shutdown_request() ==
        debug_system::EShutdownReason::critical_incident);
    TEST_EXPECT(ctx,
        !(service->process_event<
            debug_system::EEventLevel::fatal,
            debug_system::EEventType::event>(
            invalid_usage_point,
            breakpoint_override,
            true,
            debug_system::EShutdownReason::fatal_incident,
            0u,
            nullptr,
            0u)));
    TEST_EXPECT(ctx,
        service->read_shutdown_request() ==
        debug_system::EShutdownReason::fatal_incident);
    service->request_shutdown(debug_system::EShutdownReason::fatal_incident);
    service->request_shutdown(
        debug_system::EShutdownReason::critical_incident);
    TEST_EXPECT(ctx,
        service->read_shutdown_request() ==
        debug_system::EShutdownReason::fatal_incident);
    service->request_shutdown(
        debug_system::EShutdownReason::panic_incident);
    TEST_EXPECT(ctx,
        service->read_shutdown_request() ==
        debug_system::EShutdownReason::panic_incident);

    TEST_EXPECT(ctx, debug_system::uninstall_service(service));
    TEST_EXPECT(ctx, debug_system::get_service() == nullptr);
}

void test_writer_and_direct_paths(TTestContext& ctx)
{
    constexpr const char* event_path = "debug_service_test.log";
    constexpr const char* direct_path = "debug_service_test_direct.log";
    constexpr char source_file[] =
        "discarded/source/prefix/which/is/intentionally/longer/than/"
        "the/bounded/source/storage/available/to/the/debug/event/"
        "src/tests/DebugService_test_suite.cpp";
    constexpr std::uint32_t source_line = 321u;
    static_assert(sizeof(source_file) >
        debug_system::k_source_file_capacity);

    TInstance<debug_system::CDebugServiceState> owner =
        TInstance<debug_system::CDebugServiceState>::create();
    TEST_EXPECT(ctx, owner.is_ready());

    debug_system::CDebugServiceState* const service = owner.operator->();
    TEST_EXPECT(ctx,
        service->configure_log_paths(event_path, direct_path));
    TEST_EXPECT(ctx,
        std::strcmp(service->event_log_path(), event_path) == 0);
    TEST_EXPECT(ctx,
        std::strcmp(service->direct_log_path(), direct_path) == 0);
    TEST_EXPECT(ctx, service->open_logs());
    TEST_EXPECT(ctx, debug_system::install_service(service));
    TEST_EXPECT(ctx, service->start());
    TEST_EXPECT(ctx,
        service->thread_state() ==
        debug_system::EServiceThreadState::running);

    MV_INFO("first transported event");
    TEST_EXPECT(ctx, debug_system::submit_text("second transported event"));
    TEST_EXPECT(ctx, debug_system::submit_text("literal {braces}"));
    debug_system::EBreakpointOverride breakpoint_override =
        debug_system::EBreakpointOverride::disabled;
    TEST_EXPECT(ctx,
        (debug_system::process_event<
            debug_system::EEventLevel::error,
            debug_system::EEventType::event>(
            source_file,
            source_line,
            breakpoint_override,
            true,
            debug_system::EShutdownReason::none,
            "typed {} {} {}",
            std::int32_t{ -7 },
            true,
            debug_system::CInlineText16{ "payload" })));

    char oversized[debug_system::k_event_text_capacity + 32u];
    std::memset(oversized, 'x', sizeof(oversized) - 1u);
    oversized[sizeof(oversized) - 1u] = 0;
    TEST_EXPECT(ctx, debug_system::submit_text(oversized));
    MV_REPORT("rich %s %d %.1f", "report", 42, 3.5);
    const std::uint32_t panic_incident_id = service->allocate_incident_id();
    TEST_EXPECT(ctx, panic_incident_id == 7u);
    const debug_system::SEventUsagePoint panic_usage_point{
        source_file, sizeof(source_file) - 1u, source_line + 2u };
    TEST_EXPECT(ctx,
        service->try_write_panic_record(
            panic_usage_point,
            panic_incident_id,
            "panic substrate record",
            22u));

    const module_ids::id_type previous_module_id =
        system_context::get_ambient_module_id();
    const thread_ids::id_type previous_thread_id =
        system_context::get_ambient_thread_id();
    const module_ids::id_type unregistered_module = module_ids::make_id(
        mount_point_ids::render,
        module_ids::application_index);
    const system_ids::id_type unregistered_system =
        system_ids::make_system_id(unregistered_module, thread_ids::host);
    system_context::set_ambient_module_id(unregistered_module);
    system_context::set_ambient_thread_id(thread_ids::host);
    TEST_EXPECT(ctx,
        debug_system::submit_text("unregistered system identity"));

    system_context::set_ambient_module_id();
    system_context::set_ambient_thread_id();
    const system_ids::id_type invalid_system =
        system_context::get_ambient_system_id();
    TEST_EXPECT(ctx, !system_ids::is_valid_id(invalid_system));
    TEST_EXPECT(ctx, debug_system::submit_text("invalid system identity"));

    system_context::set_ambient_module_id(previous_module_id);
    system_context::set_ambient_thread_id(previous_thread_id);

#if MV_DEVELOPMENT_BUILD
    service->publish_configuration(debug_system::k_information_level_mask);
    MV_ASSERT(std::uint32_t{} == 1u);
#endif

    TEST_EXPECT(ctx, service->stop());
    TEST_EXPECT(ctx,
        service->thread_state() ==
        debug_system::EServiceThreadState::stopped);
    TEST_EXPECT(ctx, service->is_event_transport_closed());
    TEST_EXPECT(ctx, debug_system::uninstall_service(service));

    char system_marker[32]{};
    const int system_marker_size = std::snprintf(
        system_marker,
        sizeof(system_marker),
        "[%016llx]",
        static_cast<unsigned long long>(
            system_ids::host.raw_value()));
    TEST_EXPECT(ctx, system_marker_size == 18);
    TEST_EXPECT(ctx,
        file_contains(event_path, "[executable:host]"));
    TEST_EXPECT(ctx,
        file_contains(direct_path, "[executable:host]"));

    char unregistered_marker[64]{};
    const int unregistered_marker_size = std::snprintf(
        unregistered_marker,
        sizeof(unregistered_marker),
        "[unregistered-system:%016llx]",
        static_cast<unsigned long long>(unregistered_system.raw_value()));
    TEST_EXPECT(ctx, unregistered_marker_size > 0);

    char invalid_marker[64]{};
    const int invalid_marker_size = std::snprintf(
        invalid_marker,
        sizeof(invalid_marker),
        "[invalid-system:%016llx]",
        static_cast<unsigned long long>(invalid_system.raw_value()));
    TEST_EXPECT(ctx, invalid_marker_size > 0);

    const std::size_t source_file_size =
        sizeof(source_file) - 1u;
    const char* const source_suffix =
        source_file +
        (source_file_size -
            (debug_system::k_source_file_capacity - 1u));
    char source_marker[160]{};
    const int source_marker_size = std::snprintf(
        source_marker,
        sizeof(source_marker),
        "[%s:%u] typed -7 true payload",
        source_suffix,
        source_line);
    TEST_EXPECT(ctx, source_marker_size > 0);

    TEST_EXPECT(ctx, file_contains(event_path, "[0000000001]"));
    TEST_EXPECT(ctx, !file_contains(event_path, system_marker));
    TEST_EXPECT(ctx, file_contains(event_path, "[info:event]"));
    TEST_EXPECT(ctx, file_contains(event_path, "first transported event"));
    TEST_EXPECT(ctx, file_contains(event_path, "[0000000002]"));
    TEST_EXPECT(ctx, file_contains(event_path, "second transported event"));
    TEST_EXPECT(ctx, file_contains(event_path, "[0000000003]"));
    TEST_EXPECT(ctx, file_contains(event_path, "literal {braces}"));
    TEST_EXPECT(ctx, file_contains(event_path, "[0000000004]"));
    TEST_EXPECT(ctx, file_contains(event_path, "[error:event]"));
    TEST_EXPECT(ctx, file_contains(event_path, source_marker));
    TEST_EXPECT(ctx, file_contains(direct_path, "[0000000005]"));
    TEST_EXPECT(ctx, !file_contains(direct_path, system_marker));
    TEST_EXPECT(ctx, file_contains(direct_path, "[0000000006]"));
    TEST_EXPECT(ctx, file_contains(direct_path, "rich report 42 3.5"));
    TEST_EXPECT(ctx, file_contains(direct_path, "[0000000007]"));
    TEST_EXPECT(ctx, file_contains(direct_path, "[panic:event]"));
    TEST_EXPECT(ctx, file_contains(direct_path, "panic substrate record"));
    TEST_EXPECT(ctx, file_contains(event_path, "[0000000008]"));
    TEST_EXPECT(ctx, file_contains(event_path, unregistered_marker));
    TEST_EXPECT(ctx,
        file_contains(event_path, "unregistered system identity"));
    TEST_EXPECT(ctx, file_contains(event_path, "[0000000009]"));
    TEST_EXPECT(ctx, file_contains(event_path, invalid_marker));
    TEST_EXPECT(ctx, file_contains(event_path, "invalid system identity"));
#if MV_DEVELOPMENT_BUILD
    TEST_EXPECT(ctx, file_contains(event_path, "[0000000010]"));
    TEST_EXPECT(ctx,
        file_contains(event_path,
            "Assertion failed: std::uint32_t{} == 1u"));
#endif
}

void test_lazy_log_opening(TTestContext& ctx)
{
    constexpr const char* event_path = "debug_service_lazy_test.log";
    constexpr const char* direct_path = "debug_service_lazy_test_direct.log";

    TInstance<debug_system::CDebugServiceState> owner =
        TInstance<debug_system::CDebugServiceState>::create();
    TEST_EXPECT(ctx, owner.is_ready());

    debug_system::CDebugServiceState* const service = owner.operator->();
    TEST_EXPECT(ctx,
        service->configure_log_paths(event_path, direct_path));
    TEST_EXPECT(ctx, debug_system::install_service(service));
    TEST_EXPECT(ctx, service->start());

    char oversized[debug_system::k_event_text_capacity + 32u];
    std::memset(oversized, 'y', sizeof(oversized) - 1u);
    oversized[sizeof(oversized) - 1u] = 0;
    TEST_EXPECT(ctx, debug_system::submit_text(oversized));

    TEST_EXPECT(ctx, service->stop());
    TEST_EXPECT(ctx, debug_system::uninstall_service(service));
    TEST_EXPECT(ctx, file_contains(direct_path, "[0000000001] "));
}

}   //  namespace debug_service_tests

int run_debug_service_tests()
{
    debug_service_tests::TTestContext ctx;
    debug_service_tests::compile_public_macro_interface(false);
    debug_service_tests::test_exclusive_lock_try_acquire(ctx);
    debug_service_tests::test_argument_encoding(ctx);
    debug_service_tests::test_argument_formatting(ctx);
    debug_service_tests::test_system_id_name_registry(ctx);
    debug_service_tests::test_provisioning_and_shared_words(ctx);
    debug_service_tests::test_writer_and_direct_paths(ctx);
    debug_service_tests::test_lazy_log_opening(ctx);

    std::cout << "DebugService: " << ctx.passed << " passed, "
        << ctx.failed << " failed\n";
    return (ctx.failed == 0) ? 0 : 1;
}
