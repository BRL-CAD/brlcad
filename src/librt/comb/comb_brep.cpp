/*                    C O M B _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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
/** @file comb_brep.cpp
 *
 * Convert a combination to b-rep form (with NURBS boolean evaluations)
 *
 */

#include "common.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "raytrace.h"
#include "bg/trimesh.h"
#include "bn/mat.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/process.h"
#include "bu/str.h"
#include "rt/geom.h"
#include "rt/comb.h"
#include "rt/primitives/pipe.h"
#include "nmg.h"
#include "brep.h"

#include "../librt_private.h"

static const fastf_t BREP_BOOLEAN_REGULARIZATION_SCALES[] = {0.20, 0.50, 0.90};
static const unsigned int BREP_BOOLEAN_REGULARIZATION_JITTER_STEPS = 10;
static const fastf_t BREP_BOOLEAN_REGULARIZATION_JITTER_SPAN = 0.10;
static const double BREP_BOOLEAN_REGULARIZATION_ANGLE_TOLERANCE =
    ON_PI / 1800.0;
static const fastf_t BREP_BOOLEAN_HALFSPACE_INTERIOR_WITNESS_DISTANCE = 1.0;
static const double BREP_BOOLEAN_HALFSPACE_MARGIN_MULTIPLIER = 2.0;
static const double BREP_BOOLEAN_TRANSFORM_APPROXIMATION_TOLERANCE_SCALE = 0.5;
/* Legacy ARBs are commonly rounded enough to make nominally planar faces
 * fail the absolute modeling tolerance.  Accept a repaired ARB only when the
 * maximum vertex displacement is both a small fraction of its own span and
 * absolutely bounded in model space.  rt_arb_repair independently limits the
 * convex-hull volume change to two percent. */
static const fastf_t BREP_BOOLEAN_ARB_REPAIR_RELATIVE_LIMIT = 0.005;
static const fastf_t BREP_BOOLEAN_ARB_REPAIR_TOLERANCE_MULTIPLIER = 200.0;


const char *
rt_brep_failure_stage_description(enum rt_brep_failure_stage stage)
{
    switch (stage) {
	case RT_BREP_FAILURE_STAGE_NONE:
	    return "none";
	case RT_BREP_FAILURE_STAGE_TREE_STRUCTURE:
	    return "tree_structure";
	case RT_BREP_FAILURE_STAGE_LEAF_LOOKUP:
	    return "leaf_lookup";
	case RT_BREP_FAILURE_STAGE_LEAF_IMPORT:
	    return "leaf_import";
	case RT_BREP_FAILURE_STAGE_LEAF_CONVERSION:
	    return "leaf_conversion";
	case RT_BREP_FAILURE_STAGE_LEAF_TRANSFORM:
	    return "leaf_transform";
	case RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION:
	    return "boolean_evaluation";
	case RT_BREP_FAILURE_STAGE_OUTPUT_VALIDATION:
	    return "output_validation";
	default:
	    return "unknown";
    }
}


const char *
rt_brep_failure_reason_description(enum rt_brep_failure_reason reason)
{
    switch (reason) {
	case RT_BREP_FAILURE_REASON_NONE:
	    return "none";
	case RT_BREP_FAILURE_REASON_INVALID_TREE:
	    return "invalid_tree";
	case RT_BREP_FAILURE_REASON_MISSING_OBJECT:
	    return "missing_object";
	case RT_BREP_FAILURE_REASON_INVALID_OBJECT:
	    return "invalid_object";
	case RT_BREP_FAILURE_REASON_UNSUPPORTED_OBJECT:
	    return "unsupported_object";
	case RT_BREP_FAILURE_REASON_INVALID_TRANSFORM:
	    return "invalid_transform";
	case RT_BREP_FAILURE_REASON_BOOLEAN_INDETERMINATE:
	    return "boolean_indeterminate";
	case RT_BREP_FAILURE_REASON_WORK_BUDGET:
	    return "work_budget";
	case RT_BREP_FAILURE_REASON_OUTPUT_INVALID:
	    return "output_invalid";
	case RT_BREP_FAILURE_REASON_OUTPUT_NOT_SOLID:
	    return "output_not_solid";
	case RT_BREP_FAILURE_REASON_INTERNAL_ERROR:
	    return "internal_error";
	default:
	    return "unknown";
    }
}


enum BooleanRegularizationBound {
    BREP_BOOLEAN_REGULARIZATION_INNER,
    BREP_BOOLEAN_REGULARIZATION_OUTER
};


/* Internal-only result.  A halfspace has no finite B-Rep by itself, but an
 * enclosing intersection may be able to evaluate the other operand first and
 * supply the finite clipping context it needs. */
static const int BREP_BOOLEAN_FINITE_CONTEXT_REQUIRED = -1000;


static bool
boolean_status_retryable(int status)
{
    return status == BREP_BOOLEAN_FAILURE ||
	status == BREP_BOOLEAN_INDETERMINATE;
}


static int
converted_brep_status(const ON_Brep *brep, const char *source)
{
    if (brep == NULL) {
	bu_log("%s B-Rep conversion produced no result.\n", source);
	return BREP_BOOLEAN_UNSUPPORTED;
    }
    if (brep->m_F.Count() == 0) {
	bu_log("%s B-Rep conversion produced no faces.\n", source);
	return BREP_BOOLEAN_INDETERMINATE;
    }
    ON_wString validation_text;
    ON_TextLog validation_log(validation_text);
    if (!brep->IsValid(&validation_log)) {
	const ON_String message(validation_text);
	bu_log("%s B-Rep conversion produced invalid topology%s%s.\n", source,
	    message.Array() && message.Array()[0] ? ": " : "",
	    message.Array() ? message.Array() : "");
	return BREP_BOOLEAN_INDETERMINATE;
    }
    if (!brep->IsSolid()) {
	bu_log("%s B-Rep conversion produced a non-solid result.\n", source);
	return BREP_BOOLEAN_INDETERMINATE;
    }
    return BREP_BOOLEAN_SUCCESS;
}


/* BRL-CAD matrices encode uniform scale in the homogeneous divisor at MSA.
 * OpenNURBS applies a general projective matrix directly to control points,
 * but several analytic surface implementations transform their vector data
 * without that division.  Convert the BRL-CAD representation to an equivalent
 * conventional affine matrix so points and vectors receive the same scale. */
static bool
brlcad_mat_affine_candidate(ON_Xform &xform, fastf_t *determinant,
	vect_t projective, const mat_t matrix)
{
    if (matrix == NULL || determinant == NULL || projective == NULL ||
	!std::isfinite(matrix[MSA]) || ZERO(matrix[MSA]))
    {
	return false;
    }

    const fastf_t homogeneous_scale = matrix[MSA];
    mat_t affine = MAT_INIT_IDN;
    for (size_t i = 0; i < 12; ++i) {
	affine[i] = matrix[i] / homogeneous_scale;
	if (!std::isfinite(affine[i])) {
	    return false;
	}
    }
    for (size_t i = 12; i < MSA; ++i) {
	const fastf_t projective_term = matrix[i] / homogeneous_scale;
	if (!std::isfinite(projective_term)) {
	    return false;
	}
	projective[i - 12] = projective_term;
    }

    *determinant = bn_mat_det3(affine);
    xform = ON_Xform(affine);
    if (!std::isfinite(*determinant) || !xform.IsValid() ||
	!xform.IsAffine() || xform.Rank() != 4)
    {
	return false;
    }
    return true;
}


static bool
brlcad_mat_to_opennurbs_affine(ON_Xform &xform, fastf_t *determinant,
	const mat_t matrix)
{
    vect_t projective;
    if (!brlcad_mat_affine_candidate(xform, determinant, projective,
	    matrix))
    {
	return false;
    }
    return ON_NearZero(projective[X], ON_ZERO_TOLERANCE) &&
	ON_NearZero(projective[Y], ON_ZERO_TOLERANCE) &&
	ON_NearZero(projective[Z], ON_ZERO_TOLERANCE);
}


/* Bound the point displacement caused by dropping a legacy matrix's
 * numerically nonzero projective row.  After normalization by MSA, the exact
 * map is A(p)/(1 + q.p), while the affine replacement is A(p). */
static bool
brlcad_mat_to_bounded_opennurbs_affine(ON_Xform &xform,
	fastf_t *determinant, fastf_t *model_error, const mat_t matrix,
	const ON_BoundingBox &source_bounds, fastf_t downstream_expansion,
	const struct bn_tol &tol)
{
    if (model_error == NULL || !source_bounds.IsValid() ||
	!std::isfinite(downstream_expansion) || !(downstream_expansion > 0.0))
    {
	return false;
    }
    *model_error = 0.0;

    vect_t projective;
    if (!brlcad_mat_affine_candidate(xform, determinant, projective,
	    matrix))
    {
	return false;
    }

    const double coordinate_extent[3] = {
	std::max(fabs(source_bounds.m_min.x), fabs(source_bounds.m_max.x)),
	std::max(fabs(source_bounds.m_min.y), fabs(source_bounds.m_max.y)),
	std::max(fabs(source_bounds.m_min.z), fabs(source_bounds.m_max.z))
    };
    double denominator_variation = 0.0;
    for (size_t coordinate = 0; coordinate < 3; ++coordinate) {
	denominator_variation += fabs(projective[coordinate]) *
	    coordinate_extent[coordinate];
    }
    if (!std::isfinite(denominator_variation) ||
	denominator_variation >= 1.0)
    {
	return false;
    }
    if (denominator_variation == 0.0) {
	return true;
    }

    double maximum_output_coordinate[3] = {0.0, 0.0, 0.0};
    for (size_t row = 0; row < 3; ++row) {
	double interval_minimum = xform.m_xform[row][3];
	double interval_maximum = xform.m_xform[row][3];
	for (size_t column = 0; column < 3; ++column) {
	    const double coefficient = xform.m_xform[row][column];
	    const double source_minimum = source_bounds.m_min[
		static_cast<int>(column)];
	    const double source_maximum = source_bounds.m_max[
		static_cast<int>(column)];
	    interval_minimum += coefficient >= 0.0 ?
		coefficient * source_minimum : coefficient * source_maximum;
	    interval_maximum += coefficient >= 0.0 ?
		coefficient * source_maximum : coefficient * source_minimum;
	}
	if (!std::isfinite(interval_minimum) ||
	    !std::isfinite(interval_maximum))
	{
	    return false;
	}
	maximum_output_coordinate[row] = std::max(fabs(interval_minimum),
	    fabs(interval_maximum));
    }
    const double maximum_output_magnitude = std::hypot(
	maximum_output_coordinate[X], std::hypot(
	    maximum_output_coordinate[Y], maximum_output_coordinate[Z]));
    const double local_error = maximum_output_magnitude *
	denominator_variation / (1.0 - denominator_variation);
    const double bounded_model_error = local_error *
	std::max(1.0, static_cast<double>(downstream_expansion));
    const double allowed_error =
	BREP_BOOLEAN_TRANSFORM_APPROXIMATION_TOLERANCE_SCALE *
	(tol.dist > SMALL_FASTF ? tol.dist : BN_TOL_DIST);
    if (!std::isfinite(bounded_model_error) ||
	bounded_model_error > allowed_error || bounded_model_error > MAX_FASTF)
    {
	return false;
    }

    *model_error = static_cast<fastf_t>(bounded_model_error);
    return true;
}


/* Return a conservative upper bound on the amount by which the affine
 * portion of a BRL-CAD matrix can enlarge a displacement.  The spectral norm
 * is bounded by sqrt(||A||_1 ||A||_inf); unlike the Frobenius norm this gives
 * exactly one for an identity transform. */
static bool
brlcad_mat_expansion_bound(fastf_t *expansion, const mat_t matrix)
{
    if (expansion == NULL)
	return false;
    if (matrix == NULL || bn_mat_is_identity(matrix)) {
	*expansion = 1.0;
	return true;
    }
    ON_Xform affine_transform;
    fastf_t determinant = 0.0;
    vect_t projective;
    if (!brlcad_mat_affine_candidate(affine_transform, &determinant,
	    projective, matrix))
	return false;

    double row_norm = 0.0;
    double column_norm = 0.0;
    for (size_t row = 0; row < 3; ++row) {
	double row_sum = 0.0;
	for (size_t column = 0; column < 3; ++column) {
	    const double coefficient = std::fabs(
		affine_transform.m_xform[row][column]);
	    if (!std::isfinite(coefficient))
		return false;
	    row_sum += coefficient;
	}
	row_norm = std::max(row_norm, row_sum);
    }
    for (size_t column = 0; column < 3; ++column) {
	double column_sum = 0.0;
	for (size_t row = 0; row < 3; ++row)
	    column_sum += std::fabs(
		affine_transform.m_xform[row][column]);
	column_norm = std::max(column_norm, column_sum);
    }

    const double bound = std::sqrt(row_norm * column_norm);
    if (!std::isfinite(bound) || !(bound > 0.0) || bound > MAX_FASTF)
	return false;
    *expansion = static_cast<fastf_t>(bound);
    return true;
}


static bool
combined_expansion_bound(fastf_t *combined, fastf_t downstream_expansion,
	const mat_t matrix)
{
    if (combined == NULL || !std::isfinite(downstream_expansion) ||
	!(downstream_expansion > 0.0))
    {
	return false;
    }
    fastf_t local_expansion = 1.0;
    if (!brlcad_mat_expansion_bound(&local_expansion, matrix))
	return false;
    const double product = static_cast<double>(downstream_expansion) *
	static_cast<double>(local_expansion);
    if (!std::isfinite(product) || !(product > 0.0) || product > MAX_FASTF)
	return false;
    *combined = static_cast<fastf_t>(product);
    return true;
}


static fastf_t
boolean_regularization_tolerance(const struct bn_tol &tol)
{
    return tol.dist > SMALL_FASTF ? tol.dist : BN_TOL_DIST;
}


static BooleanRegularizationBound
opposite_regularization_bound(BooleanRegularizationBound bound)
{
    return bound == BREP_BOOLEAN_REGULARIZATION_INNER ?
	BREP_BOOLEAN_REGULARIZATION_OUTER :
	BREP_BOOLEAN_REGULARIZATION_INNER;
}


static double
halfspace_proxy_margin(
    const ON_BoundingBox &bounds,
    const struct bn_tol &tol)
{
    double coordinate_scale = 1.0;
    coordinate_scale = std::max(coordinate_scale, std::fabs(bounds.m_min.x));
    coordinate_scale = std::max(coordinate_scale, std::fabs(bounds.m_min.y));
    coordinate_scale = std::max(coordinate_scale, std::fabs(bounds.m_min.z));
    coordinate_scale = std::max(coordinate_scale, std::fabs(bounds.m_max.x));
    coordinate_scale = std::max(coordinate_scale, std::fabs(bounds.m_max.y));
    coordinate_scale = std::max(coordinate_scale, std::fabs(bounds.m_max.z));

    const double margin = BREP_BOOLEAN_HALFSPACE_MARGIN_MULTIPLIER *
	std::max(static_cast<double>(boolean_regularization_tolerance(tol)),
	    ON_EPSILON * coordinate_scale);
    return std::isfinite(margin) && margin > 0.0 ? margin : 0.0;
}


