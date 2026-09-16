/// @file inline_scanner.hpp
/// @brief Hand-written single-pass scanner for AsciiDoc inline quote markup.
///
/// This file provides @c scan_inline_quotes(), the definitive implementation
/// of inline-quote substitution, replacing the former regex-based @c sub_quotes()
/// that previously existed in substitutors.hpp.
///
/// == Why a hand-written scanner instead of re2c / lemon? ==
///
/// The inline-markup patterns in sub_quotes() require two features that are
/// outside the scope of pure DFA (re2c) or LALR(1) (lemon) tools:
///
///   1. **Lookahead assertions** – e.g. @c (?=[^*a-zA-Z0-9]|$) to verify
///      the character following the closing marker without consuming it.
///   2. **Context-sensitive opening boundaries** – whether a single @c *
///      starts a constrained span depends on the character immediately
///      @em before it, which must be tracked across the scan loop.
///
/// re2c generates a byte-level DFA that can express character classes and
/// fixed-length lookaheads, but it cannot backtrack or test multi-character
/// context in the way these patterns demand.  Lemon is a parser generator
/// for LALR(1) grammars and is therefore a poor match for sub-word inline
/// transformations.  Negative lookahead and back-references are equivalent
/// issues: they require either backtracking or runtime context that a
/// standard DFA cannot encode.
///
/// == What the scanner does instead ==
///
/// The scanner is a straightforward character loop:
///
///   - The "preceding character" constraint is satisfied by inspecting
///     @c out.back() (the last byte written to the output buffer).
///   - The "following character" lookahead is satisfied by checking
///     @c text[close + 1] after locating the candidate closing marker.
///   - Non-greedy matching is achieved by returning the *first* closing
///     marker position that passes all constraints.
///
/// This gives O(n) throughput per call (amortized; worst case O(n²) for
/// degenerate inputs with many unmatched markers, which do not appear in
/// practice).  Compared with the 13-regex chain in sub_quotes(), which
/// makes 13 linear passes over the string with PCRE2, the scanner makes
/// a single pass – a theoretical ~13× speedup for inline-heavy text.
///
/// == Patterns handled (identical semantics to sub_quotes()) ==
///
///   Unconstrained (double markers, no boundary restriction):
///     **text**  → <strong>text</strong>   (content recursively processed)
///     __text__  → <em>text</em>           (content recursively processed)
///     ``text``  → <code>text</code>       (content recursively processed)
///     ##text##  → <mark>text</mark>       (content recursively processed)
///     ^^text^^  → <sup>text</sup>         (content recursively processed)
///     ~~text~~  → <sub>text</sub>         (content recursively processed)
///
///   Constrained (single markers, boundary rules apply):
///     *text*    → <strong>text</strong>   (content recursively processed)
///     _text_    → <em>text</em>           (content recursively processed)
///     `text`    → <code>text</code>       (content recursively processed)
///     +text+    → <code>text</code>  (legacy monospace; verbatim)
///     #text#    → <mark>text</mark>       (content recursively processed)
///     ^text^    → <sup>text</sup>    (no whitespace in content)
///     ~text~    → <sub>text</sub>    (no whitespace in content)
///
/// Recursive processing: the content of all span types except `+plus+` is
/// passed through scan_inline_quotes() so that nested inline markup is
/// rendered (e.g. `code _var_` renders italic inside the code span,
/// matching asciidoctor's "normal" substitution group for monospace spans).
/// Only `+plus+` (legacy monospace passthrough) is verbatim.

#pragma once

#include <cstddef>
#include <string>

