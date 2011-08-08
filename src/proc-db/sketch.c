/*                        S K E T C H . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2011 United States Government as represented by
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
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"
#include "rtgeom.h"

extern struct rt_sketch_internal *sketch_start();

struct rt_wdb *outfp;

int
main(int argc, char **argv)
{
    struct rt_sketch_internal skt;
    struct bezier_seg bsg;
    struct line_seg lsg[4];
    struct carc_seg csg;
    point_t V;
    vect_t u_vec, v_vec;
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
    int reverse[] = {0, 0, 0, 0, 0, 0};
    genptr_t segments[] = {NULL, NULL, NULL, NULL, NULL, NULL};
    int ctrl_points[] = {0, 0, 0, 0, 0};

    if (argc > 1)
	bu_log("Usage: %s\nWarning - ignored unsupported argument \"%s\"\n", argv[0], argv[1]);

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
    skt.curve.segment[0] = (genptr_t)&bsg;

    lsg[0].magic = CURVE_LSEG_MAGIC;
    lsg[0].start = 0;
    lsg[0].end = 1;
    skt.curve.segment[1] = (genptr_t)&lsg[0];

    lsg[1].magic = CURVE_LSEG_MAGIC;
    lsg[1].start = 1;
    lsg[1].end = 2;
    skt.curve.segment[2] = (genptr_t)&lsg[1];

    lsg[2].magic = CURVE_LSEG_MAGIC;
    lsg[2].start = 2;
    lsg[2].end = 3;
    skt.curve.segment[3] = (genptr_t)&lsg[2];

    lsg[3].magic = CURVE_LSEG_MAGIC;
    lsg[3].start = 3;
    lsg[3].end = 4;
    skt.curve.segment[4] = (genptr_t)&lsg[3];

    csg.magic = CURVE_CARC_MAGIC;
    csg.radius = -1.0;
    csg.start = 6;
    csg.end = 5;
    skt.curve.segment[5] = (genptr_t)&csg;

    /* write the sketch out */
    outfp = wdb_fopen("sketch.g");
    mk_id(outfp, "sketch test");
    mk_sketch(outfp, "test_sketch", &skt);

    /* cleanup and release */
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
