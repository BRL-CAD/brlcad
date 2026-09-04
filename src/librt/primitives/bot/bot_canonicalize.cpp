/*           B O T _ C A N O N I C A L I Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "common.h"

#include <cmath>

#include "bg/pca.h"
#include "raytrace.h"
#include "rt/primitives/bot.h"


namespace {

constexpr size_t FRAME_AXIS_COUNT = 3;
constexpr size_t FRAME_PERMUTATION_COUNT = 6;

const size_t frame_permutations[FRAME_PERMUTATION_COUNT][FRAME_AXIS_COUNT] = {
    {0, 1, 2},
    {0, 2, 1},
    {1, 0, 2},
    {1, 2, 0},
    {2, 0, 1},
    {2, 1, 0}
};

const int frame_permutation_parity[FRAME_PERMUTATION_COUNT] = {
    1, -1, -1, 1, 1, -1
};


bool
bot_input_valid(const struct rt_bot_internal *bot)
{
    if (!bot || bot->num_vertices < 3 || !bot->vertices ||
	(bot->num_faces && !bot->faces))
	return false;

    for (size_t i = 0; i < bot->num_vertices; i++) {
	if (VINVALID(&bot->vertices[3 * i]))
	    return false;
    }
    for (size_t i = 0; i < 3 * bot->num_faces; i++) {
	if (bot->faces[i] < 0 || static_cast<size_t>(bot->faces[i]) >= bot->num_vertices)
	    return false;
    }

    return true;
}


void
proper_pca_frame(vect_t axes[FRAME_AXIS_COUNT])
{
    vect_t proper_z;

    VCROSS(proper_z, axes[X], axes[Y]);
    VUNITIZE(proper_z);
    VMOVE(axes[Z], proper_z);
}


bool
pca_frame_is_unique(const vect_t singular_values, size_t point_count,
		    const struct bn_tol *tol)
{
    fastf_t singular_tolerance = tol->dist * std::sqrt(static_cast<fastf_t>(point_count));

    return !NEAR_EQUAL(singular_values[X], singular_values[Y], singular_tolerance) &&
	!NEAR_EQUAL(singular_values[Y], singular_values[Z], singular_tolerance);
}


bool
anchor_frame(vect_t axes[FRAME_AXIS_COUNT], const point_t center,
	     const struct rt_bot_internal *bot, const struct bn_tol *tol)
{
    vect_t offset;
    vect_t residual;
    fastf_t farthest_sq = -1.0;
    fastf_t widest_sq = -1.0;
    size_t farthest = 0;
    size_t widest = 0;

    for (size_t i = 0; i < bot->num_vertices; i++) {
	VSUB2(offset, &bot->vertices[3 * i], center);
	fastf_t distance_sq = MAGSQ(offset);
	if (distance_sq > farthest_sq + tol->dist_sq) {
	    farthest_sq = distance_sq;
	    farthest = i;
	}
    }
    if (farthest_sq <= tol->dist_sq)
	return false;

    VSUB2(axes[X], &bot->vertices[3 * farthest], center);
    VUNITIZE(axes[X]);

    for (size_t i = 0; i < bot->num_vertices; i++) {
	VSUB2(offset, &bot->vertices[3 * i], center);
	VJOIN1(residual, offset, -VDOT(offset, axes[X]), axes[X]);
	fastf_t residual_sq = MAGSQ(residual);
	if (residual_sq > widest_sq + tol->dist_sq) {
	    widest_sq = residual_sq;
	    widest = i;
	}
    }
    if (widest_sq <= tol->dist_sq)
	return false;

    VSUB2(offset, &bot->vertices[3 * widest], center);
    VJOIN1(axes[Y], offset, -VDOT(offset, axes[X]), axes[X]);
    VUNITIZE(axes[Y]);
    VCROSS(axes[Z], axes[X], axes[Y]);
    VUNITIZE(axes[Z]);
    return true;
}


void
frame_matrix(mat_t matrix, const vect_t axes[FRAME_AXIS_COUNT],
	     const size_t permutation[FRAME_AXIS_COUNT], const int signs[FRAME_AXIS_COUNT],
	     const point_t center)
{
    mat_t rotate;
    mat_t translate;

    MAT_IDN(rotate);
    for (size_t row = 0; row < FRAME_AXIS_COUNT; row++)
	VSCALE(&rotate[4 * row], axes[permutation[row]], signs[row]);
    MAT_IDN(translate);
    MAT_DELTAS_VEC_NEG(translate, center);
    bn_mat_mul(matrix, rotate, translate);
}


fastf_t
comparison_coordinate(fastf_t coordinate, const struct bn_tol *tol)
{
    return NEAR_ZERO(coordinate, tol->dist) ? 0.0 : coordinate;
}


bool
candidate_is_less(const mat_t candidate, const mat_t current,
		  const struct rt_bot_internal *bot, const struct bn_tol *tol)
{
    point_t candidate_point;
    point_t current_point;

    for (size_t i = 0; i < bot->num_vertices; i++) {
	MAT4X3PNT(candidate_point, candidate, &bot->vertices[3 * i]);
	MAT4X3PNT(current_point, current, &bot->vertices[3 * i]);
	for (size_t coordinate = 0; coordinate < ELEMENTS_PER_POINT; coordinate++) {
	    fastf_t candidate_value = comparison_coordinate(candidate_point[coordinate], tol);
	    fastf_t current_value = comparison_coordinate(current_point[coordinate], tol);
	    if (candidate_value < current_value - tol->dist)
		return true;
	    if (candidate_value > current_value + tol->dist)
		return false;
	}
    }

    return false;
}

}


extern "C" int
rt_bot_canonicalize(struct rt_db_internal *canonical,
		    mat_t canonical_to_input,
		    const struct rt_db_internal *input,
		    const struct bn_tol *tol,
		    enum rt_canonicalize_mode mode)
{
    const struct rt_bot_internal *bot;
    struct rt_bot_internal *canonical_bot;
    struct rt_db_internal transformed;
    point_t center;
    vect_t axes[FRAME_AXIS_COUNT];
    vect_t singular_values;
    mat_t candidate;
    mat_t input_to_canonical;
    bool have_candidate = false;

    if (!canonical || !canonical_to_input || !input || !tol)
	return RT_CANONICALIZE_ERROR;
    if (mode < RT_CANONICALIZE_RIGID || mode > RT_CANONICALIZE_AFFINE)
	return RT_CANONICALIZE_ERROR;
    if (input->idb_type != ID_BOT)
	return RT_CANONICALIZE_ERROR;

    bot = static_cast<const struct rt_bot_internal *>(input->idb_ptr);
    RT_BOT_CK_MAGIC(bot);
    if (!bot_input_valid(bot))
	return RT_CANONICALIZE_ERROR;

    /* rt_bot_mat currently reconstructs only the common per-vertex normal
     * layout.  Decline other layouts until that transform contract is fixed. */
    if ((bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) &&
	bot->num_normals != bot->num_vertices)
	return RT_CANONICALIZE_UNSUPPORTED;

    if (bg_pca_svd(&center, &axes[X], &axes[Y], &axes[Z], &singular_values,
	bot->num_vertices, reinterpret_cast<const point_t *>(bot->vertices)) != BRLCAD_OK)
	return RT_CANONICALIZE_ERROR;

    if (pca_frame_is_unique(singular_values, bot->num_vertices, tol)) {
	proper_pca_frame(axes);
    } else if (!anchor_frame(axes, center, bot, tol)) {
	return RT_CANONICALIZE_ERROR;
    }

    /* PCA axis signs are arbitrary, and equivalent parameterizations may
     * permute axes.  Search all proper signed permutations and retain the
     * lexicographically smallest vertex sequence.  This preserves BOT index
     * topology while making pushed copies choose the same local frame. */
    for (size_t permutation = 0; permutation < FRAME_PERMUTATION_COUNT; permutation++) {
	for (int xsign = -1; xsign <= 1; xsign += 2) {
	    for (int ysign = -1; ysign <= 1; ysign += 2) {
		int signs[FRAME_AXIS_COUNT] = {
		    xsign,
		    ysign,
		    frame_permutation_parity[permutation] * xsign * ysign
		};
		frame_matrix(candidate, axes, frame_permutations[permutation], signs, center);
		if (!have_candidate || candidate_is_less(candidate, input_to_canonical, bot, tol)) {
		    MAT_COPY(input_to_canonical, candidate);
		    have_candidate = true;
		}
	    }
	}
    }

    canonical_bot = rt_bot_dup(bot);
    if (!canonical_bot)
	return RT_CANONICALIZE_ERROR;

    RT_DB_INTERNAL_INIT(&transformed);
    transformed.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    transformed.idb_minor_type = ID_BOT;
    transformed.idb_meth = &OBJ[ID_BOT];
    transformed.idb_ptr = canonical_bot;
    if (OBJ[ID_BOT].ft_mat(&transformed, input_to_canonical, input) != BRLCAD_OK) {
	rt_bot_internal_free(canonical_bot);
	return RT_CANONICALIZE_ERROR;
    }

    bn_mat_inv(canonical_to_input, input_to_canonical);
    canonical->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    canonical->idb_minor_type = ID_BOT;
    canonical->idb_meth = &OBJ[ID_BOT];
    canonical->idb_ptr = canonical_bot;
    return RT_CANONICALIZE_OK;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