static bool
halfspace_plane(
    point_t plane_point,
    vect_t outward_normal,
    const struct rt_half_internal *halfspace,
    const mat_t transform,
    fastf_t offset)
{
    if (halfspace == NULL) {
	return false;
    }

    VMOVE(outward_normal, halfspace->eqn);
    const fastf_t normal_length = MAGNITUDE(outward_normal);
    if (!std::isfinite(normal_length) || normal_length <= SMALL_FASTF) {
	return false;
    }

    const fastf_t inverse_normal_length = 1.0 / normal_length;
    const fastf_t distance = (halfspace->eqn[W] + offset) *
	inverse_normal_length;
    if (!std::isfinite(distance)) {
	return false;
    }
    VSCALE(outward_normal, outward_normal, inverse_normal_length);
    VSCALE(plane_point, outward_normal, distance);

    if (transform == NULL || bn_mat_is_identity(transform)) {
	return true;
    }

    vect_t tangent_u, tangent_v, interior_offset;
    bn_vec_perp(tangent_u, outward_normal);
    const fastf_t tangent_u_length = MAGNITUDE(tangent_u);
    if (!std::isfinite(tangent_u_length) || tangent_u_length <= SMALL_FASTF) {
	return false;
    }
    VSCALE(tangent_u, tangent_u, 1.0 / tangent_u_length);
    VCROSS(tangent_v, outward_normal, tangent_u);
    const fastf_t tangent_v_length = MAGNITUDE(tangent_v);
    if (!std::isfinite(tangent_v_length) || tangent_v_length <= SMALL_FASTF) {
	return false;
    }
    VSCALE(tangent_v, tangent_v, 1.0 / tangent_v_length);
    VSCALE(interior_offset, outward_normal,
	BREP_BOOLEAN_HALFSPACE_INTERIOR_WITNESS_DISTANCE);

    point_t tangent_u_point, tangent_v_point, interior_point;
    VADD2(tangent_u_point, plane_point, tangent_u);
    VADD2(tangent_v_point, plane_point, tangent_v);
    VSUB2(interior_point, plane_point, interior_offset);

    point_t transformed_plane_point, transformed_tangent_u_point;
    point_t transformed_tangent_v_point, transformed_interior_point;
    MAT4X3PNT(transformed_plane_point, transform, plane_point);
    MAT4X3PNT(transformed_tangent_u_point, transform, tangent_u_point);
    MAT4X3PNT(transformed_tangent_v_point, transform, tangent_v_point);
    MAT4X3PNT(transformed_interior_point, transform, interior_point);

    vect_t transformed_tangent_u, transformed_tangent_v;
    vect_t transformed_interior_offset;
    VSUB2(transformed_tangent_u, transformed_tangent_u_point,
	transformed_plane_point);
    VSUB2(transformed_tangent_v, transformed_tangent_v_point,
	transformed_plane_point);
    VSUB2(transformed_interior_offset, transformed_interior_point,
	transformed_plane_point);
    VCROSS(outward_normal, transformed_tangent_u, transformed_tangent_v);
    const fastf_t transformed_normal_length = MAGNITUDE(outward_normal);
    if (!std::isfinite(transformed_normal_length) ||
	transformed_normal_length <= SMALL_FASTF)
    {
	return false;
    }
    VSCALE(outward_normal, outward_normal, 1.0 / transformed_normal_length);
    if (VDOT(outward_normal, transformed_interior_offset) > 0.0) {
	VSCALE(outward_normal, outward_normal, -1.0);
    }
    VMOVE(plane_point, transformed_plane_point);
    return true;
}


static void
halfspace_proxy_corner(
    point_t corner,
    const point_t plane_point,
    const vect_t tangent_u,
    const vect_t tangent_v,
    const vect_t outward_normal,
    double u_coordinate,
    double v_coordinate,
    double normal_coordinate)
{
    vect_t u_offset, v_offset, normal_offset;
    VSCALE(u_offset, tangent_u, u_coordinate);
    VSCALE(v_offset, tangent_v, v_coordinate);
    VSCALE(normal_offset, outward_normal, normal_coordinate);
    VADD2(corner, plane_point, u_offset);
    VADD2(corner, corner, v_offset);
    VADD2(corner, corner, normal_offset);
}


struct BooleanRegularization {
    explicit BooleanRegularization(fastf_t unit_scale)
	: m_unit_scale(unit_scale),
	  m_maximum_magnitude(0.0),
	  m_supported(true)
	{}

    fastf_t
    Factor(
	const struct bn_tol &tol,
	BooleanRegularizationBound bound,
	const std::string &instance_key,
	fastf_t model_expansion = 1.0)
    {
	if (!std::isfinite(model_expansion) || !(model_expansion > 0.0)) {
	    MarkUnsupported();
	    return 0.0;
	}
	/* Use the complete tree-occurrence path and Boolean role rather than
	 * evaluation order.  A subtree may be visited more than once (notably
	 * while constructing a regularized XOR), and every visit to one logical
	 * primitive instance must receive the same offset.  FNV-1a is small,
	 * deterministic across processes, and sufficient for selecting one of
	 * the deliberately coarse jitter slots. */
	uint64_t hash = UINT64_C(1469598103934665603);
	for (const unsigned char character : instance_key) {
	    hash ^= character;
	    hash *= UINT64_C(1099511628211);
	}
	hash ^= bound == BREP_BOOLEAN_REGULARIZATION_INNER ?
	    static_cast<unsigned char>('i') : static_cast<unsigned char>('o');
	hash *= UINT64_C(1099511628211);
	const fastf_t tolerance = boolean_regularization_tolerance(tol);
	const fastf_t jitter = static_cast<fastf_t>(hash %
	    BREP_BOOLEAN_REGULARIZATION_JITTER_STEPS) /
	    static_cast<fastf_t>(BREP_BOOLEAN_REGULARIZATION_JITTER_STEPS);
	const fastf_t model_magnitude = tolerance *
	    (m_unit_scale + BREP_BOOLEAN_REGULARIZATION_JITTER_SPAN * jitter);
	const fastf_t magnitude = model_magnitude /
	    std::max(static_cast<fastf_t>(1.0), model_expansion);
	if (!std::isfinite(magnitude) || !(magnitude > 0.0)) {
	    MarkUnsupported();
	    return 0.0;
	}
	const fastf_t bounded_model_magnitude = magnitude * model_expansion;
	m_maximum_magnitude = std::max(m_maximum_magnitude,
	    bounded_model_magnitude);
	return bound == BREP_BOOLEAN_REGULARIZATION_INNER ?
	    -magnitude : magnitude;
    }

    bool
    IsSupported() const
    {
	return m_supported;
    }

    void
    MarkUnsupported()
    {
	m_supported = false;
    }

    fastf_t
    MaximumMagnitude() const
    {
	return m_maximum_magnitude;
    }

    fastf_t
    UnitScale() const
    {
	return m_unit_scale;
    }

private:
	fastf_t m_unit_scale;
	fastf_t m_maximum_magnitude;
	bool m_supported;
};


struct BooleanConversionState {
    fastf_t maximum_regularization = 0.0;
    fastf_t maximum_primitive_approximation = 0.0;
    fastf_t maximum_transform_approximation = 0.0;
    unsigned int regularization_attempts = 0;
    unsigned int whole_tree_regularization_attempts = 0;
    unsigned int repaired_arb_count = 0;
    const union tree *root_tree = NULL;
    std::set<const struct directory *> active_combinations;
    struct rt_brep_failure_info failure = RT_BREP_FAILURE_INFO_INIT;
    bool capture_failure_snapshot = false;
};


static std::atomic<uint64_t> failure_snapshot_serial(0);


static void
initialize_failure_info(struct rt_brep_failure_info &failure)
{
    const struct rt_brep_failure_info initialized = RT_BREP_FAILURE_INFO_INIT;
    failure = initialized;
}


static enum rt_brep_failure_reason
failure_reason_for_status(int status,
    enum rt_brep_failure_reason fallback)
{
    switch (status) {
	case BREP_BOOLEAN_BUDGET_EXCEEDED:
	    return RT_BREP_FAILURE_REASON_WORK_BUDGET;
	case BREP_BOOLEAN_INTERNAL_ERROR:
	    return RT_BREP_FAILURE_REASON_INTERNAL_ERROR;
	default:
	    break;
    }
    if (fallback != RT_BREP_FAILURE_REASON_NONE)
	return fallback;
    return status == BREP_BOOLEAN_UNSUPPORTED ?
	RT_BREP_FAILURE_REASON_UNSUPPORTED_OBJECT :
	RT_BREP_FAILURE_REASON_INVALID_OBJECT;
}


static int
snapshot_tree_operation(op_type operation)
{
    switch (operation) {
	case BOOLEAN_UNION:
	    return OP_UNION;
	case BOOLEAN_INTERSECT:
	    return OP_INTERSECT;
	case BOOLEAN_DIFF:
	    return OP_SUBTRACT;
	case BOOLEAN_XOR:
	    return OP_XOR;
	default:
	    return OP_NOP;
    }
}


static int
tree_boolean_operation(int operation)
{
    switch (operation) {
	case OP_UNION:
	    return BOOLEAN_UNION;
	case OP_INTERSECT:
	    return BOOLEAN_INTERSECT;
	case OP_SUBTRACT:
	    return BOOLEAN_DIFF;
	case OP_XOR:
	    return BOOLEAN_XOR;
	default:
	    return -1;
    }
}


static size_t
brep_naked_edge_count(const ON_Brep *brep)
{
    if (brep == NULL)
	return 0;
    size_t count = 0;
    for (int edge_index = 0; edge_index < brep->m_E.Count(); ++edge_index) {
	if (brep->m_E[edge_index].TrimCount() != 2)
	    ++count;
    }
    return count;
}


static enum rt_brep_failure_reason
brep_failure_reason(const ON_Brep *brep,
    enum rt_brep_failure_reason fallback)
{
    if (brep == NULL || brep->m_F.Count() == 0)
	return fallback;
    if (!brep->IsValid())
	return RT_BREP_FAILURE_REASON_OUTPUT_INVALID;
    if (!brep->IsSolid())
	return RT_BREP_FAILURE_REASON_OUTPUT_NOT_SOLID;
    return fallback;
}


static bool
write_snapshot_brep(struct db_i *dbip, const char *name,
    const ON_Brep *brep)
{
    if (dbip == NULL || name == NULL || name[0] == '\0' || brep == NULL)
	return false;

    struct rt_brep_internal *brep_internal;
    BU_ALLOC(brep_internal, struct rt_brep_internal);
    brep_internal->magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal->brep = ON_Brep::New(*brep);
    if (brep_internal->brep == NULL) {
	bu_free(brep_internal, "failure snapshot B-Rep internal");
	return false;
    }

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_BREP;
    internal.idb_minor_type = ID_BREP;
    internal.idb_meth = &OBJ[ID_BREP];
    internal.idb_ptr = brep_internal;
    struct directory *entry = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0,
	RT_DIR_SOLID, (void *)&internal.idb_type);
    if (entry == RT_DIR_NULL || rt_db_put_internal(entry, dbip, &internal) < 0) {
	if (entry == RT_DIR_NULL)
	    rt_db_free_internal(&internal);
	return false;
    }
    return true;
}


static union tree *
snapshot_tree_leaf(const char *name)
{
    union tree *leaf;
    BU_GET(leaf, union tree);
    RT_TREE_INIT(leaf);
    leaf->tr_l.tl_op = OP_DB_LEAF;
    leaf->tr_l.tl_name = bu_strdup(name);
    leaf->tr_l.tl_mat = NULL;
    return leaf;
}


static bool
write_snapshot_combination(struct db_i *dbip, op_type operation,
    const struct rt_brep_failure_info &failure, const struct bn_tol *tol)
{
    if (dbip == NULL)
	return false;
    const int tree_operation = snapshot_tree_operation(operation);
    if (tree_operation == OP_NOP)
	return false;

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_COMBINATION;
    internal.idb_minor_type = ID_COMBINATION;
    internal.idb_meth = &OBJ[ID_COMBINATION];
    struct rt_comb_internal *combination;
    BU_ALLOC(combination, struct rt_comb_internal);
    RT_COMB_INTERNAL_INIT(combination);
    combination->region_flag = 1;
    union tree *root;
    BU_GET(root, union tree);
    RT_TREE_INIT(root);
    root->tr_op = tree_operation;
    root->tr_b.tb_left = snapshot_tree_leaf("left.brep");
    root->tr_b.tb_right = snapshot_tree_leaf("right.brep");
    root->tr_b.tb_regionp = REGION_NULL;
    combination->tree = root;
    internal.idb_ptr = combination;

    char value[128] = {0};
    (void)bu_avs_add(&internal.idb_avs, "brep_failure_format", "1");
    (void)bu_avs_add(&internal.idb_avs, "brep_failure_tree_path",
	failure.tree_path);
    (void)bu_avs_add(&internal.idb_avs, "brep_failure_stage",
	rt_brep_failure_stage_description(failure.stage));
    (void)bu_avs_add(&internal.idb_avs, "brep_failure_reason",
	rt_brep_failure_reason_description(failure.reason));
    snprintf(value, sizeof(value), "%d", failure.status);
    (void)bu_avs_add(&internal.idb_avs, "brep_failure_status", value);
    snprintf(value, sizeof(value), "%d", failure.operation);
    (void)bu_avs_add(&internal.idb_avs, "brep_failure_operation", value);
    (void)bu_avs_add(&internal.idb_avs, "brep_boolean_failure_stage",
	ON_BooleanFailureStageDescription(failure.boolean_failure.stage));
    (void)bu_avs_add(&internal.idb_avs, "brep_boolean_failure_reason",
	ON_BooleanFailureReasonDescription(failure.boolean_failure.reason));
    snprintf(value, sizeof(value), "%d", failure.boolean_failure.face_index);
    (void)bu_avs_add(&internal.idb_avs, "brep_boolean_failure_face", value);
    snprintf(value, sizeof(value), "%d",
	failure.boolean_failure.peer_face_index);
    (void)bu_avs_add(&internal.idb_avs, "brep_boolean_failure_peer_face",
	value);
    snprintf(value, sizeof(value), "%d", failure.boolean_failure.edge_index);
    (void)bu_avs_add(&internal.idb_avs, "brep_boolean_failure_edge", value);
    snprintf(value, sizeof(value), "%d",
	failure.boolean_failure.peer_edge_index);
    (void)bu_avs_add(&internal.idb_avs, "brep_boolean_failure_peer_edge",
	value);
    snprintf(value, sizeof(value), "%zu",
	failure.boolean_failure.intersection_candidates);
    (void)bu_avs_add(&internal.idb_avs,
	"brep_boolean_failure_intersection_candidates", value);
    snprintf(value, sizeof(value), "%zu",
	failure.boolean_failure.intersection_events);
    (void)bu_avs_add(&internal.idb_avs,
	"brep_boolean_failure_intersection_events", value);
    snprintf(value, sizeof(value), "%zu",
	failure.boolean_failure.projection_failures);
    (void)bu_avs_add(&internal.idb_avs,
	"brep_boolean_failure_projection_failures", value);
    snprintf(value, sizeof(value), "%zu",
	failure.boolean_failure.output_naked_edges);
    (void)bu_avs_add(&internal.idb_avs,
	"brep_boolean_failure_output_naked_edges", value);
    snprintf(value, sizeof(value), "%zu",
	failure.boolean_failure.output_nonmanifold_edges);
    (void)bu_avs_add(&internal.idb_avs,
	"brep_boolean_failure_output_nonmanifold_edges", value);
    snprintf(value, sizeof(value), "%zu",
	failure.boolean_failure.ambiguous_edge_matches);
    (void)bu_avs_add(&internal.idb_avs,
	"brep_boolean_failure_ambiguous_edge_matches", value);
    snprintf(value, sizeof(value), "%.17g",
	failure.boolean_failure.tolerance);
    (void)bu_avs_add(&internal.idb_avs, "brep_boolean_failure_tolerance",
	value);
    snprintf(value, sizeof(value), "%.17g", tol ? tol->dist : BN_TOL_DIST);
    (void)bu_avs_add(&internal.idb_avs, "brep_failure_tolerance_dist", value);
    snprintf(value, sizeof(value), "%.17g", tol ? tol->perp : SMALL_FASTF);
    (void)bu_avs_add(&internal.idb_avs, "brep_failure_tolerance_perp", value);
    snprintf(value, sizeof(value), "%.17g", tol ? tol->para : 1.0 - SMALL_FASTF);
    (void)bu_avs_add(&internal.idb_avs, "brep_failure_tolerance_para", value);

    struct directory *entry = db_diradd(dbip, "failure.r",
	RT_DIR_PHONY_ADDR, 0, RT_DIR_COMB | RT_DIR_REGION,
	(void *)&internal.idb_type);
    if (entry == RT_DIR_NULL || rt_db_put_internal(entry, dbip, &internal) < 0) {
	if (entry == RT_DIR_NULL)
	    rt_db_free_internal(&internal);
	return false;
    }
    return true;
}


