/// @file pdf.hpp
/// @brief PDF converter for asciiquack.
///
/// Walks the Document AST and produces a PDF byte string using minipdf.hpp.
/// Supports Letter (8.5"×11") and A4 page sizes; outputs documents with
/// correct heading hierarchy, body text, bullet and numbered lists, code
/// blocks, admonition blocks, inline bold / italic / monospace text, and
/// basic horizontal rules.
///
/// Custom TrueType fonts can be specified per-style via the FontSet struct
/// (attributes: pdf-font, pdf-font-bold, pdf-font-italic, pdf-font-bold-italic,
/// pdf-font-mono, pdf-font-mono-bold).  When a style has no custom font the
/// corresponding PDF base-14 fallback (Helvetica family / Courier) is used.
///
/// Usage:
///   auto doc = Parser::parse_string(src, opts);
///   std::string pdf_bytes = asciiquack::convert_to_pdf(*doc);             // Letter, Helvetica
///   std::string pdf_bytes = asciiquack::convert_to_pdf(*doc, true);       // A4
///   std::string pdf_bytes = asciiquack::convert_to_pdf(*doc, false,
///                               "/usr/share/fonts/myfont.ttf");  // custom body font
///
///   asciiquack::FontSet fs;
///   fs.regular    = "/path/to/NotoSans-Regular.ttf";
///   fs.bold       = "/path/to/NotoSans-Bold.ttf";
///   fs.italic     = "/path/to/NotoSans-Italic.ttf";
///   fs.bold_italic = "/path/to/NotoSans-BoldItalic.ttf";
///   fs.mono       = "/path/to/NotoSansMono-Regular.ttf";
///   fs.mono_bold  = "/path/to/NotoSansMono-Bold.ttf";
///   std::string pdf_bytes = asciiquack::convert_to_pdf(*doc, false, fs);

#pragma once

#include "document.hpp"
#include "minipdf.hpp"
#include "substitutors.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace asciiquack {

// ─────────────────────────────────────────────────────────────────────────────
// Font set – paths to optional TrueType fonts for each style slot
// ─────────────────────────────────────────────────────────────────────────────

/// Paths to TrueType (.ttf) font files for each text style used in the PDF
/// output.  Leave any field empty to fall back to the corresponding PDF
/// base-14 font (Helvetica family for body text, Courier for monospace).
struct FontSet {
    std::string regular;     ///< pdf-font              – body text (F1)
    std::string bold;        ///< pdf-font-bold         – bold text (F2)
    std::string italic;      ///< pdf-font-italic       – italic text (F3)
    std::string bold_italic; ///< pdf-font-bold-italic  – bold-italic text (F4)
    std::string mono;        ///< pdf-font-mono         – monospace text (F5)
    std::string mono_bold;   ///< pdf-font-mono-bold    – bold monospace text (F6)
};

// ─────────────────────────────────────────────────────────────────────────────
// Inline text spans
// ─────────────────────────────────────────────────────────────────────────────

struct TextSpan {
    std::string        text;
    minipdf::FontStyle style = minipdf::FontStyle::Regular;
    float              size  = 11.0f;  ///< font size in points (0 = use context default)
};

/// Parse a dimension string (e.g. "3.9in", "150px", "5cm", "50%") and
/// return the equivalent value in PDF points (1 pt = 1/72 inch).
/// When no unit suffix is recognised the value is returned as-is (assumed to
/// already be in points).  A content-area width @p content_w (in points) is
/// required for percentage values.  Returns 0 on parse failure.
static float parse_dimension_pts(const std::string& s, float content_w = 468.0f) {
    if (s.empty()) { return 0.0f; }
    std::size_t end = 0;
    float value = 0.0f;
    try { value = std::stof(s, &end); }
    catch (...) { return 0.0f; }
    if (value <= 0.0f) { return 0.0f; }
    std::string unit = s.substr(end);
    // Trim leading whitespace from unit string
    std::size_t us = unit.find_first_not_of(' ');
    if (us != std::string::npos) { unit = unit.substr(us); }
    if (unit == "in")  { return value * 72.0f; }
    if (unit == "cm")  { return value * 72.0f / 2.54f; }
    if (unit == "mm")  { return value * 72.0f / 25.4f; }
    if (unit == "px")  { return value * 72.0f / 96.0f; }  // 96 dpi screen
    if (unit == "pt")  { return value; }
    if (unit == "pc")  { return value * 12.0f; }           // 1 pica = 12 pt
    if (unit == "%")   { return value * content_w / 100.0f; }
    if (unit.empty())  { return value; }                   // bare number → points
    return 0.0f;  // unrecognised unit → auto
}

/// Apply typographic substitutions to plain text for PDF output.
///
/// This is the PDF-specific equivalent of sub_replacements() from
/// substitutors.hpp.  It operates on the raw source text (not HTML-encoded)
/// and outputs WinAnsiEncoding single-byte characters for the typographic
/// symbols, which is the encoding used by all fonts in minipdf.hpp.
///
/// Applied substitutions (WinAnsiEncoding byte values):
///   " -- " / "-- " / " --"  →  em dash (0x97, U+2014)
///   "..."                    →  ellipsis (0x85, U+2026)
///   "(C)"                    →  copyright © (0xA9)
///   "(R)"                    →  registered ® (0xAE)
///   "(TM)"                   →  trademark ™ (0x99)
///   smart apostrophe         →  right single quote (0x92, U+2019)
[[nodiscard]] static std::string apply_pdf_typo_subs(const std::string& text) {
    std::string out = text;

    auto replace_all = [](std::string& s,
                          const std::string_view from, const std::string_view to) {
        std::size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    };

    // Em dash: " -- " interior, "-- " at start, " --" at end, and "word--word".
    replace_all(out, " -- ", " \x97 ");
    if (out.size() >= 3 &&
        out[0] == '-' && out[1] == '-' && out[2] == ' ') {
        out = " \x97 " + out.substr(3);
    }
    {
        const std::size_t sz = out.size();
        if (sz >= 3 &&
            out[sz-3] == ' ' && out[sz-2] == '-' && out[sz-1] == '-') {
            out.resize(sz - 3);
            out += " \x97";
        }
    }
    // "word--word" → "word—word" (no surrounding spaces, both sides alphanumeric).
    // This matches asciidoc's unconstrained em-dash substitution.
    {
        std::string result;
        result.reserve(out.size());
        const std::size_t sz = out.size();
        for (std::size_t i = 0; i < sz; ++i) {
            if (i + 1 < sz && out[i] == '-' && out[i+1] == '-') {
                // Only convert if both neighbours are strict word characters
                // (alphanumeric or underscore), matching asciidoc's rule exactly.
                bool prev_word = (i > 0) &&
                    (std::isalnum(static_cast<unsigned char>(out[i-1])) ||
                     out[i-1] == '_');
                bool next_word = (i + 2 < sz) &&
                    (std::isalnum(static_cast<unsigned char>(out[i+2])) ||
                     out[i+2] == '_');
                if (prev_word && next_word) {
                    result += '\x97';
                    ++i;  // skip the second '-'
                    continue;
                }
            }
            result += out[i];
        }
        out = std::move(result);
    }

    // Ellipsis (only in non-verbatim context; applied here before entity encoding)
    replace_all(out, "...", "\x85");

    // Copyright / Registered / Trademark
    replace_all(out, "(C)",  "\xa9");
    replace_all(out, "(R)",  "\xae");
    replace_all(out, "(TM)", "\x99");

    // Smart apostrophe: word'word  →  word'word (WinAnsi 0x92 = U+2019)
    {
        std::string result;
        result.reserve(out.size());
        const std::size_t sz = out.size();
        for (std::size_t i = 0; i < sz; ++i) {
            if (out[i] == '\'' && i > 0 && i + 1 < sz) {
                const char prev = out[i-1];
                const char next = out[i+1];
                const bool pw = std::isalnum(static_cast<unsigned char>(prev)) || prev == '_';
                const bool nw = std::isalnum(static_cast<unsigned char>(next)) || next == '_';
                if (pw && nw) { result += '\x92'; continue; }
            }
            result += out[i];
        }
        out = std::move(result);
    }

    return out;
}

