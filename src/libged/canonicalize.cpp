/*                       C A N O N I C A L I Z E . C P P
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

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "bg/trimesh.h"
#include "rt/func.h"

#include "canonicalize_private.h"


namespace {

/* Matrix products in pushed models commonly subtract large translations to
 * recover small relative placements.  Thousands of machine epsilons admit
 * that accumulated roundoff without treating database-tolerance differences
 * as identical transforms. */
constexpr fastf_t MATRIX_ROUNDOFF_MULTIPLIER = 4096.0;
constexpr fastf_t MATRIX_METRIC_TOLERANCE_MULTIPLIER = 2.0;
constexpr fastf_t TWO_LENGTH_METRIC_TERM_COUNT = 2.0;
constexpr fastf_t THREE_LENGTH_METRIC_TERM_COUNT = 3.0;
constexpr fastf_t FOUR_LENGTH_METRIC_TERM_COUNT = 4.0;
constexpr fastf_t FIVE_LENGTH_METRIC_TERM_COUNT = 5.0;
constexpr fastf_t EIGHT_LENGTH_METRIC_TERM_COUNT = 8.0;
constexpr fastf_t BOT_METRIC_TOLERANCE_MULTIPLIER = 4.0;


bool
near_value(fastf_t a, fastf_t b, fastf_t tolerance)
{
    return std::fabs(a - b) <= tolerance;
}


fastf_t
matrix_roundoff_tolerance(fastf_t a, fastf_t b, fastf_t database_limit)
{
    const fastf_t epsilon = std::numeric_limits<fastf_t>::epsilon();
    fastf_t scale = std::max({1.0, std::fabs(a), std::fabs(b)});
    fastf_t roundoff_limit = MATRIX_ROUNDOFF_MULTIPLIER * epsilon * scale;
    return std::min(database_limit, roundoff_limit);
}


bool
bot_payload_equal(const struct rt_bot_internal *a,
		  const struct rt_bot_internal *b,
		  const struct bn_tol *tol)
{
    if (a->mode != b->mode || a->orientation != b->orientation ||
	a->bot_flags != b->bot_flags || a->num_vertices != b->num_vertices ||
	a->num_faces != b->num_faces || a->num_normals != b->num_normals ||
	a->num_face_normals != b->num_face_normals || a->num_uvs != b->num_uvs ||
	a->num_face_uvs != b->num_face_uvs)
	return false;

    if (bg_trimesh_diff(a->faces, a->num_faces,
	reinterpret_cast<const point_t *>(a->vertices), a->num_vertices,
	b->faces, b->num_faces, reinterpret_cast<const point_t *>(b->vertices),
	b->num_vertices, tol->dist))
	return false;

    if ((a->thickness == nullptr) != (b->thickness == nullptr) ||
	(a->face_mode == nullptr) != (b->face_mode == nullptr) ||
	(a->normals == nullptr) != (b->normals == nullptr) ||
	(a->face_normals == nullptr) != (b->face_normals == nullptr) ||
	(a->uvs == nullptr) != (b->uvs == nullptr) ||
	(a->face_uvs == nullptr) != (b->face_uvs == nullptr))
	return false;

    for (size_t i = 0; a->thickness && i < a->num_faces; i++) {
	if (!near_value(a->thickness[i], b->thickness[i], tol->dist))
	    return false;
    }
    for (size_t i = 0; a->face_mode && i < a->num_faces; i++) {
	if (BU_BITTEST(a->face_mode, i) != BU_BITTEST(b->face_mode, i))
	    return false;
    }
    for (size_t i = 0; a->normals && i < 3 * a->num_normals; i++) {
	if (!near_value(a->normals[i], b->normals[i], tol->perp))
	    return false;
    }
    for (size_t i = 0; a->face_normals && i < 3 * a->num_face_normals; i++) {
	if (a->face_normals[i] != b->face_normals[i])
	    return false;
    }
    for (size_t i = 0; a->uvs && i < 3 * a->num_uvs; i++) {
	if (!near_value(a->uvs[i], b->uvs[i], tol->dist))
	    return false;
    }
    for (size_t i = 0; a->face_uvs && i < 3 * a->num_face_uvs; i++) {
	if (a->face_uvs[i] != b->face_uvs[i])
	    return false;
    }

    return true;
}


