//  File:   StringBuffers.hpp
//  Author: Ritchie Brannan
//  Date:   22 Feb 26
//
//  POD string buffers and buffer view noexcept container utilities
//
//  Notes:
//  - Requires C++ 17 or greater
//  - No exceptions.

#pragma once

#ifndef STRING_BUFFERS_HPP_INCLUDED
#define STRING_BUFFERS_HPP_INCLUDED

#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::uint8_t
#include <cstring>      //  std::strlen, std::memcpy
#include <limits>       //  std::numeric_limits
#include <utility>      //  std::move

#include "internal/PodMemory.hpp"
#include "ByteBuffers.hpp"
#include "TPodArrays.hpp"
#include "algo/validate_permutations.hpp"
#include "debug/debug.hpp"

//==============================================================================
//  Forward declarations
//==============================================================================

class CStringView;
class CSimpleString;
class CStringBuffer;
class CStableStringBuffer;

//==============================================================================
//  Shared string helpers base structure
//==============================================================================

struct CStringBase
{
static inline char* cast_to_cstring(std::uint8_t* const string) noexcept { return reinterpret_cast<char* const>(string); }
static inline const char* cast_to_cstring(const std::uint8_t* const string) noexcept { return reinterpret_cast<const char* const>(string); }
static inline std::uint8_t* cast_to_string(char* const cstring) noexcept { return reinterpret_cast<std::uint8_t* const>(cstring); }
static inline const std::uint8_t* cast_to_string(const char* const cstring) noexcept { return reinterpret_cast<const std::uint8_t* const>(cstring); }
static inline std::size_t strz_length(const char* const cstring) noexcept { return (cstring != nullptr) ? std::strlen(cstring) : std::size_t{ 0 }; }
static inline std::size_t strz_length(const std::uint8_t* const string) noexcept { return strz_length(cast_to_cstring(string)); }
};

//==============================================================================
//  CStringView
//  Non-owning view of a utf8 string.
//  Canonical empty is { m_string = nullptr, m_length = 0 } ONLY.
//==============================================================================

class CStringView : private CStringBase
{
public:
    CStringView() noexcept = default;
    CStringView(CStringView& other) noexcept = default;
    explicit CStringView(const char* const cstring) noexcept : m_string(nullptr), m_length(0u) { set(cstring); }
    explicit CStringView(const std::uint8_t* const string) noexcept : m_string(nullptr), m_length(0u) { set(string); }
    explicit CStringView(const char* const cstring, const std::size_t length) noexcept : m_string(nullptr), m_length(0u) { set(cstring, length); }
    explicit CStringView(const std::uint8_t* const string, const std::size_t length) noexcept : m_string(nullptr), m_length(0u) { set(string, length); }
    CStringView(const CStringView& other) noexcept : m_string(other.m_string), m_length(other.m_length) {}
    ~CStringView() noexcept = default;

    CStringView& operator=(CStringView& other) noexcept = default;
    CStringView& operator=(const CStringView& other) noexcept { m_string = other.m_string; m_length = other.m_length; return *this; }

    //  Accessors
    const char* cstring() const noexcept { return cast_to_cstring(m_string); }
    const std::uint8_t* string() const noexcept { return m_string; }
    std::size_t length() const noexcept { return (m_string != nullptr) ? m_length : 0u; }
    bool empty() const noexcept { return (m_string == nullptr); }

    //  Relationship identity
    [[nodiscard]] std::int32_t relationship(const CStringView& other) const noexcept;
    [[nodiscard]] std::int32_t relationship(const CSimpleString& other) const noexcept;

    //  Comparators
    [[nodiscard]] bool operator==(const CStringView& other) const noexcept { return relationship(other) == 0; }
    [[nodiscard]] bool operator>=(const CStringView& other) const noexcept { return relationship(other) >= 0; }
    [[nodiscard]] bool operator<=(const CStringView& other) const noexcept { return relationship(other) <= 0; }
    [[nodiscard]] bool operator>(const CStringView& other) const noexcept { return relationship(other) > 0; }
    [[nodiscard]] bool operator<(const CStringView& other) const noexcept { return relationship(other) < 0; }
    [[nodiscard]] bool operator==(const CSimpleString& other) const noexcept { return relationship(other) == 0; }
    [[nodiscard]] bool operator>=(const CSimpleString& other) const noexcept { return relationship(other) >= 0; }
    [[nodiscard]] bool operator<=(const CSimpleString& other) const noexcept { return relationship(other) <= 0; }
    [[nodiscard]] bool operator>(const CSimpleString& other) const noexcept { return relationship(other) > 0; }
    [[nodiscard]] bool operator<(const CSimpleString& other) const noexcept { return relationship(other) < 0; }

    //  Initialisers
    bool set(const char* const cstring) noexcept { return set(cast_to_string(cstring)); }
    bool set(const std::uint8_t* const string) noexcept { clear(); if (string != nullptr) { m_string = string; m_length = strz_length(m_string); } return m_string != nullptr; }
    bool set(const char* const cstring, const std::size_t length) noexcept { return set(cast_to_string(cstring), length); }
    bool set(const std::uint8_t* const string, const std::size_t length) noexcept { clear(); if (string != nullptr) { m_string = string; m_length = length; } return m_string != nullptr; }

