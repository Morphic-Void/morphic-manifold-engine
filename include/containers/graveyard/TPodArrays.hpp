//  File:   TPodArrays.hpp
//  Author: Ritchie Brannan
//  Date:   17 Feb 26
//
//  POD array/vector and array view noexcept container utilities
//
//  Notes:
//  - Requires C++ 17 or greater
//  - T must be trivially copyable.
//  - No exceptions.
//  - View empty state is canonical: { nullptr, 0 } ONLY. { data, 0 } is disallowed.
//  - Vector is STL-adjacent:
//      * resize(size) changes size
//      * reserve(minimum_capacity) ensures total capacity >= minimum_capacity
//      * ensure_free(extra) ensures (capacity - size) >= extra
//      * shrink_to_fit() reallocates capacity to match size
//  - Observation accessors are fail-safe:
//      * if storage invariants appear broken size/capacity/empty
//        report 0/true to avoid implying accessible elements.
//  - For TPodVector, invariants broken means:
//      * {data == nullptr, capacity != 0} or
//      * {data != nullptr, capacity == 0} or
//      * {size > capacity}
//  - For TPodArrayView, TPodArray and TDynamicPodArray, invariants broken means:
//      * {data == nullptr, size != 0} or
//      * {data != nullptr, size == 0}
//

#pragma once

#ifndef TPOD_ARRAYS_HPP_INCLUDED
#define TPOD_ARRAYS_HPP_INCLUDED

#include <algorithm>    //  std::min
#include <cstddef>      //  std::size_t
#include <cstring>      //  std::memcpy, std::memmove, std::memset
#include <type_traits>  //  std::is_trivially_copyable, std::is_const, std::remove_const_t
#include <utility>      //  std::move

#include "internal/PodMemory.hpp"
#include "debug/debug.hpp"

//==============================================================================
//  Shared helpers
//==============================================================================

template<typename T>
T& array_oob_ref() noexcept
{
    VE_HARD_ASSERT(false);
    using U = std::remove_const_t<T>;
    static U last_gasp;
    std::memset(&last_gasp, 0, sizeof(U));
    return static_cast<T&>(last_gasp);
}

template<typename T>
const T& const_array_oob_ref() noexcept
{
    VE_HARD_ASSERT(false);
    using U = std::remove_const_t<T>;
    static U last_gasp;
    std::memset(&last_gasp, 0, sizeof(U));
    return static_cast<const T&>(last_gasp);
}

//==============================================================================
//  Forward declarations
//==============================================================================

template<typename T> class TPodArrayView;
template<typename T> class TPodArray;
template<typename T> class TDynamicPodArray;
template<typename T> class TPodVector;

//==============================================================================
//  Detached storage blocks (handoff tokens).
//==============================================================================

template<typename T>
struct TPodArrayStorage
{
    T* data = nullptr;
    std::size_t size = 0u;
};

template<typename T>
struct TPodVectorStorage
{
    T* data = nullptr;
    std::size_t size = 0u;
    std::size_t capacity = 0u;
};

//==============================================================================
//  TPodArrayView<T>
//  Non-owning view over a contiguous POD range.
//  Canonical empty is { data = nullptr, size = 0 } ONLY.
//==============================================================================

template<typename T>
class TPodArrayView
{
private:
    static_assert(std::is_trivially_copyable<T>::value, "TPodArrayView<T> requires trivially copyable T.");

public:
    using value_type = T;

    TPodArrayView() noexcept = default;
    TPodArrayView(TPodArrayView<T>& other) noexcept = default;
    TPodArrayView(TPodArray<T>& other) noexcept : m_storage({ other.data(), other.size() }) {}
    TPodArrayView(TDynamicPodArray<T>& other) noexcept : m_storage({ other.data(), other.size() }) {}
    TPodArrayView(TPodVector<T>& other) noexcept : m_storage({ other.data(), other.size() }) {}
    TPodArrayView(T* const data, const std::size_t size) noexcept { (void)set(data, size); };
    ~TPodArrayView() noexcept = default;

    TPodArrayView<T>& operator=(TPodArrayView<T>& other) noexcept = default;
    TPodArrayView<T>& operator=(TPodArray<T>& other) noexcept { m_storage = { other.data(), other.size() }; return *this; }
    TPodArrayView<T>& operator=(TDynamicPodArray<T>& other) noexcept { m_storage = { other.data(), other.size() }; return *this; }
    TPodArrayView<T>& operator=(TPodVector<T>& other) noexcept { m_storage = { other.data(), other.size() }; return *this; }
    TPodArrayView<T>& set(T* const data, const std::size_t size) noexcept;

    //  Sub-views.
    TPodArrayView subview(const std::size_t offset, const std::size_t count) const noexcept;
    TPodArrayView head_to(const std::size_t count) const noexcept;
    TPodArrayView tail_from(const std::size_t offset) const noexcept;

    //  Common accessors.
    T* data() noexcept { return is_safe() ? m_storage.data : nullptr; }
    const T* data() const noexcept { return is_safe() ? m_storage.data : nullptr; }
    std::size_t size() const noexcept { return is_safe() ? m_storage.size : 0u; }
    std::size_t size_in_bytes() const noexcept { return is_safe() ? (m_storage.size * sizeof(T)) : 0u; }
    bool empty() const noexcept { return (m_storage.data == nullptr) || (m_storage.size == 0u); }