bool
canonical_geometry_equal(const struct rt_db_internal *a,
			 const struct rt_db_internal *b,
			 const struct bn_tol *tol)
{
    if (a->idb_minor_type != b->idb_minor_type)
	return false;

    switch (a->idb_minor_type) {
	case ID_HALF: {
	    const auto *ahalf = static_cast<const struct rt_half_internal *>(a->idb_ptr);
	    const auto *bhalf = static_cast<const struct rt_half_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(ahalf->eqn, bhalf->eqn, tol->perp) &&
		near_value(ahalf->eqn[W], bhalf->eqn[W], tol->dist);
	}
	case ID_ELL:
	case ID_SPH: {
	    const auto *aell = static_cast<const struct rt_ell_internal *>(a->idb_ptr);
	    const auto *bell = static_cast<const struct rt_ell_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(aell->v, bell->v, tol->dist) &&
		VNEAR_EQUAL(aell->a, bell->a, tol->dist) &&
		VNEAR_EQUAL(aell->b, bell->b, tol->dist) &&
		VNEAR_EQUAL(aell->c, bell->c, tol->dist);
	}
	case ID_SUPERELL: {
	    const auto *as = static_cast<const struct rt_superell_internal *>(a->idb_ptr);
	    const auto *bs = static_cast<const struct rt_superell_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(as->v, bs->v, tol->dist) &&
		VNEAR_EQUAL(as->a, bs->a, tol->dist) &&
		VNEAR_EQUAL(as->b, bs->b, tol->dist) &&
		VNEAR_EQUAL(as->c, bs->c, tol->dist) &&
		near_value(as->n, bs->n, tol->perp) &&
		near_value(as->e, bs->e, tol->perp);
	}
	case ID_RPC: {
	    const auto *arpc = static_cast<const struct rt_rpc_internal *>(a->idb_ptr);
	    const auto *brpc = static_cast<const struct rt_rpc_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(arpc->rpc_V, brpc->rpc_V, tol->dist) &&
		VNEAR_EQUAL(arpc->rpc_B, brpc->rpc_B, tol->dist) &&
		VNEAR_EQUAL(arpc->rpc_H, brpc->rpc_H, tol->dist) &&
		near_value(arpc->rpc_r, brpc->rpc_r, tol->dist);
	}
	case ID_RHC: {
	    const auto *arhc = static_cast<const struct rt_rhc_internal *>(a->idb_ptr);
	    const auto *brhc = static_cast<const struct rt_rhc_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(arhc->rhc_V, brhc->rhc_V, tol->dist) &&
		VNEAR_EQUAL(arhc->rhc_B, brhc->rhc_B, tol->dist) &&
		VNEAR_EQUAL(arhc->rhc_H, brhc->rhc_H, tol->dist) &&
		near_value(arhc->rhc_r, brhc->rhc_r, tol->dist) &&
		near_value(arhc->rhc_c, brhc->rhc_c, tol->dist);
	}
	case ID_EPA: {
	    const auto *aepa = static_cast<const struct rt_epa_internal *>(a->idb_ptr);
	    const auto *bepa = static_cast<const struct rt_epa_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(aepa->epa_V, bepa->epa_V, tol->dist) &&
		VNEAR_EQUAL(aepa->epa_H, bepa->epa_H, tol->dist) &&
		VNEAR_EQUAL(aepa->epa_Au, bepa->epa_Au, tol->perp) &&
		near_value(aepa->epa_r1, bepa->epa_r1, tol->dist) &&
		near_value(aepa->epa_r2, bepa->epa_r2, tol->dist);
	}
	case ID_EHY: {
	    const auto *aehy = static_cast<const struct rt_ehy_internal *>(a->idb_ptr);
	    const auto *behy = static_cast<const struct rt_ehy_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(aehy->ehy_V, behy->ehy_V, tol->dist) &&
		VNEAR_EQUAL(aehy->ehy_H, behy->ehy_H, tol->dist) &&
		VNEAR_EQUAL(aehy->ehy_Au, behy->ehy_Au, tol->perp) &&
		near_value(aehy->ehy_r1, behy->ehy_r1, tol->dist) &&
		near_value(aehy->ehy_r2, behy->ehy_r2, tol->dist) &&
		near_value(aehy->ehy_c, behy->ehy_c, tol->dist);
	}
	case ID_PARTICLE: {
	    const auto *apart = static_cast<const struct rt_part_internal *>(a->idb_ptr);
	    const auto *bpart = static_cast<const struct rt_part_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(apart->part_V, bpart->part_V, tol->dist) &&
		VNEAR_EQUAL(apart->part_H, bpart->part_H, tol->dist) &&
		near_value(apart->part_vrad, bpart->part_vrad, tol->dist) &&
		near_value(apart->part_hrad, bpart->part_hrad, tol->dist);
	}
	case ID_TOR: {
	    const auto *ator = static_cast<const struct rt_tor_internal *>(a->idb_ptr);
	    const auto *btor = static_cast<const struct rt_tor_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(ator->v, btor->v, tol->dist) &&
		VNEAR_EQUAL(ator->h, btor->h, tol->perp) &&
		near_value(ator->r_a, btor->r_a, tol->dist) &&
		near_value(ator->r_h, btor->r_h, tol->dist);
	}
	case ID_ETO: {
	    const auto *aeto = static_cast<const struct rt_eto_internal *>(a->idb_ptr);
	    const auto *beto = static_cast<const struct rt_eto_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(aeto->eto_V, beto->eto_V, tol->dist) &&
		VNEAR_EQUAL(aeto->eto_N, beto->eto_N, tol->perp) &&
		VNEAR_EQUAL(aeto->eto_C, beto->eto_C, tol->dist) &&
		near_value(aeto->eto_r, beto->eto_r, tol->dist) &&
		near_value(aeto->eto_rd, beto->eto_rd, tol->dist);
	}
	case ID_TGC:
	case ID_REC: {
	    const auto *atgc = static_cast<const struct rt_tgc_internal *>(a->idb_ptr);
	    const auto *btgc = static_cast<const struct rt_tgc_internal *>(b->idb_ptr);
	    return VNEAR_EQUAL(atgc->v, btgc->v, tol->dist) &&
		VNEAR_EQUAL(atgc->h, btgc->h, tol->dist) &&
		VNEAR_EQUAL(atgc->a, btgc->a, tol->dist) &&
		VNEAR_EQUAL(atgc->b, btgc->b, tol->dist) &&
		VNEAR_EQUAL(atgc->c, btgc->c, tol->dist) &&
		VNEAR_EQUAL(atgc->d, btgc->d, tol->dist);
	}
	case ID_ARB8: {
	    const auto *aarb = static_cast<const struct rt_arb_internal *>(a->idb_ptr);
	    const auto *barb = static_cast<const struct rt_arb_internal *>(b->idb_ptr);
	    for (size_t i = 0; i < 8; i++) {
		if (!VNEAR_EQUAL(aarb->pt[i], barb->pt[i], tol->dist))
		    return false;
	    }
	    return true;
	}
	case ID_BOT:
	    return bot_payload_equal(static_cast<const struct rt_bot_internal *>(a->idb_ptr),
		static_cast<const struct rt_bot_internal *>(b->idb_ptr), tol);
	default:
	    return false;
    }
}


