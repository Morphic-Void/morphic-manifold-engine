
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
// 
//  File:   memory_primitives.hpp
//  Author: Ritchie Brannan
//  Date:   22 Feb 26
//
//  Raw memory ownership and view primitives.
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//  - Alignment values are expressed in bytes.
//
//  Overview:
//  - CMemoryToken provides owning raw byte storage.
//  - CMemoryView and CMemoryConstView provide non-owning byte views.
//  - TMemoryToken<T> provides owning typed storage over T[].
//  - TMemoryView<T> and TMemoryConstView<T> provide typed views.
//  - Byte and typed views support explicit adoption across the byte/typed
//    boundary where compatibility permits.
//
//  Scope:
//  - This layer models raw storage and address alignment only.
//  - It does not track allocation size beyond caller-provided arguments.
//  - It does not perform bounds checking.
//  - Extent validation belongs to higher layers.
//  - Typed variants reinterpret storage as tightly packed T[] only.
//  - No construction, destruction, or non-trivial relocation is performed.
//
//  State model:
//  - Canonical empty state is {data == nullptr, align == 0}.
//  - Canonical ready state is {data != nullptr, align != 0}.
//  - Broken invariant states, handled fail-safely if encountered, are:
//      * {data == nullptr, align != 0}
//      * {data != nullptr, align == 0}
//
//  Observation model:
//  - Pointer and align observers are fail-safe:
//      * with broken invariants they report the canonical empty state
//  - Readiness and emptiness observers are fail-safe:
//      * with broken invariants they report not-ready / empty
//  - Validity observers report invariant validity directly.
//
//  Alignment model:
//  - For CMemoryToken, align is the stored normalised alignment intent.
//  - For CMemoryView and CMemoryConstView, align is the currently guaranteed
//    alignment of the pointed-to address and may be less than actual.
//  - View align may reduce when subviews are created from byte offsets.
//  - Typed variants derive alignment from T and the underlying allocation.
//  - Typed subviews advance in whole T elements and so preserve T alignment.
//
//  Constness model:
//  - const on CMemoryToken and CMemoryView applies to the wrapper object only.
//  - It does not imply immutable access to the referenced memory.
//  - Read-only access to referenced memory is represented only by
//    CMemoryConstView and TMemoryConstView<T>.
//
//  Typed model:
//  - TMemoryToken<T> manages storage as a contiguous T[] region.
//  - TMemoryView<T> and TMemoryConstView<T> provide typed access without
//    additional metadata.
//  - Element count is provided externally to allocation/reallocation.
//  - Typed reallocation requires trivially copyable T.
//  - No element lifetime management is performed in this layer.
//
//  Adoption model:
//  - Byte views may adopt typed views directly as a weakening projection.
//  - Typed views may adopt byte views only when the pointed-to address and
//    guaranteed alignment are compatible with T.
//  - Canonical empty state is preserved across byte/typed adoption.
//  - Failed typed adoption leaves the destination typed view unchanged.
//  - Extent compatibility for typed ranges belongs to higher layers.
//
//  Relationship between byte and typed forms:
//  - Byte primitives define the canonical ownership and alignment model.
//  - Typed primitives are a thin reinterpretation layer over raw storage.
//  - Explicit adoption functions provide the checked crossing points between
//    byte and typed view forms.
//  - Both forms share the same fail-safe and invariant semantics.

#pragma once

#ifndef MEMORY_PRIMITIVES_HPP_INCLUDED
#define MEMORY_PRIMITIVES_HPP_INCLUDED

#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::uint8_t
#include <cstring>      //  std::memset
#include <type_traits>  //  std::is_trivially_copyable

#include "memory_allocation.hpp"
#include "bit_utils/bit_ops.hpp"
#include "debug/debug.hpp"

