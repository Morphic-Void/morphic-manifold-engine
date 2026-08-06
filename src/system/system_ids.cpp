
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   system_ids.cpp
//  Author: Ritchie Brannan
//  Date:   26 Jul 26
//
//  Built-in system id registration and validation tables.

#include <cstddef>      //  std::size_t
#include <cstring>      //  std::memcpy

#include "system/system_ids.hpp"

namespace system_id_registry
{

namespace
{

static const STypeRegistration s_type_registrations[] =
{
#define MV_SYSTEM_TYPE(name) { type_ids::name, type_ids::name##_index, #name },
#include "system/type_ids.def"
#undef MV_SYSTEM_TYPE
};

static const SMountPointRegistration s_mount_point_registrations[] =
{
#define MV_SYSTEM_MOUNT_POINT(name) { mount_point_ids::name, mount_point_ids::name##_index, #name },
#define MV_SYSTEM_MODULE(name, mount_point_name)
#include "system/runtime_ids.def"
#undef MV_SYSTEM_MODULE
#undef MV_SYSTEM_MOUNT_POINT
};

static const SThreadRegistration s_thread_registrations[] =
{
#define MV_SYSTEM_THREAD(name) { thread_ids::name, thread_ids::name##_index, #name },
#include "system/thread_ids.def"
#undef MV_SYSTEM_THREAD
};

static const SModuleRegistration s_module_registrations[] =
{
#define MV_SYSTEM_MOUNT_POINT(name)
#define MV_SYSTEM_MODULE(name, mount_point_name) { module_ids::name, module_ids::name##_index, mount_point_ids::mount_point_name, #name },
#include "system/runtime_ids.def"
#undef MV_SYSTEM_MODULE
#undef MV_SYSTEM_MOUNT_POINT
};

template<typename T>
constexpr std::size_t count_of(const T(&)[1]) noexcept
{
    return 1u;
}

template<typename T, std::size_t N>
constexpr std::size_t count_of(const T(&)[N]) noexcept
{
    return N;
}

}   //  namespace

const STypeRegistration* types() noexcept
{
    return s_type_registrations;
}

std::size_t type_count() noexcept
{
    return count_of(s_type_registrations);
}

const SMountPointRegistration* mount_points() noexcept
{
    return s_mount_point_registrations;
}

std::size_t mount_point_count() noexcept
{
    return count_of(s_mount_point_registrations);
}

const SThreadRegistration* threads() noexcept
{
    return s_thread_registrations;
}

std::size_t thread_count() noexcept
{
    return count_of(s_thread_registrations);
}

const SModuleRegistration* modules() noexcept
{
    return s_module_registrations;
}

std::size_t module_count() noexcept
{
    return count_of(s_module_registrations);
}

const STypeRegistration* find_type(const type_ids::id_type id) noexcept
{
    if (!type_ids::is_valid_id(id))
    {
        return nullptr;
    }

    const type_ids::index_type index = type_ids::decode_id(id);
    return (index < type_count()) ? &s_type_registrations[index] : nullptr;
}

const SMountPointRegistration* find_mount_point(
    const mount_point_ids::id_type id) noexcept
{
    if (!mount_point_ids::is_valid_id(id))
    {
        return nullptr;
    }

    const mount_point_ids::index_type index = mount_point_ids::decode_id(id);
    return (index.raw_value() < mount_point_count())
        ? &s_mount_point_registrations[index.raw_value()]
        : nullptr;
}

const SThreadRegistration* find_thread(const thread_ids::id_type id) noexcept
{
    if (!thread_ids::is_valid_id(id))
    {
        return nullptr;
    }

    const thread_ids::index_type index = thread_ids::decode_id(id);
    return (index.raw_value() < thread_count())
        ? &s_thread_registrations[index.raw_value()]
        : nullptr;
}

const SModuleRegistration* find_module(const module_ids::id_type id) noexcept
{
    if (!module_ids::is_valid_id(id))
    {
        return nullptr;
    }

    const module_ids::index_type index = module_ids::decode_id(id);
    if (index.raw_value() >= module_count())
    {
        return nullptr;
    }

    const SModuleRegistration& registration =
        s_module_registrations[index.raw_value()];
    return (registration.id == id) ? &registration : nullptr;
}

const char* lookup_type_name(const type_ids::id_type id) noexcept
{
    const STypeRegistration* const registration = find_type(id);
    return (registration != nullptr) ? registration->name : nullptr;
}

const char* lookup_mount_point_name(const mount_point_ids::id_type id) noexcept
{
    const SMountPointRegistration* const registration = find_mount_point(id);
    return (registration != nullptr) ? registration->name : nullptr;
}

const char* lookup_thread_name(const thread_ids::id_type id) noexcept
{
    const SThreadRegistration* const registration = find_thread(id);
    return (registration != nullptr) ? registration->name : nullptr;
}

const char* lookup_module_name(const module_ids::id_type id) noexcept
{
    const SModuleRegistration* const registration = find_module(id);
    return (registration != nullptr) ? registration->name : nullptr;
}

bool format_system_name(
    const system_ids::id_type id,
    char* const destination,
    const std::size_t destination_capacity,
    std::size_t& out_size) noexcept
{
    out_size = 0u;

    if ((destination == nullptr) || (destination_capacity == 0u))
    {
        return false;
    }

    destination[0] = 0;

    if (!system_ids::is_valid_id(id))
    {
        return false;
    }

    const char* const module_name =
        lookup_module_name(system_ids::get_module_id(id));
    const char* const thread_name =
        lookup_thread_name(system_ids::get_thread_id(id));
    if ((module_name == nullptr) || (thread_name == nullptr))
    {
        return false;
    }

    std::size_t module_size = 0u;
    while (module_name[module_size] != 0)
    {
        ++module_size;
    }

    std::size_t thread_size = 0u;
    while (thread_name[thread_size] != 0)
    {
        ++thread_size;
    }

    const std::size_t required_size = module_size + 1u + thread_size;
    if (required_size >= destination_capacity)
    {
        return false;
    }

    std::memcpy(destination, module_name, module_size);
    destination[module_size] = ':';
    std::memcpy(destination + module_size + 1u, thread_name, thread_size);
    destination[required_size] = 0;
    out_size = required_size;
    return true;
}

bool has_mount_point(const mount_point_ids::id_type id) noexcept
{
    return find_mount_point(id) != nullptr;
}

mount_point_ids::id_type lookup_mount_point_id(const module_ids::id_type id) noexcept
{
    const SModuleRegistration* const registration = find_module(id);
    return (registration != nullptr)
        ? registration->mount_point_id
        : mount_point_ids::id_type{ 0u };
}

bool validate_type_registrations() noexcept
{
    for (std::size_t i = 0u; i < type_count(); ++i)
    {
        const STypeRegistration& registration = s_type_registrations[i];
        if (!type_ids::is_valid_id(registration.id) ||
            !type_ids::is_valid_index(registration.index) ||
            (registration.index != i) ||
            (type_ids::encode_id(registration.index) != registration.id) ||
            (registration.name == nullptr) ||
            (registration.name[0] == 0))
        {
            return false;
        }
    }

    return true;
}

bool validate_mount_point_registrations() noexcept
{
    for (std::size_t i = 0u; i < mount_point_count(); ++i)
    {
        const SMountPointRegistration& outer = s_mount_point_registrations[i];
        if (!mount_point_ids::is_valid_id(outer.id) ||
            !mount_point_ids::is_valid_index(outer.index) ||
            (outer.index.raw_value() != i) ||
            (mount_point_ids::make_id(outer.index) != outer.id) ||
            (outer.name == nullptr) ||
            (outer.name[0] == 0))
        {
            return false;
        }
    }
    return true;
}

bool validate_thread_registrations() noexcept
{
    for (std::size_t i = 0u; i < thread_count(); ++i)
    {
        const SThreadRegistration& outer = s_thread_registrations[i];
        if (!thread_ids::is_valid_id(outer.id) ||
            !thread_ids::is_valid_index(outer.index) ||
            (outer.index.raw_value() != i) ||
            (thread_ids::make_id(outer.index) != outer.id) ||
            (outer.name == nullptr) ||
            (outer.name[0] == 0))
        {
            return false;
        }
    }
    return true;
}

bool validate_module_registrations() noexcept
{
    for (std::size_t i = 0u; i < module_count(); ++i)
    {
        const SModuleRegistration& outer = s_module_registrations[i];
        if (!module_ids::is_valid_id(outer.id) ||
            !module_ids::is_valid_index(outer.index) ||
            !mount_point_ids::is_valid_id(outer.mount_point_id) ||
            (outer.index.raw_value() != i) ||
            (module_ids::make_id(outer.mount_point_id, outer.index) != outer.id) ||
            (module_ids::get_mount_point_id(outer.id) != outer.mount_point_id) ||
            !has_mount_point(outer.mount_point_id) ||
            (outer.name == nullptr) ||
            (outer.name[0] == 0))
        {
            return false;
        }
    }
    return true;
}

bool validate_all() noexcept
{
    return validate_type_registrations()
        && validate_mount_point_registrations()
        && validate_thread_registrations()
        && validate_module_registrations();
}

}   //  namespace system_id_registry
