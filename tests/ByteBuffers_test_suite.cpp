
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   ByteBuffers_test_suite.cpp
//  Primary implementation: OpenAI tools
//  Used, occasionally adjusted, and accepted by: Ritchie Brannan
//  Date:   14 Jul 26

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <utility>

#include "containers/ByteBuffers.hpp"
#include "memory/memory_token.hpp"
#include "memory/memory_view.hpp"
#include "tests/ByteBuffers_test_suite.hpp"
#include "tests/support/test_context.hpp"

namespace
{

using TTestContext = tests::TTestContext;

void test_byte_buffer_constness_and_limits(TTestContext& ctx)
{
    static_assert(std::is_same_v<decltype(std::declval<const CByteBuffer&>().data()), std::uint8_t*>);
    static_assert(std::is_same_v<decltype(std::declval<const CByteBuffer&>().view()), CByteView>);
    static_assert(std::is_same_v<decltype(std::declval<const CByteView&>().data()), std::uint8_t*>);
    static_assert(std::is_same_v<decltype(std::declval<const CByteConstView&>().data()), const std::uint8_t*>);
    static_assert(std::is_same_v<decltype(std::declval<const CByteRectBuffer&>().view()), CByteRectView>);
    static_assert(std::is_same_v<decltype(std::declval<const CByteRectView&>().data()), std::uint8_t*>);
    static_assert(std::is_same_v<decltype(std::declval<const CByteRectConstView&>().data()), const std::uint8_t*>);

    CByteBuffer buffer;
    TEST_EXPECT(ctx, buffer.is_valid());
    TEST_EXPECT(ctx, buffer.is_empty());
    TEST_EXPECT(ctx, !buffer.is_ready());
    TEST_EXPECT(ctx, !buffer.view().is_valid());
    TEST_EXPECT(ctx, !buffer.const_view().is_valid());
    TEST_EXPECT(ctx, !buffer.allocate(memory::k_byte_size_ceiling + 1u));
}

void test_byte_buffer_allocation_and_resize(TTestContext& ctx)
{
    CByteBuffer buffer;
    const std::uint8_t bytes[]{ 1u, 2u, 3u, 4u };

    TEST_EXPECT(ctx, buffer.allocate(8u, 16u));
    TEST_EXPECT(ctx, buffer.is_ready());
    TEST_EXPECT(ctx, buffer.size() == 0u);
    TEST_EXPECT(ctx, buffer.capacity() == 8u);
    TEST_EXPECT(ctx, buffer.align() == 16u);
    TEST_EXPECT(ctx, !buffer.view().is_valid());

    TEST_EXPECT(ctx, buffer.append(bytes, 4u));
    TEST_EXPECT(ctx, buffer.size() == 4u);
    TEST_EXPECT(ctx, std::memcmp(buffer.data(), bytes, 4u) == 0);

    TEST_EXPECT(ctx, buffer.reserve(20u));
    TEST_EXPECT(ctx, buffer.capacity() >= 20u);
    TEST_EXPECT(ctx, std::memcmp(buffer.data(), bytes, 4u) == 0);

    TEST_EXPECT(ctx, buffer.resize(6u));
    TEST_EXPECT(ctx, buffer.size() == 6u);
    TEST_EXPECT(ctx, buffer.data()[4] == 0u);
    TEST_EXPECT(ctx, buffer.data()[5] == 0u);

    TEST_EXPECT(ctx, buffer.set_size(2u));
    TEST_EXPECT(ctx, buffer.size() == 2u);
    TEST_EXPECT(ctx, buffer.shrink_to_fit());
    TEST_EXPECT(ctx, buffer.capacity() == 2u);
    TEST_EXPECT(ctx, buffer.size() == 2u);
    TEST_EXPECT(ctx, buffer.data()[0] == 1u);
    TEST_EXPECT(ctx, buffer.data()[1] == 2u);
}

void test_byte_views_and_slicing(TTestContext& ctx)
{
    alignas(16) std::uint8_t storage[]{ 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u };

    CByteView empty_view;
    TEST_EXPECT(ctx, !empty_view.is_valid());
    TEST_EXPECT(ctx, empty_view.is_empty());
    TEST_EXPECT(ctx, (!CByteView{ storage, 0u, 16u }.is_valid()));
    TEST_EXPECT(ctx, (!CByteConstView{ storage, 0u, 16u }.is_valid()));

    CByteView view{ storage, 8u, 16u };
    TEST_EXPECT(ctx, view.is_valid());
    TEST_EXPECT(ctx, view.size() == 8u);
    TEST_EXPECT(ctx, view.align() == 16u);

    CByteView middle = view.subview(2u, 4u);
    TEST_EXPECT(ctx, middle.is_valid());
    TEST_EXPECT(ctx, middle.data() == (storage + 2u));
    TEST_EXPECT(ctx, middle.size() == 4u);
    TEST_EXPECT(ctx, middle.align() == 2u);

    middle.data()[1] = 99u;
    TEST_EXPECT(ctx, storage[3] == 99u);
    TEST_EXPECT(ctx, view.head_to(3u).size() == 3u);
    TEST_EXPECT(ctx, view.tail_from(5u).size() == 3u);
    TEST_EXPECT(ctx, !view.head_to(0u).is_valid());
    TEST_EXPECT(ctx, !view.subview(7u, 2u).is_valid());
    TEST_EXPECT(ctx, !view.tail_from(8u).is_valid());

    const CByteConstView const_view = view.const_view();
    TEST_EXPECT(ctx, const_view.is_valid());
    TEST_EXPECT(ctx, const_view.data() == storage);
    TEST_EXPECT(ctx, const_view.subview(1u, 3u).data() == (storage + 1u));

    memory::CMemoryView raw_view{ storage, 8u, 1u, 16u };
    CByteView adopted{ raw_view, MetaByteView{ 6u } };
    TEST_EXPECT(ctx, adopted.is_valid());
    TEST_EXPECT(ctx, adopted.size() == 6u);
    TEST_EXPECT(ctx, adopted.data() == storage);
    TEST_EXPECT(ctx, (!CByteView{ raw_view, MetaByteView{ 9u } }.is_valid()));
}

void test_byte_buffer_move_and_disown(TTestContext& ctx)
{
    CByteBuffer source;
    const std::uint8_t bytes[]{ 21u, 22u, 23u, 24u };

    TEST_EXPECT(ctx, source.reallocate(4u, 8u, 8u));
    TEST_EXPECT(ctx, std::memcpy(source.data(), bytes, 4u) == source.data());

    CByteBuffer moved{ std::move(source) };
    TEST_EXPECT(ctx, moved.is_ready());
    TEST_EXPECT(ctx, moved.size() == 4u);
    TEST_EXPECT(ctx, moved.capacity() == 8u);
    TEST_EXPECT(ctx, std::memcmp(moved.data(), bytes, 4u) == 0);
    TEST_EXPECT(ctx, source.is_valid());
    TEST_EXPECT(ctx, source.is_empty());
    TEST_EXPECT(ctx, source.allocate(2u));

    MetaByteBuffer meta;
    memory::CMemoryToken token = moved.disown(meta);
    TEST_EXPECT(ctx, meta.size == 4u);
    TEST_EXPECT(ctx, meta.capacity == 8u);
    TEST_EXPECT(ctx, token.is_relocatable());
    TEST_EXPECT(ctx, token.count() == 8u);
    TEST_EXPECT(ctx, token.storage_alignment() == 8u);
    TEST_EXPECT(ctx, std::memcmp(token.data(), bytes, 4u) == 0);
    TEST_EXPECT(ctx, moved.is_valid());
    TEST_EXPECT(ctx, moved.is_empty());
    TEST_EXPECT(ctx, moved.append(bytes, 2u));
    TEST_EXPECT(ctx, moved.size() == 2u);
}

void test_byte_rect_layout_and_contiguity(TTestContext& ctx)
{
    CByteRectBuffer rect;
    TEST_EXPECT(ctx, rect.is_valid());
    TEST_EXPECT(ctx, rect.is_empty());
    TEST_EXPECT(ctx, !rect.is_ready());

    TEST_EXPECT(ctx, rect.allocate(5u, 3u, 8u, false));
    TEST_EXPECT(ctx, rect.is_ready());
    TEST_EXPECT(ctx, rect.row_pitch() == 8u);
    TEST_EXPECT(ctx, rect.row_width() == 5u);
    TEST_EXPECT(ctx, rect.row_count() == 3u);
    TEST_EXPECT(ctx, rect.align() == 8u);
    TEST_EXPECT(ctx, !rect.is_contiguous());
    TEST_EXPECT(ctx, !rect.byte_view().is_valid());

    std::memset(rect.data(), 0xff, rect.row_pitch() * rect.row_count());
    rect.zero_fill();
    for (std::size_t y = 0u; y < rect.row_count(); ++y)
    {
        TEST_EXPECT(ctx, rect.row_data(y) == (rect.data() + (y * rect.row_pitch())));
        for (std::size_t x = 0u; x < rect.row_width(); ++x)
        {
            TEST_EXPECT(ctx, rect.row_data(y)[x] == 0u);
        }
        for (std::size_t x = rect.row_width(); x < rect.row_pitch(); ++x)
        {
            TEST_EXPECT(ctx, rect.row_data(y)[x] == 0xffu);
        }
    }

    CByteRectView sub = rect.view().subview(1u, 1u, 3u, 2u);
    TEST_EXPECT(ctx, sub.is_valid());
    TEST_EXPECT(ctx, sub.row_pitch() == 8u);
    TEST_EXPECT(ctx, sub.row_width() == 3u);
    TEST_EXPECT(ctx, sub.row_count() == 2u);
    TEST_EXPECT(ctx, sub.data() == (rect.row_data(1u) + 1u));
    TEST_EXPECT(ctx, !rect.view().subview(4u, 2u, 2u, 2u).is_valid());

    TEST_EXPECT(ctx, rect.reallocate(4u, 2u, 1u, true));
    TEST_EXPECT(ctx, rect.is_contiguous());
    TEST_EXPECT(ctx, rect.row_pitch() == 4u);
    TEST_EXPECT(ctx, rect.byte_view().is_valid());
    TEST_EXPECT(ctx, rect.byte_view().size() == 8u);
}

void test_byte_rect_copy_and_view_adoption(TTestContext& ctx)
{
    alignas(8) std::uint8_t storage[10u]{ 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u };

    CByteRectConstView src{ storage, 5u, 5u, 2u, 8u };
    TEST_EXPECT(ctx, src.is_valid());
    TEST_EXPECT(ctx, src.is_contiguous());

    CByteRectBuffer copied;
    TEST_EXPECT(ctx, copied.construct_and_copy_from(src));
    TEST_EXPECT(ctx, copied.is_ready());
    TEST_EXPECT(ctx, copied.row_width() == 5u);
    TEST_EXPECT(ctx, copied.row_count() == 2u);
    TEST_EXPECT(ctx, copied.row_pitch() == 8u);
    TEST_EXPECT(ctx, copied.align() == 8u);
    TEST_EXPECT(ctx, !copied.is_contiguous());
    TEST_EXPECT(ctx, std::memcmp(copied.row_data(0u), storage, 5u) == 0);
    TEST_EXPECT(ctx, std::memcmp(copied.row_data(1u), storage + 5u, 5u) == 0);
    TEST_EXPECT(ctx, copied.row_data(0u)[5] == 0u);
    TEST_EXPECT(ctx, copied.row_data(0u)[6] == 0u);
    TEST_EXPECT(ctx, copied.row_data(0u)[7] == 0u);

    memory::CMemoryView raw_rect{ storage, 10u, 1u, 8u };
    CByteRectView adopted{ raw_rect, MetaByteRectView{ 5u, 5u, 2u } };
    TEST_EXPECT(ctx, adopted.is_valid());
    TEST_EXPECT(ctx, adopted.row_pitch() == 5u);
    TEST_EXPECT(ctx, adopted.row_width() == 5u);
    TEST_EXPECT(ctx, adopted.row_count() == 2u);
    TEST_EXPECT(ctx, adopted.is_contiguous());
}

}   //  namespace

int run_byte_buffer_tests()
{
    TTestContext ctx;
    test_byte_buffer_constness_and_limits(ctx);
    test_byte_buffer_allocation_and_resize(ctx);
    test_byte_views_and_slicing(ctx);
    test_byte_buffer_move_and_disown(ctx);
    test_byte_rect_layout_and_contiguity(ctx);
    test_byte_rect_copy_and_view_adoption(ctx);

    std::cout << "ByteBuffers: " << ctx.passed << " passed, " << ctx.failed << " failed\n";
    return (ctx.failed == 0) ? 0 : 1;
}

