//  File:   ByteBuffers.hpp
//  Author: Ritchie Brannan
//  Date:   22 Feb 26
//
//  POD byte buffer and byte-rect buffer utilities (noexcept containers)
//
//  Requirements:
//  - Requires C++17 or greater.
//  - No exceptions.
//  - Buffers store raw bytes (std::uint8_t).
//  - Sizes, capacities, row extents and alignment values are expressed in bytes.
//
//  Design overview:
//  - CByteBuffer owns a contiguous byte allocation.
//  - CByteView and CByteConstView provide non-owning contiguous byte views.
//  - CByteRectBuffer owns rectangular byte storage with aligned row starts.
//  - CByteRectView and CByteRectConstView provide non-owning views over
//    rectangular byte ranges with explicit row pitch.
//
//  Memory model:
//  - Storage ownership is provided by memory::CMemoryToken.
//  - Logical extent metadata is stored separately from the owning token/view.
//  - Alignment guarantees are carried by the memory token / memory view.
//
//  Observation model:
//  - Accessor functions are fail-safe.
//  - If invariants appear broken, observers return empty / zero values.
//  - Validity observers report invariant validity directly.
//
//  CByteBuffer invariants:
//      {data == nullptr, size == 0, capacity == 0} (canonical empty)
//      {data != nullptr, size <= capacity, capacity != 0}
//      capacity <= memory::k_max_elements
//
//  CByteBuffer observation model:
//      - size == 0 reports empty even when capacity != 0.
//      - size == 0, capacity != 0 is still a ready state.
// 
//  Rect byte model:
//  - row_pitch is the byte step from one row start to the next.
//  - row_width is the active byte extent within each row.
//  - row_count is the number of rows.
//  - row_width may be less than row_pitch.
//  - A rect is contiguous as a byte buffer only when row_width == row_pitch.
//
//  CByteRectView / CByteRectBuffer metadata invariants:
//      {row_pitch == 0, row_width == 0, row_count == 0} (canonical empty)
//      {row_pitch != 0, row_width != 0, row_count != 0}
//      row_width <= row_pitch
//      row_pitch * row_count <= memory::k_max_elements
//
//  Alignment model:
//  - Alignment applies to the base address / current view origin.
//  - For CByteBuffer, passing align == 0 requests reuse of the current alignment
//    intent; if none exists, the allocation layer normalisation rules apply.
//  - For CByteRectBuffer, row starts are aligned to at least align().
//  - Owning rect buffers derive row_pitch by rounding row_width up to a
//    multiple of the requested alignment.
//  - Subviews may reduce the guaranteed alignment according to their byte
//    offset from the original view origin.
//
//  Scope:
//  - This layer models rectangular byte storage only.
//  - It does not impose higher-level element, texel, block, pixel or structure semantics.
//  - Such interpretations belong in wrapper layers above this substrate.
//
//  Size / capacity model:
//  - For CByteBuffer, size() represents the logical byte extent.
//  - For CByteBuffer, capacity() represents the allocated byte extent.
//  - size() may be adjusted without reallocation using set_size(),
//    provided the new size does not exceed capacity().
//
//  Growth behaviour:
//  - resize(size) changes logical size and grows capacity when required
//    using the configured buffer growth policy.
//  - reserve(min_capacity) increases capacity if required using the growth policy.
//  - reallocate(size, capacity) provides direct control over both values.
//
//  Initialisation behaviour:
//  - Some operations may expose previously uninitialised destination bytes.
//  - Where provided, clear / clear_uninitialised parameters request zeroing
//    of destination bytes that are not populated by preserved source content.
//  - set_size() does not initialise memory and is intended for workflows
//    where the caller fills the newly exposed region explicitly.
//

#pragma once

#ifndef BYTE_BUFFERS_HPP_INCLUDED
#define BYTE_BUFFERS_HPP_INCLUDED

#include <algorithm>    //  std::min
#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::uint8_t
#include <cstring>      //  std::memcpy, std::memset
#include <utility>      //  std::move

#include "memory/memory_allocation.hpp"
#include "memory/memory_primitives.hpp"
#include "bit_utils/bit_ops.hpp"
#include "debug/debug.hpp"

//==============================================================================
//  Forward declarations
//==============================================================================

class CByteBuffer;
class CByteView;
class CByteConstView;
class CByteRectBuffer;
class CByteRectView;
class CByteRectConstView;

//==============================================================================
//  Byte buffer metadata structs
//==============================================================================

struct MetaByteView
{
    std::size_t size = 0u;
    void reset() noexcept { size = 0u; }
    [[nodiscard]] bool is_valid() const noexcept { return size <= memory::k_max_elements; }
    [[nodiscard]] bool is_empty() const noexcept { return size == 0u; }
    [[nodiscard]] bool is_ready() const noexcept { return (size - 1u) < memory::k_max_elements; }
};

struct MetaByteBuffer
{
    std::size_t size = 0u;
    std::size_t capacity = 0u;
    void reset() noexcept { size = capacity = 0u; }
    [[nodiscard]] bool is_valid() const noexcept { return (size <= capacity) && (capacity <= memory::k_max_elements); }
    [[nodiscard]] bool is_empty() const noexcept { return (size == 0u) || (capacity == 0u); }
    [[nodiscard]] bool is_ready() const noexcept { return (size <= capacity) && ((capacity - 1u) < memory::k_max_elements); }
    [[nodiscard]] MetaByteView byte_view() const noexcept { return MetaByteView{ size }; }
};

//==============================================================================
//  CByteBuffer
//  Owning unique contiguous byte buffer with optional spare capacity.
//==============================================================================