static bool
write_failure_snapshot(struct rt_brep_failure_info &failure,
    const ON_Brep *left, const ON_Brep *right, op_type operation,
    const struct bn_tol *tol)
{
    const char *directory = getenv("LIBRT_BREP_BOOLEAN_FAILURE_DIR");
    if (directory == NULL || directory[0] == '\0' ||
	BU_STR_EQUAL(directory, "0") || left == NULL || right == NULL)
    {
	return false;
    }
    if (!bu_file_directory(directory)) {
	bu_mkdir(directory);
	if (!bu_file_directory(directory)) {
	    bu_log("Unable to create B-Rep Boolean failure snapshot directory %s.\n",
		directory);
	    return false;
	}
    }

    std::string name;
    char path[MAXPATHLEN] = {0};
    for (size_t attempt = 0; attempt < 100; ++attempt) {
	const uint64_t serial = failure_snapshot_serial.fetch_add(1,
	    std::memory_order_relaxed) + 1;
	char candidate[RT_BREP_FAILURE_SNAPSHOT_NAME_MAX] = {0};
	snprintf(candidate, sizeof(candidate), "brep_boolean_failure_%d_%llu.g",
	    bu_pid(), static_cast<unsigned long long>(serial));
	if (!bu_dir(path, sizeof(path), directory, candidate, NULL))
	    return false;
	if (bu_file_exists(path, NULL))
	    continue;
	name = candidate;
	break;
    }
    if (name.empty())
	return false;

    struct db_i *snapshot = db_create(path, BRLCAD_DB_FORMAT_LATEST);
    bool success = snapshot != DBI_NULL;
    if (success)
	success = db_update_ident(snapshot,
	    "BRL-CAD NURBS Boolean failure replay", 1.0) >= 0;
    if (success)
	success = write_snapshot_brep(snapshot, "left.brep", left);
    if (success)
	success = write_snapshot_brep(snapshot, "right.brep", right);
    if (success)
	success = write_snapshot_combination(snapshot, operation, failure, tol);
    if (snapshot != DBI_NULL)
	db_close(snapshot);
    if (!success) {
	(void)bu_file_delete(path);
	bu_log("Unable to write B-Rep Boolean failure snapshot %s.\n", path);
	return false;
    }

    bu_strlcpy(failure.snapshot_name, name.c_str(),
	sizeof(failure.snapshot_name));
    bu_log("B-Rep Boolean failure snapshot: %s (replay object failure.r).\n",
	path);
    return true;
}


static void
record_tree_failure(BooleanConversionState &state,
    enum rt_brep_failure_stage stage, enum rt_brep_failure_reason reason,
    int status, const std::string &tree_path, const char *object_name,
    int operation, BooleanRegularization *regularization,
    const ON_Brep *left = NULL, const ON_Brep *right = NULL,
    const ON_Brep *output = NULL, const struct bn_tol *tol = NULL,
    const struct brep_boolean_failure_info *boolean_failure = NULL)
{
    if (state.failure.stage != RT_BREP_FAILURE_STAGE_NONE)
	return;
    initialize_failure_info(state.failure);
    state.failure.stage = stage;
    state.failure.reason = failure_reason_for_status(status, reason);
    state.failure.status = status;
    state.failure.operation = operation;
    state.failure.regularized = regularization != NULL ? 1 : 0;
    state.failure.regularization_scale = regularization != NULL ?
	regularization->UnitScale() : 0.0;
    state.failure.left_faces = left ? left->m_F.Count() : 0;
    state.failure.left_edges = left ? left->m_E.Count() : 0;
    state.failure.right_faces = right ? right->m_F.Count() : 0;
    state.failure.right_edges = right ? right->m_E.Count() : 0;
    state.failure.output_faces = output ? output->m_F.Count() : 0;
    state.failure.output_edges = output ? output->m_E.Count() : 0;
    state.failure.output_naked_edges = brep_naked_edge_count(output);
    if (boolean_failure != NULL)
	state.failure.boolean_failure = *boolean_failure;
    bu_strlcpy(state.failure.tree_path, tree_path.c_str(),
	sizeof(state.failure.tree_path));
    if (object_name != NULL)
	bu_strlcpy(state.failure.object_name, object_name,
	    sizeof(state.failure.object_name));

    if (state.capture_failure_snapshot &&
	stage == RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION &&
	operation >= BOOLEAN_UNION && operation <= BOOLEAN_XOR)
    {
	(void)write_failure_snapshot(state.failure, left, right,
	    static_cast<op_type>(operation), tol);
    }
}


static int
evaluate_tree_boolean(ON_Brep *result, const ON_Brep *left,
    const ON_Brep *right, op_type operation, const std::string &tree_path,
    const struct bn_tol *tol, BooleanRegularization *regularization,
    BooleanConversionState &state)
{
    struct brep_boolean_failure_info boolean_failure =
	BREP_BOOLEAN_FAILURE_INFO_INIT;
    const int status = ON_Boolean(result, left, right, operation,
	&boolean_failure);
    if (status < BREP_BOOLEAN_SUCCESS) {
	record_tree_failure(state, RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION,
	    RT_BREP_FAILURE_REASON_BOOLEAN_INDETERMINATE, status, tree_path,
	    NULL, operation, regularization, left, right, result, tol,
	    &boolean_failure);
    }
    return status;
}


static bool
bounded_halfspace_brep(
    ON_Brep **brep,
    const struct rt_half_internal *halfspace,
    const mat_t transform,
    const ON_Brep *finite_context,
    const struct bn_tol &tol,
    BooleanRegularization *regularization,
    BooleanRegularizationBound bound,
    const std::string &instance_path,
    fastf_t model_expansion)
{
    if (brep == NULL || *brep == NULL || finite_context == NULL) {
	return false;
    }

    const fastf_t offset = regularization == NULL ? 0.0 :
	regularization->Factor(tol, bound, instance_path, model_expansion);
    point_t plane_point;
    vect_t outward_normal;
    if (!halfspace_plane(plane_point, outward_normal, halfspace, transform,
	offset))
    {
	return false;
    }

    ON_BoundingBox bounds;
    if (!brep_GetTrimmedBoundingBox(*finite_context, bounds, false) ||
	!bounds.IsValid())
    {
	bounds = finite_context->BoundingBox();
    }
    if (!bounds.IsValid()) {
	return false;
    }

    const double margin = halfspace_proxy_margin(bounds, tol);
    if (!(margin > 0.0)) {
	return false;
    }

    vect_t tangent_u, tangent_v;
    bn_vec_perp(tangent_u, outward_normal);
    const fastf_t tangent_u_length = MAGNITUDE(tangent_u);
    if (!std::isfinite(tangent_u_length) || tangent_u_length <= SMALL_FASTF) {
	return false;
    }
    VSCALE(tangent_u, tangent_u, 1.0 / tangent_u_length);
    VCROSS(tangent_v, outward_normal, tangent_u);
    const fastf_t tangent_v_length = MAGNITUDE(tangent_v);
    if (!std::isfinite(tangent_v_length) || tangent_v_length <= SMALL_FASTF) {
	return false;
    }
    VSCALE(tangent_v, tangent_v, 1.0 / tangent_v_length);

    bool initialized = false;
    double minimum_u = 0.0, maximum_u = 0.0;
    double minimum_v = 0.0, maximum_v = 0.0;
    double minimum_normal = 0.0;
    for (int x_index = 0; x_index < 2; ++x_index) {
	const double x = x_index == 0 ? bounds.m_min.x : bounds.m_max.x;
	for (int y_index = 0; y_index < 2; ++y_index) {
	    const double y = y_index == 0 ? bounds.m_min.y : bounds.m_max.y;
	    for (int z_index = 0; z_index < 2; ++z_index) {
		const double z = z_index == 0 ? bounds.m_min.z : bounds.m_max.z;
		point_t point;
		VSET(point, x, y, z);
		vect_t offset_from_plane;
		VSUB2(offset_from_plane, point, plane_point);
		const double u_coordinate = VDOT(tangent_u, offset_from_plane);
		const double v_coordinate = VDOT(tangent_v, offset_from_plane);
		const double normal_coordinate = VDOT(outward_normal,
		    offset_from_plane);
		if (!std::isfinite(u_coordinate) || !std::isfinite(v_coordinate) ||
		    !std::isfinite(normal_coordinate))
		{
		    return false;
		}
		if (!initialized) {
		    minimum_u = maximum_u = u_coordinate;
		    minimum_v = maximum_v = v_coordinate;
		    minimum_normal = normal_coordinate;
		    initialized = true;
		} else {
		    minimum_u = std::min(minimum_u, u_coordinate);
		    maximum_u = std::max(maximum_u, u_coordinate);
		    minimum_v = std::min(minimum_v, v_coordinate);
		    maximum_v = std::max(maximum_v, v_coordinate);
		    minimum_normal = std::min(minimum_normal, normal_coordinate);
		}
	    }
	}
    }

    if (!initialized) {
	return false;
    }
    minimum_u -= margin;
    maximum_u += margin;
    minimum_v -= margin;
    maximum_v += margin;
    minimum_normal = std::min(minimum_normal - margin, -margin);

    point_t proxy_corners[8];
    halfspace_proxy_corner(proxy_corners[0], plane_point, tangent_u, tangent_v,
	outward_normal, minimum_u, minimum_v, minimum_normal);
    halfspace_proxy_corner(proxy_corners[1], plane_point, tangent_u, tangent_v,
	outward_normal, maximum_u, minimum_v, minimum_normal);
    halfspace_proxy_corner(proxy_corners[2], plane_point, tangent_u, tangent_v,
	outward_normal, maximum_u, maximum_v, minimum_normal);
    halfspace_proxy_corner(proxy_corners[3], plane_point, tangent_u, tangent_v,
	outward_normal, minimum_u, maximum_v, minimum_normal);
    halfspace_proxy_corner(proxy_corners[4], plane_point, tangent_u, tangent_v,
	outward_normal, minimum_u, minimum_v, 0.0);
    halfspace_proxy_corner(proxy_corners[5], plane_point, tangent_u, tangent_v,
	outward_normal, maximum_u, minimum_v, 0.0);
    halfspace_proxy_corner(proxy_corners[6], plane_point, tangent_u, tangent_v,
	outward_normal, maximum_u, maximum_v, 0.0);
    halfspace_proxy_corner(proxy_corners[7], plane_point, tangent_u, tangent_v,
	outward_normal, minimum_u, maximum_v, 0.0);

    ON_3dPoint corners[8];
    for (int corner_index = 0; corner_index < 8; ++corner_index) {
	corners[corner_index] = ON_3dPoint(proxy_corners[corner_index]);
    }
    ON_Brep *proxy = ON_BrepBox(corners);
    if (proxy == NULL || !proxy->IsValid() || !proxy->IsSolid()) {
	delete proxy;
	return false;
    }
    **brep = *proxy;
    delete proxy;
    return true;
}


static bool
breps_have_near_coplanar_planar_faces(
    const ON_Brep *first,
    const ON_Brep *second,
    const struct bn_tol &tol)
{
    if (first == NULL || second == NULL) {
	return false;
    }

    const fastf_t distance_tolerance = boolean_regularization_tolerance(tol);
    for (int first_index = 0; first_index < first->m_F.Count(); ++first_index) {
	const ON_BrepFace &first_face = first->m_F[first_index];
	const ON_Surface *first_surface = first_face.SurfaceOf();
	ON_Plane first_plane;
	if (first_surface == NULL || !first_surface->IsPlanar(&first_plane)) {
	    continue;
	}
	const ON_BoundingBox first_bounds = first_face.BoundingBox();
	if (!first_bounds.IsValid()) {
	    continue;
	}

	for (int second_index = 0; second_index < second->m_F.Count();
	     ++second_index)
	{
	    const ON_BrepFace &second_face = second->m_F[second_index];
	    const ON_Surface *second_surface = second_face.SurfaceOf();
	    ON_Plane second_plane;
	    if (second_surface == NULL || !second_surface->IsPlanar(&second_plane) ||
		first_plane.Normal().IsParallelTo(second_plane.Normal(),
		    BREP_BOOLEAN_REGULARIZATION_ANGLE_TOLERANCE) == 0 ||
		fabs(first_plane.DistanceTo(second_plane.Origin())) >
		    distance_tolerance)
	    {
		continue;
	    }

	    const ON_BoundingBox second_bounds = second_face.BoundingBox();
	    if (second_bounds.IsValid() && first_bounds.MinimumDistanceTo(
		second_bounds) <= distance_tolerance)
	    {
		return true;
	    }
	}
    }

    return false;
}


static bool
tree_transform_is_identity(const matp_t transform)
{
    if (transform == NULL)
	return true;
    for (size_t index = 0; index < 16; ++index) {
	const fastf_t expected = index % 5 == 0 ? 1.0 : 0.0;
	if (transform[index] != expected)
	    return false;
    }
    return true;
}


