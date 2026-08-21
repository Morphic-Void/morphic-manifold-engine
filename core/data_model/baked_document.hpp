//  Copyright (c) 2026 Ritchie Brannan / Morphic Void Limited
//  License: MIT (see LICENSE file in repository root)
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

struct CBakedStringRef { std::uint32_t offset; std::uint32_t length; };

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
    CBakedDocument(const void* bytes, std::size_t size) noexcept { (void)reset(bytes, size); }
    [[nodiscard]] bool reset(const void* bytes, std::size_t size) noexcept
    {
        m_bytes = nullptr; m_size = 0u; m_header = nullptr;
        if (!validate(static_cast<const std::uint8_t*>(bytes), size)) return false;
        m_bytes = static_cast<const std::uint8_t*>(bytes); m_size = size;
        m_header = reinterpret_cast<const CBakedDocumentHeader*>(m_bytes); return true;
    }
    [[nodiscard]] bool is_ready() const noexcept { return m_header != nullptr; }
    [[nodiscard]] bool is_valid() const noexcept { return is_ready() && validate(m_bytes, m_size); }
    [[nodiscard]] bool is_canonical() const noexcept { return is_ready() && ((m_header->flags & k_flag_recovered_duplicate_arrays) == 0u); }
    [[nodiscard]] bool contains_recovered_duplicate_arrays() const noexcept { return is_ready() && !is_canonical(); }
    [[nodiscard]] CBakedNodeIndex root() const noexcept { return is_ready() ? CBakedNodeIndex{m_header->root_index} : CBakedNodeIndex{}; }
    [[nodiscard]] std::uint32_t node_count() const noexcept { return is_ready() ? m_header->node_count - 1u : 0u; }
    [[nodiscard]] std::uint32_t property_name_count() const noexcept { return is_ready() ? m_header->name_ref_count - 1u : 0u; }
    [[nodiscard]] std::uint32_t string_value_count() const noexcept { return is_ready() ? m_header->value_ref_count - 1u : 0u; }
    [[nodiscard]] EJsonNodeType node_type(CBakedNodeIndex n) const noexcept { const CBakedNode* p = node_slot(n); return p ? p->type : EJsonNodeType::invalid; }
    [[nodiscard]] bool boolean_value(CBakedNodeIndex n, bool& v) const noexcept { const CBakedNode* p=node_slot(n); if (!p || p->type != EJsonNodeType::boolean) return false; v=p->payload.unsigned_bits != 0u; return true; }
    [[nodiscard]] bool integer_value(CBakedNodeIndex n, std::int64_t& v) const noexcept { const CBakedNode* p=node_slot(n); if (!p || p->type != EJsonNodeType::integer) return false; v=p->payload.integer_value; return true; }
    [[nodiscard]] bool floating_point_value(CBakedNodeIndex n, double& v) const noexcept { const CBakedNode* p=node_slot(n); if (!p || p->type != EJsonNodeType::floating_point) return false; v=p->payload.floating_value; return true; }
    [[nodiscard]] CStringView string_value(CBakedNodeIndex n) const noexcept { const CBakedNode* p=node_slot(n); return (p && p->type == EJsonNodeType::string) ? string_value(p->payload.string_value) : CStringView{}; }
    [[nodiscard]] CStringView string_value(CStringValueId id) const noexcept { return string_from(m_header ? m_header->value_refs_offset : 0u, m_header ? m_header->value_ref_count : 0u, m_header ? m_header->value_bytes_offset : 0u, m_header ? m_header->value_bytes_size : 0u, id.query_value()); }
    [[nodiscard]] CStringView property_name(CPropertyNameId id) const noexcept { return string_from(m_header ? m_header->name_refs_offset : 0u, m_header ? m_header->name_ref_count : 0u, m_header ? m_header->name_bytes_offset : 0u, m_header ? m_header->name_bytes_size : 0u, id.query_value()); }
    [[nodiscard]] CBakedNodeIndex parent(CBakedNodeIndex n) const noexcept { const CBakedNode* p=node_slot(n); return p ? p->parent : CBakedNodeIndex{}; }
    [[nodiscard]] CPropertyNameId name_in_parent(CBakedNodeIndex n) const noexcept { const CBakedNode* p=node_slot(n); return p ? p->name_in_parent : CPropertyNameId{}; }
    [[nodiscard]] CBakedNodeIndex previous_sibling(CBakedNodeIndex n) const noexcept { const CBakedNode* p=node_slot(n); const CBakedNode* q=p ? node_slot(p->parent) : nullptr; return (p && q && n.query_value()>q->first_child_index && n.query_value()<q->first_child_index+q->child_count) ? CBakedNodeIndex{n.query_value()-1u} : CBakedNodeIndex{}; }
    [[nodiscard]] CBakedNodeIndex next_sibling(CBakedNodeIndex n) const noexcept { const CBakedNode* p=node_slot(n); const CBakedNode* q=p ? node_slot(p->parent) : nullptr; return (p && q && n.query_value()>=q->first_child_index && n.query_value()+1u<q->first_child_index+q->child_count) ? CBakedNodeIndex{n.query_value()+1u} : CBakedNodeIndex{}; }
    [[nodiscard]] std::uint32_t child_count(CBakedNodeIndex n) const noexcept { const CBakedNode* p=node_slot(n); return (p && is_container(p->type)) ? p->child_count : 0u; }
    [[nodiscard]] CBakedNodeIndex first_child(CBakedNodeIndex n) const noexcept { const CBakedNode* p=node_slot(n); return (p && is_container(p->type) && p->child_count) ? CBakedNodeIndex{p->first_child_index} : CBakedNodeIndex{}; }
    [[nodiscard]] CBakedNodeIndex last_child(CBakedNodeIndex n) const noexcept { const CBakedNode* p=node_slot(n); return (p && is_container(p->type) && p->child_count) ? CBakedNodeIndex{static_cast<std::uint32_t>(p->first_child_index+p->child_count-1u)} : CBakedNodeIndex{}; }
    [[nodiscard]] CBakedNodeIndex array_at(CBakedNodeIndex n, std::uint32_t i) const noexcept { const CBakedNode* p=node_slot(n); return (p && is_array(p->type) && i < p->child_count) ? CBakedNodeIndex{static_cast<std::uint32_t>(p->first_child_index+i)} : CBakedNodeIndex{}; }
    [[nodiscard]] CBakedNodeIndex object_child(CBakedNodeIndex n, CPropertyNameId id) const noexcept { const CBakedNode* p=node_slot(n); if(!p || p->type != EJsonNodeType::object || !id.is_valid()) return {}; for(std::uint32_t i=0;i<p->child_count;++i) { CBakedNodeIndex c{p->first_child_index+i}; if(name_in_parent(c)==id) return c; } return {}; }
    [[nodiscard]] CBakedNodeIndex object_child(CBakedNodeIndex n, const CStringView& name) const noexcept { const CBakedNode* p=node_slot(n); if (!p || p->type != EJsonNodeType::object || name.string()==nullptr) return {}; for(std::uint32_t i=0;i<p->child_count;++i) { CBakedNodeIndex c{p->first_child_index+i}; CStringView x=property_name(name_in_parent(c)); if(x.length()==name.length() && std::memcmp(x.string(),name.string(),x.length())==0) return c; } return {}; }
    [[nodiscard]] bool check_integrity() const noexcept { return is_valid(); }

