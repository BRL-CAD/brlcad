/*                      R E A D S T R G . C
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
/** @file iges/readstrg.c
 *
 * This routine reads the next field in "card" buffer.  It expects the
 * field to contain a character string of the form "nHstring" where n
 * is the length of "string". If "id" is not the null string, then
 * "id" is printed followed by "string".  If "id" is null, then the
 * only action taken is to read the string (effectively skipping the
 * field).
 *
 * "eofd" is the "end-of-field" delimiter
 * "eord" is the "end-of-record" delimiter
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"


/*
 * Read the next field from the shared global "card" buffer, expecting a
 * Hollerith string of the form "nHstring".  If "id" is non-empty the
 * string is echoed; the field is otherwise just skipped.  Advances the
 * field "counter" and auto-advances to the next record when the field
 * spans a record boundary.
 */
void
Readstrg(const char *id)
{
    char *value = NULL;
    Readname(&value, id ? id : "");
    if (value)
	bu_free(value, "IGES string");
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