    //  Make the view empty
    void clear() noexcept { m_string = nullptr; m_length = 0; }

private:
    std::int32_t private_compare(const CStringView& other) const noexcept;

    const std::uint8_t* m_string = nullptr;
    std::size_t m_length = 0u;
};

//==============================================================================
//  CSimpleString
//  Owning unique string: string + length. No resizing (immutable size/capacity).
//  Canonical empty is { m_string = nullptr, m_length = 0 } ONLY.
//==============================================================================

class CSimpleString : private CStringBase
{
public:
    CSimpleString() noexcept = default;
    CSimpleString(const CSimpleString&) = delete;
    CSimpleString& operator=(const CSimpleString&) = delete;

    explicit CSimpleString(const char* const cstring) noexcept : m_string(nullptr), m_length(0u) { set(cstring); }
    explicit CSimpleString(const std::uint8_t* const string) noexcept : m_string(nullptr), m_length(0u) { set(string); }
    explicit CSimpleString(const char* const cstring, const std::size_t length) noexcept : m_string(nullptr), m_length(0u) { set(cstring, length); }
    explicit CSimpleString(const std::uint8_t* const string, const std::size_t length) noexcept : m_string(nullptr), m_length(0u) { set(string, length); }
    CSimpleString(CSimpleString&& other) noexcept : m_string(other.m_string), m_length(other.m_length) { other.m_string = nullptr; other.m_length = 0u; }

    ~CSimpleString() noexcept { deallocate(); }

    CSimpleString& operator=(CSimpleString&& other) noexcept;

    //  Accessors
    CStringView view() const noexcept { return CStringView{ m_string, m_length }; }
    [[nodiscard]] const char* cstring() const noexcept { return cast_to_cstring(string()); }
    [[nodiscard]] const std::uint8_t* string() const noexcept { return m_string; }
    [[nodiscard]] std::size_t length() const noexcept { return (m_string != nullptr) ? m_length : 0u; }
    [[nodiscard]] bool is_empty() const noexcept { return (m_string == nullptr); }

    //  Relationship identity
    [[nodiscard]] std::int32_t relationship(const CStringView& other) const noexcept { return view().relationship(other); }
    [[nodiscard]] std::int32_t relationship(const CSimpleString& other) const noexcept { return view().relationship(other); }

    //  Comparators
    [[nodiscard]] bool operator==(const CStringView& other) const noexcept { return relationship(other) == 0; }
    [[nodiscard]] bool operator>=(const CStringView& other) const noexcept { return relationship(other) >= 0; }
    [[nodiscard]] bool operator<=(const CStringView& other) const noexcept { return relationship(other) <= 0; }
    [[nodiscard]] bool operator>(const CStringView& other) const noexcept { return relationship(other) > 0; }
    [[nodiscard]] bool operator<(const CStringView& other) const noexcept { return relationship(other) < 0; }
    [[nodiscard]] bool operator==(const CSimpleString& other) const noexcept { return relationship(other) == 0; }
    [[nodiscard]] bool operator>=(const CSimpleString& other) const noexcept { return relationship(other) >= 0; }
    [[nodiscard]] bool operator<=(const CSimpleString& other) const noexcept { return relationship(other) <= 0; }
    [[nodiscard]] bool operator>(const CSimpleString& other) const noexcept { return relationship(other) > 0; }
    [[nodiscard]] bool operator<(const CSimpleString& other) const noexcept { return relationship(other) < 0; }

    //  Initialisers
    bool set(const char* const cstring) noexcept { return set(cast_to_string(cstring)); }
    bool set(const char* const cstring, const std::size_t length) noexcept { return set(cast_to_string(cstring), length); }
    bool set(const std::uint8_t* const string) noexcept;
    bool set(const std::uint8_t* const string, const std::size_t length) noexcept;

    //  Return any allocated memory to the heap
    void deallocate() noexcept;

private:
    bool private_allocate(const std::uint8_t* const string, const std::size_t length) noexcept;

    std::uint8_t* m_string = nullptr;
    std::size_t m_length = 0u;
};

//==============================================================================
//  CStringBuffer
//  Owning unique appendable string buffer.
//
//  String length is not stored, making this container unsuitable for
//  strings with embedded (mid-string) terminators unless the length is
//  stored separately and associated with the offset into this buffer.
//==============================================================================

class CStringBuffer : private CStringBase
{
public:
    CStringBuffer(const CStringBuffer&) = delete;
    CStringBuffer& operator=(const CStringBuffer&) = delete;

    CStringBuffer() noexcept : m_buffer(), m_size(0u) {}
    explicit CStringBuffer(const std::size_t size) noexcept : m_buffer(), m_size(0u) { reserve(size); }
    CStringBuffer(CStringBuffer&& other) noexcept;

    ~CStringBuffer() noexcept { deallocate(); }

    CStringBuffer& operator=(CStringBuffer&& other) noexcept;

