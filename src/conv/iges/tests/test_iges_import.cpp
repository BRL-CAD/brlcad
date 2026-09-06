/*               T E S T _ I G E S _ I M P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include "../iges_brep_import.h"
#include "../iges_import.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/annot.h"
#include "rt/primitives/datum.h"
#include "wdb.h"

namespace {

struct Entity {
    int type = 0;
    int form = 0;
    std::string label;
    std::string parameters;
    int transform = 0;
};

std::string
field(int64_t value)
{
    std::ostringstream output;
    output << std::setw(8) << value;
    return output.str();
}

std::string
record(const std::string &data, char section, int sequence)
{
    std::ostringstream trailer;
    trailer << section << std::setw(7) << sequence;
    std::string result = data.substr(0, 72);
    result.resize(72, ' ');
    result += trailer.str();
    return result + '\n';
}

std::string
directory_first(const Entity &entity, int parameter_record, int sequence)
{
    std::string data;
    data += field(entity.type);
    data += field(parameter_record);
    data += field(0); /* structure */
    data += field(entity.type == 110 ? 2 : 0); /* dashed test line */
    data += field(0); /* level */
    data += field(0); /* view */
    data += field(entity.transform);
    data += field(0); /* label associativity */
    data += field(0); /* status */
    return record(data, 'D', sequence);
}

std::string
directory_second(const Entity &entity, int parameter_lines, int sequence)
{
    std::string data;
    data += field(entity.type);
    data += field(0); /* line weight */
    data += field(entity.type == 110 ? 2 : 0); /* red test line */
    data += field(parameter_lines);
    data += field(entity.form);
    data += field(0);
    data += field(0);
    std::ostringstream label;
    label << std::left << std::setw(8) << entity.label.substr(0, 8);
    data += label.str();
    data += field(0);
    return record(data, 'D', sequence);
}

std::string
parameter_record(const std::string &data, int owner, int sequence)
{
    std::string body = data.substr(0, 64);
    body.resize(64, ' ');
    body += field(owner);
    return record(body, 'P', sequence);
}

std::string
sample(const std::vector<Entity> &entities, const char *description,
    const char *global = "1H,,1H;;")
{
    std::vector<int> starts;
    std::vector<int> lines;
    int parameter_sequence = 1;
    for (const Entity &entity : entities) {
	starts.push_back(parameter_sequence);
	const int count = static_cast<int>((entity.parameters.size() + 63) / 64);
	lines.push_back(count);
	parameter_sequence += count;
    }

    std::string result;
    result += record(description, 'S', 1);
    result += record(global, 'G', 1);
    int directory_sequence = 1;
    for (size_t i = 0; i < entities.size(); ++i) {
	result += directory_first(entities[i], starts[i], directory_sequence++);
	result += directory_second(entities[i], lines[i], directory_sequence++);
    }
    parameter_sequence = 1;
    for (size_t i = 0; i < entities.size(); ++i) {
	const int owner = static_cast<int>(i * 2 + 1);
	for (int line = 0; line < lines[i]; ++line) {
	    result += parameter_record(entities[i].parameters.substr(
		static_cast<size_t>(line) * 64, 64), owner,
		parameter_sequence++);
	}
    }
    result += record("", 'T', 1);
    return result;
}

std::string
annotation_sample()
{
    const std::vector<Entity> entities = {
	{110, 0, "LINE", "110,0,0,0,10,0,0;"},
	{212, 0, "NOTE", "212,1,5,20,4,1,1.5707963267948966,0,0,0,2,3,0,5HHELLO;"},
	{214, 1, "LEADER", "214,2,2,1,0,0,0,5,0,10,5;"},
	{210, 0, "DIM", "210,1,3,5;"},
	{116, 0, "POINT", "116,1,2,3;"},
	{110, 0, "A(B", "110,0,0,0,0,1,0;"},
	{110, 0, "A[B", "110,0,0,0,0,2,0;"},
	{308, 0, "SUBDEF", "308,0,4Hwire,1,1;"},
	{408, 0, "SUBINST", "408,15,10,20,30,2;"}
    };
    return sample(entities, "semantic annotation test");
}


std::string
bounded_surface_sample()
{
    const std::vector<Entity> entities = {
	{128, 0, "SURFACE",
	    "128,1,1,1,1,0,0,1,0,0,0,0,1,1,0,0,1,1,1,1,1,1,"
	    "0,0,0,10,0,0,0,10,0,10,10,0,0,1,0,1;"},
	{110, 0, "BOTTOM", "110,0,0,0.05,10,0,0.05;"},
	{110, 0, "RIGHT", "110,10,0,0.05,10,10,0.05;"},
	{110, 0, "TOP", "110,10,10,0.05,0,10,0.05;"},
	{110, 0, "LEFT", "110,0,10,0.05,0,0,0.05;"},
	{141, 0, "BOUNDARY",
	    "141,0,0,1,4,3,1,0,5,1,0,7,1,0,9,1,0;"},
	{143, 0, "FACE", "143,0,1,1,11;"}
    };
    return sample(entities, "bounded surface tolerance test");
}


bool
expect(bool condition, const char *message)
{
    if (!condition)
	std::fprintf(stderr, "%s\n", message);
    return condition;
}

const struct rt_annot_internal *
read_annotation(struct db_i *dbip, const char *name, struct rt_db_internal &intern)
{
    struct directory *directory = db_lookup(dbip, name, LOOKUP_QUIET);
    if (directory == RT_DIR_NULL ||
	    rt_db_get_internal(&intern, directory, dbip, nullptr) < 0 ||
	    intern.idb_type != ID_ANNOT)
	return nullptr;
    return static_cast<const struct rt_annot_internal *>(intern.idb_ptr);
}

const struct rt_datum_internal *
read_datum(struct db_i *dbip, const char *name, struct rt_db_internal &intern)
{
    struct directory *directory = db_lookup(dbip, name, LOOKUP_QUIET);
    if (directory == RT_DIR_NULL ||
	    rt_db_get_internal(&intern, directory, dbip, nullptr) < 0 ||
	    intern.idb_type != ID_DATUM)
	return nullptr;
    return static_cast<const struct rt_datum_internal *>(intern.idb_ptr);
}

bool
test_semantic_annotations()
{
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_buffer(annotation_sample(), "annotation.iges");
    if (!expect(document.valid(), "semantic test IGES did not parse"))
	return false;

    char path[MAXPATHLEN] = {0};
    FILE *temporary = bu_temp_file(path, sizeof(path));
    if (!expect(temporary != nullptr, "could not create temporary database path"))
	return false;
    std::fclose(temporary);
    bu_file_delete(path);

    struct rt_wdb *wdbp = wdb_fopen(path);
    if (!expect(wdbp != RT_WDB_NULL, "could not create temporary database"))
	return false;
    brlcad::iges::ImportOptions options;
    options.root_name = "drawing";
    const brlcad::iges::ImportResult result =
	brlcad::iges::import_annotations(document, wdbp, options);
    bool passed = expect(result.success, "semantic annotation import failed") &&
	expect(result.statistics.annotations_written == 5,
	    "semantic annotation count is wrong") &&
	expect(result.statistics.datums_written == 1,
	    "semantic datum count is wrong") &&
	expect(result.statistics.semantic_groups_written == 3,
	    "semantic dimension and subfigure groups were not written") &&
	expect(db_lookup(wdbp->dbip, "drawing", LOOKUP_QUIET) != RT_DIR_NULL,
	    "semantic drawing root was not written") &&
	expect(db_lookup(wdbp->dbip, "DIM.annot_group", LOOKUP_QUIET) != RT_DIR_NULL,
	    "semantic dimension group name is missing") &&
	expect(db_lookup(wdbp->dbip, "wire.annot_def", LOOKUP_QUIET) != RT_DIR_NULL,
	    "annotation subfigure definition is missing") &&
	expect(db_lookup(wdbp->dbip, "wire_instance_D17.annot_instance",
		LOOKUP_QUIET) != RT_DIR_NULL,
	    "annotation subfigure instance is missing") &&
	expect(db_lookup(wdbp->dbip, "A_B.annot", LOOKUP_QUIET) != RT_DIR_NULL,
	    "first sanitized collision name is missing") &&
	expect(db_lookup(wdbp->dbip, "A_B.annot.D13", LOOKUP_QUIET) != RT_DIR_NULL,
	    "sanitized collision did not use its stable IGES entity suffix");

    struct rt_db_internal line_internal;
    RT_DB_INTERNAL_INIT(&line_internal);
    const struct rt_annot_internal *line =
	read_annotation(wdbp->dbip, "LINE.annot", line_internal);
    passed = expect(line != nullptr, "line annotation is missing") && passed;
    if (line) {
	passed = expect(line->ant.count == 1,
	    "line annotation segment count is wrong") && passed;
	passed = expect(line->styles &&
		line->styles[0].role == RT_ANNOT_ROLE_GEOMETRY &&
		line->styles[0].line_pattern == RT_ANNOT_LINE_DASHED &&
		(line->styles[0].flags & RT_ANNOT_STYLE_COLOR),
	    "line annotation semantics were not preserved") && passed;
	passed = expect(NEAR_EQUAL(line->u_vec[0], 1.0, SMALL_FASTF) &&
		NEAR_ZERO(line->u_vec[1], SMALL_FASTF) &&
		NEAR_ZERO(line->v_vec[0], SMALL_FASTF) &&
		NEAR_EQUAL(line->v_vec[1], 1.0, SMALL_FASTF),
	    "projected annotation does not use the canonical XY basis") && passed;
    }
    rt_db_free_internal(&line_internal);

    struct rt_db_internal note_internal;
    RT_DB_INTERNAL_INIT(&note_internal);
    const struct rt_annot_internal *note =
	read_annotation(wdbp->dbip, "NOTE.annot", note_internal);
    passed = expect(note && note->ant.count == 1 && note->styles &&
	    note->styles[0].role == RT_ANNOT_ROLE_TEXT,
	    "General Note text semantics were not preserved") && passed;
    rt_db_free_internal(&note_internal);

    struct rt_db_internal leader_internal;
    RT_DB_INTERNAL_INIT(&leader_internal);
    const struct rt_annot_internal *leader =
	read_annotation(wdbp->dbip, "LEADER.annot", leader_internal);
    bool have_leader = false;
    bool have_arrowhead = false;
    if (leader && leader->styles) {
	for (size_t i = 0; i < leader->ant.count; ++i) {
	    have_leader = have_leader ||
		leader->styles[i].role == RT_ANNOT_ROLE_LEADER;
	    have_arrowhead = have_arrowhead ||
		leader->styles[i].role == RT_ANNOT_ROLE_ARROWHEAD;
	}
    }
    passed = expect(have_leader && have_arrowhead,
	"Leader and arrowhead roles were not preserved") && passed;
    rt_db_free_internal(&leader_internal);

    struct rt_db_internal datum_internal;
    RT_DB_INTERNAL_INIT(&datum_internal);
    const struct rt_datum_internal *datum =
	read_datum(wdbp->dbip, "POINT.datum", datum_internal);
    passed = expect(datum && datum->type == RT_DATUM_POINT &&
	    datum->role == RT_DATUM_ROLE_REFERENCE &&
	    NEAR_EQUAL(datum->pnt[0], 1.0, SMALL_FASTF) &&
	    NEAR_EQUAL(datum->pnt[1], 2.0, SMALL_FASTF) &&
	    NEAR_ZERO(datum->pnt[2], SMALL_FASTF),
	"Point entity was not preserved as a projected reference datum") && passed;
    rt_db_free_internal(&datum_internal);

    struct bu_attribute_value_set attributes;
    bu_avs_init_empty(&attributes);
    struct directory *group = db_lookup(wdbp->dbip, "DIM.annot_group",
	LOOKUP_QUIET);
    if (group != RT_DIR_NULL)
	db5_get_attributes(wdbp->dbip, &attributes, group);
    const char *semantic = bu_avs_get(&attributes, "iges.semantic");
    passed = expect(semantic && BU_STR_EQUAL(semantic, "general_label"),
	"dimension semantic attribute is missing") && passed;
    bu_avs_free(&attributes);

    bu_avs_init_empty(&attributes);
    group = db_lookup(wdbp->dbip, "wire.annot_def", LOOKUP_QUIET);
    if (group != RT_DIR_NULL)
	db5_get_attributes(wdbp->dbip, &attributes, group);
    const char *original_name = bu_avs_get(&attributes, "iges.name");
    passed = expect(original_name && BU_STR_EQUAL(original_name, "wire"),
	"subfigure source name was not preserved") && passed;
    bu_avs_free(&attributes);

    bu_avs_init_empty(&attributes);
    group = db_lookup(wdbp->dbip, "wire_instance_D17.annot_instance",
	LOOKUP_QUIET);
    if (group != RT_DIR_NULL)
	db5_get_attributes(wdbp->dbip, &attributes, group);
    semantic = bu_avs_get(&attributes, "iges.semantic");
    const char *definition = bu_avs_get(&attributes, "iges.definition");
    passed = expect(semantic && BU_STR_EQUAL(semantic, "subfigure_instance") &&
	    definition && BU_STR_EQUAL(definition, "15"),
	"subfigure instance metadata is missing") && passed;
    bu_avs_free(&attributes);

    wdb_close(wdbp);
    bu_file_delete(path);
    return passed;
}

