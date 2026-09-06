/*                      R E A D T I M E . C
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
/** @file iges/readtime.c
 *
 * This routine reads the next field in "card" buffer
 *	It expects the field to contain a string of the form:
 *		13HYYMMDD.HHNNSS or 15HYYYYMMDD.HHNNSS
 *		where:
 *			YY is the year (last 2 digits) or YYYY is the year (all 4 digits)
 *			MM is the month (01 - 12)
 *			DD is the day (01 - 31)
 *			HH is the hour (00 -23)
 *			NN is the minute (00 - 59)
 *			SS is the second (00 - 59)
 *	The string is read and printed out in the form:
 *		/MM/DD/YYYY at HH:NN:SS
 *
 *	"eofd" is the "end-of-field" delimiter
 *	"eord" is the "end-of-record" delimiter
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"
#include "iges_output.h"


/*
 * Read the next field from the shared global "card" buffer, expecting a
 * Hollerith date/time stamp (13H... or 15H... form), and print it in a
 * human-readable "MM/DD/YYYY at HH:NN:SS" format.  Advances the field
 * "counter" and auto-advances to the next record when the field spans a
 * record boundary.
 */
void
Readtime(const char *id)
{
    char *value = NULL;
    Readname(&value, "");
    if (!value)
	return;
    const size_t length = strlen(value);
    const size_t short_length = 13; /* YYMMDD.HHMMSS */
    const size_t long_length = 15; /* YYYYMMDD.HHMMSS */
    if (length != short_length && length != long_length) {
	iges_output_legacy_warning(0, "invalid_legacy_timestamp",
	    "timestamp must have 13 or 15 characters");
	bu_free(value, "IGES timestamp");
	return;
    }
    const size_t year_digits = length == short_length ? 2 : 4;
    for (size_t i = 0; i < length; ++i) {
	if (i == year_digits + 4 ? value[i] != '.' :
	    (value[i] < '0' || value[i] > '9')) {
	    iges_output_legacy_warning(0, "invalid_legacy_timestamp",
		"timestamp must use YYMMDD.HHMMSS or YYYYMMDD.HHMMSS");
	    bu_free(value, "IGES timestamp");
	    return;
	}
    }
    int year = 0;
    for (size_t i = 0; i < year_digits; ++i)
	year = year * 10 + value[i] - '0';
    if (length == short_length)
	year += 1900;
    bu_log("%s%.2s/%.2s/%d at %.2s:%.2s:%.2s\n", id ? id : "",
	value + year_digits, value + year_digits + 2, year,
	value + year_digits + 5, value + year_digits + 7, value + year_digits + 9);
    bu_free(value, "IGES timestamp");
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
