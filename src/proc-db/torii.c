/*                         T O R I I . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file proc-db/torii.c
 *
 * Create a bunch of torii.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "bu/app.h"
#include "bn.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

typedef struct torus {
    point_t position;
    double majorRadius;
    double minorRadius;
    int direction;
} torus_t;

/* torus container */
typedef struct torusArray {
    torus_t *torus;
    unsigned long int count;
    unsigned long int max;
} torusArray_t;

/* organize containers by recursion level */
typedef struct torusLevels {
    torusArray_t *level;
    unsigned short int levels;
} torusLevels_t;


void
usage(char *progname)
{
    fprintf(stderr, "Usage: %s db_file.g\n", progname);
    bu_exit(-1, NULL);
}


int
create_torii(int level, int currentLevel, torusLevels_t *torii, point_t position, const int dirArray[6][6], int dir)
{
    point_t newPosition;

    VMOVE(newPosition, position);

    /* recursive case */
    if (level > 0) {
	printf("create_torii: %d\n", level);

	if (dirArray[dir][0]==1) {
	    printf("direction 0\n");
	    create_torii(level-1, currentLevel+1, torii, newPosition, dirArray, 0);
	}

	if (dirArray[dir][1]==1) {
	    printf("direction 1\n");
	    create_torii(level-1, currentLevel+1, torii, newPosition, dirArray, 1);
	}

	if (dirArray[dir][2]==1) {
	    printf("direction 2\n");
	    create_torii(level-1, currentLevel+1, torii, newPosition, dirArray, 2);
	}

	if (dirArray[dir][3]==1) {
	    printf("direction 3\n");
	    create_torii(level-1, currentLevel+1, torii, newPosition, dirArray, 3);
	}

	if (dirArray[dir][4]==1) {
	    printf("direction 4\n");
	    create_torii(level-1, currentLevel+1, torii, newPosition, dirArray, 4);
	}

	if (dirArray[dir][5]==1) {
	    printf("direction 5\n");
	    create_torii(level-1, currentLevel+1, torii, newPosition, dirArray, 5);
	}

    } else {
	/* TESTING DYNAMIC RECURSION */

	torusArray_t *ta = &torii->level[currentLevel-1];
	/* base case */
	printf("base case (%d levels deep)\n", currentLevel);

	/* see if we need to allocate more memory */
	if (ta->count >= ta->max) {
	    if ((ta->torus = (struct torus *)realloc(ta->torus, (ta->count+6)*sizeof(torus_t))) == NULL) {
		bu_log("Unable to allocate memory for torii during runtime\n");
		perror("torus_t allocation during runtime failed");
		bu_exit(3, NULL);
	    }
	    ta->max+=6;
	}

	VMOVE(ta->torus[ta->count].position, newPosition);
	ta->torus[ta->count].majorRadius = 100.0;
	ta->torus[ta->count].minorRadius = 2.0;
	ta->torus[ta->count].direction = dir;
	ta->count++;
    }
    bu_log("returning from create_torii\n");

    return 0;
}