constexpr double SURFACE_SAMPLE_U = 0.3;
constexpr double SURFACE_SAMPLE_V = 0.7;

struct ImportedFace {
    bool written = false;
    bool valid = false;
    int edges = 0;
    int faces = 0;
    int loops = 0;
    int closed_edges = 0;
    int reversed_closed_trims = 0;
    bool solid = false;
    int singular_trims = 0;
    double maximum_parameter_gap = 0.0;
    double maximum_edge_tolerance = 0.0;
    ON_3dPoint surface_sample;
};

bool
run_import(const brlcad::iges::Document &document,
    const brlcad::iges::ImportOptions &options,
    brlcad::iges::BrepImportResult &result,
    const std::function<bool(struct rt_wdb *)> &inspect)
{
    char path[MAXPATHLEN] = {0};
    FILE *temporary = bu_temp_file(path, sizeof(path));
    if (!temporary)
	return false;
    std::fclose(temporary);
    bu_file_delete(path);

    struct rt_wdb *wdbp = wdb_fopen(path);
    if (wdbp == RT_WDB_NULL)
	return false;
    result = brlcad::iges::import_breps(document, wdbp, options);
    const bool checked = inspect(wdbp);
    wdb_close(wdbp);
    bu_file_delete(path);
    return checked;
}

bool
read_surface(struct rt_wdb *wdbp, ImportedFace &face,
    struct bu_attribute_value_set *attributes)
{
    struct directory *directory = db_lookup(wdbp->dbip, "FACE", LOOKUP_QUIET);
    face = ImportedFace();
    face.written = directory != RT_DIR_NULL;
    if (face.written) {
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	if (rt_db_get_internal(&internal, directory, wdbp->dbip, nullptr) >= 0) {
	    if (internal.idb_type == ID_BREP) {
		const struct rt_brep_internal *brep =
		    static_cast<const struct rt_brep_internal *>(internal.idb_ptr);
		if (brep && brep->brep) {
		    face.valid = brep->brep->IsValid();
		    face.edges = brep->brep->m_E.Count();
		    face.faces = brep->brep->m_F.Count();
		    face.loops = brep->brep->m_L.Count();
		    face.solid = brep->brep->IsSolid();
		    for (int i = 0; i < brep->brep->m_E.Count(); ++i) {
			const ON_BrepEdge &edge = brep->brep->m_E[i];
			face.maximum_edge_tolerance = std::max(face.maximum_edge_tolerance,
			    edge.m_tolerance);
			if (edge.m_vi[0] != edge.m_vi[1])
			    continue;
			++face.closed_edges;
			for (int j = 0; j < edge.m_ti.Count(); ++j)
			    face.reversed_closed_trims += brep->brep->m_T[edge.m_ti[j]].m_bRev3d;
		    }
		    for (int i = 0; i < brep->brep->m_L.Count(); ++i) {
			const ON_BrepLoop &loop = brep->brep->m_L[i];
			for (int j = 0; j < loop.m_ti.Count(); ++j) {
			    const ON_BrepTrim &trim = brep->brep->m_T[loop.m_ti[j]];
			    const ON_BrepTrim &next = brep->brep->m_T[
				loop.m_ti[(j + 1) % loop.m_ti.Count()]];
			    face.maximum_parameter_gap = std::max(face.maximum_parameter_gap,
				trim.PointAtEnd().DistanceTo(next.PointAtStart()));
			}
		    }
		    if (brep->brep->m_S.Count() > 0) {
			const ON_Surface *surface = brep->brep->m_S[0];
			face.surface_sample = surface->PointAt(
			    surface->Domain(0).ParameterAt(SURFACE_SAMPLE_U),
			    surface->Domain(1).ParameterAt(SURFACE_SAMPLE_V));
		    }
		    for (int i = 0; i < brep->brep->m_T.Count(); ++i)
			if (brep->brep->m_T[i].m_type == ON_BrepTrim::singular)
			    ++face.singular_trims;
		}
	    }
	    rt_db_free_internal(&internal);
	}
	if (attributes)
	    db5_get_attributes(wdbp->dbip, attributes, directory);
    }
    return true;
}

bool
run_surface_import(const brlcad::iges::Document &document,
    const brlcad::iges::ImportOptions &options,
    brlcad::iges::BrepImportResult &result, ImportedFace &face,
    struct bu_attribute_value_set *attributes)
{
    return run_import(document, options, result, [&](struct rt_wdb *wdbp) {
	return read_surface(wdbp, face, attributes);
    });
}


int
append_entity(std::vector<Entity> &entities, int type, const std::string &parameters,
    const std::string &label = "")
{
    const int id = static_cast<int>(entities.size() * 2 + 1);
    entities.push_back({type, 0, label, parameters});
    return id;
}

int
append_quad(std::vector<Entity> &entities, const std::array<ON_3dPoint, 4> &corners,
    bool recover_boundary = false)
{
    std::ostringstream surface;
    surface << "128,1,1,1,1,0,0,1,0,0,0,0,1,1,0,0,1,1,1,1,1,1";
    for (int corner : {0, 1, 3, 2})
	surface << ',' << corners[corner].x << ',' << corners[corner].y << ',' << corners[corner].z;
    surface << ",0,1,0,1;";
    const int base = append_entity(entities, 128, surface.str());
    const std::array<ON_2dPoint, 4> parameters = {
	ON_2dPoint(0, 0), ON_2dPoint(1, 0), ON_2dPoint(1, 1), ON_2dPoint(0, 1)
    };
    std::ostringstream model;
    std::ostringstream parameter;
    model << "102,4";
    parameter << "102," << (recover_boundary ? 3 : 4);
    for (size_t i = 0; i < corners.size(); ++i) {
	const size_t next = (i + 1) % corners.size();
	std::ostringstream line;
	line << "110," << corners[i].x << ',' << corners[i].y << ',' << corners[i].z
	    << ',' << corners[next].x << ',' << corners[next].y << ',' << corners[next].z << ';';
	model << ',' << append_entity(entities, 110, line.str());
	std::ostringstream uv;
	uv << "110," << parameters[i].x << ',' << parameters[i].y << ",0,"
	    << parameters[next].x << ',' << parameters[next].y << ",0;";
	const int id = append_entity(entities, 110, uv.str());
	if (!recover_boundary || i + 1 < corners.size())
	    parameter << ',' << id;
    }
    model << ';';
    parameter << ';';
    const int model_id = append_entity(entities, 102, model.str());
    const int parameter_id = append_entity(entities, 102, parameter.str());
    std::ostringstream boundary;
    boundary << "142,1," << base << ',' << parameter_id << ',' << model_id << ",1;";
    const int boundary_id = append_entity(entities, 142, boundary.str());
    std::ostringstream face;
    face << "144," << base << ",1,0," << boundary_id << ';';
    return append_entity(entities, 144, face.str());
}

enum class BoxOrientation {
    Outward,
    Inward,
    Inconsistent
};

std::vector<int>
append_box(std::vector<Entity> &entities, double minimum, double maximum,
    BoxOrientation orientation = BoxOrientation::Outward, bool recover_boundary = false)
{
    const std::array<ON_3dPoint, 8> corners = {
	ON_3dPoint(minimum, minimum, minimum), ON_3dPoint(maximum, minimum, minimum),
	ON_3dPoint(maximum, maximum, minimum), ON_3dPoint(minimum, maximum, minimum),
	ON_3dPoint(minimum, minimum, maximum), ON_3dPoint(maximum, minimum, maximum),
	ON_3dPoint(maximum, maximum, maximum), ON_3dPoint(minimum, maximum, maximum)
    };
    const std::array<std::array<int, 4>, 6> sides = {{
	{0, 3, 2, 1}, {0, 1, 5, 4}, {1, 2, 6, 5},
	{2, 3, 7, 6}, {3, 0, 4, 7}, {4, 5, 6, 7}
    }};
    std::vector<int> faces;
    for (size_t i = 0; i < sides.size(); ++i) {
	std::array<ON_3dPoint, 4> quad;
	for (size_t j = 0; j < quad.size(); ++j)
	    quad[j] = corners[sides[i][j]];
	if (orientation == BoxOrientation::Inward ||
	    (orientation == BoxOrientation::Inconsistent && i == 1))
	    std::swap(quad[1], quad[3]);
	faces.push_back(append_quad(entities, quad, recover_boundary && i + 1 == sides.size()));
    }
    return faces;
}

