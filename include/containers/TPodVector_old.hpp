
//  File:   TPodVector.hpp
//  Author: Ritchie Brannan
//  Date:   20 Mar 26
//
//  POD vector and typed view utilities (noexcept containers)
//
//  Requirements:
//  - Requires C++17 or later.
//  - No exceptions.
//  - Template parameter T must be non-const and trivially copyable.
//  - Storage is tightly packed contiguous T elements with no per-element padding.
//  - Sizes, capacities, and indices are expressed in elements.
//
//  Overview:
//  - TPodVector<T> owns a contiguous allocation of tightly packed T elements.
//  - TPodView<T> provides a non-owning mutable typed view.
//  - TPodConstView<T> provides a non-owning immutable typed view.
//  - The typed layer uses CByteBuffer as a byte-storage substrate.
//
//  Scope:
//  - This layer models contiguous tightly packed trivially copyable
//    element storage only.
//  - It does not perform construction or destruction.
//  - It does not support non-trivial relocation semantics.
//  - Higher-level element meaning belongs in wrapper layers above
//    this substrate.
//
//  Memory model:
//  - Ownership and byte reallocation mechanics are provided by CByteBuffer.
//  - TPodVector<T> interprets byte storage as tightly packed T[].
//  - size() and capacity() are reported in elements.
//  - size_in_bytes(), capacity_in_bytes(), and available_in_bytes()
//    expose the underlying byte extents.
//
//  Growth model:
//  - Automatic growth for TPodVector<T> uses
//    memory::vector_growth_policy() in element units.
//  - TPodVector<T> does not inherit CByteBuffer's automatic buffer growth policy.
//  - reserve(minimum_capacity) ensures total element capacity is
//    at least minimum_capacity.
//  - ensure_free(extra) ensures at least extra spare elements beyond
//    the current logical size.
//  - shrink_to_fit() reduces capacity to exactly match the current logical size.
//
//  Alignment model:
//  - Storage uses tightly packed T[] layout with element stride sizeof(T).
//  - Base allocation alignment is derived from T only.
//  - The typed allocation alignment is the maximum of:
//      alignof(T)
//      low_bit_mask(sizeof(T))
//      (both of which have a minimum return value of 1)
//  - The resulting alignment request is then subject to the underlying
//    memory layer's alignment normalisation rules.
//  - The typed allocation alignment is not configured per instance or
//    per operation.
//
//  Observation model:
//  - Accessor functions are fail-safe.
//  - Public status observers are derived from the underlying
//    byte-buffer/view state.
//  - Full typed invariant verification is provided separately by
//    check_invariants() and assert_invariants() where present.
//  - size == 0 reports empty even when capacity != 0.
//  - size == 0, capacity != 0 is still a ready state.
//  - Spare capacity beyond size is not part of the logical element  range.
//
//  Typed storage invariants:
//      {data == nullptr, size == 0, capacity == 0} (canonical empty)
//      {data != nullptr, size <= capacity, capacity != 0}
//      size_in_bytes() and capacity_in_bytes() are exact multiples of sizeof(T)
//      current storage alignment is sufficient for T
//
//  Initialisation model:
//  - resize(size) grows the logical range and zeroes newly exposed elements.
//  - push_back_zeroed(count) and insert_zeroed(index, count) expose zeroed storage.
//  - push_back_uninit(count) and insert_uninit(index, count) expose uninitialised
//    storage for caller fill.
//  - reserve() and ensure_free() may provide additional spare capacity
//    without changing logical size.
//  - Zeroed growth is byte-zeroing only. It is not default construction.
//
//  Copy / representation model:
//  - Element transfer uses byte-wise copy / move operations.
//  - For trivially copyable types, padding bytes may be propagated by copy.
//  - Byte-level equality is not guaranteed for semantically equal values.
//

#pragma once

#ifndef TPOD_VECTOR_HPP_INCLUDED
#define TPOD_VECTOR_HPP_INCLUDED

#include <algorithm>    //  std::max, std::min
#include <cstddef>      //  std::size_t
#include <cstdint>      //  std::uint8_t
#include <cstring>      //  std::memcpy, std::memmove, std::memset
#include <type_traits>  //  std::is_const_v, std::is_trivially_copyable_v
#include <utility>      //  std::move

#include "containers/ByteBuffers.hpp"
#include "memory/memory_allocation.hpp"
#include "debug/debug.hpp"

