/*                        B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
/** @file libged/brep/brep.cpp
 *
 * The LIBGED brep command.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <locale>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/color.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../ged_private.h"
#include "./ged_brep.h"

/* FIXME: how should we set up brep functionality without introducing
 * lots of new public librt functions?  right now, we reach into librt
 * directly and export what we need from brep_debug.cpp which sucks.
 */
extern "C" {
RT_EXPORT extern int rt_brep_boolean(struct rt_db_internal *out, const struct rt_db_internal *ip1, const struct rt_db_internal *ip2, db_op_t operation);
}


struct brep_convert_args {
    int no_evaluation;
    struct bu_vls suffix;
};

static const struct bu_cmd_option brep_convert_options[] = {
    BU_CMD_FLAG(NULL, "no-evaluation", struct brep_convert_args, no_evaluation,
	"For a combination, create a CSG BREP tree without evaluating booleans"),
    BU_CMD_VLS_APPEND(NULL, "suffix", struct brep_convert_args, suffix, "suffix",
	"Suffix used for no-evaluation object names"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand brep_convert_operands[] = {
    BU_CMD_OPERAND("output_name", BU_CMD_VALUE_STRING, 0, 1,
	"Output BREP object name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema brep_convert_schema = {
    "brep", "Generate a BREP representation of the selected object",
    brep_convert_options, brep_convert_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


struct brep_split_args {
    fastf_t thickness;
    struct bu_vls output_object;
    int object_per_face;
};

static const struct bu_cmd_option brep_split_options[] = {
    BU_CMD_NUMBER("t", "thickness", struct brep_split_args, thickness, "thickness",
	"Default plate-mode thickness"),
    BU_CMD_VLS_APPEND("o", "output-object", struct brep_split_args, output_object,
	"name", "Output-object root name"),
    BU_CMD_FLAG("O", "object-per-face", struct brep_split_args, object_per_face,
	"Create one BREP object per face"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand brep_split_operands[] = {
    BU_CMD_OPERAND("face_indices", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Face indices, ranges, or lists", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema brep_split_schema = {
    "split", "Convert a BREP object into plate-mode BREP objects",
    brep_split_options, brep_split_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

// Indices are specified for info and plot commands - parsing logic is common to both
int
_brep_indices(std::set<int> &elements, struct bu_vls *vls, int argc, const char **argv) {
    std::set<int> indices;
    for (int i = 0; i < argc; i++) {
	std::string s1(argv[i]);
	size_t pos_dash = s1.find_first_of("-:", 0);
	size_t pos_comma = s1.find_first_of(",/;", 0);
	if (pos_dash != std::string::npos) {
	    // May have a range - find out
	    std::string s2 = s1.substr(0, pos_dash);
	    s1.erase(0, pos_dash + 1);
	    int val1, val2, vtmp;
	    if (!bu_cmd_integer_from_str(&val1, s1.c_str())) {
		bu_vls_printf(vls, "Invalid index specification: %s\n", s1.c_str());
		return BRLCAD_ERROR;
	    } 
	    if (!bu_cmd_integer_from_str(&val2, s2.c_str())) {
		bu_vls_printf(vls, "Invalid index specification: %s\n", s2.c_str());
		return BRLCAD_ERROR;
	    }
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
		int val1;
		if (!bu_cmd_integer_from_str(&val1, ss.c_str())) {
		    bu_vls_printf(vls, "Invalid index specification: %s\n", ss.c_str());
		    return BRLCAD_ERROR;
		} else {
		    elements.insert(val1);
		}
		s1.erase(0, pos_comma + 1);
		pos_comma = s1.find_first_of(",/;", 0);
	    }
	    if (s1.length()) {
		int val1;
		if (!bu_cmd_integer_from_str(&val1, s1.c_str())) {
		    bu_vls_printf(vls, "Invalid index specification: %s\n", s1.c_str());
		    return BRLCAD_ERROR;
		} 
		elements.insert(val1);
	    }
	    continue;
	}

	// Nothing fancy looking - see if its a number
	int val = 0;
	if (bu_cmd_integer_from_str(&val, argv[i])) {
	    elements.insert(val);
	} else {
	    bu_vls_printf(vls, "Invalid index specification: %s\n", argv[i]);
	    return BRLCAD_ERROR;
	}
    }

    return BRLCAD_OK;
}

static int
_brep_cmd_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
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
_brep_cmd_boolean(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> bool <op> <objname2> <output_objname>";
    const char *purpose_string = "perform BRep boolean evaluations";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    if (argc != 4) {
	bu_vls_printf(gb->gedp->ged_result_str, "brep <objname1> bool <op> <objname2> <output_objname>\n");
	return BRLCAD_ERROR;
    }


    // We've already looked up the first sold, get the second
    struct directory *dp2 = db_lookup(gedp->dbip, argv[2], LOOKUP_NOISY);
    if (dp2 == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, ": %s is not a solid or does not exist in database", argv[3]);
	return BRLCAD_ERROR;
    } else {
	int real_flag = (gb->dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	if (!real_flag) {
	    /* solid doesn't exist */
	    bu_vls_printf(gedp->ged_result_str, ": %s is not a real solid", argv[2]);
	    return BRLCAD_ERROR;
	}
    }
    struct rt_db_internal intern2;
    GED_DB_GET_INTERN(gedp, &intern2, dp2, bn_mat_identity, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&intern2);

    db_op_t op = DB_OP_NULL;
    op = db_str2op(argv[1]);
    if (op == DB_OP_NULL) {
	bu_vls_printf(gedp->ged_result_str, ": invalid boolean operation specified: %s", argv[1]);
	return BRLCAD_ERROR;
    }

    struct rt_db_internal intern_res;
    rt_brep_boolean(&intern_res, &gb->intern, &intern2, op);
    struct rt_brep_internal *bip = (struct rt_brep_internal *)intern_res.idb_ptr;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    mk_brep(wdbp, argv[3], (void *)(bip->brep));
    rt_db_free_internal(&intern2);
    rt_db_free_internal(&intern_res);

    return BRLCAD_OK;
}

extern "C" int
_brep_cmd_bot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> bot <output_name>";
    const char *purpose_string = "generate a triangle mesh from the BRep object";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;

    argc--; argv++;

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    const struct bg_tess_tol *ttol = (const struct bg_tess_tol *)&gb->wdbp->wdb_ttol;
    struct rt_brep_internal *bi = (struct rt_brep_internal*)gb->intern.idb_ptr;
    struct bu_vls bname_bot = BU_VLS_INIT_ZERO;

    if (!argc) {
	bu_vls_sprintf(&bname_bot, "%s.bot", gb->solid_name.c_str());
    } else {
	bu_vls_sprintf(&bname_bot, "%s", argv[0]);
    }

    const char *bot_name = bu_vls_cstr(&bname_bot);

    int fcnt, fncnt, ncnt, vcnt;
    int *faces = NULL;
    fastf_t *vertices = NULL;
    int *face_normals = NULL;
    fastf_t *normals = NULL;

    struct bg_tess_tol cdttol = BG_TESS_TOL_INIT_ZERO;
    cdttol.abs = ttol->abs;
    cdttol.rel = ttol->rel;
    cdttol.norm = ttol->norm;
    ON_Brep_CDT_State *s_cdt = ON_Brep_CDT_Create((void *)bi->brep, gb->solid_name.c_str());
    ON_Brep_CDT_Tol_Set(s_cdt, &cdttol);
    if (ON_Brep_CDT_Tessellate(s_cdt, 0, NULL) == -1) {
	bu_vls_printf(gedp->ged_result_str, "tessellation failed\n");
	ON_Brep_CDT_Destroy(s_cdt);
	bu_vls_free(&bname_bot);
	return BRLCAD_ERROR;
    }
    ON_Brep_CDT_Mesh(&faces, &fcnt, &vertices, &vcnt, &face_normals, &fncnt, &normals, &ncnt, s_cdt, 0, NULL);
    ON_Brep_CDT_Destroy(s_cdt);

    struct rt_bot_internal *bot;
    BU_GET(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags = 0;
    bot->num_vertices = vcnt;
    bot->num_faces = fcnt;
    bot->vertices = vertices;
    bot->faces = faces;
    bot->thickness = NULL;
    bot->face_mode = (struct bu_bitv *)NULL;
    bot->num_normals = ncnt;
    bot->num_face_normals = fncnt;
    bot->normals = normals;
    bot->face_normals = face_normals;

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_export(wdbp, bot_name, (void *)bot, ID_BOT, 1.0)) {
	bu_vls_free(&bname_bot);
	return BRLCAD_ERROR;
    }

    bu_vls_free(&bname_bot);
    return BRLCAD_OK;
}

extern "C" int
_brep_cmd_bots(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> bots <objname2> [objname3 ...]";
    const char *purpose_string = "generate overlap free meshes for multiple BRep objects";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    argc--; argv++;

    if (!argc || !argv) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    const char **obj_names = (const char **)bu_calloc(argc, sizeof(char *), "new argv");
    obj_names[0] = gb->dp->d_namep;
    for (int iav = 0; iav < argc; iav++) {
	obj_names[iav+1] = argv[iav];
    }
    int obj_cnt = argc+1;

    double ovlp_max_smallest = DBL_MAX;

    const struct bg_tess_tol *ttol = (const struct bg_tess_tol *)&gb->wdbp->wdb_ttol;
    struct bg_tess_tol cdttol = BG_TESS_TOL_INIT_ZERO;
    cdttol.abs = ttol->abs;
    cdttol.rel = ttol->rel;
    cdttol.norm = ttol->norm;

    std::vector<ON_Brep_CDT_State *> ss_cdt;
    std::vector<std::string> bot_names;
    std::vector<struct rt_brep_internal *> o_bi;

    // Set up
    for (int i = 0; i < obj_cnt; i++) {
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_brep_internal* bi;
	if ((dp = db_lookup(gedp->dbip, obj_names[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a solid or does not exist in database", obj_names[i]);
	    return BRLCAD_ERROR;
	}
	GED_DB_GET_INTERN(gedp, &intern, dp, bn_mat_identity, BRLCAD_ERROR);
	RT_CK_DB_INTERNAL(&intern);
	bi = (struct rt_brep_internal*)intern.idb_ptr;
	if (!RT_BREP_TEST_MAGIC(bi)) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a brep solid", obj_names[i]);
	    return BRLCAD_ERROR;
	}

	std::string bname = std::string(obj_names[i]) + std::string("-bot");
	ON_Brep_CDT_State *s_cdt = ON_Brep_CDT_Create((void *)bi->brep, obj_names[i]);
	ON_Brep_CDT_Tol_Set(s_cdt, &cdttol);
	o_bi.push_back(bi);
	ss_cdt.push_back(s_cdt);
	bot_names.push_back(bname);

	double bblen = bi->brep->BoundingBox().Diagonal().Length() * 0.01;
	ovlp_max_smallest = (bblen < ovlp_max_smallest) ? bblen : ovlp_max_smallest;
    }

    // Do tessellations
    for (int i = 0; i < obj_cnt; i++) {
	ON_Brep_CDT_Tessellate(ss_cdt[i], 0, NULL);
    }

    // Do comparison/resolution
    struct ON_Brep_CDT_State **s_a = (struct ON_Brep_CDT_State **)bu_calloc(ss_cdt.size(), sizeof(struct ON_Brep_CDT_State *), "state array");
    for (size_t i = 0; i < ss_cdt.size(); i++) {
	s_a[i] = ss_cdt[i];
    }
    if (ON_Brep_CDT_Ovlp_Resolve(s_a, obj_cnt, ovlp_max_smallest, INT_MAX) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Error: RESOLVE fail.");
	return BRLCAD_ERROR;
    }

    // Make final meshes
    for (int i = 0; i < obj_cnt; i++) {
	int fcnt, fncnt, ncnt, vcnt;
	int *faces = NULL;
	fastf_t *vertices = NULL;
	int *face_normals = NULL;
	fastf_t *normals = NULL;

	ON_Brep_CDT_Mesh(&faces, &fcnt, &vertices, &vcnt, &face_normals, &fncnt, &normals, &ncnt, ss_cdt[i], 0, NULL);
	ON_Brep_CDT_Destroy(ss_cdt[i]);

	struct bu_vls bot_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&bot_name, "%s", bot_names[i].c_str());

	struct rt_bot_internal *bot;
	BU_GET(bot, struct rt_bot_internal);
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->bot_flags = 0;
	bot->num_vertices = vcnt;
	bot->num_faces = fcnt;
	bot->vertices = vertices;
	bot->faces = faces;
	bot->thickness = NULL;
	bot->face_mode = (struct bu_bitv *)NULL;
	bot->num_normals = ncnt;
	bot->num_face_normals = fncnt;
	bot->normals = normals;
	bot->face_normals = face_normals;

	if (wdb_export(gb->wdbp, bu_vls_cstr(&bot_name), (void *)bot, ID_BOT, 1.0)) {
	    return BRLCAD_ERROR;
	}
	bu_vls_free(&bot_name);
    }

    return BRLCAD_OK;
}

extern "C" int
_brep_cmd_brep(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> brep [opts] [output_name]";
    const char *purpose_string = "generate a BRep representation of the specified object";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;

    argc--;argv++;

    struct brep_convert_args args = {0, BU_VLS_INIT_ZERO};
    struct bu_vls bname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&args.suffix, ".brep");

    struct ged *gedp = gb->gedp;
    if (gb->intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is already a brep\n", gb->solid_name.c_str());
	bu_vls_free(&args.suffix);
	return BRLCAD_ERROR;
    }

    int operand_index = bu_cmd_schema_parse_complete(&brep_convert_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_free(&args.suffix);
	return BRLCAD_ERROR;
    }
    argc -= operand_index;
    argv += operand_index;

    if (args.no_evaluation && gb->intern.idb_type == ID_COMBINATION) {
	struct bu_vls bname_suffix;
	bu_vls_init(&bname_suffix);
	bu_vls_sprintf(&bname_suffix, "%s%s", gb->solid_name.c_str(), bu_vls_cstr(&args.suffix));
	if (db_lookup(gedp->dbip, bu_vls_cstr(&bname_suffix), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_cstr(&bname_suffix));
	    bu_vls_free(&bname);
	    bu_vls_free(&args.suffix);
	    bu_vls_free(&bname_suffix);
	    return BRLCAD_OK;
	}

	// brep_conversion_comb frees the intern, so make a new copy specifically for it to avoid
	// a double-free with the top level cleanup of gb->intern
	struct rt_db_internal intern;
	GED_DB_GET_INTERN(gedp, &intern, gb->dp, bn_mat_identity, BRLCAD_ERROR);
	RT_CK_DB_INTERNAL(&intern);

	struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
	brep_conversion_comb(&intern, bu_vls_cstr(&bname_suffix), bu_vls_cstr(&args.suffix), wdbp, mk_conv2mm);
	bu_vls_free(&bname_suffix);
	bu_vls_free(&bname);
	bu_vls_free(&args.suffix);
	return BRLCAD_OK;
    }

    // Won't need the suffix if we've gotten this far
    bu_vls_free(&args.suffix);

    if (argc == 0) {
	/* brep obj */
	bu_vls_sprintf(&bname, "%s.brep", gb->solid_name.c_str());
    } else {
	/* brep obj brepname/suffix */
	bu_vls_sprintf(&bname, "%s", argv[0]);
    }

    // Attempt to evaluate to a single object
    struct rt_db_internal brep_db_internal;
    ON_Brep* brep;
    if (db_lookup(gedp->dbip, bu_vls_cstr(&bname), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_cstr(&bname));
	bu_vls_free(&bname);
	return BRLCAD_OK;
    }
    int ret = brep_conversion(&gb->intern, &brep_db_internal, gedp->dbip);
    if (ret == -1) {
	bu_vls_printf(gedp->ged_result_str, "%s doesn't have a "
		      "brep-conversion function yet. Type: %s", gb->solid_name.c_str(),
		      gb->intern.idb_meth->ft_label);
    } else if (ret == -2) {
	bu_vls_printf(gedp->ged_result_str, "%s cannot be converted "
		      "to brep correctly.", gb->solid_name.c_str());
    } else {
	brep = ((struct rt_brep_internal *)brep_db_internal.idb_ptr)->brep;
	struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
	ret = mk_brep(wdbp, bu_vls_cstr(&bname), brep);
	if (ret == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%s is made.", bu_vls_cstr(&bname));
	}
	rt_db_free_internal(&brep_db_internal);
    }
    bu_vls_free(&bname);
    return BRLCAD_OK;
}

extern "C" int
_brep_cmd_csg(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> csg";
    const char *purpose_string = "generate a CSG representation of the specified BRep object";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;

    struct bu_vls bname_csg;
    bu_vls_init(&bname_csg);
    bu_vls_sprintf(&bname_csg, "csg_%s", gb->solid_name.c_str());
    if (db_lookup(gedp->dbip, bu_vls_cstr(&bname_csg), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_cstr(&bname_csg));
	bu_vls_free(&bname_csg);
	return BRLCAD_OK;
    }
    bu_vls_free(&bname_csg);

    return _ged_brep_to_csg(gedp, gb->dp->d_namep, gb->verbosity);
}

extern "C" int
_brep_cmd_dump(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> dump <output_file_name>";
    const char *purpose_string = "Write the BRep object(s) in the tree to a .3dm file";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;

    argc--; argv++;

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "need output filename to store 3DM output in.");
	return BRLCAD_ERROR;
    }
    if (bu_file_exists(argv[0], NULL)) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: file %s already exists\n", argv[0]);
	return BRLCAD_ERROR;
    }

    ONX_Model model;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&msg, "BRL-CAD 3dm dump of brep objects in %s obj %s", gedp->dbip->dbi_filename, gb->solid_name.c_str());
    model.m_sStartSectionComments = bu_vls_cstr(&msg);
    model.m_properties.m_Application.m_application_name = "BRL-CAD";
    model.m_properties.m_Application.m_application_URL = "https://brlcad.org";

    // Find all brep paths
    struct bu_ptbl breps = BU_PTBL_INIT_ZERO;
    const char *brep_search = "-type brep";
    db_update_nref(gedp->dbip);
    (void)db_search(&breps, DB_SEARCH_TREE, brep_search, 1, &gb->dp, gedp->dbip, NULL, NULL, NULL);
    for (size_t i = 0; i < BU_PTBL_LEN(&breps); i++) {
	struct db_full_path *fp = (struct db_full_path *)BU_PTBL_GET(&breps, i);
	mat_t m;
	struct bu_color c;
	MAT_IDN(m);
	db_path_to_mat(gedp->dbip, fp, m, 0);
	db_full_path_color(&c, fp, gedp->dbip);
	struct directory *dp = DB_FULL_PATH_CUR_DIR(fp);
	struct rt_db_internal intern;
	if (rt_db_get_internal(&intern, dp, gedp->dbip, m) < 0) {
	    bu_log("Error - unable to get internal of %s\n", dp->d_namep);
	    continue;
	}
	ON_Brep *o_brep = ((struct rt_brep_internal *)intern.idb_ptr)->brep;
	if (!o_brep) {
	    bu_log("Error - no ON_Brep data found for %s\n", dp->d_namep);
	    continue;
	}

	ON_3dmObjectAttributes attrs;

	// NOTE: this naive conversion ONLY works on ASCII chars 
	std::string dnamep_str(dp->d_namep);
	std::wstring wc(dnamep_str.begin(), dnamep_str.end());
	attrs.SetName(wc.c_str(), true);

	int rgb[3];
	bu_color_to_rgb_ints(&c, &rgb[0], &rgb[1], &rgb[2]);
	attrs.m_color = ON_Color(rgb[0], rgb[1], rgb[2]);
	attrs.SetColorSource(ON::color_from_object);

	model.AddModelGeometryComponent(o_brep, &attrs, true);
    }

    ON_TextLog error_log; // errors printed to stdout
    model.Write(argv[0], 0, &error_log);

    return BRLCAD_OK;
}

extern "C" int
_brep_cmd_flip(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> flip";
    const char *purpose_string = "flip all face normals on the specified BRep object";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gb->intern.idb_ptr;

    b_ip->brep->Flip();

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (mk_brep(wdbp, gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

extern "C" int
_brep_cmd_geo(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    const char *purpose_string = "NURBS geometry editing support for brep objects";
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", purpose_string);
	return BRLCAD_OK;
    }
    if (argc >= 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	return brep_geo(gb, argc, argv);
    }

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    /* Deprecation notice: NURBS surface CV editing is now available via the
     * 'edit' command (e.g. "edit <obj> select_surface_cv <face> <i> <j>" and
     * "edit <obj> move_surface_cv <dx> <dy> <dz>").  The 'brep geo' sub-command
     * is preserved for backward compatibility but may be removed in a future
     * release.  Please migrate to 'edit' for CV manipulation. */
    bu_vls_printf(gb->gedp->ged_result_str,
	    "NOTE: 'brep geo' is deprecated for CV editing. "
	    "Use 'edit <obj> select_surface_cv / move_surface_cv / "
	    "set_surface_cv_position' instead.\n");

    argc--; argv++;

    return brep_geo(gb, argc, argv);
}

extern "C" int
_brep_cmd_info(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;

    const char *purpose_string = "print detailed information about components of the BRep object";
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", purpose_string);
	return BRLCAD_OK;
    }
    if (argc >= 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	return brep_info(gedp->ged_result_str, NULL, argc, argv);
    }

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }
    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gb->intern.idb_ptr;

    argc--; argv++;

    // If the user doesn't specify, give an overview
    if (!argc) {
	const char *is_valid = (rt_brep_valid(NULL, &gb->intern, 0)) ? "YES" : "NO";
	bu_vls_printf(gedp->ged_result_str, "%s -- Valid: %s,", gb->solid_name.c_str(), is_valid);
	const char *is_solid = (b_ip->brep->IsSolid()) ? "YES" : "NO";
	bu_vls_printf(gedp->ged_result_str, " Solid: %s,", is_solid);
	const char *is_plate_mode = (rt_brep_plate_mode(&gb->intern)) ? "YES" : "NO";
	bu_vls_printf(gedp->ged_result_str, " Plate mode: %s", is_plate_mode);
	if (BU_STR_EQUAL(is_plate_mode, "YES")) {
	    double thickness; int nocos;
	    rt_brep_plate_mode_getvals(&thickness, &nocos, &gb->intern);
	    if (nocos) {
		bu_vls_printf(gedp->ged_result_str, "[%f (NOCOS)]\n", thickness);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "[%f (COS)]\n", thickness);
	    }
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\n");
	}
    }

    return brep_info(gedp->ged_result_str, b_ip->brep, argc, argv);
}


extern "C" int
_brep_cmd_intersect(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> intersect <obj2> <i> <j> [PP|PC|PS|CC|CS|SS]\n";
    const char *purpose_string = "calculate intersections between BRep object components";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    if (argc != 4 && argc != 5) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    int i, j;
    if (!bu_cmd_integer_from_str(&i, argv[2])) {
	bu_vls_printf(gedp->ged_result_str, "invalid integer: %s\n", argv[2]);
	return BRLCAD_ERROR;
    }
    if (!bu_cmd_integer_from_str(&j, argv[3])) {
	bu_vls_printf(gedp->ged_result_str, "invalid integer: %s\n", argv[3]);
	return BRLCAD_ERROR;
    }

    // We've already looked up the first sold, get the second
    struct directory *dp2 = db_lookup(gedp->dbip, argv[1], LOOKUP_NOISY);
    if (dp2 == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, ": %s is not a solid or does not exist in database", argv[3]);
	return BRLCAD_ERROR;
    } else {
	int real_flag = (gb->dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	if (!real_flag) {
	    /* solid doesn't exist */
	    bu_vls_printf(gedp->ged_result_str, ": %s is not a real solid", argv[1]);
	    return BRLCAD_ERROR;
	}
    }
    struct rt_db_internal intern2;
    GED_DB_GET_INTERN(gedp, &intern2, dp2, bn_mat_identity, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&intern2);


    if (argc == 4 || BU_STR_EQUAL(argv[4], "SS")) {
	brep_intersect_surface_surface(&gb->intern, &intern2, i, j, gb->vbp);
    } else if (BU_STR_EQUAL(argv[4], "PP")) {
	brep_intersect_point_point(&gb->intern, &intern2, i, j);
    } else if (BU_STR_EQUAL(argv[4], "PC")) {
	brep_intersect_point_curve(&gb->intern, &intern2, i, j);
    } else if (BU_STR_EQUAL(argv[4], "PS")) {
	brep_intersect_point_surface(&gb->intern, &intern2, i, j);
    } else if (BU_STR_EQUAL(argv[4], "CC")) {
	brep_intersect_curve_curve(&gb->intern, &intern2, i, j);
    } else if (BU_STR_EQUAL(argv[4], "CS")) {
	brep_intersect_curve_surface(&gb->intern, &intern2, i, j);
    } else {
	bu_vls_printf(gedp->ged_result_str, "Invalid intersection type %s.\n", argv[6]);
    }

    if (gedp->new_cmd_forms) {
	struct bview *view = gedp->ged_gvp;
	bv_vlblock_obj(gb->vbp, view, "brep_intersect");
    } else {
	char namebuf[65];
	_ged_cvt_vlblock_to_solids(gedp, gb->vbp, namebuf, 0);
    }

    rt_db_free_internal(&intern2);

    return BRLCAD_OK;
}

extern "C" int
_brep_cmd_pick(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;

    const char *purpose_string = "graphically identify components of the BRep object";
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", purpose_string);
	return BRLCAD_OK;
    }
    if (argc >= 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	return brep_pick(gb, argc, argv);
    }

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    argc--; argv++;

    return brep_pick(gb, argc, argv);
}