struct AssemblySummary {
    size_t objects = 0;
    size_t solids = 0;
    size_t faces = 0;
    size_t naked_edges = 0;
    size_t plate_objects = 0;
    size_t review_objects = 0;
    double signed_volume = 0.0;
};

bool
inspect_assembly(struct rt_wdb *wdbp, const std::vector<int> &sources, AssemblySummary &summary)
{
    summary = AssemblySummary();
    std::set<int> seen;
    bool valid = true;
    struct directory *entry;
    FOR_ALL_DIRECTORY_START(entry, wdbp->dbip) {
	if (entry->d_major_type != DB5_MAJORTYPE_BRLCAD || entry->d_minor_type != ID_BREP)
	    continue;
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	if (rt_db_get_internal(&internal, entry, wdbp->dbip, nullptr) < 0)
	    return false;
	const ON_Brep &brep = *static_cast<const struct rt_brep_internal *>(internal.idb_ptr)->brep;
	++summary.objects;
	summary.faces += brep.m_F.Count();
	summary.solids += brep.IsSolid();
	valid = valid && brep.IsValid();
	for (int i = 0; i < brep.m_E.Count(); ++i)
	    summary.naked_edges += brep.m_E[i].m_ti.Count() == 1;
	if (brep.IsSolid()) {
	    /* The fixture consists of complete planar quads.  Signed tetrahedra
	     * distinguish a preserved inward cavity from two positive boxes. */
	    for (int i = 0; i < brep.m_F.Count(); ++i) {
		const ON_BrepFace &face = brep.m_F[i];
		const ON_Interval u = face.Domain(0);
		const ON_Interval v = face.Domain(1);
		const ON_3dVector a = face.PointAt(u.Min(), v.Min()) - ON_3dPoint::Origin;
		const ON_3dVector b = face.PointAt(u.Max(), v.Min()) - ON_3dPoint::Origin;
		const ON_3dVector c = face.PointAt(u.Max(), v.Max()) - ON_3dPoint::Origin;
		const ON_3dVector d = face.PointAt(u.Min(), v.Max()) - ON_3dPoint::Origin;
		const double volume = (a * ON_CrossProduct(b, c) + a * ON_CrossProduct(c, d)) / 6.0;
		summary.signed_volume += face.m_bRev ? -volume : volume;
	    }
	}
	struct bu_attribute_value_set attributes;
	bu_avs_init_empty(&attributes);
	valid = db5_get_attributes(wdbp->dbip, &attributes, entry) >= 0 && valid;
	const char *metadata = bu_avs_get(&attributes, "iges.face_metadata");
	const char *plate = bu_avs_get(&attributes, "_plate_mode_thickness");
	const char *review = bu_avs_get(&attributes, "iges.import_status");
	summary.plate_objects += plate != nullptr;
	summary.review_objects += review && BU_STR_EQUAL(review, "needs_review");
	valid = valid && !(plate && brep.IsSolid()) && metadata;
	if (metadata) {
	    const std::string text(metadata);
	    size_t matched = 0;
	    for (int source : sources) {
		const std::string needle = "\"entity\":" + std::to_string(source) + ',';
		if (text.find(needle) == std::string::npos)
		    continue;
		valid = seen.insert(source).second && valid;
		++matched;
	    }
	    valid = valid && matched == static_cast<size_t>(brep.m_F.Count());
	    for (int i = 0; i < brep.m_F.Count(); ++i)
		valid = valid && text.find("\"face\":" + std::to_string(i) + ',') != std::string::npos;
	    if (review)
		valid = valid && text.find("\"recovery\":") != std::string::npos;
	}
	bu_avs_free(&attributes);
	rt_db_free_internal(&internal);
    } FOR_ALL_DIRECTORY_END;
    return valid && seen.size() == sources.size();
}

bool
has_diagnostic(const brlcad::iges::BrepImportResult &result, const char *code)
{
    return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
	[&](const brlcad::iges::ImportDiagnostic &diagnostic) {
	    return diagnostic.code == code;
	});
}

bool
test_nested_instances()
{
    std::vector<Entity> entities;
    std::array<ON_3dPoint, 4> corners = {ON_3dPoint(0, 0, 0), ON_3dPoint(1, 0, 0),
	ON_3dPoint(1, 1, 0), ON_3dPoint(0, 1, 0)};
    const int inner_face = append_quad(entities, corners);
    const int inner = append_entity(entities, 308,
	"308,0,5HINNER,1," + std::to_string(inner_face) + ';', "INNER");
    const int child = append_entity(entities, 408,
	"408," + std::to_string(inner) + ",10,0,0,1;", "CHILD");
    for (ON_3dPoint &point : corners)
	point.z = 1.0;
    const int outer_face = append_quad(entities, corners);
    const int outer = append_entity(entities, 308,
	"308,1,5HOUTER,2," + std::to_string(outer_face) + ',' + std::to_string(child) + ';', "OUTER");
    append_entity(entities, 408, "408," + std::to_string(outer) + ",100,0,0,1;", "ROOT");
    const auto document = brlcad::iges::Document::parse_buffer(sample(entities,
	"nested instances must resolve before their containing definition is written"));
    brlcad::iges::ImportOptions options;
    options.strict = true;
    brlcad::iges::BrepImportResult result;
    const auto inspect = [&](struct rt_wdb *wdbp) {
	struct directory *entry = db_lookup(wdbp->dbip, "OUTER", LOOKUP_QUIET);
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	if (entry == RT_DIR_NULL || rt_db_get_internal(&internal, entry, wdbp->dbip, nullptr) < 0)
	    return false;
	const auto *comb = static_cast<const struct rt_comb_internal *>(internal.idb_ptr);
	const bool complete = db_tree_nleaves(comb->tree) == 2;
	rt_db_free_internal(&internal);
	return complete;
    };
    bool passed = expect(document.valid() && run_import(document, options, result, inspect) &&
	result.success && result.statistics.unresolved_members == 0 && result.statistics.omitted == 0,
	"nested instance disappeared from a containing subfigure");
    entities[(child - 1) / 2].parameters = "408," + std::to_string(outer) + ",0,0,0,1;";
    const auto cycle = brlcad::iges::Document::parse_buffer(sample(entities, "cyclic nested instance"));
    passed = expect(run_import(cycle, options, result, [](struct rt_wdb *) { return true; }) &&
	!result.success && has_diagnostic(result, "hierarchy_cycle"),
	"strict import accepted a cyclic instance hierarchy") && passed;
    options.strict = false;
    passed = expect(run_import(cycle, options, result, [](struct rt_wdb *) { return true; }) &&
	result.success && result.statistics.unresolved_members > 0,
	"cyclic hierarchy prevented recovery of its independent geometry") && passed;

    std::vector<Entity> repeated;
    append_quad(repeated, corners);
    const int line = append_entity(repeated, 110, "110,0,0,0,1,0,0;", "WIRE");
    const int empty = append_entity(repeated, 308, "308,0,5HEMPTY,1," + std::to_string(line) + ';', "EMPTY");
    for (int i = 0; i < 3; ++i)
	append_entity(repeated, 408, "408," + std::to_string(empty) + ",0,0,0,1;");
    const auto empty_instances = brlcad::iges::Document::parse_buffer(sample(repeated,
	"repeated instances of a definition without supported surface geometry"));
    passed = expect(run_import(empty_instances, options, result, [](struct rt_wdb *) { return true; }) &&
	result.success && result.statistics.unresolved_members == 1 && result.statistics.omitted == 3,
	"repeated empty definitions multiplied unresolved source-member counts") && passed;
    return passed;
}

bool
test_import_progress()
{
    std::vector<Entity> entities;
    const std::vector<int> sources = append_box(entities, 0.0, 1.0, BoxOrientation::Outward, true);
    const auto document = brlcad::iges::Document::parse_buffer(sample(entities,
	"progress counts must not double-count retried faces"));
    brlcad::iges::ImportOptions options;
    brlcad::iges::BrepImportResult result;
    for (const auto repair : {brlcad::iges::RepairMode::BestEffort, brlcad::iges::RepairMode::Safe}) {
	size_t previous = 0;
	bool consistent = true;
	bool recovery_seen = false;
	bool hierarchy_seen = false;
	options.repair = repair;
	options.progress = [&](const char *stage, const char *activity, size_t completed,
	    size_t total, int64_t entity) {
	    if (BU_STR_EQUAL(stage, "hierarchy"))
		hierarchy_seen = true;
	    if (!BU_STR_EQUAL(stage, "geometry"))
		return;
	    consistent = consistent && total == sources.size() && completed >= previous &&
		completed <= total && !hierarchy_seen;
	    previous = completed;
	    if (BU_STR_EQUAL(activity, "recovering trimmed faces")) {
		consistent = consistent && entity == sources.back();
		recovery_seen = true;
	    }
	};
	if (!expect(run_import(document, options, result, [](struct rt_wdb *) { return true; }) &&
	    result.success && consistent && hierarchy_seen && previous == sources.size() &&
	    recovery_seen == (repair == brlcad::iges::RepairMode::BestEffort),
	    "progress lost stages, counted retries, or failed to count omitted faces"))
	    return false;
    }
    return true;
}