//==============================================================================
//  Forward declarations
//==============================================================================

template<typename T> class TPodVector;
template<typename T> class TPodView;
template<typename T> class TPodConstView;

//==============================================================================
//  OOB PANIC helpers
//==============================================================================

template<typename T>
T& pod_vector_oob_ref() noexcept
{
    static_assert(!std::is_const_v<T>, "pod_vector_oob_ref<T>() requires non-const T.");
    VE_HARD_ASSERT(false);
    static T last_gasp{};
    std::memset(&last_gasp, 0, sizeof(T));
    return last_gasp;
}

template<typename T>
const T& pod_vector_oob_const_ref() noexcept
{
    static_assert(!std::is_const_v<T>, "pod_vector_oob_const_ref<T>() requires non-const T.");
    VE_HARD_ASSERT(false);
    static T last_gasp{};
    std::memset(&last_gasp, 0, sizeof(T));
    return last_gasp;
}

template<typename T>
T& pod_vector_empty_ref() noexcept
{
    static_assert(!std::is_const_v<T>, "pod_vector_empty_ref<T>() requires non-const T.");
    VE_HARD_ASSERT(false);
    static T last_gasp{};
    std::memset(&last_gasp, 0, sizeof(T));
    return last_gasp;
}


template<typename T>
const T& pod_vector_empty_const_ref() noexcept
{
    static_assert(!std::is_const_v<T>, "pod_vector_empty_const_ref<T>() requires non-const T.");
    VE_HARD_ASSERT(false);
    static T last_gasp{};
    std::memset(&last_gasp, 0, sizeof(T));
    return last_gasp;
}

//==============================================================================
//  TPodVector<T>
//  Owning unique vector-like container over CByteBuffer.
//==============================================================================

template<typename T>
class TPodVector
{
private:
    static_assert(std::is_trivially_copyable_v<T>, "TPodVector<T> requires trivially copyable T.");
    static_assert(!std::is_const_v<T>, "TPodVector<T> requires non-const T.");

public:
    using value_type = T;

    //  Default and deleted lifetime
    TPodVector() noexcept = default;
    TPodVector(const TPodVector&) noexcept = delete;
    TPodVector& operator=(const TPodVector&) noexcept = delete;
    TPodVector(TPodVector&&) noexcept = default;
    TPodVector& operator=(TPodVector&&) noexcept = default;
    ~TPodVector() noexcept { deallocate(); };

    //  Status
    [[nodiscard]] bool is_valid() const noexcept { return m_buffer.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_buffer.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_buffer.is_ready(); }

    //  Views
    [[nodiscard]] TPodView<T> view() const noexcept;
    [[nodiscard]] TPodConstView<T> const_view() const noexcept;

    //  Accessors
    [[nodiscard]] T* data() const noexcept { return reinterpret_cast<T*>(m_buffer.data()); }
    [[nodiscard]] std::size_t align() const noexcept { return m_buffer.align(); }
    [[nodiscard]] std::size_t size() const noexcept { return size_in_bytes() / k_element_size; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_in_bytes() / k_element_size; }
    [[nodiscard]] std::size_t available() const noexcept { return available_in_bytes() / k_element_size; }
    [[nodiscard]] std::size_t size_in_bytes() const noexcept { return m_buffer.size(); }
    [[nodiscard]] std::size_t capacity_in_bytes() const noexcept { return m_buffer.capacity(); }
    [[nodiscard]] std::size_t available_in_bytes() const noexcept { return m_buffer.available(); }

    //  Element accessors
    [[nodiscard]] T& first() noexcept { const std::size_t count = size(); return (count != 0u) ? data()[0] : pod_vector_empty_ref<T>(); }
    [[nodiscard]] T& last() noexcept { const std::size_t count = size(); return (count != 0u) ? data()[count - 1u] : pod_vector_empty_ref<T>(); }
    [[nodiscard]] T& operator[](const std::size_t index) noexcept { const std::size_t count = size(); return (index < count) ? data()[index] : pod_vector_oob_ref<T>(); }
    [[nodiscard]] const T& first() const noexcept { const std::size_t count = size(); return (count != 0u) ? data()[0] : pod_vector_empty_const_ref<T>(); }
    [[nodiscard]] const T& last() const noexcept { const std::size_t count = size(); return (count != 0u) ? data()[count - 1u] : pod_vector_empty_const_ref<T>(); }
    [[nodiscard]] const T& operator[](const std::size_t index) const noexcept { const std::size_t count = size(); return (index < count) ? data()[index] : pod_vector_oob_const_ref<T>(); }

