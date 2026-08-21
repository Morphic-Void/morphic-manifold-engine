
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   baked_document.hpp
//  Author: Ritchie Brannan
//  Date:   21 August 2026
//
//  One-allocation immutable baked document artifact and checked view.

#pragma once

#ifndef BAKED_DOCUMENT_HPP_INCLUDED
#define BAKED_DOCUMENT_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include "containers/ByteBuffers.hpp"
#include "data_model/baked_document_builder.hpp"

struct CBakedStringRef
{
    std::uint32_t offset;
    std::uint32_t length;
};

// Exactly sixteen uint32 fields: stable 64-byte in-memory wire header.
struct CBakedDocumentHeader
{
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t header_size;
    std::uint32_t flags;
    std::uint32_t total_size;
    std::uint32_t root_index;
    std::uint32_t nodes_offset;
    std::uint32_t node_count;
    std::uint32_t name_refs_offset;
    std::uint32_t name_ref_count;
    std::uint32_t name_bytes_offset;
    std::uint32_t name_bytes_size;
    std::uint32_t value_refs_offset;
    std::uint32_t value_ref_count;
    std::uint32_t value_bytes_offset;
    std::uint32_t value_bytes_size;
    std::uint32_t payload_crc;
};

static_assert(sizeof(CBakedDocumentHeader) == 64u);
static_assert(sizeof(CBakedStringRef) == 8u);

class CBakedDocument
{
public:
    static constexpr std::uint32_t k_magic = 0x4B44424Du; // "MBDK"
    static constexpr std::uint16_t k_version = 1u;
    static constexpr std::uint32_t k_flag_recovered_duplicate_arrays = 1u;

    CBakedDocument() noexcept = default;
    CBakedDocument(const void* const bytes, const std::size_t size) noexcept;
    [[nodiscard]] bool reset(const void* const bytes, const std::size_t size) noexcept;
    [[nodiscard]] bool is_ready() const noexcept;
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool is_canonical() const noexcept;
    [[nodiscard]] bool contains_recovered_duplicate_arrays() const noexcept;
    [[nodiscard]] CBakedNodeIndex root() const noexcept;
    [[nodiscard]] std::uint32_t node_count() const noexcept;
    [[nodiscard]] std::uint32_t property_name_count() const noexcept;
    [[nodiscard]] std::uint32_t string_value_count() const noexcept;
    [[nodiscard]] EJsonNodeType node_type(const CBakedNodeIndex node) const noexcept;
    [[nodiscard]] bool boolean_value(const CBakedNodeIndex node, bool& value) const noexcept;
    [[nodiscard]] bool integer_value(const CBakedNodeIndex node, std::int64_t& value) const noexcept;
    [[nodiscard]] bool floating_point_value(const CBakedNodeIndex node, double& value) const noexcept;
    [[nodiscard]] CStringView string_value(const CBakedNodeIndex node) const noexcept;
    [[nodiscard]] CStringView string_value(const CStringValueId value) const noexcept;
    [[nodiscard]] CStringView property_name(const CPropertyNameId name) const noexcept;
    [[nodiscard]] CBakedNodeIndex parent(const CBakedNodeIndex node) const noexcept;
    [[nodiscard]] CPropertyNameId name_in_parent(const CBakedNodeIndex node) const noexcept;
    [[nodiscard]] CBakedNodeIndex previous_sibling(const CBakedNodeIndex node) const noexcept;
    [[nodiscard]] CBakedNodeIndex next_sibling(const CBakedNodeIndex node) const noexcept;
    [[nodiscard]] std::uint32_t child_count(const CBakedNodeIndex container) const noexcept;
    [[nodiscard]] CBakedNodeIndex first_child(const CBakedNodeIndex container) const noexcept;
    [[nodiscard]] CBakedNodeIndex last_child(const CBakedNodeIndex container) const noexcept;
    [[nodiscard]] CBakedNodeIndex array_at(const CBakedNodeIndex array, const std::uint32_t index) const noexcept;
    [[nodiscard]] CBakedNodeIndex object_child(const CBakedNodeIndex object, const CPropertyNameId name) const noexcept;
    [[nodiscard]] CBakedNodeIndex object_child(const CBakedNodeIndex object, const CStringView& name) const noexcept;
    [[nodiscard]] bool check_integrity() const noexcept;

private:
    [[nodiscard]] static bool is_array(const EJsonNodeType type) noexcept;
    [[nodiscard]] static bool is_container(const EJsonNodeType type) noexcept;
    [[nodiscard]] static std::uint32_t crc(const std::uint8_t* bytes, std::size_t byte_count) noexcept;
    [[nodiscard]] static bool range(const std::uint32_t offset, const std::uint64_t byte_count, const std::uint32_t total_size) noexcept;
    [[nodiscard]] static bool validate(const std::uint8_t* const bytes, const std::size_t byte_count) noexcept;
    [[nodiscard]] const CBakedNode* node_slot(const CBakedNodeIndex node) const noexcept;
    [[nodiscard]] CStringView string_from(
        const std::uint32_t references_offset,
        const std::uint32_t reference_count,
        const std::uint32_t bytes_offset,
        const std::uint32_t bytes_size,
        const std::uint32_t string_id) const noexcept;