extern "C" int
_brep_cmd_plate_mode(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> plate_mode [[thickness][cos][nocos]]";
    const char *purpose_string = "Report and set plate mode properties of BRep";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    if (!rt_brep_plate_mode(&gb->intern)) {
	bu_vls_printf(gb->gedp->ged_result_str, ": brep object %s is not a plate mode brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    if (argc != 1 && argc != 2) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    if (argc == 1) {
	double thickness;
	int nocos;
	rt_brep_plate_mode_getvals(&thickness, &nocos, &gb->intern);
	thickness = gb->gedp->dbip->dbi_base2local * thickness;
	if (nocos) {
	    bu_vls_printf(gb->gedp->ged_result_str, "%f (NOCOS)", thickness);
	} else {
	    bu_vls_printf(gb->gedp->ged_result_str, "%f (COS)", thickness);
	}
	return BRLCAD_OK;
    }

    const char *val = argv[1];
    struct bu_attribute_value_set avs;
    double local2base = gb->gedp->dbip->dbi_local2base;

    // Make sure we can get attributes
    if (db5_get_attributes(gb->gedp->dbip, &avs, gb->dp)) {
	bu_vls_printf(gb->gedp->ged_result_str, "Error setting plate mode value\n");
	return BRLCAD_ERROR;
    };

    if (BU_STR_EQUIV(val, "cos")) {
	(void)bu_avs_add(&avs, "_plate_mode_nocos", "0");
	if (db5_replace_attributes(gb->dp, &avs, gb->gedp->dbip)) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Error setting plate mode value\n");
	    return BRLCAD_ERROR;
	} else {
	    bu_vls_printf(gb->gedp->ged_result_str, "%s", val);
	}
	return BRLCAD_OK;
    }

    if (BU_STR_EQUIV(val, "nocos")) {
	(void)bu_avs_add(&avs, "_plate_mode_nocos", "1");
	if (db5_replace_attributes(gb->dp, &avs, gb->gedp->dbip)) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Error setting plate mode value\n");
	    return BRLCAD_ERROR;
	} else {
	    bu_vls_printf(gb->gedp->ged_result_str, "%s", val);
	}
	return BRLCAD_OK;
    }

    // Unpack the string
    double pthickness;
    char *endptr = NULL;
    errno = 0;
    pthickness = strtod(val, &endptr);
    if ((endptr != NULL && strlen(endptr) > 0) || (errno == ERANGE)) {
	pthickness = 0;
    }

    // Apply units to the value
    double pthicknessmm = local2base * pthickness;

    // Create and set the attribute string
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << pthicknessmm;
    std::string sd = ss.str();
    (void)bu_avs_add(&avs, "_plate_mode_thickness", sd.c_str());
    if (db5_replace_attributes(gb->dp, &avs, gb->gedp->dbip)) {
	bu_vls_printf(gb->gedp->ged_result_str, "Error setting plate mode value\n");
	return BRLCAD_ERROR;
    } else {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", val);
    }
    return BRLCAD_OK;
}


