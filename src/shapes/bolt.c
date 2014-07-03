/*                          B O L T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "wdb.h"

static void
printusage(void)
{
    fprintf(stderr,"Usage: bolt  <-- (if no arguments, go into interactive mode)\n");
    fprintf(stderr,"or\n");
    fprintf(stderr,"Usage: bolt -o# -f name.g -n# -hd# -hh# -wd# -wh# -sd# -sh#\n");
    fprintf(stderr,"       (units mm)\n");
}


int
main(int argc, char **argv)
{
    struct rt_wdb *fpw;		/* File to be written to. */
    char filemged[26] = {0};	/* Mged file create. */
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
    int i, j, k;		/* Loop counters. */
    char *temp;			/* Temporary character string. */
    char temp1[16];		/* Temporary character string. */

    char solnam[9];		/* Solid name. */
    char regnam[9];		/* Region name. */
    char grpnam[9];		/* Group name. */
    int numblt;			/* Number of bolts to be created (<=26). */

    struct wmember comb;	/* Used to make regions. */
    struct wmember comb1;	/* Used to make groups. */
    int ret;

    if (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-h") || BU_STR_EQUAL(argv[1], "-?")) {
	    printusage();
	    return 0;
	}
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
	printusage();
	fprintf(stderr,"\n       Program continues running:\n\n");
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
	printf("Enter name of mged file to be created (25 char max).\n\t");
	(void)fflush(stdout);
	ret = scanf("%26s", filemged);
	if (ret == 0) {
	    perror("scanf");
	}
	if (BU_STR_EQUAL(filemged, ""))
	    bu_strlcpy(filemged, "bolt.g", sizeof(filemged));

	/* Find the number of bolts to be created (<=26). */
	printf("Enter the number of bolts to be created (26 max).\n\t");
	(void)fflush(stdout);
	ret = scanf("%d", &numblt);
	if (ret == 0) {
	    perror("scanf");
	    numblt = 1;
	}
	else if (numblt < 1)
	    numblt = 1;
	else if (numblt > 26)
	    numblt = 26;

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
	/* START # 2 */

	/* List of options. */
	/*	-o# - # = 1 => bolt head */
	/*	          2 => head & washer */
	/*	          3 => head, washer, & stem */
	/*	          4 => head & stem */
	/*	-fname - name = name of .g file */
	/*  	-n# - # = number of bolts to create (<=26)  */
	/*	-hd# - # = head diameter */
	/*	-hh# - # = head height */
	/*	-wd# - # = washer diameter */
	/*	-wh# - # = washer height */
	/*	-sd# - # = stem diameter */
	/*	-sh# - # = stem height */

	for (i = 1; i < argc; i++) {
	    /* START # 3 */
	    /* Put argument into temporary character string. */
	    temp = argv[i];

	    if (temp[0] != '-') {
	    	printf("bolt: illegal option %s ; missing leading '-'\n", argv[i]);
	    	return 0;
	    }

	    /* -o - set type of bolt to make. */
	    if (temp[1] == 'o') {
		/* START # 4 */
		if (temp[2] == '1')
		    iopt = 1;
		else if (temp[2] == '2')
		    iopt = 2;
		else if (temp[2] == '3')
		    iopt = 3;
		else if (temp[2] == '4')
		    iopt = 4;
	    }						/* END # 4 */

	    /* -f - mged file name. */
	    else if (temp[1] == 'f') {
		/* START # 5 */
		j = 2;
		k = 0;
		while ((temp[j] != '\0') && (k < 25)) {
		    /* START # 6 */
		    filemged[k] = temp[j];
		    j++;
		    k++;
		}					/* END # 6 */
		filemged[k] = '\0';
	    }						/* END # 5 */

	    /* -n - number of bolts to be created. */
	    else if (temp[1] == 'n') {
		/* START # 6.05 */
		/* Set up temporary character string, temp1. */
		j = 2;
		k = 0;
		while ((temp[j] != '\0') && (k < 15)) {
		    temp1[k] = temp[j];
		    j++;
		    k++;
		}
		temp1[k] = '\0';
		sscanf(temp1, "%d", &numblt);
		if (numblt > 26) numblt = 26;
	    }						/* END # 6.05 */

	    /* Take care of all other arguments. */
	    else {
		/* START # 6.1 */
		/* Set temporary character string, temp1. */
		j = 3;
		k = 0;
		while ((temp[j] != '\0') && (k < 15)) {
		    temp1[k] = temp[j];
		    j++;
		    k++;
		}
		temp1[k] = '\0';

		/* -hd & -hh - head diameter & height. */
		if (temp[1] == 'h') {
		    /* START # 7 */
		    if (temp[2] == 'd') {
			/* Head diameter. */
			sscanf(temp1, "%lf", &hd);
		    } else if (temp[2] =='h') {
			/* Head height. */
			sscanf(temp1, "%lf", &hh);
		    }
		}					/* END # 7 */

		/* -wd & -wh - washer diameter & height. */
		else if (temp[1] == 'w') {
		    /* START # 8 */
		    if (temp[2] == 'd') {
			/* Washer diameter. */
			sscanf(temp1, "%lf", &wd);
		    } else if (temp[2] == 'h') {
			/* Washer height. */
			sscanf(temp1, "%lf", &wh);
		    }
		}					/* END # 8 */

		/* -sd & -sh - stem washer diameter & height. */
		else if (temp[1] == 's') {
		    /* START # 9 */
		    if (temp[2] == 'd') {
			/* Stem diameter. */
			sscanf(temp1, "%lf", &sd);
		    } else if (temp[2] == 'h') {
			/* Stem height. */
			sscanf(temp1, "%lf", &sh);
		    }
		}					/* END # 9 */
		else {
		    printf("bolt: illegal option -- %c\n", temp[1]);
		    printusage();
		    return 0;
		}
	    }						/* END # 6.1 */

	}						/* END # 3 */
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
    wdb_close(fpw);
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
