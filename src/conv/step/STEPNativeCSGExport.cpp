/*             S T E P N A T I V E C S G E X P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "AP_Common.h"
#include "STEPGeneratedAPI.h"
#include "STEPNativeCSGExport.h"
#include "STEPBRepFallback.h"
#include "ON_Brep.h"
#include "Shape_Definition_Representation.h"

#include "bu/log.h"
#include "bn/mat.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/arb8.h"
#include "vmath.h"
#include "wdb.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <string>

namespace {

enum CsgNodeKind {
    CSG_NODE_INVALID = 0,
    CSG_NODE_PRIMITIVE,
    CSG_NODE_HALF_SPACE,
    CSG_NODE_SOLID_MODEL,
    CSG_NODE_BOOLEAN
};

struct CsgNode {
    CsgNodeKind kind = CSG_NODE_INVALID;
    STEPentity *entity = NULL;
    STEPentity *representation = NULL;
};

std::string
step_label(const char *value)
{
    std::string result("'");
    if (value) {
	for (const char *c = value; *c; ++c) {
	    result.push_back(*c);
	    if (*c == '\'') result.push_back('\'');
	}
    }
    result.push_back('\'');
    return result;
}

bool
near_scalar(double first, double second)
{
    const double scale = std::max(std::max(std::fabs(first), std::fabs(second)), 1.0);
    return std::fabs(first - second) <= std::max(1.0e-7, scale * 1.0e-9);
}

bool
near_point(const double *first, const double *second)
{
    return near_scalar(first[0], second[0]) &&
	near_scalar(first[1], second[1]) && near_scalar(first[2], second[2]);
}

bool
perpendicular(const double *first, const double *second)
{
    const double product = MAGNITUDE(first) * MAGNITUDE(second);
    return product > SMALL_FASTF && std::fabs(VDOT(first, second)) <= product * 1.0e-9;
}

#ifdef AP242
bool
parallel(const double *first, const double *second)
{
    const double product = MAGNITUDE(first) * MAGNITUDE(second);
    if (product <= SMALL_FASTF) return false;
    double cross[3];
    VCROSS(cross, first, second);
    return MAGNITUDE(cross) <= product * 1.0e-9;
}
#endif

bool
orthogonal_frame(const double *x, const double *y, const double *z)
{
    return perpendicular(x, y) && perpendicular(y, z) && perpendicular(z, x);
}

void
add3(double *result, const double *origin, const double *first, const double *second)
{
    VADD3(result, origin, first, second);
}

void
add4(double *result, const double *origin, const double *first,
    const double *second, const double *third)
{
    double tmp[3];
    VADD3(tmp, origin, first, second);
    VADD2(result, tmp, third);
}

STEPentity *
new_entity(AP203_Contents *sc, const char *name)
{
    return brlcad::step::CreateEntity(sc->registry, sc->instance_list, name);
}

STEPentity *
cartesian_point(AP203_Contents *sc, const double *point)
{
    STEPentity *result = new_entity(sc, "CARTESIAN_POINT");
    if (!result) return NULL;
    brlcad::step::SetString(result, "name", "");
    XYZ_to_Cartesian_point(point[0], point[1], point[2], result);
    return result;
}

STEPentity *
direction(AP203_Contents *sc, const double *vector)
{
    double normalized[3];
    VMOVE(normalized, vector);
    if (MAGNITUDE(normalized) <= SMALL_FASTF) return NULL;
    VUNITIZE(normalized);
    STEPentity *result = new_entity(sc, "DIRECTION");
    if (!result) return NULL;
    brlcad::step::SetString(result, "name", "");
    XYZ_to_Direction(normalized[0], normalized[1], normalized[2], result);
    return result;
}

STEPentity *
axis1(AP203_Contents *sc, const double *origin, const double *axis)
{
    STEPentity *location = cartesian_point(sc, origin);
    STEPentity *orientation = direction(sc, axis);
    STEPentity *placement = new_entity(sc, "AXIS1_PLACEMENT");
    if (!location || !orientation || !placement) return NULL;
    brlcad::step::SetString(placement, "name", "");
    brlcad::step::SetEntity(placement, "location", location);
    brlcad::step::SetEntity(placement, "axis", orientation);
    return placement;
}

STEPentity *
axis2(AP203_Contents *sc, const double *origin, const double *zaxis, const double *xaxis)
{
    STEPentity *location = cartesian_point(sc, origin);
    STEPentity *axis = direction(sc, zaxis);
    STEPentity *reference = direction(sc, xaxis);
    STEPentity *placement = new_entity(sc, "AXIS2_PLACEMENT_3D");
    if (!location || !axis || !reference || !placement) return NULL;
    brlcad::step::SetString(placement, "name", "");
    brlcad::step::SetEntity(placement, "location", location);
    brlcad::step::SetEntity(placement, "axis", axis);
    brlcad::step::SetEntity(placement, "ref_direction", reference);
    return placement;
}

class STEPNativeCSGExportBuilder
{
public:
    STEPNativeCSGExportBuilder(struct rt_wdb *writer, AP203_Contents *contents)
	: wdbp(writer), sc(contents)
    {
    }

    CsgNode build_directory(struct directory *dp, const mat_t transform, bool root_call = false)
    {
	if (!dp) return fail("encountered a missing database object");
	if (!(dp->d_flags & RT_DIR_COMB)) return build_solid(dp, transform, !root_call);

	const std::string name(dp->d_namep);
	if (!active_combinations.insert(name).second)
	    return fail("cyclic combination reference at " + name);

	struct rt_db_internal internal;
	if (rt_db_get_internal(&internal, dp, wdbp->dbip, bn_mat_identity) < 0) {
	    active_combinations.erase(name);
	    return fail("cannot read combination " + name);
	}
	RT_CK_DB_INTERNAL(&internal);
	if (internal.idb_minor_type != DB5_MINORTYPE_BRLCAD_COMBINATION) {
	    rt_db_free_internal(&internal);
	    active_combinations.erase(name);
	    return fail("database object changed type while reading " + name);
	}
	struct rt_comb_internal *comb = static_cast<struct rt_comb_internal *>(internal.idb_ptr);
	RT_CK_COMB(comb);
	CsgNode result = comb->tree ? build_tree(comb->tree, transform) :
	    fail("combination " + name + " has an empty tree");
	rt_db_free_internal(&internal);
	active_combinations.erase(name);
	return result;
    }

    const std::string &error() const { return diagnostic; }

private:
    struct rt_wdb *wdbp;
    AP203_Contents *sc;
    std::set<std::string> active_combinations;
    std::string diagnostic;

    CsgNode fail(const std::string &message)
    {
	if (diagnostic.empty()) diagnostic = message;
	return CsgNode();
    }

    CsgNode primitive(STEPentity *entity)
    {
	CsgNode result;
	result.kind = entity ? CSG_NODE_PRIMITIVE : CSG_NODE_INVALID;
	result.entity = entity;
	return result;
    }

    CsgNode native_ellipsoid(struct directory *dp, const struct rt_ell_internal *ell)
    {
	const double a = MAGNITUDE(ell->a);
	const double b = MAGNITUDE(ell->b);
	const double c = MAGNITUDE(ell->c);
	if (a <= SMALL_FASTF || b <= SMALL_FASTF || c <= SMALL_FASTF ||
	    !orthogonal_frame(ell->a, ell->b, ell->c))
	    return CsgNode();

	const std::string label = step_label(dp->d_namep);
	if (near_scalar(a, b) && near_scalar(b, c)) {
	    STEPentity *sphere = new_entity(sc, "SPHERE");
	    if (!sphere) return CsgNode();
	    brlcad::step::SetString(sphere, "name", label.c_str());
	    brlcad::step::SetReal(sphere, "radius", a);
	    brlcad::step::SetEntity(sphere, "centre", cartesian_point(sc, ell->v));
	    return primitive(sphere);
	}
#ifdef AP242
	STEPentity *result = new_entity(sc, "ELLIPSOID");
	if (!result) return CsgNode();
	brlcad::step::SetString(result, "name", label.c_str());
	brlcad::step::SetEntity(result, "position", axis2(sc, ell->v, ell->c, ell->a));
	brlcad::step::SetReal(result, "semi_axis_1", a);
	brlcad::step::SetReal(result, "semi_axis_2", b);
	brlcad::step::SetReal(result, "semi_axis_3", c);
	return primitive(result);
#else
	return CsgNode();
#endif
    }

    CsgNode native_tgc(struct directory *dp, const struct rt_tgc_internal *tgc)
    {
	const double height = MAGNITUDE(tgc->h);
	const double ar = MAGNITUDE(tgc->a);
	const double br = MAGNITUDE(tgc->b);
	const double cr = MAGNITUDE(tgc->c);
	const double dr = MAGNITUDE(tgc->d);
	const std::string label = step_label(dp->d_namep);
	const bool circular = height > SMALL_FASTF && ar > SMALL_FASTF && br > SMALL_FASTF &&
	    near_scalar(ar, br) && near_scalar(cr, dr) && perpendicular(tgc->a, tgc->b) &&
	    perpendicular(tgc->a, tgc->h) && perpendicular(tgc->b, tgc->h) &&
	    (cr <= SMALL_FASTF || (perpendicular(tgc->c, tgc->d) &&
		perpendicular(tgc->c, tgc->h) && perpendicular(tgc->d, tgc->h)));
	if (circular) {
	    STEPentity *placement = axis1(sc, tgc->v, tgc->h);
	    if (!placement) return CsgNode();
	    if (near_scalar(ar, cr)) {
		STEPentity *cylinder = new_entity(sc, "RIGHT_CIRCULAR_CYLINDER");
		if (!cylinder) return CsgNode();
		brlcad::step::SetString(cylinder, "name", label.c_str());
		brlcad::step::SetEntity(cylinder, "position", placement);
		brlcad::step::SetReal(cylinder, "height", height);
		brlcad::step::SetReal(cylinder, "radius", ar);
		return primitive(cylinder);
	    }

	    STEPentity *cone = new_entity(sc, "RIGHT_CIRCULAR_CONE");
	    if (!cone) return CsgNode();
	    brlcad::step::SetString(cone, "name", label.c_str());
	    brlcad::step::SetEntity(cone, "position", placement);
	    brlcad::step::SetReal(cone, "height", height);
	    brlcad::step::SetReal(cone, "radius", ar);
	    brlcad::step::SetReal(cone, "semi_angle",
		    std::atan2(cr - ar, height) * sc->radians_to_plane_angle);
	    return primitive(cone);
	}

#ifdef AP242
	/* AP242's ECCENTRIC_CONE is an elliptical frustum whose top ellipse
	 * retains the base orientation and scales uniformly, with an optional
	 * lateral offset.  That is an exact subset of BRL-CAD's TGC. */
	if (ar <= SMALL_FASTF || br <= SMALL_FASTF || !perpendicular(tgc->a, tgc->b))
	    return CsgNode();
	double xaxis[3], yaxis[3], zaxis[3];
	VSCALE(xaxis, tgc->a, 1.0 / ar);
	VSCALE(yaxis, tgc->b, 1.0 / br);
	VCROSS(zaxis, xaxis, yaxis);
	VUNITIZE(zaxis);
	if (VDOT(tgc->h, zaxis) < 0.0) {
	    VREVERSE(zaxis, zaxis);
	    VREVERSE(yaxis, yaxis);
	}
	const double axial_height = VDOT(tgc->h, zaxis);
	if (axial_height <= SMALL_FASTF) return CsgNode();
	const double ratio_a = cr / ar;
	const double ratio_b = dr / br;
	if (!near_scalar(ratio_a, ratio_b) || ratio_a < 0.0 ||
	    ((cr > SMALL_FASTF || dr > SMALL_FASTF) &&
	    (!parallel(tgc->c, xaxis) || !parallel(tgc->d, yaxis))))
	    return CsgNode();
	double reconstructed[3], lateral_x[3], lateral_y[3], vertical[3];
	const double x_offset = VDOT(tgc->h, xaxis);
	const double y_offset = VDOT(tgc->h, yaxis);
	VSCALE(lateral_x, xaxis, x_offset);
	VSCALE(lateral_y, yaxis, y_offset);
	VSCALE(vertical, zaxis, axial_height);
	VADD3(reconstructed, lateral_x, lateral_y, vertical);
	if (!near_point(reconstructed, tgc->h)) return CsgNode();

	STEPentity *cone = new_entity(sc, "ECCENTRIC_CONE");
	if (!cone) return CsgNode();
	brlcad::step::SetString(cone, "name", label.c_str());
	brlcad::step::SetEntity(cone, "position", axis2(sc, tgc->v, zaxis, xaxis));
	brlcad::step::SetReal(cone, "semi_axis_1", ar);
	brlcad::step::SetReal(cone, "semi_axis_2", br);
	brlcad::step::SetReal(cone, "height", axial_height);
	brlcad::step::SetReal(cone, "x_offset", x_offset);
	brlcad::step::SetReal(cone, "y_offset", y_offset);
	brlcad::step::SetReal(cone, "ratio", ratio_a);
	return primitive(cone);
