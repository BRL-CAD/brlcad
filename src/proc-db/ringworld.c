/*                    R I N G W O R L D . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2011 United States Government as represented by
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

/** @file proc-db/ringworld.c
 *
 * Generate a 'ringworld', as imagined by Larry Niven.
 *
 * Some urls/notes
 *
 * http://www.youtube.com/watch?v=sR2296df-bc
 * http://www.freewebs.com/knownspace/rw.htm
 * http://www.alcyone.com/max/reference/scifi/ringworld.html
 *
 * sun: 1.93e26g, 1.36e9m, 3.6e33ergs/sec
 * ring: 2e30g, 1.53e11m radius, 1.605e9m width, 1600km rim walls 200' wide at
 * top. 30m bottom plus 370m foam
 * shadow squares: 20, 1.6e6x4e6m, 4.6e7m orbit
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"

#define SUN_DIAMETER	1.36e9
#define RING_ORBIT	1.53e11
#define RING_WIDTH	1.605e9
#define RING_FLOOR_THICKNESS	30
#define RING_WALL_THICKNESS	60.96
#define RING_WALL_HEIGHT	1000*1600
#define SHADOWRING_ORBIT	4.6e7
#define SHADOWRING_NUM		20
#define SHADOWRING_WIDTH	1.6e6
#define SHADOWRING_LENGTH	4.0e6
#define SHADOWRING_THICKNESS	30

int
mk_sol(struct rt_wdb *fp, double radius)
{
    struct wmember c;
    point_t p = { 0,0,0};
    /* make a sphere! tada! */
    mk_sph(fp, "sun.s", p, radius * 1000.0);

    BU_LIST_INIT(&c.l);
    mk_addmember("sun.s", &c.l, NULL, WMOP_UNION);
    mk_lcomb(fp, "sun.r", &c, 1, NULL, NULL, NULL, 0);
    return 0;
}

int
mk_ring(struct rt_wdb *fp, double orbit, double width, double thick, double wallthick, double wallheight)
{
    point_t base = {0,0,0}, height = {0,0,0};
    struct wmember c;
    /* make 3 rcc's and glue them together */

    /* this additive component */
    base[Z] = -1000*width/2.0;
    height[Z] = 1000*width;
    mk_rcc(fp, "ringadd.s", base, height, 1000*orbit);

    /* the small cut to make the walls 'n stuff */
    base[Z] += 1000*wallthick;
    height[Z] -= 1000*2.0*wallthick;
    mk_rcc(fp, "ringsub1.s", base, height, 1000*(orbit-thick));

    /* the big cut out of the middle */
    base[Z] = -1000*width;
    height[Z] = 1000*2*width;
    mk_rcc(fp, "ringsub2.s", base, height, 1000*(orbit - wallheight));

    /* and do the CSG */
    BU_LIST_INIT(&c.l);
    mk_addmember("ringadd.s", &c.l, NULL, WMOP_UNION);
    mk_addmember("ringsub1.s", &c.l, NULL, WMOP_SUBTRACT);
    mk_addmember("ringsub2.s", &c.l, NULL, WMOP_SUBTRACT);
    mk_lcomb(fp, "ring.r", &c, 1, "plastic", "", NULL, 0);

    return 0;
}

int
mk_shadowring(struct rt_wdb *UNUSED(fp), double UNUSED(orbit), int UNUSED(num), double UNUSED(width), double UNUSED(length), double UNUSED(thickness))
{
    /* make the shadow ring, num * arb8s */
    return 0;
}

int
main(int argc, char *argv[])
{
    static const char usage[] = "Usage:\n%s [-h] [-o outfile] \n\n  -h      \tShow help\n  -o file \tFile to write out (default: ringworld.g)\n\n";

    char outfile[MAXPATHLEN] = "ringworld.g";
    int optc = 0;
    struct rt_wdb *fp;

    while ((optc = bu_getopt(argc, argv, "Hho:n:")) != -1) {
	switch (optc) {
	    case 'o':
		snprintf(outfile, MAXPATHLEN, "%s", bu_optarg);;
		break;
	    case 'h' :
	    case 'H' :
	    case '?' :
		printf(usage, *argv);
		return optc == '?' ? EXIT_FAILURE : EXIT_SUCCESS;
	}
    }

    if (bu_file_exists(outfile))
	bu_exit(EXIT_FAILURE, "ERROR: %s already exists.  Remove file and try again.", outfile);

    bu_log("Writing ringworld out to [%s]\n", outfile);

    fp = wdb_fopen(outfile);

    mk_sol(fp, SUN_DIAMETER);
    mk_ring(fp, RING_ORBIT, RING_WIDTH, RING_FLOOR_THICKNESS, RING_WALL_THICKNESS, RING_WALL_HEIGHT);
    mk_shadowring(fp, SHADOWRING_ORBIT, SHADOWRING_NUM, SHADOWRING_WIDTH, SHADOWRING_LENGTH, SHADOWRING_THICKNESS);

    /* generate a comb all.g */
    {
	struct wmember c;
	BU_LIST_INIT(&c.l);
	mk_addmember("ring.r", &c.l, NULL, WMOP_UNION);
	mk_addmember("sun.r", &c.l, NULL, WMOP_UNION);
	/* mk_addmember("shadowring.r", &c.l, NULL, WMOP_UNION); */
	mk_lcomb(fp, "all.g", &c, 0, NULL, NULL, NULL, 0);
    }


    wdb_close(fp);
    bu_log("BRL-CAD geometry database file [%s] created.\nDone.\n", outfile);

    return EXIT_SUCCESS;
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
