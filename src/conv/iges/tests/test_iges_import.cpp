/*               T E S T _ I G E S _ I M P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include "../iges_import.h"

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
    data += field(0); /* transform */
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
sample()
{
    const std::vector<Entity> entities = {
	{110, 0, "LINE", "110,0,0,0,10,0,0;"},
	{212, 0, "NOTE", "212,1,5,20,4,1,1.5707963267948966,0,0,0,2,3,0,5HHELLO;"},
	{214, 1, "LEADER", "214,2,2,1,0,0,0,5,0,10,5;"},
	{210, 0, "DIM", "210,1,3,5;"},
	{116, 0, "POINT", "116,1,2,3;"},
	{110, 0, "A(B", "110,0,0,0,0,1,0;"},
	{110, 0, "A[B", "110,0,0,0,0,2,0;"}
    };
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
    result += record("semantic annotation test", 'S', 1);
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
	brlcad::iges::Document::parse_buffer(sample(), "annotation.iges");
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
	expect(result.statistics.semantic_groups_written == 1,
	    "semantic dimension group was not written") &&
	expect(db_lookup(wdbp->dbip, "drawing", LOOKUP_QUIET) != RT_DIR_NULL,
	    "semantic drawing root was not written") &&
	expect(db_lookup(wdbp->dbip, "DIM.annot_group", LOOKUP_QUIET) != RT_DIR_NULL,
	    "semantic dimension group name is missing") &&
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

    wdb_close(wdbp);
    bu_file_delete(path);
    return passed;
}

} /* namespace */

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    return test_semantic_annotations() ? 0 : 1;
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
