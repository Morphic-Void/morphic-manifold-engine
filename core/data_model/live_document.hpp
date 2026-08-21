
//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
//
//  File:   live_document.hpp
//  Author: Ritchie Brannan
//  Date:   20 August 2026
//  Mutable, JSON-shaped, single-threaded document construction model.

#pragma once

#ifndef LIVE_DOCUMENT_HPP_INCLUDED
#define LIVE_DOCUMENT_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <limits>

#include "containers/StringBuffers.hpp"
#include "containers/TPodOrderedSlots.hpp"
#include "data_model/data_model_types.hpp"

class CLiveDocument
{
public:
    CLiveDocument() noexcept = default;
    CLiveDocument(const CLiveDocument&) = delete;
    CLiveDocument& operator=(const CLiveDocument&) = delete;
    CLiveDocument(CLiveDocument&&) = delete;
    CLiveDocument& operator=(CLiveDocument&&) = delete;
    ~CLiveDocument() noexcept = default;

    //  A document is accessed by one thread at a time. Reading is permitted
    //  only while mutation is prohibited and the document is quiescent.
    [[nodiscard]] bool initialise(const std::size_t initial_slot_count = 0u) noexcept;
    void deallocate() noexcept;
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool is_ready() const noexcept;
    [[nodiscard]] bool is_empty() const noexcept;

    [[nodiscard]] CNodeKey root() const noexcept;
    [[nodiscard]] bool set_root(const CNodeKey node) noexcept;

    [[nodiscard]] CNodeKey create_null() noexcept;
    [[nodiscard]] CNodeKey create_boolean(const bool value) noexcept;
    [[nodiscard]] CNodeKey create_integer(const std::int64_t value) noexcept;
    [[nodiscard]] CNodeKey create_floating_point(const double value) noexcept;
    [[nodiscard]] CNodeKey create_string(const CStringView& value) noexcept;
    [[nodiscard]] CNodeKey create_array() noexcept;
    [[nodiscard]] CNodeKey create_object() noexcept;

    [[nodiscard]] CPropertyNameId intern_property_name(const CStringView& name) noexcept;
    [[nodiscard]] CStringView property_name(const CPropertyNameId name) const noexcept;
    [[nodiscard]] CStringView string_value(const CStringValueId value) const noexcept;

    [[nodiscard]] bool append_array_child(const CNodeKey array, const CNodeKey child) noexcept;
    [[nodiscard]] bool insert_array_child_before(const CNodeKey array, const CNodeKey before, const CNodeKey child) noexcept;
    [[nodiscard]] bool add_object_child(const CNodeKey object, const CStringView& name, const CNodeKey child) noexcept;
    [[nodiscard]] bool detach(const CNodeKey child) noexcept;
    [[nodiscard]] bool erase_detached(const CNodeKey node) noexcept;

    [[nodiscard]] EJsonNodeType node_type(const CNodeKey node) const noexcept;
    [[nodiscard]] bool boolean_value(const CNodeKey node, bool& value) const noexcept;
    [[nodiscard]] bool integer_value(const CNodeKey node, std::int64_t& value) const noexcept;
    [[nodiscard]] bool floating_point_value(const CNodeKey node, double& value) const noexcept;
    [[nodiscard]] CStringView string_value(const CNodeKey node) const noexcept;
    [[nodiscard]] CNodeKey parent(const CNodeKey node) const noexcept;
    [[nodiscard]] CPropertyNameId name_in_parent(const CNodeKey node) const noexcept;
    [[nodiscard]] CNodeKey previous_sibling(const CNodeKey node) const noexcept;
    [[nodiscard]] CNodeKey next_sibling(const CNodeKey node) const noexcept;
    [[nodiscard]] std::uint32_t child_count(const CNodeKey container) const noexcept;
    [[nodiscard]] CNodeKey first_child(const CNodeKey container) const noexcept;
    [[nodiscard]] CNodeKey last_child(const CNodeKey container) const noexcept;
    [[nodiscard]] CNodeKey object_child(const CNodeKey object, const CPropertyNameId name) const noexcept;
    [[nodiscard]] CNodeKey object_child(const CNodeKey object, const CStringView& name) const noexcept;
    [[nodiscard]] CNodeKey array_at(const CNodeKey array, const std::uint32_t index) const noexcept;
    [[nodiscard]] bool array_cursor_at(const CNodeKey array, const std::uint32_t index, CArrayCursor& cursor) const noexcept;
    [[nodiscard]] bool array_cursor_next(CArrayCursor& cursor) const noexcept;
    [[nodiscard]] bool check_integrity() const noexcept;

private:
    [[nodiscard]] static bool is_array_type(const EJsonNodeType type) noexcept;
    [[nodiscard]] static bool is_container_type(const EJsonNodeType type) noexcept;
    [[nodiscard]] static bool check_stable_strings(const CStableStrings& strings) noexcept;
    [[nodiscard]] CNodeKey create_node(const EJsonNodeType type) noexcept;
    [[nodiscard]] CJsonSlot* node_slot(const CNodeKey node) noexcept;
    [[nodiscard]] const CJsonSlot* node_slot(const CNodeKey node) const noexcept;
    [[nodiscard]] bool can_attach(const CNodeKey parent, const CNodeKey child) const noexcept;
    [[nodiscard]] bool attach_before(
        const CNodeKey parent,
        const CNodeKey before,
        const CNodeKey child,
        const CPropertyNameId name) noexcept;
    [[nodiscard]] bool object_has_name(const CNodeKey object, const CPropertyNameId name) const noexcept;
    [[nodiscard]] bool check_container_integrity(const CJsonSlot& container) const noexcept;

