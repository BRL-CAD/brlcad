/*                  A P 2 4 2 P M I . C P P
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

#include "AP242PMI.h"

#include "BRLCADWrapper.h"
#include "Factory.h"
#include "GlobalUnitAssignedContext.h"
#include "STEPGeneratedAPI.h"
#include "STEPString.h"
#include "STEPWrapper.h"
#include "Axis2Placement2D.h"
#include "Axis2Placement3D.h"
#include "CartesianPoint.h"
#include "Curve.h"
#include "CylindricalSurface.h"
#include "Line.h"
#include "LocalUnits.h"
#include "Plane.h"
#include "SphericalSurface.h"
#include "STEPEntity.h"

#include "vmath.h"
#include "opennurbs.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <map>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace ap242_pmi {

using brlcad::step::PMIRecord;

std::string
trim_ascii(const std::string &value)
{
    size_t begin = 0;
    while (begin < value.size() &&
	std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    size_t end = value.size();
    while (end > begin &&
	std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(begin, end - begin);
}


std::string
record_value(const std::string &source)
{
    const size_t equal = source.find('=');
    if (equal == std::string::npos) return std::string();
    size_t end = source.find_last_not_of(" \t\r\n");
    if (end == std::string::npos || end <= equal) return std::string();
    if (source[end] == ';') {
	if (end == 0) return std::string();
	--end;
    }
    return trim_ascii(source.substr(equal + 1, end - equal));
}


std::vector<std::string>
source_arguments(const std::string &source)
{
    std::vector<std::string> result;
    const size_t equal = source.find('=');
    const size_t open = equal == std::string::npos ? std::string::npos :
	source.find('(', equal + 1);
    if (open == std::string::npos) return result;

    size_t argument = open + 1;
    int depth = 1;
    bool string = false;
    bool comment = false;
    for (size_t i = open + 1; i < source.size(); ++i) {
	const char c = source[i];
	if (comment) {
	    if (c == '*' && i + 1 < source.size() && source[i + 1] == '/') {
		comment = false;
		++i;
	    }
	    continue;
	}
	if (string) {
	    if (c != '\'') continue;
	    if (i + 1 < source.size() && source[i + 1] == '\'') {
		++i;
		continue;
	    }
	    string = false;
	    continue;
	}
	if (c == '/' && i + 1 < source.size() && source[i + 1] == '*') {
	    comment = true;
	    ++i;
	    continue;
	}
	if (c == '\'') {
	    string = true;
	    continue;
	}
	if (c == '(') {
	    ++depth;
	    continue;
	}
	if (c == ')') {
	    if (--depth == 0) {
		result.push_back(trim_ascii(source.substr(argument, i - argument)));
		break;
	    }
	    continue;
	}
	if (c == ',' && depth == 1) {
	    result.push_back(trim_ascii(source.substr(argument, i - argument)));
	    argument = i + 1;
	}
    }
    return result;
}


bool
contains(const std::string &value, const char *text)
{
    return text && value.find(text) != std::string::npos;
}


bool
presentation_type(const std::string &name)
{
    return contains(name, "ANNOTATION") || contains(name, "DRAUGHTING") ||
	contains(name, "CALLOUT") || contains(name, "STYLE") ||
	contains(name, "COLOUR") || contains(name, "FONT") ||
	contains(name, "SYMBOL") || contains(name, "GLYPH") ||
	contains(name, "TESSELLATED") || contains(name, "TEXT_LITERAL") ||
	contains(name, "COMPOSITE_TEXT") || contains(name, "CAMERA_MODEL") ||
	name == "TEXT_STRING_REPRESENTATION" ||
	name == "PLANAR_EXTENT" || name == "VIEW_VOLUME" ||
	name.compare(0, 13, "PRESENTATION_") == 0;
}


std::string
classify_type(const std::string &type,
    const std::vector<std::string> &components)
{
    std::vector<std::string> names = components;
    if (!type.empty() && type != "COMPLEX") names.push_back(type);
    std::string category;
    for (const std::string &name : names) {
	if (presentation_type(name))
	    category = "presentation";
	if (contains(name, "DATUM") || contains(name, "DIMENSIONAL_") ||
		contains(name, "GEOMETRIC_TOLERANCE") ||
		contains(name, "TOLERANCE_ZONE") ||
		name == "TOLERANCE_VALUE" ||
		(name.size() >= 10 && name.compare(name.size() - 10, 10,
		    "_TOLERANCE") == 0) ||
		(name.size() >= 10 && name.compare(name.size() - 10, 10,
		    "_DIMENSION") == 0))
	    return "semantic";
	if (contains(name, "RELATIONSHIP") || contains(name, "ASSOCIATION") ||
		contains(name, "USAGE") || name == "SHAPE_ASPECT" ||
		name == "ID_ATTRIBUTE")
	    category = "association";
	if (contains(name, "MEASURE") || contains(name, "UNIT") ||
		contains(name, "MODIFIER") || name == "DIMENSIONAL_EXPONENTS" ||
		name == "LIMITS_AND_FITS")
	    category = "dependency";
    }
    return category;
}


bool
presentation_geometry_dependency(STEPWrapper &wrapper, uint64_t id)
{
    return wrapper.LazyIsSchemaEntity(id, "CURVE") ||
	wrapper.LazyIsSchemaEntity(id, "POINT") ||
	wrapper.LazyIsSchemaEntity(id, "PLACEMENT") ||
	wrapper.LazyIsSchemaEntity(id, "DIRECTION") ||
	wrapper.LazyIsSchemaEntity(id, "VECTOR") ||
	wrapper.LazyIsSchemaEntity(id, "PLANE") ||
	wrapper.LazyIsSchemaEntity(id, "PLANAR_EXTENT") ||
	wrapper.LazyIsSchemaEntity(id, "COORDINATES_LIST") ||
	wrapper.LazyIsSchemaEntity(id, "TESSELLATED_ITEM") ||
	wrapper.LazyIsSchemaEntity(id, "ANNOTATION_FILL_AREA") ||
	wrapper.LazyIsSchemaEntity(id, "GEOMETRIC_SET") ||
	wrapper.LazyIsSchemaEntity(id, "COMPOSITE_CURVE_SEGMENT");
}


void
add_schema_roots(STEPWrapper &wrapper, std::set<uint64_t> &roots,
    const char *type)
{
    const std::vector<uint64_t> ids = wrapper.LazyInstancesBySchemaType(type);
    roots.insert(ids.begin(), ids.end());
}


std::set<uint64_t>
pmi_graph_ids(STEPWrapper &wrapper)
{
    std::set<uint64_t> result;
    const char *semantic_roots[] = {
	"DATUM", "DATUM_FEATURE", "DATUM_TARGET", "DATUM_REFERENCE",
	"GENERAL_DATUM_REFERENCE", "DATUM_SYSTEM", "GEOMETRIC_TOLERANCE",
	"DIMENSIONAL_SIZE", "DIMENSIONAL_LOCATION", "TOLERANCE_ZONE",
	"PLUS_MINUS_TOLERANCE", "TOLERANCE_VALUE"
    };
    const char *presentation_roots[] = {
	"ANNOTATION_OCCURRENCE", "ANNOTATION_PLANE", "DRAUGHTING_CALLOUT",
	"DRAUGHTING_MODEL", "CAMERA_MODEL"
    };
    for (const char *type : semantic_roots) add_schema_roots(wrapper, result, type);
    for (const char *type : presentation_roots) add_schema_roots(wrapper, result, type);

    std::deque<uint64_t> pending(result.begin(), result.end());
    while (!pending.empty()) {
	if (wrapper.CancellationRequested()) break;
	const uint64_t id = pending.front();
	pending.pop_front();
	/* Follow presentation geometry only in the forward direction.  Curves
	 * and points are often shared by model topology; reverse traversal from
	 * one annotation point must not absorb the entire product shape. */
	for (uint64_t neighbour : wrapper.LazyForwardReferences(id)) {
	    const std::string type = wrapper.LazyTypeName(neighbour);
	    const std::vector<std::string> components =
		wrapper.LazyComponentTypes(neighbour);
	    if (classify_type(type, components).empty() &&
		    !presentation_geometry_dependency(wrapper, neighbour))
		continue;
	    if (result.insert(neighbour).second) pending.push_back(neighbour);
	}
	/* Reverse links are needed for ownership and associativity, but only
	 * classified PMI records are safe to traverse in this direction. */
	for (uint64_t neighbour : wrapper.LazyReverseReferences(id)) {
	    const std::string type = wrapper.LazyTypeName(neighbour);
	    const std::vector<std::string> components =
		wrapper.LazyComponentTypes(neighbour);
	    if (classify_type(type, components).empty()) continue;
	    if (result.insert(neighbour).second) pending.push_back(neighbour);
	}
    }
    return result;
}


int64_t
direct_product(STEPWrapper &wrapper, uint64_t root,
    const std::map<uint64_t, int64_t> &identities)
{
    std::set<uint64_t> visited;
    std::deque<std::pair<uint64_t, unsigned int> > pending;
    pending.push_back(std::make_pair(root, 0));
    while (!pending.empty()) {
	const uint64_t id = pending.front().first;
	const unsigned int depth = pending.front().second;
	pending.pop_front();
	if (!visited.insert(id).second) continue;
	const auto product = identities.find(id);
	if (product != identities.end()) return product->second;
	if (depth >= 5) continue;
	for (uint64_t reference : wrapper.LazyForwardReferences(id)) {
	    const std::string type = wrapper.LazyTypeName(reference);
	    if (identities.find(reference) != identities.end() ||
		type == "PRODUCT_DEFINITION_SHAPE" || type == "PROPERTY_DEFINITION" ||
		type == "PRODUCT_DEFINITION" ||
		type == "PRODUCT_DEFINITION_FORMATION" || type == "PRODUCT")
		pending.push_back(std::make_pair(reference, depth + 1));
	}
    }
    return 0;
}


void
retain_graph(STEPWrapper &wrapper, const std::set<uint64_t> &ids)
{
    std::map<uint64_t, int64_t> product_identities;
    for (const auto &entry : wrapper.Document().products) {
	product_identities[static_cast<uint64_t>(entry.first)] = entry.first;
	for (int64_t id : entry.second.formation_entity_ids)
	    if (id > 0) product_identities[static_cast<uint64_t>(id)] = entry.first;
	for (int64_t id : entry.second.definition_entity_ids)
	    if (id > 0) product_identities[static_cast<uint64_t>(id)] = entry.first;
    }
    /* Presentation models are commonly connected to the owning shape only
     * through a mechanical-design-and-draughting relationship.  Treat the
     * already resolved product representations as ownership identities so a
     * presentation-only PMI graph does not depend on an incidental semantic
     * datum link to find its product. */
    for (const auto &entry : wrapper.Document().representation_coverage)
	if (entry.first > 0 && entry.second.product_id > 0)
	    product_identities[static_cast<uint64_t>(entry.first)] =
		entry.second.product_id;

    for (uint64_t id : ids) {
	if (!id || id > static_cast<uint64_t>(INT64_MAX)) continue;
	PMIRecord &record = wrapper.Document().pmi_records[static_cast<int64_t>(id)];
	record.entity_id = static_cast<int64_t>(id);
	record.component_types = wrapper.LazyComponentTypes(id);
	record.type = record.component_types.empty() ? wrapper.LazyTypeName(id) :
	    "COMPLEX";
	record.category = classify_type(record.type, record.component_types);
	if (record.category.empty()) record.category = "dependency";
	record.value = record_value(wrapper.LazySourceRecord(id));
	for (uint64_t reference : wrapper.LazyForwardReferences(id))
	    if (reference <= static_cast<uint64_t>(INT64_MAX))
		record.references.push_back(static_cast<int64_t>(reference));
	record.product_id = direct_product(wrapper, id, product_identities);
	if (record.category == "semantic") record.native_status = "retained_only";
    }

    bool changed = true;
    while (changed) {
	changed = false;
	for (auto &entry : wrapper.Document().pmi_records) {
	    PMIRecord &record = entry.second;
	    if (record.product_id > 0) continue;
	    for (int64_t reference : record.references) {
		auto related = wrapper.Document().pmi_records.find(reference);
		if (related == wrapper.Document().pmi_records.end() ||
			related->second.product_id <= 0)
		    continue;
		record.product_id = related->second.product_id;
		changed = true;
		break;
	    }
	    if (record.product_id > 0) continue;
	    for (uint64_t reverse : wrapper.LazyReverseReferences(
		    static_cast<uint64_t>(record.entity_id))) {
		auto related = wrapper.Document().pmi_records.find(
		    static_cast<int64_t>(reverse));
		if (related == wrapper.Document().pmi_records.end() ||
			related->second.product_id <= 0)
		    continue;
		record.product_id = related->second.product_id;
		changed = true;
		break;
	    }
	}
    }
}


double
representation_length_factor(STEPWrapper &wrapper, uint64_t representation_id)
{
    const double default_length = 1000.0;
    uint64_t context_id = 0;
    for (uint64_t reference : wrapper.LazyForwardReferences(representation_id)) {
	if (!wrapper.LazyIsSchemaEntity(reference, "REPRESENTATION_CONTEXT")) continue;
	context_id = reference;
	break;
    }
    if (!context_id || context_id > static_cast<uint64_t>(INT_MAX))
	return default_length;
    SDAI_Application_instance *context = wrapper.getEntity(
	static_cast<int>(context_id));
    SDAI_Application_instance *unit_component = context ?
	wrapper.getEntity(context, "Global_Unit_Assigned_Context") : NULL;
    if (!unit_component) return default_length;
    GlobalUnitAssignedContext units;
    if (!units.Load(&wrapper, unit_component)) return default_length;
    const double length = units.GetLengthConversionFactor();
    return std::isfinite(length) && length > 0.0 ? length : default_length;
}


struct DatumCandidate {
    rt_datum_type type = RT_DATUM_AUTO;
    point_t origin = VINIT_ZERO;
    vect_t direction = VINIT_ZERO;
    vect_t x_axis = VINIT_ZERO;
    vect_t y_axis = VINIT_ZERO;
};


bool
copy_candidate(DatumCandidate &candidate, rt_datum_type type,
    const double *origin, const double *direction, const double *x_axis,
    const double *y_axis, double length)
{
    if (!origin || !std::isfinite(length) || length <= 0.0) return false;
    candidate.type = type;
    VSET(candidate.origin, origin[0] * length, origin[1] * length,
	origin[2] * length);
    if (direction) {
	VSET(candidate.direction, direction[0], direction[1], direction[2]);
	if (MAGNITUDE(candidate.direction) <= SMALL_FASTF) return false;
	VUNITIZE(candidate.direction);
    }
    if (x_axis) {
	VSET(candidate.x_axis, x_axis[0], x_axis[1], x_axis[2]);
	if (MAGNITUDE(candidate.x_axis) > SMALL_FASTF) VUNITIZE(candidate.x_axis);
    }
    if (y_axis) {
	VSET(candidate.y_axis, y_axis[0], y_axis[1], y_axis[2]);
	if (MAGNITUDE(candidate.y_axis) > SMALL_FASTF) VUNITIZE(candidate.y_axis);
    }
    return true;
}


bool
candidate_from_entity(STEPWrapper &wrapper, uint64_t id, double length,
    DatumCandidate &candidate)
{
    if (!id || id > static_cast<uint64_t>(INT_MAX)) return false;
    uint64_t geometry_id = id;
    if (wrapper.LazyIsSchemaEntity(id, "FACE_SURFACE")) {
	geometry_id = 0;
	for (uint64_t reference : wrapper.LazyForwardReferences(id)) {
	    if (!wrapper.LazyIsSchemaEntity(reference, "SURFACE")) continue;
	    geometry_id = reference;
	    break;
	}
	if (!geometry_id) return false;
    }
    SDAI_Application_instance *instance = wrapper.getEntity(
	static_cast<int>(geometry_id));
    STEPEntity *object = Factory::CreateObject(&wrapper, instance);
    if (!object) return false;
    if (Plane *plane = dynamic_cast<Plane *>(object))
	return copy_candidate(candidate, RT_DATUM_PLANE, plane->GetOrigin(),
	    plane->GetNormal(), plane->GetXAxis(), plane->GetYAxis(), length);
    if (CylindricalSurface *cylinder =
	    dynamic_cast<CylindricalSurface *>(object))
	return copy_candidate(candidate, RT_DATUM_LINE, cylinder->GetOrigin(),
	    cylinder->GetNormal(), NULL, NULL, length);
    if (SphericalSurface *sphere = dynamic_cast<SphericalSurface *>(object))
	return copy_candidate(candidate, RT_DATUM_POINT, sphere->GetOrigin(),
	    NULL, NULL, NULL, length);
    if (CartesianPoint *point = dynamic_cast<CartesianPoint *>(object))
	return copy_candidate(candidate, RT_DATUM_POINT, point->Point3d(),
	    NULL, NULL, NULL, length);
    if (Line *line = dynamic_cast<Line *>(object))
	return copy_candidate(candidate, RT_DATUM_LINE, line->GetOrigin(),
	    line->GetDirection(), NULL, NULL, length);
    return false;
}


bool
parallel(const vect_t left, const vect_t right)
{
    return std::fabs(std::fabs(VDOT(left, right)) - 1.0) <= 1.0e-8;
}


bool
merge_candidates(const std::vector<DatumCandidate> &candidates,
    double tolerance, DatumCandidate &result, std::string &reason)
{
    if (candidates.empty()) {
	reason = "no exactly supported datum geometry was associated";
	return false;
    }
    result = candidates.front();
    for (size_t i = 1; i < candidates.size(); ++i) {
	const DatumCandidate &candidate = candidates[i];
	if (candidate.type != result.type) {
	    reason = "associated datum geometry has mixed derived kinds";
	    return false;
	}
	if (result.type == RT_DATUM_POINT) {
	    if (DIST_PNT_PNT(result.origin, candidate.origin) > tolerance) {
		reason = "associated point or spherical centers do not coincide";
		return false;
	    }
	} else if (result.type == RT_DATUM_LINE) {
	    if (!parallel(result.direction, candidate.direction)) {
		reason = "associated linear or cylindrical axes are not parallel";
		return false;
	    }
	    vect_t separation, cross;
	    VSUB2(separation, candidate.origin, result.origin);
	    VCROSS(cross, separation, result.direction);
	    if (MAGNITUDE(cross) > tolerance) {
		reason = "associated linear or cylindrical axes do not coincide";
		return false;
	    }
	} else if (result.type == RT_DATUM_PLANE) {
	    if (!parallel(result.direction, candidate.direction)) {
		reason = "associated planar normals are not parallel";
		return false;
	    }
	    vect_t separation;
	    VSUB2(separation, candidate.origin, result.origin);
	    if (std::fabs(VDOT(separation, result.direction)) > tolerance) {
		reason = "associated planes are distinct; a derived median plane "
		    "cannot be inferred safely";
		return false;
	    }
	}
    }
    return true;
}


