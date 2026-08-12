
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   ErasedPod_test_suite.cpp

#include <cstdint>
#include <cstring>
#include <iostream>
#include <type_traits>

#include "system/erased_pod.hpp"
#include "system/transported_types.hpp"
#include "tests/ErasedPod_test_suite.hpp"
#include "threading/messages/CErasedPodMsg.hpp"

namespace
{

struct TTestContext
{
    void expect(const bool condition, const char* const expression, const int line)
    {
        if (condition)
        {
            ++passed;
        }
        else
        {
            ++failed;
            std::cerr << "ErasedPod test failure at line " << line << ": " << expression << '\n';
        }
    }

    int passed{ 0 };
    int failed{ 0 };
};

#define TEST_EXPECT(ctx, expression) (ctx).expect(!!(expression), #expression, __LINE__)

struct CCanonicalValue
{
    CCanonicalValue() noexcept : value(0u) {}
    std::uint32_t value;
};

struct CNonTriviallyCopyable
{
    CNonTriviallyCopyable() noexcept = default;
    CNonTriviallyCopyable(const CNonTriviallyCopyable& other) noexcept : value(other.value) {}
    std::uint32_t value;
};

struct alignas(16) SAlignedPod
{
    std::uint64_t values[2];
};

struct alignas(32) SOverAlignedPod
{
    std::uint64_t values[4];
};

struct SOversizedPod
{
    std::uint64_t values[7];
};

void test_erased_pod_redefinition_and_access(TTestContext& ctx)
{
    using storage_type = TErasedPod<sizeof(UnrecognisedMsg), alignof(UnrecognisedMsg)>;

    static_assert(std::is_trivially_copyable_v<storage_type>);
    static_assert(std::is_standard_layout_v<storage_type>);
    static_assert(storage_type::k_payload_align == 16u);
    static_assert(alignof(storage_type) == 16u);
    static_assert(storage_type::is_compatible_with<UnrecognisedMsg>());
    static_assert(storage_type::is_compatible_with<FileSaveResult>());
    static_assert(!std::is_trivial_v<CCanonicalValue>);
    static_assert(std::is_trivially_copyable_v<CCanonicalValue>);
    static_assert(storage_type::is_compatible_with<CCanonicalValue>());
    static_assert(!storage_type::is_compatible_with<CNonTriviallyCopyable>());

    storage_type storage;
    TEST_EXPECT(ctx, storage.query_type_id() == system_type_ids::undefined);
    TEST_EXPECT(ctx, storage.query_tag() == 0u);
    TEST_EXPECT(ctx, storage.payload<UnrecognisedMsg>() == nullptr);

    storage.set_tag(41u);
    UnrecognisedMsg& unrecognised = storage.redefine<UnrecognisedMsg>();
    unrecognised.msg_id = system_type_ids::file_load_request;

    TEST_EXPECT(ctx, storage.query_type_id() == k_system_type_id_v<UnrecognisedMsg>);
    TEST_EXPECT(ctx, storage.query_tag() == 41u);
    TEST_EXPECT(ctx, storage.is_a<UnrecognisedMsg>());
    TEST_EXPECT(ctx, storage.payload<UnrecognisedMsg>() == &unrecognised);
    TEST_EXPECT(ctx, storage.payload<UnrecognisedMsg>()->msg_id == system_type_ids::file_load_request);

    const storage_type& const_storage = storage;
    TEST_EXPECT(ctx, const_storage.payload<UnrecognisedMsg>() == &unrecognised);

    FileSaveResult& result = storage.redefine<FileSaveResult>();
    TEST_EXPECT(ctx, !result.success);
    TEST_EXPECT(ctx, storage.query_tag() == 41u);
    result.success = true;

    TEST_EXPECT(ctx, storage.query_type_id() == k_system_type_id_v<FileSaveResult>);
    TEST_EXPECT(ctx, storage.payload<UnrecognisedMsg>() == nullptr);
    TEST_EXPECT(ctx, storage.payload<FileSaveResult>() == &result);
    TEST_EXPECT(ctx, storage.payload<FileSaveResult>()->success);
}

void test_erased_pod_clears_complete_storage_extent(TTestContext& ctx)
{
    using storage_type = TErasedPod<16u, 16u>;
    static_assert(std::is_trivially_copyable_v<storage_type>);
    static_assert(sizeof(storage_type) == 32u);

    storage_type reused;
    reused.set_tag(73u);
    std::memset(
        reinterpret_cast<unsigned char*>(&reused) + 16u, 0xff,
        sizeof(reused) - 16u);
    FileSaveResult& redefined = reused.redefine<FileSaveResult>();

    storage_type fresh;
    fresh.set_tag(73u);
    FileSaveResult& initial = fresh.redefine<FileSaveResult>();

    TEST_EXPECT(ctx, !redefined.success);
    TEST_EXPECT(ctx, !initial.success);
    TEST_EXPECT(ctx, reused.query_tag() == 73u);
    TEST_EXPECT(ctx, std::memcmp(&reused, &fresh, sizeof(storage_type)) == 0);
}

void test_erased_pod_header_and_extended_alignment(TTestContext& ctx)
{
    using minimum_storage_type = TErasedPod<16u, 1u>;
    using extended_storage_type = TErasedPod<16u, 32u>;

    static_assert(minimum_storage_type::k_payload_align == 16u);
    static_assert(minimum_storage_type::is_compatible_with<SAlignedPod>());
    static_assert(alignof(minimum_storage_type) == 16u);
    static_assert(extended_storage_type::k_payload_align == 32u);
    static_assert(alignof(extended_storage_type) == 32u);
    static_assert(sizeof(extended_storage_type) == 64u);

    minimum_storage_type initial;
    const unsigned char* const initial_bytes = reinterpret_cast<const unsigned char*>(&initial);
    bool reserved_is_zero = true;
    for (std::size_t byte_index = 8u; byte_index < 16u; ++byte_index)
    {
        reserved_is_zero = reserved_is_zero && (initial_bytes[byte_index] == 0u);
    }
    TEST_EXPECT(ctx, reserved_is_zero);

    extended_storage_type reused;
    reused.set_tag(19u);
    std::memset(
        reinterpret_cast<unsigned char*>(&reused) + 16u, 0xff,
        sizeof(reused) - 16u);
    (void)reused.redefine<FileSaveResult>();

    extended_storage_type fresh;
    fresh.set_tag(19u);
    (void)fresh.redefine<FileSaveResult>();

    TEST_EXPECT(ctx, reused.query_tag() == 19u);
    TEST_EXPECT(ctx, std::memcmp(&reused, &fresh, sizeof(reused)) == 0);
}

void test_thread_message_copy_boundary(TTestContext& ctx)
{
    using threading::CErasedPodMsg;

    static_assert(std::is_trivially_copyable_v<CErasedPodMsg>);
    static_assert(std::is_standard_layout_v<CErasedPodMsg>);
    static_assert(CErasedPodMsg::is_payload_compatible_with<FileSaveResult>());
    static_assert(CErasedPodMsg::is_payload_compatible_with<SAlignedPod>());
    static_assert(CErasedPodMsg::is_payload_compatible_with<CCanonicalValue>());
    static_assert(CErasedPodMsg::is_payload_compatible_with<FileSaveRequest>());
    static_assert(CErasedPodMsg::is_payload_compatible_with<TgaSaveRequest>());
    static_assert(CErasedPodMsg::is_payload_compatible_with<TgaEncodeRequest>());
    static_assert(CErasedPodMsg::is_payload_compatible_with<TgaDecodeRequest>());
    static_assert(!CErasedPodMsg::is_payload_compatible_with<CNonTriviallyCopyable>());
    static_assert(!CErasedPodMsg::is_payload_compatible_with<SOverAlignedPod>());
    static_assert(!CErasedPodMsg::is_payload_compatible_with<SOversizedPod>());
    static_assert(sizeof(CErasedPodMsg) == 64u);
    static_assert(alignof(CErasedPodMsg) == 16u);

    CErasedPodMsg message;
    const unsigned char zero_message[sizeof(CErasedPodMsg)]{};
    TEST_EXPECT(ctx, std::memcmp(&message, zero_message, sizeof(message)) == 0);
    TEST_EXPECT(ctx, !message.has_message_type());
    TEST_EXPECT(ctx, message.query_message_type_id() == system_type_ids::undefined);
    TEST_EXPECT(ctx, message.query_async_slot() == 0);

    message.set_async_slot(41);
    TEST_EXPECT(ctx, message.query_async_slot() == 41);

    FileSaveResult source{ true };
    message.assign_payload(source);
    source.success = false;

    TEST_EXPECT(ctx, message.has_message_type());
    TEST_EXPECT(ctx, message.query_async_slot() == 41);
    TEST_EXPECT(ctx, message.is_payload_a<FileSaveResult>());
    TEST_EXPECT(ctx, message.query_message_type_id() == k_system_type_id_v<FileSaveResult>);

    FileSaveResult copied{ false };
    TEST_EXPECT(ctx, message.copy_payload_to(copied));
    TEST_EXPECT(ctx, copied.success);

    copied.success = false;
    TEST_EXPECT(ctx, message.copy_payload_to(copied));
    TEST_EXPECT(ctx, copied.success);

    UnrecognisedMsg mismatch{ system_type_ids::tga_save_request };
    TEST_EXPECT(ctx, !message.copy_payload_to(mismatch));
    TEST_EXPECT(ctx, mismatch.msg_id == system_type_ids::tga_save_request);
}

void test_thread_message_clears_previous_representation(TTestContext& ctx)
{
    threading::CErasedPodMsg reused;
    const TgaSaveRequest larger{
        nullptr, CAssetId{},
        { image::codec::tga::image_encode_src::AutoTrue32, true, true, false } };
    reused.assign_payload(larger);

    const UnrecognisedMsg small{ system_type_ids::file_save_result };
    reused.assign_payload(small);

    threading::CErasedPodMsg fresh;
    fresh.assign_payload(small);

    TEST_EXPECT(ctx, std::memcmp(&reused, &fresh, sizeof(reused)) == 0);
}

}   //  namespace

int run_erased_pod_tests()
{
    TTestContext ctx;
    test_erased_pod_redefinition_and_access(ctx);
    test_erased_pod_clears_complete_storage_extent(ctx);
    test_erased_pod_header_and_extended_alignment(ctx);
    test_thread_message_copy_boundary(ctx);
    test_thread_message_clears_previous_representation(ctx);

    std::cout << "ErasedPod: " << ctx.passed << " passed, " << ctx.failed << " failed\n";
    return ctx.failed;
}
