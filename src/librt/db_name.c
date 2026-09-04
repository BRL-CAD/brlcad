/*                   D B _ N A M E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <stdint.h>

#include "rt/db_io.h"

#define DB_NAME_UTF8_REPLACEMENT_CHARACTER 0xfffdU

static int
db_name_utf8_continuation(unsigned char c)
{
    return (c & 0xc0U) == 0x80U;
}


static uint32_t
db_name_next_utf8_codepoint(const unsigned char **position)
{
    const unsigned char *input = *position;
    const unsigned char first = *input++;

    if (first < 0x80U) {
	*position = input;
	return first;
    }

    if (first >= 0xc2U && first <= 0xdfU && db_name_utf8_continuation(input[0])) {
	*position = input + 1;
	return ((uint32_t)(first & 0x1fU) << 6) |
	    (uint32_t)(input[0] & 0x3fU);
    }

    if (first >= 0xe0U && first <= 0xefU && input[0] && input[1] &&
	    db_name_utf8_continuation(input[0]) && db_name_utf8_continuation(input[1]) &&
	    (first != 0xe0U || input[0] >= 0xa0U) &&
	    (first != 0xedU || input[0] < 0xa0U)) {
	*position = input + 2;
	return ((uint32_t)(first & 0x0fU) << 12) |
	    ((uint32_t)(input[0] & 0x3fU) << 6) |
	    (uint32_t)(input[1] & 0x3fU);
    }

    if (first >= 0xf0U && first <= 0xf4U && input[0] && input[1] && input[2] &&
	    db_name_utf8_continuation(input[0]) && db_name_utf8_continuation(input[1]) &&
	    db_name_utf8_continuation(input[2]) &&
	    (first != 0xf0U || input[0] >= 0x90U) &&
	    (first != 0xf4U || input[0] < 0x90U)) {
	*position = input + 3;
	return ((uint32_t)(first & 0x07U) << 18) |
	    ((uint32_t)(input[0] & 0x3fU) << 12) |
	    ((uint32_t)(input[1] & 0x3fU) << 6) |
	    (uint32_t)(input[2] & 0x3fU);
    }

    *position = input;
    return DB_NAME_UTF8_REPLACEMENT_CHARACTER;
}


static const char *
db_name_latin_ascii(uint32_t codepoint)
{
    switch (codepoint) {
	case 0x00c0U: case 0x00c1U: case 0x00c2U: case 0x00c3U:
	case 0x00c4U: case 0x00c5U: return "A";
	case 0x00c6U: return "AE";
	case 0x00c7U: return "C";
	case 0x00c8U: case 0x00c9U: case 0x00caU: case 0x00cbU: return "E";
	case 0x00ccU: case 0x00cdU: case 0x00ceU: case 0x00cfU: return "I";
	case 0x00d0U: return "D";
	case 0x00d1U: return "N";
	case 0x00d2U: case 0x00d3U: case 0x00d4U: case 0x00d5U:
	case 0x00d6U: case 0x00d8U: return "O";
	case 0x00d9U: case 0x00daU: case 0x00dbU: case 0x00dcU: return "U";
	case 0x00ddU: return "Y";
	case 0x00deU: return "TH";
	case 0x00dfU: return "ss";
	case 0x00e0U: case 0x00e1U: case 0x00e2U: case 0x00e3U:
	case 0x00e4U: case 0x00e5U: return "a";
	case 0x00e6U: return "ae";
	case 0x00e7U: return "c";
	case 0x00e8U: case 0x00e9U: case 0x00eaU: case 0x00ebU: return "e";
	case 0x00ecU: case 0x00edU: case 0x00eeU: case 0x00efU: return "i";
	case 0x00f0U: return "d";
	case 0x00f1U: return "n";
	case 0x00f2U: case 0x00f3U: case 0x00f4U: case 0x00f5U:
	case 0x00f6U: case 0x00f8U: return "o";
	case 0x00f9U: case 0x00faU: case 0x00fbU: case 0x00fcU: return "u";
	case 0x00fdU: case 0x00ffU: return "y";
	case 0x00feU: return "th";
	default: return NULL;
    }
}


static void
db_name_append_separator(struct bu_vls *output, int *separator)
{
    if (*separator && bu_vls_strlen(output) > 0)
	bu_vls_putc(output, '_');
    *separator = 0;
}


int
db_sanitize_name(struct bu_vls *output, const char *input)
{
    const unsigned char *position;
    int separator = 0;

    if (!output || !input)
	return -1;

    bu_vls_trunc(output, 0);
    position = (const unsigned char *)input;
    while (*position) {
	const uint32_t codepoint = db_name_next_utf8_codepoint(&position);
	const char *ascii;

	if ((codepoint >= 'a' && codepoint <= 'z') ||
		(codepoint >= 'A' && codepoint <= 'Z') ||
		(codepoint >= '0' && codepoint <= '9')) {
	    db_name_append_separator(output, &separator);
	    bu_vls_putc(output, (char)codepoint);
	    continue;
	}

	ascii = db_name_latin_ascii(codepoint);
	if (ascii) {
	    db_name_append_separator(output, &separator);
	    bu_vls_strcat(output, ascii);
	    continue;
	}

	if (codepoint > 0x7fU) {
	    separator = bu_vls_strlen(output) > 0;
	    db_name_append_separator(output, &separator);
	    bu_vls_printf(output, "u%lX", (unsigned long)codepoint);
	    continue;
	}

	separator = bu_vls_strlen(output) > 0;
    }

    return 0;
}
