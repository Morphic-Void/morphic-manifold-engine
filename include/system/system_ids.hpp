
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   system_ids.hpp
//  Author: Ritchie Brannan
//  Date:   26 Jul 26
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//
//  Fixed encoded system id spaces for closed-system use across DLLs.
//
//  Defines:
//  - 32-bit encoded type ids;
//  - 64-bit encoded runtime ids for mounting points, modules, threads,
//    and combined systems;
//  - explicit strong index and id wrappers for those runtime identities;
//  - built-in registration and validation access for those runtime ids.
//
//  Common helper surface by domain:
//
//  - type_ids:
//      is_valid_index(), encode_index(), encode_id(), decode_id(), is_valid_id()
//  - mount_point_ids:
//      make_index(), make_id(), decode_id(), is_valid_index(), is_valid_id()
//  - thread_ids:
//      make_index(), make_id(), decode_id(), is_valid_index(), is_valid_id()
//  - module_ids:
//      make_index(), make_id(mount_point_id, module_index), decode_id(),
//      get_mount_point_id(), get_mount_point_index(),
//      is_valid_index(), is_valid_id()
//  - system_ids:
//      make_system_id(module_id, thread_id), get_module_id(), get_thread_id(),
//      get_mount_point_id(), get_mount_point_index(), is_valid_id()
//
//  Built-in registration access is provided through system_id_registry:
//
//  - mount_points(), mount_point_count()
//  - threads(), thread_count()
//  - modules(), module_count()
//  - has_mount_point(), lookup_mount_point_id()
//  - validate_mount_point_registrations(), validate_thread_registrations(),
//    validate_module_registrations(), validate_all()
//
//  This file defines ids and id-handling helpers only. It does not
//  bind ids to C++ payload types.

#pragma once

#ifndef SYSTEM_IDS_HPP_INCLUDED
#define SYSTEM_IDS_HPP_INCLUDED

#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::int32_t, std::uint32_t, std::uint64_t
#include <limits>       //  std::numeric_limits

#include "bit_utils/bit_ops.hpp"

//==============================================================================
//  Helper utility for id handling
//==============================================================================

namespace system_id_util
{

template<typename Tag, typename Repr>
class TValue
{
public:
    using repr_type = Repr;

    constexpr TValue() noexcept = default;
    explicit constexpr TValue(const Repr value) noexcept : m_value(value) {}

    [[nodiscard]] constexpr Repr raw_value() const noexcept { return m_value; }
    [[nodiscard]] constexpr bool is_valid() const noexcept { return m_value != Repr{ 0 }; }
    explicit constexpr operator bool() const noexcept { return is_valid(); }
    constexpr operator Repr() const noexcept { return m_value; }

private:
    Repr m_value{ 0 };
};

template<typename Tag, typename Repr>
class TIndexValue
{
public:
    using repr_type = Repr;
    static constexpr Repr k_invalid_value = std::numeric_limits<Repr>::max();

    constexpr TIndexValue() noexcept = default;
    explicit constexpr TIndexValue(const Repr value) noexcept : m_value(value) {}

    [[nodiscard]] constexpr Repr raw_value() const noexcept { return m_value; }
    [[nodiscard]] constexpr bool is_valid() const noexcept { return m_value != k_invalid_value; }
    explicit constexpr operator bool() const noexcept { return is_valid(); }

private:
    Repr m_value{ k_invalid_value };
};

template<typename Tag, typename Repr>
constexpr bool operator==(const TValue<Tag, Repr> lhs, const TValue<Tag, Repr> rhs) noexcept
{
    return lhs.raw_value() == rhs.raw_value();
}

template<typename Tag, typename Repr>
constexpr bool operator!=(const TValue<Tag, Repr> lhs, const TValue<Tag, Repr> rhs) noexcept
{
    return !(lhs == rhs);
}

template<typename Tag, typename Repr>
constexpr bool operator==(const TIndexValue<Tag, Repr> lhs, const TIndexValue<Tag, Repr> rhs) noexcept
{
    return lhs.raw_value() == rhs.raw_value();
}

template<typename Tag, typename Repr>
constexpr bool operator!=(const TIndexValue<Tag, Repr> lhs, const TIndexValue<Tag, Repr> rhs) noexcept
{
    return !(lhs == rhs);
}

template<typename IdTag, typename IndexTag, typename Repr, Repr t_encoded_field_mask>
struct TEncodedField
{
    using id_type = TValue<IdTag, Repr>;
    using index_type = TIndexValue<IndexTag, Repr>;