    TPodOrderedSlots<CJsonSlot, CNodeKey> m_nodes;
    CStableStrings m_property_names;
    CStableStrings m_string_values;
    CNodeKey m_root;
    std::uint64_t m_next_node_key{ 1u };
};

//==============================================================================
//  CLiveDocument out of class function bodies
//==============================================================================

inline bool CLiveDocument::is_empty() const noexcept
{
    return m_nodes.is_empty();
}

inline CNodeKey CLiveDocument::root() const noexcept
{
    return m_root;
}

inline bool CLiveDocument::is_array_type(const EJsonNodeType type) noexcept
{
    return (type == EJsonNodeType::array) || (type == EJsonNodeType::recovered_duplicate_array);
}

inline bool CLiveDocument::is_container_type(const EJsonNodeType type) noexcept
{
    return is_array_type(type) || (type == EJsonNodeType::object);
}

inline bool CLiveDocument::check_stable_strings(const CStableStrings& strings) noexcept
{
    return (strings.memory_allocation_count() == 0u) || strings.check_integrity();
}

inline CJsonSlot* CLiveDocument::node_slot(const CNodeKey node) noexcept
{
    return m_nodes.get_slot(node);
}

inline const CJsonSlot* CLiveDocument::node_slot(const CNodeKey node) const noexcept
{
    return m_nodes.get_slot(node);
}

inline bool CLiveDocument::initialise(const std::size_t initial_slot_count) noexcept
{
    deallocate();
    return m_nodes.initialise(initial_slot_count);
}

inline void CLiveDocument::deallocate() noexcept
{
    m_nodes.deallocate();
    m_property_names.deallocate();
    m_string_values.deallocate();
    m_root = CNodeKey{};
}

inline bool CLiveDocument::is_valid() const noexcept
{
    return m_nodes.is_valid() && check_stable_strings(m_property_names) && check_stable_strings(m_string_values);
}

inline bool CLiveDocument::is_ready() const noexcept { return m_nodes.is_ready(); }

inline bool CLiveDocument::set_root(const CNodeKey node) noexcept
{
    CJsonSlot* const slot = node_slot(node);
    if ((slot == nullptr) || slot->parent.is_valid() || slot->previous_sibling.is_valid() || slot->next_sibling.is_valid()) return false;
    m_root = node;
    return true;
}

inline CNodeKey CLiveDocument::create_node(const EJsonNodeType type) noexcept
{
    if (!is_ready() || (type == EJsonNodeType::invalid) || (m_next_node_key == 0u)) return CNodeKey{};
    const CNodeKey key{ m_next_node_key++ };
    CJsonSlot slot{};
    slot.self = key;
    slot.type = type;
    if (is_container_type(type)) slot.payload.children = CChildList{};
    return (m_nodes.insert(key, slot) >= 0) ? key : CNodeKey{};
}

inline CNodeKey CLiveDocument::create_null() noexcept { return create_node(EJsonNodeType::null_value); }
inline CNodeKey CLiveDocument::create_array() noexcept { return create_node(EJsonNodeType::array); }
inline CNodeKey CLiveDocument::create_object() noexcept { return create_node(EJsonNodeType::object); }

