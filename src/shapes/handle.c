/*                        H A N D L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
/** @file shapes/handle.c
 *
 * Program to make a handle using libwdb.  The objects will be in
 * millimeters.  This handle will be constructed using three
 * cylinders, two tori, and two arb8s.  The base of the handle will be
 * centered at (0, 0, 0) and the height of the handle will extend in
 * the positive z-direction.
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
#include "raytrace.h"
#include "wdb.h"

void
printusage()
{
    printf("Usage: handle  <-- (if no arguments, go into interactive mode)\n");
    printf("or\n");
    printf("Usage: handle -f name.g -n number_of_handles -l handle_length -H handle_height\n");
    printf("       -r1 torus_radius -r2 cylinder_radius\n");
    printf("       (units mm)\n");
    printf("\nThis program constructs a handle with the base centered\n");
    printf("at (0, 0, 0) and the height extending in the positive z-\n");
    printf("direction.  The handle is to be composed of 3 cylinders, \n");
    printf("2 tori, and 2 arb8s.\n\n");
	/* List of options. */
	/* -fname - name = name of .g file. */
	/* -n# - # = number of handles. */
	/* -l# - # = length of handle in mm. */
	/* -H# - # = height of handle in mm. */
	/* -r1# - # = r1 radius of torus. */
	/* -r2# - # = r2 radius of cylinder. */
}