    static constexpr Repr k_id_field_mask = t_encoded_field_mask;
    static constexpr Repr k_payload_mask = (Repr{ 1 } << bit_ops::count_set_bits(k_id_field_mask)) - 1u;
    static constexpr Repr k_invalid_id_mask = ~k_id_field_mask;
    static constexpr std::int32_t k_id_field_shift = bit_ops::lo_bit_index(k_id_field_mask);

    static constexpr index_type make_index(const Repr value) noexcept
    {
        return (value < k_payload_mask) ? index_type(value) : index_type{};
    }

    static constexpr bool is_valid_index(const index_type index) noexcept
    {
        return index.is_valid() && (index.raw_value() < k_payload_mask);
    }

    static constexpr bool is_valid_id(const id_type id) noexcept
    {
        return id.is_valid() && ((id.raw_value() & k_invalid_id_mask) == 0u);
    }

    static constexpr id_type make_id(const index_type index) noexcept
    {
        return is_valid_index(index)
            ? id_type(static_cast<Repr>(
                bit_ops::spread_to_even_bits(((index.raw_value() + 1u) & k_payload_mask) ^ k_payload_mask) << k_id_field_shift))
            : id_type{};
    }

    static constexpr index_type get_index(const id_type id) noexcept
    {
        return is_valid_id(id)
            ? index_type(static_cast<Repr>(
                (((bit_ops::pack_from_even_bits(id.raw_value() >> k_id_field_shift) ^ k_payload_mask) & k_payload_mask) - 1u)))
            : index_type{};
    }
};

template<std::uint64_t t_payload_mask, std::uint32_t t_payload_offset>
struct TEvenMask
{
    static constexpr std::uint64_t value =
        (bit_ops::spread_to_even_bits(t_payload_mask) << (t_payload_offset * 2u));
};

struct CTypeIdTag {};
struct CMountPointIdTag {};
struct CMountPointIndexTag {};
struct CModuleIdTag {};
struct CModuleIndexTag {};
struct CThreadIdTag {};
struct CThreadIndexTag {};
struct CSystemIdTag {};

}   //  namespace system_id_util

//==============================================================================
//  ID field definitions
//==============================================================================

namespace type_ids
{
using id_type = system_id_util::TValue<system_id_util::CTypeIdTag, std::uint32_t>;
using index_type = std::uint32_t;
static constexpr id_type undefined{};
static constexpr std::uint32_t k_id_field_mask = 0x55555555u;      //  16 payload bits
static constexpr std::uint32_t k_payload_mask = 0x0000ffffu;
static constexpr std::uint32_t k_invalid_id_mask = ~k_id_field_mask;
static constexpr std::int32_t k_id_field_shift = 0;
static constexpr index_type k_invalid_index = k_payload_mask;
}

namespace runtime_id_layout
{
static constexpr std::uint32_t k_thread_payload_bits = 16u;
static constexpr std::uint32_t k_mount_point_payload_bits = 6u;
static constexpr std::uint32_t k_module_payload_bits = 10u;

static constexpr std::uint32_t k_thread_payload_offset = 0u;
static constexpr std::uint32_t k_mount_point_payload_offset = k_thread_payload_offset + k_thread_payload_bits;
static constexpr std::uint32_t k_module_payload_offset = k_mount_point_payload_offset + k_mount_point_payload_bits;

static constexpr std::uint64_t k_thread_field_mask =
    system_id_util::TEvenMask<((std::uint64_t{ 1 } << k_thread_payload_bits) - 1u), k_thread_payload_offset>::value;
static constexpr std::uint64_t k_mount_point_field_mask =
    system_id_util::TEvenMask<((std::uint64_t{ 1 } << k_mount_point_payload_bits) - 1u), k_mount_point_payload_offset>::value;
static constexpr std::uint64_t k_module_field_mask =
    system_id_util::TEvenMask<((std::uint64_t{ 1 } << k_module_payload_bits) - 1u), k_module_payload_offset>::value;
}

namespace mount_point_ids
{
using field = system_id_util::TEncodedField<
    system_id_util::CMountPointIdTag,
    system_id_util::CMountPointIndexTag,
    std::uint64_t,
    runtime_id_layout::k_mount_point_field_mask>;
using id_type = field::id_type;
using index_type = field::index_type;
}

namespace thread_ids
{
using field = system_id_util::TEncodedField<
    system_id_util::CThreadIdTag,
    system_id_util::CThreadIndexTag,
    std::uint64_t,
    runtime_id_layout::k_thread_field_mask>;
using id_type = field::id_type;
using index_type = field::index_type;
}

