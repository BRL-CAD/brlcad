/*                     B O T _ S P L I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "common.h"

#include <cstdlib>

#include "bu/app.h"
#include "bu/bitv.h"
#include "bu/log.h"
#include "rt/geom.h"
#include "rt/primitives/bot.h"
#include "../primitives/bot/decimate_private.h"

static int failures = 0;

#define CHECK(_condition, _message) do { \
    if (!(_condition)) { \
	bu_log("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (_message)); \
	++failures; \
    } \
} while (0)


static void
check_component(const struct rt_bot_internal *component,
	const struct rt_bot_internal *source, size_t component_index)
{
    size_t first_face = component_index * 2;
    size_t first_vertex = component_index * 4;
    size_t first_attribute = component_index * 6;
    int expected_faces[] = {0, 1, 2, 0, 2, 3};

    CHECK(component->mode == source->mode, "mode was not preserved");
    CHECK(component->orientation == source->orientation,
	"orientation was not preserved");
    CHECK(component->bot_flags == source->bot_flags,
	"BOT flags were not preserved");
    CHECK(component->num_faces == 2, "unexpected component face count");
    CHECK(component->num_vertices == 4, "unexpected compact vertex count");
    for (size_t i = 0; i < 6; ++i)
	CHECK(component->faces[i] == expected_faces[i],
	    "face vertex index was not remapped correctly");
    for (size_t i = 0; i < 4; ++i)
	CHECK(VEQUAL(&component->vertices[i * 3],
		&source->vertices[(first_vertex + i) * 3]),
	    "vertex data was not preserved");

    if (source->mode == RT_BOT_PLATE || source->mode == RT_BOT_PLATE_NOCOS) {
	CHECK(component->thickness != NULL, "plate thickness is missing");
	CHECK(component->face_mode != NULL, "plate face mode is missing");
	for (size_t i = 0; i < 2; ++i)
	    CHECK(NEAR_EQUAL(component->thickness[i],
		    source->thickness[first_face + i], SMALL_FASTF),
		"plate thickness was not preserved");
	CHECK(!!BU_BITTEST(component->face_mode, 0) ==
		!!BU_BITTEST(source->face_mode, first_face),
	    "first plate face mode was not preserved");
	CHECK(!!BU_BITTEST(component->face_mode, 1) ==
		!!BU_BITTEST(source->face_mode, first_face + 1),
	    "second plate face mode was not preserved");
    } else {
	CHECK(component->thickness == NULL, "non-plate BOT gained thickness");
	CHECK(component->face_mode == NULL, "non-plate BOT gained face mode");
    }

    CHECK(component->num_normals == 6, "unexpected compact normal count");
    CHECK(component->num_face_normals == 2,
	"unexpected face-normal count");
    CHECK(component->num_uvs == 6, "unexpected compact UV count");
    CHECK(component->num_face_uvs == 2, "unexpected face-UV count");
    for (size_t i = 0; i < 6; ++i) {
	CHECK(component->face_normals[i] == (int)i,
	    "face-normal index was not remapped correctly");
	CHECK(component->face_uvs[i] == (int)i,
	    "face-UV index was not remapped correctly");
	CHECK(VEQUAL(&component->normals[i * 3],
		&source->normals[(first_attribute + i) * 3]),
	    "normal data was not preserved");
	CHECK(VEQUAL(&component->uvs[i * 3],
		&source->uvs[(first_attribute + i) * 3]),
	    "UV data was not preserved");
    }
}


static void
test_mode(unsigned char mode)
{
    fastf_t vertices[] = {
	0, 0, 0,  1, 0, 0,  1, 1, 0,  0, 1, 0,
	2, 0, 0,  3, 0, 0,  3, 1, 0,  2, 1, 0
    };
    int faces[] = {
	0, 1, 2,  0, 2, 3,
	4, 5, 6,  4, 6, 7
    };
    fastf_t thickness[] = {0.5, 1.5, 2.5, 3.5};
    fastf_t normals[36];
    fastf_t uvs[36];
    int face_normals[12];
    int face_uvs[12];
    for (int i = 0; i < 12; ++i) {
	VSET(&normals[i * 3], i + 0.1, i + 0.2, i + 0.3);
	VSET(&uvs[i * 3], i + 0.4, i + 0.5, i + 0.6);
	face_normals[i] = i;
	face_uvs[i] = i;
    }

    struct bu_bitv *face_mode = bu_bitv_new(4);
    BU_BITSET(face_mode, 0);
    BU_BITSET(face_mode, 3);

    struct rt_bot_internal source = {};
    source.magic = RT_BOT_INTERNAL_MAGIC;
    source.mode = mode;
    source.orientation = RT_BOT_CCW;
    source.bot_flags = RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS |
	RT_BOT_USE_FLOATS | RT_BOT_HAS_TEXTURE_UVS;
    source.num_faces = 4;
    source.faces = faces;
    source.num_vertices = 8;
    source.vertices = vertices;
    if (mode == RT_BOT_PLATE || mode == RT_BOT_PLATE_NOCOS) {
	source.thickness = thickness;
	source.face_mode = face_mode;
    }
    source.num_normals = 12;
    source.normals = normals;
    source.num_face_normals = 4;
    source.face_normals = face_normals;
    source.num_uvs = 12;
    source.uvs = uvs;
    source.num_face_uvs = 4;
    source.face_uvs = face_uvs;

    struct rt_bot_list *components = rt_bot_split(&source);
    CHECK(components != NULL, "rt_bot_split failed");
    if (components) {
	size_t component_count = 0;
	struct rt_bot_list *entry;
	for (BU_LIST_FOR(entry, rt_bot_list, &components->l)) {
	    if (component_count < 2)
		check_component(entry->bot, &source, component_count);
	    ++component_count;
	}
	CHECK(component_count == 2, "unexpected number of BOT components");
	rt_bot_list_free(components, 1);
    }

    int selected_faces[] = {2, 3};
    struct rt_bot_internal *subset = rt_bot_subset(&source,
	selected_faces, 2);
    CHECK(subset != NULL, "metadata-aware BOT subset failed");
    if (subset) {
	check_component(subset, &source, 1);
	rt_bot_internal_free(subset);
	BU_PUT(subset, struct rt_bot_internal);
    }

    int replacement_faces[] = {4, 5, 6, 4, 6, 5};
    struct rt_bot_internal *garbage_collected = rt_bot_gc(&source,
	replacement_faces, selected_faces, 2);
    CHECK(garbage_collected != NULL, "metadata-aware BOT GC failed");
    if (garbage_collected) {
	int expected_faces[] = {0, 1, 2, 0, 2, 1};
	CHECK(garbage_collected->num_faces == 2 &&
		garbage_collected->num_vertices == 3,
	    "BOT GC did not compact replacement geometry");
	for (size_t i = 0; i < 6; ++i)
	    CHECK(garbage_collected->faces[i] == expected_faces[i],
		"BOT GC replacement face mapping is wrong");
	CHECK(garbage_collected->bot_flags == source.bot_flags,
	    "BOT GC flags were not preserved");
	CHECK(garbage_collected->num_normals == 6 &&
		garbage_collected->num_uvs == 6,
	    "BOT GC indexed data was not preserved");
	if (mode == RT_BOT_PLATE || mode == RT_BOT_PLATE_NOCOS) {
	    CHECK(NEAR_EQUAL(garbage_collected->thickness[0], thickness[2],
		    SMALL_FASTF) &&
		    NEAR_EQUAL(garbage_collected->thickness[1], thickness[3],
		    SMALL_FASTF),
		"BOT GC thickness provenance is wrong");
	}
	rt_bot_internal_free(garbage_collected);
	BU_PUT(garbage_collected, struct rt_bot_internal);
    }

    struct rt_bot_list *patches = rt_bot_patches(&source);
    CHECK(patches != NULL, "BOT patch extraction failed");
    if (patches) {
	size_t patch_count = 0;
	struct rt_bot_list *patch_entry;
	for (BU_LIST_FOR(patch_entry, rt_bot_list, &patches->l)) {
	    ++patch_count;
	    CHECK(patch_entry->bot->mode == source.mode,
		"BOT patch mode was not preserved");
	    CHECK(patch_entry->bot->bot_flags == source.bot_flags,
		"BOT patch flags were not preserved");
	    CHECK(patch_entry->bot->num_faces == source.num_faces,
		"BOT patch face count is wrong");
	    CHECK(patch_entry->bot->num_normals == source.num_normals &&
		    patch_entry->bot->num_uvs == source.num_uvs,
		"BOT patch indexed data was not preserved");
	    if (mode == RT_BOT_PLATE || mode == RT_BOT_PLATE_NOCOS) {
		CHECK(patch_entry->bot->thickness != NULL,
		    "BOT patch plate thickness is missing");
		CHECK(patch_entry->bot->face_mode != NULL,
		    "BOT patch plate face mode is missing");
	    }
	}
	CHECK(patch_count == 1, "coplanar BOT produced multiple patches");
	rt_bot_list_free(patches, 1);
    }
    bu_bitv_free(face_mode);
}


static void
test_connected_and_invalid_plate()
{
    fastf_t vertices[] = {
	0, 0, 0,  1, 0, 0,  1, 1, 0,  0, 1, 0,
	2, 0, 0,  3, 0, 0,  2, 1, 0
    };
    int connected_faces[] = {0, 1, 2, 0, 2, 3};
    struct rt_bot_internal bot = {};
    bot.magic = RT_BOT_INTERNAL_MAGIC;
    bot.mode = RT_BOT_SURFACE;
    bot.orientation = RT_BOT_UNORIENTED;
    bot.num_faces = 2;
    bot.faces = connected_faces;
    bot.num_vertices = 4;
    bot.vertices = vertices;

    struct rt_bot_list *components = rt_bot_split(&bot);
    CHECK(components != NULL, "connected BOT split failed");
    if (components) {
	CHECK(BU_LIST_IS_EMPTY(&components->l),
	    "connected BOT unexpectedly produced output components");
	rt_bot_list_free(components, 1);
    }

    int disconnected_faces[] = {0, 1, 2, 4, 5, 6};
    bot.mode = RT_BOT_PLATE_NOCOS;
    bot.num_faces = 2;
    bot.faces = disconnected_faces;
    bot.num_vertices = 7;
    bot.thickness = NULL;
    components = rt_bot_split(&bot);
    CHECK(components == NULL,
	"invalid plate BOT should fail instead of producing corrupt output");

    struct rt_bot_repair_info repair_info = RT_BOT_REPAIR_INFO_INIT;
    struct rt_bot_internal *repaired = NULL;
    CHECK(rt_bot_repair(&repaired, &bot, &repair_info) == -1 &&
	    repaired == NULL,
	"solid-mesh repair should reject plate BOTs rather than lose plate data");
}


static void
test_repair_data_preservation()
{
    fastf_t vertices[] = {
	0, 0, 0,
	1, 0, 0,
	0, 1, 0,
	0, 0, 1,
	0, 0, 0
    };
    int faces[] = {
	0, 2, 1,
	4, 1, 3,
	1, 2, 3,
	2, 0, 3
    };
    fastf_t normals[36];
    fastf_t uvs[36];
    int face_normals[12];
    int face_uvs[12];
    for (int i = 0; i < 12; ++i) {
	VSET(&normals[i * 3], i + 0.1, i + 0.2, i + 0.3);
	VSET(&uvs[i * 3], i + 0.4, i + 0.5, i + 0.6);
	face_normals[i] = i;
	face_uvs[i] = i;
    }
    struct rt_bot_internal bot = {};
    bot.magic = RT_BOT_INTERNAL_MAGIC;
    bot.mode = RT_BOT_SOLID;
    bot.orientation = RT_BOT_CCW;
    bot.bot_flags = RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_HAS_TEXTURE_UVS;
    bot.num_faces = 4;
    bot.faces = faces;
    bot.num_vertices = 5;
    bot.vertices = vertices;
    bot.num_normals = 12;
    bot.normals = normals;
    bot.num_face_normals = 4;
    bot.face_normals = face_normals;
    bot.num_uvs = 12;
    bot.uvs = uvs;
    bot.num_face_uvs = 4;
    bot.face_uvs = face_uvs;

    struct rt_bot_repair_info repair_info = RT_BOT_REPAIR_INFO_INIT;
    struct rt_bot_internal *repaired = NULL;
    int result = rt_bot_repair(&repaired, &bot, &repair_info);
    CHECK(result == 0 && repaired != NULL,
	"repair did not merge a duplicated solid vertex");
    CHECK(repair_info.output_data_loss == 0,
	"merge-only repair incorrectly reported data loss");
    if (repaired) {
	CHECK(repaired->bot_flags == bot.bot_flags,
	    "merge-only repair did not preserve BOT flags");
	CHECK(repaired->num_face_normals == bot.num_face_normals &&
		repaired->num_face_uvs == bot.num_face_uvs,
	    "merge-only repair did not preserve indexed face data");
	rt_bot_internal_free(repaired);
	BU_PUT(repaired, struct rt_bot_internal);
    }
}


static void
test_decimation_surface_validation()
{
    fastf_t source_vertices[] = {
	0.0, 0.0, 0.0,
	1.0, 0.0, 0.0,
	1.0, 1.0, 0.0,
	0.0, 1.0, 0.0
    };
    int source_faces[] = {0, 1, 2, 0, 2, 3};
    fastf_t candidate_vertices[] = {
	0.25, 0.25, 0.0,
	0.75, 0.25, 0.05,
	0.50, 0.75, 1.0
    };

    struct rt_bot_internal source = {};
    source.magic = RT_BOT_INTERNAL_MAGIC;
    source.num_faces = 2;
    source.faces = source_faces;
    source.num_vertices = 4;
    source.vertices = source_vertices;

    struct rt_bot_internal candidate = {};
    candidate.magic = RT_BOT_INTERNAL_MAGIC;
    candidate.num_vertices = 3;
    candidate.vertices = candidate_vertices;

    size_t offending_vertex = 99;
    int result = rt_bot_decimation_is_within_distance(&offending_vertex,
	&source, &candidate, 0.1);
    CHECK(result == 0 && offending_vertex == 2,
	"decimation surface validator missed an outlier");

    candidate.num_vertices = 2;
    result = rt_bot_decimation_is_within_distance(&offending_vertex,
	&source, &candidate, 0.1);
    CHECK(result == 1,
	"decimation surface validator rejected valid vertices");
}


int
main(int UNUSED(argc), char **argv)
{
    bu_setprogname(argv[0]);

    test_mode(RT_BOT_SURFACE);
    test_mode(RT_BOT_SOLID);
    test_mode(RT_BOT_PLATE);
    test_mode(RT_BOT_PLATE_NOCOS);
    test_connected_and_invalid_plate();
    test_repair_data_preservation();
    test_decimation_surface_validation();
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
