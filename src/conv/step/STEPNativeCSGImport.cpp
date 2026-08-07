/*             S T E P N A T I V E C S G I M P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "STEPNativeCSGImport.h"
#include "BRLCADWrapper.h"
#include "STEPGeneratedAPI.h"
#include "STEPString.h"
#include "STEPWrapper.h"
#include "ap_schema.h"

#include "GlobalUnitAssignedContext.h"
#include "Factory.h"
#include "SolidModel.h"

#include "vmath.h"
#include "rt/primitives/arb8.h"
#include "opennurbs.h"

#include <cmath>
#include <map>
#include <set>

namespace {

STEPentity *
csg_product_definition(STEPWrapper &wrapper, SDAI_Select *definition)
{
    if (!definition) return NULL;
    STEPentity *selected = dynamic_cast<STEPentity *>(
	brlcad::step::SelectedEntity(definition));
    if (selected && wrapper.IsSchemaEntity(selected, "PRODUCT_DEFINITION"))
	return selected;
    if (selected && wrapper.IsSchemaEntity(selected, "PRODUCT_DEFINITION_SHAPE"))
	return csg_product_definition(wrapper,
	    wrapper.getSelectAttribute(selected, "definition"));
    return NULL;
}

int64_t
csg_product_id(STEPentity *definition)
{
    STEPentity *formation = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(definition, "formation"));
    STEPentity *product = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(formation, "of_product"));
    return product ? product->STEPfile_id : 0;
}

bool
point_coordinates(STEPentity *point, double *coordinates)
{
    STEPaggregate *values = brlcad::step::Aggregate(point, "coordinates");
    RealNode *node = values ? static_cast<RealNode *>(values->GetHead()) : NULL;
    size_t count = 0;
    while (node && count < 3) {
	coordinates[count++] = node->value;
	node = static_cast<RealNode *>(node->NextNode());
    }
    while (count < 3) coordinates[count++] = 0.0;
    return values != NULL;
}

bool
direction_ratios(STEPentity *direction, double *ratios)
{
    STEPaggregate *values = brlcad::step::Aggregate(direction, "direction_ratios");
    RealNode *node = values ? static_cast<RealNode *>(values->GetHead()) : NULL;
    size_t count = 0;
    while (node && count < 3) {
	ratios[count++] = node->value;
	node = static_cast<RealNode *>(node->NextNode());
    }
    while (count < 3) ratios[count++] = 0.0;
    if (!values || MAGNITUDE(ratios) <= SMALL_FASTF) return false;
    VUNITIZE(ratios);
    return true;
}

bool
axis1_placement(STEPentity *placement, double length,
    double *origin, double *axis)
{
    STEPentity *location = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(placement, "location"));
    if (!location || !point_coordinates(location, origin)) return false;
    VSCALE(origin, origin, length);
    VSET(axis, 0.0, 0.0, 1.0);
    STEPentity *direction = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(placement, "axis"));
    return !direction || direction_ratios(direction, axis);
}

bool
axis2_placement(STEPentity *placement, double length,
    double *origin, double *xaxis, double *yaxis, double *zaxis)
{
    STEPentity *location = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(placement, "location"));
    if (!location || !point_coordinates(location, origin)) return false;
    VSCALE(origin, origin, length);
    VSET(zaxis, 0.0, 0.0, 1.0);
    VSET(xaxis, 1.0, 0.0, 0.0);
    STEPentity *axis = dynamic_cast<STEPentity *>(brlcad::step::Entity(placement, "axis"));
    STEPentity *reference = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(placement, "ref_direction"));
    if (axis && !direction_ratios(axis, zaxis)) return false;
    if (reference && !direction_ratios(reference, xaxis)) return false;
    VJOIN1(xaxis, xaxis, -(VDOT(xaxis, zaxis)), zaxis);
    if (MAGNITUDE(xaxis) <= SMALL_FASTF) return false;
    VUNITIZE(xaxis);
    VCROSS(yaxis, zaxis, xaxis);
    if (MAGNITUDE(yaxis) <= SMALL_FASTF) return false;
    VUNITIZE(yaxis);
    return true;
}

struct RepresentationUnits {
    double length = 1000.0;
    double plane_angle = 1.0;
};

RepresentationUnits
csg_representation_units(STEPWrapper &wrapper, STEPentity *representation)
{
    RepresentationUnits result;
    STEPentity *context = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(representation, "context_of_items"));
    if (!context) return result;
    GlobalUnitAssignedContext units(&wrapper, context->STEPfile_id);
    if (!units.Load(&wrapper, context)) return result;
    const double length = units.GetLengthConversionFactor();
    const double angle = units.GetPlaneAngleConversionFactor();
    if (length > 0.0) result.length = length;
    if (angle > 0.0) result.plane_angle = angle;
    return result;
}

struct STEPNativeCSGImporter {
    STEPWrapper &wrapper;
    BRLCADWrapper &database;
    std::string product_name;
    double length;
    double plane_angle;
    std::map<int64_t, std::string> converted;
    std::set<int64_t> active;

    STEPNativeCSGImporter(STEPWrapper &w, BRLCADWrapper &d, const std::string &name,
	const RepresentationUnits &units)
	: wrapper(w), database(d), product_name(name), length(units.length),
	  plane_angle(units.plane_angle) {}

    static STEPentity *primitive_entity(SDAI_Select *select)
    {
	return dynamic_cast<STEPentity *>(brlcad::step::SelectedEntity(select));
    }

#ifdef AP242
    bool faceted_geometry(STEPentity *faceted, double *points) const
    {
	const bool tetrahedron = wrapper.IsSchemaEntity(faceted, "TETRAHEDRON");
	const bool hexahedron = wrapper.IsSchemaEntity(faceted, "CONVEX_HEXAHEDRON");
	if (!tetrahedron && !hexahedron) return false;
	const size_t expected = tetrahedron ? 4 : 8;
	const std::vector<SDAI_Application_instance *> point_entities =
	    brlcad::step::Entities(faceted, "points");
	double source[24] = {0.0};
	size_t count = 0;
	for (std::vector<SDAI_Application_instance *>::const_iterator i =
		point_entities.begin(); i != point_entities.end() && count < expected; ++i) {
	    STEPentity *point = dynamic_cast<STEPentity *>(*i);
	    if (!point || !point_coordinates(point, &source[count * 3])) return false;
	    for (size_t coordinate = 0; coordinate < 3; ++coordinate)
		if (!std::isfinite(source[count * 3 + coordinate])) return false;
	    VSCALE(&source[count * 3], &source[count * 3], length);
	    ++count;
	}
	if (count != expected || point_entities.size() != expected) return false;
	if (tetrahedron) {
	    VMOVE(&points[0], &source[0]);
	    VMOVE(&points[3], &source[3]);
	    VMOVE(&points[6], &source[6]);
	    VMOVE(&points[9], &source[6]);
	    for (size_t i = 4; i < 8; ++i) VMOVE(&points[i * 3], &source[9]);
	} else {
	    for (size_t i = 0; i < 8; ++i) VMOVE(&points[i * 3], &source[i * 3]);
	}

	struct rt_arb_internal arb;
	arb.magic = RT_ARB_INTERNAL_MAGIC;
	for (size_t i = 0; i < 8; ++i) VMOVE(arb.pt[i], &points[i * 3]);
	struct bn_tol tolerance;
	tolerance.magic = BN_TOL_MAGIC;
	tolerance.dist = BN_TOL_DIST;
	tolerance.dist_sq = tolerance.dist * tolerance.dist;
	tolerance.perp = SMALL_FASTF;
	tolerance.para = 1.0 - tolerance.perp;
	return rt_arb_validate(NULL, &arb, &tolerance, NULL) == 0;
    }
#endif

    bool validate_primitive(SDAI_Select *select) const
    {
	STEPentity *entity = primitive_entity(select);
	if (!entity || entity->STEPfile_id <= 0 || !std::isfinite(length) || length <= 0.0)
	    return false;
	double origin[3], axis[3], xaxis[3], yaxis[3], zaxis[3];
	if (wrapper.IsSchemaEntity(entity, "SPHERE")) {
	    STEPentity *centre = dynamic_cast<STEPentity *>(
		brlcad::step::Entity(entity, "centre"));
	    const double radius = wrapper.getRealAttribute(entity, "radius");
	    return point_coordinates(centre, origin) && std::isfinite(radius) && radius > 0.0;
	}
#ifdef AP242
	if (wrapper.IsSchemaEntity(entity, "RECTANGULAR_PYRAMID")) {
	    const double x = wrapper.getRealAttribute(entity, "xlength");
	    const double y = wrapper.getRealAttribute(entity, "ylength");
	    const double h = wrapper.getRealAttribute(entity, "height");
	    return axis2_placement(dynamic_cast<STEPentity *>(
		brlcad::step::Entity(entity, "position")), length, origin, xaxis, yaxis, zaxis) &&
		std::isfinite(x) && std::isfinite(y) && std::isfinite(h) &&
		x > 0.0 && y > 0.0 && h > 0.0;
	}
	if (wrapper.IsSchemaEntity(entity, "FACETED_PRIMITIVE")) {
	    double points[24];
	    return faceted_geometry(entity, points);
	}
	if (wrapper.IsSchemaEntity(entity, "ECCENTRIC_CONE")) {
	    const double a = wrapper.getRealAttribute(entity, "semi_axis_1");
	    const double b = wrapper.getRealAttribute(entity, "semi_axis_2");
	    const double h = wrapper.getRealAttribute(entity, "height");
	    const double xo = wrapper.getRealAttribute(entity, "x_offset");
	    const double yo = wrapper.getRealAttribute(entity, "y_offset");
	    const double ratio = wrapper.getRealAttribute(entity, "ratio");
	    return axis2_placement(dynamic_cast<STEPentity *>(brlcad::step::Entity(entity,
		"position")), length, origin, xaxis, yaxis, zaxis) &&
		std::isfinite(a) && std::isfinite(b) && std::isfinite(h) &&
		std::isfinite(xo) && std::isfinite(yo) && std::isfinite(ratio) &&
		a > 0.0 && b > 0.0 && h > 0.0 && ratio >= 0.0;
	}
	if (wrapper.IsSchemaEntity(entity, "ELLIPSOID")) {
	    const double a = wrapper.getRealAttribute(entity, "semi_axis_1");
	    const double b = wrapper.getRealAttribute(entity, "semi_axis_2");
	    const double c = wrapper.getRealAttribute(entity, "semi_axis_3");
	    return axis2_placement(dynamic_cast<STEPentity *>(brlcad::step::Entity(entity,
		"position")), length, origin, xaxis, yaxis, zaxis) &&
		std::isfinite(a) && std::isfinite(b) && std::isfinite(c) &&
		a > 0.0 && b > 0.0 && c > 0.0;
	}
#endif
	if (wrapper.IsSchemaEntity(entity, "BLOCK") ||
	    wrapper.IsSchemaEntity(entity, "RIGHT_ANGULAR_WEDGE")) {
	    const double x = wrapper.getRealAttribute(entity, "x");
	    const double y = wrapper.getRealAttribute(entity, "y");
	    const double z = wrapper.getRealAttribute(entity, "z");
	    const double ltx = wrapper.IsSchemaEntity(entity, "RIGHT_ANGULAR_WEDGE") ?
		wrapper.getRealAttribute(entity, "ltx") : 0.0;
	    return axis2_placement(dynamic_cast<STEPentity *>(brlcad::step::Entity(entity,
		"position")), length, origin, xaxis, yaxis, zaxis) &&
		std::isfinite(x) && std::isfinite(y) && std::isfinite(z) &&
		x > 0.0 && y > 0.0 && z > 0.0 &&
		(!wrapper.IsSchemaEntity(entity, "RIGHT_ANGULAR_WEDGE") ||
		(std::isfinite(ltx) && ltx >= 0.0 && ltx < x));
	}
	if (wrapper.IsSchemaEntity(entity, "RIGHT_CIRCULAR_CYLINDER")) {
	    const double height = wrapper.getRealAttribute(entity, "height");
	    const double radius = wrapper.getRealAttribute(entity, "radius");
	    return axis1_placement(dynamic_cast<STEPentity *>(brlcad::step::Entity(entity,
		"position")), length, origin, axis) && std::isfinite(height) &&
		std::isfinite(radius) && height > 0.0 && radius > 0.0;
	}
	if (wrapper.IsSchemaEntity(entity, "RIGHT_CIRCULAR_CONE")) {
	    const double height = wrapper.getRealAttribute(entity, "height");
	    const double radius = wrapper.getRealAttribute(entity, "radius");
	    const double angle = wrapper.getRealAttribute(entity, "semi_angle");
	    if (!axis1_placement(dynamic_cast<STEPentity *>(brlcad::step::Entity(entity,
		"position")), length, origin, axis) || !std::isfinite(height) ||
		!std::isfinite(radius) || !std::isfinite(angle) || height <= 0.0 ||
		radius < 0.0)
		return false;
	    const double top_radius = radius + height * std::tan(angle * plane_angle);
	    return std::isfinite(top_radius) && top_radius >= 0.0 &&
		(radius > 0.0 || top_radius > 0.0);
	}
	if (wrapper.IsSchemaEntity(entity, "TORUS")) {
	    const double major = wrapper.getRealAttribute(entity, "major_radius");
	    const double minor = wrapper.getRealAttribute(entity, "minor_radius");
	    return axis1_placement(dynamic_cast<STEPentity *>(brlcad::step::Entity(entity,
		"position")), length, origin, axis) && std::isfinite(major) &&
		std::isfinite(minor) && major > 0.0 && minor > 0.0 && minor < major;
	}
	return false;
    }

    bool half_space_geometry(STEPentity *half_space,
	double *origin, double *outward) const
    {
	/* BOXED_HALF_SPACE has an additional finite enclosure.  Treating it as
	 * its unbounded supertype would change the Boolean result, so only the
	 * exact unbounded plane case is accepted here. */
	if (!half_space || half_space->STEPfile_id <= 0 ||
	    wrapper.IsSchemaEntity(half_space, "BOXED_HALF_SPACE"))
	    return false;
	STEPentity *plane = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(half_space, "base_surface"));
	if (!plane || !wrapper.IsSchemaEntity(plane, "PLANE")) return false;
	double xaxis[3], yaxis[3];
	if (!axis2_placement(dynamic_cast<STEPentity *>(brlcad::step::Entity(plane,
		"position")), length, origin, xaxis, yaxis, outward))
	    return false;
	/* ISO 10303-42: agreement TRUE means the surface normal points away
	 * from material, exactly BRL-CAD's outward-normal convention. */
	if (wrapper.getBooleanAttribute(half_space, "agreement_flag") != BTrue)
	    VREVERSE(outward, outward);
	return true;
    }

    bool validate_half_space(STEPentity *half_space) const
    {
	double origin[3], outward[3];
	return half_space_geometry(half_space, origin, outward);
    }

    bool validate_root(SDAI_Select *select, std::set<int64_t> &validation_active,
	unsigned int depth = 0) const
    {
	if (!select || depth > 4096) return false;
	STEPentity *entity = dynamic_cast<STEPentity *>(brlcad::step::SelectedEntity(select));
	if (entity && wrapper.IsSchemaEntity(entity, "BOOLEAN_RESULT"))
	    return validate_boolean(entity, validation_active, depth + 1);
	if (entity) return validate_primitive(select);
	return false;
    }

    bool validate_operand(SDAI_Select *select, std::set<int64_t> &validation_active,
	unsigned int depth) const
    {
	if (!select || depth > 4096) return false;
	STEPentity *entity = dynamic_cast<STEPentity *>(brlcad::step::SelectedEntity(select));
	if (!entity || entity->STEPfile_id <= 0) return false;
	if (wrapper.IsSchemaEntity(entity, "BOOLEAN_RESULT"))
	    return validate_boolean(entity, validation_active, depth + 1);
	if (wrapper.IsSchemaEntity(entity, "HALF_SPACE_SOLID"))
	    return validate_half_space(entity);
	if (validate_primitive(select)) return true;
	if (wrapper.IsSchemaEntity(entity, "SOLID_MODEL")) {
	    if (!wrapper.IsSchemaEntity(entity, "CSG_SOLID"))
		return wrapper.IsSchemaEntity(entity, "MANIFOLD_SOLID_BREP") ||
		    wrapper.IsSchemaEntity(entity, "SOLID_REPLICA");
	    if (!validation_active.insert(entity->STEPfile_id).second) return false;
	    const bool valid = validate_root(wrapper.getSelectAttribute(entity,
		"tree_root_expression"), validation_active, depth + 1);
	    validation_active.erase(entity->STEPfile_id);
	    return valid;
	}
	return false;
    }

    bool validate_boolean(STEPentity *result, std::set<int64_t> &validation_active,
	unsigned int depth) const
    {
	if (!result || result->STEPfile_id <= 0 || depth > 4096 ||
	    !validation_active.insert(result->STEPfile_id).second)
	    return false;
	const std::string operation = wrapper.getEnumAttributeName(result, "operator");
	const bool known_operation = operation == "union" ||
	    operation == "intersection" || operation == "difference";
	const bool valid = known_operation &&
	    validate_operand(wrapper.getSelectAttribute(result, "first_operand"),
		validation_active, depth + 1) &&
	    validate_operand(wrapper.getSelectAttribute(result, "second_operand"),
		validation_active, depth + 1);
	validation_active.erase(result->STEPfile_id);
	return valid;
    }

    std::string primitive(SDAI_Select *select)
    {
	STEPentity *entity = primitive_entity(select);
	if (!entity || entity->STEPfile_id <= 0) return std::string();

	std::map<int64_t, std::string>::const_iterator existing = converted.find(entity->STEPfile_id);
	if (existing != converted.end()) return existing->second;
	const std::string original = wrapper.getStringAttribute(entity, "name");
	const std::string name = database.StableBRLCADName(product_name + "_csg_primitive",
	    entity->STEPfile_id) + ".s";
	STEPentity *position = dynamic_cast<STEPentity *>(brlcad::step::Entity(entity, "position"));
	bool success = false;

	if (wrapper.IsSchemaEntity(entity, "SPHERE")) {
	    double center[3];
	    STEPentity *centre = dynamic_cast<STEPentity *>(brlcad::step::Entity(entity, "centre"));
	    if (point_coordinates(centre, center)) {
		VSCALE(center, center, length);
		success = database.WriteSphere(name, center,
		    wrapper.getRealAttribute(entity, "radius") * length,
		    entity->STEPfile_id, original);
	    }
#ifdef AP242
	} else if (wrapper.IsSchemaEntity(entity, "RECTANGULAR_PYRAMID")) {
	    double origin[3], xaxis[3], yaxis[3], zaxis[3];
	    if (axis2_placement(position, length, origin, xaxis, yaxis, zaxis)) {
		double x[3], y[3], height[3], half_x[3], half_y[3], base_centre[3];
		double apex[3], points[24];
		VSCALE(x, xaxis, wrapper.getRealAttribute(entity, "xlength") * length);
		VSCALE(y, yaxis, wrapper.getRealAttribute(entity, "ylength") * length);
		VSCALE(height, zaxis, wrapper.getRealAttribute(entity, "height") * length);
		VMOVE(&points[0], origin);
		VADD2(&points[3], origin, x);
		VADD3(&points[6], origin, x, y);
		VADD2(&points[9], origin, y);
		VSCALE(half_x, x, 0.5);
		VSCALE(half_y, y, 0.5);
		VADD3(base_centre, origin, half_x, half_y);
		VADD2(apex, base_centre, height);
		for (size_t i = 4; i < 8; ++i) VMOVE(&points[i * 3], apex);
		success = database.WriteArb8(name, points, entity->STEPfile_id, original);
	    }
	} else if (wrapper.IsSchemaEntity(entity, "FACETED_PRIMITIVE")) {
	    double points[24];
	    if (faceted_geometry(entity, points))
		success = database.WriteArb8(name, points, entity->STEPfile_id, original);
	} else if (wrapper.IsSchemaEntity(entity, "ECCENTRIC_CONE")) {
	    double origin[3], xaxis[3], yaxis[3], zaxis[3];
	    if (axis2_placement(position, length, origin, xaxis, yaxis, zaxis)) {
		double height[3], base_a[3], base_b[3], top_a[3], top_b[3];
		VSCALE(height, zaxis, wrapper.getRealAttribute(entity, "height") * length);
		VJOIN1(height, height, wrapper.getRealAttribute(entity, "x_offset") * length, xaxis);
		VJOIN1(height, height, wrapper.getRealAttribute(entity, "y_offset") * length, yaxis);
		VSCALE(base_a, xaxis, wrapper.getRealAttribute(entity, "semi_axis_1") * length);
		VSCALE(base_b, yaxis, wrapper.getRealAttribute(entity, "semi_axis_2") * length);
		VSCALE(top_a, base_a, wrapper.getRealAttribute(entity, "ratio"));
		VSCALE(top_b, base_b, wrapper.getRealAttribute(entity, "ratio"));
		success = database.WriteTgc(name, origin, height, base_a, base_b,
		    top_a, top_b, entity->STEPfile_id, original);
	    }
	} else if (wrapper.IsSchemaEntity(entity, "ELLIPSOID")) {
	    double origin[3], xaxis[3], yaxis[3], zaxis[3];
	    if (axis2_placement(position, length, origin, xaxis, yaxis, zaxis)) {
		double a[3], b[3], c[3];
		VSCALE(a, xaxis, wrapper.getRealAttribute(entity, "semi_axis_1") * length);
		VSCALE(b, yaxis, wrapper.getRealAttribute(entity, "semi_axis_2") * length);
		VSCALE(c, zaxis, wrapper.getRealAttribute(entity, "semi_axis_3") * length);
		success = database.WriteEllipsoid(name, origin, a, b, c,
		    entity->STEPfile_id, original);
	    }
#endif
	} else if (wrapper.IsSchemaEntity(entity, "BLOCK") ||
		wrapper.IsSchemaEntity(entity, "RIGHT_ANGULAR_WEDGE")) {
	    double origin[3], xaxis[3], yaxis[3], zaxis[3];
	    if (axis2_placement(position, length, origin, xaxis, yaxis, zaxis)) {
		double x[3], y[3], z[3], points[24];
		VSCALE(x, xaxis, wrapper.getRealAttribute(entity, "x") * length);
		VSCALE(y, yaxis, wrapper.getRealAttribute(entity, "y") * length);
		VSCALE(z, zaxis, wrapper.getRealAttribute(entity, "z") * length);
		VMOVE(&points[0], origin);
		VADD2(&points[3], origin, x);
		VADD3(&points[6], origin, x, y);
		VADD2(&points[9], origin, y);
		VADD2(&points[12], origin, z);
		if (wrapper.IsSchemaEntity(entity, "BLOCK")) {
		    VADD3(&points[15], origin, x, z);
		} else {
		    double ltx[3];
		    VSCALE(ltx, xaxis, wrapper.getRealAttribute(entity, "ltx") * length);
		    VADD3(&points[15], origin, ltx, z);
		}
		VADD2(&points[18], &points[15], y);
		VADD2(&points[21], &points[12], y);
		success = database.WriteArb8(name, points, entity->STEPfile_id, original);
	    }
	} else if (wrapper.IsSchemaEntity(entity, "RIGHT_CIRCULAR_CYLINDER")) {
	    double origin[3], axis[3], height[3];
	    if (axis1_placement(position, length, origin, axis)) {
		VSCALE(height, axis, wrapper.getRealAttribute(entity, "height") * length);
		success = database.WriteRcc(name, origin, height,
		    wrapper.getRealAttribute(entity, "radius") * length,
		    entity->STEPfile_id, original);
	    }
	} else if (wrapper.IsSchemaEntity(entity, "RIGHT_CIRCULAR_CONE")) {
	    double origin[3], axis[3], height[3], xaxis[3], yaxis[3];
	    if (axis1_placement(position, length, origin, axis)) {
		bn_vec_ortho(xaxis, axis);
		VCROSS(yaxis, axis, xaxis);
		const double h = wrapper.getRealAttribute(entity, "height");
		const double radius = wrapper.getRealAttribute(entity, "radius");
		VSCALE(height, axis, h * length);
		const double top_radius = radius + h *
		    std::tan(wrapper.getRealAttribute(entity, "semi_angle") * plane_angle);
		double top_xaxis[3], top_yaxis[3];
		VSCALE(top_xaxis, xaxis, top_radius * length);
		VSCALE(top_yaxis, yaxis, top_radius * length);
		VSCALE(xaxis, xaxis, radius * length);
		VSCALE(yaxis, yaxis, radius * length);
		success = database.WriteTgc(name, origin, height, xaxis, yaxis,
		    top_xaxis, top_yaxis, entity->STEPfile_id, original);
	    }
	} else if (wrapper.IsSchemaEntity(entity, "TORUS")) {
	    double center[3], axis[3];
	    if (axis1_placement(position, length, center, axis))
		success = database.WriteTorus(name, center, axis,
		    wrapper.getRealAttribute(entity, "major_radius") * length,
		    wrapper.getRealAttribute(entity, "minor_radius") * length,
		    entity->STEPfile_id, original);
	}

	if (!success) return std::string();
	converted[entity->STEPfile_id] = name;
	return name;
    }

    std::string half_space(STEPentity *half)
    {
	if (!half || half->STEPfile_id <= 0) return std::string();
	std::map<int64_t, std::string>::const_iterator existing = converted.find(half->STEPfile_id);
	if (existing != converted.end()) return existing->second;
	double origin[3], outward[3];
	if (!half_space_geometry(half, origin, outward)) return std::string();
	const double distance = VDOT(outward, origin);
	const std::string name = database.StableBRLCADName(product_name + "_csg_half",
	    half->STEPfile_id) + ".s";
	if (!database.WriteHalf(name, outward, distance, half->STEPfile_id,
		wrapper.getStringAttribute(half, "name")))
	    return std::string();
	converted[half->STEPfile_id] = name;
	return name;
    }

    std::string solid_model(STEPentity *solid)
    {
	if (!solid || solid->STEPfile_id <= 0) return std::string();
	std::map<int64_t, std::string>::const_iterator existing = converted.find(solid->STEPfile_id);
	if (existing != converted.end()) return existing->second;

	SolidModel *model = dynamic_cast<SolidModel *>(Factory::CreateObject(&wrapper, solid));
	if (!model) return std::string();
	ON_Brep *brep = ON_Brep::New();
	if (!brep || !model->LoadONBrep(brep)) {
	    delete brep;
	    return std::string();
	}
	const std::string name = database.StableBRLCADName(product_name + "_csg_brep",
	    solid->STEPfile_id);
	mat_t identity;
	MAT_IDN(identity);
	if (!database.WriteBrep(name, brep, identity, false, solid->STEPfile_id,
		wrapper.getStringAttribute(solid, "name"))) {
	    delete brep;
	    return std::string();
	}
	delete brep;
	converted[solid->STEPfile_id] = name;
	return name;
    }

    std::string boolean_result(STEPentity *result)
    {
	if (!result || result->STEPfile_id <= 0 || !active.insert(result->STEPfile_id).second)
	    return std::string();
	std::map<int64_t, std::string>::const_iterator existing = converted.find(result->STEPfile_id);
	if (existing != converted.end()) {
	    active.erase(result->STEPfile_id);
	    return existing->second;
	}
	const std::string first = operand(wrapper.getSelectAttribute(result,
	    "first_operand"));
	const std::string second = operand(wrapper.getSelectAttribute(result,
	    "second_operand"));
	if (first.empty() || second.empty()) {
	    active.erase(result->STEPfile_id);
	    return std::string();
	}
	const std::string name = database.StableBRLCADName(product_name + "_csg_node",
	    result->STEPfile_id);
	mat_t identity;
	MAT_IDN(identity);
	int operation = WMOP_UNION;
	const std::string op = wrapper.getEnumAttributeName(result, "operator");
	if (op == "intersection") operation = WMOP_INTERSECT;
	else if (op == "difference") operation = WMOP_SUBTRACT;
	const bool success = database.AddMember(name, first, identity, WMOP_UNION) &&
	    database.AddMember(name, second, identity, operation);
	active.erase(result->STEPfile_id);
	if (!success) return std::string();
	converted[result->STEPfile_id] = name;
	return name;
    }

    std::string operand(SDAI_Select *select)
    {
	if (!select) return std::string();
	STEPentity *entity = dynamic_cast<STEPentity *>(
	    brlcad::step::SelectedEntity(select));
	if (!entity) return std::string();
	if (wrapper.IsSchemaEntity(entity, "BOOLEAN_RESULT"))
	    return boolean_result(entity);
	if (wrapper.IsSchemaEntity(entity, "HALF_SPACE_SOLID"))
	    return half_space(entity);
	if (wrapper.IsSchemaEntity(entity, "SOLID_MODEL")) {
	    return wrapper.IsSchemaEntity(entity, "CSG_SOLID") ?
		root(wrapper.getSelectAttribute(entity, "tree_root_expression")) :
		solid_model(entity);
	}
	return primitive(select);
    }

    std::string root(SDAI_Select *select)
    {
	if (!select) return std::string();
	STEPentity *entity = dynamic_cast<STEPentity *>(
	    brlcad::step::SelectedEntity(select));
	if (entity && wrapper.IsSchemaEntity(entity, "BOOLEAN_RESULT"))
	    return boolean_result(entity);
	if (entity) return primitive(select);
	return std::string();
    }
};

} // namespace

