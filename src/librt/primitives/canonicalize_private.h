/*                 C A N O N I C A L I Z E _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef LIBRT_PRIMITIVES_CANONICALIZE_PRIVATE_H
#define LIBRT_PRIMITIVES_CANONICALIZE_PRIVATE_H

#include "common.h"

#include <stddef.h>

#include "rt/functab.h"


/* Construct a right-handed frame whose local X and Z axes follow two
 * perpendicular primitive directions.  Scalar-radius primitives cannot
 * represent a nonuniform affine transform in ft_mat, so affine requests use
 * the strongest exactly reconstructable subset: a similarity transform. */
int
rt_make_canonical_similarity_frame(mat_t canonical_to_input,
	fastf_t *canonical_lengths, const point_t origin,
	const vect_t input_x, const vect_t input_z, const fastf_t *lengths,
	size_t length_count, const struct bn_tol *tol,
	enum rt_canonicalize_mode mode);

#endif /* LIBRT_PRIMITIVES_CANONICALIZE_PRIVATE_H */
