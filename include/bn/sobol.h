/*                        S O B O L . H
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

/*----------------------------------------------------------------------*/
/** @addtogroup bn_sobol
 *
 * @brief
 * Generation of the Sobol quasi-random low-discrepancy sequence of numbers.
 *
 * @par Usage:
 * @code
 * double *snums;
 * struct bn_soboldata *sd = bn_sobol_create(3, time(NULL));
 * if (sd) {
 *   bn_sobol_skip(s, 4000, snums);
 *   for (i = 0; i < 4000; i++) {
 *     snums = bn_sobol_next(s, NULL, NULL);
 *     printf("x[%d]: %g %g %g", i, snums[0], snums[1], snums[2]);
 *   }
 *   bn_sobol_destroy(sd);
 * }
 * @endcode
 *
 */
/** @{ */
/** @file sobol.h */

#ifndef BN_SOBOL_H
#define BN_SOBOL_H

#include "common.h"
#include "bn/defines.h"
#include "vmath.h"

__BEGIN_DECLS

struct bn_soboldata;

/**
 * Maximum dimension of Sobol output array
 */
#define BN_SOBOL_MAXDIM 1111

/**
 * Create and initialize an instance of a Sobol sequence data container.  If seed
 * is non-zero the value will be used in initialization, otherwise a default will
 * be used.  User must destroy the returned data with bn_sobol_destroy */
BN_EXPORT extern struct bn_soboldata * bn_sobol_create(unsigned int sdim, unsigned long seed);

/** Destroy a Sobol data container */
BN_EXPORT extern void bn_sobol_destroy(struct bn_soboldata *s);

/**
 * Return the next vector in Sobol sequence, scaled to (lb[i], ub[i]) interval.
 *
 * If lb and ub are NULL, x[i] will be in the range (0,1).
 *
 * The return vector is read only and is managed internally by bn_sobodata.
 *
 * Note: not performing the scale saves some math operations, so NULL lb and ub
 * are recommend if the required interval for the caller's application happens
 * to be (0,1).
 *
 * Note: If the user attempts to read more than 2^32-1 points from the sequence,
 * the generator will fall back on pseudo random number generation.
 */
BN_EXPORT extern double * bn_sobol_next(struct bn_soboldata *s, const double *lb, const double *ub);

/**
 * If the caller knows in advance how many numbers (n) they want to compute,
 * this function supports implementation of the Acworth et al (1998) strategy
 * of skipping a number of points equal to the largest power of 2 smaller than
 * n for better performance.
 *
 * Joe and Kuo indicate in their notes at
 * http://web.maths.unsw.edu.au/~fkuo/sobol/ that they are "less persuaded" by
 * this recommendation, but this function is available for callers who wish to
 * use it.
 */
BN_EXPORT extern void bn_sobol_skip(struct bn_soboldata *s, unsigned n);

/**
 * @brief
 * Generate a sample point on a sphere per Marsaglia (1972), using the Sobol
 * data sequence s to drive the selection.
 *
 * The caller is responsible for initializing the bn_sobodata sequence before
 * generating points.
 *
 * TODO: investigate the http://www.dtic.mil/docs/citations/ADA510216
 * scrambling method to see if basic Sobol sequence can be improved on for
 * spherical sampling Also relevant: people.sc.fsu.edu/~hcc8471/ssobol.pdf
 */
BN_EXPORT extern void bn_sobol_sph_sample(point_t sample, const point_t center, const fastf_t radius, struct bn_soboldata *s);

__END_DECLS

#endif  /* BN_SOBOL_H */
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