namespace memory
{

//==============================================================================
//  Shared utility functions
//==============================================================================

namespace util
{

[[nodiscard]] inline bool is_valid(const std::uint8_t* const data, const std::size_t align) noexcept
{
    return (data == nullptr) == (align == 0u);
}

[[nodiscard]] inline bool is_empty(const std::uint8_t* const data, const std::size_t align) noexcept
{
    return (data == nullptr) || (align == 0u);
}

[[nodiscard]] inline bool is_ready(const std::uint8_t* const data, const std::size_t align) noexcept
{
    return (data != nullptr) && (align != 0u);
}

[[nodiscard]] inline std::uint8_t* data(std::uint8_t* const data, const std::size_t align) noexcept
{
    return (align != 0u) ? data : nullptr;
}

[[nodiscard]] inline const std::uint8_t* data(const std::uint8_t* const data, const std::size_t align) noexcept
{
    return (align != 0u) ? data : nullptr;
}

[[nodiscard]] inline std::size_t align(const std::uint8_t* const data, const std::size_t align) noexcept
{
    return (data != nullptr) ? align : std::size_t{ 0 };
}

[[nodiscard]] inline std::size_t norm_align(const std::size_t align) noexcept
{
    return bit_ops::reduce_alignment_to_pow2(align);
}

[[nodiscard]] inline std::size_t offset_align(const std::size_t align, const std::size_t offset) noexcept
{
    return bit_ops::highest_common_alignment(align, offset);
}

[[nodiscard]] inline std::size_t common_align(const std::size_t align, const std::size_t other_align) noexcept
{
    return bit_ops::highest_common_alignment(align, other_align);
}

}   //  namespace util

//==============================================================================
//  Forward declarations
//==============================================================================

class CMemoryToken;
class CMemoryView;
class CMemoryConstView;
template<typename T> class TMemoryToken;
template<typename T> class TMemoryView;
template<typename T> class TMemoryConstView;

//==============================================================================
//  CMemoryToken
//==============================================================================

class CMemoryToken
{
public:

    //  Default and deleted lifetime
    CMemoryToken() noexcept = default;
    CMemoryToken(const CMemoryToken&) noexcept = delete;
    CMemoryToken& operator=(const CMemoryToken&) noexcept = delete;

    //  Move lifetime
    CMemoryToken(CMemoryToken&&) noexcept;
    CMemoryToken& operator=(CMemoryToken&&) noexcept;
    ~CMemoryToken() noexcept { deallocate(); }

    //  Status
    [[nodiscard]] bool is_valid() const noexcept { return util::is_valid(m_data, m_align); }
    [[nodiscard]] bool is_empty() const noexcept { return util::is_empty(m_data, m_align); }
    [[nodiscard]] bool is_ready() const noexcept { return util::is_ready(m_data, m_align); }

    //  Views
    [[nodiscard]] CMemoryView view() const noexcept;
    [[nodiscard]] CMemoryConstView const_view() const noexcept;

    //  Common accessors (see constness model above)
    [[nodiscard]] std::uint8_t* data() const noexcept { return util::data(m_data, m_align); }
    [[nodiscard]] std::size_t align() const noexcept { return util::align(m_data, m_align); }

    //  Capacity management (state unchanged on failure)
    [[nodiscard]] bool allocate(const std::size_t size, const std::size_t align, const bool zero = true) noexcept;
    [[nodiscard]] bool reallocate(const std::size_t old_size, const std::size_t new_size, const std::size_t align, const bool zero_extra = true) noexcept;
    void deallocate() noexcept;

private:
    std::uint8_t* m_data = nullptr;
    std::size_t m_align = 0u;
};

//==============================================================================
//  CMemoryView
//==============================================================================

class CMemoryView
{
public:

    //  Default lifetime
    CMemoryView() noexcept = default;
    CMemoryView(CMemoryView&&) noexcept = default;
    CMemoryView& operator=(CMemoryView&&) noexcept = default;
    CMemoryView(const CMemoryView&) noexcept = default;
    CMemoryView& operator=(const CMemoryView&) noexcept = default;
    ~CMemoryView() noexcept = default;

    //  Construction
    CMemoryView(std::uint8_t* const data, const std::size_t align) noexcept { (void)set(data, align); }

    //  View state
    CMemoryView& set(std::uint8_t* const data, const std::size_t align) noexcept;
    CMemoryView& reset() noexcept { m_data = nullptr; m_align = 0u; return *this; }

    //  Adoption
    template<typename T> void adopt(const TMemoryView<T>& view) noexcept;