    const std::uint8_t* m_bytes{ nullptr };
    std::size_t m_size{ 0u };
    const CBakedDocumentHeader* m_header{ nullptr };
    friend class CBakedDocumentBlock;
};

class CBakedDocumentBlock
{
public:
    CBakedDocumentBlock() noexcept = default;
    CBakedDocumentBlock(CBakedDocumentBlock&&) noexcept = default;
    CBakedDocumentBlock& operator=(CBakedDocumentBlock&&) noexcept = default;
    CBakedDocumentBlock(const CBakedDocumentBlock&) = delete;
    CBakedDocumentBlock& operator=(const CBakedDocumentBlock&) = delete;
    [[nodiscard]] bool build_from(const CBakedDocumentBuilder& source) noexcept;
    void deallocate() noexcept;
    [[nodiscard]] bool is_ready() const noexcept;
    [[nodiscard]] const CBakedDocument& document() const noexcept;
    [[nodiscard]] const CByteBuffer& bytes() const noexcept;

private:
    [[nodiscard]] static std::uint64_t align_up(const std::uint64_t value, const std::uint64_t alignment) noexcept;
    static void copy_strings(CByteBuffer& destination, const CStableStrings& source, const std::uint32_t references_offset,
        const std::uint32_t bytes_offset, const std::uint32_t string_count) noexcept;

    CByteBuffer m_bytes;
    CBakedDocument m_document;
};

//==============================================================================
//  CBakedDocument and CBakedDocumentBlock out of class function bodies
//==============================================================================

inline CBakedDocument::CBakedDocument(const void* const bytes, const std::size_t size) noexcept
{
    (void)reset(bytes, size);
}

inline bool CBakedDocument::reset(const void* const bytes, const std::size_t size) noexcept
{
    m_bytes = nullptr;
    m_size = 0u;
    m_header = nullptr;

    const std::uint8_t* const document_bytes = static_cast<const std::uint8_t*>(bytes);
    if (!validate(document_bytes, size)) return false;
    m_bytes = document_bytes;
    m_size = size;
    m_header = reinterpret_cast<const CBakedDocumentHeader*>(m_bytes);
    return true;
}

inline bool CBakedDocument::is_ready() const noexcept { return m_header != nullptr; }
inline bool CBakedDocument::is_valid() const noexcept { return is_ready() && validate(m_bytes, m_size); }
inline bool CBakedDocument::is_canonical() const noexcept { return is_ready() && ((m_header->flags & k_flag_recovered_duplicate_arrays) == 0u); }
inline bool CBakedDocument::contains_recovered_duplicate_arrays() const noexcept { return is_ready() && !is_canonical(); }
inline CBakedNodeIndex CBakedDocument::root() const noexcept { return is_ready() ? CBakedNodeIndex{ m_header->root_index } : CBakedNodeIndex{}; }
inline std::uint32_t CBakedDocument::node_count() const noexcept { return is_ready() ? (m_header->node_count - 1u) : 0u; }
inline std::uint32_t CBakedDocument::property_name_count() const noexcept { return is_ready() ? (m_header->name_ref_count - 1u) : 0u; }
inline std::uint32_t CBakedDocument::string_value_count() const noexcept { return is_ready() ? (m_header->value_ref_count - 1u) : 0u; }