static bool
tree_transforms_identical(const matp_t first, const matp_t second)
{
    if (first == second)
	return true;
    if (first == NULL)
	return tree_transform_is_identity(second);
    if (second == NULL)
	return tree_transform_is_identity(first);
    for (size_t index = 0; index < 16; ++index)
	if (first[index] != second[index])
	    return false;
    return true;
}


/* These identities are safe only when the complete operands, including leaf
 * transforms, are the same.  Recognizing them before surface intersection
 * avoids manufacturing coincident curves and zero-area topology for A union
 * A, A intersect A, A - A, and A xor A. */
static bool
trees_identical(const union tree *first, const union tree *second)
{
    if (first == second)
	return first != NULL;
    if (first == NULL || second == NULL || first->tr_op != second->tr_op)
	return false;

    switch (first->tr_op) {
	case OP_DB_LEAF:
	    return first->tr_l.tl_name != NULL &&
		second->tr_l.tl_name != NULL &&
		BU_STR_EQUAL(first->tr_l.tl_name, second->tr_l.tl_name) &&
		tree_transforms_identical(first->tr_l.tl_mat,
		    second->tr_l.tl_mat);
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    return trees_identical(first->tr_b.tb_left,
		second->tr_b.tb_left) &&
		trees_identical(first->tr_b.tb_right,
		    second->tr_b.tb_right);
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    return trees_identical(first->tr_b.tb_left,
		second->tr_b.tb_left);
	case OP_NOP:
	    return true;
	default:
	    return false;
    }
}


enum LooseSubtreeBoundsStatus {
    LOOSE_SUBTREE_BOUNDS_INVALID = -1,
    LOOSE_SUBTREE_BOUNDS_UNAVAILABLE = 0,
    LOOSE_SUBTREE_BOUNDS_AVAILABLE = 1
};


static bool
finite_bounding_box(const ON_BoundingBox &bounds)
{
    return bounds.IsValid() &&
	std::isfinite(bounds.m_min.x) &&
	std::isfinite(bounds.m_min.y) &&
	std::isfinite(bounds.m_min.z) &&
	std::isfinite(bounds.m_max.x) &&
	std::isfinite(bounds.m_max.y) &&
	std::isfinite(bounds.m_max.z);
}


static bool
transform_bounding_box(ON_BoundingBox &bounds, const ON_Xform &transform)
{
    if (!finite_bounding_box(bounds))
	return false;

    const ON_BoundingBox source(bounds);
    ON_BoundingBox transformed;
    bool grow = false;
    for (int x = 0; x < 2; ++x) {
	for (int y = 0; y < 2; ++y) {
	    for (int z = 0; z < 2; ++z) {
		ON_3dPoint corner(
		    x ? source.m_max.x : source.m_min.x,
		    y ? source.m_max.y : source.m_min.y,
		    z ? source.m_max.z : source.m_min.z);
		corner.Transform(transform);
		if (!corner.IsValid())
		    return false;
		transformed.Set(corner, grow);
		grow = true;
	    }
	}
    }
    if (!finite_bounding_box(transformed))
	return false;
    bounds = transformed;
    return true;
}


/* Calculate a conservative bound without converting primitive leaves to
 * B-Reps.  This is intentionally narrower than general database bounding:
 * it walks the unevaluated CSG tree, honors every leaf instance transform,
 * and never treats an unavailable or infinite bound as evidence of
 * disjointness.  A subtraction is contained by its left operand, but its
 * right subtree is still visited so missing members and malformed trees
 * cannot be hidden by pruning. */
static int
loose_subtree_bounds(
    ON_BoundingBox &bounds,
    const union tree *subtree,
    const struct db_i *dbip,
    const struct bn_tol *tol,
    std::set<const struct directory *> &active_combinations)
{
    if (subtree == NULL || dbip == NULL)
	return LOOSE_SUBTREE_BOUNDS_INVALID;

    switch (subtree->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    {
		if (subtree->tr_b.tb_left == NULL ||
		    subtree->tr_b.tb_right == NULL)
		{
		    return LOOSE_SUBTREE_BOUNDS_INVALID;
		}

		ON_BoundingBox left_bounds;
		ON_BoundingBox right_bounds;
		const int left_status = loose_subtree_bounds(left_bounds,
		    subtree->tr_b.tb_left, dbip, tol, active_combinations);
		const int right_status = loose_subtree_bounds(right_bounds,
		    subtree->tr_b.tb_right, dbip, tol, active_combinations);
		if (left_status == LOOSE_SUBTREE_BOUNDS_INVALID ||
		    right_status == LOOSE_SUBTREE_BOUNDS_INVALID)
		{
		    return LOOSE_SUBTREE_BOUNDS_INVALID;
		}

		if (subtree->tr_op == OP_SUBTRACT) {
		    if (left_status == LOOSE_SUBTREE_BOUNDS_AVAILABLE)
			bounds = left_bounds;
		    return left_status;
		}
		if (left_status != LOOSE_SUBTREE_BOUNDS_AVAILABLE ||
		    right_status != LOOSE_SUBTREE_BOUNDS_AVAILABLE)
		{
		    return LOOSE_SUBTREE_BOUNDS_UNAVAILABLE;
		}

		if (subtree->tr_op == OP_UNION || subtree->tr_op == OP_XOR) {
		    bounds = left_bounds;
		    bounds.Union(right_bounds);
		} else {
		    bounds.m_min.x = std::max(left_bounds.m_min.x,
			right_bounds.m_min.x);
		    bounds.m_min.y = std::max(left_bounds.m_min.y,
			right_bounds.m_min.y);
		    bounds.m_min.z = std::max(left_bounds.m_min.z,
			right_bounds.m_min.z);
		    bounds.m_max.x = std::min(left_bounds.m_max.x,
			right_bounds.m_max.x);
		    bounds.m_max.y = std::min(left_bounds.m_max.y,
			right_bounds.m_max.y);
		    bounds.m_max.z = std::min(left_bounds.m_max.z,
			right_bounds.m_max.z);
		}
		return finite_bounding_box(bounds) ?
		    LOOSE_SUBTREE_BOUNDS_AVAILABLE :
		    LOOSE_SUBTREE_BOUNDS_UNAVAILABLE;
	    }
	case OP_DB_LEAF:
	    break;
	default:
	    return LOOSE_SUBTREE_BOUNDS_INVALID;
    }

    const char *name = subtree->tr_l.tl_name;
    if (name == NULL || name[0] == '\0')
	return LOOSE_SUBTREE_BOUNDS_INVALID;
    struct directory *directory = db_lookup(dbip, name, LOOKUP_QUIET);
    if (directory == RT_DIR_NULL)
	return LOOSE_SUBTREE_BOUNDS_INVALID;

    ON_Xform leaf_transform;
    bool transform_bounds = subtree->tr_l.tl_mat != NULL &&
	!bn_mat_is_identity(subtree->tr_l.tl_mat);
    if (transform_bounds) {
	fastf_t determinant = 0.0;
	if (!brlcad_mat_to_opennurbs_affine(leaf_transform, &determinant,
		subtree->tr_l.tl_mat))
	{
	    return LOOSE_SUBTREE_BOUNDS_INVALID;
	}
    }

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    if (rt_db_get_internal(&internal, directory, dbip, bn_mat_identity) < 0)
	return LOOSE_SUBTREE_BOUNDS_INVALID;

    int status = LOOSE_SUBTREE_BOUNDS_UNAVAILABLE;
    if (internal.idb_type == ID_COMBINATION) {
	const struct rt_comb_internal *combination =
	    static_cast<const struct rt_comb_internal *>(internal.idb_ptr);
	if (combination == NULL || combination->magic != RT_COMB_MAGIC ||
	    combination->tree == NULL ||
	    active_combinations.find(directory) != active_combinations.end())
	{
	    status = LOOSE_SUBTREE_BOUNDS_INVALID;
	} else {
	    active_combinations.insert(directory);
	    status = loose_subtree_bounds(bounds, combination->tree, dbip, tol,
		active_combinations);
	    active_combinations.erase(directory);
	}
    } else if (internal.idb_meth == NULL) {
	status = LOOSE_SUBTREE_BOUNDS_INVALID;
    } else if (internal.idb_meth->ft_bbox != NULL) {
	point_t minimum;
	point_t maximum;
	if (internal.idb_meth->ft_bbox(&internal, &minimum, &maximum, tol) == 0) {
	    bounds = ON_BoundingBox(ON_3dPoint(minimum), ON_3dPoint(maximum));
	    if (finite_bounding_box(bounds))
		status = LOOSE_SUBTREE_BOUNDS_AVAILABLE;
	}
    }
    rt_db_free_internal(&internal);

    if (status == LOOSE_SUBTREE_BOUNDS_AVAILABLE && transform_bounds &&
	!transform_bounding_box(bounds, leaf_transform))
    {
	return LOOSE_SUBTREE_BOUNDS_INVALID;
    }
    return status;
}


static bool
bounding_boxes_separated(const ON_BoundingBox &first,
    const ON_BoundingBox &second, fastf_t distance_tolerance)
{
    if (!finite_bounding_box(first) || !finite_bounding_box(second) ||
	!std::isfinite(distance_tolerance) || distance_tolerance < 0.0)
    {
	return false;
    }
    return first.m_max.x < second.m_min.x - distance_tolerance ||
	second.m_max.x < first.m_min.x - distance_tolerance ||
	first.m_max.y < second.m_min.y - distance_tolerance ||
	second.m_max.y < first.m_min.y - distance_tolerance ||
	first.m_max.z < second.m_min.z - distance_tolerance ||
	second.m_max.z < first.m_min.z - distance_tolerance;
}


static int
conv_tree(
    ON_Brep **b,
    const union tree *t,
    const struct db_i *dbip,
    const struct bn_tol *tol,
    BooleanRegularization *regularization,
    BooleanRegularizationBound bound,
    const ON_Brep *finite_context,
    const std::string &tree_path,
    fastf_t model_expansion,
    BooleanConversionState &state);


/* A snapped ARB may also be reordered into its canonical storage.  Compare
 * vertex sets rather than array slots so a storage-only repair has zero
 * geometric cost.  The symmetric vertex-set distance bounds the Hausdorff
 * distance between the two convex hulls: every hull point is a convex
 * combination of vertices, each of which has a representative within the
 * returned distance. */
static fastf_t
arb_vertex_set_distance(const struct rt_arb_internal *first,
    const struct rt_arb_internal *second)
{
    if (first == NULL || second == NULL)
	return INFINITY;

    fastf_t maximum = 0.0;
    for (int direction = 0; direction < 2; ++direction) {
	const struct rt_arb_internal *source = direction ? second : first;
	const struct rt_arb_internal *target = direction ? first : second;
	for (int source_index = 0; source_index < 8; ++source_index) {
	    fastf_t nearest = INFINITY;
	    for (int target_index = 0; target_index < 8; ++target_index) {
		const fastf_t distance = DIST_PNT_PNT(source->pt[source_index],
		    target->pt[target_index]);
		nearest = std::min(nearest, distance);
	    }
	    maximum = std::max(maximum, nearest);
	}
    }
    return maximum;
}


static fastf_t
arb_vertex_span(const struct rt_arb_internal *arb)
{
    if (arb == NULL)
	return 0.0;

    fastf_t span = 0.0;
    for (int first = 0; first < 8; ++first) {
	for (int second = first + 1; second < 8; ++second)
	    span = std::max(span, DIST_PNT_PNT(arb->pt[first],
		arb->pt[second]));
    }
    return span;
}