    [[nodiscard]] std::size_t operator+=(const char* const cstring) noexcept { return append(cstring); }
    [[nodiscard]] std::size_t operator+=(const std::uint8_t* const string) noexcept { return append(string); }

    //  Common accessors
    const std::uint8_t* data() const noexcept { return m_buffer.data(); }
    std::size_t size() const noexcept { return m_size; }
    std::size_t capacity() const noexcept { return m_buffer.size(); }
    std::size_t available() const noexcept { return m_buffer.size() - m_size; }
    bool initialised() const noexcept { return !m_buffer.empty() && (m_size >= 1u); }
    bool empty() const noexcept { return m_buffer.empty() || (m_size <= 1u); }

    //  String accessors
    CStringView view(const std::size_t offset) const noexcept { return CStringView(string(offset)); }
    [[nodiscard]] const char* cstring(const std::size_t offset) const noexcept { return cast_to_cstring(string(offset)); }
    [[nodiscard]] const std::uint8_t* string(const std::size_t offset) const noexcept;
    [[nodiscard]] bool is_valid_offset(const std::size_t offset) const noexcept;

    //  String appending
    [[nodiscard]] std::size_t append(const char* const cstring) noexcept { return append(cast_to_string(cstring)); }
    [[nodiscard]] std::size_t append(const char* const cstring, const std::size_t length) noexcept { return append(cast_to_string(cstring), length); }
    [[nodiscard]] std::size_t append(const std::uint8_t* const string) noexcept;
    [[nodiscard]] std::size_t append(const std::uint8_t* const string, const std::size_t length) noexcept;
    [[nodiscard]] std::size_t append(const CStringView& view) noexcept { return append(view.string(), view.length()); };
    [[nodiscard]] std::size_t append(const CSimpleString& string) noexcept { return append(string.view()); };

    //  Capacity management (may grow and relocate, state unchanged on failure)
    //
    //  Note:
    //    If clear_empty is false, the user is responsible for initialising any
    //    newly observable uninitialised data and accepting the consequences.
    bool resize(const std::size_t size, const bool clear_empty = true) noexcept;
    bool resize_exact(const std::size_t size, const bool clear_empty = true) noexcept;
    bool reserve(const std::size_t minimum_capacity) noexcept;
    bool reserve_exact(const std::size_t exact_capacity) noexcept;
    bool ensure_free(const std::size_t length) noexcept;
    bool shrink_to_fit() noexcept;
    void deallocate() noexcept; //  free and become empty (no-op if empty)

    //  Validate invariants
    [[nodiscard]] bool check_integrity() const noexcept;

    //  Invalid offset return value
    static constexpr std::size_t k_invalid_offset = 0u;

private:
    std::size_t private_append(const std::uint8_t* const string, const std::size_t length) noexcept;
    bool private_reallocate(const std::size_t capacity) noexcept;
    bool private_resize(const std::size_t size, const std::size_t capacity, const bool clear_empty = true) noexcept;

    static inline [[nodiscard]] bool failed_integrity_check() noexcept;

    CByteBuffer m_buffer;
};

//==============================================================================
//  CStableStringBuffer
//  Owning unique stable ID appendable string buffer.
//==============================================================================

class CStableStringBuffer : private CStringBase
{
public:
    CStableStringBuffer(const CStableStringBuffer&) = delete;
    CStableStringBuffer& operator=(const CStableStringBuffer&) = delete;

    CStableStringBuffer() noexcept : m_string_buffer(), m_string_refs(), m_ref_index_to_id(), m_id_to_ref_index(), m_sorted_ref_indices() {}
    explicit CStableStringBuffer(const std::size_t string_count, const std::size_t string_storage_size) noexcept : m_string_buffer(), m_string_refs(), m_ref_index_to_id(), m_id_to_ref_index(), m_sorted_ref_indices() { initialise(string_count, string_storage_size); }
    CStableStringBuffer(CStableStringBuffer&& other) noexcept;

    ~CStableStringBuffer() noexcept { deallocate(); }

    CStableStringBuffer& operator=(CStableStringBuffer&& other) noexcept;

    [[nodiscard]] std::size_t operator+=(const char* const cstring) noexcept { return append(cstring); }
    [[nodiscard]] std::size_t operator+=(const std::uint8_t* const string) noexcept { return append(string); }

    //  Accessors
    CStringView view(const std::size_t id) const noexcept;
    [[nodiscard]] bool is_valid_id(const std::size_t id) const noexcept;

    //  Index conversions
    //
    //  Notes:
    //    id, ref_index and rank reserve the value 0 for internal use.
    //    A return value of 0 therefore indicates failure.
    //
    //    After sort(), and until new strings are appended:
    //        ref_index == rank
    //
    //    Lookup complexity:
    //      id_to_ref_index      : O(1)
    //      ref_index_to_id      : O(1)
    //      rank_to_ref_index    : O(1)
    //      ref_index_to_rank    : O(N)  (linear search)
    [[nodiscard]] std::size_t id_to_ref_index(const std::size_t id) const noexcept;
    [[nodiscard]] std::size_t ref_index_to_id(const std::size_t ref_index) const noexcept;
    [[nodiscard]] std::size_t rank_to_ref_index(const std::size_t rank) const noexcept;
    [[nodiscard]] std::size_t ref_index_to_rank(const std::size_t ref_index) const noexcept;