inline CNodeKey CLiveDocument::create_boolean(const bool value) noexcept
{
    const CNodeKey key = create_node(EJsonNodeType::boolean);
    if (CJsonSlot* const slot = node_slot(key)) slot->payload.unsigned_bits = value ? 1u : 0u;
    return key;
}

inline CNodeKey CLiveDocument::create_integer(const std::int64_t value) noexcept
{
    const CNodeKey key = create_node(EJsonNodeType::integer);
    if (CJsonSlot* const slot = node_slot(key)) slot->payload.integer_value = value;
    return key;
}

inline CNodeKey CLiveDocument::create_floating_point(const double value) noexcept
{
    const CNodeKey key = create_node(EJsonNodeType::floating_point);
    if (CJsonSlot* const slot = node_slot(key)) slot->payload.floating_value = value;
    return key;
}

inline CNodeKey CLiveDocument::create_string(const CStringView& value) noexcept
{
    if (value.string() == nullptr) return CNodeKey{};
    const std::size_t id = m_string_values.append(value.string(), value.length());
    if ((id == CStableStrings::k_invalid_id) || (id > std::numeric_limits<std::uint32_t>::max())) return CNodeKey{};
    const CNodeKey key = create_node(EJsonNodeType::string);
    if (CJsonSlot* const slot = node_slot(key)) slot->payload.string_value = CStringValueId{ static_cast<std::uint32_t>(id) };
    return key;
}

inline CPropertyNameId CLiveDocument::intern_property_name(const CStringView& name) noexcept
{
    if (name.string() == nullptr) return CPropertyNameId{};
    const std::size_t id = m_property_names.append(name.string(), name.length());
    if ((id == CStableStrings::k_invalid_id) || (id > std::numeric_limits<std::uint32_t>::max())) return CPropertyNameId{};
    return CPropertyNameId{ static_cast<std::uint32_t>(id) };
}

inline CStringView CLiveDocument::property_name(const CPropertyNameId name) const noexcept
{
    return name.is_valid() ? m_property_names.view(name.query_value()) : CStringView{};
}

inline CStringView CLiveDocument::string_value(const CStringValueId value) const noexcept
{
    return value.is_valid() ? m_string_values.view(value.query_value()) : CStringView{};
}

inline bool CLiveDocument::can_attach(const CNodeKey parent_key, const CNodeKey child_key) const noexcept
{
    const CJsonSlot* const parent_slot = node_slot(parent_key);
    const CJsonSlot* const child_slot = node_slot(child_key);
    return (parent_slot != nullptr) && is_container_type(parent_slot->type) && (child_slot != nullptr) && (child_key != m_root) &&
        !child_slot->parent.is_valid() && !child_slot->previous_sibling.is_valid() && !child_slot->next_sibling.is_valid() &&
        (parent_slot->payload.children.count != std::numeric_limits<std::uint32_t>::max());
}

inline bool CLiveDocument::attach_before(
    const CNodeKey parent_key,
    const CNodeKey before_key,
    const CNodeKey child_key,
    const CPropertyNameId name) noexcept
{
    if (!can_attach(parent_key, child_key)) return false;
    CJsonSlot* const parent_slot = node_slot(parent_key);
    CJsonSlot* const child_slot = node_slot(child_key);
    CJsonSlot* before_slot = nullptr;
    if (before_key.is_valid())
    {
        before_slot = node_slot(before_key);
        if ((before_slot == nullptr) || (before_slot->parent != parent_key)) return false;
    }
    CChildList& list = parent_slot->payload.children;
    const CNodeKey previous_key = (before_slot != nullptr) ? before_slot->previous_sibling : list.last;
    CJsonSlot* const previous_slot = node_slot(previous_key);
    if (previous_key.is_valid() && ((previous_slot == nullptr) || (previous_slot->parent != parent_key))) return false;

    child_slot->parent = parent_key;
    child_slot->previous_sibling = previous_key;
    child_slot->next_sibling = before_key;
    child_slot->name_in_parent = name;
    if (previous_slot != nullptr) previous_slot->next_sibling = child_key; else list.first = child_key;
    if (before_slot != nullptr) before_slot->previous_sibling = child_key; else list.last = child_key;
    ++list.count;
    ++list.revision;
    return true;
}