    //  Status
    [[nodiscard]] bool is_valid() const noexcept { return util::is_valid(m_data, m_align); }
    [[nodiscard]] bool is_empty() const noexcept { return util::is_empty(m_data, m_align); }
    [[nodiscard]] bool is_ready() const noexcept { return util::is_ready(m_data, m_align); }

    //  Views and sub-views (offset parameter validation is a caller responsibility)
    [[nodiscard]] CMemoryView subview(const std::size_t offset = 0u) const noexcept;
    [[nodiscard]] CMemoryConstView const_view() const noexcept;

    //  Common accessors (see constness model above)
    [[nodiscard]] std::uint8_t* data() const noexcept { return util::data(m_data, m_align); }
    [[nodiscard]] std::size_t align() const noexcept { return util::align(m_data, m_align); }

    //  Constants
    static constexpr std::size_t k_max_elements = t_max_elements<std::uint8_t>();
    static constexpr std::size_t k_element_size = sizeof(std::uint8_t);

private:
    std::uint8_t* m_data = nullptr;
    std::size_t m_align = 0u;
};

//==============================================================================
//  CMemoryConstView
//==============================================================================

class CMemoryConstView
{
public:

    //  Default lifetime
    CMemoryConstView() noexcept = default;
    CMemoryConstView(CMemoryConstView&&) noexcept = default;
    CMemoryConstView& operator=(CMemoryConstView&&) noexcept = default;
    CMemoryConstView(const CMemoryConstView&) noexcept = default;
    CMemoryConstView& operator=(const CMemoryConstView&) noexcept = default;
    ~CMemoryConstView() noexcept = default;

    //  Construction and conversion
    CMemoryConstView(const std::uint8_t* const data, const std::size_t align) noexcept { (void)set(data, align); }
    CMemoryConstView(const CMemoryView& view) noexcept { (void)set(view); }
    CMemoryConstView& operator=(const CMemoryView& view) noexcept { return set(view); }

    //  View state
    CMemoryConstView& set(const std::uint8_t* const data, const std::size_t align) noexcept;
    CMemoryConstView& set(const CMemoryView& view) noexcept;
    CMemoryConstView& reset() noexcept { m_data = nullptr; m_align = 0u; return *this; }

    //  Adoption
    template<typename T> void adopt(const TMemoryView<T>& view) noexcept;
    template<typename T> void adopt(const TMemoryConstView<T>& view) noexcept;

    //  Status
    [[nodiscard]] bool is_valid() const noexcept { return util::is_valid(m_data, m_align); }
    [[nodiscard]] bool is_empty() const noexcept { return util::is_empty(m_data, m_align); }
    [[nodiscard]] bool is_ready() const noexcept { return util::is_ready(m_data, m_align); }

    //  Views and sub-views (offset parameter validation is a caller responsibility)
    [[nodiscard]] CMemoryConstView subview(const std::size_t offset = 0u) const noexcept;

    //  Common accessors (read-only memory access)
    [[nodiscard]] const std::uint8_t* data() const noexcept { return util::data(m_data, m_align); }
    [[nodiscard]] std::size_t align() const noexcept { return util::align(m_data, m_align); }

    //  Constants
    static constexpr std::size_t k_max_elements = t_max_elements<std::uint8_t>();
    static constexpr std::size_t k_element_size = sizeof(std::uint8_t);

private:
    const std::uint8_t* m_data = nullptr;
    std::size_t m_align = 0u;
};

//==============================================================================
//  TMemoryToken
//==============================================================================

template<typename T>
class TMemoryToken
{
public:

    //  Default and deleted lifetime
    TMemoryToken() noexcept = default;
    TMemoryToken(const TMemoryToken&) noexcept = delete;
    TMemoryToken& operator=(const TMemoryToken&) noexcept = delete;

    //  Move lifetime
    TMemoryToken(TMemoryToken&& other) noexcept;
    TMemoryToken& operator=(TMemoryToken&& other) noexcept;
    ~TMemoryToken() noexcept { deallocate(); }

