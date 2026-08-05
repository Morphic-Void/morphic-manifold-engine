
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   event_arguments.cpp
//  Primary implementation: OpenAI tools
//  Reviewed and accepted by: Ritchie Brannan
//  Date:   29 Jul 26
//
//  Bounded argument decoding and formatting for transported debug events.

#include "debug/event_arguments.hpp"

#include <cstdio>
#include <cstring>

namespace debug_system
{

namespace event_formatting
{

struct SOutput
{
    char* destination;
    std::size_t capacity;
    std::size_t size;
};

[[nodiscard]] EEventArgumentType get_type(const SEventArguments& arguments, const std::size_t index) noexcept
{
    return static_cast<EEventArgumentType>((arguments.type_tags >> static_cast<std::uint32_t>(index * 4u)) & 0x0fu);
}

[[nodiscard]] std::size_t payload_size(const EEventArgumentType type) noexcept
{
    switch (type)
    {
        case EEventArgumentType::false_value:
        case EEventArgumentType::true_value:
        {
            return 0u;
        }
        case EEventArgumentType::int32:
        case EEventArgumentType::uint32:
        case EEventArgumentType::float32:
        {
            return 4u;
        }
        case EEventArgumentType::int64:
        case EEventArgumentType::uint64:
        case EEventArgumentType::float64:
        case EEventArgumentType::external_string_reference:
        case EEventArgumentType::external_path_reference:
        {
            return 8u;
        }
        case EEventArgumentType::inline_text:
        {
            return k_inline_text_capacity;
        }
        default:
        {
            return k_event_argument_payload_capacity + 1u;
        }
    }
}

[[nodiscard]] bool append(SOutput& output, const char* const text, const std::size_t size) noexcept
{
    if ((size >= output.capacity) || (output.size > (output.capacity - size - 1u)))
    {
        return false;
    }

    std::memcpy(output.destination + output.size, text, size);
    output.size += size;
    output.destination[output.size] = 0;
    return true;
}

template<typename T>
[[nodiscard]] T read_value(const SEventArguments& arguments, const std::size_t payload_offset) noexcept
{
    T value{};
    std::memcpy(&value, arguments.payload + payload_offset, sizeof(value));
    return value;
}

[[nodiscard]] EEventFormatResult append_argument(SOutput& output, const SEventArguments& arguments, const EEventArgumentType type, const std::size_t payload_offset) noexcept
{
    char text[64]{};
    int size = 0;

    switch (type)
    {
        case EEventArgumentType::false_value:
        {
            return append(output, "false", 5u) ? EEventFormatResult::success : EEventFormatResult::output_too_small;
        }
        case EEventArgumentType::true_value:
        {
            return append(output, "true", 4u) ? EEventFormatResult::success : EEventFormatResult::output_too_small;
        }
        case EEventArgumentType::int32:
        {
            size = std::snprintf(text, sizeof(text), "%d", read_value<std::int32_t>(arguments, payload_offset));
            break;
        }
        case EEventArgumentType::uint32:
        {
            size = std::snprintf(text, sizeof(text), "%u", read_value<std::uint32_t>(arguments, payload_offset));
            break;
        }
        case EEventArgumentType::int64:
        {
            size = std::snprintf(text, sizeof(text), "%lld", static_cast<long long>(read_value<std::int64_t>(arguments, payload_offset)));
            break;
        }
        case EEventArgumentType::uint64:
        {
            size = std::snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(read_value<std::uint64_t>(arguments, payload_offset)));
            break;
        }
        case EEventArgumentType::float32:
        {
            size = std::snprintf(text, sizeof(text), "%.9g", static_cast<double>(read_value<float>(arguments, payload_offset)));
            break;
        }
        case EEventArgumentType::float64:
        {
            size = std::snprintf(text, sizeof(text), "%.17g", read_value<double>(arguments, payload_offset));
            break;
        }
        case EEventArgumentType::inline_text:
        {
            const char* const value = reinterpret_cast<const char*>(arguments.payload + payload_offset);
            const void* const terminator = std::memchr(value, 0, k_inline_text_capacity);
            if (terminator == nullptr)
            {
                return EEventFormatResult::invalid_descriptor;
            }

            const std::size_t value_size = static_cast<const char*>(terminator) - value;
            return append(output, value, value_size) ? EEventFormatResult::success : EEventFormatResult::output_too_small;
        }
        case EEventArgumentType::external_string_reference:
        case EEventArgumentType::external_path_reference:
        {
            return EEventFormatResult::unsupported_argument;
        }
        default:
        {
            return EEventFormatResult::invalid_descriptor;
        }
    }

