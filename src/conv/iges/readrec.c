/*                       R E A D R E C . C
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
/** @file iges/readrec.c
 *
 * This routine reads record number "recno" from the IGES file and
 * places it in the "card" buffer (available for reading by
 * "readstrg", "readint", "readflt", "readtime", "readcols", etc.  The
 * record is located by calculating the offset into the file and doing
 * an "fseek".  This obviously depends on the "UNIX" characteristic of
 * "fseek" understanding offsets in bytes.  The record length is
 * calculated by the routine "recsize".  This routine id typically
 * called before calling the other "read" routines to get to the
 * desired record.  The "read" routines then call this routine if the
 * buffer empties.
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

int
Readrec(int recno)
{

    int i, ch;
    long offset;

    currec = recno;
    offset = (recno - 1) * reclen;
    if (fseek(fd, offset, 0)) {
	bu_log("Error in seek\n");
	perror("Readrec");
	bu_exit(1, NULL);
    }
    counter = 0;

    for (i=0; i<reclen; i++) {
	if ((ch=getc(fd)) == EOF)
	    return 1;
	card[i] = ch;
    }

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
