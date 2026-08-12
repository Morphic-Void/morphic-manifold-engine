//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)

#include <cstring>
#include <iostream>
#include <type_traits>

#include "debug/event_arguments.hpp"
#include "host/host_local_type_registry.hpp"
#include "host/system_id_definitions.hpp"
#include "modules/module_binding.hpp"
#include "modules/module_binding.hpp"
#include "system/local_type_registry.hpp"
#include "system/system_id_registry.hpp"
#include "system/transported_types.hpp"
#include "tests/SystemTypeIdentity_test_suite.hpp"

namespace
{
struct CUnregisteredType;
struct TTestContext
{
    void expect(const bool condition, const char* const expression, const int line)
    {
        if (condition) ++passed;
        else { ++failed; std::cerr << "SystemTypeIdentity test failure at line "
            << line << ": " << expression << '\n'; }
    }
    int passed{ 0 };
    int failed{ 0 };
};
#define TEST_EXPECT(ctx, expression) (ctx).expect(!!(expression), #expression, __LINE__)

void test_encoding(TTestContext& ctx)
{
    static_assert(type_ids::k_encoded_payload_mask == 0x15555555u);
    static_assert(type_ids::k_system_type_flag == 0x40000000u);
    static_assert(type_ids::k_id_field_mask == 0x55555555u);
    static_assert(type_ids::k_invalid_id_mask == 0xaaaaaaaau);
    static_assert(type_ids::k_capacity == 0x7fffu);
    static_assert(type_ids::k_max_ordinal == 0x7ffeu);
    static_assert(type_ids::k_category_bit == 0x8000u);
    static_assert(type_ids::k_system_index_min == 0x8000u);
    static_assert(type_ids::k_system_index_max == 0xfffeu);
    static_assert(type_ids::k_invalid_index == 0xffffu);
    static_assert(local_type_ids::k_local_index_min == 0u);
    static_assert(local_type_ids::k_local_index_max == 0x7ffeu);
    static_assert(local_type_ids::k_capacity == type_ids::k_capacity);
    static_assert(!std::is_same_v<type_ids::id_type, local_type_ids::id_type>);

    constexpr type_ids::index_type system_first_index = type_ids::encode_index(0u);
    constexpr type_ids::index_type system_last_index = type_ids::encode_index(type_ids::k_max_ordinal);
    constexpr local_type_ids::index_type local_first_index = local_type_ids::encode_index(0u);
    constexpr local_type_ids::index_type local_last_index = local_type_ids::encode_index(local_type_ids::k_max_ordinal);
    constexpr type_ids::id_type system_first = type_ids::encode_id(system_first_index);
    constexpr type_ids::id_type system_last = type_ids::encode_id(system_last_index);
    constexpr local_type_ids::id_type local_first = local_type_ids::encode_id(local_first_index);
    constexpr local_type_ids::id_type local_last = local_type_ids::encode_id(local_last_index);
    static_assert(system_first.raw_value() == 0x55555555u);
    static_assert(system_last.raw_value() == 0x40000001u);
    static_assert(local_first.raw_value() == 0x15555555u);
    static_assert(local_last.raw_value() == 0x00000001u);
    static_assert(type_ids::decode_id(system_first) == 0x8000u);
    static_assert(type_ids::decode_id(system_last) == 0xfffeu);
    static_assert(local_type_ids::decode_id(local_first) == 0u);
    static_assert(local_type_ids::decode_id(local_last) == 0x7ffeu);
    static_assert(type_ids::is_valid_index(0xfffeu));
    static_assert(!type_ids::is_valid_index(0xffffu));
    static_assert(!type_ids::is_valid_index(0x7ffeu));
    static_assert(local_type_ids::is_valid_index(0x7ffeu));
    static_assert(!local_type_ids::is_valid_index(0x8000u));

    TEST_EXPECT(ctx, !type_ids::is_valid_index(type_ids::k_invalid_index));
    TEST_EXPECT(ctx, !local_type_ids::is_valid_index(local_type_ids::k_invalid_index));
    TEST_EXPECT(ctx, type_ids::encode_id(type_ids::k_invalid_index) == type_ids::undefined);
    TEST_EXPECT(ctx, local_type_ids::encode_id(local_type_ids::k_invalid_index) == local_type_ids::undefined);
    TEST_EXPECT(ctx, !type_ids::is_valid_id(type_ids::id_type{ local_first.raw_value() }));
    TEST_EXPECT(ctx, !local_type_ids::is_valid_id(local_type_ids::id_type{ system_first.raw_value() }));
    TEST_EXPECT(ctx, !type_ids::is_valid_id(type_ids::id_type{ type_ids::k_system_type_flag }));
    TEST_EXPECT(ctx, !local_type_ids::is_valid_id(local_type_ids::id_type{}));
}

void test_registration_categories(TTestContext& ctx)
{
    static_assert(k_type_id_binding_category_v<CByteBuffer> ==
        ETypeIdBindingCategory::system);
    static_assert(k_type_id_binding_category_v<host::CHost> ==
        ETypeIdBindingCategory::local);
    static_assert(k_local_type_id_v<host::CHost> == host_local_type_ids::host_runtime);
    static_assert(!k_can_register_type_id_v<CByteBuffer>);
    static_assert(!k_can_register_type_id_v<host::CHost>);
    static_assert(k_can_register_type_id_v<CUnregisteredType>);
    static_assert(!debug_system::is_supported_event_argument_v<local_type_ids::id_type>);
    TEST_EXPECT(ctx, type_ids::is_valid_id(k_type_id_v<CByteBuffer>));
    TEST_EXPECT(ctx, local_type_ids::is_valid_id(k_local_type_id_v<host::CHost>));
}

void test_advertised_identity_negotiation(TTestContext& ctx)
{
    constexpr modules::SAdvertisedIdentity host_identity{
        module_ids::executable, { 7u, 1u }, 2u, 4u };
    constexpr modules::SAdvertisedIdentity module_identity{
        module_ids::application, { 9u, 3u }, 1u, 3u };
    constexpr modules::SAdvertisedIdentity incompatible_identity{
        module_ids::application, { 9u, 3u }, 5u, 6u };
    constexpr modules::SAdvertisedIdentity invalid_identity{
        module_ids::application, { 9u, 3u }, 4u, 3u };

    static_assert(modules::is_valid_advertised_identity(host_identity));
    static_assert(modules::is_valid_advertised_identity(module_identity));
    static_assert(!modules::is_valid_advertised_identity(invalid_identity));
    static_assert(modules::functional_ranges_overlap(host_identity, module_identity));
    static_assert(!modules::functional_ranges_overlap(host_identity, incompatible_identity));
    static_assert(modules::highest_common_functional_major(
        host_identity, module_identity) == 3u);

    TEST_EXPECT(ctx, modules::functional_ranges_overlap(module_identity, host_identity));
    TEST_EXPECT(ctx, modules::highest_common_functional_major(
        module_identity, host_identity) == 3u);
}

void test_local_names_and_lookup(TTestContext& ctx)
{
    static_assert(local_type_registry::is_valid_name_literal("123456789012345"));
    static_assert(!local_type_registry::is_valid_name_literal("1234567890123456"));
    constexpr local_type_registry::SLocalTypeName name = local_type_registry::make_name("short");
    static_assert(name.bytes[0] == 's' && name.bytes[5] == 0 && name.bytes[15] == 0);
    static_assert(name.bytes[6] == 0 && name.bytes[7] == 0 && name.bytes[8] == 0 &&
        name.bytes[9] == 0 && name.bytes[10] == 0 && name.bytes[11] == 0 &&
        name.bytes[12] == 0 && name.bytes[13] == 0 && name.bytes[14] == 0);

    TEST_EXPECT(ctx, local_type_registry::view_is_installed());
    const local_type_registry::SLocalTypeRegistration* const registration =
        local_type_registry::find_type(host_local_type_ids::host_runtime);
    TEST_EXPECT(ctx, registration != nullptr);
    if (registration != nullptr)
        TEST_EXPECT(ctx, std::strcmp(registration->short_name.bytes, "host_runtime") == 0);
    TEST_EXPECT(ctx, local_type_registry::find_type(
        static_cast<const local_type_registry::SLocalTypeRegistryView*>(nullptr),
        host_local_type_ids::host_runtime) == nullptr);
    const local_type_registry::SLocalTypeRegistryView unavailable_view{ nullptr, 1u };
    TEST_EXPECT(ctx, local_type_registry::find_type(
        &unavailable_view, host_local_type_ids::host_runtime) == nullptr);
    TEST_EXPECT(ctx, local_type_registry::find_type(
        local_type_ids::encode_id(host_local_type_ids::k_count)) == nullptr);
    TEST_EXPECT(ctx, !local_type_registry::install_view(host::local_type_registry_view()));

    local_type_registry::SLocalTypeRegistration corrupt{
        local_type_ids::encode_id(0u), 0u, local_type_registry::make_name("valid") };
    corrupt.short_name.bytes[15] = 'x';
    const local_type_registry::SLocalTypeRegistryView corrupt_view{ &corrupt, 1u };
    TEST_EXPECT(ctx, !local_type_registry::validate_view(corrupt_view));
    TEST_EXPECT(ctx, local_type_registry::find_type(&corrupt_view, corrupt.id) == nullptr);
}

void test_system_authority(TTestContext& ctx)
{
    TEST_EXPECT(ctx, system_id_registry::view_is_installed());
    TEST_EXPECT(ctx, system_id_registry::validate_all());
    TEST_EXPECT(ctx, system_id_registry::installed_view() != nullptr);
    TEST_EXPECT(ctx, !system_id_registry::install_view(host::system_registry_view()));
    TEST_EXPECT(ctx, system_id_registry::find_type(
        static_cast<const system_id_registry::SSystemRegistryView*>(nullptr),
        type_ids::byte_buffer) == nullptr);
    const system_id_registry::SSystemRegistryView unavailable_view{ nullptr, 1u };
    TEST_EXPECT(ctx, system_id_registry::find_type(
        &unavailable_view, type_ids::encode_id(type_ids::encode_index(0u))) == nullptr);

    constexpr char long_name[] = "a-system-name-is-not-limited-to-fifteen-bytes";
    const system_id_registry::STypeRegistration registration{
        type_ids::encode_id(type_ids::encode_index(0u)), type_ids::encode_index(0u),
        long_name, sizeof(long_name) - 1u };
    const system_id_registry::SSystemRegistryView view{ &registration, 1u };
    TEST_EXPECT(ctx, system_id_registry::validate_view(view));
    TEST_EXPECT(ctx, system_id_registry::find_type(&view, registration.id) == &registration);

    system_id_registry::STypeRegistration corrupt = registration;
    corrupt.id = type_ids::encode_id(type_ids::encode_index(1u));
    const system_id_registry::SSystemRegistryView corrupt_view{ &corrupt, 1u };
    TEST_EXPECT(ctx, !system_id_registry::validate_view(corrupt_view));
    TEST_EXPECT(ctx, system_id_registry::find_type(&corrupt_view, corrupt.id) == nullptr);
}
}

int run_system_type_identity_tests()
{
    TTestContext ctx;
    test_encoding(ctx);
    test_registration_categories(ctx);
    test_advertised_identity_negotiation(ctx);
    test_local_names_and_lookup(ctx);
    test_system_authority(ctx);
    std::cout << "SystemTypeIdentity: " << ctx.passed << " passed, "
        << ctx.failed << " failed\n";
    return ctx.failed;
}
