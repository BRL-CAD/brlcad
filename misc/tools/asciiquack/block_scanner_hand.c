/// @file block_scanner_hand.c
/// @brief Hand-written AsciiDoc block-line scanner and attribute-list parser.
///
/// Provides two public C functions, both without any generated code:
///
///   AqBlockScanResult aq_scan_block_line(const char *line, size_t len);
///   int aq_parse_attr_list(const char *content, size_t len,
///                          AqAttrCallback cb, void *userdata);
///
/// == Performance characteristics ==
///
///   1. memchr() for description-list detection.
///      The hot path for TEXT lines is scanning for "::" or ";;".
///      memchr() uses SIMD on modern glibc and processes 16–32 bytes per
///      cycle.
///
///   2. Single-pass classification + capture extraction done together.
///
///   3. memcmp() prefix matching for keyword macros.
///      "image::", "ifdef::", etc. are matched with short memcmp() calls
///      that the compiler emits as word-level integer comparisons.
///
///   4. Early exit for the most common types (TEXT, BLANK).

#include "block_scanner.h"
#include <string.h>
#include <stddef.h>

/* ── Internal helpers ─────────────────────────────────────────────────────── */

/** Build an AqSpan from two pointers within [base, base+len]. */
static AqSpan make_span(const char *base, const char *s, const char *e)
{
    AqSpan sp;
    sp.off = (unsigned short)(s - base);
    sp.len = (e > s) ? (unsigned short)(e - s) : 0u;
    return sp;
}

/** Skip leading ASCII whitespace; return pointer to first non-WS byte. */
static const char *skip_ws(const char *p, const char *pe)
{
    while (p < pe && (*p == ' ' || *p == '\t')) ++p;
    return p;
}

/** Return pointer to first byte past trailing ASCII whitespace. */
static const char *rskip_ws(const char *p, const char *pe)
{
    while (pe > p && (*(pe - 1) == ' ' || *(pe - 1) == '\t')) --pe;
    return pe;
}

/** True if c is an ASCII letter or digit (used in attr-entry / roman checks). */
static int is_word(char c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_';
}

/** True if c is in the set of valid roman-numeral characters used by AsciiDoc. */
static int is_roman(char c)
{
    return c == 'I' || c == 'V' || c == 'X' ||
           c == 'i' || c == 'v' || c == 'x';
}

/* ── Thematic-break pre-check ─────────────────────────────────────────────── */
/*
 * Detects:
 *   a) Three or more single-quote characters: '''  '''''  etc.
 *   b) ^{0,3}([-*_])( *)\1\2\1$ — exactly three of the same char, equal gaps.
 */
static int is_thematic_break(const char *line, size_t len)
{
    if (len < 3) return 0;

    /* a) Three or more single-quotes */
    {
        size_t i = 0;
        while (i < len && line[i] == '\'') ++i;
        if (i == len && i >= 3) return 1;
    }

    /* b) Optional 0-3 leading spaces, then ch [sp*] ch [sp*] ch EOL */
    size_t start = 0;
    while (start < 3 && start < len && line[start] == ' ') ++start;
    if (start >= len) return 0;

    char ch = line[start];
    if (ch != '-' && ch != '*' && ch != '_') return 0;

    const char *p  = line + start;
    const char *pe = line + len;
    ++p; /* skip first ch */

    const char *sp1_start = p;
    while (p < pe && *p == ' ') ++p;
    size_t sp1 = (size_t)(p - sp1_start);

    if (p >= pe || *p != ch) return 0;
    ++p; /* skip second ch */

    const char *sp2_start = p;
    while (p < pe && *p == ' ') ++p;
    size_t sp2 = (size_t)(p - sp2_start);

    if (sp1 != sp2) return 0;
    if (p >= pe || *p != ch) return 0;
    ++p; /* skip third ch */

    return (p == pe);
}

