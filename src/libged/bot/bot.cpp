/*                         B O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/bot/bot.cpp
 *
 * The LIBGED bot command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "fort.h"
}

#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "bg/chull.h"
#include "bg/pca.h"
#include "bg/trimesh.h"
#include "rt/geom.h"
#include "wdb.h"

#include "./ged_bot.h"
#include "../ged_private.h"

// TODO - I think this may be the same for brep and bot, which suggests it should be
// a common libged utility function of some sort...
static int
_bot_face_specifiers(std::set<int> &elements, struct bu_vls *vls, int argc, const char **argv) {
    for (int i = 0; i < argc; i++) {
	std::string s1(argv[i]);
	size_t pos_dash = s1.find_first_of("-:", 0);
	size_t pos_comma = s1.find_first_of(",/;", 0);
	if (pos_dash != std::string::npos) {
	    // May have a range - find out
	    std::string s2 = s1.substr(0, pos_dash);
	    s1.erase(0, pos_dash + 1);
	    char *n1 = bu_strdup(s1.c_str());
	    char *n2 = bu_strdup(s2.c_str());
	    int val1, val2, vtmp;
	    if (bu_opt_int(NULL, 1, (const char **)&n1, &val1) < 0) {
		bu_vls_printf(vls, "Invalid index specification: %s\n", n1);
		bu_free(n1, "n1");
		bu_free(n2, "n2");
		return BRLCAD_ERROR;
	    }
	    if (bu_opt_int(NULL, 1, (const char **)&n2, &val2) < 0) {
		bu_vls_printf(vls, "Invalid index specification: %s\n", n2);
		bu_free(n1, "n1");
		bu_free(n2, "n2");
		return BRLCAD_ERROR;
	    }
	    bu_free(n1, "n1");
	    bu_free(n2, "n2");
	    if (val1 > val2) {
		vtmp = val2;
		val2 = val1;
		val1 = vtmp;
	    }
	    for (int j = val1; j <= val2; j++) {
		elements.insert(j);
	    }
	    continue;
	}
	if (pos_comma != std::string::npos) {
	    // May have a set - find out
	    while (pos_comma != std::string::npos) {
		std::string ss = s1.substr(0, pos_comma);
		char *n1 = bu_strdup(ss.c_str());
		int val1;
		if (bu_opt_int(NULL, 1, (const char **)&n1, &val1) < 0) {
		    bu_vls_printf(vls, "Invalid index specification: %s\n", n1);
		    bu_free(n1, "n1");
		    return BRLCAD_ERROR;
		} else {
		    elements.insert(val1);
		}
		s1.erase(0, pos_comma + 1);
		pos_comma = s1.find_first_of(",/;", 0);
	    }
	    if (s1.length()) {
		char *n1 = bu_strdup(s1.c_str());
		int val1;
		if (bu_opt_int(NULL, 1, (const char **)&n1, &val1) < 0) {
		    bu_vls_printf(vls, "Invalid index specification: %s\n", n1);
		    bu_free(n1, "n1");
		    return BRLCAD_ERROR;
		}
		elements.insert(val1);
	    }
	    continue;
	}

	// Nothing fancy looking - see if its a number
	int val = 0;
	if (bu_opt_int(NULL, 1, &argv[i], &val) >= 0) {
	    elements.insert(val);
	} else {
	    bu_vls_printf(vls, "Invalid index specification: %s\n", argv[i]);
	    return BRLCAD_ERROR;
	}
    }

    return BRLCAD_OK;
}


int
_bot_obj_setup(struct _ged_bot_info *gb, const char *name)
{
    gb->dp = db_lookup(gb->gedp->dbip, name, LOOKUP_NOISY);
    if (gb->dp == RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, ": %s is not a solid or does not exist in database", name);
	return BRLCAD_ERROR;
    } else {
	int real_flag = (gb->dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	if (!real_flag) {
	    /* solid doesn't exist */
	    bu_vls_printf(gb->gedp->ged_result_str, ": %s is not a real solid", name);
	    return BRLCAD_ERROR;
	}
    }

    gb->solid_name = std::string(name);

    BU_GET(gb->intern, struct rt_db_internal);

    GED_DB_GET_INTERNAL(gb->gedp, gb->intern, gb->dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(gb->intern);

    if (gb->intern->idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type bot\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


int
_bot_cmd_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", ps);
	return 1;
    }
    return 0;
}


