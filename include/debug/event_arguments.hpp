
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   event_arguments.hpp
//  Author: OpenAI Codex
//  Date:   29 Jul 26
//
//  Bounded argument encoding for transported debug events.

#pragma once

#ifndef DEBUG_EVENT_ARGUMENTS_HPP_INCLUDED
#define DEBUG_EVENT_ARGUMENTS_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace debug_system
{

constexpr std::size_t k_event_argument_count = 8u;
constexpr std::size_t k_event_argument_payload_capacity = 64u;
constexpr std::size_t k_inline_text_capacity = 16u;

enum class EEventArgumentType : std::uint8_t
{
    unused = 0u,
    false_value = 1u,
    true_value = 2u,
    int32 = 3u,
    uint32 = 4u,
    int64 = 5u,
    uint64 = 6u,
    float32 = 7u,
    float64 = 8u,
    inline_text = 9u,
    external_string_reference = 10u,
    external_path_reference = 11u
};

enum class EEventFormatResult : std::uint32_t
{
    success = 0u,
    invalid_descriptor,
    malformed_format,
    argument_mismatch,
    unsupported_argument,
    output_too_small
};

class CInlineText16
{
public:
    constexpr CInlineText16() noexcept = default;

    template<std::size_t t_size>
    explicit constexpr CInlineText16(const char (&text)[t_size]) noexcept
    {
        static_assert((t_size > 0u), "CInlineText16 requires storage for a terminator.");
        static_assert((t_size <= k_inline_text_capacity), "CInlineText16 text exceeds 15 characters plus its terminator.");

        for (std::size_t index = 0u; index < (t_size - 1u); ++index)
        {
            m_text[index] = text[index];
        }
    }

    [[nodiscard]] constexpr const char* data() const noexcept
    {
        return m_text;
    }

private:
    char m_text[k_inline_text_capacity]{};
};

struct SEventArguments
{
    std::uint32_t type_tags = 0u;
    std::uint8_t parameter_count = 0u;
    std::uint8_t payload_size = 0u;
    std::uint16_t reserved = 0u;
    std::byte payload[k_event_argument_payload_capacity]{};
};

static_assert((sizeof(SEventArguments) == 72u), "SEventArguments must retain its fixed transport representation.");

namespace event_argument_detail
{

template<typename T>
using CArgumentType = std::remove_cv_t<std::remove_reference_t<T>>;

template<typename T>
struct TArgumentTraits
{
    static constexpr bool k_supported = false;
    static constexpr EEventArgumentType k_type = EEventArgumentType::unused;
    static constexpr std::size_t k_payload_size = 0u;
};

#define MV_DEFINE_EVENT_ARGUMENT_TRAITS(cpp_type, argument_type) \
    template<> \
    struct TArgumentTraits<cpp_type> \
    { \
        static constexpr bool k_supported = true; \
        static constexpr EEventArgumentType k_type = argument_type; \
        static constexpr std::size_t k_payload_size = sizeof(cpp_type); \
    }

MV_DEFINE_EVENT_ARGUMENT_TRAITS(std::int32_t, EEventArgumentType::int32);
MV_DEFINE_EVENT_ARGUMENT_TRAITS(std::uint32_t, EEventArgumentType::uint32);
MV_DEFINE_EVENT_ARGUMENT_TRAITS(std::int64_t, EEventArgumentType::int64);
MV_DEFINE_EVENT_ARGUMENT_TRAITS(std::uint64_t, EEventArgumentType::uint64);
MV_DEFINE_EVENT_ARGUMENT_TRAITS(float, EEventArgumentType::float32);
MV_DEFINE_EVENT_ARGUMENT_TRAITS(double, EEventArgumentType::float64);
MV_DEFINE_EVENT_ARGUMENT_TRAITS(CInlineText16, EEventArgumentType::inline_text);

#undef MV_DEFINE_EVENT_ARGUMENT_TRAITS

template<>
struct TArgumentTraits<bool>
{
    static constexpr bool k_supported = true;
    static constexpr EEventArgumentType k_type = EEventArgumentType::unused;
    static constexpr std::size_t k_payload_size = 0u;
};

template<typename T>
inline constexpr bool k_supported = TArgumentTraits<CArgumentType<T>>::k_supported;

template<typename... Args>
inline constexpr std::size_t k_payload_size = (0u + ... + TArgumentTraits<CArgumentType<Args>>::k_payload_size);

template<typename T>
void encode_argument(SEventArguments& destination, T&& argument, const std::size_t parameter_index, std::size_t& payload_offset) noexcept
{
    using CValue = CArgumentType<T>;

    EEventArgumentType type = TArgumentTraits<CValue>::k_type;
    if constexpr (std::is_same_v<CValue, bool>)
    {
        type = argument
            ? EEventArgumentType::true_value
            : EEventArgumentType::false_value;
    }

    destination.type_tags |= static_cast<std::uint32_t>(type) << static_cast<std::uint32_t>(parameter_index * 4u);

    if constexpr (TArgumentTraits<CValue>::k_payload_size != 0u)
    {
        if constexpr (std::is_same_v<CValue, CInlineText16>)
        {
            std::memcpy((destination.payload + payload_offset), argument.data(), k_inline_text_capacity);
        }
        else
        {
            const CValue value = std::forward<T>(argument);
            std::memcpy((destination.payload + payload_offset), &value, sizeof(value));
        }

        payload_offset += TArgumentTraits<CValue>::k_payload_size;
    }
}

}   //  namespace event_argument_detail

template<typename T>
inline constexpr bool is_supported_event_argument_v = event_argument_detail::k_supported<T>;

template<typename... Args>
void encode_event_arguments_into(SEventArguments& result, Args&&... arguments) noexcept
{
    static_assert((sizeof...(Args) <= k_event_argument_count),
        "A debug event accepts at most eight dynamic arguments.");
    static_assert((event_argument_detail::k_supported<Args> && ...),
        "A debug event argument has an unsupported type.");
    static_assert((event_argument_detail::k_payload_size<Args...> <= k_event_argument_payload_capacity),
        "The encoded debug event arguments exceed 64 bytes.");

    result = {};
    result.parameter_count = static_cast<std::uint8_t>(sizeof...(Args));
    result.payload_size = static_cast<std::uint8_t>(event_argument_detail::k_payload_size<Args...>);

    std::size_t parameter_index = 0u;
    std::size_t payload_offset = 0u;
    (event_argument_detail::encode_argument(result, std::forward<Args>(arguments), parameter_index++, payload_offset), ...);
}

template<typename... Args>
[[nodiscard]] SEventArguments encode_event_arguments(Args&&... arguments) noexcept
{
    SEventArguments result{};
    encode_event_arguments_into(result, std::forward<Args>(arguments)...);
    return result;
}

[[nodiscard]] EEventFormatResult format_event_text(
    char* const destination, const std::size_t destination_capacity,
    const char* const format, const std::size_t format_size,
    const SEventArguments& arguments, std::size_t& out_size) noexcept;

}   //  namespace debug_system

#endif  //  #ifndef DEBUG_EVENT_ARGUMENTS_HPP_INCLUDED
