/*                        R A N D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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
/** @addtogroup bn_rnt
 *
 * @brief
 * A supply of fast pseudo-random numbers from table in bn/rand.c.
 * The values are in the open interval (i.e. exclusive) of 0.0 to 1.0
 * range with a period of 4096.
 *
 * @par Usage:
 @code
 unsigned idx;
 float f;

 BN_RANDSEED(idx, integer_seed);

 while (NEED_MORE_RAND_NUMBERS) {
 f = BN_RANDOM(idx);
 }
 @endcode
 *
 * Note that the values from bn_rand_half() become all 0.0 when the
 * benchmark flag is set (bn_rand_halftab is set to all 0's).  The
 * numbers from bn_rand_table do not change, because the procedural
 * noise would cease to exist.
 */
/** @{ */
/** @file rand.h */

#ifndef BN_RAND_H
#define BN_RAND_H

#include "common.h"
#include "bn/defines.h"
#include "vmath.h"

__BEGIN_DECLS

#define BN_RAND_TABSIZE 4096
#define BN_RAND_TABMASK 0xfff
#define BN_RANDSEED(_i, _seed)  _i = ((unsigned)_seed) % BN_RAND_TABSIZE

/**
 * This is our table of random numbers.  Rather than calling drand48()
 * or random() or rand() we just pick numbers out of this table.  This
 * table has 4096 unique entries with floating point values ranging
 * from the open interval (i.e. exclusive) 0.0 to 1.0 range.
 *
 * There are convenience macros for access in the bn.h header.
 */
BN_EXPORT extern const float bn_rand_table[BN_RAND_TABSIZE];

/** BN_RANDOM always gives numbers between the open interval 0.0 to 1.0 */
#define BN_RANDOM(_i)	bn_rand_table[ _i = (_i+1) % BN_RAND_TABSIZE ]

/** BN_RANDHALF always gives numbers between the open interval -0.5 and 0.5 */
#define BN_RANDHALF(_i) (bn_rand_table[ _i = (_i+1) % BN_RAND_TABSIZE ]-0.5)
#define BN_RANDHALF_INIT(_p) _p = bn_rand_table

#define BN_RANDHALFTABSIZE 16535	/* Powers of two give streaking */
BN_EXPORT extern int bn_randhalftabsize;

/**
 *  The actual table of random floating point numbers with values in
 *  the closed interval (i.e. inclusive) -0.5 to +0.5 range.
 *
 *  For benchmarking purposes, this table is zeroed.
 */
BN_EXPORT extern float bn_rand_halftab[BN_RANDHALFTABSIZE];

/**
 * random numbers between the closed interval -0.5 to 0.5 inclusive,
 * except when benchmark flag is set, when this becomes a constant 0.0
 *
 * @param _p float pointer type initialized by bn_rand_init()
 *
 */
#define bn_rand_half(_p)	\
    ((++(_p) >= &bn_rand_halftab[bn_randhalftabsize] || \
      (_p) < bn_rand_halftab) ? \
     *((_p) = bn_rand_halftab) : *(_p))

/**
 * initialize the seed for the large random number table (halftab)
 *
 * @param _p float pointer to be initialized, used for bn_rand0to1()
 * and bn_rand_half()
 * @param _seed Integer SEED for offset in the table.
 *
 */
#define bn_rand_init(_p, _seed)	\
    (_p) = &bn_rand_halftab[ \
	(int)(\
	    (bn_rand_halftab[(_seed)%bn_randhalftabsize] + 0.5) * \
	    (bn_randhalftabsize-1)) ]

/**
 * random numbers in the closed interval 0.0 to 1.0 range (inclusive)
 * except when benchmarking, when this is always 0.5
 *
 * @param _q float pointer type initialized by bn_rand_init()
 *
 */
#define bn_rand0to1(_q)	(bn_rand_half(_q)+0.5)

#define BN_SINTABSIZE 2048

#define bn_tab_sin(_a)	(((_a) > 0) ? \
			 (bn_sin_table[(int)((0.5+ (_a)*(BN_SINTABSIZE / M_2PI)))&(BN_SINTABSIZE-1)]) :\
			 (-bn_sin_table[(int)((0.5- (_a)*(BN_SINTABSIZE / M_2PI)))&(BN_SINTABSIZE-1)]))