extern "C" int
_bot_cmd_get(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot get <faces|minEdge|maxEdge|orientation|thickness|type|vertices> <objname>";
    const char *purpose_string = "Report specific information about a BoT shape";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct ged *gedp = gb->gedp;

    argc--; argv++;

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (_bot_obj_setup(gb, argv[1]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);

    /* Thickness is a special case, since we may need to report multiple values if we have a plate
     * mode bot with non-uniform thickness. */
    if (BU_STR_EQUAL(argv[0], "thickness")) {
	if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
	    bu_vls_printf(gedp->ged_result_str, "BoT is not plate mode - thicknesses can only be set on plate-mode BoTs");
	    return BRLCAD_ERROR;
	}

	// Check if our thickness is uniform across the bot.  If it is, return
	// one number.  Otherwise print each face's thickness.  (TODO - the
	// latter would probably be more readable if we printed ranges of face
	// indexes with the same thickness, but deferring doing that until we
	// have a use case that needs it.  (The which command has printing of
	// ranges, so it might be a useful reference.)
	fastf_t seed = bot->thickness[0];
	int uniform = 1;
	for (size_t i = 1; i < bot->num_faces; i++) {
	    if (!NEAR_EQUAL(seed, bot->thickness[i], 0.1*BN_TOL_DIST)) {
		uniform = 0;
		break;
	    }
	}

	if (uniform) {
	    bu_vls_printf(gedp->ged_result_str, "%g", seed);
	    return BRLCAD_OK;
	} else {
	    for (size_t i = 0; i < bot->num_faces; i++) {
		bu_vls_printf(gedp->ged_result_str, "%zd:%g\n", i, bot->thickness[i]);
	    }
	    return BRLCAD_OK;
	}
    }


    fastf_t propVal = rt_bot_propget(bot, argv[0]);

    /* print result string */
    if (!EQUAL(propVal, -1.0)) {
	int intprop = (int) propVal;
	fastf_t tmp = (int) propVal;

	if (EQUAL(propVal, tmp)) {
	    /* int result */
	    /* for orientation and mode, print something more informative than just
	     * the number */
	    if (BU_STR_EQUAL(argv[0], "orientation")) {
		switch (intprop) {
		    case RT_BOT_UNORIENTED:
			bu_vls_printf(gedp->ged_result_str, "none");
			break;
		    case RT_BOT_CCW:
			bu_vls_printf(gedp->ged_result_str, "ccw");
			break;
		    case RT_BOT_CW:
			bu_vls_printf(gedp->ged_result_str, "cw");
			break;
		}
	    } else if (BU_STR_EQUAL(argv[0], "type") || BU_STR_EQUAL(argv[0], "mode")) {
		switch (intprop) {
		    case RT_BOT_SURFACE:
			bu_vls_printf(gedp->ged_result_str, "surface");
			break;
		    case RT_BOT_SOLID:
			bu_vls_printf(gedp->ged_result_str, "solid");
			break;
		    case RT_BOT_PLATE:
			bu_vls_printf(gedp->ged_result_str, "plate");
			break;
		    case RT_BOT_PLATE_NOCOS:
			bu_vls_printf(gedp->ged_result_str, "plate_nocos");
			break;
		}
	    } else {
		bu_vls_printf(gedp->ged_result_str, "%d", (int) propVal);
	    }
	} else {
	    /* float result */
	    bu_vls_printf(gedp->ged_result_str, "%f", propVal);
	}
    } else {
	bu_vls_printf(gedp->ged_result_str, "%s is not a valid argument!", argv[1]);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


extern "C" int
_bot_cmd_set(void *bs, int argc, const char **argv)
{
    bool processed = false;
    const char *usage_string = "bot set <orientation|type> <objname> <val>\nbot set thickness <objname> <#|#:#>";
    const char *purpose_string = "Set BoT object properties";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct ged *gedp = gb->gedp;

    argc--; argv++;

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (_bot_obj_setup(gb, argv[1]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "orientation")) {

	processed = true;

	int mode = INT_MAX;
	struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
	if (BU_STR_EQUIV(argv[2], "none") || BU_STR_EQUIV(argv[2], "unoriented") || BU_STR_EQUIV(argv[2], "no")) {
	    mode = RT_BOT_UNORIENTED;
	}
	if (BU_STR_EQUIV(argv[2], "ccw") || BU_STR_EQUIV(argv[2], "counterclockwise") || BU_STR_EQUIV(argv[2], "rh")) {
	    mode = RT_BOT_CCW;
	}
	if (BU_STR_EQUIV(argv[2], "cw") || BU_STR_EQUIV(argv[2], "clockwise") || BU_STR_EQUIV(argv[2], "lh")) {
	    mode = RT_BOT_CW;
	}
	if (mode == INT_MAX) {
	    bu_vls_printf(gedp->ged_result_str, "Possible orientations are: none (no), ccw (rh), and cw (lh)");
	    return BRLCAD_ERROR;
	}
	bot->orientation = mode;
    }

    if (BU_STR_EQUAL(argv[0], "type") || BU_STR_EQUAL(argv[0], "mode")) {

	processed = true;

	int mode = INT_MAX;
	struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
	if (BU_STR_EQUIV(argv[2], "surface") || BU_STR_EQUIV(argv[2], "surf")) {
	    mode = RT_BOT_SURFACE;
	}
	if (BU_STR_EQUIV(argv[2], "solid") || BU_STR_EQUIV(argv[2], "sol")) {
	    mode = RT_BOT_SOLID;
	}
	if (BU_STR_EQUIV(argv[2], "plate")) {
	    mode = RT_BOT_PLATE;
	}
	if (BU_STR_EQUIV(argv[2], "plate_nocos")) {
	    mode = RT_BOT_PLATE_NOCOS;
	}
	if (mode == INT_MAX) {
	    bu_vls_printf(gedp->ged_result_str, "Possible types are: surface solid plate plate_nocos");
	    return BRLCAD_ERROR;
	}
	int old_mode = bot->mode;
	bot->mode = mode;
	if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	    if (old_mode != RT_BOT_PLATE && old_mode != RT_BOT_PLATE_NOCOS) {
		/* need to create some thicknesses */
		bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t), "BOT thickness");
		bot->face_mode = bu_bitv_new(bot->num_faces);
	    }
	} else {
	    if (old_mode == RT_BOT_PLATE || old_mode == RT_BOT_PLATE_NOCOS) {
		/* free the per face memory */
		bu_free((char *)bot->thickness, "BOT thickness");
		bot->thickness = (fastf_t *)NULL;
		bu_free((char *)bot->face_mode, "BOT face_mode");
		bot->face_mode = (struct bu_bitv *)NULL;
	    }
	}
    }

    if (BU_STR_EQUAL(argv[0], "thickness")) {

	processed = true;

	struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
	if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
	    bu_vls_printf(gedp->ged_result_str, "BoT is not plate mode - thickness can only be set on plate-mode BoTs");
	    return BRLCAD_ERROR;
	}

	// See which argument type we have
	char *targ = bu_strdup(argv[2]);
	char *c = strchr(targ, ':');
	int face_id = 0;
	fastf_t thickness = 0;
	if (c) {
	    // We have a colon separated argument, which should be a face index and thickness value.
	    // Split the string accordingly
	    char *face_id_str = targ;
	    *c = '\0';
	    c++;
	    char *thickness_str = c;

	    if (bu_opt_int(NULL, 1, (const char **)&face_id_str, &face_id) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Invalid face index specification: %s\n", face_id_str);
		bu_free(targ, "targ");
		return BRLCAD_ERROR;
	    }
	    if (bu_opt_fastf_t(NULL, 1, (const char **)&thickness_str, &thickness) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Invalid index specification: %s\n", thickness_str);
		bu_free(targ, "targ");
		return BRLCAD_ERROR;
	    }
	    bot->thickness[face_id] = thickness;
	} else {
	    if (bu_opt_fastf_t(NULL, 1, (const char **)&targ, &thickness) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Invalid thickness specification: %s\n", targ);
		bu_free(targ, "targ");
		return BRLCAD_ERROR;
	    }
	    for (size_t i = 0; i < bot->num_faces; i++)
		bot->thickness[i] = thickness;
	}
	bu_free(targ, "targ");
    }

    // If we didn't have a subcommand that we knew how to handle, report an error
    if (!processed) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(gb->dp, gedp->dbip, gb->intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Failed to update BoT");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


extern "C" int
_bot_cmd_chull(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] chull <objname> [output_bot]";
    const char *purpose_string = "Generate the BoT's convex hull and store it in an object";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    if (!argc) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n%s\n", usage_string, purpose_string);
	return BRLCAD_ERROR;
    }


    if (_bot_obj_setup(gb, argv[0]) == BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    int retval = 0;
    int fc = 0;
    int vc = 0;
    point_t *vert_array;
    int *faces;
    unsigned char err = 0;

    retval = bg_3d_chull(&faces, &fc, &vert_array, &vc, (const point_t *)bot->vertices, (int)bot->num_vertices);

    if (retval != 3) {
	return BRLCAD_ERROR;
    }

    struct bu_vls out_name = BU_VLS_INIT_ZERO;
    if (argc > 1) {
	bu_vls_sprintf(&out_name, "%s", argv[1]);
    } else {
	bu_vls_sprintf(&out_name, "%s.hull", gb->dp->d_namep);
    }

    if (db_lookup(gb->gedp->dbip, bu_vls_cstr(&out_name), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, "Object %s already exists!\n", bu_vls_cstr(&out_name));
	bu_vls_free(&out_name);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    retval = mk_bot(wdbp, bu_vls_cstr(&out_name), RT_BOT_SOLID, RT_BOT_CCW, err, vc, fc, (fastf_t *)vert_array, faces, NULL, NULL);

    bu_vls_free(&out_name);
    bu_free(faces, "free faces");
    bu_free(vert_array, "free verts");

    if (retval) {
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


extern "C" int
_bot_cmd_flip(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot flip [-t] <objname>";
    const char *purpose_string = "Flip BoT triangle normal directions (turns the BoT \"inside out\")";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    // See if we have any high level options set
    int test_flipped = 0;
    struct bu_opt_desc d[2];
    BU_OPT(d[0], "t", "test",    "",      NULL,                 &test_flipped,         "Test if the specified bot is inside-out");
    BU_OPT_NULL(d[1]);

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (argc != 1) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);

    if (test_flipped) {
	int ftest = rt_bot_inside_out(bot);
	if (ftest < 0)
	    return BRLCAD_ERROR;
	if (!ftest) {
	    bu_vls_printf(gb->gedp->ged_result_str, "OK");
	} else {
	    bu_vls_printf(gb->gedp->ged_result_str, "BoT is inside out");
	}
	return BRLCAD_OK;
    }

    rt_bot_flip(bot);

    if (rt_db_put_internal(gb->dp, gb->gedp->dbip, gb->intern, &rt_uniresource) < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to update BoT");
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gb->gedp->ged_result_str, "BoT faces flipped");
    return BRLCAD_OK;
}

extern "C" int
_bot_cmd_isect(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] isect <objname> <objname2>";
    const char *purpose_string = "(TODO) Test if BoT <objname> intersects with BoT <objname2>";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (argc != 2) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (_bot_obj_setup(gb, argv[0]) == BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)gb->intern->idb_ptr;

    struct directory *bot_dp_2;
    struct rt_db_internal intern_2;
    GED_DB_LOOKUP(gb->gedp, bot_dp_2, argv[1], LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
    GED_DB_GET_INTERNAL(gb->gedp, &intern_2, bot_dp_2, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
    if (intern_2.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern_2.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type bot\n", argv[1]);
	rt_db_free_internal(&intern_2);
	return BRLCAD_ERROR;
    }
    struct rt_bot_internal *bot_2 = (struct rt_bot_internal *)intern_2.idb_ptr;

    int fc_1 = (int)bot->num_faces;
    int fc_2 = (int)bot_2->num_faces;
    int vc_1 = (int)bot->num_vertices;
    int vc_2 = (int)bot_2->num_vertices;
    point_t *verts_1 = (point_t *)bot->vertices;
    point_t *verts_2 = (point_t *)bot_2->vertices;
    int *faces_1 = bot->faces;
    int *faces_2 = bot_2->faces;

    (void)bg_trimesh_isect(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			   faces_1, fc_1, verts_1, vc_1, faces_2, fc_2, verts_2, vc_2);

    rt_db_free_internal(&intern_2);

    return BRLCAD_OK;
}

extern "C" int
_bot_cmd_sync(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot sync <objname>";
    const char *purpose_string = "Synchronize connected BoT triangle orientations";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (argc != 1) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    int flip_cnt = bg_trimesh_sync(bot->faces, bot->faces, bot->num_faces);
    if (flip_cnt < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to perform BoT sync");
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(gb->dp, gb->gedp->dbip, gb->intern, &rt_uniresource) < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to update BoT");
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gb->gedp->ged_result_str, "Performed %d face flipping operations", flip_cnt);
    return BRLCAD_OK;
}

static void
_bot_vlblock_plot(struct ged *gedp, struct bv_vlblock *vbp, const char *sname)
{
    struct bview *view = gedp->ged_gvp;
    if (gedp->new_cmd_forms) {
	struct bu_vls nroot = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&nroot, "bot::%s", sname);
	bv_vlblock_obj(vbp, view, bu_vls_cstr(&nroot));
	bu_vls_free(&nroot);
    } else {
	_ged_cvt_vlblock_to_solids(gedp, vbp, sname, 0);
    }
}

extern "C" int
_bot_cmd_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot plot <objname> tri_index [tri_index ...]";
    const char *purpose_string = "Plot specified triangle face(s)";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct bu_color *color = gb->color;
    struct bv_vlblock *vbp = gb->vbp;
    struct bu_list *vlfree = gb->vlfree;

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    argc--; argv++;

    std::set<int> elements;
    if (_bot_face_specifiers(elements, gb->gedp->ged_result_str, argc, argv) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }

    unsigned char rgb[3] = {255, 255, 0};
    if (color)
	bu_color_to_rgb_chars(color, rgb);

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);

    struct bu_list *vhead = bv_vlblock_find(vbp, (int)rgb[0], (int)rgb[1], (int)rgb[2]);

    std::set<int>::iterator f_it;
    for (f_it = elements.begin(); f_it != elements.end(); ++f_it) {
	point_t v[3];
	for (int i = 0; i < 3; i++)
          VMOVE(v[i], &bot->vertices[bot->faces[*f_it*3+i]*3]);
	BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, v[1], BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead, v[2], BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_DRAW);
    }

    _bot_vlblock_plot(gb->gedp, vbp, "_bot_face_plot");

    return BRLCAD_OK;
}