extern "C" int
_brep_cmd_plot(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    const char *purpose_string = "visualize specific components of a BRep object";
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", purpose_string);
	return BRLCAD_OK;
    }
    if (argc >= 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	return brep_plot(gb, argc, argv);
    }

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    argc--; argv++;

    return brep_plot(gb, argc, argv);
}

extern "C" int
_brep_cmd_repair(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;

    const char *purpose_string = "attempt repairs on the BRep object";
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", purpose_string);
	return BRLCAD_OK;
    }
    if (argc >= 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	return brep_info(gedp->ged_result_str, NULL, argc, argv);
    }

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }
    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gb->intern.idb_ptr;

    argc--; argv++;

    return brep_repair(gedp, b_ip->brep, gb->solid_name.c_str(), argc, argv);
}

extern "C" int
_brep_cmd_selection(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> selection <append/translate> <selection_name> startx starty startz dirx diry dirz";
    const char *purpose_string = "select specific components of a BRep object";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    int i;
    struct rt_selection_set *selection_set;
    struct bu_ptbl *selections;
    struct rt_selection *new_selection;
    struct rt_selection_query query;
    const char *cmd, *solid_name, *selection_name;

    /*     0
     * subcommand
     */
    if (argc < 1) {
	return BRLCAD_ERROR;
    }

    solid_name = gb->solid_name.c_str();
    struct rt_db_internal *ip = &gb->intern;
    cmd = argv[0];

    if (BU_STR_EQUAL(cmd, "append")) {
	/* append to named selection - selection is created if it doesn't exist */
	void (*free_selection)(struct rt_selection *);

	/*        1         2      3      4     5    6    7
	 * selection_name startx starty startz dirx diry dirz
	 */
	if (argc != 8) {
	    bu_log("wrong args for selection append");
	    return -1;
	}
	selection_name = argv[1];

	/* find matching selections */
	query.start[X] = atof(argv[2]);
	query.start[Y] = atof(argv[3]);
	query.start[Z] = atof(argv[4]);
	query.dir[X] = atof(argv[5]);
	query.dir[Y] = atof(argv[6]);
	query.dir[Z] = atof(argv[7]);
	query.sorting = RT_SORT_CLOSEST_TO_START;

	selection_set = ip->idb_meth->ft_find_selections(ip, &query);
	if (!selection_set) {
	    bu_log("no matching selections");
	    return -1;
	}

	/* could be multiple options, just grabbing the first and
	 * freeing the rest
	 */
	selections = &selection_set->selections;
	new_selection = (struct rt_selection *)BU_PTBL_GET(selections, 0);

	free_selection = selection_set->free_selection;
	for (i = BU_PTBL_LEN(selections) - 1; i > 0; --i) {
	    long *s = BU_PTBL_GET(selections, i);
	    free_selection((struct rt_selection *)s);
	    bu_ptbl_rm(selections, s);
	}
	bu_ptbl_free(selections);
	BU_FREE(selection_set, struct rt_selection_set);

	/* get existing/new selections set in gedp */
	selection_set = ged_get_selection_set(gedp, solid_name, selection_name);
	selection_set->free_selection = free_selection;
	selections = &selection_set->selections;

	/* TODO: Need to implement append by passing new and
	 * existing selection to an rt_brep_evaluate_selection.
	 * For now, new selection simply replaces old one.
	 */
	for (i = BU_PTBL_LEN(selections) - 1; i >= 0; --i) {
	    long *s = BU_PTBL_GET(selections, i);
	    free_selection((struct rt_selection *)s);
	    bu_ptbl_rm(selections, s);
	}
	bu_ptbl_ins(selections, (long *)new_selection);
    } else if (BU_STR_EQUAL(cmd, "translate")) {
	struct rt_selection_operation operation;

	/*        1       2  3  4
	 * selection_name dx dy dz
	 */
	if (argc != 5) {
	    return BRLCAD_ERROR;
	}
	selection_name = argv[1];

	selection_set = ged_get_selection_set(gedp, solid_name, selection_name);
	selections = &selection_set->selections;

	if (BU_PTBL_LEN(selections) < 1) {
	    return BRLCAD_ERROR;
	}

	for (i = 0; i < (int)BU_PTBL_LEN(selections); ++i) {
	    int ret;
	    operation.type = RT_SELECTION_TRANSLATION;
	    operation.parameters.tran.dx = atof(argv[2]);
	    operation.parameters.tran.dy = atof(argv[3]);
	    operation.parameters.tran.dz = atof(argv[4]);

	    ret = ip->idb_meth->ft_process_selection(ip, gedp->dbip,
						     (struct rt_selection *)BU_PTBL_GET(selections, i), &operation);

	    if (ret != 0) {
		return BRLCAD_ERROR;
	    }
	}
	GED_DB_PUT_INTERN(gedp, gb->dp, &gb->intern, BRLCAD_ERROR);
    }
    return BRLCAD_OK;
}

