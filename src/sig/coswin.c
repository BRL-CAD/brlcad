/*                        C O S W I N . C
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
/** @file coswin.c
 *
 *  Do Cosine windowing effecting p percent of the samples in
 *  the buffer.  Precomputes the window function.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"

static int	_init_length = 0;	/* Internal: last initialized size */
static int	maxinitlen = 0;
static double	*coswintab = NULL;

/*
 *		I N I T _ C O S W I N T A B
 *
 * Internal routine to initialize the cosine window table for
 *  a given effect length (number of sample at each end effected).
 */
int
init_coswintab(int size)
{
    int	i;
    double	theta;

    if ( size > maxinitlen ) {
	if ( coswintab != NULL ) {
	    bu_free( coswintab, "coswintab" );
	    maxinitlen = 0;
	}
	coswintab = (double *)bu_malloc(size*sizeof(double), "coswintab");
	maxinitlen = size;
    }

    /* Check for odd lengths? XXX */

    /*
     * Size is okay.  Set up tables.
     */
    for ( i = 0; i < size; i++ ) {
	theta = M_PI * i / (double)(size);
	coswintab[ i ] = 0.5 - 0.5 * cos( theta );
    }

    /*
     * Mark size and return success.
     */
    _init_length = size;
    return 1;
}


void
coswin(double *data, int length, double percent)
{
    int	num, i;

    num = percent * length/2 + 0.5;

    /* Check for window table initialization */
    if ( num != _init_length ) {
	if ( init_coswintab( num ) == 0 ) {
	    /* Can't do requested size */
	    return;
	}
    }

    /* Do window - could use pointers here... */
    for ( i = 0; i < num; i++ ) {
	data[i] *= coswintab[i];
	data[length-i-1] *= coswintab[i];
    }

    bu_free(coswintab, "coswintab");
}

/*
 * Complex Data Version.
 */
void
ccoswin(bn_complex_t *data, int length, double percent)
{
    int	num, i;

    num = percent * length/2 + 0.5;

    /* Check for window table initialization */
    if ( num != _init_length ) {
	if ( init_coswintab( num ) == 0 ) {
	    /* Can't do requested size */
	    return;
	}
    }

    /* Do window - could use pointers here... */
    for ( i = 0; i < num; i++ ) {
	data[i].re *= coswintab[i];
	data[length-i-1].re *= coswintab[i];
    }

    bu_free(coswintab, "coswintab");
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