void
ImportSTEPNativeCSG(STEPWrapper &wrapper, BRLCADWrapper &database,
    const std::vector<uint64_t> &excluded_sdrs)
{
    /* The lazy type index is exact-type keyed, so request both association
     * forms explicitly.  An association traversed by the generic graph can
     * still own a CSG_SOLID that was deliberately left to this importer.
     * Select those CSG associations from the zero-copy reference index rather
     * than materializing every already-handled SDR and its complete sibling
     * geometry closure. */
    if (wrapper.HasLazyIndex()) {
	std::set<uint64_t> candidates;
	const char *types[] = {"SHAPE_DEFINITION_REPRESENTATION",
	    "PROPERTY_DEFINITION_REPRESENTATION"};
	for (const char *type : types) {
	    const std::vector<uint64_t> ids = wrapper.LazyInstancesByType(type);
	    candidates.insert(ids.begin(), ids.end());
	}
	std::vector<uint64_t> csg_links;
	for (std::set<uint64_t>::const_iterator candidate = candidates.begin();
		candidate != candidates.end(); ++candidate) {
	    const std::vector<uint64_t> references =
		wrapper.LazyForwardReferences(*candidate);
	    for (std::vector<uint64_t>::const_iterator reference = references.begin();
		    reference != references.end(); ++reference) {
		if (!wrapper.LazyIsSchemaEntity(*reference,
			"CSG_SHAPE_REPRESENTATION")) continue;
		csg_links.push_back(*candidate);
		break;
	    }
	}
	wrapper.SetInstanceIds(csg_links);
    } else {
	wrapper.SetInstanceTypes({"SHAPE_DEFINITION_REPRESENTATION",
	    "PROPERTY_DEFINITION_REPRESENTATION"}, excluded_sdrs);
    }
    for (int i = 0; i < wrapper.InstanceCount(); ++i) {
	SDAI_Application_instance *instance = wrapper.InstanceAt(i);
	if (!instance || instance->STEPfile_id <= 0)
	    continue;
	STEPentity *link = dynamic_cast<STEPentity *>(instance);
	SDAI_Select *represented = wrapper.getSelectAttribute(link, "definition");
	STEPentity *definition = NULL;
	if (wrapper.IsSchemaEntity(instance, "PROPERTY_DEFINITION_REPRESENTATION")) {
	    STEPentity *property = wrapper.IsSchemaEntity(instance,
		"SHAPE_DEFINITION_REPRESENTATION") ?
		dynamic_cast<STEPentity *>(brlcad::step::Entity(link, "definition")) :
		dynamic_cast<STEPentity *>(brlcad::step::SelectedEntity(represented));
	    if (property && wrapper.IsSchemaEntity(property, "PROPERTY_DEFINITION"))
		definition = csg_product_definition(wrapper,
		    wrapper.getSelectAttribute(property, "definition"));
	} else {
	    continue;
	}
	STEPentity *representation = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(link, "used_representation"));
	if (!definition || !representation ||
	    !wrapper.IsSchemaEntity(representation, "CSG_SHAPE_REPRESENTATION"))
	    continue;
	const int64_t product_id = csg_product_id(definition);
	std::map<int64_t, brlcad::step::Product>::iterator product =
	    wrapper.Document().products.find(product_id);
	if (product == wrapper.Document().products.end() || product->second.output_name.empty())
	    continue;

	const RepresentationUnits units = csg_representation_units(wrapper, representation);
	STEPNativeCSGImporter conversion(wrapper, database, product->second.output_name, units);
	const std::vector<SDAI_Application_instance *> items =
	    brlcad::step::Entities(representation, "items");
	for (std::vector<SDAI_Application_instance *>::const_iterator item =
		items.begin(); item != items.end(); ++item) {
	    STEPentity *solid = dynamic_cast<STEPentity *>(*item);
	    if (solid && wrapper.IsSchemaEntity(solid, "CSG_SOLID") &&
		wrapper.ShouldConvertEntity(solid->STEPfile_id)) {
		++wrapper.Statistics().geometry_attempted;
		std::set<int64_t> validation_active;
		validation_active.insert(solid->STEPfile_id);
		SDAI_Select *tree = wrapper.getSelectAttribute(solid,
		    "tree_root_expression");
		const bool valid_tree = conversion.validate_root(tree,
		    validation_active);
		const std::string expression = valid_tree ?
		    conversion.root(tree) : std::string();
		const std::string item_name = database.StableBRLCADName(
		    product->second.output_name + "_csg_item", solid->STEPfile_id);
		const brlcad::step::Style *style = NULL;
		std::map<int64_t, brlcad::step::Style>::const_iterator styled =
		    wrapper.Document().styles.find(solid->STEPfile_id);
		if (styled == wrapper.Document().styles.end())
		    styled = wrapper.Document().styles.find(representation->STEPfile_id);
		if (styled != wrapper.Document().styles.end()) style = &styled->second;
		mat_t identity;
		MAT_IDN(identity);
		if (!expression.empty() && database.AddMember(item_name, expression, identity) &&
		    database.SetCombinationProperties(item_name, true, solid->STEPfile_id,
			wrapper.getStringAttribute(solid, "name"), style) &&
		    database.AddMember(product->second.output_name, item_name, identity)) {
		    ++wrapper.Statistics().geometry_written;
		    if (style) ++wrapper.Statistics().styles_applied;
		    brlcad::step::Representation &record =
			wrapper.Document().representations[solid->STEPfile_id];
		    record.entity_id = solid->STEPfile_id;
		    record.product_id = product_id;
		    record.type = "CSG_SOLID";
		    record.output_name = item_name;
		    wrapper.RecordRepresentationItemCoverage(solid->STEPfile_id,
			brlcad::step::RepresentationCoverageStatus::Handled,
			"exact CSG tree converted successfully");
		} else {
		    ++wrapper.Statistics().geometry_skipped;
		    wrapper.RecordSkippedItem(solid->STEPfile_id, "CSG_SOLID",
			"unsupported or invalid exact CSG tree");
		    wrapper.RecordRepresentationItemCoverage(solid->STEPfile_id,
			brlcad::step::RepresentationCoverageStatus::Unsupported,
			"unsupported or invalid exact CSG tree");
		    wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, solid->STEPfile_id,
			"CSG_SOLID", "tree_root_expression", "unsupported or invalid exact CSG tree");
		}
	    }
	}
    }
    wrapper.ClearEntityCache();
    wrapper.ResetInstanceTypes();
}
