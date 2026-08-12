
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   ErasedOwner_test_suite.cpp

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <new>
#include <type_traits>
#include <utility>

#include "debug/macros.hpp"
#include "memory/memory_context.hpp"
#include "system/erased_owner.hpp"
#include "system/erased_owner_transport.hpp"
#include "system/transported_types.hpp"
#include "tests/ErasedOwner_test_suite.hpp"
#include "threading/messages/CErasedMessageTransports.hpp"
#include "threading/transports/TOwningTransport.hpp"

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
            std::cerr << "ErasedOwner test failure at line " << line << ": " << expression << '\n';
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

void test_registration_and_empty_state(TTestContext& ctx)
{
    static_assert(k_system_type_id_v<CByteBuffer> == system_type_ids::byte_buffer);
    static_assert(k_system_type_id_v<CByteRectBuffer> == system_type_ids::byte_rect_buffer);
    static_assert(k_system_type_id_v<CSimpleString> == system_type_ids::simple_string);
    static_assert(k_system_type_id_v<CStringBuffer> == system_type_ids::string_buffer);
    static_assert(k_system_type_id_v<CStableStrings> == system_type_ids::stable_strings);
    static_assert(k_is_erased_owner_payload_v<LoadedFile>);
    static_assert(!k_is_erased_owner_payload_v<FileLoadRequest>);
    static_assert(std::is_same_v<decltype(CErasedOwner{}.query_type_id()), system_type_id>);

    CErasedOwner owner;
    const CErasedOwner& const_owner = owner;

    TEST_EXPECT(ctx, owner.is_empty());
    TEST_EXPECT(ctx, !owner.is_ready());
    TEST_EXPECT(ctx, !owner);
    TEST_EXPECT(ctx, owner.query_type_id() == system_type_ids::undefined);
    TEST_EXPECT(ctx, owner.payload<LoadedFile>() == nullptr);
    TEST_EXPECT(ctx, const_owner.payload<LoadedFile>() == nullptr);
    TEST_EXPECT(ctx, !owner.has_any_hazard());
    TEST_EXPECT(ctx, owner.hazard_mask() == 0u);

    owner.destroy();
    TEST_EXPECT(ctx, owner.is_empty());
}

void test_creation_accounting_and_destruction(TTestContext& ctx)
{
    TAllocatorState state;
    memory::CMemoryAllocator allocator(
        &state, allocate_test_memory, deallocate_test_memory, system_ids::host);
    memory::CMemoryContext context(allocator, system_ids::host);
    const TMemoryContextScope context_scope(&context);

    {
        CErasedOwner owner = CErasedOwner::create<LoadedFile>();
        LoadedFile* const payload = owner.payload<LoadedFile>();

        TEST_EXPECT(ctx, owner.is_ready());
        TEST_EXPECT(ctx, owner.query_type_id() == k_system_type_id_v<LoadedFile>);
        TEST_EXPECT(ctx, payload != nullptr);
        TEST_EXPECT(ctx, owner.payload<EncodedTga>() == nullptr);
        TEST_EXPECT(ctx, context.get_live_allocation_count() == 1u);

        TEST_EXPECT(ctx, payload->buffer.allocate(64u, 16u));
        TEST_EXPECT(ctx, context.get_live_allocation_count() == 2u);
    }

    TEST_EXPECT(ctx, context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, context.get_live_allocated_bytes() == 0u);
}

void test_moves_and_hazards(TTestContext& ctx)
{
    CErasedOwner source = CErasedOwner::create<LoadedFile>();
    LoadedFile* const original_payload = source.payload<LoadedFile>();
    TEST_EXPECT(ctx, original_payload->buffer.allocate(29u));
    source.add_hazard(mount_point_ids::executable);
    source.add_hazard(mount_point_ids::render);

    CErasedOwner moved{ std::move(source) };
    TEST_EXPECT(ctx, source.is_empty());
    TEST_EXPECT(ctx, !source.has_any_hazard());
    TEST_EXPECT(ctx, moved.payload<LoadedFile>() == original_payload);
    TEST_EXPECT(ctx, moved.has_hazard(mount_point_ids::executable));
    TEST_EXPECT(ctx, moved.has_hazard(mount_point_ids::render));
    TEST_EXPECT(ctx, moved.payload<LoadedFile>()->buffer.is_ready());

    CErasedOwner destination = CErasedOwner::create<EncodedTga>();
    destination = std::move(moved);
    TEST_EXPECT(ctx, moved.is_empty());
    TEST_EXPECT(ctx, destination.payload<LoadedFile>() == original_payload);

    destination.remove_hazard(mount_point_ids::render);
    TEST_EXPECT(ctx, destination.has_hazard(mount_point_ids::executable));
    TEST_EXPECT(ctx, !destination.has_hazard(mount_point_ids::render));
    TEST_EXPECT(ctx, destination.has_any_hazard());
}

