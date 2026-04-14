/// @file document.hpp
/// @brief Document model (Abstract Syntax Tree) for the asciiquack C++17
///        AsciiDoc processor.
///
/// The object hierarchy mirrors the Ruby Asciidoctor model:
///
///   Document  (root)
///     Section           (== Title, === Sub, …)
///       Block           (paragraph, listing, literal, admonition, …)
///       List            (ulist / olist / dlist / colist)
///         ListItem
///       Table
///         TableRow
///           TableCell
///
/// Inline formatting is represented inside the *source* string of a simple
/// block; it is resolved by the Substitutors into HTML during conversion.
///
/// All heap-allocated nodes are managed through shared_ptr to keep ownership
/// straightforward and avoid dangling-pointer bugs.

#pragma once

#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace asciiquack {

// ─────────────────────────────────────────────────────────────────────────────
// Forward declarations
// ─────────────────────────────────────────────────────────────────────────────

class Block;
class Section;
class Document;
class List;
class ListItem;
class Table;

using BlockPtr   = std::shared_ptr<Block>;
using SectionPtr = std::shared_ptr<Section>;
using DocumentPtr = std::shared_ptr<Document>;
using ListPtr    = std::shared_ptr<List>;
using ListItemPtr = std::shared_ptr<ListItem>;
using TablePtr   = std::shared_ptr<Table>;

// ─────────────────────────────────────────────────────────────────────────────
// Enumerations
// ─────────────────────────────────────────────────────────────────────────────

/// Identifies the structural role of a block node.
enum class BlockContext {
    Document,
    Preamble,
    Section,
    Paragraph,
    Listing,       ///< source-code / verbatim listing
    Literal,       ///< monospace literal block
    Example,
    Quote,
    Verse,
    Sidebar,
    Admonition,
    Ulist,         ///< unordered list
    Olist,         ///< ordered list
    Dlist,         ///< description list
    Colist,        ///< callout list
    Table,
    Image,
    Video,
    Audio,
    Pass,          ///< passthrough block
    Open,
    ThematicBreak,
    PageBreak,
    FloatingTitle,
    Toc,
};

/// Determines how a block's content is treated.
enum class ContentModel {
    Compound,   ///< contains other blocks
    Simple,     ///< contains inline content (a single paragraph)
    Verbatim,   ///< verbatim – no inline subs except callouts
    Raw,        ///< raw – no substitutions at all
    Empty,      ///< no content
};

/// Recognised admonition labels.
enum class AdmonitionType {
    Note,
    Tip,
    Warning,
    Important,
    Caution,
};

/// Document processing safe-mode, controls filesystem / network access.
enum class SafeMode {
    Unsafe = 0,   ///< no restrictions
    Safe   = 1,   ///< document-relative includes only
    Server = 10,  ///< conversion attributes locked
    Secure = 20,  ///< no file I/O (default for the CLI)
};

/// List variant.
enum class ListType {
    Unordered,
    Ordered,
    Description,
    Callout,
};

/// Ordered-list numbering style.
enum class OrderedListStyle {
    Arabic,
    LowerAlpha,
    UpperAlpha,
    LowerRoman,
    UpperRoman,
};

// ─────────────────────────────────────────────────────────────────────────────
// Block  (base class for every structural node)
// ─────────────────────────────────────────────────────────────────────────────

/// Base class for every node in the AST.
///
/// A Block may contain child blocks (compound model), a single text string
/// (simple model), or verbatim source lines (verbatim / raw model).
class Block {
public:
    explicit Block(BlockContext ctx,
                   Block*       parent       = nullptr,
                   ContentModel content_model = ContentModel::Compound)
        : context_(ctx), parent_(parent), content_model_(content_model) {}

    virtual ~Block() = default;

    // Prevent accidental copying of large AST subtrees.
    Block(const Block&)            = delete;
    Block& operator=(const Block&) = delete;
    Block(Block&&)                 = default;
    Block& operator=(Block&&)      = default;

    // ── Identity ─────────────────────────────────────────────────────────────

