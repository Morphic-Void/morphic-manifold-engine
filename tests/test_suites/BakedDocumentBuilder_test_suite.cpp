//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)

#include <cstring>
#include <iostream>
#include <type_traits>

#include "data_model/baked_document_builder.hpp"
#include "tests/support/test_context.hpp"

namespace
{
using TTestContext = tests::TTestContext;

CStringView text(const char* const value) noexcept { return CStringView{ value }; }

void test_bake_preserves_reachable_semantics(TTestContext& ctx)
{
    static_assert(sizeof(CBakedNode) == 32u);
    static_assert(std::is_trivially_copyable_v<CBakedNode>);

    CLiveDocument live;
    TEST_EXPECT(ctx, live.initialise());
    const CNodeKey root = live.create_object();
    const CNodeKey values = live.create_array();
    const CNodeKey first = live.create_integer(7);
    const CNodeKey second = live.create_string(text("two"));
    const CNodeKey enabled = live.create_boolean(true);
    const CNodeKey detached_string = live.create_string(text("discarded"));
    const CNodeKey detached_object_value = live.create_null();
    TEST_EXPECT(ctx, live.set_root(root));
    TEST_EXPECT(ctx, live.add_object_child(root, text("values"), values));
    TEST_EXPECT(ctx, live.add_object_child(root, text("enabled"), enabled));
    TEST_EXPECT(ctx, live.append_array_child(values, first));
    TEST_EXPECT(ctx, live.append_array_child(values, second));
    TEST_EXPECT(ctx, detached_string.is_valid());
    TEST_EXPECT(ctx, detached_object_value.is_valid());
    TEST_EXPECT(ctx, live.check_integrity());

    CBakedDocumentBuilder baked;
    TEST_EXPECT(ctx, baked.build_from(live));
    TEST_EXPECT(ctx, baked.is_ready());
    TEST_EXPECT(ctx, baked.check_integrity());
    TEST_EXPECT(ctx, baked.node_count() == 5u);
    TEST_EXPECT(ctx, baked.property_name_count() == 2u);
    TEST_EXPECT(ctx, baked.string_value_count() == 1u);

    const CBakedNodeIndex baked_root = baked.root();
    const CBakedNodeIndex baked_values = baked.object_child(baked_root, text("values"));
    const CBakedNodeIndex baked_enabled = baked.object_child(baked_root, text("enabled"));
    TEST_EXPECT(ctx, baked.node_type(baked_root) == EJsonNodeType::object);
    TEST_EXPECT(ctx, baked_values.is_valid());
    TEST_EXPECT(ctx, baked_enabled.is_valid());
    TEST_EXPECT(ctx, baked.parent(baked_values) == baked_root);
    TEST_EXPECT(ctx, baked.previous_sibling(baked_values) == CBakedNodeIndex{});
    TEST_EXPECT(ctx, baked.next_sibling(baked_values) == baked_enabled);
    TEST_EXPECT(ctx, baked.child_count(baked_values) == 2u);
    const CBakedNodeIndex baked_first = baked.array_at(baked_values, 0u);
    const CBakedNodeIndex baked_second = baked.array_at(baked_values, 1u);
    std::int64_t integer = 0;
    bool boolean = false;
    TEST_EXPECT(ctx, baked.integer_value(baked_first, integer) && (integer == 7));
    TEST_EXPECT(ctx, baked.string_value(baked_second).length() == 3u);
    TEST_EXPECT(ctx, std::memcmp(baked.string_value(baked_second).string(), "two", 3u) == 0);
    TEST_EXPECT(ctx, baked.boolean_value(baked_enabled, boolean) && boolean);
}

void test_bake_is_atomic_and_rejects_invalid_source(TTestContext& ctx)
{
    CLiveDocument valid;
    TEST_EXPECT(ctx, valid.initialise());
    const CNodeKey valid_root = valid.create_array();
    TEST_EXPECT(ctx, valid.set_root(valid_root));

    CBakedDocumentBuilder baked;
    TEST_EXPECT(ctx, baked.build_from(valid));
    const CBakedNodeIndex old_root = baked.root();
    const std::uint32_t old_count = baked.node_count();

    CLiveDocument rootless;
    TEST_EXPECT(ctx, rootless.initialise());
    TEST_EXPECT(ctx, rootless.create_null().is_valid());
    TEST_EXPECT(ctx, !baked.build_from(rootless));
    TEST_EXPECT(ctx, baked.root() == old_root);
    TEST_EXPECT(ctx, baked.node_count() == old_count);
    TEST_EXPECT(ctx, baked.check_integrity());

    CLiveDocument unready;
    TEST_EXPECT(ctx, !baked.build_from(unready));
    TEST_EXPECT(ctx, baked.root() == old_root);
    TEST_EXPECT(ctx, baked.check_integrity());
}
}

int run_baked_document_builder_tests()
{
    TTestContext ctx;
    test_bake_preserves_reachable_semantics(ctx);
    test_bake_is_atomic_and_rejects_invalid_source(ctx);
    std::cout << "BakedDocumentBuilder: " << ctx.passed << " passed, " << ctx.failed << " failed\n";
    return ctx.failed;
}