    //  Vector operations
    void clear() noexcept { (void)set_size(0u); }
    [[nodiscard]] bool push_back(const T& item) noexcept { return push_back(&item); }
    [[nodiscard]] bool push_back(const T* const items, const std::size_t count = 1u) noexcept;
    [[nodiscard]] bool push_back(const TPodConstView<T>& src) noexcept { return push_back(src.data(), src.size()); }
    [[nodiscard]] T* push_back_zeroed(const std::size_t count = 1u) noexcept;
    [[nodiscard]] T* push_back_uninit(const std::size_t count = 1u) noexcept;
    [[nodiscard]] std::size_t try_push_back(const T* const items, const std::size_t count = 1u) noexcept;
    [[nodiscard]] std::size_t try_push_back(const TPodConstView<T>& src) noexcept { return try_push_back(src.data(), src.size()); }
    [[nodiscard]] bool pop_back(T& item) noexcept { return pop_back(&item); }
    [[nodiscard]] bool pop_back(T* const items, const std::size_t count = 1u) noexcept;
    [[nodiscard]] bool pop_back(const TPodView<T>& dst) noexcept { return pop_back(dst.data(), dst.size()); }
    [[nodiscard]] bool pop_back_preserve_order(T* const items, const std::size_t count = 1u) noexcept;
    [[nodiscard]] bool pop_back_preserve_order(const TPodView<T>& dst) noexcept { return pop_back_preserve_order(dst.data(), dst.size()); }
    [[nodiscard]] bool pop_back(const std::size_t count = 1u) noexcept;
    [[nodiscard]] std::size_t try_pop_back(T* const items, const std::size_t count = 1u) noexcept;
    [[nodiscard]] std::size_t try_pop_back(const TPodView<T>& dst) noexcept { return try_pop_back(dst.data(), dst.size()); };
    [[nodiscard]] std::size_t try_pop_back(const std::size_t count = 1u) noexcept;
    [[nodiscard]] bool insert(const std::size_t index, const T& item) noexcept { return insert(index, &item); }
    [[nodiscard]] bool insert(const std::size_t index, const T* const items, const std::size_t count = 1u) noexcept;
    [[nodiscard]] T* insert_zeroed(const std::size_t index, const std::size_t count = 1u) noexcept;
    [[nodiscard]] T* insert_uninit(const std::size_t index, const std::size_t count = 1u) noexcept;
    [[nodiscard]] bool erase(const std::size_t index, const std::size_t count = 1u) noexcept;
    [[nodiscard]] bool swap_insert(const std::size_t index, const T& item) noexcept { return swap_insert(index, &item); }
    [[nodiscard]] bool swap_insert(const std::size_t index, const T* const items, const std::size_t count = 1u) noexcept;
    [[nodiscard]] bool swap_erase(const std::size_t index, const std::size_t count = 1u) noexcept;

    //  Logical size adjustment
    [[nodiscard]] bool set_size(const std::size_t size) noexcept { return (size <= k_max_elements) ? m_buffer.set_size(size * k_element_size) : false; }

    //  Allocation and capacity management
    [[nodiscard]] bool allocate(const std::size_t capacity) noexcept;
    [[nodiscard]] bool reallocate(const std::size_t capacity) noexcept { return reallocate(size(), capacity); }
    [[nodiscard]] bool reallocate(const std::size_t size, const std::size_t capacity) noexcept;
    [[nodiscard]] bool resize(const std::size_t size) noexcept;
    [[nodiscard]] bool reserve(const std::size_t minimum_capacity) noexcept;
    [[nodiscard]] bool ensure_free(const std::size_t extra) noexcept;
    [[nodiscard]] bool shrink_to_fit() noexcept { return m_buffer.shrink_to_fit(); }
    void deallocate() noexcept { m_buffer.deallocate(); }

    //  Invariant and validity checking
    [[nodiscard]] bool check_invariants() const noexcept;
    [[nodiscard]] bool assert_invariants() const noexcept;

private:
    static constexpr std::size_t k_max_elements = memory::t_max_elements<T>();
    static constexpr std::size_t k_element_size = sizeof(T);
    static constexpr std::size_t k_max_bytes = k_max_elements * k_element_size;
    static constexpr std::size_t k_align = memory::t_default_align<T>();