private:
    [[nodiscard]] static bool is_array(EJsonNodeType t) noexcept { return t == EJsonNodeType::array || t == EJsonNodeType::recovered_duplicate_array; }
    [[nodiscard]] static bool is_container(EJsonNodeType t) noexcept { return is_array(t) || t == EJsonNodeType::object; }
    [[nodiscard]] static std::uint32_t crc(const std::uint8_t* p, std::size_t n) noexcept { std::uint32_t x=0xFFFFFFFFu; for(;n--; ++p) { x^=*p; for(unsigned b=0;b<8;++b) x=(x>>1u)^((x&1u)?0xEDB88320u:0u); } return ~x; }
    [[nodiscard]] static bool range(std::uint32_t off, std::uint64_t bytes, std::uint32_t total) noexcept { return off <= total && bytes <= static_cast<std::uint64_t>(total-off); }
    [[nodiscard]] static bool validate(const std::uint8_t* p, std::size_t n) noexcept;
    [[nodiscard]] const CBakedNode* node_slot(CBakedNodeIndex n) const noexcept { return (!m_header || !n.is_valid() || n.query_value() >= m_header->node_count) ? nullptr : reinterpret_cast<const CBakedNode*>(m_bytes+m_header->nodes_offset)+n.query_value(); }
    [[nodiscard]] CStringView string_from(std::uint32_t ro, std::uint32_t rc, std::uint32_t bo, std::uint32_t bs, std::uint32_t id) const noexcept { if(!m_header || id==0u || id>=rc) return {}; const CBakedStringRef& r=reinterpret_cast<const CBakedStringRef*>(m_bytes+ro)[id]; return (r.offset <= bs && r.length <= bs-r.offset) ? CStringView{reinterpret_cast<const char*>(m_bytes+bo+r.offset),r.length} : CStringView{}; }
    const std::uint8_t* m_bytes{nullptr}; std::size_t m_size{0u}; const CBakedDocumentHeader* m_header{nullptr};
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
    void deallocate() noexcept { m_bytes.deallocate(); m_document=CBakedDocument{}; }
    [[nodiscard]] bool is_ready() const noexcept { return m_document.is_ready(); }
    [[nodiscard]] const CBakedDocument& document() const noexcept { return m_document; }
    [[nodiscard]] const CByteBuffer& bytes() const noexcept { return m_bytes; }