namespace module_ids
{
using id_type = system_id_util::TValue<system_id_util::CModuleIdTag, std::uint64_t>;
using index_type = system_id_util::TIndexValue<system_id_util::CModuleIndexTag, std::uint64_t>;

static constexpr std::uint64_t k_mount_point_id_mask = runtime_id_layout::k_mount_point_field_mask;
static constexpr std::uint64_t k_module_id_mask = runtime_id_layout::k_module_field_mask;
static constexpr std::uint64_t k_id_field_mask = k_mount_point_id_mask | k_module_id_mask;
static constexpr std::uint64_t k_invalid_id_mask = ~k_id_field_mask;
static constexpr std::uint64_t k_payload_mask = (std::uint64_t{ 1 } << runtime_id_layout::k_module_payload_bits) - 1u;
static constexpr std::int32_t k_id_field_shift = bit_ops::lo_bit_index(k_module_id_mask);
}

namespace system_ids
{
using id_type = system_id_util::TValue<system_id_util::CSystemIdTag, std::uint64_t>;

static constexpr std::uint64_t k_module_id_mask = module_ids::k_id_field_mask;
static constexpr std::uint64_t k_thread_id_mask = runtime_id_layout::k_thread_field_mask;
static constexpr std::uint64_t k_invalid_id_mask = ~(k_module_id_mask | k_thread_id_mask);
}

//==============================================================================
//  Built-in registration and validation access
//==============================================================================

namespace system_id_registry
{

struct STypeRegistration
{
    type_ids::id_type id{ type_ids::undefined };
    type_ids::index_type index{ type_ids::k_invalid_index };
    const char* name{ nullptr };
};

struct SMountPointRegistration
{
    mount_point_ids::id_type id{};
    mount_point_ids::index_type index{};
    const char* name{ nullptr };
};

struct SThreadRegistration
{
    thread_ids::id_type id{};
    thread_ids::index_type index{};
    const char* name{ nullptr };
};

struct SModuleRegistration
{
    module_ids::id_type id{};
    module_ids::index_type index{};
    mount_point_ids::id_type mount_point_id{};
    const char* name{ nullptr };
};

[[nodiscard]] const STypeRegistration* types() noexcept;
[[nodiscard]] std::size_t type_count() noexcept;
[[nodiscard]] const SMountPointRegistration* mount_points() noexcept;
[[nodiscard]] std::size_t mount_point_count() noexcept;
[[nodiscard]] const SThreadRegistration* threads() noexcept;
[[nodiscard]] std::size_t thread_count() noexcept;
[[nodiscard]] const SModuleRegistration* modules() noexcept;
[[nodiscard]] std::size_t module_count() noexcept;

[[nodiscard]] const STypeRegistration* find_type(type_ids::id_type id) noexcept;
[[nodiscard]] const SMountPointRegistration* find_mount_point(mount_point_ids::id_type id) noexcept;
[[nodiscard]] const SThreadRegistration* find_thread(thread_ids::id_type id) noexcept;
[[nodiscard]] const SModuleRegistration* find_module(module_ids::id_type id) noexcept;

[[nodiscard]] const char* lookup_type_name(type_ids::id_type id) noexcept;
[[nodiscard]] const char* lookup_mount_point_name(mount_point_ids::id_type id) noexcept;
[[nodiscard]] const char* lookup_thread_name(thread_ids::id_type id) noexcept;
[[nodiscard]] const char* lookup_module_name(module_ids::id_type id) noexcept;
[[nodiscard]] bool format_system_name(
    system_ids::id_type id,
    char* destination,
    std::size_t destination_capacity,
    std::size_t& out_size) noexcept;

[[nodiscard]] bool has_mount_point(mount_point_ids::id_type id) noexcept;
[[nodiscard]] mount_point_ids::id_type lookup_mount_point_id(module_ids::id_type id) noexcept;
[[nodiscard]] bool validate_type_registrations() noexcept;
[[nodiscard]] bool validate_mount_point_registrations() noexcept;
[[nodiscard]] bool validate_thread_registrations() noexcept;
[[nodiscard]] bool validate_module_registrations() noexcept;
[[nodiscard]] bool validate_all() noexcept;

}   //  namespace system_id_registry

//==============================================================================
//  Type id helpers
//==============================================================================