bool
make_transform_output(struct rt_db_internal *output,
		      const struct rt_db_internal *input)
{
    RT_DB_INTERNAL_INIT(output);
    output->idb_major_type = input->idb_major_type;
    output->idb_minor_type = input->idb_minor_type;
    output->idb_meth = input->idb_meth;

    switch (input->idb_minor_type) {
	case ID_ELL:
	case ID_SPH: {
	    BU_ALLOC(output->idb_ptr, struct rt_ell_internal);
	    auto *ell = static_cast<struct rt_ell_internal *>(output->idb_ptr);
	    ell->magic = RT_ELL_INTERNAL_MAGIC;
	    return true;
	}
	case ID_SUPERELL: {
	    BU_ALLOC(output->idb_ptr, struct rt_superell_internal);
	    auto *superell = static_cast<struct rt_superell_internal *>(
		output->idb_ptr);
	    superell->magic = RT_SUPERELL_INTERNAL_MAGIC;
	    return true;
	}
	case ID_RPC: {
	    BU_ALLOC(output->idb_ptr, struct rt_rpc_internal);
	    auto *rpc = static_cast<struct rt_rpc_internal *>(output->idb_ptr);
	    rpc->rpc_magic = RT_RPC_INTERNAL_MAGIC;
	    return true;
	}
	case ID_RHC: {
	    BU_ALLOC(output->idb_ptr, struct rt_rhc_internal);
	    auto *rhc = static_cast<struct rt_rhc_internal *>(output->idb_ptr);
	    rhc->rhc_magic = RT_RHC_INTERNAL_MAGIC;
	    return true;
	}
	case ID_EPA: {
	    BU_ALLOC(output->idb_ptr, struct rt_epa_internal);
	    auto *epa = static_cast<struct rt_epa_internal *>(output->idb_ptr);
	    epa->epa_magic = RT_EPA_INTERNAL_MAGIC;
	    return true;
	}
	case ID_EHY: {
	    BU_ALLOC(output->idb_ptr, struct rt_ehy_internal);
	    auto *ehy = static_cast<struct rt_ehy_internal *>(output->idb_ptr);
	    ehy->ehy_magic = RT_EHY_INTERNAL_MAGIC;
	    return true;
	}
	case ID_PARTICLE: {
	    BU_ALLOC(output->idb_ptr, struct rt_part_internal);
	    auto *part = static_cast<struct rt_part_internal *>(output->idb_ptr);
	    part->part_magic = RT_PART_INTERNAL_MAGIC;
	    return true;
	}
	case ID_HALF: {
	    BU_ALLOC(output->idb_ptr, struct rt_half_internal);
	    auto *half = static_cast<struct rt_half_internal *>(output->idb_ptr);
	    half->magic = RT_HALF_INTERNAL_MAGIC;
	    return true;
	}
	case ID_TOR: {
	    BU_ALLOC(output->idb_ptr, struct rt_tor_internal);
	    auto *tor = static_cast<struct rt_tor_internal *>(output->idb_ptr);
	    tor->magic = RT_TOR_INTERNAL_MAGIC;
	    return true;
	}
	case ID_ETO: {
	    BU_ALLOC(output->idb_ptr, struct rt_eto_internal);
	    auto *eto = static_cast<struct rt_eto_internal *>(output->idb_ptr);
	    eto->eto_magic = RT_ETO_INTERNAL_MAGIC;
	    return true;
	}
	case ID_TGC:
	case ID_REC: {
	    BU_ALLOC(output->idb_ptr, struct rt_tgc_internal);
	    auto *tgc = static_cast<struct rt_tgc_internal *>(output->idb_ptr);
	    tgc->magic = RT_TGC_INTERNAL_MAGIC;
	    return true;
	}
	case ID_ARB8: {
	    BU_ALLOC(output->idb_ptr, struct rt_arb_internal);
	    auto *arb = static_cast<struct rt_arb_internal *>(output->idb_ptr);
	    arb->magic = RT_ARB_INTERNAL_MAGIC;
	    return true;
	}
	case ID_BOT:
	    output->idb_ptr = rt_bot_dup(
		static_cast<const struct rt_bot_internal *>(input->idb_ptr));
	    return output->idb_ptr != nullptr;
	default:
	    return false;
    }
}


bool
primitive_bounds_equal(struct rt_db_internal *a, struct rt_db_internal *b,
		       const struct bn_tol *tol)
{
    if (!a->idb_meth->ft_bbox || !b->idb_meth->ft_bbox)
	return true;

    point_t a_minimum;
    point_t a_maximum;
    point_t b_minimum;
    point_t b_maximum;
    int a_status = a->idb_meth->ft_bbox(a, &a_minimum, &a_maximum, tol);
    int b_status = b->idb_meth->ft_bbox(b, &b_minimum, &b_maximum, tol);
    if (a_status != b_status)
	return false;
    if (a_status < 0)
	return true;

    return VNEAR_EQUAL(a_minimum, b_minimum, tol->dist) &&
	VNEAR_EQUAL(a_maximum, b_maximum, tol->dist);
}


void
ell_shape_matrix(fastf_t shape[9], const struct rt_ell_internal *ell)
{
    const fastf_t *axes[] = {ell->a, ell->b, ell->c};

    for (size_t row = 0; row < 3; row++) {
	for (size_t column = 0; column < 3; column++) {
	    shape[3 * row + column] = 0.0;
	    for (const fastf_t *axis : axes)
		shape[3 * row + column] += axis[row] * axis[column];
	}
    }
}


bool
shape_matrices_equal(const fastf_t a[9], const fastf_t b[9], fastf_t radius,
		     size_t axis_count, const struct bn_tol *tol)
{
    /* An ellipse is unchanged when its spanning axes are permuted, negated, or
     * rotated within an equal-radius subspace.  Comparing the sum of their
     * outer products recognizes those equivalent parameterizations.  If each
     * axis moves by at most d, one outer-product component moves by at most
     * 2*r*d + d^2. */
    fastf_t matrix_tolerance = axis_count *
	(2.0 * radius * tol->dist + tol->dist * tol->dist);
    for (size_t i = 0; i < 9; i++) {
	if (!near_value(a[i], b[i], matrix_tolerance))
	    return false;
    }
    return true;
}


void
axis_shape_matrix(fastf_t shape[9], const vect_t axis)
{
    for (size_t row = 0; row < 3; row++) {
	for (size_t column = 0; column < 3; column++)
	    shape[3 * row + column] = axis[row] * axis[column];
    }
}