void test_allocation_failure_is_canonical(TTestContext& ctx)
{
    TAllocatorState state;
    state.reject_allocation = true;
    memory::CMemoryAllocator allocator(
        &state, allocate_test_memory, deallocate_test_memory, system_ids::host);
    memory::CMemoryContext context(allocator, system_ids::host);

    CErasedOwner owner = CErasedOwner::create<LoadedFile>(&context);

    TEST_EXPECT(ctx, owner.is_empty());
    TEST_EXPECT(ctx, !owner.is_ready());
    TEST_EXPECT(ctx, owner.query_type_id() == system_type_ids::undefined);
    TEST_EXPECT(ctx, owner.hazard_mask() == 0u);
    TEST_EXPECT(ctx, context.get_live_allocation_count() == 0u);
}

void test_owner_reattribution(TTestContext& ctx)
{
    TAllocatorState state;
    memory::CMemoryAllocator allocator(
        &state, allocate_test_memory, deallocate_test_memory, system_ids::host);
    memory::CMemoryAllocator incompatible_allocator(
        &state, allocate_test_memory, deallocate_test_memory, system_ids::host);
    memory::CMemoryContext source_context(allocator, system_ids::host);
    memory::CMemoryContext target_context(allocator, system_ids::host);
    memory::CMemoryContext incompatible_context(incompatible_allocator, system_ids::host);
    const TMemoryContextScope context_scope(&source_context);

    CErasedOwner owner = CErasedOwner::create<LoadedFile>();
    LoadedFile* const payload = owner.payload<LoadedFile>();
    TEST_EXPECT(ctx, payload->buffer.allocate(64u, 16u));
    owner.add_hazard(mount_point_ids::asset);

    const std::uint32_t allocation_count = source_context.get_live_allocation_count();
    const std::uint64_t allocation_size = source_context.get_live_allocated_bytes();
    TEST_EXPECT(ctx, allocation_count == 2u);
    TEST_EXPECT(ctx, allocation_size != 0u);
    TEST_EXPECT(ctx, owner.memory_context() == &source_context);
    TEST_EXPECT(ctx, owner.can_reattribute_to(&target_context));
    TEST_EXPECT(ctx, !owner.can_reattribute_to(&incompatible_context));
    TEST_EXPECT(ctx, !owner.reattribute(&incompatible_context));
    TEST_EXPECT(ctx, source_context.get_live_allocation_count() == allocation_count);
    TEST_EXPECT(ctx, target_context.get_live_allocation_count() == 0u);

    TEST_EXPECT(ctx, owner.reattribute(&target_context));
    TEST_EXPECT(ctx, owner.memory_context() == &target_context);
    TEST_EXPECT(ctx, owner.payload<LoadedFile>() == payload);
    TEST_EXPECT(ctx, owner.has_hazard(mount_point_ids::asset));
    TEST_EXPECT(ctx, source_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, source_context.get_live_allocated_bytes() == 0u);
    TEST_EXPECT(ctx, target_context.get_live_allocation_count() == allocation_count);
    TEST_EXPECT(ctx, target_context.get_live_allocated_bytes() == allocation_size);

    owner.destroy();
    TEST_EXPECT(ctx, target_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, target_context.get_live_allocated_bytes() == 0u);

    CErasedOwner empty;
    TEST_EXPECT(ctx, empty.reattribute(&target_context));
    TEST_EXPECT(ctx, empty.is_empty());
    TEST_EXPECT(ctx, empty.memory_context() == nullptr);
}

