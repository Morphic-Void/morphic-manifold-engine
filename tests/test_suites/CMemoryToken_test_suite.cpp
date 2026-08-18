
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   CMemoryToken_test_suite.cpp
//  Author: Ritchie Brannan
//  Date:   13 Jul 26

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <new>

#include "memory/memory_token.hpp"
#include "tests/test_suites/CMemoryToken_test_suite.hpp"
#include "tests/support/test_context.hpp"

namespace
{

using memory::CMemoryToken;

constexpr std::size_t k_stable_stride = sizeof(std::uint32_t);
constexpr std::size_t k_stable_alignment = alignof(std::uint32_t);
constexpr std::size_t k_buffer_capacity_hint = 3u;
constexpr std::size_t k_buffer_capacity = 4u;

using TTestContext = tests::TTestContext;

void* MV_STD_ABI_CALL test_allocate(
    void*,
    const std::size_t alignment,
    const std::size_t bytes) noexcept
{
    return ::operator new(bytes, std::align_val_t{ alignment }, std::nothrow);
}

bool MV_STD_ABI_CALL test_deallocate(
    void*,
    const std::size_t alignment,
    void* const ptr) noexcept
{
    ::operator delete(ptr, std::align_val_t{ alignment });
    return true;
}

std::size_t round_up_to_pow2(std::size_t value) noexcept
{
    std::size_t result = 1u;
    while (result < value)
    {
        result <<= 1u;
    }
    return result;
}

std::size_t stable_buffer_count(const std::size_t count, const std::size_t per_buffer_capacity) noexcept
{
    return (count == 0u) ? 0u : (1u + ((count - 1u) / per_buffer_capacity));
}

std::size_t stable_directory_capacity(const std::size_t buffer_count) noexcept
{
    if (buffer_count <= 1u)
    {
        return 0u;
    }
    return (buffer_count <= CMemoryToken::k_min_directory_capacity) ?
        CMemoryToken::k_min_directory_capacity :
        round_up_to_pow2(buffer_count);
}

std::uint32_t expected_stable_allocation_count(
    const std::size_t count,
    const std::size_t per_buffer_capacity) noexcept
{
    const std::size_t buffer_count = stable_buffer_count(count, per_buffer_capacity);
    return static_cast<std::uint32_t>(buffer_count + ((buffer_count > 1u) ? 1u : 0u));
}

std::uint64_t expected_stable_allocated_bytes(
    const memory::CMemoryContext& context,
    const std::size_t stride,
    const std::size_t storage_alignment,
    const std::size_t per_buffer_capacity,
    const std::size_t count) noexcept
{
    const std::size_t buffer_count = stable_buffer_count(count, per_buffer_capacity);
    if (buffer_count == 0u)
    {
        return 0u;
    }

    const std::size_t conditioned_buffer_alignment = context.condition_alignment(storage_alignment);
    const std::size_t conditioned_buffer_bytes = context.condition_bytes(
        conditioned_buffer_alignment, per_buffer_capacity * stride);
    std::uint64_t total = static_cast<std::uint64_t>(buffer_count) * conditioned_buffer_bytes;
    if (buffer_count > 1u)
    {
        const std::size_t conditioned_directory_alignment = context.condition_alignment(alignof(void*));
        total += context.condition_bytes(
            conditioned_directory_alignment,
            stable_directory_capacity(buffer_count) * sizeof(void*));
    }
    return total;
}

void write_value(CMemoryToken& token, const std::size_t index, const std::uint32_t value)
{
    *static_cast<std::uint32_t*>(token.index_ptr(index)) = value;
}

std::uint32_t read_value(const CMemoryToken& token, const std::size_t index)
{
    return *static_cast<const std::uint32_t*>(token.index_ptr(index));
}

void test_stable_map_index_growth_and_slack(TTestContext& ctx, memory::CMemoryContext& context)
{
    CMemoryToken token{ k_stable_stride, k_stable_alignment, k_buffer_capacity_hint, &context };
    TEST_EXPECT(ctx, token.is_stable());
    TEST_EXPECT(ctx, token.per_buffer_capacity() == k_buffer_capacity);
    TEST_EXPECT(ctx, token.storage_alignment() == k_stable_alignment);
    TEST_EXPECT(ctx, token.count() == 0u);
    TEST_EXPECT(ctx, token.bytes() == 0u);

    std::array<void*, 9u> addresses{};
    std::array<std::uint32_t, 9u> values{};

    for (std::size_t index = 0u; index < addresses.size(); ++index)
    {
        addresses[index] = token.map_index(index, false);
        values[index] = static_cast<std::uint32_t>(0x100u + index);
        TEST_EXPECT(ctx, addresses[index] != nullptr);
        TEST_EXPECT(ctx, token.count() == (index + 1u));
        write_value(token, index, values[index]);

        for (std::size_t preserved = 0u; preserved <= index; ++preserved)
        {
            TEST_EXPECT(ctx, token.index_ptr(preserved) == addresses[preserved]);
            TEST_EXPECT(ctx, read_value(token, preserved) == values[preserved]);
        }
    }

    TEST_EXPECT(ctx, addresses[0] == token.index_ptr(0u));
    TEST_EXPECT(ctx, addresses[3] == token.index_ptr(3u));
    TEST_EXPECT(ctx, addresses[4] == token.index_ptr(4u));
    TEST_EXPECT(ctx, addresses[8] == token.index_ptr(8u));
    TEST_EXPECT(ctx, token.count() == 9u);
    TEST_EXPECT(ctx, token.bytes() == (9u * k_stable_stride));
    TEST_EXPECT(ctx, token.bytes() < expected_stable_allocated_bytes(
        context, token.stride(), token.storage_alignment(), token.per_buffer_capacity(), token.count()));
    TEST_EXPECT(ctx, token.contains_index(8u));
    TEST_EXPECT(ctx, !token.contains_index(9u));
    TEST_EXPECT(ctx, token.index_ptr(9u) == nullptr);
    TEST_EXPECT(ctx, context.get_live_allocation_count() == expected_stable_allocation_count(9u, k_buffer_capacity));
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() == expected_stable_allocated_bytes(
        context, token.stride(), token.storage_alignment(), token.per_buffer_capacity(), 9u));

