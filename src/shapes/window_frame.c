/*                  W I N D O W _ F R A M E . C
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
/** @file shapes/window_frame.c
 *
 * Program to make a window frame using libwdb.  The objects will be in
 * millimeters.  The window frames are composed of four arb8s and eight
 * cylinders.  The front of the window frame is centered at (0, 0, 0) and
 * extends in the negative x-direction the depth of the window frame.
 *
 * Introduced in BRL-CAD release 4.4.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "rt/db4.h"
#include "vmath.h"
#include "bu/app.h"
#include "bu/arg.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "wdb.h"

static const char explain[]   = "This program constructs a window frame with all\n\
edges and corners rounded.\n";

struct window_frame_args {
    int help;
    const char *output_file;
    int count;
    double height;
    double width;
    double depth;
    double radius;
    double frame_width;
};

static const struct bu_arg_option window_frame_options[] = {
    BU_ARG_FLAG("h", "help", struct window_frame_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE("f", "output", struct window_frame_args, output_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "database.g",
	"Write the generated geometry to this database"),
    BU_ARG_INTEGER_RANGE("n", "count", struct window_frame_args, count,
	1, 1, 1, 256, "count", "Create this many window frames"),
    BU_ARG_VALUE_VALIDATE("H", "height", struct window_frame_args, height,
	BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate, "height",
	"Set the positive frame height in millimeters"),
    BU_ARG_VALUE_VALIDATE("w", "width", struct window_frame_args, width,
	BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate, "width",
	"Set the positive frame width in millimeters"),
    BU_ARG_VALUE_VALIDATE("d", "depth", struct window_frame_args, depth,
	BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate, "depth",
	"Set the positive frame depth in millimeters"),
    BU_ARG_VALUE_VALIDATE("r", "radius", struct window_frame_args, radius,
	BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate, "radius",
	"Set the positive rounded-corner radius in millimeters"),
    BU_ARG_VALUE_VALIDATE("i", "material-width", struct window_frame_args,
	frame_width, BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate,
	"material-width", "Set the positive frame-material width in millimeters"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema window_frame_schema = BU_ARG_SCHEMA(
    "window_frame", "Create a BRL-CAD database of rounded window frames",
    window_frame_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar window_frame_grammar = {
    &window_frame_schema, NULL, NULL
};

int
main(int argc, char **argv)
{
    /* START # 1 */
    struct rt_wdb *fpw;		/* File to be written to. */
