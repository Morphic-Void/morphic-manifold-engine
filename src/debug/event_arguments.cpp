
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

[[nodiscard]] std::size_t value_size(const EEventArgumentType type) noexcept
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
        {
            return 8u;
        }
        case EEventArgumentType::inline_text:
        {
            return k_inline_text_capacity;
        }
        case EEventArgumentType::type_id:
        {
            return sizeof(type_ids::id_type);
        }
        default:
        {
            return k_event_argument_slot_size + 1u;
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
[[nodiscard]] T read_value(
    const SEventParameterValue (&parameters)[k_event_argument_count],
    const std::size_t parameter_index) noexcept
{
    T value{};
    std::memcpy(&value, parameters[parameter_index].bytes, sizeof(value));
    return value;
}

[[nodiscard]] EEventFormatResult append_integer_hex(
    SOutput& output,
    const std::uint64_t value,
    const char insertion_specifier) noexcept
{
    const char* format = nullptr;
    switch (insertion_specifier)
    {
        case '#':
        {
            format = "#%llx";
            break;
        }
        case 'x':
        {
            format = "x%llx";
            break;
        }
        case 'X':
        {
            format = "x%llX";
            break;
        }
        default:
        {
            return EEventFormatResult::invalid_descriptor;
        }
    }

    char text[64]{};
    const int size = std::snprintf(text, sizeof(text), format, static_cast<unsigned long long>(value));

    return append(output, text, static_cast<std::size_t>(size)) ? EEventFormatResult::success : EEventFormatResult::output_too_small;
}

[[nodiscard]] EEventFormatResult append_hex_argument(
    SOutput& output,
    const SEventParameterValue (&parameters)[k_event_argument_count],
    const EEventArgumentType type,
    const std::size_t parameter_index,
    const char insertion_specifier) noexcept
{
    switch (type)
    {
        case EEventArgumentType::int32:
        case EEventArgumentType::uint32:
        {
            return append_integer_hex(
                output,
                read_value<std::uint32_t>(parameters, parameter_index),
                insertion_specifier);
        }
        case EEventArgumentType::int64:
        case EEventArgumentType::uint64:
        {
            return append_integer_hex(
                output,
                read_value<std::uint64_t>(parameters, parameter_index),
                insertion_specifier);
        }
        case EEventArgumentType::false_value:
        case EEventArgumentType::true_value:
        case EEventArgumentType::float32:
        case EEventArgumentType::float64:
        case EEventArgumentType::inline_text:
        case EEventArgumentType::type_id:
        {
            return EEventFormatResult::unsupported_argument;
        }
        default:
        {
            return EEventFormatResult::invalid_descriptor;
        }
    }
}

[[nodiscard]] EEventFormatResult append_type_id(SOutput& output, const type_ids::id_type id) noexcept
{
    const char* const name = system_id_registry::lookup_type_name(id);
    if (name != nullptr)
    {
        std::size_t name_size = 0u;
        while (name[name_size] != 0)
        {
            ++name_size;
        }

        return append(output, name, name_size) ? EEventFormatResult::success : EEventFormatResult::output_too_small;
    }

    char text[64]{};
    const bool valid = type_ids::is_valid_id(id);
    const int size = std::snprintf(text, sizeof(text),
        valid ? "unregistered-type:0x%08x" : "invalid-type:0x%08x",
        static_cast<unsigned int>(id));

    return append(output, text, static_cast<std::size_t>(size)) ? EEventFormatResult::success : EEventFormatResult::output_too_small;
}

[[nodiscard]] EEventFormatResult append_argument(
    SOutput& output,
    const SEventParameterValue (&parameters)[k_event_argument_count],
    const EEventArgumentType type,
    const std::size_t parameter_index) noexcept
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
            size = std::snprintf(text, sizeof(text), "%d", read_value<std::int32_t>(parameters, parameter_index));
            break;
        }
        case EEventArgumentType::uint32:
        {
            size = std::snprintf(text, sizeof(text), "%u", read_value<std::uint32_t>(parameters, parameter_index));
            break;
        }
        case EEventArgumentType::int64:
        {
            size = std::snprintf(text, sizeof(text), "%lld", static_cast<long long>(read_value<std::int64_t>(parameters, parameter_index)));
            break;
        }
        case EEventArgumentType::uint64:
        {
            size = std::snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(read_value<std::uint64_t>(parameters, parameter_index)));
            break;
        }
        case EEventArgumentType::float32:
        {
            size = std::snprintf(text, sizeof(text), "%.9g", static_cast<double>(read_value<float>(parameters, parameter_index)));
            break;
        }
        case EEventArgumentType::float64:
        {
            size = std::snprintf(text, sizeof(text), "%.17g", read_value<double>(parameters, parameter_index));
            break;
        }
        case EEventArgumentType::inline_text:
        {
            const char* const value = reinterpret_cast<const char*>(parameters[parameter_index].bytes);
            const void* const terminator = std::memchr(value, 0, k_inline_text_capacity);
            if (terminator == nullptr)
            {
                return EEventFormatResult::invalid_descriptor;
            }

            const std::size_t value_size = static_cast<const char*>(terminator) - value;
            return append(output, value, value_size) ? EEventFormatResult::success : EEventFormatResult::output_too_small;
        }
        case EEventArgumentType::type_id:
        {
            return append_type_id(output, read_value<type_ids::id_type>(parameters, parameter_index));
        }
        default:
        {
            return EEventFormatResult::invalid_descriptor;
        }
    }

    return append(output, text, static_cast<std::size_t>(size)) ? EEventFormatResult::success : EEventFormatResult::output_too_small;
}