    [[nodiscard]] BlockContext  context()      const noexcept { return context_; }
    [[nodiscard]] ContentModel  content_model() const noexcept { return content_model_; }
    [[nodiscard]] Block*        parent()       const noexcept { return parent_; }

    void set_parent(Block* p) noexcept { parent_ = p; }
    void set_context(BlockContext ctx) noexcept { context_ = ctx; }
    void set_content_model(ContentModel cm) noexcept { content_model_ = cm; }

    /// Node name used by the converter dispatch (e.g. "paragraph", "section").
    [[nodiscard]] virtual std::string node_name() const { return "block"; }

    // ── Identification & metadata ─────────────────────────────────────────────

    [[nodiscard]] const std::string& id()    const noexcept { return id_; }
    [[nodiscard]] const std::string& title() const noexcept { return title_; }
    [[nodiscard]] bool has_title()           const noexcept { return !title_.empty(); }

    void set_id(std::string v)    { id_    = std::move(v); }
    void set_title(std::string v) { title_ = std::move(v); }

    [[nodiscard]] const std::string& style()   const noexcept { return style_; }
    [[nodiscard]] const std::string& caption() const noexcept { return caption_; }
    [[nodiscard]] const std::string& role()    const noexcept { return role_; }

    void set_style(std::string v)   { style_   = std::move(v); }
    void set_caption(std::string v) { caption_ = std::move(v); }
    void set_role(std::string v)    { role_    = std::move(v); }

    // ── Attributes ────────────────────────────────────────────────────────────

    using AttrMap = std::unordered_map<std::string, std::string>;

    [[nodiscard]] bool has_attr(const std::string& name) const {
        return attributes_.count(name) > 0;
    }

    /// Return a stable reference to the attribute value, or to the empty
    /// string if the attribute is not set.  The returned reference is valid
    /// as long as the block is alive and the attribute is not removed.
    [[nodiscard]] const std::string& attr(const std::string& name) const {
        auto it = attributes_.find(name);
        return (it != attributes_.end()) ? it->second : empty_;
    }

    /// Return the attribute value by value, or @p fallback if not set.
    ///
    /// This overload deliberately returns by value (not by reference) so that
    /// the caller can safely store the result even when the attribute is absent
    /// and the fallback would otherwise be a dangling reference to a temporary.
    [[nodiscard]] std::string attr(const std::string& name,
                                   std::string fallback) const {
        auto it = attributes_.find(name);
        return (it != attributes_.end()) ? it->second : std::move(fallback);
    }

    void set_attr(std::string name, std::string value) {
        attributes_.insert_or_assign(std::move(name), std::move(value));
    }

    void remove_attr(const std::string& name) { attributes_.erase(name); }

    [[nodiscard]] const AttrMap& attributes() const noexcept { return attributes_; }
    [[nodiscard]]       AttrMap& attributes()       noexcept { return attributes_; }

    // ── Children (compound content model) ────────────────────────────────────

    [[nodiscard]] const std::vector<BlockPtr>& blocks() const noexcept { return blocks_; }
    [[nodiscard]]       std::vector<BlockPtr>& blocks()       noexcept { return blocks_; }
    [[nodiscard]] bool has_blocks() const noexcept { return !blocks_.empty(); }

    /// Append a child block; sets its parent pointer.
    void append(BlockPtr child) {
        if (child) {
            child->set_parent(this);
            blocks_.push_back(std::move(child));
        }
    }

    // ── Verbatim / simple content ─────────────────────────────────────────────

    /// Plain text content (for simple / verbatim blocks).
    [[nodiscard]] const std::string& source() const noexcept { return source_; }
    void set_source(std::string v) { source_ = std::move(v); }

    /// Individual source lines (set by the parser for verbatim blocks).
    [[nodiscard]] const std::vector<std::string>& lines() const noexcept { return lines_; }
    void set_lines(std::vector<std::string> v) { lines_ = std::move(v); }
    void append_line(std::string ln) { lines_.push_back(std::move(ln)); }

    // ── Document reference ────────────────────────────────────────────────────