// bg_fit_plane uses this underlying math, but will only return the center
// point and normal vector.  We want all the axis vectors for the coordinate
// system, so we do the SVD calculation here.
//
// Probably this could (and should) be a libbg function that isn't specific to
// BoTs, but doing it here for testing purposes.
extern "C" int
_bot_cmd_pca(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot pca <objname> [output_name]";
    const char *purpose_string = "Calculate Principle Component Analysis for BoT";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    argc--; argv++;

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    point_t *vp = (point_t *)bot->vertices;

    // Find the center point
    point_t center = VINIT_ZERO;
    vect_t xaxis, yaxis, zaxis;

    if (bg_pca(&center, &xaxis, &yaxis, &zaxis, bot->num_vertices, vp) != BRLCAD_OK)
	return BRLCAD_ERROR;

    bu_log("X axis:  %g %g %g\n", V3ARGS(xaxis));
    bu_log("Y axis:  %g %g %g\n", V3ARGS(yaxis));
    bu_log("Z axis:  %g %g %g\n", V3ARGS(zaxis));

    // Do what ETO prep does to set up a BRL-CAD rotation matrix
    mat_t R, T, RT;
    // Rotation
    MAT_IDN(R);
    VMOVE(&R[0], xaxis);
    VMOVE(&R[4], yaxis);
    VMOVE(&R[8], zaxis);
    // Translation
    MAT_IDN(T);
    MAT_DELTAS_VEC_NEG(T, center);
    // Combine
    bn_mat_mul(RT, R, T);
    bn_mat_print("Rotation & Translation matrix", RT);

    if (!argc)
	return BRLCAD_OK;

    // We have another arg - make a relocated copy of the input BoT.
    // First, validate there is no output name collision
    struct directory *dp = db_lookup(gb->gedp->dbip, argv[0], LOOKUP_QUIET);
    if (dp != RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, "Object %s already exists in the database - not writing\n", argv[0]);
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *moved_bot = rt_bot_dup(bot);
    for (size_t i = 0; i < moved_bot->num_vertices; i++) {
	// Set up a vect_t from the BoT vertex array
	vect_t v1;
	VMOVE(v1, &moved_bot->vertices[i*3]);
	// Apply matrix to the vect_t
	vect_t v2;
	MAT4X3PNT(v2, RT, v1);
	// Put the new vertex position back in in the moved_bot vertex array
	VMOVE(&moved_bot->vertices[i*3], v2);
    }

    // Validate output name doesn't collide
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)moved_bot;

    dp = db_diradd(gb->gedp->dbip, argv[0], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, "Cannot add %s to directory\n", argv[0]);
	rt_bot_internal_free(moved_bot);
	BU_PUT(moved_bot, struct rt_bot_internal);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }
    if (rt_db_put_internal(dp, gb->gedp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to write %s to database\n", argv[0]);
	rt_bot_internal_free(moved_bot);
	BU_PUT(moved_bot, struct rt_bot_internal);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

extern "C" int
_bot_cmd_split(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    const char *usage_string = "bot split <objname>";
    const char *purpose_string = "Split BoT into objects containing topologically connected triangle subsets";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (argc != 1) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);

    int **fsets = NULL;
    int *fset_cnts = NULL;

    int split_cnt = bg_trimesh_split(&fsets, &fset_cnts, bot->faces, bot->num_faces);
    if (split_cnt <= 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "BoT split unsuccessful");
	ret = BRLCAD_ERROR;
	goto bot_split_done;
    }

    if (split_cnt == 1) {
	bu_vls_printf(gb->gedp->ged_result_str, "BoT is fully connected topologically, not splitting");
	goto bot_split_done;
    }

    // Two or more triangle sets - time for new bots
    for (int i = 0; i < split_cnt; i++) {
	// Because these are independent objects, we don't want to just make lots of copies
	// of the full original vertex set.  Use bg_trimesh_3d_gc to boil down the data to
	// a minimal representation of this BoT subset
	struct rt_db_internal intern;
	struct directory *dp = RT_DIR_NULL;
	struct bu_vls bname = BU_VLS_INIT_ZERO;
	int *ofaces = NULL;
	point_t *opnts = NULL;
	int n_opnts = 0;
	int n_ofaces = bg_trimesh_3d_gc(&ofaces, &opnts, &n_opnts,
					(const int *)fsets[i], fset_cnts[i], (const point_t *)bot->vertices);
	if (n_ofaces < 0) {
	    ret = BRLCAD_ERROR;
	    goto bot_split_done;
	}
	struct rt_bot_internal *nbot;
	BU_ALLOC(nbot, struct rt_bot_internal);
	nbot->magic = RT_BOT_INTERNAL_MAGIC;
	nbot->mode = bot->mode;
	nbot->orientation = bot->orientation;
	nbot->thickness = NULL;
	nbot->face_mode = NULL;
	nbot->num_faces = n_ofaces;
	nbot->num_vertices = n_opnts;
	nbot->faces = ofaces;
	nbot->vertices = (fastf_t *)opnts;

	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_meth = &OBJ[ID_BOT];
	intern.idb_ptr = (void *)nbot;

	// TODO - more robust name generation
	bu_vls_sprintf(&bname, "%s.%d", gb->dp->d_namep, i);
	dp = db_diradd(gb->gedp->dbip, bu_vls_cstr(&bname), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Cannot add %s to directory\n", bu_vls_cstr(&bname));
	    ret = BRLCAD_ERROR;
	    bu_vls_free(&bname);
	    goto bot_split_done;
	}

	if (rt_db_put_internal(dp, gb->gedp->dbip, &intern, &rt_uniresource) < 0) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Failed to write %s to database\n", bu_vls_cstr(&bname));
	    rt_db_free_internal(&intern);
	    ret = BRLCAD_ERROR;
	    bu_vls_free(&bname);
	    goto bot_split_done;
	}

	bu_vls_free(&bname);
    }

