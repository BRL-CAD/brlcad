/*                      R E A D C O L S . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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
/** @file iges/readcols.c
 *
 * This routine reads a specific number of characters from the "card"
 * buffer.  The number is "cols".  The string of characters read is
 * pointed to by "id".
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Readcols(char *id, int cols)
{
    int i;
    char *tmp;

    tmp = id;

    for (i=0; i<cols; i++)
	*tmp++ = card[counter++];
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
