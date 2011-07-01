/*                      M E T A B A L L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file proc-db/metaball.c
 *
 * This is an example of how to programmatically create and combine
 * metaball primitives together.
 *
 * This example uses two specific techniques for creating the
 * metaballs, one using libwdb (i.e., the mk_metaball() function) and
 * another using librt (i.e., creating an rt_metaball_internal
 * directly).  The librt approach shows how points can be read from
 * existing metaballs to be combined into a new "mega metaball".
 *
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


/**
 * Creates one metaball object with 'count' random points using
 * LIBWDB's mk_metaball() interface.  
 */
static void
make_meatballs(struct rt_wdb *fp, const char *name, long count)
{
    static float *ctx; /* random context */
    static const int method = 1; /* 1==ISO */
    static const fastf_t threshold = 1.0;
    static const fastf_t SZ = 1000.0;

    long i;
    fastf_t **pts;

    RT_CK_WDB(fp);

    bn_rand_init(ctx, rand());
    bu_log("Creating [%s] object with %ld random point%s\n", name, count, count > 1 ? "s" : "");

    /* allocate a dynamic array of pointers to points.  this may be
     * subject to change but is presently the format mk_metaball()
     * wants.
     */
    pts = (fastf_t **)bu_calloc(count, sizeof(fastf_t*), "alloc metaball pts array");
    for (i=0; i < count; i++) {
	pts[i] = (fastf_t *)bu_malloc(sizeof(fastf_t)*5, "alloc metaball point");
    }

    /* create random metaball point values positioned randomly within
     * a cube with random field strengths.
     */
    for (i=0; i < count; i++) {
	/* just for explicit clarity */
	fastf_t x = bn_rand_half(ctx) * SZ;
	fastf_t y = bn_rand_half(ctx) * SZ;
	fastf_t z = bn_rand_half(ctx) * SZ;
	fastf_t field_strength = bn_rand0to1(ctx) * SZ / (2.0 * sqrt(count));

	VSET(pts[i], x, y, z);
	pts[i][3] = field_strength; /* something blobbly random */
	pts[i][4] = 0.0; /* sweat/goo field is unused with iso method */
    }

    /* pack the meat, create the metaball */
    mk_metaball(fp, name, count, method, threshold, (const fastf_t **)pts);

    /* free up our junk */
    for (i=0; i < count; i++) {
	bu_free(pts[i], "dealloc metaball point");
    }
    bu_free(pts, "dealloc metaball pts array");
}


/**
 * Creates one metaball object that includes all points from an
 * existing list (in 'av') of named metaballs.  This routine creates
 * an rt_metaball_internal object directly via LIBRT given it requires
 * reading existing metaballs from the database.
 */
static void
mix_balls(struct db_i *dbip, const char *name, int ac, const char *av[])
{
    int i;
    struct directory *dp;
    struct rt_metaball_internal *newmp;

    RT_CK_DBI(dbip);

    /* allocate a struct rt_metaball_internal object that we'll
     * manually fill in with points from the other metaballs being
     * joined together.
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

	/* get a handle on the existing database object */
	bu_log("\t%s\n", av[i]);
	dp = db_lookup(dbip, av[i], 1);
	if (!dp) {
	    bu_log("Unable to find %s\n", av[i]);
	}

	/* load the existing database object */
	if (rt_db_get_internal(&dir, dp, dbip, NULL, &rt_uniresource) < 0) {
	    bu_log("Unable to load %s\n", av[i]);
	    continue;
	}

	/* access the metaball-specific internal structure */
	mp = (struct rt_metaball_internal *)dir.idb_ptr;
	RT_METABALL_CK_MAGIC(mp);

	/* iterate over each point in that database objct and add it
	 * to our new metaball.
	 */
	for (BU_LIST_FOR(mpt, wdb_metaballpt, &mp->metaball_ctrl_head)) {
	    bu_log("Adding point (%lf %lf %lf)\n", V3ARGS(mpt->coord));
	    rt_metaball_add_point(newmp, (const point_t *)&mpt->coord, mpt->fldstr, mpt->sweat);
	}
    }

    bu_log("Joining balls together and creating [%s] object\n", name);

    /* write out new "mega metaball" out to disk */
    dbip->dbi_wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    wdb_export(dbip->dbi_wdbp, name, newmp, ID_METABALL, 1.0);
}


/**
 * main driver logic that creates the various random metaballs and
 * then creates another that combines them all into one.
 */
static void
make_spaghetti(const char *filename, const char *name, long count)
{
    const char *balls[4] = {"someballs.s", "moreballs.s", "manyballs.s", NULL};
    const char title[BUFSIZ] = "metaball";
    struct rt_wdb *fp;
    struct db_i *dbip;

    long some, more, many;

    /* get a write-only handle */
    fp = wdb_fopen(filename);
    if (fp == RT_WDB_NULL) {
	bu_exit(EXIT_FAILURE, "ERROR: unable to open file for writing.\n");
    }

    mk_id(fp, title);

    /* just to make things interesting, make varying sized sets */
    some = (long)ceil((double)count * (1.0 / 111.0));
    more = (long)ceil((double)count * (10.0 / 111.0));
    many = (long)ceil((double)count * (100.0 / 111.0));

    /* create individual metaballs with random points using LIBWDB */
    make_meatballs(fp, balls[0], some);
    make_meatballs(fp, balls[1], more);
    make_meatballs(fp, balls[2], many);

    mk_comb1(fp, "someballs.r", balls[0], 1);
    mk_comb1(fp, "moreballs.r", balls[1], 1);
    mk_comb1(fp, "manyballs.r", balls[2], 1);
    mk_comb1(fp, "meatballs.r", "meatballs.s", 1);

    wdb_close(fp);

    /* done with the write-only, now begins read/write */
    dbip = db_open(filename, "rw");
    if (dbip == DBI_NULL) {
	perror("ERROR");
	bu_exit(EXIT_FAILURE, "Failed to open geometry file [%s].  Aborting.\n", filename);
    }

    /* load a database directory */
    db_dirbuild(dbip);

    /* combine all metaballs into one "mega metaball" */
    mix_balls(dbip, name, 3, balls);

    /* and clean up */
    db_close(dbip);
}


int
main(int argc, char *argv[])
{
    static const char usage[] = "Usage:\n%s [-h] [-o outfile] [-n count]\n\n  -h      \tShow help\n  -o file \tFile to write out (default: metaball.g)\n  -n count\tTotal metaball point count (default 555)\n\n";

    char outfile[MAXPATHLEN] = "metaball.g";
    int optc = 0;
    long count = 555;

    while ((optc = bu_getopt(argc, argv, "Hho:n:")) != -1) {
	switch (optc) {
	    case 'o':
		snprintf(outfile, MAXPATHLEN, "%s", bu_optarg);;
		break;
	    case 'n':
		count = atoi(bu_optarg);
		break;
	    case 'h' :
	    case 'H' :
	    case '?' :
		printf(usage, *argv);
		return optc == '?' ? EXIT_FAILURE : EXIT_SUCCESS;
	}
    }

    if (count <= 0) {
	bu_exit(EXIT_FAILURE, "ERROR: count must be greater than zero");
    }

    if (bu_file_exists(outfile)) {
	bu_exit(EXIT_FAILURE, "ERROR: %s already exists.  Remove file and try again.", outfile);
    }

    bu_log("Writing metaballs out to [%s]\n", outfile);

    /* make dinner */
    make_spaghetti(outfile, "meatballs.s", count);

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
