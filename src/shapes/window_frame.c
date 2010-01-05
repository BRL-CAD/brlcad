/*                  W I N D O W _ F R A M E . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2010 United States Government as represented by
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
/** @file window_frame.c
 *
 * Program to make a window frame using libwdb.  The objects will be
 * in millimeters.  The window frames are composed of four arb8s and *
 * eight cylinders.  The front of the window frame is centered at *
 * (0, 0, 0) and extends in the negative x-direction the depth of the
 * * window frame.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "db.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"


int
main(int argc, char **argv)
{
    /* START # 1  */
    struct rt_wdb *fpw;		/* File to be written to. */
    char filemged[26];		/* Mged file create. */
    double hgt, wid, dpt;	/* Height, width, & depth of outside window */
    /* frame. */
    double rds;			/* Radius of the corner of window frame. */
    double isw;			/* Width of frame itself. */
    point_t pts[8];		/* Eight points of arb8. */
    point_t bs;			/* Base of rcc. */
    vect_t ht;			/* Height of rcc. */
    fastf_t rad;		/* Radius of rcc. */
    char *temp;			/* Temporary character string. */
    char temp1[16];		/* Temporary character string. */

    char solnam[9];		/* Solid name. */
    char regnam[8];		/* Region name. */
    char grpnam[5];		/* Group name. */
    int numwin;			/* Number of windows to be created (<=26). */

    struct wmember comb;	/* Used to make regions. */
    struct wmember comb1;	/* Used to make groups. */

    int i, j, k;		/* Loop counters. */

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
    if (argc == 1)
    {
	/* START # 3  */

	/* Print info about the window. */
	(void)printf("\nThe window frames are composed of 4 arb8s and 8\n");
	(void)printf("cylinders.  The front of the window frame is centered\n");
	(void)printf("at (0, 0, 0) and extends in the negative x-direction\n");
	(void)printf("the depth of the window frame.\n\n");

	/* Find name of mged file to be created. */
	(void)printf("Enter the mged file to be created (25 char max).\n\t");
	(void)fflush(stdout);
	(void)scanf("%26s", filemged);

	/* Find the number of window frames to create. */
	(void)printf("Enter the number of window frames to create (26 max).\n\t");
	(void)fflush(stdout);
	(void)scanf("%d", &numwin);

	/* Find the dimensions of the window frames. */
	(void)printf("Enter the height, width, and depth of the window frame.\n\t");
	(void)fflush(stdout);
	(void)scanf("%lf %lf %lf", &hgt, &wid, &dpt);
	(void)printf("Enter the radius of the corner.\n\t");
	(void)fflush(stdout);
	(void)scanf("%lf", &rds);
	(void)printf("Enter the actual width of the window frame.\n\t");
	(void)fflush(stdout);
	(void)scanf("%lf", &isw);

    }							/* END # 3  */

    /* If there are arguments get answers from arguments. */
    else
    {
	/* START # 4  */
	/* List options. */
	/*	-fname - name = mged file name. */
	/*	-n# - # = number of window frames. */
	/*	-h# - # = height of window frame in mm. */
	/*	-w# - # = width of window frame in mm. */
	/*	-d# - # = depth of window frame in mm. */
	/*	-r# - # = radius of window frame corner in mm. */
	/*	-i# - # = width of frame itself in mm. */

	for (i=1; i<argc; i++)
	{
	    /* START # 5  */
	    /* Put argument in temporary character string. */
	    temp = argv[i];

	    /*  -f - mged file. */
	    if (temp[1] == 'f')
	    {
		/* START # 6  */
		j = 2;
		k = 0;
		while ( (temp[j] != '\0') && (k < 25) )
		{
		    /* START # 7  */
		    filemged[k] = temp[j];
		    j++;
		    k++;
		}					/* END # 7  */
		filemged[k] = '\0';
	    }						/* END # 6  */

	    /* All other options. */
	    else
	    {
		/* START # 8  */
		/* Set up temporary character string. */
		j = 2;
		k = 0;
		while ( (temp[j] != '\0') && (k < 15) )
		{
		    /* START # 9  */
		    temp1[k] = temp[j];
		    j++;
		    k++;
		}					/* END # 9  */
		temp1[k] = '\0';
		if (temp[1] == 'n')
		{
		    (void)sscanf(temp1, "%d", &numwin);
		    if (numwin > 26) numwin = 26;
		}
		else if (temp[1] == 'h') (void)sscanf(temp1, "%lf", &hgt);
		else if (temp[1] == 'w') (void)sscanf(temp1, "%lf", &wid);
		else if (temp[1] == 'd') (void)sscanf(temp1, "%lf", &dpt);
		else if (temp[1] == 'r') (void)sscanf(temp1, "%lf", &rds);
		else if (temp[1] == 'i') (void)sscanf(temp1, "%lf", &isw);
	    }						/* END # 8  */
	}						/* END # 5  */
    }							/* END # 4  */

    /* Print out all info. */
    (void)printf("\nmged file:  %s\n", filemged);
    (void)printf("height of window frame:  %f mm\n", hgt);
    (void)printf("width of window frame:  %f mm\n", wid);
    (void)printf("depth of window frame:  %f mm\n", dpt);
    (void)printf("radius of corner:  %f mm\n", rds);
    (void)printf("width of frame:  %f mm\n", isw);
    (void)printf("number of window frames:  %d\n\n", numwin);
    (void)fflush(stdout);

    /* Open mged file. */
    fpw = wdb_fopen(filemged);

    /* Write ident record. */
    mk_id(fpw, "window frames");

    for (i=0; i<numwin; i++)
    {
	/* START # 2  */
	/* Create first arb8. */
	pts[0][0] = (fastf_t)0.;
	pts[0][1] = (fastf_t) (wid / 2. - rds);
	pts[0][2] = (fastf_t) (hgt / 2.);
	pts[1][0] = (fastf_t)0.;
	pts[1][1] = (fastf_t) (wid / 2. - rds);
	pts[1][2] = (fastf_t) ((-hgt) / 2.);
	pts[2][0] = (fastf_t)0.;
	pts[2][1] = (fastf_t) (rds - wid / 2.);
	pts[2][2] = (fastf_t) ((-hgt) / 2.);
	pts[3][0] = (fastf_t)0.;
	pts[3][1] = (fastf_t) (rds - wid / 2.);
	pts[3][2] = (fastf_t) (hgt / 2.);
	pts[4][0] = (fastf_t)(-dpt);
	pts[4][1] = (fastf_t) (wid / 2. - rds);
	pts[4][2] = (fastf_t) (hgt / 2.);
	pts[5][0] = (fastf_t)(-dpt);
	pts[5][1] = (fastf_t) (wid / 2. - rds);
	pts[5][2] = (fastf_t) ((-hgt) / 2.);
	pts[6][0] = (fastf_t)(-dpt);
	pts[6][1] = (fastf_t) (rds - wid / 2.);
	pts[6][2] = (fastf_t) ((-hgt) / 2.);
	pts[7][0] = (fastf_t)(-dpt);
	pts[7][1] = (fastf_t) (rds - wid / 2.);
	pts[7][2] = (fastf_t) (hgt / 2.);
	solnam[5] = 97 + i;
	solnam[6] = '0';
	solnam[7] = '1';
	mk_arb8(fpw, solnam, &pts[0][X]);

	/* Create second arb8. */
	pts[0][1] = (fastf_t) (wid / 2.);
	pts[0][2] = (fastf_t) (hgt / 2. - rds);
	pts[1][1] = (fastf_t) (wid / 2.);
	pts[1][2] = (fastf_t) (rds - hgt / 2.);
	pts[2][1] = (fastf_t) ((-wid) / 2.);
	pts[2][2] = (fastf_t) (rds - hgt / 2.);
	pts[3][1] = (fastf_t) ((-wid) / 2.);
	pts[3][2] = (fastf_t) (hgt / 2. - rds);
	pts[4][1] = (fastf_t) (wid / 2.);
	pts[4][2] = (fastf_t) (hgt / 2. - rds);
	pts[5][1] = (fastf_t) (wid / 2.);
	pts[5][2] = (fastf_t) (rds - hgt / 2.);
	pts[6][1] = (fastf_t) ((-wid) / 2.);
	pts[6][2] = (fastf_t) (rds - hgt / 2.);
	pts[7][1] = (fastf_t) ((-wid) / 2.);
	pts[7][2] = (fastf_t) (hgt / 2. - rds);
	solnam[7] = '2';
	mk_arb8(fpw, solnam, &pts[0][X]);

	/* Create cylinder 1. */
	bs[0] = (fastf_t)0.;
	bs[1] = (fastf_t) (wid / 2. - rds);
	bs[2] = (fastf_t) (hgt / 2. - rds);
	ht[0] = (fastf_t)(-dpt);
	ht[1] = (fastf_t)0.;
	ht[2] = (fastf_t)0.;
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
	pts[0][0] = (fastf_t)0.;
	pts[0][1] = (fastf_t) (wid / 2. - rds);
	pts[0][2] = (fastf_t) (hgt / 2. - isw);
	pts[1][0] = (fastf_t)0.;
	pts[1][1] = (fastf_t) (wid / 2. - rds);
	pts[1][2] = (fastf_t) ((-hgt) / 2. + isw);
	pts[2][0] = (fastf_t)0.;
	pts[2][1] = (fastf_t) (rds - wid / 2.);
	pts[2][2] = (fastf_t) ((-hgt) / 2. + isw);
	pts[3][0] = (fastf_t)0.;
	pts[3][1] = (fastf_t) (rds - wid / 2.);
	pts[3][2] = (fastf_t) (hgt / 2. - isw);
	pts[4][0] = (fastf_t)(-dpt);
	pts[4][1] = (fastf_t) (wid / 2. - rds);
	pts[4][2] = (fastf_t) (hgt / 2. - isw);
	pts[5][0] = (fastf_t)(-dpt);
	pts[5][1] = (fastf_t) (wid / 2. - rds);
	pts[5][2] = (fastf_t) ((-hgt) / 2. + isw);
	pts[6][0] = (fastf_t)(-dpt);
	pts[6][1] = (fastf_t) (rds - wid / 2.);
	pts[6][2] = (fastf_t) ((-hgt) / 2. + isw);
	pts[7][0] = (fastf_t)(-dpt);
	pts[7][1] = (fastf_t) (rds - wid / 2.);
	pts[7][2] = (fastf_t) (hgt / 2. - isw);
	solnam[7] = '7';
	mk_arb8(fpw, solnam, &pts[0][X]);

	/* Create arb8 4. */
	pts[0][1] = (fastf_t) (wid / 2. - isw);
	pts[0][2] = (fastf_t) (hgt / 2. - rds);
	pts[1][1] = (fastf_t) (wid / 2. - isw);
	pts[1][2] = (fastf_t) (rds - hgt / 2.);
	pts[2][1] = (fastf_t) ((-wid) / 2. + isw);
	pts[2][2] = (fastf_t) (rds - hgt / 2.);
	pts[3][1] = (fastf_t) ((-wid) / 2. + isw);
	pts[3][2] = (fastf_t) (hgt / 2. - rds);
	pts[4][1] = (fastf_t) (wid / 2. - isw);
	pts[4][2] = (fastf_t) (hgt / 2. - rds);
	pts[5][1] = (fastf_t) (wid / 2. - isw);
	pts[5][2] = (fastf_t) (rds - hgt / 2.);
	pts[6][1] = (fastf_t) ((-wid) / 2. + isw);
	pts[6][2] = (fastf_t) (rds - hgt / 2.);
	pts[7][1] = (fastf_t) ((-wid) / 2. + isw);
	pts[7][2] = (fastf_t) (hgt / 2. - rds);
	solnam[7] = '8';
	mk_arb8(fpw, solnam, &pts[0][X]);

	/* Create cylinder 5. */
	bs[0] = (fastf_t)0.;
	bs[1] = (fastf_t) (wid / 2. - rds);
	bs[2] = (fastf_t) (hgt / 2. - rds);
	ht[0] = (fastf_t)(-dpt);
	ht[1] = (fastf_t)0.;
	ht[2] = (fastf_t)0.;
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
    }							/* START # 2  */

    /* Close file. */
    wdb_close(fpw);
    return 0;
}							/* END # 1  */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