    //  Views
    [[nodiscard]] TMemoryView<T> view() const noexcept { return TMemoryView<T>{ m_data }; }
    [[nodiscard]] TMemoryConstView<T> const_view() const noexcept { return TMemoryConstView<T>{ m_data }; }

    //  Common accessors (see constness model above)
    [[nodiscard]] T* data() const noexcept { return m_data; }

    //  Capacity management (state unchanged on failure)
    [[nodiscard]] bool allocate(const std::size_t size, const bool zero = true) noexcept;
    [[nodiscard]] bool reallocate(const std::size_t old_size, const std::size_t new_size, const bool zero_extra = true) noexcept;
    [[nodiscard]] bool clone(const TMemoryToken<T> src, const std::size_t size) const noexcept;
    void deallocate() noexcept;

    //  Constants
    static constexpr std::size_t k_max_elements = t_max_elements<T>();
    static constexpr std::size_t k_element_size = sizeof(T);
    static constexpr std::size_t k_align = t_default_align<T>();

private:
    T* m_data = nullptr;
};

//==============================================================================
//  TMemoryView
//==============================================================================

template<typename T>
class TMemoryView
{
public:

    //  Default lifetime
    TMemoryView() noexcept = default;
    TMemoryView(TMemoryView&&) noexcept = default;
    TMemoryView& operator=(TMemoryView&&) noexcept = default;
    TMemoryView(const TMemoryView&) noexcept = default;
    TMemoryView& operator=(const TMemoryView&) noexcept = default;
    ~TMemoryView() noexcept = default;

    //  Construction
    TMemoryView(T* const data) noexcept { m_data = data; }

    //  View state
    TMemoryView& set(T* const data) noexcept { m_data = data; return *this; }
    TMemoryView& reset() noexcept { m_data = nullptr; return *this; }

    //  Adoption
    [[nodiscard]] static bool can_adopt(const std::uint8_t* const data) noexcept;
    [[nodiscard]] static bool can_adopt(const std::uint8_t* const data, const std::size_t align) noexcept;
    [[nodiscard]] static bool can_adopt(const CMemoryView& view) noexcept;
    [[nodiscard]] bool adopt(const std::uint8_t* data) noexcept;
    [[nodiscard]] bool adopt(const std::uint8_t* data, const std::size_t align) noexcept;
    [[nodiscard]] bool adopt(const CMemoryView& view) noexcept;

    //  Views and sub-views (offset parameter validation is a caller responsibility)
    [[nodiscard]] TMemoryView<T> subview(const std::size_t offset = 0u) const noexcept;
    [[nodiscard]] TMemoryConstView<T> const_view() const noexcept { return TMemoryConstView<T>{ m_data }; }

    //  Common accessors (see constness model above)
    [[nodiscard]] T* data() const noexcept { return m_data; }

    //  Constants
    static constexpr std::size_t k_max_elements = t_max_elements<T>();
    static constexpr std::size_t k_element_size = sizeof(T);
    static constexpr std::size_t k_align = t_default_align<T>();

private:
    T* m_data = nullptr;
};

//==============================================================================
//  TMemoryConstView
//==============================================================================

template<typename T>
class TMemoryConstView
{
public:

    //  Default lifetime
    TMemoryConstView() noexcept = default;
    TMemoryConstView(TMemoryConstView&&) noexcept = default;
    TMemoryConstView& operator=(TMemoryConstView&&) noexcept = default;
    TMemoryConstView(const TMemoryConstView&) noexcept = default;
    TMemoryConstView& operator=(const TMemoryConstView&) noexcept = default;
    ~TMemoryConstView() noexcept = default;

    //  Construction and conversion
    TMemoryConstView(const T* const data) noexcept { m_data = data; }
    TMemoryConstView(const TMemoryView<T>& view) noexcept { m_data = view.data(); }
    TMemoryConstView& operator=(const TMemoryView<T>& view) noexcept { m_data = view.data(); return *this; }

    //  View state
    TMemoryConstView& set(const T* const data) noexcept { m_data = data; return *this; }
    TMemoryConstView& set(const TMemoryView<T>& view) noexcept { m_data = view.data(); return *this; }
    TMemoryConstView& reset() noexcept { m_data = nullptr; return *this; }