    //  String id queries
    [[nodiscard]] std::size_t find_id(const char* const cstring) noexcept { return find_id(cast_to_string(cstring)); }
    [[nodiscard]] std::size_t find_id(const char* const cstring, const std::size_t length) noexcept { return find_id(cast_to_string(cstring), length); }
    [[nodiscard]] std::size_t find_id(const std::uint8_t* const string) noexcept;
    [[nodiscard]] std::size_t find_id(const std::uint8_t* const string, const std::size_t length) noexcept;

    //  String appending
    [[nodiscard]] std::size_t append(const char* const cstring) noexcept { return append(cast_to_string(cstring)); }
    [[nodiscard]] std::size_t append(const char* const cstring, const std::size_t length) noexcept { return append(cast_to_string(cstring), length); }
    [[nodiscard]] std::size_t append(const std::uint8_t* const string) noexcept;
    [[nodiscard]] std::size_t append(const std::uint8_t* const string, const std::size_t length) noexcept;

    //  Physically sort the strings
    bool sort() noexcept;

    //  Capacity management
    bool initialise(const std::size_t string_count, const std::size_t string_storage_size) noexcept;
    bool ensure_free(const std::size_t length) noexcept;   //  success guarantees the subsequent append will not fail
    bool shrink_to_fit() noexcept;
    void deallocate() noexcept;

    //  Validate invariants
    [[nodiscard]] bool check_integrity(const bool check_lexical_order = true) const noexcept;

    //  Invalid return values
    static constexpr std::size_t k_invalid_id = 0u;
    static constexpr std::size_t k_invalid_ref_index = 0u;
    static constexpr std::size_t k_invalid_offset = 0u;
    static constexpr std::size_t k_invalid_rank = 0u;

private:
    std::size_t private_find_ref_index(const std::uint8_t* const string, const std::size_t length, std::size_t& insert_at) noexcept;
    std::size_t private_find_id(const std::uint8_t* const string, const std::size_t length) noexcept;
    std::size_t private_append(const std::uint8_t* const string, const std::size_t length) noexcept;

    static inline [[nodiscard]] bool failed_integrity_check() noexcept;

    struct StringRef { std::size_t offset = 0u; std::size_t length = 0u; };

    CStringBuffer m_string_buffer;
    TPodVector<StringRef> m_string_refs;
    TPodVector<std::size_t> m_ref_index_to_id;
    TPodVector<std::size_t> m_id_to_ref_index;
    TPodVector<std::size_t> m_sorted_ref_indices;
};

//==============================================================================
//  CStringView
//==============================================================================

[[nodiscard]] inline std::int32_t CStringView::relationship(const CStringView& other) const noexcept
{
    return private_compare(other);
}

[[nodiscard]] inline std::int32_t CStringView::relationship(const CSimpleString& other) const noexcept
{
    return relationship(other.view());
}

[[nodiscard]] inline std::int32_t CStringView::private_compare(const CStringView& other) const noexcept
{
    if ((m_string != nullptr) && (other.m_string != nullptr))
    {
        std::size_t count = (m_length <= other.m_length) ? m_length : other.m_length;
        for (std::size_t index = 0; index < count; ++index)
        {
            if (m_string[index] != other.m_string[index])
            {
                return (m_string[index] > other.m_string[index]) ? 1 : -1;
            }
        }
    }
    return (m_length == other.m_length) ? 0 : ((m_length > other.m_length) ? 1 : -1);
}

//==============================================================================
//  CSimpleString
//==============================================================================

inline CSimpleString& CSimpleString::operator=(CSimpleString&& other) noexcept
{
    if (this != &other)
    {
        deallocate();
        m_string = other.m_string;
        m_length = other.m_length;
        other.m_string = nullptr;
        other.m_length = 0u;
    }
    return *this;
}

inline bool CSimpleString::set(const std::uint8_t* const string) noexcept
{
    deallocate();
    return (string != nullptr) ? private_allocate(string, strz_length(string)) : false;
}

inline bool CSimpleString::set(const std::uint8_t* const string, const std::size_t length) noexcept
{
    deallocate();
    return (string != nullptr) ? private_allocate(string, length) : false;
}

inline void CSimpleString::deallocate() noexcept
{
    if (m_string != nullptr)
    {
        pod_memory::t_deallocate<std::uint8_t>(m_string);
        m_string = nullptr;
    }
    m_length = 0u;
}

inline bool CSimpleString::private_allocate(const std::uint8_t* const string, const std::size_t length) noexcept
{
    m_string = pod_memory::t_allocate<std::uint8_t>(length + 1u);
    if (m_string != nullptr)
    {
        std::memcpy(m_string, string, length);
        m_string[length] = 0u;
        m_length = length;
        return true;
    }
    return false;
}

//==============================================================================
//  CStringBuffer
//==============================================================================