[[nodiscard]] bool bytes_are_zero(
    const std::byte* const bytes, const std::size_t begin) noexcept
{
    for (std::size_t index = begin; index < k_event_argument_slot_size; ++index)
    {
        if (bytes[index] != std::byte{})
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] EEventFormatResult validate(
    const std::uint8_t parameter_count,
    const EEventArgumentType (&parameter_types)[k_event_argument_count],
    const SEventParameterValue (&parameters)[k_event_argument_count]) noexcept
{
    if (parameter_count > k_event_argument_count)
    {
        return EEventFormatResult::invalid_descriptor;
    }

    for (std::size_t index = 0u; index < parameter_count; ++index)
    {
        const EEventArgumentType type = parameter_types[index];
        const std::size_t size = value_size(type);
        if ((size > k_event_argument_slot_size) ||
            !bytes_are_zero(parameters[index].bytes, size))
        {
            return EEventFormatResult::invalid_descriptor;
        }

        if (type == EEventArgumentType::inline_text)
        {
            const void* const terminator = std::memchr(
                parameters[index].bytes, 0, k_inline_text_capacity);
            if (terminator == nullptr)
            {
                return EEventFormatResult::invalid_descriptor;
            }

            const std::size_t terminator_index =
                static_cast<const std::byte*>(terminator) - parameters[index].bytes;
            if (!bytes_are_zero(parameters[index].bytes, terminator_index + 1u))
            {
                return EEventFormatResult::invalid_descriptor;
            }
        }
    }

    for (std::size_t index = parameter_count; index < k_event_argument_count; ++index)
    {
        if ((parameter_types[index] != EEventArgumentType::unused) ||
            !bytes_are_zero(parameters[index].bytes, 0u))
        {
            return EEventFormatResult::invalid_descriptor;
        }
    }

    return EEventFormatResult::success;
}

}   //  namespace event_formatting

EEventFormatResult format_event_text(
    char* const destination, const std::size_t destination_capacity,
    const char* const format, const std::size_t format_size,
    const std::uint8_t parameter_count,
    const EEventArgumentType (&parameter_types)[k_event_argument_count],
    const SEventParameterValue (&parameters)[k_event_argument_count],
    std::size_t& out_size) noexcept
{
    out_size = 0u;
    if ((destination == nullptr) || (destination_capacity == 0u) || (format == nullptr))
    {
        return EEventFormatResult::invalid_descriptor;
    }

    destination[0] = 0;
    const EEventFormatResult validation = event_formatting::validate(
        parameter_count, parameter_types, parameters);
    if (validation != EEventFormatResult::success)
    {
        return validation;
    }

    event_formatting::SOutput output{ destination, destination_capacity, 0u };
    std::size_t argument_index = 0u;
    std::size_t literal_begin = 0u;

    for (std::size_t index = 0u; index < format_size; ++index)
    {
        const char token = format[index];
        if (token == 0)
        {
            return EEventFormatResult::malformed_format;
        }

        if ((token != '{') && (token != '}'))
        {
            continue;
        }

        if ((index + 1u) >= format_size)
        {
            return EEventFormatResult::malformed_format;
        }

        const char next = format[index + 1u];
        if ((token == '{') && (next == '}'))
        {
            std::size_t insertion_begin = index;
            if (index > literal_begin)
            {
                const char preceding = format[index - 1u];
                if ((preceding == '#') || (preceding == 'x') || (preceding == 'X'))
                {
                    --insertion_begin;
                }
            }

            if (!event_formatting::append(output, format + literal_begin, insertion_begin - literal_begin))
            {
                return EEventFormatResult::output_too_small;
            }

            if (argument_index >= parameter_count)
            {
                return EEventFormatResult::argument_mismatch;
            }

            const EEventArgumentType type = parameter_types[argument_index];
            EEventFormatResult result;
            if (insertion_begin < index)
            {
                result = event_formatting::append_hex_argument(
                    output, parameters, type, argument_index, format[insertion_begin]);
            }
            else
            {
                result = event_formatting::append_argument(
                    output, parameters, type, argument_index);
            }

            if (result != EEventFormatResult::success)
            {
                return result;
            }

            ++argument_index;
            ++index;
            literal_begin = index + 1u;
            continue;
        }

        if (next != token)
        {
            return EEventFormatResult::malformed_format;
        }

        if (!event_formatting::append(output, format + literal_begin, index - literal_begin))
        {
            return EEventFormatResult::output_too_small;
        }

        if (!event_formatting::append(output, &token, 1u))
        {
            return EEventFormatResult::output_too_small;
        }

        ++index;
        literal_begin = index + 1u;
    }

    if (!event_formatting::append(output, format + literal_begin, format_size - literal_begin))
    {
        return EEventFormatResult::output_too_small;
    }

    if (argument_index != parameter_count)
    {
        return EEventFormatResult::argument_mismatch;
    }

    out_size = output.size;
    return EEventFormatResult::success;
}

}   //  namespace debug_system
