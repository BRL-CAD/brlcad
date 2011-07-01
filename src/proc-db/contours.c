/*                      C O N T O U R S . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
/** @file proc-db/contours.c
 *
 * Program to read "Brain Mapping Project" data and plot it.
 *
 */

#include "common.h"

#include <stdio.h>

#include "bu.h"
#include "plot3.h"


int x, y, z;
int npts;
char name[128];

int
main(int argc, char *argv[])
{
    int i;

    if (argc > 0)
	bu_log("Usage: %s\n", argv[0]);

    pl_3space(stdout, -32768,  -32768,  -32768, 32767, 32767, 32767);
    while (!feof(stdin)) {
	if (scanf("%d %d %128s", &npts, &z, name) != 3)  break;
	for (i=0; i<npts; i++) {
	    if (scanf("%d %d", &x, &y) != 2)
		fprintf(stderr, "bad xy\n");
	    if (i==0)
		pl_3move(stdout, x, y, z);
	    else
		pl_3cont(stdout, x, y, z);
	}
	/* Close curves? */
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