    //  Array accessors.
    T& operator[](const std::size_t index) noexcept;
    const T& operator[](const std::size_t index) const noexcept;

private:
    static constexpr std::size_t k_max_size = pod_memory::t_max_elements<T>();

    bool tripwire_is_safe() const noexcept;
    bool is_safe() const noexcept { return ((m_storage.data == nullptr) == (m_storage.size == 0u)) && (m_storage.size <= k_max_size); }

    TPodArrayStorage<T> m_storage;
};

//==============================================================================
//  TPodArray<T>
//  Owning unique array: data + size. No resizing (immutable size/capacity).
//  Canonical empty is { data = nullptr, size = 0 } ONLY.
// 
//  Adoption of TPodVector by TPodArray may zero any
//  previously unused capacity to make the entire allocation coherent
//  because size becomes capacity.
//==============================================================================

template<typename T>
class TPodArray
{
private:
    static_assert(std::is_trivially_copyable<T>::value, "TPodArray<T> requires trivially copyable T.");
    static_assert(!std::is_const<T>::value, "TPodArray<T> requires non-const T.");

public:
    using value_type = T;

    TPodArray(const TPodArray&) = delete;
    TPodArray& operator=(const TPodArray&) = delete;

    TPodArray() noexcept;
    TPodArray(TPodArray<T>&& other) noexcept;
    TPodArray(TDynamicPodArray<T>&& other) noexcept;
    TPodArray(TPodVector<T>&& other) noexcept;  //  tail is zeroed

    ~TPodArray() noexcept;

    TPodArray<T>& operator=(TPodArray<T>&& other) noexcept;
    TPodArray<T>& operator=(TDynamicPodArray<T>&& other) noexcept;
    TPodArray<T>& operator=(TPodVector<T>&& other) noexcept;    //  tail is zeroed

    //  Handoff token (this container becomes empty).
    TPodArrayStorage<T> disown() noexcept;

    //  Ownership transfer (allocation free).
    void adopt(TPodArray<T>&& donor) noexcept;
    void adopt(TDynamicPodArray<T>&& donor) noexcept;

    //  Vector ownership transfer (allocation free).
    // - size = donor.capacity
    // - adopt() zeroes any empty capacity
    // - adopt_uninit_tail() leaves empty capacity space unmodified
    // - returns donor's old logical size (may be 0)
    std::size_t adopt(TPodVector<T>&& donor) noexcept;
    std::size_t adopt_uninit_tail(TPodVector<T>&& donor) noexcept;

    //  Views and sub-views.
    TPodArrayView<T> view() noexcept { return TPodArrayView<T>(m_storage.data, m_storage.size); }
    TPodArrayView<T> subview(const std::size_t offset, const std::size_t count) noexcept { return view().subview(offset, count); }
    TPodArrayView<T> head_to(const std::size_t count) noexcept { return view().head_to(count); }
    TPodArrayView<T> tail_from(const std::size_t offset) noexcept { return view().tail_from(offset); }
    TPodArrayView<const T> view() const noexcept { return TPodArrayView<const T>(m_storage.data, m_storage.size); }
    TPodArrayView<const T> subview(const std::size_t offset, const std::size_t count) const noexcept { return view().subview(offset, count); }
    TPodArrayView<const T> head_to(const std::size_t count) const noexcept { return view().head_to(count); }
    TPodArrayView<const T> tail_from(const std::size_t offset) const noexcept { return view().tail_from(offset); }

    //  Common accessors.
    T* data() noexcept { return is_safe() ? m_storage.data : nullptr; }
    const T* data() const noexcept { return is_safe() ? m_storage.data : nullptr; }
    std::size_t size() const noexcept { return is_safe() ? m_storage.size : 0u; }
    std::size_t size_in_bytes() const noexcept { return is_safe() ? (m_storage.size * sizeof(T)) : 0u; }
    bool empty() const noexcept { return (m_storage.data == nullptr) || (m_storage.size == 0u); }

    //  Array accessors.
    T& operator[](const std::size_t index) noexcept;
    const T& operator[](const std::size_t index) const noexcept;

    //  Capacity management.
    //  - deallocate() discards all memory allocation, no-op if empty
    void deallocate() noexcept;

private:
    static constexpr std::size_t k_max_size = pod_memory::t_max_elements<T>();

    void private_zero_tail(const std::size_t tail_start) noexcept;
    bool tripwire_is_safe() const noexcept;
    bool is_safe() const noexcept { return ((m_storage.data == nullptr) == (m_storage.size == 0u)) && (m_storage.size <= k_max_size); }

    TPodArrayStorage<T> m_storage;
};

//==============================================================================
//  TDynamicPodArray<T>
//  Owning unique resizable array: data + size. No spare capacity.
//  Canonical empty is { data = nullptr, size = 0 } ONLY.
//
//  Adoption of TPodVector by TDynamicPodArray may zero any
//  previously unused capacity to make the entire allocation coherent
//  because size becomes capacity.
//==============================================================================

template<typename T>
class TDynamicPodArray
{
private:
    static_assert(std::is_trivially_copyable<T>::value, "TDynamicPodArray<T> requires trivially copyable T.");
    static_assert(!std::is_const<T>::value, "TDynamicPodArray<T> requires non-const T.");

public:
    using value_type = T;

    TDynamicPodArray(const TDynamicPodArray&) = delete;
    TDynamicPodArray& operator=(const TDynamicPodArray&) = delete;