std::string
datum_identifier(STEPWrapper &wrapper, uint64_t id)
{
    const std::vector<std::string> arguments = source_arguments(
	wrapper.LazySourceRecord(id));
    return arguments.empty() ? std::string() :
	brlcad::step::decode_string(arguments.back());
}


std::set<uint64_t>
datum_features(STEPWrapper &wrapper, uint64_t datum_id)
{
    std::set<uint64_t> result;
    for (uint64_t relationship : wrapper.LazyReverseReferences(datum_id)) {
	if (!wrapper.LazyIsSchemaEntity(relationship,
		"SHAPE_ASPECT_RELATIONSHIP")) continue;
	for (uint64_t reference : wrapper.LazyForwardReferences(relationship)) {
	    if (reference == datum_id) continue;
	    if (wrapper.LazyIsSchemaEntity(reference, "DATUM_FEATURE") ||
		    wrapper.LazyIsSchemaEntity(reference, "DATUM_TARGET"))
		result.insert(reference);
	}
    }
    return result;
}


bool
has_exact_type(STEPWrapper &wrapper, uint64_t id, const char *type)
{
    if (wrapper.LazyTypeName(id) == type) return true;
    const std::vector<std::string> components = wrapper.LazyComponentTypes(id);
    return std::find(components.begin(), components.end(), type) !=
	components.end();
}


bool
supported_datum_geometry(STEPWrapper &wrapper, uint64_t id)
{
    return wrapper.LazyIsSchemaEntity(id, "FACE_SURFACE") ||
	wrapper.LazyIsSchemaEntity(id, "PLANE") ||
	wrapper.LazyIsSchemaEntity(id, "CYLINDRICAL_SURFACE") ||
	wrapper.LazyIsSchemaEntity(id, "SPHERICAL_SURFACE") ||
	wrapper.LazyIsSchemaEntity(id, "CARTESIAN_POINT") ||
	wrapper.LazyIsSchemaEntity(id, "LINE");
}


std::vector<DatumCandidate>
datum_candidates(STEPWrapper &wrapper, const std::set<uint64_t> &features)
{
    std::vector<DatumCandidate> result;
    std::set<uint64_t> handled_geometry;
    for (uint64_t feature : features) {
	for (uint64_t association : wrapper.LazyReverseReferences(feature)) {
	    /* Presentation associations may also inherit from representation
	     * usage entities.  Their callouts do not define datum geometry. */
	    if (!has_exact_type(wrapper, association,
		    "ITEM_IDENTIFIED_REPRESENTATION_USAGE") &&
		    !has_exact_type(wrapper, association,
		    "GEOMETRIC_ITEM_SPECIFIC_USAGE"))
		continue;
	    uint64_t representation = 0;
	    const std::vector<uint64_t> references =
		wrapper.LazyForwardReferences(association);
	    for (uint64_t reference : references)
		if (wrapper.LazyIsSchemaEntity(reference, "REPRESENTATION")) {
		    representation = reference;
		    break;
		}
	    const double length = representation ?
		representation_length_factor(wrapper, representation) : 1000.0;
	    for (uint64_t reference : references) {
		if (!supported_datum_geometry(wrapper, reference) ||
			reference == representation ||
			!handled_geometry.insert(reference).second)
		    continue;
		DatumCandidate candidate;
		if (candidate_from_entity(wrapper, reference, length, candidate))
		    result.push_back(candidate);
	    }
	}
    }
    return result;
}


void
create_native_datums(STEPWrapper &wrapper, BRLCADWrapper &database)
{
    const std::vector<uint64_t> datums = wrapper.LazyInstancesByType("DATUM");
    const double tolerance = std::max(wrapper.Statistics().tolerance_mm,
	1.0e-7);
    for (uint64_t id : datums) {
	auto retained = wrapper.Document().pmi_records.find(
	    static_cast<int64_t>(id));
	if (retained == wrapper.Document().pmi_records.end()) continue;
	PMIRecord &record = retained->second;
	const std::set<uint64_t> features = datum_features(wrapper, id);
	DatumCandidate candidate;
	std::string reason;
	if (!merge_candidates(datum_candidates(wrapper, features), tolerance,
		candidate, reason)) {
	    record.native_status = reason;
	    continue;
	}
	const auto product = wrapper.Document().products.find(record.product_id);
	if (product == wrapper.Document().products.end() ||
		product->second.output_name.empty()) {
	    record.native_status = "resolved geometry has no retained product owner";
	    continue;
	}
	const std::string identifier = datum_identifier(wrapper, id);
	const std::string stem = identifier.empty() ? "datum" : identifier;
	const std::string name = database.StableBRLCADName(
	    product->second.output_name + "_datum_" + stem,
	    static_cast<int64_t>(id));
	const double *direction = candidate.type == RT_DATUM_POINT ? NULL :
	    candidate.direction;
	const double *x_axis = candidate.type == RT_DATUM_PLANE &&
	    MAGNITUDE(candidate.x_axis) > SMALL_FASTF ? candidate.x_axis : NULL;
	const double *y_axis = candidate.type == RT_DATUM_PLANE &&
	    MAGNITUDE(candidate.y_axis) > SMALL_FASTF ? candidate.y_axis : NULL;
	mat_t identity;
	MAT_IDN(identity);
	if (!database.WriteDatum(name, candidate.type, candidate.origin, direction,
		x_axis, y_axis, 10.0, static_cast<int64_t>(id),
		identifier, identifier) ||
		!database.AddMember(product->second.output_name, name, identity)) {
	    record.native_status = "resolved datum could not be written";
	    wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		static_cast<int64_t>(id), "DATUM", "database",
		"resolved AP242 datum could not be written");
	    continue;
	}
	record.native_object = name;
	record.native_kind = candidate.type == RT_DATUM_POINT ? "point" :
	    (candidate.type == RT_DATUM_LINE ? "line" : "plane");
	record.native_status = "resolved_exactly";
	++wrapper.Statistics().pmi_native_datums;
	if (!database.dry_run) {
	    database.SetAttribute(name, "step:pmi:category", "semantic");
	    database.SetAttribute(name, "step:pmi:product_id",
		std::to_string(record.product_id));
	    std::ostringstream links;
	    bool first = true;
	    for (uint64_t feature : features) {
		if (!first) links << ' ';
		first = false;
		links << feature;
	    }
	    database.SetAttribute(name, "step:pmi:feature_source_ids", links.str());
	}
    }
    wrapper.ClearEntityCache();
}


bool
parse_reals(const std::string &text, std::vector<double> &values)
{
    values.clear();
    const char *cursor = text.c_str();
    while (*cursor) {
	while (*cursor && (std::isspace(static_cast<unsigned char>(*cursor)) ||
		*cursor == '(' || *cursor == ')' || *cursor == ',')) ++cursor;
	if (!*cursor) break;
	char *end = NULL;
	const double value = std::strtod(cursor, &end);
	if (!end || end == cursor || !std::isfinite(value)) return false;
	values.push_back(value);
	cursor = end;
    }
    return !values.empty();
}


bool
parse_integers(const std::string &text, std::vector<int> &values)
{
    values.clear();
    const char *cursor = text.c_str();
    while (*cursor) {
	while (*cursor && (std::isspace(static_cast<unsigned char>(*cursor)) ||
		*cursor == '(' || *cursor == ')' || *cursor == ',')) ++cursor;
	if (!*cursor) break;
	char *end = NULL;
	const long value = std::strtol(cursor, &end, 10);
	if (!end || end == cursor || value < 1 ||
		value > std::numeric_limits<int>::max()) return false;
	values.push_back(static_cast<int>(value));
	cursor = end;
    }
    return !values.empty();
}


bool
real_rows(GenericAggregate *aggregate, std::vector<std::array<double, 3> > &rows)
{
    rows.clear();
    STEPnode *node = aggregate ? static_cast<STEPnode *>(aggregate->GetHead()) : NULL;
    while (node) {
	std::string text;
	std::vector<double> values;
	if (!parse_reals(node->asStr(text), values) || values.size() != 3)
	    return false;
	rows.push_back({{values[0], values[1], values[2]}});
	node = static_cast<STEPnode *>(node->NextNode());
    }
    return aggregate && !rows.empty();
}


bool
integer_rows(GenericAggregate *aggregate, std::vector<std::vector<int> > &rows)
{
    rows.clear();
    STEPnode *node = aggregate ? static_cast<STEPnode *>(aggregate->GetHead()) : NULL;
    while (node) {
	std::string text;
	std::vector<int> values;
	if (!parse_integers(node->asStr(text), values) || values.size() < 2)
	    return false;
	rows.push_back(values);
	node = static_cast<STEPnode *>(node->NextNode());
    }
    return aggregate && !rows.empty();
}


bool
integer_rows_optional(GenericAggregate *aggregate,
    std::vector<std::vector<int> > &rows, size_t minimum_width,
    size_t maximum_width = 0)
{
    rows.clear();
    if (!aggregate) return false;
    STEPnode *node = static_cast<STEPnode *>(aggregate->GetHead());
    while (node) {
	std::string text;
	std::vector<int> values;
	if (!parse_integers(node->asStr(text), values) ||
		values.size() < minimum_width ||
		(maximum_width && values.size() > maximum_width))
	    return false;
	rows.push_back(values);
	node = static_cast<STEPnode *>(node->NextNode());
    }
    return true;
}


bool
integer_values(IntAggregate *aggregate, std::vector<int> &values)
{
    values.clear();
    if (!aggregate) return false;
    IntNode *node = static_cast<IntNode *>(aggregate->GetHead());
    while (node) {
	if (node->value < 1 ||
		node->value > std::numeric_limits<int>::max())
	    return false;
	values.push_back(static_cast<int>(node->value));
	node = static_cast<IntNode *>(node->NextNode());
    }
    return true;
}


std::string
entity_name(STEPWrapper &wrapper, uint64_t id)
{
    const std::vector<std::string> arguments = source_arguments(
	wrapper.LazySourceRecord(id));
    return arguments.empty() ? std::string() :
	brlcad::step::decode_string(arguments.front());
}


double
product_length_factor(STEPWrapper &wrapper, int64_t product_id)
{
    for (const auto &entry : wrapper.Document().representation_coverage)
	if (entry.second.product_id == product_id)
	    return representation_length_factor(wrapper,
		static_cast<uint64_t>(entry.first));
    for (const auto &entry : wrapper.Document().pmi_records)
	if (entry.second.product_id == product_id &&
		wrapper.LazyIsSchemaEntity(
		    static_cast<uint64_t>(entry.first), "REPRESENTATION"))
	    return representation_length_factor(wrapper,
		static_cast<uint64_t>(entry.first));
    return 1000.0;
}


struct MarkerStyle {
    std::string shape = "plus";
    double size = 1.0;
};


std::vector<STEPentity *>
occurrence_styles(STEPWrapper &wrapper, uint64_t occurrence_id)
{
    std::vector<STEPentity *> result;
    if (!occurrence_id || occurrence_id > static_cast<uint64_t>(INT_MAX))
	return result;
    for (uint64_t reference : wrapper.LazyForwardReferences(occurrence_id)) {
	if (!wrapper.LazyIsSchemaEntity(reference,
		"PRESENTATION_STYLE_ASSIGNMENT") ||
		reference > static_cast<uint64_t>(INT_MAX))
	    continue;
	STEPentity *assignment = dynamic_cast<STEPentity *>(wrapper.getEntity(
	    static_cast<int>(reference)));
	for (SDAI_Application_instance *style_instance :
		brlcad::step::Entities(assignment, "styles")) {
	    STEPentity *style = dynamic_cast<STEPentity *>(style_instance);
	    if (style) result.push_back(style);
	}
    }
    return result;
}