    token.deallocate();
    TEST_EXPECT(ctx, context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() == 0u);
}

void test_stable_requested_count_and_internal_slack(TTestContext& ctx, memory::CMemoryContext& context)
{
    CMemoryToken token{ k_stable_stride, k_stable_alignment, k_buffer_capacity_hint, &context };

    TEST_EXPECT(ctx, token.allocate(5u, true));
    TEST_EXPECT(ctx, token.count() == 5u);
    TEST_EXPECT(ctx, token.bytes() == (5u * k_stable_stride));
    TEST_EXPECT(ctx, token.per_buffer_capacity() == k_buffer_capacity);
    TEST_EXPECT(ctx, token.index_ptr(4u) != nullptr);
    TEST_EXPECT(ctx, token.index_ptr(5u) == nullptr);
    TEST_EXPECT(ctx, context.get_live_allocation_count() == expected_stable_allocation_count(5u, k_buffer_capacity));
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() == expected_stable_allocated_bytes(
        context, token.stride(), token.storage_alignment(), token.per_buffer_capacity(), 5u));
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() > token.bytes());

    token.deallocate();
    TEST_EXPECT(ctx, context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() == 0u);
}

void test_stable_bounds_and_max_count(TTestContext& ctx, memory::CMemoryContext& context)
{
    CMemoryToken token{ k_stable_stride, k_stable_alignment, k_buffer_capacity_hint, &context };
    const std::size_t max_count = token.max_count();

    TEST_EXPECT(ctx, max_count == memory::max_elements(k_stable_stride));
    TEST_EXPECT(ctx, max_count == (memory::k_byte_size_ceiling / k_stable_stride));
    TEST_EXPECT(ctx, token.can_grow_to(0u));
    TEST_EXPECT(ctx, token.can_grow_to(max_count));
    TEST_EXPECT(ctx, !token.can_grow_to(max_count + 1u));
    TEST_EXPECT(ctx, token.index_ptr(0u) == nullptr);
    TEST_EXPECT(ctx, token.map_index(max_count, true) == nullptr);
    TEST_EXPECT(ctx, !token.allocate(max_count + 1u, true));
    TEST_EXPECT(ctx, !token.grow_to(max_count + 1u, true));
    TEST_EXPECT(ctx, token.count() == 0u);
    TEST_EXPECT(ctx, !token.owns_storage());
}

