//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  TMpmcTransport_test_suite.cpp
//
//  Standalone test suite for threading::transports::TMpmc transport family.

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "threading/transports/TMpmcTransport.hpp"
#include "tests/TMpmcTransport_test_suite.hpp"

using threading::transports::EMpmcTransportStatus;
using threading::transports::TAcquiredArenaSlot;
using threading::transports::TMpmcArenaTransport;
using threading::transports::TMpmcIndexRing;
using threading::transports::TMpmcJobTransport;
using threading::transports::TReservedArenaSlot;

namespace tests
{

struct TTestContext
{
    std::uint32_t passed = 0u;
    std::uint32_t failed = 0u;

    void fail(const char* const expr,
        const char* const file,
        const int line,
        const std::string& message = {})
    {
        ++failed;
        std::cerr << file << '(' << line << "): FAIL: " << expr;
        if (!message.empty())
        {
            std::cerr << " : " << message;
        }
        std::cerr << '\n';
    }

    void pass() noexcept
    {
        ++passed;
    }

    [[nodiscard]] int exit_code() const noexcept
    {
        return (failed == 0u) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
};

#define TEST_EXPECT_TRUE(ctx, expr) \
    do { if (expr) { (ctx).pass(); } else { (ctx).fail(#expr, __FILE__, __LINE__); } } while (false)

#define TEST_EXPECT_FALSE(ctx, expr) \
    do { if (!(expr)) { (ctx).pass(); } else { (ctx).fail("!(" #expr ")", __FILE__, __LINE__); } } while (false)

#define TEST_EXPECT_EQ(ctx, lhs, rhs) \
    do { \
        const auto test_lhs_value = (lhs); \
        const auto test_rhs_value = (rhs); \
        if (test_lhs_value == test_rhs_value) { \
            (ctx).pass(); \
        } else { \
            (ctx).fail(#lhs " == " #rhs, __FILE__, __LINE__); \
        } \
    } while (false)

static void print_summary(const char* const suite_name, const TTestContext& ctx)
{
    std::cout
        << suite_name
        << ": passed=" << ctx.passed
        << " failed=" << ctx.failed
        << '\n';
}

void test_raw_ring_capacity_conditioning(TTestContext& ctx)
{
    TEST_EXPECT_EQ(ctx, TMpmcIndexRing<0u>::k_capacity, 16u);
    TEST_EXPECT_EQ(ctx, TMpmcIndexRing<1u>::k_capacity, 16u);
    TEST_EXPECT_EQ(ctx, TMpmcIndexRing<16u>::k_capacity, 16u);
    TEST_EXPECT_EQ(ctx, TMpmcIndexRing<17u>::k_capacity, 32u);
    TEST_EXPECT_EQ(ctx, TMpmcIndexRing<1000000u>::k_capacity, 1048576u);
}

void test_raw_ring_empty_and_full_start(TTestContext& ctx)
{
    TMpmcIndexRing<16u> empty_ring;
    TMpmcIndexRing<16u> full_ring(true);

    TEST_EXPECT_TRUE(ctx, empty_ring.is_valid());
    TEST_EXPECT_EQ(ctx, empty_ring.readable_count(), 0u);
    TEST_EXPECT_EQ(ctx, empty_ring.writable_count(), 16u);

    TEST_EXPECT_TRUE(ctx, full_ring.is_valid());
    TEST_EXPECT_EQ(ctx, full_ring.readable_count(), 16u);
    TEST_EXPECT_EQ(ctx, full_ring.writable_count(), 0u);

    std::uint32_t payload = 999u;
    std::uint32_t sequence = 999u;
    TEST_EXPECT_TRUE(ctx, full_ring.pop(payload, sequence));
    TEST_EXPECT_EQ(ctx, payload, 0u);
    TEST_EXPECT_EQ(ctx, sequence, 0u);
}

void test_raw_ring_sequence_identity_and_wrap(TTestContext& ctx)
{
    TMpmcIndexRing<16u> ring;
    std::uint32_t ignored_sequence = 0u;

    for (std::uint32_t value = 0u; value < 16u; ++value)
    {
        std::uint32_t push_sequence = 0u;
        TEST_EXPECT_TRUE(ctx, ring.push(100u + value, push_sequence));
        TEST_EXPECT_EQ(ctx, push_sequence, value);
    }

    TEST_EXPECT_FALSE(ctx, ring.push(999u, ignored_sequence));

    for (std::uint32_t value = 0u; value < 16u; ++value)
    {
        std::uint32_t payload = 0u;
        std::uint32_t pop_sequence = 0u;
        TEST_EXPECT_TRUE(ctx, ring.pop(payload, pop_sequence));
        TEST_EXPECT_EQ(ctx, payload, 100u + value);
        TEST_EXPECT_EQ(ctx, pop_sequence, value);
    }

    for (std::uint32_t value = 0u; value < 20u; ++value)
    {
        std::uint32_t push_sequence = 0u;
        std::uint32_t pop_sequence = 0u;
        std::uint32_t payload = 0u;

        TEST_EXPECT_TRUE(ctx, ring.push(200u + value, push_sequence));
        TEST_EXPECT_TRUE(ctx, ring.pop(payload, pop_sequence));
        TEST_EXPECT_EQ(ctx, payload, 200u + value);
        TEST_EXPECT_EQ(ctx, pop_sequence, push_sequence);
    }
}

void test_arena_transport_basic_pipeline(TTestContext& ctx)
{
    TMpmcArenaTransport<std::uint32_t, 16u> transport;
    std::uint32_t reserve_index = 0u;
    std::uint32_t reserve_sequence = 0u;
    std::uint32_t publish_sequence = 0u;
    std::uint32_t acquire_index = 0u;
    std::uint32_t acquire_sequence = 0u;
    std::uint32_t recycle_sequence = 0u;

    TEST_EXPECT_TRUE(ctx, transport.is_valid());
    TEST_EXPECT_TRUE(ctx, transport.is_open());
    TEST_EXPECT_EQ(ctx, transport.outstanding_count(), 0u);

    std::uint32_t* reserved = transport.reserve(reserve_index, reserve_sequence);
    TEST_EXPECT_TRUE(ctx, reserved != nullptr);
    TEST_EXPECT_EQ(ctx, reserve_index, 0u);
    TEST_EXPECT_EQ(ctx, reserve_sequence, 0u);
    TEST_EXPECT_EQ(ctx, transport.outstanding_count(), 1u);

    *reserved = 4242u;
    TEST_EXPECT_TRUE(ctx, transport.publish(reserved, publish_sequence));

    std::uint32_t* acquired = transport.acquire(acquire_index, acquire_sequence);
    TEST_EXPECT_TRUE(ctx, acquired != nullptr);
    TEST_EXPECT_EQ(ctx, acquire_index, reserve_index);
    TEST_EXPECT_EQ(ctx, *acquired, 4242u);

    TEST_EXPECT_TRUE(ctx, transport.recycle(acquired, recycle_sequence));
    TEST_EXPECT_EQ(ctx, transport.outstanding_count(), 0u);
    TEST_EXPECT_TRUE(ctx, transport.is_open());
}

void test_arena_transport_closing_and_closed(TTestContext& ctx)
{
    TMpmcArenaTransport<std::uint32_t, 16u> transport;
    std::uint32_t reserve_index = 0u;
    std::uint32_t reserve_sequence = 0u;
    std::uint32_t publish_sequence = 0u;
    std::uint32_t acquire_index = 0u;
    std::uint32_t acquire_sequence = 0u;
    std::uint32_t recycle_sequence = 0u;

    std::uint32_t* reserved = transport.reserve(reserve_index, reserve_sequence);
    TEST_EXPECT_TRUE(ctx, reserved != nullptr);
    *reserved = 7u;

    TEST_EXPECT_TRUE(ctx, transport.begin_closing());
    TEST_EXPECT_TRUE(ctx, transport.is_closing());
    TEST_EXPECT_TRUE(ctx, transport.reserve(reserve_index, reserve_sequence) == nullptr);

    TEST_EXPECT_TRUE(ctx, transport.publish(reserved, publish_sequence));
    std::uint32_t* acquired = transport.acquire(acquire_index, acquire_sequence);
    TEST_EXPECT_TRUE(ctx, acquired != nullptr);
    TEST_EXPECT_EQ(ctx, *acquired, 7u);
    TEST_EXPECT_TRUE(ctx, transport.recycle(acquired, recycle_sequence));

    TEST_EXPECT_TRUE(ctx, transport.is_closed());
    TEST_EXPECT_TRUE(ctx, transport.reserve(reserve_index, reserve_sequence) == nullptr);
    TEST_EXPECT_TRUE(ctx, transport.acquire(acquire_index, acquire_sequence) == nullptr);
    TEST_EXPECT_FALSE(ctx, transport.publish(acquired, publish_sequence));
    TEST_EXPECT_FALSE(ctx, transport.recycle(acquired, recycle_sequence));
}

void test_arena_transport_immediate_close_when_idle(TTestContext& ctx)
{
    TMpmcArenaTransport<std::uint32_t, 16u> transport;

    TEST_EXPECT_TRUE(ctx, transport.begin_closing());
    TEST_EXPECT_TRUE(ctx, transport.is_closed());
}

void test_arena_transport_shutdown(TTestContext& ctx)
{
    TMpmcArenaTransport<std::uint32_t, 16u> transport;
    std::uint32_t index = 0u;
    std::uint32_t sequence = 0u;

    transport.shutdown();

    TEST_EXPECT_TRUE(ctx, transport.is_shutdown());
    TEST_EXPECT_TRUE(ctx, transport.reserve(index, sequence) == nullptr);
    TEST_EXPECT_TRUE(ctx, transport.acquire(index, sequence) == nullptr);
}

void test_arena_transport_scoped_wrappers(TTestContext& ctx)
{
    TMpmcArenaTransport<std::uint32_t, 16u> transport;

    {
        TReservedArenaSlot<std::uint32_t, 16u> reserved(transport);
        TEST_EXPECT_TRUE(ctx, reserved.is_ready());
        *reserved = 55u;
    }

    {
        TAcquiredArenaSlot<std::uint32_t, 16u> acquired(transport);
        TEST_EXPECT_TRUE(ctx, acquired.is_ready());
        TEST_EXPECT_EQ(ctx, *acquired, 55u);
    }

    TEST_EXPECT_EQ(ctx, transport.outstanding_count(), 0u);
}

void test_job_transport_composition(TTestContext& ctx)
{
    TMpmcJobTransport<std::uint32_t, 16u, std::uint64_t, 32u> transport;
    TEST_EXPECT_TRUE(ctx, transport.is_valid());
    TEST_EXPECT_EQ(ctx, decltype(transport.work)::k_capacity, 16u);
    TEST_EXPECT_EQ(ctx, decltype(transport.feedback)::k_capacity, 32u);
}

int test_mpmc_transport()
{
    TTestContext ctx;

    test_raw_ring_capacity_conditioning(ctx);
    test_raw_ring_empty_and_full_start(ctx);
    test_raw_ring_sequence_identity_and_wrap(ctx);
    test_arena_transport_basic_pipeline(ctx);
    test_arena_transport_closing_and_closed(ctx);
    test_arena_transport_immediate_close_when_idle(ctx);
    test_arena_transport_shutdown(ctx);
    test_arena_transport_scoped_wrappers(ctx);
    test_job_transport_composition(ctx);

    print_summary("TMpmcTransport", ctx);
    return ctx.exit_code();
}

}   // namespace tests

int run_mpmc_transport_tests()
{
    return tests::test_mpmc_transport();
}
