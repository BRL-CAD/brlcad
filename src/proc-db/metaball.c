/*                      M E T A B A L L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file metaball.c
 *
 * Some fun at dinner.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"


static void
make_meatballs(struct rt_wdb *fp, const char *name, int count)
{
    static float *ctx; /* random context */
    static const int method = 1; /* 1==ISO */
    static const fastf_t threshold = 1.0;
    static const fastf_t SZ = 1000.0;

    int i;
    fastf_t **pts;

    bn_rand_init(ctx, rand());
    bu_log("Creating [%s] with %d random points\n", name, count);

    pts = (fastf_t **)bu_calloc(count, sizeof(fastf_t*), "alloc metaball pts array");
    for (i=0; i < count; i++) {
	pts[i] = (fastf_t *)bu_malloc(sizeof(fastf_t)*5, "alloc metaball point");
    }

    /* create random metaball points */
    for (i=0; i < count; i++) {
	/* just for explicit clarity */
	fastf_t x = bn_rand_half(ctx) * SZ;
	fastf_t y = bn_rand_half(ctx) * SZ;
	fastf_t z = bn_rand_half(ctx) * SZ;
	fastf_t field_strength = bn_rand0to1(ctx) * SZ / sqrt(count);

	VSET(pts[i], x, y, z);
	pts[i][3] = field_strength; /* something blobbly random */
	pts[i][4] = 0.0; /* not sweaty */
    }

    /* pack the meat */
    mk_metaball(fp, name, count, method, threshold, pts);

    /* free up our junk */
    for (i=0; i < count; i++) {
	bu_free(pts[i], "dealloc metaball point");
    }
    bu_free(pts, "dealloc metaball pts array");
}


static void
mix_balls(struct rt_wdb *fp, const char *name, int ac, const char *av[])
{
    int i;
    bu_log("Combining together the following metaballs:\n");
    for (i = 0; i < ac; i++) {
	bu_log("\t%s\n", av[i]);
    }
    bu_log("Joining balls together and creating [%s]\n", name);

    /* TODO: show how to load and join metaballs together */
}


static void
make_spaghetti(struct rt_wdb *fp, const char *name)
{
    const char *balls[3] = {"someballs.s", "moreballs.s", NULL};
    make_meatballs(fp, balls[0], 100);
    make_meatballs(fp, balls[1], 100);

    mix_balls(fp, name, 2, balls);
}


int
main(int argc, char *argv[])
{
    static const char usage[] = "\
Usage:\n\
\t%s [-h] [-o outfile]\n\
\n\
\t-h\tShow help\n\
\t-o file\tFile to write out\n\
\n";

    char title[BUFSIZ] = "metaball", outfile[MAXPATHLEN] = "metaball.g";
    int optc = 0, retval = EXIT_SUCCESS;
    struct rt_wdb *outfp;

    while ((optc = bu_getopt(argc, argv, "Hho:")) != -1)
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

    if (bu_file_exists(outfile)) {
	bu_exit(EXIT_FAILURE, "ERROR: %s already exists.  Remove file and try again.", outfile);
    }

    /* prepare for guests */
    outfp = wdb_fopen(outfile);
    if (outfp == RT_WDB_NULL) {
	perror("Failed to open file for writing.  Aborting.\n");
	return EXIT_FAILURE;
    }
    mk_id(outfp, title);

    /* make dinner */
    make_spaghetti(outfp, "meatballs.s");

    /* and clean up */
    wdb_close(outfp);

    return retval;
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
