/* M I M E _ S T L . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 * See the file named COPYING for more information.
 */
#ifndef BU_MIME_STL_H
#define BU_MIME_STL_H
#include "common.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include "bio.h"
#include "bu/mime.h"

#define MIME_STL_HEADER_SIZE 84
#define MIME_STL_COUNT_OFFSET 80
#define MIME_STL_TRIANGLE_SIZE 50

enum mime_stl_kind {MIME_STL_UNKNOWN, MIME_STL_ASCII, MIME_STL_BINARY};
enum mime_stl_phase {MIME_STL_START, MIME_STL_BOM2, MIME_STL_BOM3, MIME_STL_SOLID, MIME_STL_NAME,
    MIME_STL_FACET, MIME_STL_NORMAL, MIME_STL_DONE, MIME_STL_OTHER};
struct mime_stl_text {
    enum mime_stl_phase phase;
    size_t matched;
    int header, bom;
};
#define MIME_STL_TEXT_INIT {MIME_STL_START, 0, 0, 0}

struct mime_stl_profile {
    enum mime_stl_kind kind;
    int bom, ascii;
};
#define MIME_STL_PROFILE_INIT {MIME_STL_UNKNOWN, 0, 0}

static inline int
mime_stl_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

/* Names and leading whitespace may exceed bounded recognition. Keep only
 * the keyword state when inspecting the full stream. */
static inline void
mime_stl_text_feed(struct mime_stl_text *text, const unsigned char *data, size_t size)
{
    for (size_t at = 0; at < size && text->phase < MIME_STL_DONE; ++at) {
	const unsigned char c = data[at];
	if (text->phase == MIME_STL_START) {
	    text->phase = c == 0xef ? MIME_STL_BOM2 : MIME_STL_SOLID;
	    if (text->phase == MIME_STL_BOM2) continue;
	} else if (text->phase == MIME_STL_BOM2) {
	    text->phase = c == 0xbb ? MIME_STL_BOM3 : MIME_STL_OTHER;
	    continue;
	} else if (text->phase == MIME_STL_BOM3) {
	    text->phase = c == 0xbf ? MIME_STL_SOLID : MIME_STL_OTHER;
	    text->bom = text->phase == MIME_STL_SOLID;
	    continue;
	}
	if (text->phase == MIME_STL_NAME) {
	    if (c == '\n' || c == '\r') text->phase = MIME_STL_FACET;
	    continue;
	}
	if (!text->matched && mime_stl_space(c)) continue;
	const char *word = text->phase == MIME_STL_SOLID ? "solid" :
	    text->phase == MIME_STL_FACET ? "facet" : "normal";
	if (text->matched == strlen(word)) {
	    if (!mime_stl_space(c)) {text->phase = MIME_STL_OTHER; break;}
	    if (text->phase == MIME_STL_SOLID) {
		text->header = 1;
		text->phase = c == '\n' || c == '\r' ? MIME_STL_FACET : MIME_STL_NAME;
	    } else text->phase = text->phase == MIME_STL_FACET ? MIME_STL_NORMAL : MIME_STL_DONE;
	    text->matched = 0;
	} else if ((c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c) == (unsigned char)word[text->matched]) ++text->matched;
	else text->phase = MIME_STL_OTHER;
    }
}

static inline int
mime_stl_text_identified(const struct mime_stl_text *text, int whole)
{
    return text->phase == MIME_STL_DONE || (whole && text->phase == MIME_STL_NORMAL && text->matched == strlen("normal"));
}

static inline uint32_t
mime_stl_count(const struct bu_mime_probe_input *input)
{
    const unsigned char *p = input->data + MIME_STL_COUNT_OFFSET;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline int
mime_stl_binary_matches(const struct bu_mime_probe_input *input)
{
    if (input->size < MIME_STL_HEADER_SIZE || !input->file_size_known || input->file_size < MIME_STL_HEADER_SIZE) return 0;
    const uint64_t records = (uint64_t)mime_stl_count(input) * MIME_STL_TRIANGLE_SIZE;
    const uint64_t available = input->file_size - MIME_STL_HEADER_SIZE;
    return available == records;
}

static inline int
mime_stl_append(const struct mime_stl_profile *profile, struct bu_mime_traits *traits)
{
    if (profile->kind == MIME_STL_UNKNOWN) return 0;
    if (!bu_mime_traits_add(traits, "stl.serialization", profile->kind == MIME_STL_ASCII ? "text" : "binary")) return 0;
    if (profile->kind == MIME_STL_BINARY)
	return bu_mime_traits_add(traits, "encoding", "binary") && bu_mime_traits_add(traits, "byte-order", "little-endian");
    return bu_mime_traits_add(traits, "stl.utf8-bom", profile->bom ? "true" : "false") &&
	(!(profile->bom || profile->ascii) || bu_mime_traits_add(traits, "encoding", profile->bom ? "utf-8" : "ascii"));
}

/* Binary layout takes precedence over a misleading "solid" header. Full
 * readers need only the text header: their parsers validate the body. */
static inline struct mime_stl_profile
mime_stl_file_profile(FILE *file, int inspect_encoding)
{
    struct mime_stl_profile profile = MIME_STL_PROFILE_INIT;
    struct stat info;
    unsigned char data[MIME_STL_HEADER_SIZE];
    const int known = !fstat(fileno(file), &info) && info.st_size >= 0;
    size_t size = fread(data, 1, sizeof(data), file);
    const struct bu_mime_probe_input input = {data, size, feof(file), known ? (uint64_t)info.st_size : 0, known};
    if (ferror(file)) return profile;
    if (mime_stl_binary_matches(&input)) {profile.kind = MIME_STL_BINARY; return profile;}
    struct mime_stl_text text = MIME_STL_TEXT_INIT;
    do {
	mime_stl_text_feed(&text, data, size);
	if (text.header) {
	    profile.kind = MIME_STL_ASCII;
	    profile.bom = text.bom;
	    break;
	}
	if (text.phase == MIME_STL_OTHER) break;
	size = fread(data, 1, sizeof(data), file);
    } while (size);
    if (profile.kind == MIME_STL_ASCII && !profile.bom && inspect_encoding) {
	unsigned char bytes[BUFSIZ];
	if (fseek(file, 0, SEEK_SET)) {profile.kind = MIME_STL_UNKNOWN; return profile;}
	profile.ascii = 1;
	while ((size = fread(bytes, 1, sizeof(bytes), file)) != 0) {
	    for (size_t at = 0; at < size; ++at)
		if (bytes[at] > 0x7f) {profile.ascii = 0; break;}
	    if (!profile.ascii) break;
	}
    }
    return profile;
}
#endif