/**
 * table of floating point sine values in the closed (i.e. inclusive)
 * interval -1.0 to 1.0 range.
 */
BN_EXPORT extern const float bn_sin_table[BN_SINTABSIZE];

/**
 *@brief
 *  For benchmarking purposes, make the random number table predictable.
 *  Setting to all zeros keeps dithered values at their original values.
 */
BN_EXPORT extern void bn_mathtab_constant(void);

/**
 * @brief
 * Generate a sample point on a sphere per Marsaglia (1972).
 *
 * This routine use bn_randmt internally for the random numbers needed.
 *
 * Note that bn_sph_sample and its internal routines do not initialize the
 * randmt seed - the user should call bn_randmt_seed in their code if a
 * variable seed is required.
 */
BN_EXPORT extern void bn_rand_sph_sample(point_t sample, const point_t center, const fastf_t radius);

/****************************************************************************/
/* Below are thoughts on a new general interface for number generation which
 * should not be considered public API at this time. */
#if 0
/**
 * The following container holds all state associated with a particular
 * number generator.  The details of that state are specific to the
 * type of selected generator and are not public, but the container allows
 * libbn to avoid using globals to maintain state - instead, each "generator"
 * instance has its own state.
 */
typedef struct bn_num_s *bn_numgen;

/**
 * The available number generators in libbn.
 */
typedef enum {
    BN_NUMGEN_PRAND_FAST = 0,
    BN_NUMGEN_PRAND_MT1337,
    BN_NUMGEN_QRAND_SOBOL,
    BN_NUMGEN_QRAND_SCRAMBLED_SOBOL,
    BN_NUMGEN_QRAND_HALTON,
    BN_NUMGEN_NRAND_BOX_MULLER,
    BN_NUMGEN_NRAND_RATIO_OF_UNIFORMS,
    BN_NUMGEN_NULL
} bn_numgen_t;

/**
 * Create an instance of the number generator with the type
 * and seed specified by the user. By default, a generator will
 * return numbers between 0 and 1 - to adjust the range, use
 * bn_numgen_setrange */
BN_EXPORT bn_numgen bn_numgen_create(bn_numgen_t type, double seed);

/** Returns the type of a bn_numgen instance */
BN_EXPORT bn_numgen_t bn_numgen_type(bn_numgen n);

/**
 * Set the range of the numbers to be returned. Return 1 if range
 * is valid for the generator, and zero if it is unsupported. */
BN_EXPORT int bn_numgen_setrange(bn_numgen ngen, double lb, double ub);

/**
 * Some generators only support a finite number of unique returns before
 * repeating.  By default, bn_numgen generators are periodic - they will start
 * over at the beginning.
 *
 * If this function is called with flag == 0, the generator will refuse to
 * repeat itself and not assign any numbers once it reaches its periodic limit.
 * If flag is set to 1, it will restore a generator to its default behavior.
 *
 * If passed -1 in the flag var, the return code will report the existing
 * periodic flag state but will not change it in the generator.
 */
BN_EXPORT int bn_numgen_periodic(bn_numgen ngen, int flag);

/**
 * Destroy the specified bn_numgen instance */
BN_EXPORT void bn_numgen_destroy(bn_numgen n);

/**
 * Get the next cnt numbers in the generator's sequence. See the documentation
 * for each individual generator for the particulars of what output will be
 * returned in the array.  The user is responsible for allocating an array
 * large enough to hold the requested output.
 *
 * When in non-periodic mode, an exhausted generator will report zero numbers
 * assigned to output arrays and will not alter the array contents.
 *
 * Return codes:  >= 0   number of numbers assigned.
 *                 < 0   error.
 */
BN_EXPORT int bn_numgen_get_ints(int *l, size_t cnt, bn_numgen ngen);
BN_EXPORT int bn_numgen_get_ulongs(unsigned long *l, size_t cnt, bn_numgen ngen);
BN_EXPORT int bn_numgen_get_fastf_t(fastf_t *l, size_t cnt, bn_numgen ngen);
BN_EXPORT int bn_numgen_get_doubles(double *l, size_t cnt, bn_numgen ngen);

#endif

__END_DECLS

#endif  /* BN_RAND_H */
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
