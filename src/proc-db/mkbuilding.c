/*                    M K B U I L D I N G . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2026 United States Government as represented by
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

#include "mkbuilding.h"
#include "bu/app.h"

void mkbdlg_usage(void)
{
    fprintf(stderr, "Usage: mkbuilding db_file.g\n");
    fprintf(stderr, "      (if db_file.g is omitted, 'mkbuilding.g' is assumed)\n");
}


int
main(int ac, const char *av[])
{
    struct rt_wdb *db_filepointer;

    /* overall building footprint and heights, in millimeters */
    fastf_t wall_t = 6.0 * 25.4;	/* 6" thick walls */
    fastf_t slab_t = 6.0 * 25.4;	/* 6" thick floor/roof slabs */
    fastf_t width = 12.0 * 12.0 * 25.4;	/* 12' wide  (X) */
    fastf_t depth = 16.0 * 12.0 * 25.4;	/* 16' deep  (Y) */
    fastf_t height = 8.0 * 12.0 * 25.4;	/* 8'  tall  (Z) */
    fastf_t eave = 12.0 * 25.4;		/* 1'  roof overhang */

    point_t p1, p2;
    unsigned char rgb[3];

    struct wmember building;
    struct bu_list *child_list = &building.l;

    bu_setprogname(av[0]);

    if (ac < 2) {
	mkbdlg_usage();
	fprintf(stderr, "       Program continues running:\n");
	av[1]="mkbuilding.g";
    }

/* Try to disallow file names starting with - . */
    if (av[1][0] == '-') {
	if (av[1][1] != 'h' && av[1][1] !='?')
	    fprintf(stderr,"mkbuilding accepts no options except h or ? for help\n");
	mkbdlg_usage();
	return 1;
    }

    if ((db_filepointer = wdb_fopen(av[1])) == NULL) {
	perror(av[1]);
	mkbdlg_usage();
	return 1;
    }

    BU_LIST_INIT(child_list);

    /*
     * Floor slab: a solid slab spanning the full footprint, sitting
     * just below the walls.
     */
    VSET(p1, 0.0, 0.0, -slab_t);
    VSET(p2, width, depth, 0.0);
    mk_rpp(db_filepointer, "floor.s", p1, p2);
    (void)mk_addmember("floor.s", child_list, NULL, WMOP_UNION);

    /*
     * Four walls sitting on top of the floor slab.  Each wall is a
     * solid slab one wall-thickness thick along its short axis.
     */

    /* front wall (low Y), with a door opening via the frame helper */
    VSET(p1, 0.0, 0.0, 0.0);
    VSET(p2, width, wall_t, height);
    mkbldg_makeWallSegment("wall_front", db_filepointer, p1, p2);
    (void)mk_addmember("wall_front_frame", child_list, NULL, WMOP_UNION);

    /* back wall (high Y), with a window opening via the frame helper */
    VSET(p1, 0.0, depth - wall_t, 0.0);
    VSET(p2, width, depth, height);
    mkbldg_makeWallSegment("wall_back", db_filepointer, p1, p2);
    (void)mk_addmember("wall_back_frame", child_list, NULL, WMOP_UNION);

    /* left wall (low X), solid slab between the front and back walls */
    VSET(p1, 0.0, wall_t, 0.0);
    VSET(p2, wall_t, depth - wall_t, height);
    mk_rpp(db_filepointer, "wall_left.s", p1, p2);
    (void)mk_addmember("wall_left.s", child_list, NULL, WMOP_UNION);

    /* right wall (high X), solid slab between the front and back walls */
    VSET(p1, width - wall_t, wall_t, 0.0);
    VSET(p2, width, depth - wall_t, height);
    mk_rpp(db_filepointer, "wall_right.s", p1, p2);
    (void)mk_addmember("wall_right.s", child_list, NULL, WMOP_UNION);

    /*
     * Roof slab: a flat slab covering the walls, extended outward on
     * all sides to form a modest eave overhang.
     */
    VSET(p1, -eave, -eave, height);
    VSET(p2, width + eave, depth + eave, height + slab_t);
    mk_rpp(db_filepointer, "roof.s", p1, p2);
    (void)mk_addmember("roof.s", child_list, NULL, WMOP_UNION);

    /*
     * Assemble everything under a single top-level group named "all".
     */
    VSET(rgb, 200, 180, 150);
    mk_lcomb(db_filepointer, "all", &building, 0, NULL, NULL, rgb, 0);

    db_close(db_filepointer->dbip);
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