    //  Adoption
    [[nodiscard]] static bool can_adopt(const std::uint8_t* const data) noexcept;
    [[nodiscard]] static bool can_adopt(const std::uint8_t* const data, const std::size_t align) noexcept;
    [[nodiscard]] static bool can_adopt(const CMemoryView& view) noexcept;
    [[nodiscard]] static bool can_adopt(const CMemoryConstView& view) noexcept;
    [[nodiscard]] bool adopt(const std::uint8_t* data) noexcept;
    [[nodiscard]] bool adopt(const std::uint8_t* data, const std::size_t align) noexcept;
    [[nodiscard]] bool adopt(const CMemoryView& view) noexcept;
    [[nodiscard]] bool adopt(const CMemoryConstView& view) noexcept;

    //  Views and sub-views (offset parameter validation is a caller responsibility)
    [[nodiscard]] TMemoryConstView<T> subview(const std::size_t offset = 0u) const noexcept;

    //  Common accessors (read-only memory access)
    [[nodiscard]] const T* data() const noexcept { return m_data; }

    //  Constants
    static constexpr std::size_t k_max_elements = t_max_elements<T>();
    static constexpr std::size_t k_element_size = sizeof(T);
    static constexpr std::size_t k_align = t_default_align<T>();

private:
    const T* m_data = nullptr;
};

//==============================================================================
//  Compile-time guarantees
//==============================================================================

static_assert(std::is_trivially_copyable_v<CMemoryView>, "CMemoryView must be trivially copyable");
static_assert(std::is_trivially_destructible_v<CMemoryView>, "CMemoryView must be trivially destructible");

static_assert(std::is_trivially_copyable_v<CMemoryConstView>, "CMemoryConstView must be trivially copyable");
static_assert(std::is_trivially_destructible_v<CMemoryConstView>, "CMemoryConstView must be trivially destructible");

static_assert(std::is_trivially_copyable_v<TMemoryView<std::uint8_t>>, "TMemoryView<T> must be trivially copyable");
static_assert(std::is_trivially_destructible_v<TMemoryView<std::uint8_t>>, "TMemoryView<T> must be trivially destructible");

static_assert(std::is_trivially_copyable_v<TMemoryConstView<std::uint8_t>>, "TMemoryConstView<T> must be trivially copyable");
static_assert(std::is_trivially_destructible_v<TMemoryConstView<std::uint8_t>>, "TMemoryConstView<T> must be trivially destructible");

//==============================================================================
//  CMemoryToken out of class function bodies
//==============================================================================

inline CMemoryToken::CMemoryToken(CMemoryToken&& other) noexcept
{
    m_data = other.m_data;
    m_align = other.m_align;
    other.m_data = nullptr;
    other.m_align = 0u;
}

inline CMemoryToken& CMemoryToken::operator=(CMemoryToken&& other) noexcept
{
    if (this != &other)
    {
        deallocate();
        m_data = other.m_data;
        m_align = other.m_align;
        other.m_data = nullptr;
        other.m_align = 0u;
    }
    return *this;
}

inline CMemoryView CMemoryToken::view() const noexcept
{
    return is_ready() ? CMemoryView{ m_data, m_align } : CMemoryView{};
}

inline CMemoryConstView CMemoryToken::const_view() const noexcept
{
    return is_ready() ? CMemoryConstView{ m_data, m_align } : CMemoryConstView{};
}

inline bool CMemoryToken::allocate(const std::size_t size, const std::size_t align, const bool zero) noexcept
{
    if (size == 0u)
    {
        deallocate();
        return true;
    }
    if (size <= k_max_elements)
    {
        const std::size_t norm_align = util::norm_align(align);
        std::uint8_t* const data = reinterpret_cast<std::uint8_t*>(byte_allocate(size, norm_align));
        if (data != nullptr)
        {
            deallocate();
            m_data = data;
            m_align = norm_align;
            if (zero)
            {
                std::memset(m_data, 0, size);
            }
            MV_HARD_ASSERT(bit_ops::is_pow2(m_align));
            MV_HARD_ASSERT((reinterpret_cast<std::uintptr_t>(m_data) & (m_align - 1u)) == 0u);
            return true;
        }
    }
    return false;
}

inline bool CMemoryToken::reallocate(const std::size_t old_size, const std::size_t new_size, const std::size_t align, const bool zero_extra) noexcept
{
    if (new_size == 0u)
    {
        deallocate();
        return true;
    }
    if (std::max(old_size, new_size) <= k_max_elements)
    {
        const std::size_t norm_align = util::norm_align(align);
        if ((old_size == new_size) && (norm_align == m_align))
        {
            return true;
        }
        std::uint8_t* const data = reinterpret_cast<std::uint8_t*>(byte_allocate(new_size, norm_align));
        if (data != nullptr)
        {
            if (old_size != 0u)
            {
                const std::size_t copy_size = std::min(new_size, old_size);
                if (m_data != nullptr)
                {
                    std::memcpy(data, m_data, copy_size);
                }
                else if (zero_extra)
                {
                    std::memset(data, 0, copy_size);
                }
            }
            if (zero_extra && (new_size > old_size))
            {
                std::memset((data + old_size), 0, (new_size - old_size));
            }
            deallocate();
            m_data = data;
            m_align = norm_align;
            MV_HARD_ASSERT(bit_ops::is_pow2(m_align));
            MV_HARD_ASSERT((reinterpret_cast<std::uintptr_t>(m_data) & (m_align - 1u)) == 0u);
            return true;
        }
    }
    return false;
}

inline void CMemoryToken::deallocate() noexcept
{
    if (m_data != nullptr)
    {   //  note: the null check is not strictly required
        byte_deallocate(m_data, m_align);
        m_data = nullptr;
    }
    m_align = 0u;
}

//==============================================================================
//  CMemoryView out of class function bodies
//==============================================================================

inline CMemoryView& CMemoryView::set(std::uint8_t* const data, const std::size_t align) noexcept
{
    if (data != nullptr)
    {
        m_data = data;
        m_align = util::common_align(static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(data)), util::norm_align(align));
        MV_HARD_ASSERT(bit_ops::is_pow2(m_align));
        MV_HARD_ASSERT((reinterpret_cast<std::uintptr_t>(m_data) & (m_align - 1u)) == 0u);
    }
    else
    {
        MV_HARD_ASSERT(align == 0u);
        reset();
    }
    return *this;
}

