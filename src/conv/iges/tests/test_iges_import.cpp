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
#include <cmath>
#include <cstdio>
#include <iomanip>
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
sample(const std::vector<Entity> &entities, const char *description)
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
    result += record("1H,,1H;;", 'G', 1);
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
run_surface_import(const brlcad::iges::Document &document,
    const brlcad::iges::ImportOptions &options,
    brlcad::iges::BrepImportResult &result, ImportedFace &face,
    struct bu_attribute_value_set *attributes)
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
    wdb_close(wdbp);
    bu_file_delete(path);
    return true;
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
has_diagnostic(const brlcad::iges::BrepImportResult &result, const char *code)
{
    return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
	[&](const brlcad::iges::ImportDiagnostic &diagnostic) {
	    return diagnostic.code == code;
	});
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
    return passed;
}

bool
test_trim_loop_tolerance()
{
    const std::vector<Entity> entities = {
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
    const std::vector<Entity> entities = {
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
    return expect(document.valid() && run_surface_import(document, options,
	result, face, nullptr) && result.success && face.valid && face.edges == 4 &&
	face.maximum_edge_tolerance <= 1.0e-6,
	"periodic pullback produced a trim outside the native surface domain");
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
    return expect(plane_caps.valid() && run_surface_import(plane_caps, options,
	result, face, nullptr) && result.success && face.valid && face.solid &&
	face.edges == 3 && face.closed_edges == 2,
	"analytic plane caps lost their complete circular boundaries") && passed;
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
    bool passed = test_semantic_annotations();
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
    passed = test_degenerate_revolution() && passed;
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
