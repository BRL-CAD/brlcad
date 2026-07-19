/*                     A P 2 1 4 C S G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "AP214CSG.h"
#include "BRLCADWrapper.h"
#include "STEPString.h"
#include "STEPWrapper.h"

#include "GlobalUnitAssignedContext.h"

#include "vmath.h"

#include <cmath>
#include <map>
#include <set>

namespace {

SdaiProduct_definition *
csg_product_definition(SdaiCharacterized_definition *definition)
{
    if (!definition) return NULL;
    if (definition->IsCharacterized_product_definition() == LTrue) {
	SdaiCharacterized_product_definition *select = *definition;
	if (select && select->IsProduct_definition() == LTrue) {
	    SdaiProduct_definition *product = *select;
	    return product;
	}
    }
    if (definition->IsShape_definition() == LTrue) {
	SdaiShape_definition *select = *definition;
	if (select && select->IsProduct_definition_shape() == LTrue) {
	    SdaiProduct_definition_shape *shape = *select;
	    return shape ? csg_product_definition(shape->definition_()) : NULL;
	}
    }
    return NULL;
}

int64_t
csg_product_id(SdaiProduct_definition *definition)
{
    SdaiProduct_definition_formation *formation = definition ? definition->formation_() : NULL;
    SdaiProduct *product = formation ? formation->of_product_() : NULL;
    return product ? product->STEPfile_id : 0;
}

bool
point_coordinates(SdaiPoint *point, double *coordinates)
{
    SdaiCartesian_point *cartesian = dynamic_cast<SdaiCartesian_point *>(point);
    RealAggregate *values = cartesian ? cartesian->coordinates_() : NULL;
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
direction_ratios(SdaiDirection *direction, double *ratios)
{
    RealAggregate *values = direction ? direction->direction_ratios_() : NULL;
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
axis1_placement(SdaiAxis1_placement *placement, double length,
    double *origin, double *axis)
{
    if (!placement || !point_coordinates(placement->location_(), origin)) return false;
    VSCALE(origin, origin, length);
    VSET(axis, 0.0, 0.0, 1.0);
    SdaiDirection *direction = placement->axis_();
    return !direction || direction_ratios(direction, axis);
}

bool
axis2_placement(SdaiAxis2_placement_3d *placement, double length,
    double *origin, double *xaxis, double *yaxis, double *zaxis)
{
    if (!placement || !point_coordinates(placement->location_(), origin)) return false;
    VSCALE(origin, origin, length);
    VSET(zaxis, 0.0, 0.0, 1.0);
    VSET(xaxis, 1.0, 0.0, 0.0);
    if (placement->axis_() && !direction_ratios(placement->axis_(), zaxis)) return false;
    if (placement->ref_direction_() && !direction_ratios(placement->ref_direction_(), xaxis)) return false;
    VJOIN1(xaxis, xaxis, -(VDOT(xaxis, zaxis)), zaxis);
    if (MAGNITUDE(xaxis) <= SMALL_FASTF) return false;
    VUNITIZE(xaxis);
    VCROSS(yaxis, zaxis, xaxis);
    if (MAGNITUDE(yaxis) <= SMALL_FASTF) return false;
    VUNITIZE(yaxis);
    return true;
}

double
representation_length(STEPWrapper &wrapper, SdaiRepresentation *representation)
{
    SdaiRepresentation_context *context = representation ? representation->context_of_items_() : NULL;
    if (!context) return 1000.0;
    GlobalUnitAssignedContext units(&wrapper, context->STEPfile_id);
    if (!units.Load(&wrapper, context)) return 1000.0;
    const double factor = units.GetLengthConversionFactor();
    return factor > 0.0 ? factor : 1000.0;
}

struct CsgConversion {
    STEPWrapper &wrapper;
    BRLCADWrapper &database;
    std::string product_name;
    double length;
    std::map<int64_t, std::string> converted;
    std::set<int64_t> active;

    CsgConversion(STEPWrapper &w, BRLCADWrapper &d, const std::string &name, double factor)
	: wrapper(w), database(d), product_name(name), length(factor) {}

    static SDAI_Application_instance *primitive_entity(SdaiCsg_primitive *select)
    {
	if (!select) return NULL;
	if (select->IsSphere() == LTrue) { SdaiSphere *v = *select; return v; }
	if (select->IsBlock() == LTrue) { SdaiBlock *v = *select; return v; }
	if (select->IsRight_angular_wedge() == LTrue) { SdaiRight_angular_wedge *v = *select; return v; }
	if (select->IsTorus() == LTrue) { SdaiTorus *v = *select; return v; }
	if (select->IsRight_circular_cone() == LTrue) { SdaiRight_circular_cone *v = *select; return v; }
	if (select->IsRight_circular_cylinder() == LTrue) { SdaiRight_circular_cylinder *v = *select; return v; }
	return NULL;
    }

    bool validate_primitive(SdaiCsg_primitive *select) const
    {
	SDAI_Application_instance *entity = primitive_entity(select);
	if (!entity || entity->STEPfile_id <= 0 || !std::isfinite(length) || length <= 0.0)
	    return false;
	double origin[3], axis[3], xaxis[3], yaxis[3], zaxis[3];
	if (SdaiSphere *sphere = dynamic_cast<SdaiSphere *>(entity))
	    return point_coordinates(sphere->centre_(), origin) && std::isfinite(sphere->radius_()) &&
		sphere->radius_() > 0.0;
	if (SdaiBlock *block = dynamic_cast<SdaiBlock *>(entity))
	    return axis2_placement(block->position_(), length, origin, xaxis, yaxis, zaxis) &&
		std::isfinite(block->x_()) && std::isfinite(block->y_()) && std::isfinite(block->z_()) &&
		block->x_() > 0.0 && block->y_() > 0.0 && block->z_() > 0.0;
	if (SdaiRight_angular_wedge *wedge = dynamic_cast<SdaiRight_angular_wedge *>(entity))
	    return axis2_placement(wedge->position_(), length, origin, xaxis, yaxis, zaxis) &&
		std::isfinite(wedge->x_()) && std::isfinite(wedge->y_()) &&
		std::isfinite(wedge->z_()) && std::isfinite(wedge->ltx_()) &&
		wedge->x_() > 0.0 && wedge->y_() > 0.0 && wedge->z_() > 0.0 &&
		wedge->ltx_() >= 0.0 && wedge->ltx_() < wedge->x_();
	if (SdaiRight_circular_cylinder *cylinder = dynamic_cast<SdaiRight_circular_cylinder *>(entity))
	    return axis1_placement(cylinder->position_(), length, origin, axis) &&
		std::isfinite(cylinder->height_()) && std::isfinite(cylinder->radius_()) &&
		cylinder->height_() > 0.0 && cylinder->radius_() > 0.0;
	if (SdaiRight_circular_cone *cone = dynamic_cast<SdaiRight_circular_cone *>(entity)) {
	    if (!axis1_placement(cone->position_(), length, origin, axis) ||
		!std::isfinite(cone->height_()) || !std::isfinite(cone->radius_()) ||
		!std::isfinite(cone->semi_angle_()) || cone->height_() <= 0.0 ||
		cone->radius_() < 0.0)
		return false;
	    const double top_radius = cone->radius_() + cone->height_() * std::tan(cone->semi_angle_());
	    return std::isfinite(top_radius) && top_radius >= 0.0 &&
		(cone->radius_() > 0.0 || top_radius > 0.0);
	}
	if (SdaiTorus *torus = dynamic_cast<SdaiTorus *>(entity))
	    return axis1_placement(torus->position_(), length, origin, axis) &&
		std::isfinite(torus->major_radius_()) && std::isfinite(torus->minor_radius_()) &&
		torus->major_radius_() > 0.0 && torus->minor_radius_() > 0.0 &&
		torus->minor_radius_() < torus->major_radius_();
	return false;
    }

    bool half_space_geometry(SdaiHalf_space_solid *half_space,
	double *origin, double *outward) const
    {
	/* BOXED_HALF_SPACE has an additional finite enclosure.  Treating it as
	 * its unbounded supertype would change the Boolean result, so only the
	 * exact unbounded plane case is accepted here. */
	if (!half_space || half_space->STEPfile_id <= 0 ||
	    half_space->IsA(SCHEMA_NAMESPACE::e_boxed_half_space))
	    return false;
	SdaiPlane *plane = dynamic_cast<SdaiPlane *>(half_space->base_surface_());
	if (!plane) return false;
	double xaxis[3], yaxis[3];
	if (!axis2_placement(plane->position_(), length, origin, xaxis, yaxis, outward))
	    return false;
	/* ISO 10303-42: agreement TRUE means the surface normal points away
	 * from material, exactly BRL-CAD's outward-normal convention. */
	if (!half_space->agreement_flag_()) VREVERSE(outward, outward);
	return true;
    }

    bool validate_half_space(SdaiHalf_space_solid *half_space) const
    {
	double origin[3], outward[3];
	return half_space_geometry(half_space, origin, outward);
    }

    bool validate_root(SdaiCsg_select *select, std::set<int64_t> &validation_active,
	unsigned int depth = 0) const
    {
	if (!select || depth > 4096) return false;
	if (select->IsBoolean_result() == LTrue) {
	    SdaiBoolean_result *result = *select;
	    return validate_boolean(result, validation_active, depth + 1);
	}
	if (select->IsCsg_primitive() == LTrue) {
	    SdaiCsg_primitive *value = *select;
	    return validate_primitive(value);
	}
	return false;
    }

    bool validate_operand(SdaiBoolean_operand *select, std::set<int64_t> &validation_active,
	unsigned int depth) const
    {
	if (!select || depth > 4096) return false;
	if (select->IsBoolean_result() == LTrue) {
	    SdaiBoolean_result *result = *select;
	    return validate_boolean(result, validation_active, depth + 1);
	}
	if (select->IsCsg_primitive() == LTrue) {
	    SdaiCsg_primitive *value = *select;
	    return validate_primitive(value);
	}
	if (select->IsHalf_space_solid() == LTrue) {
	    SdaiHalf_space_solid *half_space = *select;
	    return validate_half_space(half_space);
	}
	if (select->IsSolid_model() == LTrue) {
	    SdaiSolid_model *solid = *select;
	    SdaiCsg_solid *csg = dynamic_cast<SdaiCsg_solid *>(solid);
	    if (!csg || csg->STEPfile_id <= 0 || !validation_active.insert(csg->STEPfile_id).second)
		return false;
	    const bool valid = validate_root(csg->tree_root_expression_(), validation_active, depth + 1);
	    validation_active.erase(csg->STEPfile_id);
	    return valid;
	}
	return false;
    }

    bool validate_boolean(SdaiBoolean_result *result, std::set<int64_t> &validation_active,
	unsigned int depth) const
    {
	if (!result || result->STEPfile_id <= 0 || depth > 4096 ||
	    !validation_active.insert(result->STEPfile_id).second)
	    return false;
	const Boolean_operator operation = result->operator_();
	const bool known_operation = operation == Boolean_operator__union ||
	    operation == Boolean_operator__intersection || operation == Boolean_operator__difference;
	const bool valid = known_operation &&
	    validate_operand(result->first_operand_(), validation_active, depth + 1) &&
	    validate_operand(result->second_operand_(), validation_active, depth + 1);
	validation_active.erase(result->STEPfile_id);
	return valid;
    }

    std::string primitive(SdaiCsg_primitive *select)
    {
	SDAI_Application_instance *entity = primitive_entity(select);
	if (!entity || entity->STEPfile_id <= 0) return std::string();

	std::map<int64_t, std::string>::const_iterator existing = converted.find(entity->STEPfile_id);
	if (existing != converted.end()) return existing->second;
	const std::string original = wrapper.getStringAttribute(entity, "name");
	const std::string name = database.StableBRLCADName(product_name + "_csg_primitive",
	    entity->STEPfile_id) + ".s";
	bool success = false;

	if (SdaiSphere *sphere = dynamic_cast<SdaiSphere *>(entity)) {
	    double center[3];
	    if (point_coordinates(sphere->centre_(), center)) {
		VSCALE(center, center, length);
		success = database.WriteSphere(name, center, sphere->radius_() * length,
		    entity->STEPfile_id, original);
	    }
	} else if (SdaiBlock *block = dynamic_cast<SdaiBlock *>(entity)) {
	    double origin[3], xaxis[3], yaxis[3], zaxis[3];
	    if (axis2_placement(block->position_(), length, origin, xaxis, yaxis, zaxis)) {
		double x[3], y[3], z[3], points[24];
		VSCALE(x, xaxis, block->x_() * length);
		VSCALE(y, yaxis, block->y_() * length);
		VSCALE(z, zaxis, block->z_() * length);
		VMOVE(&points[0], origin);
		VADD2(&points[3], origin, x);
		VADD3(&points[6], origin, x, y);
		VADD2(&points[9], origin, y);
		VADD2(&points[12], origin, z);
		VADD3(&points[15], origin, x, z);
		VADD2(&points[18], &points[6], z);
		VADD3(&points[21], origin, y, z);
		success = database.WriteArb8(name, points, entity->STEPfile_id, original);
	    }
	} else if (SdaiRight_angular_wedge *wedge = dynamic_cast<SdaiRight_angular_wedge *>(entity)) {
	    double origin[3], xaxis[3], yaxis[3], zaxis[3];
	    if (axis2_placement(wedge->position_(), length, origin, xaxis, yaxis, zaxis)) {
		double x[3], y[3], z[3], ltx[3], points[24];
		VSCALE(x, xaxis, wedge->x_() * length);
		VSCALE(y, yaxis, wedge->y_() * length);
		VSCALE(z, zaxis, wedge->z_() * length);
		VSCALE(ltx, xaxis, wedge->ltx_() * length);
		VMOVE(&points[0], origin);
		VADD2(&points[3], origin, x);
		VADD3(&points[6], origin, x, y);
		VADD2(&points[9], origin, y);
		VADD2(&points[12], origin, z);
		VADD3(&points[15], origin, ltx, z);
		VADD2(&points[18], &points[15], y);
		VADD2(&points[21], &points[12], y);
		success = database.WriteArb8(name, points, entity->STEPfile_id, original);
	    }
	} else if (SdaiRight_circular_cylinder *cylinder = dynamic_cast<SdaiRight_circular_cylinder *>(entity)) {
	    double origin[3], axis[3], height[3];
	    if (axis1_placement(cylinder->position_(), length, origin, axis)) {
		VSCALE(height, axis, cylinder->height_() * length);
		success = database.WriteRcc(name, origin, height, cylinder->radius_() * length,
		    entity->STEPfile_id, original);
	    }
	} else if (SdaiRight_circular_cone *cone = dynamic_cast<SdaiRight_circular_cone *>(entity)) {
	    double origin[3], axis[3], height[3], xaxis[3], yaxis[3];
	    if (axis1_placement(cone->position_(), length, origin, axis)) {
		bn_vec_ortho(xaxis, axis);
		VCROSS(yaxis, axis, xaxis);
		VSCALE(height, axis, cone->height_() * length);
		const double top_radius = cone->radius_() + cone->height_() * std::tan(cone->semi_angle_());
		double top_xaxis[3], top_yaxis[3];
		VSCALE(top_xaxis, xaxis, top_radius * length);
		VSCALE(top_yaxis, yaxis, top_radius * length);
		VSCALE(xaxis, xaxis, cone->radius_() * length);
		VSCALE(yaxis, yaxis, cone->radius_() * length);
		success = database.WriteTgc(name, origin, height, xaxis, yaxis, top_xaxis, top_yaxis,
		    entity->STEPfile_id, original);
	    }
	} else if (SdaiTorus *torus = dynamic_cast<SdaiTorus *>(entity)) {
	    double center[3], axis[3];
	    if (axis1_placement(torus->position_(), length, center, axis))
		success = database.WriteTorus(name, center, axis, torus->major_radius_() * length,
		    torus->minor_radius_() * length, entity->STEPfile_id, original);
	}

	if (!success) return std::string();
	converted[entity->STEPfile_id] = name;
	return name;
    }

    std::string half_space(SdaiHalf_space_solid *half)
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

    std::string boolean_result(SdaiBoolean_result *result)
    {
	if (!result || result->STEPfile_id <= 0 || !active.insert(result->STEPfile_id).second)
	    return std::string();
	std::map<int64_t, std::string>::const_iterator existing = converted.find(result->STEPfile_id);
	if (existing != converted.end()) {
	    active.erase(result->STEPfile_id);
	    return existing->second;
	}
	const std::string first = operand(result->first_operand_());
	const std::string second = operand(result->second_operand_());
	if (first.empty() || second.empty()) {
	    active.erase(result->STEPfile_id);
	    return std::string();
	}
	const std::string name = database.StableBRLCADName(product_name + "_csg_node",
	    result->STEPfile_id);
	mat_t identity;
	MAT_IDN(identity);
	int operation = WMOP_UNION;
	const Boolean_operator op = result->operator_();
	if (op == Boolean_operator__intersection) operation = WMOP_INTERSECT;
	else if (op == Boolean_operator__difference) operation = WMOP_SUBTRACT;
	const bool success = database.AddMember(name, first, identity, WMOP_UNION) &&
	    database.AddMember(name, second, identity, operation);
	active.erase(result->STEPfile_id);
	if (!success) return std::string();
	converted[result->STEPfile_id] = name;
	return name;
    }

    std::string operand(SdaiBoolean_operand *select)
    {
	if (!select) return std::string();
	if (select->IsBoolean_result() == LTrue) {
	    SdaiBoolean_result *result = *select;
	    return boolean_result(result);
	}
	if (select->IsCsg_primitive() == LTrue) {
	    SdaiCsg_primitive *value = *select;
	    return primitive(value);
	}
	if (select->IsHalf_space_solid() == LTrue) {
	    SdaiHalf_space_solid *half = *select;
	    return half_space(half);
	}
	if (select->IsSolid_model() == LTrue) {
	    SdaiSolid_model *solid = *select;
	    SdaiCsg_solid *csg = dynamic_cast<SdaiCsg_solid *>(solid);
	    return csg ? root(csg->tree_root_expression_()) : std::string();
	}
	return std::string();
    }

    std::string root(SdaiCsg_select *select)
    {
	if (!select) return std::string();
	if (select->IsBoolean_result() == LTrue) {
	    SdaiBoolean_result *result = *select;
	    return boolean_result(result);
	}
	if (select->IsCsg_primitive() == LTrue) {
	    SdaiCsg_primitive *value = *select;
	    return primitive(value);
	}
	return std::string();
    }
};

} // namespace

