/*                         S T A C K . C
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

#include "./iges_struct.h"

#define STKBLK 100	/* Allocation block size */

static union tree **stk;
static int jtop, stklen;

void
Initstack()
{

    jtop = (-1);
    stklen = STKBLK;
    stk = (union tree **)bu_malloc(stklen*sizeof(union tree *), "Initstack: stk");
    if (stk == NULL) {
	bu_log("Cannot allocate stack space\n");
	perror("Initstack");
	bu_exit(1, NULL);
    }
}


/* This function pushes a pointer onto the stack. */

void
Push(ptr)
    union tree *ptr;
{

    jtop++;
    if (jtop == stklen) {
	stklen += STKBLK;
	stk = (union tree **)bu_realloc((char *)stk, stklen*sizeof(union tree *),
					"Push: stk");
	if (stk == NULL) {
	    bu_log("Cannot reallocate stack space\n");
	    perror("Push");
	    bu_exit(1, NULL);
	}
    }
    stk[jtop] = ptr;
}


/* This function pops the top of the stack. */


union tree *
Pop()
{
    union tree *ptr;

    if (jtop == (-1))
	ptr=NULL;
    else {
	ptr = stk[jtop];
	jtop--;
    }

    return ptr;
}


void
Freestack()
{
    jtop = (-1);
    return;
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