#define NAME_LEN 256
    char filemged[NAME_LEN+1] = {0};	/* Mged file create. */
    double hgt = 0.0;       	/* Height, width, & depth of outside window */
    double wid = 0.0;
    double dpt = 0.0;
    /* frame. */
    double rds = 0.0;		/* Radius of the corner of window frame. */
    double isw = 0.0;			/* Width of frame itself. */
    point_t pts[8];		/* Eight points of arb8. */
    point_t bs;			/* Base of rcc. */
    vect_t ht;			/* Height of rcc. */
    fastf_t rad;		/* Radius of rcc. */
    char solnam[9];		/* Solid name. */
    char regnam[8];		/* Region name. */
    char grpnam[5];		/* Group name. */
    int numwin = 0;		/* Number of window frames to be created. */

    struct wmember comb;	/* Used to make regions. */
    struct wmember comb1;	/* Used to make groups. */

    int i, j, k;		/* Loop counters. */
    int ret;

    bu_setprogname(argv[0]);

    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int introspection = bu_arg_introspect(&msg, &window_frame_grammar,
	argc - 1, (const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    /* Set up solid, region, and group names. */
    solnam[0] = 's';
    solnam[1] = '.';
    solnam[2] = 'w';
    solnam[3] = 'f';
    solnam[4] = 'r';
    solnam[5] = ' ';
    solnam[6] = '#';
    solnam[7] = '#';
    solnam[8] = '\0';
    regnam[0] = 'r';
    regnam[1] = '.';
    regnam[2] = 'w';
    regnam[3] = 'f';
    regnam[4] = 'r';
    regnam[5] = ' ';
    regnam[6] = '#';
    regnam[7] = '\0';
    grpnam[0] = 'w';
    grpnam[1] = 'f';
    grpnam[2] = 'r';
    grpnam[3] = ' ';
    grpnam[4] = '\0';

    /* If there are no arguments ask questions. */
    if (argc == 1) {
	/* START # 3 */

	/* Print info about the window frame. */
	printf("Usage: window_frame [-f name.g] [-n #] [-H #] [-w #] [-d #] [-r #] [-i #]\n");
	printf("\n%s\n",explain);
	printf("The window frames are composed of 4 arb8s and 8\n");
	printf("cylinders.  The front of the window frame is centered\n");
	printf("at (0, 0, 0) and extends in the negative x-direction\n");
	printf("the depth of the window frame.\n\n");

	/* Find name of mged file to be created. */
	printf("Enter the mged file to be created (%d char max).\n\t", NAME_LEN);
	(void)fflush(stdout);
	ret = scanf(CPP_SCAN(NAME_LEN), filemged);
	if (ret == 0)
	    perror("scanf");
	if (BU_STR_EQUAL(filemged, ""))
	    bu_strlcpy(filemged, "window_frame.g", sizeof(filemged));

	/* Find the number of window frames to create. */
	printf("Enter the number of window frames to create (%d max).\n\t", NAME_LEN);
	(void)fflush(stdout);
	ret = scanf("%d", &numwin);
	if (ret == 0) {
	    perror("scanf");
	    numwin = 1;
	}
	else if (numwin < 1)
	    numwin = 1;
	else if (numwin > NAME_LEN)
	    numwin = NAME_LEN;

	/* Find the dimensions of the window frames. */
	printf("Enter the height, width, and depth of the window frame.\n\t");
	(void)fflush(stdout);
	ret = scanf("%lf %lf %lf", &hgt, &wid, &dpt);
	if (ret == 0) {
	    perror("scanf");
	    hgt = 1618.0; /* golden ratio */
	    wid = 1000.0;
	    dpt = 100.0;
	}
	if (hgt < SMALL_FASTF)
	    hgt = SMALL_FASTF;
	if (wid < SMALL_FASTF)
	    wid = SMALL_FASTF;
	if (dpt < SMALL_FASTF)
	    dpt = SMALL_FASTF;

	printf("Enter the radius of the corner.\n\t");
	(void)fflush(stdout);
	ret = scanf("%lf", &rds);
	if (ret == 0) {
	    perror("scanf");
	    rds = 5.0;
	}
	if (rds < SMALL_FASTF)
	    rds = SMALL_FASTF;

	printf("Enter the actual width of the window frame.\n\t");
	(void)fflush(stdout);
	ret = scanf("%lf", &isw);
	if (ret == 0) {
	    perror("scanf");
	    isw = 100.0;
	}
	if (isw < SMALL_FASTF)
	    isw = SMALL_FASTF;
    }							/* END # 3 */

    /* If there are arguments get answers from arguments. */
    else {
	struct window_frame_args args = {0};
	if (bu_arg_schema_parse_complete(&window_frame_schema, &args, &msg,
		argc - 1, (const char **)(argv + 1)) < 0)
	    bu_exit(1, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	if (args.help) {
	    char *help = bu_arg_schema_help(&window_frame_schema,
		"window_frame");
	    bu_flog(stdout, "%s", help ? help : "");
	    bu_free(help, "window_frame help");
	    return 0;
	}
	if (args.output_file) {
	    if (strlen(args.output_file) > NAME_LEN)
		bu_exit(1, "Output filename exceeds %d characters\n", NAME_LEN);
	    bu_strlcpy(filemged, args.output_file, sizeof(filemged));
	}
	numwin = args.count;
	hgt = args.height;
	wid = args.width;
	dpt = args.depth;
	rds = args.radius;
	isw = args.frame_width;
    }

    /* Print out all info. */
    printf("\nmged file:  %s\n", filemged);
    printf("height of window frame:  %f mm\n", hgt);
    printf("width of window frame:  %f mm\n", wid);
    printf("depth of window frame:  %f mm\n", dpt);
    printf("radius of corner:  %f mm\n", rds);
    printf("width of frame:  %f mm\n", isw);
    printf("number of window frames:  %d\n\n", numwin);
    (void)fflush(stdout);

    /* Open mged file. */
    fpw = wdb_fopen(filemged);

    /* Write ident record. */
    mk_id(fpw, "window frames");

    for (i=0; i<numwin; i++) {
	/* START # 2 */
	/* Create first arb8. */
	pts[0][0] = (fastf_t)0.0;
	pts[0][1] = (fastf_t) (wid / 2.0 - rds);
	pts[0][2] = (fastf_t) (hgt / 2.0);
	pts[1][0] = (fastf_t)0.0;
	pts[1][1] = (fastf_t) (wid / 2.0 - rds);
	pts[1][2] = (fastf_t) ((-hgt) / 2.0);
	pts[2][0] = (fastf_t)0.0;
	pts[2][1] = (fastf_t) (rds - wid / 2.0);
	pts[2][2] = (fastf_t) ((-hgt) / 2.0);
	pts[3][0] = (fastf_t)0.0;
	pts[3][1] = (fastf_t) (rds - wid / 2.0);
	pts[3][2] = (fastf_t) (hgt / 2.0);
	pts[4][0] = (fastf_t)(-dpt);
	pts[4][1] = (fastf_t) (wid / 2.0 - rds);
	pts[4][2] = (fastf_t) (hgt / 2.0);
	pts[5][0] = (fastf_t)(-dpt);
	pts[5][1] = (fastf_t) (wid / 2.0 - rds);
	pts[5][2] = (fastf_t) ((-hgt) / 2.0);
	pts[6][0] = (fastf_t)(-dpt);
	pts[6][1] = (fastf_t) (rds - wid / 2.0);
	pts[6][2] = (fastf_t) ((-hgt) / 2.0);
	pts[7][0] = (fastf_t)(-dpt);
	pts[7][1] = (fastf_t) (rds - wid / 2.0);
	pts[7][2] = (fastf_t) (hgt / 2.0);
	solnam[5] = 97 + i;
	solnam[6] = '0';
	solnam[7] = '1';
	mk_arb8(fpw, solnam, &pts[0][X]);

	/* Create second arb8. */
	pts[0][1] = (fastf_t) (wid / 2.0);
	pts[0][2] = (fastf_t) (hgt / 2.0 - rds);
	pts[1][1] = (fastf_t) (wid / 2.0);
	pts[1][2] = (fastf_t) (rds - hgt / 2.0);
	pts[2][1] = (fastf_t) ((-wid) / 2.0);
	pts[2][2] = (fastf_t) (rds - hgt / 2.0);
	pts[3][1] = (fastf_t) ((-wid) / 2.0);
	pts[3][2] = (fastf_t) (hgt / 2.0 - rds);
	pts[4][1] = (fastf_t) (wid / 2.0);
	pts[4][2] = (fastf_t) (hgt / 2.0 - rds);
	pts[5][1] = (fastf_t) (wid / 2.0);
	pts[5][2] = (fastf_t) (rds - hgt / 2.0);
	pts[6][1] = (fastf_t) ((-wid) / 2.0);
	pts[6][2] = (fastf_t) (rds - hgt / 2.0);
	pts[7][1] = (fastf_t) ((-wid) / 2.0);
	pts[7][2] = (fastf_t) (hgt / 2.0 - rds);
	solnam[7] = '2';
	mk_arb8(fpw, solnam, &pts[0][X]);

	/* Create cylinder 1. */
	bs[0] = (fastf_t)0.0;
	bs[1] = (fastf_t) (wid / 2.0 - rds);
	bs[2] = (fastf_t) (hgt / 2.0 - rds);
	ht[0] = (fastf_t)(-dpt);
	ht[1] = (fastf_t)0.0;
	ht[2] = (fastf_t)0.0;
	rad = (fastf_t)rds;
	solnam[7] = '3';
	mk_rcc(fpw, solnam, bs, ht, rad);

	/* Create cylinder 2. */
	bs[2] = (-bs[2]);
	solnam[7] = '4';
	mk_rcc(fpw, solnam, bs, ht, rad);

	/* Create cylinder 3. */
	bs[1] = (-bs[1]);
	solnam[7] = '5';
	mk_rcc(fpw, solnam, bs, ht, rad);

	/* Create cylinder 4. */
	bs[2] = (-bs[2]);
	solnam[7] = '6';
	mk_rcc(fpw, solnam, bs, ht, rad);

	/* Create all inside solids. */
	/* Create arb8 3. */
	pts[0][0] = (fastf_t)0.0;
	pts[0][1] = (fastf_t) (wid / 2.0 - rds);
	pts[0][2] = (fastf_t) (hgt / 2.0 - isw);
	pts[1][0] = (fastf_t)0.0;
	pts[1][1] = (fastf_t) (wid / 2.0 - rds);
	pts[1][2] = (fastf_t) ((-hgt) / 2.0 + isw);
	pts[2][0] = (fastf_t)0.0;
	pts[2][1] = (fastf_t) (rds - wid / 2.0);
	pts[2][2] = (fastf_t) ((-hgt) / 2.0 + isw);
	pts[3][0] = (fastf_t)0.0;
	pts[3][1] = (fastf_t) (rds - wid / 2.0);
	pts[3][2] = (fastf_t) (hgt / 2.0 - isw);
	pts[4][0] = (fastf_t)(-dpt);
	pts[4][1] = (fastf_t) (wid / 2.0 - rds);
	pts[4][2] = (fastf_t) (hgt / 2.0 - isw);
	pts[5][0] = (fastf_t)(-dpt);
	pts[5][1] = (fastf_t) (wid / 2.0 - rds);
	pts[5][2] = (fastf_t) ((-hgt) / 2.0 + isw);
	pts[6][0] = (fastf_t)(-dpt);
	pts[6][1] = (fastf_t) (rds - wid / 2.0);
	pts[6][2] = (fastf_t) ((-hgt) / 2.0 + isw);
	pts[7][0] = (fastf_t)(-dpt);
	pts[7][1] = (fastf_t) (rds - wid / 2.0);
	pts[7][2] = (fastf_t) (hgt / 2.0 - isw);
	solnam[7] = '7';
	mk_arb8(fpw, solnam, &pts[0][X]);

	/* Create arb8 4. */
	pts[0][1] = (fastf_t) (wid / 2.0 - isw);
	pts[0][2] = (fastf_t) (hgt / 2.0 - rds);
	pts[1][1] = (fastf_t) (wid / 2.0 - isw);
	pts[1][2] = (fastf_t) (rds - hgt / 2.0);
	pts[2][1] = (fastf_t) ((-wid) / 2.0 + isw);
	pts[2][2] = (fastf_t) (rds - hgt / 2.0);
	pts[3][1] = (fastf_t) ((-wid) / 2.0 + isw);
	pts[3][2] = (fastf_t) (hgt / 2.0 - rds);
	pts[4][1] = (fastf_t) (wid / 2.0 - isw);
	pts[4][2] = (fastf_t) (hgt / 2.0 - rds);
	pts[5][1] = (fastf_t) (wid / 2.0 - isw);
	pts[5][2] = (fastf_t) (rds - hgt / 2.0);
	pts[6][1] = (fastf_t) ((-wid) / 2.0 + isw);
	pts[6][2] = (fastf_t) (rds - hgt / 2.0);
	pts[7][1] = (fastf_t) ((-wid) / 2.0 + isw);
	pts[7][2] = (fastf_t) (hgt / 2.0 - rds);
	solnam[7] = '8';
	mk_arb8(fpw, solnam, &pts[0][X]);

	/* Create cylinder 5. */
	bs[0] = (fastf_t)0.0;
	bs[1] = (fastf_t) (wid / 2.0 - rds);
	bs[2] = (fastf_t) (hgt / 2.0 - rds);
	ht[0] = (fastf_t)(-dpt);
	ht[1] = (fastf_t)0.0;
	ht[2] = (fastf_t)0.0;
	rad = (fastf_t) (rds - isw);
	solnam[7] = '9';
	mk_rcc(fpw, solnam, bs, ht, rad);

	/* Create cylinder 6. */
	bs[2] = (-bs[2]);
	solnam[6] = '1';
	solnam[7] = '0';
	mk_rcc(fpw, solnam, bs, ht, rad);

	/* Create cylinder 7. */
	bs[1] = (-bs[1]);
	solnam[7] = '1';
	mk_rcc(fpw, solnam, bs, ht, rad);

	/* Create cylinder 8. */
	bs[2] = (-bs[2]);
	solnam[7] = '2';
	mk_rcc(fpw, solnam, bs, ht, rad);

	/* Make all regions. */

	/* Initialize list. */
	BU_LIST_INIT(&comb.l);

	solnam[6] = '0';
	solnam[7] = '1';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	solnam[7] = '2';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	solnam[7] = '7';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	regnam[5] = 97 + i;
	regnam[6] = '1';
	mk_lfcomb(fpw, regnam, &comb, 1);

	solnam[7] = '2';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	solnam[7] = '8';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	regnam[6] = '2';
	mk_lfcomb(fpw, regnam, &comb, 1);

	solnam[7] = '3';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	solnam[7] = '1';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	solnam[7] = '2';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	solnam[7] = '9';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	regnam[6] = '3';
	mk_lfcomb(fpw, regnam, &comb, 1);

	solnam[7] = '4';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	solnam[7] = '1';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	solnam[7] = '2';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	solnam[6] = '1';
	solnam[7] = '0';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	regnam[6] = '4';
	mk_lfcomb(fpw, regnam, &comb, 1);

	solnam[6] = '0';
	solnam[7] = '5';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	solnam[7] = '1';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	solnam[7] = '2';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	solnam[6] = '1';
	solnam[7] = '1';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	regnam[6] = '5';
	mk_lfcomb(fpw, regnam, &comb, 1);

	solnam[6] = '0';
	solnam[7] = '6';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	solnam[7] = '1';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	solnam[7] = '2';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	solnam[6] = '1';
	solnam[7] = '2';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	regnam[6] = '6';
	mk_lfcomb(fpw, regnam, &comb, 1);

	/* Create group. */

	/* Initialize list. */
	BU_LIST_INIT(&comb1.l);

	regnam[6] = '1';
	(void)mk_addmember(regnam, &comb1.l, NULL, WMOP_UNION);
	regnam[6] = '2';
	(void)mk_addmember(regnam, &comb1.l, NULL, WMOP_UNION);
	regnam[6] = '3';
	(void)mk_addmember(regnam, &comb1.l, NULL, WMOP_UNION);
	regnam[6] = '4';
	(void)mk_addmember(regnam, &comb1.l, NULL, WMOP_UNION);
	regnam[6] = '5';
	(void)mk_addmember(regnam, &comb1.l, NULL, WMOP_UNION);
	regnam[6] = '6';
	(void)mk_addmember(regnam, &comb1.l, NULL, WMOP_UNION);

	grpnam[3] = 97 + i;
	mk_lfcomb(fpw, grpnam, &comb1, 0);
    }							/* START # 2 */

    /* Close file. */
    db_close(fpw->dbip);

    return 0;
}							/* END # 1 */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
