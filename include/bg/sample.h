/*                        S A M P L E . H
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
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
/** @file sample.h
 *
 * Brief description
 *
 */

#ifndef BG_SAMPLE_H
#define BG_SAMPLE_H

#include "common.h"
#include "vmath.h"
#include "bv/defines.h"
#include "bg/defines.h"

__BEGIN_DECLS

/* For a given view and width, determine a recommended sampling
 * spacing to produce visually smooth curves */
BG_EXPORT extern fastf_t
bg_sample_spacing(const struct bview *v, fastf_t w);


/**
 * Calculate the coordinates of a point at the specified radian around a
 * the given ellipse
 */
BG_EXPORT extern void
bg_ell_radian_pnt(
	point_t result,
	const vect_t center,
	const vect_t axis_a,
	const vect_t axis_b,
	fastf_t radian);

/**
 * Construct an array of num_points uniformly sampled points around
 * an ellipse.  Returns the number of points in the array if successful,
 * else -1.
 */
BG_EXPORT extern int
bg_ell_sample(
	point_t **pnts,
	const vect_t center,
	const vect_t axis_a,
	const vect_t axis_b,
	int num_points);


__END_DECLS

#endif  /* BG_SAMPLE_H */
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