bot_split_done:
    if (fsets) {
	for (int i = 0; i < split_cnt; i++) {
	    if (fsets[i])
		bu_free(fsets[i], "free mesh array");
	}
	bu_free(fsets, "free mesh array container");
    }
    if (fset_cnts)
	bu_free(fset_cnts, "free cnts array");
    if (split_cnt > 1)
	bu_vls_printf(gb->gedp->ged_result_str, "Split into %d objects", split_cnt);
    return ret;
}

extern "C" int
_bot_cmd_strip(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    const char *usage_string = "bot strip <objname> <outputname>";
    const char *purpose_string = "Remove triangles forming degenerate volume from a mesh.";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (argc != 2) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (db_lookup(gb->gedp->dbip, argv[2], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, "Object %s already exists!\n", argv[1]);
	return BRLCAD_ERROR;
    }

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    struct rt_i *rtip = rt_new_rti(gb->gedp->dbip);
    rt_gettree(rtip, argv[0]);
    rt_prep(rtip);
    struct bu_ptbl tfaces = BU_PTBL_INIT_ZERO;
    int have_thin_faces = rt_bot_thin_check(&tfaces, bot, rtip, VUNITIZE_TOL, gb->verbosity);
    rt_free_rti(rtip);
    if (have_thin_faces) {
	struct rt_bot_internal *nbot = rt_bot_remove_faces(&tfaces, bot);
	struct rt_db_internal intern;
	struct directory *dp = RT_DIR_NULL;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_meth = &OBJ[ID_BOT];
	intern.idb_ptr = (void *)nbot;
	dp = db_diradd(gb->gedp->dbip, argv[1], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Cannot add %s to directory\n", argv[1]);
	    rt_db_free_internal(&intern);
	    ret = BRLCAD_ERROR;
	    goto bot_strip_done;
	}

	if (rt_db_put_internal(dp, gb->gedp->dbip, &intern, &rt_uniresource) < 0) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Failed to write %s to database\n", argv[1]);
	    rt_db_free_internal(&intern);
	    ret = BRLCAD_ERROR;
	    goto bot_strip_done;
	}
    }

