
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  Installed immutable host-owned authority for system identity names.

#pragma once

#ifndef SYSTEM_ID_REGISTRY_HPP_INCLUDED
#define SYSTEM_ID_REGISTRY_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "system/system_ids.hpp"

namespace system_id_registry
{

inline constexpr std::uint32_t k_max_system_name_size = 4096u;

struct STypeRegistration
{
    type_ids::id_type id{ type_ids::undefined };
    type_ids::index_type index{ type_ids::k_invalid_index };
    const char* name{ nullptr };
    std::uint32_t name_size{ 0u };
};

struct SMountPointRegistration
{
    mount_point_ids::id_type id{};
    mount_point_ids::index_type index{};
    const char* name{ nullptr };
    std::uint32_t name_size{ 0u };
};

struct SThreadRegistration
{
    thread_ids::id_type id{};
    thread_ids::index_type index{};
    const char* name{ nullptr };
    std::uint32_t name_size{ 0u };
};

struct SModuleRegistration
{
    module_ids::id_type id{};
    module_ids::index_type index{};
    mount_point_ids::id_type mount_point_id{};
    const char* name{ nullptr };
    std::uint32_t name_size{ 0u };
};

struct SSystemRegistryView
{
    const STypeRegistration* types{ nullptr };
    std::uint32_t type_count{ 0u };
    const SMountPointRegistration* mount_points{ nullptr };
    std::uint32_t mount_point_count{ 0u };
    const SThreadRegistration* threads{ nullptr };
    std::uint32_t thread_count{ 0u };
    const SModuleRegistration* modules{ nullptr };
    std::uint32_t module_count{ 0u };
};

static_assert(std::is_standard_layout_v<SSystemRegistryView> && std::is_trivially_copyable_v<SSystemRegistryView>);
static_assert(sizeof(SSystemRegistryView) == (sizeof(void*) * 8u));

[[nodiscard]] bool validate_view(const SSystemRegistryView& view) noexcept;
[[nodiscard]] bool install_view(const SSystemRegistryView& view) noexcept;
[[nodiscard]] bool view_is_installed() noexcept;
[[nodiscard]] const SSystemRegistryView* installed_view() noexcept;

[[nodiscard]] const STypeRegistration* types() noexcept;
[[nodiscard]] std::uint32_t type_count() noexcept;
[[nodiscard]] const SMountPointRegistration* mount_points() noexcept;
[[nodiscard]] std::uint32_t mount_point_count() noexcept;
[[nodiscard]] const SThreadRegistration* threads() noexcept;
[[nodiscard]] std::uint32_t thread_count() noexcept;
[[nodiscard]] const SModuleRegistration* modules() noexcept;
[[nodiscard]] std::uint32_t module_count() noexcept;

[[nodiscard]] const STypeRegistration* find_type(const SSystemRegistryView* const view, const type_ids::id_type id) noexcept;
[[nodiscard]] const SMountPointRegistration* find_mount_point(const SSystemRegistryView* const view, const mount_point_ids::id_type id) noexcept;
[[nodiscard]] const SThreadRegistration* find_thread(const SSystemRegistryView* const view, const thread_ids::id_type id) noexcept;
[[nodiscard]] const SModuleRegistration* find_module(const SSystemRegistryView* const view, const module_ids::id_type id) noexcept;

[[nodiscard]] const STypeRegistration* find_type(const type_ids::id_type id) noexcept;
[[nodiscard]] const SMountPointRegistration* find_mount_point(const mount_point_ids::id_type id) noexcept;
[[nodiscard]] const SThreadRegistration* find_thread(const thread_ids::id_type id) noexcept;
[[nodiscard]] const SModuleRegistration* find_module(const module_ids::id_type id) noexcept;

[[nodiscard]] const char* lookup_type_name(const type_ids::id_type id) noexcept;
[[nodiscard]] const char* lookup_mount_point_name(const mount_point_ids::id_type id) noexcept;
[[nodiscard]] const char* lookup_thread_name(const thread_ids::id_type id) noexcept;
[[nodiscard]] const char* lookup_module_name(const module_ids::id_type id) noexcept;

[[nodiscard]] bool format_system_name(
    const system_ids::id_type id,
    char* const destination,
    const std::size_t destination_capacity,
    std::size_t& out_size) noexcept;

[[nodiscard]] bool has_mount_point(const mount_point_ids::id_type id) noexcept;
[[nodiscard]] mount_point_ids::id_type lookup_mount_point_id(const module_ids::id_type id) noexcept;
[[nodiscard]] bool validate_type_registrations() noexcept;
[[nodiscard]] bool validate_mount_point_registrations() noexcept;
[[nodiscard]] bool validate_thread_registrations() noexcept;
[[nodiscard]] bool validate_module_registrations() noexcept;
[[nodiscard]] bool validate_all() noexcept;

}   //  namespace system_id_registry

#endif  //  SYSTEM_ID_REGISTRY_HPP_INCLUDED
