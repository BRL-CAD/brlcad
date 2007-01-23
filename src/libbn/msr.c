/*                           M S R . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2007 United States Government as represented by
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
/** @file msr.c
 *
 * @brief
 * Minimal Standard RANdom number generator
 *
 * @par From:
 *	Stephen K. Park and Keith W. Miller
 * @n	"Random number generators: good ones are hard to find"
 * @n	CACM vol 31 no 10, Oct 88
 *
 *  @author
 *	Christopher T. Johnson - 90/04/20
 *
 * @par Source -
 *	The U. S. Army Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */


#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"

/**
 * Note: BN_MSR_MAXTBL must be an even number.
 */
#define	BN_MSR_MAXTBL	4096	/* Size of random number tables. */


/*	bn_unif_init	Initialize a random number structure.
 *
 * @par Entry
 *	setseed	seed to use
 *	method  method to use to generate numbers;
 *
 * @par Exit
 *	returns	a pointer to a bn_unif structure.
 *	returns 0 on error.
 *
 * @par Uses
 *	None.
 *
 * @par Calls
 *	bu_malloc
 *
 * @par Method @code
 *	malloc up a structure with pointers to the numbers
 *	get space for the integer table.
 *	get space for the floating table.
 *	set pointer counters
 *	set seed if one was given and setseed != 1
@endcode
 *
 */
#define	A	16807
#define M	2147483647
#define DM	2147483647.0
#define Q	127773		/* Q = M / A */
#define R	2836		/* R = M % A */
struct bn_unif *
bn_unif_init(long int setseed, int method)
{
	struct bn_unif *p;
	p = (struct bn_unif *) bu_malloc(sizeof(struct bn_unif),"bn_unif");
	p->msr_longs = (long *) bu_malloc(BN_MSR_MAXTBL*sizeof(long), "msr long table");
	p->msr_doubles=(double *) bu_malloc(BN_MSR_MAXTBL*sizeof(double),"msr double table");
	p->msr_seed = 1;
	p->msr_long_ptr = 0;
	p->msr_double_ptr = 0;

	if (setseed&0x7fffffff) p->msr_seed=setseed&0x7fffffff;
	p->magic = BN_UNIF_MAGIC;
	return(p);
}

/*	bn_unif_long_fill	fill a random number table.
 *
 * Use the msrad algorithm to fill a random number table
 * with values from 1 to 2^31-1.  These numbers can (and are) extracted from
 * the random number table via high speed macros and bn_unif_long_fill called
 * when the table is exauseted.
 *
 * @par Entry
 *	p	pointer to a bn_unif structure.
 *
 * @par Exit
 *	if (!p) returns 1 else returns a value between 1 and 2^31-1
 *
 * @par Calls
 *	None.  msran is inlined for speed reasons.
 *
 * @par Uses
 *	None.
 *
 * @par Method @code
 *	if (!p) return(1);
 *	if p->msr_longs != NULL
 *		msr_longs is reloaded with random numbers;
 *		msr_long_ptr is set to BN_MSR_MAXTBL
 *	endif
 *	msr_seed is updated.
@endcode
 */