bool
presentation_colour_entity(STEPWrapper &wrapper, uint64_t id,
    std::array<unsigned char, 4> &colour, std::set<uint64_t> &visited,
    unsigned int depth)
{
    if (!id || id > static_cast<uint64_t>(INT_MAX) || depth > 6 ||
	    !visited.insert(id).second)
	return false;
    STEPentity *entity = dynamic_cast<STEPentity *>(wrapper.getEntity(
	static_cast<int>(id)));
    if (!entity) return false;
    if (wrapper.IsSchemaEntity(entity, "COLOUR_RGB")) {
	const double red = wrapper.getRealAttribute(entity, "red");
	const double green = wrapper.getRealAttribute(entity, "green");
	const double blue = wrapper.getRealAttribute(entity, "blue");
	if (!std::isfinite(red) || red < 0.0 || red > 1.0 ||
		!std::isfinite(green) || green < 0.0 || green > 1.0 ||
		!std::isfinite(blue) || blue < 0.0 || blue > 1.0)
	    return false;
	colour = {{static_cast<unsigned char>(std::lround(red * 255.0)),
	    static_cast<unsigned char>(std::lround(green * 255.0)),
	    static_cast<unsigned char>(std::lround(blue * 255.0)), 255}};
	return true;
    }
    if (wrapper.IsSchemaEntity(entity, "PRE_DEFINED_COLOUR")) {
	std::string name = brlcad::step::decode_string(
	    wrapper.getStringAttribute(entity, "name"));
	std::transform(name.begin(), name.end(), name.begin(),
	    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	static const std::map<std::string, std::array<unsigned char, 4> > named = {
	    {"black", {{0, 0, 0, 255}}}, {"white", {{255, 255, 255, 255}}},
	    {"red", {{255, 0, 0, 255}}}, {"green", {{0, 255, 0, 255}}},
	    {"blue", {{0, 0, 255, 255}}},
	    {"yellow", {{255, 255, 0, 255}}},
	    {"magenta", {{255, 0, 255, 255}}},
	    {"cyan", {{0, 255, 255, 255}}}
	};
	const auto found = named.find(name);
	if (found != named.end()) {
	    colour = found->second;
	    return true;
	}
    }
    for (uint64_t reference : wrapper.LazyForwardReferences(id))
	if (presentation_colour_entity(wrapper, reference, colour, visited,
		depth + 1))
	    return true;
    return false;
}


bool
presentation_colour(STEPWrapper &wrapper, uint64_t occurrence_id,
    std::array<unsigned char, 4> &colour)
{
    for (STEPentity *style : occurrence_styles(wrapper, occurrence_id)) {
	if (!style || style->STEPfile_id <= 0) continue;
	std::set<uint64_t> visited;
	if (presentation_colour_entity(wrapper,
		static_cast<uint64_t>(style->STEPfile_id), colour, visited, 0))
	    return true;
    }
    return false;
}


void
apply_curve_presentation_style(STEPWrapper &wrapper, uint64_t occurrence_id,
    double length, size_t line_start,
    std::vector<brlcad::step::AnnotationLineStyle> &line_styles)
{
    for (STEPentity *style : occurrence_styles(wrapper, occurrence_id)) {
	if (!style || !wrapper.IsSchemaEntity(style, "CURVE_STYLE")) continue;
	uint32_t pattern = RT_ANNOT_LINE_CONTINUOUS;
	SDAI_Select *font_select = wrapper.getSelectAttribute(style,
	    "curve_font");
	SDAI_Application_instance *font =
	    brlcad::step::SelectedEntity(font_select);
	if (font && wrapper.IsSchemaEntity(font,
		"CURVE_STYLE_FONT_AND_SCALING")) {
	    STEPentity *scaled = dynamic_cast<STEPentity *>(font);
	    font_select = wrapper.getSelectAttribute(scaled, "curve_font");
	    font = brlcad::step::SelectedEntity(font_select);
	}
	std::string font_name = font ? brlcad::step::decode_string(
	    wrapper.getStringAttribute(font, "name")) : std::string();
	std::transform(font_name.begin(), font_name.end(), font_name.begin(),
	    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (contains(font_name, "chain double dash") ||
		contains(font_name, "phantom"))
	    pattern = RT_ANNOT_LINE_PHANTOM;
	else if (contains(font_name, "chain") || contains(font_name, "center"))
	    pattern = RT_ANNOT_LINE_CENTER;
	else if (contains(font_name, "dash"))
	    pattern = RT_ANNOT_LINE_DASHED;
	else if (contains(font_name, "dot"))
	    pattern = RT_ANNOT_LINE_DOTTED;

	bool have_width = false;
	double width = 1.0;
	SDAI_Select *width_select = wrapper.getSelectAttribute(style,
	    "curve_width");
	const SDAI_Real *selected_width =
	    brlcad::step::SelectedReal(width_select);
	if (selected_width && std::isfinite(*selected_width) &&
		*selected_width > 0.0 && std::isfinite(length) && length > 0.0) {
	    have_width = true;
	    width = *selected_width * length;
	}
	for (size_t i = line_start; i < line_styles.size(); ++i) {
	    line_styles[i].line_pattern = pattern;
	    if (have_width) {
		line_styles[i].has_width = true;
		line_styles[i].width = width;
	    }
	}
	break;
    }
}


MarkerStyle
point_marker_style(STEPWrapper &wrapper, uint64_t occurrence_id,
    double length)
{
    MarkerStyle result;
    for (STEPentity *style : occurrence_styles(wrapper, occurrence_id)) {
	if (!wrapper.IsSchemaEntity(style, "POINT_STYLE")) continue;
	SDAI_Select *size = wrapper.getSelectAttribute(style, "marker_size");
	const SDAI_Real *selected_size = brlcad::step::SelectedReal(size);
	if (selected_size && std::isfinite(*selected_size) && *selected_size > 0.0)
	    result.size = *selected_size * length;

	SDAI_Select *marker = wrapper.getSelectAttribute(style, "marker");
	SDAI_Application_instance *marker_entity =
	    brlcad::step::SelectedEntity(marker);
	std::string token;
	if (marker_entity)
	    token = brlcad::step::decode_string(
		wrapper.getStringAttribute(marker_entity, "name"));
	else if (marker)
	    marker->STEPwrite(token);
	std::transform(token.begin(), token.end(), token.begin(),
	    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const char *shapes[] = {
	    "asterisk", "triangle", "square", "circle", "ring", "plus", "dot", "x"
	};
	for (const char *shape : shapes)
	    if (contains(token, shape)) {
		result.shape = shape;
		break;
	    }
	break;
    }
    return result;
}


bool
placement_axes(STEPWrapper &wrapper, uint64_t item_id, double length,
    point_t origin, vect_t x_axis, vect_t y_axis, vect_t z_axis,
    bool *has_location = NULL)
{
    VSET(origin, 0.0, 0.0, 0.0);
    VSET(x_axis, 1.0, 0.0, 0.0);
    VSET(y_axis, 0.0, 1.0, 0.0);
    VSET(z_axis, 0.0, 0.0, 1.0);
    if (has_location) *has_location = false;
    uint64_t location_id = 0;
	for (uint64_t reference : wrapper.LazyForwardReferences(item_id))
	if (wrapper.LazyIsSchemaEntity(reference, "AXIS2_PLACEMENT_3D") ||
		wrapper.LazyIsSchemaEntity(reference, "AXIS2_PLACEMENT_2D")) {
	    location_id = reference;
	    break;
	}
    if (!location_id || location_id > static_cast<uint64_t>(INT_MAX))
	return true;
    STEPEntity *object = Factory::CreateObject(&wrapper,
	wrapper.getEntity(static_cast<int>(location_id)));
	if (Axis2Placement3D *placement_3d =
		dynamic_cast<Axis2Placement3D *>(object)) {
	    VSCALE(origin, placement_3d->GetOrigin(), length);
	    VMOVE(x_axis, placement_3d->GetXAxis());
	    VMOVE(y_axis, placement_3d->GetYAxis());
	    VMOVE(z_axis, placement_3d->GetNormal());
	} else if (Axis2Placement2D *placement_2d =
		dynamic_cast<Axis2Placement2D *>(object)) {
	    VSET(origin, placement_2d->GetOrigin()[0] * length,
		placement_2d->GetOrigin()[1] * length, 0.0);
	    VSET(x_axis, placement_2d->GetXAxis()[0],
		placement_2d->GetXAxis()[1], 0.0);
	    VSET(y_axis, placement_2d->GetYAxis()[0],
		placement_2d->GetYAxis()[1], 0.0);
	    VSET(z_axis, 0.0, 0.0, 1.0);
	} else {
	    return false;
	}
    if (has_location) *has_location = true;
    return true;
}


bool
append_curve_set(STEPWrapper &wrapper, uint64_t curve_set_id, double length,
    const point_t placement_origin, const vect_t placement_x,
    const vect_t placement_y, const vect_t placement_z,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines, std::string &reason)
{
    if (!curve_set_id || curve_set_id > static_cast<uint64_t>(INT_MAX))
	return false;
    STEPentity *curve_set = dynamic_cast<STEPentity *>(wrapper.getEntity(
	static_cast<int>(curve_set_id)));
    uint64_t coordinate_id = 0;
    for (uint64_t reference : wrapper.LazyForwardReferences(curve_set_id))
	if (wrapper.LazyIsSchemaEntity(reference, "COORDINATES_LIST")) {
	    coordinate_id = reference;
	    break;
	}
    STEPentity *coordinate_entity = coordinate_id &&
	coordinate_id <= static_cast<uint64_t>(INT_MAX) ?
	dynamic_cast<STEPentity *>(wrapper.getEntity(
	    static_cast<int>(coordinate_id))) : NULL;
    std::vector<std::array<double, 3> > coordinates;
    std::vector<std::vector<int> > strips;
    if (!coordinate_entity || !real_rows(dynamic_cast<GenericAggregate *>(
	    brlcad::step::Aggregate(coordinate_entity, "position_coords")),
	    coordinates)) {
	reason = "tessellated annotation coordinates are malformed";
	return false;
    }
    if (!integer_rows(dynamic_cast<GenericAggregate *>(
	    brlcad::step::Aggregate(curve_set, "line_strips")), strips)) {
	reason = "tessellated annotation line strips are malformed";
	return false;
    }

    const size_t offset = vertices.size();
    vertices.reserve(vertices.size() + coordinates.size());
    for (const auto &coordinate : coordinates) {
	point_t world;
	VJOIN3(world, placement_origin, coordinate[0] * length, placement_x,
	    coordinate[1] * length, placement_y,
	    coordinate[2] * length, placement_z);
	vect_t relative;
	VSUB2(relative, world, plane_origin);
	vertices.push_back({{VDOT(relative, plane_x), VDOT(relative, plane_y)}});
    }
    for (const auto &strip : strips) {
	for (size_t i = 1; i < strip.size(); ++i) {
	    if (strip[i - 1] < 1 || strip[i] < 1 ||
		    static_cast<size_t>(strip[i - 1]) > coordinates.size() ||
		    static_cast<size_t>(strip[i]) > coordinates.size()) {
		reason = "tessellated annotation line index is out of range";
		return false;
	    }
	    lines.push_back(std::make_pair(offset +
		static_cast<size_t>(strip[i - 1] - 1), offset +
		static_cast<size_t>(strip[i] - 1)));
	}
    }
    return true;
}


void
project_world_point(const point_t world, const point_t plane_origin,
    const vect_t plane_x, const vect_t plane_y,
    std::array<double, 2> &projected)
{
    vect_t relative;
    VSUB2(relative, world, plane_origin);
    projected[0] = VDOT(relative, plane_x);
    projected[1] = VDOT(relative, plane_y);
}


void
append_marker(const std::array<double, 2> &centre, const MarkerStyle &style,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines)
{
    const double radius = style.size * 0.5;
    auto add_line = [&](double x1, double y1, double x2, double y2) {
	const size_t first = vertices.size();
	vertices.push_back({{centre[0] + radius*x1, centre[1] + radius*y1}});
	vertices.push_back({{centre[0] + radius*x2, centre[1] + radius*y2}});
	lines.push_back(std::make_pair(first, first + 1));
    };
    auto add_ring = [&]() {
	const size_t offset = vertices.size();
	const size_t segments = 16;
	for (size_t i = 0; i < segments; ++i) {
	    const double angle = 2.0 * M_PI * static_cast<double>(i) /
		static_cast<double>(segments);
	    vertices.push_back({{centre[0] + radius*std::cos(angle),
		centre[1] + radius*std::sin(angle)}});
	    if (i) lines.push_back(std::make_pair(offset + i - 1, offset + i));
	}
	lines.push_back(std::make_pair(offset + segments - 1, offset));
    };

    if (style.shape == "x" || style.shape == "asterisk") {
	add_line(-1.0, -1.0, 1.0, 1.0);
	add_line(-1.0, 1.0, 1.0, -1.0);
    }
    if (style.shape == "plus" || style.shape == "asterisk") {
	add_line(-1.0, 0.0, 1.0, 0.0);
	add_line(0.0, -1.0, 0.0, 1.0);
    } else if (style.shape == "square") {
	add_line(-1.0, -1.0, 1.0, -1.0);
	add_line(1.0, -1.0, 1.0, 1.0);
	add_line(1.0, 1.0, -1.0, 1.0);
	add_line(-1.0, 1.0, -1.0, -1.0);
    } else if (style.shape == "triangle") {
	add_line(0.0, 1.0, 0.8660254037844386, -0.5);
	add_line(0.8660254037844386, -0.5, -0.8660254037844386, -0.5);
	add_line(-0.8660254037844386, -0.5, 0.0, 1.0);
    } else if (style.shape == "circle" || style.shape == "ring" ||
	    style.shape == "dot") {
	add_ring();
    }
}


bool
append_tessellated_points(STEPWrapper &wrapper, uint64_t point_set_id,
    double length, const point_t placement_origin, const vect_t placement_x,
    const vect_t placement_y, const vect_t placement_z,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    const MarkerStyle &style,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines, std::string &reason)
{
    STEPentity *point_set = point_set_id <= static_cast<uint64_t>(INT_MAX) ?
	dynamic_cast<STEPentity *>(wrapper.getEntity(
	    static_cast<int>(point_set_id))) : NULL;
    STEPentity *coordinate_entity = point_set ? dynamic_cast<STEPentity *>(
	brlcad::step::Entity(point_set, "coordinates")) : NULL;
    std::vector<std::array<double, 3> > coordinates;
    std::vector<int> point_list;
    if (!coordinate_entity || !real_rows(dynamic_cast<GenericAggregate *>(
	    brlcad::step::Aggregate(coordinate_entity, "position_coords")),
	    coordinates) ||
	    wrapper.getIntegerAttribute(coordinate_entity, "npoints") !=
		static_cast<int>(coordinates.size())) {
	reason = "tessellated annotation point coordinates are malformed";
	return false;
    }
    if (!integer_values(dynamic_cast<IntAggregate *>(
	    brlcad::step::Aggregate(point_set, "point_list")), point_list) ||
	    point_list.empty()) {
	reason = "tessellated annotation point list is malformed";
	return false;
    }
    for (int index : point_list) {
	if (index < 1 || static_cast<size_t>(index) > coordinates.size()) {
	    reason = "tessellated annotation point index is out of range";
	    return false;
	}
	const std::array<double, 3> &coordinate =
	    coordinates[static_cast<size_t>(index - 1)];
	point_t world;
	std::array<double, 2> projected;
	VJOIN3(world, placement_origin, coordinate[0] * length, placement_x,
	    coordinate[1] * length, placement_y,
	    coordinate[2] * length, placement_z);
	project_world_point(world, plane_origin, plane_x, plane_y, projected);
	append_marker(projected, style, vertices, lines);
    }
    return true;
}


bool
append_surface_boundaries(STEPWrapper &wrapper, uint64_t surface_id,
    double length, const point_t placement_origin, const vect_t placement_x,
    const vect_t placement_y, const vect_t placement_z,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines, std::string &reason)
{
    if (!surface_id || surface_id > static_cast<uint64_t>(INT_MAX))
	return false;
    STEPentity *surface = dynamic_cast<STEPentity *>(wrapper.getEntity(
	static_cast<int>(surface_id)));
    STEPentity *coordinate_entity = surface ? dynamic_cast<STEPentity *>(
	brlcad::step::Entity(surface, "coordinates")) : NULL;
    std::vector<std::array<double, 3> > coordinates;
    if (!coordinate_entity || !real_rows(dynamic_cast<GenericAggregate *>(
	    brlcad::step::Aggregate(coordinate_entity, "position_coords")),
	    coordinates) ||
	    wrapper.getIntegerAttribute(coordinate_entity, "npoints") !=
		static_cast<int>(coordinates.size())) {
	reason = "tessellated annotation surface coordinates are malformed";
	return false;
    }

    const int pnmax = wrapper.getIntegerAttribute(surface, "pnmax");
    std::vector<int> pnindex;
    if (pnmax < 1 || !integer_values(dynamic_cast<IntAggregate *>(
	    brlcad::step::Aggregate(surface, "pnindex")), pnindex) ||
	    (!pnindex.empty() && static_cast<int>(pnindex.size()) != pnmax) ||
	    (pnindex.empty() && static_cast<size_t>(pnmax) != coordinates.size())) {
	reason = "tessellated annotation surface point indexing is malformed";
	return false;
    }
    for (int index : pnindex) {
	if (index < 1 || static_cast<size_t>(index) > coordinates.size()) {
	    reason = "tessellated annotation surface coordinate index is out of range";
	    return false;
	}
    }

    std::set<std::array<int, 3> > triangles;
    auto add_triangle = [&](int first, int second, int third) {
	const int logical[3] = {first, second, third};
	std::array<int, 3> coordinate_indices;
	for (size_t i = 0; i < 3; ++i) {
	    if (logical[i] < 1 || logical[i] > pnmax) {
		reason = "tessellated annotation surface index is out of range";
		return false;
	    }
	    coordinate_indices[i] = pnindex.empty() ? logical[i] :
		pnindex[static_cast<size_t>(logical[i] - 1)];
	}
	if (coordinate_indices[0] == coordinate_indices[1] ||
		coordinate_indices[1] == coordinate_indices[2] ||
		coordinate_indices[2] == coordinate_indices[0])
	    return true;
	std::sort(coordinate_indices.begin(), coordinate_indices.end());
	triangles.insert(coordinate_indices);
	return true;
    };

    if (has_exact_type(wrapper, surface_id, "TRIANGULATED_SURFACE_SET")) {
	std::vector<std::vector<int> > rows;
	if (!integer_rows_optional(dynamic_cast<GenericAggregate *>(
		brlcad::step::Aggregate(surface, "triangles")), rows, 3, 3) ||
		rows.empty()) {
	    reason = "tessellated annotation triangle list is malformed";
	    return false;
	}
	for (const auto &row : rows)
	    if (!add_triangle(row[0], row[1], row[2])) return false;
    } else if (has_exact_type(wrapper, surface_id,
	    "COMPLEX_TRIANGULATED_SURFACE_SET")) {
	std::vector<std::vector<int> > strips, fans;
	if (!integer_rows_optional(dynamic_cast<GenericAggregate *>(
		brlcad::step::Aggregate(surface, "triangle_strips")), strips, 3) ||
		!integer_rows_optional(dynamic_cast<GenericAggregate *>(
		brlcad::step::Aggregate(surface, "triangle_fans")), fans, 3) ||
		(strips.empty() && fans.empty())) {
	    reason = "tessellated annotation triangle strips or fans are malformed";
	    return false;
	}
	for (const auto &strip : strips) {
	    for (size_t i = 2; i < strip.size(); ++i) {
		const int first = strip[i - 2];
		const int second = strip[i - 1];
		if (!add_triangle((i % 2) == 0 ? first : second,
			(i % 2) == 0 ? second : first, strip[i]))
		    return false;
	    }
	}
	for (const auto &fan : fans)
	    for (size_t i = 2; i < fan.size(); ++i)
		if (!add_triangle(fan[0], fan[i - 1], fan[i])) return false;
    } else {
	reason = "unsupported tessellated annotation surface subtype";
	return false;
    }
    if (triangles.empty()) {
	reason = "tessellated annotation surface has no nondegenerate triangles";
	return false;
    }

    std::map<std::pair<int, int>, size_t> edge_uses;
    for (const auto &triangle : triangles) {
	for (size_t edge = 0; edge < 3; ++edge) {
	    const int first = triangle[edge];
	    const int second = triangle[(edge + 1) % 3];
	    ++edge_uses[std::make_pair(std::min(first, second),
		std::max(first, second))];
	}
    }

    const size_t offset = vertices.size();
    vertices.reserve(vertices.size() + coordinates.size());
    for (const auto &coordinate : coordinates) {
	point_t world;
	std::array<double, 2> projected;
	VJOIN3(world, placement_origin, coordinate[0] * length, placement_x,
	    coordinate[1] * length, placement_y,
	    coordinate[2] * length, placement_z);
	project_world_point(world, plane_origin, plane_x, plane_y, projected);
	vertices.push_back(projected);
    }
    size_t boundaries = 0;
    for (const auto &edge : edge_uses) {
	if (edge.second != 1) continue;
	lines.push_back(std::make_pair(offset + static_cast<size_t>(edge.first.first - 1),
	    offset + static_cast<size_t>(edge.first.second - 1)));
	++boundaries;
    }
    if (!boundaries) {
	vertices.resize(offset);
	reason = "tessellated annotation surface has no boundary edges";
	return false;
    }
    return true;
}


bool
append_tessellated_item(STEPWrapper &wrapper, uint64_t item_id, double length,
    const point_t inherited_origin, const vect_t inherited_x,
    const vect_t inherited_y, const vect_t inherited_z,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    const MarkerStyle &marker,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines,
    std::set<uint64_t> &visited, bool &have_surface, bool &have_points,
    std::string &reason)
{
    if (!visited.insert(item_id).second) {
	reason = "cyclic tessellated annotation item graph";
	return false;
    }

    point_t item_origin;
    vect_t item_x, item_y, item_z;
    VMOVE(item_origin, inherited_origin);
    VMOVE(item_x, inherited_x);
    VMOVE(item_y, inherited_y);
    VMOVE(item_z, inherited_z);
    point_t located_origin;
    vect_t located_x, located_y, located_z;
    bool located = false;
    if (!placement_axes(wrapper, item_id, length, located_origin, located_x,
	    located_y, located_z, &located)) {
	reason = "tessellated annotation placement could not be loaded";
	return false;
    }
    if (located) {
	VMOVE(item_origin, located_origin);
	VMOVE(item_x, located_x);
	VMOVE(item_y, located_y);
	VMOVE(item_z, located_z);
    }

    if (has_exact_type(wrapper, item_id, "TESSELLATED_CURVE_SET"))
	return append_curve_set(wrapper, item_id, length, item_origin, item_x,
	    item_y, item_z, plane_origin, plane_x, plane_y, vertices, lines,
	    reason);
    if (has_exact_type(wrapper, item_id, "TESSELLATED_POINT_SET")) {
	have_points = true;
	return append_tessellated_points(wrapper, item_id, length, item_origin,
	    item_x, item_y, item_z, plane_origin, plane_x, plane_y, marker,
	    vertices, lines, reason);
    }
    if (has_exact_type(wrapper, item_id, "TRIANGULATED_SURFACE_SET") ||
	    has_exact_type(wrapper, item_id,
		"COMPLEX_TRIANGULATED_SURFACE_SET")) {
	have_surface = true;
	return append_surface_boundaries(wrapper, item_id, length, item_origin,
	    item_x, item_y, item_z, plane_origin, plane_x, plane_y, vertices,
	    lines, reason);
    }
    if (!has_exact_type(wrapper, item_id, "TESSELLATED_GEOMETRIC_SET")) {
	reason = "unsupported tessellated annotation child type " +
	    wrapper.LazyTypeName(item_id);
	return false;
    }

    bool found = false;
    for (uint64_t child : wrapper.LazyForwardReferences(item_id)) {
	if (!wrapper.LazyIsSchemaEntity(child, "TESSELLATED_ITEM") ||
		wrapper.LazyIsSchemaEntity(child, "COORDINATES_LIST"))
	    continue;
	found = true;
	if (!append_tessellated_item(wrapper, child, length, item_origin,
		item_x, item_y, item_z, plane_origin, plane_x, plane_y,
		marker, vertices, lines, visited, have_surface, have_points,
		reason))
	    return false;
    }
    if (!found) {
	reason = "tessellated annotation geometric set has no drawable children";
	return false;
    }
    return true;
}


bool
append_polyline(STEPWrapper &wrapper, uint64_t polyline_id, double length,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines, std::string &reason)
{
    STEPentity *polyline = polyline_id <= static_cast<uint64_t>(INT_MAX) ?
	dynamic_cast<STEPentity *>(wrapper.getEntity(
	    static_cast<int>(polyline_id))) : NULL;
    const std::vector<SDAI_Application_instance *> points = polyline ?
	brlcad::step::Entities(polyline, "points") :
	std::vector<SDAI_Application_instance *>();
    if (points.size() < 2) {
	reason = "annotation polyline has fewer than two points";
	return false;
    }
    const size_t offset = vertices.size();
    for (SDAI_Application_instance *point_entity : points) {
	CartesianPoint *point = dynamic_cast<CartesianPoint *>(
	    Factory::CreateObject(&wrapper, point_entity));
	if (!point || !point->Point3d()) {
	    vertices.resize(offset);
	    reason = "annotation polyline point could not be loaded";
	    return false;
	}
	point_t world;
	std::array<double, 2> projected;
	VSCALE(world, point->Point3d(), length);
	project_world_point(world, plane_origin, plane_x, plane_y, projected);
	vertices.push_back(projected);
    }
    for (size_t i = 1; i < points.size(); ++i)
	lines.push_back(std::make_pair(offset + i - 1, offset + i));
    return true;
}


bool
append_circle(STEPWrapper &wrapper, uint64_t circle_id, double length,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines, std::string &reason)
{
    STEPentity *circle = circle_id <= static_cast<uint64_t>(INT_MAX) ?
	dynamic_cast<STEPentity *>(wrapper.getEntity(
	    static_cast<int>(circle_id))) : NULL;
    const double radius = circle ? wrapper.getRealAttribute(circle, "radius") *
	length : 0.0;
    point_t origin;
    vect_t x_axis, y_axis, z_axis;
    bool located = false;
    if (!circle || !std::isfinite(radius) || radius <= 0.0 ||
	    !placement_axes(wrapper, circle_id, length, origin, x_axis, y_axis,
		z_axis, &located) || !located) {
	reason = "annotation circle placement or radius is malformed";
	return false;
    }

    const size_t offset = vertices.size();
    const size_t segments = 64;
    for (size_t i = 0; i < segments; ++i) {
	const double angle = 2.0 * M_PI * static_cast<double>(i) /
	    static_cast<double>(segments);
	point_t world;
	std::array<double, 2> projected;
	VJOIN2(world, origin, radius * std::cos(angle), x_axis,
	    radius * std::sin(angle), y_axis);
	project_world_point(world, plane_origin, plane_x, plane_y, projected);
	vertices.push_back(projected);
	if (i) lines.push_back(std::make_pair(offset + i - 1, offset + i));
    }
    lines.push_back(std::make_pair(offset + segments - 1, offset));
    return true;
}


struct Affine2D {
    double origin[2] = {0.0, 0.0};
    /* Row-major linear part. */
    double matrix[4] = {1.0, 0.0, 0.0, 1.0};
};


bool placement_frame_2d(STEPWrapper &wrapper,
    SDAI_Application_instance *placement_instance, double length,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    Affine2D &frame);


class AnnotationLocalUnitScope {
public:
    explicit AnnotationLocalUnitScope(double length)
	: saved_length(LocalUnits::length)
    {
	LocalUnits::length = length;
    }

    ~AnnotationLocalUnitScope()
    {
	LocalUnits::length = saved_length;
    }

private:
    double saved_length;
};


/** Sample any STEP curve supported by the normal STEP/OpenNURBS curve
 * conversion.  A private OpenNURBS index scope is essential: the cached STEP
 * objects may also be used later to build an unrelated BREP. */
bool
sample_annotation_curve(STEPWrapper &wrapper, uint64_t curve_id,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, bool require_closed,
    std::vector<std::array<double, 2> > &points, bool &closed,
    std::string &reason)
{
    points.clear();
    closed = false;
    if (!curve_id || curve_id > static_cast<uint64_t>(INT_MAX)) {
	reason = "annotation curve reference is out of range";
	return false;
    }

    AnnotationLocalUnitScope units(length);
    STEPEntity::ONStateMap state;
    STEPEntity::ONStateScope state_scope(&state);
    Curve *curve = dynamic_cast<Curve *>(Factory::CreateObject(&wrapper,
	wrapper.getEntity(static_cast<int>(curve_id))));
    ON_Brep brep;
    if (!curve || !curve->LoadONBrep(&brep)) {
	reason = "annotation curve could not be converted: " +
	    wrapper.LazyTypeName(curve_id);
	return false;
    }
    const int curve_index = curve->GetONId();
    const ON_Curve *on_curve = curve_index >= 0 &&
	curve_index < brep.m_C3.Count() ? brep.m_C3[curve_index] : NULL;
    if (!on_curve || !on_curve->IsValid()) {
	reason = "annotation curve conversion produced no valid curve: " +
	    wrapper.LazyTypeName(curve_id);
	return false;
    }

    const ON_3dPoint start = on_curve->PointAtStart();
    const ON_3dPoint end = on_curve->PointAtEnd();
    if (!start.IsValid() || !end.IsValid()) {
	reason = "annotation curve has invalid endpoints";
	return false;
    }
    const double closure_tolerance = std::max(1.0e-7,
	1.0e-9 * std::max(1.0, std::max(start.MaximumCoordinate(),
	    end.MaximumCoordinate())));
    closed = on_curve->IsClosed() || start.DistanceTo(end) <= closure_tolerance;
    if (require_closed && !closed) {
	reason = "annotation fill boundary curve is not closed: " +
	    wrapper.LazyTypeName(curve_id);
	return false;
    }

    ON_SimpleArray<ON_3dPoint> polyline;
    if (on_curve->IsPolyline(&polyline, NULL) < 2) {
	const int span_count = std::max(1, on_curve->SpanCount());
	std::vector<double> spans(static_cast<size_t>(span_count) + 1);
	if (!on_curve->GetSpanVector(spans.data())) {
	    const ON_Interval domain = on_curve->Domain();
	    spans.resize(2);
	    spans[0] = domain.Min();
	    spans[1] = domain.Max();
	}
	const int actual_spans = static_cast<int>(spans.size()) - 1;
	const int target_samples = std::min(4096,
	    std::max(64, actual_spans * 16));
	const int per_span = std::max(2,
	    (target_samples + actual_spans - 1) / actual_spans);
	for (int span = 0; span < actual_spans; ++span) {
	    for (int sample = span ? 1 : 0; sample <= per_span; ++sample) {
		const double fraction = static_cast<double>(sample) /
		    static_cast<double>(per_span);
		const double parameter = (1.0 - fraction) * spans[span] +
		    fraction * spans[span + 1];
		const ON_3dPoint point = on_curve->PointAt(parameter);
		if (!point.IsValid()) {
		    reason = "annotation curve sampling produced an invalid point";
		    return false;
		}
		polyline.Append(point);
	    }
	}
    }
    if (polyline.Count() < (require_closed ? 3 : 2)) {
	reason = "annotation curve has too few sampled points";
	return false;
    }

    /* Closed OpenNURBS curves normally repeat the endpoint.  The native fill
     * loop and line closure are implicit, so retain a single copy. */
    if (closed && polyline.Count() > 1 &&
	polyline[0].DistanceTo(polyline[polyline.Count() - 1]) <=
	    closure_tolerance)
	polyline.Remove(polyline.Count() - 1);
    if (polyline.Count() < (require_closed ? 3 : 2)) {
	reason = "annotation curve degenerates after closure";
	return false;
    }

    points.reserve(static_cast<size_t>(polyline.Count()));
    for (int i = 0; i < polyline.Count(); ++i) {
	point_t world;
	std::array<double, 2> projected;
	VSET(world, polyline[i].x, polyline[i].y, polyline[i].z);
	project_world_point(world, plane_origin, plane_x, plane_y, projected);
	if (!std::isfinite(projected[0]) || !std::isfinite(projected[1])) {
	    points.clear();
	    reason = "annotation curve projection produced a non-finite point";
	    return false;
	}
	points.push_back(projected);
    }
    return true;
}


bool
append_annotation_curve_strokes(STEPWrapper &wrapper, uint64_t curve_id,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines, std::string &reason)
{
    if (has_exact_type(wrapper, curve_id, "POLYLINE"))
	return append_polyline(wrapper, curve_id, length, plane_origin,
	    plane_x, plane_y, vertices, lines, reason);
    if (has_exact_type(wrapper, curve_id, "CIRCLE"))
	return append_circle(wrapper, curve_id, length, plane_origin,
	    plane_x, plane_y, vertices, lines, reason);
    std::vector<std::array<double, 2> > sampled;
    bool closed = false;
    if (!sample_annotation_curve(wrapper, curve_id, length, plane_origin,
	    plane_x, plane_y, false, sampled, closed, reason))
	return false;
    const size_t offset = vertices.size();
    vertices.insert(vertices.end(), sampled.begin(), sampled.end());
    for (size_t i = 1; i < sampled.size(); ++i)
	lines.push_back(std::make_pair(offset + i - 1, offset + i));
    if (closed)
	lines.push_back(std::make_pair(offset + sampled.size() - 1, offset));
    return true;
}


bool
append_planar_box_fill(STEPWrapper &wrapper, STEPentity *box, double length,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<brlcad::step::AnnotationFill> &fills, std::string &reason)
{
    SDAI_Application_instance *placement = box ?
	brlcad::step::Entity(box, "placement") : NULL;
    const double size_x = box ? wrapper.getRealAttribute(box, "size_in_x") *
	length : 0.0;
    const double size_y = box ? wrapper.getRealAttribute(box, "size_in_y") *
	length : 0.0;
    Affine2D frame;
    if (!box || !std::isfinite(size_x) || size_x <= 0.0 ||
	    !std::isfinite(size_y) || size_y <= 0.0 ||
	    !placement_frame_2d(wrapper, placement, length, plane_origin,
		plane_x, plane_y, frame)) {
	reason = "annotation text blanking box is malformed";
	return false;
    }
    const size_t offset = vertices.size();
    const double corners[4][2] = {
	{0.0, 0.0}, {size_x, 0.0}, {size_x, size_y}, {0.0, size_y}
    };
    for (const auto &corner : corners)
	vertices.push_back({{frame.origin[0] + frame.matrix[0]*corner[0] +
	    frame.matrix[1]*corner[1],
	    frame.origin[1] + frame.matrix[2]*corner[0] +
	    frame.matrix[3]*corner[1]}});
    brlcad::step::AnnotationFill fill;
    fill.role = RT_ANNOT_ROLE_MASK;
    fill.symbol = "text blanking box";
    fill.loops.push_back(std::vector<size_t>{offset, offset + 1,
	offset + 2, offset + 3});
    fills.push_back(fill);
    return true;
}


bool
append_closed_curve_loop(STEPWrapper &wrapper, uint64_t curve_id,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, std::vector<std::array<double, 2> > &vertices,
    std::vector<size_t> &loop, std::string &reason)
{
    loop.clear();
    if (has_exact_type(wrapper, curve_id, "POLYLINE")) {
	STEPentity *polyline = curve_id <= static_cast<uint64_t>(INT_MAX) ?
	    dynamic_cast<STEPentity *>(wrapper.getEntity(
		static_cast<int>(curve_id))) : NULL;
	const std::vector<SDAI_Application_instance *> points = polyline ?
	    brlcad::step::Entities(polyline, "points") :
	    std::vector<SDAI_Application_instance *>();
	if (points.size() < 4) {
	    reason = "annotation fill polyline is not closed";
	    return false;
	}
	const size_t offset = vertices.size();
	for (SDAI_Application_instance *point_entity : points) {
	    CartesianPoint *point = dynamic_cast<CartesianPoint *>(
		Factory::CreateObject(&wrapper, point_entity));
	    if (!point || !point->Point3d()) {
		vertices.resize(offset);
		reason = "annotation fill polyline point could not be loaded";
		return false;
	    }
	    point_t world;
	    std::array<double, 2> projected;
	    VSCALE(world, point->Point3d(), length);
	    project_world_point(world, plane_origin, plane_x, plane_y,
		projected);
	    vertices.push_back(projected);
	    loop.push_back(vertices.size() - 1);
	}
	const std::array<double, 2> &first = vertices[loop.front()];
	const std::array<double, 2> &last = vertices[loop.back()];
	if (std::hypot(first[0] - last[0], first[1] - last[1]) >
		std::max(1.0e-7, length * 1.0e-9)) {
	    vertices.resize(offset);
	    loop.clear();
	    reason = "annotation fill polyline is not closed";
	    return false;
	}
	vertices.pop_back();
	loop.pop_back();
	return loop.size() >= 3;
    }
    if (has_exact_type(wrapper, curve_id, "CIRCLE")) {
	STEPentity *circle = curve_id <= static_cast<uint64_t>(INT_MAX) ?
	    dynamic_cast<STEPentity *>(wrapper.getEntity(
		static_cast<int>(curve_id))) : NULL;
	const double radius = circle ? wrapper.getRealAttribute(circle,
	    "radius") * length : 0.0;
	point_t origin;
	vect_t x_axis, y_axis, z_axis;
	bool located = false;
	if (!circle || !std::isfinite(radius) || radius <= 0.0 ||
		!placement_axes(wrapper, curve_id, length, origin, x_axis,
		    y_axis, z_axis, &located) || !located) {
	    reason = "annotation fill circle is malformed";
	    return false;
	}
	const size_t segments = 64;
	for (size_t i = 0; i < segments; ++i) {
	    const double angle = 2.0 * M_PI * static_cast<double>(i) /
		static_cast<double>(segments);
	    point_t world;
	    std::array<double, 2> projected;
	    VJOIN2(world, origin, radius * std::cos(angle), x_axis,
		radius * std::sin(angle), y_axis);
	    project_world_point(world, plane_origin, plane_x, plane_y,
		projected);
	    vertices.push_back(projected);
	    loop.push_back(vertices.size() - 1);
	}
	return true;
    }
    /* The AP242 dimension-two rule permits closed ellipses, B-splines and
     * composite curves in addition to circles and closed polylines.  Reuse
     * the production curve conversion so rational and knot variants are
     * covered as well. */
    if (wrapper.LazyIsSchemaEntity(curve_id, "CURVE")) {
	std::vector<std::array<double, 2> > sampled;
	bool closed = false;
	if (!sample_annotation_curve(wrapper, curve_id, length, plane_origin,
		plane_x, plane_y, true, sampled, closed, reason))
	    return false;
	for (const std::array<double, 2> &point : sampled) {
	    vertices.push_back(point);
	    loop.push_back(vertices.size() - 1);
	}
	return loop.size() >= 3;
    }
    reason = "unsupported annotation fill boundary curve " +
	wrapper.LazyTypeName(curve_id);
    return false;
}


bool
append_fill_area_entity(STEPWrapper &wrapper, STEPentity *area, double length,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<brlcad::step::AnnotationFill> &fills, std::string &reason)
{
    const std::vector<SDAI_Application_instance *> boundaries = area ?
	brlcad::step::Entities(area, "boundaries") :
	std::vector<SDAI_Application_instance *>();
    if (boundaries.empty()) {
	reason = "annotation fill area has no boundaries";
	return false;
    }
    const size_t vertex_start = vertices.size();
    brlcad::step::AnnotationFill fill;
    for (SDAI_Application_instance *boundary : boundaries) {
	std::vector<size_t> loop;
	if (!boundary || boundary->STEPfile_id <= 0 ||
		!append_closed_curve_loop(wrapper,
		    static_cast<uint64_t>(boundary->STEPfile_id), length,
		    plane_origin, plane_x, plane_y, vertices, loop, reason)) {
	    vertices.resize(vertex_start);
	    return false;
	}
	fill.loops.push_back(loop);
    }
    fills.push_back(fill);
    return true;
}


bool
append_fill_area(STEPWrapper &wrapper, uint64_t occurrence_id, double length,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<brlcad::step::AnnotationFill> &fills, std::string &reason)
{
    STEPentity *occurrence = occurrence_id <= static_cast<uint64_t>(INT_MAX) ?
	dynamic_cast<STEPentity *>(wrapper.getEntity(
	    static_cast<int>(occurrence_id))) : NULL;
    STEPentity *area = occurrence ? dynamic_cast<STEPentity *>(
	brlcad::step::Entity(occurrence, "item")) : NULL;
    return append_fill_area_entity(wrapper, area, length, plane_origin,
	plane_x, plane_y, vertices, fills, reason);
}


bool
append_geometric_curve_set(STEPWrapper &wrapper, uint64_t curve_set_id,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines, std::string &reason)
{
    STEPentity *curve_set = curve_set_id <= static_cast<uint64_t>(INT_MAX) ?
	dynamic_cast<STEPentity *>(wrapper.getEntity(
	    static_cast<int>(curve_set_id))) : NULL;
    const std::vector<SDAI_Application_instance *> elements = curve_set ?
	brlcad::step::Entities(curve_set, "elements") :
	std::vector<SDAI_Application_instance *>();
    if (elements.empty()) {
	reason = "annotation geometric curve set is empty or malformed";
	return false;
    }
    for (SDAI_Application_instance *element : elements) {
	if (!element || element->STEPfile_id <= 0) {
	    reason = "annotation geometric curve set has a missing element";
	    return false;
	}
	const uint64_t element_id = static_cast<uint64_t>(element->STEPfile_id);
	if (has_exact_type(wrapper, element_id, "POLYLINE")) {
	    if (!append_polyline(wrapper, element_id, length, plane_origin,
		    plane_x, plane_y, vertices, lines, reason))
		return false;
	} else if (has_exact_type(wrapper, element_id, "CIRCLE")) {
	    if (!append_circle(wrapper, element_id, length, plane_origin,
		    plane_x, plane_y, vertices, lines, reason))
		return false;
	} else if (wrapper.LazyIsSchemaEntity(element_id, "CURVE")) {
	    if (!append_annotation_curve_strokes(wrapper, element_id, length,
		    plane_origin, plane_x, plane_y, vertices, lines, reason))
		return false;
	} else {
	    reason = "unsupported annotation geometric curve type " +
		wrapper.LazyTypeName(element_id);
	    return false;
	}
    }
    return true;
}


std::string
placeholder_semantic_text(STEPWrapper &wrapper, uint64_t occurrence_id)
{
    for (uint64_t association : wrapper.LazyReverseReferences(occurrence_id)) {
	if (!wrapper.LazyIsSchemaEntity(association,
		"DRAUGHTING_MODEL_ITEM_ASSOCIATION_WITH_PLACEHOLDER") ||
		association > static_cast<uint64_t>(INT_MAX))
	    continue;
	STEPentity *usage = dynamic_cast<STEPentity *>(wrapper.getEntity(
	    static_cast<int>(association)));
	STEPentity *definition = usage ? dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(usage, "definition")) : NULL;
	if (!definition || !wrapper.IsSchemaEntity(definition,
		"PROPERTY_DEFINITION") || definition->STEPfile_id <= 0)
	    continue;
	const uint64_t definition_id =
	    static_cast<uint64_t>(definition->STEPfile_id);
	for (uint64_t link : wrapper.LazyReverseReferences(definition_id)) {
	    if (!wrapper.LazyIsSchemaEntity(link,
		    "PROPERTY_DEFINITION_REPRESENTATION") ||
		    link > static_cast<uint64_t>(INT_MAX))
		continue;
	    STEPentity *relationship = dynamic_cast<STEPentity *>(
		wrapper.getEntity(static_cast<int>(link)));
	    STEPentity *representation = relationship ?
		dynamic_cast<STEPentity *>(brlcad::step::Entity(relationship,
		    "used_representation")) : NULL;
	    std::string text;
	    for (SDAI_Application_instance *item :
		    brlcad::step::Entities(representation, "items")) {
		if (!item || !wrapper.IsSchemaEntity(item,
			"DESCRIPTIVE_REPRESENTATION_ITEM"))
		    continue;
		std::string value = brlcad::step::decode_string(
		    wrapper.getStringAttribute(item, "description"));
		if (value == "/n" || value == "\\n")
		    text.push_back('\n');
		else
		    text += value;
	    }
	    if (!text.empty()) return text;
	}
    }
    return std::string();
}


bool
append_annotation_placeholder(STEPWrapper &wrapper, uint64_t occurrence_id,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines,
    std::vector<brlcad::step::AnnotationText> &texts,
    bool &used_default_size, std::string &reason)
{
    STEPentity *occurrence = occurrence_id <= static_cast<uint64_t>(INT_MAX) ?
	dynamic_cast<STEPentity *>(wrapper.getEntity(
	    static_cast<int>(occurrence_id))) : NULL;
    STEPentity *geometry = occurrence ? dynamic_cast<STEPentity *>(
	brlcad::step::Entity(occurrence, "item")) : NULL;
    const std::vector<SDAI_Application_instance *> elements = geometry ?
	brlcad::step::Entities(geometry, "elements") :
	std::vector<SDAI_Application_instance *>();
    if (elements.empty()) {
	reason = "annotation placeholder geometric set is empty or malformed";
	return false;
    }
    const MarkerStyle marker = point_marker_style(wrapper, occurrence_id,
	length);
    bool drawable = false;
    bool have_text_frame = false;
    bool retained_placement = false;
    std::array<double, 2> text_anchor = {{0.0, 0.0}};
    double text_frame[4] = {1.0, 0.0, 0.0, 1.0};
    for (SDAI_Application_instance *element : elements) {
	if (!element || element->STEPfile_id <= 0) {
	    reason = "annotation placeholder has a missing element";
	    return false;
	}
	const uint64_t element_id =
	    static_cast<uint64_t>(element->STEPfile_id);
	if (wrapper.IsSchemaEntity(element, "AXIS2_PLACEMENT_3D")) {
	    Axis2Placement3D *placement = dynamic_cast<Axis2Placement3D *>(
		Factory::CreateObject(&wrapper, element));
	    if (!placement || !placement->GetOrigin() ||
		    !placement->GetXAxis() || !placement->GetYAxis()) {
		reason = "annotation placeholder 3D text frame could not be loaded";
		return false;
	    }
	    point_t world;
	    double candidate[4];
	    VSCALE(world, placement->GetOrigin(), length);
	    candidate[0] = VDOT(placement->GetXAxis(), plane_x);
	    candidate[2] = VDOT(placement->GetXAxis(), plane_y);
	    candidate[1] = VDOT(placement->GetYAxis(), plane_x);
	    candidate[3] = VDOT(placement->GetYAxis(), plane_y);
	    retained_placement = true;
	    if (std::isfinite(candidate[0]) && std::isfinite(candidate[1]) &&
		    std::isfinite(candidate[2]) && std::isfinite(candidate[3]) &&
		    std::fabs(candidate[0] * candidate[3] -
			candidate[1] * candidate[2]) > SMALL_FASTF) {
		project_world_point(world, plane_origin, plane_x, plane_y,
		    text_anchor);
		std::copy(candidate, candidate + 4, text_frame);
		have_text_frame = true;
	    }
	    continue;
	}
	if (wrapper.IsSchemaEntity(element, "AXIS2_PLACEMENT_2D")) {
	    Axis2Placement2D *placement = dynamic_cast<Axis2Placement2D *>(
		Factory::CreateObject(&wrapper, element));
	    if (!placement || !placement->GetOrigin() ||
		    !placement->GetXAxis() || !placement->GetYAxis()) {
		reason = "annotation placeholder 2D text frame could not be loaded";
		return false;
	    }
	    double candidate[4] = {placement->GetXAxis()[0],
		placement->GetYAxis()[0], placement->GetXAxis()[1],
		placement->GetYAxis()[1]};
	    retained_placement = true;
	    if (std::isfinite(candidate[0]) && std::isfinite(candidate[1]) &&
		    std::isfinite(candidate[2]) && std::isfinite(candidate[3]) &&
		    std::fabs(candidate[0] * candidate[3] -
			candidate[1] * candidate[2]) > SMALL_FASTF) {
		text_anchor = {{placement->GetOrigin()[0] * length,
		    placement->GetOrigin()[1] * length}};
		std::copy(candidate, candidate + 4, text_frame);
		have_text_frame = true;
	    }
	    continue;
	}
	if (wrapper.IsSchemaEntity(element, "CURVE")) {
	    if (!append_annotation_curve_strokes(wrapper, element_id, length,
		    plane_origin, plane_x, plane_y, vertices, lines, reason))
		return false;
	    drawable = true;
	    continue;
	}
	if (wrapper.IsSchemaEntity(element, "GEOMETRIC_CURVE_SET")) {
	    if (!append_geometric_curve_set(wrapper, element_id, length,
		    plane_origin, plane_x, plane_y, vertices, lines, reason))
		return false;
	    drawable = true;
	    continue;
	}
	if (wrapper.IsSchemaEntity(element, "CARTESIAN_POINT")) {
	    CartesianPoint *point = dynamic_cast<CartesianPoint *>(
		Factory::CreateObject(&wrapper, element));
	    if (!point || !point->Point3d()) {
		reason = "annotation placeholder point could not be loaded";
		return false;
	    }
	    point_t world;
	    std::array<double, 2> projected;
	    VSCALE(world, point->Point3d(), length);
	    project_world_point(world, plane_origin, plane_x, plane_y,
		projected);
	    append_marker(projected, marker, vertices, lines);
	    drawable = true;
	    continue;
	}
	reason = "unsupported annotation placeholder element " +
	    wrapper.LazyTypeName(element_id);
	return false;
    }
    if (have_text_frame) {
	std::string label = placeholder_semantic_text(wrapper, occurrence_id);
	if (label.empty()) label = brlcad::step::decode_string(
	    wrapper.getStringAttribute(occurrence, "name"));
	if (label.empty()) {
	    reason = "annotation placeholder has no display label";
	    return false;
	}
	double size = wrapper.getRealAttribute(occurrence, "line_spacing") *
	    length;
	if (!std::isfinite(size) || size <= 0.0) {
	    size = 3.5;
	    used_default_size = true;
	}
	brlcad::step::AnnotationText text;
	text.reference_vertex = vertices.size();
	text.label = label;
	text.size = size;
	text.font = "osifont";
	text.role = RT_ANNOT_ROLE_PLACEHOLDER;
	text.symbol = "annotation placeholder";
	text.x_scale = text_frame[0];
	text.xy_scale = text_frame[1];
	text.yx_scale = text_frame[2];
	text.y_scale = text_frame[3];
	if (!std::isfinite(text.x_scale) || !std::isfinite(text.xy_scale) ||
		!std::isfinite(text.yx_scale) || !std::isfinite(text.y_scale) ||
		std::fabs(text.x_scale * text.y_scale -
		    text.xy_scale * text.yx_scale) <= SMALL_FASTF) {
	    reason = "annotation placeholder text frame is singular";
	    return false;
	}
	vertices.push_back(text_anchor);
	texts.push_back(text);
	drawable = true;
    }
    if (!drawable) {
	/* Some valid AP242 producers use a placement merely as retained
	 * placeholder geometry, including a frame normal to the annotation
	 * plane.  Such a placement has no invertible 2D text transform.  It is
	 * not corrupt presentation data, and a sibling tessellated occurrence
	 * normally supplies the actual display geometry. */
	if (retained_placement) return true;
	reason = "annotation placeholder has no drawable geometry or text frame";
	return false;
    }
    return true;
}


bool
append_annotation_point(STEPWrapper &wrapper, uint64_t occurrence_id,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, const MarkerStyle &style,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines, std::string &reason)
{
    SDAI_Application_instance *point_entity = NULL;
    for (uint64_t reference : wrapper.LazyForwardReferences(occurrence_id))
	if (wrapper.LazyIsSchemaEntity(reference, "CARTESIAN_POINT") &&
		reference <= static_cast<uint64_t>(INT_MAX)) {
	    point_entity = wrapper.getEntity(static_cast<int>(reference));
	    break;
	}
    CartesianPoint *point = dynamic_cast<CartesianPoint *>(
	Factory::CreateObject(&wrapper, point_entity));
    if (!point || !point->Point3d()) {
	reason = "annotation point occurrence could not be loaded";
	return false;
    }
    point_t world;
    std::array<double, 2> projected;
    VSCALE(world, point->Point3d(), length);
    project_world_point(world, plane_origin, plane_x, plane_y, projected);
    append_marker(projected, style, vertices, lines);
    return true;
}


int
text_relative_position(const std::string &source_alignment)
{
    std::string alignment = source_alignment;
    std::transform(alignment.begin(), alignment.end(), alignment.begin(),
	[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const bool centre = contains(alignment, "centre") ||
	contains(alignment, "center");
    const bool right = contains(alignment, "right");
    const bool middle = alignment.compare(0, 6, "middle") == 0;
    const bool top = alignment.compare(0, 3, "cap") == 0 ||
	alignment.compare(0, 3, "top") == 0;
    if (top) return right ? RT_TXT_POS_TR :
	(centre ? RT_TXT_POS_TC : RT_TXT_POS_TL);
    if (middle) return right ? RT_TXT_POS_MR :
	(centre ? RT_TXT_POS_MC : RT_TXT_POS_ML);
    return right ? RT_TXT_POS_BR : (centre ? RT_TXT_POS_BC : RT_TXT_POS_BL);
}


bool
placement_frame_2d(STEPWrapper &wrapper,
    SDAI_Application_instance *placement_instance, double length,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    Affine2D &frame)
{
    if (!placement_instance) return false;
    if (wrapper.IsSchemaEntity(placement_instance, "AXIS2_PLACEMENT_2D")) {
	Axis2Placement2D *placement = dynamic_cast<Axis2Placement2D *>(
	    Factory::CreateObject(&wrapper, placement_instance));
	if (!placement || !placement->GetOrigin() || !placement->GetXAxis() ||
		!placement->GetYAxis())
	    return false;
	frame.origin[0] = placement->GetOrigin()[0] * length;
	frame.origin[1] = placement->GetOrigin()[1] * length;
	frame.matrix[0] = placement->GetXAxis()[0];
	frame.matrix[2] = placement->GetXAxis()[1];
	frame.matrix[1] = placement->GetYAxis()[0];
	frame.matrix[3] = placement->GetYAxis()[1];
	return true;
    }
    if (wrapper.IsSchemaEntity(placement_instance, "AXIS2_PLACEMENT_3D")) {
	Axis2Placement3D *placement = dynamic_cast<Axis2Placement3D *>(
	    Factory::CreateObject(&wrapper, placement_instance));
	if (!placement || !placement->GetOrigin() || !placement->GetXAxis() ||
		!placement->GetYAxis())
	    return false;
	point_t world;
	std::array<double, 2> projected;
	VSCALE(world, placement->GetOrigin(), length);
	project_world_point(world, plane_origin, plane_x, plane_y, projected);
	frame.origin[0] = projected[0];
	frame.origin[1] = projected[1];
	frame.matrix[0] = VDOT(placement->GetXAxis(), plane_x);
	frame.matrix[2] = VDOT(placement->GetXAxis(), plane_y);
	frame.matrix[1] = VDOT(placement->GetYAxis(), plane_x);
	frame.matrix[3] = VDOT(placement->GetYAxis(), plane_y);
	return true;
    }
    return false;
}


bool
mapped_item_affine(STEPWrapper &wrapper, STEPentity *mapped_item,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, Affine2D &affine, STEPentity **representation,
    std::string &reason)
{
    STEPentity *source = mapped_item ? dynamic_cast<STEPentity *>(
	brlcad::step::Entity(mapped_item, "mapping_source")) : NULL;
    SDAI_Application_instance *source_placement = source ?
	brlcad::step::Entity(source, "mapping_origin") : NULL;
    STEPentity *target = mapped_item ? dynamic_cast<STEPentity *>(
	brlcad::step::Entity(mapped_item, "mapping_target")) : NULL;
    SDAI_Application_instance *target_placement = target;
    double x_scale = 1.0;
    double y_scale = 1.0;
    if (target && wrapper.IsSchemaEntity(target, "SYMBOL_TARGET")) {
	target_placement = brlcad::step::Entity(target, "placement");
	x_scale = wrapper.getRealAttribute(target, "x_scale");
	y_scale = wrapper.getRealAttribute(target, "y_scale");
    }
    STEPentity *mapped_representation = source ? dynamic_cast<STEPentity *>(
	brlcad::step::Entity(source, "mapped_representation")) : NULL;
    Affine2D source_frame, target_frame;
    if (!source || !target || !mapped_representation ||
		!std::isfinite(x_scale) || x_scale <= 0.0 ||
		!std::isfinite(y_scale) || y_scale <= 0.0 ||
		!placement_frame_2d(wrapper, source_placement, length,
		    plane_origin, plane_x, plane_y, source_frame) ||
		!placement_frame_2d(wrapper, target_placement, length,
		    plane_origin, plane_x, plane_y, target_frame)) {
	reason = "mapped annotation source or target placement is malformed";
	return false;
    }
    const double determinant = source_frame.matrix[0] *
	source_frame.matrix[3] - source_frame.matrix[1] * source_frame.matrix[2];
    if (!std::isfinite(determinant) || std::fabs(determinant) <= SMALL_FASTF) {
	reason = "mapped annotation source placement is singular";
	return false;
    }
    const double inverse[4] = {
	source_frame.matrix[3] / determinant,
	-source_frame.matrix[1] / determinant,
	-source_frame.matrix[2] / determinant,
	source_frame.matrix[0] / determinant
    };
    const double target_scaled[4] = {
	target_frame.matrix[0] * x_scale,
	target_frame.matrix[1] * y_scale,
	target_frame.matrix[2] * x_scale,
	target_frame.matrix[3] * y_scale
    };
    affine.matrix[0] = target_scaled[0] * inverse[0] +
	target_scaled[1] * inverse[2];
    affine.matrix[1] = target_scaled[0] * inverse[1] +
	target_scaled[1] * inverse[3];
    affine.matrix[2] = target_scaled[2] * inverse[0] +
	target_scaled[3] * inverse[2];
    affine.matrix[3] = target_scaled[2] * inverse[1] +
	target_scaled[3] * inverse[3];
    affine.origin[0] = target_frame.origin[0] -
	(affine.matrix[0] * source_frame.origin[0] +
	 affine.matrix[1] * source_frame.origin[1]);
    affine.origin[1] = target_frame.origin[1] -
	(affine.matrix[2] * source_frame.origin[0] +
	 affine.matrix[3] * source_frame.origin[1]);
    if (representation) *representation = mapped_representation;
    return true;
}


void
apply_affine(const Affine2D &affine, size_t vertex_start, size_t text_start,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<brlcad::step::AnnotationText> &texts)
{
    for (size_t i = vertex_start; i < vertices.size(); ++i) {
	const double x = vertices[i][0];
	const double y = vertices[i][1];
	vertices[i][0] = affine.origin[0] + affine.matrix[0]*x +
	    affine.matrix[1]*y;
	vertices[i][1] = affine.origin[1] + affine.matrix[2]*x +
	    affine.matrix[3]*y;
    }
    for (size_t i = text_start; i < texts.size(); ++i) {
	const double old[4] = {texts[i].x_scale, texts[i].xy_scale,
	    texts[i].yx_scale, texts[i].y_scale};
	texts[i].x_scale = affine.matrix[0]*old[0] +
	    affine.matrix[1]*old[2];
	texts[i].xy_scale = affine.matrix[0]*old[1] +
	    affine.matrix[1]*old[3];
	texts[i].yx_scale = affine.matrix[2]*old[0] +
	    affine.matrix[3]*old[2];
	texts[i].y_scale = affine.matrix[2]*old[1] +
	    affine.matrix[3]*old[3];
    }
}


bool
text_placement(STEPWrapper &wrapper, STEPentity *literal, double length,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    std::array<double, 2> &anchor, double &rotation, std::string &reason)
{
    SDAI_Application_instance *placement_instance = literal ?
	brlcad::step::Entity(literal, "placement") : NULL;
    if (!placement_instance) {
	reason = "annotation text literal has no placement";
	return false;
    }
    const double *origin = NULL;
    const double *x_axis = NULL;
    if (wrapper.IsSchemaEntity(placement_instance, "AXIS2_PLACEMENT_3D")) {
	Axis2Placement3D *placement = dynamic_cast<Axis2Placement3D *>(
	    Factory::CreateObject(&wrapper, placement_instance));
	if (placement) {
	    origin = placement->GetOrigin();
	    x_axis = placement->GetXAxis();
	}
	if (!origin || !x_axis) {
	    reason = "annotation text 3D placement could not be loaded";
	    return false;
	}
	point_t world;
	VSCALE(world, origin, length);
	project_world_point(world, plane_origin, plane_x, plane_y, anchor);
	rotation = std::atan2(VDOT(x_axis, plane_y),
	    VDOT(x_axis, plane_x)) * RAD2DEG;
    } else if (wrapper.IsSchemaEntity(placement_instance,
	    "AXIS2_PLACEMENT_2D")) {
	Axis2Placement2D *placement = dynamic_cast<Axis2Placement2D *>(
	    Factory::CreateObject(&wrapper, placement_instance));
	if (placement) {
	    origin = placement->GetOrigin();
	    x_axis = placement->GetXAxis();
	}
	if (!origin || !x_axis) {
	    reason = "annotation text 2D placement could not be loaded";
	    return false;
	}
	anchor = {{origin[0] * length, origin[1] * length}};
	rotation = std::atan2(x_axis[1], x_axis[0]) * RAD2DEG;
    } else {
	reason = "annotation text has an unsupported placement type";
	return false;
    }
    if (!std::isfinite(anchor[0]) || !std::isfinite(anchor[1]) ||
	    !std::isfinite(rotation)) {
	reason = "annotation text placement is malformed";
	return false;
    }
    return true;
}


bool
append_text_literal(STEPWrapper &wrapper, STEPentity *literal, double length,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<brlcad::step::AnnotationText> &texts, bool &used_default_size,
    std::string &reason)
{
    const std::string label = brlcad::step::decode_string(
	wrapper.getStringAttribute(literal, "literal"));
    if (label.empty()) {
	reason = "annotation text literal is empty or malformed";
	return false;
    }
    std::array<double, 2> anchor;
    double rotation = 0.0;
    if (!text_placement(wrapper, literal, length, plane_origin, plane_x,
	    plane_y, anchor, rotation, reason))
	return false;

    const std::string path = wrapper.getEnumAttributeName(literal, "path");
    if (contains(path, "left")) rotation += 180.0;
    else if (contains(path, "up")) rotation += 90.0;
    else if (contains(path, "down")) rotation -= 90.0;

    double size = 3.5;
    STEPentity *extent = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(literal, "extent"));
    if (extent) {
	size = wrapper.getRealAttribute(extent, "size_in_y") * length;
    } else {
	used_default_size = true;
    }
    if (!std::isfinite(size) || size <= 0.0) {
	reason = "annotation text extent is malformed";
	return false;
    }

    STEPentity *font = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(literal, "font"));
    std::string font_name = font ? brlcad::step::decode_string(
	wrapper.getStringAttribute(font, "name")) : std::string();
    if (font_name.empty()) font_name = "osifont";

    brlcad::step::AnnotationText text;
    text.reference_vertex = vertices.size();
    text.label = label;
    text.size = size;
    text.rotation = rotation;
    text.relative_position = text_relative_position(brlcad::step::decode_string(
	wrapper.getStringAttribute(literal, "alignment")));
    text.font = font_name;
    vertices.push_back(anchor);
    texts.push_back(text);
    return true;
}


bool append_mapped_presentation(STEPWrapper &wrapper, STEPentity *mapped,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines,
    std::vector<brlcad::step::AnnotationText> &texts,
    std::vector<brlcad::step::AnnotationFill> &fills,
    std::set<uint64_t> &active, bool &used_default_size,
    bool symbol_role, const std::string &symbol_name, std::string &reason,
    std::vector<brlcad::step::AnnotationLineStyle> *line_styles = NULL);

bool append_defined_character_glyph(STEPWrapper &wrapper, STEPentity *glyph,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, std::vector<std::array<double, 2> > &vertices,
    std::vector<brlcad::step::AnnotationText> &texts, std::string &reason);


void
apply_text_delineation(STEPWrapper &wrapper, STEPentity *item,
    size_t text_start, std::vector<brlcad::step::AnnotationText> &texts)
{
    if (!item ||
	    (!wrapper.IsSchemaEntity(item, "TEXT_LITERAL_WITH_DELINEATION") &&
	     !wrapper.IsSchemaEntity(item,
		 "COMPOSITE_TEXT_WITH_DELINEATION")))
	return;
    std::string delineation = brlcad::step::decode_string(
	wrapper.getStringAttribute(item, "delineation"));
    std::transform(delineation.begin(), delineation.end(),
	delineation.begin(),
	[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    uint32_t flag = 0;
    if (contains(delineation, "underline"))
	flag = RT_ANNOT_STYLE_UNDERLINE;
    else if (contains(delineation, "overline"))
	flag = RT_ANNOT_STYLE_OVERLINE;
    if (!flag) return;
    for (size_t i = text_start; i < texts.size(); ++i)
	texts[i].style_flags |= flag;
}


bool
append_text_presentation_extras(STEPWrapper &wrapper, STEPentity *item,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > *lines,
    std::vector<brlcad::step::AnnotationFill> *fills, std::string &reason)
{
    const bool associated = item &&
	(wrapper.IsSchemaEntity(item, "TEXT_LITERAL_WITH_ASSOCIATED_CURVES") ||
	 wrapper.IsSchemaEntity(item, "COMPOSITE_TEXT_WITH_ASSOCIATED_CURVES"));
    if (associated) {
	if (!lines) {
	    reason = "annotation text associated curves have no stroke target";
	    return false;
	}
	const std::vector<SDAI_Application_instance *> curves =
	    brlcad::step::Entities(item, "associated_curves");
	if (curves.empty()) {
	    reason = "annotation text associated curve set is empty";
	    return false;
	}
	for (SDAI_Application_instance *curve : curves) {
	    if (!curve || curve->STEPfile_id <= 0 ||
		    !append_annotation_curve_strokes(wrapper,
			static_cast<uint64_t>(curve->STEPfile_id), length,
			plane_origin, plane_x, plane_y, vertices, *lines,
			reason))
		return false;
	}
    }

    const bool blanked = item &&
	(wrapper.IsSchemaEntity(item, "TEXT_LITERAL_WITH_BLANKING_BOX") ||
	 wrapper.IsSchemaEntity(item, "COMPOSITE_TEXT_WITH_BLANKING_BOX"));
    if (blanked) {
	if (!fills) {
	    reason = "annotation text blanking box has no fill target";
	    return false;
	}
	STEPentity *box = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(item, "blanking"));
	if (!append_planar_box_fill(wrapper, box, length, plane_origin,
		plane_x, plane_y, vertices, *fills, reason))
	    return false;
    }
    return true;
}


bool
append_text_item(STEPWrapper &wrapper, uint64_t item_id, double length,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<brlcad::step::AnnotationText> &texts,
    std::set<uint64_t> &active, bool &used_default_size, std::string &reason,
    std::vector<std::pair<size_t, size_t> > *mapped_lines = NULL,
    std::vector<brlcad::step::AnnotationFill> *mapped_fills = NULL)
{
    if (!item_id || item_id > static_cast<uint64_t>(INT_MAX) ||
	    !active.insert(item_id).second) {
	reason = "cyclic or missing annotation text item";
	return false;
    }
    STEPentity *item = dynamic_cast<STEPentity *>(wrapper.getEntity(
	static_cast<int>(item_id)));
    const size_t text_start = texts.size();
    bool valid = false;
    if (item && wrapper.IsSchemaEntity(item, "TEXT_LITERAL")) {
	valid = append_text_literal(wrapper, item, length, plane_origin,
	    plane_x, plane_y, vertices, texts, used_default_size, reason);
	if (valid)
	    valid = append_text_presentation_extras(wrapper, item, length,
		plane_origin, plane_x, plane_y, vertices, mapped_lines,
		mapped_fills, reason);
    } else if (item && wrapper.IsSchemaEntity(item, "COMPOSITE_TEXT")) {
	const std::vector<SDAI_Application_instance *> collected =
	    brlcad::step::Entities(item, "collected_text");
	valid = collected.size() >= 2;
	if (!valid)
	    reason = "annotation composite text has fewer than two items";
	for (SDAI_Application_instance *child : collected) {
	    if (!valid || !child || child->STEPfile_id <= 0 ||
		    !append_text_item(wrapper,
			static_cast<uint64_t>(child->STEPfile_id), length,
			plane_origin, plane_x, plane_y, vertices, texts, active,
			used_default_size, reason, mapped_lines, mapped_fills)) {
		valid = false;
		break;
	    }
	}
	if (valid)
	    valid = append_text_presentation_extras(wrapper, item, length,
		plane_origin, plane_x, plane_y, vertices, mapped_lines,
		mapped_fills, reason);
	} else if (item && (wrapper.IsSchemaEntity(item, "ANNOTATION_TEXT") ||
		wrapper.IsSchemaEntity(item, "ANNOTATION_TEXT_CHARACTER"))) {
	std::vector<std::pair<size_t, size_t> > ignored_lines;
	std::vector<brlcad::step::AnnotationFill> ignored_fills;
	std::vector<std::pair<size_t, size_t> > &target_lines = mapped_lines ?
	    *mapped_lines : ignored_lines;
	std::vector<brlcad::step::AnnotationFill> &target_fills = mapped_fills ?
	    *mapped_fills : ignored_fills;
	valid = append_mapped_presentation(wrapper, item, length, plane_origin,
	    plane_x, plane_y, vertices, target_lines, texts, target_fills,
	    active, used_default_size,
	    wrapper.IsSchemaEntity(item, "ANNOTATION_TEXT_CHARACTER"),
	    std::string(), reason);
	if (valid && !mapped_lines &&
		(!ignored_lines.empty() || !ignored_fills.empty())) {
	    reason = "mapped text unexpectedly contains non-text geometry";
	    valid = false;
	}
	} else if (item && wrapper.IsSchemaEntity(item,
		"DEFINED_CHARACTER_GLYPH")) {
	valid = append_defined_character_glyph(wrapper, item, length,
	    plane_origin, plane_x, plane_y, vertices, texts, reason);
    } else {
	reason = "unsupported annotation text item " +
	    wrapper.LazyTypeName(item_id);
    }
    if (valid) apply_text_delineation(wrapper, item, text_start, texts);
    active.erase(item_id);
    return valid;
}


std::string
defined_item_name(STEPWrapper &wrapper, STEPentity *item)
{
    if (!item) return std::string();
    std::string name = brlcad::step::decode_string(
	wrapper.getStringAttribute(item, "name"));
    if (!name.empty()) return name;
    const std::vector<std::string> arguments = source_arguments(
	wrapper.LazySourceRecord(static_cast<uint64_t>(item->STEPfile_id)));
    return arguments.empty() ? std::string() :
	brlcad::step::decode_string(arguments.front());
}


std::string
symbol_label(const std::string &source)
{
    std::string name = source;
    std::transform(name.begin(), name.end(), name.begin(),
	[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    static const std::map<std::string, std::string> labels = {
	{"diameter", "\xe2\x8c\x80"}, {"plus minus", "\xc2\xb1"},
	{"radius", "R"}, {"spherical diameter", "S\xe2\x8c\x80"},
	{"spherical radius", "SR"}, {"square", "\xe2\x96\xa1"},
	{"arc length", "\xe2\x8c\x92"}, {"straightness", "\xe2\x80\x94"},
	{"flatness", "\xe2\x8f\xa5"}, {"circularity", "\xe2\x97\x8b"},
	{"cylindricity", "\xe2\x8c\xad"}, {"profile of a line", "\xe2\x8c\x92"},
	{"profile of a surface", "\xe2\x8c\x93"}, {"parallelism", "\xe2\x88\xa5"},
	{"perpendicularity", "\xe2\x9f\x82"}, {"angularity", "\xe2\x88\xa0"},
	{"position", "\xe2\x8c\x96"}, {"concentricity", "\xe2\x97\x8e"},
	{"symmetry", "\xe2\x89\xa1"}, {"circular runout", "\xe2\x86\x97"},
	{"total runout", "\xe2\x87\x88"}, {"least material condition", "L"},
	{"maximum material condition", "M"},
	{"regardless of feature size", "S"}, {"filled arrow", "\xe2\x96\xb6"},
	{"open arrow", "\xe2\x96\xb7"}, {"filled dot", "\xe2\x97\x8f"},
	{"blanked dot", "\xe2\x97\x8b"}, {"filled box", "\xe2\x96\xa0"},
	{"blanked box", "\xe2\x96\xa1"}, {"filled triangle", "\xe2\x96\xb2"},
	{"blanked triangle", "\xe2\x96\xb3"}, {"dimension origin", "\xe2\x97\x8e"},
	{"integral symbol", "\xe2\x88\xab"}
    };
    const auto found = labels.find(name);
    return found == labels.end() ? source : found->second;
}


bool
append_defined_character_glyph(STEPWrapper &wrapper, STEPentity *glyph,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, std::vector<std::array<double, 2> > &vertices,
    std::vector<brlcad::step::AnnotationText> &texts, std::string &reason)
{
    STEPentity *definition = glyph ? dynamic_cast<STEPentity *>(
	brlcad::step::Entity(glyph, "definition")) : NULL;
    SDAI_Application_instance *placement = glyph ?
	brlcad::step::Entity(glyph, "placement") : NULL;
    const std::string name = defined_item_name(wrapper, definition);
    Affine2D frame;
    if (name.empty() || !placement_frame_2d(wrapper, placement, length,
		plane_origin, plane_x, plane_y, frame)) {
	reason = "defined character glyph is malformed";
	return false;
    }
    brlcad::step::AnnotationText text;
    text.reference_vertex = vertices.size();
    text.label = symbol_label(name);
    text.size = 3.5;
    text.font = "osifont";
    text.role = RT_ANNOT_ROLE_SYMBOL;
    text.symbol = name;
    text.x_scale = frame.matrix[0];
    text.xy_scale = frame.matrix[1];
    text.yx_scale = frame.matrix[2];
    text.y_scale = frame.matrix[3];
    vertices.push_back({{frame.origin[0], frame.origin[1]}});
    texts.push_back(text);
    return true;
}


void append_line_styles(
    const std::vector<std::pair<size_t, size_t> > &lines, size_t start,
    uint32_t role, std::vector<brlcad::step::AnnotationLineStyle> &styles,
    const std::string &symbol);

void apply_presentation_colour(const std::array<unsigned char, 4> &colour,
    bool have_colour, size_t line_start, size_t text_start, size_t fill_start,
    std::vector<brlcad::step::AnnotationLineStyle> &line_styles,
    std::vector<brlcad::step::AnnotationText> &texts,
    std::vector<brlcad::step::AnnotationFill> &fills);

bool append_mapped_element(STEPWrapper &wrapper,
    SDAI_Application_instance *instance, double length,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines,
    std::vector<brlcad::step::AnnotationLineStyle> &line_styles,
    std::vector<brlcad::step::AnnotationText> &texts,
    std::vector<brlcad::step::AnnotationFill> &fills,
    std::set<uint64_t> &active, bool &used_default_size, std::string &reason);


bool
append_mapped_presentation(STEPWrapper &wrapper, STEPentity *mapped,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines,
    std::vector<brlcad::step::AnnotationText> &texts,
    std::vector<brlcad::step::AnnotationFill> &fills,
    std::set<uint64_t> &active, bool &used_default_size,
    bool symbol_role, const std::string &symbol_name, std::string &reason,
    std::vector<brlcad::step::AnnotationLineStyle> *line_styles)
{
    Affine2D affine;
    STEPentity *representation = NULL;
    if (!mapped_item_affine(wrapper, mapped, length, plane_origin, plane_x,
		plane_y, affine, &representation, reason))
	return false;
    const std::vector<SDAI_Application_instance *> items =
	brlcad::step::Entities(representation, "items");
    if (items.empty()) {
	reason = "mapped annotation representation is empty";
	return false;
    }
    const size_t vertex_start = vertices.size();
    const size_t line_start = lines.size();
    const size_t style_start = line_styles ? line_styles->size() : 0;
    const size_t text_start = texts.size();
    const size_t fill_start = fills.size();
    point_t local_origin = VINIT_ZERO;
    vect_t local_x = {1.0, 0.0, 0.0};
    vect_t local_y = {0.0, 1.0, 0.0};
    for (SDAI_Application_instance *instance : items) {
	if (!instance || instance->STEPfile_id <= 0) continue;
	const uint64_t item_id = static_cast<uint64_t>(instance->STEPfile_id);
	STEPentity *item = dynamic_cast<STEPentity *>(instance);
	const size_t item_line_start = lines.size();
	const size_t item_text_start = texts.size();
	const size_t item_fill_start = fills.size();
	if (wrapper.IsSchemaEntity(instance, "AXIS2_PLACEMENT_2D") ||
		wrapper.IsSchemaEntity(instance, "AXIS2_PLACEMENT_3D") ||
		wrapper.IsSchemaEntity(instance, "PLANAR_EXTENT")) continue;
	if (wrapper.IsSchemaEntity(instance, "ANNOTATION_OCCURRENCE") ||
		wrapper.IsSchemaEntity(instance, "DRAUGHTING_CALLOUT")) {
	    if (!line_styles || !append_mapped_element(wrapper, instance, length,
		    local_origin, local_x, local_y, vertices, lines,
		    *line_styles, texts, fills, active, used_default_size,
		    reason))
		goto mapped_error;
	    continue;
	} else if (wrapper.IsSchemaEntity(instance, "TEXT_LITERAL") ||
		wrapper.IsSchemaEntity(instance, "COMPOSITE_TEXT") ||
		wrapper.IsSchemaEntity(instance, "ANNOTATION_TEXT") ||
		wrapper.IsSchemaEntity(instance, "ANNOTATION_TEXT_CHARACTER") ||
		wrapper.IsSchemaEntity(instance, "DEFINED_CHARACTER_GLYPH")) {
	    if (!append_text_item(wrapper, item_id, length, local_origin,
		    local_x, local_y, vertices, texts, active,
		    used_default_size, reason, &lines, &fills))
		goto mapped_error;
	} else if (wrapper.IsSchemaEntity(instance, "ANNOTATION_FILL_AREA")) {
	    if (!append_fill_area_entity(wrapper, item, length, local_origin,
		    local_x, local_y, vertices, fills, reason))
		goto mapped_error;
	} else if (wrapper.IsSchemaEntity(instance, "POLYLINE")) {
	    if (!append_polyline(wrapper, item_id, length, local_origin,
		    local_x, local_y, vertices, lines, reason))
		goto mapped_error;
	} else if (wrapper.IsSchemaEntity(instance, "CIRCLE")) {
	    if (!append_circle(wrapper, item_id, length, local_origin,
		    local_x, local_y, vertices, lines, reason))
		goto mapped_error;
	} else if (wrapper.IsSchemaEntity(instance, "GEOMETRIC_CURVE_SET")) {
	    if (!append_geometric_curve_set(wrapper, item_id, length,
		    local_origin, local_x, local_y, vertices, lines, reason))
		goto mapped_error;
	} else if (wrapper.IsSchemaEntity(instance, "CURVE")) {
	    if (!append_annotation_curve_strokes(wrapper, item_id, length,
		    local_origin, local_x, local_y, vertices, lines, reason))
		goto mapped_error;
	} else {
	    reason = "unsupported mapped annotation representation item " +
		wrapper.LazyTypeName(item_id);
	    goto mapped_error;
	}
	if (symbol_role) {
	    for (size_t i = item_text_start; i < texts.size(); ++i) {
		texts[i].role = RT_ANNOT_ROLE_SYMBOL;
		if (!symbol_name.empty()) texts[i].symbol = symbol_name;
	    }
	    for (size_t i = item_fill_start; i < fills.size(); ++i) {
		fills[i].role = RT_ANNOT_ROLE_SYMBOL;
		if (!symbol_name.empty()) fills[i].symbol = symbol_name;
	    }
	}
	if (line_styles)
	    append_line_styles(lines, item_line_start, symbol_role ?
		RT_ANNOT_ROLE_SYMBOL : RT_ANNOT_ROLE_TEXT_DECORATION,
		*line_styles, symbol_role ? symbol_name :
		std::string("text associated curve"));
    }
    if (vertices.size() == vertex_start) {
	reason = "mapped annotation representation has no drawable items";
	goto mapped_error;
    }
    apply_affine(affine, vertex_start, text_start, vertices, texts);
    return true;

mapped_error:
    vertices.resize(vertex_start);
    lines.resize(line_start);
    if (line_styles) line_styles->resize(style_start);
    texts.resize(text_start);
    fills.resize(fill_start);
    return false;
}


bool
append_defined_symbol(STEPWrapper &wrapper, STEPentity *defined,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, std::vector<std::array<double, 2> > &vertices,
    std::vector<brlcad::step::AnnotationText> &texts, std::string &reason)
{
    STEPentity *definition = defined ? dynamic_cast<STEPentity *>(
	brlcad::step::Entity(defined, "definition")) : NULL;
    STEPentity *target = defined ? dynamic_cast<STEPentity *>(
	brlcad::step::Entity(defined, "target")) : NULL;
    SDAI_Application_instance *placement = target ?
	brlcad::step::Entity(target, "placement") : NULL;
    const double x_scale = target ? wrapper.getRealAttribute(target,
	"x_scale") : 0.0;
    const double y_scale = target ? wrapper.getRealAttribute(target,
	"y_scale") : 0.0;
    const std::string name = defined_item_name(wrapper, definition);
    Affine2D frame;
    if (name.empty() || !std::isfinite(x_scale) || x_scale <= 0.0 ||
		!std::isfinite(y_scale) || y_scale <= 0.0 ||
		!placement_frame_2d(wrapper, placement, length, plane_origin,
		    plane_x, plane_y, frame)) {
	reason = "defined annotation symbol is malformed";
	return false;
    }
    brlcad::step::AnnotationText text;
    text.reference_vertex = vertices.size();
    text.label = symbol_label(name);
    text.size = 3.5;
    text.font = "osifont";
    text.role = RT_ANNOT_ROLE_SYMBOL;
    text.symbol = name;
    text.x_scale = frame.matrix[0] * x_scale;
    text.xy_scale = frame.matrix[1] * y_scale;
    text.yx_scale = frame.matrix[2] * x_scale;
    text.y_scale = frame.matrix[3] * y_scale;
    vertices.push_back({{frame.origin[0], frame.origin[1]}});
    texts.push_back(text);
    return true;
}


bool
append_mapped_element(STEPWrapper &wrapper,
    SDAI_Application_instance *instance, double length,
    const point_t plane_origin, const vect_t plane_x, const vect_t plane_y,
    std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines,
    std::vector<brlcad::step::AnnotationLineStyle> &line_styles,
    std::vector<brlcad::step::AnnotationText> &texts,
    std::vector<brlcad::step::AnnotationFill> &fills,
    std::set<uint64_t> &active, bool &used_default_size, std::string &reason)
{
    if (!instance || instance->STEPfile_id <= 0) {
	reason = "mapped annotation has a missing presentation element";
	return false;
    }
    const uint64_t occurrence_id =
	static_cast<uint64_t>(instance->STEPfile_id);
    if (!active.insert(occurrence_id).second) {
	reason = "cyclic mapped annotation occurrence graph";
	return false;
    }
    STEPentity *occurrence = dynamic_cast<STEPentity *>(instance);
    const size_t line_start = lines.size();
    const size_t text_start = texts.size();
    const size_t fill_start = fills.size();
    bool valid = false;

    if (wrapper.IsSchemaEntity(instance, "DRAUGHTING_CALLOUT")) {
	valid = true;
	const std::vector<SDAI_Application_instance *> contents =
	    brlcad::step::Entities(occurrence, "contents");
	if (contents.empty()) {
	    reason = "mapped draughting callout is empty";
	    valid = false;
	}
	for (SDAI_Application_instance *content : contents)
	    if (valid && !append_mapped_element(wrapper, content, length,
		    plane_origin, plane_x, plane_y, vertices, lines,
		    line_styles, texts, fills, active, used_default_size,
		    reason))
		valid = false;
	active.erase(occurrence_id);
	return valid;
    }

    STEPentity *item = occurrence ? dynamic_cast<STEPentity *>(
	brlcad::step::Entity(occurrence, "item")) : NULL;
    const uint64_t item_id = item && item->STEPfile_id > 0 ?
	static_cast<uint64_t>(item->STEPfile_id) : 0;

    if (wrapper.IsSchemaEntity(instance, "ANNOTATION_CURVE_OCCURRENCE")) {
	valid = item &&
	    ((wrapper.IsSchemaEntity(item, "GEOMETRIC_CURVE_SET") &&
	    append_geometric_curve_set(wrapper, item_id, length, plane_origin,
		plane_x, plane_y, vertices, lines, reason)) ||
	    (wrapper.IsSchemaEntity(item, "CURVE") &&
	    append_annotation_curve_strokes(wrapper, item_id, length,
		plane_origin, plane_x, plane_y, vertices, lines, reason)));
	uint32_t role = RT_ANNOT_ROLE_GEOMETRY;
	if (wrapper.IsSchemaEntity(instance, "LEADER_CURVE"))
	    role = RT_ANNOT_ROLE_LEADER;
	else if (wrapper.IsSchemaEntity(instance, "PROJECTION_CURVE"))
	    role = RT_ANNOT_ROLE_EXTENSION;
	else if (wrapper.IsSchemaEntity(instance, "DIMENSION_CURVE"))
	    role = RT_ANNOT_ROLE_DIMENSION;
	if (valid) append_line_styles(lines, line_start, role, line_styles,
	    std::string());
	if (valid) apply_curve_presentation_style(wrapper, occurrence_id,
	    length, line_start, line_styles);
    } else if (wrapper.IsSchemaEntity(instance,
	    "ANNOTATION_FILL_AREA_OCCURRENCE")) {
	valid = item && append_fill_area_entity(wrapper, item, length,
	    plane_origin, plane_x, plane_y, vertices, fills, reason);
    } else if (wrapper.IsSchemaEntity(instance,
	    "ANNOTATION_TEXT_OCCURRENCE")) {
	valid = item_id && append_text_item(wrapper, item_id, length,
	    plane_origin, plane_x, plane_y, vertices, texts, active,
	    used_default_size, reason, &lines, &fills);
	if (valid) append_line_styles(lines, line_start,
	    RT_ANNOT_ROLE_TEXT_DECORATION, line_styles,
	    "text associated curve");
    } else if (wrapper.IsSchemaEntity(instance,
	    "ANNOTATION_SYMBOL_OCCURRENCE")) {
	if (item && wrapper.IsSchemaEntity(item, "DEFINED_SYMBOL"))
	    valid = append_defined_symbol(wrapper, item, length, plane_origin,
		plane_x, plane_y, vertices, texts, reason);
	else if (item && wrapper.IsSchemaEntity(item, "ANNOTATION_SYMBOL") &&
		active.insert(item_id).second) {
	    valid = append_mapped_presentation(wrapper, item, length,
		plane_origin, plane_x, plane_y, vertices, lines, texts, fills,
		active, used_default_size, true, entity_name(wrapper, item_id),
		reason, &line_styles);
	    active.erase(item_id);
	} else if (item) {
	    reason = "cyclic or unsupported mapped annotation symbol";
	}
	if (valid && wrapper.IsSchemaEntity(instance, "TERMINATOR_SYMBOL")) {
	    for (size_t i = line_start; i < line_styles.size(); ++i) {
		line_styles[i].role = RT_ANNOT_ROLE_ARROWHEAD;
		line_styles[i].symbol = "terminator";
	    }
	    for (size_t i = text_start; i < texts.size(); ++i)
		texts[i].role = RT_ANNOT_ROLE_ARROWHEAD;
	    for (size_t i = fill_start; i < fills.size(); ++i)
		fills[i].role = RT_ANNOT_ROLE_ARROWHEAD;
	}
    } else if (wrapper.IsSchemaEntity(instance,
	    "ANNOTATION_PLACEHOLDER_OCCURRENCE")) {
	valid = append_annotation_placeholder(wrapper, occurrence_id, length,
	    plane_origin, plane_x, plane_y, vertices, lines, texts,
	    used_default_size, reason);
	if (valid) append_line_styles(lines, line_start,
	    RT_ANNOT_ROLE_PLACEHOLDER, line_styles,
	    "annotation placeholder");
	if (valid) apply_curve_presentation_style(wrapper, occurrence_id,
	    length, line_start, line_styles);
    } else if (wrapper.IsSchemaEntity(instance,
	    "ANNOTATION_POINT_OCCURRENCE")) {
	valid = append_annotation_point(wrapper, occurrence_id, length,
	    plane_origin, plane_x, plane_y,
	    point_marker_style(wrapper, occurrence_id, length), vertices, lines,
	    reason);
	if (valid) append_line_styles(lines, line_start, RT_ANNOT_ROLE_SYMBOL,
	    line_styles, "point marker");
    } else if (wrapper.IsSchemaEntity(instance,
	    "TESSELLATED_ANNOTATION_OCCURRENCE")) {
	point_t identity_origin = VINIT_ZERO;
	vect_t identity_x = {1.0, 0.0, 0.0};
	vect_t identity_y = {0.0, 1.0, 0.0};
	vect_t identity_z = {0.0, 0.0, 1.0};
	std::set<uint64_t> visited;
	bool have_surface = false;
	bool have_points = false;
	valid = item_id && append_tessellated_item(wrapper, item_id, length,
	    identity_origin, identity_x, identity_y, identity_z, plane_origin,
	    plane_x, plane_y, point_marker_style(wrapper, occurrence_id, length),
	    vertices, lines, visited, have_surface, have_points, reason);
	if (valid) append_line_styles(lines, line_start, RT_ANNOT_ROLE_GEOMETRY,
	    line_styles, std::string());
    } else {
	reason = "unsupported mapped annotation presentation element " +
	    wrapper.LazyTypeName(occurrence_id);
    }

    if (valid) {
	std::array<unsigned char, 4> colour;
	const bool have_colour = presentation_colour(wrapper, occurrence_id,
	    colour);
	apply_presentation_colour(colour, have_colour, line_start, text_start,
	    fill_start, line_styles, texts, fills);
    } else if (reason.empty()) {
	reason = "mapped annotation presentation element is malformed";
    }
    active.erase(occurrence_id);
    return valid;
}


bool
append_annotation_symbol(STEPWrapper &wrapper, uint64_t occurrence_id,
    double length, const point_t plane_origin, const vect_t plane_x,
    const vect_t plane_y, std::vector<std::array<double, 2> > &vertices,
    std::vector<std::pair<size_t, size_t> > &lines,
    std::vector<brlcad::step::AnnotationLineStyle> &line_styles,
    std::vector<brlcad::step::AnnotationText> &texts,
    std::vector<brlcad::step::AnnotationFill> &fills,
    bool &used_default_size, std::string &reason)
{
    const size_t text_start = texts.size();
    const size_t fill_start = fills.size();
    STEPentity *occurrence = occurrence_id <= static_cast<uint64_t>(INT_MAX) ?
	dynamic_cast<STEPentity *>(wrapper.getEntity(
	    static_cast<int>(occurrence_id))) : NULL;
    STEPentity *item = occurrence ? dynamic_cast<STEPentity *>(
	brlcad::step::Entity(occurrence, "item")) : NULL;
    if (!item || item->STEPfile_id <= 0) {
	reason = "annotation symbol occurrence has no symbol item";
	return false;
    }
    bool converted = false;
    if (wrapper.IsSchemaEntity(item, "DEFINED_SYMBOL"))
	converted = append_defined_symbol(wrapper, item, length, plane_origin,
	    plane_x, plane_y, vertices, texts, reason);
    if (wrapper.IsSchemaEntity(item, "ANNOTATION_SYMBOL")) {
	std::set<uint64_t> active;
	active.insert(static_cast<uint64_t>(item->STEPfile_id));
	converted = append_mapped_presentation(wrapper, item, length, plane_origin,
	    plane_x, plane_y, vertices, lines, texts, fills, active,
	    used_default_size, true, entity_name(wrapper,
		static_cast<uint64_t>(item->STEPfile_id)), reason,
	    &line_styles);
    }
    if (!converted) {
	if (reason.empty())
	    reason = "unsupported annotation symbol occurrence item " +
		wrapper.LazyTypeName(static_cast<uint64_t>(item->STEPfile_id));
	return false;
    }
    if (wrapper.LazyIsSchemaEntity(occurrence_id, "TERMINATOR_SYMBOL")) {
	for (size_t i = text_start; i < texts.size(); ++i)
	    texts[i].role = RT_ANNOT_ROLE_ARROWHEAD;
	for (size_t i = fill_start; i < fills.size(); ++i)
	    fills[i].role = RT_ANNOT_ROLE_ARROWHEAD;
    }
    return true;
}


std::set<uint64_t>
annotation_occurrences(STEPWrapper &wrapper, uint64_t annotation_plane)
{
    std::set<uint64_t> result;
    std::set<uint64_t> visited;
    std::deque<std::pair<uint64_t, unsigned int> > pending;
    pending.push_back(std::make_pair(annotation_plane, 0));
    while (!pending.empty()) {
	const uint64_t id = pending.front().first;
	const unsigned int depth = pending.front().second;
	pending.pop_front();
	if (!visited.insert(id).second || depth > 4) continue;
	if (wrapper.LazyIsSchemaEntity(id,
		"TESSELLATED_ANNOTATION_OCCURRENCE") ||
		wrapper.LazyIsSchemaEntity(id, "ANNOTATION_CURVE_OCCURRENCE") ||
		wrapper.LazyIsSchemaEntity(id,
		    "ANNOTATION_FILL_AREA_OCCURRENCE") ||
		wrapper.LazyIsSchemaEntity(id, "ANNOTATION_POINT_OCCURRENCE") ||
		wrapper.LazyIsSchemaEntity(id, "ANNOTATION_SYMBOL_OCCURRENCE") ||
		wrapper.LazyIsSchemaEntity(id, "ANNOTATION_TEXT_OCCURRENCE") ||
		wrapper.LazyIsSchemaEntity(id,
		    "ANNOTATION_PLACEHOLDER_OCCURRENCE")) {
	    result.insert(id);
	    continue;
	}
	for (uint64_t reference : wrapper.LazyForwardReferences(id)) {
	    const std::string type = wrapper.LazyTypeName(reference);
	    if (wrapper.LazyIsSchemaEntity(reference, "ANNOTATION_OCCURRENCE") ||
		    wrapper.LazyIsSchemaEntity(reference, "DRAUGHTING_CALLOUT") ||
		    contains(type, "CALLOUT") || contains(type, "ANNOTATION") ||
		    contains(type, "PLACEHOLDER"))
		pending.push_back(std::make_pair(reference, depth + 1));
	}
    }
    return result;
}


void
append_line_styles(const std::vector<std::pair<size_t, size_t> > &lines,
    size_t start, uint32_t role,
    std::vector<brlcad::step::AnnotationLineStyle> &styles,
    const std::string &symbol = std::string())
{
    while (styles.size() < start) styles.push_back(
	brlcad::step::AnnotationLineStyle());
    while (styles.size() < lines.size()) {
	brlcad::step::AnnotationLineStyle style;
	style.role = role;
	style.symbol = symbol;
	styles.push_back(style);
    }
}


void
apply_presentation_colour(const std::array<unsigned char, 4> &colour,
    bool have_colour, size_t line_start, size_t text_start, size_t fill_start,
    std::vector<brlcad::step::AnnotationLineStyle> &line_styles,
    std::vector<brlcad::step::AnnotationText> &texts,
    std::vector<brlcad::step::AnnotationFill> &fills)
{
    if (!have_colour) return;
    for (size_t i = line_start; i < line_styles.size(); ++i) {
	line_styles[i].has_color = true;
	line_styles[i].color = colour;
    }
    for (size_t i = text_start; i < texts.size(); ++i) {
	texts[i].has_color = true;
	texts[i].color = colour;
    }
    for (size_t i = fill_start; i < fills.size(); ++i) {
	/* A blanking box means "erase what is behind the text".  Its color is
	 * the viewer background, not the foreground text style. */
	if (fills[i].role == RT_ANNOT_ROLE_MASK) continue;
	fills[i].has_color = true;
	fills[i].color = colour;
    }
}


void
create_native_annotations(STEPWrapper &wrapper, BRLCADWrapper &database)
{
    const std::vector<uint64_t> planes =
	wrapper.LazyInstancesBySchemaType("ANNOTATION_PLANE");
    for (uint64_t id : planes) {
	auto retained = wrapper.Document().pmi_records.find(
	    static_cast<int64_t>(id));
	if (retained == wrapper.Document().pmi_records.end()) continue;
	PMIRecord &record = retained->second;
	const auto product = wrapper.Document().products.find(record.product_id);
	if (product == wrapper.Document().products.end() ||
		product->second.output_name.empty()) {
	    record.native_status = "annotation plane has no retained product owner";
	    continue;
	}
	const double length = product_length_factor(wrapper, record.product_id);
	point_t plane_origin;
	vect_t plane_x, plane_y;
	STEPentity *plane_occurrence = id <= static_cast<uint64_t>(INT_MAX) ?
	    dynamic_cast<STEPentity *>(wrapper.getEntity(static_cast<int>(id))) :
	    NULL;
	STEPentity *plane_item = plane_occurrence ? dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(plane_occurrence, "item")) : NULL;
	uint64_t plane_id = plane_item && plane_item->STEPfile_id > 0 ?
	    static_cast<uint64_t>(plane_item->STEPfile_id) : 0;
	bool plane_box = false;
	double plane_box_x = 0.0;
	double plane_box_y = 0.0;
	if (plane_item && wrapper.IsSchemaEntity(plane_item, "PLANE")) {
	    Plane *plane = dynamic_cast<Plane *>(Factory::CreateObject(&wrapper,
		plane_item));
	    if (!plane) {
		record.native_status =
		    "annotation plane geometry could not be loaded";
		continue;
	    }
	    VSCALE(plane_origin, plane->GetOrigin(), length);
	    VMOVE(plane_x, plane->GetXAxis());
	    VMOVE(plane_y, plane->GetYAxis());
	} else if (plane_item && wrapper.IsSchemaEntity(plane_item,
		"PLANAR_BOX")) {
	    vect_t plane_z;
	    bool located = false;
	    plane_box_x = wrapper.getRealAttribute(plane_item, "size_in_x") *
		length;
	    plane_box_y = wrapper.getRealAttribute(plane_item, "size_in_y") *
		length;
	    if (!std::isfinite(plane_box_x) || plane_box_x <= 0.0 ||
		    !std::isfinite(plane_box_y) || plane_box_y <= 0.0 ||
		    !placement_axes(wrapper, plane_id, length, plane_origin,
			plane_x, plane_y, plane_z, &located) || !located) {
		record.native_status = "annotation planar box is malformed";
		continue;
	    }
	    plane_box = true;
	} else {
	    record.native_status =
		"annotation plane has no supported plane or planar box";
	    continue;
	}

	std::vector<std::array<double, 2> > vertices;
	std::vector<std::pair<size_t, size_t> > lines;
	std::vector<brlcad::step::AnnotationLineStyle> line_styles;
	std::vector<brlcad::step::AnnotationText> texts;
	std::vector<brlcad::step::AnnotationFill> fills;
	if (plane_box) {
	    const size_t start = vertices.size();
	    vertices.push_back({{0.0, 0.0}});
	    vertices.push_back({{plane_box_x, 0.0}});
	    vertices.push_back({{plane_box_x, plane_box_y}});
	    vertices.push_back({{0.0, plane_box_y}});
	    for (size_t corner = 0; corner < 4; ++corner)
		lines.push_back(std::make_pair(start + corner,
		    start + ((corner + 1) % 4)));
	    append_line_styles(lines, 0, RT_ANNOT_ROLE_GEOMETRY,
		line_styles, "annotation plane boundary");
	    apply_curve_presentation_style(wrapper, id, length, 0,
		line_styles);
	    std::array<unsigned char, 4> plane_colour;
	    const bool have_plane_colour = presentation_colour(wrapper, id,
		plane_colour);
	    apply_presentation_colour(plane_colour, have_plane_colour, 0, 0, 0,
		line_styles, texts, fills);
	}
	std::set<uint64_t> occurrence_ids = annotation_occurrences(wrapper, id);
	std::string reason;
	bool valid = true;
	bool have_surface = false;
	bool have_points = false;
	bool have_analytic = false;
	bool have_text = false;
	bool have_fill = false;
	bool have_symbol = false;
	bool used_default_text_size = false;
	for (uint64_t occurrence_id : occurrence_ids) {
	    const size_t line_start = lines.size();
	    const size_t text_start = texts.size();
	    const size_t fill_start = fills.size();
	    std::array<unsigned char, 4> colour;
	    const bool have_colour = presentation_colour(wrapper, occurrence_id,
		colour);
	    if (wrapper.LazyIsSchemaEntity(occurrence_id,
		    "ANNOTATION_SYMBOL_OCCURRENCE")) {
		if (!append_annotation_symbol(wrapper, occurrence_id, length,
			plane_origin, plane_x, plane_y, vertices, lines,
			line_styles, texts, fills, used_default_text_size,
			reason)) {
		    valid = false;
		    break;
		}
		const bool terminator = wrapper.LazyIsSchemaEntity(occurrence_id,
		    "TERMINATOR_SYMBOL");
		append_line_styles(lines, line_start, terminator ?
		    RT_ANNOT_ROLE_ARROWHEAD : RT_ANNOT_ROLE_SYMBOL,
		    line_styles, terminator ? "terminator" : "symbol");
		apply_presentation_colour(colour, have_colour, line_start,
		    text_start, fill_start, line_styles, texts, fills);
		have_symbol = true;
		continue;
	    }
	    if (wrapper.LazyIsSchemaEntity(occurrence_id,
		    "ANNOTATION_FILL_AREA_OCCURRENCE")) {
		if (!append_fill_area(wrapper, occurrence_id, length,
			plane_origin, plane_x, plane_y, vertices, fills, reason)) {
		    valid = false;
		    break;
		}
		apply_presentation_colour(colour, have_colour, line_start,
		    text_start, fill_start, line_styles, texts, fills);
		have_fill = true;
		continue;
	    }
	    if (wrapper.LazyIsSchemaEntity(occurrence_id,
		    "ANNOTATION_CURVE_OCCURRENCE")) {
		STEPentity *occurrence = occurrence_id <=
		    static_cast<uint64_t>(INT_MAX) ? dynamic_cast<STEPentity *>(
			wrapper.getEntity(static_cast<int>(occurrence_id))) : NULL;
		SDAI_Application_instance *item = occurrence ?
		    brlcad::step::Entity(occurrence, "item") : NULL;
		const uint64_t item_id = item && item->STEPfile_id > 0 ?
		    static_cast<uint64_t>(item->STEPfile_id) : 0;
		const bool converted = item &&
		    ((wrapper.IsSchemaEntity(item, "GEOMETRIC_CURVE_SET") &&
		    append_geometric_curve_set(wrapper, item_id, length,
			plane_origin, plane_x, plane_y, vertices, lines, reason)) ||
		    (wrapper.IsSchemaEntity(item, "CURVE") &&
		    append_annotation_curve_strokes(wrapper, item_id, length,
			plane_origin, plane_x, plane_y, vertices, lines, reason)));
		if (!converted) {
		    if (reason.empty())
			reason = "annotation curve occurrence has no supported curve or curve set";
		    valid = false;
		    break;
		}
		uint32_t role = RT_ANNOT_ROLE_GEOMETRY;
		if (wrapper.LazyIsSchemaEntity(occurrence_id, "LEADER_CURVE"))
		    role = RT_ANNOT_ROLE_LEADER;
		else if (wrapper.LazyIsSchemaEntity(occurrence_id,
			"PROJECTION_CURVE"))
		    role = RT_ANNOT_ROLE_EXTENSION;
		else if (wrapper.LazyIsSchemaEntity(occurrence_id,
			"DIMENSION_CURVE"))
		    role = RT_ANNOT_ROLE_DIMENSION;
		append_line_styles(lines, line_start, role, line_styles);
		apply_curve_presentation_style(wrapper, occurrence_id, length,
		    line_start, line_styles);
		apply_presentation_colour(colour, have_colour, line_start,
		    text_start, fill_start, line_styles, texts, fills);
		have_analytic = true;
		continue;
	    }
	    if (wrapper.LazyIsSchemaEntity(occurrence_id,
		    "ANNOTATION_POINT_OCCURRENCE")) {
		const MarkerStyle marker = point_marker_style(wrapper,
		    occurrence_id, length);
		if (!append_annotation_point(wrapper, occurrence_id, length,
			plane_origin, plane_x, plane_y, marker, vertices, lines,
			reason)) {
		    valid = false;
		    break;
		}
		append_line_styles(lines, line_start, RT_ANNOT_ROLE_SYMBOL,
		    line_styles, "point marker");
		apply_presentation_colour(colour, have_colour, line_start,
		    text_start, fill_start, line_styles, texts, fills);
		have_points = true;
		continue;
	    }
	    if (wrapper.LazyIsSchemaEntity(occurrence_id,
		    "ANNOTATION_TEXT_OCCURRENCE")) {
		STEPentity *occurrence = occurrence_id <=
		    static_cast<uint64_t>(INT_MAX) ? dynamic_cast<STEPentity *>(
			wrapper.getEntity(static_cast<int>(occurrence_id))) : NULL;
		SDAI_Application_instance *item = occurrence ?
		    brlcad::step::Entity(occurrence, "item") : NULL;
		std::set<uint64_t> active;
		if (!item || item->STEPfile_id <= 0 ||
			!append_text_item(wrapper,
			    static_cast<uint64_t>(item->STEPfile_id), length,
			    plane_origin, plane_x, plane_y, vertices, texts, active,
			    used_default_text_size, reason, &lines, &fills)) {
		    if (reason.empty())
			reason = "annotation text occurrence has no supported text item";
		    valid = false;
		    break;
		}
		append_line_styles(lines, line_start,
		    RT_ANNOT_ROLE_TEXT_DECORATION, line_styles,
		    "text associated curve");
		apply_presentation_colour(colour, have_colour, line_start,
		    text_start, fill_start, line_styles, texts, fills);
		have_text = true;
		continue;
	    }
	    if (wrapper.LazyIsSchemaEntity(occurrence_id,
		    "ANNOTATION_PLACEHOLDER_OCCURRENCE")) {
		if (!append_annotation_placeholder(wrapper, occurrence_id, length,
			plane_origin, plane_x, plane_y, vertices, lines, texts,
			used_default_text_size, reason)) {
		    valid = false;
		    break;
		}
		if (lines.size() == line_start && texts.size() == text_start)
		    continue;
		append_line_styles(lines, line_start, RT_ANNOT_ROLE_PLACEHOLDER,
		    line_styles, "annotation placeholder");
		apply_curve_presentation_style(wrapper, occurrence_id, length,
		    line_start, line_styles);
		apply_presentation_colour(colour, have_colour, line_start,
		    text_start, fill_start, line_styles, texts, fills);
		if (texts.size() > text_start) have_text = true;
		have_analytic = true;
		continue;
	    }
	    uint64_t tessellated_set_id = 0;
	    for (uint64_t reference : wrapper.LazyForwardReferences(occurrence_id))
		if (has_exact_type(wrapper, reference,
			"TESSELLATED_GEOMETRIC_SET")) {
		    tessellated_set_id = reference;
		    break;
		}
	    if (!tessellated_set_id)
		continue;
	    point_t identity_origin = VINIT_ZERO;
	    vect_t identity_x = {1.0, 0.0, 0.0};
	    vect_t identity_y = {0.0, 1.0, 0.0};
	    vect_t identity_z = {0.0, 0.0, 1.0};
	    const MarkerStyle marker = point_marker_style(wrapper,
		occurrence_id, length);
	    std::set<uint64_t> visited;
	    if (!append_tessellated_item(wrapper, tessellated_set_id, length,
		    identity_origin, identity_x, identity_y, identity_z,
		    plane_origin, plane_x, plane_y, marker, vertices, lines,
		    visited, have_surface, have_points, reason))
		valid = false;
	    if (!valid) break;
	    append_line_styles(lines, line_start, RT_ANNOT_ROLE_GEOMETRY,
		line_styles);
	    apply_presentation_colour(colour, have_colour, line_start,
		text_start, fill_start, line_styles, texts, fills);
	}
	if (!valid || vertices.empty() ||
		(lines.empty() && texts.empty() && fills.empty())) {
	    record.native_status = reason.empty() ?
		"annotation has no supported graphical content" : reason;
	    if (!valid) {
		++wrapper.Statistics().pmi_invalid_records;
		wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		    static_cast<int64_t>(id), "ANNOTATION_PLANE",
		    "presentation", record.native_status);
	    }
	    continue;
	}

	const std::string original = entity_name(wrapper, id);
	const std::string name = database.StableBRLCADName(
	    product->second.output_name + "_annotation_" +
	    (original.empty() ? "pmi" : original), static_cast<int64_t>(id));
	mat_t identity;
	MAT_IDN(identity);
	if (!database.WriteAnnotation(name, plane_origin, plane_x, plane_y,
		vertices, lines, line_styles, texts, fills,
		static_cast<int64_t>(id), original) ||
		!database.AddMember(product->second.output_name, name, identity)) {
	    record.native_status = "resolved annotation could not be written";
	    wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		static_cast<int64_t>(id), "ANNOTATION_PLANE", "database",
		"resolved AP242 annotation could not be written");
	    continue;
	}
	record.native_object = name;
	record.native_kind = "presentation_annotation";
	record.native_status = have_symbol ?
	    "STEP symbols resolved as native annotation glyphs or mapped geometry" :
	    (have_fill ?
	    "STEP fill areas resolved as native filled annotation loops" :
	    (have_text ? (used_default_text_size ?
	    "STEP text resolved as native annotation text with default 3.5 mm height" :
	    "STEP text and extent resolved as native annotation text") :
	    (have_surface ?
	    "graphical curves and tessellated surface boundaries resolved as annotation strokes" :
	    (have_analytic ? "geometric curves resolved as annotation strokes" :
	    (have_points ? "STEP points resolved as annotation marker strokes" :
	    "tessellated curves resolved as annotation strokes")))));
	++wrapper.Statistics().pmi_native_annotations;
	if (!database.dry_run) {
	    database.SetAttribute(name, "step:pmi:category", "presentation");
	    database.SetAttribute(name, "step:pmi:product_id",
		std::to_string(record.product_id));
	    database.SetAttribute(name, "step:pmi:annotation_plane_source_id",
		std::to_string(plane_id));
	}
    }
    wrapper.ClearEntityCache();
}


void
retain_global_attributes(STEPWrapper &wrapper)
{
    for (const auto &entry : wrapper.Document().pmi_records) {
	const PMIRecord &record = entry.second;
	const std::string prefix = "STEP::AP242::PMI::#" +
	    std::to_string(record.entity_id) + "::";
	wrapper.Document().global_attributes[prefix + "TYPE"] = record.type;
	if (!record.component_types.empty()) {
	    std::ostringstream components;
	    for (size_t i = 0; i < record.component_types.size(); ++i) {
		if (i) components << ' ';
		components << record.component_types[i];
	    }
	    wrapper.Document().global_attributes[prefix + "COMPONENT_TYPES"] =
		components.str();
	}
	wrapper.Document().global_attributes[prefix + "CATEGORY"] = record.category;
	wrapper.Document().global_attributes[prefix + "VALUE"] = record.value;
	std::ostringstream references;
	for (size_t i = 0; i < record.references.size(); ++i) {
	    if (i) references << ' ';
	    references << record.references[i];
	}
	wrapper.Document().global_attributes[prefix + "REFERENCES"] =
	    references.str();
	if (record.product_id > 0)
	    wrapper.Document().global_attributes[prefix + "PRODUCT_ID"] =
		std::to_string(record.product_id);
	if (!record.native_object.empty())
	    wrapper.Document().global_attributes[prefix + "NATIVE_OBJECT"] =
		record.native_object;
	if (!record.native_kind.empty())
	    wrapper.Document().global_attributes[prefix + "NATIVE_KIND"] =
		record.native_kind;
	if (!record.native_status.empty())
	    wrapper.Document().global_attributes[prefix + "NATIVE_STATUS"] =
		record.native_status;
    }
}

} // namespace ap242_pmi


void
ImportAP242PMI(STEPWrapper &wrapper, BRLCADWrapper &database)
{
    if (!wrapper.HasLazyIndex()) {
	wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning, 0,
	    "AP242_PMI", std::string(),
	    "AP242 PMI retention requires the lazy Part 21 index");
	return;
    }
    const std::set<uint64_t> ids = ap242_pmi::pmi_graph_ids(wrapper);
    wrapper.SetProgress("retaining AP242 PMI association graph", 0, ids.size());
    ap242_pmi::retain_graph(wrapper, ids);
    ap242_pmi::create_native_datums(wrapper, database);
    ap242_pmi::create_native_annotations(wrapper, database);
    ap242_pmi::retain_global_attributes(wrapper);

    for (const auto &entry : wrapper.Document().pmi_records) {
	const std::string &category = entry.second.category;
	if (category == "semantic") ++wrapper.Statistics().pmi_semantic_records;
	else if (category == "presentation")
	    ++wrapper.Statistics().pmi_presentation_records;
	else if (category == "association")
	    ++wrapper.Statistics().pmi_association_records;
	else ++wrapper.Statistics().pmi_dependency_records;
	wrapper.Document().unsupported_counts.erase(entry.second.type);
	for (const std::string &component : entry.second.component_types)
	    wrapper.Document().unsupported_counts.erase(component);
    }
    wrapper.SetProgress("AP242 PMI association graph retained", ids.size(),
	ids.size());
}
