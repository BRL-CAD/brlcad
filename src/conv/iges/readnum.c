/*                        R E A D N U M . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "iges_struct.h"

#include <ctype.h>
#include <errno.h>

#include "iges_extern.h"
#include "iges_output.h"


int
iges_read_number(char *field)
{
    const char section = card[IGES_SECTION_COL];
    const int last_column = section == 'P' ? PARAMLEN : CARDLEN;
    size_t length = 0;
    int excessive = 0;

    field[0] = '\0';
    for (;;) {
	/* The last data column is inclusive.  Check before fetching a byte,
	 * so a boundary digit is neither dropped nor read from the DE field. */
	if (counter > last_column &&
	    (Readrec(currec + 1) || card[IGES_SECTION_COL] != section)) {
	    iges_output_legacy_warning(0, "truncated_legacy_number",
		"numeric parameter extends past the end of its section");
	    return -1;
	}
	const unsigned char character = (unsigned char)card[counter];
	if (character == eofd || character == eord) {
	    if (character == eofd)
		++counter;
	    break;
	}
	++counter;
	if (length < MAX_NUM - 1)
	    field[length++] = character;
	else
	    excessive = 1;
    }
    field[length] = '\0';
    if (excessive) {
	/* Consume the whole field even when it does not fit, so subsequent
	 * parameters cannot accidentally read its remaining digits. */
	iges_output_legacy_warning(0, "excessive_legacy_number",
	    "numeric parameter exceeds the supported field length");
	return -1;
    }
    while (length && isspace((unsigned char)field[length - 1]))
	field[--length] = '\0';
    return length ? 1 : 0;
}


void
iges_read_real(double *value, double factor, const char *id)
{
    char field[MAX_NUM];
    const int status = iges_read_number(field);
    if (!status)
	return;
    if (status < 0) {
	*value = 0.0;
	return;
    }
    for (char *character = field; *character; ++character)
	if (*character == 'D' || *character == 'd')
	    *character = 'e';
    char *end = NULL;
    errno = 0;
    const double parsed = strtod(field, &end);
    if (end == field || *end || errno == ERANGE ||
	!isfinite(parsed) || !isfinite(parsed * factor)) {
	iges_output_legacy_warning(0, "invalid_legacy_real",
	    "real parameter is invalid or outside the supported range");
	/* Match the legacy invalid-number fallback, but report it and never
	 * pass non-finite coordinates to geometry constructors. */
	*value = 0.0;
    } else {
	*value = parsed * factor;
    }
    if (*id)
	bu_log("%s%g\n", id, *value);
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