inline EJsonNodeType CBakedDocument::node_type(const CBakedNodeIndex node) const noexcept
{
    const CBakedNode* const node_record = node_slot(node);
    return node_record ? node_record->type : EJsonNodeType::invalid;
}

inline bool CBakedDocument::boolean_value(const CBakedNodeIndex node, bool& value) const noexcept
{
    const CBakedNode* const node_record = node_slot(node);
    if (!node_record || (node_record->type != EJsonNodeType::boolean))
    {
        return false;
    }
    value = node_record->payload.unsigned_bits != 0u;
    return true;
}

inline bool CBakedDocument::integer_value(const CBakedNodeIndex node, std::int64_t& value) const noexcept
{
    const CBakedNode* const node_record = node_slot(node);
    if (!node_record || (node_record->type != EJsonNodeType::integer))
    {
        return false;
    }
    value = node_record->payload.integer_value;
    return true;
}

inline bool CBakedDocument::floating_point_value(const CBakedNodeIndex node, double& value) const noexcept
{
    const CBakedNode* const node_record = node_slot(node);
    if (!node_record || (node_record->type != EJsonNodeType::floating_point))
    {
        return false;
    }
    value = node_record->payload.floating_value;
    return true;
}

inline CStringView CBakedDocument::string_value(const CBakedNodeIndex node) const noexcept
{
    const CBakedNode* const node_record = node_slot(node);
    return (node_record && (node_record->type == EJsonNodeType::string)) ? string_value(node_record->payload.string_value) : CStringView{};
}

inline CStringView CBakedDocument::string_value(const CStringValueId value) const noexcept
{
    if (m_header == nullptr)
    {
        return string_from(0u, 0u, 0u, 0u, value.query_value());
    }
    return string_from(
        m_header->value_refs_offset,
        m_header->value_ref_count,
        m_header->value_bytes_offset,
        m_header->value_bytes_size,
        value.query_value());
}

inline CStringView CBakedDocument::property_name(const CPropertyNameId name) const noexcept
{
    if (m_header == nullptr)
    {
        return string_from(0u, 0u, 0u, 0u, name.query_value());
    }
    return string_from(
        m_header->name_refs_offset,
        m_header->name_ref_count,
        m_header->name_bytes_offset,
        m_header->name_bytes_size,
        name.query_value());
}

inline CBakedNodeIndex CBakedDocument::parent(const CBakedNodeIndex node) const noexcept
{
    const CBakedNode* const node_record = node_slot(node);
    return node_record ? node_record->parent : CBakedNodeIndex{};
}

inline CPropertyNameId CBakedDocument::name_in_parent(const CBakedNodeIndex node) const noexcept
{
    const CBakedNode* const node_record = node_slot(node);
    return node_record ? node_record->name_in_parent : CPropertyNameId{};
}

inline CBakedNodeIndex CBakedDocument::previous_sibling(const CBakedNodeIndex node) const noexcept
{
    const CBakedNode* const node_record = node_slot(node);
    const CBakedNode* const parent_record = node_record ? node_slot(node_record->parent) : nullptr;
    if (!parent_record || (node.query_value() <= parent_record->first_child_index) ||
        (node.query_value() >= (parent_record->first_child_index + parent_record->child_count)))
    {
        return CBakedNodeIndex{};
    }
    return CBakedNodeIndex{ node.query_value() - 1u };
}

