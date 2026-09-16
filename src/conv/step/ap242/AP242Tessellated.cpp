/*               A P 2 4 2 T E S S E L L A T E D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "AP242Tessellated.h"

#include "BRLCADWrapper.h"
#include "GlobalUnitAssignedContext.h"
#include "STEPGeneratedAPI.h"
#include "STEPTessellatedMesh.h"
#include "STEPWrapper.h"
#include "ap_schema.h"

#include "vmath.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

double
representation_length_factor(STEPWrapper &wrapper, uint64_t representation_id)
{
    const double default_length = 1000.0;
	const std::vector<uint64_t> references =
	    wrapper.LazyForwardReferences(representation_id);
	uint64_t context_id = 0;
	for (std::vector<uint64_t>::const_iterator reference = references.begin();
	     reference != references.end(); ++reference) {
	    if (wrapper.LazyIsSchemaEntity(*reference, "REPRESENTATION_CONTEXT")) {
		context_id = *reference;
		break;
	    }
	}
	if (!context_id || context_id > static_cast<uint64_t>(INT_MAX))
	    return default_length;
	SDAI_Application_instance *context = wrapper.getEntity(static_cast<int>(context_id));
	SDAI_Application_instance *unit_component = context ?
	    wrapper.getEntity(context, "Global_Unit_Assigned_Context") : NULL;
	if (!unit_component) return default_length;
	GlobalUnitAssignedContext units;
	if (!units.Load(&wrapper, unit_component)) return default_length;
	const double length = units.GetLengthConversionFactor();
	return std::isfinite(length) && length > 0.0 ? length : default_length;
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
generic_real_rows(GenericAggregate *aggregate, std::vector<std::vector<double> > &rows,
    size_t required_width)
{
    rows.clear();
    STEPnode *node = aggregate ? static_cast<STEPnode *>(aggregate->GetHead()) : NULL;
    while (node) {
	std::string text;
	std::vector<double> row;
	if (!parse_reals(node->asStr(text), row) || row.size() != required_width)
	    return false;
	rows.push_back(row);
	node = static_cast<STEPnode *>(node->NextNode());
    }
    return aggregate != NULL;
}

bool
generic_integer_rows(GenericAggregate *aggregate, std::vector<std::vector<int> > &rows,
    size_t minimum_width, size_t maximum_width = 0)
{
    rows.clear();
    STEPnode *node = aggregate ? static_cast<STEPnode *>(aggregate->GetHead()) : NULL;
    while (node) {
	std::string text;
	std::vector<int> row;
	if (!parse_integers(node->asStr(text), row) || row.size() < minimum_width ||
	    (maximum_width > 0 && row.size() > maximum_width))
	    return false;
	rows.push_back(row);
	node = static_cast<STEPnode *>(node->NextNode());
    }
    return aggregate != NULL;
}

std::vector<int>
integer_values(IntAggregate *aggregate)
{
    std::vector<int> values;
    IntNode *node = aggregate ? static_cast<IntNode *>(aggregate->GetHead()) : NULL;
    while (node) {
	if (node->value < 1 || node->value > std::numeric_limits<int>::max()) {
	    values.clear();
	    return values;
	}
	values.push_back(static_cast<int>(node->value));
	node = static_cast<IntNode *>(node->NextNode());
    }
    return values;
}

const brlcad::step::TessellatedCoordinateData *
coordinates(brlcad::step::TessellatedMeshBuilder &mesh,
    STEPWrapper &wrapper, STEPentity *entity)
{
    if (!entity || entity->STEPfile_id <= 0) {
	mesh.error = "missing coordinates_list";
	return NULL;
    }
    std::vector<std::vector<double> > rows;
    if (!generic_real_rows(dynamic_cast<GenericAggregate *>(
	    brlcad::step::Aggregate(entity, "position_coords")), rows, 3)) {
	mesh.error = "coordinates_list position_coords is malformed";
	return NULL;
    }
    return mesh.DefineCoordinates(entity->STEPfile_id, rows,
	static_cast<size_t>(wrapper.getIntegerAttribute(entity, "npoints")));
}

bool
append_triangles(brlcad::step::TessellatedMeshBuilder &mesh,
    STEPWrapper &wrapper, STEPentity *coordinate_entity,
    int pnmax, GenericAggregate *normal_aggregate, IntAggregate *pnindex_aggregate,
    GenericAggregate *triangle_aggregate)
{
    const brlcad::step::TessellatedCoordinateData *coordinate_data =
	coordinates(mesh, wrapper, coordinate_entity);
    if (!coordinate_data) return false;
    const std::vector<int> pnindex = integer_values(pnindex_aggregate);
    if (pnindex_aggregate && pnindex_aggregate->EntryCount() > 0 && pnindex.empty()) {
	mesh.error = "pnindex contains an invalid value";
	return false;
    }
    std::vector<std::vector<double> > normals;
    if (!generic_real_rows(normal_aggregate, normals, 3)) {
	mesh.error = "normal list is malformed";
	return false;
    }
    if (!mesh.ValidateIndexing(*coordinate_data, pnmax, pnindex, normals)) return false;

    std::vector<std::vector<int> > triangles;
    if (!generic_integer_rows(triangle_aggregate, triangles, 3, 3) || triangles.empty()) {
	mesh.error = "triangle list is empty or malformed";
	return false;
    }
    for (size_t triangle = 0; triangle < triangles.size(); ++triangle) {
	if (!mesh.AddTriangle(*coordinate_data, pnindex, pnmax, normals,
		triangles[triangle][0], triangles[triangle][1], triangles[triangle][2]))
	    return false;
    }
    return true;
}

bool
append_strips_and_fans(brlcad::step::TessellatedMeshBuilder &mesh,
    STEPWrapper &wrapper, STEPentity *coordinate_entity,
    int pnmax, GenericAggregate *normal_aggregate, IntAggregate *pnindex_aggregate,
    GenericAggregate *strip_aggregate, GenericAggregate *fan_aggregate)
{
    const brlcad::step::TessellatedCoordinateData *coordinate_data =
	coordinates(mesh, wrapper, coordinate_entity);
    if (!coordinate_data) return false;
    const std::vector<int> pnindex = integer_values(pnindex_aggregate);
    if (pnindex_aggregate && pnindex_aggregate->EntryCount() > 0 && pnindex.empty()) {
	mesh.error = "pnindex contains an invalid value";
	return false;
    }
    std::vector<std::vector<double> > normals;
    if (!generic_real_rows(normal_aggregate, normals, 3)) {
	mesh.error = "normal list is malformed";
	return false;
    }
    if (!mesh.ValidateIndexing(*coordinate_data, pnmax, pnindex, normals)) return false;

    std::vector<std::vector<int> > strips;
    std::vector<std::vector<int> > fans;
    if (!generic_integer_rows(strip_aggregate, strips, 3) ||
	!generic_integer_rows(fan_aggregate, fans, 3) ||
	(strips.empty() && fans.empty())) {
	mesh.error = "triangle strip and fan lists are empty or malformed";
	return false;
    }
    for (size_t strip = 0; strip < strips.size(); ++strip) {
	for (size_t i = 2; i < strips[strip].size(); ++i) {
	    const int first = strips[strip][i - 2];
	    const int second = strips[strip][i - 1];
	    if (!mesh.AddTriangle(*coordinate_data, pnindex, pnmax, normals,
		    (i % 2) == 0 ? first : second,
		    (i % 2) == 0 ? second : first, strips[strip][i]))
		return false;
	}
    }
    for (size_t fan = 0; fan < fans.size(); ++fan) {
	for (size_t i = 2; i < fans[fan].size(); ++i) {
	    if (!mesh.AddTriangle(*coordinate_data, pnindex, pnmax, normals,
		    fans[fan][0], fans[fan][i - 1], fans[fan][i]))
		return false;
	}
    }
    return true;
}

bool
append_face(brlcad::step::TessellatedMeshBuilder &mesh,
    STEPWrapper &wrapper, STEPentity *face)
{
    STEPentity *coordinate_entity = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(face, "coordinates"));
    const int pnmax = wrapper.getIntegerAttribute(face, "pnmax");
    GenericAggregate *normals = dynamic_cast<GenericAggregate *>(
	brlcad::step::Aggregate(face, "normals"));
    IntAggregate *pnindex = dynamic_cast<IntAggregate *>(
	brlcad::step::Aggregate(face, "pnindex"));
    if (wrapper.IsSchemaEntity(face, "TRIANGULATED_FACE"))
	return append_triangles(mesh, wrapper, coordinate_entity, pnmax, normals,
	    pnindex, dynamic_cast<GenericAggregate *>(
		brlcad::step::Aggregate(face, "triangles")));
    if (wrapper.IsSchemaEntity(face, "COMPLEX_TRIANGULATED_FACE"))
	return append_strips_and_fans(mesh, wrapper, coordinate_entity, pnmax, normals,
	    pnindex, dynamic_cast<GenericAggregate *>(
		brlcad::step::Aggregate(face, "triangle_strips")),
	    dynamic_cast<GenericAggregate *>(
		brlcad::step::Aggregate(face, "triangle_fans")));
    mesh.error = "unsupported tessellated_face subtype";
    return false;
}

bool
append_surface_set(brlcad::step::TessellatedMeshBuilder &mesh,
    STEPWrapper &wrapper, STEPentity *surface)
{
    STEPentity *coordinate_entity = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(surface, "coordinates"));
    const int pnmax = wrapper.getIntegerAttribute(surface, "pnmax");
    GenericAggregate *normals = dynamic_cast<GenericAggregate *>(
	brlcad::step::Aggregate(surface, "normals"));
    IntAggregate *pnindex = dynamic_cast<IntAggregate *>(
	brlcad::step::Aggregate(surface, "pnindex"));
    if (wrapper.IsSchemaEntity(surface, "TRIANGULATED_SURFACE_SET"))
	return append_triangles(mesh, wrapper, coordinate_entity, pnmax, normals,
	    pnindex, dynamic_cast<GenericAggregate *>(
		brlcad::step::Aggregate(surface, "triangles")));
    if (wrapper.IsSchemaEntity(surface, "COMPLEX_TRIANGULATED_SURFACE_SET"))
	return append_strips_and_fans(mesh, wrapper, coordinate_entity, pnmax, normals,
	    pnindex, dynamic_cast<GenericAggregate *>(
		brlcad::step::Aggregate(surface, "triangle_strips")),
	    dynamic_cast<GenericAggregate *>(
		brlcad::step::Aggregate(surface, "triangle_fans")));
    mesh.error = "unsupported tessellated_surface_set subtype";
    return false;
}

bool
append_structured_items(brlcad::step::TessellatedMeshBuilder &mesh,
    STEPWrapper &wrapper, STEPentity *item)
{
    bool found_face = false;
	const std::vector<SDAI_Application_instance *> items =
	    brlcad::step::Entities(item, "items");
    for (std::vector<SDAI_Application_instance *>::const_iterator i = items.begin();
	 i != items.end(); ++i) {
	STEPentity *face = dynamic_cast<STEPentity *>(*i);
	if (face && wrapper.IsSchemaEntity(face, "TESSELLATED_FACE")) {
	    found_face = true;
	    if (!append_face(mesh, wrapper, face)) return false;
	}
    }
    if (!found_face) mesh.error = "tessellated solid or shell contains no faces";
    return found_face;
}

bool
append_item(brlcad::step::TessellatedMeshBuilder &mesh,
    STEPWrapper &wrapper, STEPentity *item)
{
    if (wrapper.IsSchemaEntity(item, "TESSELLATED_SOLID"))
	return append_structured_items(mesh, wrapper, item);
    if (wrapper.IsSchemaEntity(item, "TESSELLATED_SHELL"))
	return append_structured_items(mesh, wrapper, item);
    if (wrapper.IsSchemaEntity(item, "TESSELLATED_FACE"))
	return append_face(mesh, wrapper, item);
    if (wrapper.IsSchemaEntity(item, "TESSELLATED_SURFACE_SET"))
	return append_surface_set(mesh, wrapper, item);
    mesh.error = "tessellated item type has no BOT mapping";
    return false;
}

const brlcad::step::Style *
tessellated_style(STEPWrapper &wrapper, int64_t item_id, int64_t representation_id)
{
    std::map<int64_t, brlcad::step::Style>::const_iterator style =
	wrapper.Document().styles.find(item_id);
    if (style == wrapper.Document().styles.end())
	style = wrapper.Document().styles.find(representation_id);
    return style == wrapper.Document().styles.end() ? NULL : &style->second;
}

} // namespace

void
ImportAP242Tessellated(STEPWrapper &wrapper, BRLCADWrapper &database)
{
    /* The schema-neutral lazy graph has already resolved the association's
     * product and item IDs.  Use that durable graph rather than rematerializing
     * AP242's represented_definition SELECT, whose generated layout has
     * changed between schema editions. */
    std::set<std::pair<int64_t, int64_t> > converted_items;
    for (std::map<int64_t, brlcad::step::RepresentationCoverage>::const_iterator coverage =
	    wrapper.Document().representation_coverage.begin();
	 coverage != wrapper.Document().representation_coverage.end(); ++coverage) {
	const int64_t product_id = coverage->second.product_id;
	std::map<int64_t, brlcad::step::Product>::iterator product =
	    wrapper.Document().products.find(product_id);
	if (product == wrapper.Document().products.end() || product->second.output_name.empty())
	    continue;
	const double length = representation_length_factor(wrapper,
	    static_cast<uint64_t>(coverage->first));
	for (std::vector<brlcad::step::RepresentationItemCoverage>::const_iterator item_coverage =
		coverage->second.items.begin();
	     item_coverage != coverage->second.items.end(); ++item_coverage) {
	    SDAI_Application_instance *item_entity = wrapper.getEntity(item_coverage->entity_id);
	    STEPentity *item = dynamic_cast<STEPentity *>(item_entity);
	    if (!item || item->STEPfile_id <= 0 ||
		!wrapper.ShouldConvertEntity(item->STEPfile_id)) continue;

	    const int64_t item_id = item->STEPfile_id;
	    const std::string type = wrapper.LazyTypeName(static_cast<uint64_t>(item_id));
	    const bool known_mesh_type =
		wrapper.IsSchemaEntity(item, "TESSELLATED_SOLID") ||
		wrapper.IsSchemaEntity(item, "TESSELLATED_SHELL") ||
		wrapper.IsSchemaEntity(item, "TRIANGULATED_FACE") ||
		wrapper.IsSchemaEntity(item, "COMPLEX_TRIANGULATED_FACE") ||
		wrapper.IsSchemaEntity(item, "TRIANGULATED_SURFACE_SET") ||
		wrapper.IsSchemaEntity(item, "COMPLEX_TRIANGULATED_SURFACE_SET");
	    if (wrapper.IsSchemaEntity(item, "COORDINATES_LIST")) {
		wrapper.RecordRepresentationItemCoverage(item_id,
		    brlcad::step::RepresentationCoverageStatus::IntentionallyNonGeometric,
		    "coordinates_list supplies tessellated vertices but is not standalone geometry");
		continue;
	    }
	    if (!known_mesh_type) continue;
	    if (!converted_items.insert(std::make_pair(product_id, item_id)).second)
		continue;

	    ++wrapper.Statistics().geometry_attempted;
	    const bool allow_winding_repair = !wrapper.ImportOptions().exact &&
		wrapper.ImportOptions().repair != brlcad::step::RepairMode::None;
	    const bool solid_intent = wrapper.IsSchemaEntity(item, "TESSELLATED_SOLID");
	    brlcad::step::TessellatedMeshBuilder mesh(length,
		allow_winding_repair,
		allow_winding_repair ? wrapper.Statistics().tolerance_mm : 0.0,
		allow_winding_repair && solid_intent);
	    const bool supported = append_item(mesh, wrapper, item);
	    if (!supported || mesh.vertices.size() < 9 || mesh.faces.size() < 3) {
		++wrapper.Statistics().geometry_skipped;
		const std::string reason = mesh.error.empty() ?
		    "unsupported or empty AP242 tessellated item" : mesh.error;
		wrapper.RecordSkippedItem(item_id, type, reason);
		wrapper.RecordRepresentationItemCoverage(item_id,
		    known_mesh_type ?
			brlcad::step::RepresentationCoverageStatus::Malformed :
			brlcad::step::RepresentationCoverageStatus::Unsupported,
		    reason);
		wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		    item_id, type, "tessellation", reason);
		continue;
	    }

	    const bool closed = mesh.Closed();
	    if (mesh.discarded_repeated_triangles > 0 && !closed) {
		++wrapper.Statistics().geometry_skipped;
		const std::string reason =
		    "discarding repeated-vertex triangles did not leave a closed mesh";
		wrapper.RecordSkippedItem(item_id, type, reason);
		wrapper.RecordRepresentationItemCoverage(item_id,
		    brlcad::step::RepresentationCoverageStatus::Malformed, reason);
		wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		    item_id, type, "items", reason);
		continue;
	    }
	    const int mode = solid_intent && closed ? RT_BOT_SOLID : RT_BOT_SURFACE;
	    const std::string output_name = database.StableBRLCADName(
		product->second.output_name + "_tessellated_item", item_id);
	    const brlcad::step::Style *style = tessellated_style(wrapper, item_id,
		coverage->first);
	    mat_t identity;
	    MAT_IDN(identity);
	    const std::string original_name = wrapper.getStringAttribute(item, "name");
	    const bool written = database.WriteBot(output_name,
		mesh.vertices.size() / 3, mesh.faces.size() / 3,
		mesh.vertices.data(), mesh.faces.data(), identity, item_id,
		original_name, style, mode, RT_BOT_CCW, "AP242_TESSELLATED") &&
		database.AddMember(product->second.output_name, output_name, identity);
	    if (!written) {
		++wrapper.Statistics().geometry_skipped;
		const std::string reason = "failed to write AP242 tessellated BOT";
		wrapper.RecordSkippedItem(item_id, type, reason);
		wrapper.RecordRepresentationItemCoverage(item_id,
		    brlcad::step::RepresentationCoverageStatus::Skipped, reason);
		wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		    item_id, type, "database", reason);
		continue;
	    }

	    ++wrapper.Statistics().geometry_written;
	    if (style) ++wrapper.Statistics().styles_applied;
	    brlcad::step::Representation &record = wrapper.Document().representations[item_id];
	    record.entity_id = item_id;
	    record.product_id = product_id;
	    record.type = type;
	    record.output_name = output_name;
	    wrapper.RecordRepresentationItemCoverage(item_id,
		brlcad::step::RepresentationCoverageStatus::Handled,
		mode == RT_BOT_SOLID ?
		    "AP242 tessellated solid converted to a closed BOT" :
		    "AP242 tessellation converted to a surface BOT");
	    if (!database.dry_run) {
		const std::string intent = solid_intent ? "solid" : "surface";
		const std::string closure = closed ? "true" : "false";
		database.SetAttribute(output_name, "step:tessellated_intent", intent);
		database.SetAttribute(output_name + ".s", "step:tessellated_intent", intent);
		database.SetAttribute(output_name, "step:tessellated_closed", closure);
		database.SetAttribute(output_name + ".s", "step:tessellated_closed", closure);
	    }
	    if (mesh.reversed_by_normals > 0) {
		wrapper.RecordRepair(item_id, type, "normals",
		    "reversed triangle winding to agree with supplied AP242 normals");
	    }
	    if (mesh.merged_by_tolerance > 0) {
		wrapper.RecordRepair(item_id, type, "coordinates",
		    "merged tessellated vertices within the asserted model tolerance");
	    }
	    if (mesh.discarded_repeated_triangles > 0) {
		wrapper.RecordRepair(item_id, type, "items",
		    "discarded repeated-vertex triangles after proving the remaining "
		    "tessellated solid is closed");
	    }
	    if (solid_intent && !closed) {
		wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
		    item_id, type, "items",
		    "tessellated_solid is not a closed consistently oriented mesh; "
		    "preserved as a surface BOT");
	    }
	}
    }
    wrapper.ClearEntityCache();
}
