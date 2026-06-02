
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   memory_primitives.hpp
//  Author: Ritchie Brannan
//  Date:   22 Feb 26
//
//  Raw memory ownership and view primitives.
//
//  Provides:
//  - CMemoryToken for owned byte storage;
//  - TMemoryToken<T> for owned typed storage;
//  - CMemoryView / CMemoryConstView for non-owning byte views;
//  - TMemoryView<T> / TMemoryConstView<T> for non-owning typed views;
//  - checked adoption and stealing between byte and typed forms.
//
//  Does not provide:
//  - bounds checking;
//  - non-trivial object construction/destruction;
//  - non-trivial relocation;
//  - container semantics;
//  - view-carried extent;
//  - deep allocation accounting.
//
//  Cross-header ownership, extent, observation, reallocation, alignment, and
//  accounting policy is documented in docs/memory/memory_subsystem.md.

#pragma once

#ifndef MEMORY_PRIMITIVES_HPP_INCLUDED
#define MEMORY_PRIMITIVES_HPP_INCLUDED

#include <algorithm>    //  std::min, std::max
#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::uint8_t
#include <cstring>      //  std::memcpy, std::memset
#include <type_traits>  //  std::is_trivially_copyable_v

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
    [[nodiscard]] bool is_valid() const noexcept { return is_ready() || ((m_data == nullptr) && (m_align == 0u) && (m_bytes == 0u)); }
    [[nodiscard]] bool is_empty() const noexcept { return (m_data == nullptr) || (m_align == 0u) || (m_bytes == 0u); }
    [[nodiscard]] bool is_ready() const noexcept { return (m_data != nullptr) && (m_align != 0u) && (m_bytes != 0u); }
    [[nodiscard]] bool owns_memory() const noexcept { return m_data != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return is_ready(); }

    //  Adoption
    template<typename T> void steal(TMemoryToken<T>& token) noexcept;

    //  Views
    [[nodiscard]] CMemoryView view() const noexcept;
    [[nodiscard]] CMemoryConstView const_view() const noexcept;

    //  Common accessors (see constness model above)
    [[nodiscard]] std::uint8_t* data() const noexcept { return ((m_align != 0u) && (m_bytes != 0u)) ? m_data : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return ((m_data != nullptr) && (m_bytes != 0u)) ? m_align : std::size_t{ 0 }; }
    [[nodiscard]] std::size_t bytes() const noexcept { return ((m_data != nullptr) && (m_align != 0u)) ? m_bytes : std::size_t{ 0 }; }

    //  Capacity management (state unchanged on failure)
    [[nodiscard]] bool allocate(const std::size_t bytes, const std::size_t align, const bool zero = true) noexcept;
    [[nodiscard]] bool reallocate(const std::size_t copy_bytes, const std::size_t bytes, const std::size_t align, const bool zero_extra = true) noexcept;
    [[nodiscard]] bool clone(const CMemoryToken& src) noexcept;
    void deallocate() noexcept;

private:
    std::uint8_t* m_data = nullptr;
    std::size_t m_align = 0u;
    std::size_t m_bytes = 0u;