namespace type_ids
{

//  Type-id states:
//  - undefined is the canonical zero value and has no type attribution;
//  - a valid id has a structurally valid non-zero encoding;
//  - a registered id additionally resolves through system_id_registry::find_type().
constexpr bool is_defined(const id_type id) noexcept
{
    return id != undefined;
}

constexpr bool is_valid_index(const index_type value) noexcept
{
    return value < k_payload_mask;
}

constexpr bool is_valid_id(const id_type id) noexcept
{
    return is_defined(id) && ((id & k_invalid_id_mask) == 0u);
}

constexpr index_type encode_index(const index_type value) noexcept
{
    return is_valid_index(value) ? value : k_invalid_index;
}

constexpr id_type encode_id(const index_type value) noexcept
{
    return is_valid_index(value)
        ? static_cast<id_type>(bit_ops::spread_to_even_bits(((value + 1u) & k_payload_mask) ^ k_payload_mask) << k_id_field_shift)
        : undefined;
}

constexpr index_type decode_id(const id_type id) noexcept
{
    return is_valid_id(id)
        ? static_cast<index_type>(((bit_ops::pack_from_even_bits(id >> k_id_field_shift) ^ k_payload_mask) & k_payload_mask) - 1u)
        : k_invalid_index;
}

}   //  namespace type_ids

//==============================================================================
//  Mount point id helpers
//==============================================================================

namespace mount_point_ids
{

constexpr index_type make_index(const std::uint64_t value) noexcept { return field::make_index(value); }
constexpr bool is_valid_index(const index_type index) noexcept { return field::is_valid_index(index); }
constexpr bool is_valid_id(const id_type id) noexcept { return field::is_valid_id(id); }
constexpr id_type make_id(const index_type index) noexcept { return field::make_id(index); }
constexpr index_type decode_id(const id_type id) noexcept { return field::get_index(id); }

}   //  namespace mount_point_ids

//==============================================================================
//  Thread id helpers
//==============================================================================

namespace thread_ids
{

constexpr index_type make_index(const std::uint64_t value) noexcept { return field::make_index(value); }
constexpr bool is_valid_index(const index_type index) noexcept { return field::is_valid_index(index); }
constexpr bool is_valid_id(const id_type id) noexcept { return field::is_valid_id(id); }
constexpr id_type make_id(const index_type index) noexcept { return field::make_id(index); }
constexpr index_type decode_id(const id_type id) noexcept { return field::get_index(id); }

}   //  namespace thread_ids

//==============================================================================
//  Module id helpers
//==============================================================================

namespace module_ids
{

constexpr index_type make_index(const std::uint64_t value) noexcept
{
    return (value < k_payload_mask) ? index_type(value) : index_type{};
}

constexpr bool is_valid_index(const index_type index) noexcept
{
    return index.is_valid() && (index.raw_value() < k_payload_mask);
}

constexpr bool is_valid_id(const id_type id) noexcept
{
    return id.is_valid() &&
        ((id.raw_value() & k_invalid_id_mask) == 0u) &&
        ((id.raw_value() & k_mount_point_id_mask) != 0u) &&
        ((id.raw_value() & k_module_id_mask) != 0u);
}

constexpr id_type make_id(const mount_point_ids::id_type mount_point_id, const index_type module_index) noexcept
{
    return (mount_point_ids::is_valid_id(mount_point_id) && is_valid_index(module_index))
        ? id_type(mount_point_id.raw_value()
            | (bit_ops::spread_to_even_bits(((module_index.raw_value() + 1u) & k_payload_mask) ^ k_payload_mask) << k_id_field_shift))
        : id_type{};
}

constexpr index_type decode_id(const id_type id) noexcept
{
    return is_valid_id(id)
        ? index_type(static_cast<std::uint64_t>(
            (((bit_ops::pack_from_even_bits(id.raw_value() >> k_id_field_shift) ^ k_payload_mask) & k_payload_mask) - 1u)))
        : index_type{};
}

constexpr mount_point_ids::id_type get_mount_point_id(const id_type id) noexcept
{
    return is_valid_id(id)
        ? mount_point_ids::id_type(id.raw_value() & k_mount_point_id_mask)
        : mount_point_ids::id_type{};
}

constexpr mount_point_ids::index_type get_mount_point_index(const id_type id) noexcept
{
    return mount_point_ids::decode_id(get_mount_point_id(id));
}

}   //  namespace module_ids

//==============================================================================
//  Combined system ids (module + thread) helpers
//==============================================================================