private:
    CByteBuffer m_bytes; CBakedDocument m_document;
};

inline bool CBakedDocument::validate(const std::uint8_t* p, const std::size_t n) noexcept
{
    if (!p || n < sizeof(CBakedDocumentHeader) || n > std::numeric_limits<std::uint32_t>::max()) return false;
    const auto& h=*reinterpret_cast<const CBakedDocumentHeader*>(p);
    if (h.magic != k_magic || h.version != k_version || h.header_size != sizeof(h) || h.total_size != n || h.node_count < 2u || h.root_index == 0u || h.root_index >= h.node_count) return false;
    if ((reinterpret_cast<std::uintptr_t>(p) % alignof(CBakedNode)) != 0u || (h.nodes_offset % alignof(CBakedNode)) != 0u || (h.name_refs_offset % alignof(CBakedStringRef)) != 0u || (h.value_refs_offset % alignof(CBakedStringRef)) != 0u) return false;
    if (!range(h.nodes_offset, std::uint64_t(h.node_count)*sizeof(CBakedNode), h.total_size) || !range(h.name_refs_offset, std::uint64_t(h.name_ref_count)*sizeof(CBakedStringRef), h.total_size) || !range(h.value_refs_offset, std::uint64_t(h.value_ref_count)*sizeof(CBakedStringRef), h.total_size) || !range(h.name_bytes_offset,h.name_bytes_size,h.total_size) || !range(h.value_bytes_offset,h.value_bytes_size,h.total_size) || h.name_ref_count == 0u || h.value_ref_count == 0u) return false;
    if (crc(p + h.header_size, n - h.header_size) != h.payload_crc) return false;
    const auto* nodes=reinterpret_cast<const CBakedNode*>(p+h.nodes_offset);
    const auto check_refs=[p](std::uint32_t ro,std::uint32_t rc,std::uint32_t bs) noexcept { const auto* refs=reinterpret_cast<const CBakedStringRef*>(p+ro); if(refs[0].offset != 0u || refs[0].length != 0u) return false; for(std::uint32_t i=1;i<rc;++i) if(refs[i].offset > bs || refs[i].length > bs-refs[i].offset) return false; return true; };
    if (!check_refs(h.name_refs_offset,h.name_ref_count,h.name_bytes_size) || !check_refs(h.value_refs_offset,h.value_ref_count,h.value_bytes_size)) return false;
    if (nodes[0].type != EJsonNodeType::invalid) return false;
    for (std::uint32_t i=1;i<h.node_count;++i) {
        const CBakedNode& x=nodes[i];
        if (x.type <= EJsonNodeType::invalid || x.type > EJsonNodeType::recovered_duplicate_array || x.parent.query_value() >= h.node_count || x.name_in_parent.query_value() >= h.name_ref_count) return false;
        if (x.type == EJsonNodeType::string && x.payload.string_value.query_value() >= h.value_ref_count) return false;
        if (is_container(x.type)) { if (x.child_count && (x.first_child_index == 0u || x.first_child_index >= h.node_count || x.child_count > h.node_count-x.first_child_index)) return false; for(std::uint32_t c=0;c<x.child_count;++c) if(nodes[x.first_child_index+c].parent.query_value()!=i) return false; }
        else if (x.child_count != 0u || x.first_child_index != 0u) return false;
    }
    return nodes[h.root_index].parent == CBakedNodeIndex{};
}