private:
    template<typename T>
    friend class TMemoryToken;
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
    [[nodiscard]] bool is_valid() const noexcept { return (m_data == nullptr) == (m_align == 0u); }
    [[nodiscard]] bool is_empty() const noexcept { return (m_data == nullptr) || (m_align == 0u); }
    [[nodiscard]] bool is_ready() const noexcept { return (m_data != nullptr) && (m_align != 0u); }

    //  Views and sub-views (offset parameter validation is a caller responsibility)
    [[nodiscard]] CMemoryView subview(const std::size_t offset = 0u) const noexcept;
    [[nodiscard]] CMemoryConstView const_view() const noexcept;

    //  Common accessors (see constness model above)
    [[nodiscard]] std::uint8_t* data() const noexcept { return (m_align != 0u) ? m_data : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return (m_data != nullptr) ? m_align : std::size_t{ 0 }; }

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
    [[nodiscard]] bool is_valid() const noexcept { return (m_data == nullptr) == (m_align == 0u); }
    [[nodiscard]] bool is_empty() const noexcept { return (m_data == nullptr) || (m_align == 0u); }
    [[nodiscard]] bool is_ready() const noexcept { return (m_data != nullptr) && (m_align != 0u); }

    //  Views and sub-views (offset parameter validation is a caller responsibility)
    [[nodiscard]] CMemoryConstView subview(const std::size_t offset = 0u) const noexcept;

    //  Common accessors (read-only memory access)
    [[nodiscard]] const std::uint8_t* data() const noexcept { return (m_align != 0u) ? m_data : nullptr; }
    [[nodiscard]] std::size_t align() const noexcept { return (m_data != nullptr) ? m_align : std::size_t{ 0 }; }

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

    //  Status
    [[nodiscard]] bool is_valid() const noexcept { return (m_data == nullptr) == (m_count == 0u); }
    [[nodiscard]] bool is_empty() const noexcept { return (m_data == nullptr) || (m_count == 0u); }
    [[nodiscard]] bool is_ready() const noexcept { return (m_data != nullptr) && (m_count != 0u); }
    [[nodiscard]] bool owns_memory() const noexcept { return m_data != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return is_ready(); }

    //  Adoption
    [[nodiscard]] static bool can_steal(const CMemoryToken& token) noexcept;
    [[nodiscard]] bool steal(CMemoryToken& token) noexcept;

    //  Views
    [[nodiscard]] TMemoryView<T> view() const noexcept { return TMemoryView<T>{ m_data }; }
    [[nodiscard]] TMemoryConstView<T> const_view() const noexcept { return TMemoryConstView<T>{ m_data }; }

    //  Common accessors (see constness model above)
    [[nodiscard]] T* data() const noexcept { return (m_count != 0u) ? m_data : nullptr; }
    [[nodiscard]] std::size_t count() const noexcept { return (m_data != nullptr) ? m_count : std::size_t{ 0u }; }
    [[nodiscard]] std::size_t bytes() const noexcept { return (m_data != nullptr) ? (m_count * k_element_size) : std::size_t{ 0u }; }

    //  Capacity management (state unchanged on failure)
    [[nodiscard]] bool allocate(const std::size_t count, const bool zero = true) noexcept;
    [[nodiscard]] bool reallocate(const std::size_t copy_count, const std::size_t count, const bool zero_extra = true) noexcept;
    [[nodiscard]] bool clone(const TMemoryToken<T>& src) noexcept;
    void deallocate() noexcept;

    //  Constants
    static constexpr std::size_t k_max_elements = t_max_elements<T>();
    static constexpr std::size_t k_element_size = sizeof(T);
    static constexpr std::size_t k_align = t_default_align<T>();

private:
    T* m_data = nullptr;
    std::size_t m_count = 0u;

private:
    friend class CMemoryToken;
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
    [[nodiscard]] static bool can_adopt(std::uint8_t* const data) noexcept;
    [[nodiscard]] static bool can_adopt(std::uint8_t* const data, const std::size_t align) noexcept;
    [[nodiscard]] static bool can_adopt(const CMemoryView& view) noexcept;
    [[nodiscard]] bool adopt(std::uint8_t* const data) noexcept;
    [[nodiscard]] bool adopt(std::uint8_t* const data, const std::size_t align) noexcept;
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
    [[nodiscard]] bool adopt(const std::uint8_t* const data) noexcept;
    [[nodiscard]] bool adopt(const std::uint8_t* const data, const std::size_t align) noexcept;
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
    m_bytes = other.m_bytes;
    other.m_data = nullptr;
    other.m_align = 0u;
    other.m_bytes = 0u;
}

inline CMemoryToken& CMemoryToken::operator=(CMemoryToken&& other) noexcept
{
    if (this != &other)
    {
        deallocate();
        m_data = other.m_data;
        m_align = other.m_align;
        m_bytes = other.m_bytes;
        other.m_data = nullptr;
        other.m_align = 0u;
        other.m_bytes = 0u;
    }
    return *this;
}

template<typename T>
inline void CMemoryToken::steal(TMemoryToken<T>& token) noexcept
{
    deallocate();
    if (token.owns_memory())
    {
        m_data = reinterpret_cast<std::uint8_t*>(token.m_data);
        m_align = TMemoryToken<T>::k_align;
        m_bytes = token.bytes();
        MV_HARD_ASSERT(m_bytes != 0u);
    }
    token.m_data = nullptr;
    token.m_count = 0u;
}

inline CMemoryView CMemoryToken::view() const noexcept
{
    return is_ready() ? CMemoryView{ m_data, m_align } : CMemoryView{};
}