    TDynamicPodArray() noexcept;
    TDynamicPodArray(TPodArray<T>&& other) noexcept;
    TDynamicPodArray(TDynamicPodArray<T>&& other) noexcept;
    TDynamicPodArray(TPodVector<T>&& other) noexcept;   //  tail is zeroed

    ~TDynamicPodArray() noexcept;

    TDynamicPodArray<T>& operator=(TPodArray<T>&& other) noexcept;
    TDynamicPodArray<T>& operator=(TDynamicPodArray<T>&& other) noexcept;
    TDynamicPodArray<T>& operator=(TPodVector<T>&& other) noexcept; //  tail is zeroed

    //  Handoff token (this container becomes empty).
    TPodArrayStorage<T> disown() noexcept;

    //  Ownership transfer (allocation free).
    void adopt(TPodArray<T>&& donor) noexcept;
    void adopt(TDynamicPodArray&& donor) noexcept;

    //  Vector ownership transfer (allocation free).
    // - size = donor.capacity
    // - adopt() zeroes any empty capacity
    // - adopt_uninit_tail() leaves empty capacity space unmodified
    // - returns donor's old logical size (may be 0)
    std::size_t adopt(TPodVector<T>&& donor) noexcept;
    std::size_t adopt_uninit_tail(TPodVector<T>&& donor) noexcept;

    //  Views and sub-views.
    TPodArrayView<T> view() noexcept { return TPodArrayView<T>(m_storage.data, m_storage.size); }
    TPodArrayView<T> subview(const std::size_t offset, const std::size_t count) noexcept { return view().subview(offset, count); }
    TPodArrayView<T> head_to(const std::size_t count) noexcept { return view().head_to(count); }
    TPodArrayView<T> tail_from(const std::size_t offset) noexcept { return view().tail_from(offset); }
    TPodArrayView<const T> view() const noexcept { return TPodArrayView<const T>(m_storage.data, m_storage.size); }
    TPodArrayView<const T> subview(const std::size_t offset, const std::size_t count) const noexcept { return view().subview(offset, count); }
    TPodArrayView<const T> head_to(const std::size_t count) const noexcept { return view().head_to(count); }
    TPodArrayView<const T> tail_from(const std::size_t offset) const noexcept { return view().tail_from(offset); }

    //  Common accessors.
    T* data() noexcept { return is_safe() ? m_storage.data : nullptr; }
    const T* data() const noexcept { return is_safe() ? m_storage.data : nullptr; }
    std::size_t size() const noexcept { return is_safe() ? m_storage.size : 0u; }
    std::size_t size_in_bytes() const noexcept { return is_safe() ? (m_storage.size * sizeof(T)) : 0u; }
    bool empty() const noexcept { return (m_storage.data == nullptr) || (m_storage.size == 0u); }

    //  Array accessors.
    T& operator[](const std::size_t index) noexcept;
    const T& operator[](const std::size_t index) const noexcept;

    //  Capacity management (state unchanged on failure).
    //  - allocate() discards content, optionally zero
    //  - reallocate() retains content that fits, optionally zero additional space
    //  - deallocate() discards all memory allocation, no-op if empty
    bool allocate(const std::size_t size, const bool zero = true) noexcept;
    bool reallocate(const std::size_t size, const bool zero_extra = true) noexcept;
    void deallocate() noexcept;

private:
    static constexpr std::size_t k_max_size = pod_memory::t_max_elements<T>();

    bool allocation_resize(const std::size_t size, const bool copy = true, const bool clear = false) noexcept;
    void private_zero_tail(const std::size_t tail_start) noexcept;
    bool tripwire_is_safe() const noexcept;
    bool is_safe() const noexcept { return ((m_storage.data == nullptr) == (m_storage.size == 0u)) && (m_storage.size <= k_max_size); }

    TPodArrayStorage<T> m_storage;
};

//==============================================================================
//  TPodVector<T>
//  Owning unique vector-like container: data + size + capacity.
//  Canonical empty is { data = nullptr, size = 0, capacity = 0 } ONLY.
//
//  Allocated-empty {data != nullptr, size = 0, capacity > 0 } is valid,
//    for example after clear() or after reserve() with size still 0.
//
//  Observation is fail-safe:
//    if storage appears unusable (data==nullptr or capacity==0),
//    the common accessors report nullptr or 0 and empty() reports true,
//    even if this is inconsistent with internal fields.
// 
//  Spare capacity bytes are not initialized/zeroed by reserve;
//  only elements in [0,size) are initialized.
//==============================================================================

template<typename T>
class TPodVector
{
private:
    static_assert(std::is_trivially_copyable<T>::value, "TPodVector<T> requires trivially copyable T.");
    static_assert(!std::is_const<T>::value, "TPodVector<T> requires non-const T.");

public:
    using value_type = T;

    TPodVector(const TPodVector&) = delete;
    TPodVector& operator=(const TPodVector&) = delete;

    TPodVector() noexcept;
    TPodVector(TPodArray<T>&& other) noexcept;
    TPodVector(TDynamicPodArray<T>&& other) noexcept;
    TPodVector(TPodVector<T>&& other) noexcept;

    ~TPodVector() noexcept;

    TPodVector<T>& operator=(TPodArray<T>&& other) noexcept;
    TPodVector<T>& operator=(TDynamicPodArray<T>&& other) noexcept;
    TPodVector<T>& operator=(TPodVector<T>&& other) noexcept;