inline CStringBuffer::CStringBuffer(CStringBuffer&& other) noexcept : m_buffer(), m_size(0u)
{
    m_buffer = std::move(other.m_buffer);
    m_size = other.m_size;
    other.m_size = 0u;
}

inline CStringBuffer& CStringBuffer::operator=(CStringBuffer&& other) noexcept
{
    if (this != &other)
    {
        m_buffer = std::move(other.m_buffer);
        m_size = other.m_size;
        other.m_size = 0u;
    }
    return *this;
}

[[nodiscard]] inline const std::uint8_t* CStringBuffer::string(const std::size_t offset) const noexcept
{
    const std::uint8_t* data = m_buffer.data();
    return ((data != nullptr) && (offset > 0u) && (offset < m_size) && (data[offset - 1u] == 0u)) ? (data + offset) : nullptr;
}

[[nodiscard]] inline bool CStringBuffer::is_valid_offset(const std::size_t offset) const noexcept
{
    const std::uint8_t* data = m_buffer.data();
    return (data != nullptr) && (offset > 0u) && (offset < m_size) && (data[offset - 1u] == 0u);
}

[[nodiscard]] inline std::size_t CStringBuffer::append(const std::uint8_t* const string) noexcept
{
    return (string != nullptr) ? private_append(string, strz_length(string)) : std::size_t(0u);
}

[[nodiscard]] inline std::size_t CStringBuffer::append(const std::uint8_t* const string, const std::size_t length) noexcept
{
    return (string != nullptr) ? private_append(string, length) : std::size_t(0u);
}

inline bool CStringBuffer::resize(const std::size_t size, const bool clear_empty) noexcept
{
    return private_resize(size, pod_memory::buffer_growth_policy(size), clear_empty);
}

inline bool CStringBuffer::resize_exact(const std::size_t size, const bool clear_empty) noexcept
{
    return private_resize(size, size, clear_empty);
}

inline bool CStringBuffer::reserve(const std::size_t minimum_capacity) noexcept
{
    return private_reallocate(pod_memory::buffer_growth_policy(minimum_capacity));
}

inline bool CStringBuffer::reserve_exact(const std::size_t exact_capacity) noexcept
{
    return private_reallocate(exact_capacity);
}

inline bool CStringBuffer::ensure_free(const std::size_t length) noexcept
{
    return private_reallocate(m_size + length + (m_buffer.empty() ? 2u : 1u));
}

inline bool CStringBuffer::shrink_to_fit() noexcept
{
    return private_reallocate(m_size);
}

inline void CStringBuffer::deallocate() noexcept
{
    m_buffer.deallocate();
    m_size = 0u;
}

[[nodiscard]] inline bool CStringBuffer::check_integrity() const noexcept
{
    const std::uint8_t* data = m_buffer.data();
    const std::size_t capacity = m_buffer.size();

    if (data == nullptr)
    {
        if ((capacity != 0u) || (m_size != 0u))
        {
            return failed_integrity_check();
        }
    }
    else
    {
        if ((capacity == 0u) || (m_size == 0u) || (m_size > capacity))
        {
            return failed_integrity_check();
        }

        if ((data[0] != 0u) || (data[m_size - 1u] != 0u))
        {
            return failed_integrity_check();
        }
    }
    return true;
}

inline std::size_t CStringBuffer::private_append(const std::uint8_t* const string, const std::size_t length) noexcept
{
    if (ensure_free(length))
    {
        VE_HARD_ASSERT((m_buffer.data() != nullptr) && ((m_size + length + 1u) <= m_buffer.size()));
        std::size_t offset = m_size;
        std::uint8_t* data = m_buffer.data();
        std::memcpy((data + m_size), string, length);
        m_size += length;
        data[m_size] = 0u;
        ++m_size;
        return offset;
    }
    return k_invalid_offset;
}

inline bool CStringBuffer::private_reallocate(const std::size_t capacity) noexcept
{
    if (capacity > m_buffer.size())
    {
        if (m_buffer.empty())
        {
            if (m_buffer.allocate(capacity))
            {
                m_buffer.data()[0] = 0u;
                m_size = 1u;
                return true;
            }
            m_size = 0u;
            return false;
        }
        return m_buffer.reallocate(capacity);
    }
    return true;
}

inline bool CStringBuffer::private_resize(const std::size_t size, const std::size_t capacity, const bool clear_empty) noexcept
{
    VE_HARD_ASSERT((size != 0u) && (size <= capacity));
    if ((size != 0u) && (size <= capacity) && ((size <= m_buffer.size()) || private_reallocate(capacity)))
    {
        VE_HARD_ASSERT((m_buffer.data() != nullptr) && (size <= m_buffer.size()));
        if (clear_empty && (size > m_size))
        {
            memset((m_buffer.data() + m_size), 0, (size - m_size));
        }
        m_size = size;
        return true;
    }
    return false;
}

[[nodiscard]] inline bool CStringBuffer::failed_integrity_check() noexcept
{
    return VE_FAIL_SAFE_ASSERT(false);
}

//==============================================================================
//  CStableStringBuffer
//==============================================================================

