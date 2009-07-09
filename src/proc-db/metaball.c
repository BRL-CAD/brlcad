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
#include "rtgeom.h"
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

    RT_CK_WDB(fp);

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
	pts[i][4] = 0.0; /* not sweaty, unused with iso method */
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
mix_balls(struct db_i *dbip, const char *name, int ac, const char *av[])
{
    int i;
    struct directory *dp;
    struct rt_metaball_internal *newmp;

    RT_CK_DBI(dbip);

    /* manually create a metaballl instead of calling mk_metaball just
     * to show a different creation approach.
     */
    BU_GETSTRUCT(newmp, rt_metaball_internal);
    newmp->magic = RT_METABALL_INTERNAL_MAGIC;
    newmp->threshold = 1.0;
    newmp->method = 1;
    BU_LIST_INIT(&newmp->metaball_ctrl_head);

    bu_log("Combining together the following metaballs:\n");
    for (i = 0; i < ac; i++) {
	struct rt_db_internal dir;
	struct rt_metaball_internal *mp;
	struct wdb_metaballpt *mpt;

	bu_log("\t%s\n", av[i]);
	dp = db_lookup(dbip, av[i], 1);
	if (!dp) {
	    bu_log("Unable to find %s\n", av[i]);
	}

	if (rt_db_get_internal(&dir, dp, dbip, NULL, &rt_uniresource) < 0) {
	    bu_log("Unable to load %s\n", av[i]);
	    continue;
	}

	/* get the primitive-specific internal structure */
	mp = (struct rt_metaball_internal *)dir.idb_ptr;
	RT_METABALL_CK_MAGIC(mp);

	/* iterate over each point and add it to our new metaball */
	for (BU_LIST_FOR(mpt, wdb_metaballpt, &mp->metaball_ctrl_head)) {
	    bu_log("Adding point (%lf %lf %lf)\n", V3ARGS(mpt->coord));
	    rt_metaball_add_point(newmp, &(mpt->coord), mpt->fldstr, mpt->sweat);
	}
    }

    bu_log("Joining balls together and creating [%s]\n", name);
    dbip->dbi_wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    wdb_export(dbip->dbi_wdbp, name, newmp, ID_METABALL, 1.0);
}


static void
make_spaghetti(const char *filename, const char *name)
{
    const char *balls[3] = {"someballs.s", "moreballs.s", NULL};
    const char title[BUFSIZ] = "metaball";
    struct rt_wdb *fp;
    struct db_i *dbip;

    /* get a write-only handle */
    fp = wdb_fopen(filename);
    if (fp == RT_WDB_NULL) {
	bu_exit(EXIT_FAILURE, "ERROR: unable to open file for writing.\n");
    }

    mk_id(fp, title);

    make_meatballs(fp, balls[0], 100);
    make_meatballs(fp, balls[1], 100);

    wdb_close(fp);

    /* done with the write-only, now begins read/write */
    dbip = db_open(filename, "rw");
    if (dbip == DBI_NULL) {
	perror("ERROR");
	bu_exit(EXIT_FAILURE, "Failed to open geometry file [%s].  Aborting.\n", filename);
    }
    db_dirbuild(dbip);

    mix_balls(dbip, name, 2, balls);

    /* and clean up */
    db_close(dbip);
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

    char outfile[MAXPATHLEN] = "metaball.g";
    int optc = 0, retval = EXIT_SUCCESS;

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

    /* make dinner */
    make_spaghetti(outfile, "meatballs.s");

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