bot_strip_done:

    return ret;
}

static void
bot_output(ft_table_t *table, struct db_i *dbip, struct directory *dp)
{
    if (!table)
	return;
    if (dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
	return;
    struct rt_db_internal intern;
    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
    RT_DB_INTERNAL_INIT(&intern);
    RT_CK_RESOURCE(&rt_uniresource);
    if (db_get_external(&ext, dp, dbip) < 0)
	return;
    if (rt_db_external5_to_internal5(&intern, &ext, dp->d_namep, dbip, NULL, &rt_uniresource) < 0) {
	bu_free_external(&ext);
	return;
    }
    if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_free_external(&ext);
	return;
    }
    struct rt_bot_internal *bot = (struct rt_bot_internal *)intern.idb_ptr;

    struct bu_vls str = BU_VLS_INIT_ZERO;

    // Object Path
    //db_path_to_vls(&str, fp);
    ft_write(table, dp->d_namep);

    // Disk Size
    bu_vls_sprintf(&str, "%zd", dp->d_len);
    ft_write(table, bu_vls_cstr(&str));

    // Number of vertices
    bu_vls_sprintf(&str, "%zd", bot->num_vertices);
    ft_write(table, bu_vls_cstr(&str));

    // Number of faces
    bu_vls_sprintf(&str, "%zd", bot->num_faces);
    ft_write(table, bu_vls_cstr(&str));

    // Number of face normals
    bu_vls_sprintf(&str, "%zd", bot->num_face_normals);
    ft_write(table, bu_vls_cstr(&str));

    // Number of unit surface normals
    bu_vls_sprintf(&str, "%zd", bot->num_normals);
    ft_write(table, bu_vls_cstr(&str));

    // Orientation
    switch (bot->orientation) {
	case RT_BOT_CW:
	    bu_vls_sprintf(&str, "CW");
	    break;
	case RT_BOT_CCW:
	    bu_vls_sprintf(&str, "CCW");
	    break;
	default:
	    bu_vls_sprintf(&str, "NONE");
    }
    ft_write(table, bu_vls_cstr(&str));

    // Mode
    switch (bot->mode) {
	case RT_BOT_SOLID:
	    bu_vls_sprintf(&str, "SOLID");
	    break;
	case RT_BOT_SURFACE:
	    bu_vls_sprintf(&str, "SURFACE");
	    break;
	case RT_BOT_PLATE:
	    bu_vls_sprintf(&str, "PLATE");
	    break;
	case RT_BOT_PLATE_NOCOS:
	    bu_vls_sprintf(&str, "PLATE_NOCOS");
	    break;
	default:
	    bu_vls_trunc(&str, 0);
    }
    ft_write(table, bu_vls_cstr(&str));

    // UV Vert Cnt
    bu_vls_sprintf(&str, "%zd", bot->num_uvs);
    ft_write(table, bu_vls_cstr(&str));

    // UV Face Cnt
    bu_vls_sprintf(&str, "%zd", bot->num_face_uvs);
    ft_write(table, bu_vls_cstr(&str));

    // Attribute size
    struct db5_raw_internal raw;
    if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) != NULL) {
	bu_vls_sprintf(&str, "%zd", raw.attributes.ext_nbytes);
    } else {
	bu_vls_trunc(&str, 0);
    }
    ft_write(table, bu_vls_cstr(&str));

    ft_ln(table);

    // Have what we need - clean up
    bu_free_external(&ext);
}

