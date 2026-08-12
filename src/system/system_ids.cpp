
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  Shared installed-view lookup for host-authored system identity names.

#include "system/system_id_registry.hpp"

#include <cstring>

namespace system_id_registry
{

namespace
{

SSystemRegistryView s_installed_view{};
bool s_view_installed{ false };

template<typename T>
[[nodiscard]] bool table_shape_is_valid(const T* const table, const std::uint32_t count) noexcept
{
    return (count == 0u) ? (table == nullptr) : (table != nullptr);
}

[[nodiscard]] bool name_is_valid(const char* const name, const std::uint32_t size) noexcept
{
    return (name != nullptr) && (size != 0u) && (size <= k_max_system_name_size) &&
        (name[size] == 0) && (std::memchr(name, 0, size) == nullptr);
}

}   //  namespace

bool validate_view(const SSystemRegistryView& view) noexcept
{
    if (!table_shape_is_valid(view.types, view.type_count) ||
        !table_shape_is_valid(view.mount_points, view.mount_point_count) ||
        !table_shape_is_valid(view.threads, view.thread_count) ||
        !table_shape_is_valid(view.modules, view.module_count) ||
        (view.type_count > type_ids::k_capacity))
    {
        return false;
    }

    for (std::uint32_t index = 0u; index < view.type_count; ++index)
    {
        const STypeRegistration& registration = view.types[index];
        if (!type_ids::is_valid_id(registration.id) ||
            !type_ids::is_valid_index(registration.index) ||
            (registration.index != type_ids::encode_index(index)) ||
            (type_ids::encode_id(registration.index) != registration.id) ||
            !name_is_valid(registration.name, registration.name_size))
        {
            return false;
        }
    }

    for (std::uint32_t index = 0u; index < view.mount_point_count; ++index)
    {
        const SMountPointRegistration& registration = view.mount_points[index];
        if (!mount_point_ids::is_valid_id(registration.id) ||
            !mount_point_ids::is_valid_index(registration.index) ||
            (registration.index.raw_value() != index) ||
            (mount_point_ids::make_id(registration.index) != registration.id) ||
            !name_is_valid(registration.name, registration.name_size))
        {
            return false;
        }
    }

    for (std::uint32_t index = 0u; index < view.thread_count; ++index)
    {
        const SThreadRegistration& registration = view.threads[index];
        if (!thread_ids::is_valid_id(registration.id) ||
            !thread_ids::is_valid_index(registration.index) ||
            (registration.index.raw_value() != index) ||
            (thread_ids::make_id(registration.index) != registration.id) ||
            !name_is_valid(registration.name, registration.name_size))
        {
            return false;
        }
    }

    for (std::uint32_t index = 0u; index < view.module_count; ++index)
    {
        const SModuleRegistration& registration = view.modules[index];
        if (!module_ids::is_valid_id(registration.id) ||
            !module_ids::is_valid_index(registration.index) ||
            !mount_point_ids::is_valid_id(registration.mount_point_id) ||
            (registration.index.raw_value() != index) ||
            (module_ids::make_id(registration.mount_point_id, registration.index) != registration.id) ||
            (module_ids::get_mount_point_id(registration.id) != registration.mount_point_id) ||
            (find_mount_point(&view, registration.mount_point_id) == nullptr) ||
            !name_is_valid(registration.name, registration.name_size))
        {
            return false;
        }
    }
    return true;
}

bool install_view(const SSystemRegistryView& view) noexcept
{
    if (s_view_installed || !validate_view(view))
    {
        return false;
    }
    s_installed_view = view;
    s_view_installed = true;
    return true;
}

bool view_is_installed() noexcept { return s_view_installed; }
const SSystemRegistryView* installed_view() noexcept { return s_view_installed ? &s_installed_view : nullptr; }

const STypeRegistration* types() noexcept { return s_view_installed ? s_installed_view.types : nullptr; }
std::uint32_t type_count() noexcept { return s_view_installed ? s_installed_view.type_count : 0u; }
const SMountPointRegistration* mount_points() noexcept { return s_view_installed ? s_installed_view.mount_points : nullptr; }
std::uint32_t mount_point_count() noexcept { return s_view_installed ? s_installed_view.mount_point_count : 0u; }
const SThreadRegistration* threads() noexcept { return s_view_installed ? s_installed_view.threads : nullptr; }
std::uint32_t thread_count() noexcept { return s_view_installed ? s_installed_view.thread_count : 0u; }
const SModuleRegistration* modules() noexcept { return s_view_installed ? s_installed_view.modules : nullptr; }
std::uint32_t module_count() noexcept { return s_view_installed ? s_installed_view.module_count : 0u; }

const STypeRegistration* find_type(const SSystemRegistryView* const view, const type_ids::id_type id) noexcept
{
    if ((view == nullptr) || !type_ids::is_valid_id(id) ||
        (view->type_count > type_ids::k_capacity) ||
        !table_shape_is_valid(view->types, view->type_count))
    {
        return nullptr;
    }
    const type_ids::index_type index = type_ids::decode_id(id);
    const type_ids::index_type ordinal = type_ids::decode_index(index);
    if (ordinal >= view->type_count)
    {
        return nullptr;
    }
    const STypeRegistration& registration = view->types[ordinal];
    return (registration.id == id) && (registration.index == index) &&
        name_is_valid(registration.name, registration.name_size) ? &registration : nullptr;
}

const SMountPointRegistration* find_mount_point(const SSystemRegistryView* const view, const mount_point_ids::id_type id) noexcept
{
    if ((view == nullptr) || !mount_point_ids::is_valid_id(id) ||
        !table_shape_is_valid(view->mount_points, view->mount_point_count))
    {
        return nullptr;
    }
    const mount_point_ids::index_type index = mount_point_ids::decode_id(id);
    if (index.raw_value() >= view->mount_point_count)
    {
        return nullptr;
    }
    const SMountPointRegistration& registration = view->mount_points[index.raw_value()];
    return (registration.id == id) && (registration.index == index) &&
        name_is_valid(registration.name, registration.name_size) ? &registration : nullptr;
}

const SThreadRegistration* find_thread(const SSystemRegistryView* const view, const thread_ids::id_type id) noexcept
{
    if ((view == nullptr) || !thread_ids::is_valid_id(id) ||
        !table_shape_is_valid(view->threads, view->thread_count))
    {
        return nullptr;
    }
    const thread_ids::index_type index = thread_ids::decode_id(id);
    if (index.raw_value() >= view->thread_count)
    {
        return nullptr;
    }
    const SThreadRegistration& registration = view->threads[index.raw_value()];
    return (registration.id == id) && (registration.index == index) &&
        name_is_valid(registration.name, registration.name_size) ? &registration : nullptr;
}

const SModuleRegistration* find_module(const SSystemRegistryView* const view, const module_ids::id_type id) noexcept
{
    if ((view == nullptr) || !module_ids::is_valid_id(id) ||
        !table_shape_is_valid(view->modules, view->module_count))
    {
        return nullptr;
    }
    const module_ids::index_type index = module_ids::decode_id(id);
    if (index.raw_value() >= view->module_count)
    {
        return nullptr;
    }
    const SModuleRegistration& registration = view->modules[index.raw_value()];
    return (registration.id == id) && (registration.index == index) &&
        mount_point_ids::is_valid_id(registration.mount_point_id) &&
        (module_ids::get_mount_point_id(registration.id) == registration.mount_point_id) &&
        name_is_valid(registration.name, registration.name_size) ? &registration : nullptr;
}

const STypeRegistration* find_type(const type_ids::id_type id) noexcept { return find_type(installed_view(), id); }
const SMountPointRegistration* find_mount_point(const mount_point_ids::id_type id) noexcept { return find_mount_point(installed_view(), id); }
const SThreadRegistration* find_thread(const thread_ids::id_type id) noexcept { return find_thread(installed_view(), id); }
const SModuleRegistration* find_module(const module_ids::id_type id) noexcept { return find_module(installed_view(), id); }

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
    const system_ids::id_type id, char* const destination,
    const std::size_t destination_capacity, std::size_t& out_size) noexcept
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

    const SModuleRegistration* const module = find_module(system_ids::get_module_id(id));
    const SThreadRegistration* const thread = find_thread(system_ids::get_thread_id(id));
    if ((module == nullptr) || (thread == nullptr))
    {
        return false;
    }

    const std::size_t required_size =
        static_cast<std::size_t>(module->name_size) + 1u + thread->name_size;
    if (required_size >= destination_capacity)
    {
        return false;
    }
    std::memcpy(destination, module->name, module->name_size);
    destination[module->name_size] = ':';
    std::memcpy((destination + module->name_size + 1u), thread->name, thread->name_size);
    destination[required_size] = 0;
    out_size = required_size;
    return true;
}