    [[nodiscard]] bool is_internal_ptr(const T* const ptr) const noexcept;

    CByteBuffer m_buffer;
};

//==============================================================================
//  TPodView<T>
//  Non-owning view over a contiguous mutable POD range.
//==============================================================================

template<typename T>
class TPodView
{
private:
    static_assert(std::is_trivially_copyable_v<T>, "TPodView<T> requires trivially copyable T.");
    static_assert(!std::is_const_v<T>, "TPodView<T> requires non-const T.");

public:

    //  Default lifetime
    TPodView() noexcept = default;
    TPodView(const TPodView&) noexcept = default;
    TPodView& operator=(const TPodView&) noexcept = default;
    ~TPodView() noexcept = default;

    //  Construction
    TPodView(T* const data, const std::size_t size) noexcept { (void)set(data, size); }
    explicit TPodView(const CByteView& view) noexcept { (void)set(view); }

    //  View state
    TPodView& set(T* const data, const std::size_t size) noexcept;
    TPodView& set(const CByteView& view) noexcept;
    TPodView& reset() noexcept { m_view.reset(); return *this; }

    //  Status
    [[nodiscard]] bool is_valid() const noexcept { return m_view.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_view.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_view.is_ready(); }

    //  Derived views
    [[nodiscard]] TPodConstView<T> const_view() const noexcept { return is_ready() ? TPodConstView<T>{ m_view.const_view() } : TPodConstView<T>{}; }
    [[nodiscard]] TPodView subview(const std::size_t offset, const std::size_t count) const noexcept;
    [[nodiscard]] TPodView head_to(const std::size_t count) const noexcept;
    [[nodiscard]] TPodView tail_from(const std::size_t offset) const noexcept;

    //  Accessors
    [[nodiscard]] T* data() const noexcept { return reinterpret_cast<T*>(m_view.data()); }
    [[nodiscard]] std::size_t align() const noexcept { return m_view.align(); }
    [[nodiscard]] std::size_t size() const noexcept { return size_in_bytes() / k_element_size; }
    [[nodiscard]] std::size_t size_in_bytes() const noexcept { return m_view.size(); }

    //  Element accessors
    T& operator[](const std::size_t index) noexcept { const std::size_t count = size(); return (index < count) ? data()[index] : pod_vector_oob_ref<T>(); }
    const T& operator[](const std::size_t index) const noexcept { const std::size_t count = size(); return (index < count) ? data()[index] : pod_vector_oob_const_ref<T>(); }

    //  Invariant and validity checking
    [[nodiscard]] bool check_invariants() const noexcept;
    [[nodiscard]] bool assert_invariants() const noexcept;

private:
    static constexpr std::size_t k_max_elements = memory::t_max_elements<T>();
    static constexpr std::size_t k_element_size = sizeof(T);
    static constexpr std::size_t k_max_bytes = k_max_elements * k_element_size;
    static constexpr std::size_t k_align = memory::t_default_align<T>();

    CByteView m_view;
};

//==============================================================================
//  TPodConstView<T>
//  Non-owning view over a contiguous immutable POD range.
//==============================================================================

template<typename T>
class TPodConstView
{
private:
    static_assert(std::is_trivially_copyable_v<T>, "TPodConstView<T> requires trivially copyable T.");
    static_assert(!std::is_const_v<T>, "TPodConstView<T> requires non-const T.");

public:

    //  Default lifetime
    TPodConstView() noexcept = default;
    TPodConstView(const TPodConstView&) noexcept = default;
    TPodConstView& operator=(const TPodConstView&) noexcept = default;
    ~TPodConstView() noexcept = default;

    //  Construction
    TPodConstView(const T* const data, const std::size_t size) noexcept { (void)set(data, size); }
    explicit TPodConstView(const CByteConstView& view) noexcept { (void)set(view); }

    //  View state
    TPodConstView& set(const T* const data, const std::size_t size) noexcept;
    TPodConstView& set(const CByteConstView& view) noexcept;
    TPodConstView& reset() noexcept { m_view.reset(); return *this; }

