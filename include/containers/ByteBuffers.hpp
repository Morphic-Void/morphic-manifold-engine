
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   ByteBuffers.hpp
//  Author: Ritchie Brannan
//  Date:   22 Feb 26
//
//  POD byte buffer and byte-rect buffer utilities (noexcept containers)
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//  - Storage is raw bytes (std::uint8_t).
//  - Sizes, capacities, extents, and alignment values are in bytes.
//
//  Overview:
//  - CByteBuffer owns contiguous byte storage with optional spare capacity.
//  - CByteView and CByteConstView provide non-owning contiguous views.
//  - CByteRectBuffer owns rectangular byte storage with aligned row starts.
//  - CByteRectView and CByteRectConstView provide non-owning rect views.
//
//  Scope:
//  - Models byte storage only.
//  - No element, texel, pixel, or structure semantics are imposed.
//  - Higher-level interpretation belongs in wrapper layers.
//
//  Memory model:
//  - Storage ownership is provided by memory::CMemoryToken.
//  - Views are provided by memory::CMemoryView and
//    memory::CMemoryConstView.
//  - Logical extent metadata is stored separately from the storage handle.
//
//  Status model:
//  - is_valid() reports invariant validity.
//  - is_empty() reports logical emptiness.
//  - is_ready() reports a non-empty state suitable for access.
//  - Accessors are fail-safe and return null, zero, or empty results
//    when not ready.
//
//  Constness model:
//  - Const on buffers and mutable views does not imply immutable storage.
//  - Immutable access is provided by CByteConstView and CByteRectConstView.
//
//  Alignment model:
//  - Alignment applies to the base address or current view origin.
//  - Passing align == 0 (or row_align == 0) reuses existing alignment intent
//    when available, otherwise allocation-layer normalisation rules apply.
//  - Rect buffers derive row_pitch from row_width and alignment.
//  - Subviews may reduce alignment guarantees based on offset.
//
//  Contiguous model:
//  - size() is the logical byte extent.
//  - capacity() is the allocated byte extent.
//  - size() may be less than capacity().
//  - set_size() may expose uninitialised bytes.
//
//  Rect model:
//  - row_pitch is the byte step between rows.
//  - row_width is the active byte extent within each row.
//  - row_count is the number of rows.
//  - Rects are byte-contiguous only when row_width == row_pitch.
//  - byte_view() and byte_const_view() return empty when not contiguous.
//
//  Metadata invariants:
//
//  MetaByteView:
//      size == 0                                          (canonical empty)
//      size <= memory::k_max_elements
//
//  MetaByteBuffer:
//      {size == 0, capacity == 0}                         (canonical empty)
//      {size == 0, capacity != 0}                         (valid ready)
//      size <= capacity <= memory::k_max_elements
//
//  MetaByteRectView:
//      {0, 0, 0}                                          (canonical empty)
//      row_width <= row_pitch
//      row_pitch * row_count <= memory::k_max_elements
//      contiguous iff row_width == row_pitch
//
//  Notes:
//  - Detailed behavioural contracts are documented in ByteBuffers.md.
//  - On failure, mutating operations leave state unchanged unless
//    stated otherwise.
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
//  MetaByteView
//  Metadata for a contiguous byte view.
//
//  State model:
//  - size is the logical byte extent.
//  - size == 0 is the canonical empty state.
//  - ready requires size != 0 and size <= memory::k_max_elements.
//==============================================================================

struct MetaByteView
{
    std::size_t size = 0u;
    void reset() noexcept { size = 0u; }
    [[nodiscard]] bool is_valid() const noexcept { return size <= memory::k_max_elements; }
    [[nodiscard]] bool is_empty() const noexcept { return size == 0u; }
    [[nodiscard]] bool is_ready() const noexcept { return memory::in_non_empty_range(size, memory::k_max_elements); }
};