bool has_mount_point(const mount_point_ids::id_type id) noexcept { return find_mount_point(id) != nullptr; }

mount_point_ids::id_type lookup_mount_point_id(const module_ids::id_type id) noexcept
{
    const SModuleRegistration* const registration = find_module(id);
    return (registration != nullptr) ? registration->mount_point_id : mount_point_ids::id_type{};
}

bool validate_type_registrations() noexcept
{
    const SSystemRegistryView* const view = installed_view();
    if (view == nullptr) return false;
    for (std::uint32_t i = 0u; i < view->type_count; ++i)
        if (find_type(view, view->types[i].id) != &view->types[i]) return false;
    return true;
}

bool validate_mount_point_registrations() noexcept
{
    const SSystemRegistryView* const view = installed_view();
    if (view == nullptr) return false;
    for (std::uint32_t i = 0u; i < view->mount_point_count; ++i)
        if (find_mount_point(view, view->mount_points[i].id) != &view->mount_points[i]) return false;
    return true;
}

bool validate_thread_registrations() noexcept
{
    const SSystemRegistryView* const view = installed_view();
    if (view == nullptr) return false;
    for (std::uint32_t i = 0u; i < view->thread_count; ++i)
        if (find_thread(view, view->threads[i].id) != &view->threads[i]) return false;
    return true;
}

bool validate_module_registrations() noexcept
{
    const SSystemRegistryView* const view = installed_view();
    if (view == nullptr) return false;
    for (std::uint32_t i = 0u; i < view->module_count; ++i)
        if (find_module(view, view->modules[i].id) != &view->modules[i]) return false;
    return true;
}

bool validate_all() noexcept
{
    const SSystemRegistryView* const view = installed_view();
    return (view != nullptr) && validate_view(*view);
}

}   //  namespace system_id_registry
