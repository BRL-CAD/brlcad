/// @file substitutors.hpp
/// @brief Text substitution engine for asciiquack.
///
/// Mirrors the Ruby Asciidoctor Substitutors module.
///
/// Substitution pipeline (applied in order for "normal" subs):
///   0. pass macros       – extract pass:[] regions into protected stash
///   1. specialcharacters  – escape &, <, >
///   2. quotes             – bold, italic, monospace, …
///   3. attributes         – {name} references  (+ counter: macros)
///   4. replacements       – (C), (R), (TM), --, …, '
///   5. macros             – link:, image:, <<xref>>, footnote:, etc.
///   6. post_replacements  – hard line-break marker (" +")
///   9. pass restore       – restore protected stash regions
///
/// Each function is a pure transformation: it takes a string and returns a
/// new string with the substitution applied.

#pragma once

#include "inline_scanner.hpp"
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace asciiquack {

// ─────────────────────────────────────────────────────────────────────────────
// InlineContext  –  mutable state shared across inline substitutions
// ─────────────────────────────────────────────────────────────────────────────

/// Holds per-conversion mutable state needed by the inline substitution
/// pipeline: attribute map reference, named counters, and collected footnotes.
struct FootnoteEntry {
    int         number;   ///< sequential number (1-based)
    std::string id;       ///< named anchor id (empty for anonymous footnotes)
    std::string text;     ///< footnote body text
};

/// Context passed through the inline substitution pipeline.
/// The caller (Html5Converter) owns this and passes it by pointer.
struct InlineContext {
    const std::unordered_map<std::string, std::string>& attrs;

    /// Mutable counter map.  May be nullptr if counter: macros are not needed.
    std::unordered_map<std::string, int>* counters = nullptr;

    /// Collected footnotes.  May be nullptr if footnotes are not needed.
    std::vector<FootnoteEntry>* footnotes = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// 0. Pass macro extraction / restore  (must run before all other subs)
// ─────────────────────────────────────────────────────────────────────────────
//
// pass:[content]   – emit content with NO substitutions
// pass:q[content]  – apply only the quotes substitution to content
// pass:c[content]  – apply only the specialchars substitution to content
//
// We use STX (\x02) and ETX (\x03) as placeholder delimiters.  They don't
// appear in normal text and survive all subsequent substitution steps.

/// Extract pass:[] macros from @p text, store their (possibly processed) content
/// in @p stash, and return the text with each macro replaced by a numeric
/// placeholder ("\x02<index>\x03").  @p stash may be passed to restore_pass_macros()
/// after all other substitutions have been applied to the placeholder text.
[[nodiscard]] inline std::string extract_pass_macros(
        const std::string& text,
        std::vector<std::string>& stash)
{
    // Fast path: no "pass:" in text → nothing to do.
    if (text.find("pass:") == std::string::npos) { return text; }

    std::string out;
    out.reserve(text.size());
    const std::size_t n = text.size();

    for (std::size_t i = 0; i < n; ) {
        // Scan for "pass:" (5 bytes)
        if (i + 5 <= n &&
            text[i]   == 'p' && text[i+1] == 'a' && text[i+2] == 's' &&
            text[i+3] == 's' && text[i+4] == ':') {

            std::size_t j = i + 5;
            // Consume optional lowercase subs spec (e.g. "c", "q", "cq")
            const std::size_t spec_start = j;
            while (j < n && text[j] >= 'a' && text[j] <= 'z') { ++j; }

            // Must be followed by '['
            if (j < n && text[j] == '[') {
                const std::size_t bracket_open = j;
                ++j;  // consume '['

                // Scan for ']', treating '\]' as an escaped bracket.
                bool found_close = false;
                while (j < n) {
                    if (text[j] == '\\' && j + 1 < n && text[j+1] == ']') {
                        j += 2;  // skip escaped ']'
                    } else if (text[j] == ']') {
                        found_close = true;
                        break;
                    } else {
                        ++j;
                    }
                }

                if (found_close) {
                    // Extract subs spec string
                    const std::string subs_spec(text, spec_start,
                                                bracket_open - spec_start);

                    // Extract content, unescaping '\]'
                    std::string content;
                    content.reserve(j - bracket_open - 1);
                    for (std::size_t k = bracket_open + 1; k < j; ) {
                        if (text[k] == '\\' && k + 1 < j && text[k+1] == ']') {
                            content += ']';
                            k += 2;
                        } else {
                            content += text[k++];
                        }
                    }

                    // Apply requested substitutions to the pass content
                    if (!subs_spec.empty()) {
                        if (subs_spec.find('c') != std::string::npos) {
                            std::string esc;
                            esc.reserve(content.size());
                            for (char c2 : content) {
                                switch (c2) {
                                    case '&': esc += "&amp;";  break;
                                    case '<': esc += "&lt;";   break;
                                    case '>': esc += "&gt;";   break;
                                    default:  esc += c2;       break;
                                }
                            }
                            content = std::move(esc);
                        }
                        if (subs_spec.find('q') != std::string::npos) {
                            content = "\x05q\x05" + content;
                        }
                    }

                    out += '\x02';
                    out += std::to_string(stash.size());
                    out += '\x03';
                    stash.push_back(std::move(content));
                    i = j + 1;  // past ']'
                    continue;
                }
                // No matching ']' found — fall through to emit 'p' literally.
            }
            // Not a valid pass macro (no '[' or no ']') — emit 'p' and move on.
        }
        out += text[i++];
    }

    return out;
}

/// Replace placeholders with their stashed content.
[[nodiscard]] inline std::string restore_pass_macros(
        const std::string& text,
        const std::vector<std::string>& stash);  // defined after sub_quotes

// ─────────────────────────────────────────────────────────────────────────────
// 1. Special characters
// ─────────────────────────────────────────────────────────────────────────────

/// Escape HTML special characters: & < >
[[nodiscard]] inline std::string sub_specialchars(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 16);
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;";  break;
            case '<': out += "&lt;";   break;
            case '>': out += "&gt;";   break;
            default:  out += c;        break;
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Inline quote formatting
// ─────────────────────────────────────────────────────────────────────────────
//
// Constrained  (*word*, _word_, `word`, #word#, ^word^, ~word~)
//   – may not be immediately preceded / followed by a word character
// Unconstrained (**text**, __text__, ``text``, ##text##, ^^text^^, ~~text~~)
//   – no boundary restriction

namespace detail {

/// (No longer used — kept as an empty namespace to avoid downstream breaks.)

} // namespace detail

/// Apply inline quote substitutions (*bold*, _italic_, etc.).
///
/// Delegates to scan_inline_quotes() from inline_scanner.hpp — a single-pass
/// hand-written scanner that requires no regex engine.
[[nodiscard]] inline std::string sub_quotes(const std::string& text) {
    return scan_inline_quotes(text);
}

/// Replace placeholders (inserted by extract_pass_macros) with stashed content.
[[nodiscard]] inline std::string restore_pass_macros(
        const std::string& text,
        const std::vector<std::string>& stash)
{
    if (stash.empty()) { return text; }

    std::string out;
    out.reserve(text.size());
    const std::size_t n = text.size();

    for (std::size_t i = 0; i < n; ) {
        if (text[i] == '\x02') {
            // Find the closing ETX
            std::size_t j = i + 1;
            while (j < n && text[j] != '\x03') { ++j; }
            if (j < n) {
                std::string idx_str(text, i + 1, j - i - 1);
                try {
                    std::size_t idx = std::stoul(idx_str);
                    if (idx < stash.size()) {
                        std::string content = stash[idx];
                        // Check for q-subs sentinel (\x05 q \x05 prefix)
                        if (content.size() >= 3 &&
                            content[0] == '\x05' && content[1] == 'q' &&
                            content[2] == '\x05') {
                            content = sub_quotes(content.substr(3));
                        }
                        out += content;
                    }
                } catch (...) {}
                i = j + 1;
                continue;
            }
        }
        out += text[i++];
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Attribute references
// ─────────────────────────────────────────────────────────────────────────────

/// Expand {attribute-name} references using the supplied attribute map.
[[nodiscard]] inline std::string sub_attributes(
        const std::string&                                    text,
        const std::unordered_map<std::string, std::string>&   attrs)
{
    // Fast path: no opening brace → nothing to do.
    if (text.find('{') == std::string::npos) { return text; }

    // Resolve attribute-missing policy (default: skip)
    std::string missing_policy = "skip";
    {
        auto it = attrs.find("attribute-missing");
        if (it != attrs.end()) { missing_policy = it->second; }
    }

    std::string out;
    out.reserve(text.size());
    const std::size_t n = text.size();

    for (std::size_t i = 0; i < n; ) {
        if (text[i] != '{') { out += text[i++]; continue; }

        // Check if valid attribute reference: {name} where name = \w[\w\-]*
        std::size_t j = i + 1;
        if (j >= n) { out += text[i++]; continue; }

        const char fc = text[j];
        if (!std::isalnum(static_cast<unsigned char>(fc)) && fc != '_') {
            out += text[i++]; continue;
        }

        // Scan name: word chars and hyphens
        while (j < n && (std::isalnum(static_cast<unsigned char>(text[j])) ||
                          text[j] == '_' || text[j] == '-')) {
            ++j;
        }

        if (j < n && text[j] == '}') {
            std::string name(text, i + 1, j - i - 1);
            auto ai = attrs.find(name);
            if (ai != attrs.end()) {
                out += ai->second;
            } else if (missing_policy == "drop") {
                // drop: replace with empty string
            } else {
                if (missing_policy == "warn") {
                    std::cerr << "asciiquack: WARNING: skipping reference to missing attribute: "
                              << name << "\n";
                }
                out.append(text, i, j - i + 1);  // leave {name} as-is
            }
            i = j + 1;  // past '}'
        } else {
            out += text[i++];  // not a valid reference, emit '{' literally
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Typographic replacements
// ─────────────────────────────────────────────────────────────────────────────

/// Apply typographic replacements:
///   --          →  em-dash (&#8212;)
///   ...         →  ellipsis (&#8230;&#8203;)
///   (C)/(R)/(TM) →  ©/®/™
///   arrows/smart apostrophe
[[nodiscard]] inline std::string sub_replacements(const std::string& text) {
    std::string out = text;

    // Helper: replace all occurrences of a fixed string.
    auto replace_all = [](std::string& s, std::string_view from, std::string_view to) {
        std::size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    };

    // Em-dash handling (order matters: interior first, then start, then end).
    replace_all(out, " -- ", "&#8201;&#8212;&#8201;");
    if (out.size() >= 3 &&
        out[0] == '-' && out[1] == '-' && out[2] == ' ') {
        out = "&#8201;&#8212;&#8201;" + out.substr(3);
    }
    {
        const std::size_t sz = out.size();
        if (sz >= 3 &&
            out[sz-3] == ' ' && out[sz-2] == '-' && out[sz-1] == '-') {
            out.resize(sz - 3);
            out += "&#8201;&#8212;";
        }
    }

    // Ellipsis
    replace_all(out, "...", "&#8230;&#8203;");

    // Copyright / Registered / Trademark  (uppercase only, Asciidoctor-compatible)
    replace_all(out, "(C)",  "&#169;");
    replace_all(out, "(R)",  "&#174;");
    replace_all(out, "(TM)", "&#8482;");

    // Arrows (note: by this stage '<' is already "&lt;" and '>' is "&gt;")
    replace_all(out, "-&gt;", "&#8594;");
    replace_all(out, "&lt;-", "&#8592;");
    replace_all(out, "=&gt;", "&#8658;");
    replace_all(out, "&lt;=", "&#8656;");

    // Smart apostrophe: word'word → word&#8217;word
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
                if (pw && nw) { result += "&#8217;"; continue; }
            }
            result += out[i];
        }
        out = std::move(result);
    }

    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. Inline macros
// ─────────────────────────────────────────────────────────────────────────────

/// Apply inline macro substitutions (links, images, xrefs, anchors).
///
/// Single left-to-right pass over the text.  No regex required.
[[nodiscard]] inline std::string sub_macros(const std::string& text) {

    // ── Helpers ──────────────────────────────────────────────────────────────

    // True if text[pos..] starts with the given prefix.
    auto sw = [&](std::size_t pos, std::string_view prefix) -> bool {
        if (pos + prefix.size() > text.size()) { return false; }
        return text.compare(pos, prefix.size(), prefix.data(), prefix.size()) == 0;
    };

    // Find the first unescaped ']' starting from after_open.
    auto close_bracket = [&](std::size_t after_open) -> std::size_t {
        for (std::size_t k = after_open; k < text.size(); ++k) {
            if (text[k] == ']') { return k; }
        }
        return std::string::npos;
    };

    // Parse inline image bracket: "alt,width=N,height=N" etc.
    auto parse_img_bracket = [](const std::string& bracket,
                                std::string& alt,
                                std::string& width_val,
                                std::string& height_val) {
        alt.clear(); width_val.clear(); height_val.clear();
        std::istringstream ss(bracket);
        std::string tok;
        bool first = true;
        while (std::getline(ss, tok, ',')) {
            auto b = tok.find_first_not_of(" \t");
            auto e = tok.find_last_not_of(" \t");
            if (b == std::string::npos) { first = false; continue; }
            tok = tok.substr(b, e - b + 1);
            auto eq = tok.find('=');
            if (eq == std::string::npos) {
                if (first) { alt = tok; }
            } else {
                std::string key = tok.substr(0, eq);
                std::string val = tok.substr(eq + 1);
                if (key == "width")  { width_val  = val; }
                if (key == "height") { height_val = val; }
            }
            first = false;
        }
    };

    std::string out;
    out.reserve(text.size() + 64);
    const std::size_t n = text.size();

    for (std::size_t i = 0; i < n; ) {
        const char c = text[i];

        // ── [[anchor]] ──────────────────────────────────────────────────────
        if (c == '[' && sw(i, "[[")) {
            std::size_t j = i + 2;
            if (j < n && (std::isalpha(static_cast<unsigned char>(text[j])) ||
                          text[j] == '_' || text[j] == ':')) {
                const std::size_t id_s = j;
                while (j < n && (std::isalnum(static_cast<unsigned char>(text[j])) ||
                                  text[j]=='_' || text[j]=='-' ||
                                  text[j]=='.' || text[j]==':')) { ++j; }
                const std::size_t id_e = j;
                if (j < n && text[j] == ',') {
                    // skip reftext
                    while (j < n && !(j + 1 < n && text[j]==']' && text[j+1]==']')) { ++j; }
                }
                if (j + 1 < n && text[j]==']' && text[j+1]==']') {
                    out += "<a id=\"";
                    out.append(text, id_s, id_e - id_s);
                    out += "\"></a>";
                    i = j + 2;
                    continue;
                }
            }
        }

        // ── &lt;&lt;id[,text]&gt;&gt; ────────────────────────────────────
        if (c == '&' && sw(i, "&lt;&lt;")) {
            std::size_t j = i + 8;
            const std::size_t id_s = j;
            while (j < n && text[j] != ',' && !sw(j, "&gt;")) { ++j; }
            if (j > id_s) {
                std::size_t id_e = j;
                while (id_e > id_s && (text[id_e-1]==' '||text[id_e-1]=='\t')) { --id_e; }
                std::string id(text, id_s, id_e - id_s);
                std::string disp = id;
                if (j < n && text[j] == ',') {
                    ++j;
                    while (j < n && (text[j]==' '||text[j]=='\t')) { ++j; }
                    const std::size_t disp_s = j;
                    while (j < n && !sw(j, "&gt;")) { ++j; }
                    std::size_t disp_e = j;
                    while (disp_e > disp_s && (text[disp_e-1]==' '||text[disp_e-1]=='\t')) { --disp_e; }
                    if (disp_e > disp_s) { disp = std::string(text, disp_s, disp_e - disp_s); }
                }
                if (sw(j, "&gt;&gt;")) {
                    out += "<a href=\"#"; out += id; out += "\">"; out += disp; out += "</a>";
                    i = j + 8;
                    continue;
                }
            }
        }

        // ── xref:id[text] ────────────────────────────────────────────────
        if (c == 'x' && sw(i, "xref:")) {
            std::size_t j = i + 5;
            const std::size_t id_s = j;
            while (j < n && text[j] != '[' && text[j] != ' ' && text[j] != '\t') { ++j; }
            if (j > id_s && j < n && text[j] == '[') {
                std::string id(text, id_s, j - id_s);
                const std::size_t cls = close_bracket(j + 1);
                if (cls != std::string::npos) {
                    std::string txt(text, j + 1, cls - j - 1);
                    if (txt.empty()) { txt = id; }
                    out += "<a href=\"#"; out += id; out += "\">"; out += txt; out += "</a>";
                    i = cls + 1;
                    continue;
                }
            }
        }

        // ── link:url[text] ───────────────────────────────────────────────
        if (c == 'l' && sw(i, "link:")) {
            std::size_t j = i + 5;
            const std::size_t url_s = j;
            while (j < n && text[j] != '[') { ++j; }
            if (j > url_s && j < n) {
                std::string url(text, url_s, j - url_s);
                const std::size_t cls = close_bracket(j + 1);
                if (cls != std::string::npos) {
                    std::string txt(text, j + 1, cls - j - 1);
                    if (txt.empty()) { txt = url; }
                    out += "<a href=\""; out += url; out += "\">"; out += txt; out += "</a>";
                    i = cls + 1;
                    continue;
                }
            }
        }

        // ── https?://url  (auto-link) ────────────────────────────────────
        if (c == 'h' && (sw(i, "https://") || sw(i, "http://"))) {
            const bool in_href = (!out.empty() && out.back() == '"');
            std::size_t j = i;
            while (j < n && text[j] != ' ' && text[j] != '\t' && text[j] != '\n' &&
                   text[j] != '<' && text[j] != '>' && text[j] != '[' &&
                   text[j] != ']' && text[j] != '"') {
                ++j;
            }
            if (in_href) {
                out.append(text, i, j - i);
            } else {
                std::string url(text, i, j - i);
                out += "<a href=\""; out += url; out += "\">"; out += url; out += "</a>";
            }
            i = j;
            continue;
        }

        // ── image:path[attrs]  (inline image, not image::) ───────────────
        if (c == 'i' && sw(i, "image:") &&
            !(i + 6 < n && text[i + 6] == ':')) {
            std::size_t j = i + 6;
            const std::size_t tgt_s = j;
            while (j < n && text[j] != '[') { ++j; }
            if (j > tgt_s && j < n) {
                std::string target(text, tgt_s, j - tgt_s);
                const std::size_t cls = close_bracket(j + 1);
                if (cls != std::string::npos) {
                    std::string bracket(text, j + 1, cls - j - 1);
                    std::string alt, width_val, height_val;
                    parse_img_bracket(bracket, alt, width_val, height_val);
                    out += "<span class=\"image\"><img src=\"";
                    out += target;
                    out += "\" alt=\"";
                    out += sub_specialchars(alt);
                    out += "\"";
                    if (!width_val.empty())  { out += " width=\"";  out += width_val;  out += "\""; }
                    if (!height_val.empty()) { out += " height=\""; out += height_val; out += "\""; }
                    out += "></span>";
                    i = cls + 1;
                    continue;
                }
            }
        }

        // ── kbd:[keys] ───────────────────────────────────────────────────
        if (c == 'k' && sw(i, "kbd:[")) {
            std::size_t j = i + 4;  // points to '['
            const std::size_t cls = close_bracket(j + 1);
            if (cls != std::string::npos) {
                std::string keys_str(text, j + 1, cls - j - 1);
                std::string keys_html;
                std::istringstream ks(keys_str);
                std::string kpart;
                bool first_key = true;
                while (std::getline(ks, kpart, '+')) {
                    while (!kpart.empty() && kpart.front() == ' ') { kpart.erase(0, 1); }
                    while (!kpart.empty() && kpart.back()  == ' ') { kpart.pop_back(); }
                    if (!first_key) { keys_html += "+"; }
                    keys_html += "<kbd>" + kpart + "</kbd>";
                    first_key = false;
                }
                out += "<span class=\"keyseq\">"; out += keys_html; out += "</span>";
                i = cls + 1;
                continue;
            }
        }

        // ── btn:[label] ──────────────────────────────────────────────────
        if (c == 'b' && sw(i, "btn:[")) {
            std::size_t j = i + 4;  // points to '['
            const std::size_t cls = close_bracket(j + 1);
            if (cls != std::string::npos) {
                std::string label(text, j + 1, cls - j - 1);
                out += "<b class=\"button\">"; out += label; out += "</b>";
                i = cls + 1;
                continue;
            }
        }

        // ── menu:name[items] ─────────────────────────────────────────────
        if (c == 'm' && sw(i, "menu:")) {
            std::size_t j = i + 5;
            const std::size_t menu_s = j;
            while (j < n && text[j] != '[') { ++j; }
            if (j > menu_s && j < n) {
                std::string menu_str(text, menu_s, j - menu_s);
                while (!menu_str.empty() && menu_str.back() == ' ') { menu_str.pop_back(); }
                const std::size_t cls = close_bracket(j + 1);
                if (cls != std::string::npos) {
                    std::string items_str(text, j + 1, cls - j - 1);
                    std::string html = "<span class=\"menuseq\"><span class=\"menu\">";
                    html += menu_str;
                    html += "</span>";
                    if (!items_str.empty()) {
                        std::istringstream is(items_str);
                        std::string part;
                        while (std::getline(is, part, '>')) {
                            while (!part.empty() && part.front() == ' ') { part.erase(0, 1); }
                            while (!part.empty() && part.back()  == ' ') { part.pop_back(); }
                            if (!part.empty()) {
                                html += "<span class=\"caret\">&#8250;</span>"
                                        "<span class=\"menuitem\">" + part + "</span>";
                            }
                        }
                    }
                    html += "</span>";
                    out += html;
                    i = cls + 1;
                    continue;
                }
            }
        }

        // ── stem/latexmath/asciimath:[expr] ──────────────────────────────
        if ((c == 's' && sw(i, "stem:[")) ||
            (c == 'l' && sw(i, "latexmath:[")) ||
            (c == 'a' && sw(i, "asciimath:["))) {
            std::size_t bracket_pos = text.find('[', i);
            if (bracket_pos != std::string::npos) {
                const std::size_t cls = close_bracket(bracket_pos + 1);
                if (cls != std::string::npos) {
                    std::string kind(text, i, bracket_pos - i);
                    std::string expr(text, bracket_pos + 1, cls - bracket_pos - 1);
                    if (kind == "asciimath") {
                        out += "`"; out += expr; out += "`";
                    } else {
                        out += "\\("; out += expr; out += "\\)";
                    }
                    i = cls + 1;
                    continue;
                }
            }
        }

        out += c;
        ++i;
    }

    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// 5b. Footnote + counter macro substitutions (context-aware)
// ─────────────────────────────────────────────────────────────────────────────

/// Apply counter:/counter2: and footnote:/footnoteref: macros to @p text,
/// updating the mutable state in @p ctx.
[[nodiscard]] inline std::string sub_macros_with_ctx(
        const std::string& text,
        InlineContext& ctx)
{
    std::string out = text;

    // ── counter: / counter2: ─────────────────────────────────────────────────
    if (out.find("counter") != std::string::npos && ctx.counters) {
        std::string after;
        after.reserve(out.size());
        const std::size_t n = out.size();

        for (std::size_t i = 0; i < n; ) {
            // Check for "counter:" or "counter2:"
            if (i + 8 <= n && out[i]=='c' && out[i+1]=='o' && out[i+2]=='u' &&
                out[i+3]=='n' && out[i+4]=='t' && out[i+5]=='e' && out[i+6]=='r') {
                std::size_t j = i + 7;
                const bool is2 = (j < n && out[j] == '2');
                if (is2) { ++j; }
                if (j < n && out[j] == ':') {
                    ++j;
                    // Parse name: \w[\w\-]*
                    const std::size_t name_s = j;
                    while (j < n && (std::isalnum(static_cast<unsigned char>(out[j])) ||
                                     out[j] == '_' || out[j] == '-')) {
                        ++j;
                    }
                    if (j > name_s) {
                        std::string name(out, name_s, j - name_s);
                        // Optional [initial]
                        std::string initial = "1";
                        if (j < n && out[j] == '[') {
                            const std::size_t cls = out.find(']', j + 1);
                            if (cls != std::string::npos) {
                                initial = out.substr(j + 1, cls - j - 1);
                                j = cls + 1;
                            }
                        }

                        const bool alpha = !initial.empty() &&
                                           std::isalpha(static_cast<unsigned char>(initial[0]));
                        auto cit = ctx.counters->find(name);
                        int val;
                        if (cit == ctx.counters->end()) {
                            if (alpha) {
                                val = static_cast<int>(
                                    std::tolower(static_cast<unsigned char>(initial[0])) - 'a' + 1);
                            } else {
                                try { val = std::stoi(initial); } catch (...) { val = 1; }
                            }
                            (*ctx.counters)[name] = val;
                        } else {
                            ++cit->second;
                            val = cit->second;
                        }

                        if (!is2) {  // counter2: emits nothing
                            if (alpha) {
                                after += static_cast<char>('a' + (val - 1));
                            } else {
                                after += std::to_string(val);
                            }
                        }
                        i = j;
                        continue;
                    }
                }
            }
            after += out[i++];
        }
        out = std::move(after);
    }

    // ── footnote:[text] / footnoteref:[id,text] ───────────────────────────────
    if (out.find("footnote") != std::string::npos && ctx.footnotes) {
        std::string after;
        after.reserve(out.size());
        const std::size_t n = out.size();

        for (std::size_t i = 0; i < n; ) {
            // Check for "footnote:" or "footnoteref:"
            if (i + 9 <= n && out.compare(i, 8, "footnote") == 0) {
                std::size_t j = i + 8;
                const bool is_ref = (j + 3 <= n && out.compare(j, 3, "ref") == 0);
                if (is_ref) { j += 3; }
                if (j < n && out[j] == ':') {
                    ++j;
                    if (j < n && out[j] == '[') {
                        const std::size_t cls = out.find(']', j + 1);
                        if (cls != std::string::npos) {
                            std::string content(out, j + 1, cls - j - 1);
                            std::string fn_id;
                            std::string fn_text;
                            const auto comma = content.find(',');
                            if (comma != std::string::npos) {
                                fn_id   = content.substr(0, comma);
                                fn_text = content.substr(comma + 1);
                                while (!fn_text.empty() && fn_text.front() == ' ') { fn_text.erase(0, 1); }
                            } else {
                                fn_text = content;
                            }

                            int fn_num = 0;
                            if (!fn_id.empty()) {
                                for (const auto& e : *ctx.footnotes) {
                                    if (e.id == fn_id) { fn_num = e.number; break; }
                                }
                            }
                            if (fn_num == 0) {
                                fn_num = static_cast<int>(ctx.footnotes->size()) + 1;
                                ctx.footnotes->push_back({fn_num, fn_id, fn_text});
                            }

                            const std::string fn_s = std::to_string(fn_num);
                            after += "<sup class=\"footnote\">"
                                     "<a id=\"_footnoteref_" + fn_s + "\" "
                                     "class=\"footnote\" "
                                     "href=\"#_footnotedef_" + fn_s + "\" "
                                     "title=\"View footnote.\">" +
                                     fn_s + "</a></sup>";
                            i = cls + 1;
                            continue;
                        }
                    }
                }
            }
            after += out[i++];
        }
        out = std::move(after);
    }

    return out;
}

/// Replace trailing " +" with <br>.
[[nodiscard]] inline std::string sub_post_replacements(const std::string& text) {
    const std::size_t n = text.size();
    if (n >= 2 && text[n-1] == '+' && text[n-2] == ' ') {
        return std::string(text, 0, n - 2) + "<br>";
    }
    return text;
}

// ─────────────────────────────────────────────────────────────────────────────
// Combined pipelines
// ─────────────────────────────────────────────────────────────────────────────

/// Apply the "normal" substitution pipeline to inline text:
///   pass-extract → specialcharacters → quotes → attributes →
///   replacements → macros → post → pass-restore
/// Optionally accepts an @p ctx for counter: and footnote: macro processing.
[[nodiscard]] inline std::string apply_normal_subs(
        const std::string&                                    text,
        const std::unordered_map<std::string, std::string>&   attrs,
        InlineContext* ctx = nullptr)
{
    // Extract pass:[] regions first so they are protected from all other subs.
    std::vector<std::string> pass_stash;
    std::string s = extract_pass_macros(text, pass_stash);

    s = sub_specialchars(s);
    s = sub_quotes(s);
    s = sub_attributes(s, attrs);
    s = sub_replacements(s);
    s = sub_macros(s);
    if (ctx) { s = sub_macros_with_ctx(s, *ctx); }
    s = sub_post_replacements(s);

    // Restore any pass:[] regions that were extracted.
    if (!pass_stash.empty()) { s = restore_pass_macros(s, pass_stash); }
    return s;
}

/// Apply only special-character escaping (for verbatim / listing blocks).
[[nodiscard]] inline std::string apply_verbatim_subs(const std::string& text) {
    // Strip trailing whitespace from each line to match Asciidoctor's behaviour,
    // then apply special character escaping.
    std::string stripped;
    stripped.reserve(text.size());
    std::size_t line_start = 0;
    while (line_start <= text.size()) {
        std::size_t nl = text.find('\n', line_start);
        std::size_t line_end = (nl == std::string::npos) ? text.size() : nl;
        // Find last non-space character in this line
        std::size_t last_non_space = line_end;
        while (last_non_space > line_start &&
               (text[last_non_space - 1] == ' ' || text[last_non_space - 1] == '\t')) {
            --last_non_space;
        }
        stripped.append(text, line_start, last_non_space - line_start);
        if (nl != std::string::npos) {
            stripped += '\n';
            line_start = nl + 1;
        } else {
            break;
        }
    }
    return sub_specialchars(stripped);
}

/// Apply header-level subs: specialcharacters + attributes.
[[nodiscard]] inline std::string apply_header_subs(
        const std::string&                                    text,
        const std::unordered_map<std::string, std::string>&   attrs)
{
    std::string s = sub_specialchars(text);
    return sub_attributes(s, attrs);
}

// ─────────────────────────────────────────────────────────────────────────────
// ID generation helpers  (mirrors Asciidoctor's Section#generate_id)
// ─────────────────────────────────────────────────────────────────────────────

/// Convert a section title to a valid HTML id attribute value.
///
/// Algorithm:
///   1. Lower-case the title.
///   2. Strip any HTML tags.
///   3. Replace sequences of non-word / non-hyphen characters with the
///      id-separator (default "_").
///   4. Prepend the id-prefix   (default "_").
///   5. Strip leading/trailing separators.
[[nodiscard]] inline std::string generate_id(
        const std::string& title,
        const std::string& prefix    = "_",
        const std::string& separator = "_")
{
    // 1. Lowercase
    std::string id;
    id.reserve(title.size());
    for (char c : title) {
        id += static_cast<char>(
            (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c);
    }

    // 2. Strip HTML tags: anything between < and >
    {
        std::string out2;
        out2.reserve(id.size());
        bool in_tag = false;
        for (char c2 : id) {
            if (c2 == '<') { in_tag = true; }
            else if (c2 == '>') { in_tag = false; }
            else if (!in_tag) { out2 += c2; }
        }
        id = std::move(out2);
    }
    // 3. Strip HTML entities: &...; sequences
    {
        std::string out2;
        out2.reserve(id.size());
        bool in_ent = false;
        for (char c2 : id) {
            if (c2 == '&') { in_ent = true; }
            else if (c2 == ';' && in_ent) { in_ent = false; }
            else if (!in_ent) { out2 += c2; }
        }
        id = std::move(out2);
    }

    // 4. Replace runs of unwanted characters with the separator
    std::string clean;
    clean.reserve(id.size());
    bool in_sep = false;
    for (char c : id) {
        bool ok = (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') ||
                  c == '-' || c == '_' || c == '.';
        if (ok) {
            clean += c;
            in_sep = false;
        } else {
            if (!in_sep && !clean.empty()) {
                clean += separator;
                in_sep = true;
            }
        }
    }
    // Trim trailing separator
    while (!clean.empty() && clean.back() == separator[0]) {
        clean.pop_back();
    }

    return prefix + clean;
}

} // namespace asciiquack
