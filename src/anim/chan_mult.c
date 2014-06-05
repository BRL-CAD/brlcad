/*                     C H A N _ M U L T . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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
 *
 */
/** @file chan_mult.c
 *
 * Multiply specified columns of data by a given factor.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bio.h"
#include "bu.h"

static void
printusage (void)
{
    fprintf(stderr, "Usage: chan_mult factor num_columns column [col ... ] < in.file > out.file\n");
    bu_exit(-1, NULL);
}

int
main(int argc, char **argv)
{
    double factor, temp;
    int i, j, doit, of, count, val, *col_list;

    if ( BU_STR_EQUAL(argv[1], "-h") || BU_STR_EQUAL(argv[1], "-?") )
	printusage();
    if (argc < 4)
	printusage();

    sscanf(*(argv+1), "%lf", &factor);
    sscanf(*(argv+2), "%d", &of);
    col_list = (int *) bu_calloc(argc-2, sizeof(int), "int array");
    for (i=3;i<argc;i++) {
	sscanf(*(argv+i), "%d", col_list+(i-3));
    }

    count = 0;
    while (!feof(stdin)) {
	val = scanf("%lf", &temp);
	if (val<1)
	    ;
	else {
	    doit = 0;
	    for (j=0;j<argc-3;j++) {
		if (col_list[j]==count)
		    doit = 1;
	    }
	    if (doit)
		printf("%.10g\t", temp*factor);
	    else
		printf("%.10g\t", temp);
	}
	if (count == (of-1))
	    printf("\n");
	count = (count+1)%of;
    }

    bu_free(col_list, "int array");
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
