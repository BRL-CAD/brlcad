/*                         M U L T I P O L Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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


#define FAILSTR "failure in multipoly.c"
/* the Max and Min macros in vmath.h are destructive and not suited for here, should this be
 * added somewhere else? */
#define Max(a, b) (((a) > (b)) ? (a) : (b))
#define Min(a, b) (((a) > (b)) ? (a) : (b))

/**
 *        bn_multipoly_new
 *
 * @brief create new multipoly of a given size with coefficients set to 0
 */
struct bn_multipoly *
bn_multipoly_new(int dgrs, int dgrt)
{
    struct bn_multipoly *new = bu_malloc(sizeof(struct bn_multipoly), FAILSTR);
    int    i, s, t;

    new->cf = bu_malloc(dgrs * sizeof(double *), FAILSTR);

    for (i = 0; i < dgrs; i++) {
	new->cf[i] = bu_malloc(dgrt * sizeof(double), FAILSTR);
    }

    new->dgrs = dgrs;
    new->dgrt = dgrt;
    for (s = 0; s < dgrs; s++) {
	for (t = 0; t < dgrt; t++) {
	    new->cf[s][t] = 0;
	}
    }
    return new;
}

/**
 *        bn_multipoly_grow
 *
 * @brief grow the cf array to be at least [dgrx][dgry], sets new entries to 0
 */
struct bn_multipoly *
bn_multipoly_grow(register struct bn_multipoly *P, int dgrs, int dgrt)
{
    int i, j;
    if (dgrs > P->dgrs) {
	P->cf = bu_realloc(P->cf, dgrs * sizeof(double *), FAILSTR);
	for (i = P->dgrs; i < dgrs; i++) {
	    P->cf[i] = bu_malloc(Max(P->dgrt, dgrt) * sizeof(double), FAILSTR);
	    for (j = 0; j < Max(P->dgrt, dgrt); j++) {
		P->cf[i][j] = 0;
	    }
	}
    }
    if (dgrt > P->dgrt) {
	for (i = 0; i < P->dgrt; i++) {
	    P->cf[i] = bu_realloc(P->cf, dgrt * sizeof(double *), FAILSTR);
	    for (j = P->dgrt; j < dgrt; j++) {
		P->cf[i][j] = 0;
	    }
	}
    }
    return P;
}

/**
 *        bn_multipoly_set
 *
 * @brief set a coefficient growing cf array if needed
 */
struct bn_multipoly *
bn_multipoly_set(register struct bn_multipoly *P, int s, int t, double val)
{
    bn_multipoly_grow(P, s + 1, t + 1);
    P->cf[s][t] = val;
    return P;
}

/**
 *        bn_multipoly_add
 *
 * @brief add two polynomials
 */
struct bn_multipoly *
bn_multpoly_add(register struct bn_multipoly *p1, register struct bn_multipoly *p2)
{
    struct bn_multipoly *sum = bn_multipoly_new(Max(p1->dgrs, p2->dgrs), Max(p1->dgrt, p2->dgrs));
    int s, t;
    for (s = 0; s < sum->dgrs; s++) {
	for (t = 0; t < sum->dgrt; t++) {
	    sum->cf[s][t] = p1->cf[s][t] + p2->cf[s][t];
	}
    }
    return sum;
}

/**
 *        bn_multipoly_mul
 *
 * @brief multiply two polynomials
 */

struct bn_multipoly *
bn_multipoly_mul(register struct bn_multipoly *p1, register struct bn_multipoly *p2)
{
    struct bn_multipoly *product = bn_multipoly_new(p1->dgrs + p2->dgrs, p1->dgrt + p2->dgrt);
    int s1,s2,t1,t2;
    for (s1 = 0; s1 < p1->dgrs; s1++) {
	for (t1 = 0; t1 < p1->dgrt; t1++) {
	    for (s2 = 0; s2 < p2->dgrs; s2++) {
		for (t2 = 0; t2 < p2->dgrt; p2++) {
		    product->cf[s1 + s2][t1 + t2] = p1->cf[s1][t1] * p2->cf[s2][t2];
		}
	    }
	}
    }
    return product;
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
