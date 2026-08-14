
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:    erased_owner_operations.cpp
//  Authors: Ritchie Brannan / OpenAI Codex
//  Date:    14 Aug 26
//
//  Installed component-local CErasedOwner operation authority and the
//  canonical SYSTEM operation table instantiated in this binary component.

#include <array>

#include "system/erased_owner.hpp"
#include "system/transported_types.hpp"

namespace erased_owner_operations
{

namespace
{

constexpr std::array<SRegistration, system_type_ids::k_count>
make_system_operations() noexcept
{
    std::array<SRegistration, system_type_ids::k_count> result{};

#define MV_ERASED_OWNER_PAYLOAD(type) \
    result[system_type_ids::decode_index( \
        system_type_ids::decode_id(k_system_type_id_v<type>))] = \
        SRegistration{ k_type_id_v<type>, \
            TDefaultOperationsFactory<type>::make() };
#define MV_ERASED_OWNER_PAYLOAD_WITH_STORAGE(type, member) \
    result[system_type_ids::decode_index( \
        system_type_ids::decode_id(k_system_type_id_v<type>))] = \
        SRegistration{ k_type_id_v<type>, \
            TNestedOperationsFactory<type, &type::member>::make() };
#include "system/system_erased_owner_payloads.def"
#undef MV_ERASED_OWNER_PAYLOAD_WITH_STORAGE
#undef MV_ERASED_OWNER_PAYLOAD

    return result;
}

constexpr auto s_system_operations = make_system_operations();
constexpr SCategoryView s_system_view{ s_system_operations.data(), static_cast<std::uint32_t>(s_system_operations.size()) };

SRegistryView s_installed_view{};
bool s_view_installed{ false };

[[nodiscard]] bool validate_category_view(const SCategoryView& view, const ETypeIdCategory category) noexcept
{
    if ((view.count > system_type_ids::k_capacity) ||
        ((view.count == 0u) != (view.registrations == nullptr)))
    {
        return false;
    }

    for (std::uint32_t index = 0u; index < view.count; ++index)
    {
        const SRegistration& registration = view.registrations[index];
        if (!registration.identity.is_valid())
        {
            if (!registration.operations.is_empty())
            {
                return false;
            }
            continue;
        }

        if ((registration.identity.category() != category) ||
            !registration.operations.is_complete())
        {
            return false;
        }

        if (category == ETypeIdCategory::system)
        {
            system_type_id identity;
            if (!registration.identity.try_system_type_id(identity) ||
                (system_type_ids::decode_index(system_type_ids::decode_id(identity)) != index))
            {
                return false;
            }
        }
        else
        {
            local_type_id identity;
            if (!registration.identity.try_local_type_id(identity) ||
                (local_type_ids::decode_index(local_type_ids::decode_id(identity)) != index))
            {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] const SRegistration* find_in_category(const SCategoryView& view, const type_id identity, const std::uint32_t index) noexcept
{
    if ((index >= view.count) || (view.registrations == nullptr))
    {
        return nullptr;
    }

    const SRegistration& registration = view.registrations[index];
    return ((registration.identity == identity) && registration.operations.is_complete())
        ? &registration
        : nullptr;
}

}   //  namespace

bool validate_view(const SRegistryView& view) noexcept
{
    return
        validate_category_view(view.system, ETypeIdCategory::system) &&
        validate_category_view(view.local, ETypeIdCategory::local);
}

bool install_view(const SRegistryView& view) noexcept
{
    if (s_view_installed || !validate_view(view))
    {
        return false;
    }
    s_installed_view = view;
    s_view_installed = true;
    return true;
}

bool view_is_installed() noexcept
{
    return s_view_installed;
}

const SRegistryView* installed_view() noexcept
{
    return s_view_installed ? &s_installed_view : nullptr;
}

const SRegistration* find(const SRegistryView* const view, const type_id identity) noexcept
{
    if ((view == nullptr) || !identity.is_valid())
    {
        return nullptr;
    }

    system_type_id system_identity;
    if (identity.try_system_type_id(system_identity))
    {
        const std::uint32_t index = system_type_ids::decode_index(system_type_ids::decode_id(system_identity));
        return find_in_category(view->system, identity, index);
    }

    local_type_id local_identity;
    if (!identity.try_local_type_id(local_identity))
    {
        return nullptr;
    }
    const std::uint32_t index = local_type_ids::decode_index(local_type_ids::decode_id(local_identity));
    return find_in_category(view->local, identity, index);
}

const SRegistration* find(const type_id identity) noexcept
{
    return find(installed_view(), identity);
}

const SCategoryView& system_operations_view() noexcept
{
    return s_system_view;
}

}   //  namespace erased_owner_operations