//==============================================================================
//  MetaByteBuffer
//  Metadata for an owning contiguous byte buffer.
//
//  State model:
//  - size is the logical byte extent.
//  - capacity is the allocated byte extent.
//  - {size == 0, capacity == 0} is the canonical empty state.
//  - {size == 0, capacity != 0} is a valid ready state.
//  - ready requires size <= capacity <= memory::k_max_elements.
//==============================================================================

struct MetaByteBuffer
{
    std::size_t size = 0u;
    std::size_t capacity = 0u;
    void reset() noexcept { size = capacity = 0u; }
    [[nodiscard]] bool is_valid() const noexcept { return (size <= capacity) && (capacity <= memory::k_max_elements); }
    [[nodiscard]] bool is_empty() const noexcept { return (size == 0u) || (capacity == 0u); }
    [[nodiscard]] bool is_ready() const noexcept { return (size <= capacity) && memory::in_non_empty_range(capacity, memory::k_max_elements); }
    [[nodiscard]] MetaByteView byte_view() const noexcept { return MetaByteView{ size }; }
};

//==============================================================================
//  CByteBuffer
//  Owning contiguous byte buffer with optional spare capacity.
//==============================================================================

class CByteBuffer
{
public:

    //  Default and deleted lifetime
    CByteBuffer() noexcept = default;
    CByteBuffer(const CByteBuffer&) noexcept = delete;
    CByteBuffer& operator=(const CByteBuffer&) noexcept = delete;

    //  Move/lifetime
    CByteBuffer(CByteBuffer&&) noexcept;
    CByteBuffer& operator=(CByteBuffer&&) noexcept;
    ~CByteBuffer() noexcept { deallocate(); };

    //  Status
    [[nodiscard]] bool is_valid() const noexcept { return m_token.is_valid() && m_meta.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_token.is_empty() || m_meta.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_token.is_ready() && m_meta.is_ready(); }

    //  Views
    [[nodiscard]] CByteView view() const noexcept;
    [[nodiscard]] CByteConstView const_view() const noexcept;