bool
test_post_recovery_assembly()
{
    std::vector<Entity> entities;
    const std::vector<int> box = append_box(entities, 0.0, 1.0, BoxOrientation::Outward, true);
    const auto document = brlcad::iges::Document::parse_buffer(sample(entities,
	"a recovered final face must close the original box"));
    brlcad::iges::ImportOptions options;
    brlcad::iges::BrepImportResult result;
    AssemblySummary summary;
    const auto inspect = [&](struct rt_wdb *wdbp) {
	return inspect_assembly(wdbp, box, summary);
    };
    bool passed = expect(document.valid() && run_import(document, options, result, inspect) &&
	result.success && summary.objects == 1 && summary.solids == 1 && summary.faces == 6 &&
	summary.naked_edges == 0 && summary.review_objects == 1 &&
	result.statistics.recovered_faces_written == 1 && result.statistics.omitted == 0 &&
	result.statistics.reassembly_edges_merged > 0 && result.statistics.solid_breps_written == 1,
	"recovered box face remained separate or lost its metadata");

    options.repair = brlcad::iges::RepairMode::Safe;
    const std::vector<int> incomplete(box.begin(), box.end() - 1);
    passed = expect(run_import(document, options, result, [&](struct rt_wdb *wdbp) {
	return inspect_assembly(wdbp, incomplete, summary);
    }) && result.success && summary.objects == 1 && summary.solids == 0 && summary.faces == 5 &&
	summary.naked_edges == 4 && result.statistics.omitted == 1 &&
	result.statistics.recovered_faces_written == 0,
	"conservative import lost open faces or invented a missing box face") && passed;

    options = brlcad::iges::ImportOptions();
    std::vector<Entity> missing = entities;
    int64_t surface_id = 0;
    document.parameters(brlcad::iges::EntityId(box.back()))->values[1].integer(surface_id);
    /* The final surface is a valid offset definition, not supported by
     * this importer.  UV repair cannot supply the missing surface builder. */
    missing[(surface_id - 1) / 2].type = 140;
    missing[(surface_id - 1) / 2].parameters = "140,0,0,1,1,1;";
    const auto invalid = brlcad::iges::Document::parse_buffer(sample(missing,
	"an unrecoverable face must leave the other five faces intact"));
    passed = expect(run_import(invalid, options, result, [&](struct rt_wdb *wdbp) {
	return inspect_assembly(wdbp, incomplete, summary);
    }) && result.success && summary.faces == 5 && summary.solids == 0 &&
	result.statistics.omitted == 1,
	"best-effort import fabricated an unrecoverable box face") && passed;

    const std::array<ON_3dPoint, 4> sheet = {
	ON_3dPoint(5, 0, 0), ON_3dPoint(6, 0, 0), ON_3dPoint(6, 1, 0), ON_3dPoint(5, 1, 0)
    };
    entities.clear();
    std::vector<int> sources = append_box(entities, 0.0, 1.0, BoxOrientation::Inconsistent, true);
    sources.push_back(append_quad(entities, sheet));
    std::array<ON_3dPoint, 4> second_sheet = sheet;
    for (ON_3dPoint &point : second_sheet)
	point.z += 1.0;
    sources.push_back(append_quad(entities, second_sheet));
    const auto mixed = brlcad::iges::Document::parse_buffer(sample(entities,
	"a closed box must not inherit an unrelated sheet's plate mode"));
    options.default_plate_thickness = 0.1;
    passed = expect(run_import(mixed, options, result, [&](struct rt_wdb *wdbp) {
	return inspect_assembly(wdbp, sources, summary);
    }) && result.success && summary.objects == 2 && summary.solids == 1 && summary.faces == 8 &&
	summary.plate_objects == 1 && summary.naked_edges == 8 &&
	result.statistics.plate_mode_objects_thickened == 1,
	"open sheets were split apart, prevented solid recognition, or applied plate mode to the box") && passed;

    entities.clear();
    sources = append_box(entities, 0.0, 3.0);
    const std::vector<int> cavity = append_box(entities, 1.0, 2.0, BoxOrientation::Inward, true);
    sources.insert(sources.end(), cavity.begin(), cavity.end());
    sources.push_back(append_quad(entities, sheet));
    const auto nested = brlcad::iges::Document::parse_buffer(sample(entities,
	"reassembly must preserve an inward cavity alongside an open sheet"));
    constexpr double VOLUME_COMPARISON_TOLERANCE = 1.0e-10;
    passed = expect(run_import(nested, options, result, [&](struct rt_wdb *wdbp) {
	return inspect_assembly(wdbp, sources, summary);
    }) && result.success && summary.objects == 2 && summary.solids == 1 && summary.faces == 13 &&
	NEAR_EQUAL(summary.signed_volume, 26.0, VOLUME_COMPARISON_TOLERANCE) && summary.plate_objects == 1 &&
	summary.review_objects == 1 && result.statistics.recovered_faces_written == 1,
	"component extraction detached or filled the nested cavity") && passed;

    entities.clear();
    sources = append_box(entities, 0.0, 1.0, BoxOrientation::Outward, true);
    std::ostringstream body;
    body << "402," << sources.size() - 1;
    for (size_t i = 0; i + 1 < sources.size(); ++i)
	body << ',' << sources[i];
    body << ';';
    append_entity(entities, 402, body.str(), "BODY");
    entities.back().form = 1;
    append_entity(entities, 402, "402,1," + std::to_string(sources.back()) + ';', "CAP");
    entities.back().form = 1;
    const auto separate_owners = brlcad::iges::Document::parse_buffer(sample(entities,
	"recovery must not join faces belonging to different source owners"));
    passed = expect(run_import(separate_owners, options, result, [&](struct rt_wdb *wdbp) {
	return inspect_assembly(wdbp, sources, summary);
    }) && result.success && summary.objects == 2 && summary.solids == 0 && summary.faces == 6 &&
	summary.naked_edges == 8 && summary.review_objects == 1 &&
	result.statistics.recovered_faces_written == 1 && result.statistics.reassembly_edges_merged == 0,
	"reassembly crossed an IGES ownership boundary") && passed;
    return passed;
}

bool
test_bounded_surface_tolerance()
{
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_buffer(bounded_surface_sample(),
	    "bounded.iges");
    if (!expect(document.valid(), "bounded-surface test IGES did not parse"))
	return false;

    brlcad::iges::ImportOptions conservative_options;
    brlcad::iges::BrepImportResult conservative_result;
    ImportedFace conservative_face;
    if (!expect(run_surface_import(document, conservative_options,
	    conservative_result, conservative_face, nullptr),
	    "could not run conservative bounded-surface import"))
	return false;
    bool passed = expect(!conservative_result.success && !conservative_face.written &&
	    !conservative_face.valid &&
	    conservative_result.statistics.bounded_surfaces_seen == 1 &&
	    conservative_result.statistics.relaxed_faces_written == 0 &&
	    conservative_result.statistics.omitted == 1,
	"conservative import did not reject the out-of-tolerance face");

    brlcad::iges::ImportOptions relaxed_options;
    relaxed_options.maximum_repair_tolerance = 0.1;
    brlcad::iges::BrepImportResult relaxed_result;
    ImportedFace relaxed_face;
    struct bu_attribute_value_set attributes;
    bu_avs_init_empty(&attributes);
    if (!expect(run_surface_import(document, relaxed_options,
	    relaxed_result, relaxed_face, &attributes),
	    "could not run relaxed bounded-surface import")) {
	bu_avs_free(&attributes);
	return false;
    }
    const char *status = bu_avs_get(&attributes, "iges.tolerance_status");
    const char *basis = bu_avs_get(&attributes, "iges.tolerance_basis");
    const char *maximum = bu_avs_get(&attributes,
	"iges.maximum_repair_tolerance_mm");
    const char *nominal = bu_avs_get(&attributes,
	"iges.nominal_tolerance_mm");
    const char *face_metadata = bu_avs_get(&attributes, "iges.face_metadata");
    const bool flagged = status && BU_STR_EQUAL(status, "relaxed") &&
	basis && BU_STR_EQUAL(basis, "import_default") && maximum && nominal &&
	face_metadata && std::string(face_metadata).find("repair_tolerance_mm") !=
	    std::string::npos;
    passed = expect(relaxed_result.success && relaxed_face.written && relaxed_face.valid &&
	    relaxed_result.statistics.bounded_surfaces_seen == 1 &&
	    relaxed_result.statistics.relaxed_faces_written == 1 &&
	    relaxed_result.statistics.omitted == 0 &&
	    NEAR_EQUAL(relaxed_result.statistics.maximum_repair_tolerance_used,
		0.1, SMALL_FASTF) && flagged,
	"relaxed import did not preserve and flag the repaired valid face") &&
	passed;
    bu_avs_free(&attributes);
    return passed;
}