// TODO - there are multiple copies of this - need to find a good place for it...
static int
single_conversion(
    struct rt_db_internal *intern,
    ON_Brep **brep,
    const struct db_i *dbip,
    const struct bn_tol *tol,
    BooleanRegularization *regularization,
    BooleanRegularizationBound bound,
    const ON_Brep *finite_context,
    const std::string &instance_path,
    fastf_t model_expansion,
    BooleanConversionState &state)
{
    struct bn_tol default_tol = BN_TOL_INIT_TOL;
    const struct bn_tol *effective_tol = tol ? tol : &default_tol;
    const char *source = intern->idb_meth != NULL ?
	intern->idb_meth->ft_label : "primitive";
    bool exact_arb_conversion = false;
    struct rt_arb_internal repaired_arb = {};
    struct rt_db_internal repaired_internal;
    struct rt_db_internal *conversion_internal = intern;

    if (intern->idb_type == ID_BOT) {
	const struct rt_bot_internal *bot =
	    static_cast<const struct rt_bot_internal *>(intern->idb_ptr);
	if (bot == NULL || bot->magic != RT_BOT_INTERNAL_MAGIC) {
	    *brep = NULL;
	    return BREP_BOOLEAN_INVALID_INPUT;
	}
	if (bot->mode != RT_BOT_SOLID) {
	    bu_log("BOT B-Rep conversion supports solid mode only; mode %u "
		"cannot be represented as an equivalent closed solid.\n",
		static_cast<unsigned int>(bot->mode));
	    *brep = NULL;
	    return BREP_BOOLEAN_UNSUPPORTED;
	}
	if (bot->num_vertices >
		static_cast<size_t>(std::numeric_limits<int>::max()) ||
	    bot->num_faces >
		static_cast<size_t>(std::numeric_limits<int>::max()) ||
	    bot->vertices == NULL || bot->faces == NULL)
	{
	    bu_log("BOT B-Rep conversion rejected malformed solid data.\n");
	    *brep = NULL;
	    return BREP_BOOLEAN_INVALID_INPUT;
	}

	/* A solid mode flag is only a declaration; it does not prove that the
	 * indexed triangle mesh is a closed, oriented manifold.  Converting an
	 * open or nonmanifold triangle soup through NMG yields an open B-Rep and
	 * previously disguised invalid source geometry as an indeterminate
	 * conversion failure.  Do not fill holes or otherwise invent a different
	 * solid here. */
	struct bg_trimesh_solid_errors errors =
	    BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
	const int invalid_topology = bg_trimesh_solid2(
	    static_cast<int>(bot->num_vertices),
	    static_cast<int>(bot->num_faces),
	    const_cast<fastf_t *>(bot->vertices), bot->faces, &errors);
	if (invalid_topology != 0) {
	    bu_log("BOT B-Rep conversion rejected invalid solid topology "
		"(degenerate_faces=%d unmatched_edges=%d excess_edges=%d "
		"misoriented_edges=%d).\n", errors.degenerate.count,
		errors.unmatched.count, errors.excess.count,
		errors.misoriented.count);
	    bg_free_trimesh_solid_errors(&errors);
	    *brep = NULL;
	    return BREP_BOOLEAN_INVALID_INPUT;
	}
	bg_free_trimesh_solid_errors(&errors);
    }

    if (intern->idb_type == ID_ARS) {
	const struct rt_ars_internal *ars =
	    static_cast<const struct rt_ars_internal *>(intern->idb_ptr);
	if (!_rt_ars_tessellable(ars, effective_tol, 1)) {
	    *brep = NULL;
	    return BREP_BOOLEAN_INVALID_INPUT;
	}
    }

    if (intern->idb_type == ID_TGC) {
	const struct rt_tgc_internal *tgc =
	    static_cast<const struct rt_tgc_internal *>(intern->idb_ptr);
	if (tgc == NULL || tgc->magic != RT_TGC_INTERNAL_MAGIC ||
	    intern->idb_meth == NULL || intern->idb_meth->ft_validate == NULL)
	{
	    *brep = NULL;
	    return BREP_BOOLEAN_INVALID_INPUT;
	}
	struct bu_vls diagnostic = BU_VLS_INIT_ZERO;
	const int issues = intern->idb_meth->ft_validate(&diagnostic, intern,
	    effective_tol);
	if (issues != 0) {
	    bu_log("TGC B-Rep conversion rejected invalid input%s%s\n",
		bu_vls_strlen(&diagnostic) ? ": " : ".",
		bu_vls_strlen(&diagnostic) ? bu_vls_cstr(&diagnostic) : "");
	    bu_vls_free(&diagnostic);
	    *brep = NULL;
	    return BREP_BOOLEAN_INVALID_INPUT;
	}
	bu_vls_free(&diagnostic);
    }

    if (intern->idb_type == ID_PIPE) {
	const struct rt_pipe_internal *pipe =
	    static_cast<const struct rt_pipe_internal *>(intern->idb_ptr);
	if (pipe == NULL || pipe->pipe_magic != RT_PIPE_INTERNAL_MAGIC ||
	    rt_pipe_validate(NULL, pipe, 0) != 0)
	{
	    bu_log("PIPE B-Rep conversion rejected invalid input.\n");
	    *brep = NULL;
	    return BREP_BOOLEAN_INVALID_INPUT;
	}
    }

    if (intern->idb_type == ID_CLINE) {
	const struct rt_cline_internal *cline =
	    static_cast<const struct rt_cline_internal *>(intern->idb_ptr);
	if (cline == NULL || cline->magic != RT_CLINE_INTERNAL_MAGIC) {
	    *brep = NULL;
	    return BREP_BOOLEAN_INVALID_INPUT;
	}
	/* Positive CLINE thickness is measured along every incident ray, not
	 * normal to a fixed inner surface.  Consequently its occupancy depends
	 * on viewing direction and no closed B-Rep can be semantically
	 * equivalent.  rt_cline_brep remains available to geometry exporters as
	 * a documented fixed-wall approximation, but exact CSG evaluation must
	 * not silently substitute that different solid. */
	if (cline->thickness > 0.0) {
	    bu_log("Plate-mode CLINE has ray-dependent line-of-sight thickness "
		"and no equivalent closed B-Rep.\n");
	    *brep = NULL;
	    return BREP_BOOLEAN_UNSUPPORTED;
	}
    }

    if (intern->idb_type == ID_DSP) {
	const struct rt_dsp_internal *dsp =
	    static_cast<const struct rt_dsp_internal *>(intern->idb_ptr);
	if (dsp == NULL || dsp->magic != RT_DSP_INTERNAL_MAGIC ||
	    dsp->dsp_xcnt < 2 || dsp->dsp_ycnt < 2 || dsp->dsp_buf == NULL)
	{
	    bu_log("DSP B-Rep conversion rejected invalid or unavailable "
		"height-field data.\n");
	    *brep = NULL;
	    return BREP_BOOLEAN_INVALID_INPUT;
	}
	if (dsp->dsp_xcnt > RT_DSP_BREP_MAX_CONTROL_POINTS ||
	    dsp->dsp_ycnt > RT_DSP_BREP_MAX_CONTROL_POINTS)
	{
	    bu_log("DSP B-Rep conversion exceeds the %u by %u control-point "
		"budget; got %u by %u.\n",
		RT_DSP_BREP_MAX_CONTROL_POINTS,
		RT_DSP_BREP_MAX_CONTROL_POINTS, dsp->dsp_xcnt,
		dsp->dsp_ycnt);
	    *brep = NULL;
	    return BREP_BOOLEAN_BUDGET_EXCEEDED;
	}
    }

    if (intern->idb_type == ID_VOL) {
	const struct rt_vol_internal *vol =
	    static_cast<const struct rt_vol_internal *>(intern->idb_ptr);
	size_t patch_count = 0;
	const int preflight = _rt_vol_brep_patch_count(vol,
	    RT_VOL_BREP_MAX_INITIAL_PATCHES, &patch_count);
	if (preflight < 0) {
	    bu_log("VOL B-Rep conversion rejected invalid or unavailable "
		"voxel data.\n");
	    *brep = NULL;
	    return BREP_BOOLEAN_INVALID_INPUT;
	}
	if (preflight > 0) {
	    bu_log("VOL B-Rep conversion requires more than %zu initial "
		"planar patches.\n",
		static_cast<size_t>(RT_VOL_BREP_MAX_INITIAL_PATCHES));
	    *brep = NULL;
	    return BREP_BOOLEAN_BUDGET_EXCEEDED;
	}
	if (patch_count == 0) {
	    (*brep)->Destroy();
	    return BREP_BOOLEAN_EMPTY;
	}
    }

    if (intern->idb_type == ID_ARB8) {
	const struct rt_arb_internal *arb =
	    static_cast<const struct rt_arb_internal *>(intern->idb_ptr);
	struct bu_vls diagnostic = BU_VLS_INIT_ZERO;
	int issues = 0;
	int validation_result = arb != NULL &&
	    arb->magic == RT_ARB_INTERNAL_MAGIC ?
	    rt_arb_validate(&diagnostic, arb, effective_tol, &issues) :
	    RT_ARB_VALIDATE_TWISTED;
	if (validation_result & RT_ARB_VALIDATE_NONSTANDARD) {
	    if (rt_arb_repair(&repaired_arb, arb, effective_tol, 0) >= 0) {
		bu_vls_trunc(&diagnostic, 0);
		validation_result = rt_arb_validate(&diagnostic, &repaired_arb,
		    effective_tol, &issues);
		if (!(validation_result & ~RT_ARB_VALIDATE_CONCAVE)) {
		    repaired_internal = *intern;
		    repaired_internal.idb_ptr = &repaired_arb;
		    conversion_internal = &repaired_internal;
		    arb = &repaired_arb;
		}
	    }
	}
	/* A nearly planar authored ARB can be made representable by snapping its
	 * vertices to best-fit face-plane intersections.  A legacy vertex ordering
	 * can also describe the same convex point set with non-planar canonical
	 * faces; canonicalizing that ordering has zero vertex-set displacement.
	 * rt_arb_repair has a deliberately generous interactive repair allowance,
	 * so impose the stricter conversion contract here: after all pending
	 * transforms any nonzero symmetric vertex-set displacement must remain
	 * below an absolute cap and no more than 0.5 percent of the ARB's local
	 * span.  Twisted or concave inputs are not silently convexified. */
	if ((validation_result & RT_ARB_VALIDATE_NONCOPLANAR) &&
	    !(validation_result & (RT_ARB_VALIDATE_CONCAVE |
		RT_ARB_VALIDATE_TWISTED)) &&
	    rt_arb_repair(&repaired_arb, arb, effective_tol,
		RT_ARB_REPAIR_SNAP_VERTICES) >= 0)
	{
	    int repaired_issues = 0;
	    struct bu_vls repaired_diagnostic = BU_VLS_INIT_ZERO;
	    const int repaired_validation = rt_arb_validate(&repaired_diagnostic,
		&repaired_arb, effective_tol, &repaired_issues);
	    const fastf_t local_span = arb_vertex_span(arb);
	    const fastf_t expansion = std::max(static_cast<fastf_t>(1.0),
		model_expansion);
	    const fastf_t tolerance_budget =
		boolean_regularization_tolerance(*effective_tol) * 0.99 /
		expansion;
	    const fastf_t relative_budget = local_span *
		BREP_BOOLEAN_ARB_REPAIR_RELATIVE_LIMIT;
	    const fastf_t absolute_cap = boolean_regularization_tolerance(
		*effective_tol) * BREP_BOOLEAN_ARB_REPAIR_TOLERANCE_MULTIPLIER /
		expansion;
	    const fastf_t local_budget = std::max(tolerance_budget,
		std::min(relative_budget, absolute_cap));
	    const fastf_t local_displacement = arb_vertex_set_distance(arb,
		&repaired_arb);
	    const fastf_t model_displacement = local_displacement *
		model_expansion;
	    if (repaired_validation == 0 && repaired_issues == 0 &&
		std::isfinite(model_displacement) &&
		local_displacement <= local_budget)
	    {
		repaired_internal = *intern;
		repaired_internal.idb_ptr = &repaired_arb;
		conversion_internal = &repaired_internal;
		validation_result = 0;
		issues = 0;
		bu_vls_trunc(&diagnostic, 0);
		exact_arb_conversion = true;
		state.maximum_primitive_approximation = std::max(
		    state.maximum_primitive_approximation, model_displacement);
		state.repaired_arb_count++;
		bu_log("ARB B-Rep conversion repaired a nearly planar or "
		    "noncanonical input "
		    "with maximum model-space vertex displacement %.17g "
		    "(%.3g of modeling tolerance, %.3g of ARB span); output "
		    "is approximate.\n",
		    static_cast<double>(model_displacement),
		    static_cast<double>(model_displacement /
			boolean_regularization_tolerance(*effective_tol)),
		    static_cast<double>(local_span > SMALL_FASTF ?
			local_displacement / local_span : 0.0));
	    } else if (repaired_validation == 0 && repaired_issues == 0 &&
		std::isfinite(local_displacement))
	    {
		bu_log("ARB B-Rep repair candidate exceeded the conversion "
		    "budget (model-space displacement %.17g, limit %.17g).\n",
		    static_cast<double>(model_displacement),
		    static_cast<double>(local_budget * model_expansion));
	    }
	    bu_vls_free(&repaired_diagnostic);
	}
	if (validation_result & ~RT_ARB_VALIDATE_CONCAVE)
	{
	    bu_log("ARB B-Rep conversion rejected invalid input%s%s",
		bu_vls_strlen(&diagnostic) ? ": " : ".\n",
		bu_vls_strlen(&diagnostic) ? bu_vls_cstr(&diagnostic) : "");
	    bu_vls_free(&diagnostic);
	    *brep = NULL;
	    return BREP_BOOLEAN_INVALID_INPUT;
	}
	exact_arb_conversion = exact_arb_conversion ||
	    (validation_result & RT_ARB_VALIDATE_CONCAVE) != 0;
	bu_vls_free(&diagnostic);
    }

    /* Offsetting an ARB's face planes replaces a concave volume with their
     * convex intersection.  Its exact B-Rep is nevertheless both a valid
     * inner and outer bound, so retain it during a regularized retry. */
    if (regularization != NULL && !exact_arb_conversion &&
	conversion_internal->idb_type != ID_COMBINATION &&
	conversion_internal->idb_type != ID_BREP &&
	conversion_internal->idb_meth != NULL &&
	conversion_internal->idb_meth->ft_perturb != NULL)
    {
	struct rt_db_internal *perturbed = NULL;
	const int perturb_result = conversion_internal->idb_meth->ft_perturb(
	    &perturbed, conversion_internal, 0, regularization->Factor(
		*effective_tol, bound, instance_path, model_expansion));
	if (perturb_result != BRLCAD_OK || perturbed == NULL) {
	    if (perturbed != NULL) {
		rt_db_free_internal(perturbed);
		BU_PUT(perturbed, struct rt_db_internal);
	    }
	    regularization->MarkUnsupported();
	    *brep = NULL;
	    return BREP_BOOLEAN_UNSUPPORTED;
	}

	ON_Brep *perturbed_brep = ON_Brep::New();
	ON_Brep *perturbed_owner = perturbed_brep;
	const int perturbed_result = single_conversion(perturbed, &perturbed_brep,
	    dbip, effective_tol, NULL, BREP_BOOLEAN_REGULARIZATION_INNER,
	    finite_context, instance_path, model_expansion, state);
	rt_db_free_internal(perturbed);
	BU_PUT(perturbed, struct rt_db_internal);
	if (perturbed_result >= BREP_BOOLEAN_SUCCESS && perturbed_brep != NULL) {
	    **brep = *perturbed_brep;
	    if (perturbed_brep != perturbed_owner) {
		delete perturbed_owner;
	    }
	    delete perturbed_brep;
	    return perturbed_result;
	}
	if (perturbed_brep != perturbed_owner) {
	    delete perturbed_brep;
	}
	delete perturbed_owner;
	*brep = NULL;
	return perturbed_result < BREP_BOOLEAN_SUCCESS ? perturbed_result :
	    BREP_BOOLEAN_INDETERMINATE;
    }

    if (intern->idb_type == ID_BREP) {
        // already a brep
        RT_BREP_CK_MAGIC(intern->idb_ptr);
        **brep = *((struct rt_brep_internal *)intern->idb_ptr)->brep;
    } else if (intern->idb_type == ID_COMBINATION) {
        struct rt_comb_internal *comb =
	    (struct rt_comb_internal *)intern->idb_ptr;
	if (comb == NULL || comb->tree == NULL) {
	    *brep = NULL;
	    return BREP_BOOLEAN_INVALID_INPUT;
	}
	const int status = conv_tree(brep, comb->tree, dbip, effective_tol,
	    regularization, bound, finite_context, instance_path + "/c",
	    model_expansion, state);
	if (status != BREP_BOOLEAN_SUCCESS)
	    return status;
	} else if (conversion_internal->idb_meth != NULL &&
	    conversion_internal->idb_meth->ft_brep != NULL)
	{
	fastf_t primitive_approximation = 0.0;
	if (conversion_internal->idb_type == ID_PIPE) {
	    _rt_pipe_brep_with_info(brep, conversion_internal, effective_tol,
		&primitive_approximation);
	} else if (conversion_internal->idb_type == ID_ELL) {
	    _rt_ell_brep_with_info(brep, conversion_internal, effective_tol,
		&primitive_approximation);
	} else {
	    conversion_internal->idb_meth->ft_brep(brep, conversion_internal,
		effective_tol);
	}
	const int status = converted_brep_status(*brep, source);
	if (status == BREP_BOOLEAN_SUCCESS && primitive_approximation > 0.0) {
	    const double transformed_approximation =
		static_cast<double>(primitive_approximation) * model_expansion;
	    if (std::isfinite(transformed_approximation) &&
		transformed_approximation <= MAX_FASTF)
	    {
		state.maximum_primitive_approximation = std::max(
		    state.maximum_primitive_approximation,
		    static_cast<fastf_t>(transformed_approximation));
	    }
	}
	return status;
    } else {
        *brep = NULL;
        return BREP_BOOLEAN_UNSUPPORTED;
    }
    return converted_brep_status(*brep, source);
}


static void
delete_tree_conversion(ON_Brep *converted, ON_Brep *owner)
{
    if (converted != owner) {
	delete converted;
    }
    delete owner;
}


