/*                        H A M W I N . C
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
/** @file hamwin.c
 *
 * Apply a Hamming Window to the given samples.
 * Precomputes the window function.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"


static int _init_length = 0;	/* Internal: last initialized size */
static double *hamwintab = NULL;


/* Internal routine to initialize the hamming window table
 * of a given length.
 * Returns zero on failure.
 */
static int
init_hamwintab(int size)
{
    static int maxinitlen = 0;

    int i;
    double theta;

    if (size > maxinitlen) {
	if (hamwintab != NULL) {
	    bu_free(hamwintab, "free hamwintab");
	    maxinitlen = 0;
	}
	hamwintab = (double *)bu_malloc(size*sizeof(double), "alloc hamwintab");
	if (!hamwintab) {
	    fprintf(stderr, "coswin: couldn't malloc space for %d elements\n", size);
	    return 0;
	}
	maxinitlen = size;
    }

    /* Check for odd lengths? XXX */

    /*
     * Size is okay.  Set up tables.
     */
    for (i = 0; i < size; i++) {
	theta = M_2PI * i / (double)(size);
	hamwintab[ i ] = 0.54 - 0.46 * cos(theta);
    }

    /*
     * Mark size and return success.
     */
    _init_length = size;
    return 1;
}


void
hamwin(double *data, int length)
{
    int i;

    /* Check for window table initialization */
    if (length != _init_length) {
	if (init_hamwintab(length) == 0) {
	    /* Can't do requested size */
	    return;
	}
    }

    /* Do window - could use pointers here... */
    for (i = 0; i < length; i++) {
	data[i] *= hamwintab[i];
    }
}


/*
 * Complex Data Version.
 */
void
chamwin(bn_complex_t *data, int length)
{
    int i;

    /* Check for window table initialization */
    if (length != _init_length) {
	if (init_hamwintab(length) == 0) {
	    /* Can't do requested size */
	    return;
	}
    }

    /* Do window - could use pointers here... */
    for (i = 0; i < length; i++) {
	data[i].re *= hamwintab[i];
    }
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