    //  Accessors
    [[nodiscard]] std::uint8_t* data() const noexcept { return m_meta.is_ready() ? m_token.data() : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return m_meta.is_ready() ? m_token.align() : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t size() const noexcept { return is_ready() ? m_meta.size : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t capacity() const noexcept { return is_ready() ? m_meta.capacity : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t available() const noexcept { return is_ready() ? (m_meta.capacity - m_meta.size) : std::size_t{ 0 }; }

    //  Append and logical size
    [[nodiscard]] bool append(const std::uint8_t* const data, const std::size_t size) noexcept;
    [[nodiscard]] bool append(const std::size_t size, const bool zero = true) noexcept;
    [[nodiscard]] bool set_size(const std::size_t size) noexcept;

    //  Allocation and capacity management
    [[nodiscard]] bool allocate(const std::size_t capacity, const std::size_t align = 0u) noexcept;
    [[nodiscard]] bool reallocate(const std::size_t size, const std::size_t capacity, const std::size_t align = 0u) noexcept;
    [[nodiscard]] bool resize(const std::size_t size, const std::size_t align = 0u) noexcept;
    [[nodiscard]] bool reserve(const std::size_t minimum_capacity, const std::size_t align = 0u) noexcept;
    [[nodiscard]] bool ensure_free(const std::size_t extra, const std::size_t align = 0u) noexcept;
    [[nodiscard]] bool shrink_to_fit() noexcept;
    [[nodiscard]] bool construct_and_copy_from(const CByteConstView& view) noexcept;
    void deallocate() noexcept;

    //  Utilities
    void zero_fill() const noexcept;

    //  Handoff
    memory::CMemoryToken disown(MetaByteBuffer& meta) noexcept { meta = m_meta; m_meta.reset(); return std::move(m_token); }

private:
    memory::CMemoryToken m_token;
    MetaByteBuffer m_meta;
};

//==============================================================================
//  CByteView
//  Non-owning mutable view over a contiguous byte range.
//==============================================================================

class CByteView
{
public:

    //  Default lifetime
    CByteView() noexcept = default;
    CByteView(CByteView&&) noexcept = default;
    CByteView& operator=(CByteView&&) noexcept = default;
    CByteView(const CByteView&) noexcept = default;
    CByteView& operator=(const CByteView&) noexcept = default;
    ~CByteView() noexcept = default;

    //  Construction
    CByteView(std::uint8_t* const data, const std::size_t size, const std::size_t align = 0u) noexcept { (void)set(data, size, align); }
    CByteView(const memory::CMemoryView& view, const MetaByteView& meta) noexcept { (void)set(view, meta); }

    //  View state
    CByteView& set(std::uint8_t* const data, const std::size_t size, const std::size_t align = 0u) noexcept;
    CByteView& set(const memory::CMemoryView& view, const MetaByteView& meta) noexcept;
    CByteView& reset() noexcept { m_view.reset(); m_meta.reset(); return *this; }

    //  Status
    [[nodiscard]] bool is_valid() const noexcept { return m_view.is_valid() && m_meta.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_view.is_empty() || m_meta.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_view.is_ready() && m_meta.is_ready(); }

    //  Derived views
    [[nodiscard]] CByteConstView const_view() const noexcept;
    [[nodiscard]] CByteView subview(const std::size_t offset, const std::size_t count) const noexcept;
    [[nodiscard]] CByteView head_to(const std::size_t count) const noexcept;
    [[nodiscard]] CByteView tail_from(const std::size_t offset) const noexcept;

    //  Accessors
    [[nodiscard]] std::uint8_t* data() const noexcept { return m_meta.is_ready() ? m_view.data() : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return m_meta.is_ready() ? m_view.align() : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t size() const noexcept { return is_ready() ? m_meta.size : std::size_t{ 0 }; }

    //  Utilities
    void zero_fill() const noexcept;

private:
    memory::CMemoryView m_view;
    MetaByteView m_meta;
};

//==============================================================================
//  CByteConstView
//  Non-owning immutable view over a contiguous byte range.
//==============================================================================

class CByteConstView
{
public:

    //  Default lifetime
    CByteConstView() noexcept = default;
    CByteConstView(CByteConstView&&) noexcept = default;
    CByteConstView& operator=(CByteConstView&&) noexcept = default;
    CByteConstView(const CByteConstView&) noexcept = default;
    CByteConstView& operator=(const CByteConstView&) noexcept = default;
    ~CByteConstView() noexcept = default;

    //  Construction
    CByteConstView(const std::uint8_t* const data, const std::size_t size, const std::size_t align = 0u) noexcept { (void)set(data, size, align); }
    CByteConstView(const memory::CMemoryConstView& view, const MetaByteView& meta) noexcept { (void)set(view, meta); }

    //  View state
    CByteConstView& set(const std::uint8_t* const data, const std::size_t size, const std::size_t align = 0u) noexcept;
    CByteConstView& set(const memory::CMemoryConstView& view, const MetaByteView& meta) noexcept;
    CByteConstView& reset() noexcept { m_view.reset(); m_meta.reset(); return *this; }

    //  Status
    [[nodiscard]] bool is_valid() const noexcept { return m_view.is_valid() && m_meta.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_view.is_empty() || m_meta.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_view.is_ready() && m_meta.is_ready(); }

    //  Derived views
    [[nodiscard]] CByteConstView subview(const std::size_t offset, const std::size_t count) const noexcept;
    [[nodiscard]] CByteConstView head_to(const std::size_t count) const noexcept;
    [[nodiscard]] CByteConstView tail_from(const std::size_t offset) const noexcept;

    //  Accessors
    [[nodiscard]] const std::uint8_t* data() const noexcept { return m_meta.is_ready() ? m_view.data() : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return m_meta.is_ready() ? m_view.align() : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t size() const noexcept { return is_ready() ? m_meta.size : std::size_t{ 0 }; }

private:
    memory::CMemoryConstView m_view;
    MetaByteView m_meta;
};

//==============================================================================
//  MetaByteRectView
//  Metadata for a rectangular byte range.
//
//  Rect model:
//  - row_pitch is the byte step from one row start to the next.
//  - row_width is the active byte extent within each row.
//  - row_count is the number of rows.
//  - contiguous iff row_width == row_pitch.
//
//  State model:
//  - {0, 0, 0} is the canonical empty state.
//  - non-empty states require all three fields to be non-zero.
//  - ready requires row_width <= row_pitch and
//    row_pitch * row_count <= memory::k_max_elements.
//==============================================================================

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

//==============================================================================
//  MetaByteRectBuffer
//  Owning rect buffers use the same metadata model as rect views.
//==============================================================================

using MetaByteRectBuffer = MetaByteRectView;

//==============================================================================
//  CByteRectBuffer
//  Owning rectangular byte buffer with aligned row starts.
//==============================================================================

class CByteRectBuffer
{
public:

    //  Default and deleted lifetime
    CByteRectBuffer() noexcept = default;
    CByteRectBuffer(const CByteRectBuffer&) noexcept = delete;
    CByteRectBuffer& operator=(const CByteRectBuffer&) noexcept = delete;

    //  Move/lifetime
    CByteRectBuffer(CByteRectBuffer&&) noexcept;
    CByteRectBuffer& operator=(CByteRectBuffer&&) noexcept;
    ~CByteRectBuffer() noexcept { deallocate(); };

    //  Status
    [[nodiscard]] bool is_valid() const noexcept { return m_token.is_valid() && m_meta.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_token.is_empty() || m_meta.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_token.is_ready() && m_meta.is_ready(); }
    [[nodiscard]] bool is_contiguous() const noexcept { return m_token.is_ready() && m_meta.is_contiguous(); }

    //  Views
    [[nodiscard]] CByteRectView view() const noexcept;
    [[nodiscard]] CByteRectConstView const_view() const noexcept;

    //  Contiguous byte views
    [[nodiscard]] CByteView byte_view() const noexcept { return is_contiguous() ? CByteView{ m_token.view(), m_meta.byte_view() } : CByteView{}; }
    [[nodiscard]] CByteConstView byte_const_view() const noexcept { return is_contiguous() ? CByteConstView{ m_token.const_view(), m_meta.byte_view() } : CByteConstView{}; }

    //  Accessors
    [[nodiscard]] std::uint8_t* data() const noexcept { return m_meta.is_ready() ? m_token.data() : nullptr; }
    [[nodiscard]] std::uint8_t* row_data(const std::size_t y) const noexcept { return (is_ready() && (y < m_meta.row_count)) ? (m_token.data() + (y * m_meta.row_pitch)) : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return m_meta.is_ready() ? m_token.align() : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_pitch() const noexcept { return is_ready() ? m_meta.row_pitch : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_width() const noexcept { return is_ready() ? m_meta.row_width : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_count() const noexcept { return is_ready() ? m_meta.row_count : std::size_t{ 0 }; }

    //  Allocation and ownership
    [[nodiscard]] bool allocate(const std::size_t row_width, const std::size_t row_count, const std::size_t row_align = 0u, const bool zero = true) noexcept;
    [[nodiscard]] bool reallocate(const std::size_t row_width, const std::size_t row_count, const std::size_t row_align = 0u, const bool zero_uninitialised = true) noexcept;
    [[nodiscard]] bool construct_and_copy_from(const CByteRectConstView& view) noexcept;
    void deallocate() noexcept;

    //  Utilities
    void zero_fill() const noexcept;

    //  Handoff
    memory::CMemoryToken disown(MetaByteRectBuffer& meta) noexcept { meta = m_meta; m_meta.reset(); return std::move(m_token); }

private:
    memory::CMemoryToken m_token;
    MetaByteRectBuffer m_meta;
};

//==============================================================================
//  CByteRectView
//  Non-owning mutable view over a rectangular byte range.
//==============================================================================

class CByteRectView
{
public:

    //  Default lifetime
    CByteRectView() noexcept = default;
    CByteRectView(CByteRectView&&) noexcept = default;
    CByteRectView& operator=(CByteRectView&&) noexcept = default;
    CByteRectView(const CByteRectView&) noexcept = default;
    CByteRectView& operator=(const CByteRectView&) noexcept = default;
    ~CByteRectView() noexcept = default;

    //  Construction
    CByteRectView(std::uint8_t* const data, const std::size_t row_pitch, const std::size_t row_width, const std::size_t row_count, const std::size_t align = 0u) noexcept { (void)set(data, row_pitch, row_width, row_count, align); }
    CByteRectView(const memory::CMemoryView& view, const MetaByteRectView& meta) noexcept { (void)set(view, meta); }

    //  View state
    CByteRectView& set(std::uint8_t* const data, const std::size_t row_pitch, const std::size_t row_width, const std::size_t row_count, const std::size_t align = 0u) noexcept;
    CByteRectView& set(const memory::CMemoryView& view, const MetaByteRectView& meta) noexcept;
    CByteRectView& reset() noexcept { m_view.reset(); m_meta.reset(); return *this; }

    //  Status
    [[nodiscard]] bool is_valid() const noexcept { return m_view.is_valid() && m_meta.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_view.is_empty() || m_meta.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_view.is_ready() && m_meta.is_ready(); }
    [[nodiscard]] bool is_contiguous() const noexcept { return m_view.is_ready() && m_meta.is_contiguous(); }

    //  Derived views
    [[nodiscard]] CByteRectConstView const_view() const noexcept;
    [[nodiscard]] CByteRectView subview(const std::size_t x, const std::size_t y, const std::size_t width, const std::size_t height) const noexcept;

    //  Contiguous byte views
    [[nodiscard]] CByteView byte_view() const noexcept { return is_contiguous() ? CByteView{ m_view, m_meta.byte_view() } : CByteView{}; }
    [[nodiscard]] CByteConstView byte_const_view() const noexcept { return is_contiguous() ? CByteConstView{ m_view.const_view(), m_meta.byte_view() } : CByteConstView{}; }

    //  Accessors
    [[nodiscard]] std::uint8_t* data() const noexcept { return m_meta.is_ready() ? m_view.data() : nullptr; }
    [[nodiscard]] std::uint8_t* row_data(const std::size_t y) const noexcept { return (is_ready() && (y < m_meta.row_count)) ? (m_view.data() + (y * m_meta.row_pitch)) : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return m_meta.is_ready() ? m_view.align() : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_pitch() const noexcept { return is_ready() ? m_meta.row_pitch : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_width() const noexcept { return is_ready() ? m_meta.row_width : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t row_count() const noexcept { return is_ready() ? m_meta.row_count : std::size_t{ 0 }; }

    //  Utilities
    void zero_fill() const noexcept;

private:
    memory::CMemoryView m_view;
    MetaByteRectView m_meta;
};

//==============================================================================
//  CByteRectConstView
//  Non-owning immutable view over a rectangular byte range.
//==============================================================================

class CByteRectConstView
{
public:

    //  Default lifetime
    CByteRectConstView() noexcept = default;
    CByteRectConstView(CByteRectConstView&&) noexcept = default;
    CByteRectConstView& operator=(CByteRectConstView&&) noexcept = default;
    CByteRectConstView(const CByteRectConstView&) noexcept = default;
    CByteRectConstView& operator=(const CByteRectConstView&) noexcept = default;
    ~CByteRectConstView() noexcept = default;

    //  Construction
    CByteRectConstView(const std::uint8_t* const data, const std::size_t row_pitch, const std::size_t row_width, const std::size_t row_count, const std::size_t align = 0u) noexcept { (void)set(data, row_pitch, row_width, row_count, align); }
    CByteRectConstView(const memory::CMemoryConstView& view, const MetaByteRectView& meta) noexcept { (void)set(view, meta); }

    //  View state
    CByteRectConstView& set(const std::uint8_t* const data, const std::size_t row_pitch, const std::size_t row_width, const std::size_t row_count, const std::size_t align = 0u) noexcept;
    CByteRectConstView& set(const memory::CMemoryConstView& view, const MetaByteRectView& meta) noexcept;
    CByteRectConstView& reset() noexcept { m_view.reset(); m_meta.reset(); return *this; }

    //  Status
    [[nodiscard]] bool is_valid() const noexcept { return m_view.is_valid() && m_meta.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_view.is_empty() || m_meta.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_view.is_ready() && m_meta.is_ready(); }
    [[nodiscard]] bool is_contiguous() const noexcept { return m_view.is_ready() && m_meta.is_contiguous(); }

    //  Derived views
    [[nodiscard]] CByteRectConstView subview(const std::size_t x, const std::size_t y, const std::size_t width, const std::size_t height) const noexcept;

    //  Contiguous byte views
    [[nodiscard]] CByteConstView byte_view() const noexcept { return is_contiguous() ? CByteConstView{ m_view, m_meta.byte_view() } : CByteConstView{}; }

    //  Accessors
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
//  MetaByteRectView out of class function bodies
//==============================================================================

[[nodiscard]] inline bool MetaByteRectView::set(const std::size_t pitch, const std::size_t width, const std::size_t count) noexcept
{
    using memory::in_non_empty_range;
    if (in_non_empty_range(width, pitch) && in_non_empty_range(count, (memory::k_max_elements / pitch)))
    {
        row_pitch = pitch;
        row_width = width;
        row_count = count;
        return true;
    }
    return false;
}

[[nodiscard]] inline MetaByteRectView MetaByteRectView::subview(const std::size_t x, const std::size_t y, const std::size_t width, const std::size_t height) const noexcept
{
    if (is_ready() &&
        (std::max(x, (width - 1u)) < row_width) && (x <= (row_width - width)) &&
        (std::max(y, (height - 1u)) < row_count) && (y <= (row_count - height)))
    {
        return MetaByteRectView{ row_pitch, width, height };
    }
    return MetaByteRectView{};
}

[[nodiscard]] inline bool MetaByteRectView::range_ok() const noexcept
{
    using memory::in_non_empty_range;
    return
        in_non_empty_range(row_pitch, memory::k_max_elements) &&
        in_non_empty_range(row_width, row_pitch) &&
        in_non_empty_range(row_count, (memory::k_max_elements / row_pitch));
}

//==============================================================================
//  CByteBuffer out of class function bodies
//==============================================================================

inline CByteBuffer::CByteBuffer(CByteBuffer&& other) noexcept
{
    m_token = other.disown(m_meta);
}

inline CByteBuffer& CByteBuffer::operator=(CByteBuffer&& other) noexcept
{
    if (this != &other)
    {
        m_token = other.disown(m_meta);
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

[[nodiscard]] inline bool CByteBuffer::append(const std::uint8_t* const data, const std::size_t size) noexcept
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

[[nodiscard]] inline bool CByteBuffer::append(const std::size_t size, const bool zero) noexcept
{
    if (size == 0u)
    {
        return is_valid();
    }

    if (ensure_free(size))
    {
        if (zero)
        {
            std::memset((m_token.data() + m_meta.size), 0, size);
        }
        m_meta.size += size;
        return true;
    }
    return false;
}

[[nodiscard]] inline bool CByteBuffer::set_size(const std::size_t size) noexcept
{
    if (is_ready() && (size <= m_meta.capacity))
    {
        m_meta.size = size;
        return true;
    }
    return false;
}

[[nodiscard]] inline bool CByteBuffer::allocate(const std::size_t capacity, const std::size_t align) noexcept
{
    return reallocate(0u, capacity, align);
}

[[nodiscard]] inline bool CByteBuffer::reallocate(const std::size_t size, const std::size_t capacity, const std::size_t align) noexcept
{
    if (is_valid() && (size <= capacity) && (capacity <= memory::k_max_elements))
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
                MV_HARD_ASSERT(m_token.align() == norm_align);
                MV_HARD_ASSERT(m_meta.size == size);
                MV_HARD_ASSERT(m_meta.capacity == capacity);
                MV_HARD_ASSERT(is_ready());
                return true;
            }
            return false;
        }
    }
    return false;
}

[[nodiscard]] inline bool CByteBuffer::resize(const std::size_t size, const std::size_t align) noexcept
{
    return (size <= memory::k_max_elements) ? reallocate(size, ((size > m_meta.capacity) ? memory::buffer_growth_policy(size) : m_meta.capacity), align) : false;
}

[[nodiscard]] inline bool CByteBuffer::reserve(const std::size_t minimum_capacity, const std::size_t align) noexcept
{
    return (minimum_capacity <= memory::k_max_elements) ? reallocate(m_meta.size, std::max(memory::buffer_growth_policy(minimum_capacity), m_meta.capacity), align) : false;
}

[[nodiscard]] inline bool CByteBuffer::ensure_free(const std::size_t extra, const std::size_t align) noexcept
{
    return (extra <= (memory::k_max_elements - m_meta.size)) ? reserve((m_meta.size + extra), align) : false;
}

[[nodiscard]] inline bool CByteBuffer::shrink_to_fit() noexcept
{
    return reallocate(m_meta.size, m_meta.size, m_token.align());
}

[[nodiscard]] inline bool CByteBuffer::construct_and_copy_from(const CByteConstView& view) noexcept
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
            MV_HARD_ASSERT(m_token.align() == view.align());
            MV_HARD_ASSERT(m_meta.size == view.size());
            MV_HARD_ASSERT(m_meta.capacity == view.size());
            MV_HARD_ASSERT(is_ready());
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

inline void CByteBuffer::zero_fill() const noexcept
{
    std::uint8_t* fill = data();
    if (fill != nullptr)
    {
        std::memset(fill, 0, m_meta.size);
    }
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

inline void CByteView::zero_fill() const noexcept
{
    std::uint8_t* fill = data();
    if (fill != nullptr)
    {
        std::memset(fill, 0, m_meta.size);
    }
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
//  CByteRectBuffer out of class function bodies
//==============================================================================

inline CByteRectBuffer::CByteRectBuffer(CByteRectBuffer&& other) noexcept
{
    m_token = other.disown(m_meta);
}

inline CByteRectBuffer& CByteRectBuffer::operator=(CByteRectBuffer&& other) noexcept
{
    if (this != &other)
    {
        m_token = other.disown(m_meta);
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

[[nodiscard]] inline bool CByteRectBuffer::allocate(const std::size_t row_width, const std::size_t row_count, const std::size_t row_align, const bool zero) noexcept
{
    if (reallocate(row_width, row_count, row_align, false))
    {
        if (zero)
        {
            std::memset(m_token.data(), 0, (m_meta.row_pitch * m_meta.row_count));
        }
        return true;
    }
    return false;
}

[[nodiscard]] inline bool CByteRectBuffer::reallocate(const std::size_t row_width, const std::size_t row_count, const std::size_t row_align, const bool zero_uninitialised) noexcept
{
    using memory::in_non_empty_range;
    if ((row_width == 0u) && (row_count == 0u))
    {   //  this is a deallocation request
        deallocate();
        return true;
    }

    if (in_non_empty_range(row_width, memory::k_max_elements))
    {   //  row_width is valid
        const std::size_t use_align = memory::util::norm_align((row_align != 0u) ? row_align : m_token.align());
        const std::size_t row_pitch = bit_ops::round_up_to_pow2_multiple(row_width, use_align);
        if (in_non_empty_range(row_count, (memory::k_max_elements / row_pitch)))
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
                        {   //  needs row by row copying and optional zeroing
                            const std::size_t copy_width = std::min(row_width, m_meta.row_width);
                            const std::size_t zero_width = zero_uninitialised ? (row_pitch - copy_width) : std::size_t{ 0 };
                            for (std::size_t count = std::min(row_count, m_meta.row_count); count != 0u; --count)
                            {
                                std::memcpy(dst_data, src_data, copy_width);
                                if (zero_width != 0u)
                                {
                                    std::memset((dst_data + copy_width), 0, zero_width);
                                }
                                src_data += m_meta.row_pitch;
                                dst_data += row_pitch;
                            }
                        }
                        if (zero_uninitialised && (row_count > m_meta.row_count))
                        {   //  there is extra accessible space to zero below the image
                            std::memset((token.data() + (row_pitch * m_meta.row_count)), 0, (row_pitch * (row_count - m_meta.row_count)));
                        }
                    }
                    else if (zero_uninitialised)
                    {   //  primary allocation zero
                        std::memset(token.data(), 0, bytes);
                    }
                    MetaByteRectBuffer meta{ row_pitch, row_width, row_count };
                    m_token = std::move(token);
                    m_meta = meta;
                    MV_HARD_ASSERT(m_token.align() == use_align);
                    MV_HARD_ASSERT(m_meta.row_pitch == row_pitch);
                    MV_HARD_ASSERT(m_meta.row_width == row_width);
                    MV_HARD_ASSERT(m_meta.row_count == row_count);
                    MV_HARD_ASSERT(is_ready());
                    return true;
                }
            }
        }
    }
    return false;
}

[[nodiscard]] inline bool CByteRectBuffer::construct_and_copy_from(const CByteRectConstView& view) noexcept
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
            {   //  needs row by row copying, with destination tail zeroing if present
                const std::size_t zero_width = row_pitch - row_width;
                for (std::size_t count = row_count; count != 0u; --count)
                {
                    std::memcpy(dst_data, src_data, row_width);
                    if (zero_width != 0u)
                    {
                        std::memset((dst_data + row_width), 0, zero_width);
                    }
                    src_data += src_pitch;
                    dst_data += row_pitch;
                }
            }
            MetaByteRectBuffer meta{ row_pitch, row_width, row_count };
            m_token = std::move(token);
            m_meta = meta;
            MV_HARD_ASSERT(m_token.align() == row_align);
            MV_HARD_ASSERT(m_meta.row_pitch == row_pitch);
            MV_HARD_ASSERT(m_meta.row_width == row_width);
            MV_HARD_ASSERT(m_meta.row_count == row_count);
            MV_HARD_ASSERT(is_ready());
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

inline void CByteRectBuffer::zero_fill() const noexcept
{
    std::uint8_t* fill = data();
    if (fill != nullptr)
    {
        if (m_meta.is_contiguous())
        {
            std::memset(fill, 0, (m_meta.row_pitch * m_meta.row_count));
        }
        else
        {
            for (std::size_t count = m_meta.row_count; count != 0; --count)
            {
                std::memset(fill, 0, m_meta.row_width);
                fill += m_meta.row_pitch;
            }
        }
    }
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

inline void CByteRectView::zero_fill() const noexcept
{
    std::uint8_t* fill = data();
    if (fill != nullptr)
    {
        if (m_meta.is_contiguous())
        {
            std::memset(fill, 0, (m_meta.row_pitch * m_meta.row_count));
        }
        else
        {
            for (std::size_t count = m_meta.row_count; count != 0; --count)
            {
                std::memset(fill, 0, m_meta.row_width);
                fill += m_meta.row_pitch;
            }
        }
    }
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