inline CMemoryConstView CMemoryToken::const_view() const noexcept
{
    return is_ready() ? CMemoryConstView{ m_data, m_align } : CMemoryConstView{};
}

inline bool CMemoryToken::allocate(const std::size_t bytes, const std::size_t align, const bool zero) noexcept
{
    if (bytes == 0u)
    {
        deallocate();
        return true;
    }
    if (bytes <= k_max_elements)
    {
        const std::size_t norm_align = util::norm_align(align);
        std::uint8_t* const data = reinterpret_cast<std::uint8_t*>(byte_allocate(bytes, norm_align));
        if (data != nullptr)
        {
            deallocate();
            m_data = data;
            m_align = norm_align;
            m_bytes = bytes;
            if (zero)
            {
                std::memset(m_data, 0, bytes);
            }
            MV_HARD_ASSERT(bit_ops::is_pow2(m_align));
            MV_HARD_ASSERT((reinterpret_cast<std::uintptr_t>(m_data) & (m_align - 1u)) == 0u);
            return true;
        }
    }
    return false;
}

inline bool CMemoryToken::reallocate(const std::size_t copy_bytes, const std::size_t bytes, const std::size_t align, const bool zero_extra) noexcept
{
    const std::size_t min_bytes = std::min(m_bytes, bytes);
    if ((bytes <= k_max_elements) && (copy_bytes <= min_bytes))
    {
        if (bytes == 0u)
        {
            deallocate();
            return true;
        }
        const std::size_t norm_align = util::norm_align(align);
        if ((m_bytes == bytes) && (m_data != nullptr) && (norm_align == m_align))
        {
            if (zero_extra && (bytes > copy_bytes))
            {
                std::memset((m_data + copy_bytes), 0, (bytes - copy_bytes));
            }
            return true;
        }
        std::uint8_t* const data = reinterpret_cast<std::uint8_t*>(byte_allocate(bytes, norm_align));
        if (data != nullptr)
        {
            if (copy_bytes != 0u)
            {
                if (m_data != nullptr)
                {
                    std::memcpy(data, m_data, copy_bytes);
                }
                else if (zero_extra)
                {
                    std::memset(data, 0, copy_bytes);
                }
            }
            if (zero_extra && (bytes > copy_bytes))
            {
                std::memset((data + copy_bytes), 0, (bytes - copy_bytes));
            }
            deallocate();
            m_data = data;
            m_align = norm_align;
            m_bytes = bytes;
            MV_HARD_ASSERT(bit_ops::is_pow2(m_align));
            MV_HARD_ASSERT((reinterpret_cast<std::uintptr_t>(m_data) & (m_align - 1u)) == 0u);
            return true;
        }
    }
    return false;
}

inline bool CMemoryToken::clone(const CMemoryToken& src) noexcept
{
    if (!src.is_ready())
    {
        deallocate();
        return true;
    }
    std::uint8_t* const data = reinterpret_cast<std::uint8_t*>(byte_allocate(src.m_bytes, src.m_align));
    if (data != nullptr)
    {
        deallocate();
        m_data = data;
        m_align = src.m_align;
        m_bytes = src.m_bytes;
        std::memcpy(m_data, src.m_data, m_bytes);
        return true;
    }
    return false;
}

inline void CMemoryToken::deallocate() noexcept
{
    if (m_data != nullptr)
    {   //  note: the null check is not strictly required
        MV_HARD_ASSERT(m_align != 0u);
        MV_HARD_ASSERT(m_bytes != 0u);
        byte_deallocate(m_data, m_align);
        m_data = nullptr;
    }
    m_align = 0u;
    m_bytes = 0u;
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
    m_count = other.m_count;
    other.m_data = nullptr;
    other.m_count = 0u;
}

template<typename T>
inline TMemoryToken<T>& TMemoryToken<T>::operator=(TMemoryToken<T>&& other) noexcept
{
    if (this != &other)
    {
        deallocate();
        m_data = other.m_data;
        m_count = other.m_count;
        other.m_data = nullptr;
        other.m_count = 0u;
    }
    return *this;
}

template<typename T>
inline bool TMemoryToken<T>::can_steal(const CMemoryToken& token) noexcept
{
    return token.is_valid() && ((token.m_data == nullptr) || ((token.m_align == k_align) && ((token.m_bytes % k_element_size) == 0u)));
}

