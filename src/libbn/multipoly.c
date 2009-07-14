/*                         M U L T I P O L Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2009 United States Government as represented by
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
/** @addtogroup multipoly */
/** @{ */
/** @file libbn/multipoly.c
 *
 *	Library for dealing with bivariate polynomials.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"

#define Abs( a )		((a) >= 0 ? (a) : -(a))
#define Max( a, b )		((a) > (b) ? (a) : (b))

#ifndef M_PI
#  define M_PI	3.14159265358979323846
#endif
#define PI_DIV_3	(M_PI/3.0)

#define SQRT3			1.732050808
#define THIRD			0.333333333333333333333333333
#define INV_TWENTYSEVEN		0.037037037037037037037037037
#define	CUBEROOT( a )	(( (a) >= 0.0 ) ? pow( a, THIRD ) : -pow( -(a), THIRD ))

/**
 *        bn_multipoly_set
 *
 * @brief set a coefficient growing cf array if needed
 */
struct bn_multipoly *
bn_multipoly_set(register struct bn_multipoly *multipoly, int xi, int yi, double val)
{
    double **oldcf = multipoly->cf;
    int i,j;
    if (xi < multipoly->dgrx && yi < multipoly->dgry) {
	(multipoly->cf)[xi][yi] = val;
	return multipoly;
    } else if (xi < multipoly->dgrx) {
	multipoly->cf = bu_malloc(sizeof(double) * multipoly->dgrx * yi, "bu_malloc failure in bn_multipoly");
    } else if (yi < multipoly->dgrx) {
	multipoly->cf = bu_malloc(sizeof(double) * xi * multipoly->dgry, "bu_malloc failure in bn_multipoly");
    }
    for(i = 0; i < multipoly->dgrx; i++) {
	for (j = 0; j < multipoly->dgry; j++) {
	    multipoly->cf[i][j] = oldcf[i][j];
	}
    }
    bu_free(oldcf, "bu_free failure in bn_multipoly");
    multipoly->dgrx = Max(multipoly->dgrx, xi);
    multipoly->dgry = Max(multipoly->dgry, yi);
    return multipoly;
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
