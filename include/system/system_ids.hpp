
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   system_ids.hpp
//  Author: Ritchie Brannan
//  Date:   24 Feb 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Fixed encoded system id spaces for closed-system use across DLLs.
//
//  Defines encoded erased type ids, thread ids, and module ids, and
//  combined module/thread system ids, together with validation, encode,
//  and decode helpers.
//
//  This file defines ids and id-handling helpers only. It does not
//  bind ids to C++ types.

#pragma once

#ifndef SYSTEM_IDS_HPP_INCLUDED
#define SYSTEM_IDS_HPP_INCLUDED

#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::int32_t, std::uint32_t

#include "bit_utils/bit_ops.hpp"

//==============================================================================
//  Helper utility for id handling
//==============================================================================

namespace system_id_util
{

//------------------------------------------------------------------------------

template<std::size_t t_encoded_field>
struct TField
{
    static constexpr std::size_t k_id_field_mask = t_encoded_field;
    static constexpr std::size_t k_payload_mask = (std::size_t{ 1 } << bit_ops::count_set_bits(k_id_field_mask)) - 1u;
    static constexpr std::size_t k_max_id_count = k_payload_mask;
    static constexpr std::size_t k_invalid_id_mask = ~k_id_field_mask;
    static constexpr std::int32_t k_id_field_shift = bit_ops::lo_bit_index(k_id_field_mask);

    static constexpr bool is_valid_id(const std::size_t id) noexcept
    {
        return ((id != 0u) && ((id & k_invalid_id_mask) == 0u));
    }

    static constexpr std::size_t encode_id(const std::size_t value) noexcept
    {
        return (value < k_max_id_count)
            ? (bit_ops::spread_to_even_bits(value ^ k_payload_mask) << k_id_field_shift)
            : std::size_t{ 0u };
    }

    static constexpr std::size_t decode_id(const std::size_t id) noexcept
    {
        return is_valid_id(id)
            ? (bit_ops::pack_from_even_bits(id >> k_id_field_shift) ^ k_payload_mask)
            : std::size_t{ 0u };
    }
};

//------------------------------------------------------------------------------

//  The encoding spaces
using   type_ids_field = TField<0x55555555u>;
using module_ids_field = TField<0xAAA00000u>;
using thread_ids_field = TField<0x000AAAAAu>;

//------------------------------------------------------------------------------

}	//	namespace system_id_util

//==============================================================================
//  Type ids
//==============================================================================

namespace type_ids
{

//------------------------------------------------------------------------------

using field = system_id_util::type_ids_field;

//------------------------------------------------------------------------------

constexpr bool is_valid_id(const std::size_t id) noexcept { return field::is_valid_id(id); }
constexpr std::size_t encode_id(const std::size_t value) noexcept { return field::encode_id(value); }
constexpr std::size_t decode_id(const std::size_t id) noexcept { return field::decode_id(id); }

//------------------------------------------------------------------------------

constexpr std::size_t byte_buffer = encode_id(1u);
constexpr std::size_t byte_rect_buffer = encode_id(2u);
constexpr std::size_t simple_string = encode_id(3u);
constexpr std::size_t string_buffer = encode_id(4u);
constexpr std::size_t stable_strings = encode_id(5u);

//------------------------------------------------------------------------------

}   //  namespace type_ids

//==============================================================================
//  Module ids
//==============================================================================

namespace module_ids
{

//------------------------------------------------------------------------------

using field = system_id_util::module_ids_field;

//------------------------------------------------------------------------------

constexpr bool is_valid_id(const std::size_t id) noexcept { return field::is_valid_id(id); }
constexpr std::size_t encode_id(const std::size_t value) noexcept { return field::encode_id(value); }
constexpr std::size_t decode_id(const std::size_t id) noexcept { return field::decode_id(id); }

//------------------------------------------------------------------------------

constexpr std::size_t executable = encode_id(1u);
constexpr std::size_t application = encode_id(2u);

constexpr std::size_t platform_windows = encode_id(4u);
constexpr std::size_t platform_linux = encode_id(5u);
constexpr std::size_t platform_osx = encode_id(6u);

constexpr std::size_t conditioning_general = encode_id(8u);
constexpr std::size_t conditioning_directx_windows = encode_id(9u);
constexpr std::size_t conditioning_vulkan_windows = encode_id(10u);
constexpr std::size_t conditioning_vulkan_linux = encode_id(11u);
constexpr std::size_t conditioning_vulkan_osx = encode_id(12u);

constexpr std::size_t rendering_directx_windows = encode_id(16u);
constexpr std::size_t rendering_vulkan_windows = encode_id(17u);
constexpr std::size_t rendering_vulkan_linux = encode_id(18u);
constexpr std::size_t rendering_vulkan_osx = encode_id(19u);

//------------------------------------------------------------------------------

}   //  namespace module_ids