#else
	return CsgNode();
#endif
    }

    bool block_parameters(const struct rt_arb_internal *arb, double *origin,
	    double *x, double *y, double *z) const
    {
	VMOVE(origin, arb->pt[0]);
	VSUB2(x, arb->pt[1], origin);
	VSUB2(y, arb->pt[3], origin);
	VSUB2(z, arb->pt[4], origin);
	if (!orthogonal_frame(x, y, z)) return false;
	double expected[3];
	add3(expected, origin, x, y);
	if (!near_point(expected, arb->pt[2])) return false;
	add3(expected, origin, x, z);
	if (!near_point(expected, arb->pt[5])) return false;
	add4(expected, origin, x, y, z);
	if (!near_point(expected, arb->pt[6])) return false;
	add3(expected, origin, y, z);
	return near_point(expected, arb->pt[7]);
    }

    bool wedge_parameters(const struct rt_arb_internal *arb, double *origin,
	    double *x, double *y, double *z, double &ltx) const
    {
	VMOVE(origin, arb->pt[0]);
	VSUB2(x, arb->pt[1], origin);
	VSUB2(y, arb->pt[3], origin);
	VSUB2(z, arb->pt[4], origin);
	if (!orthogonal_frame(x, y, z)) return false;
	double expected[3], top_x[3];
	add3(expected, origin, x, y);
	if (!near_point(expected, arb->pt[2])) return false;
	VSUB2(top_x, arb->pt[5], arb->pt[4]);
	ltx = MAGNITUDE(top_x);
	if (ltx < 0.0 || ltx >= MAGNITUDE(x) ||
	    (ltx > SMALL_FASTF && (!perpendicular(top_x, y) ||
		!perpendicular(top_x, z) || VDOT(top_x, x) <= 0.0)))
	    return false;
	if (ltx > SMALL_FASTF) {
	    double scaled[3];
	    VSCALE(scaled, x, ltx / MAGNITUDE(x));
	    if (!near_point(scaled, top_x)) return false;
	}
	VADD2(expected, arb->pt[5], y);
	if (!near_point(expected, arb->pt[6])) return false;
	add3(expected, origin, y, z);
	return near_point(expected, arb->pt[7]);
    }