void test_growth_policy_ceiling(TTestContext& ctx)
{
    TEST_EXPECT(ctx, memory::vector_growth_policy(1u, 1u) == 1u);
    TEST_EXPECT(ctx, memory::vector_growth_policy(1u, 100u) == 32u);
    TEST_EXPECT(ctx, memory::vector_growth_policy(100u, 100u) == 100u);
    TEST_EXPECT(ctx, memory::buffer_growth_policy(1u, 2048u) == 2048u);
    TEST_EXPECT(ctx, memory::default_growth_policy(1u, 16u) == 16u);
}

void test_stable_clone_preserves_content_and_configuration(TTestContext& ctx, memory::CMemoryContext& context)
{
    CMemoryToken source{ k_stable_stride, k_stable_alignment, k_buffer_capacity_hint, &context };
    TEST_EXPECT(ctx, source.allocate(9u, false));
    for (std::size_t index = 0u; index < source.count(); ++index)
    {
        write_value(source, index, static_cast<std::uint32_t>(0x200u + index));
    }

    CMemoryToken clone;
    TEST_EXPECT(ctx, clone.clone(source));
    TEST_EXPECT(ctx, clone.is_stable());
    TEST_EXPECT(ctx, clone.context() == &context);
    TEST_EXPECT(ctx, clone.count() == source.count());
    TEST_EXPECT(ctx, clone.stride() == source.stride());
    TEST_EXPECT(ctx, clone.storage_alignment() == source.storage_alignment());
    TEST_EXPECT(ctx, clone.per_buffer_capacity() == source.per_buffer_capacity());
    TEST_EXPECT(ctx, clone.owns_storage());
    TEST_EXPECT(ctx, context.get_live_allocation_count() ==
        (2u * expected_stable_allocation_count(source.count(), source.per_buffer_capacity())));
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() ==
        (2u * expected_stable_allocated_bytes(
            context, source.stride(), source.storage_alignment(), source.per_buffer_capacity(), source.count())));

    for (std::size_t index = 0u; index < source.count(); ++index)
    {
        TEST_EXPECT(ctx, clone.index_ptr(index) != nullptr);
        TEST_EXPECT(ctx, clone.index_ptr(index) != source.index_ptr(index));
        TEST_EXPECT(ctx, read_value(clone, index) == read_value(source, index));
    }

    write_value(clone, 4u, 0xfeedu);
    TEST_EXPECT(ctx, read_value(clone, 4u) == 0xfeedu);
    TEST_EXPECT(ctx, read_value(source, 4u) == 0x204u);

    clone.deallocate();
    source.deallocate();
    TEST_EXPECT(ctx, context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() == 0u);
}

void test_stable_clone_to_compatible_alternate_context(
    TTestContext& ctx,
    memory::CMemoryContext& source_context,
    memory::CMemoryContext& alternate_context)
{
    CMemoryToken source{ k_stable_stride, k_stable_alignment, k_buffer_capacity_hint, &source_context };
    TEST_EXPECT(ctx, source.allocate(5u, false));
    for (std::size_t index = 0u; index < source.count(); ++index)
    {
        write_value(source, index, static_cast<std::uint32_t>(0x300u + index));
    }

    const std::uint32_t expected_allocations =
        expected_stable_allocation_count(source.count(), source.per_buffer_capacity());
    const std::uint64_t expected_bytes = expected_stable_allocated_bytes(
        source_context, source.stride(), source.storage_alignment(), source.per_buffer_capacity(), source.count());

    CMemoryToken clone;
    TEST_EXPECT(ctx, clone.clone(source, &alternate_context));
    TEST_EXPECT(ctx, clone.is_stable());
    TEST_EXPECT(ctx, clone.context() == &alternate_context);
    TEST_EXPECT(ctx, clone.count() == source.count());
    TEST_EXPECT(ctx, clone.per_buffer_capacity() == source.per_buffer_capacity());
    TEST_EXPECT(ctx, source_context.get_live_allocation_count() == expected_allocations);
    TEST_EXPECT(ctx, source_context.get_live_allocated_bytes() == expected_bytes);
    TEST_EXPECT(ctx, alternate_context.get_live_allocation_count() == expected_allocations);
    TEST_EXPECT(ctx, alternate_context.get_live_allocated_bytes() == expected_bytes);

    for (std::size_t index = 0u; index < source.count(); ++index)
    {
        TEST_EXPECT(ctx, clone.index_ptr(index) != source.index_ptr(index));
        TEST_EXPECT(ctx, read_value(clone, index) == read_value(source, index));
    }

    write_value(clone, 1u, 0xbeefu);
    TEST_EXPECT(ctx, read_value(source, 1u) == 0x301u);

    clone.deallocate();
    source.deallocate();
    TEST_EXPECT(ctx, source_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, source_context.get_live_allocated_bytes() == 0u);
    TEST_EXPECT(ctx, alternate_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, alternate_context.get_live_allocated_bytes() == 0u);
}