bool
test_singular_boundary()
{
    std::vector<Entity> entities = {
	{128, 0, "SURFACE",
	    "128,1,1,1,1,0,0,1,0,0,0,0,1,1,0,0,1,1,1,1,1,1,"
	    "0,0,0,0,1,0,1,0,0,0,1,0,0,1,0,1;"},
	{110, 0, "PBOTTOM", "110,0,0,0,1,0,0;"},
	{110, 0, "PPOLE", "110,1,0,0,1,1,0;"},
	{110, 0, "PTOP", "110,1,1,0,0,1,0;"},
	{110, 0, "PLEFT", "110,0,1,0,0,0,0;"},
	{110, 0, "MBOTTOM", "110,0,0,0,0,1,0;"},
	{110, 0, "MTOP", "110,0,1,0,1,0,0;"},
	{110, 0, "MLEFT", "110,1,0,0,0,0,0;"},
	{102, 0, "PARAM", "102,4,3,5,7,9;"},
	{102, 0, "MODEL", "102,3,11,13,15;"},
	{142, 0, "BOUNDARY", "142,1,1,17,19,1;"},
	{144, 0, "FACE", "144,1,1,0,21;"}
    };
    brlcad::iges::ImportOptions options;
    options.exact = true;
    brlcad::iges::BrepImportResult result;
    ImportedFace face;
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_buffer(sample(entities,
	    "singular boundary without a model edge"));
    if (!expect(document.valid() && run_surface_import(document, options,
	    result, face, nullptr), "could not run singular boundary import"))
	return false;
    bool passed = expect(result.success && face.valid && face.edges == 3 &&
	face.singular_trims == 1 && result.statistics.repairs == 0 &&
	result.statistics.omitted == 0,
	"exact import did not preserve the unmatched singular trim");

    std::vector<Entity> missing_pole = entities;
    missing_pole[8].parameters = "102,3,3,7,9;";
    const auto incomplete_pole = brlcad::iges::Document::parse_buffer(sample(missing_pole,
	"pole side omitted from both model and parameter boundaries"));
    options.exact = false;
    passed = expect(run_surface_import(incomplete_pole, options, result, face, nullptr) &&
	result.success && face.valid && face.edges == 3 && face.singular_trims == 1 &&
	has_diagnostic(result, "inserted_singular_boundary"),
	"missing pole side was not completed as a singular trim") && passed;
    options.exact = true;
    passed = expect(run_surface_import(incomplete_pole, options, result, face, nullptr) &&
	result.statistics.repairs == 0 && face.singular_trims == 0,
	"exact mode inserted a missing pole boundary") && passed;

    std::vector<Entity> rounded = entities;
    rounded[1].parameters = "110,0,0,0,0.99999999,0,0;";
    rounded[2].parameters = "110,0.99999999,0,0,0.99999999,1,0;";
    rounded[3].parameters = "110,0.99999999,1,0,0,1,0;";
    const brlcad::iges::Document near_pole =
	brlcad::iges::Document::parse_buffer(sample(rounded,
	    "rounded parameter coordinates near a pole"));
    options.exact = false;
    passed = expect(near_pole.valid() && run_surface_import(near_pole,
	options, result, face, nullptr) && result.success && face.valid &&
	face.singular_trims == 1 && result.statistics.repairs > 0 &&
	has_diagnostic(result, "snapped_singular_boundary"),
	"bounded repair did not preserve the rounded pole boundary") && passed;
    options.exact = true;
    passed = expect(run_surface_import(near_pole, options, result, face,
	nullptr) && !result.success && !face.written,
	"exact import snapped a rounded pole boundary") && passed;

    std::vector<Entity> rounded_surface = entities;
    rounded_surface[0].parameters =
	"128,1,1,1,1,0,0,1,0,0,0,0,1,1,0,0,1,1,1,1,1,1,"
	"0,0,0,0,1,0,1,0,0,0.00000001,1,0,0,1,0,1;";
    const brlcad::iges::Document approximate_pole =
	brlcad::iges::Document::parse_buffer(sample(rounded_surface,
	    "rounded surface control points at a pole"));
    options.exact = false;
    passed = expect(approximate_pole.valid() && run_surface_import(approximate_pole,
	options, result, face, nullptr) && result.success && face.valid &&
	face.edges == 3 && face.singular_trims == 1 &&
	has_diagnostic(result, "approximated_singular_boundary"),
	"bounded pole recognition rejected rounded control points") && passed;
    options.exact = true;
    passed = expect(run_surface_import(approximate_pole, options, result, face,
	nullptr) && !result.success && !face.written,
	"exact import approximated a surface pole") && passed;
    options.exact = false;
    options.maximum_repair_tolerance = ON_ZERO_TOLERANCE;
    passed = expect(run_surface_import(approximate_pole, options, result, face,
	nullptr) && !result.success && !face.written,
	"surface pole approximation exceeded the requested tolerance") && passed;
    options.maximum_repair_tolerance = 0.0;

    std::vector<Entity> segmented = entities;
    segmented[5] = {126, 0, "ACROSS",
	"126,2,1,1,0,1,0,0,0,0.5,1,1,1,1,1,"
	"0,0,0,0,1,0,1,0,0,0,1,0,0,1;"};
    segmented[9].parameters = "102,2,11,15;";
    const brlcad::iges::Document crossing =
	brlcad::iges::Document::parse_buffer(sample(segmented,
	    "model curve crosses a parameter-space pole"));
    options.exact = false;
    passed = expect(crossing.valid() && run_surface_import(crossing,
	options, result, face, nullptr) && result.success && face.valid &&
	face.edges == 3 && face.singular_trims == 1 &&
	has_diagnostic(result, "matched_boundary_segments"),
	"differently segmented pole boundary was not matched") && passed;
    options.maximum_repair_tolerance = 2.0;
    passed = expect(run_surface_import(crossing, options, result, face,
	nullptr) && result.success && face.valid && face.edges == 3,
	"increased tolerance lost a uniquely matched boundary") && passed;
    options.maximum_repair_tolerance = 0.0;
    options.exact = true;
    passed = expect(run_surface_import(crossing, options, result, face,
	nullptr) && !result.success && !face.written,
	"exact import reconstructed boundary segmentation") && passed;

    /* The extra parameter segment must not be accepted on a regular side. */
    entities[0].parameters =
	"128,1,1,1,1,0,0,1,0,0,0,0,1,1,0,0,1,1,1,1,1,1,"
	"0,0,0,0,1,0,1,0,0,1,1,0,0,1,0,1;";
    const brlcad::iges::Document nonsingular =
	brlcad::iges::Document::parse_buffer(sample(entities,
	    "mismatched nonsingular boundary"));
    passed = expect(nonsingular.valid() && run_surface_import(nonsingular,
	options, result, face, nullptr) && !result.success && !face.written &&
	result.statistics.omitted == 1 &&
	has_diagnostic(result, "boundary_curve_cardinality"),
	"unmatched nonsingular boundary was not rejected") && passed;
    options = brlcad::iges::ImportOptions();
    struct bu_attribute_value_set attributes;
    bu_avs_init_empty(&attributes);
    passed = expect(run_surface_import(nonsingular, options, result, face, &attributes) &&
	result.success && face.valid && face.edges == 3 && face.singular_trims == 0 &&
	result.statistics.omitted == 0 && result.statistics.recovered_faces_written == 1 &&
	has_diagnostic(result, "recovered_trimmed_face"),
	"best-effort import did not recover the authored model boundary") && passed;
    const char *status = bu_avs_get(&attributes, "iges.import_status");
    const char *recovery = bu_avs_get(&attributes, "iges.recovery");
    passed = expect(status && BU_STR_EQUAL(status, "needs_review") &&
	recovery && BU_STR_EQUAL(recovery, "model_boundaries"),
	"best-effort boundary reconstruction was not flagged for review") && passed;
    bu_avs_free(&attributes);
    for (int mode = 0; mode < 5; ++mode) {
	options = brlcad::iges::ImportOptions();
	if (mode == 0)
	    options.repair = brlcad::iges::RepairMode::Safe;
	else if (mode == 1)
	    options.exact = true;
	else if (mode == 2)
	    options.strict = true;
	else if (mode == 3)
	    options.repair = brlcad::iges::RepairMode::None;
	else
	    options.maximum_repair_tolerance = 0.001;
	passed = expect(run_surface_import(nonsingular, options, result, face, nullptr) &&
	    !face.written && result.statistics.recovered_faces_written == 0 &&
	    result.statistics.omitted == 1,
	    "best-effort recovery ignored an explicit conservative setting") && passed;
    }
    return passed;
}

bool
test_trim_loop_tolerance()
{
    std::vector<Entity> entities = {
	{128, 0, "SURFACE",
	    "128,1,1,1,1,0,0,1,0,0,0,0,1,1,0,0,1,1,1,1,1,1,"
	    "0,0,0,10,0,0,0,10,0,10,10,0,0,1,0,1;"},
	{110, 0, "PBOTTOM", "110,0,0,0,1,0.005,0;"},
	{110, 0, "PRIGHT", "110,1,0.0051,0,1,1,0;"},
	{110, 0, "PTOP", "110,1,1,0,0,1,0;"},
	{110, 0, "PLEFT", "110,0,1,0,0,0,0;"},
	{110, 0, "MBOTTOM", "110,0,0,0,10,0,0;"},
	{110, 0, "MRIGHT", "110,10,0,0,10,10,0;"},
	{110, 0, "MTOP", "110,10,10,0,0,10,0;"},
	{110, 0, "MLEFT", "110,0,10,0,0,0,0;"},
	{102, 0, "PARAM", "102,4,3,5,7,9;"},
	{102, 0, "MODEL", "102,4,11,13,15,17;"},
	{142, 0, "BOUNDARY", "142,1,1,19,21,1;"},
	{144, 0, "FACE", "144,1,1,0,23;"}
    };
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_buffer(sample(entities,
	    "trim loop with inconsistent model and parameter endpoints"));
    if (!expect(document.valid(), "trim-loop test IGES did not parse"))
	return false;
    brlcad::iges::ImportOptions options;
    brlcad::iges::BrepImportResult result;
    ImportedFace face;
    bool passed = expect(run_surface_import(document, options, result,
	face, nullptr) && result.statistics.repairs == 0 &&
	result.statistics.relaxed_faces_written == 0,
	"default import repaired the out-of-tolerance trim loop");
    options.maximum_repair_tolerance = 0.001;
    passed = expect(run_surface_import(document, options, result, face,
	nullptr) && result.statistics.repairs == 0 &&
	result.statistics.relaxed_faces_written == 0,
	"trim-loop repair exceeded the supplied tolerance") && passed;

    options.maximum_repair_tolerance = 0.1;
    struct bu_attribute_value_set attributes;
    bu_avs_init_empty(&attributes);
    passed = expect(run_surface_import(document, options, result, face,
	&attributes) && result.success && face.valid && face.edges == 4 &&
	NEAR_ZERO(face.maximum_parameter_gap, SMALL_FASTF) &&
	result.statistics.omitted == 0 &&
	result.statistics.relaxed_faces_written == 1 &&
	NEAR_EQUAL(result.statistics.maximum_repair_tolerance_used, 0.05,
	    ON_ZERO_TOLERANCE) && has_diagnostic(result, "relaxed_parameter_loop"),
	"explicit trim-loop repair did not produce and report a valid face") && passed;
    const char *status = bu_avs_get(&attributes, "iges.tolerance_status");
    const char *metadata = bu_avs_get(&attributes, "iges.face_metadata");
    passed = expect(status && BU_STR_EQUAL(status, "relaxed") && metadata &&
	std::string(metadata).find("repair_tolerance_mm") != std::string::npos,
	"trim-loop repair tolerance is missing from the database") && passed;
    bu_avs_free(&attributes);

    options.exact = true;
    passed = expect(run_surface_import(document, options, result, face,
	nullptr) && result.statistics.repairs == 0 &&
	result.statistics.relaxed_faces_written == 0,
	"exact import repaired a trim loop") && passed;

    entities[1].parameters = "110,0,0,0,1,0.00005,0;";
    entities[2].parameters = "110,1,0.000051,0,1,1,0;";
    const auto relative = brlcad::iges::Document::parse_buffer(sample(entities,
	"trim endpoint errors bounded by local model size"));
    options = brlcad::iges::ImportOptions();
    bu_avs_init_empty(&attributes);
    passed = expect(run_surface_import(relative, options, result, face, &attributes) &&
	result.success && face.valid && result.statistics.relaxed_faces_written == 1 &&
	NEAR_EQUAL(result.statistics.maximum_repair_tolerance_used, 0.0005, ON_ZERO_TOLERANCE),
	"default object-relative tolerance did not repair the bounded trim gap") && passed;
    const char *basis = bu_avs_get(&attributes, "iges.repair_tolerance_basis");
    passed = expect(basis && BU_STR_EQUAL(basis, "object_relative"),
	"relative repair was not identified in the output metadata") && passed;
    bu_avs_free(&attributes);

    std::vector<Entity> mixed_sizes = entities;
    mixed_sizes[1].parameters = "110,0,0,0,1,0.005,0;";
    mixed_sizes[2].parameters = "110,1,0.0051,0,1,1,0;";
    mixed_sizes.push_back({128, 0, "LARGE",
	"128,1,1,1,1,0,0,1,0,0,0,0,1,1,0,0,1,1,1,1,1,1,"
	"0,0,0,1000000,0,0,0,1000000,0,1000000,1000000,0,0,1,0,1;"});
    const auto assembly_sizes = brlcad::iges::Document::parse_buffer(sample(mixed_sizes,
	"an unrelated large surface must not enlarge a small boundary's repair allowance"));
    passed = expect(run_surface_import(assembly_sizes, options, result, face, nullptr) &&
	result.statistics.repairs == 0 && result.statistics.relaxed_faces_written == 0,
	"an unrelated object enlarged the local relative tolerance") && passed;

    options.relative_tolerance = 0.0;
    passed = expect(run_surface_import(relative, options, result, face, nullptr) &&
	result.statistics.repairs == 0,
	"zero relative tolerance did not retain source-based behavior") && passed;
    options = brlcad::iges::ImportOptions();
    options.maximum_repair_tolerance = 0.0001;
    passed = expect(run_surface_import(relative, options, result, face, nullptr) &&
	result.statistics.repairs == 0,
	"relative tolerance overrode the explicit absolute repair allowance") && passed;

    for (int mode = 0; mode < 3; ++mode) {
	options = brlcad::iges::ImportOptions();
	options.exact = mode == 0;
	options.strict = mode == 1;
	if (mode == 2)
	    options.repair = brlcad::iges::RepairMode::None;
	passed = expect(run_surface_import(relative, options, result, face, nullptr) &&
	    result.statistics.repairs == 0 && result.statistics.relaxed_faces_written == 0,
	    "relative tolerance enabled a prohibited repair") && passed;
    }

    options = brlcad::iges::ImportOptions();
    const auto inches = brlcad::iges::Document::parse_buffer(sample(entities,
	"relative repair allowance is converted from source inches to millimeters",
	"1H,,1H;,,,,,,,,,,,1,1,,,,,1e-9;"));
    constexpr double MILLIMETERS_PER_INCH = 25.4;
    passed = expect(run_surface_import(inches, options, result, face, nullptr) &&
	result.success && face.valid && result.statistics.relaxed_faces_written == 1 &&
	NEAR_EQUAL(result.statistics.maximum_repair_tolerance_used,
	    0.0005 * MILLIMETERS_PER_INCH, ON_ZERO_TOLERANCE),
	"relative repair allowance did not respect source length units") && passed;

    const int placement = static_cast<int>(entities.size() * 2 + 1);
    entities.back().transform = placement;
    entities.push_back({124, 0, "SCALE", ""});
    for (double scale : {1.0, 1000.0}) {
	std::ostringstream matrix;
	matrix << "124," << scale << ",0,0,1000000,0," << scale
	    << ",0,0,0,0," << scale << ",0;";
	entities.back().parameters = matrix.str();
	const auto transformed = brlcad::iges::Document::parse_buffer(sample(entities,
	    "relative repair is invariant under translation and uniform scaling"));
	passed = expect(run_surface_import(transformed, options, result, face, nullptr) &&
	    result.success && face.valid && result.statistics.relaxed_faces_written == 1 &&
	    NEAR_EQUAL(result.statistics.maximum_repair_tolerance_used,
		0.0005 * scale, ON_ZERO_TOLERANCE),
	    "relative trim tolerance did not follow transformed model size") && passed;
    }

    for (double invalid : {-1.0, std::numeric_limits<double>::infinity(),
	std::numeric_limits<double>::quiet_NaN()}) {
	options.relative_tolerance = invalid;
	passed = expect(run_surface_import(relative, options, result, face, nullptr) &&
	    !result.success && !face.written && has_diagnostic(result, "invalid_repair_tolerance"),
	    "invalid relative tolerance was not rejected") && passed;
    }
    return passed;
}