/* ── Description-list separator search ───────────────────────────────────── */
/*
 * Search for a valid "::" / ":::" / "::::" or ";;" separator in [start, pe).
 * The separator must be followed by whitespace or end-of-line.
 *
 * Uses memchr() for SIMD-accelerated ':' / ';' lookup so that the hot case
 * (TEXT lines with no separator) returns immediately without scanning every
 * byte individually.
 *
 * Returns pointer to the first byte of the separator, or NULL if none found.
 * On success *sep_len is set to the separator length (2, 3, or 4 for "::").
 */
static const char *find_descript_sep(const char *start, const char *pe,
                                     size_t *sep_len)
{
    const char *q;

    /* Search for '::' */
    for (q = start; q < pe; ) {
        const char *colon = (const char *)memchr(q, ':', (size_t)(pe - q));
        if (!colon) break;
        if (colon + 1 < pe && colon[1] == ':') {
            /* Count extended ':' for ':::' or '::::' */
            size_t ext = 0;
            while (ext < 2 && colon + 2 + ext < pe && colon[2 + ext] == ':')
                ++ext;
            const char *after = colon + 2 + ext;
            if (after >= pe || *after == ' ' || *after == '\t') {
                *sep_len = 2 + ext;
                return colon;
            }
        }
        q = colon + 1;
    }

    /* Search for ';;' */
    for (q = start; q < pe; ) {
        const char *semi = (const char *)memchr(q, ';', (size_t)(pe - q));
        if (!semi) break;
        if (semi + 1 < pe && semi[1] == ';') {
            const char *after = semi + 2;
            if (after >= pe || *after == ' ' || *after == '\t') {
                *sep_len = 2;
                return semi;
            }
        }
        q = semi + 1;
    }

    return NULL;
}

/* ── Build a DESCRIPT result from a known separator position ─────────────── */
static AqBlockScanResult make_descript(const char *line, const char *pe,
                                       const char *term_start,
                                       const char *sep, size_t sep_len)
{
    AqBlockScanResult r;
    memset(&r, 0, sizeof(r));
    r.type    = AQ_BT_LIST_DESCRIPT;
    r.caps[0] = make_span(line, term_start, sep);
    r.caps[1] = make_span(line, sep, sep + sep_len);
    const char *body = skip_ws(sep + sep_len, pe);
    r.caps[2] = make_span(line, body, pe);
    return r;
}

/* ── Keyword-macro helpers ────────────────────────────────────────────────── */
/*
 * Try to match a line starting with a known keyword followed by "::".
 * If matched, extract target (between "::" and '[') and attrs (inside '[...]').
 * Returns an IMAGE or MEDIA result, or a zeroed-out result with type TEXT.
 *
 * For IMAGE:   caps[0]=target, caps[1]=attrs
 * For MEDIA:   caps[0]=macro-name, caps[1]=target, caps[2]=attrs
 */
static AqBlockScanResult try_keyword_macro(const char *line, size_t len,
                                           AqBlockToken type,
                                           size_t kw_len)
{
    AqBlockScanResult r;
    memset(&r, 0, sizeof(r));
    r.type = AQ_BT_TEXT;

    /* line[kw_len..kw_len+1] must be "::" */
    if (len < kw_len + 2) return r;
    /* target starts after "::" */
    const char *tgt = line + kw_len + 2;
    const char *pe  = line + len;
    if (tgt >= pe || *tgt == '\0') return r; /* target must be non-empty */

    /* Find '[' */
    const char *bracket = (const char *)memchr(tgt, '[', (size_t)(pe - tgt));
    if (!bracket || bracket == tgt) return r; /* '[' required and target non-empty */

    /* Line must end with ']' */
    if (pe[-1] != ']') return r;

    r.type = type;
    if (type == AQ_BT_BLOCK_IMAGE || type == AQ_BT_INCLUDE) {
        r.caps[0] = make_span(line, tgt, bracket);
        r.caps[1] = make_span(line, bracket + 1, pe - 1); /* inside [...] */
    } else {
        /* BLOCK_MEDIA: caps[0]=macro name, caps[1]=target, caps[2]=attrs */
        r.caps[0] = make_span(line, line, line + kw_len);
        r.caps[1] = make_span(line, tgt, bracket);
        r.caps[2] = make_span(line, bracket + 1, pe - 1);
    }
    return r;
}