void test_stable_reattribute_between_compatible_contexts(
    TTestContext& ctx,
    memory::CMemoryContext& source_context,
    memory::CMemoryContext& target_context)
{
    CMemoryToken token{ k_stable_stride, k_stable_alignment, k_buffer_capacity_hint, &source_context };
    TEST_EXPECT(ctx, token.allocate(9u, true));

    const std::uint32_t expected_allocations =
        expected_stable_allocation_count(token.count(), token.per_buffer_capacity());
    const std::uint64_t expected_bytes = expected_stable_allocated_bytes(
        source_context, token.stride(), token.storage_alignment(), token.per_buffer_capacity(), token.count());

    TEST_EXPECT(ctx, token.can_reattribute_to(&target_context));
    TEST_EXPECT(ctx, token.reattribute(&target_context));
    TEST_EXPECT(ctx, token.context() == &target_context);
    TEST_EXPECT(ctx, source_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, source_context.get_live_allocated_bytes() == 0u);
    TEST_EXPECT(ctx, target_context.get_live_allocation_count() == expected_allocations);
    TEST_EXPECT(ctx, target_context.get_live_allocated_bytes() == expected_bytes);

    token.deallocate();
    TEST_EXPECT(ctx, target_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, target_context.get_live_allocated_bytes() == 0u);
}

void test_aggregate_accounting_and_context_replacement(
    TTestContext& ctx,
    memory::CMemoryContext& source_context,
    memory::CMemoryContext& target_context,
    memory::CMemoryContext& incompatible_context)
{
    CMemoryToken relocatable{ sizeof(std::uint64_t), alignof(std::uint64_t), &source_context };
    CMemoryToken stable{ k_stable_stride, k_stable_alignment, k_buffer_capacity_hint, &source_context };
    CMemoryToken empty{ 1u, 1u, &source_context };
    TEST_EXPECT(ctx, relocatable.allocate(7u, true));
    TEST_EXPECT(ctx, stable.allocate(9u, true));

    void* const relocatable_address = relocatable.data();
    void* const stable_address = stable.index_ptr(4u);
    const std::uint32_t expected_allocations = source_context.get_live_allocation_count();
    const std::uint64_t expected_bytes = source_context.get_live_allocated_bytes();

    TEST_EXPECT(ctx, relocatable.memory_token_count() == 1u);
    TEST_EXPECT(ctx, stable.memory_token_count() == 1u);
    TEST_EXPECT(ctx, empty.memory_token_count() == 1u);
    TEST_EXPECT(ctx, (relocatable.memory_allocation_count() + stable.memory_allocation_count()) == expected_allocations);
    TEST_EXPECT(ctx, (relocatable.memory_allocation_size() + stable.memory_allocation_size()) == expected_bytes);
    TEST_EXPECT(ctx, relocatable.can_reattribute_to(&target_context));
    TEST_EXPECT(ctx, stable.can_reattribute_to(&target_context));
    TEST_EXPECT(ctx, !relocatable.can_reattribute_to(&incompatible_context));
    TEST_EXPECT(ctx, !stable.can_reattribute_to(&incompatible_context));
    TEST_EXPECT(ctx, relocatable.context() == &source_context);
    TEST_EXPECT(ctx, stable.context() == &source_context);
    TEST_EXPECT(ctx, empty.context() == &source_context);
    TEST_EXPECT(ctx, source_context.get_live_allocation_count() == expected_allocations);
    TEST_EXPECT(ctx, source_context.get_live_allocated_bytes() == expected_bytes);
    TEST_EXPECT(ctx, incompatible_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, incompatible_context.get_live_allocated_bytes() == 0u);

    TEST_EXPECT(ctx, memory::reattribute(source_context, target_context, expected_allocations, expected_bytes));
    relocatable.unsafe_replace_context_without_accounting(&source_context, &target_context);
    stable.unsafe_replace_context_without_accounting(&source_context, &target_context);
    empty.unsafe_replace_context_without_accounting(&source_context, &target_context);
    TEST_EXPECT(ctx, relocatable.context() == &target_context);
    TEST_EXPECT(ctx, stable.context() == &target_context);
    TEST_EXPECT(ctx, empty.context() == &target_context);
    TEST_EXPECT(ctx, relocatable.data() == relocatable_address);
    TEST_EXPECT(ctx, stable.index_ptr(4u) == stable_address);
    TEST_EXPECT(ctx, source_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, source_context.get_live_allocated_bytes() == 0u);
    TEST_EXPECT(ctx, target_context.get_live_allocation_count() == expected_allocations);
    TEST_EXPECT(ctx, target_context.get_live_allocated_bytes() == expected_bytes);

    relocatable.deallocate();
    stable.deallocate();
    TEST_EXPECT(ctx, target_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, target_context.get_live_allocated_bytes() == 0u);
}