template<typename T>
void CMemoryView::adopt(const TMemoryView<T>& view) noexcept
{
    if (view.data() != nullptr)
    {
        m_data = reinterpret_cast<std::uint8_t*>(view.data());
        m_align = TMemoryView<T>::k_align;
    }
    else
    {
        reset();
    }
}

inline CMemoryView CMemoryView::subview(const std::size_t offset) const noexcept
{
    if (is_ready())
    {
        const std::size_t subview_align = util::offset_align(m_align, offset);
        MV_HARD_ASSERT(bit_ops::is_pow2(m_align));
        MV_HARD_ASSERT(bit_ops::is_pow2(subview_align));
        MV_HARD_ASSERT((reinterpret_cast<std::uintptr_t>(m_data + offset) & (subview_align - 1u)) == 0u);
        return CMemoryView{ (m_data + offset), subview_align };
    }
    return CMemoryView{};
}

inline CMemoryConstView CMemoryView::const_view() const noexcept
{
    return is_ready() ? CMemoryConstView{ m_data, m_align } : CMemoryConstView{};
}

//==============================================================================
//  CMemoryConstView out of class function bodies
//==============================================================================

inline CMemoryConstView& CMemoryConstView::set(const std::uint8_t* const data, const std::size_t align) noexcept
{
    if (data != nullptr)
    {
        m_data = data;
        m_align = util::common_align(static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(data)), util::norm_align(align));
        MV_HARD_ASSERT(bit_ops::is_pow2(m_align));
        MV_HARD_ASSERT((reinterpret_cast<std::uintptr_t>(m_data) & (m_align - 1u)) == 0u);
    }
    else
    {
        MV_HARD_ASSERT(align == 0u);
        reset();
    }
    return *this;
}