#ifdef AP242
    bool pyramid_parameters(const struct rt_arb_internal *arb, double *origin,
	    double *x, double *y, double *height) const
    {
	VMOVE(origin, arb->pt[0]);
	VSUB2(x, arb->pt[1], origin);
	VSUB2(y, arb->pt[3], origin);
	if (!perpendicular(x, y)) return false;
	double expected[3], centre[3], half_x[3], half_y[3];
	add3(expected, origin, x, y);
	if (!near_point(expected, arb->pt[2])) return false;
	VSCALE(half_x, x, 0.5);
	VSCALE(half_y, y, 0.5);
	add3(centre, origin, half_x, half_y);
	VSUB2(height, arb->pt[4], centre);
	return MAGNITUDE(height) > SMALL_FASTF && perpendicular(height, x) &&
	    perpendicular(height, y);
    }
#endif

    void right_handed_origin(double *origin, double *x, double *y, const double *z) const
    {
	double derived_y[3];
	VCROSS(derived_y, z, x);
	if (VDOT(derived_y, y) < 0.0) {
	    VADD2(origin, origin, y);
	    VREVERSE(y, y);
	}
    }

    CsgNode native_arb8(struct directory *dp, const struct rt_arb_internal *arb,
	int arb_type)
    {
	double origin[3], x[3], y[3], z[3];
	const std::string label = step_label(dp->d_namep);
	if (block_parameters(arb, origin, x, y, z)) {
	    right_handed_origin(origin, x, y, z);
	    STEPentity *block = new_entity(sc, "BLOCK");
	    if (!block) return CsgNode();
	    brlcad::step::SetString(block, "name", label.c_str());
	    brlcad::step::SetEntity(block, "position", axis2(sc, origin, z, x));
	    brlcad::step::SetReal(block, "x", MAGNITUDE(x));
	    brlcad::step::SetReal(block, "y", MAGNITUDE(y));
	    brlcad::step::SetReal(block, "z", MAGNITUDE(z));
	    return primitive(block);
	}

	double ltx = 0.0;
	if (wedge_parameters(arb, origin, x, y, z, ltx)) {
	    right_handed_origin(origin, x, y, z);
	    STEPentity *wedge = new_entity(sc, "RIGHT_ANGULAR_WEDGE");
	    if (!wedge) return CsgNode();
	    brlcad::step::SetString(wedge, "name", label.c_str());
	    brlcad::step::SetEntity(wedge, "position", axis2(sc, origin, z, x));
	    brlcad::step::SetReal(wedge, "x", MAGNITUDE(x));
	    brlcad::step::SetReal(wedge, "y", MAGNITUDE(y));
	    brlcad::step::SetReal(wedge, "z", MAGNITUDE(z));
	    brlcad::step::SetReal(wedge, "ltx", ltx);
	    return primitive(wedge);
	}

#ifdef AP242
	struct bn_tol tolerance;
	tolerance.magic = BN_TOL_MAGIC;
	tolerance.dist = BN_TOL_DIST;
	tolerance.dist_sq = tolerance.dist * tolerance.dist;
	tolerance.perp = SMALL_FASTF;
	tolerance.para = 1.0 - tolerance.perp;
	if (rt_arb_validate(NULL, arb, &tolerance, NULL) != 0) return CsgNode();

	/* AP242's pyramid has a rectangular base starting at position.location
	 * and an apex directly above its centre. */
	if (arb_type == 5 && pyramid_parameters(arb, origin, x, y, z)) {
	    right_handed_origin(origin, x, y, z);
	    STEPentity *pyramid = new_entity(sc, "RECTANGULAR_PYRAMID");
	    if (!pyramid) return CsgNode();
	    brlcad::step::SetString(pyramid, "name", label.c_str());
	    brlcad::step::SetEntity(pyramid, "position", axis2(sc, origin, z, x));
	    brlcad::step::SetReal(pyramid, "xlength", MAGNITUDE(x));
	    brlcad::step::SetReal(pyramid, "ylength", MAGNITUDE(y));
	    brlcad::step::SetReal(pyramid, "height", MAGNITUDE(z));
	    return primitive(pyramid);
	}

	/* A valid standard ARB4 is precisely a tetrahedron, and a valid ARB8
	 * is the convex hexahedron defined by AP242.  ARB6-ARB7 do not match a
	 * faceted_primitive subtype in this schema. */
	if (arb_type != 4 && arb_type != 8) return CsgNode();
	STEPentity *faceted = new_entity(sc, arb_type == 4 ?
	    "TETRAHEDRON" : "CONVEX_HEXAHEDRON");
	if (!faceted) return CsgNode();
	brlcad::step::SetString(faceted, "name", label.c_str());
	static const int tetrahedron_vertices[4] = {0, 1, 2, 4};
	const int point_count = arb_type == 4 ? 4 : 8;
	for (int i = 0; i < point_count; ++i) {
	    const int index = arb_type == 4 ? tetrahedron_vertices[i] : i;
	    STEPentity *point = cartesian_point(sc, arb->pt[index]);
	    if (!point) return CsgNode();
	    brlcad::step::AddEntity(faceted, "points", point);
	}
	return primitive(faceted);
#else
	(void)arb_type;
	return CsgNode();
#endif
    }

    CsgNode native_torus(struct directory *dp, const struct rt_tor_internal *tor)
    {
	if (tor->r_a <= SMALL_FASTF || tor->r_h <= SMALL_FASTF || tor->r_h >= tor->r_a ||
	    MAGNITUDE(tor->h) <= SMALL_FASTF)
	    return CsgNode();
	STEPentity *result = new_entity(sc, "TORUS");
	if (!result) return CsgNode();
	const std::string label = step_label(dp->d_namep);
	brlcad::step::SetString(result, "name", label.c_str());
	brlcad::step::SetEntity(result, "position", axis1(sc, tor->v, tor->h));
	brlcad::step::SetReal(result, "major_radius", tor->r_a);
	brlcad::step::SetReal(result, "minor_radius", tor->r_h);
	return primitive(result);
    }

    CsgNode native_half(struct directory *dp, const struct rt_half_internal *half)
    {
	double normal[3] = {half->eqn[0], half->eqn[1], half->eqn[2]};
	const double magnitude = MAGNITUDE(normal);
	if (magnitude <= SMALL_FASTF) return CsgNode();
	VUNITIZE(normal);
	double origin[3];
	VSCALE(origin, normal, half->eqn[3] / magnitude);
	double xaxis[3];
	bn_vec_ortho(xaxis, normal);
	STEPentity *plane = new_entity(sc, "PLANE");
	STEPentity *result = new_entity(sc, "HALF_SPACE_SOLID");
	if (!plane || !result) return CsgNode();
	const std::string label = step_label(dp->d_namep);
	brlcad::step::SetString(plane, "name", label.c_str());
	brlcad::step::SetEntity(plane, "position", axis2(sc, origin, normal, xaxis));
	brlcad::step::SetString(result, "name", label.c_str());
	brlcad::step::SetEntity(result, "base_surface", plane);
	/* BRL-CAD HALF stores N.P <= d; agreement TRUE has the same outward N. */
	brlcad::step::SetBoolean(result, "agreement_flag", BTrue);
	CsgNode node;
	node.kind = CSG_NODE_HALF_SPACE;
	node.entity = result;
	return node;
    }

    CsgNode native_primitive(struct directory *dp, const struct rt_db_internal *internal)
    {
	switch (internal->idb_minor_type) {
	    case DB5_MINORTYPE_BRLCAD_ELL:
		return native_ellipsoid(dp,
		    static_cast<const struct rt_ell_internal *>(internal->idb_ptr));
	    case DB5_MINORTYPE_BRLCAD_TGC:
		return native_tgc(dp,
		    static_cast<const struct rt_tgc_internal *>(internal->idb_ptr));
	    case DB5_MINORTYPE_BRLCAD_ARB8: {
		struct bn_tol tolerance;
		tolerance.magic = BN_TOL_MAGIC;
		tolerance.dist = BN_TOL_DIST;
		tolerance.dist_sq = tolerance.dist * tolerance.dist;
		tolerance.perp = SMALL_FASTF;
		tolerance.para = 1.0 - tolerance.perp;
		const int arb_type = rt_arb_std_type(internal, &tolerance);
		return native_arb8(dp,
		    static_cast<const struct rt_arb_internal *>(internal->idb_ptr), arb_type);
	    }
	    case DB5_MINORTYPE_BRLCAD_TOR:
		if (internal->idb_avs.magic == BU_AVS_MAGIC &&
		    bu_avs_get(&internal->idb_avs, "matrix:nonuniform"))
		    return CsgNode();
		return native_torus(dp,
		    static_cast<const struct rt_tor_internal *>(internal->idb_ptr));
	    case DB5_MINORTYPE_BRLCAD_HALF:
		return native_half(dp,
		    static_cast<const struct rt_half_internal *>(internal->idb_ptr));
	    default:
		return CsgNode();
	}
    }

    CsgNode brep_operand(struct directory *dp, struct rt_db_internal *internal)
    {
	if (!internal->idb_meth || !internal->idb_meth->ft_brep)
	    return fail(std::string("no exact AP CSG primitive or BRep conversion for ") +
		dp->d_namep + " (" + (internal->idb_meth ? internal->idb_meth->ft_label : "unknown") + ")");
	struct bn_tol tolerance;
	tolerance.magic = BN_TOL_MAGIC;
	tolerance.dist = BN_TOL_DIST;
	tolerance.dist_sq = tolerance.dist * tolerance.dist;
	tolerance.perp = SMALL_FASTF;
	tolerance.para = 1.0 - tolerance.perp;
	ON_Brep *brep = brlcad::step::BRepFallback(internal, &tolerance);
	if (!brep) return fail(std::string("BRep conversion failed for ") + dp->d_namep);
	if (!brep->IsValid() || !brep->IsSolid()) {
	    delete brep;
	    return fail(std::string("BRep fallback for ") + dp->d_namep +
		" is not a valid closed solid and cannot be a STEP boolean operand");
	}
	STEPentity *shape = NULL;
	STEPentity *product = NULL;
	STEPentity *manifold = NULL;
	std::string brep_diagnostic;
	const bool brep_written = ON_BRep_to_STEP(dp, brep, sc, &shape,
	    &product, &manifold, false, &brep_diagnostic);
	delete brep;
	if (!brep_written || !shape || !manifold)
	    return fail(std::string("STEP BRep materialization failed for ") +
		dp->d_namep + (brep_diagnostic.empty() ? std::string() :
		std::string(": ") + brep_diagnostic));
	CsgNode result;
	result.kind = CSG_NODE_SOLID_MODEL;
	result.entity = manifold;
	result.representation = shape;
	return result;
    }

    CsgNode build_solid(struct directory *dp, const mat_t transform, bool allow_brep)
    {
	struct rt_db_internal internal;
	if (rt_db_get_internal(&internal, dp, wdbp->dbip, transform) < 0)
	    return fail(std::string("cannot read solid ") + dp->d_namep);
	RT_CK_DB_INTERNAL(&internal);
	CsgNode result = native_primitive(dp, &internal);
	if (allow_brep && result.kind == CSG_NODE_INVALID &&
	    internal.idb_minor_type != DB5_MINORTYPE_BRLCAD_HALF)
	    result = brep_operand(dp, &internal);
	if (allow_brep && result.kind == CSG_NODE_INVALID && diagnostic.empty())
	    diagnostic = std::string("cannot represent ") + dp->d_namep + " as a CSG operand";
	rt_db_free_internal(&internal);
	return result;
    }

    CsgNode boolean_result(int operation, const CsgNode &left, const CsgNode &right)
    {
	STEPentity *result = new_entity(sc, "BOOLEAN_RESULT");
	if (!result) return fail("could not allocate BOOLEAN_RESULT");
	brlcad::step::SetString(result, "name", "");
	if (!brlcad::step::SetEntity(result, "first_operand", left.entity) ||
	    !brlcad::step::SetEntity(result, "second_operand", right.entity))
	    return fail("could not materialize a STEP boolean operand SELECT");
	switch (operation) {
	    case OP_UNION:
		brlcad::step::SetEnum(result, "operator", "UNION");
		break;
	    case OP_INTERSECT:
		brlcad::step::SetEnum(result, "operator", "INTERSECTION");
		break;
	    case OP_SUBTRACT:
		brlcad::step::SetEnum(result, "operator", "DIFFERENCE");
		break;
	    default:
		return fail("unsupported boolean operation while constructing BOOLEAN_RESULT");
	}
	CsgNode node;
	node.kind = CSG_NODE_BOOLEAN;
	node.entity = result;
	return node;
    }

    CsgNode build_tree(const union tree *tree, const mat_t transform)
    {
	if (!tree) return fail("encountered a null boolean-tree node");
	RT_CK_TREE(tree);
	switch (tree->tr_op) {
	    case OP_DB_LEAF: {
		struct directory *child = db_lookup(wdbp->dbip, tree->tr_l.tl_name, LOOKUP_QUIET);
		if (child == RT_DIR_NULL)
		    return fail(std::string("cannot find boolean-tree leaf ") + tree->tr_l.tl_name);
		mat_t combined;
		if (tree->tr_l.tl_mat)
		    bn_mat_mul(combined, transform, tree->tr_l.tl_mat);
		else
		    MAT_COPY(combined, transform);
		return build_directory(child, combined, false);
	    }
	    case OP_UNION:
	    case OP_INTERSECT:
	    case OP_SUBTRACT: {
		CsgNode left = build_tree(tree->tr_b.tb_left, transform);
		CsgNode right = build_tree(tree->tr_b.tb_right, transform);
		if (!left.entity || !right.entity) return CsgNode();
		return boolean_result(tree->tr_op, left, right);
	    }
	    case OP_XOR: {
		/* AP203e2/AP214/AP242 omit XOR, but its exact set identity is expressible. */
		CsgNode left = build_tree(tree->tr_b.tb_left, transform);
		CsgNode right = build_tree(tree->tr_b.tb_right, transform);
		if (!left.entity || !right.entity) return CsgNode();
		CsgNode either = boolean_result(OP_UNION, left, right);
		CsgNode both = boolean_result(OP_INTERSECT, left, right);
		return either.entity && both.entity ? boolean_result(OP_SUBTRACT, either, both) : CsgNode();
	    }
	    case OP_NOT:
		return fail("STEP CSG has no finite universal-set operand for BRL-CAD NOT");
	    case OP_GUARD:
	    case OP_XNOP:
		return fail("BRL-CAD guard/no-op tree nodes have no lossless AP CSG encoding");
	    default: {
		std::ostringstream message;
		message << "unsupported BRL-CAD boolean opcode " << tree->tr_op;
		return fail(message.str());
	    }
	}
    }
};

} // namespace