extern "C" int
_brep_cmd_solid(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> solid";
    const char *purpose_string = "report on solidity of the specified BRep";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gb->intern.idb_ptr;
    int solid = (b_ip->brep->IsSolid()) ? 1 : 0;
    if (solid) {
	bu_vls_printf(gb->gedp->ged_result_str, "brep is solid\n");
    } else {
	bu_vls_printf(gb->gedp->ged_result_str, "brep is NOT solid\n");
    }
    return BRLCAD_OK;
}

extern "C" int
_brep_cmd_shrink_surfaces(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> shrink_surfaces";
    const char *purpose_string = "tightens surfaces to face trimming curves";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }


    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gb->intern.idb_ptr;

    b_ip->brep->ShrinkSurfaces();

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (mk_brep(wdbp, gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

extern "C" int
_brep_cmd_split(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> split [-t #] [-O] [-o output_name] [face_indices]";
    const char *purpose_string = "convert BRep object into a set of plate mode BRep objects";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }
    if (!rt_brep_valid(NULL, &gb->intern, 0)) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not valid\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }
    if (rt_brep_plate_mode(&gb->intern)) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is already a plate mode brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    argc--; argv++;

    struct brep_split_args args = {0.0, BU_VLS_INIT_ZERO, 0};
    int operand_index = bu_cmd_schema_parse_complete(&brep_split_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_free(&args.output_object);
	return BRLCAD_ERROR;
    }
    argc -= operand_index;
    argv += operand_index;

    const int object_per_face = args.object_per_face;
    const fastf_t thickness = args.thickness;
    struct bu_vls &ocomb = args.output_object;

	if (!bu_vls_strlen(&args.output_object)) {
	// If the caller didn't provide a name, generate one
	bu_vls_sprintf(&args.output_object, "%s.plates", gb->solid_name.c_str());
    }

    // If we have anything left, it should be indices
    std::set<int> elements;
    if (argc) {
	if (_brep_indices(elements, gb->gedp->ged_result_str, argc, argv) != BRLCAD_OK) {
	    bu_vls_free(&args.output_object);
	    return BRLCAD_ERROR;
	}
    }

    if (db_lookup(gedp->dbip, bu_vls_cstr(&args.output_object), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, ": %s already exists.", bu_vls_cstr(&args.output_object));
	bu_vls_free(&args.output_object);
	return BRLCAD_ERROR;
    }

    ON_Brep *orig_brep = ((struct rt_brep_internal *)gb->intern.idb_ptr)->brep;
    ON_Brep *brep = (object_per_face) ? NULL : new ON_Brep();
    struct wmember wcomb;
    if (object_per_face) {
	BU_LIST_INIT(&wcomb.l);
    }

    // If we have nothing, process all faces
    if (!elements.size()) {
	for (int i = 0; i < orig_brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    int valid_faces = 0;
    std::set<int>::iterator e_it;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
	int f_id = *e_it;
	ON_Brep *fbrep = orig_brep->SubBrep(1, &f_id);
	if (!fbrep || !fbrep->IsValid()) {
	    bu_vls_printf(gedp->ged_result_str, "NOTE: face %d failed to produce a valid brep, skipping", f_id);
	    continue;
	} else {
	    valid_faces++;
	}
	if (object_per_face) {
	    struct bu_vls fbrep_name = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&fbrep_name, "%s.%d", gb->solid_name.c_str(), f_id);
	    (void)mk_addmember(bu_vls_cstr(&fbrep_name), &(wcomb.l), NULL, DB_OP_UNION);
	    mk_brep(wdbp, bu_vls_cstr(&fbrep_name), fbrep);
	    delete fbrep;
	    if (thickness > 0) {
		struct bu_attribute_value_set avs;
		struct directory *ndp = db_lookup(gedp->dbip, bu_vls_cstr(&fbrep_name), LOOKUP_QUIET);
		if (ndp == RT_DIR_NULL) {
		    bu_vls_printf(gedp->ged_result_str, ": failed to create brep for face %d", f_id);
		    bu_vls_free(&fbrep_name);
		    bu_vls_free(&ocomb);
		    return BRLCAD_ERROR;
		}
		if (db5_get_attributes(gb->gedp->dbip, &avs, gb->dp)) {
		    bu_vls_printf(gedp->ged_result_str, ": failed to get attributes from face brep  %s", bu_vls_cstr(&fbrep_name));
		    bu_vls_free(&fbrep_name);
		    bu_vls_free(&ocomb);
		    return BRLCAD_ERROR;
		};
		double local2base = gb->gedp->dbip->dbi_local2base;
		double pthicknessmm = local2base * thickness;
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << pthicknessmm;
		std::string sd = ss.str();
		(void)bu_avs_add(&avs, "_plate_mode_thickness", sd.c_str());
		if (db5_replace_attributes(ndp, &avs, gb->gedp->dbip)) {
		    bu_vls_printf(gedp->ged_result_str, ": failed to set plate mode thickness for face %d", f_id);
		    bu_vls_free(&fbrep_name);
		    bu_vls_free(&ocomb);
		    return BRLCAD_ERROR;
		}
	    }
	    bu_vls_free(&fbrep_name);
	} else {
	    brep->Append(*fbrep);
	    delete fbrep;
	}
    }

    if (!valid_faces) {
	bu_vls_free(&ocomb);
	if (!object_per_face) {
	    delete brep;
	}
	return BRLCAD_ERROR;
    }

    int ret;

    if (!object_per_face) {
	ret = mk_brep(wdbp, bu_vls_cstr(&ocomb), brep);
	if (thickness > 0) {
	    struct bu_attribute_value_set avs;
	    struct directory *ndp = db_lookup(gedp->dbip, bu_vls_cstr(&ocomb), LOOKUP_QUIET);
	    if (ndp == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, ": failed to create brep");
		bu_vls_free(&ocomb);
		return BRLCAD_ERROR;
	    }
	    if (db5_get_attributes(gb->gedp->dbip, &avs, gb->dp)) {
		bu_vls_printf(gedp->ged_result_str, ": failed to get attributes from brep");
		bu_vls_free(&ocomb);
		return BRLCAD_ERROR;
	    };
	    double local2base = gb->gedp->dbip->dbi_local2base;
	    double pthicknessmm = local2base * thickness;
	    std::ostringstream ss;
	    ss << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << pthicknessmm;
	    std::string sd = ss.str();
	    (void)bu_avs_add(&avs, "_plate_mode_thickness", sd.c_str());
	    if (db5_replace_attributes(ndp, &avs, gb->gedp->dbip)) {
		bu_vls_printf(gedp->ged_result_str, ": failed to set plate mode thickness");
		bu_vls_free(&ocomb);
		return BRLCAD_ERROR;
	    }
	}
    } else {
	ret = mk_lcomb(wdbp, bu_vls_cstr(&ocomb), &wcomb, 0, NULL, NULL, NULL, 0);
    }

    bu_vls_free(&ocomb);
    return ret;
}