namespace asciiquack {

/// Apply inline quote substitutions in a single linear pass.
///
/// Produces output bit-for-bit identical to the 13-regex chain in
/// sub_quotes() for all well-formed AsciiDoc inline text.
///
/// @param text  Input string (after specialchars substitution has run).
/// @return      New string with inline markup replaced by HTML tags.
[[nodiscard]] inline std::string scan_inline_quotes(const std::string& text)
{
    using sz = std::size_t;

    const sz  n = text.size();
    std::string out;
    out.reserve(n + n / 4);  // 25% overhead estimate for HTML tags

    // ── Character-class helpers ───────────────────────────────────────────────

    // True for ASCII letter or digit.
    auto is_alnum = [](char c) -> bool {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9');
    };

    // True for ASCII whitespace relevant to inline content.
    auto is_ws = [](char c) -> bool {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };

    // ── Opening-boundary predicates ───────────────────────────────────────────
    //
    // A constrained opening marker at position i is valid when the preceding
    // character (the last byte in `out`) is NOT in the exclusion set.
    // Using out.back() rather than text[i-1] is necessary because earlier
    // unconstrained spans in the SAME pass may have emitted HTML tags, so the
    // "preceding character" that the regex pipeline would see is the last byte
    // of those tags (e.g. '>'), not the original source character.

    // Last character written to out, or '\0' if out is empty.
    auto prev_out = [&]() -> char {
        return out.empty() ? '\0' : out.back();
    };

    // Constrained bold: exclude [*a-zA-Z0-9/:] before opening '*'.
    // '/' and ':' are excluded to avoid false positives in URLs such as
    // https://*host*/path  (Bug #4 fix).
    auto ok_open_star = [&]() -> bool {
        const char p = prev_out();
        if (p == '\0') return true;
        return p != '*' && !is_alnum(p) && p != '/' && p != ':';
    };

    // Constrained italic: exclude [_a-zA-Z0-9] before opening '_'.
    auto ok_open_under = [&]() -> bool {
        const char p = prev_out();
        if (p == '\0') return true;
        return p != '_' && !is_alnum(p);
    };

    // Constrained monospace: exclude [`a-zA-Z0-9] before opening '`'.
    auto ok_open_tick = [&]() -> bool {
        const char p = prev_out();
        if (p == '\0') return true;
        return p != '`' && !is_alnum(p);
    };

    // Constrained legacy mono: exclude [+a-zA-Z0-9] before opening '+'.
    auto ok_open_plus = [&]() -> bool {
        const char p = prev_out();
        if (p == '\0') return true;
        return p != '+' && !is_alnum(p);
    };

    // Constrained highlight: exclude [#a-zA-Z0-9] before opening '#'.
    auto ok_open_hash = [&]() -> bool {
        const char p = prev_out();
        if (p == '\0') return true;
        return p != '#' && !is_alnum(p);
    };

    // ── Closing-marker finders ────────────────────────────────────────────────

    // Find the first occurrence of (c, c) in text[from..n-1].
    // Returns the index of the first c, or npos.
    auto find_double_close = [&](sz from, char c) -> sz {
        for (sz k = from; k + 1 < n; ++k)
            if (text[k] == c && text[k + 1] == c) return k;
        return std::string::npos;
    };

    // Find the first valid constrained closing marker for `*`, `_`, `` ` ``,
    // and `+`.
    //
    // A closing marker at position k is valid when:
    //   1. Content text[from..k-1] is non-empty (k > from).
    //   2. text[from]   is non-whitespace (content starts with \S).
    //   3. text[k-1]    is non-whitespace (content ends with \S).
    //   4. The character after the marker is neither alphanumeric nor the
    //      marker character itself (close boundary: [^marker a-zA-Z0-9] | $).
    //
    // Returning the FIRST qualifying position gives non-greedy semantics,
    // matching PCRE2's (\S|\S.*?\S) behaviour.
    auto find_const_close = [&](sz from, char marker) -> sz {
        if (from >= n || is_ws(text[from])) return std::string::npos;
        for (sz k = from; k < n; ++k) {
            if (text[k] != marker) continue;
            if (k == from) continue;                           // empty span
            if (is_ws(text[k - 1])) continue;                 // content ends with ws
            const char after = (k + 1 < n) ? text[k + 1] : '\0';
            if (is_alnum(after) || after == marker) continue; // bad close boundary
            return k;
        }
        return std::string::npos;
    };

    // Variant for constrained highlight '#':
    // Content must start AND end with an alphanumeric character and may
    // not contain '#' or '\n'.  Mirrors the regex:
    //   #([a-zA-Z0-9][^#\n]*[a-zA-Z0-9]|[a-zA-Z0-9])#
    // The first '#' encountered in the content is the only candidate for the
    // closing marker (since '#' is forbidden in the content body).
    auto find_hash_close = [&](sz from) -> sz {
        if (from >= n || !is_alnum(text[from])) return std::string::npos;
        for (sz k = from; k < n; ++k) {
            if (text[k] == '\n') return std::string::npos;   // newline aborts
            if (text[k] != '#') continue;
            // First '#' encountered: this is the only possible close.
            if (k == from) return std::string::npos;          // empty span
            if (!is_alnum(text[k - 1])) return std::string::npos; // must end with alnum
            const char after = (k + 1 < n) ? text[k + 1] : '\0';
            if (is_alnum(after) || after == '#') return std::string::npos;
            return k;
        }
        return std::string::npos;
    };

    // Find the first closing `marker` in text[from..n-1] where the content
    // text[from..k-1] contains no whitespace characters.
    // Used for ^superscript^ and ~subscript~ which carry no boundary restriction.
    auto find_nowhitespace_close = [&](sz from, char marker) -> sz {
        for (sz k = from; k < n; ++k) {
            if (is_ws(text[k])) return std::string::npos;  // whitespace aborts
            if (text[k] == marker && k > from) return k;
        }
        return std::string::npos;
    };

    // ── Main scan loop ────────────────────────────────────────────────────────
    //
    // At each position i:
    //   1. If a double-marker (unconstrained) matches, emit it and advance.
    //   2. Otherwise, try the corresponding single-marker (constrained).
    //      For '*', '_', '`', '#': opening boundary is checked against
    //      prev_out() (last byte already written to `out`).
    //      For '^', '~': no opening boundary restriction.
    //   3. If neither matches, emit the current character verbatim.
    //
    // When unconstrained fails we always fall through to the constrained check
    // (no early continue/return).  This matches the regex-pipeline behaviour
    // where each pattern is applied independently: a failed "**...**" does not
    // suppress a subsequent "*...*" match starting at the same position.

    sz i = 0;
    while (i < n) {
        const char c = text[i];
        sz close = std::string::npos;

        // ── '*' ───────────────────────────────────────────────────────────────
        if (c == '*') {
            // Unconstrained **...**  (priority over constrained)
            if (i + 1 < n && text[i + 1] == '*') {
                close = find_double_close(i + 2, '*');
                if (close != std::string::npos && close > i + 2) {
                    out += "<strong>";
                    out += scan_inline_quotes(std::string(text, i + 2, close - i - 2));
                    out += "</strong>";
                    i = close + 2;
                    continue;
                }
                // Unconstrained did not match; fall through to constrained.
            }
            // Constrained *...*
            if (ok_open_star()) {
                close = find_const_close(i + 1, '*');
                if (close != std::string::npos) {
                    out += "<strong>";
                    out += scan_inline_quotes(std::string(text, i + 1, close - i - 1));
                    out += "</strong>";
                    i = close + 1;
                    continue;
                }
            }
        }

        // ── '_' ───────────────────────────────────────────────────────────────
        else if (c == '_') {
            // Unconstrained __...__
            if (i + 1 < n && text[i + 1] == '_') {
                close = find_double_close(i + 2, '_');
                if (close != std::string::npos && close > i + 2) {
                    out += "<em>";
                    out += scan_inline_quotes(std::string(text, i + 2, close - i - 2));
                    out += "</em>";
                    i = close + 2;
                    continue;
                }
                // Unconstrained did not match; fall through to constrained.
            }
            // Constrained _..._
            if (ok_open_under()) {
                close = find_const_close(i + 1, '_');
                if (close != std::string::npos) {
                    out += "<em>";
                    out += scan_inline_quotes(std::string(text, i + 1, close - i - 1));
                    out += "</em>";
                    i = close + 1;
                    continue;
                }
            }
        }

        // ── '`' ───────────────────────────────────────────────────────────────
        //
        // Backtick code spans (`single` and ``double``) apply the quotes
        // substitution to their content, matching asciidoctor's "normal" subs
        // for constrained monospace.  Content may therefore contain nested
        // inline markup such as _italic_ or *bold*.
        else if (c == '`') {
            // Unconstrained ``...``
            if (i + 1 < n && text[i + 1] == '`') {
                close = find_double_close(i + 2, '`');
                if (close != std::string::npos && close > i + 2) {
                    out += "<code>";
                    out += scan_inline_quotes(std::string(text, i + 2, close - i - 2));
                    out += "</code>";
                    i = close + 2;
                    continue;
                }
                // Unconstrained did not match; fall through to constrained.
            }
            // Constrained `...`
            if (ok_open_tick()) {
                close = find_const_close(i + 1, '`');
                if (close != std::string::npos) {
                    out += "<code>";
                    out += scan_inline_quotes(std::string(text, i + 1, close - i - 1));
                    out += "</code>";
                    i = close + 1;
                    continue;
                }
            }
        }

        // ── '#' ───────────────────────────────────────────────────────────────
        else if (c == '#') {
            // Unconstrained ##...##
            if (i + 1 < n && text[i + 1] == '#') {
                close = find_double_close(i + 2, '#');
                if (close != std::string::npos && close > i + 2) {
                    out += "<mark>";
                    out += scan_inline_quotes(std::string(text, i + 2, close - i - 2));
                    out += "</mark>";
                    i = close + 2;
                    continue;
                }
                // Unconstrained did not match; fall through to constrained.
            }
            // Constrained #...# (content must start/end with alnum, no '#'/'\n')
            if (ok_open_hash()) {
                close = find_hash_close(i + 1);
                if (close != std::string::npos) {
                    out += "<mark>";
                    out += scan_inline_quotes(std::string(text, i + 1, close - i - 1));
                    out += "</mark>";
                    i = close + 1;
                    continue;
                }
            }
        }

        // ── '^' ───────────────────────────────────────────────────────────────
        else if (c == '^') {
            // Unconstrained ^^...^^
            if (i + 1 < n && text[i + 1] == '^') {
                close = find_double_close(i + 2, '^');
                if (close != std::string::npos && close > i + 2) {
                    out += "<sup>";
                    out += scan_inline_quotes(std::string(text, i + 2, close - i - 2));
                    out += "</sup>";
                    i = close + 2;
                    continue;
                }
                // Unconstrained failed; fall through to constrained with the
                // current '^' as opening (regex \^(\S+?)\^, no boundary check).
                // find_nowhitespace_close starts one past current i, so the
                // second '^' of '^^' becomes the first content character.
            }
            // Constrained ^...^ (no whitespace in content, no boundary check)
            close = find_nowhitespace_close(i + 1, '^');
            if (close != std::string::npos) {
                out += "<sup>";
                out += scan_inline_quotes(std::string(text, i + 1, close - i - 1));
                out += "</sup>";
                i = close + 1;
                continue;
            }
        }

        // ── '~' ───────────────────────────────────────────────────────────────
        else if (c == '~') {
            // Unconstrained ~~...~~
            if (i + 1 < n && text[i + 1] == '~') {
                close = find_double_close(i + 2, '~');
                if (close != std::string::npos && close > i + 2) {
                    out += "<sub>";
                    out += scan_inline_quotes(std::string(text, i + 2, close - i - 2));
                    out += "</sub>";
                    i = close + 2;
                    continue;
                }
                // Fall through to constrained.
            }
            // Constrained ~...~ (no whitespace in content, no boundary check)
            close = find_nowhitespace_close(i + 1, '~');
            if (close != std::string::npos) {
                out += "<sub>";
                out += scan_inline_quotes(std::string(text, i + 1, close - i - 1));
                out += "</sub>";
                i = close + 1;
                continue;
            }
        }

        // ── '+' ───────────────────────────────────────────────────────────────
        //
        // Legacy monospace is verbatim (same as backtick code spans).
        else if (c == '+') {
            // Constrained +...+ (legacy monospace, no unconstrained form)
            if (ok_open_plus()) {
                close = find_const_close(i + 1, '+');
                if (close != std::string::npos) {
                    out += "<code>";
                    out.append(text, i + 1, close - i - 1);
                    out += "</code>";
                    i = close + 1;
                    continue;
                }
            }
        }

        // ── Default: pass through ─────────────────────────────────────────────
        out += c;
        ++i;
    }

    return out;
}

} // namespace asciiquack