inline CBakedNodeIndex CBakedDocument::next_sibling(const CBakedNodeIndex node) const noexcept
{
    const CBakedNode* const node_record = node_slot(node);
    const CBakedNode* const parent_record = node_record ? node_slot(node_record->parent) : nullptr;
    if (!parent_record || (node.query_value() < parent_record->first_child_index) ||
        ((node.query_value() + 1u) >= (parent_record->first_child_index + parent_record->child_count)))
    {
        return CBakedNodeIndex{};
    }
    return CBakedNodeIndex{ node.query_value() + 1u };
}

inline std::uint32_t CBakedDocument::child_count(const CBakedNodeIndex container) const noexcept
{
    const CBakedNode* const node_record = node_slot(container);
    return (node_record && is_container(node_record->type)) ? node_record->child_count : 0u;
}

inline CBakedNodeIndex CBakedDocument::first_child(const CBakedNodeIndex container) const noexcept
{
    const CBakedNode* const node_record = node_slot(container);
    if (!node_record || !is_container(node_record->type) || !node_record->child_count) return CBakedNodeIndex{};
    return CBakedNodeIndex{ node_record->first_child_index };
}

inline CBakedNodeIndex CBakedDocument::last_child(const CBakedNodeIndex container) const noexcept
{
    const CBakedNode* const node_record = node_slot(container);
    if (!node_record || !is_container(node_record->type) || !node_record->child_count) return CBakedNodeIndex{};
    return CBakedNodeIndex{ node_record->first_child_index + node_record->child_count - 1u };
}

inline CBakedNodeIndex CBakedDocument::array_at(const CBakedNodeIndex array, const std::uint32_t index) const noexcept
{
    const CBakedNode* const node_record = node_slot(array);
    if (!node_record || !is_array(node_record->type) || (index >= node_record->child_count)) return CBakedNodeIndex{};
    return CBakedNodeIndex{ node_record->first_child_index + index };
}

inline bool CBakedDocument::is_array(const EJsonNodeType type) noexcept
{
    return (type == EJsonNodeType::array) || (type == EJsonNodeType::recovered_duplicate_array);
}

inline bool CBakedDocument::is_container(const EJsonNodeType type) noexcept { return is_array(type) || (type == EJsonNodeType::object); }

inline CBakedNodeIndex CBakedDocument::object_child(const CBakedNodeIndex object, const CPropertyNameId name) const noexcept
{
    const CBakedNode* const object_record = node_slot(object);
    if (!object_record || (object_record->type != EJsonNodeType::object) || !name.is_valid())
    {
        return CBakedNodeIndex{};
    }
    for (std::uint32_t child_offset = 0u; child_offset < object_record->child_count; ++child_offset)
    {
        const CBakedNodeIndex child{ object_record->first_child_index + child_offset };
        if (name_in_parent(child) == name) return child;
    }
    return CBakedNodeIndex{};
}

inline CBakedNodeIndex CBakedDocument::object_child(const CBakedNodeIndex object, const CStringView& name) const noexcept
{
    const CBakedNode* const object_record = node_slot(object);
    if (!object_record || (object_record->type != EJsonNodeType::object) || (name.string() == nullptr))
    {
        return CBakedNodeIndex{};
    }
    for (std::uint32_t child_offset = 0u; child_offset < object_record->child_count; ++child_offset)
    {
        const CBakedNodeIndex child{ object_record->first_child_index + child_offset };
        const CStringView child_name = property_name(name_in_parent(child));
        if ((child_name.length() == name.length()) && (std::memcmp(child_name.string(), name.string(), child_name.length()) == 0))
        {
            return child;
        }
    }
    return CBakedNodeIndex{};
}

inline bool CBakedDocument::check_integrity() const noexcept { return is_valid(); }

inline std::uint32_t CBakedDocument::crc(const std::uint8_t* bytes, std::size_t byte_count) noexcept
{
    std::uint32_t value = 0xFFFFFFFFu;
    for (; byte_count--; ++bytes)
    {
        value ^= *bytes;
        for (std::uint32_t bit = 0u; bit < 8u; ++bit)
        {
            value = (value >> 1u) ^ ((value & 1u) ? 0xEDB88320u : 0u);
        }
    }
    return ~value;
}

