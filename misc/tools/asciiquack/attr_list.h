// @file attr_list.h
// @brief Public C API for the AsciiDoc attribute-list parser.
//
// Parses the CONTENT of an AsciiDoc block-attribute or attribute-entry
// line.  Examples:
//
//   [source,java]          → positional "source", positional "java"
//   [id="myid",title=Foo]  → named "id"="myid",  named "title"="Foo"
//   [numbered,start=5]     → positional "numbered", named "start"="5"
//
// Usage:
//   // For "[source,java]", pass the inner content "source,java":
//   aq_parse_attr_list("source,java", 11, my_callback, userdata);
//
// The parser is implemented in block_scanner_hand.c (hand-written,
// no external tools required).

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/**
 * Callback invoked once for each attribute found in the attribute list.
 *
 * For positional attributes the key is "1", "2", "3", etc. (one-based).
 * For named attributes (key=value) the key is the attribute name.
 *
 * @p key and @p value are NOT NUL-terminated; use @p key_len / @p val_len.
 * @p userdata is the value passed to aq_parse_attr_list().
 */
typedef void (*AqAttrCallback)(void       *userdata,
                               const char *key,    size_t key_len,
                               const char *value,  size_t val_len);

/**
 * Parse the content of an AsciiDoc block-attribute or role string.
 *
 * @param content  Pointer to the first byte INSIDE the brackets, i.e. for
 *                 the line "[source,java]" pass "source,java".  The string
 *                 must be NUL-terminated (content[len] == '\\0').
 * @param len      Length of @p content, not including the NUL terminator.
 * @param cb       Callback invoked for each attribute.  Must not be NULL.
 * @param userdata Opaque pointer forwarded to @p cb.
 * @return         0 on success, -1 if a syntax error was encountered
 *                 (the callback may still have been called for any
 *                 attributes parsed before the error).
 */
int aq_parse_attr_list(const char    *content,
                       size_t         len,
                       AqAttrCallback cb,
                       void          *userdata);

#ifdef __cplusplus
} /* extern "C" */
#endif