inline bool CBakedDocumentBlock::build_from(const CBakedDocumentBuilder& source) noexcept
{
    if (!source.is_ready() || !source.check_integrity() || source.m_nodes.size() > std::numeric_limits<std::uint32_t>::max()) return false;
    const std::uint32_t node_count=static_cast<std::uint32_t>(source.m_nodes.size());
    const std::uint32_t nc=source.m_property_name_count+1u, vc=source.m_string_value_count+1u;
    if (nc == 0u || vc == 0u) return false;
    std::uint64_t nb=0u,vb=0u;
    for(std::uint32_t i=1;i<nc;++i) { const CStringView s=source.m_property_names.view(i); if(!s.string() || s.length()>std::numeric_limits<std::uint32_t>::max() || (nb += s.length()) > std::numeric_limits<std::uint32_t>::max()) return false; }
    for(std::uint32_t i=1;i<vc;++i) { const CStringView s=source.m_string_values.view(i); if(!s.string() || s.length()>std::numeric_limits<std::uint32_t>::max() || (vb += s.length()) > std::numeric_limits<std::uint32_t>::max()) return false; }
    const auto align_up=[](std::uint64_t x,std::uint64_t a) noexcept { return (x+a-1u)&~(a-1u); };
    std::uint64_t at=sizeof(CBakedDocumentHeader); const std::uint32_t no=static_cast<std::uint32_t>(align_up(at,alignof(CBakedNode))); at=std::uint64_t(no)+std::uint64_t(node_count)*sizeof(CBakedNode);
    const std::uint32_t nro=static_cast<std::uint32_t>(align_up(at,alignof(CBakedStringRef))); at=std::uint64_t(nro)+std::uint64_t(nc)*sizeof(CBakedStringRef); const std::uint32_t nbo=static_cast<std::uint32_t>(at); at+=nb;
    const std::uint32_t vro=static_cast<std::uint32_t>(align_up(at,alignof(CBakedStringRef))); at=std::uint64_t(vro)+std::uint64_t(vc)*sizeof(CBakedStringRef); const std::uint32_t vbo=static_cast<std::uint32_t>(at); at+=vb;
    if (at > std::numeric_limits<std::uint32_t>::max()) return false;
    CByteBuffer staged; if(!staged.resize(static_cast<std::size_t>(at),alignof(CBakedNode))) return false; std::memset(staged.data(),0,staged.size());
    auto* h=reinterpret_cast<CBakedDocumentHeader*>(staged.data()); *h={CBakedDocument::k_magic,CBakedDocument::k_version,static_cast<std::uint16_t>(sizeof(CBakedDocumentHeader)),source.m_contains_recovered_duplicate_arrays ? CBakedDocument::k_flag_recovered_duplicate_arrays : 0u,static_cast<std::uint32_t>(at),source.m_root.query_value(),no,node_count,nro,nc,nbo,static_cast<std::uint32_t>(nb),vro,vc,vbo,static_cast<std::uint32_t>(vb),0u};
    std::memcpy(staged.data()+no,source.m_nodes.data(),std::size_t(node_count)*sizeof(CBakedNode));
    auto copy_strings=[&](const CStableStrings& strings,std::uint32_t ro,std::uint32_t bo,std::uint32_t count) noexcept { auto* refs=reinterpret_cast<CBakedStringRef*>(staged.data()+ro); std::uint32_t cursor=0u; for(std::uint32_t i=1;i<count;++i) { CStringView s=strings.view(i); refs[i]={cursor,static_cast<std::uint32_t>(s.length())}; std::memcpy(staged.data()+bo+cursor,s.string(),s.length()); cursor+=static_cast<std::uint32_t>(s.length()); } };
    copy_strings(source.m_property_names,nro,nbo,nc); copy_strings(source.m_string_values,vro,vbo,vc); h->payload_crc=CBakedDocument::crc(staged.data()+sizeof(*h),staged.size()-sizeof(*h));
    CBakedDocument checked{staged.data(),staged.size()}; if(!checked.is_ready()) return false; m_bytes=std::move(staged); m_document=CBakedDocument{m_bytes.data(),m_bytes.size()}; return m_document.is_ready();
}

#endif
