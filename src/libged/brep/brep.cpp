/*                        B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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

#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
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
    GED_DB_GET_INTERNAL(gedp, &intern2, dp2, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
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
    mk_brep(gedp->ged_wdbp, argv[3], (void *)(bip->brep));
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

    const struct bg_tess_tol *ttol = (const struct bg_tess_tol *)&gb->gedp->ged_wdbp->wdb_ttol;
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

    if (wdb_export(gedp->ged_wdbp, bot_name, (void *)bot, ID_BOT, 1.0)) {
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

    const struct bg_tess_tol *ttol = (const struct bg_tess_tol *)&gedp->ged_wdbp->wdb_ttol;
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
	GED_DB_GET_INTERNAL(gedp, &intern, dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
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

	if (wdb_export(gedp->ged_wdbp, bu_vls_cstr(&bot_name), (void *)bot, ID_BOT, 1.0)) {
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
    // TODO - this needs a better help output - it has actual options per bu_opt_desc...
    if (_brep_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;

    argc--;argv++;

    int no_evaluation = 0;
    struct bu_vls bname = BU_VLS_INIT_ZERO;
    struct bu_vls suffix = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&suffix, ".brep");

    struct bu_opt_desc d[3];
    BU_OPT(d[0], "", "no-evaluation", "",         NULL,        &no_evaluation, "if converting a comb object, create a CSG brep tree rather than evluating booleans");
    BU_OPT(d[1], "", "suffix",        "str",      &bu_opt_vls, &suffix,        "suffix for use in no-evalution object naming");
    BU_OPT_NULL(d[2]);

    struct ged *gedp = gb->gedp;
    if (gb->intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is already a brep\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    bu_opt_parse(NULL, argc, argv, d);

    if (no_evaluation && gb->intern.idb_type == ID_COMBINATION) {
	struct bu_vls bname_suffix;
	bu_vls_init(&bname_suffix);
	bu_vls_sprintf(&bname_suffix, "%s%s", gb->solid_name.c_str(), bu_vls_cstr(&suffix));
	if (db_lookup(gedp->dbip, bu_vls_cstr(&bname_suffix), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_cstr(&bname_suffix));
	    bu_vls_free(&bname);
	    bu_vls_free(&suffix);
	    bu_vls_free(&bname_suffix);
	    return BRLCAD_OK;
	}

	// brep_conversion_comb frees the intern, so make a new copy specifically for it to avoid
	// a double-free with the top level cleanup of gb->intern
	struct rt_db_internal intern;
	GED_DB_GET_INTERNAL(gedp, &intern, gb->dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
	RT_CK_DB_INTERNAL(&intern);

	brep_conversion_comb(&intern, bu_vls_cstr(&bname_suffix), bu_vls_cstr(&suffix), gedp->ged_wdbp, mk_conv2mm);
	bu_vls_free(&bname_suffix);
	bu_vls_free(&bname);
	bu_vls_free(&suffix);
	return BRLCAD_OK;
    }

    // Won't need the suffix if we've gotten this far
    bu_vls_free(&suffix);

    if (argc == 0) {
	/* brep obj */
	bu_vls_sprintf(&bname, "%s.brep", gb->solid_name.c_str());
    } else {
	/* brep obj brepname/suffix */
	bu_vls_sprintf(&bname, "%s", argv[0]);
    }

    // Attempt to evalute to a single object
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
	ret = mk_brep(gedp->ged_wdbp, bu_vls_cstr(&bname), brep);
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

    // Delete the old object
    const char *av[3];
    char *ncpy = bu_strdup(gb->solid_name.c_str());
    av[0] = "kill";
    av[1] = ncpy;
    av[2] = NULL;
    (void)ged_exec(gb->gedp, 2, (const char **)av);
    bu_free(ncpy, "free name cpy");

    // Make the new one
    if (mk_brep(gb->gedp->ged_wdbp, gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
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
    if (bu_opt_int(gedp->ged_result_str, 1, &argv[2], (void *)&i) < 0) {
	return BRLCAD_ERROR;
    }
    if (bu_opt_int(gedp->ged_result_str, 1, &argv[3], (void *)&j) < 0) {
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
    GED_DB_GET_INTERNAL(gedp, &intern2, dp2, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
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

    const char *nview = getenv("GED_TEST_NEW_CMD_FORMS");
    if (BU_STR_EQUAL(nview, "1")) {
	struct bview *view = gedp->ged_gvp;
	struct bu_ptbl *vobjs = (view->independent) ? view->gv_view_objs : view->gv_view_shared_objs;
	bv_vlblock_to_objs(vobjs, "brep_intersect::", gb->vbp, view, gedp->free_scene_obj, &gedp->vlfree);
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
	GED_DB_PUT_INTERNAL(gedp, gb->dp, &gb->intern, &rt_uniresource, BRLCAD_ERROR);
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

    // Delete the old object
    const char *av[3];
    char *ncpy = bu_strdup(gb->solid_name.c_str());
    av[0] = "kill";
    av[1] = ncpy;
    av[2] = NULL;
    (void)ged_exec(gb->gedp, 2, (const char **)av);
    bu_free(ncpy, "free name cpy");

    // Make the new one
    if (mk_brep(gb->gedp->ged_wdbp, gb->solid_name.c_str(), (void *)b_ip->brep)) {
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

    int object_per_face = 0;
    double thickness = 0.0;
    struct bu_vls ocomb = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[4];
    BU_OPT(d[0], "t", "thickness", "#", &bu_opt_fastf_t, &thickness , "default plate mode thickness");
    BU_OPT(d[1], "o", "output-object", "<name>", &bu_opt_vls, &ocomb, "specify an output object root name");
    BU_OPT(d[2], "O", "object-per-face", "", NULL, &object_per_face, "create one brep object per face");
    BU_OPT_NULL(d[3]);
    argc = bu_opt_parse(NULL, argc, argv, d);

    if (!bu_vls_strlen(&ocomb)) {
	// If the caller didn't provide a name, generate one
	bu_vls_sprintf(&ocomb, "%s.plates", gb->solid_name.c_str());
    }

    // If we have anything left, it should be indices
    std::set<int> elements;
    if (argc) {
	if (_brep_indices(elements, gb->gedp->ged_result_str, argc, argv) != BRLCAD_OK) {
	    bu_vls_free(&ocomb);
	    return BRLCAD_ERROR;
	}
    }

    if (db_lookup(gedp->dbip, bu_vls_cstr(&ocomb), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, ": %s already exists.", bu_vls_cstr(&ocomb));
	bu_vls_free(&ocomb);
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
	    mk_brep(gedp->ged_wdbp, bu_vls_cstr(&fbrep_name), fbrep);
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
	ret = mk_brep(gedp->ged_wdbp, bu_vls_cstr(&ocomb), brep);
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
	ret = mk_lcomb(gedp->ged_wdbp, bu_vls_cstr(&ocomb), &wcomb, 0, NULL, NULL, NULL, 0);
    }
    return ret;
}

extern "C" int
_brep_cmd_tikz(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> tikz <outfile>";
    const char *purpose_string = "generate Tikz 3dplot of BRep object";
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

const struct bu_cmdtab _brep_cmds[] = {
    { "bool",            _brep_cmd_boolean},
    { "bot",             _brep_cmd_bot},
    { "bots",            _brep_cmd_bots},
    { "brep",            _brep_cmd_brep},
    { "csg",             _brep_cmd_csg},
    { "flip",            _brep_cmd_flip},
    { "info",            _brep_cmd_info},
    { "intersect",       _brep_cmd_intersect},
    { "pick",            _brep_cmd_pick},
    { "plate_mode",      _brep_cmd_plate_mode},
    { "plot",            _brep_cmd_plot},
    { "selection",       _brep_cmd_selection},
    { "solid",           _brep_cmd_solid},
    { "split",           _brep_cmd_split},
    { "shrink_surfaces", _brep_cmd_shrink_surfaces},
    { "tikz",            _brep_cmd_tikz},
    { "valid",           _brep_cmd_valid},
    { "weld",            _brep_cmd_weld},
    { (char *)NULL,      NULL}
};


static int
_ged_brep_opt_color(struct bu_vls *msg, size_t argc, const char **argv, void *set_c)
{
    struct bu_color **set_color = (struct bu_color **)set_c;
    BU_GET(*set_color, struct bu_color);
    return bu_opt_color(msg, argc, argv, (void *)(*set_color));
}

extern "C" int
ged_brep_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;
    struct _ged_brep_info gb;
    gb.gedp = gedp;
    gb.cmds = _brep_cmds;
    gb.verbosity = 0;
    struct bu_color *color = NULL;
    int plotres = 100;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return BRLCAD_ERROR;
    }

    // Clear results
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the brep command - start processing args
    argc--; argv++;

    // See if we have any high level options set
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",    "",      NULL,                 &help,         "Print help");
    BU_OPT(d[1], "C", "color",   "r/g/b", &_ged_brep_opt_color, &color,        "Set color");
    BU_OPT(d[2], "v", "verbose", "",      NULL,                 &gb.verbosity, "Verbose output");
    BU_OPT(d[3], "",  "plotres", "#",     &bu_opt_int,          &plotres,      "Plotting resolution");
    BU_OPT_NULL(d[3]);

    gb.gopts = d;


    const char *bargs_help = "[options] <objname> subcommand [args]";
    struct bu_opt_desc *bdesc = (struct bu_opt_desc *)d;
    const struct bu_cmdtab *bcmds = (const struct bu_cmdtab *)_brep_cmds;

    if (!argc) {
	_ged_subcmd_help(gedp, bdesc, bcmds, "brep", bargs_help, &gb, 0, NULL);
	return BRLCAD_OK;
    }



    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_brep_cmds, argv[i]) == BRLCAD_OK) {
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
	    _ged_subcmd_help(gedp, bdesc, bcmds, "brep", bargs_help, &gb, argc, argv);
	} else {
	    _ged_subcmd_help(gedp, bdesc, bcmds, "brep", bargs_help, &gb, 0, NULL);
	}
	return BRLCAD_OK;
    }

    // Must have a subcommand
    if (cmd_pos == -1) {
	bu_vls_printf(gedp->ged_result_str, ": no valid subcommand specified\n");
	_ged_subcmd_help(gedp, bdesc, bcmds, "brep", bargs_help, &gb, 0, NULL);
	return BRLCAD_ERROR;
    }


    if (opt_ret != 1) {
	bu_vls_printf(gedp->ged_result_str, ": no object specified before subcommand\n");
	bu_vls_printf(gedp->ged_result_str, "brep [options] <objname> subcommand [args]\n");
	if (color) {
	    BU_PUT(color, struct bu_color);
	}
	return BRLCAD_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    gb.solid_name = std::string(argv[0]);
    gb.dp = db_lookup(gedp->dbip, gb.solid_name.c_str(), LOOKUP_NOISY);
    if (gb.dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, ": %s is not a solid or does not exist in database", gb.solid_name.c_str());
	if (color) {
	    BU_PUT(color, struct bu_color);
	}
	return BRLCAD_ERROR;
    } else {
	int real_flag = (gb.dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	if (!real_flag) {
	    /* solid doesn't exist */
	    bu_vls_printf(gedp->ged_result_str, ": %s is not a real solid", gb.solid_name.c_str());
	    if (color) {
		BU_PUT(color, struct bu_color);
	    }
	    return BRLCAD_ERROR;
	}
    }

    GED_DB_GET_INTERNAL(gedp, &gb.intern, gb.dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&gb.intern);

    gb.vbp = bv_vlblock_init(&RTG.rtg_vlfree, 32);
    gb.color = color;
    gb.plotres = plotres;

    // Jump the processing past any options specified
    argc = argc - cmd_pos;
    argv = &argv[cmd_pos];

    int ret;
    if (bu_cmd(_brep_cmds, argc, argv, 0, (void *)&gb, &ret) == BRLCAD_OK) {
	rt_db_free_internal(&gb.intern);
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }

    bv_vlblock_free(gb.vbp);
    gb.vbp = (struct bv_vlblock *)NULL;
    rt_db_free_internal(&gb.intern);
    return BRLCAD_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
struct ged_cmd_impl brep_cmd_impl = { "brep", ged_brep_core, GED_CMD_DEFAULT };
const struct ged_cmd brep_cmd = { &brep_cmd_impl };
struct ged_cmd_impl dplot_cmd_impl = { "dplot", ged_dplot_core, GED_CMD_DEFAULT };
const struct ged_cmd dplot_cmd = { &dplot_cmd_impl };
const struct ged_cmd *brep_cmds[] = { &brep_cmd, &dplot_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  brep_cmds, 2 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
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