inline CMemoryConstView& CMemoryConstView::set(const CMemoryView& view) noexcept
{
    if (view.is_ready())
    {
        m_data = view.data();
        m_align = view.align();
    }
    else
    {
        reset();
    }
    return *this;
}

template<typename T>
void CMemoryConstView::adopt(const TMemoryView<T>& view) noexcept
{
    if (view.data() != nullptr)
    {
        m_data = reinterpret_cast<const std::uint8_t*>(view.data());
        m_align = TMemoryView<T>::k_align;
    }
    else
    {
        reset();
    }
}


template<typename T>
void CMemoryConstView::adopt(const TMemoryConstView<T>& view) noexcept
{
    if (view.data() != nullptr)
    {
        m_data = reinterpret_cast<const std::uint8_t*>(view.data());
        m_align = TMemoryConstView<T>::k_align;
    }
    else
    {
        reset();
    }
}

inline CMemoryConstView CMemoryConstView::subview(const std::size_t offset) const noexcept
{
    if (is_ready())
    {
        const std::size_t subview_align = util::offset_align(m_align, offset);
        MV_HARD_ASSERT(bit_ops::is_pow2(m_align));
        MV_HARD_ASSERT(bit_ops::is_pow2(subview_align));
        MV_HARD_ASSERT((reinterpret_cast<std::uintptr_t>(m_data + offset) & (subview_align - 1u)) == 0u);
        return CMemoryConstView{ (m_data + offset), subview_align };
    }
    return CMemoryConstView{};
}

//==============================================================================
//  TMemoryToken out of class function bodies
//==============================================================================

template<typename T>
inline TMemoryToken<T>::TMemoryToken(TMemoryToken<T>&& other) noexcept
{
    m_data = other.m_data;
    other.m_data = nullptr;
}

template<typename T>
inline TMemoryToken<T>& TMemoryToken<T>::operator=(TMemoryToken<T>&& other) noexcept
{
    if (this != &other)
    {
        deallocate();
        m_data = other.m_data;
        other.m_data = nullptr;
    }
    return *this;
}

template<typename T>
inline bool TMemoryToken<T>::allocate(const std::size_t size, const bool zero) noexcept
{
    if (size == 0u)
    {
        deallocate();
        return true;
    }
    if (size <= k_max_elements)
    {
        T* data = t_allocate<T>(size);
        if (data != nullptr)
        {
            deallocate();
            m_data = data;
            if (zero)
            {
                std::memset(m_data, 0, (size * k_element_size));
            }
            return true;
        }
    }
    return false;
}

template<typename T>
inline bool TMemoryToken<T>::reallocate(const std::size_t old_size, const std::size_t new_size, const bool zero_extra) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>, "TMemoryToken<T>::reallocate requires trivially copyable T");
    if (new_size == 0u)
    {
        deallocate();
        return true;
    }
    if (std::max(old_size, new_size) <= k_max_elements)
    {
        if (old_size == new_size)
        {
            return true;
        }
        T* data = t_allocate<T>(new_size);
        if (data != nullptr)
        {
            if (old_size != 0u)
            {
                const std::size_t copy_size = std::min(new_size, old_size) * k_element_size;
                if (m_data != nullptr)
                {
                    std::memcpy(data, m_data, copy_size);
                }
                else if (zero_extra)
                {
                    std::memset(data, 0, copy_size);
                }
            }
            if (zero_extra && (new_size > old_size))
            {
                std::memset((data + old_size), 0, ((new_size - old_size) * k_element_size));
            }
            deallocate();
            m_data = data;
            return true;
        }
    }
    return false;
}

template<typename T>
inline void TMemoryToken<T>::deallocate() noexcept
{
    if (m_data != nullptr)
    {
        t_deallocate<T>(m_data);
        m_data = nullptr;
    }
}

template<typename T>
inline bool TMemoryToken<T>::clone(const TMemoryToken<T> src, const std::size_t size) const noexcept
{
    if (size == 0u)
    {
        deallocate();
        return true;
    }
    if (src.m_data != nullptr)
    {
        T* data = t_allocate<T>(size);
        if (data != nullptr)
        {
            deallocate();
            m_data = data;
            std::memcpy(m_data, src.m_data, (size * k_element_size));
            return true;
        }
    }
    return false;
}

