/*                      R E A D N A M E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file iges/readname.c
 *
 * This routine reads the next field in "card" buffer.  It expects the
 * field to contain a character string of the form "nHstring" where n
 * is the length of "string". If "id" is not the null string, then
 * "id" is printed followed by "string".  A pointer to the string is
 * returned in "ptr".
 *
 * "eofd" is the "end-of-field" delimiter
 * "eord" is the "end-of-record" delimiter
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"
#include "iges_output.h"
#include <ctype.h>


/*
 * Read the next field from the shared global "card" buffer, expecting a
 * Hollerith string of the form "nHstring", and return a newly allocated
 * copy (with room for one extra character) via "ptr" (NULL for an empty
 * field).  Advances the field "counter" and auto-advances to the next
 * record when the field spans a record boundary.
 */
/* Bound malformed lengths before allocation or advancing through records. */
#define IGES_MAX_NAME_LENGTH 1000000U

static int
name_record(int last_column)
{
    if (counter <= last_column || !Readrec(currec + 1))
	return 1;
    iges_output_legacy_warning(0, "truncated_legacy_name",
	"Hollerith string extends past the end of the file");
    return 0;
}

void
Readname(char **ptr, const char *id)
{
    const int last_column = card[IGES_SECTION_COL] == 'P' ? PARAMLEN : CARDLEN;
    size_t length = 0;
    int have_digit = 0;
    *ptr = NULL;
    if (!name_record(last_column))
	return;
    if (card[counter] == eofd) {
	++counter;
	return;
    }
    if (card[counter] == eord)
	return;

    for (;;) {
	if (!name_record(last_column))
	    return;
	const unsigned char character = (unsigned char)card[counter++];
	if (isspace(character))
	    continue;
	if (!have_digit && (character == eofd || character == eord)) {
	    if (character == eord)
		--counter;
	    return;
	}
	if ((character == 'H' || character == 'h') && have_digit)
	    break;
	if (!isdigit(character) ||
	    length > (IGES_MAX_NAME_LENGTH - (character - '0')) / 10) {
	    iges_output_legacy_warning(0, "invalid_legacy_name",
		"Hollerith string has an invalid or excessive length");
	    /* Do not consume a record terminator while discarding a bad prefix. */
	    if (character == eord || character == eofd)
		--counter;
	    Skip_field();
	    return;
	}
	length = length * 10 + (character - '0');
	have_digit = 1;
    }

    *ptr = (char *)bu_malloc(length + 1, "IGES Hollerith string");
    for (size_t i = 0; i < length; ++i) {
	if (!name_record(last_column)) {
	    bu_free(*ptr, "IGES Hollerith string");
	    *ptr = NULL;
	    return;
	}
	(*ptr)[i] = card[counter++];
    }
    (*ptr)[length] = '\0';
    /* Boundary checks precede reading either prefix or data.  In particular,
     * an H in the last column must not start parsing a second length. */
    Skip_field();
    if (*id)
	bu_log("%s%s\n", id, *ptr);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
