/*                         F I N D P . C
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
/** @file iges/findp.c
 *
 * This routine reads the last record in the IGES file.  That record
 * contains the number of records in each section.  These numbers are
 * used to calculate the starting record number for the parameter
 * section and the directory section.  space is then reserved for the
 * directory.  This routine depends on "fseek" and "ftell" operating
 * with offsets given in bytes.
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

/*
 * Read the terminate (last) record of the IGES file into the shared global
 * "card" buffer to learn how many records each section contains, then
 * compute the starting record numbers of the directory ("dstart") and
 * parameter ("pstart") sections and allocate the "dir" array of directory
 * entries.  Restores the previously current record before returning
 * "pstart".
 */
int
Findp(void)
{
    int saverec, rec2;
    size_t i;
    char str[8];

    str[7] = '\0';


    saverec = currec;	/* save current record number */

    rec2 = (int)nrecords;	/* the terminate record is the last one indexed */
    Readrec(rec2);	/* read last record into "card" buffer */
    dstart = 0;
    pstart = 0;
    for (i = 0; i < 3; i++) {
	counter++;	/* skip the single letter section ID */
	Readcols(str, 7);	/* read the number of records in the section */
	pstart += atoi(str);	/* increment pstart */
	if (i == 1) {
	    /* Global section */
	    /* set record number for start of directory section */
	    dstart = pstart;
	}
    }

    /* restore current record */
    currec = saverec;
    Readrec(currec);

    /* make space for directory entries */
    totentities = (pstart - dstart)/2;
    if (totentities > 0) {
	dir = (struct iges_directory **)bu_calloc(totentities,
						  sizeof(struct iges_directory *),
						  "IGES directory*");

	for (i = 0; i < totentities; i++) {
	    BU_ALLOC(dir[i], struct iges_directory);
	    dir[i]->name = (char *)NULL;
	    dir[i]->trans = (-1);
	}
    } else
	totentities = 0;

    dirarraylen = totentities;

    return pstart;
}


/*
 * Free the "dir" array of directory entries (and any per-entry
 * transformation matrices) allocated by Findp.
 */
void
Free_dir(void)
{
    size_t i;

    for (i = 0; i < totentities; i++) {
	if (dir[i]->type == 124 || dir[i]->type == 700)
	    bu_free(dir[i]->rot, "Free_dir: dir[i]->rot");
	bu_free(dir[i], "Free_dir: dir[i]");
    }

    if (totentities > 0)
	bu_free(dir, "Free_dir: dir");
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