inline bool CBakedDocument::range(const std::uint32_t offset, const std::uint64_t byte_count, const std::uint32_t total_size) noexcept
{
    return (offset <= total_size) && (byte_count <= static_cast<std::uint64_t>(total_size - offset));
}

inline const CBakedNode* CBakedDocument::node_slot(const CBakedNodeIndex node) const noexcept
{
    if (!m_header || !node.is_valid() || (node.query_value() >= m_header->node_count))
    {
        return nullptr;
    }
    return reinterpret_cast<const CBakedNode*>(m_bytes + m_header->nodes_offset) + node.query_value();
}

inline CStringView CBakedDocument::string_from(
    const std::uint32_t references_offset,
    const std::uint32_t reference_count,
    const std::uint32_t bytes_offset,
    const std::uint32_t bytes_size,
    const std::uint32_t string_id) const noexcept
{
    if (!m_header || (string_id == 0u) || (string_id >= reference_count))
    {
        return CStringView{};
    }
    const CBakedStringRef& reference = reinterpret_cast<const CBakedStringRef*>(m_bytes + references_offset)[string_id];
    if ((reference.offset > bytes_size) || (reference.length > (bytes_size - reference.offset)))
    {
        return CStringView{};
    }
    return CStringView{ reinterpret_cast<const char*>(m_bytes + bytes_offset + reference.offset), reference.length };
}

inline bool CBakedDocument::validate(const std::uint8_t* const bytes, const std::size_t byte_count) noexcept
{
    if (!bytes || (byte_count < sizeof(CBakedDocumentHeader)) || (byte_count > std::numeric_limits<std::uint32_t>::max()))
    {
        return false;
    }
    const CBakedDocumentHeader& header = *reinterpret_cast<const CBakedDocumentHeader*>(bytes);
    if ((header.magic != k_magic) || (header.version != k_version) || (header.header_size != sizeof(header)) ||
        (header.total_size != byte_count) || (header.node_count < 2u) || (header.root_index == 0u) ||
        (header.root_index >= header.node_count))
    {
        return false;
    }
    if ((reinterpret_cast<std::uintptr_t>(bytes) % alignof(CBakedNode)) != 0u ||
        (header.nodes_offset % alignof(CBakedNode)) != 0u ||
        (header.name_refs_offset % alignof(CBakedStringRef)) != 0u ||
        (header.value_refs_offset % alignof(CBakedStringRef)) != 0u)
    {
        return false;
    }
    if (!range(header.nodes_offset, std::uint64_t(header.node_count) * sizeof(CBakedNode), header.total_size) ||
        !range(header.name_refs_offset, std::uint64_t(header.name_ref_count) * sizeof(CBakedStringRef), header.total_size) ||
        !range(header.value_refs_offset, std::uint64_t(header.value_ref_count) * sizeof(CBakedStringRef), header.total_size) ||
        !range(header.name_bytes_offset, header.name_bytes_size, header.total_size) ||
        !range(header.value_bytes_offset, header.value_bytes_size, header.total_size) ||
        (header.name_ref_count == 0u) || (header.value_ref_count == 0u))
    {
        return false;
    }
    if (crc(bytes + header.header_size, byte_count - header.header_size) != header.payload_crc)
    {
        return false;
    }
    const CBakedNode* const nodes = reinterpret_cast<const CBakedNode*>(bytes + header.nodes_offset);
    const CBakedStringRef* const property_name_references = reinterpret_cast<const CBakedStringRef*>(bytes + header.name_refs_offset);
    const CBakedStringRef* const string_value_references = reinterpret_cast<const CBakedStringRef*>(bytes + header.value_refs_offset);
    if ((property_name_references[0].offset != 0u) || (property_name_references[0].length != 0u) ||
        (string_value_references[0].offset != 0u) || (string_value_references[0].length != 0u))
    {
        return false;
    }
    for (std::uint32_t reference_index = 1u; reference_index < header.name_ref_count; ++reference_index)
    {
        const CBakedStringRef& reference = property_name_references[reference_index];
        if ((reference.offset > header.name_bytes_size) || (reference.length > (header.name_bytes_size - reference.offset)))
        {
            return false;
        }
    }
    for (std::uint32_t reference_index = 1u; reference_index < header.value_ref_count; ++reference_index)
    {
        const CBakedStringRef& reference = string_value_references[reference_index];
        if ((reference.offset > header.value_bytes_size) || (reference.length > (header.value_bytes_size - reference.offset)))
        {
            return false;
        }
    }
    if (nodes[0].type != EJsonNodeType::invalid)
    {
        return false;
    }
    for (std::uint32_t node_index = 1u; node_index < header.node_count; ++node_index)
    {
        const CBakedNode& node = nodes[node_index];
        if ((node.type <= EJsonNodeType::invalid) || (node.type > EJsonNodeType::recovered_duplicate_array) ||
            (node.parent.query_value() >= header.node_count) || (node.name_in_parent.query_value() >= header.name_ref_count))
        {
            return false;
        }
        if ((node.type == EJsonNodeType::string) && (node.payload.string_value.query_value() >= header.value_ref_count))
        {
            return false;
        }
        if (!is_container(node.type))
        {
            if ((node.child_count != 0u) || (node.first_child_index != 0u))
            {
                return false;
            }
            continue;
        }
        if (node.child_count && ((node.first_child_index == 0u) || (node.first_child_index >= header.node_count) ||
            (node.child_count > (header.node_count - node.first_child_index))))
        {
            return false;
        }
        for (std::uint32_t child_offset = 0u; child_offset < node.child_count; ++child_offset)
        {
            if (nodes[node.first_child_index + child_offset].parent.query_value() != node_index)
            {
                return false;
            }
        }
    }
    return nodes[header.root_index].parent == CBakedNodeIndex{};
}

