/*                        I D E N T S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
 *
 */
/** @file burst/idents.c
 *
 */

#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "vmath.h"

#include "./burst.h"
#include "./extern.h"


#define DEBUG_IDENTS 0

int
findIdents(ident, idp)
    int ident;
    Ids *idp;
{
#if DEBUG_IDENTS
    brst_log("findIdents(%d)\n", ident);
#endif
    for (idp = idp->i_next; idp != IDS_NULL; idp = idp->i_next) {
#if DEBUG_IDENTS
	brst_log("lower=%d, upper=%d\n", (int) idp->i_lower,
		 (int) idp->i_upper);
#endif
	if (ident >= (int) idp->i_lower
	    &&	ident <= (int) idp->i_upper
	    )
	    return 1;
    }
#if DEBUG_IDENTS
    brst_log("returned 0\n");
#endif
    return 0;
}


Colors *
findColors(int ident, Colors *colp)
{
    for (colp = colp->c_next; colp != COLORS_NULL; colp = colp->c_next) {
	if (ident >= (int) colp->c_lower && ident <= (int) colp->c_upper)
	    return colp;
    }
    return COLORS_NULL;
}


/*
  void freeIdents(Ids *idp)

  Free up linked list, except for the head node.
*/
void
freeIdents(Ids *idp)
{
    if (idp->i_next == NULL)
	return;	/* finished */
    freeIdents(idp->i_next);
    free((char *) idp->i_next);
}


int
readIdents(Ids *idlist, FILE *fp)
{
    char input_buf[BUFSIZ];
    int lower, upper;
    Ids *idp;
    freeIdents(idlist); /* free old list if it exists */
    for (idp = idlist;
	 bu_fgets(input_buf, BUFSIZ, fp) != NULL;
	) {
	char *token;
	token = strtok(input_buf, ", -:; \t");
	if (token == NULL || sscanf(token, "%d", &lower) < 1)
	    continue;
	token = strtok(NULL, " \t");
	if (token == NULL || sscanf(token, "%d", &upper) < 1)
	    upper = lower;
	if ((idp->i_next = (Ids *) malloc(sizeof(Ids))) == NULL) {
	    Malloc_Bomb(sizeof(Ids));
	    return 0;
	}
	idp = idp->i_next;
	idp->i_lower = lower;
	idp->i_upper = upper;
    }
    idp->i_next = NULL;
    return 1;
}


int
readColors(Colors *colorlist, FILE *fp)
{
    char input_buf[BUFSIZ];
    int lower, upper;
    int rgb[3];
    Colors *colp;
    for (colp = colorlist;
	 bu_fgets(input_buf, BUFSIZ, fp) != NULL;
	) {
	int items = sscanf(input_buf, "%d %d %d %d %d\n", &lower, &upper, &rgb[0], &rgb[1], &rgb[2]);
	if (items < 5) {
	    if (items == EOF)
		break;
	    else {
		brst_log("readColors(): only %d items read\n",
			 items);
		continue;
	    }
	}

	colp->c_next = (Colors *) malloc(sizeof(Colors));
	if (colp->c_next == NULL) {
	    Malloc_Bomb(sizeof(Colors));
	    return 0;
	}
	colp = colp->c_next;
	colp->c_lower = lower;
	colp->c_upper = upper;
	VMOVE(colp->c_rgb, rgb);
    }
    colp->c_next = NULL;
    return 1;
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