void test_all_registered_payload_reattribution(TTestContext& ctx)
{
    TAllocatorState state;
    memory::CMemoryAllocator allocator(
        &state, allocate_test_memory, deallocate_test_memory, system_ids::host);
    memory::CMemoryContext source_context(allocator, system_ids::host);
    memory::CMemoryContext target_context(allocator, system_ids::host);
    const TMemoryContextScope context_scope(&source_context);

    CErasedOwner encoded = CErasedOwner::create<EncodedTga>();
    TEST_EXPECT(ctx, encoded.payload<EncodedTga>()->buffer.allocate(32u));
    TEST_EXPECT(ctx, encoded.reattribute(&target_context));
    TEST_EXPECT(ctx, encoded.memory_context() == &target_context);
    encoded.destroy();

    CErasedOwner decoded = CErasedOwner::create<DecodedTga>();
    TEST_EXPECT(ctx, decoded.payload<DecodedTga>()->buffer.allocate(8u, 4u));
    TEST_EXPECT(ctx, decoded.reattribute(&target_context));
    TEST_EXPECT(ctx, decoded.memory_context() == &target_context);
    decoded.destroy();

    TEST_EXPECT(ctx, source_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, target_context.get_live_allocation_count() == 0u);
}

void test_existing_owning_transport(TTestContext& ctx)
{
    threading::transports::TOwning<CErasedOwner> transport;
    TEST_EXPECT(ctx, transport.initialise(32u));

    CErasedOwner posted = CErasedOwner::create<LoadedFile>();
    LoadedFile* const original_payload = posted.payload<LoadedFile>();
    TEST_EXPECT(ctx, original_payload->buffer.allocate(43u));
    posted.add_hazard(mount_point_ids::conditioning);

    TEST_EXPECT(ctx, transport.post(std::move(posted)));
    TEST_EXPECT(ctx, posted.is_empty());

    CErasedOwner received;
    TEST_EXPECT(ctx, transport.read(received));
    TEST_EXPECT(ctx, received.payload<LoadedFile>() == original_payload);
    TEST_EXPECT(ctx, received.payload<LoadedFile>()->buffer.is_ready());
    TEST_EXPECT(ctx, received.has_hazard(mount_point_ids::conditioning));

    transport.deallocate();
}

void test_erased_owner_transport_attribution(TTestContext& ctx)
{
    TAllocatorState state;
    memory::CMemoryAllocator allocator(
        &state, allocate_test_memory, deallocate_test_memory, system_ids::host);
    memory::CMemoryContext producer_context(allocator, system_ids::host);
    memory::CMemoryContext transport_context(allocator, system_ids::host);
    memory::CMemoryContext recipient_context(allocator, system_ids::host);

    threading::transports::CErasedOwnerTransport transport(
        &transport_context, &recipient_context);
    threading::transports::CErasedOwnerProducerEndpoint producer(transport);
    threading::transports::CErasedOwnerConsumerEndpoint consumer(transport);

    TEST_EXPECT(ctx, transport.initialise(32u));
    TEST_EXPECT(ctx, transport.memory_context() == &transport_context);
    TEST_EXPECT(ctx, transport.recipient_memory_context() == &recipient_context);
    TEST_EXPECT(ctx, producer.is_valid());
    TEST_EXPECT(ctx, consumer.is_valid());
    TEST_EXPECT(ctx, transport_context.get_live_allocation_count() == 1u);

    const TMemoryContextScope context_scope(&producer_context);
    CErasedOwner posted = CErasedOwner::create<LoadedFile>();
    LoadedFile* const payload = posted.payload<LoadedFile>();
    TEST_EXPECT(ctx, payload->buffer.allocate(48u));
    posted.add_hazard(mount_point_ids::conditioning);

    TEST_EXPECT(ctx, producer_context.get_live_allocation_count() == 2u);
    TEST_EXPECT(ctx, producer.post(std::move(posted)));
    TEST_EXPECT(ctx, posted.is_empty());
    TEST_EXPECT(ctx, producer_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, transport_context.get_live_allocation_count() == 3u);

    CErasedOwner received;
    TEST_EXPECT(ctx, consumer.read(received));
    TEST_EXPECT(ctx, received.payload<LoadedFile>() == payload);
    TEST_EXPECT(ctx, received.has_hazard(mount_point_ids::conditioning));
    TEST_EXPECT(ctx, received.memory_context() == &recipient_context);
    TEST_EXPECT(ctx, transport_context.get_live_allocation_count() == 1u);
    TEST_EXPECT(ctx, recipient_context.get_live_allocation_count() == 2u);

    received.destroy();
    transport.deallocate();
    TEST_EXPECT(ctx, transport_context.get_live_allocation_count() == 0u);
    TEST_EXPECT(ctx, recipient_context.get_live_allocation_count() == 0u);
}