inline CStableStringBuffer::CStableStringBuffer(CStableStringBuffer&& other) noexcept
{
    m_string_buffer = std::move(other.m_string_buffer);
    m_string_refs = std::move(other.m_string_refs);
    m_ref_index_to_id = std::move(other.m_ref_index_to_id);
    m_id_to_ref_index = std::move(other.m_id_to_ref_index);
    m_sorted_ref_indices = std::move(other.m_sorted_ref_indices);
}

inline CStableStringBuffer& CStableStringBuffer::operator=(CStableStringBuffer&& other) noexcept
{
    if (this != &other)
    {
        m_string_buffer = std::move(other.m_string_buffer);
        m_string_refs = std::move(other.m_string_refs);
        m_ref_index_to_id = std::move(other.m_ref_index_to_id);
        m_id_to_ref_index = std::move(other.m_id_to_ref_index);
        m_sorted_ref_indices = std::move(other.m_sorted_ref_indices);
    }
    return *this;
}

inline CStringView CStableStringBuffer::view(const std::size_t id) const noexcept
{
    if (is_valid_id(id))
    {
        const StringRef& ref = m_string_refs[m_id_to_ref_index[id]];
        return CStringView{ m_string_buffer.string(ref.offset), ref.length };
    }
    return CStringView{};
}

[[nodiscard]] inline bool CStableStringBuffer::is_valid_id(const std::size_t id) const noexcept
{
    return (id != k_invalid_id) && (id < m_id_to_ref_index.size());
}

[[nodiscard]] inline std::size_t CStableStringBuffer::id_to_ref_index(const std::size_t id) const noexcept
{
    if ((id != k_invalid_id) && (id < m_id_to_ref_index.size()))
    {
        return m_id_to_ref_index[id];
    }
    return k_invalid_ref_index;
}

[[nodiscard]] inline std::size_t CStableStringBuffer::ref_index_to_id(const std::size_t ref_index) const noexcept
{
    if ((ref_index != k_invalid_ref_index) && (ref_index < m_ref_index_to_id.size()))
    {
        return m_ref_index_to_id[ref_index];
    }
    return k_invalid_id;
}

[[nodiscard]] inline std::size_t CStableStringBuffer::rank_to_ref_index(const std::size_t rank) const noexcept
{
    if ((rank > 0u) && (rank < m_sorted_ref_indices.size()))
    {
        return m_sorted_ref_indices[rank];
    }
    return k_invalid_ref_index;
}

[[nodiscard]] inline std::size_t CStableStringBuffer::ref_index_to_rank(const std::size_t ref_index) const noexcept
{
    if (ref_index != k_invalid_ref_index)
    {
        const std::size_t size = m_sorted_ref_indices.size();
        if (ref_index < size)
        {
            const std::size_t* sorted_ref_indices_data = m_sorted_ref_indices.data();
            if (sorted_ref_indices_data[ref_index] == ref_index)
            {   //  this ref_index is in it's sorted location (O(1) for sorted data)
                return ref_index;
            }
            for (std::size_t rank = 1u; rank < size; ++rank)
            {
                if (sorted_ref_indices_data[rank] == ref_index)
                {
                    return rank;
                }
            }
        }
    }
    return k_invalid_rank;
}

[[nodiscard]] inline std::size_t CStableStringBuffer::find_id(const std::uint8_t* const string) noexcept
{
    return (string != nullptr) ? private_find_id(string, strz_length(string)) : k_invalid_id;
}

[[nodiscard]] inline std::size_t CStableStringBuffer::find_id(const std::uint8_t* const string, const std::size_t length) noexcept
{
    return (string != nullptr) ? private_find_id(string, length) : k_invalid_id;
}

[[nodiscard]] inline std::size_t CStableStringBuffer::append(const std::uint8_t* const string) noexcept
{
    return (string != nullptr) ? private_append(string, strz_length(string)) : k_invalid_id;
}

[[nodiscard]] inline std::size_t CStableStringBuffer::append(const std::uint8_t* const string, const std::size_t length) noexcept
{
    return (string != nullptr) ? private_append(string, length) : k_invalid_id;
}

