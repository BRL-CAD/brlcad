/* M I M E _ P N M . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
#ifndef BU_MIME_PNM_H
#define BU_MIME_PNM_H
#include "common.h"
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "bu/mime_runtime.h"
#include "bu/vls.h"

/* PGM and PPM share the three-number header. Leave the raw raster's first
 * byte untouched, even if it is whitespace or starts with '#'. */
enum {MIME_PNM_BYTE_MAX = 255, MIME_PNM_MAXVAL = 65535};

struct mime_pnm_input {
    FILE *stream;
    const unsigned char *cursor;
    const unsigned char *end;
    int ended;
};

struct mime_pnm_header {
    int kind;
    size_t width;
    size_t height;
    size_t maxval;
    int identified;
    int complete;
};

static inline int
mime_pnm_byte(struct mime_pnm_input *input)
{
    int byte = input->stream ? fgetc(input->stream) :
	(input->cursor == input->end ? EOF : *input->cursor++);
    if (byte == EOF) input->ended = 1;
    return byte;
}

static inline int
mime_pnm_unread(struct mime_pnm_input *input, int byte)
{
    if (byte == EOF)
	return 1;
    if (input->stream)
	return ungetc(byte, input->stream) != EOF;
    --input->cursor;
    return 1;
}

static inline int
mime_pnm_number(struct mime_pnm_input *input, size_t *value)
{
    int byte;
    do {
	byte = mime_pnm_byte(input);
	if (byte == '#')
	    while (byte != EOF && byte != '\n' && byte != '\r')
		byte = mime_pnm_byte(input);
    } while (byte != EOF && isspace((unsigned char)byte));
    if (byte == EOF || !isdigit((unsigned char)byte))
	return 0;
    *value = 0;
    do {
	size_t digit = (size_t)(byte - '0');
	if (*value > (SIZE_MAX - digit) / 10)
	    return 0;
	*value = *value * 10 + digit;
	byte = mime_pnm_byte(input);
    } while (byte != EOF && isdigit((unsigned char)byte));
    if (byte != EOF && byte != '#' && !isspace((unsigned char)byte))
	return 0;
    return mime_pnm_unread(input, byte);
}

static inline int
mime_pnm_header_read(struct mime_pnm_input *input, struct mime_pnm_header *header)
{
    memset(header, 0, sizeof(*header));
    if (mime_pnm_byte(input) != 'P')
	return 0;
    header->kind = mime_pnm_byte(input);
    if (header->kind != '2' && header->kind != '3' && header->kind != '5' && header->kind != '6')
	return 0;
    int separator = mime_pnm_byte(input);
    if (separator == EOF || (!isspace((unsigned char)separator) && separator != '#') ||
	!mime_pnm_unread(input, separator))
	return 0;
    header->identified = 1;
    if (!mime_pnm_number(input, &header->width) || !mime_pnm_number(input, &header->height) ||
	!mime_pnm_number(input, &header->maxval) || !header->width || !header->height ||
	!header->maxval || header->maxval > MIME_PNM_MAXVAL)
	return 0;
    /* The raw raster starts immediately after exactly one separator.
     * Its first sample may itself be whitespace or '#'. */
    separator = mime_pnm_byte(input);
    if (separator == EOF)
	return 0;
    header->complete = (header->kind == '5' || header->kind == '6') ?
	isspace((unsigned char)separator) != 0 : mime_pnm_unread(input, separator);
    return header->complete;
}

static inline int
mime_pnm_binary(const struct mime_pnm_header *header)
{
    return header->kind == '5' || header->kind == '6';
}

static inline const char *
mime_pnm_type(const struct mime_pnm_header *header)
{
    return header->kind == '2' || header->kind == '5' ? "image/x-portable-graymap" : "image/x-portable-pixmap";
}

static inline int
mime_pnm_append_traits(const struct mime_pnm_header *header, struct bu_mime_traits *traits)
{
    if (!header->identified || !bu_mime_traits_add(traits, "encoding", mime_pnm_binary(header) ? "binary" : "ascii")) return 0;
    if (!header->complete) return 1;
    struct bu_vls maxval = BU_VLS_INIT_ZERO;
    bu_vls_printf(&maxval, "%zu", header->maxval);
    int valid = bu_mime_traits_add(traits,
	header->kind == '2' || header->kind == '5' ? "pgm.maxval" : "ppm.maxval", bu_vls_cstr(&maxval));
    bu_vls_free(&maxval);
    if (mime_pnm_binary(header)) {
	valid = valid && bu_mime_traits_add(traits, "bit-depth", header->maxval <= MIME_PNM_BYTE_MAX ? "8" : "16");
	if (header->maxval > MIME_PNM_BYTE_MAX)
	    valid = valid && bu_mime_traits_add(traits, "byte-order", "big-endian");
    }
    return valid;
}

static inline int
mime_pnm_profile_matches(const struct mime_pnm_header *header, const struct bu_mime_traits *requested, const struct bu_mime_traits *supplemental)
{
    struct bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    int valid = header->complete && mime_pnm_append_traits(header, &observed) &&
	bu_mime_traits_match(&observed, supplemental, requested);
    bu_mime_traits_free(&observed);
    return valid;
}
#endif
