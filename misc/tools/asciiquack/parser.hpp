/// @file parser.hpp
/// @brief AsciiDoc parser – declaration.
///
/// The Parser converts a stream of lines (from a Reader) into a Document AST.
/// It mirrors the structure of the Ruby Asciidoctor Parser class:
///
///   Parser::parse()  is the main entry point.
///   parse_document_header() reads the optional document title / author / revision
///     lines and any leading attribute entries.
///   parse_blocks() iterates over the remaining lines, dispatching to specialised
///     parse_*() functions for each block type.
///
/// All methods are static; the Parser class itself is never instantiated.

#pragma once

#include "document.hpp"
#include "reader.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace asciiquack {

// ─────────────────────────────────────────────────────────────────────────────
// ParseOptions
// ─────────────────────────────────────────────────────────────────────────────

/// Options that influence parsing behaviour.
struct ParseOptions {
    SafeMode    safe_mode       = SafeMode::Secure;
    std::string doctype         = "article";  ///< article | book | manpage | inline
    std::string backend         = "html5";
    bool        parse_header_only = false;    ///< stop after the document header
    bool        sourcemap       = false;      ///< attach source-location info
    std::unordered_map<std::string, std::string> attributes; ///< pre-set attrs
};

// ─────────────────────────────────────────────────────────────────────────────
// Parser
// ─────────────────────────────────────────────────────────────────────────────

class Parser {
public:
    Parser()  = delete;
    ~Parser() = delete;

    // ── Public API ─────────────────────────────────────────────────────────────

    /// Parse the AsciiDoc text held by @p reader into a new Document and
    /// return it.
    [[nodiscard]] static DocumentPtr parse(Reader&            reader,
                                           const ParseOptions& opts = {});

    /// Convenience: parse a string directly.
    [[nodiscard]] static DocumentPtr parse_string(
            const std::string&  content,
            const ParseOptions& opts        = {},
            const std::string&  source_path = "<stdin>");

    // ── Header / attribute parsing (also used by sub-parsers) ─────────────────

    /// Parse the optional document header (title, author, revision, leading
    /// attribute entries) from @p reader into @p doc.
    static void parse_document_header(Reader& reader, Document& doc);

    /// Parse a single attribute-entry line ( :name: value ) and apply it to
    /// the document.  Returns true if the line was an attribute entry.
    /// This overload reads (and consumes) the line and any continuation lines
    /// from @p reader.
    static bool parse_attribute_entry(Reader& reader, Document& doc);

    /// Overload: parse from a pre-read line only (no continuation support).
    /// Caller must call reader.skip_line() if this returns true.
    static bool parse_attribute_entry(const std::string& line, Document& doc);

    // ── Block-level parsing ────────────────────────────────────────────────────

    /// Parse the document body (everything after the header).
    static void parse_blocks(Reader& reader, Block& parent);

    /// Parse blocks until @p terminator is encountered (or EOF).
    /// The terminator line is consumed but not added to @p parent.
    static void parse_blocks_until(Reader& reader, Block& parent,
                                   const std::string& terminator);

    /// Parse the next block from @p reader and append it to @p parent.
    /// Returns false if no block was produced (e.g. blank region).
    static bool parse_next_block(Reader& reader, Block& parent);

    // ── Section title helpers ──────────────────────────────────────────────────

    /// Return the section level encoded by @p marker ("=" → 0, "==" → 1, …).
    /// Returns -1 if @p line is not a section-title line.
    [[nodiscard]] static int section_level(const std::string& line);

    /// Extract the title text from an ATX-style section title line.
    [[nodiscard]] static std::string section_title_text(const std::string& line);

private:
    // ── Specialised block parsers ──────────────────────────────────────────────

    static std::shared_ptr<Section> parse_section(
            Reader& reader, Block& parent, const std::string& title_line,
            std::unordered_map<std::string, std::string>& pending_attrs);

    static BlockPtr parse_delimited_block(
            Reader& reader, Block& parent,
            const std::string& delimiter,
            std::unordered_map<std::string, std::string>& pending_attrs);

    static BlockPtr parse_paragraph(
            Reader& reader, Block& parent,
            std::unordered_map<std::string, std::string>& pending_attrs);

    static BlockPtr parse_listing(
            Reader& reader, Block& parent,
            const std::string& delimiter,
            std::unordered_map<std::string, std::string>& pending_attrs);

    static BlockPtr parse_literal(
            Reader& reader, Block& parent,
            const std::string& delimiter,
            std::unordered_map<std::string, std::string>& pending_attrs);

    static BlockPtr parse_example(
            Reader& reader, Block& parent,
            std::unordered_map<std::string, std::string>& pending_attrs);

    static BlockPtr parse_sidebar(
            Reader& reader, Block& parent,
            std::unordered_map<std::string, std::string>& pending_attrs);

    static BlockPtr parse_quote(
            Reader& reader, Block& parent,
            std::unordered_map<std::string, std::string>& pending_attrs);

    static BlockPtr parse_admonition_block(
            Reader& reader, Block& parent,
            std::unordered_map<std::string, std::string>& pending_attrs,
            const std::string& delimiter);

    static BlockPtr parse_image(
            Reader& reader, Block& parent, const std::string& line,
            std::unordered_map<std::string, std::string>& pending_attrs);

    static std::shared_ptr<Table> parse_table(
            Reader& reader, Block& parent,
            std::unordered_map<std::string, std::string>& pending_attrs);

    static std::shared_ptr<List> parse_list(
            Reader& reader, Block& parent,
            ListType list_type, const std::string& first_line,
            std::unordered_map<std::string, std::string>& pending_attrs);

    // ── Block attribute helpers ────────────────────────────────────────────────

    /// Consume any block-attribute lines (e.g., [source,java] or .Title) that
    /// precede the next block and accumulate them in @p pending_attrs.
    static void collect_block_attributes(
            Reader& reader,
            std::unordered_map<std::string, std::string>& pending_attrs,
            std::string& pending_title,
            std::string& pending_id);

    /// Apply @p pending_attrs, title, and id to @p block.
    static void apply_block_attributes(
            Block& block,
            std::unordered_map<std::string, std::string>& pending_attrs,
            const std::string& pending_title,
            const std::string& pending_id);

    // ── Inline author / revision helpers ──────────────────────────────────────

    static std::vector<AuthorInfo> parse_author_line(const std::string& line);
    static RevisionInfo            parse_revision_line(const std::string& line);

    // ── List item helpers ──────────────────────────────────────────────────────

    static std::string list_marker(ListType lt, const std::string& line);
    static int         list_marker_level(ListType lt, const std::string& marker);
};

} // namespace asciiquack
