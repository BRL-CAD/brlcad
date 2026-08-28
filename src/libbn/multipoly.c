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

#include "bu/malloc.h"
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

    if (dgrs <= 0 || dgrt <= 0) {
	return NULL;
	}

    BU_ALLOC(newmp, struct bn_multipoly);
    newmp->cf = (double **)bu_calloc(dgrs, sizeof(double *), FAILSTR);

    for (i = 0; i < dgrs; i++) {
	newmp->cf[i] = (double *)bu_calloc(dgrt, sizeof(double), FAILSTR);
    }

    newmp->dgrs = dgrs;
    newmp->dgrt = dgrt;
    return newmp;
}


void
bn_multipoly_free(struct bn_multipoly *poly)
{
    int s;

    if (!poly) {
	return;
    }

    for (s = 0; s < poly->dgrs; s++) {
	bu_free(poly->cf[s], "bn_multipoly coefficients");
    }
    bu_free(poly->cf, "bn_multipoly coefficient rows");
    bu_free(poly, "bn_multipoly");
}

/**
 *        bn_multipoly_grow
 *
 * @brief grow the cf array to be at least [dgrx][dgry], sets new entries to 0
 */
struct bn_multipoly *
bn_multipoly_grow(register struct bn_multipoly *P, int dgrs, int dgrt)
{
    double **coefficients;
    int new_dgrs, new_dgrt;
    int s, t;

    if (!P) {
	return NULL;
    }

    new_dgrs = (dgrs > P->dgrs) ? dgrs : P->dgrs;
    new_dgrt = (dgrt > P->dgrt) ? dgrt : P->dgrt;
    if (new_dgrs == P->dgrs && new_dgrt == P->dgrt) {
	return P;
    }

    coefficients = (double **)bu_calloc(new_dgrs, sizeof(double *), FAILSTR);
    for (s = 0; s < new_dgrs; s++) {
	coefficients[s] = (double *)bu_calloc(new_dgrt, sizeof(double), FAILSTR);
    }
    for (s = 0; s < P->dgrs; s++) {
	for (t = 0; t < P->dgrt; t++) {
	    coefficients[s][t] = P->cf[s][t];
	}
	bu_free(P->cf[s], "bn_multipoly coefficients");
    }
    bu_free(P->cf, "bn_multipoly coefficient rows");

    P->cf = coefficients;
    P->dgrs = new_dgrs;
    P->dgrt = new_dgrt;
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
    if (!P || s < 0 || t < 0) {
	return NULL;
    }
    if (!bn_multipoly_grow(P, s + 1, t + 1)) {
	return NULL;
    }
    P->cf[s][t] = val;
    return P;
}

/**
 *        bn_multipoly_add
 *
 * @brief add two polynomials
 */
struct bn_multipoly *
bn_multipoly_add(register const struct bn_multipoly *p1, register const struct bn_multipoly *p2)
{
    struct bn_multipoly *sum;
    int s, t;

	if (!p1 || !p2 || p1->dgrs <= 0 || p1->dgrt <= 0 ||
	p2->dgrs <= 0 || p2->dgrt <= 0) {
	return NULL;
    }

    sum = bn_multipoly_new((p1->dgrs > p2->dgrs) ? p1->dgrs : p2->dgrs,
			    (p1->dgrt > p2->dgrt) ? p1->dgrt : p2->dgrt);
    for (s = 0; s < sum->dgrs; s++) {
	for (t = 0; t < sum->dgrt; t++) {
	    if (s < p1->dgrs && t < p1->dgrt) {
		sum->cf[s][t] += p1->cf[s][t];
	    }
	    if (s < p2->dgrs && t < p2->dgrt) {
		sum->cf[s][t] += p2->cf[s][t];
	    }
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
bn_multipoly_mul(register const struct bn_multipoly *p1, register const struct bn_multipoly *p2)
{
    struct bn_multipoly *product;
    int s1, s2, t1, t2;

    if (!p1 || !p2 || p1->dgrs <= 0 || p1->dgrt <= 0 ||
	p2->dgrs <= 0 || p2->dgrt <= 0) {
	return NULL;
	}

    product = bn_multipoly_new(p1->dgrs + p2->dgrs - 1,
				p1->dgrt + p2->dgrt - 1);
    for (s1 = 0; s1 < p1->dgrs; s1++) {
	for (t1 = 0; t1 < p1->dgrt; t1++) {
	    for (s2 = 0; s2 < p2->dgrs; s2++) {
		for (t2 = 0; t2 < p2->dgrt; t2++) {
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
