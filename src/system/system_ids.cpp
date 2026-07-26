
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   system_ids.cpp
//  Author: Ritchie Brannan
//  Date:   26 Jul 26
//
//  Built-in system id registration and validation tables.

#include <cstddef>      //  std::size_t

#include "system/system_ids.hpp"

namespace system_id_registry
{

namespace
{

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

bool has_mount_point(const mount_point_ids::id_type id) noexcept
{
    for (std::size_t i = 0u; i < mount_point_count(); ++i)
    {
        if (s_mount_point_registrations[i].id == id)
        {
            return true;
        }
    }
    return false;
}

mount_point_ids::id_type lookup_mount_point_id(const module_ids::id_type id) noexcept
{
    for (std::size_t i = 0u; i < module_count(); ++i)
    {
        if (s_module_registrations[i].id == id)
        {
            return s_module_registrations[i].mount_point_id;
        }
    }
    return mount_point_ids::id_type{};
}

bool validate_mount_point_registrations() noexcept
{
    for (std::size_t i = 0u; i < mount_point_count(); ++i)
    {
        const SMountPointRegistration& outer = s_mount_point_registrations[i];
        if (!mount_point_ids::is_valid_id(outer.id) ||
            !mount_point_ids::is_valid_index(outer.index) ||
            (mount_point_ids::make_id(outer.index) != outer.id))
        {
            return false;
        }

        for (std::size_t j = i + 1u; j < mount_point_count(); ++j)
        {
            const SMountPointRegistration& inner = s_mount_point_registrations[j];
            if ((outer.id == inner.id) || (outer.index == inner.index))
            {
                return false;
            }
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
            (thread_ids::make_id(outer.index) != outer.id))
        {
            return false;
        }

        for (std::size_t j = i + 1u; j < thread_count(); ++j)
        {
            const SThreadRegistration& inner = s_thread_registrations[j];
            if ((outer.id == inner.id) || (outer.index == inner.index))
            {
                return false;
            }
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
            (module_ids::make_id(outer.mount_point_id, outer.index) != outer.id) ||
            (module_ids::get_mount_point_id(outer.id) != outer.mount_point_id) ||
            !has_mount_point(outer.mount_point_id))
        {
            return false;
        }

        for (std::size_t j = i + 1u; j < module_count(); ++j)
        {
            const SModuleRegistration& inner = s_module_registrations[j];
            if ((outer.id == inner.id) || (outer.index == inner.index))
            {
                return false;
            }
        }
    }
    return true;
}

bool validate_all() noexcept
{
    return validate_mount_point_registrations()
        && validate_thread_registrations()
        && validate_module_registrations();
}

}   //  namespace system_id_registry
