//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)

#include <cstring>
#include <iostream>
#include <type_traits>

#include "data_model/live_document.hpp"
#include "tests/support/test_context.hpp"

namespace
{
using TTestContext = tests::TTestContext;

CStringView text(const char* const value) noexcept
{
    return CStringView{ value };
}

void test_layout_and_scalars(TTestContext& ctx)
{
    static_assert(sizeof(CJsonSlot) == 64u);
    static_assert(std::is_trivially_copyable_v<CJsonSlot>);

    CLiveDocument document;
    TEST_EXPECT(ctx, document.initialise(4u));
    const CNodeKey null_node = document.create_null();
    const CNodeKey bool_node = document.create_boolean(true);
    const CNodeKey integer_node = document.create_integer(-37);
    const CNodeKey float_node = document.create_floating_point(2.5);
    const CNodeKey string_node = document.create_string(text("value"));
    TEST_EXPECT(ctx, null_node.is_valid());
    TEST_EXPECT(ctx, bool_node.is_valid());
    TEST_EXPECT(ctx, integer_node.is_valid());
    TEST_EXPECT(ctx, float_node.is_valid());
    TEST_EXPECT(ctx, string_node.is_valid());
    TEST_EXPECT(ctx, document.node_type(null_node) == EJsonNodeType::null_value);
    bool bool_value = false;
    std::int64_t integer_value = 0;
    double float_value = 0.0;
    TEST_EXPECT(ctx, document.boolean_value(bool_node, bool_value) && bool_value);
    TEST_EXPECT(ctx, document.integer_value(integer_node, integer_value) && (integer_value == -37));
    TEST_EXPECT(ctx, document.floating_point_value(float_node, float_value) && (float_value == 2.5));
    TEST_EXPECT(ctx, document.string_value(string_node).length() == 5u);
    TEST_EXPECT(ctx, std::memcmp(document.string_value(string_node).string(), "value", 5u) == 0);
    TEST_EXPECT(ctx, document.check_integrity());
}

void test_array_mutation_and_cursor(TTestContext& ctx)
{
    CLiveDocument document;
    TEST_EXPECT(ctx, document.initialise());
    const CNodeKey array = document.create_array();
    const CNodeKey first = document.create_integer(1);
    const CNodeKey last = document.create_integer(3);
    const CNodeKey middle = document.create_integer(2);
    TEST_EXPECT(ctx, document.set_root(array));
    TEST_EXPECT(ctx, document.append_array_child(array, first));
    TEST_EXPECT(ctx, document.append_array_child(array, last));
    TEST_EXPECT(ctx, document.insert_array_child_before(array, last, middle));
    TEST_EXPECT(ctx, document.child_count(array) == 3u);
    TEST_EXPECT(ctx, document.array_at(array, 0u) == first);
    TEST_EXPECT(ctx, document.array_at(array, 1u) == middle);
    TEST_EXPECT(ctx, document.array_at(array, 2u) == last);
    TEST_EXPECT(ctx, !document.previous_sibling(first).is_valid());
    TEST_EXPECT(ctx, !document.next_sibling(last).is_valid());

    CArrayCursor cursor;
    TEST_EXPECT(ctx, document.array_cursor_at(array, 0u, cursor));
    TEST_EXPECT(ctx, cursor.current == first);
    TEST_EXPECT(ctx, document.array_cursor_next(cursor));
    TEST_EXPECT(ctx, cursor.current == middle);
    TEST_EXPECT(ctx, document.detach(middle));
    TEST_EXPECT(ctx, !document.array_cursor_next(cursor));
    TEST_EXPECT(ctx, document.child_count(array) == 2u);
    TEST_EXPECT(ctx, document.first_child(array) == first);
    TEST_EXPECT(ctx, document.last_child(array) == last);
    TEST_EXPECT(ctx, document.parent(middle) == CNodeKey{});
    TEST_EXPECT(ctx, document.check_integrity());
}

void test_object_names_and_rejected_moves(TTestContext& ctx)
{
    CLiveDocument document;
    TEST_EXPECT(ctx, document.initialise());
    const CNodeKey object = document.create_object();
    const CNodeKey enabled = document.create_boolean(true);
    const CNodeKey duplicate = document.create_boolean(false);
    const CNodeKey array = document.create_array();
    TEST_EXPECT(ctx, document.set_root(object));
    TEST_EXPECT(ctx, document.add_object_child(object, text("enabled"), enabled));
    TEST_EXPECT(ctx, !document.add_object_child(object, text("enabled"), duplicate));
    TEST_EXPECT(ctx, !document.append_array_child(array, enabled));
    const CPropertyNameId enabled_name = document.intern_property_name(text("enabled"));
    TEST_EXPECT(ctx, document.object_child(object, enabled_name) == enabled);
    TEST_EXPECT(ctx, document.child_count(object) == 1u);
    TEST_EXPECT(ctx, document.parent(duplicate) == CNodeKey{});
    TEST_EXPECT(ctx, document.check_integrity());
}

void test_stale_key_rejection(TTestContext& ctx)
{
    CLiveDocument document;
    TEST_EXPECT(ctx, document.initialise(1u));
    const CNodeKey stale = document.create_null();
    TEST_EXPECT(ctx, stale.is_valid());
    TEST_EXPECT(ctx, document.erase_detached(stale));
    const CNodeKey replacement = document.create_null();
    TEST_EXPECT(ctx, replacement.is_valid());
    TEST_EXPECT(ctx, replacement != stale);
    TEST_EXPECT(ctx, document.node_type(stale) == EJsonNodeType::invalid);
    TEST_EXPECT(ctx, document.node_type(replacement) == EJsonNodeType::null_value);
    TEST_EXPECT(ctx, document.check_integrity());
}
}

int run_live_document_tests()
{
    TTestContext ctx;
    test_layout_and_scalars(ctx);
    test_array_mutation_and_cursor(ctx);
    test_object_names_and_rejected_moves(ctx);
    test_stale_key_rejection(ctx);
    std::cout << "LiveDocument: " << ctx.passed << " passed, " << ctx.failed << " failed\n";
    return ctx.failed;
}
