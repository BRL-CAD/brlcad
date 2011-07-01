/*                       M A K E D I R . C
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
/** @file iges/makedir.c
 *
 * Routine to read the directory section of an IGES file.  and store
 * needed info in the 'directory' structures.  dir[n] is the structure
 * for entity #n.  The directory entry for entity #n is located on
 * line D'2n+1' of the iges file.
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

#define CR '\015'

void
Makedir()
{

    int found, i, saverec, entcount=(-1), paramptr=0, paramguess=0;
    char str[9];

    str[8] = '\0';
    bu_log("Reading Directory Section...\n");
    bu_log("Number of entities checked:\n");

    if (totentities < 1)
	goto out;

    Readrec(dstart+1);	/* read first record in directory section */

    while (1) {
	if (card[72] != 'D')	/* We are not in the directory section */
	    break;

	entcount++;	/* increment count of entities */

	if (entcount%100 == 0) {
	    printf("\t%d%c", entcount, CR);
	}

	/* save the directory record number for this entity */
	dir[entcount]->direct = atoi(&card[73]);

	/* set reference count to 0 */
	dir[entcount]->referenced = 0;

	/* set record number to read for next entity */
	saverec = currec + 2;

	Readcols(str, 8);	/* read entity type */
	dir[entcount]->type = atoi(str);

	Readcols(str, 8);	/* read pointer to parameter entry */

	/* convert it to a file record number */
	paramptr = atoi(str);
	if (paramptr == 0 && entcount > 0) {
	    paramguess = 1;
	    dir[entcount]->param = dir[entcount-1]->param + dir[entcount-1]->paramlines;
	} else if (paramptr > 0)
	    dir[entcount]->param = paramptr + pstart;
	else
	    bu_log("Entity number %d does not have a correct parameter pointer\n",
		   entcount);

	if (dir[entcount]->type == 422) {
	    /* This is an attribute instance, so get the definition */
	    Readcols(str, 8);
	    dir[entcount]->referenced = (-atoi(str));
	} else
	    counter += 8;

	counter += 16;	/* skip 16 columns */

	Readcols(str, 8);    /* read pointer to view entity */
	dir[entcount]->view = atoi(str);

	Readcols(str, 8);	/* read pointer to transformation entity */

	/* convert it to a "dir" index
	 * Use (DE + 1)/2 - 1 rather than (DE-1)/2 to get
	 * a (-1) value in "trans" field when DE is 0
	 */
	dir[entcount]->trans = (atoi(str) + 1)/2 - 1;

	/* skip next field */
	counter += 8;

	Readcols(str, 8);	/* read status entry */
	dir[entcount]->status = atoi(str);

	Readrec(currec + 1);	/* read next record into buffer */
	counter += 16;		/* skip first two fields */

	Readcols(str, 8);	/* read pointer to color entity */
	/* if pointer is negative, convert to a 'dir' index */
	dir[entcount]->colorp = atoi(str);
	if (dir[entcount]->colorp < 0)
	    dir[entcount]->colorp = (dir[entcount]->colorp + 1)/2;

	Readcols(str, 8);	/* read parameter line count */
	dir[entcount]->paramlines = atoi(str);
	if (dir[entcount]->paramlines == 0)
	    dir[entcount]->paramlines = 1;
	Readcols(str, 8);	/* read form number */
	dir[entcount]->form = atoi(str);

	/* Look for entity type in list and incrememt that count */

	found = 0;
	for (i=0; i<ntypes; i++) {
	    if (typecount[i].type == dir[entcount]->type) {
		typecount[i].count++;
		found = 1;
		break;
	    }
	}
	if (!found) {
	    typecount[0].count++;
	    if (dir[entcount]->type > 0)
		dir[entcount]->type = (-dir[entcount]->type); /* for easy recognition */
	}

	/* Check if this is a transformation entity */
	if (dir[entcount]->type == 124 || dir[entcount]->type == 700) {
	    /* Read and store the matrix */
	    if (dir[entcount]->param <= pstart) {
		bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
		       dir[entcount]->direct, dir[entcount]->name);
		dir[entcount]->rot = NULL;
	    } else {
		dir[entcount]->rot = (mat_t *)bu_malloc(sizeof(mat_t), "Makedir:matrix");
		Readmatrix(dir[entcount]->param, *dir[entcount]->rot);
	    }
	} else /* set to NULL */
	    dir[entcount]->rot = NULL;

	Readrec(saverec); /* restore previous record */
    }

out:
    bu_log("\t%d\n\n", entcount+1);
    if (paramguess)
	bu_log("Some entities did not have proper parameter pointers, so a resonable guess was made\n");
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
