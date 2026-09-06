/*                       R E A D I N T . C
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
/** @file iges/readint.c
 *
 * This routine reads the next field in "card" buffer.  It expects the
 * field to contain a string of integers.  The string of integers is
 * read and converted to type "int" and returned in "inum".  If "id"
 * is not the null string, then "id" is printed followed by the
 * number.
 *
 * "eofd" is the "end-of-field" delimiter
 * "eord" is the "end-of-record" delimiter
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"
#include "iges_output.h"
#include <errno.h>
#include <limits.h>


/*
 * Read the next field from the shared global "card" buffer, parse it as an
 * integer, and store the result in "inum".  Advances the field "counter"
 * and auto-advances to the next record when the field spans a record
 * boundary.
 */
void
Readint(int *inum, const char *id)
{
    char num[MAX_NUM];
    const int status = iges_read_number(num);
    if (!status)
	return;
    if (status < 0) {
	*inum = INT_MIN;
	return;
    }

    char *end = NULL;
    errno = 0;
    const long value = strtol(num, &end, 10);
    if (end == num || *end || errno == ERANGE || value < INT_MIN || value > INT_MAX) {
	iges_output_legacy_warning(0, "invalid_legacy_integer",
	    "integer parameter is invalid or outside the supported range");
	*inum = INT_MIN;
    } else {
	*inum = (int)value;
    }
    if (*id != '\0')
	bu_log("%s%d\n", id, *inum);
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