    //  Views.
    TPodArrayView<T> view() noexcept { return TPodArrayView<T>(m_storage.data, m_storage.size); }
    TPodArrayView<const T> view() const noexcept { return TPodArrayView<const T>(m_storage.data, m_storage.size); }

    //  Empty the vector, keeping the allocation.
    void clear() noexcept { m_storage.size = 0; }

    //  Handoff token (this container becomes empty).
    TPodVectorStorage<T> disown() noexcept;

    //  Ownership transfer (allocation free).
    void adopt(TPodArray<T>&& donor) noexcept;    //  capacity = size = donor.size
    void adopt(TDynamicPodArray<T>&& donor) noexcept; //  capacity = size = donor.size
    void adopt(TPodVector<T>&& donor) noexcept;

    //  Common accessors.
    T* data() noexcept { return is_safe() ? m_storage.data : nullptr; }
    const T* data() const noexcept { return is_safe() ? m_storage.data : nullptr; }
    std::size_t size() const noexcept { return is_safe() ? m_storage.size : 0u; }
    std::size_t capacity() const noexcept { return is_safe() ? m_storage.capacity : 0u; }
    std::size_t available() const noexcept { return is_safe() ? (m_storage.capacity - m_storage.size) : 0u; }
    std::size_t size_in_bytes() const noexcept { return size() * sizeof(T); }
    std::size_t capacity_bytes() const noexcept { return capacity() * sizeof(T); }
    std::size_t available_bytes() const noexcept { return available() * sizeof(T); }
    bool empty() const noexcept { return (m_storage.data == nullptr) || (m_storage.size == 0u) || (m_storage.capacity == 0u); }

    //  Element accessors.
    T& first() noexcept { return (*this)[0u]; }
    T& last() noexcept { return (*this)[m_storage.size - 1u]; }
    const T& first() const noexcept { return (*this)[0u]; }
    const T& last() const noexcept { return (*this)[m_storage.size - 1u]; }
    T& operator[](const std::size_t index) noexcept;
    const T& operator[](const std::size_t index) const noexcept;
    bool insert(const std::size_t index, const T& value) noexcept;
    bool insert(const std::size_t index) noexcept;
    bool erase(const std::size_t index) noexcept;
    bool swap_insert(const std::size_t index, const T& value) noexcept;
    bool swap_erase(const std::size_t index) noexcept;
    bool push_back(const T& value) noexcept;
    bool push_back() noexcept;
    bool pop_back(T& value) noexcept;
    bool pop_back() noexcept;
    std::size_t try_push_many(TPodArrayView<const T> src) noexcept;   //  returns count pushed from prefix
    std::size_t try_pop_many(TPodArrayView<T> dst) noexcept;          //  returns count popped into prefix

    //  Capacity management (may grow and relocate, state unchanged on failure).
    bool resize(const std::size_t size) noexcept;
    bool reserve(const std::size_t minimum_capacity) noexcept;
    bool reserve_exact(const std::size_t exact_capacity) noexcept;
    bool ensure_free(const std::size_t extra) noexcept;
    bool shrink_to_fit() noexcept;
    void deallocate() noexcept; //  free and become empty (no-op if empty).

private:
    static constexpr std::size_t k_max_capacity = pod_memory::t_max_elements<T>();

    bool private_reallocate(const std::size_t capacity) noexcept;
    void private_zero_tail(const std::size_t tail_start) noexcept;
    bool tripwire_is_safe() const noexcept;
    bool is_safe() const noexcept { return ((m_storage.data == nullptr) == (m_storage.capacity == 0u)) && (m_storage.size <= m_storage.capacity) && (m_storage.capacity <= k_max_capacity); }

    TPodVectorStorage<T> m_storage;
};

//==============================================================================
//  TPodArrayView<T>
//==============================================================================

template<typename T>
inline TPodArrayView<T>& TPodArrayView<T>::set(T* const data, const std::size_t size) noexcept
{
    if ((data == nullptr) || (size == 0u))
    {
        m_storage.data = nullptr;
        m_storage.size = 0u;
    }
    else
    {
        m_storage.data = data;
        m_storage.size = size;
    }
}

template<typename T>
inline TPodArrayView<T> TPodArrayView<T>::subview(const std::size_t offset, const std::size_t count) const noexcept
{
    if (!tripwire_is_safe() || (count == 0u) || (offset >= m_storage.size) || (count > (m_storage.size - offset)))
    {
        return TPodArrayView(nullptr, 0u);
    }
    return TPodArrayView((m_storage.data + offset), count);
}

template<typename T>
inline TPodArrayView<T> TPodArrayView<T>::head_to(const std::size_t count) const noexcept
{
    if (!tripwire_is_safe() || (count == 0u) || (count > m_storage.size))
    {
        return TPodArrayView(nullptr, 0u);
    }
    return TPodArrayView(m_storage.data, count);
}

template<typename T>
inline TPodArrayView<T> TPodArrayView<T>::tail_from(const std::size_t offset) const noexcept
{
    if (!tripwire_is_safe() || (offset >= m_storage.size))
    {
        return TPodArrayView(nullptr, 0u);
    }
    return TPodArrayView((m_storage.data + offset), (m_storage.size - offset));
}

template<typename T>
inline T& TPodArrayView<T>::operator[](const std::size_t index) noexcept
{
#if defined(_DEBUG)
    return ((m_storage.data == nullptr) || (index >= m_storage.size)) ? array_oob_ref<T>() : m_storage.data[index];
#else
    return m_storage.data[index];
#endif
}

