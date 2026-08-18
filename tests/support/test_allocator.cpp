//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)

#include "tests/support/test_allocator.hpp"

#include <new>

namespace tests
{

void* MV_STD_ABI_CALL allocate_test_memory(
    void* const state,
    const std::size_t alignment,
    const std::size_t bytes) noexcept
{
    if ((state != nullptr) &&
        static_cast<TAllocatorFixture*>(state)->reject_allocation)
    {
        return nullptr;
    }
    return ::operator new[](bytes, std::align_val_t{ alignment }, std::nothrow);
}

bool MV_STD_ABI_CALL deallocate_test_memory(
    void*, const std::size_t alignment, void* const pointer) noexcept
{
    if (pointer == nullptr)
    {
        return false;
    }
    ::operator delete[](pointer, std::align_val_t{ alignment });
    return true;
}

}   //  namespace tests