//==============================================================================
//  Thread ids
//==============================================================================

namespace thread_ids
{

//------------------------------------------------------------------------------

using field = system_id_util::thread_ids_field;

//------------------------------------------------------------------------------

constexpr bool is_valid_id(const std::size_t id) noexcept { return field::is_valid_id(id); }
constexpr std::size_t encode_id(const std::size_t value) noexcept { return field::encode_id(value); }
constexpr std::size_t decode_id(const std::size_t id) noexcept { return field::decode_id(id); }

//------------------------------------------------------------------------------

constexpr std::size_t host = encode_id(1u);
constexpr std::size_t application = encode_id(2u);

constexpr std::size_t rendering = encode_id(4u);
constexpr std::size_t rhi = encode_id(5u);

constexpr std::size_t physics = encode_id(8u);

constexpr std::size_t background_file_io = encode_id(16u);
constexpr std::size_t background_conditioning = encode_id(17u);

constexpr std::size_t jobs_worker_00 = encode_id(32u);
constexpr std::size_t jobs_worker_01 = encode_id(33u);
constexpr std::size_t jobs_worker_02 = encode_id(34u);
constexpr std::size_t jobs_worker_03 = encode_id(35u);
constexpr std::size_t jobs_worker_04 = encode_id(36u);
constexpr std::size_t jobs_worker_05 = encode_id(37u);
constexpr std::size_t jobs_worker_06 = encode_id(38u);
constexpr std::size_t jobs_worker_07 = encode_id(39u);
constexpr std::size_t jobs_worker_08 = encode_id(40u);
constexpr std::size_t jobs_worker_09 = encode_id(41u);
constexpr std::size_t jobs_worker_10 = encode_id(42u);
constexpr std::size_t jobs_worker_11 = encode_id(43u);
constexpr std::size_t jobs_worker_12 = encode_id(44u);
constexpr std::size_t jobs_worker_13 = encode_id(45u);
constexpr std::size_t jobs_worker_14 = encode_id(46u);
constexpr std::size_t jobs_worker_15 = encode_id(47u);
constexpr std::size_t jobs_worker_16 = encode_id(48u);
constexpr std::size_t jobs_worker_17 = encode_id(49u);
constexpr std::size_t jobs_worker_18 = encode_id(50u);
constexpr std::size_t jobs_worker_19 = encode_id(51u);
constexpr std::size_t jobs_worker_20 = encode_id(52u);
constexpr std::size_t jobs_worker_21 = encode_id(53u);
constexpr std::size_t jobs_worker_22 = encode_id(54u);
constexpr std::size_t jobs_worker_23 = encode_id(55u);
constexpr std::size_t jobs_worker_24 = encode_id(56u);
constexpr std::size_t jobs_worker_25 = encode_id(57u);
constexpr std::size_t jobs_worker_26 = encode_id(58u);
constexpr std::size_t jobs_worker_27 = encode_id(59u);
constexpr std::size_t jobs_worker_28 = encode_id(60u);
constexpr std::size_t jobs_worker_29 = encode_id(61u);
constexpr std::size_t jobs_worker_30 = encode_id(62u);
constexpr std::size_t jobs_worker_31 = encode_id(63u);

//------------------------------------------------------------------------------

}   //  namespace thread_ids

//==============================================================================
//  Combined system ids
//==============================================================================

namespace system_ids
{

//------------------------------------------------------------------------------

constexpr std::size_t k_module_id_mask = system_id_util::module_ids_field::k_id_field_mask;
constexpr std::size_t k_thread_id_mask = system_id_util::thread_ids_field::k_id_field_mask;
constexpr std::size_t k_invalid_id_mask = ~(k_module_id_mask | k_thread_id_mask);

//------------------------------------------------------------------------------

constexpr bool is_valid_id(const std::size_t system_id) noexcept
{
    return
        ((system_id & k_invalid_id_mask) == 0u) &&
        ((system_id & k_module_id_mask) != 0u) &&
        ((system_id & k_thread_id_mask) != 0u);
}

constexpr std::size_t make_system_id(const std::size_t module_id, const std::size_t thread_id) noexcept
{
    if (module_ids::is_valid_id(module_id) && thread_ids::is_valid_id(thread_id))
    {
        return module_id | thread_id;
    }
    return 0u;
}

constexpr std::size_t get_module_id(const std::size_t system_id) noexcept
{
    return is_valid_id(system_id) ? (system_id & k_module_id_mask) : std::size_t{ 0u };
}

constexpr std::size_t get_thread_id(const std::size_t system_id) noexcept
{
    return is_valid_id(system_id) ? (system_id & k_thread_id_mask) : std::size_t{ 0u };
}

//------------------------------------------------------------------------------

}   //  namespace system_ids

#endif  //  #ifndef SYSTEM_IDS_HPP_INCLUDED