template<typename T>
inline const T& TPodArrayView<T>::operator[](const std::size_t index) const noexcept
{
#if defined(_DEBUG)
    return ((m_storage.data == nullptr) || (index >= m_storage.size)) ? const_array_oob_ref<T>() : m_storage.data[index];
#else
    return m_storage.data[index];
#endif
}

template<typename T>
inline bool TPodArrayView<T>::tripwire_is_safe() const noexcept
{
    return VE_FAIL_SAFE_ASSERT(is_safe());
}

//==============================================================================
//  TPodArray<T>
//==============================================================================

template<typename T>
inline TPodArray<T>::TPodArray() noexcept
{
    m_storage = {};
}

template<typename T>
inline TPodArray<T>::TPodArray(TPodArray&& other) noexcept
{
    m_storage = other.m_storage;
    other.m_storage = {};
}

template<typename T>
inline TPodArray<T>::TPodArray(TDynamicPodArray<T>&& other) noexcept
{
    m_storage = {};
    adopt(std::move(other));
}

template<typename T>
inline TPodArray<T>::TPodArray(TPodVector<T>&& other) noexcept
{
    m_storage = {};
    (void)adopt(std::move(other));  //  default: zero tail
}

template<typename T>
inline TPodArray<T>::~TPodArray() noexcept
{
    deallocate();
}

template<typename T>
inline TPodArray<T>& TPodArray<T>::operator=(TPodArray&& other) noexcept
{
    if (this != &other)
    {
        deallocate();
        m_storage = other.m_storage;
        other.m_storage = {};
    }
    return *this;
}

template<typename T>
inline TPodArray<T>& TPodArray<T>::operator=(TDynamicPodArray<T>&& other) noexcept
{
    adopt(std::move(other));
    return *this;
}

template<typename T>
inline TPodArray<T>& TPodArray<T>::operator=(TPodVector<T>&& other) noexcept
{
    (void)adopt(std::move(other));  //  default: zero tail
    return *this;
}

template<typename T>
inline TPodArrayStorage<T> TPodArray<T>::disown() noexcept
{
    TPodArrayStorage<T> storage = m_storage;
    m_storage = {};
    return storage;
}

template<typename T>
inline void TPodArray<T>::adopt(TPodArray&& donor) noexcept
{
    if (this != &donor)
    {
        deallocate();
        m_storage = donor.m_storage;
        donor.m_storage = {};
    }
}

template<typename T>
inline void TPodArray<T>::adopt(TDynamicPodArray<T>&& donor) noexcept
{
    deallocate();
    m_storage = donor.disown();
}

template<typename T>
inline std::size_t TPodArray<T>::adopt(TPodVector<T>&& donor) noexcept
{
    std::size_t donor_size = adopt_uninit_tail(std::move(donor));
    private_zero_tail(donor_size);
    return donor_size;
}

template<typename T>
inline std::size_t TPodArray<T>::adopt_uninit_tail(TPodVector<T>&& donor) noexcept
{
    std::size_t donor_size = 0u;
    if (donor.tripwire_is_safe())
    {
        deallocate();
        const TPodVectorStorage<T> storage = donor.disown();
        if ((storage.data != nullptr) && (storage.capacity != 0u))
        {
            m_storage.data = storage.data;
            m_storage.size = storage.capacity;
            donor_size = storage.size;
        }
    }
    return donor_size;
}

template<typename T>
inline T& TPodArray<T>::operator[](const std::size_t index) noexcept
{
#if defined(_DEBUG)
    return ((m_storage.data == nullptr) || (index >= m_storage.size)) ? array_oob_ref<T>() : m_storage.data[index];
#else
    return m_storage.data[index];
#endif
}

template<typename T>
inline const T& TPodArray<T>::operator[](const std::size_t index) const noexcept
{
#if defined(_DEBUG)
    return ((m_storage.data == nullptr) || (index >= m_storage.size)) ? const_array_oob_ref<T>() : m_storage.data[index];
#else
    return m_storage.data[index];
#endif
}

template<typename T>
inline void TPodArray<T>::deallocate() noexcept
{
    if (m_storage.data != nullptr)
    {
        pod_memory::t_deallocate<T>(m_storage.data);
    }
    m_storage = {};
}

template<typename T>
inline void TPodArray<T>::private_zero_tail(const std::size_t tail_start) noexcept
{
    if ((m_storage.data != nullptr) && (tail_start < m_storage.size))
    {
        std::memset((m_storage.data + tail_start), 0, (m_storage.size - tail_start) * sizeof(T));
    }
}

template<typename T>
inline bool TPodArray<T>::tripwire_is_safe() const noexcept
{
    return VE_FAIL_SAFE_ASSERT(is_safe());
}

//==============================================================================
//  TDynamicPodArray<T>
//==============================================================================

template<typename T>
inline TDynamicPodArray<T>::TDynamicPodArray() noexcept
{
    m_storage = {};
}

template<typename T>
inline TDynamicPodArray<T>::TDynamicPodArray(TPodArray<T>&& other) noexcept
{
    m_storage = {};
    adopt(std::move(other));
}

template<typename T>
inline TDynamicPodArray<T>::TDynamicPodArray(TDynamicPodArray&& other) noexcept
{
    m_storage = other.m_storage;
    other.m_storage = {};
}

template<typename T>
inline TDynamicPodArray<T>::TDynamicPodArray(TPodVector<T>&& other) noexcept
{
    m_storage = {};
    (void)adopt(std::move(other));  //  default: zero tail
}

