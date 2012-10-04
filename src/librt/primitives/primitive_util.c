/*               P R I M I T I V E _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2012 United States Government as represented by
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
/** @file primitive_util.c
 *
 * General utility routines for librt primitives that are not part of
 * the librt API. Prototypes for these routines are located in
 * librt_private.h.
 */

#include "../librt_private.h"

/**
 * If only the absolute tolerance is valid (positive), it is returned.
 *
 * If only the relative tolerance is valid (in (0.0, 1.0)), the return value
 * is the relative tolerance multiplied by rel_to_abs.
 *
 * If both tolerances are valid, the smaller is used.
 * If neither is valid, a default of .1 * rel_to_abs is used.
 */
#define DEFAULT_REL_TOL .1
fastf_t
primitive_get_absolute_tolerance(
	const struct rt_tess_tol *ttol,
	fastf_t rel_to_abs)
{
    fastf_t tol, rel_tol, abs_tol;
    int rel_tol_is_valid;

    rel_tol = ttol->rel;
    abs_tol = ttol->abs;

    rel_tol_is_valid = 0;
    if (rel_tol > 0.0 && rel_tol < 1.0) {
	rel_tol_is_valid = 1;
    }

    if (abs_tol > 0.0 && (!rel_tol_is_valid || rel_tol > abs_tol)) {
	tol = abs_tol;
    } else {
	if (!rel_tol_is_valid) {
	    rel_tol = DEFAULT_REL_TOL;
	}
	tol = rel_tol * rel_to_abs;
    }

    return tol;
}

/**
 * Gives a rough estimate of the maximum number of times a primitive's bounding
 * box diagonal will be sampled based on the sample density of the view.
 *
 * Practically, it is an estimate of the maximum number of pixels that would be
 * used if the diagonal line were drawn in the current view window.
 *
 * It is currently used in adaptive plot routines to help choose how many
 * sample points should be used for plotted curves.
 */
fastf_t
primitive_diagonal_samples(
	struct rt_db_internal *ip,
	const struct rt_view_info *info)
{
    point_t bbox_min, bbox_max;
    fastf_t primitive_diagonal_mm, samples_per_mm;
    fastf_t diagonal_samples;

    ip->idb_meth->ft_bbox(ip, &bbox_min, &bbox_max, info->tol);
    primitive_diagonal_mm = DIST_PT_PT(bbox_min, bbox_max);

    samples_per_mm = sqrt(info->view_samples) / info->view_size;
    diagonal_samples = samples_per_mm * primitive_diagonal_mm;

    return diagonal_samples;
}