void test_erased_owner_transport_rejection(TTestContext& ctx)
{
    TAllocatorState state;
    memory::CMemoryAllocator transport_allocator(
        &state, allocate_test_memory, deallocate_test_memory, system_ids::host);
    memory::CMemoryAllocator incompatible_allocator(
        &state, allocate_test_memory, deallocate_test_memory, system_ids::host);
    memory::CMemoryContext transport_context(transport_allocator, system_ids::host);
    memory::CMemoryContext incompatible_context(incompatible_allocator, system_ids::host);

    threading::transports::CErasedOwnerTransport transport(&transport_context);
    CErasedOwner unready_owner = CErasedOwner::create<LoadedFile>(&incompatible_context);
    LoadedFile* const unready_payload = unready_owner.payload<LoadedFile>();
    TEST_EXPECT(ctx, !transport.post(std::move(unready_owner)));
    TEST_EXPECT(ctx, unready_owner.payload<LoadedFile>() == unready_payload);
    TEST_EXPECT(ctx, unready_owner.memory_context() == &incompatible_context);

    TEST_EXPECT(ctx, transport.initialise(32u));
    CErasedOwner incompatible_owner =
        CErasedOwner::create<LoadedFile>(&incompatible_context);
    LoadedFile* const incompatible_payload = incompatible_owner.payload<LoadedFile>();
    TEST_EXPECT(ctx, !transport.post(std::move(incompatible_owner)));
    TEST_EXPECT(ctx, incompatible_owner.payload<LoadedFile>() == incompatible_payload);
    TEST_EXPECT(ctx, incompatible_owner.memory_context() == &incompatible_context);

    const TMemoryContextScope context_scope(&transport_context);
    CErasedOwner retained = CErasedOwner::create<LoadedFile>(&transport_context);
    LoadedFile* const retained_payload = retained.payload<LoadedFile>();
    TEST_EXPECT(ctx, transport.post(std::move(retained)));

    CErasedOwner received;
    TEST_EXPECT(ctx, transport.read(received));
    TEST_EXPECT(ctx, received.payload<LoadedFile>() == retained_payload);
    TEST_EXPECT(ctx, received.memory_context() == &transport_context);
    received.destroy();

    CErasedOwner unread = CErasedOwner::create<LoadedFile>(&transport_context);
    TEST_EXPECT(ctx, unread.payload<LoadedFile>()->buffer.allocate(24u));
    TEST_EXPECT(ctx, transport.post(std::move(unread)));
    transport.deallocate();
    TEST_EXPECT(ctx, transport_context.get_live_allocation_count() == 0u);
}

void test_erased_owner_message(TTestContext& ctx)
{
    threading::CErasedOwnerMsg source;
    TEST_EXPECT(ctx, !source.has_message_type());
    TEST_EXPECT(ctx, !source.has_owner());
    TEST_EXPECT(ctx, source.query_message_type_id() == system_type_ids::undefined);
    TEST_EXPECT(ctx, source.query_async_slot() == 0);

    CErasedOwner content = CErasedOwner::create<DecodedTga>();
    DecodedTga* const payload = content.payload<DecodedTga>();
    TEST_EXPECT(ctx, payload != nullptr);
    TEST_EXPECT(ctx, payload->buffer.allocate(8u, 4u));

    source.set_message_type<TgaDecodeResult>();
    source.set_async_slot(-7);
    source.set_owner(std::move(content));

    TEST_EXPECT(ctx, source.is_message_a<TgaDecodeResult>());
    TEST_EXPECT(ctx, source.query_owner_type_id() == k_system_type_id_v<DecodedTga>);
    TEST_EXPECT(ctx, source.owner().payload<DecodedTga>() == payload);

    threading::CErasedOwnerMsg moved{ std::move(source) };
    TEST_EXPECT(ctx, !source.has_message_type());
    TEST_EXPECT(ctx, !source.has_owner());
    TEST_EXPECT(ctx, source.query_async_slot() == 0);
    TEST_EXPECT(ctx, moved.query_async_slot() == -7);
    TEST_EXPECT(ctx, moved.is_message_a<TgaDecodeResult>());
    TEST_EXPECT(ctx, moved.owner().payload<DecodedTga>() == payload);

    CErasedOwner detached = moved.take_owner();
    TEST_EXPECT(ctx, !moved.has_owner());
    TEST_EXPECT(ctx, moved.has_message_type());
    TEST_EXPECT(ctx, detached.payload<DecodedTga>() == payload);
}