extern "C" int
_brep_cmd_tikz(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> tikz <outfile>";
    const char *purpose_string = "generate PGF/TikZ 3D plot of BRep object";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    argc--; argv++;

    if (argc != 1 || !argv) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    return brep_tikz(gb, argv[0]);
}


extern "C" int
_brep_cmd_valid(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;

    const char *purpose_string = "report on validity of the specified BRep";
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", purpose_string);
	return BRLCAD_OK;
    }
    if (argc >= 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	return brep_info(gedp->ged_result_str, NULL, argc, argv);
    }

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    argc--; argv++;

    return brep_valid(gedp->ged_result_str, &gb->intern, argc, argv);
}

static int
_brep_cmd_topo(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    const char *purpose_string = "NURBS topology editing support for brep objects";
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", purpose_string);
	return BRLCAD_OK;
    }
    if (argc >= 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	return brep_topo(gb, argc, argv);
    }

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    argc--; argv++;

    return brep_topo(gb, argc, argv);
}

#if 0
extern "C" int
_brep_cmd_weld(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> weld <output> [obj1 obj2 ...]";
    const char *purpose_string = "weld BRep plate mode objects into a solid object.";
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    argc--; argv++;

    bu_vls_printf(gb->gedp->ged_result_str, "UNIMPLEMENTED\n");
    return BRLCAD_ERROR;
}
#endif

