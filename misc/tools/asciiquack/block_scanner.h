/// @file block_scanner.h
/// @brief Pure-C public API for the AsciiDoc block-line scanner.
///
/// The scanner classifies a single line of AsciiDoc source into a token type
/// and optionally records the byte offsets of key sub-matches (captures).
///
/// Usage:
///   AqBlockScanResult r = aq_scan_block_line(line.c_str(), line.size());
///   if (r.type == AQ_BT_ATTR_ENTRY) { ... }
///
/// Implementation: block_scanner_hand.c (hand-written, no external tools).

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* ── Token types ─────────────────────────────────────────────────────────── */

typedef enum AqBlockToken {
    AQ_BT_TEXT            = 0,  /**< Ordinary text / unrecognised */
    AQ_BT_BLANK,                /**< Empty or all-whitespace */
    AQ_BT_COMMENT,              /**< Single-line comment: // text (not ////) */
    AQ_BT_BLOCK_COMMENT,        /**< Block-comment delimiter: //// */
    AQ_BT_ATTR_ENTRY,           /**< Attribute entry: :name: value */
    AQ_BT_SECTION_TITLE,        /**< ATX section title: == Heading */
    AQ_BT_BLOCK_ATTR,           /**< Block-attribute line: [source,java] */
    AQ_BT_BLOCK_TITLE,          /**< Block title: .Title */
    AQ_BT_BLOCK_ANCHOR,         /**< Block anchor: [[id]] */
    AQ_BT_DELIMITER,            /**< Delimited-block fence: ----, ====, ... */
    AQ_BT_LIST_UNORD,           /**< Unordered list item: - text, * text */
    AQ_BT_LIST_ORD,             /**< Ordered list item: 1. text, a. text */
    AQ_BT_LIST_DESCRIPT,        /**< Description list: term:: body */
    AQ_BT_LIST_CALLOUT,         /**< Callout list: <1> text */
    AQ_BT_THEMATIC_BREAK,       /**< Thematic break: ''' or Markdown-style */
    AQ_BT_PAGE_BREAK,           /**< Page break: <<< */
    AQ_BT_BLOCK_IMAGE,          /**< Block image macro: image::target[attrs] */
    AQ_BT_BLOCK_MEDIA,          /**< Block media macro: video:: or audio:: */
    AQ_BT_IFDEF,                /**< ifdef::attr[] */
    AQ_BT_IFNDEF,               /**< ifndef::attr[] */
    AQ_BT_IFEVAL,               /**< ifeval::[expr] */
    AQ_BT_ENDIF,                /**< endif::attr[] */
    AQ_BT_INCLUDE               /**< include::path[attrs] */
} AqBlockToken;

/* ── Captured spans ──────────────────────────────────────────────────────── */

/**
 * A single captured span: byte offset and length within the scanned line.
 * An entry with @c len == 0 and @c off == 0 is absent/unmatched.
 */
typedef struct AqSpan {
    unsigned short off; /**< Byte offset from start of line */
    unsigned short len; /**< Length in bytes (0 = absent) */
} AqSpan;

/**
 * Result returned by @c aq_scan_block_line.
 *
 * The meaning of each caps[] slot depends on @c type:
 *
 *  AQ_BT_ATTR_ENTRY    caps[0]=name  caps[1]=value
 *  AQ_BT_SECTION_TITLE caps[0]=leading-equals  caps[1]=title-text
 *  AQ_BT_BLOCK_ATTR    caps[0]=content (inside the brackets)
 *  AQ_BT_BLOCK_TITLE   caps[0]=title-text (after the leading '.')
 *  AQ_BT_BLOCK_ANCHOR  caps[0]=anchor-id  caps[1]=optional-reftext
 *  AQ_BT_DELIMITER     caps[0]=the delimiter string itself
 *  AQ_BT_LIST_UNORD    caps[0]=marker  caps[1]=item-text
 *  AQ_BT_LIST_ORD      caps[0]=marker  caps[1]=item-text
 *  AQ_BT_LIST_DESCRIPT caps[0]=term  caps[1]=separator  caps[2]=body-text
 *  AQ_BT_LIST_CALLOUT  caps[0]=number  caps[1]=item-text
 *  AQ_BT_BLOCK_IMAGE   caps[0]=target  caps[1]=attr-content (no brackets)
 *  AQ_BT_BLOCK_MEDIA   caps[0]="video"/"audio"  caps[1]=target  caps[2]=attrs
 *  AQ_BT_IFDEF/IFNDEF  caps[0]=attribute-name
 *  AQ_BT_IFEVAL        caps[0]=expression
 *  AQ_BT_ENDIF         caps[0]=attribute-name (may be empty)
 *  AQ_BT_INCLUDE       caps[0]=path  caps[1]=attr-content
 */
typedef struct AqBlockScanResult {
    AqBlockToken type;
    AqSpan       caps[4];
} AqBlockScanResult;

/* ── Public function ─────────────────────────────────────────────────────── */

/**
 * Classify a single AsciiDoc source line.
 *
 * @param line  Pointer to the first character of the line.  The string must
 *              be NUL-terminated (i.e. @p line[@p len] == '\\0'), but the NUL
 *              itself is NOT included in @p len.  Passing @c str.c_str() and
 *              @c str.size() for a @c std::string is always correct.
 * @param len   Number of bytes in the line, not counting the NUL terminator.
 * @return      A result struct whose @c type identifies the line kind and
 *              whose @c caps[] record the principal sub-match positions.
 */
AqBlockScanResult aq_scan_block_line(const char *line, size_t len);

#ifdef __cplusplus
} /* extern "C" */
#endif