class CByteBuffer
{
public:
    CByteBuffer() noexcept = default;
    CByteBuffer(const CByteBuffer&) noexcept = delete;
    CByteBuffer& operator=(const CByteBuffer&) noexcept = delete;

    CByteBuffer(CByteBuffer&&) noexcept;
    CByteBuffer& operator=(CByteBuffer&&) noexcept;
    ~CByteBuffer() noexcept { deallocate(); };

    //  Status reporting
    [[nodiscard]] bool is_valid() const noexcept { return m_token.is_valid() && m_meta.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_token.is_empty() || m_meta.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_token.is_ready() && m_meta.is_ready(); }

    //  Views
    [[nodiscard]] CByteView view() const noexcept;
    [[nodiscard]] CByteConstView const_view() const noexcept;

    //  Common accessors (see constness model above)
    [[nodiscard]] std::uint8_t* data() const noexcept { return m_meta.is_ready() ? m_token.data() : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return m_meta.is_ready() ? m_token.align() : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t size() const noexcept { return is_ready() ? m_meta.size : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t capacity() const noexcept { return is_ready() ? m_meta.capacity : std::size_t{ 0 }; }

    //  Appending
    [[nodiscard]] bool append(const std::uint8_t* const data, const std::size_t size) noexcept;
    [[nodiscard]] bool append(const std::size_t size, const bool clear = true) noexcept;

    //  Simple size setting (allows exposure of uninitialised data)
    //
    //  Sets logical size without initialising newly exposed bytes.
    //  Intended for workflows where the caller writes the exposed region explicitly.
    [[nodiscard]] bool set_size(const std::size_t size) noexcept;

    //  Size and capacity management (state unchanged on failure):

    //  Allocates contiguous byte storage with logical size 0.
    //  If align == 0, existing alignment intent is reused when available;
    //  otherwise the allocation layer default normalisation rules apply.
    [[nodiscard]] bool allocate(const std::size_t capacity, const std::size_t align = 0u) noexcept;

    //  Reallocates contiguous byte storage while preserving the overlapping prefix of the existing content.
    //  Preserved content:
    //  - bytes: min(old_size, new_size)
    //  Newly exposed logical bytes [old_size : new_size) are zeroed.
    //  Passing size == 0 and capacity == 0 requests deallocation.
    [[nodiscard]] bool reallocate(const std::size_t size, const std::size_t capacity, const std::size_t align = 0u) noexcept;

    //  Changes logical size and grows capacity when required using the configured buffer growth policy.
    //  Newly exposed logical bytes [old_size : new_size) are zeroed.
    [[nodiscard]] bool resize(const std::size_t size, const std::size_t align = 0u) noexcept;

    //  Ensures capacity is at least minimum_capacity using the configured buffer growth policy.
    //  Logical size is unchanged.
    [[nodiscard]] bool reserve(const std::size_t minimum_capacity, const std::size_t align = 0u) noexcept;

    //  Ensures at least extra bytes of free space beyond the current logical
    //  size using the configured buffer growth policy.
    //  Logical size is unchanged.
    [[nodiscard]] bool ensure_free(const std::size_t extra, const std::size_t align = 0u) noexcept;

    //  Reduces capacity to exactly match the current logical size.
    [[nodiscard]] bool shrink_to_fit() noexcept;

    //  Constructs this buffer by copying from a const byte view.
    //  - On success: replaces current state with a newly allocated copy of the view.
    //  - If view is empty: deallocates and returns true.
    //  - If view is invalid or allocation fails: returns false and leaves state unchanged.
    [[nodiscard]] bool construct_and_copy_from(const CByteConstView& view) noexcept;

    //  Discard all memory allocation, no-op if empty
    void deallocate() noexcept;

    //  Handoff functions
    memory::CMemoryToken disown() noexcept { m_meta.reset(); return std::move(m_token); }
    MetaByteBuffer meta() const noexcept { return m_meta; }

private:
    memory::CMemoryToken m_token;
    MetaByteBuffer m_meta;
};

//==============================================================================
//  CByteView
//  Non-owning view over a mutable contiguous byte range.
//==============================================================================

class CByteView
{
public:
    CByteView() noexcept = default;
    CByteView(CByteView&&) noexcept = default;
    CByteView& operator=(CByteView&&) noexcept = default;
    CByteView(const CByteView&) noexcept = default;
    CByteView& operator=(const CByteView&) noexcept = default;
    ~CByteView() noexcept = default;

    CByteView(std::uint8_t* const data, const std::size_t size, const std::size_t align = 0u) noexcept { (void)set(data, size, align); }
    CByteView(const memory::CMemoryView& view, const MetaByteView& meta) noexcept { (void)set(view, meta); }

    //  View setters
    CByteView& set(std::uint8_t* const data, const std::size_t size, const std::size_t align = 0u) noexcept;
    CByteView& set(const memory::CMemoryView& view, const MetaByteView& meta) noexcept;
    CByteView& reset() noexcept { m_view.reset(); m_meta.reset(); return *this; }

    //  Status reporting
    [[nodiscard]] bool is_valid() const noexcept { return m_view.is_valid() && m_meta.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_view.is_empty() || m_meta.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_view.is_ready() && m_meta.is_ready(); }

    //  Views and sub-views
    [[nodiscard]] CByteConstView const_view() const noexcept;
    [[nodiscard]] CByteView subview(const std::size_t offset, const std::size_t count) const noexcept;
    [[nodiscard]] CByteView head_to(const std::size_t count) const noexcept;
    [[nodiscard]] CByteView tail_from(const std::size_t offset) const noexcept;

    //  Common accessors (see constness model above)
    [[nodiscard]] std::uint8_t* data() const noexcept { return m_meta.is_ready() ? m_view.data() : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return m_meta.is_ready() ? m_view.align() : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t size() const noexcept { return is_ready() ? m_meta.size : std::size_t{ 0 }; }

private:
    memory::CMemoryView m_view;
    MetaByteView m_meta;
};

//==============================================================================
//  CByteConstView
//  Non-owning view over an immutable contiguous byte range.
//==============================================================================

class CByteConstView
{
public:
    CByteConstView() noexcept = default;
    CByteConstView(CByteConstView&&) noexcept = default;
    CByteConstView& operator=(CByteConstView&&) noexcept = default;
    CByteConstView(const CByteConstView&) noexcept = default;
    CByteConstView& operator=(const CByteConstView&) noexcept = default;
    ~CByteConstView() noexcept = default;

    CByteConstView(const std::uint8_t* const data, const std::size_t size, const std::size_t align = 0u) noexcept { (void)set(data, size, align); }
    CByteConstView(const memory::CMemoryConstView& view, const MetaByteView& meta) noexcept { (void)set(view, meta); }

    //  View setters
    CByteConstView& set(const std::uint8_t* const data, const std::size_t size, const std::size_t align = 0u) noexcept;
    CByteConstView& set(const memory::CMemoryConstView& view, const MetaByteView& meta) noexcept;
    CByteConstView& reset() noexcept { m_view.reset(); m_meta.reset(); return *this; }

    //  Status reporting
    [[nodiscard]] bool is_valid() const noexcept { return m_view.is_valid() && m_meta.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_view.is_empty() || m_meta.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_view.is_ready() && m_meta.is_ready(); }

    //  Sub-views
    [[nodiscard]] CByteConstView subview(const std::size_t offset, const std::size_t count) const noexcept;
    [[nodiscard]] CByteConstView head_to(const std::size_t count) const noexcept;
    [[nodiscard]] CByteConstView tail_from(const std::size_t offset) const noexcept;

    //  Common accessors (see constness model above)
    [[nodiscard]] const std::uint8_t* data() const noexcept { return m_meta.is_ready() ? m_view.data() : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return m_meta.is_ready() ? m_view.align() : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t size() const noexcept { return is_ready() ? m_meta.size : std::size_t{ 0 }; }

private:
    memory::CMemoryConstView m_view;
    MetaByteView m_meta;
};

//==============================================================================
//  CByteBuffer out of class function bodies
//==============================================================================

inline CByteBuffer::CByteBuffer(CByteBuffer&& other) noexcept
{
    m_meta = other.meta();
    m_token = other.disown();
}

inline CByteBuffer& CByteBuffer::operator=(CByteBuffer&& other) noexcept
{
    if (this != &other)
    {
        m_meta = other.meta();
        m_token = other.disown();
    }
    return *this;
}

[[nodiscard]] inline CByteView CByteBuffer::view() const noexcept
{
    return is_ready() ? CByteView{ m_token.view(), m_meta.byte_view() } : CByteView{};
}

[[nodiscard]] inline CByteConstView CByteBuffer::const_view() const noexcept
{
    return is_ready() ? CByteConstView{ m_token.const_view(), m_meta.byte_view() } : CByteConstView{};
}

[[nodiscard]] bool CByteBuffer::append(const std::uint8_t* const data, const std::size_t size) noexcept
{
    if (data == nullptr)
    {
        return size == 0u;
    }

    if (ensure_free(size))
    {
        std::memcpy((m_token.data() + m_meta.size), data, size);
        m_meta.size += size;
        return true;
    }
    return false;
}

[[nodiscard]] bool CByteBuffer::append(const std::size_t size, const bool clear) noexcept
{
    if (ensure_free(size))
    {
        if (clear)
        {
            std::memset((m_token.data() + m_meta.size), 0, size);
        }
        m_meta.size += size;
        return true;
    }
    return false;
}

[[nodiscard]] bool CByteBuffer::set_size(const std::size_t size) noexcept
{
    if (is_ready() && (size <= m_meta.capacity))
    {
        m_meta.size = size;
        return true;
    }
    return false;
}

[[nodiscard]] bool CByteBuffer::allocate(const std::size_t capacity, const std::size_t align) noexcept
{
    return reallocate(0u, capacity, align);
}

[[nodiscard]] bool CByteBuffer::reallocate(const std::size_t size, const std::size_t capacity, const std::size_t align) noexcept
{
    if ((size <= capacity) && (capacity <= memory::k_max_elements))
    {
        if (capacity == 0u)
        {
            deallocate();
            return true;
        }

        const std::size_t norm_align = memory::util::norm_align((align == 0u) ? m_token.align() : align);
        if ((capacity == m_meta.capacity) && (norm_align <= m_token.align()))
        {
            if (size > m_meta.size)
            {
                std::memset((m_token.data() + m_meta.size), 0, (size - m_meta.size));
            }
            m_meta.size = size;
            return true;
        }

        //  Reallocation path
        {
            memory::CMemoryToken token;
            if (token.allocate(capacity, norm_align, false))
            {
                const std::size_t copy_size = std::min(size, m_meta.size);
                if (copy_size != 0u)
                {
                    std::memcpy(token.data(), m_token.data(), copy_size);
                }
                if (size > m_meta.size)
                {
                    std::memset((token.data() + m_meta.size), 0, (size - m_meta.size));
                }
                MetaByteBuffer meta{ size, capacity };
                m_token = std::move(token);
                m_meta = meta;
                VE_HARD_ASSERT(m_token.align() == norm_align);
                VE_HARD_ASSERT(m_meta.size == size);
                VE_HARD_ASSERT(m_meta.capacity == capacity);
                VE_HARD_ASSERT(is_ready());
                return true;
            }
            return false;
        }
    }
    return false;
}

[[nodiscard]] bool CByteBuffer::resize(const std::size_t size, const std::size_t align) noexcept
{
    return (size <= memory::k_max_elements) ? reallocate(size, ((size > m_meta.capacity) ? memory::buffer_growth_policy(size) : m_meta.capacity), align) : false;
}

[[nodiscard]] bool CByteBuffer::reserve(const std::size_t minimum_capacity, const std::size_t align) noexcept
{
    return (minimum_capacity <= memory::k_max_elements) ? reallocate(m_meta.size, std::max(memory::buffer_growth_policy(minimum_capacity), m_meta.capacity), align) : false;
}

[[nodiscard]] bool CByteBuffer::ensure_free(const std::size_t extra, const std::size_t align) noexcept
{
    return (extra <= (memory::k_max_elements - m_meta.size)) ? reserve((m_meta.size + extra), align) : false;
}

[[nodiscard]] bool CByteBuffer::shrink_to_fit() noexcept
{
    return reallocate(m_meta.size, m_meta.size, m_token.align());
}

[[nodiscard]] bool CByteBuffer::construct_and_copy_from(const CByteConstView& view) noexcept
{
    if (!view.is_valid())
    {
        return false;
    }

    if (view.is_empty())
    {
        deallocate();
        return true;
    }

    //  Construction and copy path
    {
        memory::CMemoryToken token;
        if (token.allocate(view.size(), view.align(), false))
        {
            std::memcpy(token.data(), view.data(), view.size());
            MetaByteBuffer meta{ view.size(), view.size() };
            m_token = std::move(token);
            m_meta = meta;
            VE_HARD_ASSERT(m_token.align() == view.align());
            VE_HARD_ASSERT(m_meta.size == view.size());
            VE_HARD_ASSERT(m_meta.capacity == view.size());
            VE_HARD_ASSERT(is_ready());
            return true;
        }
        return false;
    }
}

inline void CByteBuffer::deallocate() noexcept
{
    m_token.deallocate();
    m_meta.reset();
}

//==============================================================================
//  CByteView out of class function bodies
//==============================================================================

inline CByteView& CByteView::set(std::uint8_t* const data, const std::size_t size, const std::size_t align) noexcept
{
    if ((data != nullptr) && (size != 0u))
    {
        m_view.set(data, align);
        m_meta.size = size;
    }
    else
    {
        reset();
    }
    return *this;
}

inline CByteView& CByteView::set(const memory::CMemoryView& view, const MetaByteView& meta) noexcept
{
    if (view.is_ready() && meta.is_ready())
    {
        m_view = view;
        m_meta = meta;
    }
    else
    {
        reset();
    }
    return *this;
}

[[nodiscard]] inline CByteConstView CByteView::const_view() const noexcept
{
    return is_ready() ? CByteConstView{ m_view.const_view(), m_meta } : CByteConstView{};
}

[[nodiscard]] inline CByteView CByteView::subview(const std::size_t offset, const std::size_t count) const noexcept
{
    return (is_ready() && (m_meta.size > offset) && (count <= (m_meta.size - offset))) ? CByteView{ m_view.subview(offset), MetaByteView{ count } } : CByteView{};
}

[[nodiscard]] inline CByteView CByteView::head_to(const std::size_t count) const noexcept
{
    return (is_ready() && (m_meta.size > (count - 1u))) ? CByteView{ m_view, MetaByteView{ count } } : CByteView{};
}

[[nodiscard]] inline CByteView CByteView::tail_from(const std::size_t offset) const noexcept
{
    return (is_ready() && (m_meta.size > offset)) ? CByteView{ m_view.subview(offset), MetaByteView{ m_meta.size - offset } } : CByteView{};
}

//==============================================================================
//  CByteConstView out of class function bodies
//==============================================================================

inline CByteConstView& CByteConstView::set(const std::uint8_t* const data, const std::size_t size, const std::size_t align) noexcept
{
    if ((data != nullptr) && (size != 0u))
    {
        m_view.set(data, align);
        m_meta.size = size;
    }
    else
    {
        reset();
    }
    return *this;
}

inline CByteConstView& CByteConstView::set(const memory::CMemoryConstView& view, const MetaByteView& meta) noexcept
{
    if (view.is_ready() && meta.is_ready())
    {
        m_view = view;
        m_meta = meta;
    }
    else
    {
        reset();
    }
    return *this;
}

[[nodiscard]] inline CByteConstView CByteConstView::subview(const std::size_t offset, const std::size_t count) const noexcept
{
    return (is_ready() && (m_meta.size > offset) && (count <= (m_meta.size - offset))) ? CByteConstView{ m_view.subview(offset), MetaByteView{ count } } : CByteConstView{};
}

[[nodiscard]] inline CByteConstView CByteConstView::head_to(const std::size_t count) const noexcept
{
    return (is_ready() && (m_meta.size > (count - 1u))) ? CByteConstView{ m_view, MetaByteView{ count } } : CByteConstView{};
}

[[nodiscard]] inline CByteConstView CByteConstView::tail_from(const std::size_t offset) const noexcept
{
    return (is_ready() && (m_meta.size > offset)) ? CByteConstView{ m_view.subview(offset), MetaByteView{ m_meta.size - offset } } : CByteConstView{};
}

//==============================================================================
//  Byte rect buffer metadata structs
//==============================================================================

//  Rectangular byte-range metadata.
//  row_pitch: byte step from one row start to the next
//  row_width: active byte extent within each row
//  row_count: number of rows
struct MetaByteRectView
{
    std::size_t row_pitch = 0u;
    std::size_t row_width = 0u;
    std::size_t row_count = 0u;
    void reset() noexcept { row_pitch = row_width = row_count = 0u; }
    [[nodiscard]] bool set(const std::size_t pitch, const std::size_t width, const std::size_t count) noexcept;
    [[nodiscard]] bool is_valid() const noexcept { return any_zero() ? all_zero() : range_ok(); }
    [[nodiscard]] bool is_empty() const noexcept { return any_zero(); }
    [[nodiscard]] bool is_ready() const noexcept { return any_zero() ? false : range_ok(); }
    [[nodiscard]] bool is_contiguous() const noexcept { return is_ready() && (row_width == row_pitch); }
    [[nodiscard]] std::size_t size_as_buffer() const noexcept { return is_contiguous() ? (row_pitch * row_count) : std::size_t{ 0 }; }
    [[nodiscard]] MetaByteView byte_view() const noexcept { return MetaByteView{ size_as_buffer() }; }
    [[nodiscard]] MetaByteRectView subview(const std::size_t x, const std::size_t y, const std::size_t width, const std::size_t height) const noexcept;
private:
    [[nodiscard]] bool any_zero() const noexcept { return std::min(row_pitch, std::min(row_width, row_count)) == 0u; }
    [[nodiscard]] bool all_zero() const noexcept { return (row_pitch | row_width | row_count) == 0u; }
    [[nodiscard]] bool range_ok() const noexcept;
};

[[nodiscard]] bool MetaByteRectView::set(const std::size_t pitch, const std::size_t width, const std::size_t count) noexcept
{
    if (((width - 1u) < pitch) && ((count - 1u) < (memory::k_max_elements / pitch)))
    {
        row_pitch = pitch;
        row_width = width;
        row_count = count;
        return true;
    }
    return false;
}

[[nodiscard]] MetaByteRectView MetaByteRectView::subview(const std::size_t x, const std::size_t y, const std::size_t width, const std::size_t height) const noexcept
{
    if (is_ready() &&
        (std::max(x, (width - 1u)) < row_width) && (x <= (row_width - width)) &&
        (std::max(y, (height - 1u)) < row_count) && (y <= (row_count - height)))
    {
        return MetaByteRectView{ row_pitch, width, height };
    }
    return MetaByteRectView{};
}

[[nodiscard]] bool MetaByteRectView::range_ok() const noexcept
{
    return
        ((row_pitch - 1u) < memory::k_max_elements) &&
        ((row_width - 1u) < row_pitch) &&
        ((row_count - 1u) < (memory::k_max_elements / row_pitch));
}

using MetaByteRectBuffer = MetaByteRectView;

//==============================================================================
//  CByteRectBuffer
//  Owning unique contiguous byte rect buffer
//==============================================================================

class CByteRectBuffer
{
public:
    CByteRectBuffer() noexcept = default;
    CByteRectBuffer(const CByteRectBuffer&) noexcept = delete;
    CByteRectBuffer& operator=(const CByteRectBuffer&) noexcept = delete;

    CByteRectBuffer(CByteRectBuffer&&) noexcept;
    CByteRectBuffer& operator=(CByteRectBuffer&&) noexcept;
    ~CByteRectBuffer() noexcept { deallocate(); };

    //  Status reporting
    [[nodiscard]] bool is_valid() const noexcept { return m_token.is_valid() && m_meta.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_token.is_empty() || m_meta.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_token.is_ready() && m_meta.is_ready(); }
    [[nodiscard]] bool is_contiguous() const noexcept { return m_token.is_ready() && m_meta.is_contiguous(); }

    //  Views
    [[nodiscard]] CByteRectView view() const noexcept;
    [[nodiscard]] CByteRectConstView const_view() const noexcept;

    //  Byte views (will be empty if the buffer is non-contiguous)
    [[nodiscard]] CByteView byte_view() const noexcept { return is_contiguous() ? CByteView{ m_token.view(), m_meta.byte_view() } : CByteView{}; }
    [[nodiscard]] CByteConstView byte_const_view() const noexcept { return is_contiguous() ? CByteConstView{ m_token.const_view(), m_meta.byte_view() } : CByteConstView{}; }

    //  Common accessors (see constness model above)
    [[nodiscard]] std::uint8_t* data() const noexcept { return m_meta.is_ready() ? m_token.data() : nullptr; }
    [[nodiscard]] std::uint8_t* row_data(const std::size_t y) const noexcept { return (is_ready() && (y < m_meta.row_count)) ? (m_token.data() + (y * m_meta.row_pitch)) : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return m_meta.is_ready() ? m_token.align() : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_pitch() const noexcept { return is_ready() ? m_meta.row_pitch : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_width() const noexcept { return is_ready() ? m_meta.row_width : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_count() const noexcept { return is_ready() ? m_meta.row_count : std::size_t{ 0 }; }

    //  Size and capacity management (state unchanged on failure):
   
    //  Allocates rectangular byte storage for row_count rows of row_width bytes.
    //  The stored row pitch is derived by rounding row_width up to a multiple of the requested alignment.
    //  If row_align == 0, existing alignment intent is reused when available;
    //  otherwise the allocation layer default normalization rules apply.
    [[nodiscard]] bool allocate(const std::size_t row_width, const std::size_t row_count, const std::size_t row_align = 0u, const bool clear = true) noexcept;

    //  Reallocates rectangular byte storage while preserving the overlapping prefix of each preserved row.
    //  Preserved content:
    //  - rows:    min(old_row_count, new_row_count)
    //  - bytes:   min(old_row_width, new_row_width) per preserved row
    //
    //  If clear_uninitialised is true, destination bytes not populated by preserved source content are zeroed.
    //  This includes:
    //  - newly added rows
    //  - newly exposed bytes within preserved rows
    //  - row tail bytes not written during row-by-row preservation
    //
    //  Passing row_width == 0 and row_count == 0 requests deallocation.
    [[nodiscard]] bool reallocate(const std::size_t row_width, const std::size_t row_count, const std::size_t row_align = 0u, const bool clear_uninitialised = true) noexcept;

    //  Constructs this rect buffer by copying from a const rect view.
    //  - On success: replaces current state with a newly allocated rect buffer
    //    containing a copy of the source view.
    //  - Destination row pitch is derived by rounding row_width up to a multiple
    //    of the source view's guaranteed alignment.
    //  - Bytes within [0 : row_width) are copied for each row.
    //  - Bytes within [row_width : row_pitch) are zeroed.
    //  - If view is empty: deallocates and returns true.
    //  - If view is invalid or allocation fails: returns false and leaves state unchanged.
    [[nodiscard]] bool construct_and_copy_from(const CByteRectConstView& view) noexcept;

    //  Discards all memory allocation, no-op if empty
    void deallocate() noexcept;

    //  Handoff functions
    memory::CMemoryToken disown() noexcept { m_meta.reset(); return std::move(m_token); }
    MetaByteRectBuffer meta() const noexcept { return m_meta; }

private:
    memory::CMemoryToken m_token;
    MetaByteRectBuffer m_meta;
};

//==============================================================================
//  CByteRectView
//  Non-owning rect view over a mutable potentially non-contiguous byte range.
//==============================================================================

class CByteRectView
{
public:
    CByteRectView() noexcept = default;
    CByteRectView(CByteRectView&&) noexcept = default;
    CByteRectView& operator=(CByteRectView&&) noexcept = default;
    CByteRectView(const CByteRectView&) noexcept = default;
    CByteRectView& operator=(const CByteRectView&) noexcept = default;
    ~CByteRectView() noexcept = default;

    CByteRectView(std::uint8_t* const data, const std::size_t row_pitch, const std::size_t row_width, const std::size_t row_count, const std::size_t align = 0u) noexcept { (void)set(data, row_pitch, row_width, row_count, align); }
    CByteRectView(const memory::CMemoryView& view, const MetaByteRectView& meta) noexcept { (void)set(view, meta); }

    //  View setters
    CByteRectView& set(std::uint8_t* const data, const std::size_t row_pitch, const std::size_t row_width, const std::size_t row_count, const std::size_t align = 0u) noexcept;
    CByteRectView& set(const memory::CMemoryView& view, const MetaByteRectView& meta) noexcept;
    CByteRectView& reset() noexcept { m_view.reset(); m_meta.reset(); return *this; }

    //  Status reporting
    [[nodiscard]] bool is_valid() const noexcept { return m_view.is_valid() && m_meta.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_view.is_empty() || m_meta.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_view.is_ready() && m_meta.is_ready(); }
    [[nodiscard]] bool is_contiguous() const noexcept { return m_view.is_ready() && m_meta.is_contiguous(); }

    //  Views
    [[nodiscard]] CByteRectConstView const_view() const noexcept;

    //  Sub-views
    //  x and width are expressed in bytes within each row.
    //  y and height are expressed in rows.
    [[nodiscard]] CByteRectView subview(const std::size_t x, const std::size_t y, const std::size_t width, const std::size_t height) const noexcept;

    //  Byte views (will be empty if the view is non-contiguous)
    [[nodiscard]] CByteView byte_view() const noexcept { return is_contiguous() ? CByteView{ m_view, m_meta.byte_view() } : CByteView{}; }
    [[nodiscard]] CByteConstView byte_const_view() const noexcept { return is_contiguous() ? CByteConstView{ m_view.const_view(), m_meta.byte_view() } : CByteConstView{}; }

    //  Common accessors (see constness model above)
    [[nodiscard]] std::uint8_t* data() const noexcept { return m_meta.is_ready() ? m_view.data() : nullptr; }
    [[nodiscard]] std::uint8_t* row_data(const std::size_t y) const noexcept { return (is_ready() && (y < m_meta.row_count)) ? (m_view.data() + (y * m_meta.row_pitch)) : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return m_meta.is_ready() ? m_view.align() : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_pitch() const noexcept { return is_ready() ? m_meta.row_pitch : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_width() const noexcept { return is_ready() ? m_meta.row_width : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_count() const noexcept { return is_ready() ? m_meta.row_count : std::size_t{ 0 }; }

private:
    memory::CMemoryView m_view;
    MetaByteRectView m_meta;
};

//==============================================================================
//  CByteRectConstView
//  Non-owning rect view over an immutable potentially non-contiguous byte range.
//==============================================================================

class CByteRectConstView
{
public:
    CByteRectConstView() noexcept = default;
    CByteRectConstView(CByteRectConstView&&) noexcept = default;
    CByteRectConstView& operator=(CByteRectConstView&&) noexcept = default;
    CByteRectConstView(const CByteRectConstView&) noexcept = default;
    CByteRectConstView& operator=(const CByteRectConstView&) noexcept = default;
    ~CByteRectConstView() noexcept = default;

    CByteRectConstView(const std::uint8_t* const data, const std::size_t row_pitch, const std::size_t row_width, const std::size_t row_count, const std::size_t align = 0u) noexcept { (void)set(data, row_pitch, row_width, row_count, align); }
    CByteRectConstView(const memory::CMemoryConstView& view, const MetaByteRectView& meta) noexcept { (void)set(view, meta); }

    //  View setters
    CByteRectConstView& set(const std::uint8_t* const data, const std::size_t row_pitch, const std::size_t row_width, const std::size_t row_count, const std::size_t align = 0u) noexcept;
    CByteRectConstView& set(const memory::CMemoryConstView& view, const MetaByteRectView& meta) noexcept;
    CByteRectConstView& reset() noexcept { m_view.reset(); m_meta.reset(); return *this; }

    //  Status reporting
    [[nodiscard]] bool is_valid() const noexcept { return m_view.is_valid() && m_meta.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_view.is_empty() || m_meta.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_view.is_ready() && m_meta.is_ready(); }
    [[nodiscard]] bool is_contiguous() const noexcept { return m_view.is_ready() && m_meta.is_contiguous(); }

    //  Sub-views
    //  x and width are expressed in bytes within each row.
    //  y and height are expressed in rows.
    [[nodiscard]] CByteRectConstView subview(const std::size_t x, const std::size_t y, const std::size_t width, const std::size_t height) const noexcept;

    //  Byte views (will be empty if the view is non-contiguous)
    [[nodiscard]] CByteConstView byte_view() const noexcept { return is_contiguous() ? CByteConstView{ m_view, m_meta.byte_view() } : CByteConstView{}; }

    //  Common accessors (see constness model above)
    [[nodiscard]] const std::uint8_t* data() const noexcept { return m_meta.is_ready() ? m_view.data() : nullptr; }
    [[nodiscard]] const std::uint8_t* row_data(const std::size_t y) const noexcept { return (is_ready() && (y < m_meta.row_count)) ? (m_view.data() + (y * m_meta.row_pitch)) : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return m_meta.is_ready() ? m_view.align() : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_pitch() const noexcept { return is_ready() ? m_meta.row_pitch : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_width() const noexcept { return is_ready() ? m_meta.row_width : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_count() const noexcept { return is_ready() ? m_meta.row_count : std::size_t{ 0 }; }

private:
    memory::CMemoryConstView m_view;
    MetaByteRectView m_meta;
};

//==============================================================================
//  CByteRectBuffer out of class function bodies
//==============================================================================

inline CByteRectBuffer::CByteRectBuffer(CByteRectBuffer&& other) noexcept
{
    m_meta = other.meta();
    m_token = other.disown();
}

inline CByteRectBuffer& CByteRectBuffer::operator=(CByteRectBuffer&& other) noexcept
{
    if (this != &other)
    {
        m_meta = other.meta();
        m_token = other.disown();
    }
    return *this;
}

[[nodiscard]] inline CByteRectView CByteRectBuffer::view() const noexcept
{
    return is_ready() ? CByteRectView{ m_token.view(), m_meta } : CByteRectView{};
}

[[nodiscard]] inline CByteRectConstView CByteRectBuffer::const_view() const noexcept
{
    return is_ready() ? CByteRectConstView{ m_token.const_view(), m_meta } : CByteRectConstView{};
}

[[nodiscard]] inline bool CByteRectBuffer::allocate(const std::size_t row_width, const std::size_t row_count, const std::size_t row_align, const bool clear) noexcept
{
    if (reallocate(row_width, row_count, row_align, false))
    {
        if (clear)
        {
            std::memset(m_token.data(), 0, (m_meta.row_pitch * m_meta.row_count));
        }
        return true;
    }
    return false;
}

[[nodiscard]] inline bool CByteRectBuffer::reallocate(const std::size_t row_width, const std::size_t row_count, const std::size_t row_align, const bool clear_uninitialised) noexcept
{
    if ((row_width == 0u) && (row_count == 0u))
    {   //  this is a deallocation request
        deallocate();
        return true;
    }

    if ((row_width - 1u) < memory::k_max_elements)
    {   //  row_width is valid
        const std::size_t use_align = memory::util::norm_align((row_align != 0u) ? row_align : m_token.align());
        const std::size_t row_pitch = bit_ops::round_up_to_pow2_multiple(row_width, use_align);
        if ((row_count - 1u) < (memory::k_max_elements / row_pitch))
        {   //  row_count is valid
            if ((use_align <= m_token.align()) && (row_pitch == m_meta.row_pitch) && (row_width == m_meta.row_width) && (row_count == m_meta.row_count))
            {   //  nothing meaningful has changed that requires reallocation
                return true;
            }

            //  Reallocation path
            {   //  something meaningful has changed that requires reallocation
                const std::size_t bytes = row_pitch * row_count;
                memory::CMemoryToken token;
                if (token.allocate(bytes, use_align, false))
                {   //  the allocation succeeded
                    if (is_ready())
                    {   //  true reallocation
                        const std::uint8_t* src_data = m_token.data();
                        std::uint8_t* dst_data = token.data();
                        if ((row_width == row_pitch) && (row_width == m_meta.row_width) && (row_pitch == m_meta.row_pitch))
                        {   //  a single copy is possible
                            std::memcpy(dst_data, src_data, (row_pitch * std::min(row_count, m_meta.row_count)));
                        }
                        else
                        {   //  needs row by row copying and optional clearing
                            const std::size_t copy_width = std::min(row_width, m_meta.row_width);
                            const std::size_t clear_width = clear_uninitialised ? (row_pitch - copy_width) : std::size_t{ 0 };
                            for (std::size_t count = std::min(row_count, m_meta.row_count); count != 0u; --count)
                            {
                                std::memcpy(dst_data, src_data, copy_width);
                                if (clear_width != 0u)
                                {
                                    std::memset((dst_data + copy_width), 0, clear_width);
                                }
                                src_data += m_meta.row_pitch;
                                dst_data += row_pitch;
                            }
                        }
                        if (clear_uninitialised && (row_count > m_meta.row_count))
                        {   //  there is extra accessible space to clear below the image
                            std::memset((token.data() + (row_pitch * m_meta.row_count)), 0, (row_pitch * (row_count - m_meta.row_count)));
                        }
                    }
                    else if (clear_uninitialised)
                    {   //  primary allocation clear
                        std::memset(token.data(), 0, bytes);
                    }
                    MetaByteRectBuffer meta{ row_pitch, row_width, row_count };
                    m_token = std::move(token);
                    m_meta = meta;
                    VE_HARD_ASSERT(m_token.align() == use_align);
                    VE_HARD_ASSERT(m_meta.row_pitch == row_pitch);
                    VE_HARD_ASSERT(m_meta.row_width == row_width);
                    VE_HARD_ASSERT(m_meta.row_count == row_count);
                    VE_HARD_ASSERT(is_ready());
                    return true;
                }
            }
        }
    }
    return false;
}

[[nodiscard]] bool CByteRectBuffer::construct_and_copy_from(const CByteRectConstView& view) noexcept
{
    if (!view.is_valid())
    {
        return false;
    }

    if (view.is_empty())
    {
        deallocate();
        return true;
    }

    //  Construction and copy path
    {
        memory::CMemoryToken token;
        const std::size_t row_align = view.align();
        const std::size_t row_width = view.row_width();
        const std::size_t row_count = view.row_count();
        const std::size_t row_pitch = bit_ops::round_up_to_pow2_multiple(row_width, row_align);
        if (token.allocate((row_pitch * row_count), row_align, false))
        {
            const std::size_t src_pitch = view.row_pitch();
            const std::uint8_t* src_data = view.data();
            std::uint8_t* dst_data = token.data();
            if ((row_width == row_pitch) && (row_pitch == src_pitch))
            {   //  a single copy is possible
                std::memcpy(dst_data, src_data, (row_pitch * row_count));
            }
            else
            {   //  needs row by row copying, with destination tail clearing if present
                const std::size_t clear_width = row_pitch - row_width;
                for (std::size_t count = row_count; count != 0u; --count)
                {
                    std::memcpy(dst_data, src_data, row_width);
                    if (clear_width != 0u)
                    {
                        std::memset((dst_data + row_width), 0, clear_width);
                    }
                    src_data += src_pitch;
                    dst_data += row_pitch;
                }
            }
            MetaByteRectBuffer meta{ row_pitch, row_width, row_count };
            m_token = std::move(token);
            m_meta = meta;
            VE_HARD_ASSERT(m_token.align() == row_align);
            VE_HARD_ASSERT(m_meta.row_pitch == row_pitch);
            VE_HARD_ASSERT(m_meta.row_width == row_width);
            VE_HARD_ASSERT(m_meta.row_count == row_count);
            VE_HARD_ASSERT(is_ready());
            return true;
        }
        return false;
    }
}

inline void CByteRectBuffer::deallocate() noexcept
{
    m_token.deallocate();
    m_meta.reset();
}

//==============================================================================
//  CByteRectView out of class function bodies
//==============================================================================

inline CByteRectView& CByteRectView::set(std::uint8_t* const data, const std::size_t row_pitch, const std::size_t row_width, const std::size_t row_count, const std::size_t align) noexcept
{
    if ((data != nullptr) && m_meta.set(row_pitch, row_width, row_count))
    {
        m_view.set(data, align);
    }
    else
    {
        reset();
    }
    return *this;
}

inline CByteRectView& CByteRectView::set(const memory::CMemoryView& view, const MetaByteRectView& meta) noexcept
{
    if (view.is_ready() && meta.is_ready())
    {
        m_view = view;
        m_meta = meta;
    }
    else
    {
        reset();
    }
    return *this;
}

[[nodiscard]] inline CByteRectConstView CByteRectView::const_view() const noexcept
{
    return is_ready() ? CByteRectConstView{ m_view.const_view(), m_meta } : CByteRectConstView{};
}

[[nodiscard]] inline CByteRectView CByteRectView::subview(const std::size_t x, const std::size_t y, const std::size_t width, const std::size_t height) const noexcept
{
    if (is_ready())
    {
        const MetaByteRectView meta(m_meta.subview(x, y, width, height));
        if (meta.is_ready())
        {
            const std::size_t offset = x + (y * m_meta.row_pitch);
            return CByteRectView{ m_view.subview(offset), meta };
        }
    }
    return CByteRectView{};
}

//==============================================================================
//  CByteRectConstView out of class function bodies
//==============================================================================

inline CByteRectConstView& CByteRectConstView::set(const std::uint8_t* const data, const std::size_t row_pitch, const std::size_t row_width, const std::size_t row_count, const std::size_t align) noexcept
{
    if ((data != nullptr) && m_meta.set(row_pitch, row_width, row_count))
    {
        m_view.set(data, align);
    }
    else
    {
        reset();
    }
    return *this;
}

inline CByteRectConstView& CByteRectConstView::set(const memory::CMemoryConstView& view, const MetaByteRectView& meta) noexcept
{
    if (view.is_ready() && meta.is_ready())
    {
        m_view = view;
        m_meta = meta;
    }
    else
    {
        reset();
    }
    return *this;
}

[[nodiscard]] inline CByteRectConstView CByteRectConstView::subview(const std::size_t x, const std::size_t y, const std::size_t width, const std::size_t height) const noexcept
{
    if (is_ready())
    {
        const MetaByteRectView meta(m_meta.subview(x, y, width, height));
        if (meta.is_ready())
        {
            const std::size_t offset = x + (y * m_meta.row_pitch);
            return CByteRectConstView{ m_view.subview(offset), meta };
        }
    }
    return CByteRectConstView{};
}

#endif  //  #ifndef BYTE_BUFFERS_HPP_INCLUDED