StepNativeCsgStatus
ExportSTEPNativeCSG(struct directory *dp, struct rt_wdb *wdbp,
    AP203_Contents *sc, std::string &diagnostic)
{
    diagnostic.clear();
    if (!dp || !wdbp || !sc || !sc->registry || !sc->instance_list) {
	diagnostic = "native CSG exporter received incomplete conversion state";
	return STEP_NATIVE_CSG_ERROR;
    }

    mat_t output_scale;
    MAT_IDN(output_scale);
    output_scale[15] = sc->length_unit_mm;
    STEPNativeCSGExportBuilder builder(wdbp, sc);
    CsgNode root = builder.build_directory(dp, output_scale, true);
    if (!root.entity) {
	diagnostic = builder.error();
	if (diagnostic.empty()) {
	    diagnostic = "object has no exact implicit CSG root; using its ordinary BRep export path";
	    return STEP_NATIVE_CSG_NOT_APPLICABLE;
	}
	return STEP_NATIVE_CSG_ERROR;
    }
    const bool legal_root = root.kind == CSG_NODE_BOOLEAN ||
	root.kind == CSG_NODE_PRIMITIVE;
    if (!legal_root) {
	if (root.kind == CSG_NODE_HALF_SPACE) {
	    diagnostic = "an unbounded HALF_SPACE_SOLID is legal only as a boolean operand";
	    return STEP_NATIVE_CSG_ERROR;
	}
	/* A one-leaf combination may have needed the BRep operand fallback.  A
	 * SOLID_MODEL is legal inside a BOOLEAN_RESULT but not as a CSG_SOLID
	 * root.  Reuse that already-created representation as the product's
	 * ordinary BRep representation instead of constructing it a second time.
	 */
	if (root.kind == CSG_NODE_SOLID_MODEL && root.representation) {
	    STEPentity *product = Add_Shape_Definition_Representation(dp, sc,
		root.representation);
	    if (!product) {
		diagnostic = "could not relate the BRep CSG fallback to a product";
		return STEP_NATIVE_CSG_ERROR;
	    }
	    if (dp->d_flags & RT_DIR_COMB) {
		(*sc->comb_to_step)[dp] = product;
		(*sc->comb_to_step_shape)[dp] = root.representation;
		(*sc->comb_to_step_manifold)[dp] = root.entity;
	    } else {
		(*sc->solid_to_step)[dp] = product;
		(*sc->solid_to_step_shape)[dp] = root.representation;
		(*sc->solid_to_step_manifold)[dp] = root.entity;
	    }
	    diagnostic = "object has no legal CSG_SOLID root; exported its exact BRep representation";
	    return STEP_NATIVE_CSG_SUCCESS;
	}
	diagnostic = "object has no legal CSG_SOLID root; using its ordinary BRep export path";
	return STEP_NATIVE_CSG_NOT_APPLICABLE;
    }

    const std::string label = step_label(dp->d_namep);
    STEPentity *solid = new_entity(sc, "CSG_SOLID");
    STEPentity *representation = new_entity(sc, "CSG_SHAPE_REPRESENTATION");
    if (!solid || !representation) {
	diagnostic = "could not allocate the AP CSG representation root";
	return STEP_NATIVE_CSG_ERROR;
    }
    brlcad::step::SetString(solid, "name", label.c_str());
    if (!brlcad::step::SetEntity(solid, "tree_root_expression", root.entity)) {
	diagnostic = "could not assign the CSG root expression";
	return STEP_NATIVE_CSG_ERROR;
    }
    brlcad::step::SetString(representation, "name", label.c_str());
    if (!brlcad::step::AddEntity(representation, "items", solid)) {
	diagnostic = "could not add the CSG representation item";
	return STEP_NATIVE_CSG_ERROR;
    }
    brlcad::step::SetEntity(representation, "context_of_items",
	    sc->default_context);

    STEPentity *product = Add_Shape_Definition_Representation(dp, sc, representation);
    if (!product) {
	diagnostic = "could not relate the AP CSG representation to a product";
	return STEP_NATIVE_CSG_ERROR;
    }
    if (dp->d_flags & RT_DIR_COMB) {
	(*sc->comb_to_step)[dp] = product;
	(*sc->comb_to_step_shape)[dp] = representation;
	(*sc->comb_to_step_manifold)[dp] = solid;
    } else {
	(*sc->solid_to_step)[dp] = product;
	(*sc->solid_to_step_shape)[dp] = representation;
	(*sc->solid_to_step_manifold)[dp] = solid;
    }
    return STEP_NATIVE_CSG_SUCCESS;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