const struct bu_cmdtab _brep_cmds[] = {
    { "bool",            _brep_cmd_boolean},
    { "bot",             _brep_cmd_bot},
    { "bots",            _brep_cmd_bots},
    { "brep",            _brep_cmd_brep},
    { "csg",             _brep_cmd_csg},
    { "dump",            _brep_cmd_dump},
    { "flip",            _brep_cmd_flip},
    { "geo",             _brep_cmd_geo},
    { "info",            _brep_cmd_info},
    { "intersect",       _brep_cmd_intersect},
    { "pick",            _brep_cmd_pick},
    { "plate_mode",      _brep_cmd_plate_mode},
    { "plot",            _brep_cmd_plot},
    { "repair",          _brep_cmd_repair},
    { "selection",       _brep_cmd_selection},
    { "solid",           _brep_cmd_solid},
    { "split",           _brep_cmd_split},
    { "shrink_surfaces", _brep_cmd_shrink_surfaces},
    { "tikz",            _brep_cmd_tikz},
    { "valid",           _brep_cmd_valid},
    //{ "weld",            _brep_cmd_weld},
    { "topo",            _brep_cmd_topo},
    { (char *)NULL,      NULL}
};


struct brep_root_args {
    int help;
    struct bu_color color;
    int verbosity;
    int plotres;
};