bool
test_revolution_parameters()
{
    std::vector<Entity> entities = {
	{110, 0, "AXIS", "110,0,0,0,0,0,1;"},
	{110, 0, "LINE", "110,2,0,0,2,0,10;"},
	{120, 0, "FACE", "120,1,3,0,3.141592653589793;"}
    };
    brlcad::iges::ImportOptions options;
    options.exact = true;
    brlcad::iges::BrepImportResult result;
    ImportedFace face;
    const brlcad::iges::Document cylinder =
	brlcad::iges::Document::parse_buffer(sample(entities,
	    "cylinder with angular surface parameters"));
    const double rotation = SURFACE_SAMPLE_V * ON_PI;
    const ON_3dPoint cylinder_point(2.0 * std::cos(rotation),
	2.0 * std::sin(rotation), 10.0 * SURFACE_SAMPLE_U);
    bool passed = expect(cylinder.valid() && run_surface_import(cylinder,
	options, result, face, nullptr) && result.success && face.valid &&
	face.surface_sample.DistanceTo(cylinder_point) < ON_ZERO_TOLERANCE,
	"revolution did not preserve curve-first, angle-second parameters");

    /* A nonzero starting angle and non-quadrant sample distinguish the
     * authored angular parameter from the rational arc parameter. */
    entities[1] = {100, 0, "ARC", "100,0,3,0,3,1,2,0;"};
    const brlcad::iges::Document revolved_arc =
	brlcad::iges::Document::parse_buffer(sample(entities,
	    "revolution with a circular generatrix"));
    const double curve_angle = (1.0 + SURFACE_SAMPLE_U) * ON_PI / 2.0;
    const double x = 3.0 + std::cos(curve_angle);
    const double y = std::sin(curve_angle);
    const ON_3dPoint arc_point(x * std::cos(rotation) - y * std::sin(rotation),
	x * std::sin(rotation) + y * std::cos(rotation), 0.0);
    passed = expect(revolved_arc.valid() && run_surface_import(revolved_arc,
	options, result, face, nullptr) && result.success && face.valid &&
	face.surface_sample.DistanceTo(arc_point) < ON_ZERO_TOLERANCE,
	"circular generatrix lost its original angular domain") && passed;

    entities[1] = {100, 0, "ARC", "100,0,0,0,0.0254,0,0,0.0254;", 7};
    entities.push_back({124, 0, "PLACE", "124,1,0,0,1000,0,1,0,2000,0,0,1,3;"});
    const auto placed_arc = brlcad::iges::Document::parse_buffer(sample(entities,
	"small circular generatrix with a placement transform"));
    const double placed_angle = SURFACE_SAMPLE_U * ON_PI / 2.0;
    const double placed_x = 1000.0 + 0.0254 * std::cos(placed_angle);
    const double placed_y = 2000.0 + 0.0254 * std::sin(placed_angle);
    const ON_3dPoint placed_point(placed_x * std::cos(rotation) - placed_y * std::sin(rotation),
	placed_x * std::sin(rotation) + placed_y * std::cos(rotation), 3.0);
    passed = expect(placed_arc.valid() && run_surface_import(placed_arc,
	options, result, face, nullptr) && result.success && face.valid &&
	face.surface_sample.DistanceTo(placed_point) < ON_ZERO_TOLERANCE,
	"transformed small circular generatrix was not preserved") && passed;

    entities[3].parameters = "124,2,0,0,1000,0,1,0,2000,0,0,1,3;";
    const auto stretched_arc = brlcad::iges::Document::parse_buffer(sample(entities,
	"nonuniform transform must not approximate an ellipse by a circle"));
    passed = expect(run_surface_import(stretched_arc, options, result, face, nullptr) &&
	!result.success && !face.written && has_diagnostic(result, "revolution_arc_parameters"),
	"nonuniformly transformed generatrix was silently approximated") && passed;
    return passed;
}

bool
test_natural_boundary()
{
    std::vector<Entity> entities = {
	{128, 0, "SURFACE",
	    "128,1,1,1,1,0,0,1,0,0,0,0,1,1,0,0,1,1,1,1,1,1,"
	    "0,0,0,10,0,0,0,10,0,10,10,0,0,1,0,1;"},
	{144, 0, "FACE", "144,1,0,0,0;"},
	{100, 0, "PARAM", "100,0,0.5,0.5,0.6,0.5,0.6,0.5;"},
	{100, 0, "MODEL", "100,0,5,5,6,5,6,5;"},
	{142, 0, "HOLE", "142,1,1,5,7,1;"}
    };
    brlcad::iges::ImportOptions options;
    options.exact = true;
    brlcad::iges::BrepImportResult result;
    ImportedFace face;
    const auto natural = brlcad::iges::Document::parse_buffer(sample(entities,
	"trimmed surface with its natural outer boundary"));
    bool passed = expect(natural.valid() && run_surface_import(natural, options,
	result, face, nullptr) && result.success && face.valid && face.edges == 4 &&
	face.loops == 1 && result.statistics.omitted == 0,
	"natural outer surface boundary was not imported");
    entities[1].parameters = "144,1,0,1,0,9;";
    const auto hole = brlcad::iges::Document::parse_buffer(sample(entities,
	"natural outer boundary with an explicit inner loop"));
    return expect(hole.valid() && run_surface_import(hole, options,
	result, face, nullptr) && result.success && face.valid && face.edges == 5 &&
	face.loops == 2 && result.statistics.omitted == 0,
	"natural outer boundary lost its inner loop") && passed;
}

bool
test_bounded_plane()
{
    std::vector<Entity> entities = {
	{108, 1, "FACE", "108,0,0,1,1,5,0,0,1,1;"},
	{108, -1, "INNER", "108,0,0,1,1,7,0,0,1,1;"},
	{100, 0, "OUTER", "100,1,0,0,2,0,2,0;"},
	{100, 0, "HOLE", "100,1,0,0,1,0,1,0;"},
	{402, 9, "PARENT", "402,1,1,1,3;"}
    };
    brlcad::iges::ImportOptions options;
    options.exact = true;
    brlcad::iges::BrepImportResult result;
    ImportedFace face;
    const auto annulus = brlcad::iges::Document::parse_buffer(sample(entities,
	"bounded plane with a single-parent hole association"));
    bool passed = expect(annulus.valid() && run_surface_import(annulus, options,
	result, face, nullptr) && result.success && face.valid && face.loops == 2 &&
	face.closed_edges == 2 && result.statistics.omitted == 0 &&
	result.statistics.repairs == 0 && NEAR_EQUAL(face.surface_sample.z, 1.0, SMALL_FASTF),
	"bounded plane or its associated hole was not preserved");

    entities[0].parameters = "108,0,0,1,-1,5,0,0,1,1;";
    const auto opposite_constant = brlcad::iges::Document::parse_buffer(sample(entities,
	"legacy plane constant uses the opposite sign"));
    options.exact = false;
    passed = expect(run_surface_import(opposite_constant, options, result, face, nullptr) &&
	result.success && face.valid && face.loops == 2 &&
	has_diagnostic(result, "repaired_plane_constant") &&
	NEAR_EQUAL(face.surface_sample.z, 1.0, SMALL_FASTF),
	"boundary-supported plane constant repair failed") && passed;
    options.exact = true;
    return expect(run_surface_import(opposite_constant, options, result, face, nullptr) &&
	!result.success && !face.written && result.statistics.repairs == 0,
	"exact mode repaired an inconsistent bounded plane") && passed;
}

bool
test_small_boundary_pullback()
{
    const std::vector<Entity> entities = {
	{128, 0, "SURFACE",
	    "128,1,1,1,1,0,0,1,0,0,0,0,1,1,0,0,1,1,1,1,1,1,"
	    "0,0,0,1,0,0,0,1,0,1,1,0,0,1,0,1;"},
	{110, 0, "BOTTOM", "110,0.5,0.5,0,0.50005,0.5,0;"},
	{110, 0, "RIGHT", "110,0.50005,0.5,0,0.50005,0.50005,0;"},
	{110, 0, "TOP", "110,0.50005,0.50005,0,0.5,0.50005,0;"},
	{110, 0, "LEFT", "110,0.5,0.50005,0,0.5,0.5,0;"},
	{141, 0, "BOUNDARY", "141,0,0,1,4,3,1,0,5,1,0,7,1,0,9,1,0;"},
	{143, 0, "FACE", "143,0,1,1,11;"}
    };
    const auto document = brlcad::iges::Document::parse_buffer(sample(entities,
	"small features must survive missing parameter-curve recovery"));
    brlcad::iges::ImportOptions options;
    brlcad::iges::BrepImportResult result;
    ImportedFace face;
    return expect(document.valid() && run_surface_import(document, options,
	result, face, nullptr) && result.success && face.valid && face.edges == 4 &&
	!has_diagnostic(result, "discarded_collapsed_boundary"),
	"pullback repair collapsed real small boundary segments");
}