    if ((size < 0) || (static_cast<std::size_t>(size) >= sizeof(text)))
    {
        return EEventFormatResult::invalid_descriptor;
    }

    return append(output, text, static_cast<std::size_t>(size)) ? EEventFormatResult::success : EEventFormatResult::output_too_small;
}

[[nodiscard]] EEventFormatResult validate(
    const SEventArguments& arguments) noexcept
{
    if ((arguments.parameter_count > k_event_argument_count) ||
        (arguments.payload_size > k_event_argument_payload_capacity) ||
        (arguments.reserved != 0u))
    {
        return EEventFormatResult::invalid_descriptor;
    }

    if ((arguments.parameter_count < k_event_argument_count) &&
        ((arguments.type_tags >> static_cast<std::uint32_t>(arguments.parameter_count * 4u)) != 0u))
    {
        return EEventFormatResult::invalid_descriptor;
    }

    std::size_t expected_payload_size = 0u;
    for (std::size_t index = 0u; index < arguments.parameter_count; ++index)
    {
        const std::size_t size = payload_size(get_type(arguments, index));
        if ((size > k_event_argument_payload_capacity) || (expected_payload_size > (k_event_argument_payload_capacity - size)))
        {
            return EEventFormatResult::invalid_descriptor;
        }

        expected_payload_size += size;
    }

    return (expected_payload_size == arguments.payload_size) ? EEventFormatResult::success : EEventFormatResult::invalid_descriptor;
}

}   //  namespace event_formatting

EEventFormatResult format_event_text(
    char* const destination, const std::size_t destination_capacity,
    const char* const format, const std::size_t format_size,
    const SEventArguments& arguments, std::size_t& out_size) noexcept
{
    out_size = 0u;
    if ((destination == nullptr) || (destination_capacity == 0u) || (format == nullptr))
    {
        return EEventFormatResult::invalid_descriptor;
    }

    destination[0] = 0;
    const EEventFormatResult validation = event_formatting::validate(arguments);
    if (validation != EEventFormatResult::success)
    {
        return validation;
    }

    event_formatting::SOutput output{ destination, destination_capacity, 0u };
    std::size_t argument_index = 0u;
    std::size_t payload_offset = 0u;

    for (std::size_t index = 0u; index < format_size; ++index)
    {
        const char character = format[index];
        if (character == 0)
        {
            return EEventFormatResult::malformed_format;
        }

        if ((character != '{') && (character != '}'))
        {
            if (!event_formatting::append(output, &character, 1u))
            {
                return EEventFormatResult::output_too_small;
            }
            continue;
        }

        if ((index + 1u) >= format_size)
        {
            return EEventFormatResult::malformed_format;
        }

        const char next = format[index + 1u];
        if (next == character)
        {
            if (!event_formatting::append(output, &character, 1u))
            {
                return EEventFormatResult::output_too_small;
            }
            ++index;
            continue;
        }

        if ((character != '{') || (next != '}'))
        {
            return EEventFormatResult::malformed_format;
        }

        if (argument_index >= arguments.parameter_count)
        {
            return EEventFormatResult::argument_mismatch;
        }

        const EEventArgumentType type = event_formatting::get_type(arguments, argument_index);
        const EEventFormatResult result = event_formatting::append_argument(output, arguments, type, payload_offset);
        if (result != EEventFormatResult::success)
        {
            return result;
        }

        payload_offset += event_formatting::payload_size(type);
        ++argument_index;
        ++index;
    }

    if (argument_index != arguments.parameter_count)
    {
        return EEventFormatResult::argument_mismatch;
    }

    out_size = output.size;
    return EEventFormatResult::success;
}

}   //  namespace debug_system