inline std::uint64_t CBakedDocumentBlock::align_up(const std::uint64_t value, const std::uint64_t alignment) noexcept
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

inline void CBakedDocumentBlock::copy_strings(
    CByteBuffer& destination,
    const CStableStrings& source,
    const std::uint32_t references_offset,
    const std::uint32_t bytes_offset,
    const std::uint32_t string_count) noexcept
{
    CBakedStringRef* const references = reinterpret_cast<CBakedStringRef*>(destination.data() + references_offset);
    std::uint32_t current_bytes_offset = 0u;
    for (std::uint32_t string_index = 1u; string_index < string_count; ++string_index)
    {
        const CStringView string = source.view(string_index);
        references[string_index] = CBakedStringRef{ current_bytes_offset, static_cast<std::uint32_t>(string.length()) };
        std::memcpy(destination.data() + bytes_offset + current_bytes_offset, string.string(), string.length());
        current_bytes_offset += static_cast<std::uint32_t>(string.length());
    }
}

inline bool CBakedDocumentBlock::build_from(const CBakedDocumentBuilder& source) noexcept
{
    if (!source.is_ready() || !source.check_integrity() || (source.m_nodes.size() > std::numeric_limits<std::uint32_t>::max()))
    {
        return false;
    }

    const std::uint32_t baked_node_count = static_cast<std::uint32_t>(source.m_nodes.size());
    const std::uint32_t property_name_reference_count = source.m_property_name_count + 1u;
    const std::uint32_t string_value_reference_count = source.m_string_value_count + 1u;
    if ((property_name_reference_count == 0u) || (string_value_reference_count == 0u))
    {
        return false;
    }

    std::uint64_t property_name_bytes_size = 0u;
    for (std::uint32_t string_index = 1u; string_index < property_name_reference_count; ++string_index)
    {
        const CStringView string = source.m_property_names.view(string_index);
        if ((string.string() == nullptr) || (string.length() > std::numeric_limits<std::uint32_t>::max()) ||
            ((property_name_bytes_size += string.length()) > std::numeric_limits<std::uint32_t>::max()))
        {
            return false;
        }
    }
    std::uint64_t string_value_bytes_size = 0u;
    for (std::uint32_t string_index = 1u; string_index < string_value_reference_count; ++string_index)
    {
        const CStringView string = source.m_string_values.view(string_index);
        if ((string.string() == nullptr) || (string.length() > std::numeric_limits<std::uint32_t>::max()) ||
            ((string_value_bytes_size += string.length()) > std::numeric_limits<std::uint32_t>::max()))
        {
            return false;
        }
    }

    std::uint64_t total_size = sizeof(CBakedDocumentHeader);
    const std::uint32_t nodes_offset = static_cast<std::uint32_t>(align_up(total_size, alignof(CBakedNode)));
    total_size = std::uint64_t(nodes_offset) + (std::uint64_t(baked_node_count) * sizeof(CBakedNode));
    const std::uint32_t property_name_references_offset = static_cast<std::uint32_t>(align_up(total_size, alignof(CBakedStringRef)));
    total_size = std::uint64_t(property_name_references_offset) + (std::uint64_t(property_name_reference_count) * sizeof(CBakedStringRef));
    const std::uint32_t property_name_bytes_offset = static_cast<std::uint32_t>(total_size);
    total_size += property_name_bytes_size;
    const std::uint32_t string_value_references_offset = static_cast<std::uint32_t>(align_up(total_size, alignof(CBakedStringRef)));
    total_size = std::uint64_t(string_value_references_offset) + (std::uint64_t(string_value_reference_count) * sizeof(CBakedStringRef));
    const std::uint32_t string_value_bytes_offset = static_cast<std::uint32_t>(total_size);
    total_size += string_value_bytes_size;
    if (total_size > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    CByteBuffer staged_bytes;
    if (!staged_bytes.resize(static_cast<std::size_t>(total_size), alignof(CBakedNode)))
    {
        return false;
    }
    std::memset(staged_bytes.data(), 0, staged_bytes.size());

    CBakedDocumentHeader* const header = reinterpret_cast<CBakedDocumentHeader*>(staged_bytes.data());
    *header = CBakedDocumentHeader{
        CBakedDocument::k_magic,
        CBakedDocument::k_version,
        static_cast<std::uint16_t>(sizeof(CBakedDocumentHeader)),
        source.m_contains_recovered_duplicate_arrays ? CBakedDocument::k_flag_recovered_duplicate_arrays : 0u,
        static_cast<std::uint32_t>(total_size),
        source.m_root.query_value(),
        nodes_offset,
        baked_node_count,
        property_name_references_offset,
        property_name_reference_count,
        property_name_bytes_offset,
        static_cast<std::uint32_t>(property_name_bytes_size),
        string_value_references_offset,
        string_value_reference_count,
        string_value_bytes_offset,
        static_cast<std::uint32_t>(string_value_bytes_size),
        0u
    };
    std::memcpy(staged_bytes.data() + nodes_offset, source.m_nodes.data(), std::size_t(baked_node_count) * sizeof(CBakedNode));
    copy_strings(staged_bytes, source.m_property_names, property_name_references_offset, property_name_bytes_offset, property_name_reference_count);
    copy_strings(staged_bytes, source.m_string_values, string_value_references_offset, string_value_bytes_offset, string_value_reference_count);
    header->payload_crc = CBakedDocument::crc(staged_bytes.data() + sizeof(*header), staged_bytes.size() - sizeof(*header));

    const CBakedDocument checked_document{ staged_bytes.data(), staged_bytes.size() };
    if (!checked_document.is_ready()) return false;
    m_bytes = std::move(staged_bytes);
    m_document = CBakedDocument{ m_bytes.data(), m_bytes.size() };
    return m_document.is_ready();
}

inline void CBakedDocumentBlock::deallocate() noexcept
{
    m_bytes.deallocate();
    m_document = CBakedDocument{};
}

inline bool CBakedDocumentBlock::is_ready() const noexcept
{
    return m_document.is_ready();
}

inline const CBakedDocument& CBakedDocumentBlock::document() const noexcept
{
    return m_document;
}

inline const CByteBuffer& CBakedDocumentBlock::bytes() const noexcept
{
    return m_bytes;
}

#endif  //  BAKED_DOCUMENT_HPP_INCLUDED