template<typename T>
inline TDynamicPodArray<T>::~TDynamicPodArray() noexcept
{
    deallocate();
}

template<typename T>
inline TDynamicPodArray<T>& TDynamicPodArray<T>::operator=(TDynamicPodArray&& other) noexcept
{
    if (this != &other)
    {
        deallocate();
        m_storage = other.m_storage;
        other.m_storage = {};
    }
    return *this;
}

template<typename T>
inline TDynamicPodArray<T>& TDynamicPodArray<T>::operator=(TPodArray<T>&& other) noexcept
{
    adopt(std::move(other));
    return *this;
}

template<typename T>
inline TDynamicPodArray<T>& TDynamicPodArray<T>::operator=(TPodVector<T>&& other) noexcept
{
    (void)adopt(std::move(other));  //  default: zero tail
    return *this;
}

template<typename T>
inline TPodArrayStorage<T> TDynamicPodArray<T>::disown() noexcept
{
    TPodArrayStorage<T> storage = m_storage;
    m_storage = {};
    return storage;
}

template<typename T>
inline void TDynamicPodArray<T>::adopt(TPodArray<T>&& donor) noexcept
{
    deallocate();
    m_storage = donor.disown();
}

template<typename T>
inline void TDynamicPodArray<T>::adopt(TDynamicPodArray&& donor) noexcept
{
    if (this != &donor)
    {
        deallocate();
        m_storage = donor.m_storage;
        donor.m_storage = {};
    }
}

template<typename T>
inline std::size_t TDynamicPodArray<T>::adopt(TPodVector<T>&& donor) noexcept
{
    std::size_t donor_size = adopt_uninit_tail(std::move(donor));
    private_zero_tail(donor_size);
    return donor_size;
}

template<typename T>
inline std::size_t TDynamicPodArray<T>::adopt_uninit_tail(TPodVector<T>&& donor) noexcept
{
    std::size_t donor_size = 0u;
    if (donor.tripwire_is_safe())
    {
        deallocate();
        const TPodVectorStorage<T> storage = donor.disown();
        if ((storage.data != nullptr) && (storage.capacity != 0u))
        {
            m_storage.data = storage.data;
            m_storage.size = storage.capacity;
            donor_size = storage.size;
        }
    }
    return donor_size;
}

template<typename T>
inline T& TDynamicPodArray<T>::operator[](const std::size_t index) noexcept
{
#if defined(_DEBUG)
    return ((m_storage.data == nullptr) || (index >= m_storage.size)) ? array_oob_ref<T>() : m_storage.data[index];
#else
    return m_storage.data[index];
#endif
}

template<typename T>
inline const T& TDynamicPodArray<T>::operator[](const std::size_t index) const noexcept
{
#if defined(_DEBUG)
    return ((m_storage.data == nullptr) || (index >= m_storage.size)) ? const_array_oob_ref<T>() : m_storage.data[index];
#else
    return m_storage.data[index];
#endif
}

template<typename T>
inline bool TDynamicPodArray<T>::allocate(const std::size_t size, const bool zero) noexcept
{
    return allocation_resize(size, false, zero);
}

template<typename T>
inline bool TDynamicPodArray<T>::reallocate(const std::size_t size, const bool zero_extra) noexcept
{
    return allocation_resize(size, true, zero_extra);
}

template<typename T>
inline void TDynamicPodArray<T>::deallocate() noexcept
{
    if (m_storage.data != nullptr)
    {
        pod_memory::t_deallocate<T>(m_storage.data);
    }
    m_storage = {};
}

template<typename T>
inline bool TDynamicPodArray<T>::allocation_resize(const std::size_t size, const bool copy, const bool clear) noexcept
{
    bool success = false;
    if (tripwire_is_safe() && (size <= k_max_size))
    {
        if (size == m_storage.size)
        {   //  no size change
            success = true;
        }
        else if (size == 0u)
        {   //  deallocate
            deallocate();
            success = true;
        }
        else
        {   //  reallocate to grow or shrink
            T* data = pod_memory::t_allocate<T>(size);
            if (data != nullptr)
            {
                if (m_storage.data != nullptr)
                {
                    if (copy)
                    {
                        std::memcpy(data, m_storage.data, (sizeof(T) * std::min(size, m_storage.size)));
                    }
                    pod_memory::t_deallocate<T>(m_storage.data);
                }
                if (clear && (size > m_storage.size))
                {
                    const std::size_t offset = copy ? m_storage.size : 0;
                    std::memset((data + offset), 0, (sizeof(T) * (size - offset)));
                }
                m_storage.data = data;
                m_storage.size = size;
                success = true;
            }
        }
    }
    return VE_FAIL_SAFE_ASSERT(success);
}

template<typename T>
inline void TDynamicPodArray<T>::private_zero_tail(const std::size_t tail_start) noexcept
{
    if ((m_storage.data != nullptr) && (tail_start < m_storage.size))
    {
        std::memset((m_storage.data + tail_start), 0, (m_storage.size - tail_start) * sizeof(T));
    }
}

template<typename T>
inline bool TDynamicPodArray<T>::tripwire_is_safe() const noexcept
{
    return VE_FAIL_SAFE_ASSERT(is_safe());
}

//==============================================================================
//  TPodVector<T>
//==============================================================================

template<typename T>
inline TPodVector<T>::TPodVector() noexcept
{
    m_storage = {};
}

