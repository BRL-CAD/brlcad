/*                    M K B U I L D I N G . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file proc-db/mkbuilding.c
 *
 * application entry point for mkbuilding proc-db
 *
 */

#include "common.h"
#include "mkbuilding.h"

void mkbdlg_usage(void)
{
    fprintf(stderr, "Usage: mkbuilding db_file.g\n");
}


int
main(int ac, char *av[])
{
    struct rt_wdb *db_filepointer;
    point_t p1, p2;

    if (ac < 2) {
	mkbdlg_usage();
	return 1;
    }

    if ((db_filepointer = wdb_fopen(av[1])) == NULL) {
	perror(av[1]);
	mkbdlg_usage();
	return 1;
    }

    VSET(p1, 0.0, 0.0, 0.0);
    VSET(p2, (25.4*6), (25.4*12*10), (25.4*12*8));

    mkbldg_makeWallSegment("TestWall", db_filepointer, p1, p2);

    wdb_close(db_filepointer);
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
