/*                       R E A D C N V . C
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
/** @file iges/readcnv.c
 *
 * This routine reads the next field in "card" buffer. It expects the
 * field to contain a string representing a "float". The string is
 * read and converted to type "fastf_t", multiplied by "conv_factor",
 * and returned in "inum".  If "id" is not the null string, then "id"
 * is printed followed by the number.  "conv_factor" is a factor to
 * convert to mm and multiply by a scale factor.
 *
 * "eofd" is the "end-of-field" delimiter
 * "eord" is the "end-of-record" delimiter
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"


/*
 * Read the next field from the shared global "card" buffer, parse it as a
 * floating-point value, multiply by "conv_factor" (units + scale to mm),
 * and store the result in "inum".  Advances the field "counter" and
 * auto-advances to the next record when the field spans a record boundary.
 */
void
Readcnv(fastf_t *inum, const char *id)
{
    iges_read_real(inum, conv_factor, id);
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