extern "C" int
_bot_cmd_stat(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    const char *usage_string = "bot stat <pattern>";
    const char *purpose_string = "Report information about bot object(s)";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (argc != 1) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    // Collect full path objects from search pattern
    struct directory **paths;
    int path_cnt = db_ls(gb->gedp->dbip, DB_LS_HIDDEN, argv[0], &paths);

    // Set up table
    ft_table_t *table = ft_create_table();
    ft_set_border_style(table, FT_SIMPLE_STYLE);

    ft_write(table, "Object Path");
    ft_write(table, "Disk Size");
    ft_write(table, "Verts");
    ft_write(table, "Faces");
    ft_write(table, "Face Normals");
    ft_write(table, "Surf Normals");
    ft_write(table, "Orientation");
    ft_write(table, "Mode");
    ft_write(table, "UV Vert Cnt");
    ft_write(table, "UV Face Cnt");
    ft_write(table, "Attr Size");
    ft_ln(table);
    ft_add_separator(table);

    for (int i = 0; i < path_cnt; i++) {
	bot_output(table, gb->gedp->dbip, paths[i]);
    }

    bu_vls_printf(gb->gedp->ged_result_str, "%s\n", ft_to_string(table));
    ft_destroy_table(table);
    bu_free(paths, "paths");
    return ret;
}

