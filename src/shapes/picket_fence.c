/*                  P I C K E T _ F E N C E . C
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
/** @file shapes/picket_fence.c
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "wdb.h"

struct picket_fence_args {
    int help;
    int round_posts;
};

static const struct bu_arg_option picket_fence_options[] = {
    BU_ARG_FLAG("r", "round-posts", struct picket_fence_args, round_posts,
	"Add a round post at each waypoint"),
    BU_ARG_FLAG("h", "help", struct picket_fence_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 0),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand picket_fence_operands[] = {
    BU_ARG_OPERAND("filename", BU_ARG_VALUE_FILE, 1, 1,
	"Output BRL-CAD database", NULL),
    BU_ARG_OPERAND("prefix", BU_ARG_VALUE_STRING, 1, 1,
	"Generated object name prefix", NULL),
    BU_ARG_OPERAND_VALIDATE("height", BU_ARG_VALUE_NUMBER, 1, 1,
	bu_arg_positive_number_validate, "Positive fence height in millimeters",
	NULL),
    BU_ARG_OPERAND_VALIDATE("spacing", BU_ARG_VALUE_NUMBER, 1, 1,
	bu_arg_nonnegative_number_validate,
	"Non-negative picket spacing in millimeters", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_operand picket_fence_waypoint_roles[] = {
    BU_ARG_OPERAND("x", BU_ARG_VALUE_NUMBER, 1, 1, "X coordinate", NULL),
    BU_ARG_OPERAND("y", BU_ARG_VALUE_NUMBER, 1, 1, "Y coordinate", NULL),
    BU_ARG_OPERAND("z", BU_ARG_VALUE_NUMBER, 1, 1, "Z coordinate", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_operand_group picket_fence_operand_groups[] = {
    BU_ARG_OPERAND_GROUP("waypoint", picket_fence_waypoint_roles, 2,
	BU_ARG_COUNT_UNLIMITED, "Fence path waypoints"),
    BU_ARG_OPERAND_GROUP_NULL
};

static const struct bu_arg_schema picket_fence_schema = BU_ARG_SCHEMA(
    "picket_fence", "Create fence geometry along a three-dimensional path",
    picket_fence_options, picket_fence_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(picket_fence_operand_groups, NULL, NULL, NULL,
	NULL));

static const struct bu_arg_grammar picket_fence_grammar = {
    &picket_fence_schema, NULL, NULL
};


int
main(int argc, char *argv[])
{
    long i, j, k, l;
    struct rt_wdb *fp_db = NULL;

    char firstname[256] = {0};
    char prefix[256] = {0};

    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls pname= BU_VLS_INIT_ZERO;
    struct bu_vls w1name = BU_VLS_INIT_ZERO;
    struct bu_vls w2name = BU_VLS_INIT_ZERO;

    struct wmember wm;
    struct wmember wm2;
    struct wmember fwm;
    struct wmember swm;
    struct wmember *nwm;

    bu_setprogname(argv[0]);

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

    struct picket_fence_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    int introspection = bu_arg_introspect(&msg, &picket_fence_grammar,
	argc - 1, (const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    argc--;
    argv++;
    int operand_index = bu_arg_schema_parse_complete(&picket_fence_schema,
	&args, &msg, argc, (const char **)argv);
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&picket_fence_schema, "picket_fence");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "picket_fence help");
	return 0;
    }

    argc -= operand_index;
    argv += operand_index;
    bu_strlcpy(prefix, argv[1], sizeof(prefix));

    if (!bu_arg_number_from_str(NULL, &height, argv[2]) ||
	!bu_arg_number_from_str(NULL, &ps, argv[3]))
	bu_exit(1, "Internal error converting validated fence dimensions\n");

    fp_db = wdb_fopen(argv[0]);
    if (fp_db == NULL) {
	perror(argv[0]);
	bu_exit(2, NULL);
    }
    mk_id(fp_db, "Picket Fence");

    argc -= 4;
    argv += 4;

    BU_LIST_INIT(&wm.l);
    BU_LIST_INIT(&wm2.l);
    BU_LIST_INIT(&fwm.l);
    BU_LIST_INIT(&swm.l);

    for (j = 0; j < (argc / 3) - 1; j++) {
	if (!bu_arg_number_from_str(NULL, &x_0, argv[0 + (3 * j)]) ||
	    !bu_arg_number_from_str(NULL, &y_0, argv[1 + (3 * j)]) ||
	    !bu_arg_number_from_str(NULL, &z_0, argv[2 + (3 * j)]) ||
	    !bu_arg_number_from_str(NULL, &x_1, argv[3 + (3 * j)]) ||
	    !bu_arg_number_from_str(NULL, &y_1, argv[4 + (3 * j)]) ||
	    !bu_arg_number_from_str(NULL, &z_1, argv[5 + (3 * j)]))
	    bu_exit(1, "Internal error converting validated waypoint coordinates\n");

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

	bu_vls_sprintf(&w1name, "%swedge1-%ld.s", prefix, j);
	mk_wedge(fp_db, bu_vls_cstr(&w1name), w0, w0x, w0z, xlen, ylen, zlen, x_top_len);

	bu_vls_sprintf(&w2name, "%swedge2-%ld.s", prefix, j);
	mk_wedge(fp_db, bu_vls_cstr(&w2name), w1, w1x, w1z, xlen, ylen, zlen, x_top_len);

	bu_vls_sprintf(&name, "%spost-%ld.s", prefix, j);
	mk_arb8(fp_db, bu_vls_cstr(&name), s0);
	mk_addmember(bu_vls_cstr(&name), &wm.l, NULL, WMOP_UNION);
	mk_addmember(bu_vls_cstr(&w1name), &wm.l, NULL, WMOP_SUBTRACT);
	mk_addmember(bu_vls_cstr(&w2name), &wm.l, NULL, WMOP_SUBTRACT);

	if (args.round_posts) {
	    bu_vls_sprintf(&name, "%spost_c.s", prefix);
	    mk_tgc(fp_db, bu_vls_cstr(&name), c0, c0h, c0a, c0b, c0c, c0d);
	    mk_addmember(bu_vls_cstr(&name), &wm.l, NULL, WMOP_UNION);
	    mk_addmember(bu_vls_cstr(&w1name), &wm.l, NULL, WMOP_SUBTRACT);
	    mk_addmember(bu_vls_cstr(&w2name), &wm.l, NULL, WMOP_SUBTRACT);
	}

	bu_vls_sprintf(&name, "%sls%ld.s", prefix, j);
	mk_arb8(fp_db, bu_vls_cstr(&name), s1);
	mk_addmember(bu_vls_cstr(&name), &swm.l, NULL, WMOP_UNION);

	for (k = 0; k < 8; k++)
	    s1[(3 * k) + 2] += (height / 3);

	bu_vls_sprintf(&name, "%shs%ld.s", prefix, j);
	mk_arb8(fp_db, bu_vls_cstr(&name), s1);
	mk_addmember(bu_vls_cstr(&name), &swm.l, NULL, WMOP_UNION);

	bu_vls_sprintf(&pname, "%sp-%ld.c", prefix, j);
	matcolor[0] = 50;
	matcolor[1] = 30;
	matcolor[2] = 10;
	mk_lcomb(fp_db, bu_vls_cstr(&pname), &wm, 0, "plastic", "", matcolor, 0);

	for (i = 0; i < numposts; i++) {
	    bu_vls_sprintf(&name, "%sp%ld-%ld.r", prefix, j, i);
	    mk_addmember(bu_vls_cstr(&pname), &wm2.l, NULL, WMOP_UNION);

	    matcolor[0] = 50;
	    matcolor[1] = 50;
	    matcolor[2] = 20;
	    mk_lcomb(fp_db, bu_vls_cstr(&name), &wm2, 0, "plastic", "", matcolor, 0);

	    nwm = mk_addmember(bu_vls_cstr(&name), &swm.l, NULL, WMOP_UNION);
	    for (k = 0; k < 16; k++)
		nwm->wm_mat[k] = 0;
	    nwm->wm_mat[0] = 1;
	    nwm->wm_mat[5] = 1;
	    nwm->wm_mat[7] = i * pwidth;
	    nwm->wm_mat[10] = 1;
	    nwm->wm_mat[11] = i * zstep;
	    nwm->wm_mat[15] = 1;
	}

	bu_vls_sprintf(&name, "%ssec%ld.c", prefix, j);
	matcolor[0] = 50;
	matcolor[1] = 50;
	matcolor[2] = 20;
	mk_lcomb(fp_db, bu_vls_cstr(&name), &swm, 0, "plastic", "", matcolor, 0);

	nwm = mk_addmember(bu_vls_cstr(&name), &fwm.l, NULL, WMOP_SUBTRACT);
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

	nwm = mk_addmember(bu_vls_cstr(&name), &fwm.l, NULL, WMOP_UNION);
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
	    bu_strlcpy(firstname, bu_vls_cstr(&name), sizeof(firstname));
	    for (l = 0; l < 16; l++)
		first_mat[l] = nwm->wm_mat[l];
	}
    }

    nwm = mk_addmember(firstname, &fwm.l, NULL, WMOP_SUBTRACT);
    for (l = 0; l < 16; l++)
	nwm->wm_mat[l] = first_mat[l];

    bu_vls_printf(&name, "%sfence.c", prefix);
    matcolor[0] = 50;
    matcolor[1] = 50;
    matcolor[2] = 20;
    mk_lcomb(fp_db, bu_vls_cstr(&name), &fwm, 0, "plastic", "", matcolor, 0);

    db_close(fp_db->dbip);
    bu_vls_free(&name);

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