bool
superell_geometry_equal(const struct rt_superell_internal *a,
			const struct rt_superell_internal *b,
			const struct bn_tol *tol)
{
    if (!VNEAR_EQUAL(a->v, b->v, tol->dist) ||
	!near_value(a->n, b->n, tol->perp) ||
	!near_value(a->e, b->e, tol->perp))
	return false;

    const fastf_t *a_axes[] = {a->a, a->b, a->c};
    const fastf_t *b_axes[] = {b->a, b->b, b->c};
    for (size_t i = 0; i < 3; i++) {
	fastf_t a_shape[9];
	fastf_t b_shape[9];
	axis_shape_matrix(a_shape, a_axes[i]);
	axis_shape_matrix(b_shape, b_axes[i]);
	fastf_t maximum_radius = std::max(MAGNITUDE(a_axes[i]),
	    MAGNITUDE(b_axes[i]));
	if (!shape_matrices_equal(a_shape, b_shape, maximum_radius, 1, tol))
	    return false;
    }
    return true;
}


void
tgc_section(point_t center, fastf_t shape[9],
	    const struct rt_tgc_internal *tgc, fastf_t parameter)
{
    vect_t axes[2];

    VJOIN1(center, tgc->v, parameter, tgc->h);
    VBLEND2(axes[0], 1.0 - parameter, tgc->a, parameter, tgc->c);
    VBLEND2(axes[1], 1.0 - parameter, tgc->b, parameter, tgc->d);
    for (size_t row = 0; row < 3; row++) {
	for (size_t column = 0; column < 3; column++) {
	    shape[3 * row + column] = axes[0][row] * axes[0][column] +
		axes[1][row] * axes[1][column];
	}
    }
}


bool
tgc_geometry_equal_direction(const struct rt_tgc_internal *a,
			     const struct rt_tgc_internal *b, bool reverse_b,
			     const struct bn_tol *tol)
{
    constexpr fastf_t SECTION_PARAMETERS[] = {0.0, 0.5, 1.0};
    fastf_t maximum_radius = std::max({MAGNITUDE(a->a), MAGNITUDE(a->b),
	MAGNITUDE(a->c), MAGNITUDE(a->d), MAGNITUDE(b->a), MAGNITUDE(b->b),
	MAGNITUDE(b->c), MAGNITUDE(b->d)});

    for (fastf_t parameter : SECTION_PARAMETERS) {
	point_t a_center;
	point_t b_center;
	fastf_t a_shape[9];
	fastf_t b_shape[9];
	fastf_t b_parameter = reverse_b ? 1.0 - parameter : parameter;
	tgc_section(a_center, a_shape, a, parameter);
	tgc_section(b_center, b_shape, b, b_parameter);
	if (!VNEAR_EQUAL(a_center, b_center, tol->dist) ||
	    !shape_matrices_equal(a_shape, b_shape, maximum_radius, 2, tol))
	    return false;
    }
    return true;
}


bool
part_geometry_equal(const struct rt_part_internal *a,
		    const struct rt_part_internal *b,
		    const struct bn_tol *tol)
{
    if (VNEAR_EQUAL(a->part_V, b->part_V, tol->dist) &&
	VNEAR_EQUAL(a->part_H, b->part_H, tol->dist) &&
	near_value(a->part_vrad, b->part_vrad, tol->dist) &&
	near_value(a->part_hrad, b->part_hrad, tol->dist))
	return true;

    point_t a_end;
    point_t b_end;
    VADD2(a_end, a->part_V, a->part_H);
    VADD2(b_end, b->part_V, b->part_H);
    return VNEAR_EQUAL(a->part_V, b_end, tol->dist) &&
	VNEAR_EQUAL(a_end, b->part_V, tol->dist) &&
	near_value(a->part_vrad, b->part_hrad, tol->dist) &&
	near_value(a->part_hrad, b->part_vrad, tol->dist);
}