inline bool CLiveDocument::append_array_child(const CNodeKey array, const CNodeKey child) noexcept
{
    const CJsonSlot* const slot = node_slot(array);
    return (slot != nullptr) && is_array_type(slot->type) && attach_before(array, CNodeKey{}, child, CPropertyNameId{});
}

inline bool CLiveDocument::insert_array_child_before(const CNodeKey array, const CNodeKey before, const CNodeKey child) noexcept
{
    const CJsonSlot* const slot = node_slot(array);
    return (slot != nullptr) && is_array_type(slot->type) && before.is_valid() && attach_before(array, before, child, CPropertyNameId{});
}

inline bool CLiveDocument::object_has_name(const CNodeKey object, const CPropertyNameId name) const noexcept
{
    for (CNodeKey child = first_child(object); child.is_valid(); child = next_sibling(child))
    {
        const CJsonSlot* const child_slot = node_slot(child);
        if ((child_slot != nullptr) && (child_slot->name_in_parent == name)) return true;
    }
    return false;
}

inline bool CLiveDocument::add_object_child(const CNodeKey object, const CStringView& name, const CNodeKey child) noexcept
{
    const CJsonSlot* const slot = node_slot(object);
    if ((slot == nullptr) || (slot->type != EJsonNodeType::object)) return false;
    const CPropertyNameId name_id = intern_property_name(name);
    return name_id.is_valid() && !object_has_name(object, name_id) && attach_before(object, CNodeKey{}, child, name_id);
}

inline bool CLiveDocument::detach(const CNodeKey child_key) noexcept
{
    CJsonSlot* const child_slot = node_slot(child_key);
    if ((child_slot == nullptr) || !child_slot->parent.is_valid() || (child_key == m_root)) return false;
    CJsonSlot* const parent_slot = node_slot(child_slot->parent);
    CJsonSlot* const previous_slot = node_slot(child_slot->previous_sibling);
    CJsonSlot* const next_slot = node_slot(child_slot->next_sibling);
    if ((parent_slot == nullptr) || !is_container_type(parent_slot->type) || (parent_slot->payload.children.count == 0u) ||
        (child_slot->previous_sibling.is_valid() && (previous_slot == nullptr)) ||
        (child_slot->next_sibling.is_valid() && (next_slot == nullptr)))
    {
        return false;
    }
    CChildList& list = parent_slot->payload.children;
    if (previous_slot != nullptr) previous_slot->next_sibling = child_slot->next_sibling; else list.first = child_slot->next_sibling;
    if (next_slot != nullptr) next_slot->previous_sibling = child_slot->previous_sibling; else list.last = child_slot->previous_sibling;
    child_slot->parent = CNodeKey{};
    child_slot->previous_sibling = CNodeKey{};
    child_slot->next_sibling = CNodeKey{};
    child_slot->name_in_parent = CPropertyNameId{};
    --list.count;
    ++list.revision;
    return true;
}

inline bool CLiveDocument::erase_detached(const CNodeKey node) noexcept
{
    const CJsonSlot* const slot = node_slot(node);
    return (slot != nullptr) && (node != m_root) && !slot->parent.is_valid() &&
        !slot->previous_sibling.is_valid() && !slot->next_sibling.is_valid() &&
        (!is_container_type(slot->type) || (slot->payload.children.count == 0u)) &&
        m_nodes.erase(node);
}

inline EJsonNodeType CLiveDocument::node_type(const CNodeKey node) const noexcept
{
    const CJsonSlot* const slot = node_slot(node);
    return (slot != nullptr) ? slot->type : EJsonNodeType::invalid;
}

inline bool CLiveDocument::boolean_value(const CNodeKey node, bool& value) const noexcept
{
    const CJsonSlot* const slot = node_slot(node);
    if ((slot == nullptr) || (slot->type != EJsonNodeType::boolean))
    {
        return false;
    }
    value = slot->payload.unsigned_bits != 0u;
    return true;
}

inline bool CLiveDocument::integer_value(const CNodeKey node, std::int64_t& value) const noexcept
{
    const CJsonSlot* const slot = node_slot(node);
    if ((slot == nullptr) || (slot->type != EJsonNodeType::integer))
    {
        return false;
    }
    value = slot->payload.integer_value;
    return true;
}