static int
regularized_tree_difference(
    ON_Brep *difference,
    const std::string &tree_path,
    const union tree *minuend_tree,
    const std::string &minuend_path,
    BooleanRegularizationBound minuend_bound,
    const union tree *subtrahend_tree,
    const std::string &subtrahend_path,
    BooleanRegularizationBound subtrahend_bound,
    const struct db_i *dbip,
    const struct bn_tol *tol,
    BooleanRegularization *regularization,
    const ON_Brep *finite_context,
    fastf_t model_expansion,
    BooleanConversionState &state)
{
    ON_Brep *minuend_owner = ON_Brep::New();
    ON_Brep *minuend = minuend_owner;
    int ret = conv_tree(&minuend, minuend_tree, dbip, tol, regularization,
	minuend_bound, finite_context, minuend_path, model_expansion, state);
    if (ret < BREP_BOOLEAN_SUCCESS || ret == BREP_BOOLEAN_EMPTY) {
	delete_tree_conversion(minuend, minuend_owner);
	difference->Destroy();
	return ret;
    }

    ON_Brep *subtrahend_owner = ON_Brep::New();
    ON_Brep *subtrahend = subtrahend_owner;
    ret = conv_tree(&subtrahend, subtrahend_tree, dbip, tol, regularization,
	subtrahend_bound, minuend, subtrahend_path, model_expansion, state);
    if (ret < BREP_BOOLEAN_SUCCESS) {
	delete_tree_conversion(subtrahend, subtrahend_owner);
	delete_tree_conversion(minuend, minuend_owner);
	difference->Destroy();
	return ret;
    }

    if (ret == BREP_BOOLEAN_EMPTY) {
	*difference = *minuend;
	ret = BREP_BOOLEAN_SUCCESS;
    } else {
	ret = evaluate_tree_boolean(difference, minuend, subtrahend,
	    BOOLEAN_DIFF, tree_path, tol, regularization, state);
    }
    delete_tree_conversion(subtrahend, subtrahend_owner);
    delete_tree_conversion(minuend, minuend_owner);
    return ret;
}


/* A direct XOR of two inner (or two outer) approximations is not itself a
 * certified bound: perturbing an overlap can exchange material between the
 * operands.  Construct the symmetric difference from bounded differences.
 *
 * inner: (A_inner - B_outer) union (B_inner - A_outer)
 * outer: (A_outer - B_inner) union (B_outer - A_inner)
 */
static int
regularized_tree_xor(
    ON_Brep *result,
    const union tree *first_tree,
    const union tree *second_tree,
    const struct db_i *dbip,
    const struct bn_tol *tol,
    BooleanRegularization *regularization,
    BooleanRegularizationBound bound,
    const ON_Brep *finite_context,
    const std::string &tree_path,
    fastf_t model_expansion,
    BooleanConversionState &state)
{
    ON_Brep first_difference;
    int first_status = regularized_tree_difference(&first_difference,
	tree_path + ".X0",
	first_tree, tree_path + ".L", bound, second_tree, tree_path + ".R",
	opposite_regularization_bound(bound), dbip, tol, regularization,
	finite_context, model_expansion, state);
    if (first_status < BREP_BOOLEAN_SUCCESS) {
	return first_status;
    }

    ON_Brep second_difference;
    int second_status = regularized_tree_difference(&second_difference,
	tree_path + ".X1",
	second_tree, tree_path + ".R", bound, first_tree, tree_path + ".L",
	opposite_regularization_bound(bound), dbip, tol, regularization,
	finite_context, model_expansion, state);
    if (second_status < BREP_BOOLEAN_SUCCESS) {
	return second_status;
    }

    if (first_status == BREP_BOOLEAN_EMPTY &&
	second_status == BREP_BOOLEAN_EMPTY)
    {
	result->Destroy();
	return BREP_BOOLEAN_EMPTY;
    }
    if (first_status == BREP_BOOLEAN_EMPTY) {
	*result = second_difference;
	return BREP_BOOLEAN_SUCCESS;
    }
    if (second_status == BREP_BOOLEAN_EMPTY) {
	*result = first_difference;
	return BREP_BOOLEAN_SUCCESS;
    }

    /* For an inner bound, these two terms cannot share material:
     *
     *   A_inner - B_outer
     *   B_inner - A_outer
     *
     * A_inner is contained by A_outer, and B_inner is contained by B_outer,
     * so membership in both terms would be contradictory.  Keep the two
     * components disconnected instead of asking the Boolean evaluator to
     * rediscover that fact across the deliberately narrow regularization gap.
     * The outer-bound terms may overlap in that gap and still require a true
     * union. */
    if (bound == BREP_BOOLEAN_REGULARIZATION_INNER) {
	*result = first_difference;
	result->Append(second_difference);
	result->ShrinkSurfaces();
	if (!result->Compact() || !result->IsValid() || !result->IsSolid()) {
	    result->Destroy();
	    return BREP_BOOLEAN_INDETERMINATE;
	}
	return BREP_BOOLEAN_SUCCESS;
    }
    return evaluate_tree_boolean(result, &first_difference,
	&second_difference, BOOLEAN_UNION, tree_path + ".XU", tol,
	regularization, state);
}


struct IntersectionOperand {
    const union tree *subtree = NULL;
    std::string path;
};


static bool
collect_intersection_operands(const union tree *subtree,
    const std::string &tree_path, std::vector<IntersectionOperand> &operands)
{
    if (subtree == NULL) {
	return false;
    }
    if (subtree->tr_op == OP_INTERSECT) {
	return collect_intersection_operands(subtree->tr_b.tb_left,
		tree_path + ".L", operands) &&
	    collect_intersection_operands(subtree->tr_b.tb_right,
		tree_path + ".R", operands);
    }
	operands.push_back({subtree, tree_path});
    return true;
}


/* Intersections are associative and commutative.  Flattening them lets a
 * finite operand (or an enclosing finite context) be established before any
 * halfspace-only operands are converted.  Each halfspace then clips the
 * accumulated finite result instead of being intersected with another proxy
 * box, whose artificial side faces can be coincident. */
static int
evaluate_intersection_tree(
    ON_Brep *result,
    const union tree *intersection_tree,
    const struct db_i *dbip,
    const struct bn_tol *tol,
    BooleanRegularization *regularization,
    BooleanRegularizationBound bound,
    const ON_Brep *finite_context,
    bool *retry_eligible,
    const std::string &tree_path,
    fastf_t model_expansion,
    BooleanConversionState &state)
{
	std::vector<IntersectionOperand> operands;
	if (!collect_intersection_operands(intersection_tree, tree_path, operands) ||
	operands.empty())
    {
	return BREP_BOOLEAN_INVALID_INPUT;
    }

    ON_Brep *accumulator_owner = NULL;
    ON_Brep *accumulator = NULL;
    std::vector<bool> processed(operands.size(), false);
    if (finite_context != NULL) {
	accumulator_owner = accumulator = ON_Brep::New(*finite_context);
    } else {
	for (size_t operand_index = 0; operand_index < operands.size();
	     ++operand_index)
	{
	    ON_Brep *candidate_owner = ON_Brep::New();
	    ON_Brep *candidate = candidate_owner;
	    const int status = conv_tree(&candidate,
		operands[operand_index].subtree,
		dbip, tol, regularization, bound, NULL,
		operands[operand_index].path, model_expansion, state);
	    if (status == BREP_BOOLEAN_FINITE_CONTEXT_REQUIRED) {
		delete_tree_conversion(candidate, candidate_owner);
		continue;
	    }
	    if (status < BREP_BOOLEAN_SUCCESS) {
		delete_tree_conversion(candidate, candidate_owner);
		return status;
	    }
	    if (status == BREP_BOOLEAN_EMPTY) {
		delete_tree_conversion(candidate, candidate_owner);
		result->Destroy();
		return BREP_BOOLEAN_EMPTY;
	    }
	    accumulator_owner = candidate_owner;
	    accumulator = candidate;
	    processed[operand_index] = true;
	    break;
	}
    }

    if (accumulator == NULL) {
	return BREP_BOOLEAN_FINITE_CONTEXT_REQUIRED;
    }

    for (size_t operand_index = 0; operand_index < operands.size();
	 operand_index++)
    {
	if (processed[operand_index]) {
	    continue;
	}
	ON_Brep *candidate_owner = ON_Brep::New();
	ON_Brep *candidate = candidate_owner;
	int status = conv_tree(&candidate, operands[operand_index].subtree, dbip,
	    tol, regularization, bound, accumulator,
	    operands[operand_index].path, model_expansion, state);
	if (status < BREP_BOOLEAN_SUCCESS) {
	    delete_tree_conversion(candidate, candidate_owner);
	    delete_tree_conversion(accumulator, accumulator_owner);
	    return status;
	}
	if (status == BREP_BOOLEAN_EMPTY) {
	    delete_tree_conversion(candidate, candidate_owner);
	    delete_tree_conversion(accumulator, accumulator_owner);
	    result->Destroy();
	    return BREP_BOOLEAN_EMPTY;
	}

	ON_Brep *combined = ON_Brep::New();
	status = evaluate_tree_boolean(combined, accumulator, candidate,
	    BOOLEAN_INTERSECT, tree_path + ".I" +
	    std::to_string(operand_index), tol, regularization, state);
	if (boolean_status_retryable(status) && regularization == NULL &&
	    tol != NULL && retry_eligible != NULL)
	{
	    *retry_eligible = breps_have_near_coplanar_planar_faces(
		accumulator, candidate, *tol);
	}
	delete_tree_conversion(candidate, candidate_owner);
	delete_tree_conversion(accumulator, accumulator_owner);
	if (status != BREP_BOOLEAN_SUCCESS) {
	    delete combined;
	    if (status == BREP_BOOLEAN_EMPTY) {
		result->Destroy();
	    }
	    return status;
	}
	accumulator_owner = accumulator = combined;
    }

    *result = *accumulator;
    delete_tree_conversion(accumulator, accumulator_owner);
    return BREP_BOOLEAN_SUCCESS;
}

