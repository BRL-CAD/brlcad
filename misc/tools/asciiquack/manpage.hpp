/// @file manpage.hpp
/// @brief Man page (troff/groff) converter for asciiquack.
///
/// Walks the Document AST and produces a troff-formatted man page (.1 / .N
/// file) that is compatible with the output of the Ruby Asciidoctor manpage
/// backend.
///
/// The converter is a pure function: it takes a const Document& and returns
/// a std::string.  All state is held on the call stack.
///
/// Output format: groff/troff man macros (man(7)).
///   .TH   – title header
///   .SH   – section heading (uppercased)
///   .SS   – sub-section heading
///   .PP   – paragraph
///   .B    – bold
///   .I    – italic
///   .nf / .fi – no-fill / fill (for preformatted content)
///   .RS / .RE  – relative indent start / end
///   .IP   – list item with hanging indent
///   .TP   – term–description pair

#pragma once

#include "document.hpp"
#include "substitutors.hpp"
#include "outbuf.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <unordered_map>

namespace asciiquack {

// ─────────────────────────────────────────────────────────────────────────────
// ManpageConverter
// ─────────────────────────────────────────────────────────────────────────────

class ManpageConverter {
public:
    ManpageConverter() = default;

    /// Convert a parsed Document to a troff-formatted man page string.
    [[nodiscard]] std::string convert(const Document& doc) const {
        counters_.clear();
        OutputBuffer out;
        convert_document(doc, out);
        return out.release();
    }

private:
    // ── Per-conversion mutable state ──────────────────────────────────────────
    mutable std::unordered_map<std::string, int> counters_;

    // ── Troff text escaping ───────────────────────────────────────────────────

    /// Escape text for use in troff running text.
    /// Backslashes are doubled; a leading '.' or '\'' is preceded by \& to
    /// prevent it from being interpreted as a troff request.
    [[nodiscard]] static std::string troff_escape(const std::string& text) {
        std::string out;
        out.reserve(text.size() + 8);
        for (std::size_t i = 0; i < text.size(); ++i) {
            char c = text[i];
            if (c == '\\') {
                out += "\\\\";
            } else if (c == '-') {
                // Use \- so that copy-paste produces a real hyphen-minus
                out += "\\-";
            } else {
                out += c;
            }
        }
        // If the line starts with '.' or '\'', prepend \& to avoid macro
        // interpretation.
        if (!out.empty() && (out[0] == '.' || out[0] == '\'')) {
            out.insert(0, "\\&");
        }
        return out;
    }

    /// Convert an AsciiDoc inline text string to troff inline markup.
    /// Replaces *bold*, _italic_, `mono`, and attribute references.
    [[nodiscard]] std::string inline_subs(const std::string& text,
                                          const Document&    doc) const {
        // Apply attribute substitution first
        std::string s = sub_attributes(text, doc.attributes());
        // Apply basic inline markup conversions for troff.
        // troff_inline() already escapes plain-text content; do NOT apply
        // troff_escape() afterwards or it will double-escape the \fB...\fR
        // markers that troff_inline() has just generated.
        return troff_inline(s);
    }