const struct bu_cmdtab _bot_cmds[] = {
    { "check",      _bot_cmd_check},
    { "chull",      _bot_cmd_chull},
    { "decimate",   _bot_cmd_decimate},
    { "dump",       _bot_cmd_dump},
    { "extrude",    _bot_cmd_extrude},
    { "flip",       _bot_cmd_flip},
    { "get",        _bot_cmd_get},
    { "isect",      _bot_cmd_isect},
    { "pca",        _bot_cmd_pca},
    { "plot",       _bot_cmd_plot},
    { "remesh",     _bot_cmd_remesh},
    { "repair",     _bot_cmd_repair},
    { "set",        _bot_cmd_set},
    { "smooth",     _bot_cmd_smooth},
    { "split",      _bot_cmd_split},
    { "stat",       _bot_cmd_stat},
    { "strip",      _bot_cmd_strip},
    { "subd",       _bot_cmd_subd},
    { "sync",       _bot_cmd_sync},
    { (char *)NULL,      NULL}
};


static int
_ged_bot_opt_color(struct bu_vls *msg, size_t argc, const char **argv, void *set_c)
{
    struct bu_color **set_color = (struct bu_color **)set_c;
    BU_GET(*set_color, struct bu_color);
    return bu_opt_color(msg, argc, argv, (void *)(*set_color));
}


extern "C" int
ged_bot_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;
    struct _ged_bot_info gb;
    gb.gedp = gedp;
    gb.cmds = _bot_cmds;
    gb.verbosity = 0;
    gb.visualize = 0;
    gb.vlfree = &rt_vlfree;
    struct bu_color *color = NULL;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return BRLCAD_ERROR;
    }

    // Clear results
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the bot command - start processing args
    argc--; argv++;

    // See if we have any high level options set
    struct bu_opt_desc d[5];
    BU_OPT(d[0], "h", "help",    "",      NULL,                 &help,         "Print help");
    BU_OPT(d[1], "v", "verbose", "",      NULL,                 &gb.verbosity, "Verbose output");
    BU_OPT(d[2], "V", "visualize", "",    NULL,                 &gb.visualize, "Visualize results");
    BU_OPT(d[3], "C", "color",   "r/g/b", &_ged_bot_opt_color,  &color,        "Set plotting color");
    BU_OPT_NULL(d[4]);

    gb.gopts = d;

    const char *b_args = "[options] <objname> subcommand [args]";
    const struct bu_cmdtab *bcmds = (const struct bu_cmdtab *)_bot_cmds;
    struct bu_opt_desc *boptd = (struct bu_opt_desc *)d;

    if (!argc) {
	_ged_subcmd_help(gedp, boptd, bcmds, "bot", b_args, &gb, 0, NULL);
	return BRLCAD_OK;
    }

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_bot_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;

    int opt_ret = bu_opt_parse(NULL, acnt, argv, d);

    if (help) {
	if (cmd_pos >= 0) {
	    argc = argc - cmd_pos;
	    argv = &argv[cmd_pos];
	    _ged_subcmd_help(gedp, boptd, bcmds, "bot", b_args, &gb, argc, argv);
	} else {
	    _ged_subcmd_help(gedp, boptd, bcmds, "bot", b_args, &gb, 0, NULL);
	}
	return BRLCAD_OK;
    }

    // Must have a subcommand
    if (cmd_pos == -1) {
	bu_vls_printf(gedp->ged_result_str, ": no valid subcommand specified\n");
	_ged_subcmd_help(gedp, boptd, bcmds, "bot", b_args, &gb, 0, NULL);
	return BRLCAD_ERROR;
    }

    if (opt_ret < 0) {
	_ged_subcmd_help(gedp, boptd, bcmds, "bot", b_args, &gb, 0, NULL);
	return BRLCAD_ERROR;
    }

    // Jump the processing past any options specified
    for (int i = cmd_pos; i < argc; i++) {
	argv[i - cmd_pos] = argv[i];
    }
    argc = argc - cmd_pos;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    if (gb.visualize || BU_STR_EQUAL(argv[cmd_pos], "plot")) {
	GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
	gb.vbp = bv_vlblock_init(gb.vlfree, 32);
    }
    gb.color = color;

    int ret = BRLCAD_ERROR;
    if (bu_cmd(_bot_cmds, argc, argv, 0, (void *)&gb, &ret) == BRLCAD_OK) {
	ret = BRLCAD_OK;
	goto bot_cleanup;
    }

    bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);