static int
conv_tree(
    ON_Brep **b,
    const union tree *t,
    const struct db_i *dbip,
    const struct bn_tol *tol,
    BooleanRegularization *regularization,
    BooleanRegularizationBound bound,
    const ON_Brep *finite_context,
    const std::string &tree_path,
    fastf_t model_expansion,
    BooleanConversionState &state)
{
    if (b == NULL || *b == NULL || t == NULL || dbip == NULL) {
	record_tree_failure(state, RT_BREP_FAILURE_STAGE_TREE_STRUCTURE,
	    RT_BREP_FAILURE_REASON_INVALID_TREE, BREP_BOOLEAN_INVALID_INPUT,
	    tree_path, NULL, -1, regularization);
	if (b != NULL)
	    *b = NULL;
	return BREP_BOOLEAN_INVALID_INPUT;
    }

    ON_Brep *left = NULL, *right = NULL, *old = NULL;
    int ret = 0;
    bool retry_eligible = false;
    BooleanRegularizationBound right_bound = bound;
    enum rt_brep_failure_stage failure_stage = RT_BREP_FAILURE_STAGE_NONE;
    enum rt_brep_failure_reason failure_reason =
	RT_BREP_FAILURE_REASON_NONE;
    const char *failure_object = NULL;
    switch (t->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    if (t->tr_b.tb_left == NULL || t->tr_b.tb_right == NULL) {
		ret = BREP_BOOLEAN_INVALID_INPUT;
		failure_stage = RT_BREP_FAILURE_STAGE_TREE_STRUCTURE;
		failure_reason = RT_BREP_FAILURE_REASON_INVALID_TREE;
		break;
	    }
	    if (trees_identical(t->tr_b.tb_left, t->tr_b.tb_right)) {
		if (t->tr_op == OP_UNION || t->tr_op == OP_INTERSECT) {
		    return conv_tree(b, t->tr_b.tb_left, dbip, tol,
			regularization, bound, finite_context, tree_path + ".L",
			model_expansion, state);
		}

		/* Validate/convert the operand once before declaring A-A or A xor A
		 * empty.  This preserves errors for missing or malformed operands. */
		ON_Brep *operand_owner = ON_Brep::New();
		ON_Brep *operand = operand_owner;
		const int operand_status = conv_tree(&operand,
		    t->tr_b.tb_left, dbip, tol, regularization, bound,
		    finite_context, tree_path + ".L", model_expansion, state);
		delete_tree_conversion(operand, operand_owner);
		if (operand_status >= BREP_BOOLEAN_SUCCESS) {
		    (*b)->Destroy();
		    return BREP_BOOLEAN_EMPTY;
		}
		return operand_status;
	    }
	    /* Absorb a repeated subtractor before constructing the intermediate
	     * difference.  Besides avoiding unnecessary work, this preserves the
	     * exact regularized-set identity
	     *
	     *                 (A - B) union B = A union B.
	     *
	     * Evaluating the tree literally manufactures B-derived trim curves on
	     * A and then immediately intersects those curves with B again.  That is
	     * both numerically harder and capable of exhausting SSI refinement for
	     * an operation whose result never depended on the intermediate trims.
	     * Do not apply the rewrite to a perturbation retry: the inner and outer
	     * instances of B intentionally differ there. */
	    if (t->tr_op == OP_UNION && regularization == NULL) {
		const union tree *first = t->tr_b.tb_left;
		const union tree *second = t->tr_b.tb_right;
		const union tree *simplified_left = NULL;
		const union tree *simplified_right = NULL;
		if (first->tr_op == OP_SUBTRACT &&
		    first->tr_b.tb_left != NULL &&
		    first->tr_b.tb_right != NULL &&
		    trees_identical(first->tr_b.tb_right, second))
		{
		    simplified_left = first->tr_b.tb_left;
		    simplified_right = second;
		} else if (second->tr_op == OP_SUBTRACT &&
		    second->tr_b.tb_left != NULL &&
		    second->tr_b.tb_right != NULL &&
		    trees_identical(second->tr_b.tb_right, first))
		{
		    simplified_left = first;
		    simplified_right = second->tr_b.tb_left;
		}

		if (simplified_left != NULL && simplified_right != NULL) {
		    union tree simplified;
		    RT_TREE_INIT(&simplified);
		    simplified.tr_op = OP_UNION;
		    simplified.tr_b.tb_regionp = REGION_NULL;
		    simplified.tr_b.tb_left = const_cast<union tree *>(
			simplified_left);
		    simplified.tr_b.tb_right = const_cast<union tree *>(
			simplified_right);
		    return conv_tree(b, &simplified, dbip, tol, regularization,
			bound, finite_context, tree_path + ".S", model_expansion,
			state);
		}
	    }
	    if (t->tr_op == OP_XOR && regularization != NULL) {
		ret = regularized_tree_xor(*b, t->tr_b.tb_left,
		    t->tr_b.tb_right, dbip, tol, regularization, bound,
		    finite_context, tree_path, model_expansion, state);
		break;
	    }
	    if (t->tr_op == OP_INTERSECT) {
		ret = evaluate_intersection_tree(*b, t, dbip, tol,
		    regularization, bound, finite_context, &retry_eligible,
		    tree_path, model_expansion, state);
		break;
	    }
	    if (t->tr_op == OP_SUBTRACT) {
		old = left = ON_Brep::New();
		ret = conv_tree(&left, t->tr_b.tb_left, dbip, tol,
		    regularization, bound, finite_context, tree_path + ".L",
		    model_expansion, state);
		if (ret < BREP_BOOLEAN_SUCCESS) {
		    delete old;
		    break;
		}
		if (ret == BREP_BOOLEAN_EMPTY) {
		    delete left;
		    break;
		}
		/* A spatially disjoint subtractor cannot change the converted
		 * minuend.  Prove that before converting the right subtree so an
		 * irrelevant unsupported or geometrically invalid primitive does
		 * not reject an otherwise exact result.  Regularized evaluation is
		 * intentionally excluded because its perturbed bounds differ. */
		if (regularization == NULL) {
		    ON_BoundingBox left_bounds;
		    ON_BoundingBox right_bounds;
		    std::set<const struct directory *> active_combinations;
		    const int right_bounds_status = loose_subtree_bounds(
			right_bounds, t->tr_b.tb_right, dbip, tol,
			active_combinations);
		    const fastf_t distance_tolerance = tol != NULL ?
			boolean_regularization_tolerance(*tol) : BN_TOL_DIST;
		    if (right_bounds_status == LOOSE_SUBTREE_BOUNDS_AVAILABLE &&
			brep_GetTrimmedBoundingBox(*left, left_bounds, false) &&
			bounding_boxes_separated(left_bounds, right_bounds,
			    distance_tolerance))
		    {
			**b = *left;
			delete left;
			return BREP_BOOLEAN_SUCCESS;
		    }
		}
		old = right = ON_Brep::New();
		right_bound = opposite_regularization_bound(bound);
		ret = conv_tree(&right, t->tr_b.tb_right, dbip, tol,
		    regularization, right_bound, left, tree_path + ".R",
		    model_expansion, state);
		if (ret < BREP_BOOLEAN_SUCCESS) {
		    delete old;
		    delete left;
		    break;
		}
	    } else {
		old = right = ON_Brep::New();
		ret = conv_tree(&right, t->tr_b.tb_right, dbip, tol,
		    regularization, bound, finite_context, tree_path + ".R",
		    model_expansion, state);
		if (ret < BREP_BOOLEAN_SUCCESS) {
		    delete old;
		    break;
		}
		old = left = ON_Brep::New();
		ret = conv_tree(&left, t->tr_b.tb_left, dbip, tol,
		    regularization, bound, finite_context, tree_path + ".L",
		    model_expansion, state);
		if (ret < BREP_BOOLEAN_SUCCESS) {
		    delete old;
		    delete right;
		    break;
		}
	    }
	    if (ret >= BREP_BOOLEAN_SUCCESS) {
		// Perform NURBS evaluations
		if (t->tr_op == OP_UNION)
		    ret = evaluate_tree_boolean(*b, left, right,
			BOOLEAN_UNION, tree_path, tol, regularization, state);
		else if (t->tr_op == OP_INTERSECT)
		    ret = evaluate_tree_boolean(*b, left, right,
			BOOLEAN_INTERSECT, tree_path, tol, regularization,
			state);
		else if (t->tr_op == OP_SUBTRACT)
		    ret = evaluate_tree_boolean(*b, left, right,
			BOOLEAN_DIFF, tree_path, tol, regularization, state);
		else if (t->tr_op == OP_XOR)
		    ret = evaluate_tree_boolean(*b, left, right,
			BOOLEAN_XOR, tree_path, tol, regularization, state);
		else {
		    bu_log("operation %d isn't supported yet.\n", t->tr_op);
		    ret = BREP_BOOLEAN_UNSUPPORTED;
		    failure_stage = RT_BREP_FAILURE_STAGE_TREE_STRUCTURE;
		    failure_reason = RT_BREP_FAILURE_REASON_UNSUPPORTED_OBJECT;
		}
	if (boolean_status_retryable(ret) && regularization == NULL &&
	    tol != NULL)
	{
	    retry_eligible = breps_have_near_coplanar_planar_faces(left,
		right, *tol);
	}
		delete left;
		delete right;
	    }
	    break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    bu_log("operation %d isn't supported yet.\n", t->tr_op);
	    ret = BREP_BOOLEAN_UNSUPPORTED;
	    failure_stage = RT_BREP_FAILURE_STAGE_TREE_STRUCTURE;
	    failure_reason = RT_BREP_FAILURE_REASON_UNSUPPORTED_OBJECT;
	    break;
	case OP_DB_LEAF:
	    {
		char *name = t->tr_l.tl_name;
	if (name == NULL || name[0] == '\0') {
	    ret = BREP_BOOLEAN_INVALID_INPUT;
	    failure_stage = RT_BREP_FAILURE_STAGE_TREE_STRUCTURE;
	    failure_reason = RT_BREP_FAILURE_REASON_INVALID_TREE;
	    break;
	}
	failure_object = name;
	fastf_t leaf_model_expansion = 1.0;
	if (!combined_expansion_bound(&leaf_model_expansion,
		model_expansion, t->tr_l.tl_mat))
	{
	    ret = BREP_BOOLEAN_INVALID_INPUT;
	    failure_stage = RT_BREP_FAILURE_STAGE_LEAF_TRANSFORM;
	    failure_reason = RT_BREP_FAILURE_REASON_INVALID_TRANSFORM;
	    break;
	}
	directory *dir;
		dir = db_lookup(dbip, name, LOOKUP_QUIET);
		if (dir != RT_DIR_NULL) {
		    rt_db_internal intern;
		    RT_DB_INTERNAL_INIT(&intern);
		    if (rt_db_get_internal(&intern, dir, dbip,
			    bn_mat_identity) < 0)
		    {
			ret = BREP_BOOLEAN_INVALID_INPUT;
			failure_stage = RT_BREP_FAILURE_STAGE_LEAF_IMPORT;
			failure_reason = RT_BREP_FAILURE_REASON_INVALID_OBJECT;
			break;
		    }
		    RT_CK_DB_INTERNAL(&intern);
		    if (intern.idb_type == ID_HALF) {
			struct rt_half_internal *halfspace =
			    (struct rt_half_internal *)intern.idb_ptr;
			if (halfspace == NULL ||
			    halfspace->magic != RT_HALF_INTERNAL_MAGIC)
			{
			    ret = BREP_BOOLEAN_INVALID_INPUT;
			    failure_stage = RT_BREP_FAILURE_STAGE_LEAF_CONVERSION;
			    failure_reason = RT_BREP_FAILURE_REASON_INVALID_OBJECT;
			} else if (finite_context == NULL) {
			    ret = BREP_BOOLEAN_FINITE_CONTEXT_REQUIRED;
			} else {
			    bool affine_halfspace_transform = true;
			    if (t->tr_l.tl_mat != NULL &&
				!bn_mat_is_identity(t->tr_l.tl_mat))
			    {
				ON_Xform affine_transform;
				fastf_t determinant = 0.0;
				affine_halfspace_transform =
				    brlcad_mat_to_opennurbs_affine(
					affine_transform, &determinant,
					t->tr_l.tl_mat);
			    }
			    if (!affine_halfspace_transform) {
				bu_log("Halfspace B-Rep conversion rejected a "
				    "non-affine leaf transform.\n");
				ret = BREP_BOOLEAN_INVALID_INPUT;
				failure_stage = RT_BREP_FAILURE_STAGE_LEAF_TRANSFORM;
				failure_reason = RT_BREP_FAILURE_REASON_INVALID_TRANSFORM;
			    } else {
				ret = bounded_halfspace_brep(b, halfspace,
				    t->tr_l.tl_mat, finite_context, *tol,
				    regularization, bound,
				    tree_path, leaf_model_expansion) ?
				    BREP_BOOLEAN_SUCCESS :
				    BREP_BOOLEAN_INDETERMINATE;
				if (ret < BREP_BOOLEAN_SUCCESS) {
				    failure_stage =
					RT_BREP_FAILURE_STAGE_LEAF_CONVERSION;
				    failure_reason =
					RT_BREP_FAILURE_REASON_BOOLEAN_INDETERMINATE;
				}
			    }
			}
		    } else if (intern.idb_type == ID_COMBINATION &&
			!state.active_combinations.insert(dir).second)
		    {
			bu_log("Combination B-Rep conversion detected a cyclic "
			    "reference to %s.\n", name);
			ret = BREP_BOOLEAN_INVALID_INPUT;
			failure_stage = RT_BREP_FAILURE_STAGE_TREE_STRUCTURE;
			failure_reason = RT_BREP_FAILURE_REASON_INVALID_TREE;
		    } else {
			ret = single_conversion(&intern, b, dbip, tol,
			    regularization, bound, finite_context,
			    tree_path, leaf_model_expansion, state);
			if (intern.idb_type == ID_COMBINATION)
			    state.active_combinations.erase(dir);
		    }
		    if (ret == BREP_BOOLEAN_SUCCESS && *b != NULL &&
			intern.idb_type != ID_HALF)
		    {
		    if (t->tr_l.tl_mat != NULL && !bn_mat_is_identity(t->tr_l.tl_mat)) {
			    ON_Xform xform;
			    fastf_t determinant = 0.0;
			    fastf_t transform_approximation = 0.0;
			    ON_BoundingBox source_bounds;
			    if ((!brep_GetTrimmedBoundingBox(**b, source_bounds,
					false) &&
				    !(source_bounds = (*b)->BoundingBox()).IsValid()) ||
				!brlcad_mat_to_bounded_opennurbs_affine(xform,
				    &determinant, &transform_approximation,
				    t->tr_l.tl_mat, source_bounds, model_expansion,
				    *tol))
			    {
				ret = BREP_BOOLEAN_INVALID_INPUT;
				failure_stage = RT_BREP_FAILURE_STAGE_LEAF_TRANSFORM;
				failure_reason = RT_BREP_FAILURE_REASON_INVALID_TRANSFORM;
			    } else if ((*b)->Transform(xform)) {
				state.maximum_transform_approximation = std::max(
				    state.maximum_transform_approximation,
				    transform_approximation);
				if (determinant < 0.0)
				    (*b)->Flip();
				ret = converted_brep_status(*b,
				    intern.idb_meth != NULL ?
				    intern.idb_meth->ft_label : "primitive");
			    } else {
				ret = BREP_BOOLEAN_INDETERMINATE;
				failure_stage = RT_BREP_FAILURE_STAGE_LEAF_TRANSFORM;
				failure_reason = RT_BREP_FAILURE_REASON_INVALID_TRANSFORM;
			    }
			}
		    }
		    rt_db_free_internal(&intern);
		} else {
		    bu_log("Cannot find %s.\n", name);
		    ret = BREP_BOOLEAN_INVALID_INPUT;
		    failure_stage = RT_BREP_FAILURE_STAGE_LEAF_LOOKUP;
		    failure_reason = RT_BREP_FAILURE_REASON_MISSING_OBJECT;
		}
		break;
	    }
	default:
	    bu_log("OPCODE NOT IMPLEMENTED: %d\n", t->tr_op);
	    ret = BREP_BOOLEAN_UNSUPPORTED;
	    failure_stage = RT_BREP_FAILURE_STAGE_TREE_STRUCTURE;
	    failure_reason = RT_BREP_FAILURE_REASON_UNSUPPORTED_OBJECT;
    }
    if (regularization != NULL && !regularization->IsSupported()) {
	ret = BREP_BOOLEAN_UNSUPPORTED;
    }
    if (boolean_status_retryable(ret) && regularization == NULL &&
	retry_eligible && t != state.root_tree)
    {
	for (fastf_t scale : BREP_BOOLEAN_REGULARIZATION_SCALES) {
	    state.regularization_attempts++;
	    BooleanRegularization retry(scale);
	    ON_Brep *retry_owner = ON_Brep::New();
	    ON_Brep *retry_result = retry_owner;
	    bu_log("Retrying Boolean operation %d with regularized primitive "
		"operands at %.2g of modeling tolerance.\n", t->tr_op,
		static_cast<double>(scale));
	    const int retry_status = conv_tree(&retry_result, t, dbip, tol,
		&retry, bound, finite_context, tree_path, model_expansion,
		state);
	    if (!retry.IsSupported()) {
		delete_tree_conversion(retry_result, retry_owner);
		ret = BREP_BOOLEAN_UNSUPPORTED;
		break;
	    }
	    if (retry_status >= BREP_BOOLEAN_SUCCESS) {
		**b = *retry_result;
		delete_tree_conversion(retry_result, retry_owner);
		ret = retry_status;
		state.maximum_regularization = std::max(
		    state.maximum_regularization, retry.MaximumMagnitude());
		break;
	    }
	    delete_tree_conversion(retry_result, retry_owner);
	    ret = retry_status;
	    if (!boolean_status_retryable(retry_status))
		break;
	}
    }
    if (ret < BREP_BOOLEAN_SUCCESS &&
	ret != BREP_BOOLEAN_FINITE_CONTEXT_REQUIRED &&
	state.failure.stage == RT_BREP_FAILURE_STAGE_NONE)
    {
	if (failure_stage == RT_BREP_FAILURE_STAGE_NONE) {
	    if (t->tr_op == OP_DB_LEAF) {
		failure_stage = RT_BREP_FAILURE_STAGE_LEAF_CONVERSION;
		failure_reason = ret == BREP_BOOLEAN_INDETERMINATE ?
		    brep_failure_reason(*b,
			RT_BREP_FAILURE_REASON_OUTPUT_INVALID) :
		    RT_BREP_FAILURE_REASON_NONE;
	    } else if (tree_boolean_operation(t->tr_op) >= 0) {
		failure_stage = RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION;
		failure_reason = RT_BREP_FAILURE_REASON_BOOLEAN_INDETERMINATE;
	    } else {
		failure_stage = RT_BREP_FAILURE_STAGE_TREE_STRUCTURE;
		failure_reason = RT_BREP_FAILURE_REASON_INVALID_TREE;
	    }
	}
	record_tree_failure(state, failure_stage, failure_reason, ret,
	    tree_path, failure_object, tree_boolean_operation(t->tr_op),
	    regularization, NULL, NULL, *b, tol);
    }
    if (ret < BREP_BOOLEAN_SUCCESS) {
	*b = NULL;
    }

    return ret;
}


static void
initialize_conversion_info(struct rt_brep_conversion_info *info,
    enum rt_brep_conversion_fidelity fidelity)
{
    if (info == NULL)
	return;
    info->fidelity = fidelity;
    info->declared_relative_tolerance = 0.0;
    info->approximation_flags = 0;
    info->regularization_attempts = 0;
    info->whole_tree_regularization_attempts = 0;
    info->repaired_arb_count = 0;
    info->initial_status = BREP_BOOLEAN_INTERNAL_ERROR;
    initialize_failure_info(info->failure);
}


static void
mark_approximate_conversion(struct rt_brep_conversion_info &info,
    double relative_tolerance)
{
    info.fidelity = RT_BREP_CONVERSION_FIDELITY_APPROXIMATE;
    info.declared_relative_tolerance = std::max(
	info.declared_relative_tolerance, relative_tolerance);
}