/* ── Conditional-directive helpers ────────────────────────────────────────── */
/* "ifdef::attr[]" / "ifndef::attr[]" / "endif::attr[]"
 *   caps[0] = attribute name (between "::" and '[')                          */
static AqBlockScanResult try_conditional(const char *line, size_t len,
                                         AqBlockToken type, size_t kw_len)
{
    AqBlockScanResult r;
    memset(&r, 0, sizeof(r));
    r.type = AQ_BT_TEXT;

    if (len < kw_len + 2) return r;
    const char *attr = line + kw_len + 2; /* after "::" */
    const char *pe   = line + len;
    if (pe[-1] != ']') return r;

    const char *bracket = (const char *)memchr(attr, '[', (size_t)(pe - attr));
    if (!bracket) return r;

    r.type    = type;
    r.caps[0] = make_span(line, attr, bracket);
    return r;
}

/* "ifeval::[expr]"
 *   caps[0] = expression (inside '[...]')                                     */
static AqBlockScanResult try_ifeval(const char *line, size_t len)
{
    /* "ifeval::[" = 9 chars */
    if (len < 10) return (AqBlockScanResult){ .type = AQ_BT_TEXT };
    if (memcmp(line, "ifeval::[", 9) != 0) return (AqBlockScanResult){ .type = AQ_BT_TEXT };
    const char *pe = line + len;
    if (pe[-1] != ']') return (AqBlockScanResult){ .type = AQ_BT_TEXT };
    AqBlockScanResult r;
    memset(&r, 0, sizeof(r));
    r.type    = AQ_BT_IFEVAL;
    r.caps[0] = make_span(line, line + 9, pe - 1);
    return r;
}

/* ── Classify a line that begins at non-WS character p ───────────────────── */
/*
 * Called for lines that may start with keyword macros, list items, or just
 * be ordinary text.  'p' is already past any leading whitespace.
 * Handles: keyword macros, list items, description lists, and TEXT.
 */
