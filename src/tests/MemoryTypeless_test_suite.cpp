//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   MemoryTypeless_test_suite.cpp

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <new>
#include <type_traits>
#include <utility>

#include "debug/debug.hpp"
#include "memory/memory_context.hpp"
#include "memory/memory_typeless.hpp"
#include "tests/MemoryTypeless_test_suite.hpp"

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
            std::cerr << "MemoryTypeless test failure at line " << line << ": " << expression << '\n';
        }
    }

    int passed{ 0 };
    int failed{ 0 };
};

#define TEST_EXPECT(ctx, expression) (ctx).expect(!!(expression), #expression, __LINE__)

struct TAllocatorState
{
    bool reject_allocation{ false };
};

void* MV_STD_ABI_CALL allocate_test_memory(
    void* const state,
    const std::size_t alignment,
    const std::size_t bytes) noexcept
{
    if (static_cast<TAllocatorState*>(state)->reject_allocation)
    {
        return nullptr;
    }
    return ::operator new[](bytes, std::align_val_t{ alignment }, std::nothrow);
}

bool MV_STD_ABI_CALL deallocate_test_memory(
    void*,
    const std::size_t alignment,
    void* const ptr) noexcept
{
    ::operator delete[](ptr, std::align_val_t{ alignment });
    return true;
}

class TMemoryContextScope
{
public:
    explicit TMemoryContextScope(memory::CMemoryContext* const context) noexcept
        : m_previous(memory::set_thread_memory_context(context))
    {
    }

    TMemoryContextScope(const TMemoryContextScope&) = delete;
    TMemoryContextScope& operator=(const TMemoryContextScope&) = delete;

    ~TMemoryContextScope() noexcept
    {
        (void)memory::set_thread_memory_context(m_previous);
    }

private:
    memory::CMemoryContext* m_previous;
};

struct alignas(64) TTrackedPayload
{
    TTrackedPayload() noexcept
    {
        ++construction_count;
        ++live_count;
    }

    TTrackedPayload(TTrackedPayload&& other) noexcept
        : value(other.value)
    {
        ++construction_count;
        ++live_count;
        other.value = 0;
    }

    TTrackedPayload& operator=(TTrackedPayload&& other) noexcept
    {
        value = other.value;
        other.value = 0;
        return *this;
    }

    ~TTrackedPayload() noexcept
    {
        ++destruction_count;
        --live_count;
    }

    static void reset() noexcept
    {
        construction_count = 0;
        destruction_count = 0;
        live_count = 0;
    }

    int value{ 0 };

    static int construction_count;
    static int destruction_count;
    static int live_count;
};

int TTrackedPayload::construction_count = 0;
int TTrackedPayload::destruction_count = 0;
int TTrackedPayload::live_count = 0;

constexpr std::size_t k_payload_type_id = 0x101u;
constexpr std::size_t k_other_type_id = 0x102u;

void test_empty_and_cast_contract(TTestContext& ctx)
{
    memory::CTypeless typeless;
    const memory::CTypeless& const_typeless = typeless;

    static_assert(std::is_same_v<
        decltype(memory::typeless_cast<TTrackedPayload, k_payload_type_id>(typeless)),
        TTrackedPayload*>);
    static_assert(std::is_same_v<
        decltype(memory::typeless_cast<TTrackedPayload, k_payload_type_id>(const_typeless)),
        const TTrackedPayload*>);

    TEST_EXPECT(ctx, typeless.is_empty());
    TEST_EXPECT(ctx, !typeless.is_ready());
    TEST_EXPECT(ctx, !typeless);
    TEST_EXPECT(ctx, typeless.query_type_id() == 0u);
    TEST_EXPECT(ctx, (memory::typeless_cast<TTrackedPayload, k_payload_type_id>(typeless) == nullptr));

    typeless.destroy_and_deallocate();
    TEST_EXPECT(ctx, typeless.is_empty());
}