template<typename T>
inline TPodVector<T>::TPodVector(TPodArray<T>&& other) noexcept
{
    m_storage = {};
    adopt(std::move(other));
}

template<typename T>
inline TPodVector<T>::TPodVector(TDynamicPodArray<T>&& other) noexcept
{
    m_storage = {};
    adopt(std::move(other));
}

template<typename T>
inline TPodVector<T>::TPodVector(TPodVector&& other) noexcept
{
    m_storage = other.m_storage;
    other.m_storage = {};
}

template<typename T>
inline TPodVector<T>::~TPodVector() noexcept
{
    deallocate();
}

template<typename T>
inline TPodVector<T>& TPodVector<T>::operator=(TPodVector&& other) noexcept
{
    if (this != &other)
    {
        deallocate();
        m_storage = other.m_storage;
        other.m_storage = {};
    }
    return *this;
}

template<typename T>
inline TPodVector<T>& TPodVector<T>::operator=(TPodArray<T>&& other) noexcept
{
    adopt(std::move(other));
    return *this;
}

template<typename T>
inline TPodVector<T>& TPodVector<T>::operator=(TDynamicPodArray<T>&& other) noexcept
{
    adopt(std::move(other));
    return *this;
}

template<typename T>
inline TPodVectorStorage<T> TPodVector<T>::disown() noexcept
{
    TPodVectorStorage<T> storage = m_storage;
    m_storage = {};
    return storage;
}

template<typename T>
inline void TPodVector<T>::adopt(TPodArray<T>&& donor) noexcept
{
    deallocate();
    const TPodArrayStorage<T> storage = donor.disown();
    if ((storage.data != nullptr) && (storage.size != 0u))
    {
        m_storage.data = storage.data;
        m_storage.size = storage.size;
        m_storage.capacity = storage.size;
    }
}

template<typename T>
inline void TPodVector<T>::adopt(TDynamicPodArray<T>&& donor) noexcept
{
    deallocate();
    const TPodArrayStorage<T> storage = donor.disown();
    if ((storage.data != nullptr) && (storage.size != 0u))
    {
        m_storage.data = storage.data;
        m_storage.size = storage.size;
        m_storage.capacity = storage.size;
    }
}

template<typename T>
inline void TPodVector<T>::adopt(TPodVector&& donor) noexcept
{
    if (this != &donor)
    {
        deallocate();
        m_storage = donor.m_storage;
        donor.m_storage = {};
    }
}

template<typename T>
inline T& TPodVector<T>::operator[](const std::size_t index) noexcept
{
#if defined(_DEBUG)
    return ((m_storage.data == nullptr) || (index >= m_storage.size)) ? array_oob_ref<T>() : m_storage.data[index];
#else
    return m_storage.data[index];
#endif
}

template<typename T>
inline const T& TPodVector<T>::operator[](const std::size_t index) const noexcept
{
#if defined(_DEBUG)
    return ((m_storage.data == nullptr) || (index >= m_storage.size)) ? const_array_oob_ref<T>() : m_storage.data[index];
#else
    return m_storage.data[index];
#endif
}

template<typename T>
inline bool TPodVector<T>::insert(const std::size_t index, const T& value) noexcept
{
    if ((index <= m_storage.size) && ensure_free(1u))
    {
        if (index != m_storage.size)
        {
            std::memmove((m_storage.data + (index + 1u)), (m_storage.data + index), (sizeof(T) * (m_storage.size - index)));
        }
        m_storage.data[index] = value;
        ++m_storage.size;
        return true;
    }
    return false;
}

template<typename T>
inline bool TPodVector<T>::insert(const std::size_t index) noexcept
{
    if ((index <= m_storage.size) && ensure_free(1u))
    {
        if (index != m_storage.size)
        {
            std::memmove((m_storage.data + (index + 1u)), (m_storage.data + index), (sizeof(T) * (m_storage.size - index)));
        }
        std::memset((m_storage.data + index), 0, sizeof(T));
        ++m_storage.size;
        return true;
    }
    return false;
}

template<typename T>
inline bool TPodVector<T>::erase(const std::size_t index) noexcept
{
    if (tripwire_is_safe() && (index < m_storage.size))
    {
        --m_storage.size;
        if (index != m_storage.size)
        {
            std::memmove((m_storage.data + index), (m_storage.data + (index + 1u)), (sizeof(T) * (m_storage.size - index)));
        }
        return true;
    }
    return false;
}

template<typename T>
inline bool TPodVector<T>::swap_insert(const std::size_t index, const T& value) noexcept
{
    if ((index <= m_storage.size) && ensure_free(1u))
    {
        if (index != m_storage.size)
        {
            std::memcpy(&m_storage.data[m_storage.size], &m_storage.data[index], sizeof(T));
        }
        m_storage.data[index] = value;
        ++m_storage.size;
        return true;
    }
    return false;
}

template<typename T>
inline bool TPodVector<T>::swap_erase(const std::size_t index) noexcept
{
    if (tripwire_is_safe() && (index < m_storage.size))
    {
        --m_storage.size;
        std::memcpy(&m_storage.data[index], &m_storage.data[m_storage.size], sizeof(T));
        return true;
    }
    return false;
}

template<typename T>
inline bool TPodVector<T>::push_back(const T& value) noexcept
{
    if (ensure_free(1u))
    {
        m_storage.data[m_storage.size] = value;
        ++m_storage.size;
        return true;
    }
    return false;
}

