
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   service.cpp
//  Primary implementation: OpenAI tools
//  Reviewed and accepted by: Ritchie Brannan
//  Date:   28 Jul 26
//
//  Bounded executable-owned debug service substrate.

#include "debug/service.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <limits>

#include "platform/system/debugger.hpp"
#include "platform/threading/thread_naming.hpp"
#include "platform/threading/wait_word.hpp"

namespace debug_system
{

namespace ambient_state
{

//  Each linked module stores its own installed debug service pointer.
CDebugServiceState* s_service = nullptr;

}   //  namespace ambient_state

namespace breakpoint_state
{

constexpr std::uint32_t inactive = 0u;
constexpr std::uint32_t installing = 1u;
constexpr std::uint32_t active = 2u;

}   //  namespace breakpoint_state

//==============================================================================
//  CDebugServiceState out of class function bodies
//==============================================================================

bool CDebugServiceState::copy_path(char (&destination)[k_log_path_capacity], const char* const source) noexcept
{
    if (source == nullptr)
    {
        return false;
    }

    const std::size_t length = std::strlen(source);
    if ((length == 0u) || (length >= k_log_path_capacity))
    {
        return false;
    }

    std::memcpy(destination, source, length + 1u);
    return true;
}

bool CDebugServiceState::write_record(platform::filesystem::Log& log,
    const EEventLevel level, const EEventType type, const SIncidentContext& incident,
    const char* const text, const std::size_t text_size) noexcept
{
    if (!is_valid_event_metadata(level, type))
    {
        return false;
    }

    return write_named_record(log, level_name(level), type_name(type), incident, text, text_size);
}

bool CDebugServiceState::write_named_record(platform::filesystem::Log& log,
    const char* const level, const char* const type, const SIncidentContext& incident,
    const char* const text, const std::size_t text_size) noexcept
{
    const SEventSource& source = incident.source;
    if ((level == nullptr) || (type == nullptr) ||
        (text == nullptr) || (text_size == 0u) ||
        (source.file_size >= k_source_file_capacity) ||
        (text_size > static_cast<std::size_t>(std::numeric_limits<int>::max())))
    {
        return false;
    }

    char system_name[96]{};
    std::size_t system_name_size = 0u;
    const bool have_system_name = system_id_registry::format_system_name(source.system_id, system_name, sizeof(system_name), system_name_size);

    if (have_system_name)
    {
        if (source.file_size != 0u)
        {
            return log.write("[%010u] [%.*s] [%s:%s] [%.*s:%u] %.*s\n", incident.incident_id,
                static_cast<int>(system_name_size), system_name,
                level, type, static_cast<int>(source.file_size), source.file, source.line, static_cast<int>(text_size), text) >= 0;
        }

        return log.write("[%010u] [%.*s] [%s:%s] %.*s\n", incident.incident_id,
            static_cast<int>(system_name_size), system_name, level, type, static_cast<int>(text_size), text) >= 0;
    }
    else
    {
        if (source.file_size != 0u)
        {
            return log.write("[%010u] [%s-system:%016llx] [%s:%s] [%.*s:%u] %.*s\n", incident.incident_id,
                (system_ids::is_valid_id(source.system_id) ? "unregistered" : "invalid"), static_cast<unsigned long long>(source.system_id.raw_value()),
                level, type, static_cast<int>(source.file_size), source.file, source.line, static_cast<int>(text_size), text) >= 0;
        }

        return log.write("[%010u] [%s-system:%016llx] [%s:%s] %.*s\n", incident.incident_id,
                (system_ids::is_valid_id(source.system_id) ? "unregistered" : "invalid"),
                static_cast<unsigned long long>(source.system_id.raw_value()),
                level, type, static_cast<int>(text_size), text) >= 0;
    }
}

bool CDebugServiceState::capture_source(SEventSource& destination, const system_ids::id_type system_id, const SEventUsagePoint& usage_point) noexcept
{
    if ((usage_point.file_size != 0u) && (usage_point.file == nullptr))
    {
        return false;
    }

    destination = {};
    destination.system_id = system_id;
    destination.line = usage_point.line;

    if (usage_point.file_size == 0u)
    {
        return true;
    }

    const std::size_t copied_size = (usage_point.file_size < k_source_file_capacity) ? usage_point.file_size : (k_source_file_capacity - 1u);
    const char* const copied_source = usage_point.file + (usage_point.file_size - copied_size);

    std::memcpy(destination.file, copied_source, copied_size);
    destination.file[copied_size] = 0;
    destination.file_size = static_cast<std::uint32_t>(copied_size);
    return true;
}

bool CDebugServiceState::capture_incident(SIncidentContext& destination, const SEventUsagePoint& usage_point) noexcept
{
    destination = {};
    if (!capture_source(destination.source, system_context::get_ambient_system_id(), usage_point))
    {
        return false;
    }

    destination.incident_id = allocate_incident_id();
    return true;
}

EEventFormatResult CDebugServiceState::format_event_content(
    char* const destination, const std::size_t destination_capacity,
    const char* const format, const std::size_t format_size,
    const std::uint16_t literal_prefix_size, const SEventArguments& arguments,
    std::size_t& output_size) noexcept
{
    output_size = 0u;
    if ((destination == nullptr) || (destination_capacity == 0u) ||
        (format == nullptr) || (format_size == 0u) || (literal_prefix_size > format_size))
    {
        return EEventFormatResult::malformed_format;
    }

    if (destination_capacity <= literal_prefix_size)
    {
        return EEventFormatResult::output_too_small;
    }

    for (std::size_t index = 0u; index < literal_prefix_size; ++index)
    {
        if (format[index] == 0)
        {
            return EEventFormatResult::malformed_format;
        }
    }

    std::memcpy(destination, format, literal_prefix_size);

    if (literal_prefix_size == format_size)
    {
        if ((arguments.type_tags != 0u) || (arguments.parameter_count != 0u) ||
            (arguments.payload_size != 0u) || (arguments.reserved != 0u))
        {
            return EEventFormatResult::argument_mismatch;
        }

        destination[literal_prefix_size] = 0;
        output_size = literal_prefix_size;
        return EEventFormatResult::success;
    }

    std::size_t formatted_size = 0u;
    const EEventFormatResult result = format_event_text(
        destination + literal_prefix_size, destination_capacity - literal_prefix_size,
        format + literal_prefix_size, format_size - literal_prefix_size,
        arguments, formatted_size);
    if (result == EEventFormatResult::success)
    {
        output_size = literal_prefix_size + formatted_size;
    }
    return result;
}

const char* CDebugServiceState::level_name(const EEventLevel level) noexcept
{
    switch (level)
    {
        case EEventLevel::info:
        {
            return "info";
        }
        case EEventLevel::detail:
        {
            return "detail";
        }
        case EEventLevel::trace:
        {
            return "trace";
        }
        case EEventLevel::assert:
        {
            return "assert";
        }
        case EEventLevel::warning:
        {
            return "warning";
        }
        case EEventLevel::error:
        {
            return "error";
        }
        case EEventLevel::critical:
        {
            return "critical";
        }
        case EEventLevel::fatal:
        {
            return "fatal";
        }
        default:
        {
            return "invalid";
        }
    }
}

const char* CDebugServiceState::type_name(const EEventType type) noexcept
{
    switch (type)
    {
        case EEventType::condition:
        {
            return "condition";
        }
        case EEventType::event:
        {
            return "event";
        }
        default:
        {
            return "invalid";
        }
    }
}

bool CDebugServiceState::configure_log_paths(const char* const event_path, const char* const direct_path) noexcept
{
    if ((thread_state() != EServiceThreadState::empty) || m_event_log.opened() || m_direct_log.opened())
    {
        return false;
    }

    char new_event_path[k_log_path_capacity]{};
    char new_direct_path[k_log_path_capacity]{};
    if (!copy_path(new_event_path, event_path) || !copy_path(new_direct_path, direct_path))
    {
        return false;
    }

    std::memcpy(m_event_log_path, new_event_path, k_log_path_capacity);
    std::memcpy(m_direct_log_path, new_direct_path, k_log_path_capacity);
    return true;
}

bool CDebugServiceState::open_logs() noexcept
{
    if ((thread_state() != EServiceThreadState::empty) || !m_direct_lock.is_valid() || !log_paths_configured())
    {
        return false;
    }

    const bool event_was_open = m_event_log.opened();
    if (!open_event_log())
    {
        return false;
    }

    if (open_direct_log())
    {
        return true;
    }

    if (!event_was_open)
    {
        m_event_log.close();
    }
    return false;
}

bool CDebugServiceState::start() noexcept
{
    if ((thread_state() != EServiceThreadState::empty) || !m_direct_lock.is_valid() || !log_paths_configured() || !m_event_transport.is_valid())
    {
        return false;
    }

    m_writer_state.value.store(static_cast<std::uint32_t>(EServiceThreadState::starting), std::memory_order_release);

    if (!m_writer_thread.create(&writer_thread_entry, this))
    {
        m_writer_state.value.store(static_cast<std::uint32_t>(EServiceThreadState::empty), std::memory_order_release);
        close_logs();
        return false;
    }

    platform::threading::wait_until_not_equal(m_writer_state.value, static_cast<std::uint32_t>(EServiceThreadState::starting));

    if (thread_state() == EServiceThreadState::running)
    {
        return true;
    }

    (void)m_writer_thread.join_and_close();
    close_logs();
    return false;
}

bool CDebugServiceState::stop() noexcept
{
    if ((thread_state() != EServiceThreadState::running) || !m_writer_thread.is_valid() || !m_event_transport.begin_closing())
    {
        return false;
    }

    signal_writer();
    const bool joined = m_writer_thread.join_and_close();
    const bool stopped = thread_state() == EServiceThreadState::stopped;
    close_logs();
    return joined && stopped && m_event_transport.is_closed();
}

EServiceThreadState CDebugServiceState::thread_state() const noexcept
{
    return static_cast<EServiceThreadState>(m_writer_state.value.load(std::memory_order_acquire));
}

bool CDebugServiceState::is_event_transport_closed() const noexcept
{
    return m_event_transport.is_closed();
}

std::uint32_t CDebugServiceState::allocate_incident_id() noexcept
{
    return m_incident_counter.value.fetch_add(1u, std::memory_order_relaxed) + 1u;
}

bool CDebugServiceState::submit_text(const char* const text) noexcept
{
    if ((text == nullptr) || (text[0] == 0))
    {
        return false;
    }

    SIncidentContext incident;
    if (!capture_incident(incident, SEventUsagePoint{}))
    {
        return false;
    }

    const std::size_t text_size = std::strlen(text);
    if (text_size >= k_event_text_capacity)
    {
        return write_direct_record(EEventLevel::info, EEventType::event, incident, text, text_size);
    }

    CReservedEventSlot reserved_event{ m_event_transport };

    if (!reserved_event)
    {
        return write_direct_record(EEventLevel::info, EEventType::event, incident, text, text_size);
    }

    reserved_event->incident_id = incident.incident_id;
    reserved_event->content_size = static_cast<std::uint32_t>(text_size);
    reserved_event->content_type = EEventContentType::text;
    reserved_event->level = EEventLevel::info;
    reserved_event->type = EEventType::event;
    reserved_event->literal_prefix_size = 0u;
    reserved_event->source = incident.source;
    std::memcpy(reserved_event->content, text, text_size + 1u);
    reserved_event->arguments = {};

    const bool published = reserved_event.publish();
    signal_writer();
    return published;
}

bool CDebugServiceState::report_va(const SEventUsagePoint& usage_point, const char* const format, std::va_list arguments) noexcept
{
    if ((format == nullptr) || (format[0] == 0) ||
        !m_direct_lock.is_valid())
    {
        return false;
    }

    SIncidentContext incident;
    if (!capture_incident(incident, usage_point))
    {
        return false;
    }

    m_direct_lock.acquire();
    bool formatted = false;
    bool written = false;
    bool flushed = false;
    if (m_direct_log.opened() || m_direct_log.open(m_direct_log_path))
    {
        const int formatted_size = std::vsnprintf(m_direct_format_buffer, k_format_buffer_capacity, format, arguments);
        formatted = (formatted_size > 0) && (static_cast<std::size_t>(formatted_size) < k_format_buffer_capacity);
        if (formatted)
        {
            written = write_record(m_direct_log, EEventLevel::info, EEventType::event, incident, m_direct_format_buffer, static_cast<std::size_t>(formatted_size));
        }
        else
        {
            written = write_record(m_direct_log, EEventLevel::info, EEventType::event, incident, k_invalid_report, k_invalid_report_size);
        }
        flushed = m_direct_log.flush();
    }
    m_direct_lock.release();
    return formatted && written && flushed;
}

bool CDebugServiceState::try_write_panic_record(
    const SEventUsagePoint& usage_point, const std::uint32_t incident_id,
    const char* const text, const std::size_t text_size) noexcept
{
    if ((text == nullptr) || (text_size == 0u) || !m_direct_lock.is_valid())
    {
        return false;
    }

    SIncidentContext incident;
    incident.incident_id = incident_id;
    if (!capture_source(incident.source, system_context::get_ambient_system_id(), usage_point))
    {
        return false;
    }

    if (!m_direct_lock.try_acquire())
    {
        return false;
    }

    bool written = false;
    bool flushed = false;
    if (m_direct_log.opened())
    {
        written = write_named_record(m_direct_log, "panic", "event", incident, text, text_size);
        flushed = m_direct_log.flush();
    }
    m_direct_lock.release();
    return written && flushed;
}

void CDebugServiceState::publish_configuration(const std::uint32_t configuration) noexcept
{
    m_configuration.value.store(configuration, std::memory_order_relaxed);
}

std::uint32_t CDebugServiceState::read_configuration() const noexcept
{
    return m_configuration.value.load(std::memory_order_relaxed);
}

bool CDebugServiceState::informational_event_enabled(const EEventLevel level) const noexcept
{
    std::uint32_t required_level = 0u;

    switch (level)
    {
        case EEventLevel::info:
        {
            required_level = 1u;
            break;
        }
        case EEventLevel::detail:
        {
            required_level = 2u;
            break;
        }
        case EEventLevel::trace:
        {
            required_level = 3u;
            break;
        }
        default:
        {
            return false;
        }
    }

    const std::uint32_t configured_level =
        (read_configuration() & k_information_level_mask) >> k_information_level_shift;
    return configured_level >= required_level;
}

bool CDebugServiceState::critical_shutdown_enabled() const noexcept
{
    return (read_configuration() & k_critical_shutdown_enabled) != 0u;
}

bool CDebugServiceState::breakpoint_enabled(const EBreakpointOverride override, const bool enabled_by_default) const noexcept
{
    if ((read_configuration() & k_breakpoints_enabled) == 0u)
    {
        return false;
    }

    switch (override)
    {
        case EBreakpointOverride::inherit:
        {
            return enabled_by_default;
        }
        case EBreakpointOverride::enabled:
        {
            return true;
        }
        case EBreakpointOverride::disabled:
        {
            return false;
        }
        default:
        {
            return false;
        }
    }
}

bool CDebugServiceState::breakpoint_pause_requested() const noexcept
{
    return m_breakpoint_state.value.load(std::memory_order_acquire) != breakpoint_state::inactive;
}

bool CDebugServiceState::read_breakpoint_context(SBreakpointContext& destination) const noexcept
{
    if (m_breakpoint_state.value.load(std::memory_order_acquire) != breakpoint_state::active)
    {
        return false;
    }

    SBreakpointContext context;
    context.incident_id = m_breakpoint_incident_id.load(std::memory_order_relaxed);
    context.system_id = system_ids::id_type(m_breakpoint_system_id.load(std::memory_order_relaxed));
    context.source_line = m_breakpoint_source_line.load(std::memory_order_relaxed);

    if (m_breakpoint_state.value.load(std::memory_order_acquire) != breakpoint_state::active)
    {
        return false;
    }

    destination = context;
    return true;
}

void CDebugServiceState::react_to_breakpoint(EBreakpointOverride& override, const bool enabled_by_default, const SIncidentContext& incident) noexcept
{
    if (!breakpoint_enabled(override, enabled_by_default))
    {
        return;
    }

    std::uint32_t expected = breakpoint_state::inactive;
    const bool owns_context = m_breakpoint_state.value.compare_exchange_strong(expected, breakpoint_state::installing, std::memory_order_acq_rel, std::memory_order_relaxed);

    if (owns_context)
    {
        m_breakpoint_incident_id.store(incident.incident_id, std::memory_order_relaxed);
        m_breakpoint_system_id.store(incident.source.system_id.raw_value(), std::memory_order_relaxed);
        m_breakpoint_source_line.store(incident.source.line, std::memory_order_relaxed);
        m_breakpoint_state.value.store(breakpoint_state::active, std::memory_order_release);
    }

    platform::system::break_into_debugger();

    if (owns_context)
    {
        m_breakpoint_state.value.store(breakpoint_state::inactive, std::memory_order_release);
    }
}

void CDebugServiceState::request_shutdown(const EShutdownReason reason) noexcept
{
    const std::uint32_t requested = static_cast<std::uint32_t>(reason);
    std::uint32_t current = m_shutdown_request.value.load(std::memory_order_relaxed);

    while ((current < requested) && !m_shutdown_request.value.compare_exchange_weak(current, requested, std::memory_order_release, std::memory_order_relaxed))
    {
    }
}

EShutdownReason CDebugServiceState::read_shutdown_request() const noexcept
{
    return static_cast<EShutdownReason>(m_shutdown_request.value.load(std::memory_order_acquire));
}

bool CDebugServiceState::log_paths_configured() const noexcept
{
    return (m_event_log_path[0] != 0) && (m_direct_log_path[0] != 0);
}

bool CDebugServiceState::open_event_log() noexcept
{
    return m_event_log.opened() || m_event_log.open(m_event_log_path);
}

bool CDebugServiceState::open_direct_log() noexcept
{
    m_direct_lock.acquire();
    const bool opened = m_direct_log.opened() || m_direct_log.open(m_direct_log_path);
    m_direct_lock.release();
    return opened;
}

void CDebugServiceState::close_logs() noexcept
{
    m_event_log.close();

    m_direct_lock.acquire();
    m_direct_log.close();
    m_direct_lock.release();
}

void CDebugServiceState::signal_writer() noexcept
{
    m_writer_wake_epoch.value.fetch_add(1u, std::memory_order_release);
    platform::threading::wake_one_waiter(m_writer_wake_epoch.value);
}

bool CDebugServiceState::write_direct_record(
    const EEventLevel level, const EEventType type, const SIncidentContext& incident,
    const char* const text, const std::size_t text_size) noexcept
{
    if (!m_direct_lock.is_valid())
    {
        return false;
    }

    m_direct_lock.acquire();
    bool written = false;
    bool flushed = false;
    if (m_direct_log.opened() || m_direct_log.open(m_direct_log_path))
    {
        written = write_record(m_direct_log, level, type, incident, text, text_size);
        flushed = m_direct_log.flush();
    }
    m_direct_lock.release();
    return written && flushed;
}

bool CDebugServiceState::write_direct_event(
    const EEventLevel level, const EEventType type, const SIncidentContext& incident,
    const char* const format, const std::size_t format_size,
    const std::uint16_t literal_prefix_size, const SEventArguments& arguments) noexcept
{
    if (!m_direct_lock.is_valid())
    {
        return false;
    }

    m_direct_lock.acquire();
    bool written = false;
    bool flushed = false;
    if (m_direct_log.opened() || m_direct_log.open(m_direct_log_path))
    {
        std::size_t text_size = 0u;
        const EEventFormatResult result = format_event_content(
            m_direct_format_buffer, k_format_buffer_capacity, format, format_size,
            literal_prefix_size, arguments, text_size);

        if (result == EEventFormatResult::success)
        {
            written = write_record(m_direct_log, level, type, incident, m_direct_format_buffer, text_size);
        }
        else
        {
            written = write_record(m_direct_log, level, type, incident, k_invalid_event, k_invalid_event_size);
        }

        flushed = m_direct_log.flush();
    }
    m_direct_lock.release();
    return written && flushed;
}

bool CDebugServiceState::write_event(const SEvent& event) noexcept
{
    const SIncidentContext incident{ event.incident_id, event.source };

    if (!is_valid_event_metadata(event.level, event.type) || (event.literal_prefix_size > event.content_size))
    {
        return write_direct_record(EEventLevel::error, EEventType::event, incident, k_invalid_event, k_invalid_event_size);
    }

    if ((event.content_size == 0u) || (event.content_size >= k_event_text_capacity))
    {
        return write_direct_record(event.level, event.type, incident, k_invalid_event, k_invalid_event_size);
    }

    if (event.content_type == EEventContentType::text)
    {
        if (event.literal_prefix_size != 0u)
        {
            return write_direct_record(event.level, event.type, incident, k_invalid_event, k_invalid_event_size);
        }

        if (write_record(m_event_log, event.level, event.type, incident, event.content, event.content_size))
        {
            return true;
        }

        return write_direct_record(event.level, event.type, incident, event.content, event.content_size);
    }

    if (event.content_type != EEventContentType::format)
    {
        return write_direct_record(event.level, event.type, incident, k_invalid_event, k_invalid_event_size);
    }

    std::size_t text_size = 0u;
    const EEventFormatResult result = format_event_content(
        m_event_format_buffer, k_format_buffer_capacity, event.content, event.content_size,
        event.literal_prefix_size, event.arguments, text_size);
    if (result != EEventFormatResult::success)
    {
        return write_direct_record(event.level, event.type, incident, k_invalid_event, k_invalid_event_size);
    }

    if (write_record(m_event_log, event.level, event.type, incident, m_event_format_buffer, text_size))
    {
        return true;
    }

    return write_direct_record(event.level, event.type, incident, m_event_format_buffer, text_size);
}

std::uint32_t CDebugServiceState::writer_thread_entry(void* const user_data) noexcept
{
    if (user_data == nullptr)
    {
        return 1u;
    }

    CDebugServiceState& service = *static_cast<CDebugServiceState*>(user_data);
    return service.writer_thread_main();
}

std::uint32_t CDebugServiceState::writer_thread_main() noexcept
{
    (void)system_context::set_ambient_thread_id(thread_ids::debug_service);
    const char* const thread_name = system_id_registry::lookup_thread_name(thread_ids::debug_service);
    if (thread_name != nullptr)
    {
        (void)platform::threading::set_current_thread_name(thread_name);
    }

    if (!open_event_log())
    {
        m_writer_state.value.store(static_cast<std::uint32_t>(EServiceThreadState::failed), std::memory_order_release);
        platform::threading::wake_all_waiters(m_writer_state.value);
        return 2u;
    }

    m_writer_state.value.store(static_cast<std::uint32_t>(EServiceThreadState::running), std::memory_order_release);
    platform::threading::wake_all_waiters(m_writer_state.value);

    for (;;)
    {
        const std::uint32_t observed_epoch = m_writer_wake_epoch.value.load(std::memory_order_acquire);

        for (;;)
        {
            CAcquiredEventSlot acquired_event{ m_event_transport };

            if (!acquired_event)
            {
                break;
            }

            (void)write_event(*acquired_event);
        }

        if (m_event_transport.is_closed() || m_event_transport.is_shutdown())
        {
            break;
        }

        if (m_writer_wake_epoch.value.load(std::memory_order_acquire) == observed_epoch)
        {
            platform::threading::wait_while_equal(m_writer_wake_epoch.value, observed_epoch);
        }
    }

    (void)m_event_log.flush_durable();
    m_event_log.close();
    m_writer_state.value.store(static_cast<std::uint32_t>(EServiceThreadState::stopped), std::memory_order_release);
    platform::threading::wake_all_waiters(m_writer_state.value);
    return 0u;
}

CDebugServiceState* get_service() noexcept
{
    return ambient_state::s_service;
}

bool install_service(CDebugServiceState* const service) noexcept
{
    if ((service == nullptr) || (ambient_state::s_service != nullptr))
    {
        return false;
    }

    ambient_state::s_service = service;
    return true;
}

bool uninstall_service(CDebugServiceState* const expected) noexcept
{
    if ((expected == nullptr) || (ambient_state::s_service != expected))
    {
        return false;
    }

    ambient_state::s_service = nullptr;
    return true;
}

bool submit_text(const char* const text) noexcept
{
    CDebugServiceState* const service = get_service();
    return (service != nullptr) && service->submit_text(text);
}

bool informational_event_enabled(const EEventLevel level) noexcept
{
    CDebugServiceState* const service = get_service();
    return (service != nullptr) && service->informational_event_enabled(level);
}

bool report(const char* const source_file, const std::size_t source_file_size, const std::uint32_t source_line, const char* const format, ...) noexcept
{
    CDebugServiceState* const service = get_service();
    if (service == nullptr)
    {
        return false;
    }

    std::va_list arguments;
    va_start(arguments, format);
    const SEventUsagePoint usage_point{ source_file, source_file_size, source_line };
    const bool reported = service->report_va(usage_point, format, arguments);
    va_end(arguments);
    return reported;
}

[[noreturn]] void panic(const char* const source_file, const std::size_t source_file_size, const std::uint32_t source_line, const char* const format, ...) noexcept
{
    constexpr char format_failure[] = "Invalid panic report";
    char text[k_format_buffer_capacity]{};

    int formatted_size = -1;
    if ((format != nullptr) && (format[0] != 0))
    {
        std::va_list arguments;
        va_start(arguments, format);
        formatted_size = std::vsnprintf(text, sizeof(text), format, arguments);
        va_end(arguments);
    }

    const bool formatted = (formatted_size > 0) && (static_cast<std::size_t>(formatted_size) < sizeof(text));
    const char* const panic_text = formatted ? text : format_failure;
    const std::size_t panic_text_size = formatted ? static_cast<std::size_t>(formatted_size) : sizeof(format_failure) - 1u;

    CDebugServiceState* const service = get_service();
    if (service != nullptr)
    {
        service->request_shutdown(EShutdownReason::panic_incident);
        const std::uint32_t incident_id = service->allocate_incident_id();
        const SEventUsagePoint usage_point{ source_file, source_file_size, source_line };
        (void)service->try_write_panic_record(usage_point, incident_id, panic_text, panic_text_size);
    }

    platform::system::write_debugger_output("PANIC: ");
    platform::system::write_debugger_output(panic_text);
    platform::system::write_debugger_output("\n");

    if (platform::system::debugger_attached())
    {
        platform::system::break_into_debugger();
    }

    platform::system::fail_fast();
}

}   //  namespace debug_system
