/*                           M S R . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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

/** @addtogroup msr */
/** @{ */
/** @file libbn/msr.c
 *
 * @brief
 * Minimal Standard RANdom number generator
 *
 * @par From:
 *	Stephen K. Park and Keith W. Miller
 * @n	"Random number generators: good ones are hard to find"
 * @n	CACM vol 31 no 10, Oct 88
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "bu/malloc.h"
#include "bu/log.h"
#include "vmath.h"
#include "bn.h"

/**
 * Note: BN_MSR_MAXTBL must be an even number, preferably a power of two.
 */
#define	BN_MSR_MAXTBL	4096	/* Size of random number tables. */


#define	A	16807
#define M	2147483647
#define DM	2147483647.0
#define Q	127773		/* Q = M / A */
#define R	2836		/* R = M % A */

struct bn_unif *
bn_unif_init(long int setseed, int method)
{
    struct bn_unif *p;
    BU_ALLOC(p, struct bn_unif);
    p->msr_longs = (long *) bu_malloc(BN_MSR_MAXTBL*sizeof(long), "msr long table");
    p->msr_doubles=(double *) bu_malloc(BN_MSR_MAXTBL*sizeof(double), "msr double table");
    p->msr_seed = 1;
    p->msr_long_ptr = 0;
    p->msr_double_ptr = 0;

    if (method != 0)
	bu_bomb("Method not yet supported in bn_unif_init()");

    if (setseed&0x7fffffff) p->msr_seed=setseed&0x7fffffff;
    p->magic = BN_UNIF_MAGIC;
    return p;
}


long
bn_unif_long_fill(struct bn_unif *p)
{
    register long test, work_seed;
    register int i;

    if (!p)
	return 1;

    /*
     * Gauss and uniform structures have the same format for the
     * first part (gauss is an extension of uniform) so that a gauss
     * structure can be passed to a uniform routine.  This means
     * that we only maintain one structure for gaussian distributions
     * rather than two.  It also means that the user can pull
     * uniform numbers from a gauss structure when the user wants.
     */
    if (p->magic != BN_UNIF_MAGIC && p->magic != BN_GAUSS_MAGIC) {
	BN_CK_UNIF(p);
    }

    work_seed = p->msr_seed;

    if ( p->msr_longs) {
	for (i=0; i < BN_MSR_MAXTBL; i++) {
	    test = A*(work_seed % Q) - R*(work_seed / Q);
	    p->msr_longs[i] = work_seed = (test < 0) ?
		test+M : test;
	}
	p->msr_long_ptr = BN_MSR_MAXTBL;
    }
    test = A*(work_seed % Q) - R*(work_seed / Q);
    p->msr_seed =  (test < 0) ? test+M : test;
    return p->msr_seed;
}


double
bn_unif_double_fill(struct bn_unif *p)
{
    register long test, work_seed;
    register int i;

    if (!p)
	return 0.0;

    /*
     * Gauss and uniform structures have the same format for the
     * first part (gauss is an extension of uniform) so that a gauss
     * structure can be passed to a uniform routine.  This means
     * that we only maintain one structure for gaussian distributions
     * rather than two.  It also means that the user can pull
     * uniform numbers from a gauss structure when the user wants.
     */
    if (p->magic != BN_UNIF_MAGIC && p->magic != BN_GAUSS_MAGIC) {
	BN_CK_UNIF(p);
    }

    work_seed = p->msr_seed;

    if (p->msr_doubles) {
	for (i=0; i < BN_MSR_MAXTBL; i++) {
	    test = A*(work_seed % Q) - R*(work_seed / Q);
	    work_seed = (test < 0) ? test+M : test;
	    p->msr_doubles[i] = ( work_seed - M/2) * 1.0/DM;
	}
	p->msr_double_ptr = BN_MSR_MAXTBL;
    }
    test = A*(work_seed % Q) - R*(work_seed / Q);
    p->msr_seed = (test < 0) ? test+M : test;

    return((p->msr_seed - M/2) * 1.0/DM);
}

/*	bn_unif_free	free random number table
 *
 */
void
bn_unif_free(struct bn_unif *p)
{
    if (!p)
	return;

    bu_free(p->msr_doubles, "msr double table");
    bu_free(p->msr_longs, "msr long table");
    p->magic = 0;
    bu_free(p, "bn_unif");
}



struct bn_gauss *
bn_gauss_init(long int setseed, int method)
{
    struct bn_gauss *p;

    if (method != 0)
	bu_bomb("Method not yet supported in bn_unif_init()");

    BU_ALLOC(p, struct bn_gauss);
    p->msr_gausses=(double *) bu_malloc(BN_MSR_MAXTBL*sizeof(double), "msr gauss table");
    p->msr_gauss_doubles=(double *) bu_malloc(BN_MSR_MAXTBL*sizeof(double), "msr gauss doubles");
    p->msr_gauss_seed = 1;
    p->msr_gauss_ptr = 0;
    p->msr_gauss_dbl_ptr = 0;

    if (setseed&0x7fffffff) p->msr_gauss_seed=setseed&0x7fffffff;
    p->magic = BN_GAUSS_MAGIC;
    return p;
}


double
bn_gauss_fill(struct bn_gauss *p)
{
    register int i;
    /* register */ double v1, v2, r, fac;

    if (!p)
	return 0.0;

    BN_CK_GAUSS(p);

    if (p->msr_gausses) {
	for (i=0; i< BN_MSR_MAXTBL-1; ) {
	    BN_UNIF_CIRCLE((struct bn_unif *)p, v1, v2, r);
	    if (r<0.00001) continue;
	    fac = sqrt(-2.0*log(r)/r);
	    p->msr_gausses[i++] = v1*fac;
	    p->msr_gausses[i++] = v2*fac;
	}
	p->msr_gauss_ptr = BN_MSR_MAXTBL;
    }

    do {
	BN_UNIF_CIRCLE((struct bn_unif *)p, v1, v2, r);
    } while (r < 0.00001);
    fac = sqrt(-2.0*log(r)/r);
    return v1*fac;
}


void
bn_gauss_free(struct bn_gauss *p)
{
    bu_free(p->msr_gauss_doubles, "msr gauss doubles");
    bu_free(p->msr_gausses, "msr gauss table");
    bu_free(p, "bn_msr_gauss");
}


#undef A
#undef M
#undef DM
#undef Q
#undef R

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
