/*                        M S R . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/** @addtogroup bn_msr
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
/** @{ */
/** @file msr.h */

#ifndef BN_MSR_H
#define BN_MSR_H

#include "common.h"
#include "bn/defines.h"
#include "bu/magic.h"

__BEGIN_DECLS

/*
 * Define data structures and constants for the "MSR" random number
 * package.
 *
 * Also define a set of macros to access the random number tables and
 * to limit the area/volume that a set of random numbers inhabit.
 */

struct bn_unif {
    uint32_t magic;
    long msr_seed;
    int msr_double_ptr;
    double *msr_doubles;
    int msr_long_ptr;
    long *msr_longs;
};

#define BN_CK_UNIF(_p)  BU_CKMAG(_p, BN_UNIF_MAGIC, "bn_unif")
#define BN_CK_GAUSS(_p) BU_CKMAG(_p, BN_GAUSS_MAGIC, "bn_gauss")


/**
 * NOTE!!! The order of msr_gauss and msr_unif MUST match in the first
 * three entries as msr_gauss is passed as a msr_unif in
 * msr_gauss_fill.
 */
struct bn_gauss {
    uint32_t magic;
    long msr_gauss_seed;
    int msr_gauss_dbl_ptr;
    double *msr_gauss_doubles;
    int msr_gauss_ptr;
    double *msr_gausses;
};


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
BN_EXPORT extern struct bn_unif *bn_unif_init(long setseed,
					      int method);

/*
 * bn_unif_free	  free random number table
 *
 */
BN_EXPORT extern void bn_unif_free(struct bn_unif *p);

/*	bn_unif_long_fill	fill a random number table.
 *
 * Use the msrad algorithm to fill a random number table
 * with values from 1 to 2^31-1.  These numbers can (and are) extracted from
 * the random number table via high speed macros and bn_unif_long_fill called
 * when the table is exhausted.
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
 *	if (!p) return 1;
 *	if p->msr_longs != NULL
 *		msr_longs is reloaded with random numbers;
 *		msr_long_ptr is set to BN_MSR_MAXTBL
 *	endif
 *	msr_seed is updated.
 @endcode
*/
BN_EXPORT extern long bn_unif_long_fill(struct bn_unif *p);

/*	bn_unif_double_fill	fill a random number table.
 *
 * Use the msrad algorithm to fill a random number table
 * with values from -0.5 to 0.5.  These numbers can (and are) extracted from
 * the random number table via high speed macros and bn_unif_double_fill
 * called when the table is exhausted.
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
BN_EXPORT extern double bn_unif_double_fill(struct bn_unif *p);

/*	bn_gauss_init	Initialize a random number struct for Gaussian
 *	numbers.
 *
 * @par Entry
 *	setseed		Seed to use.
 *	method		method to use to generate numbers (not used)
 *
 * @par Exit
 *	Returns a pointer to a bn_msr_gauss structure.
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
BN_EXPORT extern struct bn_gauss *bn_gauss_init(long setseed,
						int method);

/*
 * bn_gauss_free	free random number table
 *
 */
BN_EXPORT extern void bn_gauss_free(struct bn_gauss *p);

/*	bn_gauss_fill	fill a random number table.
 *
 * Use the msrad algorithm to fill a random number table.  These
 * numbers can (and are) extracted from the random number table via
 * high speed macros and bn_msr_gauss_fill called when the table is
 * exhausted.
 *
 * @par Entry
 *	p	pointer to a bn_msr_gauss structure.
 *
 * @par Exit
 *	if (!p) returns 0.0 else returns a value with a mean of 0 and
 *	    a variance of 1.0.
 *
 * @par Calls
 *	BN_UNIF_CIRCLE to get to uniform random number whose radius is
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
BN_EXPORT extern double bn_gauss_fill(struct bn_gauss *p);

#define BN_UNIF_LONG(_p)	\
    (((_p)->msr_long_ptr) ? \
     (_p)->msr_longs[--(_p)->msr_long_ptr] : \
     bn_unif_long_fill(_p))
#define BN_UNIF_DOUBLE(_p)	\
    (((_p)->msr_double_ptr) ? \
     (_p)->msr_doubles[--(_p)->msr_double_ptr] : \
     bn_unif_double_fill(_p))

#define BN_UNIF_CIRCLE(_p, _x, _y, _r) { \
	do { \
	    (_x) = 2.0*BN_UNIF_DOUBLE((_p)); \
	    (_y) = 2.0*BN_UNIF_DOUBLE((_p)); \
	    (_r) = (_x)*(_x)+(_y)*(_y); \
	} while ((_r) >= 1.0);  }

#define BN_UNIF_SPHERE(_p, _x, _y, _z, _r) { \
	do { \
	    (_x) = 2.0*BN_UNIF_DOUBLE(_p); \
	    (_y) = 2.0*BN_UNIF_DOUBLE(_p); \
	    (_z) = 2.0*BN_UNIF_DOUBLE(_p); \
	    (_r) = (_x)*(_x)+(_y)*(_y)+(_z)*(_z);\
	} while ((_r) >= 1.0) }

#define BN_GAUSS_DOUBLE(_p)	\
    (((_p)->msr_gauss_ptr) ? \
     (_p)->msr_gausses[--(_p)->msr_gauss_ptr] : \
     bn_gauss_fill(_p))

__END_DECLS

#endif  /* BN_MSR_H */
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