inline bool CLiveDocument::floating_point_value(const CNodeKey node, double& value) const noexcept
{
    const CJsonSlot* const slot = node_slot(node);
    if ((slot == nullptr) || (slot->type != EJsonNodeType::floating_point))
    {
        return false;
    }
    value = slot->payload.floating_value;
    return true;
}

inline CStringView CLiveDocument::string_value(const CNodeKey node) const noexcept
{
    const CJsonSlot* const slot = node_slot(node);
    return ((slot != nullptr) && (slot->type == EJsonNodeType::string)) ? string_value(slot->payload.string_value) : CStringView{};
}

inline CNodeKey CLiveDocument::parent(const CNodeKey node) const noexcept
{
    const CJsonSlot* const slot = node_slot(node);
    return (slot != nullptr) ? slot->parent : CNodeKey{};
}

inline CPropertyNameId CLiveDocument::name_in_parent(const CNodeKey node) const noexcept
{
    const CJsonSlot* const slot = node_slot(node);
    return (slot != nullptr) ? slot->name_in_parent : CPropertyNameId{};
}

inline CNodeKey CLiveDocument::previous_sibling(const CNodeKey node) const noexcept
{
    const CJsonSlot* const slot = node_slot(node);
    return (slot != nullptr) ? slot->previous_sibling : CNodeKey{};
}

inline CNodeKey CLiveDocument::next_sibling(const CNodeKey node) const noexcept
{
    const CJsonSlot* const slot = node_slot(node);
    return (slot != nullptr) ? slot->next_sibling : CNodeKey{};
}

inline std::uint32_t CLiveDocument::child_count(const CNodeKey container) const noexcept
{
    const CJsonSlot* const slot = node_slot(container);
    return ((slot != nullptr) && is_container_type(slot->type)) ? slot->payload.children.count : 0u;
}

inline CNodeKey CLiveDocument::first_child(const CNodeKey container) const noexcept
{
    const CJsonSlot* const slot = node_slot(container);
    return ((slot != nullptr) && is_container_type(slot->type)) ? slot->payload.children.first : CNodeKey{};
}

inline CNodeKey CLiveDocument::last_child(const CNodeKey container) const noexcept
{
    const CJsonSlot* const slot = node_slot(container);
    return ((slot != nullptr) && is_container_type(slot->type)) ? slot->payload.children.last : CNodeKey{};
}

inline CNodeKey CLiveDocument::object_child(const CNodeKey object, const CPropertyNameId name) const noexcept
{
    const CJsonSlot* const object_slot = node_slot(object);
    if ((object_slot == nullptr) || (object_slot->type != EJsonNodeType::object) || !name.is_valid()) return CNodeKey{};
    for (CNodeKey child = object_slot->payload.children.first; child.is_valid(); child = next_sibling(child))
    {
        const CJsonSlot* const child_slot = node_slot(child);
        if ((child_slot != nullptr) && (child_slot->name_in_parent == name)) return child;
    }
    return CNodeKey{};
}

inline CNodeKey CLiveDocument::object_child(const CNodeKey object, const CStringView& name) const noexcept
{
    if (name.string() == nullptr) return CNodeKey{};
    for (CNodeKey child = first_child(object); child.is_valid(); child = next_sibling(child))
    {
        const CStringView child_name = property_name(name_in_parent(child));
        if ((child_name.string() != nullptr) && (child_name == name)) return child;
    }
    return CNodeKey{};
}

inline CNodeKey CLiveDocument::array_at(const CNodeKey array, const std::uint32_t index) const noexcept
{
    const CJsonSlot* const array_slot = node_slot(array);
    if ((array_slot == nullptr) || !is_array_type(array_slot->type) || (index >= array_slot->payload.children.count)) return CNodeKey{};
    CNodeKey child = array_slot->payload.children.first;
    for (std::uint32_t position = 0u; position < index; ++position) child = next_sibling(child);
    return child;
}

inline bool CLiveDocument::array_cursor_at(const CNodeKey array, const std::uint32_t index, CArrayCursor& cursor) const noexcept
{
    const CJsonSlot* const array_slot = node_slot(array);
    if ((array_slot == nullptr) || !is_array_type(array_slot->type) || (index >= array_slot->payload.children.count)) return false;
    const CNodeKey child = array_at(array, index);
    if (!child.is_valid()) return false;
    cursor = CArrayCursor{ array, child, index, array_slot->payload.children.revision };
    return true;
}