inline bool CStableStringBuffer::sort() noexcept
{
    bool success = true;
    if (m_string_refs.size() > 2u)
    {   //  there is something to sort
        VE_HARD_ASSERT(check_integrity());
        CStringBuffer string_buffer;
        TDynamicPodArray<StringRef> string_refs;
        TDynamicPodArray<std::size_t> ref_index_to_id;
        TDynamicPodArray<std::size_t> id_to_ref_index;
        TDynamicPodArray<std::size_t> sorted_ref_indices;
        if (success) success = string_buffer.reserve_exact(m_string_buffer.size());
        if (success) success = string_refs.allocate(m_string_refs.size(), false);
        if (success) success = ref_index_to_id.allocate(m_ref_index_to_id.size(), false);
        if (success) success = id_to_ref_index.allocate(m_id_to_ref_index.size(), false);
        if (success) success = sorted_ref_indices.allocate(m_sorted_ref_indices.size(), false);
        if (success)
        {
            string_refs[0] = StringRef{ 0u, 0u };
            ref_index_to_id[0] = k_invalid_id;
            id_to_ref_index[0] = k_invalid_ref_index;
            sorted_ref_indices[0] = k_invalid_ref_index;
            const std::size_t count = string_refs.size();
            for (std::size_t index = 1u; index < count; ++index)
            {
                const std::size_t ref_index = m_sorted_ref_indices[index];
                const std::size_t id = m_ref_index_to_id[ref_index];
                const StringRef& ref = m_string_refs[ref_index];
                string_refs[index] = StringRef{ string_buffer.append(m_string_buffer.string(ref.offset), ref.length), ref.length };
                ref_index_to_id[index] = id;
                id_to_ref_index[id] = index;
                sorted_ref_indices[index] = index;
            }
            m_string_buffer = std::move(string_buffer);
            m_string_refs = std::move(string_refs);
            m_ref_index_to_id = std::move(ref_index_to_id);
            m_id_to_ref_index = std::move(id_to_ref_index);
            m_sorted_ref_indices = std::move(sorted_ref_indices);
            VE_HARD_ASSERT(check_integrity());
        }
    }
    return success;
}

inline bool CStableStringBuffer::initialise(const std::size_t string_count, const std::size_t string_storage_size) noexcept
{
    bool success = true;
    if (success) success = m_string_buffer.reserve(string_storage_size);
    if (success) success = m_string_refs.reserve(string_count);
    if (success) success = m_ref_index_to_id.reserve(string_count);
    if (success) success = m_id_to_ref_index.reserve(string_count);
    if (success) success = m_sorted_ref_indices.reserve(string_count);
    if (success) success = m_string_refs.push_back(StringRef{});
    if (success) success = m_ref_index_to_id.push_back(std::size_t{ 0 });
    if (success) success = m_id_to_ref_index.push_back(std::size_t{ 0 });
    if (success) success = m_sorted_ref_indices.push_back(std::size_t{ 0 });
    if (!success) deallocate();
    VE_HARD_ASSERT(success);
    return success;
}

inline bool CStableStringBuffer::ensure_free(const std::size_t length) noexcept
{   //  success guarantees that a subsequent allocation will succeed
    bool success =
        m_string_buffer.ensure_free(length) &&
        m_string_refs.ensure_free(1u) &&
        m_ref_index_to_id.ensure_free(1u) &&
        m_id_to_ref_index.ensure_free(1u) &&
        m_sorted_ref_indices.ensure_free(1u);
    VE_HARD_ASSERT(success);
    return success;
}

inline bool CStableStringBuffer::shrink_to_fit() noexcept
{
    bool success = true;
    if (!m_string_buffer.shrink_to_fit()) success = false;
    if (!m_string_refs.shrink_to_fit()) success = false;
    if (!m_ref_index_to_id.shrink_to_fit()) success = false;
    if (!m_id_to_ref_index.shrink_to_fit()) success = false;
    if (!m_sorted_ref_indices.shrink_to_fit()) success = false;
    return success;
}

inline void CStableStringBuffer::deallocate() noexcept
{
    m_string_buffer.deallocate();
    m_string_refs.deallocate();
    m_ref_index_to_id.deallocate();
    m_id_to_ref_index.deallocate();
    m_sorted_ref_indices.deallocate();
}

