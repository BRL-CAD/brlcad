/*                       R E A D F L T . C
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
/** @file iges/readflt.c
 *
 * This routine reads the next field in "card" buffer.  It expects the
 * field to contain a string representing a "float".  The string is
 * read and converted to type "float" and returned in "inum".  If "id"
 * is not the null string, then "id" is printed followed by the
 * number.
 *
 * "eofd" is the "end-of-field" delimiter
 * "eord" is the "end-of-record" delimiter
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"


/*
 * Read the next field from the shared global "card" buffer, parse it as a
 * floating-point value, and store the result in "inum" (no unit/scale
 * conversion is applied).  Advances the field "counter" and auto-advances
 * to the next record when the field spans a record boundary.
 */
void
Readflt(fastf_t *inum, const char *id)
{
    iges_read_real(inum, 1.0, id);
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