long
bn_unif_long_fill(struct bn_unif *p)
{
	register long test,work_seed;
	register int i;

	/*
	 * Gauss and uniform structures have the same format for the
	 * first part (gauss is an extention of uniform) so that a gauss
	 * structure can be passed to a uniform routine.  This means
	 * that we only maintain one structure for gaussian distributions
	 * rather than two.  It also means that the user can pull
	 * uniform numbers from a guass structure when the user wants.
	 */
	if (!p || (p->magic != BN_UNIF_MAGIC &&
	    p->magic != BN_GAUSS_MAGIC)) {
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
	return(p->msr_seed);
}

/*	bn_unif_double_fill	fill a random number table.
 *
 * Use the msrad algorithm to fill a random number table
 * with values from -0.5 to 0.5.  These numbers can (and are) extracted from
 * the random number table via high speed macros and bn_unif_double_fill
 * called when the table is exauseted.
 *
 * @par Entry
 *	p	pointer to a bn_unif structure.
 *
 * @par Exit
 *	if (!p) returns 0.0 else returns a value between -0.5 and 0.5
 *
 * @par Calls
 *	None.  msran is inlined for speed reasons.
 *
 * @par Uses
 *	None.
 *
 * @par Method @code
 *	if (!p) return (0.0)
 *	if p->msr_longs != NULL
 *		msr_longs is reloaded with random numbers;
 *		msr_long_ptr is set to BN_MSR_MAXTBL
 *	endif
 *	msr_seed is updated.
@endcode
 */
double
bn_unif_double_fill(struct bn_unif *p)
{
	register long test,work_seed;
	register int i;

	/*
	 * Gauss and uniform structures have the same format for the
	 * first part (gauss is an extention of uniform) so that a gauss
	 * structure can be passed to a uniform routine.  This means
	 * that we only maintain one structure for gaussian distributions
	 * rather than two.  It also means that the user can pull
	 * uniform numbers from a guass structure when the user wants.
	 */
	if (!p || (p->magic != BN_UNIF_MAGIC &&
	    p->magic != BN_GAUSS_MAGIC)) {
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
	bu_free(p->msr_doubles, "msr double table");
	bu_free(p->msr_longs, "msr long table");
	p->magic = 0;
	bu_free(p, "bn_unif");
}


/*	bn_gauss_init	Initialize a random number struct for gaussian
 *	numbers.
 *
 * @par Entry
 *	setseed		Seed to use.
 *	method		method to use to generate numbers (not used)
 *
 * @par Exit
 *	Returns a pointer toa bn_msr_guass structure.
 *	returns 0 on error.
 *
 * @par Calls
 *	bu_malloc
 *
 * @par Uses
 *	None.
 *
 * @par Method @code
 *	malloc up a structure
 *	get table space
 *	set seed and pointer.
 *	if setseed != 0 then seed = setseed
@endcode
 */
struct bn_gauss *
bn_gauss_init(long int setseed, int method)
{
	struct bn_gauss *p;
	p = (struct bn_gauss *) bu_malloc(sizeof(struct bn_gauss),"bn_msr_guass");
	p->msr_gausses=(double *) bu_malloc(BN_MSR_MAXTBL*sizeof(double),"msr guass table");
	p->msr_gauss_doubles=(double *) bu_malloc(BN_MSR_MAXTBL*sizeof(double),"msr guass doubles");
	p->msr_gauss_seed = 1;
	p->msr_gauss_ptr = 0;
	p->msr_gauss_dbl_ptr = 0;

	if (setseed&0x7fffffff) p->msr_gauss_seed=setseed&0x7fffffff;
	p->magic = BN_GAUSS_MAGIC;
	return(p);
}

/*	bn_gauss_fill	fill a random number table.
 *
 * Use the msrad algorithm to fill a random number table.
 * hese numbers can (and are) extracted from
 * the random number table via high speed macros and bn_msr_guass_fill
 * called when the table is exauseted.
 *
 * @par Entry
 *	p	pointer to a bn_msr_guass structure.
 *
 * @par Exit
 *	if (!p) returns 0.0 else returns a value with a mean of 0 and
 *	    a variance of 1.0.
 *
 * @par Calls
 *	BN_UNIF_CIRCLE to get to uniform random number whos radius is
 *	<= 1.0. I.e. sqrt(v1*v1 + v2*v2) <= 1.0
 *	BN_UNIF_CIRCLE is a macro which can call bn_unif_double_fill.
 *
 * @par Uses
 *	None.
 *
 * @par Method @code
 *	if (!p) return (0.0)
 *	if p->msr_longs != NULL
 *		msr_longs is reloaded with random numbers;
 *		msr_long_ptr is set to BN_MSR_MAXTBL
 *	endif
 *	msr_seed is updated.
@endcode
 */
double
bn_gauss_fill(struct bn_gauss *p)
{
	register int i;
	/* register */ double v1,v2,r,fac;

	BN_CK_GAUSS(p);

	if (p->msr_gausses) {
		for (i=0; i< BN_MSR_MAXTBL-1; ) {
			BN_UNIF_CIRCLE((struct bn_unif *)p,v1,v2,r);
			if (r<0.00001) continue;
			fac = sqrt(-2.0*log(r)/r);
			p->msr_gausses[i++] = v1*fac;
			p->msr_gausses[i++] = v2*fac;
		}
		p->msr_gauss_ptr = BN_MSR_MAXTBL;
	}

	do {
		BN_UNIF_CIRCLE((struct bn_unif *)p,v1,v2,r);
	} while (r < 0.00001);
	fac = sqrt(-2.0*log(r)/r);
	return(v1*fac);
}
/*	bn_gauss_free	free random number table
 *
 */
void
bn_gauss_free(struct bn_gauss *p)
{
	bu_free(p->msr_gauss_doubles, "msr guass doubles");
	bu_free(p->msr_gausses,"msr guass table");
	bu_free(p,"bn_msr_guass");
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
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