    /// Walk up to the Document root.  Returns nullptr for orphan nodes.
    [[nodiscard]] const Document* document() const noexcept;
    [[nodiscard]]       Document* document()       noexcept;

protected:
    BlockContext  context_;
    Block*        parent_;
    ContentModel  content_model_;

    std::string id_;
    std::string title_;
    std::string style_;
    std::string caption_;
    std::string role_;

    AttrMap attributes_;

    std::vector<BlockPtr>      blocks_;
    std::string                source_;
    std::vector<std::string>   lines_;

    static const std::string empty_;
};

inline const std::string Block::empty_;   // NOLINT(readability-redundant-declaration)

// ─────────────────────────────────────────────────────────────────────────────
// Section
// ─────────────────────────────────────────────────────────────────────────────

/// Represents a section heading and its body (everything up to the next
/// section at the same or higher level).
class Section final : public Block {
public:
    explicit Section(int level, Block* parent = nullptr)
        : Block(BlockContext::Section, parent, ContentModel::Compound),
          level_(level) {}

    [[nodiscard]] std::string node_name() const override { return "section"; }

    [[nodiscard]] int  level()    const noexcept { return level_; }
    [[nodiscard]] bool numbered() const noexcept { return numbered_; }
    [[nodiscard]] bool special()  const noexcept { return special_; }
    [[nodiscard]] int  number()   const noexcept { return number_; }
    [[nodiscard]] const std::string& sectname() const noexcept { return sectname_; }

    void set_level(int v)              { level_         = v; }
    void set_numbered(bool v)          { numbered_      = v; }
    void set_special(bool v)           { special_       = v; }
    void set_number(int v)             { number_        = v; }
    void set_sectname(std::string v)   { sectname_      = std::move(v); }
    void set_sectnum_string(std::string v) { sectnum_string_ = std::move(v); }

    [[nodiscard]] const std::string& sectnum_string() const noexcept { return sectnum_string_; }

private:
    int  level_    = 1;
    bool numbered_ = false;
    bool special_  = false;
    int  number_   = 0;
    std::string sectname_;
    std::string sectnum_string_;  ///< e.g. "1.2.3." when sectnums is active
};

// ─────────────────────────────────────────────────────────────────────────────
// Author and revision metadata
// ─────────────────────────────────────────────────────────────────────────────

struct AuthorInfo {
    std::string firstname;
    std::string middlename;
    std::string lastname;
    std::string email;

    [[nodiscard]] std::string fullname() const {
        std::string name = firstname;
        if (!middlename.empty()) { name += ' '; name += middlename; }
        if (!lastname.empty())   { name += ' '; name += lastname;   }
        return name;
    }

    [[nodiscard]] std::string initials() const {
        std::string init;
        if (!firstname.empty())  init += firstname[0];
        if (!middlename.empty()) init += middlename[0];
        if (!lastname.empty())   init += lastname[0];
        return init;
    }
};

struct RevisionInfo {
    std::string number;   ///< version number  (e.g. "1.0")
    std::string date;     ///< revision date   (e.g. "2013-01-01")
    std::string remark;   ///< optional remark (after the colon)
};

struct DocumentHeader {
    std::string             title;
    std::vector<AuthorInfo> authors;
    RevisionInfo            revision;
    bool                    has_header = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// TocEntry  –  a single entry in the auto-generated Table of Contents
// ─────────────────────────────────────────────────────────────────────────────

struct TocEntry {
    std::string id;
    std::string title;
    int         level  = 1;
    std::string sectnum;  ///< section number prefix, e.g. "1.2." (empty if not numbered)
};

// ─────────────────────────────────────────────────────────────────────────────
// Document  (root node)
// ─────────────────────────────────────────────────────────────────────────────

/// Root of the AST.  Owns all document-level state: header metadata,
/// document attributes, and safe-mode.
class Document final : public Block {
public:
    Document()
        : Block(BlockContext::Document, nullptr, ContentModel::Compound) {
        // Set default document attributes
        set_attr("asciidoctor-version", "0.1.0");
        set_attr("doctype",  "article");
        set_attr("backend",  "html5");
        set_attr("basebackend", "html");
        set_attr("filetype", "html");
        set_attr("outfilesuffix", ".html");
        set_attr("encoding", "UTF-8");
        set_attr("lang",     "en");
        set_attr("idprefix", "_");
        set_attr("idseparator", "_");
        set_attr("toc-placement", "auto");
        set_attr("sectids",  "");
        set_attr("max-include-depth", "64");
    }

