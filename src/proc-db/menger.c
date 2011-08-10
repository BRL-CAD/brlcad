/*                        M E N G E R . C
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/** @file menger.c
 *
 * Creates a Menger sponge
 *
 */

#include "common.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "bu.h"
#include "bn.h"
#include "wdb.h"


/* for tracking which axes we subtract along */
typedef enum { XDIR = 1, YDIR = 2, ZDIR = 4 } axes;


static void
usage(char *av0)
{
    bu_log("\nUsage: %s [-xyz] [-o filename] [-p {i|o}...] [-r repeat]\n\n", (av0) ? av0 : "menger");
    bu_log("Options:\n");
    bu_log("\t-{x|y|z}\tspecifies which axis to apply patterns along (default \"-xyz\")\n");
    bu_log("\t-o filename\tspecifies the output file name (default \"menger.g\")\n");
    bu_log("\t-p {o|i}...\tspecifies the inside/outside subtraction pattern (default \"i\")\n");
    bu_log("\t-r repeat\tspecifies the number of times to repeat the pattern (default 1)\n\n");
}


static void
slice(struct rt_wdb *fp, point_t origin, fastf_t depth, fastf_t width, fastf_t height, axes xyz, int exterior, size_t level, const char *prefix, struct bu_list *comb)
{
    /* 3x3x3 => 27 cubes
     *
     * first quadrant coordinate system
     */
    vect_t cell[27];
    point_t minpt, maxpt;

    if (!fp || depth < SMALL_FASTF || width < SMALL_FASTF) {
	return;
    }

    if (level > 0) {
	struct bu_vls celfix = BU_VLS_INIT_ZERO;

	level--;
	width /= 3.0;
	depth /= 3.0;
	height /= 3.0;

	/* first level */

	/* 000 */ VMOVE(cell[0], origin);
	/* 100 */ VSET(cell[1], origin[X] + width, origin[Y], origin[Z]);
	/* 200 */ VSET(cell[2], origin[X] + width + width, origin[Y], origin[Z]);

	/* 010 */ VSET(cell[3], origin[X], origin[Y] + width, origin[Z]);
	/* 110 */ VSET(cell[4], origin[X] + width, origin[Y] + width, origin[Z]);
	/* 210 */ VSET(cell[5], origin[X] + width + width, origin[Y] + width, origin[Z]);

	/* 020 */ VSET(cell[6], origin[X], origin[Y] + width + width, origin[Z]);
	/* 120 */ VSET(cell[7], origin[X] + width, origin[Y] + width + width, origin[Z]);
	/* 220 */ VSET(cell[8], origin[X] + width + width, origin[Y] + width + width, origin[Z]);

	/* second level */

	/* 001 */ VSET(cell[9], origin[X], origin[Y], origin[Z] + width);
	/* 101 */ VSET(cell[10], origin[X] + width, origin[Y], origin[Z] + width);
	/* 201 */ VSET(cell[11], origin[X] + width + width, origin[Y], origin[Z] + width);

	/* 011 */ VSET(cell[12], origin[X], origin[Y] + width, origin[Z] + width);
	/* 111 */ VSET(cell[13], origin[X] + width, origin[Y] + width, origin[Z] + width);
	/* 211 */ VSET(cell[14], origin[X] + width + width, origin[Y] + width, origin[Z] + width);

	/* 021 */ VSET(cell[15], origin[X], origin[Y] + width + width, origin[Z] + width);
	/* 121 */ VSET(cell[16], origin[X] + width, origin[Y] + width + width, origin[Z] + width);
	/* 221 */ VSET(cell[17], origin[X] + width + width, origin[Y] + width + width, origin[Z] + width);

	/* third level */

	/* 002 */ VSET(cell[18], origin[X], origin[Y], origin[Z] + width + width);
	/* 102 */ VSET(cell[19], origin[X] + width, origin[Y], origin[Z] + width + width);
	/* 202 */ VSET(cell[20], origin[X] + width + width, origin[Y], origin[Z] + width + width);

	/* 012 */ VSET(cell[21], origin[X], origin[Y] + width, origin[Z] + width + width);
	/* 112 */ VSET(cell[22], origin[X] + width, origin[Y] + width, origin[Z] + width + width);
	/* 212 */ VSET(cell[23], origin[X] + width + width, origin[Y] + width, origin[Z] + width + width);

	/* 022 */ VSET(cell[24], origin[X], origin[Y] + width + width, origin[Z] + width + width);
	/* 122 */ VSET(cell[25], origin[X] + width, origin[Y] + width + width, origin[Z] + width + width);
	/* 222 */ VSET(cell[26], origin[X] + width + width, origin[Y] + width + width, origin[Z] + width + width);

	/* RECURSION */

	/* first level */

	bu_vls_printf(&celfix, "%s_000_", prefix);
	/* 000 */ slice(fp, cell[0], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	bu_vls_printf(&celfix, "%s_100_", prefix);
	/* 100 */ slice(fp, cell[1], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	bu_vls_printf(&celfix, "%s_200_", prefix);
	/* 200 */ slice(fp, cell[2], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);

	bu_vls_printf(&celfix, "%s_010_", prefix);
	/* 010 */ slice(fp, cell[3], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	if (!(xyz & ZDIR)) {
	    bu_vls_printf(&celfix, "%s_110_", prefix);
	    /* 110 */ slice(fp, cell[4], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	}
	bu_vls_printf(&celfix, "%s_210_", prefix);
	/* 210 */ slice(fp, cell[5], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);

	bu_vls_printf(&celfix, "%s_020_", prefix);
	/* 020 */ slice(fp, cell[6], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	bu_vls_printf(&celfix, "%s_120_", prefix);
	/* 120 */ slice(fp, cell[7], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	bu_vls_printf(&celfix, "%s_220_", prefix);
	/* 220 */ slice(fp, cell[8], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);

	/* second level */

	bu_vls_printf(&celfix, "%s_001_", prefix);
	/* 001 */ slice(fp, cell[9], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	if (!(xyz & YDIR)) {
	    bu_vls_printf(&celfix, "%s_101_", prefix);
	    /* 101 */ slice(fp, cell[10], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	}
	bu_vls_printf(&celfix, "%s_201_", prefix);
	/* 201 */ slice(fp, cell[11], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);

	if (!(xyz & XDIR)) {
	    bu_vls_printf(&celfix, "%s_011_", prefix);
	    /* 011 */ slice(fp, cell[12], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	}
	if (!(xyz & (XDIR | YDIR | ZDIR))) {
	    bu_vls_printf(&celfix, "%s_111_", prefix);
	    /* 111 */ slice(fp, cell[13], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	}
	if (!(xyz & XDIR)) {
	    bu_vls_printf(&celfix, "%s_211_", prefix);
	    /* 211 */ slice(fp, cell[14], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	}

	bu_vls_printf(&celfix, "%s_021_", prefix);
	/* 021 */ slice(fp, cell[15], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	if (!(xyz & YDIR)) {
	    bu_vls_printf(&celfix, "%s_121_", prefix);
	    /* 121 */ slice(fp, cell[16], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	}
	bu_vls_printf(&celfix, "%s_221_", prefix);
	/* 221 */ slice(fp, cell[17], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);

	/* third level */

	bu_vls_printf(&celfix, "%s_002_", prefix);
	/* 002 */ slice(fp, cell[18], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	bu_vls_printf(&celfix, "%s_102_", prefix);
	/* 102 */ slice(fp, cell[19], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	bu_vls_printf(&celfix, "%s_202_", prefix);
	/* 202 */ slice(fp, cell[20], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);

	bu_vls_printf(&celfix, "%s_012_", prefix);
	/* 012 */ slice(fp, cell[21], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	if (!(xyz & ZDIR)) {
	    bu_vls_printf(&celfix, "%s_112_", prefix);
	    /* 112 */ slice(fp, cell[22], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	}
	bu_vls_printf(&celfix, "%s_212_", prefix);
	/* 212 */ slice(fp, cell[23], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);

	bu_vls_printf(&celfix, "%s_022_", prefix);
	/* 022 */ slice(fp, cell[24], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	bu_vls_printf(&celfix, "%s_122_", prefix);
	/* 122 */ slice(fp, cell[25], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);
	bu_vls_printf(&celfix, "%s_222_", prefix);
	/* 222 */ slice(fp, cell[26], depth, width, height, xyz, exterior, level, bu_vls_addr(&celfix), comb);

	bu_vls_free(&celfix);

    } else {

	/* BASE CASE */

	struct bu_vls ext = BU_VLS_INIT_ZERO;
	struct bu_vls inX = BU_VLS_INIT_ZERO;
	struct bu_vls inY = BU_VLS_INIT_ZERO;
	struct bu_vls inZ = BU_VLS_INIT_ZERO;

	bu_vls_printf(&ext, "%sext.rpp", prefix);
	bu_vls_printf(&inX, "%sinX.rpp", prefix);
	bu_vls_printf(&inY, "%sinY.rpp", prefix);
	bu_vls_printf(&inZ, "%sinZ.rpp", prefix);

	/* make exterior */
	VMOVE(minpt, origin);
	VSET(maxpt, minpt[X] + width, minpt[Y] + depth, minpt[Z] + height);
	mk_rpp(fp, bu_vls_addr(&ext), minpt, maxpt);

	/* make interiors */
	if (xyz | XDIR) {
	    VSET(minpt, origin[X], origin[Y] + (depth / 3.0), origin[Z] + (height / 3.0));
	    VSET(maxpt, minpt[X] + width, minpt[Y] + (depth / 3.0), minpt[Z] + (height / 3.0));
	    mk_rpp(fp, bu_vls_addr(&inX), minpt, maxpt);
	}
	if (xyz | YDIR) {
	    VSET(minpt, origin[X] + (width / 3.0), origin[Y], origin[Z] + (height / 3.0));
	    VSET(maxpt, minpt[X] + (width / 3.0), minpt[Y] + depth, minpt[Z] + (height / 3.0));
	    mk_rpp(fp, bu_vls_addr(&inY), minpt, maxpt);
	}
	if (xyz | ZDIR) {
	    VSET(minpt, origin[X] + (width / 3.0), origin[Y] + (depth / 3.0), origin[Z]);
	    VSET(maxpt, minpt[X] + (width / 3.0), minpt[Y] + (depth / 3.0), minpt[Z] + height);
	    mk_rpp(fp, bu_vls_addr(&inZ), minpt, maxpt);
	}

	if (exterior) {
	    /* subtract exterior, union interior */
	    /* mk_addmember(bu_vls_addr(&ext), comb, NULL, WMOP_SUBTRACT); */
	    mk_addmember(bu_vls_addr(&inX), comb, NULL, WMOP_UNION);
	    mk_addmember(bu_vls_addr(&inY), comb, NULL, WMOP_UNION);
	    mk_addmember(bu_vls_addr(&inZ), comb, NULL, WMOP_UNION);
	} else {
	    /* union exterior, subtract interior */
	    mk_addmember(bu_vls_addr(&ext), comb, NULL, WMOP_UNION);
	    mk_addmember(bu_vls_addr(&inX), comb, NULL, WMOP_SUBTRACT);
	    mk_addmember(bu_vls_addr(&inY), comb, NULL, WMOP_SUBTRACT);
	    mk_addmember(bu_vls_addr(&inZ), comb, NULL, WMOP_SUBTRACT);
	}

	bu_vls_free(&ext);
	bu_vls_free(&inX);
	bu_vls_free(&inY);
	bu_vls_free(&inZ);
    }

    return;
}


static struct bu_vls *
mengerize(struct rt_wdb *fp, point_t origin, fastf_t extent, axes xyz, const char *pattern, size_t repeat)
{
    size_t i, j;
    struct bu_vls *comb = NULL;
    struct wmember **levels = NULL;
    struct wmember *final = NULL;
    struct bu_vls cut = BU_VLS_INIT_ZERO;

    if (!fp || !pattern || extent < SMALL_FASTF || xyz == 0) {
	return NULL; /* nothing to do */
    }

    /* initialize */
    levels = bu_calloc(repeat * strlen(pattern), sizeof(struct wmember *), "alloc uts array");
    BU_GETSTRUCT(final, wmember);
    BU_LIST_INIT(&(final->l));

    /* do the pattern */
    for (i = 0; i < repeat; i++) {
	for (j = 0; j < strlen(pattern); j++) {
	    const int slot = (i * strlen(pattern)) + j;

	    BU_GETSTRUCT(levels[slot], wmember);
	    BU_LIST_INIT(&(levels[slot]->l));

	    bu_log("%s %.2zu: %s%s%s width=%lf\n",
		   (pattern[j] != 'i') ? "EXTERIOR" : "INTERIOR",
		   slot,
		   (xyz | XDIR) ? "X" : "",
		   (xyz | YDIR) ? "Y" : "",
		   (xyz | ZDIR) ? "Z" : "",
		   extent / pow(3.0, slot));
	    bu_vls_trunc(&cut, 0);
	    bu_vls_printf(&cut, "box%zu_", slot);
	    slice(fp, origin, extent, extent, extent, xyz, (pattern[j] != 'i'), /* !!! previous/solid, */ slot, bu_vls_addr(&cut), &(levels[slot]->l));

	    /* group each xyz pattern together */
	    bu_vls_trunc(&cut, 0);
	    bu_vls_printf(&cut, "level%zu.c", slot);
	    mk_comb(fp, bu_vls_addr(&cut), &(levels[slot]->l), 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0);

	    /* stash this pattern for the final combination */
	    mk_addmember(bu_vls_addr(&cut), &(final->l), NULL, WMOP_UNION);

	    /* clean up */
	    bu_free(levels[slot], "free cut");
	    levels[slot] = NULL;
	}
    }

    /* make the final combination */
    BU_GETSTRUCT(comb, bu_vls);
    BU_VLS_INIT(comb);
    bu_vls_strcat(comb, "sponge.c");
    mk_comb(fp, bu_vls_addr(comb), &(final->l), 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0);

    /* clean up */
    bu_vls_free(&cut);
    bu_free(levels, "free sponge levels");
    bu_free(final, "free final");

    return comb;
}


int
main(int ac, char *av[])
{
    int c;
    size_t i;

    /* maximum extent in positive xyz directions (i.e., it's a cube) */
    static const fastf_t EXTENT = 1000000.0;

    static const char optstring[] = "hHo:O:p:P:r:R:xXyYzZ";
    const char *av0 = av[0];

    struct bu_vls filename = BU_VLS_INIT_ZERO;
    struct bu_vls pattern = BU_VLS_INIT_ZERO;
    size_t repeat = 0;
    axes xyz = 0;

    point_t origin = VINIT_ZERO;
    struct rt_wdb *fp = NULL;
    struct bu_vls *boxes = NULL;

    bu_optind = 1;
    bu_opterr = 0;

    /* set up defaults */
    bu_vls_strcpy(&filename, "menger.g");
    bu_vls_strcpy(&pattern, "i");
    repeat = 1;

    /* process command-line options */
    while ((c = bu_getopt(ac, av, optstring)) != -1) {
	switch (c) {
	    case 'x':
	    case 'X':
		xyz |= XDIR;
		break;
	    case 'y':
	    case 'Y':
		xyz |= YDIR;
		break;
	    case 'z':
	    case 'Z':
		xyz |= ZDIR;
		break;
	    case 'o':
	    case 'O':
		bu_vls_strcpy(&filename, bu_optarg);
		if (bu_file_exists(bu_vls_addr(&filename))) {
		    bu_exit(4, "ERROR: Output file [%V] already exists\n", &filename);
		}
		break;
	    case 'p':
	    case 'P': {
		/* scan for valid pattern characters */
		bu_vls_trunc(&pattern, 0);
		for (i=0; i < strlen(bu_optarg); i++) {
		    if (bu_optarg[i] == 'i'
			|| bu_optarg[i] == 'o')
		    {
			bu_vls_putc(&pattern, bu_optarg[i]);
		    } else {
			bu_exit(5, "ERROR: Invalid pattern character encountered\nExpecting combinations of 'o' and 'i' (e.g., \"oiio\")\n");
		    }
		}
		break;
	    }
	    case 'r':
	    case 'R': {
		long val = repeat;
		if (!bu_optarg) {
		    bu_exit(3, "ERROR: missing repeat count after -r option\n");
		}
		val = strtol(bu_optarg, NULL, 0);
		if (val <= 0) {
		    bu_exit(3, "ERROR: invalid repeat specification [%ld <= 0]\n", val);
		}
		repeat = (size_t)val;
		break;
	    }
	    case '?':
	    case 'h':
	    case 'H':
	    default:
		usage(av[0]);
		if (c == '?' && bu_optopt != '?' && tolower(bu_optopt) != 'h') {
		    char *opt = strchr(optstring, bu_optopt);
		    if (opt && opt[1] == ':') {
			bu_exit(2, "ERROR: Missing option argument [-%c ARG]\n", bu_optopt);
		    }
		    bu_exit(1, "ERROR: Unrecognized option [-%c]\n", bu_optopt);
		}
		bu_exit(0, NULL);
	}
    }
    av += bu_optind;
    ac -= bu_optind;

    for (i=0; i < (size_t)ac; i++) {
	bu_log("WARNING: Skipping unknown command-line value [%s]\n", av[i]);
    }

    /* no specified axis implies all three */
    if (xyz == 0) {
	xyz = XDIR | YDIR | ZDIR;
    }

    bu_log("--------------------------------\n");
    bu_log("---  Creating Menger Sponge  ---\n");
    bu_log("--------------------------------\n");

    bu_log("Subtracting along the [%s%s%s] ax%ss\n",
	   (xyz & XDIR) ? "x" : "",
	   (xyz & YDIR) ? "y" : "",
	   (xyz & ZDIR) ? "z" : "",
	   (xyz == XDIR || xyz == YDIR || xyz == ZDIR) ? "i" : "e");
    bu_log("Using subtraction pattern [%V]\n", &pattern);
    bu_log("Repeating the pattern %zu time%s\n", repeat, (repeat == 1) ? "" : "s");
    bu_log("Writing geometry to [%V]\n\n", &filename);

    fp = wdb_fopen(bu_vls_addr(&filename));
    if (!fp) {
	bu_exit(6, "ERROR: Unable to open %V\n", &filename);
    }
    (void)mk_id_units(fp, "Menger Sponge", "m");

    /* DO IT! */
    boxes = mengerize(fp, origin, EXTENT, xyz, bu_vls_addr(&pattern), repeat);
    if (!boxes) {
	bu_exit(7, "ERROR: Unable to create sponge\n");
    }

    /* make the top-level scene:
     *
     * menger
     *  \____ u ground.r
     *   \___ u light0.r
     *    \__ u light1.r
     *     \_ u sponge.r
     *           \_____ u sponge.c
     *                     \_________ u level1.c
     *                      \            \___ u box1_000.rpp
     *                       \            \__ u box1_100.rpp
     *                        \            \_ u ...
     *                         \_____ u level2.c
     *                          \        \___ u box2_000.rpp
     *                           \        \__ u box2_100.rpp
     *                            \        \_ u ...
     *                             \_ u ...
     */
    {
	point_t pos = VINIT_ZERO;
	point_t dir = VINIT_ZERO;
	unsigned char rgb[3];

	struct wmember *menger = NULL;
	struct wmember *ground = NULL;
	struct wmember *light0 = NULL;
	struct wmember *light1 = NULL;
	struct wmember *sponge = NULL;

	BU_GETSTRUCT(menger, wmember);
	BU_GETSTRUCT(ground, wmember);
	BU_GETSTRUCT(light0, wmember);
	BU_GETSTRUCT(light1, wmember);
	BU_GETSTRUCT(sponge, wmember);

	BU_LIST_INIT(&(menger->l));
	BU_LIST_INIT(&(ground->l));
	BU_LIST_INIT(&(light0->l));
	BU_LIST_INIT(&(light1->l));
	BU_LIST_INIT(&(sponge->l));

	/* make the sponge */
	mk_addmember(bu_vls_addr(boxes), &(sponge->l), NULL, WMOP_UNION);
	mk_comb(fp, "sponge.r", &(sponge->l),
		1,         /* region */
		"plastic", /* shader name */
		NULL,      /* shader args */
		NULL,      /* rgb color */
		1000,      /* region_id */
		0,         /* aircode */
		0,         /* material */
		100,       /* los */
		0,         /* inherit */
		0,         /* append */
		0          /* gift */
	    );

	/* make a couple light sources */
	rgb[RED] = 255; rgb[GRN] = 255; rgb[BLU] = 255;
	VSET(pos, 0.0, 0.0, EXTENT * 2.0);
	mk_sph(fp, "light0.sph", pos, EXTENT * 0.01);
	mk_addmember("light0.sph", &(light0->l), NULL, WMOP_UNION);
	mk_comb(fp, "light0.r", &(light0->l), 1, "light", NULL, rgb, 1000, 0, 0, 100, 0, 0, 0);

	/* red light centered inside the shape if first pattern is an
	 * inside cut, otherwise positioned on the opposite side.
	 */
	rgb[RED] = 255; rgb[GRN] = 0; rgb[BLU] = 0;
	if (bu_vls_addr(&pattern)[0] == 'i') {
	    VSET(pos, EXTENT / 2.0, EXTENT / 2.0, EXTENT / 2.0);
	} else {
	    VSET(pos, EXTENT * 3.0, EXTENT * 3.0, EXTENT * 3.0);
	}
	mk_sph(fp, "light1.sph", pos, EXTENT * 0.01);
	mk_addmember("light1.sph", &(light1->l), NULL, WMOP_UNION);
	mk_comb(fp, "light1.r", &(light1->l), 1, "light", NULL, rgb, 1000, 0, 0, 100, 0, 0, 0);

	/* make a checker ground plane
	 * TODO: stack checker
	 */
	VSET(dir, 0.0, 0.0, 1.0);
	mk_half(fp, "ground.half", dir, 0.0);
	mk_addmember("ground.half", &(ground->l), NULL, WMOP_UNION);
	mk_comb(fp, "ground.r", &(ground->l), 1, "plastic", NULL, rgb, 1000, 0, 0, 100, 0, 0, 0);

	/* put the scene together */
	mk_addmember("ground.r", &(menger->l), NULL, WMOP_UNION);
	mk_addmember("light0.r", &(menger->l), NULL, WMOP_UNION);
	mk_addmember("light1.r", &(menger->l), NULL, WMOP_UNION);
	mk_addmember("sponge.r", &(menger->l), NULL, WMOP_UNION);
	mk_comb(fp, "menger", &(menger->l), 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0);

	/* clean up after ourselves */
	wdb_close(fp);
	bu_free(menger, "free menger");
	bu_free(ground, "free ground");
	bu_free(light0, "free light0");
	bu_free(light1, "free light1");
	bu_free(sponge, "free sponge");
    }

    bu_vls_free(&filename);
    bu_vls_free(&pattern);
    bu_vls_free(boxes);

    bu_log("\n%s: Done\n", av0);

    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
