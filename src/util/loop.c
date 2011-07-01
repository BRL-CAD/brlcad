/*                          L O O P . C
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
/** @file util/loop.c
 *
 * Simple program to output integers or floats or chars between
 * "start" and "finish", inclusive.  Default is an increment of +1 if
 * start < finish or -1 if start > finish.  User may specify an
 * alternate increment.  Also, user may left-pad output integers with
 * zeros.  There is no attempt to prevent "infinite" loops.
 *
 * Authors -
 * John Grosh
 * Phil Dykstra
 * Michael John Muuss
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "bu.h"
#include "vmath.h"


#define INTEGER 0
#define REAL 1
#define CHAR 2


int
main(int argc, char *argv[])
{
    int status = INTEGER;

    int i;
    int start,  finish, incr;

    double d;
    double dstart, dfinish, dincr;

    char c;
    char cstart, cfinish;
    int cincr;

    if (argc < 3 || argc > 5) {
	bu_exit(9, "Usage:  loop [-c|-n] start finish [incr] \n -n is the default option\n");
    }

    /* Check if -c is present in comandline argument*/

    if (argv[1][0] == '-' && argv[1][1]) status = CHAR;

    /* determine if any arguments are real */
    for (i = 1; i < argc; i++) {
	double dval = atof(argv[i]);
	int ival = atoi(argv[i]);
	if (!ZERO(dval - (double)ival)) {
	    status = REAL;
	    break;
	}
    }

    if (status == REAL) {
	dstart  = atof(argv[1]);
	dfinish = atof(argv[2]);

	if (argc == 4)
	    dincr = atof(argv[3]);
	else {
	    if (dstart > dfinish)
		dincr = -1.0;
	    else
		dincr =  1.0;
	}

	if (dincr >= 0.0)
	    for (d = dstart; d <= dfinish; d += dincr)
		printf("%g\n", d);
	else
	    for (d = dstart; d >= dfinish; d += dincr)
		printf("%g\n", d);
    } else if (status == INTEGER) {
	/* print out integer output */
	char *cp;
	char fmt_string[50];

	int field_width = 0;

	int zeros      = 0;  /* leading zeros for output */
	int zeros_arg1 = 0;  /* leading zeros in arg[1]  */
	int zeros_arg2 = 0;  /* leading zeros in arg[2]  */
	int zeros_arg3 = 0;  /* leading zeros in arg[3]  */

	/* count leading leading zeros in argv[1] */
	for (cp = argv[1]; *cp == '0'; cp++)
	    zeros_arg1++;
	if (*cp == '\0')
	    zeros_arg1--;

	/* count leading leading zeros in argv[2] */
	for (cp = argv[2]; *cp == '0'; cp++)
	    zeros_arg2++;
	if (*cp == '\0')
	    zeros_arg2--;

	/* if argv[3] exists, count leading leading zeros */
	if (argc == 4) {
	    for (cp = argv[3]; *cp == '0'; cp++)
		zeros_arg3++;
	    if (*cp == '\0')
		zeros_arg3--;
	}

	/* determine field width and leading zeros*/
	if (zeros_arg1 >= zeros_arg2 && zeros_arg1 >= zeros_arg3) {
	    field_width = strlen(argv[1]);
	    zeros       = zeros_arg1;
	} else if (zeros_arg2 >= zeros_arg1 && zeros_arg2 >= zeros_arg3) {
	    field_width = strlen(argv[2]);
	    zeros       = zeros_arg2;
	} else {
	    field_width = strlen(argv[3]);
	    zeros       = zeros_arg3;
	}

	/* printf format string fmt_string */
	if (zeros > 0)
	    snprintf(fmt_string, sizeof(fmt_string), "%%0%dd\n", field_width);
	else
	    bu_strlcpy(fmt_string, "%d\n", sizeof(fmt_string));
	fmt_string[50-1] = '\0'; /* sanity */

	start  = atoi(argv[1]);
	finish = atoi(argv[2]);

	if (argc == 4)
	    incr = atoi(argv[3]);
	else {
	    if (start > finish)
		incr = -1;
	    else
		incr =  1;
	}

	if (incr >= 0)
	    for (i = start; i <= finish; i += incr)
		printf(fmt_string, i);
	else
	    for (i = start; i >= finish; i += incr)
		printf(fmt_string, i);
    } else {
	/* print out character output */
	cstart = argv[2][0];
	cfinish = argv[3][0];
	
	if (argc == 5) cincr = atoi(argv[4]);
	else {
	    if (cstart > cfinish)
		cincr = -1;
	    else
		cincr = 1;
	}
	if (cincr >= 0)
	    for (c=cstart; c <= cfinish; c += cincr)
		printf("%c\n", c);
	else
	    for (c=cstart; c >= cfinish; c +=cincr)
		printf("%c\n", c);
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
