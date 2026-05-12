
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   host_allocator.cpp
//  Author: Ritchie Brannan
//  Date:   8 May 26
//
//  Host allocator access gate implementation.
//
//  Defines the executable-owned default allocator and restricts direct access
//  to the authorised host system identity.
//
//  Modules must receive allocator access through host-provided services rather
//  than calling this function directly.
//
//  Design constraints
//  ------------------
//  - Requires C++17 or later.
//  - No exceptions.
//  - Allocation uses nothrow new[].

#include <cstddef>      //  std::size_t
#include <new>          //  std::nothrow, aligned operator new[]/delete[]

#include "memory/host_allocator.hpp"
#include "memory/memory_allocation.hpp"
#include "system/system_ids.hpp"

namespace memory
{

//==============================================================================
//  Executable-owned default allocator
//==============================================================================

class CSystemAllocator : public IAllocator
{
public:
    CSystemAllocator() = default;
    ~CSystemAllocator() = default;

    virtual void* byte_allocate(const std::size_t bytes, const std::size_t align) noexcept override final
    {
        return ::operator new[](bytes, alignment_policy(align), std::nothrow);
    }

    virtual void byte_deallocate(void* const ptr, const std::size_t align) noexcept override final
    {
        if (ptr != nullptr)
        {
            ::operator delete[](ptr, alignment_policy(align));
        }
    }
};

IAllocator* get_the_host_allocator(const std::size_t system_id) noexcept
{
    static CSystemAllocator allocator;
    if (system_id == system_ids::host)
    {
        return &allocator;
    }
    MV_HARD_ASSERT(false);
    return nullptr;
}

}   //  namespace memory