static AqBlockScanResult classify_content(const char *line, size_t len,
                                          const char *p, const char *pe)
{
    AqBlockScanResult r;
    const char c = *p;

    /* ── Unordered list: - text  or  *{1,5} text ──────────────────────── */
    if (c == '-' && p + 1 < pe && (p[1] == ' ' || p[1] == '\t')) {
        const char *text = skip_ws(p + 1, pe);
        if (text < pe) {
            memset(&r, 0, sizeof(r));
            r.type    = AQ_BT_LIST_UNORD;
            r.caps[0] = make_span(line, p, p + 1);
            r.caps[1] = make_span(line, text, pe);
            return r;
        }
    }
    if (c == '*') {
        const char *mk = p;
        while (mk < pe && *mk == '*' && (size_t)(mk - p) < 5) ++mk;
        if (mk < pe && (*mk == ' ' || *mk == '\t')) {
            const char *text = skip_ws(mk, pe);
            if (text < pe) {
                memset(&r, 0, sizeof(r));
                r.type    = AQ_BT_LIST_UNORD;
                r.caps[0] = make_span(line, p, mk);
                r.caps[1] = make_span(line, text, pe);
                return r;
            }
        }
    }

    /* ── Ordered list: dots ─────────────────────────────────────────────── */
    if (c == '.') {
        const char *mk = p;
        while (mk < pe && *mk == '.') ++mk;
        if (mk < pe && (*mk == ' ' || *mk == '\t')) {
            const char *text = skip_ws(mk, pe);
            if (text < pe) {
                memset(&r, 0, sizeof(r));
                r.type    = AQ_BT_LIST_ORD;
                r.caps[0] = make_span(line, p, mk);
                r.caps[1] = make_span(line, text, pe);
                return r;
            }
        }
    }

    /* ── Ordered list: digits ───────────────────────────────────────────── */
    if (c >= '0' && c <= '9') {
        const char *mk = p;
        while (mk < pe && *mk >= '0' && *mk <= '9') ++mk;
        if (mk < pe && *mk == '.') {
            ++mk;
            if (mk < pe && (*mk == ' ' || *mk == '\t')) {
                const char *text = skip_ws(mk, pe);
                if (text < pe) {
                    memset(&r, 0, sizeof(r));
                    r.type    = AQ_BT_LIST_ORD;
                    r.caps[0] = make_span(line, p, mk);
                    r.caps[1] = make_span(line, text, pe);
                    return r;
                }
            }
        }
    }

    /* ── Ordered list: single letter followed by '.' ────────────────────── */
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        if (p + 1 < pe && p[1] == '.') {
            if (p + 2 < pe && (p[2] == ' ' || p[2] == '\t')) {
                const char *text = skip_ws(p + 2, pe);
                if (text < pe) {
                    memset(&r, 0, sizeof(r));
                    r.type    = AQ_BT_LIST_ORD;
                    r.caps[0] = make_span(line, p, p + 2);
                    r.caps[1] = make_span(line, text, pe);
                    return r;
                }
            }
        }
    }

    /* ── Ordered list: roman numerals + ')' ────────────────────────────── */
    if (is_roman(c)) {
        const char *mk = p;
        while (mk < pe && is_roman(*mk)) ++mk;
        if (mk < pe && *mk == ')') {
            ++mk;
            if (mk < pe && (*mk == ' ' || *mk == '\t')) {
                const char *text = skip_ws(mk, pe);
                if (text < pe) {
                    memset(&r, 0, sizeof(r));
                    r.type    = AQ_BT_LIST_ORD;
                    r.caps[0] = make_span(line, p, mk);
                    r.caps[1] = make_span(line, text, pe);
                    return r;
                }
            }
        }
    }

    /* ── Keyword macros (only when no leading WS, i.e. p == line) ───────── */
    if (p == line) {
        size_t rem = (size_t)(pe - p);

        /* image:: */
        if (rem >= 9 && memcmp(p, "image::", 7) == 0) {
            AqBlockScanResult m = try_keyword_macro(line, len,
                                                    AQ_BT_BLOCK_IMAGE, 5);
            if (m.type == AQ_BT_BLOCK_IMAGE) return m;
        }
        /* video:: */
        if (rem >= 9 && memcmp(p, "video::", 7) == 0) {
            AqBlockScanResult m = try_keyword_macro(line, len,
                                                    AQ_BT_BLOCK_MEDIA, 5);
            if (m.type == AQ_BT_BLOCK_MEDIA) return m;
        }
        /* audio:: */
        if (rem >= 9 && memcmp(p, "audio::", 7) == 0) {
            AqBlockScanResult m = try_keyword_macro(line, len,
                                                    AQ_BT_BLOCK_MEDIA, 5);
            if (m.type == AQ_BT_BLOCK_MEDIA) return m;
        }
        /* ifdef:: */
        if (rem >= 9 && memcmp(p, "ifdef::", 7) == 0) {
            AqBlockScanResult m = try_conditional(line, len, AQ_BT_IFDEF, 5);
            if (m.type == AQ_BT_IFDEF) return m;
        }
        /* ifndef:: */
        if (rem >= 10 && memcmp(p, "ifndef::", 8) == 0) {
            AqBlockScanResult m = try_conditional(line, len, AQ_BT_IFNDEF, 6);
            if (m.type == AQ_BT_IFNDEF) return m;
        }
        /* ifeval:: */
        if (rem >= 10 && c == 'i' && p[1] == 'f' && p[2] == 'e') {
            AqBlockScanResult m = try_ifeval(line, len);
            if (m.type == AQ_BT_IFEVAL) return m;
        }
        /* endif:: */
        if (rem >= 9 && memcmp(p, "endif::", 7) == 0) {
            AqBlockScanResult m = try_conditional(line, len, AQ_BT_ENDIF, 5);
            if (m.type == AQ_BT_ENDIF) return m;
        }
        /* include:: */
        if (rem >= 11 && memcmp(p, "include::", 9) == 0) {
            AqBlockScanResult m = try_keyword_macro(line, len,
                                                    AQ_BT_INCLUDE, 7);
            if (m.type == AQ_BT_INCLUDE) return m;
        }
    }

    /* ── Description list / TEXT: use memchr for fast ':' / ';' scan ────── */
    size_t sep_len = 0;
    const char *sep = find_descript_sep(p, pe, &sep_len);
    if (sep) {
        return make_descript(line, pe, p, sep, sep_len);
    }

    memset(&r, 0, sizeof(r));
    r.type = AQ_BT_TEXT;
    return r;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

