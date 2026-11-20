/*                          B O L T . C
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
/** @file shapes/bolt.c
 *
 * Program to make a bolt using libwdb.  The objects will be in mm.
 *
 * Introduced in BRL-CAD release 4.4.  Original notes:
 * This program creates an mged data base of bolts.  There are four types of
 * bolts that can be created but only one type can be in the data base at a
 * time.  The four types of bolts that may be created are listed below:
 *
 *      bolt head
 *      bolt head and washer
 *      bolt head, washer, and bolt stem
 *      bolt head and bolt stem
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/arg.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "rt/db4.h"
#include "raytrace.h"
#include "wdb.h"

struct bolt_args {
    int help;
    int type;
    const char *output_file;
    int count;
    double head_diameter;
    double head_height;
    double washer_diameter;
    double washer_height;
    double stem_diameter;
    double stem_height;
};

static const struct bu_arg_option bolt_options[] = {
    BU_ARG_FLAG("h", "help", struct bolt_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_INTEGER_RANGE("o", "type", struct bolt_args, type,
	1, 1, 1, 4, "1|2|3|4", "Select the bolt component combination"),
    BU_ARG_VALUE("f", "output", struct bolt_args, output_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "database.g",
	"Write generated geometry to this database"),
    BU_ARG_INTEGER_RANGE("n", "count", struct bolt_args, count,
	1, 1, 1, 256, "count", "Create this many bolts"),
    BU_ARG_VALUE_VALIDATE(NULL, "head-diameter", struct bolt_args,
	head_diameter, BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate,
	"millimeters", "Set the positive bolt head diameter"),
    BU_ARG_VALUE_VALIDATE(NULL, "head-height", struct bolt_args,
	head_height, BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate,
	"millimeters", "Set the positive bolt head height"),
    BU_ARG_VALUE_VALIDATE(NULL, "washer-diameter", struct bolt_args,
	washer_diameter, BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate,
	"millimeters", "Set the positive washer diameter"),
    BU_ARG_VALUE_VALIDATE(NULL, "washer-height", struct bolt_args,
	washer_height, BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate,
	"millimeters", "Set the positive washer height"),
    BU_ARG_VALUE_VALIDATE(NULL, "stem-diameter", struct bolt_args,
	stem_diameter, BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate,
	"millimeters", "Set the positive stem diameter"),
    BU_ARG_VALUE_VALIDATE(NULL, "stem-height", struct bolt_args,
	stem_height, BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate,
	"millimeters", "Set the positive stem height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema bolt_schema = BU_ARG_SCHEMA(
    "bolt", "Create a BRL-CAD database of bolt shapes",
    bolt_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar bolt_grammar = {
    &bolt_schema, NULL, NULL
};

struct bolt_legacy_option {
    const char *prefix;
    const char *canonical;
};

static const struct bolt_legacy_option bolt_legacy_options[] = {
    {"-hd", "head-diameter"},
    {"-hh", "head-height"},
    {"-wd", "washer-diameter"},
    {"-wh", "washer-height"},
    {"-sd", "stem-diameter"},
    {"-sh", "stem-height"},
    {NULL, NULL}
};


static const char **
normalize_bolt_args(int argc, char **argv)
{
    const char **args;
    int i;

    args = (const char **)bu_calloc((size_t)(argc - 1), sizeof(char *),
	"normalized bolt arguments");
    for (i = 1; i < argc; i++) {
	size_t option_index;

	args[i - 1] = argv[i];
	for (option_index = 0; bolt_legacy_options[option_index].prefix;
	     option_index++) {
	    const struct bolt_legacy_option *option =
		&bolt_legacy_options[option_index];
	    struct bu_vls normalized = BU_VLS_INIT_ZERO;

	    if (bu_strncmp(argv[i], option->prefix, 3) != 0)
		continue;
	    bu_vls_printf(&normalized, "--%s=%s", option->canonical,
		argv[i] + 3);
	    args[i - 1] = bu_strdup(bu_vls_cstr(&normalized));
	    bu_vls_free(&normalized);
	    break;
	}
    }

    return args;
}


static void
free_normalized_bolt_args(int argc, char **argv, const char **args)
{
    int i;

    for (i = 1; i < argc; i++) {
	if (args[i - 1] != argv[i])
	    bu_free((void *)args[i - 1], "normalized bolt argument");
    }
    bu_free((void *)args, "normalized bolt arguments");
}


int
main(int argc, char **argv)
{
    struct rt_wdb *fpw;		/* File to be written to. */