bool
test_periodic_boundary_pullback()
{
    std::vector<Entity> entities = {
	{128, 0, "SURFACE",
	    "128,1,8,1,2,0,1,0,0,0,0,0,1,1,"
	    "-3.141592653589793,-3.141592653589793,-3.141592653589793,"
	    "-1.5707963267948966,-1.5707963267948966,0,0,"
	    "1.5707963267948966,1.5707963267948966,"
	    "3.141592653589793,3.141592653589793,3.141592653589793,"
	    "1,1,0.7071067811865476,0.7071067811865476,"
	    "1,1,0.7071067811865476,0.7071067811865476,"
	    "1,1,0.7071067811865476,0.7071067811865476,"
	    "1,1,0.7071067811865476,0.7071067811865476,1,1,"
	    "1,0,0,1,0,1,1,1,0,1,1,1,0,1,0,0,1,1,-1,1,0,-1,1,1,"
	    "-1,0,0,-1,0,1,-1,-1,0,-1,-1,1,0,-1,0,0,-1,1,1,-1,0,1,-1,1,"
	    "1,0,0,1,0,1,0,1,-3.141592653589793,3.141592653589793;"},
	{100, 0, "BOTTOM", "100,0,0,0,-1,0,1,0;"},
	{110, 0, "RIGHT", "110,1,0,0,1,0,1;"},
	{100, 0, "TOP", "100,1,0,0,-1,0,1,0;"},
	{110, 0, "LEFT", "110,-1,0,1,-1,0,0;"},
	{141, 0, "BOUNDARY", "141,0,0,1,4,3,1,0,5,1,0,7,2,0,9,1,0;"},
	{143, 0, "FACE", "143,0,1,1,11;"}
    };
    const auto document = brlcad::iges::Document::parse_buffer(sample(entities,
	"periodic pullbacks must evaluate inside the native NURBS domain"));
    brlcad::iges::ImportOptions options;
    brlcad::iges::BrepImportResult result;
    ImportedFace face;
    bool passed = expect(document.valid() && run_surface_import(document, options,
	result, face, nullptr) && result.success && face.valid && face.edges == 4 &&
	face.maximum_edge_tolerance <= 1.0e-6,
	"periodic pullback produced a trim outside the native surface domain");

    entities[1].parameters = "100,0,0,0,0.7071067811865476,-0.7071067811865476,0.7071067811865476,0.7071067811865476;";
    entities[2].parameters = "110,0.7071067811865476,0.7071067811865476,0,0.7071067811865476,0.7071067811865476,1;";
    entities[3].parameters = "100,1,0,0,0.7071067811865476,-0.7071067811865476,0.7071067811865476,0.7071067811865476;";
    entities[4].parameters = "110,0.7071067811865476,-0.7071067811865476,1,0.7071067811865476,-0.7071067811865476,0;";
    entities.push_back({110, 0, "PBOTTOM", "110,0,2.356194490192345,0,0,3.9269908169872414,0;"});
    entities.push_back({110, 0, "PRIGHT", "110,0,3.9269908169872414,0,1,3.9269908169872414,0;"});
    entities.push_back({110, 0, "PTOP", "110,1,2.356194490192345,0,1,3.9269908169872414,0;"});
    entities.push_back({110, 0, "PLEFT", "110,1,2.356194490192345,0,0,2.356194490192345,0;"});
    entities[5].parameters = "141,1,1,1,4,3,1,1,15,5,1,1,17,7,2,1,19,9,1,1,21;";
    const auto seam_gap = brlcad::iges::Document::parse_buffer(sample(entities,
	"a seam-crossing periodic loop must not be assembled using NURBS extrapolation"));
    return expect(seam_gap.valid() && run_surface_import(seam_gap, options,
	result, face, nullptr) && !result.success && !face.written &&
	has_diagnostic(result, "trimmed_surface_loop"),
	"periodic parameter gap was allowed to unwrap into NURBS extrapolation") && passed;
}

bool
test_relative_revolution_parameters()
{
    const std::vector<Entity> entities = {
	{110, 0, "AXIS", "110,0,0,0,1,0,0;"},
	{100, 0, "ARC", "100,0,0,0,0,1,-1,0;"},
	{120, 0, "SURFACE", "120,1,3,0,1.5707963267948966;"},
	{110, 0, "PBOTTOM", "110,0,0,0,1.5707963267948966,0,0;"},
	{110, 0, "PPOLE", "110,1.5707963267948966,0,0,1.5707963267948966,1.5707963267948966,0;"},
	{110, 0, "PTOP", "110,1.5707963267948966,1.5707963267948966,0,0,1.5707963267948966,0;"},
	{110, 0, "PLEFT", "110,0,1.5707963267948966,0,0,0,0;"},
	{100, 0, "MBOTTOM", "100,0,0,0,0,1,-1,0;"},
	{126, 0, "MTOP", "126,2,2,1,0,0,0,0,0,0,1,1,1,1,0.7071067811865476,1,"
	    "-1,0,0,-1,0,1,0,0,1,0,1,0,1,0;"},
	{126, 0, "MLEFT", "126,2,2,1,0,0,0,0,0,0,1,1,1,1,0.7071067811865476,1,"
	    "0,0,1,0,1,1,0,1,0,0,1,1,0,0;"},
	{102, 0, "PARAM", "102,4,7,9,11,13;"},
	{102, 0, "MODEL", "102,3,15,17,19;"},
	{142, 0, "BOUNDARY", "142,1,5,21,23,1;"},
	{144, 0, "FACE", "144,5,1,0,25;"}
    };
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_buffer(sample(entities,
	    "explicit boundaries use a relative circular parameter"));
    brlcad::iges::ImportOptions options;
    options.exact = true;
    brlcad::iges::BrepImportResult result;
    ImportedFace face;
    const double curve_angle = SURFACE_SAMPLE_U * ON_PI / 2.0;
    const double rotation = SURFACE_SAMPLE_V * ON_PI / 2.0;
    const ON_3dPoint expected(-std::sin(curve_angle),
	std::cos(curve_angle) * std::cos(rotation),
	std::cos(curve_angle) * std::sin(rotation));
    if (!expect(document.valid() && run_surface_import(document, options,
	result, face, nullptr), "relative angular fixture could not be imported"))
	return false;
    bool passed = expect(result.success && face.valid,
	"relative angular boundary did not produce a valid face");
    passed = expect(face.surface_sample.DistanceTo(expected) < ON_ZERO_TOLERANCE,
	"relative angular surface has incorrect interior coordinates") && passed;
    passed = expect(face.singular_trims == 1 && result.statistics.repairs == 0,
	"relative angular boundary did not retain its exact singular trim") && passed;
    return expect(has_diagnostic(result, "relative_revolution_parameters"),
	"authored boundaries did not resolve the relative angular convention") && passed;
}

bool
test_ellipse_arc_near_knot()
{
    const std::vector<Entity> entities = {
	{128, 0, "SURFACE",
	    "128,1,1,1,1,0,0,1,0,0,-1,-1,1,1,-1,-1,1,1,1,1,1,1,"
	    "-1,-1,0,1,-1,0,-1,1,0,1,1,0,-1,1,-1,1;"},
	{104, 1, "ELLIPSE",
	    "104,0.1406250000001,0,1,0,0,-0.006400000000117,0,"
	    "-5.167053712025e-12,0.08000000000073,-0.02861267171391,0.07927718728889;"},
	{110, 0, "CHORD",
	    "110,-0.02861267171391,0.07927718728889,0,-5.167053712025e-12,0.08000000000073,0;"},
	{141, 0, "BOUNDARY", "141,1,1,1,2,3,1,1,3,5,1,1,5;"},
	{143, 0, "FACE", "143,1,1,1,7;"}
    };
    const auto document = brlcad::iges::Document::parse_buffer(sample(entities,
	"elliptical arc starting almost on a quadrant knot"));
    brlcad::iges::ImportOptions options;
    options.exact = true;
    brlcad::iges::BrepImportResult result;
    ImportedFace face;
    return expect(document.valid() && run_surface_import(document, options,
	result, face, nullptr) && result.success && face.valid && face.edges == 2 &&
	face.maximum_edge_tolerance <= 1.0e-6 && result.statistics.repairs == 0,
	"elliptical arc near a knot was lost or approximated");
}

