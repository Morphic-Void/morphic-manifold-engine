//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   data_model_types.hpp
//  Fundamental POD types for the mutable structured-data model.

#pragma once

#ifndef DATA_MODEL_TYPES_HPP_INCLUDED
#define DATA_MODEL_TYPES_HPP_INCLUDED

#include <cstdint>
#include <type_traits>

class CLiveDocument;

class CNodeKey
{
public:
    constexpr CNodeKey() noexcept = default;
    [[nodiscard]] constexpr bool is_valid() const noexcept { return m_value != 0u; }
    [[nodiscard]] explicit constexpr operator bool() const noexcept { return is_valid(); }
    [[nodiscard]] constexpr std::uint64_t query_value() const noexcept { return m_value; }
    [[nodiscard]] constexpr std::int32_t relationship(const CNodeKey& other) const noexcept
    {
        return (m_value < other.m_value) ? -1 : ((m_value > other.m_value) ? 1 : 0);
    }

private:
    explicit constexpr CNodeKey(const std::uint64_t value) noexcept : m_value(value) {}
    std::uint64_t m_value{ 0u };
    friend class CLiveDocument;
};

[[nodiscard]] constexpr bool operator==(const CNodeKey lhs, const CNodeKey rhs) noexcept { return lhs.query_value() == rhs.query_value(); }
[[nodiscard]] constexpr bool operator!=(const CNodeKey lhs, const CNodeKey rhs) noexcept { return !(lhs == rhs); }

class CPropertyNameId
{
public:
    constexpr CPropertyNameId() noexcept = default;
    [[nodiscard]] constexpr bool is_valid() const noexcept { return m_value != 0u; }
    [[nodiscard]] explicit constexpr operator bool() const noexcept { return is_valid(); }
    [[nodiscard]] constexpr std::uint32_t query_value() const noexcept { return m_value; }

private:
    explicit constexpr CPropertyNameId(const std::uint32_t value) noexcept : m_value(value) {}
    std::uint32_t m_value{ 0u };
    friend class CLiveDocument;
};

class CStringValueId
{
public:
    constexpr CStringValueId() noexcept = default;
    [[nodiscard]] constexpr bool is_valid() const noexcept { return m_value != 0u; }
    [[nodiscard]] explicit constexpr operator bool() const noexcept { return is_valid(); }
    [[nodiscard]] constexpr std::uint32_t query_value() const noexcept { return m_value; }

private:
    explicit constexpr CStringValueId(const std::uint32_t value) noexcept : m_value(value) {}
    std::uint32_t m_value{ 0u };
    friend class CLiveDocument;
};

[[nodiscard]] constexpr bool operator==(const CPropertyNameId lhs, const CPropertyNameId rhs) noexcept { return lhs.query_value() == rhs.query_value(); }
[[nodiscard]] constexpr bool operator!=(const CPropertyNameId lhs, const CPropertyNameId rhs) noexcept { return !(lhs == rhs); }
[[nodiscard]] constexpr bool operator==(const CStringValueId lhs, const CStringValueId rhs) noexcept { return lhs.query_value() == rhs.query_value(); }
[[nodiscard]] constexpr bool operator!=(const CStringValueId lhs, const CStringValueId rhs) noexcept { return !(lhs == rhs); }

enum class EJsonNodeType : std::uint8_t
{
    invalid = 0,
    null_value,
    boolean,
    integer,
    floating_point,
    string,
    array,
    object,
};

struct CChildList
{
    CNodeKey first;
    CNodeKey last;
    std::uint32_t count;
    std::uint32_t revision;
};

union CJsonPayload
{
    constexpr CJsonPayload() noexcept : unsigned_bits(0u) {}

    std::uint64_t unsigned_bits;
    std::int64_t integer_value;
    double floating_value;
    CStringValueId string_value;
    CChildList children;
};

//  The self key permits a registry scan without exposing slot indices.
struct CJsonSlot
{
    CNodeKey self;
    CNodeKey parent;
    CNodeKey previous_sibling;
    CNodeKey next_sibling;
    CPropertyNameId name_in_parent;
    EJsonNodeType type;
    std::uint8_t flags;
    std::uint16_t reserved;
    CJsonPayload payload;
};

struct CArrayCursor
{
    CNodeKey parent;
    CNodeKey current;
    std::uint32_t index{ 0u };
    std::uint32_t revision{ 0u };
};

static_assert(std::is_trivially_copyable_v<CNodeKey>);
static_assert(std::is_standard_layout_v<CNodeKey>);
static_assert(sizeof(CNodeKey) == sizeof(std::uint64_t));
static_assert(std::is_trivially_copyable_v<CJsonPayload>);
static_assert(std::is_trivially_copyable_v<CJsonSlot>);
static_assert(std::is_standard_layout_v<CJsonSlot>);
static_assert(sizeof(CJsonSlot) == 64u);
static_assert(alignof(CJsonSlot) >= alignof(std::uint64_t));

#endif  //  DATA_MODEL_TYPES_HPP_INCLUDED