    [[nodiscard]] std::string node_name() const override { return "document"; }

    // ── Header ────────────────────────────────────────────────────────────────

    [[nodiscard]] const DocumentHeader& header() const noexcept { return header_; }
    [[nodiscard]]       DocumentHeader& header()       noexcept { return header_; }

    [[nodiscard]] const std::string& doctitle() const noexcept { return header_.title; }
    void set_doctitle(std::string v) { header_.title = std::move(v); }

    [[nodiscard]] const std::vector<AuthorInfo>& authors() const noexcept {
        return header_.authors;
    }
    void add_author(AuthorInfo a) { header_.authors.push_back(std::move(a)); }

    [[nodiscard]] const RevisionInfo& revision() const noexcept { return header_.revision; }
    void set_revision(RevisionInfo r) { header_.revision = std::move(r); }

    // ── Document settings ─────────────────────────────────────────────────────

    [[nodiscard]] SafeMode safe_mode()  const noexcept { return safe_mode_; }
    void set_safe_mode(SafeMode sm)           { safe_mode_ = sm; }

    [[nodiscard]] const std::string& doctype() const noexcept {
        return attr("doctype");
    }
    void set_doctype(std::string v) { set_attr("doctype", std::move(v)); }

    [[nodiscard]] const std::string& backend() const noexcept {
        return attr("backend");
    }
    void set_backend(std::string v) { set_attr("backend", std::move(v)); }

    [[nodiscard]] const std::string& source_file() const noexcept { return source_file_; }
    void set_source_file(std::string v) { source_file_ = std::move(v); }

    [[nodiscard]] const std::string& base_dir() const noexcept { return base_dir_; }
    void set_base_dir(std::string v) { base_dir_ = std::move(v); }

    // ── Section numbering ─────────────────────────────────────────────────────

    int next_section_number() noexcept { return ++section_counter_; }
    [[nodiscard]] int section_counter() const noexcept { return section_counter_; }

    /// Increment the per-level section counter and return the new value.
    int increment_secnum(int level) noexcept {
        if (level >= 1 && level <= 6) {
            return ++secnum_counters_[static_cast<std::size_t>(level)];
        }
        return 0;
    }

    /// Reset all section counters deeper than @p level to zero.
    void reset_secnums_below(int level) noexcept {
        for (int l = level + 1; l <= 6; ++l) {
            secnum_counters_[static_cast<std::size_t>(l)] = 0;
        }
    }

    /// Return the current counter value for the given section level (1–6).
    [[nodiscard]] int secnum_at(int level) const noexcept {
        if (level >= 1 && level <= 6) {
            return secnum_counters_[static_cast<std::size_t>(level)];
        }
        return 0;
    }

    // ── Table of Contents catalogue ────────────────────────────────────────────

    void add_toc_entry(TocEntry e)                               { toc_entries_.push_back(std::move(e)); }
    [[nodiscard]] const std::vector<TocEntry>& toc_entries() const noexcept { return toc_entries_; }

    // ── Include-depth tracking ─────────────────────────────────────────────────

    /// Try to enter an include nesting level.  Returns false if the depth
    /// limit (controlled by the max-include-depth attribute) has been reached.
    bool try_enter_include() noexcept {
        int max = 64;
        const auto& val = attr("max-include-depth");
        if (!val.empty()) {
            try { max = std::stoi(val); } catch (...) {}
        }
        if (include_count_ >= max) { return false; }
        ++include_count_;
        return true;
    }

    // ── Reference catalogue ────────────────────────────────────────────────────

