/*                         P I C T X . C
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
/** @file pictx.c
 *
 * This is an X-windows program that will raytrace a BRL-CAD mged
 * model and/or display it using the PRISM temperature output file.
 *
 */

#include "common.h"

#include<stdio.h>
#include<string.h>
#include<math.h>
#include<stdlib.h>
#include<X11/Xlib.h>
#include<X11/Xutil.h>

#include "bu.h"


int
main(void)
{
    int ichoice;			/* Choice.  */
    const char irX[] = "ir-X";		/* Calls ir-X program.  */
    char showtherm[125];		/* Calls showtherm program.  */
    char gfile[16];		/*  .g file.  */
    char group[26];		/* Group names.  */
    int ngrp;			/* Number of groups.  */
    int i, j, k;			/* Loop counters.  */
    int matched;

    /* Find option.  */
    printf("This takes a BRL-CAD mged model with a PRISM\n");
    printf("temperature output file and raytrace and/or\n");
    printf("display it using X-Windows graphics.  Make\n");
    printf("your selection.\n");
    printf("\t0 - raytrace & store file\n");
    printf("\t1 - raytrace, store, & showtherm file\n");
    printf("\t2 - showtherm file\n");
    (void)fflush(stdout);

    matched = scanf("%d", &ichoice);
    while ((matched != 1) || ((ichoice != 0) && (ichoice != 1) && (ichoice != 2))) {
	printf("Your choice was not 0, 1, or 2, enter again!!\n");
	(void)fflush(stdout);
	matched = scanf("%d", &ichoice);
    }

    if ((ichoice == 0) || (ichoice == 1)) {
	/* Start setting showtherm variable.  */
	showtherm[0] = 's';
	showtherm[1] = 'h';
	showtherm[2] = 'o';
	showtherm[3] = 'w';
	showtherm[4] = 't';
	showtherm[5] = 'h';
	showtherm[6] = 'e';
	showtherm[7] = 'r';
	showtherm[8] = 'm';
	showtherm[9] = ' ';
	i = 10;
	/* Find name of .g file to be used.  */
	printf("Enter .g file to be raytraced (15 char max).\n\t");
	(void)fflush(stdout);

	matched = scanf("%15s", gfile);
	if (matched != 1) {
	    bu_exit(EXIT_FAILURE, "ERROR: input failure");
	}

	/* Find number of groups to be raytraced.  */
	printf("Enter the number of groups to be raytraced.\n\t");
	(void)fflush(stdout);

	matched = scanf("%d", &ngrp);
	if (matched != 1) {
	    bu_exit(EXIT_FAILURE, "ERROR: input failure");
	}

	/* Read each group & put it in the variable showtherm.  */
	j = 0;
	while ((gfile[j] != '\0') && (i < 123)) {
	    showtherm[i] = gfile[j];
	    i++;
	    j++;
	}
	for (j=0; j<ngrp; j++) {
	    printf("Enter group %d (25 char max).\n\t", j);
	    (void)fflush(stdout);

	    matched = scanf("%25s", group);
	    if (matched != 1) {
		bu_exit(EXIT_FAILURE, "ERROR: input failure");
	    }

	    showtherm[i] = ' ';
	    i++;
	    k = 0;
	    while ((group[k] != '\0') && (i < 123)) {
		showtherm[i] = group[k];
		i++;
		k++;
	    }
	}
	showtherm[i] = '\0';
	if (i >= 123) {
	    printf("There are too many characters for showtherm, \n");
	    printf("please revise pictx.\n");
	    (void)fflush(stdout);
	}

	/* Call the program showtherm with the appropriate options.  */
	/* This will raytrace a .g file & find the appropriate */
	/* temperature for each region.  */
	printf("\nThe program showtherm is now being run.\n\n");
	(void)fflush(stdout);
	if (system(showtherm) == -1) {
	    bu_exit(EXIT_FAILURE, "ERROR: attempt to call \"showtherm\" program failed.");
	}
    }

    if ((ichoice == 1) || (ichoice == 2)) {
	/* Call the program ir-X so that a file that has been raytraced */
	/* may be displayed using X-Windows.  */
	printf("\nThe program ir-X in now being run.  If option\n");
	printf("0 or 1 was used when the name of a file is asked\n");
	printf("for enter the name of the file that was just\n");
	printf("stored.\n\n");
	(void)fflush(stdout);
	if (system(irX) == -1) {
	    bu_exit(EXIT_FAILURE, "ERROR: attempt to call \"irX\" program failed.");
	}
    }

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