/// Strip AsciiDoc markup that we cannot render (links, macros, etc.) and
/// leave the visible text only.  This is a best-effort plain-text extractor
/// used so that the raw AsciiDoc source is never mis-rendered as markup.
/// HTML entities (e.g. from attribute values) are decoded to WinAnsiEncoding
/// single-byte characters on output.
static std::string strip_markup(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    const std::size_t n = s.size();

    // Helper: try to match a xref at position pos given the open/close tokens.
    // Returns true and advances pos past the xref, appending display text to out.
    // open/close are the literal delimiters (e.g. "<<" / ">>" or "&lt;&lt;" / "&gt;&gt;").
    auto try_xref = [&](const char* open_tok,  std::size_t open_len,
                        const char* close_tok, std::size_t close_len) -> bool {
        if (s.compare(i, open_len, open_tok) != 0) { return false; }
        auto close_pos = s.find(close_tok, i + open_len);
        if (close_pos == std::string::npos) { return false; }
        std::string inner = s.substr(i + open_len, close_pos - (i + open_len));
        auto comma = inner.find(',');
        std::string anchor_part = (comma != std::string::npos)
                                  ? inner.substr(0, comma) : inner;
        // Anchor must not contain spaces or newlines (guards against false matches)
        if (anchor_part.find(' ') != std::string::npos ||
            anchor_part.find('\n') != std::string::npos) { return false; }
        if (comma != std::string::npos) {
            // <<anchor,text>> → emit the display text (may itself carry markup)
            out += strip_markup(inner.substr(comma + 1));
        } else {
            // <<anchor>> → emit [anchor]
            out += '[';
            out += inner;
            out += ']';
        }
        i = close_pos + close_len;
        return true;
    };

    while (i < n) {
        // Cross-reference: raw form <<anchor,text>> and HTML-encoded &lt;&lt;...&gt;&gt;
        if (try_xref("<<",       2, ">>",       2)) { continue; }
        if (try_xref("&lt;&lt;", 8, "&gt;&gt;", 8)) { continue; }

        // link:url[label] or http(s)://url[label]
        if (s.compare(i, 4, "http") == 0 || s.compare(i, 5, "link:") == 0) {
            auto ob = s.find('[', i);
            auto cb = (ob != std::string::npos) ? s.find(']', ob) : std::string::npos;
            if (ob != std::string::npos && cb != std::string::npos) {
                // Use the link label if non-empty, else the bare URL
                std::string label = s.substr(ob + 1, cb - ob - 1);
                if (!label.empty()) {
                    out += label;
                } else {
                    auto url_end = ob;
                    auto url_start = (s[i] == 'l') ? i + 5 : i;
                    out += s.substr(url_start, url_end - url_start);
                }
                i = cb + 1;
                continue;
            }
        }
        // kbd:[key], btn:[label], menu:X[Y] – drop markers, keep content
        if (s.compare(i, 4, "kbd:") == 0 ||
            s.compare(i, 4, "btn:") == 0 ||
            s.compare(i, 5, "menu:") == 0) {
            auto ob = s.find('[', i);
            auto cb = (ob != std::string::npos) ? s.find(']', ob) : std::string::npos;
            if (ob != std::string::npos && cb != std::string::npos) {
                out += s.substr(ob + 1, cb - ob - 1);
                i = cb + 1;
                continue;
            }
        }
        // pass:[text] / pass:q[text] / pass:c[text] – extract text, strip any HTML tags
        if (s.compare(i, 5, "pass:") == 0) {
            auto ob = s.find('[', i);
            auto cb = (ob != std::string::npos) ? s.find(']', ob) : std::string::npos;
            if (ob != std::string::npos && cb != std::string::npos) {
                std::string inner = s.substr(ob + 1, cb - ob - 1);
                // Strip any HTML/XML tags from the passthrough content
                std::string plain;
                plain.reserve(inner.size());
                bool in_tag = false;
                for (char c : inner) {
                    if (c == '<') { in_tag = true; }
                    else if (c == '>') { in_tag = false; }
                    else if (!in_tag) { plain += c; }
                }
                out += plain;
                i = cb + 1;
                continue;
            }
        }
        // image:target[alt] – use alt text; skip attribute-only content
        if (s.compare(i, 6, "image:") == 0) {
            auto ob = s.find('[', i);
            auto cb = (ob != std::string::npos) ? s.find(']', ob) : std::string::npos;
            if (ob != std::string::npos && cb != std::string::npos) {
                std::string raw_alt = s.substr(ob + 1, cb - ob - 1);
                // Extract first positional param (before first comma, if any)
                auto comma = raw_alt.find(',');
                std::string alt = (comma != std::string::npos)
                                  ? raw_alt.substr(0, comma)
                                  : raw_alt;
                // Trim leading/trailing whitespace
                auto lf = alt.find_first_not_of(" \t");
                if (lf != std::string::npos) { alt = alt.substr(lf); }
                auto rt = alt.find_last_not_of(" \t");
                if (rt != std::string::npos) { alt = alt.substr(0, rt + 1); }
                // Suppress if it looks like a pure attribute (e.g. "width=64")
                // A real alt text does not look like "name=value"
                bool is_attr = (!alt.empty() && alt.find('=') != std::string::npos
                                && alt.find(' ') == std::string::npos);
                if (!alt.empty() && !is_attr) { out += "[" + alt + "]"; }
                i = cb + 1;
                continue;
            }
        }
        // footnote:[text] – drop footnote markers
        if (s.compare(i, 9, "footnote:") == 0) {
            auto ob = s.find('[', i);
            auto cb = (ob != std::string::npos) ? s.find(']', ob) : std::string::npos;
            if (ob != std::string::npos && cb != std::string::npos) {
                i = cb + 1;
                continue;
            }
        }
        // Superscript ^text^ – keep text, drop markers
        if (s[i] == '^') {
            auto end = s.find('^', i + 1);
            if (end != std::string::npos && end > i + 1) {
                out += s.substr(i + 1, end - i - 1);
                i = end + 1;
                continue;
            }
        }
        // Subscript ~text~ – keep text, drop markers
        if (s[i] == '~') {
            auto end = s.find('~', i + 1);
            if (end != std::string::npos && end > i + 1) {
                out += s.substr(i + 1, end - i - 1);
                i = end + 1;
                continue;
            }
        }
        // Highlight (mark) #text# – keep text, drop markers (constrained only)
        if (s[i] == '#' &&
            (i == 0 || !std::isalnum(static_cast<unsigned char>(s[i-1])))) {
            auto end = s.find('#', i + 1);
            if (end != std::string::npos && end > i + 1) {
                out += s.substr(i + 1, end - i - 1);
                i = end + 1;
                continue;
            }
        }
        out += s[i++];
    }

    // Decode HTML entities produced by sub_attributes() so that the PDF
    // writer receives plain characters, not HTML escapes.
    // Characters above 0x7F are encoded as single WinAnsiEncoding bytes, which
    // is the encoding used by all fonts in the PDF output (both base-14 and
    // embedded TrueType).
    auto replace_all = [](std::string& str,
                          const std::string& from, const std::string& to) {
        std::size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.size(), to);
            pos += to.size();
        }
    };
    replace_all(out, "&amp;",  "&");
    replace_all(out, "&lt;",   "<");
    replace_all(out, "&gt;",   ">");
    replace_all(out, "&quot;", "\"");
    replace_all(out, "&apos;", "'");
    // WinAnsiEncoding single-byte mappings for typographic characters:
    replace_all(out, "&#8212;", "\x97");   // em dash U+2014 (WinAnsi 0x97)
    replace_all(out, "&#8230;", "\x85");   // ellipsis U+2026 (WinAnsi 0x85)
    replace_all(out, "&#8201;", " ");      // thin space → regular space
    replace_all(out, "&#8203;", "");       // zero-width space → remove
    replace_all(out, "&#169;",  "\xa9");   // copyright © (Latin-1 0xA9)
    replace_all(out, "&#174;",  "\xae");   // registered ® (Latin-1 0xAE)
    replace_all(out, "&#8482;", "\x99");   // trademark ™ (WinAnsi 0x99)
    replace_all(out, "&#8217;", "\x92");   // right apostrophe ' (WinAnsi 0x92)
    replace_all(out, "&#8594;", "->");     // right arrow → ASCII fallback
    replace_all(out, "&#8592;", "<-");     // left arrow → ASCII fallback
    replace_all(out, "&#8658;", "=>");     // double right arrow → ASCII fallback
    replace_all(out, "&#8656;", "<=");     // double left arrow → ASCII fallback
    // Convert raw UTF-8 multibyte sequences to WinAnsiEncoding single bytes.
    // These appear when source files already contain typographic Unicode chars.
    replace_all(out, "\xe2\x80\x99", "\x92");  // U+2019 ' → right single quote
    replace_all(out, "\xe2\x80\x98", "\x91");  // U+2018 ' → left single quote
    replace_all(out, "\xe2\x80\x9c", "\x93");  // U+201C " → left double quote
    replace_all(out, "\xe2\x80\x9d", "\x94");  // U+201D " → right double quote
    replace_all(out, "\xe2\x80\x93", "\x96");  // U+2013 – → en dash
    replace_all(out, "\xe2\x80\x94", "\x97");  // U+2014 — → em dash
    replace_all(out, "\xe2\x80\xa6", "\x85");  // U+2026 … → ellipsis
    replace_all(out, "\xe2\x80\xa2", "\x95");  // U+2022 • → bullet
    replace_all(out, "\xe2\x84\xa2", "\x99");  // U+2122 ™ → trademark
    replace_all(out, "\xc2\xa0", " ");          // U+00A0 → regular space
    replace_all(out, "\xc2\xb0", "\xb0");       // U+00B0 ° → degree (same byte)
    replace_all(out, "\xc2\xa9", "\xa9");       // U+00A9 © → copyright (same byte)
    replace_all(out, "\xc2\xae", "\xae");       // U+00AE ® → registered (same byte)
    replace_all(out, "\xc2\xb5", "\xb5");       // U+00B5 µ → micro (same byte)
    replace_all(out, "\xc2\xbc", "\xbc");       // U+00BC ¼ → one quarter (same byte)
    replace_all(out, "\xc2\xbd", "\xbd");       // U+00BD ½ → one half (same byte)
    replace_all(out, "\xc2\xbe", "\xbe");       // U+00BE ¾ → three quarters (same byte)
    replace_all(out, "\xc3\x97", "\xd7");       // U+00D7 × → multiplication (same byte)
    replace_all(out, "\xc3\xb7", "\xf7");       // U+00F7 ÷ → division (same byte)
    replace_all(out, "\xc2\xb1", "\xb1");       // U+00B1 ± → plus-minus (same byte)
    replace_all(out, "\xc2\xb7", "\xb7");       // U+00B7 · → middle dot (same byte)
    return out;
}