    using RefMap = std::unordered_map<std::string, std::string>;

    [[nodiscard]] const RefMap& catalog_refs() const noexcept { return catalog_refs_; }
    [[nodiscard]]       RefMap& catalog_refs()       noexcept { return catalog_refs_; }

    void register_ref(std::string id, std::string reftext) {
        catalog_refs_.emplace(std::move(id), std::move(reftext));
    }

    [[nodiscard]] std::optional<std::string> lookup_ref(const std::string& id) const {
        auto it = catalog_refs_.find(id);
        if (it != catalog_refs_.end()) return it->second;
        return std::nullopt;
    }

private:
    DocumentHeader header_;
    SafeMode       safe_mode_     = SafeMode::Secure;
    int            section_counter_ = 0;
    std::array<int, 7> secnum_counters_{};  ///< per-level counters [1..6]
    int            include_count_  = 0;     ///< total includes processed
    std::vector<TocEntry> toc_entries_;
    std::string    source_file_;
    std::string    base_dir_;
    RefMap         catalog_refs_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Block::document()  (implemented after Document is defined)
// ─────────────────────────────────────────────────────────────────────────────

inline const Document* Block::document() const noexcept {
    const Block* node = this;
    while (node->parent_ != nullptr) { node = node->parent_; }
    return dynamic_cast<const Document*>(node);
}

inline Document* Block::document() noexcept {
    Block* node = this;
    while (node->parent_ != nullptr) { node = node->parent_; }
    return dynamic_cast<Document*>(node);
}

// ─────────────────────────────────────────────────────────────────────────────
// List nodes
// ─────────────────────────────────────────────────────────────────────────────

/// A single item inside a List.
class ListItem final : public Block {
public:
    explicit ListItem(Block* parent = nullptr)
        : Block(BlockContext::Paragraph, parent, ContentModel::Compound) {}

    [[nodiscard]] std::string node_name() const override { return "list_item"; }

    [[nodiscard]] const std::string& marker() const noexcept { return marker_; }
    [[nodiscard]] const std::string& term()   const noexcept { return term_;   }
    [[nodiscard]] bool has_term()             const noexcept { return !term_.empty(); }

    void set_marker(std::string v) { marker_ = std::move(v); }
    void set_term(std::string v)   { term_   = std::move(v); }

private:
    std::string marker_;
    std::string term_;   ///< description list term (if any)
};

/// Container for list items (ulist / olist / dlist / colist).
class List final : public Block {
public:
    explicit List(ListType lt, Block* parent = nullptr)
        : Block(list_context(lt), parent, ContentModel::Compound),
          list_type_(lt) {}

    [[nodiscard]] std::string node_name() const override {
        switch (list_type_) {
            case ListType::Unordered:   return "ulist";
            case ListType::Ordered:     return "olist";
            case ListType::Description: return "dlist";
            case ListType::Callout:     return "colist";
        }
        return "ulist";
    }

    [[nodiscard]] ListType         list_type()     const noexcept { return list_type_; }
    [[nodiscard]] OrderedListStyle ordered_style() const noexcept { return ordered_style_; }
    [[nodiscard]] bool             nested()        const noexcept { return nested_; }

    void set_ordered_style(OrderedListStyle s) { ordered_style_ = s; }
    void set_nested(bool v) { nested_ = v; }

    [[nodiscard]] const std::vector<std::shared_ptr<ListItem>>& items() const noexcept {
        return items_;
    }
    [[nodiscard]] std::vector<std::shared_ptr<ListItem>>& items() noexcept {
        return items_;
    }

    void add_item(std::shared_ptr<ListItem> item) {
        if (item) {
            item->set_parent(this);
            items_.push_back(std::move(item));
        }
    }

private:
    static BlockContext list_context(ListType lt) {
        switch (lt) {
            case ListType::Unordered:   return BlockContext::Ulist;
            case ListType::Ordered:     return BlockContext::Olist;
            case ListType::Description: return BlockContext::Dlist;
            case ListType::Callout:     return BlockContext::Colist;
        }
        return BlockContext::Ulist;
    }

