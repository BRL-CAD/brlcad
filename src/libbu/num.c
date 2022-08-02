/*                           N U M . C
 * BRL-CAD
 *
 * Copyright (c) 2019-2022 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <stdio.h>
#include <math.h>


static size_t
char_length(int64_t num)
{
    uint64_t pos = labs((long)num);

    return
	((pos < 10L) ? 1 :
	 ((pos < 100L) ? 2 :
	  ((pos < 1000L) ? 3 :
	   ((pos < 10000L) ? 4 :
	    ((pos < 100000L) ? 5 :
	     ((pos < 1000000L) ? 6 :
	      ((pos < 10000000L) ? 7 :
	       ((pos < 100000000L) ? 8 :
		((pos < 1000000000L) ? 9 :
		 ((pos < 10000000000L) ? 10 :
		  ((pos < 100000000000L) ? 11 :
		   ((pos < 1000000000000L) ? 12 :
		    ((pos < 10000000000000L) ? 13 :
		     ((pos < 100000000000000L) ? 14 :
		      ((pos < 1000000000000000L) ? 15 :
		       ((pos < 10000000000000000L) ? 16 :
			((pos < 100000000000000000L) ? 17 :
			 ((pos < 1000000000000000000L) ? 18 :
			  19)))))))))))))))))) +
	((num < 0) ? 1 : 0);
}


void
bu_num_print(const double *vals, size_t nvals, size_t cols, const char *tbl_start, const char *row_start, const char *num_fmt, const char *col_sep, const char *row_end, const char *tbl_end)
{
    size_t i;
    size_t j;
#define MAXCOLS 64
    size_t preallocated_colsizes[MAXCOLS] = {0};
    size_t preallocated_colchars[MAXCOLS] = {0};
    size_t *colsizes = preallocated_colsizes;
    size_t *colchars = preallocated_colchars;
    size_t padding = 0;

    if (!vals || nvals == 0 || cols == 0)
	return;

    if (!col_sep)
	col_sep = " ";

    if (cols > MAXCOLS) {
	/* resort to dynamic memory if necessary */
	colsizes = (size_t *)calloc(cols, sizeof(size_t));
	colchars = (size_t *)calloc(cols, sizeof(size_t));
    }

    if (tbl_start) {
	printf("%s", tbl_start);
	while (tbl_start[strlen(tbl_start)-(padding+1)] != '\n' && padding != strlen(tbl_start)) {
	    padding++;
	}
    }

    if ((row_start && strchr(row_start, '\n')) || (row_end && strchr(row_end, '\n'))) {
	/* if spanning lines, get column sizes for pretty-printing */
	i = 0;
	do {
	    char num[3 + DBL_MANT_DIG - DBL_MIN_EXP + 1]; /* max double digits plus nul */
	    size_t numlen;
	    size_t charlen;

	    if (num_fmt) {
		snprintf(num, sizeof(num), num_fmt, vals[i]);
	    } else {
		snprintf(num, sizeof(num), "%.17g", vals[i]);
	    }
	    numlen = strlen(num);
	    charlen = char_length(lround(floor(vals[i])));

	    if (colsizes[i % cols] < numlen)
		colsizes[i % cols] = numlen;
	    if (colchars[i % cols] < charlen)
		colchars[i % cols] = charlen;

	    i++;
	} while (i < nvals);
    }

    /* print our values */
    i = 0;
    do {
	char ifmt[128] = "%%%d.17g";
	char ofmt[128] = {0};

	if ((i % cols == 0) && row_start) {
	    if (i > 0) {
		for (j = 0; j < padding; j++) {
		    printf(" ");
		}
	    }
	    printf("%s", row_start);
	}

	/* if we run out of values, pad with negative zeros */
	if (num_fmt) {
	    printf(num_fmt, (i < nvals) ? vals[i] : -0.0);
	} else {
	    snprintf(ofmt, sizeof(ofmt), ifmt, colsizes[i % cols]);
	    printf(ofmt, (i < nvals) ? vals[i] : -0.0);
	}

	i++;

	if (i < nvals || i % cols != 0) {
	    if ((i % cols == 0) && row_end) {
		printf("%s", row_end);
	    } else if (col_sep) {
		printf("%s", col_sep);
	    }
	}
    } while (i < nvals || i % cols != 0);

    /* finish the row if partial */
    while (i % cols != 0) {
	printf("X");

	i++;

	if (i < nvals) {
	    if ((i % cols == 0) && row_end) {
		printf("%s", row_end);
	    } else if (col_sep) {
		printf("%s", col_sep);
	    }
	}
    }

    if (tbl_end)
	printf("%s", tbl_end);

    /* release if we had to resort to dynamic memory */
    if (colsizes != preallocated_colsizes)
	free(colsizes);
    if (colchars != preallocated_colchars)
	free(colchars);

    return;
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
