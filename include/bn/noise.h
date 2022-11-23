/*                        N O I S E . H
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


/**
 * @addtogroup bn_noise
 *
 * @brief
 * These noise functions provide mostly random noise at the integer
 * lattice points.
 *
 * The functions should be evaluated at non-integer
 * locations for their nature to be realized.
 *
 * Contains contributed code from:
 * F. Kenton Musgrave
 * Robert Skinner
 *
 */
/** @{ */
/* @file noise.h */

#ifndef BN_NOISE_H
#define BN_NOISE_H

#include "common.h"
#include "vmath.h"
#include "bn/defines.h"

__BEGIN_DECLS

/*
 * fractal noise support
 */

BN_EXPORT extern void bn_noise_init(void);

/**
 *@brief
 * Robert Skinner's Perlin-style "Noise" function
 *
 * Results are in the range [-0.5 .. 0.5].  Unlike many
 * implementations, this function provides random noise at the integer
 * lattice values.  However this produces much poorer quality and
 * should be avoided if possible.
 *
 * The power distribution of the result has no particular shape,
 * though it isn't as flat as the literature would have one believe.
 */
BN_EXPORT extern double bn_noise_perlin(point_t pt);

/* FIXME: Why isn't the result listed first? */

/**
 * Vector-valued "Noise"
 */
BN_EXPORT extern void bn_noise_vec(point_t point,
				   point_t result);

/**
 * @brief
 * Procedural fBm evaluated at "point"; returns value stored in
 * "value".
 *
 * @param point          location to sample noise
 * @param h_val      fractal increment parameter
 * @param lacunarity gap between successive frequencies
 * @param octaves  	 number of frequencies in the fBm
 *
 * The spectral properties of the result are in the APPROXIMATE range
 * [-1..1] Depending upon the number of octaves computed, this range
 * may be exceeded.  Applications should clamp or scale the result to
 * their needs.  The results have a more-or-less gaussian
 * distribution.  Typical results for 1M samples include:
 *
 * @li Min -1.15246
 * @li Max  1.23146
 * @li Mean -0.0138744
 * @li s.d.  0.306642
 * @li Var 0.0940295
 *
 * The function call pow() is relatively expensive.  Therefore, this
 * function pre-computes and saves the spectral weights in a table for
 * re-use in successive invocations.
 */
BN_EXPORT extern double bn_noise_fbm(point_t point,
				     double h_val,
				     double lacunarity,
				     double octaves);

/**
 * @brief
 * Procedural turbulence evaluated at "point";
 *
 * @return turbulence value for point
 *
 * @param point        location to sample noise at
 * @param h_val        fractal increment parameter
 * @param lacunarity   gap between successive frequencies
 * @param octaves      number of frequencies in the fBm
 *
 * The result is characterized by sharp, narrow trenches in low values
 * and a more fbm-like quality in the mid-high values.  Values are in
 * the APPROXIMATE range [0 .. 1] depending upon the number of octaves
 * evaluated.  Typical results:
 @code
 * Min  0.00857137
 * Max  1.26712
 * Mean 0.395122
 * s.d. 0.174796
 * Var  0.0305536
 @endcode
 * The function call pow() is relatively expensive.  Therefore, this
 * function pre-computes and saves the spectral weights in a table for
 * re-use in successive invocations.
 */
BN_EXPORT extern double bn_noise_turb(point_t point,
				      double h_val,
				      double lacunarity,
				      double octaves);

/**
 * From "Texturing and Modeling, A Procedural Approach" 2nd ed
 */
BN_EXPORT extern double bn_noise_mf(point_t point,
				    double h_val,
				    double lacunarity,
				    double octaves,
				    double offset);

/**
 *@brief
 * A ridged noise pattern
 *
 * From "Texturing and Modeling, A Procedural Approach" 2nd ed p338
 */
BN_EXPORT extern double bn_noise_ridged(point_t point,
					double h_val,
					double lacunarity,
					double octaves,
					double offset);

__END_DECLS

#endif  /* BN_NOISE_H */
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