/// Parse a plain-text string (after markup has been stripped) into a vector
/// of TextSpans, interpreting *bold*, _italic_, `mono`, **bold**, __italic__,
/// ``mono`` inline markup.  Nested markup (e.g. *_bold-italic_*) is handled
/// by recursively parsing the inner content with the combined style.
static std::vector<TextSpan> parse_spans(const std::string& raw,
                                         minipdf::FontStyle base_style
                                             = minipdf::FontStyle::Regular) {
    std::string text = strip_markup(raw);

    std::vector<TextSpan> spans;
    std::string current;
    std::size_t i = 0;
    const std::size_t n = text.size();

    // Helpers to flush accumulated plain text
    auto flush = [&]() {
        if (!current.empty()) {
            spans.push_back({current, base_style});
            current.clear();
        }
    };

    // Determine bold/oblique variant of base_style
    auto bold_of = [&]() {
        switch (base_style) {
            case minipdf::FontStyle::Oblique:
                return minipdf::FontStyle::BoldOblique;
            default:
                return minipdf::FontStyle::Bold;
        }
    };
    auto italic_of = [&]() {
        switch (base_style) {
            case minipdf::FontStyle::Bold:
                return minipdf::FontStyle::BoldOblique;
            default:
                return minipdf::FontStyle::Oblique;
        }
    };

    // Append a recursively-parsed inner span (handles nested markup).
    // Mono spans are NOT recursed into (they are verbatim).
    auto append_inner = [&](const std::string& inner, minipdf::FontStyle style) {
        auto inner_spans = parse_spans(inner, style);
        spans.insert(spans.end(), inner_spans.begin(), inner_spans.end());
    };

    while (i < n) {
        // ── Unconstrained bold: **...**
        if (i + 1 < n && text[i] == '*' && text[i+1] == '*') {
            auto end = text.find("**", i + 2);
            if (end != std::string::npos) {
                flush();
                append_inner(text.substr(i + 2, end - i - 2), bold_of());
                i = end + 2;
                continue;
            }
        }
        // ── Constrained bold: *word*
        if (text[i] == '*' &&
            (i == 0 || !std::isalnum(static_cast<unsigned char>(text[i-1])))) {
            auto end = text.find('*', i + 1);
            if (end != std::string::npos && end > i + 1) {
                flush();
                append_inner(text.substr(i + 1, end - i - 1), bold_of());
                i = end + 1;
                continue;
            }
        }
        // ── Unconstrained italic: __...__
        if (i + 1 < n && text[i] == '_' && text[i+1] == '_') {
            auto end = text.find("__", i + 2);
            if (end != std::string::npos) {
                flush();
                append_inner(text.substr(i + 2, end - i - 2), italic_of());
                i = end + 2;
                continue;
            }
        }
        // ── Constrained italic: _word_
        if (text[i] == '_' &&
            (i == 0 || !std::isalnum(static_cast<unsigned char>(text[i-1])))) {
            auto end = text.find('_', i + 1);
            if (end != std::string::npos && end > i + 1) {
                flush();
                append_inner(text.substr(i + 1, end - i - 1), italic_of());
                i = end + 1;
                continue;
            }
        }
        // ── Unconstrained mono: ``...``
        if (i + 1 < n && text[i] == '`' && text[i+1] == '`') {
            auto end = text.find("``", i + 2);
            if (end != std::string::npos) {
                flush();
                spans.push_back({text.substr(i + 2, end - i - 2),
                                  minipdf::FontStyle::Mono});
                i = end + 2;
                continue;
            }
        }
        // ── Constrained mono: `word`
        if (text[i] == '`' &&
            (i == 0 || !std::isalnum(static_cast<unsigned char>(text[i-1])))) {
            auto end = text.find('`', i + 1);
            if (end != std::string::npos && end > i + 1) {
                flush();
                spans.push_back({text.substr(i + 1, end - i - 1),
                                  minipdf::FontStyle::Mono});
                i = end + 1;
                continue;
            }
        }
        // ── Legacy constrained passthrough: +word+ (renders as plain text in PDF)
        if (text[i] == '+' &&
            (i == 0 || !std::isalnum(static_cast<unsigned char>(text[i-1])))) {
            auto end = text.find('+', i + 1);
            if (end != std::string::npos && end > i + 1 &&
                !std::isspace(static_cast<unsigned char>(text[i+1]))) {
                flush();
                // Render passthrough content as mono (matches asciidoc legacy behaviour)
                spans.push_back({text.substr(i + 1, end - i - 1),
                                  minipdf::FontStyle::Mono});
                i = end + 1;
                continue;
            }
        }
        current += text[i++];
    }
    flush();
    return spans;
}