void test_creation_recovery_and_alignment(TTestContext& ctx)
{
    TTrackedPayload::reset();
    memory::CTypeless typeless = memory::CTypeless::create<TTrackedPayload, k_payload_type_id>();

    TEST_EXPECT(ctx, typeless.is_ready());
    TEST_EXPECT(ctx, !typeless.is_empty());
    TEST_EXPECT(ctx, !!typeless);
    TEST_EXPECT(ctx, typeless.query_type_id() == k_payload_type_id);
    TEST_EXPECT(ctx, TTrackedPayload::construction_count == 1);
    TEST_EXPECT(ctx, TTrackedPayload::live_count == 1);

    TTrackedPayload* const payload = memory::typeless_cast<TTrackedPayload, k_payload_type_id>(typeless);
    TEST_EXPECT(ctx, payload != nullptr);
    TEST_EXPECT(ctx, (reinterpret_cast<std::uintptr_t>(payload) & (alignof(TTrackedPayload) - 1u)) == 0u);
    TEST_EXPECT(ctx, (memory::typeless_cast<TTrackedPayload, k_other_type_id>(typeless) == nullptr));

    payload->value = 27;
    const memory::CTypeless& const_typeless = typeless;
    const TTrackedPayload* const const_payload =
        memory::typeless_cast<TTrackedPayload, k_payload_type_id>(const_typeless);
    TEST_EXPECT(ctx, const_payload != nullptr);
    TEST_EXPECT(ctx, const_payload->value == 27);
}

void test_move_and_destruction(TTestContext& ctx)
{
    TTrackedPayload::reset();
    memory::CTypeless source = memory::CTypeless::create<TTrackedPayload, k_payload_type_id>();
    memory::typeless_cast<TTrackedPayload, k_payload_type_id>(source)->value = 41;

    memory::CTypeless moved{ std::move(source) };
    TEST_EXPECT(ctx, source.is_empty());
    TEST_EXPECT(ctx, moved.is_ready());
    TEST_EXPECT(ctx, (memory::typeless_cast<TTrackedPayload, k_payload_type_id>(moved)->value == 41));
    TEST_EXPECT(ctx, TTrackedPayload::live_count == 1);

    memory::CTypeless destination = memory::CTypeless::create<TTrackedPayload, k_payload_type_id>();
    TEST_EXPECT(ctx, TTrackedPayload::live_count == 2);

    destination = std::move(moved);
    TEST_EXPECT(ctx, moved.is_empty());
    TEST_EXPECT(ctx, destination.is_ready());
    TEST_EXPECT(ctx, (memory::typeless_cast<TTrackedPayload, k_payload_type_id>(destination)->value == 41));
    TEST_EXPECT(ctx, TTrackedPayload::live_count == 1);
    TEST_EXPECT(ctx, TTrackedPayload::destruction_count == 1);

    destination.destroy_and_deallocate();
    TEST_EXPECT(ctx, destination.is_empty());
    TEST_EXPECT(ctx, TTrackedPayload::live_count == 0);
    TEST_EXPECT(ctx, TTrackedPayload::destruction_count == 2);
}

void test_context_accounting_and_failure(TTestContext& ctx)
{
    TAllocatorState state;
    memory::CMemoryAllocator allocator(&state, allocate_test_memory, deallocate_test_memory, system_ids::host);
    memory::CMemoryContext context(allocator, system_ids::host);
    const TMemoryContextScope context_scope(&context);

    using node_type = memory::TTypeless<TTrackedPayload, k_payload_type_id>;
    const std::size_t conditioned_alignment = context.condition_alignment(alignof(node_type));
    const std::size_t conditioned_bytes = context.condition_bytes(conditioned_alignment, sizeof(node_type));

    {
        memory::CTypeless typeless = memory::CTypeless::create<TTrackedPayload, k_payload_type_id>();
        TEST_EXPECT(ctx, typeless.is_ready());
        TEST_EXPECT(ctx, context.get_live_allocation_count() == 1u);
        TEST_EXPECT(ctx, context.get_live_allocated_bytes() == conditioned_bytes);
    }

    TEST_EXPECT(ctx, context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() == 0u);

    state.reject_allocation = true;
    (void)debug_utils::disable_asserts();
    memory::CTypeless rejected = memory::CTypeless::create<TTrackedPayload, k_payload_type_id>();
    (void)debug_utils::enable_asserts();

    TEST_EXPECT(ctx, rejected.is_empty());
    TEST_EXPECT(ctx, context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() == 0u);
}

}   //  namespace

int run_memory_typeless_tests()
{
    TTestContext ctx;
    test_empty_and_cast_contract(ctx);
    test_creation_recovery_and_alignment(ctx);
    test_move_and_destruction(ctx);
    test_context_accounting_and_failure(ctx);

    std::cout << "MemoryTypeless: " << ctx.passed << " passed, " << ctx.failed << " failed\n";
    return (ctx.failed == 0) ? 0 : 1;
}