template<typename T>
inline bool TMemoryToken<T>::steal(CMemoryToken& token) noexcept
{
    if (can_steal(token))
    {
        deallocate();
        if (token.owns_memory())
        {
            m_data = reinterpret_cast<T*>(token.m_data);
            m_count = token.m_bytes / k_element_size;
            MV_HARD_ASSERT(m_count != 0u);
        }
        token.m_data = nullptr;
        token.m_align = 0u;
        token.m_bytes = 0u;
        return true;
    }
    return false;
}

template<typename T>
inline bool TMemoryToken<T>::allocate(const std::size_t count, const bool zero) noexcept
{
    if (count == 0u)
    {
        deallocate();
        return true;
    }
    if (count <= k_max_elements)
    {
        T* data = t_allocate<T>(count);
        if (data != nullptr)
        {
            deallocate();
            m_data = data;
            m_count = count;
            if (zero)
            {
                std::memset(m_data, 0, (count * k_element_size));
            }
            return true;
        }
    }
    return false;
}

template<typename T>
inline bool TMemoryToken<T>::reallocate(const std::size_t copy_count, const std::size_t count, const bool zero_extra) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>, "TMemoryToken<T>::reallocate requires trivially copyable T");
    const std::size_t min_count = std::min(m_count, count);
    if ((count <= k_max_elements) && (copy_count <= min_count))
    {
        if (count == 0u)
        {
            deallocate();
            return true;
        }
        if ((m_count == count) && (m_data != nullptr))
        {
            if (zero_extra && (count > copy_count))
            {
                std::memset((m_data + copy_count), 0, ((count - copy_count) * k_element_size));
            }
            return true;
        }
        T* data = t_allocate<T>(count);
        if (data != nullptr)
        {
            if (copy_count != 0u)
            {
                const std::size_t copy_bytes = copy_count * k_element_size;
                if (m_data != nullptr)
                {
                    std::memcpy(data, m_data, copy_bytes);
                }
                else if (zero_extra)
                {
                    std::memset(data, 0, copy_bytes);
                }
            }
            if (zero_extra && (count > copy_count))
            {
                std::memset((data + copy_count), 0, ((count - copy_count) * k_element_size));
            }
            deallocate();
            m_data = data;
            m_count = count;
            return true;
        }
    }
    return false;
}

template<typename T>
inline bool TMemoryToken<T>::clone(const TMemoryToken<T>& src) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>, "TMemoryToken<T>::clone requires trivially copyable T");
    if (!src.is_ready())
    {
        deallocate();
        return true;
    }
    T* data = t_allocate<T>(src.m_count);
    if (data != nullptr)
    {
        deallocate();
        m_data = data;
        m_count = src.m_count;
        std::memcpy(m_data, src.m_data, (m_count * k_element_size));
        return true;
    }
    return false;
}

template<typename T>
inline void TMemoryToken<T>::deallocate() noexcept
{
    if (m_data != nullptr)
    {
        MV_HARD_ASSERT(m_count != 0u);
        t_deallocate<T>(m_data);
        m_data = nullptr;
    }
    m_count = 0u;
}

//==============================================================================
//  TMemoryView out of class function bodies
//==============================================================================
 
template<typename T>
inline bool TMemoryView<T>::can_adopt(std::uint8_t* const data) noexcept
{
    return (reinterpret_cast<std::uintptr_t>(data) & (k_align - 1u)) == 0u;
}

template<typename T>
inline bool TMemoryView<T>::can_adopt(std::uint8_t* const data, const std::size_t align) noexcept
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
inline bool TMemoryView<T>::adopt(std::uint8_t* const data) noexcept
{
    if (can_adopt(data))
    {
        m_data = reinterpret_cast<T*>(data);
        return true;
    }
    return false;
}

template<typename T>
inline bool TMemoryView<T>::adopt(std::uint8_t* const data, const std::size_t align) noexcept
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
inline bool TMemoryConstView<T>::adopt(const std::uint8_t* const data) noexcept
{
    if (can_adopt(data))
    {
        m_data = reinterpret_cast<const T*>(data);
        return true;
    }
    return false;
}

template<typename T>
inline bool TMemoryConstView<T>::adopt(const std::uint8_t* const data, const std::size_t align) noexcept
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