static void
record_conversion_metrics(struct rt_brep_conversion_info *info,
    const BooleanConversionState &state, int initial_status)
{
    if (info == NULL)
	return;
    info->regularization_attempts = state.regularization_attempts;
    info->whole_tree_regularization_attempts =
	state.whole_tree_regularization_attempts;
    info->repaired_arb_count = state.repaired_arb_count;
    info->initial_status = initial_status;
    info->failure = state.failure;
    if (state.maximum_primitive_approximation > 0.0)
	info->approximation_flags |= RT_BREP_APPROXIMATION_PRIMITIVE;
    if (state.maximum_transform_approximation > 0.0)
	info->approximation_flags |= RT_BREP_APPROXIMATION_TRANSFORM;
    if (state.maximum_regularization > 0.0)
	info->approximation_flags |= RT_BREP_APPROXIMATION_REGULARIZATION;
    if (state.repaired_arb_count > 0)
	info->approximation_flags |= RT_BREP_APPROXIMATION_ARB_REPAIR;
}


static bool
collect_conversion_info(const union tree *subtree, const struct db_i *dbip,
    struct rt_brep_conversion_info &info,
    std::set<const struct directory *> &active_combinations,
    std::set<const struct directory *> &scanned_combinations)
{
    if (subtree == NULL || dbip == NULL)
	return false;

    switch (subtree->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    return collect_conversion_info(subtree->tr_b.tb_left, dbip, info,
		active_combinations, scanned_combinations) &&
		collect_conversion_info(subtree->tr_b.tb_right, dbip, info,
		active_combinations, scanned_combinations);
	case OP_DB_LEAF:
	    break;
	default:
	    return false;
    }

    const char *name = subtree->tr_l.tl_name;
    if (name == NULL || name[0] == '\0')
	return false;
    struct directory *directory = db_lookup(dbip, name, LOOKUP_QUIET);
    if (directory == RT_DIR_NULL)
	return false;

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    if (rt_db_get_internal(&internal, directory, dbip, bn_mat_identity) < 0)
	return false;

    bool complete = true;
    if (internal.idb_type == ID_HRT) {
	mark_approximate_conversion(info, RT_HRT_BREP_RELATIVE_TOLERANCE);
	info.approximation_flags |= RT_BREP_APPROXIMATION_PRIMITIVE;
    } else if (internal.idb_type == ID_METABALL) {
	mark_approximate_conversion(info,
	    RT_METABALL_BREP_RELATIVE_TOLERANCE);
	info.approximation_flags |= RT_BREP_APPROXIMATION_PRIMITIVE;
    } else if (internal.idb_type == ID_COMBINATION) {
	const struct rt_comb_internal *combination =
	    static_cast<const struct rt_comb_internal *>(internal.idb_ptr);
	if (combination == NULL || combination->magic != RT_COMB_MAGIC ||
	    combination->tree == NULL ||
	    active_combinations.find(directory) != active_combinations.end())
	{
	    complete = false;
	} else if (scanned_combinations.find(directory) ==
	    scanned_combinations.end())
	{
	    active_combinations.insert(directory);
	    complete = collect_conversion_info(combination->tree, dbip, info,
		active_combinations, scanned_combinations);
	    active_combinations.erase(directory);
	    if (complete)
		scanned_combinations.insert(directory);
	}
    }
    rt_db_free_internal(&internal);
    return complete;
}


int
rt_comb_brep_status_info(ON_Brep **b, const struct rt_db_internal *ip,
    const struct bn_tol *tol, const struct db_i *dbip,
    struct rt_brep_conversion_info *info)
{
    initialize_conversion_info(info, RT_BREP_CONVERSION_FIDELITY_UNKNOWN);
    BooleanConversionState state;
    const char *failure_directory = getenv(
	"LIBRT_BREP_BOOLEAN_FAILURE_DIR");
    state.capture_failure_snapshot = failure_directory != NULL &&
	failure_directory[0] != '\0' && !BU_STR_EQUAL(failure_directory, "0");
    if (b == NULL || *b == NULL || ip == NULL || dbip == NULL ||
	ip->idb_magic != RT_DB_INTERNAL_MAGIC ||
	ip->idb_type != ID_COMBINATION || ip->idb_ptr == NULL ||
	dbip->dbi_magic != DBI_MAGIC)
    {
	record_tree_failure(state, RT_BREP_FAILURE_STAGE_TREE_STRUCTURE,
	    RT_BREP_FAILURE_REASON_INVALID_TREE, BREP_BOOLEAN_INVALID_INPUT,
	    "r", NULL, -1, NULL);
	record_conversion_metrics(info, state, BREP_BOOLEAN_INVALID_INPUT);
	if (b != NULL)
	    *b = NULL;
	return BREP_BOOLEAN_INVALID_INPUT;
    }

    RT_CK_DB_INTERNAL(ip);
    RT_CK_DBI(dbip);

    struct rt_comb_internal *cip;
    cip = (struct rt_comb_internal *)ip->idb_ptr;
    if (cip->magic != RT_COMB_MAGIC) {
	record_tree_failure(state, RT_BREP_FAILURE_STAGE_TREE_STRUCTURE,
	    RT_BREP_FAILURE_REASON_INVALID_OBJECT,
	    BREP_BOOLEAN_INVALID_INPUT, "r", NULL, -1, NULL);
	record_conversion_metrics(info, state, BREP_BOOLEAN_INVALID_INPUT);
	*b = NULL;
	return BREP_BOOLEAN_INVALID_INPUT;
    }
    RT_CK_COMB(cip);

    if (cip->tree == NULL) {
	record_tree_failure(state, RT_BREP_FAILURE_STAGE_TREE_STRUCTURE,
	    RT_BREP_FAILURE_REASON_INVALID_TREE, BREP_BOOLEAN_INVALID_INPUT,
	    "r", NULL, -1, NULL);
	record_conversion_metrics(info, state, BREP_BOOLEAN_INVALID_INPUT);
	*b = NULL;
	return BREP_BOOLEAN_INVALID_INPUT;
    }

    struct bn_tol default_tol = BN_TOL_INIT_TOL;
    const struct bn_tol *effective_tol = tol ? tol : &default_tol;
    int status = BREP_BOOLEAN_INTERNAL_ERROR;
    state.root_tree = cip->tree;
    try {
	status = conv_tree(b, cip->tree, dbip, effective_tol, NULL,
	    BREP_BOOLEAN_REGULARIZATION_INNER, NULL, "r", 1.0, state);
    } catch (const std::exception &e) {
	bu_log("Combination B-Rep conversion failed: %s\n", e.what());
	status = BREP_BOOLEAN_INTERNAL_ERROR;
	record_tree_failure(state, RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION,
	    RT_BREP_FAILURE_REASON_INTERNAL_ERROR, status, "r", NULL,
	    tree_boolean_operation(cip->tree->tr_op), NULL);
    } catch (...) {
	bu_log("Combination B-Rep conversion failed with an unknown exception.\n");
	status = BREP_BOOLEAN_INTERNAL_ERROR;
	record_tree_failure(state, RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION,
	    RT_BREP_FAILURE_REASON_INTERNAL_ERROR, status, "r", NULL,
	    tree_boolean_operation(cip->tree->tr_op), NULL);
    }
    /* FINITE_CONTEXT_REQUIRED is private to the recursive evaluator.  Do not
     * leak it through public conversion metadata (or worker protocol fields)
     * when a standalone halfspace has no finite sibling context. */
    const int initial_status = status == BREP_BOOLEAN_FINITE_CONTEXT_REQUIRED ?
	BREP_BOOLEAN_UNSUPPORTED : status;
    if (initial_status < BREP_BOOLEAN_SUCCESS &&
	state.failure.stage == RT_BREP_FAILURE_STAGE_NONE)
    {
	const bool leaf = cip->tree->tr_op == OP_DB_LEAF;
	record_tree_failure(state, leaf ?
	    RT_BREP_FAILURE_STAGE_LEAF_CONVERSION :
	    RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION,
	    failure_reason_for_status(initial_status,
		RT_BREP_FAILURE_REASON_NONE), initial_status, "r",
	    leaf ? cip->tree->tr_l.tl_name : NULL,
	    tree_boolean_operation(cip->tree->tr_op), NULL, NULL, NULL,
	    b != NULL ? *b : NULL, effective_tol);
    }
    const struct rt_brep_failure_info exact_failure = state.failure;

    /* A failed intermediate often no longer contains enough usable topology
     * to prove which original faces were coincident.  Retry the complete
     * combination for every otherwise-indeterminate result, using one
     * deterministic, Boolean-role-aware perturbation plan for the whole tree.
     * This is the B-Rep analogue of facetize's region-scoped fallback. */
    if (boolean_status_retryable(status)) {
	const unsigned int subtree_attempts = state.regularization_attempts;
	unsigned int whole_tree_attempts = 0;
	for (fastf_t scale : BREP_BOOLEAN_REGULARIZATION_SCALES) {
	    whole_tree_attempts++;
	    BooleanRegularization retry(scale);
	    BooleanConversionState retry_state;
	    retry_state.root_tree = cip->tree;
	    ON_Brep *retry_owner = ON_Brep::New();
	    ON_Brep *retry_result = retry_owner;
	    bu_log("Retrying complete combination with deterministic "
		"regularized primitive instances at %.2g of modeling "
		"tolerance.\n", static_cast<double>(scale));
	    int retry_status = BREP_BOOLEAN_INTERNAL_ERROR;
	    try {
		retry_status = conv_tree(&retry_result, cip->tree, dbip,
		    effective_tol, &retry, BREP_BOOLEAN_REGULARIZATION_INNER,
		    NULL, "r", 1.0, retry_state);
	    } catch (const std::exception &error) {
		bu_log("Whole-combination B-Rep retry failed: %s\n",
		    error.what());
		retry_status = BREP_BOOLEAN_INTERNAL_ERROR;
	    } catch (...) {
		bu_log("Whole-combination B-Rep retry failed with an unknown "
		    "exception.\n");
		retry_status = BREP_BOOLEAN_INTERNAL_ERROR;
	    }

	    if (!retry.IsSupported())
		retry_status = BREP_BOOLEAN_UNSUPPORTED;
	    if (retry_status >= BREP_BOOLEAN_SUCCESS) {
		if (retry_result != retry_owner)
		    delete retry_owner;
		*b = retry_result;
		state = retry_state;
		state.root_tree = cip->tree;
		state.failure = exact_failure;
		state.regularization_attempts += subtree_attempts +
		    whole_tree_attempts;
		state.whole_tree_regularization_attempts = whole_tree_attempts;
		state.maximum_regularization = std::max(
		    state.maximum_regularization, retry.MaximumMagnitude());
		status = retry_status;
		break;
	    }

	    delete_tree_conversion(retry_result, retry_owner);
	    status = retry_status;
	    state.regularization_attempts = subtree_attempts +
		whole_tree_attempts;
	    state.whole_tree_regularization_attempts = whole_tree_attempts;
	    /* No leaf accepted an offset, so larger scales would only repeat the
	     * identical exact geometry (for example a tree made solely of B-Reps).
	     * Preserve the original result and avoid three expensive no-op passes. */
	    if (retry.MaximumMagnitude() <= 0.0) {
		status = initial_status;
		break;
	    }
	    if (!boolean_status_retryable(retry_status))
		break;
	}
    }
    if (status == BREP_BOOLEAN_FINITE_CONTEXT_REQUIRED) {
	status = BREP_BOOLEAN_UNSUPPORTED;
    }
    if (status == BREP_BOOLEAN_SUCCESS) {
	status = converted_brep_status(*b, "Combination");
	if (status < BREP_BOOLEAN_SUCCESS) {
	    record_tree_failure(state, RT_BREP_FAILURE_STAGE_OUTPUT_VALIDATION,
		brep_failure_reason(*b,
		    RT_BREP_FAILURE_REASON_OUTPUT_INVALID), status, "r", NULL,
		tree_boolean_operation(cip->tree->tr_op), NULL, NULL, NULL,
		*b, effective_tol);
	}
    }
    if (status == BREP_BOOLEAN_EMPTY && *b != NULL &&
	(*b)->m_F.Count() != 0)
    {
	bu_log("Combination B-Rep conversion reported an empty result with faces.\n");
	status = BREP_BOOLEAN_INTERNAL_ERROR;
	record_tree_failure(state, RT_BREP_FAILURE_STAGE_OUTPUT_VALIDATION,
	    RT_BREP_FAILURE_REASON_OUTPUT_INVALID, status, "r", NULL,
	    tree_boolean_operation(cip->tree->tr_op), NULL, NULL, NULL, *b,
	    effective_tol);
    }
    if (status < BREP_BOOLEAN_SUCCESS) {
	record_conversion_metrics(info, state, initial_status);
	*b = NULL;
    } else if (info != NULL) {
	initialize_conversion_info(info, RT_BREP_CONVERSION_FIDELITY_EXACT);
	std::set<const struct directory *> active_combinations;
	std::set<const struct directory *> scanned_combinations;
	try {
	    if (!collect_conversion_info(cip->tree, dbip, *info,
		    active_combinations, scanned_combinations))
	    {
		initialize_conversion_info(info,
		    RT_BREP_CONVERSION_FIDELITY_UNKNOWN);
	    }
	} catch (...) {
	    initialize_conversion_info(info,
		RT_BREP_CONVERSION_FIDELITY_UNKNOWN);
	}
	const double maximum_absolute_approximation = std::max(
	    static_cast<double>(state.maximum_regularization),
	    std::max(static_cast<double>(state.maximum_primitive_approximation),
		static_cast<double>(state.maximum_transform_approximation)));
	/* A zero-displacement ARB canonicalization preserves the point set, but
	 * it still repaired an invalid legacy encoding.  Report that intervention
	 * as approximate while retaining an honest zero declared displacement. */
	if (info->fidelity != RT_BREP_CONVERSION_FIDELITY_UNKNOWN &&
	    (maximum_absolute_approximation > 0.0 ||
	     state.repaired_arb_count > 0))
	{
	    double relative_tolerance = 0.0;
	    if (status == BREP_BOOLEAN_SUCCESS && *b != NULL) {
		const ON_BoundingBox bounds = (*b)->BoundingBox();
		const double diagonal = bounds.IsValid() ?
		    bounds.Diagonal().Length() : 0.0;
		if (std::isfinite(diagonal) && diagonal > SMALL_FASTF)
		    relative_tolerance = maximum_absolute_approximation / diagonal;
	    }
	    mark_approximate_conversion(*info, relative_tolerance);
	}
	record_conversion_metrics(info, state, initial_status);
    }
    return status;
}


int
rt_comb_brep_status(ON_Brep **b, const struct rt_db_internal *ip,
    const struct bn_tol *tol, const struct db_i *dbip)
{
    return rt_comb_brep_status_info(b, ip, tol, dbip, NULL);
}


void
rt_comb_brep(ON_Brep **b, const struct rt_db_internal *ip,
    const struct bn_tol *tol, const struct db_i *dbip)
{
    const int status = rt_comb_brep_status(b, ip, tol, dbip);
    if (status < BREP_BOOLEAN_SUCCESS) {
	bu_log("Combination B-Rep conversion failed: %s.\n",
	    ON_BooleanStatusDescription(status));
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
