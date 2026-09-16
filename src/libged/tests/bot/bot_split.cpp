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
#include <cstring>

#include "bu.h"
#include "ged.h"
#include "rt/geom.h"
#include "wdb.h"

static int failures = 0;

#define CHECK(_condition, _message) do { \
    if (!(_condition)) { \
	bu_log("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (_message)); \
	++failures; \
    } \
} while (0)


static int
write_source_bot(struct rt_wdb *wdbp, const char *name, unsigned char mode)
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
    unsigned char flags = RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS |
	RT_BOT_USE_FLOATS | RT_BOT_HAS_TEXTURE_UVS;
    int result = mk_bot_w_normals_and_uvs(wdbp, name, mode, RT_BOT_CCW,
	flags, 8, 4, vertices, faces, thickness, face_mode, 12, normals,
	face_normals, 12, uvs, face_uvs);
    bu_bitv_free(face_mode);
    return result;
}


static void
check_output_attributes(struct ged *gedp, struct directory *dp)
{
    struct bu_attribute_value_set attributes;
    bu_avs_init_empty(&attributes);
    CHECK(db5_get_attributes(gedp->dbip, &attributes, dp) == 0,
	"cannot read output BOT attributes");
    const char *attribute = bu_avs_get(&attributes, "split_test");
    CHECK(attribute && BU_STR_EQUAL(attribute, "preserved"),
	"output BOT object attributes were not preserved");
    bu_avs_free(&attributes);
}


static void
check_output_group(struct ged *gedp, const char *group_name,
	const char *first_member, const char *second_member)
{
    struct directory *dp = db_lookup(gedp->dbip, group_name, LOOKUP_QUIET);
    CHECK(dp != RT_DIR_NULL, "expected BOT split group does not exist");
    if (dp == RT_DIR_NULL)
	return;
    CHECK(dp->d_flags & RT_DIR_COMB,
	"BOT split group is not a combination");

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    CHECK(rt_db_get_internal(&internal, dp, gedp->dbip, NULL) >= 0,
	"cannot read BOT split group");
    if (!internal.idb_ptr)
	return;
    struct rt_comb_internal *comb =
	(struct rt_comb_internal *)internal.idb_ptr;
    CHECK(!comb->region_flag, "BOT split group is unexpectedly a region");
    CHECK(db_tree_nleaves(comb->tree) == 2,
	"BOT split group has the wrong member count");
    CHECK(db_find_named_leaf(comb->tree, first_member) != TREE_NULL,
	"BOT split group is missing its first member");
    CHECK(db_find_named_leaf(comb->tree, second_member) != TREE_NULL,
	"BOT split group is missing its second member");
    rt_db_free_internal(&internal);

    struct directory *first_dp = db_lookup(gedp->dbip, first_member,
	LOOKUP_QUIET);
    struct directory *second_dp = db_lookup(gedp->dbip, second_member,
	LOOKUP_QUIET);
    CHECK(first_dp != RT_DIR_NULL && first_dp->d_nref > 0,
	"first split BOT is still a top-level object");
    CHECK(second_dp != RT_DIR_NULL && second_dp->d_nref > 0,
	"second split BOT is still a top-level object");
}


static void
check_output_bot(struct ged *gedp, const char *name, unsigned char mode,
	size_t component)
{
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    CHECK(dp != RT_DIR_NULL, "expected split BOT does not exist");
    if (dp == RT_DIR_NULL)
	return;

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    CHECK(rt_db_get_internal(&internal, dp, gedp->dbip, NULL) >= 0,
	"cannot read split BOT");
    if (!internal.idb_ptr)
	return;
    struct rt_bot_internal *bot = (struct rt_bot_internal *)internal.idb_ptr;
    CHECK(bot->mode == mode, "serialized BOT mode was not preserved");
    CHECK(bot->orientation == RT_BOT_CCW,
	"serialized BOT orientation was not preserved");
    CHECK(bot->bot_flags == (RT_BOT_HAS_SURFACE_NORMALS |
	    RT_BOT_USE_NORMALS | RT_BOT_USE_FLOATS |
	    RT_BOT_HAS_TEXTURE_UVS),
	"serialized BOT flags were not preserved");
    CHECK(bot->num_faces == 2, "serialized BOT face count is wrong");
    CHECK(bot->num_vertices == 4, "serialized BOT vertex count is wrong");
    CHECK(bot->num_normals == 6 && bot->num_face_normals == 2,
	"serialized normal data is incomplete");
    CHECK(bot->num_uvs == 6 && bot->num_face_uvs == 2,
	"serialized UV data is incomplete");
    CHECK(bot->thickness != NULL, "serialized plate thickness is missing");
    CHECK(bot->face_mode != NULL, "serialized plate face mode is missing");
    if (bot->thickness) {
	CHECK(NEAR_EQUAL(bot->thickness[0], 0.5 + component * 2, SMALL_FASTF),
	    "first serialized thickness is wrong");
	CHECK(NEAR_EQUAL(bot->thickness[1], 1.5 + component * 2, SMALL_FASTF),
	    "second serialized thickness is wrong");
    }
    if (bot->face_mode) {
	CHECK(!!BU_BITTEST(bot->face_mode, 0) == (component == 0),
	    "first serialized face mode is wrong");
	CHECK(!!BU_BITTEST(bot->face_mode, 1) == (component == 1),
	    "second serialized face mode is wrong");
    }
    rt_db_free_internal(&internal);

    check_output_attributes(gedp, dp);
}


static void
check_decimated_bot(struct ged *gedp, const char *name)
{
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    CHECK(dp != RT_DIR_NULL, "expected decimated BOT does not exist");
    if (dp == RT_DIR_NULL)
	return;

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    CHECK(rt_db_get_internal(&internal, dp, gedp->dbip, NULL) >= 0,
	"cannot read decimated BOT");
    if (!internal.idb_ptr)
	return;
    struct rt_bot_internal *bot = (struct rt_bot_internal *)internal.idb_ptr;
    CHECK(bot->mode == RT_BOT_PLATE_NOCOS,
	"decimation did not preserve plate mode");
    CHECK(bot->orientation == RT_BOT_CCW,
	"decimation did not preserve orientation");
    CHECK(bot->bot_flags == (RT_BOT_HAS_SURFACE_NORMALS |
	    RT_BOT_USE_NORMALS | RT_BOT_USE_FLOATS |
	    RT_BOT_HAS_TEXTURE_UVS),
	"decimation did not preserve BOT flags");
    CHECK(bot->num_faces == 4 && bot->num_vertices == 8,
	"decimation changed the no-op test mesh size");
    CHECK(bot->num_normals == 12 && bot->num_face_normals == 4,
	"decimation did not preserve normals");
    CHECK(bot->num_uvs == 12 && bot->num_face_uvs == 4,
	"decimation did not preserve UVs");
    CHECK(bot->thickness && bot->face_mode,
	"decimation did not preserve plate face data");
    if (bot->thickness) {
	for (size_t i = 0; i < 4; ++i)
	    CHECK(NEAR_EQUAL(bot->thickness[i], 0.5 + i, SMALL_FASTF),
		"decimation changed plate thickness");
    }
    if (bot->face_mode) {
	CHECK(BU_BITTEST(bot->face_mode, 0) &&
		BU_BITTEST(bot->face_mode, 3),
	    "decimation changed plate face mode");
    }
    rt_db_free_internal(&internal);

    check_output_attributes(gedp, dp);
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 2)
	bu_exit(EXIT_FAILURE, "%s output.g\n", argv[0]);

    struct rt_wdb *wdbp = wdb_fopen(argv[1]);
    if (!wdbp)
	bu_exit(EXIT_FAILURE, "Cannot create %s\n", argv[1]);
    CHECK(write_source_bot(wdbp, "plate.bot", RT_BOT_PLATE_NOCOS) == 0,
	"failed to create bot split source");
    CHECK(write_source_bot(wdbp, "default.bot", RT_BOT_PLATE_NOCOS) == 0,
	"failed to create default-group bot split source");
    CHECK(write_source_bot(wdbp, "explicit.bot", RT_BOT_PLATE_NOCOS) == 0,
	"failed to create explicit-group bot split source");
    CHECK(write_source_bot(wdbp, "conflict.bot", RT_BOT_PLATE_NOCOS) == 0,
	"failed to create conflicting-group bot split source");
    CHECK(write_source_bot(wdbp, "legacy.bot", RT_BOT_PLATE) == 0,
	"failed to create bot_split source");
    point_t center = VINIT_ZERO;
    CHECK(mk_sph(wdbp, "plate.bot.0", center, 1.0) == 0,
	"failed to create output-name collision");
    CHECK(mk_sph(wdbp, "plate.bot_bots", center, 1.0) == 0,
	"failed to create default group-name collision");
    CHECK(mk_sph(wdbp, "plate.bot_bots1", center, 1.0) == 0,
	"failed to create incremented group-name collision");
    CHECK(mk_sph(wdbp, "taken.group", center, 1.0) == 0,
	"failed to create explicit group-name collision");
    wdb_close(wdbp);

    struct ged *gedp = ged_open("db", argv[1], 1);
    if (!gedp)
	bu_exit(EXIT_FAILURE, "Cannot open %s\n", argv[1]);

    CHECK(db5_update_attribute("plate.bot", "split_test", "preserved",
	    gedp->dbip) == 0,
	"failed to set bot split source attribute");
    CHECK(db5_update_attribute("default.bot", "split_test", "preserved",
	    gedp->dbip) == 0,
	"failed to set default-group source attribute");
    CHECK(db5_update_attribute("explicit.bot", "split_test", "preserved",
	    gedp->dbip) == 0,
	"failed to set explicit-group source attribute");
    CHECK(db5_update_attribute("legacy.bot", "split_test", "preserved",
	    gedp->dbip) == 0,
	"failed to set bot_split source attribute");

    const char *help_argv[] = {"bot", "split", "-h"};
    CHECK(ged_exec_bot(gedp, 3, help_argv) == GED_HELP,
	"bot split -h did not report help");
    CHECK(strstr(bu_vls_cstr(gedp->ged_result_str), "--grp") != NULL,
	"bot split help does not describe --grp");

    const char *default_argv[] = {"bot", "split", "default.bot"};
    CHECK(ged_exec_bot(gedp, 3, default_argv) == BRLCAD_OK,
	"bot split default grouping failed");
    check_output_bot(gedp, "default.bot.0", RT_BOT_PLATE_NOCOS, 0);
    check_output_bot(gedp, "default.bot.1", RT_BOT_PLATE_NOCOS, 1);
    check_output_group(gedp, "default.bot_bots", "default.bot.0",
	"default.bot.1");

    const char *subcommand_argv[] = {"bot", "split", "plate.bot"};
    CHECK(ged_exec_bot(gedp, 3, subcommand_argv) == BRLCAD_OK,
	"bot split command failed");
    CHECK(db_lookup(gedp->dbip, "plate.bot.0", LOOKUP_QUIET) != RT_DIR_NULL,
	"existing colliding object was overwritten");
    check_output_bot(gedp, "plate.bot.1", RT_BOT_PLATE_NOCOS, 0);
    check_output_bot(gedp, "plate.bot.2", RT_BOT_PLATE_NOCOS, 1);
    check_output_group(gedp, "plate.bot_bots2", "plate.bot.1",
	"plate.bot.2");

    const char *explicit_argv[] = {
	"bot", "split", "--grp", "explicit.bot.0", "explicit.bot"
    };
    CHECK(ged_exec_bot(gedp, 5, explicit_argv) == BRLCAD_OK,
	"bot split explicit grouping failed");
    check_output_bot(gedp, "explicit.bot.1", RT_BOT_PLATE_NOCOS, 0);
    check_output_bot(gedp, "explicit.bot.2", RT_BOT_PLATE_NOCOS, 1);
    check_output_group(gedp, "explicit.bot.0", "explicit.bot.1",
	"explicit.bot.2");

    const char *conflict_argv[] = {
	"bot", "split", "--grp", "taken.group", "conflict.bot"
    };
    CHECK(ged_exec_bot(gedp, 5, conflict_argv) == BRLCAD_ERROR,
	"bot split accepted a conflicting explicit group name");
    CHECK(db_lookup(gedp->dbip, "conflict.bot.0",
	    LOOKUP_QUIET) == RT_DIR_NULL,
	"bot split wrote output despite an explicit group conflict");

    const char *decimate_argv[] = {
	"bot", "decimate", "-t", "0.01", "plate.bot", "plate.decimated"
    };
    CHECK(ged_exec_bot(gedp, 6, decimate_argv) == BRLCAD_OK,
	"metadata-preserving bot decimate command failed");
    check_decimated_bot(gedp, "plate.decimated");

    const char *gct_decimate_argv[] = {
	"bot", "decimate", "-f", "0.00001", "plate.bot", "plate.gct"
    };
    CHECK(ged_exec_bot(gedp, 6, gct_decimate_argv) == BRLCAD_OK,
	"metadata-preserving GCT bot decimate command failed");
    check_decimated_bot(gedp, "plate.gct");

    const char *old_decimate_argv[] = {
	"bot", "decimate", "-e", "0.00001", "plate.bot", "plate.old"
    };
    CHECK(ged_exec_bot(gedp, 6, old_decimate_argv) == BRLCAD_OK,
	"metadata-preserving legacy bot decimate method failed");
    check_decimated_bot(gedp, "plate.old");

    const char *legacy_argv[] = {"bot_split", "legacy.bot"};
    CHECK(ged_exec_bot_split(gedp, 2, legacy_argv) == BRLCAD_OK,
	"bot_split compatibility command failed");
    check_output_bot(gedp, "legacy.bot.0", RT_BOT_PLATE, 0);
    check_output_bot(gedp, "legacy.bot.1", RT_BOT_PLATE, 1);
    CHECK(db_lookup(gedp->dbip, "legacy.bot_bots",
	    LOOKUP_QUIET) == RT_DIR_NULL,
	"deprecated bot_split unexpectedly changed its output hierarchy");

    ged_close(gedp);
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