/// Collapse spans with consecutive identical styles into a single span.
static std::vector<TextSpan> merge_spans(std::vector<TextSpan> spans) {
    std::vector<TextSpan> out;
    for (auto& sp : spans) {
        if (!out.empty() && out.back().style == sp.style) {
            out.back().text += sp.text;
        } else {
            out.push_back(std::move(sp));
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// PdfLayout – stateful text-flow engine
//
// Manages the current position on the page and automatically starts new pages
// when there is insufficient vertical space.
// ─────────────────────────────────────────────────────────────────────────────

class PdfLayout {
public:
    // ── Page geometry (points) ────────────────────────────────────────────────
    static constexpr float MARGIN_LEFT   = 72.0f;
    static constexpr float MARGIN_RIGHT  = 72.0f;
    static constexpr float MARGIN_TOP    = 72.0f;
    static constexpr float MARGIN_BOTTOM = 72.0f;

    // ── Default typography ────────────────────────────────────────────────────
    static constexpr float BODY_SIZE          = 11.0f;
    static constexpr float CODE_SIZE          = 10.0f;
    static constexpr float LINE_RATIO         = 1.35f;   ///< line height = size × ratio
    static constexpr float CODE_RIGHT_PADDING = 4.0f;    ///< right padding inside code block (pt)

    explicit PdfLayout(minipdf::Document& doc) : doc_(doc) {
        content_w_ = doc.page_width() - MARGIN_LEFT - MARGIN_RIGHT;
        new_page();
    }

    /// Return the content area width in points (page width minus both margins).
    float content_width() const { return content_w_; }

    // ── Page management ───────────────────────────────────────────────────────

    void new_page() {
        page_ = &doc_.new_page();
        cursor_y_ = doc_.page_height() - MARGIN_TOP;
    }

    /// Ensure at least @p pts vertical space remains; if not, break page.
    void ensure_space(float pts) {
        if (cursor_y_ - pts < MARGIN_BOTTOM) {
            new_page();
        }
    }

    /// Advance cursor down by @p pts (vertical skip).
    void skip(float pts) {
        cursor_y_ -= pts;
        if (cursor_y_ < MARGIN_BOTTOM) { new_page(); }
    }

    // ── Word-wrapped paragraph ────────────────────────────────────────────────

    /// Render @p spans word-wrapped into lines within the content width minus
    /// @p x_indent.  @p line_h is the per-line advance.
    void wrap_spans(const std::vector<TextSpan>& spans,
                    float line_h,
                    float x_indent = 0.0f) {
        if (spans.empty()) { return; }
        float avail = content_w_ - x_indent;

        // Tokenise the spans into words, preserving span metadata.
        // A "word" is a run of non-space characters in a single span.
        struct Word {
            std::string        text;
            minipdf::FontStyle style = minipdf::FontStyle::Regular;
            float              size  = BODY_SIZE;
            bool               space_before = false;  ///< was preceded by space
        };

        // Helper: return true if this word should attach to the preceding word
        // without a space (i.e. it starts with closing punctuation).
        // This handles "_italic_." → "italic." rather than "italic ."
        auto no_space_before = [](const Word& w) -> bool {
            if (w.text.empty() || w.space_before) { return false; }
            const char c = w.text[0];
            return c == '.' || c == ',' || c == ';' || c == ':' ||
                   c == '!' || c == '?' || c == ')' || c == ']' ||
                   c == '\x92';  // WinAnsi right apostrophe (')
        };

        std::vector<Word> words;
        for (const auto& sp : spans) {
            std::istringstream ss(sp.text);
            std::string tok;
            bool first = true;
            while (std::getline(ss, tok, ' ')) {
                if (tok.empty()) {
                    first = false;
                    continue;
                }
                Word w;
                w.text  = tok;
                w.style = sp.style;
                w.size  = sp.size;
                w.space_before = !first;
                words.push_back(w);
                first = false;
            }
        }

        if (words.empty()) { return; }

        // Build lines
        struct Line {
            std::vector<Word> words;
            float             total_w = 0.0f;
        };

        float space_w = tw(" ", minipdf::FontStyle::Regular, BODY_SIZE);
        std::vector<Line> lines;
        lines.push_back({});

        for (const auto& w : words) {
            float ww = tw(w.text, w.style, w.size);
            // Punctuation-only tokens attach directly to the preceding word.
            bool attach = !lines.back().words.empty() && no_space_before(w);
            float gap  = (lines.back().words.empty() || attach) ? 0.0f : space_w;
            float need = lines.back().words.empty() ? ww
                         : lines.back().total_w + gap + ww;

            if (need > avail && !lines.back().words.empty() && !attach) {
                lines.push_back({});
            }
            float add_w = lines.back().words.empty() ? ww : gap + ww;
            lines.back().total_w += add_w;
            lines.back().words.push_back(w);
        }

        // Render each line
        for (const auto& line : lines) {
            ensure_space(line_h);
            float x = MARGIN_LEFT + x_indent;
            bool  first_word = true;
            for (const auto& w : line.words) {
                if (!first_word && !no_space_before(w)) {
                    x += space_w;
                }
                page_->place_text(x, cursor_y_, w.style, w.size, w.text);
                x += tw(w.text, w.style, w.size);
                first_word = false;
            }
            cursor_y_ -= line_h;
        }
    }

    // ── Heading ───────────────────────────────────────────────────────────────

    /// Render a section heading at the given level (0 = document title).
    void heading(const std::string& title, int level) {
        static const float SIZES[] = {26.0f, 22.0f, 18.0f, 15.0f, 13.0f, 12.0f};
        float sz = SIZES[std::min(level, 5)];
        float lh = sz * LINE_RATIO;
        float bar_h = (level == 0) ? 3.0f : (level == 1) ? 1.5f : 0.0f;

        // Extra space before heading (except at very top of first page)
        float space_before = (cursor_y_ < doc_.page_height() - MARGIN_TOP - 2.0f)
                             ? sz * 1.2f : 0.0f;

        // For ruled headings the space consumed is:
        //   space_before + lh (one heading line) + bar_offset + bar_h + gap_below
        // gap_below must be large enough that body-text ascenders (≈ BODY_SIZE)
        // do not reach up into the bar.  A gap of BODY_SIZE * 1.5 is sufficient.
        const float gap_below = (level <= 1) ? BODY_SIZE * 1.5f : 2.0f;
        // bar_offset = distance from last heading line baseline to bar bottom
        // ≈ sz * 0.35 (just past heading descenders)
        const float bar_offset = sz * 0.35f;
        ensure_space(space_before + lh +
                     (bar_h > 0.0f ? bar_offset + bar_h + gap_below : gap_below));
        cursor_y_ -= space_before;

        auto spans = parse_spans(title, minipdf::FontStyle::Bold);
        // Force heading size onto all spans
        for (auto& sp : spans) { sp.size = sz; }
        spans = merge_spans(std::move(spans));
        wrap_spans(spans, lh, 0.0f);

        // Draw a coloured rule after the heading text.  The bar is placed just
        // below the last heading line's descenders, so it acts as a visual
        // separator between the heading block and the following body text.
        if (level <= 1) {
            // cursor_y_ is now at last_line_baseline - lh; recover last baseline
            float last_baseline = cursor_y_ + lh;
            float bar_bottom = last_baseline - bar_offset;
            float c = (level == 0) ? 0.3f : 0.6f;
            page_->fill_rect(MARGIN_LEFT, bar_bottom, content_w_, bar_h, c, c, c);
            // Advance cursor to below the bar with enough room for body text
            cursor_y_ = bar_bottom - bar_h - gap_below;
        } else {
            cursor_y_ -= gap_below;
        }
    }

    // ── Paragraph ─────────────────────────────────────────────────────────────

    void paragraph(const std::string& text, float x_indent = 0.0f) {
        paragraph_spans(parse_spans(text), x_indent);
    }

    void paragraph_spans(std::vector<TextSpan> spans, float x_indent = 0.0f) {
        spans = merge_spans(std::move(spans));
        float lh = BODY_SIZE * LINE_RATIO;
        wrap_spans(spans, lh, x_indent);
        cursor_y_ -= BODY_SIZE * 0.5f;  // paragraph spacing
    }

    // ── Code block ────────────────────────────────────────────────────────────

    void code_block(const std::string& source) {
        const float lh     = CODE_SIZE * LINE_RATIO;
        const float indent = 8.0f;
        const float avail_w = content_w_ - indent - CODE_RIGHT_PADDING;

        // Pre-process lines: break any line that exceeds avail_w into multiple
        // visual lines.  Continuation lines are indented by two extra spaces.
        // Prefer breaking at a space so tokens are not split across lines.
        auto split_line = [&](const std::string& line,
                               std::vector<std::string>& out) {
            if (tw(line, minipdf::FontStyle::Mono, CODE_SIZE) <= avail_w) {
                out.push_back(line);
                return;
            }
            std::string rest = line;
            bool first_seg = true;
            while (!rest.empty()) {
                // If the rest already fits, emit it as-is and stop.
                if (tw(rest, minipdf::FontStyle::Mono, CODE_SIZE) <= avail_w) {
                    out.push_back(rest);
                    rest.clear();
                    break;
                }
                // Find how many characters fit on this visual line.
                std::string seg = rest;
                while (!seg.empty() &&
                       tw(seg, minipdf::FontStyle::Mono, CODE_SIZE) > avail_w) {
                    seg.pop_back();
                }
                if (seg.empty()) {
                    // Single character is wider than avail (extremely rare).
                    seg += rest[0];
                    rest = rest.substr(1);
                } else {
                    // Prefer to break at a space that is within the segment.
                    // Look for the last space in seg (not including leading
                    // spaces on continuation lines).
                    std::size_t leader = first_seg ? 0u : 2u;
                    auto sp = seg.rfind(' ', seg.size() - 1);
                    if (sp != std::string::npos && sp > leader) {
                        seg  = rest.substr(0, sp);
                        rest = rest.substr(sp + 1);  // skip the break space
                    } else {
                        rest = rest.substr(seg.size());
                    }
                }
                out.push_back(seg);
                first_seg = false;
                if (!rest.empty()) { rest = "  " + rest; }
            }
        };

        std::vector<std::string> visual_lines;
        {
            std::istringstream ss(source);
            std::string line;
            while (std::getline(ss, line)) {
                // Handle CR+LF in case the source has Windows line endings
                if (!line.empty() && line.back() == '\r') { line.pop_back(); }
                split_line(line, visual_lines);
            }
        }

        const int n_lines = static_cast<int>(visual_lines.size());
        float block_h = static_cast<float>(n_lines) * lh + 8.0f;

        ensure_space(block_h);

        // Light grey background
        page_->fill_rect(MARGIN_LEFT - 4.0f,
                         cursor_y_ - block_h + lh * 0.25f,
                         content_w_ + 8.0f, block_h,
                         0.95f, 0.95f, 0.95f);

        cursor_y_ -= 4.0f;  // top padding

        for (const auto& seg : visual_lines) {
            ensure_space(lh);
            page_->place_text(MARGIN_LEFT + indent, cursor_y_,
                               minipdf::FontStyle::Mono, CODE_SIZE, seg);
            cursor_y_ -= lh;
        }
        // Gap below the code block must be large enough that the ascenders of
        // the following paragraph (≈ BODY_SIZE × 0.75) do not reach up into the
        // grey background rectangle.  The minimum required is ~8.9 pt; use
        // BODY_SIZE (11 pt) for a comfortable, visually consistent separation.
        cursor_y_ -= BODY_SIZE;
    }

    // ── Horizontal rule ───────────────────────────────────────────────────────

    void hrule() {
        // Leave room above the rule (4 pt) and enough room below so that
        // ascenders of the following text (≈ BODY_SIZE × 0.85) do not reach
        // up through the rule line.
        const float gap_below = BODY_SIZE * 0.85f;
        ensure_space(4.0f + gap_below);
        cursor_y_ -= 4.0f;
        page_->draw_hline(MARGIN_LEFT, cursor_y_,
                          MARGIN_LEFT + content_w_, 0.5f,
                          0.5f, 0.5f, 0.5f);
        cursor_y_ -= gap_below;
    }

    // ── Image block ───────────────────────────────────────────────────────────

    /// Embed a raster image (JPEG or PNG) at the current cursor position.
    ///
    /// The image is scaled to fit within the content area while preserving its
    /// aspect ratio.  When @p hint_w is non-zero it is used as the requested
    /// display width in points; otherwise the image fills the content width.
    /// @p hint_h is an optional height override (0 = maintain aspect ratio).
    ///
    /// If @p caption is non-empty it is rendered as a figure caption below the
    /// image, prefixed with an auto-incrementing "Figure N." label.
    ///
    /// When the image cannot be loaded (missing file, unsupported format) a
    /// plain-text placeholder is emitted instead.
    void image_block(const std::string& path,
                     float hint_w = 0.0f, float hint_h = 0.0f,
                     const std::string& caption = "") {
        auto img = minipdf::PdfImage::from_file(path);
        if (!img) {
            // Fall back to a text placeholder showing the path stem and caption.
            std::string stem = path;
            {
                auto sl = path.rfind('/');
                if (sl != std::string::npos) { stem = path.substr(sl + 1); }
            }
            paragraph("[image: " + stem + "]");
            if (!caption.empty()) {
                ++figure_counter_;
                figure_caption("Figure " + std::to_string(figure_counter_)
                               + ". " + caption);
            }
            return;
        }

        // Determine display dimensions
        float img_w = (img->width()  > 0) ? static_cast<float>(img->width())  : 1.0f;
        float img_h = (img->height() > 0) ? static_cast<float>(img->height()) : 1.0f;
        float aspect = img_h / img_w;

        float disp_w = (hint_w > 0.0f) ? hint_w : content_w_;
        disp_w = std::min(disp_w, content_w_);  // never overflow margin

        float disp_h = (hint_h > 0.0f) ? hint_h : disp_w * aspect;

        ensure_space(disp_h + BODY_SIZE);  // image height + one body line below
        if (disp_h > (cursor_y_ - MARGIN_BOTTOM)) {
            // Still too tall after a possible page break; clamp to page
            disp_h = cursor_y_ - MARGIN_BOTTOM - BODY_SIZE;
            if (disp_h <= 0.0f) { disp_h = BODY_SIZE; }
            disp_w = disp_h / aspect;
        }

        // Add image to document and place it
        std::string res = doc_.add_image(img);
        // PDF y is from the bottom; cursor_y_ is the TOP-LEFT origin in our model
        float img_y = cursor_y_ - disp_h;
        page_->place_image(MARGIN_LEFT, img_y, disp_w, disp_h, res);
        cursor_y_ = img_y - BODY_SIZE * 0.5f;  // small gap below image

        // Figure caption rendered below the image
        if (!caption.empty()) {
            ++figure_counter_;
            figure_caption("Figure " + std::to_string(figure_counter_)
                           + ". " + caption);
        }
    }

    // ── Page break ────────────────────────────────────────────────────────────

    void page_break() {
        new_page();
    }

    // ── Author / revision line ────────────────────────────────────────────────

    void meta_line(const std::string& text) {
        float lh = BODY_SIZE * LINE_RATIO;
        auto spans = parse_spans(text, minipdf::FontStyle::Oblique);
        for (auto& sp : spans) { sp.size = BODY_SIZE; }
        spans = merge_spans(std::move(spans));
        ensure_space(lh);
        wrap_spans(spans, lh, 0.0f);
    }

    // ── Admonition block ──────────────────────────────────────────────────────

    void admonition(const std::string& label, const std::string& body_text) {
        float lh = BODY_SIZE * LINE_RATIO;

        // Label (NOTE, TIP, etc.) in bold — no trailing colon, matching
        // asciidoctor's PDF output style.
        std::string lbl = label;
        for (char& c : lbl) {
            c = static_cast<char>(
                    std::toupper(static_cast<unsigned char>(c)));
        }

        // Indent body text past the label.  Computed dynamically so that long
        // labels like "IMPORTANT" don't overflow into the body text area.
        float indent = tw(lbl, minipdf::FontStyle::Bold, BODY_SIZE)
                       + BODY_SIZE;  ///< label width + one-em gap

        ensure_space(lh * 2.0f);

        page_->place_text(MARGIN_LEFT, cursor_y_,
                          minipdf::FontStyle::Bold, BODY_SIZE, lbl);

        // Body text indented
        paragraph(body_text, indent);
    }

    // ── Bullet list item ──────────────────────────────────────────────────────

    void bullet_item(const std::string& text, int depth = 0) {
        float lh    = BODY_SIZE * LINE_RATIO;
        float indent = static_cast<float>(depth) * 18.0f + 18.0f;

        ensure_space(lh);

        // Bullet character: use WinAnsiEncoding 0x95 = bullet (•)
        std::string bullet = (depth % 2 == 0) ? "\x95" : "-";
        page_->place_text(MARGIN_LEFT + indent - 12.0f, cursor_y_,
                          minipdf::FontStyle::Regular, BODY_SIZE, bullet);

        // Item text
        auto spans = parse_spans(text);
        for (auto& sp : spans) { sp.size = BODY_SIZE; }
        spans = merge_spans(std::move(spans));
        float saved_y = cursor_y_;
        wrap_spans(spans, lh, indent);

        // Restore any extra advance for multi-line items - already handled
        (void)saved_y;
    }

    // ── Ordered list item ─────────────────────────────────────────────────────

    // ── Ordered-list label helpers ────────────────────────────────────────────

    /// Convert @p n to a lowercase Roman numeral string (e.g. 1→"i", 4→"iv").
    static std::string to_lower_roman(int n) {
        static const struct { int v; const char* s; } tbl[] = {
            {1000,"m"},{900,"cm"},{500,"d"},{400,"cd"},{100,"c"},{90,"xc"},
            {50,"l"},{40,"xl"},{10,"x"},{9,"ix"},{5,"v"},{4,"iv"},{1,"i"}
        };
        std::string r;
        for (const auto& e : tbl) {
            while (n >= e.v) { r += e.s; n -= e.v; }
        }
        return r;
    }

    /// Generate an ordered-list label string for item @p number at @p depth
    /// using the asciidoctor auto-style rules when no explicit style is set.
    /// @p style overrides the depth-based choice (Arabic=always numbers, etc.).
    static std::string ordered_label(int number, int depth,
                                     OrderedListStyle style) {
        // Auto-select style from depth when not explicitly overridden
        if (style == OrderedListStyle::Arabic) {
            // Depth-based default (asciidoctor behaviour)
            switch (depth % 5) {
                case 0: break;                        // Arabic (falls through)
                case 1: style = OrderedListStyle::LowerAlpha; break;
                case 2: style = OrderedListStyle::LowerRoman; break;
                case 3: style = OrderedListStyle::UpperAlpha; break;
                case 4: style = OrderedListStyle::UpperRoman; break;
            }
        }
        switch (style) {
            case OrderedListStyle::LowerAlpha: {
                // a. b. … z. aa. ab. …
                std::string s;
                int n = number - 1;
                do {
                    s = static_cast<char>('a' + n % 26) + s;
                    n = n / 26 - 1;
                } while (n >= 0);
                return s + ".";
            }
            case OrderedListStyle::UpperAlpha: {
                std::string s;
                int n = number - 1;
                do {
                    s = static_cast<char>('A' + n % 26) + s;
                    n = n / 26 - 1;
                } while (n >= 0);
                return s + ".";
            }
            case OrderedListStyle::LowerRoman:
                return to_lower_roman(number) + ".";
            case OrderedListStyle::UpperRoman: {
                std::string r = to_lower_roman(number);
                for (char& c : r) { c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }
                return r + ".";
            }
            default: // Arabic
                return std::to_string(number) + ".";
        }
    }

    void ordered_item(int number, const std::string& text, int depth = 0,
                      OrderedListStyle style = OrderedListStyle::Arabic) {
        float lh     = BODY_SIZE * LINE_RATIO;
        float indent = static_cast<float>(depth) * 18.0f + 22.0f;

        ensure_space(lh);

        std::string label = ordered_label(number, depth, style);
        // Right-align the label just to the left of the text indent.
        // Use actual text width so roman numerals (e.g. "viii.") align correctly.
        float lw = tw(label, minipdf::FontStyle::Regular, BODY_SIZE);
        float label_x = MARGIN_LEFT + indent - lw - 3.0f;
        page_->place_text(label_x, cursor_y_,
                          minipdf::FontStyle::Regular, BODY_SIZE, label);

        auto spans = parse_spans(text);
        for (auto& sp : spans) { sp.size = BODY_SIZE; }
        spans = merge_spans(std::move(spans));
        wrap_spans(spans, lh, indent);
    }

    // ── Description list item ─────────────────────────────────────────────────

    void dlist_item(const std::string& term, const std::string& body) {
        float lh = BODY_SIZE * LINE_RATIO;
        ensure_space(lh);
        auto term_spans = parse_spans(term, minipdf::FontStyle::Bold);
        for (auto& sp : term_spans) { sp.size = BODY_SIZE; }
        term_spans = merge_spans(std::move(term_spans));
        wrap_spans(term_spans, lh, 0.0f);
        if (!body.empty()) {
            paragraph(body, 18.0f);
        }
    }

    // ── Block title ───────────────────────────────────────────────────────────

    void block_title(const std::string& title) {
        float lh = BODY_SIZE * LINE_RATIO;
        ensure_space(lh);
        page_->place_text(MARGIN_LEFT, cursor_y_,
                          minipdf::FontStyle::BoldOblique, BODY_SIZE, title);
        cursor_y_ -= lh;
    }

    /// Render a figure caption below an image.  Uses smaller italic text
    /// and leaves a half-body-size gap above it to separate from the image.
    void figure_caption(const std::string& caption) {
        const float cap_sz = BODY_SIZE * 0.9f;
        const float lh     = cap_sz * LINE_RATIO;
        ensure_space(lh);
        auto spans = parse_spans(caption, minipdf::FontStyle::Oblique);
        for (auto& sp : spans) { sp.size = cap_sz; }
        spans = merge_spans(std::move(spans));
        wrap_spans(spans, lh, 0.0f);
        cursor_y_ -= BODY_SIZE * 0.3f;  // small gap below caption
    }

    // ── Quote / verse / sidebar (indented block) ──────────────────────────────

    void quoted_block(const std::string& text, const std::string& attribution) {
        float lh = BODY_SIZE * LINE_RATIO;
        ensure_space(lh);

        // Draw left vertical bar
        float bar_x = MARGIN_LEFT + 4.0f;
        float top_y = cursor_y_ + lh;

        // Estimate wrapped height (rough)
        auto spans = parse_spans(text);
        for (auto& sp : spans) { sp.size = BODY_SIZE; }
        spans = merge_spans(std::move(spans));
        float avail = content_w_ - 24.0f;
        // Word count estimate
        int word_count = 1;
        for (char c : text) { if (c == ' ') { ++word_count; } }
        float avg_word_w = tw("word ", minipdf::FontStyle::Regular, BODY_SIZE);
        float est_lines = std::max(1.0f,
            std::ceil(static_cast<float>(word_count) * avg_word_w / avail));
        float bar_h = est_lines * lh + (attribution.empty() ? 0 : lh);

        page_->fill_rect(bar_x - 2.0f, top_y - bar_h, 3.0f, bar_h,
                         0.4f, 0.4f, 0.4f);

        wrap_spans(spans, lh, 18.0f);
        if (!attribution.empty()) {
            // em-dash: WinAnsiEncoding 0x97
            auto attr_spans = parse_spans("\x97 " + attribution,
                                          minipdf::FontStyle::Oblique);
            for (auto& sp : attr_spans) { sp.size = BODY_SIZE; }
            attr_spans = merge_spans(std::move(attr_spans));
            wrap_spans(attr_spans, lh, 36.0f);
        }
        cursor_y_ -= 4.0f;
    }

    [[nodiscard]] float cursor_y() const { return cursor_y_; }
    [[nodiscard]] float content_w() const { return content_w_; }

    // ── Table ─────────────────────────────────────────────────────────────────

    /// One pre-processed table row passed to table_block().
    struct TableCellData {
        std::string text;     ///< pre-substituted cell text
        int         colspan;  ///< number of columns this cell spans (≥ 1)
    };

    struct TableRowData {
        std::vector<TableCellData> cells;  ///< pre-substituted cell data
        bool                       header; ///< true → bold text + shaded background
    };

    /// Render a grid table.
    ///
    /// Column widths are given as absolute points and must sum to roughly
    /// content_w_.  Each row is a TableRowData with pre-substituted cell text.
    /// An optional block @p title is rendered above the table.
    ///
    /// The implementation:
    ///   - draws a light-grey background for header rows;
    ///   - word-wraps cell text to fit within each column;
    ///   - calls ensure_space() once per row so the table can span pages;
    ///   - draws a thin (0.5 pt) grid around every cell.
    void table_block(const std::vector<float>& col_w,
                     const std::vector<TableRowData>& rows,
                     const std::string& title = "") {
        if (col_w.empty() || rows.empty()) { return; }

        const std::size_t ncols    = col_w.size();
        const float       lh       = BODY_SIZE * LINE_RATIO;
        const float       pad_x    = 4.0f;          ///< horizontal cell padding (pts)
        // Vertical padding must clear the text ascenders / descenders.
        // cursor_y_ is the text BASELINE; ascenders extend roughly
        // 0.75 × BODY_SIZE above the baseline, descenders ~0.25 × below.
        const float       pad_top  = BODY_SIZE * 0.85f; ///< baseline → top border gap
        const float       pad_bot  = BODY_SIZE * 0.35f; ///< last-baseline → bottom gap
        const float       bdr      = 0.5f;          ///< border line width (pts)
        const float       bdr_grey = 0.5f;           ///< border grey level

        if (!title.empty()) {
            ++table_counter_;
            block_title("Table " + std::to_string(table_counter_) + ". " + title);
        }

        // ── helpers ──────────────────────────────────────────────────────────

        // Count how many wrapped lines are needed to fit @p spans in @p avail_w.
        auto count_lines = [&](const std::vector<TextSpan>& spans,
                               float avail_w) -> int {
            if (spans.empty()) { return 1; }
            float space_w  = tw(" ", minipdf::FontStyle::Regular, BODY_SIZE);
            int   lines    = 1;
            float line_w   = 0.0f;
            auto no_sp = [](const std::string& t) -> bool {
                if (t.empty()) { return false; }
                const char c = t[0];
                return c=='.'||c==','||c==';'||c==':'||c=='!'||c=='?'||c==')'||c==']';
            };
            for (const auto& sp : spans) {
                std::istringstream ss(sp.text);
                std::string        tok;
                while (std::getline(ss, tok, ' ')) {
                    if (tok.empty()) { continue; }
                    float ww  = tw(tok, sp.style, sp.size);
                    float gap = (line_w == 0.0f || no_sp(tok)) ? 0.0f : space_w;
                    float need = line_w + gap + ww;
                    if (need > avail_w && line_w > 0.0f && !no_sp(tok)) {
                        ++lines; line_w = ww;
                    } else {
                        line_w = (line_w == 0.0f) ? ww : need;
                    }
                }
            }
            return lines;
        };

        // ── row rendering ────────────────────────────────────────────────────

        float table_x = MARGIN_LEFT;
        float table_w = content_w_;

        for (const auto& row : rows) {
            const auto base = row.header ? minipdf::FontStyle::Bold
                                         : minipdf::FontStyle::Regular;

            // Compute row height: tallest cell drives the row.
            // row_h = pad_top (above first baseline) + nl × lh + pad_bot (below last baseline)
            float row_h = pad_top + lh + pad_bot;
            float x_scan = 0.0f;  // logical column offset for iterating cells
            for (const auto& cd : row.cells) {
                if (cd.text.empty()) { x_scan += 1; continue; }
                // Sum width over spanned columns
                int cs = std::max(1, cd.colspan);
                float cw = 0.0f;
                for (int s = 0; s < cs && static_cast<std::size_t>(x_scan + s) < ncols; ++s)
                    cw += col_w[static_cast<std::size_t>(x_scan + s)];
                x_scan += cs;
                auto spans = merge_spans(parse_spans(cd.text, base));
                for (auto& sp : spans) { sp.size = BODY_SIZE; }
                float avail_w = cw - 2.0f * pad_x;
                int   nl      = count_lines(spans, avail_w);
                float cell_h  = pad_top + static_cast<float>(nl) * lh + pad_bot;
                row_h = std::max(row_h, cell_h);
            }

            ensure_space(row_h + bdr);

            float row_top    = cursor_y_;
            float row_bottom = row_top - row_h;

            // Header rows: light-grey fill.
            if (row.header) {
                page_->fill_rect(table_x, row_bottom, table_w, row_h,
                                 0.88f, 0.88f, 0.88f);
            }

            // Render cell text – cursor is temporarily manipulated per cell and
            // restored afterwards so all cells in the same row share row_top.
            // Track which physical columns have been covered to draw dividers.
            std::vector<bool> col_covered(ncols, false);
            float x_cell = table_x;
            std::size_t col_idx = 0;
            for (const auto& cd : row.cells) {
                if (col_idx >= ncols) { break; }
                int cs = std::max(1, cd.colspan);
                // Sum width of spanned columns
                float cw = 0.0f;
                for (int s = 0; s < cs && col_idx + static_cast<std::size_t>(s) < ncols; ++s) {
                    cw += col_w[col_idx + static_cast<std::size_t>(s)];
                    col_covered[col_idx + static_cast<std::size_t>(s)] = true;
                }
                if (!cd.text.empty()) {
                    auto spans = merge_spans(parse_spans(cd.text, base));
                    for (auto& sp : spans) { sp.size = BODY_SIZE; }
                    float saved  = cursor_y_;
                    cursor_y_    = row_top - pad_top;
                    wrap_spans_in_cell(spans, lh,
                                       x_cell + pad_x,
                                       cw - 2.0f * pad_x);
                    cursor_y_ = saved;
                }
                x_cell += cw;
                col_idx += static_cast<std::size_t>(cs);
            }

            // Top border for this row.
            page_->draw_hline(table_x, row_top, table_x + table_w,
                              bdr, bdr_grey, bdr_grey, bdr_grey);

            // Vertical column separators: skip internal borders inside spans.
            {
                float vx = table_x;
                // Left outer border
                page_->fill_rect(vx - bdr * 0.5f, row_bottom,
                                 bdr, row_h + bdr,
                                 bdr_grey, bdr_grey, bdr_grey);
                // Compute which column-right-edges should have a divider.
                // Build a map of cumulative width to whether a cell boundary exists.
                float cx = table_x;
                for (std::size_t ci = 0; ci < ncols; ++ci) {
                    cx += col_w[ci];
                    // Draw divider: always on the right edge of the table,
                    // and on column boundaries that are not inside a span.
                    // col_covered marks which logical cols were inside spans.
                    // A boundary is spanned when col_covered[ci] is true
                    // AND col_covered[ci+1] is true AND they belong to the same cell.
                    // Simpler: draw divider unless ci < ncols-1 and ci+1 is also covered
                    // by a cell that started before col ci.
                    // For simplicity: we always draw the right border of the last col,
                    // and for inner borders we draw when NOT covered by a spanning cell
                    // (a spanning cell covers ncols > 1 columns, so the inner borders
                    //  between them should be skipped – mark those positions).
                    bool is_last = (ci + 1 == ncols);
                    // Determine if this column boundary is inside a colspan.
                    // We track by checking whether both this column and the next
                    // were covered, but we can't easily tell if they're the same cell.
                    // Use a simpler heuristic: if this cell is a spanning cell,
                    // suppress inner dividers within its span.
                    // We already computed col_covered; rebuild span boundaries.
                    bool draw = is_last;
                    if (!draw) {
                        // Draw if col_covered[ci] == false OR col_covered[ci+1] == false
                        // i.e., this is not inside a span.  col_covered[ci] = true means
                        // this column was explicitly claimed by some cell.
                        // A simple approach: don't draw when both ci and ci+1 are part of
                        // the same spanning cell – detect by checking if any cell's span
                        // straddles the boundary between ci and ci+1.
                        draw = true;  // default draw
                    }
                    // Rebuild from row.cells: mark inner-span boundaries.
                    // (Redo the column scan for clarity)
                    if (!is_last) {
                        std::size_t scan = 0;
                        for (const auto& cd2 : row.cells) {
                            int cs2 = std::max(1, cd2.colspan);
                            std::size_t end2 = scan + static_cast<std::size_t>(cs2);
                            // If ci is inside [scan, end2) and ci+1 is also inside,
                            // this boundary is within a span.
                            if (scan <= ci && ci + 1 < end2) { draw = false; break; }
                            scan = end2;
                            if (scan > ci + 1) { break; }
                        }
                    }
                    if (draw) {
                        page_->fill_rect(cx - bdr * 0.5f, row_bottom,
                                         bdr, row_h + bdr,
                                         bdr_grey, bdr_grey, bdr_grey);
                    }
                }
            }

            cursor_y_ = row_bottom;
        }

        // Bottom border of the last row.
        page_->draw_hline(table_x, cursor_y_, table_x + table_w,
                          bdr, bdr_grey, bdr_grey, bdr_grey);

        cursor_y_ -= BODY_SIZE * 1.2f;   // gap below table
    }

private:
    /// Word-wrap @p spans into lines and place them starting at the absolute
    /// position (x_start, cursor_y_), clipped to @p avail_w points wide.
    ///
    /// Unlike wrap_spans(), this helper does NOT call ensure_space() – it is the
    /// caller's responsibility to have already guaranteed sufficient vertical
    /// space for the entire row before invoking this function.
    void wrap_spans_in_cell(const std::vector<TextSpan>& spans,
                            float line_h,
                            float x_start,
                            float avail_w) {
        if (spans.empty()) { return; }

        struct Word {
            std::string        text;
            minipdf::FontStyle style;
            float              size;
        };

        float space_w = tw(" ", minipdf::FontStyle::Regular, BODY_SIZE);

        // Helper: return true if this word starts with closing punctuation that
        // should attach directly to the preceding word without a space.
        auto no_space_before = [](const std::string& t) -> bool {
            if (t.empty()) { return false; }
            const char c = t[0];
            return c == '.' || c == ',' || c == ';' || c == ':' ||
                   c == '!' || c == '?' || c == ')' || c == ']';
        };

        std::vector<Word> words;
        for (const auto& sp : spans) {
            std::istringstream ss(sp.text);
            std::string        tok;
            while (std::getline(ss, tok, ' ')) {
                if (!tok.empty()) {
                    words.push_back({tok, sp.style, sp.size});
                }
            }
        }
        if (words.empty()) { return; }

        // Build wrapped lines.
        struct Line {
            std::vector<Word> words;
            float             total_w = 0.0f;
        };
        std::vector<Line> lines;
        lines.push_back({});

        for (const auto& w : words) {
            float ww     = tw(w.text, w.style, w.size);
            bool  attach = !lines.back().words.empty() && no_space_before(w.text);
            float gap    = (lines.back().words.empty() || attach) ? 0.0f : space_w;
            float need   = lines.back().words.empty()
                               ? ww
                               : lines.back().total_w + gap + ww;
            if (need > avail_w && !lines.back().words.empty() && !attach) {
                lines.push_back({});
            }
            float add = lines.back().words.empty() ? ww : gap + ww;
            lines.back().total_w += add;
            lines.back().words.push_back(w);
        }

        // Render – no ensure_space; caller already reserved the row height.
        for (const auto& line : lines) {
            float x     = x_start;
            bool  first = true;
            for (const auto& w : line.words) {
                if (!first && !no_space_before(w.text)) { x += space_w; }
                page_->place_text(x, cursor_y_, w.style, w.size, w.text);
                x += tw(w.text, w.style, w.size);
                first = false;
            }
            cursor_y_ -= line_h;
        }
    }

    /// Measure the rendered width of @p text in the given style/size, using
    /// the custom font for that style when one is attached.
    [[nodiscard]] float tw(const std::string& text,
                           minipdf::FontStyle style, float size) const {
        const auto& ttf = doc_.get_font(style);
        if (ttf) {
            float w = 0.0f;
            for (char c : text) {
                w += ttf->advance_1000(static_cast<unsigned char>(c));
            }
            return w * size / 1000.0f;
        }
        return minipdf::text_width(text, style, size);
    }

    minipdf::Document&      doc_;
    minipdf::Page*          page_      = nullptr;
    float                   cursor_y_  = 0.0f;
    float                   content_w_ = 0.0f;
    int                     figure_counter_ = 0;  ///< auto-incrementing figure number
    int                     table_counter_  = 0;  ///< auto-incrementing table number
};

// ─────────────────────────────────────────────────────────────────────────────
// PdfConverter – walks the Document AST
// ─────────────────────────────────────────────────────────────────────────────

class PdfConverter {
public:
    explicit PdfConverter(bool a4 = false, const FontSet& fonts = {},
                          const std::string& images_dir = "")
        : page_size_(a4 ? minipdf::PageSize::A4 : minipdf::PageSize::Letter),
          images_dir_(images_dir) {
        using FS = minipdf::FontStyle;
        auto load = [](const std::string& path) {
            return minipdf::TtfFont::from_file(path);
        };
        fonts_[static_cast<std::size_t>(FS::Regular)]     = load(fonts.regular);
        fonts_[static_cast<std::size_t>(FS::Bold)]        = load(fonts.bold);
        fonts_[static_cast<std::size_t>(FS::Oblique)]     = load(fonts.italic);
        fonts_[static_cast<std::size_t>(FS::BoldOblique)] = load(fonts.bold_italic);
        fonts_[static_cast<std::size_t>(FS::Mono)]        = load(fonts.mono);
        fonts_[static_cast<std::size_t>(FS::MonoBold)]    = load(fonts.mono_bold);
    }

    [[nodiscard]] std::string convert(const Document& doc) const {
        minipdf::Document pdf(page_size_);
        using FS = minipdf::FontStyle;
        static constexpr FS styles[] = {
            FS::Regular, FS::Bold, FS::Oblique,
            FS::BoldOblique, FS::Mono, FS::MonoBold
        };
        for (auto s : styles) {
            const auto& f = fonts_[static_cast<std::size_t>(s)];
            if (f) { pdf.set_font(s, f); }
        }
        PdfLayout layout(pdf);

        render_document(doc, layout);

        return pdf.to_string();
    }

private:
    minipdf::PageSize                              page_size_;
    std::array<std::shared_ptr<minipdf::TtfFont>, 6> fonts_{};
    std::string                                    images_dir_;

    // ── Apply attribute substitution (document header and simple paragraphs)
    [[nodiscard]] static std::string attrs(const std::string& text,
                                           const Document& doc) {
        // First resolve {attribute} references, then apply typographic
        // substitutions (... → ellipsis, -- → em dash, (C) → ©, etc.).
        return apply_pdf_typo_subs(sub_attributes(text, doc.attributes()));
    }

    // ── Document ──────────────────────────────────────────────────────────────

    void render_document(const Document& doc, PdfLayout& layout) const {
        // Title, author, revision
        const DocumentHeader& hdr = doc.header();
        if (hdr.has_header && !hdr.title.empty()) {
            layout.heading(attrs(hdr.title, doc), 0);

            // Author line – all authors joined by ", "
            if (!hdr.authors.empty()) {
                std::string auth_line;
                for (std::size_t ai = 0; ai < hdr.authors.size(); ++ai) {
                    if (ai > 0) { auth_line += ", "; }
                    const auto& a = hdr.authors[ai];
                    if (!a.firstname.empty())  { auth_line += a.firstname; }
                    if (!a.middlename.empty()) { auth_line += " " + a.middlename; }
                    if (!a.lastname.empty())   { auth_line += " " + a.lastname;  }
                    if (!a.email.empty())      { auth_line += " <" + a.email + ">"; }
                }
                layout.meta_line(auth_line);
            }

            // Revision line
            const auto& rev = hdr.revision;
            if (!rev.number.empty() || !rev.date.empty()) {
                std::string revline;
                if (!rev.number.empty()) { revline += "v" + rev.number; }
                if (!rev.date.empty()) {
                    if (!revline.empty()) { revline += ", "; }
                    revline += rev.date;
                }
                if (!rev.remark.empty()) { revline += " — " + rev.remark; }
                layout.meta_line(revline);
            }
            layout.skip(8.0f);
        }

        for (const auto& child : doc.blocks()) {
            render_block(*child, doc, layout, 0);
        }
    }

    // ── Block dispatcher ──────────────────────────────────────────────────────

    void render_block(const Block& blk, const Document& doc,
                      PdfLayout& layout, int list_depth) const {
        switch (blk.context()) {

            case BlockContext::Section:
                render_section(dynamic_cast<const Section&>(blk), doc, layout);
                break;

            case BlockContext::Paragraph:
                if (blk.has_title()) {
                    layout.block_title(attrs(blk.title(), doc));
                }
                layout.paragraph(attrs(blk.source(), doc));
                break;

            case BlockContext::Listing:
            case BlockContext::Literal:
                if (blk.has_title()) {
                    layout.block_title(attrs(blk.title(), doc));
                }
                layout.code_block(blk.source());
                break;

            case BlockContext::Ulist:
                render_ulist(dynamic_cast<const List&>(blk), doc, layout, list_depth);
                break;

            case BlockContext::Olist:
                render_olist(dynamic_cast<const List&>(blk), doc, layout, list_depth);
                break;

            case BlockContext::Dlist:
                render_dlist(dynamic_cast<const List&>(blk), doc, layout);
                break;

            case BlockContext::Admonition:
                render_admonition(blk, doc, layout);
                break;

            case BlockContext::Quote:
            case BlockContext::Verse:
                render_quote(blk, doc, layout);
                break;

            case BlockContext::Example:
            case BlockContext::Sidebar:
                render_compound(blk, doc, layout, list_depth);
                break;

            case BlockContext::Table:
                render_table(dynamic_cast<const Table&>(blk), doc, layout);
                break;

            case BlockContext::Pass:
                // Raw pass-through: emit as plain text (PDF cannot render HTML)
                layout.paragraph(blk.source());
                break;

            case BlockContext::ThematicBreak:
                layout.hrule();
                break;

            case BlockContext::PageBreak:
                layout.page_break();
                break;

            case BlockContext::Image: {                // Resolve the image target to a file path.
                // Try, in order:
                //   1. target as-is (absolute or relative to cwd)
                //   2. images_dir_ / target
                //   3. imagesdir document attribute + "/" + target
                std::string target = blk.attr("target");
                std::string resolved;
                {
                    auto try_path = [](const std::string& p) -> bool {
                        if (p.empty()) { return false; }
                        std::ifstream tf(p, std::ios::binary);
                        return static_cast<bool>(tf);
                    };
                    if (try_path(target)) {
                        resolved = target;
                    } else if (!images_dir_.empty()) {
                        std::string candidate = images_dir_ + "/" + target;
                        if (try_path(candidate)) { resolved = candidate; }
                    }
                    if (resolved.empty()) {
                        // Try document imagesdir attribute
                        std::string idir = doc.attr("imagesdir");
                        if (!idir.empty()) {
                            std::string candidate = idir + "/" + target;
                            if (try_path(candidate)) { resolved = candidate; }
                        }
                    }
                    if (resolved.empty()) { resolved = target; }
                }

                // Parse optional width/height hints from block attributes.
                // Dimension strings like "3.9in", "150px", "50%" are converted
                // to PDF points; bare numbers are treated as points already.
                float hint_w = 0.0f, hint_h = 0.0f;
                {
                    const float cw = layout.content_width();
                    const std::string& ws = blk.attr("width");
                    const std::string& hs = blk.attr("height");
                    if (!ws.empty()) { hint_w = parse_dimension_pts(ws, cw); }
                    if (!hs.empty()) { hint_h = parse_dimension_pts(hs, cw); }
                }

                layout.image_block(resolved, hint_w, hint_h,
                                   blk.has_title() ? attrs(blk.title(), doc) : "");
                break;
            }

            case BlockContext::Preamble:
            default:
                for (const auto& child : blk.blocks()) {
                    render_block(*child, doc, layout, list_depth);
                }
                break;
        }
    }

    // ── Section ───────────────────────────────────────────────────────────────

    void render_section(const Section& sect, const Document& doc,
                        PdfLayout& layout) const {
        layout.heading(attrs(sect.title(), doc), sect.level());
        for (const auto& child : sect.blocks()) {
            render_block(*child, doc, layout, 0);
        }
    }

    // ── Admonition ────────────────────────────────────────────────────────────

    void render_admonition(const Block& blk, const Document& doc,
                           PdfLayout& layout) const {
        std::string label = blk.attr("name");
        if (label.empty()) { label = "note"; }

        if (blk.content_model() == ContentModel::Simple) {
            layout.admonition(label, attrs(blk.source(), doc));
        } else {
            // Render label then indented content — no trailing colon,
            // matching asciidoctor's PDF output style.
            std::string lbl = label;
            for (char& c : lbl) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            layout.block_title(lbl);
            for (const auto& child : blk.blocks()) {
                render_block(*child, doc, layout, 0);
            }
        }
    }

    // ── Quote / Verse ─────────────────────────────────────────────────────────

    void render_quote(const Block& blk, const Document& doc,
                      PdfLayout& layout) const {
        std::string attribution = !blk.attr("attribution").empty()
                                  ? blk.attr("attribution") : blk.attr("2");
        std::string citetitle   = !blk.attr("citetitle").empty()
                                  ? blk.attr("citetitle") : blk.attr("3");
        if (!citetitle.empty()) {
            if (!attribution.empty()) { attribution += ", "; }
            attribution += citetitle;
        }

        if (blk.content_model() == ContentModel::Simple) {
            layout.quoted_block(attrs(blk.source(), doc), attribution);
        } else {
            // Extract first paragraph for the quote body
            for (const auto& child : blk.blocks()) {
                if (child->context() == BlockContext::Paragraph) {
                    layout.quoted_block(attrs(child->source(), doc), attribution);
                } else {
                    render_block(*child, doc, layout, 0);
                }
            }
        }
    }

    // ── Compound block (example, sidebar) ────────────────────────────────────

    void render_compound(const Block& blk, const Document& doc,
                         PdfLayout& layout, int list_depth) const {
        if (blk.has_title()) {
            layout.block_title(attrs(blk.title(), doc));
        }
        layout.skip(4.0f);
        for (const auto& child : blk.blocks()) {
            render_block(*child, doc, layout, list_depth);
        }
        layout.skip(4.0f);
    }

    // ── Unordered list ────────────────────────────────────────────────────────

    void render_ulist(const List& list, const Document& doc,
                      PdfLayout& layout, int depth) const {
        if (list.has_title()) {
            layout.block_title(attrs(list.title(), doc));
        }
        for (const auto& item : list.items()) {
            layout.bullet_item(attrs(item->source(), doc), depth);
            // Nested lists or sub-blocks
            for (const auto& child : item->blocks()) {
                render_block(*child, doc, layout, depth + 1);
            }
        }
        layout.skip(4.0f);
    }

    // ── Ordered list ──────────────────────────────────────────────────────────

    void render_olist(const List& list, const Document& doc,
                      PdfLayout& layout, int depth) const {
        if (list.has_title()) {
            layout.block_title(attrs(list.title(), doc));
        }
        // Determine starting counter from start= attribute (default 1)
        int n = 1;
        const std::string& start_attr = list.attr("start");
        if (!start_attr.empty()) {
            try { n = std::stoi(start_attr); } catch (...) {}
        }
        OrderedListStyle style = list.ordered_style();
        for (const auto& item : list.items()) {
            layout.ordered_item(n, attrs(item->source(), doc), depth, style);
            for (const auto& child : item->blocks()) {
                render_block(*child, doc, layout, depth + 1);
            }
            ++n;
        }
        layout.skip(4.0f);
    }

    // ── Description list ──────────────────────────────────────────────────────

    void render_dlist(const List& list, const Document& doc,
                      PdfLayout& layout) const {
        if (list.has_title()) {
            layout.block_title(attrs(list.title(), doc));
        }
        for (const auto& item : list.items()) {
            layout.dlist_item(attrs(item->term(), doc),
                              attrs(item->source(), doc));
            for (const auto& child : item->blocks()) {
                render_block(*child, doc, layout, 0);
            }
        }
        layout.skip(4.0f);
    }

    // ── Table ─────────────────────────────────────────────────────────────────
    //
    // Converts the Table AST node into the PdfLayout::TableRowData intermediate
    // representation and delegates rendering to PdfLayout::table_block().
    //
    // Column widths are computed proportionally from the ColumnSpec weights.
    // When no specs are present every column receives an equal share of the
    // content width.

    void render_table(const Table& tbl, const Document& doc,
                      PdfLayout& layout) const {
        const auto& specs = tbl.column_specs();

        // Determine number of columns.
        std::size_t ncols = specs.size();
        if (ncols == 0) {
            for (const auto* sec : {&tbl.head_rows(),
                                    &tbl.body_rows(),
                                    &tbl.foot_rows()}) {
                if (!sec->empty()) { ncols = sec->front().cells().size(); break; }
            }
        }
        if (ncols == 0) { return; }

        // Proportional column widths.
        int total_weight = 0;
        for (std::size_t i = 0; i < ncols; ++i) {
            total_weight += (i < specs.size() && specs[i].width > 0)
                                ? specs[i].width : 1;
        }
        std::vector<float> col_w(ncols);
        for (std::size_t i = 0; i < ncols; ++i) {
            int w = (i < specs.size() && specs[i].width > 0) ? specs[i].width : 1;
            col_w[i] = layout.content_w()
                       * static_cast<float>(w) / static_cast<float>(total_weight);
        }

        // Build row data with attribute-substituted cell text and colspan info.
        std::vector<PdfLayout::TableRowData> rows;
        auto append = [&](const std::vector<TableRow>& src, bool header) {
            for (const auto& row : src) {
                PdfLayout::TableRowData rd;
                rd.header = header;
                for (const auto& cell : row.cells()) {
                    rd.cells.push_back({attrs(cell->source(), doc), cell->colspan()});
                }
                rows.push_back(std::move(rd));
            }
        };
        append(tbl.head_rows(), true);
        append(tbl.body_rows(), false);
        append(tbl.foot_rows(), false);

        std::string title = tbl.has_title() ? attrs(tbl.title(), doc) : "";
        layout.table_block(col_w, rows, title);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Free function convenience wrappers
// ─────────────────────────────────────────────────────────────────────────────

/// Convert a parsed Document to a PDF byte string using a FontSet.
/// @param a4         When true, use A4 page size; otherwise Letter (default).
/// @param fonts      Paths to TrueType font files for each text style.  Any empty
///                   path falls back to the corresponding PDF base-14 font.
/// @param images_dir Base directory to search for image files referenced by
///                   image:: blocks.  When empty, only the target path as written
///                   in the source and the document's imagesdir attribute are tried.
[[nodiscard]] inline std::string convert_to_pdf(const Document& doc,
                                                 bool a4,
                                                 const FontSet& fonts,
                                                 const std::string& images_dir = "") {
    return PdfConverter{a4, fonts, images_dir}.convert(doc);
}

/// Convert a parsed Document to a PDF byte string.
/// @param a4        When true, use A4 page size; otherwise Letter (default).
/// @param font_path Optional path to a TrueType (.ttf) file to use as the
///                  body text font (FontStyle::Regular / F1).  When empty
///                  (default) or unreadable, Helvetica is used instead.
[[nodiscard]] inline std::string convert_to_pdf(const Document& doc,
                                                 bool a4 = false,
                                                 const std::string& font_path = "") {
    FontSet fs;
    fs.regular = font_path;
    return PdfConverter{a4, fs}.convert(doc);
}

} // namespace asciiquack
