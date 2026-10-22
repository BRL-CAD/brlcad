/*                       C A N O N I C A L I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <math.h>

#include "bn/mat.h"
#include "vmath.h"

#include "canonicalize_private.h"


int
rt_make_canonical_similarity_frame(mat_t canonical_to_input,
	fastf_t *canonical_lengths, const point_t origin,
	const vect_t input_x, const vect_t input_z, const fastf_t *lengths,
	size_t length_count, const struct bn_tol *tol,
	enum rt_canonicalize_mode mode)
{
    vect_t x_axis;
    vect_t y_axis;
    vect_t z_axis;
    fastf_t x_length;
    fastf_t z_length;
    fastf_t projection;
    fastf_t uniform_scale = mode == RT_CANONICALIZE_RIGID ? 1.0 : 0.0;
    mat_t orientation;
    mat_t scale;
    mat_t oriented_scale;
    mat_t translate;

    if (!canonical_to_input || !canonical_lengths || !origin || !input_x ||
	!input_z || !lengths || !length_count || !tol ||
	mode < RT_CANONICALIZE_RIGID || mode > RT_CANONICALIZE_AFFINE)
	return 0;
    if (!isfinite(origin[X]) || !isfinite(origin[Y]) ||
	!isfinite(origin[Z]))
	return 0;

    x_length = MAGNITUDE(input_x);
    z_length = MAGNITUDE(input_z);
    if (!isfinite(x_length) || !isfinite(z_length) ||
	x_length <= tol->dist || z_length <= tol->dist)
	return 0;
    VSCALE(x_axis, input_x, 1.0 / x_length);
    VSCALE(z_axis, input_z, 1.0 / z_length);
    projection = VDOT(x_axis, z_axis);
    if (!NEAR_ZERO(projection, tol->perp))
	return 0;

    /* Remove accepted input roundoff so write-mode matrices are proper
     * rotations rather than barely-sheared approximations. */
    VJOIN1(x_axis, x_axis, -projection, z_axis);
    VUNITIZE(x_axis);
    VCROSS(y_axis, z_axis, x_axis);
    VUNITIZE(y_axis);

    for (size_t i = 0; i < length_count; i++) {
	if (!isfinite(lengths[i]) || lengths[i] <= tol->dist)
	    return 0;
	if (mode != RT_CANONICALIZE_RIGID)
	    uniform_scale = fmax(uniform_scale, lengths[i]);
    }
    for (size_t i = 0; i < length_count; i++)
	canonical_lengths[i] = lengths[i] / uniform_scale;

    MAT_IDN(orientation);
    orientation[0] = x_axis[X];
    orientation[4] = x_axis[Y];
    orientation[8] = x_axis[Z];
    orientation[1] = y_axis[X];
    orientation[5] = y_axis[Y];
    orientation[9] = y_axis[Z];
    orientation[2] = z_axis[X];
    orientation[6] = z_axis[Y];
    orientation[10] = z_axis[Z];
    MAT_IDN(scale);
    scale[15] = 1.0 / uniform_scale;
    bn_mat_mul(oriented_scale, orientation, scale);
    MAT_IDN(translate);
    MAT_DELTAS_VEC(translate, origin);
    bn_mat_mul(canonical_to_input, translate, oriented_scale);
    return 1;
}