void test_stable_deallocate_preserves_configuration_and_reuse(TTestContext& ctx, memory::CMemoryContext& context)
{
    CMemoryToken token{ k_stable_stride, k_stable_alignment, k_buffer_capacity_hint, &context };
    TEST_EXPECT(ctx, token.allocate(6u, true));

    token.deallocate();
    TEST_EXPECT(ctx, token.is_stable());
    TEST_EXPECT(ctx, token.context() == &context);
    TEST_EXPECT(ctx, token.stride() == k_stable_stride);
    TEST_EXPECT(ctx, token.storage_alignment() == k_stable_alignment);
    TEST_EXPECT(ctx, token.per_buffer_capacity() == k_buffer_capacity);
    TEST_EXPECT(ctx, token.is_empty());
    TEST_EXPECT(ctx, !token.owns_storage());
    TEST_EXPECT(ctx, token.bytes() == 0u);
    TEST_EXPECT(ctx, token.index_ptr(0u) == nullptr);
    TEST_EXPECT(ctx, context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() == 0u);

    TEST_EXPECT(ctx, token.map_index(2u, true) != nullptr);
    TEST_EXPECT(ctx, token.count() == 3u);
    TEST_EXPECT(ctx, token.owns_storage());
    TEST_EXPECT(ctx, context.get_live_allocation_count() == expected_stable_allocation_count(3u, token.per_buffer_capacity()));
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() == expected_stable_allocated_bytes(
        context, token.stride(), token.storage_alignment(), token.per_buffer_capacity(), 3u));

    token.deallocate();
    TEST_EXPECT(ctx, context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() == 0u);
}

}   //  namespace

int run_memory_token_tests()
{
    TTestContext ctx;
    memory::CMemoryAllocator allocator{ nullptr, &test_allocate, &test_deallocate };
    memory::CMemoryAllocator incompatible_allocator{ nullptr, &test_allocate, &test_deallocate };
    memory::CMemoryContext context{ allocator };
    memory::CMemoryContext alternate_context{ allocator };
    memory::CMemoryContext incompatible_context{ incompatible_allocator };

    test_stable_map_index_growth_and_slack(ctx, context);
    test_stable_requested_count_and_internal_slack(ctx, context);
    test_stable_bounds_and_max_count(ctx, context);
    test_growth_policy_ceiling(ctx);
    test_stable_clone_preserves_content_and_configuration(ctx, context);
    test_stable_clone_to_compatible_alternate_context(ctx, context, alternate_context);
    test_stable_reattribute_between_compatible_contexts(ctx, context, alternate_context);
    test_aggregate_accounting_and_context_replacement(ctx, context, alternate_context, incompatible_context);
    test_stable_deallocate_preserves_configuration_and_reuse(ctx, context);

    TEST_EXPECT(ctx, context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() == 0u);
    TEST_EXPECT(ctx, alternate_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, alternate_context.get_live_allocated_bytes() == 0u);
    TEST_EXPECT(ctx, incompatible_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, incompatible_context.get_live_allocated_bytes() == 0u);
    std::cout << "CMemoryToken: " << ctx.passed << " passed, " << ctx.failed << " failed\n";
    return (ctx.failed == 0) ? 0 : 1;
}