bool
primitive_geometry_equal(const struct rt_db_internal *a,
			 const struct rt_db_internal *b,
			 const struct bn_tol *tol)
{
    if (a->idb_minor_type != b->idb_minor_type)
	return false;

    switch (a->idb_minor_type) {
	case ID_HALF: {
	    const auto *ahalf = static_cast<const struct rt_half_internal *>(a->idb_ptr);
	    const auto *bhalf = static_cast<const struct rt_half_internal *>(b->idb_ptr);
	    fastf_t a_magnitude = MAGNITUDE(ahalf->eqn);
	    fastf_t b_magnitude = MAGNITUDE(bhalf->eqn);
	    if (a_magnitude <= SMALL_FASTF || b_magnitude <= SMALL_FASTF)
		return false;
	    vect_t a_normal;
	    vect_t b_normal;
	    VSCALE(a_normal, ahalf->eqn, 1.0 / a_magnitude);
	    VSCALE(b_normal, bhalf->eqn, 1.0 / b_magnitude);
	    return VNEAR_EQUAL(a_normal, b_normal, tol->perp) &&
		near_value(ahalf->eqn[W] / a_magnitude,
		    bhalf->eqn[W] / b_magnitude, tol->dist);
	}
	case ID_ELL:
	case ID_SPH: {
	    const auto *aell = static_cast<const struct rt_ell_internal *>(a->idb_ptr);
	    const auto *bell = static_cast<const struct rt_ell_internal *>(b->idb_ptr);
	    if (!VNEAR_EQUAL(aell->v, bell->v, tol->dist))
		return false;
	    fastf_t a_shape[9];
	    fastf_t b_shape[9];
	    ell_shape_matrix(a_shape, aell);
	    ell_shape_matrix(b_shape, bell);
	    fastf_t maximum_radius = std::max({MAGNITUDE(aell->a), MAGNITUDE(aell->b),
		MAGNITUDE(aell->c), MAGNITUDE(bell->a), MAGNITUDE(bell->b),
		MAGNITUDE(bell->c)});
	    return shape_matrices_equal(a_shape, b_shape, maximum_radius, 3, tol);
	}
	case ID_SUPERELL:
	    return superell_geometry_equal(
		static_cast<const struct rt_superell_internal *>(a->idb_ptr),
		static_cast<const struct rt_superell_internal *>(b->idb_ptr), tol);
	case ID_RPC:
	case ID_RHC:
	case ID_EPA:
	case ID_EHY:
	    return canonical_geometry_equal(a, b, tol);
	case ID_PARTICLE:
	    return part_geometry_equal(
		static_cast<const struct rt_part_internal *>(a->idb_ptr),
		static_cast<const struct rt_part_internal *>(b->idb_ptr), tol);
	case ID_TOR: {
	    const auto *ator = static_cast<const struct rt_tor_internal *>(a->idb_ptr);
	    const auto *btor = static_cast<const struct rt_tor_internal *>(b->idb_ptr);
	    vect_t a_normal;
	    vect_t b_normal;
	    VMOVE(a_normal, ator->h);
	    VMOVE(b_normal, btor->h);
	    VUNITIZE(a_normal);
	    VUNITIZE(b_normal);
	    return VNEAR_EQUAL(ator->v, btor->v, tol->dist) &&
		near_value(std::fabs(VDOT(a_normal, b_normal)), 1.0, tol->perp) &&
		near_value(ator->r_a, btor->r_a, tol->dist) &&
		near_value(ator->r_h, btor->r_h, tol->dist);
	}
	case ID_ETO: {
	    const auto *aeto = static_cast<const struct rt_eto_internal *>(a->idb_ptr);
	    const auto *beto = static_cast<const struct rt_eto_internal *>(b->idb_ptr);
	    vect_t a_normal;
	    vect_t b_normal;
	    VMOVE(a_normal, aeto->eto_N);
	    VMOVE(b_normal, beto->eto_N);
	    VUNITIZE(a_normal);
	    VUNITIZE(b_normal);
	    return VNEAR_EQUAL(aeto->eto_V, beto->eto_V, tol->dist) &&
		near_value(std::fabs(VDOT(a_normal, b_normal)), 1.0, tol->perp) &&
		near_value(MAGNITUDE(aeto->eto_C), MAGNITUDE(beto->eto_C), tol->dist) &&
		near_value(std::fabs(VDOT(aeto->eto_C, a_normal)),
		    std::fabs(VDOT(beto->eto_C, b_normal)), tol->dist) &&
		near_value(aeto->eto_r, beto->eto_r, tol->dist) &&
		near_value(aeto->eto_rd, beto->eto_rd, tol->dist);
	}
	case ID_TGC:
	case ID_REC: {
	    const auto *atgc = static_cast<const struct rt_tgc_internal *>(a->idb_ptr);
	    const auto *btgc = static_cast<const struct rt_tgc_internal *>(b->idb_ptr);
	    return tgc_geometry_equal_direction(atgc, btgc, false, tol) ||
		tgc_geometry_equal_direction(atgc, btgc, true, tol);
	}
	case ID_ARB8:
	case ID_BOT:
	    return canonical_geometry_equal(a, b, tol);
	default:
	    return false;
    }
}

} /* namespace */


extern "C" fastf_t
_ged_matrix_roundoff_tolerance(fastf_t a, fastf_t b,
			       fastf_t database_limit)
{
    return matrix_roundoff_tolerance(a, b, database_limit);
}


extern "C" int
_ged_matrices_numerically_equal(const mat_t a, const mat_t b,
				const struct bn_tol *tol)
{
    if (!a || !b || !tol)
	return 0;

    for (size_t i = 0; i < 16; i++) {
	fastf_t database_limit = (i == 3 || i == 7 || i == 11) ?
	    tol->dist : tol->perp;
	if (!near_value(a[i], b[i], matrix_roundoff_tolerance(a[i], b[i],
		database_limit)))
	    return 0;
    }
    return 1;
}


extern "C" int
_ged_canonical_geometry_equal(const struct rt_db_internal *a,
			      const struct rt_db_internal *b,
			      const struct bn_tol *tol)
{
    return a && b && tol && a->idb_ptr && b->idb_ptr &&
	canonical_geometry_equal(a, b, tol);
}