bool
test_manifold_closed_edges()
{
    std::vector<Entity> entities = {
	{110, 0, "AXIS", "110,0,0,0,0,0,1;"},
	{110, 0, "LINE", "110,1,0,0,1,0,1;"},
	{120, 0, "SIDE", "120,1,3,0,6.283185307179586;"},
	{128, 0, "BOTTOM", "128,1,1,1,1,0,0,1,0,0,-1,-1,1,1,-1,-1,1,1,1,1,1,1,"
	    "-1,-1,0,1,-1,0,-1,1,0,1,1,0,-1,1,-1,1;"},
	{128, 0, "TOP", "128,1,1,1,1,0,0,1,0,0,-1,-1,1,1,-1,-1,1,1,1,1,1,1,"
	    "-1,-1,1,1,-1,1,-1,1,1,1,1,1,-1,1,-1,1;"},
	{502, 1, "VERTICES", "502,2,1,0,0,1,0,1;"},
	{100, 0, "BCIRCLE", "100,0,0,0,1,0,1,0;"},
	{100, 0, "TCIRCLE", "100,1,0,0,1,0,1,0;"},
	{110, 0, "SEAM", "110,1,0,0,1,0,1;"},
	{504, 1, "EDGES", "504,3,13,11,1,11,1,15,11,2,11,2,17,11,1,11,2;"},
	{110, 0, "PSEAM0", "110,0,0,0,1,0,0;"},
	{110, 0, "PTOP", "110,1,0,0,1,6.283185307179586,0;"},
	{110, 0, "PSEAM1", "110,1,6.283185307179586,0,0,6.283185307179586,0;"},
	{110, 0, "PBOTTOM", "110,0,6.283185307179586,0,0,0,0;"},
	{508, 1, "SIDELOOP", "508,4,0,19,3,1,1,1,21,0,19,2,1,1,1,23,"
	    "0,19,3,0,1,1,25,0,19,1,0,1,1,27;"},
	{508, 1, "BOTLOOP", "508,1,0,19,1,1,1,0,13;"},
	{508, 1, "TOPLOOP", "508,1,0,19,2,1,1,0,13;"},
	{510, 1, "SIDEFACE", "510,5,1,1,29;"},
	{510, 1, "BOTFACE", "510,7,1,1,31;"},
	{510, 1, "TOPFACE", "510,9,1,1,33;"},
	{514, 1, "SHELL", "514,3,35,0,37,0,39,1;"},
	{186, 0, "FACE", "186,41,1,0;"}
    };
    brlcad::iges::ImportOptions options;
    options.exact = true;
    brlcad::iges::BrepImportResult result;
    ImportedFace face;
    const auto cylinder = brlcad::iges::Document::parse_buffer(sample(entities,
	"manifold cylinder with closed circle edges"));
    bool passed = expect(cylinder.valid() && run_surface_import(cylinder, options,
	result, face, nullptr) && result.success && face.valid && face.solid &&
	face.edges == 3 && face.closed_edges == 2 && face.reversed_closed_trims == 1,
	"manifold import lost a closed edge or its use orientation");

    std::vector<Entity> incomplete = entities;
    incomplete[2] = {140, 0, "SIDE", "140,0,0,1,1,7;"};
    const auto missing_side = brlcad::iges::Document::parse_buffer(sample(incomplete,
	"explicit solid with an unsupported side surface"));
    options.exact = false;
    struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
    passed = expect(run_surface_import(missing_side, options, result, face, &attributes) &&
	result.success && face.written && face.valid && !face.solid && face.faces == 2 &&
	result.statistics.invalid_solids_written == 1 && result.statistics.unreconstructed_faces == 1 &&
	result.statistics.omitted == 0 &&
	BU_STR_EQUAL(bu_avs_get(&attributes, RT_BREP_INVALID_SOLID_ATTRIBUTE), "1") &&
	bu_avs_get(&attributes, "iges.shell_metadata") &&
	!bu_avs_get(&attributes, "plate_mode"),
	"failed manifold solid did not retain its caps and invalid-solid semantics") && passed;
    bu_avs_free(&attributes);
    passed = expect(run_import(missing_side, options, result, [](struct rt_wdb *wdbp) {
	struct directory *directory = db_lookup(wdbp->dbip, "FACE", LOOKUP_QUIET);
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	if (directory == RT_DIR_NULL || rt_db_get_internal(&internal, directory, wdbp->dbip, nullptr) < 0)
	    return false;
	struct bu_vls log = BU_VLS_INIT_ZERO;
	struct rt_i *trace = rt_i_create(wdbp->dbip);
	struct soltab solid = {};
	solid.st_dp = directory;
	const bool rejected = trace && !rt_brep_plate_mode(&internal) && !rt_brep_valid(&log, &internal, 0) &&
	    OBJ[ID_BREP].ft_prep(&solid, &internal, trace) < 0;
	if (trace)
	    rt_i_destroy(trace);
	bu_vls_free(&log);
	rt_db_free_internal(&internal);
	return rejected;
    }), "preserved invalid solid was accepted as valid geometry or a plate") && passed;
    options.strict = true;
    passed = expect(run_surface_import(missing_side, options, result, face, nullptr) &&
	!result.success && !face.written && result.statistics.omitted == 1,
	"strict mode preserved an incomplete explicit solid") && passed;
    options.strict = false;
    options.invalid_brep = brlcad::iges::InvalidBrepPolicy::Reject;
    passed = expect(run_surface_import(missing_side, options, result, face, nullptr) &&
	!result.success && !face.written, "reject policy preserved an incomplete solid") && passed;
    options.invalid_brep = brlcad::iges::InvalidBrepPolicy::Preserve;
    std::vector<Entity> missing_void = entities;
    missing_void.back().parameters = "186,41,1,1,45,0;";
    missing_void.push_back({514, 1, "VOID", "514,1,47,1;"});
    missing_void.push_back({510, 1, "VOIDFACE", "510,49,0,0;"});
    missing_void.push_back({140, 0, "VOIDSURF", "140,0,0,1,1,7;"});
    const auto void_failure = brlcad::iges::Document::parse_buffer(sample(missing_void,
	"closed outer shell with an unreconstructable void"));
    passed = expect(run_surface_import(void_failure, options, result, face, &attributes) &&
	result.success && face.written && face.faces == 3 && result.statistics.solid_breps_written == 0 &&
	result.statistics.invalid_solids_written == 1 &&
	BU_STR_EQUAL(bu_avs_get(&attributes, RT_BREP_INVALID_SOLID_ATTRIBUTE), "1"),
	"missing void shell incorrectly became a valid outer solid") && passed;
    bu_avs_free(&attributes);
    incomplete[3] = incomplete[2];
    incomplete[4] = incomplete[2];
    const auto empty_solid = brlcad::iges::Document::parse_buffer(sample(incomplete,
	"explicit solid with no reconstructable surfaces"));
    passed = expect(run_surface_import(empty_solid, options, result, face, nullptr) &&
	result.success && face.written && !face.valid && face.faces == 0 &&
	result.statistics.invalid_solids_written == 1 && result.statistics.unreconstructed_faces == 3,
	"unreconstructable solid identity did not survive database serialization") && passed;

    std::vector<Entity> rounded = entities;
    rounded[11].parameters = "110,1.00001,0,0,1,6.283185307179586,0;";
    const auto gapped = brlcad::iges::Document::parse_buffer(sample(rounded,
	"manifold parameter loop with a bounded endpoint gap"));
    options.exact = false;
    passed = expect(run_surface_import(gapped, options, result, face, nullptr) &&
	result.success && face.valid && face.solid && face.edges == 3 &&
	has_diagnostic(result, "closed_parameter_loop"),
	"bounded manifold parameter-loop gap was not repaired") && passed;
    options.exact = true;
    passed = expect(run_surface_import(gapped, options, result, face, nullptr) &&
	result.statistics.repairs == 0 && !has_diagnostic(result, "closed_parameter_loop"),
	"exact mode repaired a manifold parameter-loop gap") && passed;

    entities[14].parameters = "508,4,0,19,3,1,0,0,19,2,1,0,0,19,3,0,0,0,19,1,0,0;";
    entities[15].parameters = "508,1,0,19,1,1,0;";
    entities[16].parameters = "508,1,0,19,2,1,0;";
    const auto no_trims = brlcad::iges::Document::parse_buffer(sample(entities,
	"manifold cylinder without optional parameter curves"));
    options.exact = false;
    passed = expect(no_trims.valid() && run_surface_import(no_trims, options,
	result, face, nullptr) && result.success && face.valid && face.solid &&
	face.edges == 3 && has_diagnostic(result, "recovered_parameter_curve"),
	"missing manifold trims were not recovered from model geometry") && passed;
    options.exact = true;

    passed = expect(run_surface_import(no_trims, options, result, face, nullptr) &&
	!result.success && !face.written,
	"exact import recovered missing non-planar manifold trims") && passed;

    entities[3] = {190, 0, "BOTTOM", "190,45,49;"};
    entities[4] = {190, 0, "TOP", "190,47,49;"};
    entities.push_back({116, 0, "BORIGIN", "116,0,0,0;"});
    entities.push_back({116, 0, "TORIGIN", "116,0,0,1;"});
    entities.push_back({123, 0, "NORMAL", "123,0,0,1;"});
    const auto plane_caps = brlcad::iges::Document::parse_buffer(sample(entities,
	"closed circular edges bound analytic planar caps"));
    options.exact = false;
    passed = expect(plane_caps.valid() && run_surface_import(plane_caps, options,
	result, face, nullptr) && result.success && face.valid && face.solid &&
	face.edges == 3 && face.closed_edges == 2,
	"analytic plane caps lost their complete circular boundaries") && passed;

    entities.push_back({123, 0, "PREF", "123,0,1,0;"});
    entities.push_back({123, 0, "CREF", "123,1,0,0;"});
    entities.push_back({100, 0, "PCIRCLE", "100,0,0,0,0,-1,0,-1;"});
    entities[2] = {192, 1, "SIDE", "192,45,49,1,53;"};
    entities[3] = {190, 1, "BOTTOM", "190,45,49,51;"};
    entities[4] = {190, 1, "TOP", "190,47,49,51;"};
    entities[10].parameters = "110,0,0,0,0,1,0;";
    entities[11].parameters = "110,0,1,0,360,1,0;";
    entities[12].parameters = "110,360,1,0,360,0,0;";
    entities[13].parameters = "110,360,0,0,0,0,0;";
    entities[14].parameters = "508,4,0,19,3,1,1,1,21,0,19,2,1,1,1,23,"
	"0,19,3,0,1,1,25,0,19,1,0,1,1,27;";
    entities[15].parameters = "508,1,0,19,1,1,1,0,55;";
    entities[16].parameters = "508,1,0,19,2,1,1,0,55;";
    entities[17].parameters = "510,5,1,0,29;";
    entities[18].parameters = "510,7,1,0,31;";
    entities[19].parameters = "510,9,1,0,33;";
    const auto analytic = brlcad::iges::Document::parse_buffer(sample(entities,
	"analytic cylinder with degree parameters and unidentified outer loops"));
    options.exact = true;
    return expect(analytic.valid() && run_surface_import(analytic, options,
	result, face, nullptr) && result.success && face.valid && face.solid &&
	face.maximum_edge_tolerance <= 1.0e-6 && result.statistics.repairs == 0,
	"analytic cylinder, reference directions, or unidentified loops were not preserved") && passed;
}

bool
test_degenerate_revolution()
{
    const std::vector<Entity> entities = {
	{110, 0, "AXIS", "110,0,0,0,0,0,1;"},
	{110, 0, "POINT", "110,1,0,0,1,0,0;"},
	{120, 0, "FACE", "120,1,3,0,6.283185307179586;"}
    };
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_buffer(sample(entities,
	    "revolution with collapsed generating line"));
    brlcad::iges::ImportOptions options;
    brlcad::iges::BrepImportResult result;
    ImportedFace face;
    return expect(document.valid() && run_surface_import(document, options,
	result, face, nullptr) && !result.success && !face.written &&
	result.statistics.omitted == 1 &&
	has_diagnostic(result, "degenerate_revolution_generatrix"),
	"collapsed revolution was not explicitly rejected");
}


} /* namespace */

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    ON::Begin();
    bool passed = test_semantic_annotations();
    passed = test_import_progress() && passed;
    passed = test_post_recovery_assembly() && passed;
    passed = test_nested_instances() && passed;
    passed = test_bounded_surface_tolerance() && passed;
    passed = test_singular_boundary() && passed;
    passed = test_trim_loop_tolerance() && passed;
    passed = test_revolution_parameters() && passed;
    passed = test_natural_boundary() && passed;
    passed = test_bounded_plane() && passed;
    passed = test_small_boundary_pullback() && passed;
    passed = test_periodic_boundary_pullback() && passed;
    passed = test_relative_revolution_parameters() && passed;
    passed = test_manifold_closed_edges() && passed;
    passed = test_ellipse_arc_near_knot() && passed;
    passed = test_degenerate_revolution() && passed;
    ON::End();
    return passed ? 0 : 1;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