int
output_torii(struct rt_wdb *db_fp, const char *fileName, int levels, const torusLevels_t *torii, const char *name)
{
    char scratch[256];
    struct wmember head;
    unsigned char rgb[3];
    int l;
    unsigned long int t;
    unsigned long int serial = 0;

    /* unit normals/offsets for each of the six flake directions, paired
     * as +/- along the three principal axes so the resulting torii fan
     * out into a recognizable three-dimensional cluster.
     */
    static const vect_t dirVec[6] = {
	{ 1.0,  0.0,  0.0},
	{-1.0,  0.0,  0.0},
	{ 0.0,  1.0,  0.0},
	{ 0.0, -1.0,  0.0},
	{ 0.0,  0.0,  1.0},
	{ 0.0,  0.0, -1.0}
    };

    bu_log("output_torii to file \"%s\" for %d levels using \"%s\" as the combination name\n", fileName, levels, name);

    BU_LIST_INIT(&head.l);

    /* walk every recursion level and emit each computed torus as a TOR
     * primitive, then collect them all under a single region.
     */
    for (l = 0; l < levels; l++) {
	const torusArray_t *ta = &torii->level[l];

	for (t = 0; t < ta->count; t++) {
	    const torus_t *to = &ta->torus[t];
	    point_t center;
	    vect_t normal;
	    int dir = to->direction;

	    if (dir < 0 || dir > 5)
		dir = 0;

	    /* offset the torus along its direction so siblings do not all
	     * land on top of one another; spacing scales with depth so the
	     * flake spreads outward as it recurses.
	     */
	    VSCALE(normal, dirVec[dir], 1.0);
	    VJOIN1(center, to->position, to->majorRadius * 2.0 * (double)(l + 1), dirVec[dir]);

	    snprintf(scratch, sizeof(scratch), "%s_%lu.s", name, serial);

	    if (mk_tor(db_fp, scratch, center, normal, to->majorRadius, to->minorRadius) != 0) {
		bu_log("Unable to write torus \"%s\" to the database\n", scratch);
		return 1;
	    }

	    (void)mk_addmember(scratch, &head.l, NULL, WMOP_UNION);
	    serial++;
	}
    }

    if (serial == 0) {
	bu_log("No torii were generated; nothing written to \"%s\"\n", fileName);
	return 1;
    }

    /* a pleasant brassy color for the assembled flake of torii */
    rgb[0] = 224;
    rgb[1] = 160;
    rgb[2] =  32;

    /* assemble everything into a top-level region named "all" */
    if (mk_lcomb(db_fp, "all", &head, 1, "plastic", "sh=8 sp=0.6 di=0.4", rgb, 0) != 0) {
	bu_log("Unable to write the \"all\" region to the database\n");
	return 1;
    }

    bu_log("wrote %lu torii into region \"all\"\n", serial);

    return 0;
}


int
main(int ac, char *av[])
{
    torusLevels_t torii;
    const char *prototypeName="torus";

    char fileName[512];
    struct rt_wdb *db_fp;
    char scratch[518];
    int levels=2;
    int direction=4;
    point_t initialPosition = {0.0, 0.0, 0.0};
    int i;

    /* this array is a flake pattern */
    static const int dirArray[6][6]={
	{1, 1, 0, 1, 0, 0},
	{1, 1, 1, 0, 0, 0},
	{0, 1, 1, 1, 0, 0},
	{1, 0, 1, 1, 0, 0},
	{1, 1, 1, 1, 0, 0},
	{1, 1, 1, 1, 0, 0}
    };

    bu_setprogname(av[0]);

    char *progname = *av;

    if (ac < 2) usage(progname);

    if (ac > 1) snprintf(fileName, 512, "%s", av[1]);

    bu_log("Output file name is \"%s\"\n", fileName);

    if ((db_fp = wdb_fopen(fileName)) == NULL) {
	perror(fileName);
	bu_exit(-1, NULL);
    }

    /* create the database header record */
    snprintf(scratch, sizeof(scratch), "%s Torii", fileName);
    mk_id(db_fp, scratch);

    /* init the levels array */
    torii.levels = levels;
    torii.level = (struct torusArray *)bu_calloc(levels, sizeof(torusArray_t), "torii");

    /* initialize at least a few torus to minimize allocation calls */
    for (i=0; i<levels; i++) {
	torii.level[i].torus = (struct torus *)bu_calloc(6, sizeof(torus_t), "torii.level[i].torus");
	torii.level[i].count=0;
	torii.level[i].max=6;
    }

    /* create the mofosunavabish */
    create_torii(levels, 0, &torii, initialPosition, dirArray, direction);

    /* write out the biatch to disk */
    output_torii(db_fp, fileName, levels, &torii, prototypeName);

    bu_log("\n...done! (see %s)\n", av[1]);

    db_close(db_fp->dbip);

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