[[nodiscard]] inline bool CStableStringBuffer::check_integrity(const bool check_lexical_order) const noexcept
{
    if (!m_string_buffer.check_integrity())
    {   //  CStringBuffer::failed_integrity_check() already called
        return false;
    }

    const std::size_t count = m_string_refs.size();

    if ((m_ref_index_to_id.size() != count) || (m_id_to_ref_index.size() != count) || (m_sorted_ref_indices.size() != count))
    {   //  a basic check that the vectors are the same size failed
        return failed_integrity_check();
    }

    if (count == 0u)
    {   //  no further data exists to check the integrity of
        return true;
    }

    //  fetch the raw data pointers to avoid side-effects during the checks
    const std::uint8_t* string_buffer_data = m_string_buffer.data();
    const StringRef* string_refs_data = m_string_refs.data();
    const std::size_t* ref_index_to_id_data = m_ref_index_to_id.data();
    const std::size_t* id_to_ref_index_data = m_id_to_ref_index.data();
    const std::size_t* sorted_ref_indices_data = m_sorted_ref_indices.data();

    //  check the trivial invariants for the default invalid empty reference
    if ((string_refs_data[0].offset != 0u) || (string_refs_data[0].length != 0u))
    {
        return failed_integrity_check();
    }
    if ((ref_index_to_id_data[0] != k_invalid_id) || (id_to_ref_index_data[0] != k_invalid_ref_index) || (sorted_ref_indices_data[0] != k_invalid_ref_index))
    {
        return failed_integrity_check();
    }

    //  validate the bijection of ref_index_to_id_data and id_to_ref_index_data
    for (std::size_t index = 1u; index < count; ++index)
    {
        const std::size_t id = ref_index_to_id_data[index];
        const std::size_t ref_index = id_to_ref_index_data[index];
        if ((id >= count) || (ref_index >= count))
        {   //  the id or ref_index are out of range
            return failed_integrity_check();
        }
        if ((id_to_ref_index_data[id] != index) || (ref_index_to_id_data[ref_index] != index))
        {   //  the bijection is broken
            return failed_integrity_check();
        }
    }

    //  perform a basic validatation of the string references
    const std::size_t string_buffer_size = m_string_buffer.size();
    for (std::size_t ref_index = 1u; ref_index < count; ++ref_index)
    {
        const StringRef& ref = string_refs_data[ref_index];
        if ((ref.offset == k_invalid_offset) || (ref.offset >= string_buffer_size) || (ref.length > (string_buffer_size - ref.offset - 1u)))
        {   //  the string ref structure integrity check failed
            return failed_integrity_check();
        }
        if ((!m_string_buffer.is_valid_offset(ref.offset)) || (string_buffer_data[ref.offset + ref.length] != 0u))
        {   //  the deep-dive string validation check failed
            return failed_integrity_check();
        }
    }

    //  validate the range and permutation of the sorted string references
    if (!algo::validate_permutations<std::size_t, std::uint32_t>(sorted_ref_indices_data, static_cast<std::uint32_t>(count), std::numeric_limits<std::uint32_t>::digits))
    {   //  the range and permutations check failed
        return failed_integrity_check();
    }

    //  validate the lexical ordering of the sorted string references
    if (check_lexical_order && (count > 2u))
    {
        const std::size_t initial_ref_index = sorted_ref_indices_data[1u];
        const StringRef& initial_ref = string_refs_data[initial_ref_index];
        CStringView prev_view{ (string_buffer_data + initial_ref.offset), initial_ref.length };
        for (std::size_t index = 2u; index < count; ++index)
        {
            const std::size_t ref_index = sorted_ref_indices_data[index];
            const StringRef& ref = string_refs_data[ref_index];
            const CStringView curr_view{ (string_buffer_data + ref.offset), ref.length };
            if (!(prev_view < curr_view))
            {   //  lexical ordering is broken
                return failed_integrity_check();
            }
            prev_view = curr_view;
        }
    }

    return true;
}

inline std::size_t CStableStringBuffer::private_find_ref_index(const std::uint8_t* const string, const std::size_t length, std::size_t& insert_at) noexcept
{
    insert_at = 0u;
    CStringView view{ string, length };
    std::size_t lower = 1u;
    std::size_t upper = m_sorted_ref_indices.size();
    while (lower < upper)
    {
        std::size_t pivot = (lower + upper) >> 1;
        std::size_t ref_index = m_sorted_ref_indices[pivot];
        const StringRef& ref = m_string_refs[ref_index];
        std::int32_t relationship = view.relationship(CStringView{ (m_string_buffer.data() + ref.offset), ref.length });
        if (relationship == 0)
        {
            return ref_index;
        }
        if (relationship < 0)
        {
            upper = pivot;
        }
        else
        {
            lower = pivot + 1u;
        }
    }
    insert_at = lower;
    return k_invalid_ref_index;
}

inline std::size_t CStableStringBuffer::private_find_id(const std::uint8_t* const string, const std::size_t length) noexcept
{
    std::size_t id{ k_invalid_id };
    if (m_string_refs.size() > 1u)
    {
        std::size_t insert_at{ 0 };
        std::size_t ref_index = private_find_ref_index(string, length, insert_at);
        if (ref_index != k_invalid_ref_index)
        {
            id = m_ref_index_to_id[ref_index];
        }
    }
    return id;
}

inline std::size_t CStableStringBuffer::private_append(const std::uint8_t* const string, const std::size_t length) noexcept
{
    std::size_t id{ k_invalid_id };
    if (m_string_refs.size() != 0u)
    {
        std::size_t insert_at{ 0 };
        std::size_t ref_index = private_find_ref_index(string, length, insert_at);
        if (ref_index != k_invalid_ref_index)
        {
            id = m_ref_index_to_id[ref_index];
        }
        else if (ensure_free(length))
        {
            VE_HARD_ASSERT((insert_at != k_invalid_ref_index) && (insert_at <= m_sorted_ref_indices.size()));
            id = m_string_refs.size();
            m_string_refs.push_back(StringRef{ m_string_buffer.append(string, length), length });
            m_ref_index_to_id.push_back(id);
            m_id_to_ref_index.push_back(id);
            m_sorted_ref_indices.insert(insert_at, id);
        }
    }
    else if (initialise(pod_memory::array_growth_policy(1u), pod_memory::buffer_growth_policy(length + 2u)))
    {
        id = 1u;
        m_string_refs.push_back(StringRef{ m_string_buffer.append(string, length), length });
        m_ref_index_to_id.push_back(id);
        m_id_to_ref_index.push_back(id);
        m_sorted_ref_indices.push_back(id);
    }
    return id;
}

[[nodiscard]] inline bool CStableStringBuffer::failed_integrity_check() noexcept
{
    return VE_FAIL_SAFE_ASSERT(false);
}

#endif  //  #ifndef STRING_BUFFERS_HPP_INCLUDED