void test_erased_owner_message_transport(TTestContext& ctx)
{
    TAllocatorState state;
    memory::CMemoryAllocator allocator(
        &state, allocate_test_memory, deallocate_test_memory, system_ids::host);
    memory::CMemoryContext producer_context(allocator, system_ids::host);
    memory::CMemoryContext transport_context(allocator, system_ids::host);
    memory::CMemoryContext recipient_context(allocator, system_ids::host);

    threading::transports::CErasedOwnerMsgTransport transport(
        &transport_context, &recipient_context);
    threading::transports::CErasedOwnerMsgProducerEndpoint producer(transport);
    threading::transports::CErasedOwnerMsgConsumerEndpoint consumer(transport);
    TEST_EXPECT(ctx, transport.initialise(32u));
    TEST_EXPECT(ctx, producer.is_valid());
    TEST_EXPECT(ctx, consumer.is_valid());

    const TMemoryContextScope context_scope(&producer_context);
    threading::CErasedOwnerMsg posted;
    posted.set_message_type<FileLoadResult>();
    posted.set_async_slot(31);
    CErasedOwner content = CErasedOwner::create<LoadedFile>();
    LoadedFile* const payload = content.payload<LoadedFile>();
    TEST_EXPECT(ctx, payload->buffer.allocate(48u));
    posted.set_owner(std::move(content));

    TEST_EXPECT(ctx, producer.post(std::move(posted)));
    TEST_EXPECT(ctx, !posted.has_message_type());
    TEST_EXPECT(ctx, !posted.has_owner());

    threading::CErasedOwnerMsg received;
    TEST_EXPECT(ctx, consumer.read(received));
    TEST_EXPECT(ctx, received.is_message_a<FileLoadResult>());
    TEST_EXPECT(ctx, received.query_async_slot() == 31);
    TEST_EXPECT(ctx, received.owner().payload<LoadedFile>() == payload);
    TEST_EXPECT(ctx, received.owner().memory_context() == &recipient_context);

    threading::CErasedOwnerMsg empty_completion;
    empty_completion.set_message_type<TgaEncodeResult>();
    empty_completion.set_async_slot(32);
    TEST_EXPECT(ctx, producer.post(std::move(empty_completion)));
    TEST_EXPECT(ctx, consumer.read(received));
    TEST_EXPECT(ctx, received.is_message_a<TgaEncodeResult>());
    TEST_EXPECT(ctx, received.query_async_slot() == 32);
    TEST_EXPECT(ctx, !received.has_owner());

    threading::CErasedOwnerMsg untyped;
    TEST_EXPECT(ctx, !producer.post(std::move(untyped)));

    transport.deallocate();
    TEST_EXPECT(ctx, transport_context.get_live_allocation_count() == 0u);
}

}   //  namespace

int run_erased_owner_tests()
{
    TTestContext ctx;
    test_registration_and_empty_state(ctx);
    test_creation_accounting_and_destruction(ctx);
    test_moves_and_hazards(ctx);
    test_allocation_failure_is_canonical(ctx);
    test_owner_reattribution(ctx);
    test_all_registered_payload_reattribution(ctx);
    test_existing_owning_transport(ctx);
    test_erased_owner_transport_attribution(ctx);
    test_erased_owner_transport_rejection(ctx);
    test_erased_owner_message(ctx);
    test_erased_owner_message_transport(ctx);

    std::cout << "ErasedOwner: " << ctx.passed << " passed, " << ctx.failed << " failed\n";
    return (ctx.failed == 0) ? 0 : 1;
}