void
ConvertAP214CSG(STEPWrapper &wrapper, BRLCADWrapper &database,
    const std::vector<uint64_t> &excluded_sdrs)
{
    wrapper.SetInstanceTypes({"SHAPE_DEFINITION_REPRESENTATION"}, excluded_sdrs);
    for (int i = 0; i < wrapper.InstanceCount(); ++i) {
	SDAI_Application_instance *instance = wrapper.InstanceAt(i);
	if (!instance || instance->STEPfile_id <= 0 ||
	    !instance->IsA(SCHEMA_NAMESPACE::e_shape_definition_representation))
	    continue;
	SdaiShape_definition_representation *link =
	    dynamic_cast<SdaiShape_definition_representation *>(instance);
	SdaiRepresented_definition *represented = link ? link->definition_() : NULL;
	SdaiProperty_definition *property = represented && represented->IsProperty_definition() == LTrue ?
	    static_cast<SdaiProperty_definition *>(*represented) : NULL;
	SdaiProduct_definition *definition = property ? csg_product_definition(property->definition_()) : NULL;
	SdaiRepresentation *representation = link ? link->used_representation_() : NULL;
	if (!definition || !representation ||
	    !representation->IsA(SCHEMA_NAMESPACE::e_csg_shape_representation))
	    continue;
	const int64_t product_id = csg_product_id(definition);
	std::map<int64_t, brlcad::step::Product>::iterator product =
	    wrapper.Document().products.find(product_id);
	if (product == wrapper.Document().products.end() || product->second.output_name.empty())
	    continue;

	const double length = representation_length(wrapper, representation);
	CsgConversion conversion(wrapper, database, product->second.output_name, length);
	EntityAggregate *items = representation->items_();
	EntityNode *node = items ? static_cast<EntityNode *>(items->GetHead()) : NULL;
	while (node) {
	    SdaiCsg_solid *solid = dynamic_cast<SdaiCsg_solid *>(node->node);
	    if (solid && wrapper.ShouldConvertEntity(solid->STEPfile_id)) {
		++wrapper.Statistics().geometry_attempted;
		std::set<int64_t> validation_active;
		validation_active.insert(solid->STEPfile_id);
		const bool valid_tree = conversion.validate_root(solid->tree_root_expression_(),
		    validation_active);
		const std::string expression = valid_tree ?
		    conversion.root(solid->tree_root_expression_()) : std::string();
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
		    brlcad::step::Representation &item = wrapper.Document().representations[solid->STEPfile_id];
		    item.entity_id = solid->STEPfile_id;
		    item.product_id = product_id;
		    item.type = "CSG_SOLID";
		    item.output_name = item_name;
		} else {
		    ++wrapper.Statistics().geometry_skipped;
		    wrapper.RecordSkippedItem(solid->STEPfile_id, "CSG_SOLID",
			"unsupported or invalid exact CSG tree");
		    wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, solid->STEPfile_id,
			"CSG_SOLID", "tree_root_expression", "unsupported or invalid exact CSG tree");
		}
	    }
	    node = static_cast<EntityNode *>(node->NextNode());
	}
    }
    wrapper.ClearEntityCache();
    wrapper.ResetInstanceTypes();
}