static const struct bu_cmd_option brep_root_options[] = {
    BU_CMD_FLAG("h", "help", struct brep_root_args, help, "Print command help"),
    BU_CMD_COLOR_COMPAT("C", "color", struct brep_root_args, color, "color",
	"Color used by plotted or highlighted output"),
    BU_CMD_FLAG("v", "verbose", struct brep_root_args, verbosity,
	"Enable additional diagnostic output"),
    BU_CMD_POSITIVE_INTEGER(NULL, "plotres", struct brep_root_args, plotres,
	"resolution", "Plot sampling resolution"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand brep_root_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"BREP or convertible database object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema brep_root_schema = {
    "brep", "Inspect, convert, plot, and repair BREP objects", brep_root_options,
    brep_root_operands, BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static const struct bu_cmd_operand brep_raw_operands[] = {
    BU_CMD_OPERAND("arguments", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Operation-specific arguments", NULL),
    BU_CMD_OPERAND_NULL
};
#define BREP_RAW_SCHEMA(_id, _name, _help) \
    static const struct bu_cmd_schema _id = { \
	_name, _help, NULL, brep_raw_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, \
	BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL) \
    }
BREP_RAW_SCHEMA(brep_boolean_schema, "bool", "Evaluate a BREP boolean");
BREP_RAW_SCHEMA(brep_bots_schema, "bots", "Convert BREP faces to BOTs");
BREP_RAW_SCHEMA(brep_csg_schema, "csg", "Convert a BREP to CSG");
BREP_RAW_SCHEMA(brep_dump_schema, "dump", "Dump BREP data");
BREP_RAW_SCHEMA(brep_flip_schema, "flip", "Reverse BREP orientation");
BREP_RAW_SCHEMA(brep_geo_schema, "geo", "Inspect or edit BREP geometry");
BREP_RAW_SCHEMA(brep_info_schema, "info", "Report BREP information");
BREP_RAW_SCHEMA(brep_pick_schema, "pick", "Pick BREP elements");
BREP_RAW_SCHEMA(brep_plate_mode_schema, "plate_mode", "Query or set plate-mode properties");
static const char * const brep_plot_modes[] = {
    "C2", "C3", "E", "F", "F2d", "FSBB", "FSBB2d", "FTBB", "FTBB2d",
    "FTD", "I", "L", "L2d", "S", "SCV", "SK", "SK2d", "SN", "SUV",
    "SUVP", "T", "T2d", "V", "CDT", "CDT2d", "CDTm2d", "CDTp2d", "CDTw",
    "CDTn", "CDTn2d", "CDTnw", NULL
};
static const struct bu_cmd_operand brep_plot_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("operation", BU_CMD_VALUE_KEYWORD, 1, 1,
	"Plot operation", NULL, brep_plot_modes),
    BU_CMD_OPERAND("indices", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Optional component indices or ranges", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema brep_plot_schema = {
    "plot", "Plot BREP diagnostics", NULL, brep_plot_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
BREP_RAW_SCHEMA(brep_repair_schema, "repair", "Repair BREP topology");
BREP_RAW_SCHEMA(brep_selection_schema, "selection", "Manage BREP selections");
BREP_RAW_SCHEMA(brep_solid_schema, "solid", "Convert plate mode to solid");
BREP_RAW_SCHEMA(brep_shrink_schema, "shrink_surfaces", "Shrink BREP surfaces");
BREP_RAW_SCHEMA(brep_valid_schema, "valid", "Validate BREP topology");
BREP_RAW_SCHEMA(brep_topo_schema, "topo", "Inspect or edit BREP topology");
#undef BREP_RAW_SCHEMA

static const struct bu_cmd_operand brep_bot_operands[] = {
    BU_CMD_OPERAND("output_name", BU_CMD_VALUE_STRING, 0, 1,
	"Output BOT object name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema brep_bot_schema = {
    "bot", "Generate a triangle mesh from a BREP object", NULL, brep_bot_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const char * const brep_intersect_modes[] = {
    "PP", "PC", "PS", "CC", "CS", "SS", NULL
};
static const struct bu_cmd_operand brep_intersect_operands[] = {
    BU_CMD_OPERAND("other_object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Second BREP object", "ged.db_object"),
    BU_CMD_OPERAND("first_index", BU_CMD_VALUE_INTEGER, 1, 1,
	"Component index in the selected object", NULL),
    BU_CMD_OPERAND("second_index", BU_CMD_VALUE_INTEGER, 1, 1,
	"Component index in the second object", NULL),
    BU_CMD_OPERAND_KEYWORDS("mode", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Intersection mode (default SS)", NULL, brep_intersect_modes),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema brep_intersect_schema = {
    "intersect", "Calculate BREP component intersections", NULL,
    brep_intersect_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_operand brep_tikz_operands[] = {
    BU_CMD_OPERAND("output_file", BU_CMD_VALUE_FILE, 1, 1,
	"TikZ output file", "ged.file_path"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema brep_tikz_schema = {
    "tikz", "Export a BREP plot as TikZ", NULL, brep_tikz_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static int brep_tree_execute(void *context, int argc, const char *argv[]);
static const struct bu_cmd_tree_node brep_subcommands[] = {
    BU_CMD_TREE_NODE(&brep_boolean_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_bot_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_bots_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_convert_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_csg_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_dump_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_flip_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_geo_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_info_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_intersect_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_pick_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_plate_mode_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_plot_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_repair_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_selection_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_solid_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_split_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_shrink_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_tikz_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_valid_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE(&brep_topo_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, brep_tree_execute),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree brep_tree = {
    &brep_root_schema, brep_subcommands, BU_CMD_TREE_CHILD_AFTER_FIXED_OPERANDS
};

struct brep_completion_line {
    char *copy;
    char **argv;
    size_t argc;
    size_t cursor_arg;
};

static int
brep_completion_line_parse(struct brep_completion_line *line, const char *input,
	size_t cursor_pos)
{
    size_t input_len;
    size_t p;
    int in_token = 0;
    int in_quote = 0;
    int escaped = 0;

    if (!line || !input)
	return -1;
    memset(line, 0, sizeof(*line));
    input_len = strlen(input);
    if (cursor_pos > input_len)
	cursor_pos = input_len;
    line->copy = bu_strdup(input);
    while (input_len > 0 && isspace((unsigned char)line->copy[input_len - 1]))
	line->copy[--input_len] = '\0';
    line->argv = (char **)bu_calloc(input_len + 2, sizeof(char *),
	"brep completion argv");
    line->argc = bu_argv_from_string(line->argv, input_len + 1, line->copy);

	for (p = 0; p < cursor_pos && input[p]; p++) {
	unsigned char c = (unsigned char)input[p];
	if (escaped) {
	    escaped = 0;
	    in_token = 1;
	    continue;
	}
	if (c == '\\') {
	    escaped = 1;
	    in_token = 1;
	    continue;
	}
	if (c == '"') {
	    in_quote = !in_quote;
	    in_token = 1;
	    continue;
	}
	if (!in_quote && isspace(c)) {
	    if (in_token) {
		line->cursor_arg++;
		in_token = 0;
	    }
	} else {
	    in_token = 1;
	}
    }
    if (line->cursor_arg > line->argc)
	line->cursor_arg = line->argc;
    return 0;
}

static void
brep_completion_line_free(struct brep_completion_line *line)
{
    if (!line)
	return;
    if (line->argv)
	bu_free(line->argv, "brep completion argv");
    if (line->copy)
	bu_free(line->copy, "brep completion input");
    memset(line, 0, sizeof(*line));
}

static int
brep_plot_component_count(const struct rt_brep_internal *bip, const char *mode)
{
    const ON_Brep *brep = bip ? bip->brep : NULL;
    if (!brep || !mode)
	return 0;
    if (BU_STR_EQUAL(mode, "C2")) return brep->m_C2.Count();
    if (BU_STR_EQUAL(mode, "C3")) return brep->m_C3.Count();
    if (BU_STR_EQUAL(mode, "E")) return brep->m_E.Count();
    if (BU_STR_EQUAL(mode, "L") || BU_STR_EQUAL(mode, "L2d")) return brep->m_L.Count();
    if (BU_STR_EQUAL(mode, "S") || BU_STR_EQUAL(mode, "SCV") ||
	BU_STR_EQUAL(mode, "SK") || BU_STR_EQUAL(mode, "SK2d") ||
	BU_STR_EQUAL(mode, "SN") || BU_STR_EQUAL(mode, "SUV") ||
	BU_STR_EQUAL(mode, "SUVP")) return brep->m_S.Count();
    if (BU_STR_EQUAL(mode, "T") || BU_STR_EQUAL(mode, "T2d")) return brep->m_T.Count();
    if (BU_STR_EQUAL(mode, "V")) return brep->m_V.Count();
    if (BU_STR_EQUAL(mode, "F") || BU_STR_EQUAL(mode, "F2d") ||
	BU_STR_EQUAL(mode, "FSBB") || BU_STR_EQUAL(mode, "FSBB2d") ||
	BU_STR_EQUAL(mode, "FTBB") || BU_STR_EQUAL(mode, "FTBB2d") ||
	BU_STR_EQUAL(mode, "FTD") || BU_STR_EQUAL(mode, "I") ||
	BU_STR_EQUAL(mode, "CDT") || BU_STR_EQUAL(mode, "CDT2d") ||
	BU_STR_EQUAL(mode, "CDTm2d") || BU_STR_EQUAL(mode, "CDTp2d") ||
	BU_STR_EQUAL(mode, "CDTw") || BU_STR_EQUAL(mode, "CDTn") ||
	BU_STR_EQUAL(mode, "CDTn2d") || BU_STR_EQUAL(mode, "CDTnw")) return brep->m_F.Count();
    return 0;
}

static int
brep_plot_index_value(int *value, const char *text, int limit)
{
    if (!value || !text || !text[0] || limit <= 0 ||
	!bu_cmd_integer_from_str(value, text) || *value < 0 || *value >= limit)
	return 0;
    return 1;
}

static int
brep_plot_index_token_valid(const char *token, int limit)
{
    std::string text = token ? token : "";
    size_t start = 0;
    while (start <= text.size()) {
	size_t end = text.find_first_of(",/;", start);
	std::string part = text.substr(start, end == std::string::npos ?
	    std::string::npos : end - start);
	if (part.empty())
	    return 0;
	size_t range = part.find_first_of("-:");
	if (range == std::string::npos) {
	    int value = 0;
	    if (!brep_plot_index_value(&value, part.c_str(), limit))
		return 0;
	} else {
	    int first = 0, last = 0;
	    std::string first_text = part.substr(0, range);
	    std::string last_text = part.substr(range + 1);
	    if (!brep_plot_index_value(&first, first_text.c_str(), limit) ||
		!brep_plot_index_value(&last, last_text.c_str(), limit))
		return 0;
	}
	if (end == std::string::npos)
	    break;
	start = end + 1;
    }
    return 1;
}

static int
brep_plot_index_token_prefix(const char *token)
{
    if (!token)
	return 0;
    for (const char *p = token; *p; p++) {
	if (!isdigit((unsigned char)*p) && *p != '-' && *p != ':' &&
	    *p != ',' && *p != '/' && *p != ';')
	    return 0;
    }
    return 1;
}

/* Validate the plot arguments against the selected BREP.  The generic
 * command schema deliberately treats the index operand as raw text so it can
 * represent the historical list/range syntax; this command-owned layer adds
 * the geometry-dependent bounds check. */
static int
brep_plot_indices_valid(const struct rt_brep_internal *bip, const char *mode,
	int argc, const char * const *indices)
{
    int component_count = brep_plot_component_count(bip, mode);
    if (!bip || !bip->brep || !mode || component_count <= 0)
	return argc == 0;
    for (int i = 0; i < argc; i++) {
	if (!brep_plot_index_token_valid(indices[i], component_count))
	    return 0;
    }
    return 1;
}

static void
brep_plot_context_complete(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    struct brep_completion_line line;
    size_t plot_index = (size_t)-1;
    size_t mode_index = (size_t)-1;
    const char *object_name = NULL;
    struct directory *dp = RT_DIR_NULL;
    struct rt_db_internal intern;
    int component_count;

    if (!gedp || !gedp->dbip || !input || !result ||
	brep_completion_line_parse(&line, input, cursor_pos) != 0)
	return;

    for (size_t i = 1; i + 1 < line.argc; i++) {
	if (!BU_STR_EQUAL(line.argv[i], "plot"))
	    continue;
	for (size_t m = 0; brep_plot_modes[m]; m++) {
	    if (BU_STR_EQUAL(line.argv[i + 1], brep_plot_modes[m])) {
		plot_index = i;
		mode_index = i + 1;
		break;
	    }
	}
	if (mode_index != (size_t)-1)
	    break;
    }
    if (mode_index == (size_t)-1 || line.cursor_arg <= mode_index || plot_index <= 1) {
	brep_completion_line_free(&line);
	return;
    }

    object_name = line.argv[plot_index - 1];
    dp = db_lookup(gedp->dbip, object_name, LOOKUP_QUIET);
    RT_DB_INTERNAL_INIT(&intern);
    if (dp == RT_DIR_NULL || rt_db_get_internal(&intern, dp, gedp->dbip, NULL) < 0) {
	brep_completion_line_free(&line);
	return;
    }
    if (intern.idb_type != ID_BREP) {
	rt_db_free_internal(&intern);
	brep_completion_line_free(&line);
	return;
    }
    component_count = brep_plot_component_count((struct rt_brep_internal *)intern.idb_ptr,
	line.argv[mode_index]);
    if (line.argc == mode_index + 2 &&
	(BU_STR_EQUAL(line.argv[mode_index + 1], HELPFLAG) ||
	 BU_STR_EQUAL(line.argv[mode_index + 1], PURPOSEFLAG))) {
	rt_db_free_internal(&intern);
	brep_completion_line_free(&line);
	return;
    }

    /* Validate completed index tokens against the selected BREP.  A partial
     * current token remains editable; complete numeric values and ranges must
     * be in bounds before execution is allowed to proceed. */
    for (size_t i = mode_index + 1; i < line.argc; i++) {
	if (!brep_plot_index_token_valid(line.argv[i], component_count)) {
	    int current = (i == line.cursor_arg);
	    int numeric = line.argv[i][0] &&
		strspn(line.argv[i], "0123456789") == strlen(line.argv[i]);
	    if (!current || numeric || !brep_plot_index_token_prefix(line.argv[i]) ||
		line.cursor_arg >= line.argc) {
		result->state = BU_CMD_VALIDATE_INVALID;
		result->hint = "BREP plot component index is out of range or malformed";
		if (result->completion_candidates) {
		    bu_argv_free(result->completion_count,
			(char **)result->completion_candidates);
		    result->completion_candidates = NULL;
		}
		result->completion_count = 0;
		rt_db_free_internal(&intern);
		brep_completion_line_free(&line);
		return;
	    }
	}
    }

    if (line.cursor_arg > mode_index) {
	const char *seed = line.cursor_arg < line.argc ? line.argv[line.cursor_arg] : "";
	if (!seed[0] || strspn(seed, "0123456789") == strlen(seed)) {
	    if (result->completion_candidates)
		bu_argv_free(result->completion_count,
		    (char **)result->completion_candidates);
	    result->completion_candidates = (const char **)bu_calloc(
		(size_t)component_count + 1, sizeof(char *), "BREP plot index completions");
	    result->completion_count = 0;
	    for (int i = 0; i < component_count; i++) {
		char value[32] = {0};
		snprintf(value, sizeof(value), "%d", i);
		if (!seed[0] || !strncmp(value, seed, strlen(seed)))
		    result->completion_candidates[result->completion_count++] = bu_strdup(value);
	    }
	    result->completion_type = BU_CMD_VALUE_INTEGER;
	    if (!result->completion_count) {
		bu_free((void *)result->completion_candidates, "empty BREP plot index completions");
		result->completion_candidates = NULL;
	    }
	}
    }

    rt_db_free_internal(&intern);
    brep_completion_line_free(&line);
}

static void
brep_show_help(struct ged *gedp)
{
    char *help = bu_cmd_tree_describe(&brep_tree);

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "brep native tree help");
    }
}

static int
brep_tree_execute(void *context, int argc, const char *argv[])
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)context;
    const struct bu_cmd_tree_node *node = bu_cmd_tree_find_subcommand(&brep_tree, argv[0]);
    struct bu_cmd_validate_result validation = BU_CMD_VALIDATE_RESULT_NULL;
    int ret = BRLCAD_ERROR;

    if (!node)
	return BRLCAD_ERROR;
    /* Existing operation handlers own their concise purpose/usage responses. */
    if (!(argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG))) {
	if (bu_cmd_schema_validate(node->schema, (size_t)(argc - 1), argv + 1,
		(size_t)(argc - 1), &validation) != 0 ||
		validation.state != BU_CMD_VALIDATE_VALID) {
	    if (validation.hint)
		bu_vls_printf(gb->gedp->ged_result_str, "%s\n", validation.hint);
	    bu_cmd_validate_result_clear(&validation);
	    return BRLCAD_ERROR;
	}
	bu_cmd_validate_result_clear(&validation);
    }

	if (node->schema == &brep_plot_schema && argc > 1 &&
	    gb->intern.idb_type == ID_BREP &&
	    !(argc == 3 && (BU_STR_EQUAL(argv[2], HELPFLAG) ||
		BU_STR_EQUAL(argv[2], PURPOSEFLAG))) &&
	    !brep_plot_indices_valid((const struct rt_brep_internal *)gb->intern.idb_ptr,
		argv[1], argc - 2, argv + 2)) {
	bu_vls_printf(gb->gedp->ged_result_str,
	    "BREP plot component index is out of range or malformed\n");
	return BRLCAD_ERROR;
    }

    if (bu_cmd(_brep_cmds, argc, argv, 0, context, &ret) != BRLCAD_OK) {
	bu_vls_printf(gb->gedp->ged_result_str, "subcommand %s not defined", argv[0]);
	return BRLCAD_ERROR;
    }
    return ret;
}

extern "C" int
ged_brep_core(struct ged *gedp, int argc, const char *argv[])
{
    struct brep_root_args args = {0, BU_COLOR_INIT_ZERO, 0, 100};
    struct _ged_brep_info gb;
    struct bu_color *color = NULL;
    struct bu_list *vlfree = &rt_vlfree;
    int object_index;
    int command_index;
    int ret = BRLCAD_ERROR;

    if (UNLIKELY(!gedp || !argc || !argv))
	return BRLCAD_ERROR;
    bu_vls_trunc(gedp->ged_result_str, 0);
    argc--;
    argv++;
    if (!argc) {
	brep_show_help(gedp);
	return BRLCAD_OK;
    }

    object_index = bu_cmd_schema_parse(&brep_root_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (object_index < 0) {
	brep_show_help(gedp);
	return BRLCAD_ERROR;
    }

    if (args.help) {
	command_index = object_index + 1;
	if (command_index < argc && bu_cmd_tree_find_subcommand(&brep_tree, argv[command_index])) {
	    const char *child_help[] = {argv[command_index], HELPFLAG};
	    int ignored = BRLCAD_OK;
	    gb.gedp = gedp;
	    (void)bu_cmd_tree_dispatch(&brep_tree, &gb, 2, child_help, &ignored);
	} else {
	    brep_show_help(gedp);
	}
	return BRLCAD_OK;
    }

    if (object_index >= argc) {
	bu_vls_printf(gedp->ged_result_str, "object and subcommand required\n");
	brep_show_help(gedp);
	return BRLCAD_ERROR;
    }
    command_index = object_index + 1;
    if (command_index >= argc || !bu_cmd_tree_find_subcommand(&brep_tree, argv[command_index])) {
	bu_vls_printf(gedp->ged_result_str, "no valid subcommand specified\n");
	brep_show_help(gedp);
	return BRLCAD_ERROR;
    }

    struct bu_cmd_validate_result root_validation = BU_CMD_VALIDATE_RESULT_NULL;
    if (bu_cmd_schema_validate(&brep_root_schema, (size_t)(object_index + 1), argv,
	    (size_t)(object_index + 1), &root_validation) != 0 ||
	root_validation.state != BU_CMD_VALIDATE_VALID) {
	if (root_validation.hint)
	    bu_vls_printf(gedp->ged_result_str, "%s\n", root_validation.hint);
	bu_cmd_validate_result_clear(&root_validation);
	return BRLCAD_ERROR;
    }
    bu_cmd_validate_result_clear(&root_validation);

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    gb.gedp = gedp;
    gb.verbosity = args.verbosity;
    gb.plotres = args.plotres;
    gb.wdbp = wdb_dbopen(gb.gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    gb.cmds = _brep_cmds;
    gb.solid_name = argv[object_index];
    gb.dp = db_lookup(gedp->dbip, gb.solid_name.c_str(), LOOKUP_NOISY);
    if (gb.dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, ": %s is not a solid or does not exist in database",
	    gb.solid_name.c_str());
	return BRLCAD_ERROR;
    }
    if (gb.dp->d_addr == RT_DIR_PHONY_ADDR) {
	bu_vls_printf(gedp->ged_result_str, ": %s is not a real solid", gb.solid_name.c_str());
	return BRLCAD_ERROR;
    }

    GED_DB_GET_INTERN(gedp, &gb.intern, gb.dp, bn_mat_identity, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&gb.intern);
    gb.vbp = bv_vlblock_init(vlfree, 32);
    if (bu_cmd_schema_option_present(&brep_root_schema, (size_t)(object_index + 1),
	argv, "color")) {
	BU_GET(color, struct bu_color);
	*color = args.color;
    }
    gb.color = color;

    if (bu_cmd_tree_dispatch(&brep_tree, &gb, argc - command_index,
	argv + command_index, &ret) != 0)
	ret = BRLCAD_ERROR;
    bv_vlblock_free(gb.vbp);
    rt_db_free_internal(&gb.intern);
    if (color)
	BU_PUT(color, struct bu_color);
    return ret;
}

#include "../include/plugin.h"

static const char * const dplot_modes[] = {"ssx", "isocsx", "fcurves", "lcurves", "faces", NULL};
static const struct bu_cmd_operand dplot_operands[] = {
    BU_CMD_OPERAND("logfile", BU_CMD_VALUE_FILE, 1, 1,
	"BREP diagnostic log", "ged.file_path"),
    BU_CMD_OPERAND_KEYWORDS("operation", BU_CMD_VALUE_KEYWORD, 1, 1,
	"Diagnostic plot operation", NULL, dplot_modes),
    BU_CMD_OPERAND("indices", BU_CMD_VALUE_INTEGER, 0, 2,
	"Optional surface and isocurve pair indices", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema dplot_cmd_schema = {
    "dplot", "Display BREP diagnostic plot data", NULL, dplot_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static int
ged_brep_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    int ret = ged_cmd_tree_validate(gedp, &brep_tree, input, cursor_pos, result);
    if (ret == 0)
	brep_plot_context_complete(gedp, input, cursor_pos, result);
    return ret;
}

static int
ged_brep_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &brep_tree, input, analysis);
}

static char *
ged_brep_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&brep_tree);
}

static int
ged_brep_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&brep_tree, msgs);
}

static const struct ged_cmd_grammar ged_brep_grammar = {
    "brep", "Inspect, convert, plot, and repair BREP objects", ged_brep_grammar_validate,
    ged_brep_grammar_analyze, ged_brep_grammar_json, ged_brep_grammar_lint
};

#define GED_BREP_COMMANDS(X, XID, N, NID, G, GID) \
    G(brep, ged_brep_core, GED_CMD_DEFAULT, &ged_brep_grammar) \
    N(dplot, ged_dplot_core, GED_CMD_DEFAULT, &dplot_cmd_schema)

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_BREP_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_brep", 1, GED_BREP_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