    //  Status
    [[nodiscard]] bool is_valid() const noexcept { return m_view.is_valid(); }
    [[nodiscard]] bool is_empty() const noexcept { return m_view.is_empty(); }
    [[nodiscard]] bool is_ready() const noexcept { return m_view.is_ready(); }

    //  Derived views
    [[nodiscard]] TPodConstView subview(const std::size_t offset, const std::size_t count) const noexcept;
    [[nodiscard]] TPodConstView head_to(const std::size_t count) const noexcept;
    [[nodiscard]] TPodConstView tail_from(const std::size_t offset) const noexcept;

    //  Accessors
    [[nodiscard]] const T* data() const noexcept { return reinterpret_cast<const T*>(m_view.data()); }
    [[nodiscard]] std::size_t align() const noexcept { return m_view.align(); }
    [[nodiscard]] std::size_t size() const noexcept { return size_in_bytes() / k_element_size; }
    [[nodiscard]] std::size_t size_in_bytes() const noexcept { return m_view.size(); }

    //  Element accessors
    const T& operator[](const std::size_t index) const noexcept { const std::size_t count = size(); return (index < count) ? data()[index] : pod_vector_oob_const_ref<T>(); }

    //  Invariant and validity checking
    [[nodiscard]] bool check_invariants() const noexcept;
    [[nodiscard]] bool assert_invariants() const noexcept;

private:
    static constexpr std::size_t k_max_elements = memory::t_max_elements<T>();
    static constexpr std::size_t k_element_size = sizeof(T);
    static constexpr std::size_t k_max_bytes = k_max_elements * k_element_size;
    static constexpr std::size_t k_align = memory::t_default_align<T>();

    CByteConstView m_view;
};

//==============================================================================
//  TPodVector<T> out of class function bodies
//==============================================================================

template<typename T>
[[nodiscard]] inline TPodView<T> TPodVector<T>::view() const noexcept
{
    return is_ready() ? TPodView<T>{ m_buffer.view() } : TPodView<T>{};
}