template<typename T>
inline bool TPodVector<T>::push_back() noexcept
{
    if (ensure_free(1u))
    {
        std::memset((m_storage.data + m_storage.size), 0, sizeof(T));
        ++m_storage.size;
        return true;
    }
    return false;
}

template<typename T>
inline bool TPodVector<T>::pop_back(T& value) noexcept
{
    if (tripwire_is_safe() && (m_storage.size != 0u))
    {
        --m_storage.size;
        value = m_storage.data[m_storage.size];
        return true;
    }
    return false;
}

template<typename T>
inline bool TPodVector<T>::pop_back() noexcept
{
    if (tripwire_is_safe() && (m_storage.size != 0u))
    {
        --m_storage.size;
        return true;
    }
    return false;
}

template<typename T>
inline std::size_t TPodVector<T>::try_push_many(TPodArrayView<const T> src) noexcept
{
    std::size_t pushed = 0u;
    if (tripwire_is_safe())
    {
        std::size_t count = std::min(src.size(), (k_max_capacity - m_storage.size));
        if (count != 0u)
        {
            ensure_free(count);
            pushed = std::min(count, (m_storage.capacity - m_storage.size));
            if (pushed != 0u)
            {
                std::memcpy((m_storage.data + m_storage.size), src.data(), (sizeof(T) * pushed));
                m_storage.size += pushed;
            }
        }
    }
    return pushed;
}

template<typename T>
inline std::size_t TPodVector<T>::try_pop_many(TPodArrayView<T> dst) noexcept
{
    std::size_t popped = 0u;
    if (tripwire_is_safe())
    {
        popped = std::min(dst.size(), m_storage.size);
        std::size_t size = m_storage.size;
        for (std::size_t index = 0u; index < popped; ++index)
        {
            --size;
            dst[index] = m_storage.data[size];
        }
        m_storage.size = size;
    }
    return popped;
}

template<typename T>
inline bool TPodVector<T>::resize(const std::size_t size) noexcept
{
    bool success = false;
    if (tripwire_is_safe() && (size <= k_max_capacity))
    {
        if (size > m_storage.capacity)
        {   //  reallocate to grow
            success = private_reallocate(size);
            if (success)
            {
                m_storage.size = size;
            }
        }
        else
        {   //  no reallocation required
            if (size > m_storage.size)
            {
                std::memset((m_storage.data + m_storage.size), 0, (sizeof(T) * (size - m_storage.size)));
            }
            m_storage.size = size;
            success = true;
        }
    }
    return success;
}

template<typename T>
inline bool TPodVector<T>::reserve(const std::size_t minimum_capacity) noexcept
{
    if (tripwire_is_safe() && (minimum_capacity <= k_max_capacity))
    {
        return (minimum_capacity < m_storage.capacity) ? true : private_reallocate(pod_memory::vector_growth_policy(minimum_capacity));
    }
    return false;
}

template<typename T>
inline bool TPodVector<T>::reserve_exact(const std::size_t exact_capacity) noexcept
{
    if (tripwire_is_safe() && (exact_capacity <= k_max_capacity))
    {
        return (exact_capacity < m_storage.capacity) ? true : private_reallocate(exact_capacity);
    }
    return false;
}

template<typename T>
inline bool TPodVector<T>::ensure_free(const std::size_t extra) noexcept
{
    if (tripwire_is_safe() && (extra <= (k_max_capacity - m_storage.size)))
    {
        const std::size_t required = m_storage.size + extra;
        return (required < m_storage.capacity) ? true : private_reallocate(pod_memory::vector_growth_policy(required));
    }
    return false;
}

template<typename T>
inline bool TPodVector<T>::shrink_to_fit() noexcept
{
    bool success = false;
    if (tripwire_is_safe())
    {
        if (m_storage.size < m_storage.capacity)
        {
            if (m_storage.size == 0u)
            {
                deallocate();
                success = true;
            }
            else
            {
                success = private_reallocate(m_storage.size);
            }
        }
    }
    return success;
}

template<typename T>
inline void TPodVector<T>::deallocate() noexcept
{
    if (m_storage.data != nullptr)
    {
        pod_memory::t_deallocate<T>(m_storage.data);
    }
    m_storage = {};
}

template<typename T>
inline bool TPodVector<T>::private_reallocate(const std::size_t capacity) noexcept
{
    T* data = pod_memory::t_allocate<T>(capacity);
    if (data != nullptr)
    {
        m_storage.capacity = capacity;
        m_storage.size = std::min(m_storage.size, m_storage.capacity);
        if (m_storage.data != nullptr)
        {
            std::memcpy(data, m_storage.data, (sizeof(T) * m_storage.size));
            pod_memory::t_deallocate<T>(m_storage.data);
        }
        m_storage.data = data;
        return true;
    }
    return false;
}

template<typename T>
inline void TPodVector<T>::private_zero_tail(const std::size_t tail_start) noexcept
{
    if ((m_storage.data != nullptr) && (tail_start < m_storage.capacity))
    {
        std::memset((m_storage.data + tail_start), 0, (m_storage.capacity - tail_start) * sizeof(T));
    }
}

template<typename T>
inline bool TPodVector<T>::tripwire_is_safe() const noexcept
{
    return VE_FAIL_SAFE_ASSERT(is_safe());
}

#endif  //  #ifndef TPOD_ARRAYS_HPP_INCLUDED