AqBlockScanResult aq_scan_block_line(const char *line, size_t len)
{
    AqBlockScanResult r;
    memset(&r, 0, sizeof(r));

    /* 1. Thematic-break pre-check (handles ''' and --- / * * * / _ _ _). */
    if (is_thematic_break(line, len)) {
        r.type = AQ_BT_THEMATIC_BREAK;
        return r;
    }

    const char *pe = line + len;

    /* 2. Blank: empty line or NUL sentinel. */
    if (len == 0 || line[0] == '\0') { r.type = AQ_BT_BLANK; return r; }

    const char c0 = line[0];

    /* 3. Leading whitespace: blank, or a line that starts with list marker
     *    or content after indentation.                                       */
    if (c0 == ' ' || c0 == '\t') {
        const char *p = skip_ws(line, pe);
        if (p == pe) { r.type = AQ_BT_BLANK; return r; }
        return classify_content(line, len, p, pe);
    }

    /* ── All remaining cases have no leading whitespace ─────────────────── */

    /* 4. Page break: <<< (three or more '<' chars, nothing else). */
    if (c0 == '<') {
        /* Could also be a callout list item: <N> text */
        if (len >= 3 && line[1] == '<' && line[2] == '<') {
            size_t k = 3;
            while (k < len && line[k] == '<') ++k;
            if (k == len) { r.type = AQ_BT_PAGE_BREAK; return r; }
        }
        /* Callout: <N> text  or  <.> text */
        if (len >= 4 && line[1] != '<') {
            const char *p = line + 1;
            /* Skip the digit(s) or dot */
            if (*p == '.') {
                ++p;
            } else {
                while (p < pe && *p >= '0' && *p <= '9') ++p;
            }
            if (p < pe && *p == '>') {
                ++p;
                if (p < pe && (*p == ' ' || *p == '\t')) {
                    const char *text = skip_ws(p, pe);
                    if (text < pe) {
                        r.type    = AQ_BT_LIST_CALLOUT;
                        r.caps[0] = make_span(line, line + 1, p - 1);
                        r.caps[1] = make_span(line, text, pe);
                        return r;
                    }
                }
            }
        }
        /* Nothing matched: fall through to description/text scan. */
        return classify_content(line, len, line, pe);
    }

    /* 5. Comments: // text  or  //// (block comment). */
    if (c0 == '/') {
        if (len >= 2 && line[1] == '/') {
            /* Block-comment delimiter: 4 or more '/', nothing else */
            if (len >= 4 && line[2] == '/' && line[3] == '/') {
                size_t k = 4;
                while (k < len && line[k] == '/') ++k;
                if (k == len) { r.type = AQ_BT_BLOCK_COMMENT; return r; }
            }
            /* Single-line comment: "//" where third char is not '/' */
            if (len == 2 || line[2] != '/') {
                r.type = AQ_BT_COMMENT; return r;
            }
            /* "///" (three slashes without a fourth) → falls through to
             * description/text scan, same as the re2c DFA.                  */
        }
        return classify_content(line, len, line, pe);
    }

    /* 6. Attribute entry: :name: value  (first char is ':').
     *    If the second char is ':', it's an empty description list ":: body".
     *    Otherwise attempt to parse as an attribute entry.                   */
    if (c0 == ':') {
        const char *p = line + 1;
        /* Optional '!' for unset: :!name: */
        if (p < pe && *p == '!') ++p;
        /* First char of name must be a word char */
        if (p < pe && is_word(*p)) {
            const char *name_start = line + 1; /* includes '!' if present */
            /* Advance past name chars: word chars, hyphens, spaces, apostrophes */
            while (p < pe && (is_word(*p) || *p == '-' || *p == ' ' || *p == '\''))
                ++p;
            /* Must hit closing ':' */
            if (p < pe && *p == ':') {
                const char *name_end = p;
                const char *val_start = skip_ws(p + 1, pe);
                const char *val_end   = rskip_ws(val_start, pe);
                r.type    = AQ_BT_ATTR_ENTRY;
                r.caps[0] = make_span(line, name_start, name_end);
                if (val_end > val_start)
                    r.caps[1] = make_span(line, val_start, val_end);
                return r;
            }
        }
        /* Not an attribute entry; fall through to descript scan. */
        return classify_content(line, len, line, pe);
    }

    /* 7. Section title: = Title  (1–6 '=' then whitespace then text).
     *    Also catches delimiter: ====  (exactly 4) and =====+ (5+).         */
    if (c0 == '=') {
        size_t eq_count = 1;
        while (eq_count < len && line[eq_count] == '=') ++eq_count;

        if (eq_count == len) {
            /* Line is all '=': delimiter for exactly 4, or 5+. */
            if (eq_count == 4 || eq_count >= 5) {
                r.type    = AQ_BT_DELIMITER;
                r.caps[0] = make_span(line, line, pe);
                return r;
            }
            /* 1, 2, or 3 '=' alone → TEXT */
            r.type = AQ_BT_TEXT;
            return r;
        }
        /* After the '=' run there is a non-'=' character. */
        const char *after_eq = line + eq_count;
        if ((*after_eq == ' ' || *after_eq == '\t') && eq_count <= 6) {
            /* Section title */
            const char *txt_start = skip_ws(after_eq, pe);
            const char *txt_end   = rskip_ws(txt_start, pe);
            /* Strip optional trailing setext-style '=' run */
            const char *q = txt_end;
            while (q > txt_start && *(q - 1) == '=') --q;
            if (q < txt_end) {
                /* Verify there is space before the trailing '=' */
                const char *after_text = rskip_ws(txt_start, q);
                txt_end = after_text;
            }
            if (txt_end > txt_start) {
                r.type    = AQ_BT_SECTION_TITLE;
                r.caps[0] = make_span(line, line, line + eq_count);
                r.caps[1] = make_span(line, txt_start, txt_end);
                return r;
            }
        }
        /* Not a section title (too many '=', no WS, …) → TEXT */
        r.type = AQ_BT_TEXT;
        return r;
    }

    /* 8. Block anchor [[id]] or [[id, reftext]].
     *    Block attribute [content] (not [[).                                  */
    if (c0 == '[') {
        if (len >= 2 && line[1] == '[') {
            /* Block anchor: must end with ']]' */
            if (len >= 4 && pe[-1] == ']' && pe[-2] == ']') {
                const char *inner = line + 2;
                const char *end   = pe - 2;
                /* First char of id must be a word char */
                if (inner < end && is_word(*inner)) {
                    r.type = AQ_BT_BLOCK_ANCHOR;
                    const char *comma = (const char *)memchr(inner, ',',
                                                             (size_t)(end - inner));
                    if (comma) {
                        r.caps[0] = make_span(line, inner, comma);
                        const char *ref = skip_ws(comma + 1, end);
                        const char *ref_end = rskip_ws(ref, end);
                        if (ref_end > ref)
                            r.caps[1] = make_span(line, ref, ref_end);
                    } else {
                        r.caps[0] = make_span(line, inner, end);
                    }
                    return r;
                }
            }
            /* Malformed anchor → fall through to description/text */
        } else if (len >= 2 && line[1] != ']') {
            /* Block attribute: [content] where content is non-empty */
            if (pe[-1] == ']') {
                r.type    = AQ_BT_BLOCK_ATTR;
                r.caps[0] = make_span(line, line + 1, pe - 1);
                return r;
            }
        }
        return classify_content(line, len, line, pe);
    }

    /* 9. Block title: .Title
     *    NOT ". text" (space follows) and NOT ".." (dot follows).
     *    Also guards delimiters: .... (4 dots) and .....+ (5+).               */
    if (c0 == '.') {
        if (len >= 4) {
            /* Check for all-dot delimiter: .... or .....+ */
            size_t dc = 0;
            while (dc < len && line[dc] == '.') ++dc;
            if (dc == len && (dc == 4 || dc >= 5)) {
                r.type    = AQ_BT_DELIMITER;
                r.caps[0] = make_span(line, line, pe);
                return r;
            }
        }
        /* Not a delimiter; could be block title, ordered list (". item"), or text */
        if (len >= 2) {
            char c1 = line[1];
            if (c1 != ' ' && c1 != '.' && c1 != '\0') {
                /* Block title: .Title */
                r.type    = AQ_BT_BLOCK_TITLE;
                r.caps[0] = make_span(line, line + 1, pe);
                return r;
            }
        }
        /* ". " → ordered list handled by classify_content below */
        return classify_content(line, len, line, pe);
    }

    /* 10. Delimiters starting with '-': -- (open block) or ---- (4 dashes)
     *     or -----+ (5+ dashes), or unordered list "- item".                 */
    if (c0 == '-') {
        if (len >= 2 && line[1] == '-') {
            /* Count consecutive '-' */
            size_t dc = 2;
            while (dc < len && line[dc] == '-') ++dc;
            if (dc == len) {
                /* Exactly 2 → open-block delimiter; 4 → delimiter; 5+ → delimiter */
                if (dc == 2 || dc == 4 || dc >= 5) {
                    r.type    = AQ_BT_DELIMITER;
                    r.caps[0] = make_span(line, line, pe);
                    return r;
                }
                /* dc==3: "---" is handled by the thematic-break pre-check above
                 * so we should not reach here for "---".  If we do, fall through. */
            }
        }
        return classify_content(line, len, line, pe);
    }

    /* 11. Delimiter: exactly **** (4) or *****+ (5+). Else unordered list. */
    if (c0 == '*') {
        if (len >= 4) {
            size_t dc = 0;
            while (dc < len && line[dc] == '*') ++dc;
            if (dc == len && (dc == 4 || dc >= 5)) {
                r.type    = AQ_BT_DELIMITER;
                r.caps[0] = make_span(line, line, pe);
                return r;
            }
        }
        return classify_content(line, len, line, pe);
    }

    /* 12. Delimiter: ____ (4) or _____+ (5+). */
    if (c0 == '_') {
        size_t dc = 0;
        while (dc < len && line[dc] == '_') ++dc;
        if (dc == len && (dc == 4 || dc >= 5)) {
            r.type    = AQ_BT_DELIMITER;
            r.caps[0] = make_span(line, line, pe);
            return r;
        }
        return classify_content(line, len, line, pe);
    }

    /* 13. Delimiter: ++++ (4) or +++++ (5+). */
    if (c0 == '+') {
        size_t dc = 0;
        while (dc < len && line[dc] == '+') ++dc;
        if (dc == len && (dc == 4 || dc >= 5)) {
            r.type    = AQ_BT_DELIMITER;
            r.caps[0] = make_span(line, line, pe);
            return r;
        }
        return classify_content(line, len, line, pe);
    }

    /* 14. Delimiter: ~~~~ (4 or more tildes). */
    if (c0 == '~') {
        size_t dc = 0;
        while (dc < len && line[dc] == '~') ++dc;
        if (dc == len && dc >= 4) {
            r.type    = AQ_BT_DELIMITER;
            r.caps[0] = make_span(line, line, pe);
            return r;
        }
        return classify_content(line, len, line, pe);
    }

    /* 15. Empty description-list forms: ":: body" or ";; body". */
    if (c0 == ';' && len >= 2 && line[1] == ';') {
        const char *after = skip_ws(line + 2, pe);
        r.type    = AQ_BT_LIST_DESCRIPT;
        r.caps[1] = make_span(line, line, line + 2); /* sep = ";;" */
        r.caps[2] = make_span(line, after, pe);
        return r;
    }

    /* 16. Remaining: letters, digits, or other chars — dispatch to the
     *     keyword-macro, list-item, description-list, or TEXT handler.       */
    return classify_content(line, len, line, pe);
}

