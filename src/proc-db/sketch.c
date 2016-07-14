/*                        S K E T C H . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2016 United States Government as represented by
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
/** @file proc-db/sketch.c
 *
 * Program to generate test sketches
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"
#include "rt/geom.h"

extern struct rt_sketch_internal *sketch_start();

struct rt_wdb *outfp;

int
main(int argc, char **argv)
{
    struct rt_sketch_internal skt;

    /* examples of the different segment types */
    struct bezier_seg bsg;
    struct line_seg lsg[4];
    struct carc_seg csg;

    /* position of the sketch */
    point_t V = {10.0, 20.0, 30.0};

    /* define the parameter space */
    vect_t u_vec = {1.0, 0.0, 0.0}, v_vec = {0.0, 1.0, 0.0};

    /* vertices used by this sketch */
    point2d_t verts[] = {
	{ 250, 0 },	/* 0 */
	{ 500, 0 },	/* 1 */
	{ 500, 500 },	/* 2 */
	{ 0, 500 },	/* 3 */
	{ 0, 250 },	/* 4 */
	{ 250, 250 },	/* 5 */
	{ 125, 125 },	/* 6 */
	{ 0, 125 },	/* 7 */
	{ 125, 0 },	/* 8 */
	{ 200, 200 }	/* 9 */
    };

    /* overall segments */
    int reverse[6] = {0, 0, 0, 0, 0, 0};
    void *segments[6] = {NULL, NULL, NULL, NULL, NULL, NULL};

    /* bezier */
    int ctrl_points[5] = {0, 0, 0, 0, 0};

    bu_log("Usage: %s\n",argv[0]);
    if (argc > 1) {
	if ( BU_STR_EQUAL(argv[1],"-h") || BU_STR_EQUAL(argv[1],"-?"))
	    bu_exit(1, NULL);
	bu_log("Warning - ignored unsupported argument \"%s\"\n",argv[1]);
    } else
	bu_log("       Program continues running:\n");


    VSET(V, 10, 20, 30);
    VSET(u_vec, 1, 0, 0);
    VSET(v_vec, 0, 1, 0);

    skt.magic = RT_SKETCH_INTERNAL_MAGIC;
    VMOVE(skt.V, V);
    VMOVE(skt.u_vec, u_vec);
    VMOVE(skt.v_vec, v_vec);
    skt.vert_count = 10;
    skt.verts = verts;

    skt.curve.count = 6;
    skt.curve.reverse = reverse;
    skt.curve.segment = segments;

    bsg.magic = CURVE_BEZIER_MAGIC;
    bsg.degree = 4;
    bsg.ctl_points = ctrl_points;
    bsg.ctl_points[0] = 4;
    bsg.ctl_points[1] = 7;
    bsg.ctl_points[2] = 9;
    bsg.ctl_points[3] = 8;
    bsg.ctl_points[4] = 0;
    skt.curve.segment[0] = (void *)&bsg;

    lsg[0].magic = CURVE_LSEG_MAGIC;
    lsg[0].start = 0;
    lsg[0].end = 1;
    skt.curve.segment[1] = (void *)&lsg[0];

    lsg[1].magic = CURVE_LSEG_MAGIC;
    lsg[1].start = 1;
    lsg[1].end = 2;
    skt.curve.segment[2] = (void *)&lsg[1];

    lsg[2].magic = CURVE_LSEG_MAGIC;
    lsg[2].start = 2;
    lsg[2].end = 3;
    skt.curve.segment[3] = (void *)&lsg[2];

    lsg[3].magic = CURVE_LSEG_MAGIC;
    lsg[3].start = 3;
    lsg[3].end = 4;
    skt.curve.segment[4] = (void *)&lsg[3];

    csg.magic = CURVE_CARC_MAGIC;
    csg.radius = -1.0;
    csg.start = 6;
    csg.end = 5;
    skt.curve.segment[5] = (void *)&csg;

    /* write the sketch out */
    outfp = wdb_fopen("sketch.g");
    mk_id(outfp, "sketch test");
    mk_sketch(outfp, "test_sketch", &skt);

    /* cleanup */
    wdb_close(outfp);

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