int
main(int argc, char **argv)
{
    /* START # 1 */
    struct rt_wdb *fpw;		/* File to be written to. */
#define NAME_LEN 256
    char filemged[NAME_LEN+1] = {0};	/* Mged file create. */
    double hgt = 0.0;		/* Height of handle. */
    double len = 0.0;		/* Length of handle. */
    double r1 = 0.0;		/* Radius of tori & radius of cylinders. */
    double r2 = 0.0;
    point_t pts[8];		/* Eight points of arb8. */
    point_t bs;			/* Base of rcc. */
    vect_t ht;			/* Height of rcc. */
    fastf_t rad;		/* Radius of rcc. */
    point_t cent;		/* Center of torus. */
    vect_t norm;		/* Normal of torus. */
    double rad1, rad2;		/* r1 and r2 of torus. */
    char *temp;			/* Temporary character string. */
    char temp1[NAME_LEN];	/* Temporary character string. */

    char solnam[8];		/* Solid name. */
    char regnam[8];		/* Region name. */
    char grpnam[5];		/* Group name. */
    int numhan = 0;		/* Number of handles to be created. */

    struct wmember comb;	/* Used to make regions. */
    struct wmember comb1;	/* Used to make groups. */

    int i, j, k;		/* Loop counters. */
    int ret;

    bu_setprogname(argv[0]);

    /* Set up solid, region, and group names. */
    solnam[0] = 's';
    solnam[1] = '.';
    solnam[2] = 'h';
    solnam[3] = 'a';
    solnam[4] = 'n';
    solnam[5] = ' ';
    solnam[6] = '#';
    solnam[7] = '\0';
    regnam[0] = 'r';
    regnam[1] = '.';
    regnam[2] = 'h';
    regnam[3] = 'a';
    regnam[4] = 'n';
    regnam[5] = ' ';
    regnam[6] = '#';
    regnam[7] = '\0';
    grpnam[0] = 'h';
    grpnam[1] = 'a';
    grpnam[2] = 'n';
    grpnam[3] = ' ';
    grpnam[4] = '\0';

    /* If there are no arguments ask questions. */
    if (argc == 1) {
	/* START # 3 */

	/* Explain makings of handle. */
    	printusage();
	(void)fflush(stdout);

	/* Find name of mged file to create. */
	printf("\nEnter the name of the mged file to be created ");
	printf("(%d char max).\n\t", NAME_LEN);
	(void)fflush(stdout);
	ret = scanf(CPP_SCAN(NAME_LEN), filemged);
	if (ret == 0)
	    perror("scanf");
	if (BU_STR_EQUAL(filemged, ""))
	    bu_strlcpy(filemged, "handle.g", sizeof(filemged));

	/* Find number of handles to create. */
	printf("Enter number of handles to create (%d max).\n\t", NAME_LEN);
	(void)fflush(stdout);
	ret = scanf("%d", &numhan);
	if (ret == 0) {
	    perror("scanf");
	    numhan = 1;
	}

	/* Find dimensions of handle. */
	printf("Enter the length and height of handle in mm.\n\t");
	(void)fflush(stdout);
	ret = scanf("%lf %lf", &len, &hgt);
	if (ret == 0) {
	    perror("scanf");
	    len = 100.0;
	    hgt = 10.0;
	} else {
	if (len < SMALL_FASTF)
	    len = SMALL_FASTF;
	if (hgt < SMALL_FASTF)
	    hgt = SMALL_FASTF;
	}

	printf("Enter the radius of the tori in mm.\n\t");
	(void)fflush(stdout);
	ret = scanf("%lf", &r1);
	if (ret == 0) {
	    perror("scanf");
	    r1 = 5.0;
	} else {
	if (r1 < SMALL_FASTF)
	    r1 = SMALL_FASTF;
	}

	printf("Enter the radius of the cylinders in mm.\n\t");
	(void)fflush(stdout);
	ret = scanf("%lf", &r2);
	if (ret == 0) {
	    perror("scanf");
	    r2 = 5.0;
	} else {
	if (r2 < SMALL_FASTF)
	    r2 = SMALL_FASTF;
	}
    }							/* END # 3 */

    /* if there are arguments get the answers from the arguments. */
    else {
	/* START # 4 */

	for (i=1; i<argc; i++) {
	    /* START # 5 */
	    /* Put argument into temporary character string. */
	    temp = argv[i];
	    if (temp[1] == 'h' || temp[1] == '?') {
	    	printusage();
		bu_exit(1, NULL);
	    }

	    /* -f - mged file name. */
	    if (temp[1] == 'f') {
		/* START # 6 */
		j = 2;
		k = 0;
		while ((temp[j] != '\0') && (k < NAME_LEN)) {
		    /* START # 7 */
		    filemged[k] = temp[j];
		    j++;
		    k++;
		}					/* END # 7 */
		filemged[k] = '\0';
	    }						/* END # 6 */

	    /* -n - # of handles to be created. */
	    else if (temp[1] == 'n') {
		/* START # 8 */
		/* Set up temporary character string. */
		j = 2;
		k = 0;
		while ((temp[j] != '\0') && (k < NAME_LEN)) {
		    /* START # 9 */
		    temp1[k] = temp[j];
		    j++;
		    k++;
		}					/* END # 9 */
		temp1[k] = '\0';
		sscanf(temp1, "%d", &numhan);
	    }						/* END # 8 */

	    /* -l or -h - length and height of handle in mm. */
	    else if ((temp[1] == 'l') || (temp[1] == 'H')) {
		/* START # 10 */
		/* Set up temporary character string. */
		j = 2;
		k = 0;
		while ((temp[j] != '\0') && (k < NAME_LEN)) {
		    /* START # 11 */
		    temp1[k] = temp[j];
		    j++;
		    k++;
		}					/* END # 11 */
		temp1[k] = '\0';
		if (temp[1] == 'l') sscanf(temp1, "%lf", &len);
		else if (temp[1] == 'H') sscanf(temp1, "%lf", &hgt);
	    }						/* END # 10 */

	    /* -r1 or -r2 - radii for torus. */
	    else if (temp[1] == 'r') {
		/* START # 12 */
		/* Set up temporary character string. */
		j = 3;
		k = 0;
		while ((temp[j] != '\0') && (k < NAME_LEN)) {
		    /* START # 13 */
		    temp1[k] = temp[j];
		    j++;
		    k++;
		}					/* END # 13 */
		temp1[k] = '\0';
		if (temp[2] == '1') sscanf(temp1, "%lf", &r1);
		else if (temp[2] == '2') sscanf(temp1, "%lf", &r2);
	    }						/* END # 12 */
	}						/* END # 5 */
    }							/* END # 4 */

    if (numhan < 1)
	numhan = 1;
    else if (numhan > NAME_LEN)
	numhan = NAME_LEN;

    /* Print out dimensions of the handle. */
    printf("\nmged file name:  %s\n", filemged);
    printf("length:  %f mm\n", len);
    printf("height:  %f mm\n", hgt);
    printf("radius of tori:  %f mm\n", r1);
    printf("radius of cylinders:  %f mm\n", r2);
    printf("number of handles:  %d\n\n", numhan);
    (void)fflush(stdout);

    /* Open mged file for writing to. */
    fpw = wdb_fopen(filemged);
    if (!fpw)
	bu_exit(1, "file-open failed");

    /* Write ident record. */
    mk_id(fpw, "handles");

    for (i=0; i<numhan; i++) {
	/* START # 2 */

	/* Create solids for handle. */

	/* Create top cylinder. */
	bs[0] = (fastf_t)0.0;
	bs[1] = (fastf_t) (len / 2.0 - r1 - r2);
	bs[2] = (fastf_t) (hgt - r2);
	ht[0] = (fastf_t)0.0;
	ht[1] = (fastf_t) (r1 + r1 + r2 + r2 - len);
	ht[2] = (fastf_t)0.0;
	rad = (fastf_t)r2;
	solnam[5] = 97 + i;
	solnam[6] = '1';
	mk_rcc(fpw, solnam, bs, ht, rad);

	/* Create right cylinder. */
	bs[0] = (fastf_t)0.0;
	bs[1] = (fastf_t) (len / 2.0 - r2);
	bs[2] = (fastf_t)0.0;
	ht[0] = (fastf_t)0.0;
	ht[1] = (fastf_t)0.0;
	ht[2] = (fastf_t) (hgt - r1 - r2);
	rad = (fastf_t)r2;
	solnam[6] = '2';
	mk_rcc(fpw, solnam, bs, ht, rad);

	/* Create left cylinder. */
	bs[1] = (-bs[1]);
	solnam[6] = '3';
	mk_rcc(fpw, solnam, bs, ht, rad);

	/* Create right torus. */
	cent[0] = (fastf_t)0.0;
	cent[1] = (fastf_t) (len / 2.0 -r1 - r2);
	cent[2] = (fastf_t) (hgt - r1 -r2);
	norm[0] = (fastf_t)1.0;
	norm[1] = (fastf_t)0.0;
	norm[2] = (fastf_t)0.0;
	rad1 = r1;
	rad2 = r2;
	solnam[6] = '4';
	mk_tor(fpw, solnam, cent, norm, rad1, rad2);

	/* Create left torus. */
	cent[1] = (-cent[1]);
	solnam[6] = '5';
	mk_tor(fpw, solnam, cent, norm, rad1, rad2);

	/* Create right arb8. */
	pts[0][0] = (fastf_t)r2;
	pts[0][1] = (fastf_t) (len / 2.0 - r1 - r2);
	pts[0][2] = (fastf_t)hgt;
	pts[1][0] = (fastf_t)r2;
	pts[1][1] = (fastf_t) (len / 2.0);
	pts[1][2] = (fastf_t)hgt;
	pts[2][0] = (fastf_t)r2;
	pts[2][1] = (fastf_t) (len / 2.0);
	pts[2][2] = (fastf_t) (hgt -r1 - r2);
	pts[3][0] = (fastf_t)r2;
	pts[3][1] = (fastf_t) (len / 2.0 - r1 - r2);
	pts[3][2] = (fastf_t) (hgt -r1 - r2);
	pts[4][0] = (fastf_t)(-r2);
	pts[4][1] = (fastf_t) (len / 2.0 - r1 - r2);
	pts[4][2] = (fastf_t)hgt;
	pts[5][0] = (fastf_t)(-r2);
	pts[5][1] = (fastf_t) (len / 2.0);
	pts[5][2] = (fastf_t)hgt;
	pts[6][0] = (fastf_t)(-r2);
	pts[6][1] = (fastf_t) (len / 2.0);
	pts[6][2] = (fastf_t) (hgt -r1 - r2);
	pts[7][0] = (fastf_t)(-r2);
	pts[7][1] = (fastf_t) (len / 2.0 - r1 - r2);
	pts[7][2] = (fastf_t) (hgt -r1 - r2);
	solnam[6] ='6';
	mk_arb8(fpw, solnam, &pts[0][X]);

	/* Create left arb8. */
	pts[0][1] = (-pts[0][1]);
	pts[1][1] = (-pts[1][1]);
	pts[2][1] = (-pts[2][1]);
	pts[3][1] = (-pts[3][1]);
	pts[4][1] = (-pts[4][1]);
	pts[5][1] = (-pts[5][1]);
	pts[6][1] = (-pts[6][1]);
	pts[7][1] = (-pts[7][1]);
	solnam[6] = '7';
	mk_arb8(fpw, solnam, &pts[0][X]);

	/* Create all regions. */

	/* Initialize list. */
	BU_LIST_INIT(&comb.l);

	solnam[6] = '1';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	regnam[5] = 97 + i;
	regnam[6] = '1';
	mk_lfcomb(fpw, regnam, &comb, 1);

	solnam[6] = '2';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	regnam[6] = '2';
	mk_lfcomb(fpw, regnam, &comb, 1);

	solnam[6] = '3';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	regnam[6] = '3';
	mk_lfcomb(fpw, regnam, &comb, 1);

	solnam[6] = '4';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	solnam[6] = '6';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	solnam[6] = '1';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	solnam[6] = '2';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	regnam[6] = '4';
	mk_lfcomb(fpw, regnam, &comb, 1);

	solnam[6] = '5';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	solnam[6] = '7';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_INTERSECT);
	solnam[6] = '1';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	solnam[6] = '3';
	(void)mk_addmember(solnam, &comb.l, NULL, WMOP_SUBTRACT);
	regnam[6] = '5';
	mk_lfcomb(fpw, regnam, &comb, 1);

	/* Create a group. */

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
	grpnam[3] = 97 + i;
	mk_lfcomb(fpw, grpnam, &comb1, 0);

    }							/* END # 2 */

    /* Close mged file. */
    wdb_close(fpw);
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