/* ── Attribute-list parser ────────────────────────────────────────────────── */
/*
 * Parses the CONTENT of an AsciiDoc block-attribute line (everything inside
 * the square brackets).  Examples:
 *
 *   "source,java"             → positional 1="source",  positional 2="java"
 *   "id=myid,title=The Title" → named "id"="myid",      named "title"="The Title"
 *   "A photo"                 → positional 1="A photo"
 *   "quote, Mike Muuss"       → positional 1="quote",   positional 2="Mike Muuss"
 *   "id=\"my anchor\""        → named "id"="my anchor"  (quoted value)
 *
 * Algorithm:
 *   1. Split on commas (outside of double-quoted strings).
 *   2. For each token:
 *      a. If it contains '=' (outside quotes) → named attribute.
 *      b. Otherwise → positional attribute (1-based index).
 *   3. Trim leading/trailing ASCII whitespace from keys and values.
 *   4. Strip surrounding double-quotes from values.
 */

#include "attr_list.h"
#include <stdio.h>   /* snprintf */

int aq_parse_attr_list(const char    *content,
                       size_t         len,
                       AqAttrCallback cb,
                       void          *userdata)
{
    if (!content || !cb) { return -1; }

    const char *p  = content;
    const char *pe = content + len;
    int positional_idx = 1;

    while (p < pe) {
        /* Skip leading whitespace. */
        while (p < pe && (*p == ' ' || *p == '\t')) ++p;
        if (p >= pe) { break; }

        /* Scan a comma-separated token, respecting double-quoted strings. */
        const char *tok_start = p;
        int in_q = 0;
        while (p < pe) {
            if (*p == '"')             { in_q = !in_q; ++p; continue; }
            if (*p == ',' && !in_q)   { break; }
            ++p;
        }
        const char *tok_end = p;
        if (p < pe) { ++p; } /* consume the comma */

        /* Trim trailing whitespace. */
        while (tok_end > tok_start &&
               (*(tok_end - 1) == ' ' || *(tok_end - 1) == '\t')) {
            --tok_end;
        }

        /* Find '=' outside quotes → named attribute. */
        const char *eq = NULL;
        {
            const char *q  = tok_start;
            int         qi = 0;
            while (q < tok_end) {
                if (*q == '"')       { qi = !qi; ++q; continue; }
                if (*q == '=' && !qi) { eq = q; break; }
                ++q;
            }
        }

        if (eq) {
            /* Named: key=value */
            const char *k_s = tok_start, *k_e = eq;
            while (k_s < k_e && (*k_s == ' ' || *k_s == '\t')) { ++k_s; }
            while (k_e > k_s && (*(k_e-1) == ' ' || *(k_e-1) == '\t')) { --k_e; }

            const char *v_s = eq + 1, *v_e = tok_end;
            while (v_s < v_e && (*v_s == ' ' || *v_s == '\t')) { ++v_s; }
            /* Strip surrounding double-quotes. */
            if (v_e - v_s >= 2 && *v_s == '"' && *(v_e - 1) == '"') {
                ++v_s; --v_e;
            }

            if (k_e > k_s) {
                cb(userdata,
                   k_s, (size_t)(k_e - k_s),
                   v_s, (size_t)(v_e - v_s));
            }
        } else {
            /* Positional */
            const char *v_s = tok_start, *v_e = tok_end;
            while (v_s < v_e && (*v_s == ' ' || *v_s == '\t')) { ++v_s; }
            if (v_e - v_s >= 2 && *v_s == '"' && *(v_e - 1) == '"') {
                ++v_s; --v_e;
            }

            if (v_e > v_s) {
                char key[16];
                int  key_len = snprintf(key, sizeof(key), "%d", positional_idx++);
                if (key_len > 0) {
                    cb(userdata,
                       key,  (size_t)key_len,
                       v_s,  (size_t)(v_e - v_s));
                }
            }
        }
    }
    return 0;
}