#define NAME_LEN 256
    char filemged[NAME_LEN+1] = {0};	/* Mged file create. */
    double hd, hh;		/* Diameter & height of bolt head. */
    double wd, wh;		/* Diameter & height of washer. */
    double sd, sh;		/* Diameter & height of bolt stem. */
    double leg, hyp;		/* Length of leg & hypotenuse of triangle. */
    point_t pts[8];		/* Eight points of arb8. */
    point_t bs;			/* Base of rcc. */
    vect_t ht;			/* Height of rcc. */
    fastf_t rad;		/* Radius of rcc. */
    int iopt;			/* Type of bolt to be built: 1=>bolt head,  */
				/* 2=>head & washer; 3=>head, washer, &  */
				/* stem; 4=>head & stem. */
    int i;			/* Loop counter. */

    char solnam[9];		/* Solid name. */
    char regnam[9];		/* Region name. */
    char grpnam[9];		/* Group name. */
    int numblt = 0;		/* Number of bolts to be created. */

    struct wmember comb;	/* Used to make regions. */
    struct wmember comb1;	/* Used to make groups. */
    int ret;
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    ret = bu_arg_introspect(&msg, &bolt_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (ret != 0) {
	bu_flog(ret > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return ret > 0 ? 0 : 1;
    }

    /* Zero all dimensions of bolt. */
    iopt = 0;
    hd = 0;
    hh = 0;
    wd = 0;
    wh = 0;
    sd = 0;
    sh = 0;

    /* Set up solid, region, and group names. */
    solnam[0] = 's';
    solnam[1] = '.';
    solnam[2] = 'b';
    solnam[3] = 'o';
    solnam[4] = 'l';
    solnam[5] = 't';
    solnam[6] = ' ';
    solnam[7] = '#';
    solnam[8] = '\0';
    regnam[0] = 'r';
    regnam[1] = '.';
    regnam[2] = 'b';
    regnam[3] = 'o';
    regnam[4] = 'l';
    regnam[5] = 't';
    regnam[6] = ' ';
    regnam[7] = '#';
    regnam[8] = '\0';
    grpnam[0] = 'b';
    grpnam[1] = 'o';
    grpnam[2] = 'l';
    grpnam[3] = 't';
    grpnam[4] = ' ';
    grpnam[5] = '\0';

    /* If there are no arguments ask questions. */
    if (argc == 1) {
	/* START # 1 */

	/* Find type of bolt to build. */
	printf("Enter option:\n");
	printf("\t1 - bolt head\n");
	printf("\t2 - bolt head & washer\n");
	printf("\t3 - bolt head, washer, & stem\n");
	printf("\t4 - bolt head & stem\n");
	(void)fflush(stdout);
	ret = scanf("%d", &iopt);
	if (ret == 0) {
	    perror("scanf");
	    iopt = 3;
	}
	else if (iopt < 0)
	    iopt = 0;
	else if (iopt > 4)
	    iopt = 4;

	/* Get file name of mged file to be created. */
	printf("Enter name of mged file to be created (%d char max).\n\t", NAME_LEN);
	(void)fflush(stdout);
	ret = scanf(CPP_SCAN(NAME_LEN), filemged);
	if (ret == 0) {
	    perror("scanf");
	}
	if (BU_STR_EQUAL(filemged, ""))
	    bu_strlcpy(filemged, "bolt.g", sizeof(filemged));

	/* Find the number of bolts to be created. */
	printf("Enter the number of bolts to be created (%d max).\n\t", NAME_LEN);
	(void)fflush(stdout);
	ret = scanf("%d", &numblt);
	if (ret == 0) {
	    perror("scanf");
	    numblt = 1;
	}
	else if (numblt < 1)
	    numblt = 1;
	else if (numblt > NAME_LEN)
	    numblt = NAME_LEN;

	/* Find dimensions of the bolt. */
	/* Find dimensions of head first. */
	printf("Enter diameter (flat edge to flat edge) & height of ");
	printf("bolt head.\n\t");
	(void)fflush(stdout);
	ret = scanf("%lf %lf", &hd, &hh);
	if (ret == 0) {
	    perror("scanf");
	    hd = 20.0;
	    hh = 20.0;
	}
	if (hd < SMALL_FASTF)
	    hd = SMALL_FASTF;
	if (hh < SMALL_FASTF)
	    hh = SMALL_FASTF;

	/* Find dimensions of washer if necessary. */
	if ((iopt == 2) || (iopt == 3)) {
	    printf("Enter diameter & height of washer.\n\t");
	    (void)fflush(stdout);
	    ret = scanf("%lf %lf", &wd, &wh);
	    if (ret == 0) {
		perror("scanf");
		wd = 30.0;
		wh = 2.0;
	    }
	    if (wd < SMALL_FASTF)
		wd = SMALL_FASTF;
	    if (wh < SMALL_FASTF)
		wh = SMALL_FASTF;
	}
	/* Find dimensions of bolt stem if necessary. */
	if ((iopt == 3) || (iopt == 4)) {
	    printf("Enter diameter & height of bolt stem.\n\t");
	    (void)fflush(stdout);
	    ret = scanf("%lf %lf", &sd, &sh);
	    if (ret == 0) {
		perror("scanf");
		sd = 10.0;
		sh = 100.0;
	    }
	    if (sd < SMALL_FASTF)
		sd = SMALL_FASTF;
	    if (sh < SMALL_FASTF)
		sh = SMALL_FASTF;
	}

    }							/* END # 1 */

    /* If there are arguments do not ask any questions.  Get the */
    /* answers from the arguments. */
    else {
	struct bolt_args args = {0};
	const char **normalized_args = normalize_bolt_args(argc, argv);

	ret = bu_arg_schema_parse_complete(&bolt_schema, &args, &msg,
	    argc - 1, normalized_args);
	free_normalized_bolt_args(argc, argv, normalized_args);
	if (ret < 0)
	    bu_exit(1, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	if (args.help) {
	    char *help = bu_arg_schema_help(&bolt_schema, "bolt");
	    bu_flog(stdout, "%s", help ? help : "");
	    bu_free(help, "bolt help");
	    return 0;
	}
	if (args.output_file) {
	    if (strlen(args.output_file) > NAME_LEN)
		bu_exit(1, "Output filename exceeds %d characters\n", NAME_LEN);
	    bu_strlcpy(filemged, args.output_file, sizeof(filemged));
	}
	iopt = args.type;
	numblt = args.count;
	hd = args.head_diameter;
	hh = args.head_height;
	wd = args.washer_diameter;
	wh = args.washer_height;
	sd = args.stem_diameter;
	sh = args.stem_height;
    }							/* END # 2 */

    /* Print out bolt dimensions. */
    printf("\noption:  %d - ", iopt);
    if (iopt == 1) printf("bolt head\n");
    if (iopt == 2) printf("head & washer\n");
    if (iopt == 3) printf("head, washer, & stem\n");
    if (iopt == 4) printf("head & stem\n");
    printf(".g file:  %s\n", filemged);
    printf("head diameter:  %f, & height:  %f\n", hd, hh);
    printf("washer diameter:  %f, & height:  %f\n", wd, wh);
    printf("stem diameter:  %f, & height:  %f\n", sd, sh);
    printf("number of bolts:  %d\n\n", numblt);
    (void)fflush(stdout);

    /* Open mged file for writing to. */
    fpw = wdb_fopen(filemged);

    /* Write ident record. */
    mk_id(fpw, "bolts");

    for (i = 0; i < numblt; i++) {
	/* Loop for each bolt created. */
	/* START # 20 */

	/* Create all solids needed. */
	/* Create solids of bolt head. */
	leg = tan(M_PI / 6.0) * hd / 2.0;
	hyp = leg * leg + (hd / 2.0) * (hd / 2.0);
	hyp = sqrt(hyp);
	/* Bolt head is two solids, create first solid. */
	pts[0][0] = (fastf_t) ((-hd) / 2.0);
	pts[0][1] = (fastf_t)leg;
	pts[0][2] = (fastf_t)hh;
	pts[1][0] = (fastf_t)0.0;
	pts[1][1] = (fastf_t)hyp;
	pts[1][2] = (fastf_t)hh;
	pts[2][0] = (fastf_t)0.0;
	pts[2][1] = (fastf_t)(-hyp);
	pts[2][2] = (fastf_t)hh;
	pts[3][0] = (fastf_t) ((-hd) / 2.0);
	pts[3][1] = (fastf_t)(-leg);
	pts[3][2] = (fastf_t)hh;
	pts[4][0] = (fastf_t) ((-hd) / 2.0);
	pts[4][1] = (fastf_t)leg;
	pts[4][2] = (fastf_t)0.0;
	pts[5][0] = (fastf_t)0.0;
	pts[5][1] = (fastf_t)hyp;
	pts[5][2] = (fastf_t)0.0;
	pts[6][0] = (fastf_t)0.0;
	pts[6][1] = (fastf_t)(-hyp);
	pts[6][2] = (fastf_t)0.0;
	pts[7][0] = (fastf_t) ((-hd) / 2.0);
	pts[7][1] = (fastf_t)(-leg);
	pts[7][2] = (fastf_t)0.0;
	solnam[6] = 97 + i;
	solnam[7] = '1';
	mk_arb8(fpw, solnam, &pts[0][X]);

	/* Create second solid. */
	pts[0][0] = (fastf_t) (hd / 2.0);
	pts[0][1] = (fastf_t)leg;
	pts[0][2] = (fastf_t)hh;
	pts[1][0] = (fastf_t)0.0;
	pts[1][1] = (fastf_t)hyp;
	pts[1][2] = (fastf_t)hh;
	pts[2][0] = (fastf_t)0.0;
	pts[2][1] = (fastf_t)(-hyp);
	pts[2][2] = (fastf_t)hh;
	pts[3][0] = (fastf_t) (hd / 2.0);
	pts[3][1] = (fastf_t)(-leg);
	pts[3][2] = (fastf_t)hh;
	pts[4][0] = (fastf_t) (hd / 2.0);
	pts[4][1] = (fastf_t)leg;
	pts[4][2] = (fastf_t)0.0;
	pts[5][0] = (fastf_t)0.0;
	pts[5][1] = (fastf_t)hyp;
	pts[5][2] = (fastf_t)0.0;
	pts[6][0] = (fastf_t)0.0;
	pts[6][1] = (fastf_t)(-hyp);
	pts[6][2] = (fastf_t)0.0;
	pts[7][0] = (fastf_t) (hd / 2.0);
	pts[7][1] = (fastf_t)(-leg);
	pts[7][2] = (fastf_t)0.0;
	solnam[7] = '2';
	mk_arb8(fpw, solnam, &pts[0][X]);

	/* Create washer if necessary. */
	if ((iopt == 2) || (iopt == 3)) {
	    bs[0] = (fastf_t)0.0;
	    bs[1] = (fastf_t)0.0;
	    bs[2] = (fastf_t)0.0;
	    ht[0] = (fastf_t)0.0;
	    ht[1] = (fastf_t)0.0;
	    ht[2] = (fastf_t)(-wh);
	    rad = (fastf_t) (wd / 2.0);
	    solnam[7] = '3';
	    mk_rcc(fpw, solnam, bs, ht, rad);
	}

	/* Create bolt stem if necessary. */
	if ((iopt == 3) || (iopt == 4)) {
	    bs[0] = (fastf_t)0.0;
	    bs[1] = (fastf_t)0.0;
	    bs[2] = (fastf_t)0.0;
	    ht[0] = (fastf_t)0.0;
	    ht[1] = (fastf_t)0.0;
	    ht[2] = (fastf_t)(-sh);
	    rad = (fastf_t) (sd / 2.0);
	    solnam[7] = '4';
	    mk_rcc(fpw, solnam, bs, ht, rad);
	}

	/* Create all regions. */
	/* First must initialize list. */
	BU_LIST_INIT(&comb.l);

	/* Create region for first half of bolt head. */
	solnam[7] = '1';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	solnam[7] = '2';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	/* Subtract washer if it exists. */
	if ((iopt == 2) || (iopt == 3)) {
	    solnam[7] = '3';
	    (void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	}
	/* Subtract stem if it exists. */
	if ((iopt == 3) || (iopt == 4)) {
	    solnam[7] = '3';
	    (void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	}
	regnam[6] = 97 + i;
	regnam[7] = '1';
	mk_lfcomb(fpw, regnam, &comb, 1);

	/* Create region for second half of bolt head. */
	solnam[7] = '2';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	/* Subtract washer if it exists. */
	if ((iopt == 2) || (iopt == 3)) {
	    solnam[7] = '3';
	    (void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	}
	/* Subtract stem if it exists. */
	if ((iopt == 3) || (iopt == 4)) {
	    solnam[7] = '4';
	    (void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	}
	regnam[7] = '2';
	mk_lfcomb(fpw, regnam, &comb, 1);

	/* Create region for washer if it exists. */
	if ((iopt == 2) || (iopt == 3)) {
	    solnam[7] = '3';
	    (void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	    /* Subtract bolt stem if it exists. */
	    if (iopt == 3) {
		solnam[7] = '4';
		(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	    }
	    regnam[7] = '3';
	    mk_lfcomb(fpw, regnam, &comb, 1);
	}

	/* Create region for bolt stem if it exists. */
	if ((iopt == 3) || (iopt == 4)) {
	    solnam[7] = '4';
	    (void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	    regnam[7] = '4';
	    mk_lfcomb(fpw, regnam, &comb, 1);
	}

	/* Create a group containing all bolt regions. */
	/* First initialize the list. */
	BU_LIST_INIT(&comb1.l);
	/* Add both bolt head regions to the list. */
	regnam[7] = '1';
	(void)mk_addmember(regnam, &comb1.l, NULL, WMOP_UNION);
	regnam[7] = '2';
	(void)mk_addmember(regnam, &comb1.l, NULL, WMOP_UNION);
	/* Add washer region if necessary. */
	if ((iopt == 2) || (iopt == 3)) {
	    regnam[7] = '3';
	    (void)mk_addmember(regnam, &comb1.l, NULL, WMOP_UNION);
	}
	/* Add bolt stem region if necessary. */
	if ((iopt == 3) || (iopt == 4)) {
	    regnam[7] = '4';
	    (void)mk_addmember(regnam, &comb1.l, NULL, WMOP_UNION);
	}
	/* Actually create the group. */
	grpnam[4] = 97 + i;
	mk_lfcomb(fpw, grpnam, &comb1, 0);

    }							/* END # 20 */

    /* Close mged file. */
    db_close(fpw->dbip);
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
