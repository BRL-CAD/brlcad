/*                         M U L T I P O L Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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

/** @addtogroup poly */
/** @{ */
/** @file libbn/multipoly.c
 *
 *	Library for dealing with bivariate polynomials.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "bu/interrupt.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "bn/multipoly.h"


#define FAILSTR "failure in multipoly.c"

/**
 *        bn_multipoly_new
 *
 * @brief create new multipoly of a given size with coefficients set to 0
 */
struct bn_multipoly *
bn_multipoly_new(int dgrs, int dgrt)
{
    struct bn_multipoly *newmp;
    int i;

    if (dgrs <= 0 || dgrt <= 0)
	return NULL;

    BU_ALLOC(newmp, struct bn_multipoly);
    newmp->magic = BN_MULTIPOLY_MAGIC;
    newmp->dgrs = dgrs;
    newmp->dgrt = dgrt;
    newmp->cf = (double **)bu_calloc(dgrs, sizeof(double *), FAILSTR);

    for (i = 0; i < dgrs; i++) {
	newmp->cf[i] = (double *)bu_calloc(dgrt, sizeof(double), FAILSTR);
    }

    return newmp;
}

/**
 *        bn_multipoly_free
 *
 * @brief free a multipoly structure
 */
void
bn_multipoly_free(struct bn_multipoly *p)
{
    int i;
    if (!p)
	return;
    BN_CK_MULTIPOLY(p);
    if (p->cf) {
	for (i = 0; i < p->dgrs; i++) {
	    if (p->cf[i])
		bu_free(p->cf[i], "multipoly row");
	}
	bu_free(p->cf, "multipoly cf");
	p->cf = NULL;
    }
    p->magic = 0;
    p->dgrs = 0;
    p->dgrt = 0;
    bu_free(p, "bn_multipoly");
}

/**
 *        bn_multipoly_grow
 *
 * @brief grow the cf array to be at least [dgrs][dgrt], sets new entries to 0
 */
struct bn_multipoly *
bn_multipoly_grow(struct bn_multipoly *P, int dgrs, int dgrt)
{
    int i, j;
    int target_s, target_t;

    if (!P)
	return NULL;
    BN_CK_MULTIPOLY(P);

    target_s = (dgrs > P->dgrs) ? dgrs : P->dgrs;
    target_t = (dgrt > P->dgrt) ? dgrt : P->dgrt;

    if (target_s == P->dgrs && target_t == P->dgrt)
	return P;

    /* If row count increases, reallocate the pointer array */
    if (target_s > P->dgrs) {
	P->cf = (double **)bu_realloc(P->cf, target_s * sizeof(double *), FAILSTR);
	/* Allocate new rows with target_t columns */
	for (i = P->dgrs; i < target_s; i++) {
	    P->cf[i] = (double *)bu_calloc(target_t, sizeof(double), FAILSTR);
	}
    }

    /* If column count increases, reallocate existing rows */
    if (target_t > P->dgrt) {
	for (i = 0; i < P->dgrs; i++) {
	    P->cf[i] = (double *)bu_realloc(P->cf[i], target_t * sizeof(double), FAILSTR);
	    for (j = P->dgrt; j < target_t; j++) {
		P->cf[i][j] = 0.0;
	    }
	}
    }

    P->dgrs = target_s;
    P->dgrt = target_t;
    return P;
}

/**
 *        bn_multipoly_set
 *
 * @brief set a coefficient growing cf array if needed
 */
struct bn_multipoly *
bn_multipoly_set(struct bn_multipoly *P, int s, int t, double val)
{
    if (!P || s < 0 || t < 0)
	return NULL;
    BN_CK_MULTIPOLY(P);
    if (!bn_multipoly_grow(P, s + 1, t + 1))
	return NULL;
    P->cf[s][t] = val;
    return P;
}

/**
 *        bn_multipoly_add
 *
 * @brief add two polynomials
 */
struct bn_multipoly *
bn_multipoly_add(const struct bn_multipoly *p1, const struct bn_multipoly *p2)
{
    struct bn_multipoly *sum;
    int max_s, max_t;
    int s, t;

    if (!p1 || !p2)
	return NULL;
    BN_CK_MULTIPOLY(p1);
    BN_CK_MULTIPOLY(p2);

    max_s = (p1->dgrs > p2->dgrs) ? p1->dgrs : p2->dgrs;
    max_t = (p1->dgrt > p2->dgrt) ? p1->dgrt : p2->dgrt;

    sum = bn_multipoly_new(max_s, max_t);
    if (!sum)
	return NULL;

    for (s = 0; s < max_s; s++) {
	for (t = 0; t < max_t; t++) {
	    double v1 = (s < p1->dgrs && t < p1->dgrt) ? p1->cf[s][t] : 0.0;
	    double v2 = (s < p2->dgrs && t < p2->dgrt) ? p2->cf[s][t] : 0.0;
	    sum->cf[s][t] = v1 + v2;
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
bn_multipoly_mul(const struct bn_multipoly *p1, const struct bn_multipoly *p2)
{
    struct bn_multipoly *product;
    int s1, s2, t1, t2;

    if (!p1 || !p2)
	return NULL;
    BN_CK_MULTIPOLY(p1);
    BN_CK_MULTIPOLY(p2);

    product = bn_multipoly_new(p1->dgrs + p2->dgrs, p1->dgrt + p2->dgrt);
    if (!product)
	return NULL;

    for (s1 = 0; s1 < p1->dgrs; s1++) {
	for (t1 = 0; t1 < p1->dgrt; t1++) {
	    for (s2 = 0; s2 < p2->dgrs; s2++) {
		for (t2 = 0; t2 < p2->dgrt; p2++) {
		    product->cf[s1 + s2][t1 + t2] += p1->cf[s1][t1] * p2->cf[s2][t2];
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