extern "C" int
_ged_canonical_geometry_metric(const struct rt_db_internal *object,
	const struct bn_tol *tol, fastf_t *metric, fastf_t *metric_tolerance)
{
    if (!object || !object->idb_ptr || !tol || !metric || !metric_tolerance)
	return 0;
    *metric = 0.0;
    *metric_tolerance = 0.0;

    /* Each analytic metric is a sum of vector magnitudes or scalar lengths.
     * Its tolerance is therefore the number of terms times the database
     * distance tolerance.  Objects accepted by the full comparison must then
     * overlap in a one-dimensional metric sweep. */
    switch (object->idb_minor_type) {
	case ID_HALF:
	    return 1;
	case ID_ELL:
	case ID_SPH: {
	    const auto *ell = static_cast<const struct rt_ell_internal *>(object->idb_ptr);
	    *metric = MAGNITUDE(ell->a) + MAGNITUDE(ell->b) + MAGNITUDE(ell->c);
	    *metric_tolerance = THREE_LENGTH_METRIC_TERM_COUNT * tol->dist;
	    return 1;
	}
	case ID_SUPERELL: {
	    const auto *superell = static_cast<const struct rt_superell_internal *>(
		object->idb_ptr);
	    *metric = MAGNITUDE(superell->a) + MAGNITUDE(superell->b) +
		MAGNITUDE(superell->c);
	    *metric_tolerance = THREE_LENGTH_METRIC_TERM_COUNT * tol->dist;
	    return 1;
	}
	case ID_RPC: {
	    const auto *rpc = static_cast<const struct rt_rpc_internal *>(object->idb_ptr);
	    *metric = MAGNITUDE(rpc->rpc_B) + MAGNITUDE(rpc->rpc_H) +
		std::fabs(rpc->rpc_r);
	    *metric_tolerance = THREE_LENGTH_METRIC_TERM_COUNT * tol->dist;
	    return 1;
	}
	case ID_RHC: {
	    const auto *rhc = static_cast<const struct rt_rhc_internal *>(object->idb_ptr);
	    *metric = MAGNITUDE(rhc->rhc_B) + MAGNITUDE(rhc->rhc_H) +
		std::fabs(rhc->rhc_r) + std::fabs(rhc->rhc_c);
	    *metric_tolerance = FOUR_LENGTH_METRIC_TERM_COUNT * tol->dist;
	    return 1;
	}
	case ID_EPA: {
	    const auto *epa = static_cast<const struct rt_epa_internal *>(object->idb_ptr);
	    *metric = MAGNITUDE(epa->epa_H) + std::fabs(epa->epa_r1) +
		std::fabs(epa->epa_r2);
	    *metric_tolerance = THREE_LENGTH_METRIC_TERM_COUNT * tol->dist;
	    return 1;
	}
	case ID_EHY: {
	    const auto *ehy = static_cast<const struct rt_ehy_internal *>(object->idb_ptr);
	    *metric = MAGNITUDE(ehy->ehy_H) + std::fabs(ehy->ehy_r1) +
		std::fabs(ehy->ehy_r2) + std::fabs(ehy->ehy_c);
	    *metric_tolerance = FOUR_LENGTH_METRIC_TERM_COUNT * tol->dist;
	    return 1;
	}
	case ID_PARTICLE: {
	    const auto *part = static_cast<const struct rt_part_internal *>(object->idb_ptr);
	    *metric = MAGNITUDE(part->part_H) + std::fabs(part->part_vrad) +
		std::fabs(part->part_hrad);
	    *metric_tolerance = THREE_LENGTH_METRIC_TERM_COUNT * tol->dist;
	    return 1;
	}
	case ID_TOR: {
	    const auto *tor = static_cast<const struct rt_tor_internal *>(object->idb_ptr);
	    *metric = std::fabs(tor->r_a) + std::fabs(tor->r_h);
	    *metric_tolerance = TWO_LENGTH_METRIC_TERM_COUNT * tol->dist;
	    return 1;
	}
	case ID_ETO: {
	    const auto *eto = static_cast<const struct rt_eto_internal *>(object->idb_ptr);
	    *metric = MAGNITUDE(eto->eto_C) + std::fabs(eto->eto_r) +
		std::fabs(eto->eto_rd);
	    *metric_tolerance = THREE_LENGTH_METRIC_TERM_COUNT * tol->dist;
	    return 1;
	}
	case ID_TGC:
	case ID_REC: {
	    const auto *tgc = static_cast<const struct rt_tgc_internal *>(object->idb_ptr);
	    *metric = MAGNITUDE(tgc->h) + MAGNITUDE(tgc->a) + MAGNITUDE(tgc->b) +
		MAGNITUDE(tgc->c) + MAGNITUDE(tgc->d);
	    *metric_tolerance = FIVE_LENGTH_METRIC_TERM_COUNT * tol->dist;
	    return 1;
	}
	case ID_ARB8: {
	    const auto *arb = static_cast<const struct rt_arb_internal *>(object->idb_ptr);
	    for (const point_t &point : arb->pt)
		*metric += MAGNITUDE(point);
	    *metric_tolerance = EIGHT_LENGTH_METRIC_TERM_COUNT * tol->dist;
	    return 1;
	}
	case ID_BOT: {
	    const auto *bot = static_cast<const struct rt_bot_internal *>(object->idb_ptr);
	    if (!bot->num_vertices)
		return 1;
	    const point_t *vertices = reinterpret_cast<const point_t *>(bot->vertices);
	    point_t minimum;
	    point_t maximum;
	    VMOVE(minimum, vertices[0]);
	    VMOVE(maximum, vertices[0]);
	    for (size_t i = 1; i < bot->num_vertices; i++)
		VMINMAX(minimum, maximum, vertices[i]);
	    vect_t extent;
	    VSUB2(extent, maximum, minimum);
	    *metric = MAGNITUDE(extent);
	    *metric_tolerance = BOT_METRIC_TOLERANCE_MULTIPLIER * tol->dist;
	    return 1;
	}
	default:
	    return 0;
    }
}


extern "C" int
_ged_primitive_geometry_equal(const struct rt_db_internal *a,
			      const struct rt_db_internal *b,
			      const struct bn_tol *tol)
{
    return a && b && tol && a->idb_ptr && b->idb_ptr &&
	primitive_geometry_equal(a, b, tol);
}


extern "C" int
_ged_transform_primitive(struct rt_db_internal *output, const mat_t matrix,
			 const struct rt_db_internal *input)
{
    if (!output || !matrix || !input || !input->idb_ptr || output->idb_ptr ||
	!input->idb_meth || !input->idb_meth->ft_mat)
	return BRLCAD_ERROR;
    if (!make_transform_output(output, input))
	return BRLCAD_ERROR;
    if (input->idb_meth->ft_mat(output, matrix, input) == BRLCAD_OK)
	return BRLCAD_OK;

    rt_db_free_internal(output);
    RT_DB_INTERNAL_INIT(output);
    return BRLCAD_ERROR;
}


extern "C" int
_ged_transformed_geometry_equal(const struct rt_db_internal *a,
	const mat_t a_matrix, const struct rt_db_internal *b,
	const mat_t b_matrix, const struct bn_tol *tol)
{
    if (!a || !a_matrix || !b || !b_matrix || !tol)
	return 0;

    struct rt_db_internal transformed_a;
    struct rt_db_internal transformed_b;
    RT_DB_INTERNAL_INIT(&transformed_a);
    RT_DB_INTERNAL_INIT(&transformed_b);
    if (_ged_transform_primitive(&transformed_a, a_matrix, a) != BRLCAD_OK)
	return 0;
    if (_ged_transform_primitive(&transformed_b, b_matrix, b) != BRLCAD_OK) {
	rt_db_free_internal(&transformed_a);
	return 0;
    }

    int equal = primitive_geometry_equal(&transformed_a, &transformed_b, tol) &&
	primitive_bounds_equal(&transformed_a, &transformed_b, tol);
    rt_db_free_internal(&transformed_b);
    rt_db_free_internal(&transformed_a);
    return equal;
}