inline bool CLiveDocument::array_cursor_next(CArrayCursor& cursor) const noexcept
{
    const CJsonSlot* const array_slot = node_slot(cursor.parent);
    if ((array_slot == nullptr) || !is_array_type(array_slot->type) ||
        (array_slot->payload.children.revision != cursor.revision) || !cursor.current.is_valid())
    {
        return false;
    }
    const CNodeKey next = next_sibling(cursor.current);
    if (!next.is_valid()) return false;
    cursor.current = next;
    ++cursor.index;
    return true;
}

inline bool CLiveDocument::check_container_integrity(const CJsonSlot& container) const noexcept
{
    if (!is_container_type(container.type)) return false;
    const CChildList& list = container.payload.children;
    if ((list.count == 0u) != (!list.first.is_valid() && !list.last.is_valid())) return false;
    if ((list.count != 0u) && (!list.first.is_valid() || !list.last.is_valid())) return false;
    CNodeKey previous;
    CNodeKey child = list.first;
    std::uint32_t traversed = 0u;
    while (child.is_valid())
    {
        const CJsonSlot* const child_slot = node_slot(child);
        if ((child_slot == nullptr) || (child_slot->parent != container.self) || (child_slot->previous_sibling != previous)) return false;
        if ((container.type == EJsonNodeType::object) && !child_slot->name_in_parent.is_valid()) return false;
        if (is_array_type(container.type) && child_slot->name_in_parent.is_valid()) return false;
        if (++traversed > list.count) return false;
        if (container.type == EJsonNodeType::object)
        {
            for (CNodeKey earlier = list.first; earlier != child; earlier = next_sibling(earlier))
            {
                const CJsonSlot* const earlier_slot = node_slot(earlier);
                if ((earlier_slot == nullptr) || (earlier_slot->name_in_parent == child_slot->name_in_parent)) return false;
            }
        }
        previous = child;
        child = child_slot->next_sibling;
    }
    return (traversed == list.count) && (previous == list.last) &&
        (!list.first.is_valid() || !previous_sibling(list.first).is_valid()) &&
        (!list.last.is_valid() || !next_sibling(list.last).is_valid());
}

inline bool CLiveDocument::check_integrity() const noexcept
{
    if (!m_nodes.check_integrity() || !check_stable_strings(m_property_names) || !check_stable_strings(m_string_values)) return false;
    if (m_root.is_valid())
    {
        const CJsonSlot* const root_slot = node_slot(m_root);
        if ((root_slot == nullptr) || root_slot->parent.is_valid() || root_slot->previous_sibling.is_valid() ||
            root_slot->next_sibling.is_valid())
        {
            return false;
        }
    }
    for (std::int32_t index = m_nodes.first_live(); index >= 0; index = m_nodes.next_live(index))
    {
        const CJsonSlot* const slot = m_nodes.get_slot(index);
        if ((slot == nullptr) || !slot->self.is_valid() || (node_slot(slot->self) != slot)) return false;
        if ((slot->type == EJsonNodeType::invalid) || (slot->type > EJsonNodeType::recovered_duplicate_array)) return false;
        if (slot->parent.is_valid())
        {
            const CJsonSlot* const parent_slot = node_slot(slot->parent);
            if ((parent_slot == nullptr) || !is_container_type(parent_slot->type)) return false;
        }
        else if (slot->previous_sibling.is_valid() || slot->next_sibling.is_valid() || slot->name_in_parent.is_valid()) return false;
        if (slot->previous_sibling.is_valid())
        {
            const CJsonSlot* const previous = node_slot(slot->previous_sibling);
            if ((previous == nullptr) || (previous->next_sibling != slot->self) || (previous->parent != slot->parent)) return false;
        }
        if (slot->next_sibling.is_valid())
        {
            const CJsonSlot* const next = node_slot(slot->next_sibling);
            if ((next == nullptr) || (next->previous_sibling != slot->self) || (next->parent != slot->parent)) return false;
        }
        if (is_container_type(slot->type) && !check_container_integrity(*slot)) return false;
        if ((slot->type == EJsonNodeType::string) && !m_string_values.is_valid_id(slot->payload.string_value.query_value())) return false;
        if (slot->name_in_parent.is_valid() && !m_property_names.is_valid_id(slot->name_in_parent.query_value())) return false;
    }
    return true;
}

#endif  //  LIVE_DOCUMENT_HPP_INCLUDED
