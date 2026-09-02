/*                       R E A D R E C . C
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
/** @file iges/readrec.c
 *
 * This routine reads record number "recno" from the IGES file and
 * places it in the "card" buffer (available for reading by
 * "readstrg", "readint", "readflt", "readtime", "readcols", etc.).
 * The record is located via the byte-offset index built by
 * Build_rec_index(), so it works regardless of whether records are
 * LF, CR-LF, or unterminated and whether they are padded to 80
 * columns.  The data columns of "card" are space-padded to "reclen"
 * so column-based reads are always well-defined.  This routine is
 * typically called before calling the other "read" routines to get to
 * the desired record; those routines call it again when the buffer
 * empties.
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

/*
 * Seek to and read record number "recno" (1-based) into the shared global
 * "card" buffer, updating "currec" and resetting the field "counter".
 * A trailing CR is stripped and short records are space-padded to the full
 * data width.  Returns 1 if "recno" is out of range (treated as EOF),
 * 0 on success.
 */
int
Readrec(int recno)
{
    int i, ch;
    b_off_t offset;

    currec = recno;

    if (recno < 1 || !rec_offset || (size_t)recno > nrecords)
	return 1;

    offset = rec_offset[recno - 1];
    if (bu_fseek(fd, offset, 0)) {
	bu_log("Error in seek\n");
	perror("Readrec");
	bu_exit(1, NULL);
    }
    counter = 0;

    i = 0;
    while (i < reclen) {
	ch = getc(fd);
	if (ch == EOF || ch == '\n')
	    break;
	if (ch == '\r')
	    continue;	/* strip CR of a CR-LF terminator */
	card[i++] = ch;
    }

    /* space-pad the remaining data columns (short/unpadded records) and
     * bound the column just past the last one so atoi() on a trailing
     * field cannot run into stale buffer contents */
    for (; i <= reclen; i++)
	card[i] = ' ';

    return 0;
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