namespace {

struct combination_analysis_state {
    struct db_i *dbip = nullptr;
    const struct bn_tol *tol = nullptr;
    const std::map<struct directory *, GedCanonicalChildInfo> *primitive_children =
	nullptr;
    std::map<struct directory *, size_t> record_indices;
    std::set<struct directory *> active;
    std::vector<GedCanonicalCombinationRecord> records;
    std::map<std::string, std::multimap<fastf_t, size_t>> representatives;
    size_t next_identity = 1;
    size_t failures = 0;
    bool include_metadata = true;
};


void
append_key_field(std::string &key, const char *value, size_t length)
{
    key += std::to_string(length);
    key.push_back(':');
    if (length)
	key.append(value, length);
    key.push_back(';');
}


void
append_key_string(std::string &key, const char *value)
{
    key.push_back(value ? '1' : '0');
    if (value)
	append_key_field(key, value, std::strlen(value));
    else
	key.push_back(';');
}


std::string
combination_metadata_key(const struct rt_comb_internal *comb,
			 const struct bu_attribute_value_set *attributes,
			 const std::string &topology)
{
    std::string key;
    auto append_integer = [&](long value) {
	std::string text = std::to_string(value);
	append_key_field(key, text.c_str(), text.size());
    };

    append_integer(comb->region_flag);
    append_integer(comb->is_fastgen);
    append_integer(comb->region_id);
    append_integer(comb->aircode);
    append_integer(comb->GIFTmater);
    append_integer(comb->los);
    append_integer(comb->rgb_valid);
    for (unsigned char component : comb->rgb)
	append_integer(component);
    append_key_field(key, reinterpret_cast<const char *>(&comb->temperature),
	sizeof(comb->temperature));
    append_key_string(key, bu_vls_cstr(&comb->shader));
    append_key_string(key, bu_vls_cstr(&comb->material));
    append_integer(comb->inherit);

    std::vector<const struct bu_attribute_value_pair *> sorted_attributes;
    sorted_attributes.reserve(attributes->count);
    for (size_t i = 0; i < attributes->count; i++)
	sorted_attributes.push_back(&attributes->avp[i]);
    std::sort(sorted_attributes.begin(), sorted_attributes.end(),
	[](const struct bu_attribute_value_pair *a,
	   const struct bu_attribute_value_pair *b) {
	    const char *a_name = a->name ? a->name : "";
	    const char *b_name = b->name ? b->name : "";
	    return std::strcmp(a_name, b_name) < 0;
	});
    append_integer(static_cast<long>(sorted_attributes.size()));
    for (const struct bu_attribute_value_pair *attribute : sorted_attributes) {
	append_key_string(key, attribute->name);
	append_key_string(key, attribute->value);
#if defined(USE_BINARY_ATTRIBUTES)
	append_key_field(key, reinterpret_cast<const char *>(attribute->binvalue),
	    attribute->binvaluelen);
#endif
    }
    append_key_field(key, topology.c_str(), topology.size());
    return key;
}


bool analyze_combination(combination_analysis_state &state,
			 struct directory *dp, size_t &record_index);


bool
describe_combination_tree(combination_analysis_state &state,
			  const union tree *tree, std::string &topology,
			  std::vector<GedCanonicalCombinationLeaf> &leaves)
{
    if (!tree)
	return false;

    RT_CK_TREE(tree);
    topology.push_back('(');
    topology += std::to_string(tree->tr_op);
    topology.push_back(':');
    switch (tree->tr_op) {
	case OP_DB_LEAF: {
	    struct directory *child = db_lookup(state.dbip, tree->tr_l.tl_name,
		LOOKUP_QUIET);
	    if (child == RT_DIR_NULL)
		return false;

	    GedCanonicalChildInfo child_info;
	    char child_kind;
	    if (child->d_flags & RT_DIR_COMB) {
		size_t child_record_index;
		if (!analyze_combination(state, child, child_record_index))
		    return false;
		const GedCanonicalCombinationRecord &child_record =
		    state.records[child_record_index];
		child_info.identity = child_record.identity;
		mat_t child_reference_matrix;
		mat_t child_representative_matrix;
		if (tree->tr_l.tl_mat)
		    MAT_COPY(child_reference_matrix, tree->tr_l.tl_mat);
		else
		    MAT_IDN(child_reference_matrix);
		bn_mat_mul(child_representative_matrix, child_reference_matrix,
		    child_record.representative_to_input);
		bn_mat_mul(child_info.placement, child_representative_matrix,
		    child_record.identity_placement);
		child_kind = 'C';
	    } else {
		auto primitive = state.primitive_children->find(child);
		if (primitive == state.primitive_children->end())
		    return false;
		child_info = primitive->second;
		child_kind = 'P';
	    }

	    GedCanonicalCombinationLeaf leaf;
	    leaf.child_identity = child_info.identity;
	    if (child_kind == 'C') {
		MAT_COPY(leaf.effective_matrix, child_info.placement);
	    } else {
		mat_t leaf_matrix;
		if (tree->tr_l.tl_mat)
		    MAT_COPY(leaf_matrix, tree->tr_l.tl_mat);
		else
		    MAT_IDN(leaf_matrix);
		bn_mat_mul(leaf.effective_matrix, leaf_matrix,
		    child_info.placement);
	    }
	    leaves.push_back(leaf);
	    topology.push_back(child_kind);
	    topology += std::to_string(child_info.identity);
	    break;
	}
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    if (!describe_combination_tree(state, tree->tr_b.tb_left, topology,
		    leaves) ||
		!describe_combination_tree(state, tree->tr_b.tb_right, topology,
		    leaves))
		return false;
	    break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    if (!describe_combination_tree(state, tree->tr_b.tb_left, topology,
		    leaves))
		return false;
	    break;
	case OP_NOP:
	    break;
	default:
	    return false;
    }
    topology.push_back(')');
    return true;
}


bool
combination_records_equal(
    const GedCanonicalCombinationRecord &representative,
    const GedCanonicalCombinationRecord &candidate, const struct bn_tol *tol)
{
    if (representative.bucket != candidate.bucket ||
	representative.leaves.size() != candidate.leaves.size())
	return false;

    for (size_t i = 0; i < representative.leaves.size(); i++) {
	if (representative.leaves[i].child_identity !=
		candidate.leaves[i].child_identity ||
	    !_ged_matrices_numerically_equal(
		representative.leaves[i].relative_matrix,
		candidate.leaves[i].relative_matrix, tol))
	    return false;

	mat_t reconstructed;
	bn_mat_mul(reconstructed, candidate.placement,
	    representative.leaves[i].relative_matrix);
	if (!_ged_matrices_numerically_equal(reconstructed,
		candidate.leaves[i].effective_matrix, tol))
	    return false;
    }
    return true;
}


bool
analyze_combination(combination_analysis_state &state,
		    struct directory *dp, size_t &record_index)
{
    auto existing = state.record_indices.find(dp);
    if (existing != state.record_indices.end()) {
	record_index = existing->second;
	return state.records[record_index].valid;
    }
    if (!state.active.insert(dp).second)
	return false;