    /// Apply inline troff markup (bold/italic/mono) to a pre-substituted string.
    /// Also escapes plain-text characters that have special meaning in troff
    /// (backslash and minus sign).  The caller must NOT apply troff_escape()
    /// afterwards, or the \fB...\fP sequences generated here will be corrupted.
    /// Nested inline markup (e.g. italic inside bold: *foo _bar_ baz*) is handled
    /// by recursively calling troff_inline() on the content of each span.
    ///
    /// NOTE: Inline span closings use \fP (restore previous font) rather than
    /// \fR (set roman), matching asciidoctor's manpage backend behaviour.  \fP
    /// properly restores the enclosing font context so that, for example, italic
    /// inside a bold title correctly returns to bold after the italic word.
    [[nodiscard]] static std::string troff_inline(const std::string& text) {
        std::string out;
        out.reserve(text.size() + 32);

        // Helper: returns true if character c is NOT a valid opening boundary
        // character for a constrained inline span.  Matches asciidoctor's rule:
        //   (^|[^\p{Word};:}])
        // i.e. a word character (letter/digit/_), or one of ;, :, }  – all
        // preclude constrained parsing.
        // In addition, > and < are excluded because asciidoctor processes them
        // as HTML entities (&gt; / &lt;) before applying inline patterns; the
        // resulting trailing ';' in those entities falls into the ';' exclusion.
        auto is_constrained_open_invalid = [](char c) -> bool {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') return true;
            if (c == ';' || c == ':' || c == '}') return true;
            if (c == '>' || c == '<') return true;  // would become &gt;/&lt; with ;
            return false;
        };
        std::size_t i = 0;

        // Find the end of a constrained inline span starting at `start` (one past
        // the opening marker).  The closing marker character is `marker`.  A valid
        // constrained close must be:
        //  - preceded by a non-whitespace character, AND
        //  - followed by a non-word character (or end of string).
        // Returns npos if no valid close is found.
        auto find_constrained_end = [&](std::size_t start, char marker) -> std::size_t {
            for (std::size_t j = start; j < text.size(); ++j) {
                if (text[j] != marker) continue;
                // Preceding char must be non-whitespace
                if (j == 0) continue;
                char prev = text[j - 1];
                if (prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r') continue;
                // Following char must be non-word (or end of string)
                if (j + 1 < text.size()) {
                    char next = text[j + 1];
                    if (std::isalnum(static_cast<unsigned char>(next)) || next == '_') continue;
                }
                // Non-empty span
                if (j > start) return j;
            }
            return std::string::npos;
        };

        while (i < text.size()) {
            // Em-dash: -- (but not --- or ----).
            // Asciidoctor man page backend converts -- to \(em.
            if (text[i] == '-' && i + 1 < text.size() && text[i+1] == '-') {
                // Ensure it's exactly -- (not ---, ----, etc.)
                bool prev_dash = (i > 0 && text[i-1] == '-');
                bool next_dash = (i + 2 < text.size() && text[i+2] == '-');
                if (!prev_dash && !next_dash) {
                    out += "\\(em";
                    i += 2;
                    continue;
                }
            }
            // Ellipsis: ... → .\|.\|.  (asciidoctor man page backend form)
            if (text[i] == '.' && i + 2 < text.size() && text[i+1] == '.' && text[i+2] == '.') {
                // Make sure it's not part of a longer run
                bool prev_dot = (i > 0 && text[i-1] == '.');
                bool next_dot = (i + 3 < text.size() && text[i+3] == '.');
                if (!prev_dot && !next_dot) {
                    out += ".\\|.\\|.";
                    i += 3;
                    continue;
                }
            }
            // Unconstrained bold: **...**
            if (i + 1 < text.size() && text[i] == '*' && text[i+1] == '*') {
                auto end = text.find("**", i + 2);
                if (end != std::string::npos) {
                    out += "\\fB";
                    out += troff_inline(text.substr(i + 2, end - i - 2));
                    out += "\\fP";
                    i = end + 2;
                    continue;
                }
            }
            // Constrained bold: *word* – opening * must not be preceded by an
            // invalid boundary character (word chars, ;, :, }, <, >).
            if (text[i] == '*' && (i == 0 || !is_constrained_open_invalid(text[i-1]))) {
                auto end = find_constrained_end(i + 1, '*');
                if (end != std::string::npos) {
                    out += "\\fB";
                    out += troff_inline(text.substr(i + 1, end - i - 1));
                    out += "\\fP";
                    i = end + 1;
                    continue;
                }
            }
            // Unconstrained italic: __...__
            if (i + 1 < text.size() && text[i] == '_' && text[i+1] == '_') {
                auto end = text.find("__", i + 2);
                if (end != std::string::npos) {
                    out += "\\fI";
                    out += troff_inline(text.substr(i + 2, end - i - 2));
                    out += "\\fP";
                    i = end + 2;
                    continue;
                }
            }
            // Constrained italic: _word_ – same boundary rules as bold.
            if (text[i] == '_' && (i == 0 || !is_constrained_open_invalid(text[i-1]))) {
                auto end = find_constrained_end(i + 1, '_');
                if (end != std::string::npos) {
                    out += "\\fI";
                    out += troff_inline(text.substr(i + 1, end - i - 1));
                    out += "\\fP";
                    i = end + 1;
                    continue;
                }
            }
            // Unconstrained mono: ``...``
            if (i + 1 < text.size() && text[i] == '`' && text[i+1] == '`') {
                auto end = text.find("``", i + 2);
                if (end != std::string::npos) {
                    out += "\\f(CW";
                    out += troff_inline(text.substr(i + 2, end - i - 2));
                    out += "\\fP";
                    i = end + 2;
                    continue;
                }
            }
            // Constrained mono: `word` – same boundary rules as bold.
            if (text[i] == '`' && (i == 0 || !is_constrained_open_invalid(text[i-1]))) {
                auto end = find_constrained_end(i + 1, '`');
                if (end != std::string::npos) {
                    out += "\\f(CW";
                    out += troff_inline(text.substr(i + 1, end - i - 1));
                    out += "\\fP";
                    i = end + 1;
                    continue;
                }
            }
            // Cross-reference macro: <<anchor,text>> or <<anchor>>.
            // In man pages, asciidoctor renders xrefs as just the link text
            // (or "[anchor]" for bare xrefs), discarding the anchor.
            // Guard: only match if the content between << and >> looks like a
            // valid xref (no newlines; the anchor part must contain no spaces).
            if (i + 1 < text.size() && text[i] == '<' && text[i+1] == '<') {
                auto close = text.find(">>", i + 2);
                if (close != std::string::npos) {
                    std::string inner = text.substr(i + 2, close - i - 2);
                    auto comma = inner.find(',');
                    // The anchor portion (before the comma, or the whole inner
                    // if no comma) must not contain spaces or newlines.
                    std::string anchor_part = (comma != std::string::npos)
                                              ? inner.substr(0, comma) : inner;
                    bool valid_xref = (anchor_part.find(' ') == std::string::npos &&
                                       anchor_part.find('\n') == std::string::npos);
                    if (valid_xref) {
                        if (comma != std::string::npos) {
                            // <<anchor,text>> → render the text
                            std::string xref_text = inner.substr(comma + 1);
                            out += troff_inline(xref_text);
                        } else {
                            // <<anchor>> → render as [anchor]
                            out += '[';
                            out += troff_inline(inner);
                            out += ']';
                        }
                        i = close + 2;
                        continue;
                    }
                }
            }
            // URL link macro: http://url[text] or https://url[text].
            // Render as "text <url>" matching asciidoctor's man page backend
            // which uses the .URL macro to append the URL after the link text.
            if ((text.substr(i, 7) == "http://" || text.substr(i, 8) == "https://") &&
                text.find('[', i) != std::string::npos) {
                auto bracket = text.find('[', i);
                if (bracket != std::string::npos) {
                    auto close = text.find(']', bracket + 1);
                    if (close != std::string::npos) {
                        std::string url = text.substr(i, bracket - i);
                        std::string link_text = text.substr(bracket + 1, close - bracket - 1);
                        if (!link_text.empty()) {
                            // Render as: link text <url>  (angle brackets = troff \(la/\(ra)
                            out += troff_inline(link_text);
                            out += " \\(la";
                            out += troff_escape(url);
                            out += "\\(ra";
                        } else {
                            // No link text: output the URL only
                            out += troff_inline(url);
                        }
                        i = close + 1;
                        continue;
                    }
                }
            }
            // Stem / math macro: stem:[expr], latexmath:[expr], asciimath:[expr].
            // For man pages, render just the expression content as plain text.
            {
                static const char* const prefixes[] = {"stem:", "latexmath:", "asciimath:", nullptr};
                bool handled = false;
                for (const char* const* pp = prefixes; *pp; ++pp) {
                    std::size_t plen = std::strlen(*pp);
                    if (text.size() > i + plen && text.substr(i, plen) == *pp &&
                        text[i + plen] == '[') {
                        auto close = text.find(']', i + plen + 1);
                        if (close != std::string::npos) {
                            std::string expr = text.substr(i + plen + 1, close - i - plen - 1);
                            out += troff_inline(expr);
                            i = close + 1;
                            handled = true;
                            break;
                        }
                    }
                }
                if (handled) continue;
            }
            // Unconstrained subscript: ~~text~~ → plain text (no subscript in man)
            if (i + 1 < text.size() && text[i] == '~' && text[i+1] == '~') {
                auto end = text.find("~~", i + 2);
                if (end != std::string::npos) {
                    out += troff_inline(text.substr(i + 2, end - i - 2));
                    i = end + 2;
                    continue;
                }
            }
            // Constrained subscript: ~word~ → plain text (strip ~ markers)
            if (text[i] == '~' && i + 1 < text.size() && text[i+1] != '~' && text[i+1] != ' ') {
                auto end = text.find('~', i + 1);
                if (end != std::string::npos && end > i + 1) {
                    // Make sure the closing ~ is not followed by ~
                    bool next_is_tilde = (end + 1 < text.size() && text[end+1] == '~');
                    if (!next_is_tilde) {
                        out += troff_inline(text.substr(i + 1, end - i - 1));
                        i = end + 1;
                        continue;
                    }
                }
            }
            // Unconstrained superscript: ^^text^^ → plain text
            if (i + 1 < text.size() && text[i] == '^' && text[i+1] == '^') {
                auto end = text.find("^^", i + 2);
                if (end != std::string::npos) {
                    out += troff_inline(text.substr(i + 2, end - i - 2));
                    i = end + 2;
                    continue;
                }
            }
            // Constrained superscript: ^word^ → plain text
            if (text[i] == '^' && i + 1 < text.size() && text[i+1] != '^' && text[i+1] != ' ') {
                auto end = text.find('^', i + 1);
                if (end != std::string::npos && end > i + 1) {
                    bool next_is_caret = (end + 1 < text.size() && text[end+1] == '^');
                    if (!next_is_caret) {
                        out += troff_inline(text.substr(i + 1, end - i - 1));
                        i = end + 1;
                        continue;
                    }
                }
            }
            // Plain text: escape backslash and minus, and convert common
            // Unicode characters to troff special-character sequences so that
            // they render correctly with groff -Tascii / -Tutf8.
            {
                unsigned char b0 = static_cast<unsigned char>(text[i]);
                // 2-byte UTF-8: 0xC2 prefix
                if (b0 == 0xC2 && i + 1 < text.size()) {
                    unsigned char b1 = static_cast<unsigned char>(text[i+1]);
                    if (b1 == 0xBD) { out += "1/2"; i += 2; continue; }  // ½ U+00BD
                    if (b1 == 0xBE) { out += "3/4"; i += 2; continue; }  // ¾ U+00BE
                    if (b1 == 0xBC) { out += "1/4"; i += 2; continue; }  // ¼ U+00BC
                    if (b1 == 0xB1) { out += "\\(+-"; i += 2; continue; } // ± U+00B1
                    if (b1 == 0xB7) { out += "\\(md"; i += 2; continue; } // · U+00B7
                }
                // 2-byte UTF-8: 0xC3 prefix
                if (b0 == 0xC3 && i + 1 < text.size()) {
                    unsigned char b1 = static_cast<unsigned char>(text[i+1]);
                    if (b1 == 0x97) { out += "\\(mu"; i += 2; continue; } // × U+00D7
                    if (b1 == 0xB7) { out += "\\(di"; i += 2; continue; } // ÷ U+00F7
                }
                // 3-byte UTF-8: 0xE2 prefix
                if (b0 == 0xE2 && i + 2 < text.size()) {
                    unsigned char b1 = static_cast<unsigned char>(text[i+1]);
                    unsigned char b2 = static_cast<unsigned char>(text[i+2]);
                    if (b1 == 0x80) {
                        if (b2 == 0x94) { out += "\\(em"; i += 3; continue; } // — U+2014
                        if (b2 == 0x93) { out += "\\(en"; i += 3; continue; } // – U+2013
                        if (b2 == 0x90) { out += "\\-";   i += 3; continue; } // ‐ U+2010
                        if (b2 == 0x98) { out += "'";     i += 3; continue; } // ' U+2018
                        if (b2 == 0x99) { out += "'";     i += 3; continue; } // ' U+2019
                        if (b2 == 0x9C) { out += "\"";   i += 3; continue; }  // " U+201C
                        if (b2 == 0x9D) { out += "\"";   i += 3; continue; }  // " U+201D
                        if (b2 == 0xA2) { out += "\\(bu"; i += 3; continue; } // • U+2022
                    }
                    if (b1 == 0x88) {
                        if (b2 == 0x92) { out += "\\-";   i += 3; continue; } // − U+2212 (minus sign)
                        if (b2 == 0x86) { out += "\\(->"; i += 3; continue; } // → U+2192
                        if (b2 == 0x90) { out += "\\(<-"; i += 3; continue; } // ← U+2190
                        if (b2 == 0xA5) { out += "\\(bu"; i += 3; continue; } // ∞ → bullet fallback
                    }
                }
                char c = text[i];
                if (c == '\\') { out += "\\\\"; }
                else if (c == '-') { out += "\\-"; }
                else { out += c; }
                ++i;
            }
        }
        // If the output line starts with '.' or '\'' it would be interpreted as a
        // troff macro. Prepend \& (zero-width non-printing character) to avoid that.
        if (!out.empty() && (out[0] == '.' || out[0] == '\'')) {
            out.insert(0, "\\&");
        }
        return out;
    }

    /// Emit a troff section heading (.SH).  The title is uppercased.
    static void emit_sh(const std::string& title, OutputBuffer& out) {
        std::string upper;
        upper.reserve(title.size());
        for (char c : title) { upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }
        out << ".SH " << upper << '\n';
    }

    /// Emit a troff sub-section heading (.SS).
    static void emit_ss(const std::string& title, OutputBuffer& out) {
        out << ".SS " << title << '\n';
    }

    // ── Document ──────────────────────────────────────────────────────────────

    void convert_document(const Document& doc, OutputBuffer& out) const {
        // Comment / source hint
        out << "'\\\" t\n";

        // .TH TITLE SECTION DATE [SOURCE [MANUAL]]
        const std::string& manname   = doc.attr("manname");
        // attr(name, fallback) returns by value – safe even when the attribute
        // is absent (no dangling reference).
        const std::string  manvolnum = doc.attr("manvolnum", "1");
        const std::string  date      = doc.attr("revdate",   doc.attr("docdate"));
        // :mansource: and :manmanual: are the canonical attribute names used in
        // BRL-CAD man page headers; accept them as aliases for :source: / :manual:.
        const std::string  source    = doc.has_attr("source")
            ? doc.attr("source") : doc.attr("mansource");
        const std::string  manual    = doc.has_attr("manual")
            ? doc.attr("manual") : doc.attr("manmanual");

        // Uppercase the command name for the title
        std::string manname_upper;
        for (char c : manname) {
            manname_upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (manname_upper.empty()) {
            // Fall back to the document title
            for (char c : doc.doctitle()) {
                manname_upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
        }

        out << ".TH \"" << manname_upper << "\" \"" << manvolnum << "\" \""
            << date << "\"";
        if (!source.empty()) {
            out << " \"" << source << "\"";
            if (!manual.empty()) {
                out << " \"" << manual << "\"";
            }
        }
        out << '\n';

        // Global troff settings
        out << ".ie \\n(.g .ds Aq \\(aq\n"
            << ".el       .ds Aq '\n"
            << ".ss \\n[.ss] 0\n"
            << ".nh\n"
            << ".ad l\n"
            << ".de URL\n"
            << "\\\\$2 \\(la\\\\$1\\(ra\\\\$3\n"
            << "..\n"
            << ".als MTO URL\n"
            << ".if \\n[.g] \\{\\\n"
            << ".  mso www.tmac\n"
            << ".\\}\n";

        // Render blocks
        for (const auto& child : doc.blocks()) {
            convert_block(*child, doc, out);
        }
    }

    // ── Block dispatcher ──────────────────────────────────────────────────────

    void convert_block(const Block& block, const Document& doc,
                       OutputBuffer& out) const {
        switch (block.context()) {
            case BlockContext::Section:
                convert_section(dynamic_cast<const Section&>(block), doc, out);
                break;
            case BlockContext::Paragraph:
                convert_paragraph(block, doc, out);
                break;
            case BlockContext::Listing:
            case BlockContext::Literal:
                convert_verbatim(block, out);
                break;
            case BlockContext::Ulist:
                convert_ulist(dynamic_cast<const List&>(block), doc, out);
                break;
            case BlockContext::Olist:
                convert_olist(dynamic_cast<const List&>(block), doc, out);
                break;
            case BlockContext::Dlist:
                convert_dlist(dynamic_cast<const List&>(block), doc, out);
                break;
            case BlockContext::Admonition:
                convert_admonition(block, doc, out);
                break;
            case BlockContext::Example:
            case BlockContext::Quote:
            case BlockContext::Verse:
            case BlockContext::Sidebar:
                convert_compound(block, doc, out);
                break;
            case BlockContext::Table:
                convert_table(dynamic_cast<const Table&>(block), doc, out);
                break;
            case BlockContext::Pass:
                // Raw passthrough: emit as-is (strip HTML if present)
                out << block.source() << '\n';
                break;
            case BlockContext::ThematicBreak:
                out << ".sp\n"
                    << "\\l'\\n(.lu'\n";
                break;
            case BlockContext::PageBreak:
                out << ".bp\n";
                break;
            case BlockContext::Image:
                // Images not representable in man pages; omit with comment
                out << "\\# [image: " << block.attr("target") << "]\n";
                break;
            default:
                for (const auto& child : block.blocks()) {
                    convert_block(*child, doc, out);
                }
                break;
        }
    }

    // ── Section ───────────────────────────────────────────────────────────────

    void convert_section(const Section& sect, const Document& doc,
                         OutputBuffer& out) const {
        int level = sect.level();  // 1 = ==, 2 = ===, …

        if (level == 1) {
            emit_sh(sect.title(), out);
        } else {
            emit_ss(sect.title(), out);
        }

        for (const auto& child : sect.blocks()) {
            convert_block(*child, doc, out);
        }
    }

    // ── Paragraph ─────────────────────────────────────────────────────────────

    void convert_paragraph(const Block& block, const Document& doc,
                           OutputBuffer& out) const {
        if (block.has_title()) {
            out << ".sp\n"
                << "\\fB" << troff_escape(block.title()) << "\\fP\n";
        }
        out << ".sp\n"
            << inline_subs(block.source(), doc) << '\n';
    }

    // ── Verbatim (listing / literal) ──────────────────────────────────────────

    static void convert_verbatim(const Block& block, OutputBuffer& out) {
        if (block.has_title()) {
            out << ".sp\n"
                << "\\fB" << troff_escape(block.title()) << "\\fP\n";
        }
        out << ".sp\n"
            << ".if n .RS 4\n"
            << ".nf\n"
            << ".fam C\n";
        // Split into lines, then trim leading and trailing blank lines from the
        // content (matching asciidoctor's man page backend behaviour).
        static constexpr auto verbatim_is_blank = [](const std::string& s) {
            return s.find_first_not_of(" \t") == std::string::npos;
        };
        static constexpr const char* TAB_SPACES = "        ";  // 8 spaces per tab
        std::istringstream ss(block.source());
        std::vector<std::string> lines;
        {
            std::string ln;
            while (std::getline(ss, ln)) { lines.push_back(std::move(ln)); }
        }
        // Trim leading blank lines
        while (!lines.empty() && verbatim_is_blank(lines.front())) {
            lines.erase(lines.begin());
        }
        // Trim trailing blank lines
        while (!lines.empty() && verbatim_is_blank(lines.back())) {
            lines.pop_back();
        }
        for (const auto& line : lines) {
            if (!line.empty() && (line[0] == '.' || line[0] == '\'')) {
                out << "\\&";
            }
            // Expand tabs to 8 spaces and escape backslashes, matching
            // asciidoctor's man page backend behavior (simple tab→8-space
            // replacement, not tabstop-aligned).
            for (char c : line) {
                if (c == '\t') {
                    out << TAB_SPACES;
                } else if (c == '\\') {
                    out << "\\\\";
                } else {
                    out << c;
                }
            }
            out << '\n';
        }
        out << ".fam\n"
            << ".fi\n"
            << ".if n .RE\n";
    }

    // ── Admonition ────────────────────────────────────────────────────────────

    void convert_admonition(const Block& block, const Document& doc,
                            OutputBuffer& out) const {
        const std::string name = block.attr("name", "note");
        std::string label = name;
        // Title-case
        if (!label.empty()) {
            label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
        }

        // Match asciidoctor's man page backend format for admonition blocks:
        //   .if n .sp
        //   .RS 4
        //   .it 1 an-trap
        //   .nr an-no-space-flag 1
        //   .nr an-break-flag 1
        //   .br
        //   .ps +1
        //   .B Note
        //   .ps -1
        //   .br
        //   .sp
        //   content
        //   .sp .5v
        //   .RE
        out << ".if n .sp\n"
            << ".RS 4\n"
            << ".it 1 an-trap\n"
            << ".nr an-no-space-flag 1\n"
            << ".nr an-break-flag 1\n"
            << ".br\n"
            << ".ps +1\n"
            << ".B " << label << "\n"
            << ".ps -1\n"
            << ".br\n"
            << ".sp\n";

        if (block.content_model() == ContentModel::Simple) {
            out << inline_subs(block.source(), doc) << '\n';
        } else {
            for (const auto& child : block.blocks()) {
                convert_block(*child, doc, out);
            }
        }
        out << ".sp .5v\n"
            << ".RE\n";
    }

    // ── Compound block (example, quote, sidebar) ──────────────────────────────

    void convert_compound(const Block& block, const Document& doc,
                          OutputBuffer& out) const {
        // For example blocks with titles, emit "Example N. Title" header
        // matching asciidoctor's man page backend.  Untitled example blocks
        // still get .RS 4 / .RE indentation but no numbered header.
        if (block.context() == BlockContext::Example) {
            if (block.has_title()) {
                int n = ++counters_["example"];
                std::string header = "Example\\ \\&" + std::to_string(n) + ".\\ \\&";
                // Apply inline substitutions so _italic_ / *bold* markup in
                // example titles is rendered as troff font escapes.
                header += inline_subs(block.title(), doc);
                // Match asciidoctor's man page backend format:
                //   .B title   (bold title using .B macro)
                //   .br        (line break after title)
                //   .RS 4      (indent body by 4 chars)
                //   .sp        (space before first content line)
                //   ...content...
                //   .RE        (end indent)
                //   .sp        (space after block)
                out << ".sp\n"
                    << ".B " << header << "\n"
                    << ".br\n"
                    << ".RS 4\n";
            } else {
                out << ".sp\n"
                    << ".RS 4\n";
            }
            for (const auto& child : block.blocks()) {
                convert_block(*child, doc, out);
            }
            out << ".RE\n";
        } else if (block.context() == BlockContext::Quote ||
                   block.context() == BlockContext::Verse) {
            // Quote/verse blocks get .RS 3 / .RE indentation to match
            // asciidoctor's man page backend behaviour.
            if (block.has_title()) {
                out << ".sp\n"
                    << "\\fB" << inline_subs(block.title(), doc) << "\\fP\n";
            }
            out << ".RS 3\n";
            for (const auto& child : block.blocks()) {
                convert_block(*child, doc, out);
            }
            out << ".RE\n";
        } else {
            if (block.has_title()) {
                out << ".sp\n"
                    << "\\fB" << inline_subs(block.title(), doc) << "\\fP\n";
            }
            for (const auto& child : block.blocks()) {
                convert_block(*child, doc, out);
            }
        }
    }

    // ── Unordered list ────────────────────────────────────────────────────────

    void convert_ulist(const List& list, const Document& doc,
                       OutputBuffer& out) const {
        if (list.has_title()) {
            out << ".sp\n"
                << "\\fB" << troff_escape(list.title()) << "\\fP\n";
        }
        // Each item gets its own .sp .RS 4 ... .RE block with proper
        // .ie n (nroff bullet) / .el (troff IP bullet) conditional,
        // matching DocBook XSL output for <itemizedlist> items.
        for (const auto& item : list.items()) {
            out << ".sp\n"
                << ".RS 4\n"
                << ".ie n \\{\\\n"
                << "\\h'-04'\\(bu\\h'+03'\\c\n"
                << ".\\}\n"
                << ".el \\{\\\n"
                << ".sp -1\n"
                << ".IP \\(bu 2.3\n"
                << ".\\}\n";
            out << inline_subs(item->source(), doc) << '\n';
            // Sub-blocks
            for (const auto& child : item->blocks()) {
                convert_block(*child, doc, out);
            }
            out << ".RE\n";
        }
    }

    // ── Ordered list ──────────────────────────────────────────────────────────

    void convert_olist(const List& list, const Document& doc,
                       OutputBuffer& out) const {
        if (list.has_title()) {
            out << ".sp\n"
                << "\\fB" << troff_escape(list.title()) << "\\fP\n";
        }
        int n = 1;
        // Determine the number of digits in the largest label so the
        // horizontal offset (\h'-04' for 1-char, '-05' for 2-char, etc.)
        // can be set correctly – matching asciidoctor's manpage backend.
        // For simplicity, always use 4 (handles lists up to ~99 items).
        for (const auto& item : list.items()) {
            std::string label = std::to_string(n) + ".";
            // Asciidoctor uses per-item .RS 4/.RE with a conditional:
            //   .ie n  \h'-04' N.\h'+01'\c   (nroff: manual horizontal position)
            //   .el    .IP " N." 4.2          (troff: use IP macro)
            // This produces clean, properly-wrapped numbered list output in
            // both nroff (man page viewers) and troff (typeset output).
            out << ".sp\n"
                << ".RS 4\n"
                << ".ie n \\{\\\n"
                << "\\h'-04'" << " " << label << "\\h'+01'\\c\n"
                << ".\\}\n"
                << ".el \\{\\\n"
                << ".  sp -1\n"
                << ".  IP \" " << label << "\" 4.2\n"
                << ".\\}\n"
                << inline_subs(item->source(), doc) << '\n';
            for (const auto& child : item->blocks()) {
                convert_block(*child, doc, out);
            }
            out << ".RE\n";
            ++n;
        }
    }

    // ── Description list ──────────────────────────────────────────────────────

    void convert_dlist(const List& list, const Document& doc,
                       OutputBuffer& out) const {
        if (list.has_title()) {
            out << ".sp\n"
                << "\\fB" << troff_escape(list.title()) << "\\fP\n";
        }
        const auto& items = list.items();
        for (std::size_t idx = 0; idx < items.size(); ++idx) {
            const auto& item = *items[idx];
            // Empty-term items (generated e.g. by db2adoc for multi-variant
            // synopsis blocks) carry no visible term.  If the body is also
            // empty there is nothing to render; skip the item entirely.
            // If there IS body content but no term, emit it as a .PP block
            // so the content appears without a spurious empty-bold tag line.
            bool term_empty = item.term().empty();
            if (term_empty) {
                bool has_content = !item.source().empty() || !item.blocks().empty();
                if (!has_content) { continue; }
                // Body without a term: just emit the content as paragraphs
                if (!item.source().empty()) {
                    out << ".sp\n" << inline_subs(item.source(), doc) << '\n';
                }
                for (const auto& child : item.blocks()) {
                    convert_block(*child, doc, out);
                }
                continue;
            }

            // Compact list behavior (matching asciidoctor's man page backend):
            // When one or more consecutive items have completely empty bodies
            // (no source text and no child blocks), they are joined with ", "
            // onto the same term line as the next item that has content.
            // Collect the leading empty-body items into a single term string.
            std::string combined_term = inline_subs(item.term(), doc);
            std::size_t lead = idx;
            while (item.source().empty() && item.blocks().empty() &&
                   lead + 1 < items.size()) {
                // Peek at the next item
                const auto& nxt = *items[lead + 1];
                if (!nxt.term().empty()) {
                    ++lead;
                    combined_term += ", ";
                    combined_term += inline_subs(nxt.term(), doc);
                } else {
                    break;
                }
                // If the next item also has no body, keep collecting
                if (!nxt.source().empty() || !nxt.blocks().empty()) {
                    break;
                }
            }
            // Advance the outer loop index to skip items we consumed above
            idx = lead;
            const auto& body_item = *items[idx];

            // Emit the term via inline_subs so that explicit bold/italic markup
            // (*term*) is handled correctly.  Do NOT auto-add \fB...\fP for
            // plain terms – asciidoctor only applies the markup that is
            // explicitly present in the source.
            // Use DocBook-style .PP term .RS 4 body .RE instead of .TP, so that
            // the term appears on its own line with the body indented below it.
            //
            // Append \fR after the term to unconditionally reset to Roman font.
            // Without this, a term with nested markup (e.g. *-p _x y z_*)
            // generates \fB...\fI...\fP\fP where groff tracks only one previous-
            // font slot: the second \fP restores to italic instead of Roman,
            // bleeding italic into all body text that follows.  \fR is harmless
            // when the term has no markup or single-level markup (already Roman
            // at that point).  This matches DocBook XSL's behaviour of always
            // ending term lines with an explicit \fR.
            out << ".sp\n";
            out << combined_term << "\\fR\n";
            out << ".RS 4\n";
            if (!body_item.source().empty()) {
                out << inline_subs(body_item.source(), doc) << '\n';
            }
            // Render child blocks.  Tables and nested dlist blocks must be
            // rendered OUTSIDE the .RS 4 indentation to appear at the standard
            // paragraph level (matching asciidoctor's man page backend behavior).
            // For each such child, close the .RS 4, render, and reopen .RS 4 for
            // subsequent non-table/non-dlist children.
            bool in_rs4 = true;
            for (const auto& child : body_item.blocks()) {
                bool outside_rs4 = (child->context() == BlockContext::Table ||
                                    child->context() == BlockContext::Dlist);
                if (outside_rs4) {
                    if (in_rs4) { out << ".RE\n"; in_rs4 = false; }
                    convert_block(*child, doc, out);
                } else {
                    if (!in_rs4) { out << ".RS 4\n"; in_rs4 = true; }
                    convert_block(*child, doc, out);
                }
            }
            if (in_rs4) {
                out << ".RE\n";
            }
        }
    }

    // ── Table ─────────────────────────────────────────────────────────────────
    //
    // Renders an AsciiDoc table using the tbl(1) preprocessor macros, following
    // the same conventions as the upstream Asciidoctor Ruby manpage converter:
    //   https://github.com/asciidoctor/asciidoctor (lib/asciidoctor/converter/manpage.rb)
    //
    // Structure emitted:
    //
    //   .TS
    //   allbox tab(:);
    //   [header format spec].    ← only when header rows present
    //   T{...T}:T{...T}          ← header row cells
    //   .T&                      ← separator between header and body format
    //   [body format spec].
    //   T{...T}:T{...T}          ← body (and footer) row cells
    //   .TE
    //   .sp
    //
    // The document header line already begins with '\"\ t to activate tbl(1).

    void convert_table(const Table& table, const Document& doc,
                       OutputBuffer& out) const {
        // Optional block title – asciidoctor prefixes with "Table N." numbering
        if (table.has_title()) {
            int tnum = ++counters_["table"];
            out << ".sp\n"
                << ".it 1 an-trap\n"
                << ".nr an-no-space-flag 1\n"
                << ".nr an-break-flag 1\n"
                << ".br\n"
                << ".B Table\\ \\&" << tnum << ".\\ \\&"
                << troff_escape(table.title()) << "\n";
        }

        // Determine number of columns from specs or first non-empty row
        std::size_t ncols = table.column_specs().size();
        if (ncols == 0) {
            for (const auto* sec : {&table.head_rows(),
                                    &table.body_rows(),
                                    &table.foot_rows()}) {
                if (!sec->empty()) { ncols = sec->front().cells().size(); break; }
            }
        }
        if (ncols == 0) { return; }

        out << ".TS\nallbox tab(:);\n";

        const auto& specs = table.column_specs();

        // Emit one tbl format spec line.
        // Column alignment is taken from column_specs; default is left ('l').
        // bold=true renders header/footer cells in bold (suffix 'B').
        auto emit_format = [&](bool bold) {
            for (std::size_t ci = 0; ci < ncols; ++ci) {
                if (ci > 0) out << ' ';
                char align = 'l';
                if (ci < specs.size() && !specs[ci].halign.empty()) {
                    char c = specs[ci].halign[0];
                    if (c == 'c' || c == 'r') align = c;
                }
                out << align << 't' << (bold ? "B" : "");
            }
            out << ".\n";
        };

        // Emit one table row; cells are separated by ':' and wrapped in T{...T}.
        // Matching asciidoctor's man page backend, add .sp before each cell's
        // content so the text has proper vertical spacing within tbl output.
        auto emit_row = [&](const TableRow& row) {
            for (std::size_t ci = 0; ci < ncols; ++ci) {
                out << "T{\n";
                if (ci < row.cells().size()) {
                    const std::string& cell_src = row.cells()[ci]->source();
                    if (!cell_src.empty()) {
                        out << ".sp\n";
                        out << inline_subs(cell_src, doc);
                    }
                }
                out << "\nT}" << (ci + 1 < ncols ? ":" : "\n");
            }
        };

        const bool has_header = !table.head_rows().empty();

        // Asciidoctor man page backend uses a single format line for all rows
        // (no .T& switch between header and body).  This produces consistent
        // rendering across groff versions and matches the expected tbl output.
        emit_format(false);
        if (has_header) {
            for (const auto& row : table.head_rows()) { emit_row(row); }
        }

        for (const auto& row : table.body_rows()) { emit_row(row); }
        for (const auto& row : table.foot_rows()) { emit_row(row); }

        out << ".TE\n.sp\n";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Free function convenience wrapper
// ─────────────────────────────────────────────────────────────────────────────

/// Convert a parsed Document to a troff man page string.
[[nodiscard]] inline std::string convert_to_manpage(const Document& doc) {
    return ManpageConverter{}.convert(doc);
}

} // namespace asciiquack
