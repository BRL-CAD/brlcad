/*                     D U N N C O L O R . C
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
/** @file util/dunncolor.c
 *
 * Sets the exposure values in the Dunn camera to the
 * specified arguments.
 *
 * dunncolor baseval redval greenval blueval
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bu.h"


extern int fd;
extern char cmd;
extern int polaroid;

extern void dunnopen(void);
extern int ready(int nsecs);
extern void getexposure(char *title);
extern int dunnsend(char color, int val);


int
main(int argc, char **argv)
{

    dunnopen();

    if (!ready(5)) {
	bu_exit(50, "dunncolor:  camera not ready\n");
    }

    if (argc > 2 && BU_STR_EQUAL(argv[1], "-p")) {
	/* Polaroid rather than external camera */
	polaroid = 1;
	argc--; argv++;
    }
    getexposure("old");
    if (!ready(5)) {
	bu_exit(50, "dunncolor:  camera not ready\n");
    }

    /* check argument */
    if (argc != 5 && argc != 6) {
	bu_exit(25, "usage: dunncolor [-p] baseval redval greenval blueval\n");
    }

    dunnsend('A', atoi(*++argv));
    dunnsend('R', atoi(*++argv));
    dunnsend('G', atoi(*++argv));
    dunnsend('B', atoi(*++argv));

    getexposure("new");

    if (!ready(5)) {
	bu_exit(50, "dunncolor:  camera not ready\n");
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