    ListType         list_type_;
    OrderedListStyle ordered_style_ = OrderedListStyle::Arabic;
    bool             nested_        = false;
    std::vector<std::shared_ptr<ListItem>> items_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Table nodes
// ─────────────────────────────────────────────────────────────────────────────

struct ColumnSpec {
    int         width  = 1;        ///< proportional column width
    std::string halign;            ///< left | center | right
    std::string valign;            ///< top  | middle | bottom
    std::string style;             ///< d(efault) s e m h l a
};

/// Context of a table cell: header row, body, or footer row.
enum class TableCellContext { Head, Body, Foot };

class TableCell final : public Block {
public:
    TableCell()
        : Block(BlockContext::Paragraph, nullptr, ContentModel::Simple) {}

    [[nodiscard]] std::string node_name() const override { return "table_cell"; }

    [[nodiscard]] const std::string& halign() const noexcept { return halign_; }
    [[nodiscard]] const std::string& valign() const noexcept { return valign_; }
    [[nodiscard]] const std::string& style()  const noexcept { return style_;  }
    [[nodiscard]] int colspan()               const noexcept { return colspan_; }
    [[nodiscard]] int rowspan()               const noexcept { return rowspan_; }
    [[nodiscard]] TableCellContext cell_context() const noexcept { return cell_ctx_; }

    void set_halign(std::string v)         { halign_   = std::move(v); }
    void set_valign(std::string v)         { valign_   = std::move(v); }
    void set_style(std::string v)          { style_    = std::move(v); }
    void set_colspan(int v)                { colspan_  = v; }
    void set_rowspan(int v)                { rowspan_  = v; }
    void set_cell_context(TableCellContext c) { cell_ctx_ = c; }

private:
    std::string     halign_;
    std::string     valign_;
    std::string     style_;
    int             colspan_ = 1;
    int             rowspan_ = 1;
    TableCellContext cell_ctx_ = TableCellContext::Body;
};

class TableRow {
public:
    void add_cell(std::shared_ptr<TableCell> cell) {
        if (cell) cells_.push_back(std::move(cell));
    }

    [[nodiscard]] const std::vector<std::shared_ptr<TableCell>>& cells() const noexcept {
        return cells_;
    }
    [[nodiscard]] std::vector<std::shared_ptr<TableCell>>& cells() noexcept {
        return cells_;
    }

private:
    std::vector<std::shared_ptr<TableCell>> cells_;
};

class Table final : public Block {
public:
    explicit Table(Block* parent = nullptr)
        : Block(BlockContext::Table, parent, ContentModel::Compound) {}

    [[nodiscard]] std::string node_name() const override { return "table"; }

    [[nodiscard]] const std::vector<ColumnSpec>& column_specs() const noexcept {
        return column_specs_;
    }
    void set_column_specs(std::vector<ColumnSpec> v) { column_specs_ = std::move(v); }
    void add_column_spec(ColumnSpec v)                { column_specs_.push_back(std::move(v)); }

    [[nodiscard]] std::vector<TableRow>& head_rows() noexcept { return head_rows_; }
    [[nodiscard]] const std::vector<TableRow>& head_rows() const noexcept { return head_rows_; }

    [[nodiscard]] std::vector<TableRow>& body_rows() noexcept { return body_rows_; }
    [[nodiscard]] const std::vector<TableRow>& body_rows() const noexcept { return body_rows_; }

    [[nodiscard]] std::vector<TableRow>& foot_rows() noexcept { return foot_rows_; }
    [[nodiscard]] const std::vector<TableRow>& foot_rows() const noexcept { return foot_rows_; }

    [[nodiscard]] bool has_header() const noexcept { return !head_rows_.empty(); }
    [[nodiscard]] bool has_footer() const noexcept { return !foot_rows_.empty(); }

private:
    std::vector<ColumnSpec> column_specs_;
    std::vector<TableRow>   head_rows_;
    std::vector<TableRow>   body_rows_;
    std::vector<TableRow>   foot_rows_;
};

} // namespace asciiquack
