/*                         N U M G E N . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

/** @addtogroup numgen */
/** @{ */
/** @file libbn/numgen.c
 *
 * @brief
 * Pseudo and quasi-random number generators.
 */

#include "common.h"

#include <stdio.h>
#include "vmath.h"
#include "bn/rand.h"
#include "bn/randmt.h"
#include "bn/numgen.h"

typedef struct bn_num_s {
    bn_numgen_t type;
    double lb; /* lower bound of range */
    double ub; /* upper bound of range */
    int nperiodic;
    unsigned sdim; /* dimension of sequence being generated */
    uint32_t *mdata; /* array of length 32 * sdim */
    uint32_t *m[32]; /* more convenient pointers to mdata, of direction #s */
    uint32_t *x; /* previous x = x_n, array of length sdim */
    unsigned *b; /* position of fixed point in x[i] is after bit b[i] */
    uint32_t n; /* number of x's generated so far */
    uint32_t NL_mt[NL_N]; /* the array for the state vector  */
    int NL_mti; /* mti==N+1 means mt[N] is not initialized */
} numgen;

bn_numgen
bn_numgen_create(bn_numgen_t type, double seed, int dim)
{
    numgen *generator = (numgen *)bu_calloc(1, sizeof(struct bn_numgen_s), "bn_numgen generator state");
    generator->type = type;

    if (dim > 0) return NULL; /* quell unused until we actually use it */
    if (seed < 0) return NULL; /* quell unused until we actually use it */

    switch(type) {
	case BN_NUMGEN_PRAND_MT1337:
	    /*nlopt_init_genrand(generator, (unsigned long)seed);*/
	    break;
	case BN_NUMGEN_QRAND_SOBOL:
	    /* sobol_init */
	    break;
	default:
	    break;
    }
    return generator;
}

bu_numgen_t
bn_numgen_type(bn_numgen n)
{
    numgen *g = NULL;
    if (!n) return BN_NUMGEN_NULL;
    g = (numgen *)n;
    return g->type;
}

int
bn_numgen_setrange(bn_numgen ngen, double lb, double ub)
{
    numgen *g = NULL;
    if (!ngen) return 0;
    g = (numgen *)ngen;

    /* If there are any type based range restrictions, test for them here */

    g->lb = lb;
    g->ub = ub;
    return 1;
}

int
bn_numgen_periodic(bn_numgen ngen, int flag)
{
    numgen *g = NULL;
    if (!ngen) return -1;
    g = (numgen *)ngen;
    if (flag == -1) return !g->nperoidic;
    g->nperiodic = !flag;
    return !g->nperoidic;
}

void
bn_numgen_destroy(bn_numgen ngen)
{
    numgen *g = NULL;
    if (!ngen) return;
    g = (numgen *)ngen;

    bu_free(g, "free container");
    ngen = NULL;
}

int
bn_numgen_next_doubles(double *l, size_t cnt, bn_numgen ngen)
{
    size_t i = 0;
    numgen *g = NULL;
    if (!ngen || (!l && cnt > 0)) return -1;
    g = (numgen *)ngen;
    if (!cnt) return 0;

    switch(g->type) {
	case BN_NUMGEN_PRAND_MT1337:
	    /*mt_next();*/
	    break;
	case BN_NUMGEN_QRAND_SOBOL:
	    /*sobol_next()*/
	    break;
	case BN_NUMGEN_NULL:
	    for (i = 0; i < cnt; i++) l[i] = 0.0;
	    return cnt;
	default:
	    break;
    }
}

size_t
bn_sph_pnts(point_t *pnts, size_t cnt, bn_numgen n)
{
    size_t i = 0;
    size_t ret = 0;

    if (!pnts || !n || cnt == 0) return 0;

    /* Make sure our generator has the right upper and lower bounds*/
    if(!bn_numgen_setrange(n, -1, 1)) {
	bu_log("Error - number generator does not support range -1 to 1 - cannot generate points!\n");
	return 0;
    }

    for (i = 0; i < cnt; i++) {
	double p[2] = { 0, 0 };
	double px, py, pz;
	double S;

	int success = 0;

	while (!success) {

	    /* Assume the next numbers will work until proven otherwise... */
	    success = 1;

	    /* Get our next two numbers */
	    if(bn_numgen_next_doubles((double *)p, 2, n) < 2) {
		bu_log("Error - number generator could not generate 2 points when working on point %d, aborting!\n", i);
		return 0;
	    }

	    /* Check that p[0]^2+p[1]^2 < 1 */
	    S = p[0]*p[0] + p[1]*p[1];
	    if (S >= 1) success = 0;
	}

	/* Given the random numbers, generate the xyz points on the
	 * unit sphere */
	px = 2 * p[0] * sqrt(1 - S);
	py = 2 * p[1] * sqrt(1 - S);
	pz = 1 - 2 * S;

	pnts[i][0] = px;
	pnts[i][1] = py;
	pnts[i][2] = pz;

	ret++;
    }

    return ret;
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