//==============================================================================
//  TMemoryView out of class function bodies
//==============================================================================
 
template<typename T>
inline bool TMemoryView<T>::can_adopt(const std::uint8_t* const data) noexcept
{
    return (reinterpret_cast<std::uintptr_t>(data) & (k_align - 1u)) == 0u;
}

template<typename T>
inline bool TMemoryView<T>::can_adopt(const std::uint8_t* const data, const std::size_t align) noexcept
{
    if (data == nullptr)
    {
        return align == 0u;
    }
    return can_adopt(data) && bit_ops::is_pow2(align) && (align >= k_align);
}

template<typename T>
inline bool TMemoryView<T>::can_adopt(const CMemoryView& view) noexcept
{
    return can_adopt(view.data(), view.align());
}

template<typename T>
inline bool TMemoryView<T>::adopt(const std::uint8_t* data) noexcept
{
    if (can_adopt(data))
    {
        m_data = reinterpret_cast<T*>(data);
        return true;
    }
    return false;
}

template<typename T>
inline bool TMemoryView<T>::adopt(const std::uint8_t* data, const std::size_t align) noexcept
{
    if (can_adopt(data, align))
    {
        m_data = reinterpret_cast<T*>(data);
        return true;
    }
    return false;
}

template<typename T>
inline bool TMemoryView<T>::adopt(const CMemoryView& view) noexcept
{
    if (can_adopt(view))
    {
        m_data = reinterpret_cast<T*>(view.data());
        return true;
    }
    return false;
}

template<typename T>
[[nodiscard]] TMemoryView<T> TMemoryView<T>::subview(const std::size_t offset) const noexcept
{
    return (m_data != nullptr) ? TMemoryView<T>{ m_data + offset } : TMemoryView<T>{};
}

//==============================================================================
//  TMemoryConstView out of class function bodies
//==============================================================================

template<typename T>
inline bool TMemoryConstView<T>::can_adopt(const std::uint8_t* const data) noexcept
{
    return (reinterpret_cast<std::uintptr_t>(data) & (k_align - 1u)) == 0u;
}

template<typename T>
inline bool TMemoryConstView<T>::can_adopt(const std::uint8_t* const data, const std::size_t align) noexcept
{
    if (data == nullptr)
    {
        return align == 0u;
    }
    return can_adopt(data) && bit_ops::is_pow2(align) && (align >= k_align);
}

template<typename T>
inline bool TMemoryConstView<T>::can_adopt(const CMemoryView& view) noexcept
{
    return can_adopt(view.data(), view.align());
}

template<typename T>
inline bool TMemoryConstView<T>::can_adopt(const CMemoryConstView& view) noexcept
{
    return can_adopt(view.data(), view.align());
}

template<typename T>
inline bool TMemoryConstView<T>::adopt(const std::uint8_t* data) noexcept
{
    if (can_adopt(data))
    {
        m_data = reinterpret_cast<const T*>(data);
        return true;
    }
    return false;
}

template<typename T>
inline bool TMemoryConstView<T>::adopt(const std::uint8_t* data, const std::size_t align) noexcept
{
    if (can_adopt(data, align))
    {
        m_data = reinterpret_cast<const T*>(data);
        return true;
    }
    return false;
}

template<typename T>
inline bool TMemoryConstView<T>::adopt(const CMemoryView& view) noexcept
{
    if (can_adopt(view))
    {
        m_data = reinterpret_cast<const T*>(view.data());
        return true;
    }
    return false;
}

template<typename T>
inline bool TMemoryConstView<T>::adopt(const CMemoryConstView& view) noexcept
{
    if (can_adopt(view))
    {
        m_data = reinterpret_cast<const T*>(view.data());
        return true;
    }
    return false;
}

template<typename T>
inline TMemoryConstView<T> TMemoryConstView<T>::subview(const std::size_t offset) const noexcept
{
    return (m_data != nullptr) ? TMemoryConstView<T>{ m_data + offset } : TMemoryConstView<T>{};
}

}   //  namespace memory

#endif  //  #ifndef MEMORY_PRIMITIVES_HPP_INCLUDED