/*                      N U M G E N . H
 * BRL-CAD
 *
 * Copyright (c) 2016-2022 United States Government as represented by
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
/** @addtogroup bn_rand
 *
 * @brief
 * Routines for generating series of pseudo-random or quasi-random numbers.
 *
 * @code
 * @endcode
 */
/** @{ */
/** @file numgen.h */

#ifndef BN_NUMGEN_H
#define BN_NUMGEN_H

#include "common.h"

#include "vmath.h"

#include "bn/defines.h"

__BEGIN_DECLS
/* Most of the following are API design possibilities only - not yet active */
/**
 * The following container holds all state associated with a particular
 * number generator.  The details of that state are specific to the
 * type of selected generator and are not public, but the container allows
 * libbn to avoid using globals to maintain state - instead, each "generator"
 * instance has its own state.
 */
typedef struct bn_num_s *bn_numgen;

#if 0
/**
 * The available number generators in libbn.
 */
typedef enum {
    BN_NUMGEN_PRAND_TABLE = 0,
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
BN_EXPORT bn_numgen bn_numgen_create(bn_numgen_t type, int dim, double seed);

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
BN_EXPORT int bn_numgen_next_ints(int *l, size_t cnt, bn_numgen ngen);
BN_EXPORT int bn_numgen_next_ulongs(unsigned long *l, size_t cnt, bn_numgen ngen);
BN_EXPORT int bn_numgen_next_fastf_t(fastf_t *l, size_t cnt, bn_numgen ngen);
BN_EXPORT int bn_numgen_next_doubles(double *l, size_t cnt, bn_numgen ngen);
#endif

/**
 * @brief
 * Generate points on a unit sphere per Marsaglia (1972):
 * https://projecteuclid.org/euclid.aoms/1177692644
 *
 * The user is responsible for selecting the numerical generator used to
 * supply pseudo or quasi-random numbers to bg_sph_sample - different
 * types of inputs may be needed depending on the application.
 */
BN_EXPORT extern void bn_sph_pnts(point_t *pnts, size_t cnt, bn_numgen n);



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
