/*                  P I C K E T _ F E N C E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file shapes/picket_fence.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"


int main(int argc, char *argv[])
{
    long i, j, k, l;
    struct rt_wdb *fp_db = NULL;

    char name[256] = {0};
    char pname[256] = {0};
    char w1name[256] = {0};
    char w2name[256] = {0};
    char firstname[256] = {0};
    char prefix[256] = {0};

    struct wmember wm;
    struct wmember wm2;
    struct wmember fwm;
    struct wmember swm;
    struct wmember *nwm;

    fastf_t first_mat[16] = MAT_INIT_ZERO;

    fastf_t s0[24] = {0, 0, 0,
		      0, 0, 0,
		      15, 0, 0,
		      15, 0, 0,
		      0, 0, 0,
		      0, 0, 0,
		      15, 0, 0,
		      15, 0, 0};
    point_t c0 = {15, 0, 0};
    point_t w0 = {0, 0, 0};
    point_t w1 = {25, 0, 0};
    fastf_t s1[24] = {-0.001, 0, 0,
		      -0.001, 0, 0,
		      -12.6, 0, 0,
		      -12.6, 0, 0,
		      -0.001, 0, 0,
		      -0.001, 0, 0,
		      -12.6, 0, 0,
		      -12.6, 0, 0};
    fastf_t xlen = .0001;
    fastf_t ylen = -26;
    fastf_t zlen;
    fastf_t xt, yt;
    fastf_t x_top_len = 20;
    vect_t w0x = {0, 1, 0};
    vect_t w0z = {0, 0, -1};
    vect_t w1x = {0, -1, 0};
    vect_t w1z = {0, 0, -1};
    vect_t c0a = {4, 0, 0};
    vect_t c0b = {0, 0, 0};
    vect_t c0c = {4, 0, 0};
    vect_t c0d = {0, 0, 0};
    vect_t c0h = {0, 0, 0};
    fastf_t x_0, y_0, z_0, x_1, y_1, z_1;
    fastf_t height;
    fastf_t width;
    fastf_t pwidth;
    fastf_t ps;
    long numposts;
    fastf_t zstep;
    unsigned char matcolor[3] = {0, 0, 0};

    int post = 0;

    if (argc < 11) {
	bu_exit(1, "Usage: picket_fence <filename> <prefix> <height_in_mm> <spacing> [-r] <x0> <y0> <z0> ... <xn> <yn> <zn>\n");
    }
    bu_strlcpy(prefix, argv[2], sizeof(prefix));

    height = (fastf_t)atof(argv[3]);
    if (height <= 0) {
	bu_exit(1, "Invalid argument, height must be a positive value\n");
    }

    ps = atof(argv[4]);
    if (ps < 0) {
	bu_exit(1, "Invalid argument, spacing must be non-negative\n");
    }

    if (BU_STR_EQUAL(argv[5], "-r")) {
	post = 1;
	argc--; argv++;
    }

    fp_db = wdb_fopen(argv[1]);
    if (fp_db == NULL) {
	perror(argv[1]);
	bu_exit(2, NULL);
    }
    mk_id(fp_db, "Picket Fence");

    argc-=5; argv+=5; /* chomp, leave just the waypoints */

    BU_LIST_INIT(&wm.l);
    BU_LIST_INIT(&wm2.l);
    BU_LIST_INIT(&fwm.l);
    BU_LIST_INIT(&swm.l);

    for (j = 0; j < (argc / 3) - 1; j++) {
	x_0 = (fastf_t)atof(argv[0 + (3 * j)]);
	y_0 = (fastf_t)atof(argv[1 + (3 * j)]);
	z_0 = (fastf_t)atof(argv[2 + (3 * j)]);
	x_1 = (fastf_t)atof(argv[3 + (3 * j)]);
	y_1 = (fastf_t)atof(argv[4 + (3 * j)]);
	z_1 = (fastf_t)atof(argv[5 + (3 * j)]);
	if (isnan(x_0) || isnan(y_0) || isnan(z_0) || isnan(x_1) || isnan(y_1) || isnan(z_1)) {
	    bu_exit(1, "Invalid argument, position is not a valid coordiate\n");
	}

	width = sqrt(((x_1 - x_0) * (x_1 - x_0)) + ((y_1 - y_0) * (y_1 - y_0)));
	pwidth = ((fastf_t)width /
		  (fastf_t)(((int)(width / (51+ps))) * (51+ps))) * (51+ps);
	numposts = (width / pwidth);
	zstep = (z_1 - z_0) / numposts;
	zlen = -.076001 * height;
	s0[4] = s0[7] = s0[16] = s0[19] = pwidth - ps;
	s0[14] = s0[17] = s0[20] = s0[23] = height;
	c0[1] = (pwidth - ps) / 2.0;
	w0[2] = w1[2] = .924 * height;
	w1[1] = pwidth - ps;
	s1[2] = s1[11] = z_0 + (height / 3);
	s1[5] = s1[8] = z_1 + (height / 3);
	s1[14] = s1[23] = z_0 + ((height / 3) + 102);
	s1[17] = s1[20] = z_1 + ((height / 3) + 102);
	s1[4] = s1[7] = s1[16] = s1[19] = width;
	c0b[1] = (pwidth - ps) / 2.0;
	c0d[1] = (pwidth - ps) / 2.0;
	c0h[2] = height;

	snprintf(w1name, sizeof(w1name), "%swedge1-%ld.s", prefix, j);
	mk_wedge(fp_db, w1name, w0, w0x, w0z, xlen, ylen, zlen, x_top_len);

	snprintf(w2name, sizeof(w2name), "%swedge2-%ld.s", prefix, j);
	mk_wedge(fp_db, w2name, w1, w1x, w1z, xlen, ylen, zlen, x_top_len);

	snprintf(name, sizeof(name), "%spost-%ld.s", prefix, j);
	mk_arb8(fp_db, name, s0);
	mk_addmember(name, &wm.l, NULL, WMOP_UNION);
	mk_addmember(w1name, &wm.l, NULL, WMOP_SUBTRACT);
	mk_addmember(w2name, &wm.l, NULL, WMOP_SUBTRACT);

	if (post) {
	    snprintf(name, sizeof(name), "%spost_c.s", prefix);
	    mk_tgc(fp_db, name, c0, c0h, c0a, c0b, c0c, c0d);
	    mk_addmember(name, &wm.l, NULL, WMOP_UNION);
	    mk_addmember(w1name, &wm.l, NULL, WMOP_SUBTRACT);
	    mk_addmember(w2name, &wm.l, NULL, WMOP_SUBTRACT);
	}

	snprintf(name, sizeof(name), "%sls%ld.s", prefix, j);
	mk_arb8(fp_db, name, s1);
	mk_addmember(name, &swm.l, NULL, WMOP_UNION);

	for (k = 0; k < 8; k++)
	    s1[(3 * k) + 2] += (height / 3);

	snprintf(name, sizeof(name), "%shs%ld.s", prefix, j);
	mk_arb8(fp_db, name, s1);
	mk_addmember(name, &swm.l, NULL, WMOP_UNION);

	snprintf(pname, sizeof(pname), "%sp-%ld.c", prefix, j);
	matcolor[0] = 50;
	matcolor[1] = 30;
	matcolor[2] = 10;
	mk_lcomb(fp_db, pname, &wm, 0, "plastic", "", matcolor, 0);

	for (i = 0; i < numposts; i++) {
	    snprintf(name, sizeof(name), "%sp%ld-%ld.r", prefix, j, i);
	    mk_addmember(pname, &wm2.l, NULL, WMOP_UNION);

	    matcolor[0] = 50;
	    matcolor[1] = 50;
	    matcolor[2] = 20;
	    mk_lcomb(fp_db, name, &wm2, 0, "plastic", "", matcolor, 0);

	    nwm = mk_addmember(name, &swm.l, NULL, WMOP_UNION);
	    for (k = 0; k < 16; k++)
		nwm->wm_mat[k] = 0;
	    nwm->wm_mat[0] = 1;
	    nwm->wm_mat[5] = 1;
	    nwm->wm_mat[7] = i * pwidth;
	    nwm->wm_mat[10] = 1;
	    nwm->wm_mat[11] = i * zstep;
	    nwm->wm_mat[15] = 1;
	}

	snprintf(name, sizeof(name), "%ssec%ld.c", prefix, j);
	matcolor[0] = 50;
	matcolor[1] = 50;
	matcolor[2] = 20;
	mk_lcomb(fp_db, name, &swm, 0, "plastic", "", matcolor, 0);

	nwm = mk_addmember(name, &fwm.l, NULL, WMOP_SUBTRACT);
	xt = x_1 - x_0;
	yt = y_1 - y_0;
	xt /= sqrt((xt * xt) + (yt * yt));
	yt /= sqrt((xt * xt) + (yt * yt));
	nwm->wm_mat[0] = nwm->wm_mat[5] = cos(atan2(-xt, yt));
	nwm->wm_mat[1] = -sin(atan2(-xt, yt));
	nwm->wm_mat[3] = x_0;
	nwm->wm_mat[4] = -(nwm->wm_mat[1]);
	nwm->wm_mat[5] = nwm->wm_mat[0];
	nwm->wm_mat[7] = y_0;
	nwm->wm_mat[10] = 1;
	nwm->wm_mat[11] = z_0;
	nwm->wm_mat[15] = 1;

	nwm = mk_addmember(name, &fwm.l, NULL, WMOP_UNION);
	xt = x_1 - x_0;
	yt = y_1 - y_0;
	xt /= sqrt((xt * xt) + (yt * yt));
	yt /= sqrt((xt * xt) + (yt * yt));
	nwm->wm_mat[0] = nwm->wm_mat[5] = cos(atan2(-xt, yt));
	nwm->wm_mat[1] = -sin(atan2(-xt, yt));
	nwm->wm_mat[3] = x_0;
	nwm->wm_mat[4] = -(nwm->wm_mat[1]);
	nwm->wm_mat[5] = nwm->wm_mat[0];
	nwm->wm_mat[7] = y_0;
	nwm->wm_mat[10] = 1;
	nwm->wm_mat[11] = z_0;
	nwm->wm_mat[15] = 1;

	if (j == 0) {
	    bu_strlcpy(firstname, name, sizeof(firstname));
	    for (l = 0; l < 16; l++)
		first_mat[l] = nwm->wm_mat[l];
	}
    }

    nwm = mk_addmember(firstname, &fwm.l, NULL, WMOP_SUBTRACT);
    for (l = 0; l < 16; l++)
	nwm->wm_mat[l] = first_mat[l];

    snprintf(name, sizeof(name), "%sfence.c", prefix);
    matcolor[0] = 50;
    matcolor[1] = 50;
    matcolor[2] = 20;
    mk_lcomb(fp_db, name, &fwm, 0, "plastic", "", matcolor, 0);

    wdb_close(fp_db);

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