template<typename T>
[[nodiscard]] inline TPodConstView<T> TPodVector<T>::const_view() const noexcept
{
    return is_ready() ? TPodConstView<T>{ m_buffer.const_view() } : TPodConstView<T>{};
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::push_back(const T* const items, const std::size_t count) noexcept
{
    if ((items != nullptr) && (count != 0u) && !is_internal_ptr(items) && ensure_free(count))
    {
        const std::size_t bytes = count * k_element_size;
        const std::size_t offset = m_buffer.size();
        T* const dst = reinterpret_cast<T*>(m_buffer.data() + offset);
        if (m_buffer.set_size(offset + bytes))
        {
            std::memcpy(dst, items, bytes);
            return true;
        }
        return false;
    }
    return count == 0u;
}

template<typename T>
[[nodiscard]] inline T* TPodVector<T>::push_back_zeroed(const std::size_t count) noexcept
{
    if ((count != 0u) && ensure_free(count))
    {
        const std::size_t bytes = count * k_element_size;
        const std::size_t offset = m_buffer.size();
        T* const dst = reinterpret_cast<T*>(m_buffer.data() + offset);
        if (m_buffer.set_size(offset + bytes))
        {
            std::memset(dst, 0, bytes);
            return dst;
        }
    }
    return nullptr;
}

template<typename T>
[[nodiscard]] inline T* TPodVector<T>::push_back_uninit(const std::size_t count) noexcept
{
    if ((count != 0u) && ensure_free(count))
    {
        const std::size_t bytes = count * k_element_size;
        const std::size_t offset = m_buffer.size();
        T* const dst = reinterpret_cast<T*>(m_buffer.data() + offset);
        if (m_buffer.set_size(offset + bytes))
        {
            return dst;
        }
    }
    return nullptr;
}

template<typename T>
[[nodiscard]] inline std::size_t TPodVector<T>::try_push_back(const T* const items, const std::size_t count) noexcept
{
    const std::size_t push_back_count = std::min(count, available());
    return ((push_back_count != 0u) && push_back(items, push_back_count)) ? push_back_count : std::size_t{ 0 };
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::pop_back(T* const items, const std::size_t count) noexcept
{
    if ((items != nullptr) && ((count - 1u) < size()) && !is_internal_ptr(items))
    {
        const std::size_t bytes = count * k_element_size;
        const std::size_t offset = m_buffer.size() - bytes;
        const T* src = reinterpret_cast<T*>(m_buffer.data() + m_buffer.size());
        T* dst = items;
        for (std::size_t copy_count = count; copy_count > 0u; --copy_count)
        {
            --src;
            std::memcpy(dst, src, k_element_size);
            ++dst;
        }
        return m_buffer.set_size(offset);
    }
    return count == 0u;
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::pop_back_preserve_order(T* const items, const std::size_t count) noexcept
{
    if ((items != nullptr) && ((count - 1u) < size()) && !is_internal_ptr(items))
    {
        const std::size_t bytes = count * k_element_size;
        const std::size_t offset = m_buffer.size() - bytes;
        T* const src = reinterpret_cast<T*>(m_buffer.data() + offset);
        std::memcpy(items, src, bytes);
        return m_buffer.set_size(offset);
    }
    return count == 0u;
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::pop_back(const std::size_t count) noexcept
{
    if ((count != 0u) && (count <= size()))
    {
        const std::size_t bytes = count * k_element_size;
        const std::size_t offset = m_buffer.size() - bytes;
        return m_buffer.set_size(offset);
    }
    return count == 0u;
}

template<typename T>
[[nodiscard]] inline std::size_t TPodVector<T>::try_pop_back(T* const items, const std::size_t count) noexcept
{
    const std::size_t pop_back_count = std::min(count, size());
    return ((pop_back_count != 0u) && pop_back(items, pop_back_count)) ? pop_back_count : std::size_t{ 0 };
}

template<typename T>
[[nodiscard]] inline std::size_t TPodVector<T>::try_pop_back(const std::size_t count) noexcept
{
    const std::size_t pop_back_count = std::min(count, size());
    return ((pop_back_count != 0u) && pop_back(pop_back_count)) ? pop_back_count : std::size_t{ 0 };
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::insert(const std::size_t index, const T* const items, const std::size_t count) noexcept
{
    if ((items != nullptr) && (count != 0u) && !is_internal_ptr(items))
    {
        T* const dst = insert_uninit(index, count);
        if (dst != nullptr)
        {
            std::memcpy(dst, items, (count * k_element_size));
            return true;
        }
        return false;
    }
    return count == 0u;
}

template<typename T>
[[nodiscard]] inline T* TPodVector<T>::insert_zeroed(const std::size_t index, const std::size_t count) noexcept
{
    T* const dst = insert_uninit(index, count);
    if (dst != nullptr)
    {
        std::memset(dst, 0, (count * k_element_size));
    }
    return dst;
}

template<typename T>
[[nodiscard]] inline T* TPodVector<T>::insert_uninit(const std::size_t index, const std::size_t count) noexcept
{
    const std::size_t old_size = size();
    if ((count != 0u) && (index <= old_size) && ensure_free(count))
    {
        const std::size_t new_size = old_size + count;
        if (set_size(new_size))
        {
            T* const src = data() + index;
            const std::size_t end_size = old_size - index;
            if (end_size != 0u)
            {
                T* const dst = src + count;
                std::memmove(dst, src, (end_size * k_element_size));
            }
            return src;
        }
    }
    return nullptr;
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::erase(const std::size_t index, const std::size_t count) noexcept
{
    const std::size_t old_size = size();
    if ((index < old_size) && ((count - 1u) < (old_size - index)))
    {
        const std::size_t new_size = old_size - count;
        const std::size_t end_size = old_size - index;
        if (end_size > count)
        {
            T* const dst = data() + index;
            T* const src = dst + count;
            std::memmove(dst, src, ((end_size - count) * k_element_size));
        }
        return set_size(new_size);
    }
    return count == 0u;
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::swap_insert(const std::size_t index, const T* const items, const std::size_t count) noexcept
{
    const std::size_t old_size = size();
    if ((items != nullptr) && (count != 0u) && (index <= old_size) && !is_internal_ptr(items) && ensure_free(count))
    {
        const std::size_t new_size = old_size + count;
        if (set_size(new_size))
        {
            const std::size_t end_size = old_size - index;
            const std::size_t mem_size = std::min(count, end_size);
            T* const dst = data() + index;
            if (mem_size != 0u)
            {
                std::memcpy((dst + end_size), dst, (mem_size * k_element_size));
            }
            std::memcpy(dst, items, (count * k_element_size));
            return true;
        }
        return false;
    }
    return count == 0u;
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::swap_erase(const std::size_t index, const std::size_t count) noexcept
{
    const std::size_t old_size = size();
    if ((index < old_size) && ((count - 1u) < (old_size - index)))
    {
        const std::size_t new_size = old_size - count;
        const std::size_t end_size = old_size - index;
        if (end_size > count)
        {
            const std::size_t mem_size = std::min(count, (end_size - count));
            T* const dst = data() + index;
            T* const src = dst + end_size - mem_size;
            std::memcpy(dst, src, (mem_size * k_element_size));
        }
        return set_size(new_size);
    }
    return count == 0u;
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::allocate(const std::size_t capacity) noexcept
{
    return (capacity <= k_max_elements) ? m_buffer.allocate((capacity * k_element_size), k_align) : false;
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::reallocate(const std::size_t size, const std::size_t capacity) noexcept
{
    return (std::max(size, capacity) <= k_max_elements) ? m_buffer.reallocate((size * k_element_size), (capacity * k_element_size), k_align) : false;
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::resize(const std::size_t size) noexcept
{
    return (size <= k_max_elements) ? reallocate(size, ((size > capacity()) ? memory::vector_growth_policy(size) : capacity())) : false;
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::reserve(const std::size_t minimum_capacity) noexcept
{
    return (minimum_capacity <= k_max_elements) ? reallocate(size(), std::max(memory::vector_growth_policy(minimum_capacity), capacity())) : false;
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::ensure_free(const std::size_t extra) noexcept
{
    return (extra <= (k_max_elements - size())) ? reserve(size() + extra) : false;
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::is_internal_ptr(const T* const ptr) const noexcept
{
    if ((ptr == nullptr) || !is_ready())
    {
        return false;
    }

    const T* const begin = data();
    const T* const end = begin + capacity();
    return (ptr >= begin) && (ptr < end);
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::check_invariants() const noexcept
{
    if (!m_buffer.is_valid())
    {
        return false;
    }
    if (m_buffer.is_empty())
    {
        return true;
    }
    if (!m_buffer.is_ready())
    {
        return false;
    }
    if (m_buffer.align() < k_align)
    {
        return false;
    }
    const std::size_t buffer_size_in_bytes = m_buffer.size();
    if ((buffer_size_in_bytes > k_max_bytes) || ((buffer_size_in_bytes % k_element_size) != 0u))
    {
        return false;
    }
    const std::size_t buffer_capacity_in_bytes = m_buffer.capacity();
    if ((buffer_capacity_in_bytes > k_max_bytes) || ((buffer_capacity_in_bytes % k_element_size) != 0u))
    {
        return false;
    }
    return true;
}

template<typename T>
[[nodiscard]] inline bool TPodVector<T>::assert_invariants() const noexcept
{
    const bool valid = VE_FAIL_SAFE_ASSERT(check_invariants());
    if (!valid)
    {   //  Re-enter for debugger stepping on failure.
        (void)check_invariants();
    }
    return valid;
}

//==============================================================================
//  TPodView<T> out of class function bodies
//==============================================================================

template<typename T>
inline TPodView<T>& TPodView<T>::set(T* const data, const std::size_t size) noexcept
{
    if ((data != nullptr) && ((size - 1u) < k_max_elements) && ((static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(data)) & (k_align - 1u)) == 0u))
    {
        m_view.set(reinterpret_cast<std::uint8_t*>(data), (size * k_element_size), k_align);
    }
    else
    {
        reset();
    }
    return *this;
}

template<typename T>
inline TPodView<T>& TPodView<T>::set(const CByteView& view) noexcept
{
    if (view.is_valid())
    {
        m_view = view;
        if (!check_invariants())
        {
            reset();
        }
    }
    else
    {
        reset();
    }
    return *this;
}

template<typename T>
[[nodiscard]] inline TPodView<T> TPodView<T>::subview(const std::size_t offset, const std::size_t count) const noexcept
{
    const std::size_t element_count = size();
    if (is_ready() && (offset < element_count) && (count <= (element_count - offset)))
    {
        return TPodView<T>{ m_view.subview((offset * k_element_size), (count * k_element_size)) };
    }
    return TPodView<T>{};
}

template<typename T>
[[nodiscard]] inline TPodView<T> TPodView<T>::head_to(const std::size_t count) const noexcept
{
    if (is_ready() && ((count - 1u) < size()))
    {
        return TPodView<T>{ m_view.head_to(count * k_element_size) };
    }
    return TPodView<T>{};
}

template<typename T>
[[nodiscard]] inline TPodView<T> TPodView<T>::tail_from(const std::size_t offset) const noexcept
{
    const std::size_t element_count = size();
    if (is_ready() && (offset < element_count))
    {
        return TPodView<T>{ m_view.tail_from(offset* k_element_size) };
    }
    return TPodView<T>{};
}

template<typename T>
[[nodiscard]] inline bool TPodView<T>::check_invariants() const noexcept
{
    if (!m_view.is_valid())
    {
        return false;
    }
    if (m_view.is_empty())
    {
        return true;
    }
    if (!m_view.is_ready())
    {
        return false;
    }
    if (m_view.align() < k_align)
    {
        return false;
    }
    const std::size_t view_size_in_bytes = m_view.size();
    if ((view_size_in_bytes > k_max_bytes) || ((view_size_in_bytes % k_element_size) != 0u))
    {
        return false;
    }
    return true;
}

template<typename T>
[[nodiscard]] inline bool TPodView<T>::assert_invariants() const noexcept
{
    const bool valid = VE_FAIL_SAFE_ASSERT(check_invariants());
    if (!valid)
    {   //  Re-enter for debugger stepping on failure.
        (void)check_invariants();
    }
    return valid;
}

//==============================================================================
//  TPodConstView<T> out of class function bodies
//==============================================================================

template<typename T>
inline TPodConstView<T>& TPodConstView<T>::set(const T* const data, const std::size_t size) noexcept
{
    if ((data != nullptr) && ((size - 1u) < k_max_elements) && ((static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(data)) & (k_align - 1u)) == 0u))
    {
        m_view.set(reinterpret_cast<const std::uint8_t*>(data), (size * k_element_size), k_align);
    }
    else
    {
        reset();
    }
    return *this;
}

template<typename T>
inline TPodConstView<T>& TPodConstView<T>::set(const CByteConstView& view) noexcept
{
    if (view.is_valid())
    {
        m_view = view;
        if (!check_invariants())
        {
            reset();
        }
    }
    else
    {
        reset();
    }
    return *this;
}

template<typename T>
[[nodiscard]] inline TPodConstView<T> TPodConstView<T>::subview(const std::size_t offset, const std::size_t count) const noexcept
{
    const std::size_t element_count = size();
    if (is_ready() && (offset < element_count) && (count <= (element_count - offset)))
    {
        return TPodConstView<T>{ m_view.subview((offset* k_element_size), (count* k_element_size)) };
    }
    return TPodConstView<T>{};
}

template<typename T>
[[nodiscard]] inline TPodConstView<T> TPodConstView<T>::head_to(const std::size_t count) const noexcept
{
    if (is_ready() && ((count - 1u) < size()))
    {
        return TPodConstView<T>{ m_view.head_to(count * k_element_size) };
    }
    return TPodConstView<T>{};
}

template<typename T>
[[nodiscard]] inline TPodConstView<T> TPodConstView<T>::tail_from(const std::size_t offset) const noexcept
{
    const std::size_t element_count = size();
    if (is_ready() && (offset < element_count))
    {
        return TPodConstView<T>{ m_view.tail_from(offset * k_element_size) };
    }
    return TPodConstView<T>{};
}

template<typename T>
[[nodiscard]] inline bool TPodConstView<T>::check_invariants() const noexcept
{
    if (!m_view.is_valid())
    {
        return false;
    }
    if (m_view.is_empty())
    {
        return true;
    }
    if (!m_view.is_ready())
    {
        return false;
    }
    if (m_view.align() < k_align)
    {
        return false;
    }
    const std::size_t view_size_in_bytes = m_view.size();
    if ((view_size_in_bytes > k_max_bytes) || ((view_size_in_bytes % k_element_size) != 0u))
    {
        return false;
    }
    return true;
}

template<typename T>
[[nodiscard]] inline bool TPodConstView<T>::assert_invariants() const noexcept
{
    const bool valid = VE_FAIL_SAFE_ASSERT(check_invariants());
    if (!valid)
    {   //  Re-enter for debugger stepping on failure.
        (void)check_invariants();
    }
    return valid;
}

#endif  //  TPOD_VECTOR_HPP_INCLUDED