    GedCanonicalCombinationRecord record;
    record.dp = dp;
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    bool valid = (dp->d_flags & RT_DIR_COMB) &&
	rt_db_get_internal(&intern, dp, state.dbip, nullptr) >= 0 &&
	intern.idb_minor_type == ID_COMBINATION;
    std::string topology;
    if (valid) {
	const auto *comb = static_cast<const struct rt_comb_internal *>(intern.idb_ptr);
	RT_CK_COMB(comb);
	record.region = comb->region_flag != 0;
	valid = describe_combination_tree(state, comb->tree, topology,
	    record.leaves) && !record.leaves.empty();
	if (valid)
	    record.bucket = state.include_metadata ?
		combination_metadata_key(comb, &intern.idb_avs, topology) :
		topology;
    }

    if (valid) {
	mat_t input_to_canonical;
	MAT_COPY(record.placement, record.leaves.front().effective_matrix);
	valid = bn_mat_inverse(input_to_canonical, record.placement) != 0;
	if (valid) {
	    for (GedCanonicalCombinationLeaf &leaf : record.leaves) {
		bn_mat_mul(leaf.relative_matrix, input_to_canonical,
		    leaf.effective_matrix);
		for (size_t i = 0; i < 16; i++) {
		    fastf_t value = leaf.relative_matrix[i];
		    fastf_t database_limit = (i == 3 || i == 7 || i == 11) ?
			state.tol->dist : state.tol->perp;
		    record.metric += std::fabs(value);
		    record.metric_tolerance +=
			MATRIX_METRIC_TOLERANCE_MULTIPLIER *
			_ged_matrix_roundoff_tolerance(value, value,
			    database_limit);
		}
	    }
	}
    }

    if (intern.idb_ptr)
	rt_db_free_internal(&intern);
    state.active.erase(dp);

    bool new_identity = true;
    if (valid) {
	auto representatives = state.representatives.find(record.bucket);
	if (representatives != state.representatives.end()) {
	    auto candidate_begin = representatives->second.lower_bound(
		record.metric - record.metric_tolerance);
	    auto candidate_end = representatives->second.upper_bound(
		record.metric + record.metric_tolerance);
	    for (auto candidate = candidate_begin; candidate != candidate_end;
		    ++candidate) {
		const GedCanonicalCombinationRecord &representative =
		    state.records[candidate->second];
		if (combination_records_equal(representative, record, state.tol)) {
		    mat_t identity_to_representative;
		    if (!bn_mat_inverse(identity_to_representative,
			    representative.identity_placement)) {
			valid = false;
			break;
		    }
		    record.identity = representative.identity;
		    MAT_COPY(record.identity_placement,
			representative.identity_placement);
		    bn_mat_mul(record.representative_to_input, record.placement,
			identity_to_representative);
		    new_identity = false;
		    break;
		}
	    }
	}
	if (valid && new_identity) {
	    record.identity = state.next_identity++;
	    MAT_COPY(record.identity_placement, record.placement);
	    MAT_IDN(record.representative_to_input);
	}
	record.valid = valid;
    }
    if (!valid)
	state.failures++;

    record_index = state.records.size();
    state.records.push_back(record);
    state.record_indices[dp] = record_index;
    if (valid && new_identity)
	state.representatives[record.bucket].emplace(record.metric, record_index);
    return valid;
}

} /* namespace */


int
_ged_canonical_combination_analysis(
    GedCanonicalCombinationAnalysis *analysis, struct db_i *dbip,
    const std::vector<struct directory *> &combinations,
    const std::map<struct directory *, GedCanonicalChildInfo> &primitive_children,
    const struct bn_tol *tol, bool include_metadata)
{
    if (!analysis || !dbip || !tol)
	return BRLCAD_ERROR;

    combination_analysis_state state;
    state.dbip = dbip;
    state.tol = tol;
    state.primitive_children = &primitive_children;
    state.include_metadata = include_metadata;

    std::vector<struct directory *> ordered_combinations = combinations;
    std::sort(ordered_combinations.begin(), ordered_combinations.end(),
	[](const struct directory *a, const struct directory *b) {
	    if (!a)
		return b != nullptr;
	    if (!b)
		return false;
	    return std::strcmp(a->d_namep, b->d_namep) < 0;
	});
    for (struct directory *combination : ordered_combinations) {
	if (!combination)
	    continue;
	size_t record_index;
	(void)analyze_combination(state, combination, record_index);
    }

    std::map<size_t, std::vector<size_t>> identity_records;
    for (size_t i = 0; i < state.records.size(); i++) {
	if (state.records[i].valid)
	    identity_records[state.records[i].identity].push_back(i);
    }

    analysis->records = std::move(state.records);
    analysis->groups.clear();
    analysis->failures = state.failures;
    for (auto &identity : identity_records) {
	if (identity.second.size() < 2)
	    continue;
	GedCanonicalCombinationGroup group;
	group.records = std::move(identity.second);
	analysis->groups.push_back(std::move(group));
    }
    return BRLCAD_OK;
}