namespace system_ids
{

constexpr bool is_valid_id(const id_type system_id) noexcept
{
    return system_id.is_valid() &&
        ((system_id.raw_value() & k_invalid_id_mask) == 0u) &&
        ((system_id.raw_value() & k_module_id_mask) != 0u) &&
        ((system_id.raw_value() & k_thread_id_mask) != 0u);
}

constexpr id_type make_system_id(const module_ids::id_type module_id, const thread_ids::id_type thread_id) noexcept
{
    return (module_ids::is_valid_id(module_id) && thread_ids::is_valid_id(thread_id))
        ? id_type(module_id.raw_value() | thread_id.raw_value())
        : id_type{};
}

constexpr module_ids::id_type get_module_id(const id_type system_id) noexcept
{
    return is_valid_id(system_id)
        ? module_ids::id_type(system_id.raw_value() & k_module_id_mask)
        : module_ids::id_type{};
}

constexpr thread_ids::id_type get_thread_id(const id_type system_id) noexcept
{
    return is_valid_id(system_id)
        ? thread_ids::id_type(system_id.raw_value() & k_thread_id_mask)
        : thread_ids::id_type{};
}

constexpr mount_point_ids::id_type get_mount_point_id(const id_type system_id) noexcept
{
    return module_ids::get_mount_point_id(get_module_id(system_id));
}

constexpr mount_point_ids::index_type get_mount_point_index(const id_type system_id) noexcept
{
    return module_ids::get_mount_point_index(get_module_id(system_id));
}

}   //  namespace system_ids

//==============================================================================
//  Type ids
//==============================================================================

namespace type_ids
{

#define MV_SYSTEM_TYPE(name) name##_index_value,
enum : index_type
{
#include "system/type_ids.def"
    k_count
};
#undef MV_SYSTEM_TYPE

#define MV_SYSTEM_TYPE(name) \
constexpr index_type name##_index = encode_index(name##_index_value); \
constexpr id_type name = encode_id(name##_index);
#include "system/type_ids.def"
#undef MV_SYSTEM_TYPE

}   //  namespace type_ids

//==============================================================================
//  Mount point ids
//==============================================================================

namespace mount_point_ids
{

#define MV_SYSTEM_MOUNT_POINT(name) name##_index_value,
#define MV_SYSTEM_MODULE(name, mount_point_name)
enum : index_type::repr_type
{
#include "system/runtime_ids.def"
    k_count
};
#undef MV_SYSTEM_MODULE
#undef MV_SYSTEM_MOUNT_POINT

#define MV_SYSTEM_MOUNT_POINT(name) \
constexpr index_type name##_index = make_index(name##_index_value); \
constexpr id_type name = make_id(name##_index);
#define MV_SYSTEM_MODULE(name, mount_point_name)
#include "system/runtime_ids.def"
#undef MV_SYSTEM_MODULE
#undef MV_SYSTEM_MOUNT_POINT

}   //  namespace mount_point_ids

//==============================================================================
//  Module ids
//==============================================================================

namespace module_ids
{

#define MV_SYSTEM_MOUNT_POINT(name)
#define MV_SYSTEM_MODULE(name, mount_point_name) name##_index_value,
enum : index_type::repr_type
{
#include "system/runtime_ids.def"
    k_count
};
#undef MV_SYSTEM_MODULE
#undef MV_SYSTEM_MOUNT_POINT

#define MV_SYSTEM_MOUNT_POINT(name)
#define MV_SYSTEM_MODULE(name, mount_point_name) \
constexpr index_type name##_index = make_index(name##_index_value); \
constexpr id_type name = make_id(mount_point_ids::mount_point_name, name##_index);
#include "system/runtime_ids.def"
#undef MV_SYSTEM_MODULE
#undef MV_SYSTEM_MOUNT_POINT

}   //  namespace module_ids

//==============================================================================
//  Thread ids
//==============================================================================

namespace thread_ids
{

#define MV_SYSTEM_THREAD(name) name##_index_value,
enum : index_type::repr_type
{
#include "system/thread_ids.def"
    k_count
};
#undef MV_SYSTEM_THREAD

#define MV_SYSTEM_THREAD(name) \
constexpr index_type name##_index = make_index(name##_index_value); \
constexpr id_type name = make_id(name##_index);
#include "system/thread_ids.def"
#undef MV_SYSTEM_THREAD

}   //  namespace thread_ids

//==============================================================================
//  System ids
//==============================================================================

namespace system_ids
{

constexpr id_type host = make_system_id(module_ids::executable, thread_ids::host);
constexpr id_type bg_file_io = make_system_id(module_ids::executable, thread_ids::bg_file_io);
constexpr id_type bg_conditioning = make_system_id(module_ids::executable, thread_ids::bg_conditioning);
constexpr id_type application = make_system_id(module_ids::application, thread_ids::application);

}   //  namespace system_ids

#endif  //  #ifndef SYSTEM_IDS_HPP_INCLUDED