bot_cleanup:
    if (gb.intern) {
	rt_db_free_internal(gb.intern);
	BU_PUT(gb.intern, struct rt_db_internal);
    }
    if (gb.visualize) {
	bv_vlblock_free(gb.vbp);
	gb.vbp = (struct bv_vlblock *)NULL;
    }
    if (color) {
	BU_PUT(color, struct bu_color);
    }
    return ret;
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl bot_cmd_impl = { "bot", ged_bot_core, GED_CMD_DEFAULT };
    const struct ged_cmd bot_cmd = { &bot_cmd_impl };

    struct ged_cmd_impl bot_condense_cmd_impl = {"bot_condense", ged_bot_condense_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_condense_cmd = { &bot_condense_cmd_impl };

    struct ged_cmd_impl bot_decimate_cmd_impl = {"bot_decimate", ged_bot_decimate_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_decimate_cmd = { &bot_decimate_cmd_impl };

    struct ged_cmd_impl bot_dump_cmd_impl = {"bot_dump", ged_bot_dump_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_dump_cmd = { &bot_dump_cmd_impl };

    struct ged_cmd_impl bot_exterior_cmd_impl = {"bot_exterior", ged_bot_exterior, GED_CMD_DEFAULT};
    const struct ged_cmd bot_exterior_cmd = { &bot_exterior_cmd_impl };

    struct ged_cmd_impl bot_face_fuse_cmd_impl = {"bot_face_fuse", ged_bot_face_fuse_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_face_fuse_cmd = { &bot_face_fuse_cmd_impl };

    struct ged_cmd_impl bot_face_sort_cmd_impl = {"bot_face_sort", ged_bot_face_sort_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_face_sort_cmd = { &bot_face_sort_cmd_impl };

    struct ged_cmd_impl bot_flip_cmd_impl = {"bot_flip", ged_bot_flip_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_flip_cmd = { &bot_flip_cmd_impl };

    struct ged_cmd_impl bot_fuse_cmd_impl = {"bot_fuse", ged_bot_fuse_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_fuse_cmd = { &bot_fuse_cmd_impl };

    struct ged_cmd_impl bot_merge_cmd_impl = {"bot_merge", ged_bot_merge_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_merge_cmd = { &bot_merge_cmd_impl };

    struct ged_cmd_impl bot_smooth_cmd_impl = {"bot_smooth", ged_bot_smooth_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_smooth_cmd = { &bot_smooth_cmd_impl };

    struct ged_cmd_impl bot_split_cmd_impl = {"bot_split", ged_bot_split_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_split_cmd = { &bot_split_cmd_impl };

    struct ged_cmd_impl bot_sync_cmd_impl = {"bot_sync", ged_bot_sync_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_sync_cmd = { &bot_sync_cmd_impl };

    struct ged_cmd_impl bot_vertex_fuse_cmd_impl = {"bot_vertex_fuse", ged_bot_vertex_fuse_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_vertex_fuse_cmd = { &bot_vertex_fuse_cmd_impl };

    struct ged_cmd_impl dbot_dump_cmd_impl = {"dbot_dump", ged_dbot_dump_core, GED_CMD_DEFAULT};
    const struct ged_cmd dbot_dump_cmd = { &dbot_dump_cmd_impl };

    struct ged_cmd_impl find_bot_edge_cmd_impl = {"find_bot_edge", ged_find_bot_edge_nearest_pnt_core, GED_CMD_DEFAULT};
    const struct ged_cmd find_bot_edge_cmd = { &find_bot_edge_cmd_impl };

    struct ged_cmd_impl find_bot_pnt_cmd_impl = {"find_bot_pnt", ged_find_bot_pnt_nearest_pnt_core, GED_CMD_DEFAULT};
    const struct ged_cmd find_bot_pnt_cmd = { &find_bot_pnt_cmd_impl };

    struct ged_cmd_impl get_bot_edges_cmd_impl = {"get_bot_edges", ged_get_bot_edges_core, GED_CMD_DEFAULT};
    const struct ged_cmd get_bot_edges_cmd = { &get_bot_edges_cmd_impl };

    const struct ged_cmd *bot_cmds[] = {
	&bot_cmd,
	&bot_condense_cmd,
	&bot_decimate_cmd,
	&bot_dump_cmd,
	&bot_exterior_cmd,
	&bot_face_fuse_cmd,
	&bot_face_sort_cmd,
	&bot_flip_cmd,
	&bot_fuse_cmd,
	&bot_merge_cmd,
	&bot_smooth_cmd,
	&bot_split_cmd,
	&bot_sync_cmd,
	&bot_vertex_fuse_cmd,
	&dbot_dump_cmd,
	&find_bot_edge_cmd,
	&find_bot_pnt_cmd,
	&get_bot_edges_cmd,
	NULL
    };


    static const struct ged_plugin pinfo = { GED_API, bot_cmds, sizeof(bot_cmds)/sizeof(bot_cmds[0]) };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
    {
	return &pinfo;
    }
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
