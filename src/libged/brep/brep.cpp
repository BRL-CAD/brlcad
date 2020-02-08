/*                         B R E P . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2019 United States Government as represented by
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
/** @file libged/brep.c
 *
 * The brep command.
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

#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "./ged_brep.h"

/* TODO - get rid of the need for brep_specific at this level */
#include "../librt/primitives/brep/brep_local.h"

/* FIXME: how should we set up brep functionality without introducing
 * lots of new public librt functions?  right now, we reach into librt
 * directly and export what we need from brep_debug.cpp which sucks.
 */
extern "C" {
RT_EXPORT extern int brep_command(struct bu_vls *vls, const char *solid_name, struct rt_wdb *wdbp, struct bu_color *color, const struct bg_tess_tol *ttol, const struct bn_tol *tol, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, int argc, const char *argv[], char *commtag);
RT_EXPORT extern int rt_brep_boolean(struct rt_db_internal *out, const struct rt_db_internal *ip1, const struct rt_db_internal *ip2, db_op_t operation);
}

static int
_ged_brep_plate_mode_set(struct ged *gedp, struct directory *dp, const char *val)
{
    struct bu_attribute_value_set avs;
    if (!gedp || !dp || !val) return GED_ERROR;

    double local2base = gedp->ged_wdbp->dbip->dbi_local2base;

    // Make sure we can get attributes
    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str, "Error setting plate mode value\n");
	return GED_ERROR;
    };

    if (BU_STR_EQUIV(val, "cos")) {
	(void)bu_avs_add(&avs, "_plate_mode_nocos", "0");
	if (db5_replace_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
	    bu_vls_printf(gedp->ged_result_str, "Error setting plate mode value\n");
	    return GED_ERROR;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s", val);
	}
	return GED_OK;
    }

    if (BU_STR_EQUIV(val, "nocos")) {
	(void)bu_avs_add(&avs, "_plate_mode_nocos", "1");
	if (db5_replace_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
	    bu_vls_printf(gedp->ged_result_str, "Error setting plate mode value\n");
	    return GED_ERROR;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s", val);
	}
	return GED_OK;
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
    if (db5_replace_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
    	bu_vls_printf(gedp->ged_result_str, "Error setting plate mode value\n");
	return GED_ERROR;
    } else {
    	bu_vls_printf(gedp->ged_result_str, "%s", val);
    }
    return GED_OK;
}



static int
_ged_brep_to_bot(struct ged *gedp, const char *obj_name, const struct rt_brep_internal *bi, const char *bot_name, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    if (!gedp || !bi || !bot_name || !ttol || !tol) return GED_ERROR;

    int fcnt, fncnt, ncnt, vcnt;
    int *faces = NULL;
    fastf_t *vertices = NULL;
    int *face_normals = NULL;
    fastf_t *normals = NULL;

    struct bg_tess_tol cdttol;
    cdttol.abs = ttol->abs;
    cdttol.rel = ttol->rel;
    cdttol.norm = ttol->norm;
    ON_Brep_CDT_State *s_cdt = ON_Brep_CDT_Create((void *)bi->brep, obj_name);
    ON_Brep_CDT_Tol_Set(s_cdt, &cdttol);
    if (ON_Brep_CDT_Tessellate(s_cdt, 0, NULL) == -1) {
	bu_vls_printf(gedp->ged_result_str, "tessellation failed\n");
	ON_Brep_CDT_Destroy(s_cdt);
	return GED_ERROR;
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
	return GED_ERROR;
    }

    return GED_OK;
}

// Right now this is just a quick and dirty function to exercise the libbrep logic...
static int
_ged_breps_to_bots(struct ged *gedp, int obj_cnt, const char **obj_names, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    double ovlp_max_smallest = DBL_MAX;
    if (!gedp || obj_cnt <= 0 || !obj_names || !ttol || !tol) return GED_ERROR;

    struct bg_tess_tol cdttol;
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
	if ((dp = db_lookup(gedp->ged_wdbp->dbip, obj_names[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a solid or does not exist in database", obj_names[i]);
	    return GED_ERROR;
	}
	GED_DB_GET_INTERNAL(gedp, &intern, dp, bn_mat_identity, &rt_uniresource, GED_ERROR);
	RT_CK_DB_INTERNAL(&intern);
	bi = (struct rt_brep_internal*)intern.idb_ptr;
	if (!RT_BREP_TEST_MAGIC(bi)) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a brep solid", obj_names[i]);
	    return GED_ERROR;
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
	return GED_ERROR;
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
	    return GED_ERROR;
	}
	bu_vls_free(&bot_name);
    }

    return GED_OK;
}

static int
selection_command(
	struct ged *gedp,
	struct rt_db_internal *ip,
	int argc,
	const char *argv[])
{
    int i;
    struct rt_selection_set *selection_set;
    struct bu_ptbl *selections;
    struct rt_selection *new_selection;
    struct rt_selection_query query;
    const char *cmd, *solid_name, *selection_name;

    /*  0         1          2         3
     * brep <solid_name> selection subcommand
     */
    if (argc < 4) {
	return -1;
    }

    solid_name = argv[1];
    cmd = argv[3];

    if (BU_STR_EQUAL(cmd, "append")) {
	/* append to named selection - selection is created if it doesn't exist */
	void (*free_selection)(struct rt_selection *);

	/*        4         5      6      7     8    9    10
	 * selection_name startx starty startz dirx diry dirz
	 */
	if (argc != 11) {
	    bu_log("wrong args for selection append");
	    return -1;
	}
	selection_name = argv[4];

	/* find matching selections */
	query.start[X] = atof(argv[5]);
	query.start[Y] = atof(argv[6]);
	query.start[Z] = atof(argv[7]);
	query.dir[X] = atof(argv[8]);
	query.dir[Y] = atof(argv[9]);
	query.dir[Z] = atof(argv[10]);
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

	/*        4       5  6  7
	 * selection_name dx dy dz
	 */
	if (argc != 8) {
	    return -1;
	}
	selection_name = argv[4];

	selection_set = ged_get_selection_set(gedp, solid_name, selection_name);
	selections = &selection_set->selections;

	if (BU_PTBL_LEN(selections) < 1) {
	    return -1;
	}

	for (i = 0; i < (int)BU_PTBL_LEN(selections); ++i) {
	    int ret;
	    operation.type = RT_SELECTION_TRANSLATION;
	    operation.parameters.tran.dx = atof(argv[5]);
	    operation.parameters.tran.dy = atof(argv[6]);
	    operation.parameters.tran.dz = atof(argv[7]);

	    ret = ip->idb_meth->ft_process_selection(ip, gedp->ged_wdbp->dbip,
		    (struct rt_selection *)BU_PTBL_GET(selections, i), &operation);

	    if (ret != 0) {
		return ret;
	    }
	}
    }

    return 0;
}

int
ged_brep(struct ged *gedp, int argc, const char *argv[])
{
    struct bn_vlblock*vbp;
    const char *solid_name;
    static const char *usage = "brep <obj> [command|brepname|suffix] ";
    struct directory *ndp;
    struct rt_db_internal intern;
    struct rt_brep_internal* bi;
    struct brep_specific* bs;
    struct soltab *stp;
    struct bu_color color = BU_COLOR_INIT_ZERO;
    char commtag[64];
    char namebuf[65];
    int i, j, real_flag, valid_command, ret;
    const char *commands[] = {"info", "plot", "translate", "intersect", "csg", "u", "i", "-"};
    int num_commands = (int)(sizeof(commands) / sizeof(const char *));
    db_op_t op = DB_OP_NULL;
    int opt_ret = 0;
    struct bu_opt_desc d[2];
    BU_OPT(d[0], "C", "color", "r/g/b", &bu_opt_color, &color, "Set color");
    BU_OPT_NULL(d[1]);

    opt_ret = bu_opt_parse(NULL, argc, argv, d);

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 2 || opt_ret < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n\t%s\n", argv[0], usage);
	bu_vls_printf(gedp->ged_result_str, "commands:\n");
	bu_vls_printf(gedp->ged_result_str, "\tvalid          - report on validity of specific BREP\n");
	bu_vls_printf(gedp->ged_result_str, "\tinfo           - return count information for specific BREP\n");
	bu_vls_printf(gedp->ged_result_str, "\tinfo S [index] - return information for specific BREP 'surface'\n");
	bu_vls_printf(gedp->ged_result_str, "\tinfo F [index] - return information for specific BREP 'face'\n");
	bu_vls_printf(gedp->ged_result_str, "\tplot           - plot entire BREP\n");
	bu_vls_printf(gedp->ged_result_str, "\tplot S [index] - plot specific BREP 'surface'\n");
	bu_vls_printf(gedp->ged_result_str, "\tplot F [index] - plot specific BREP 'face'\n");
	bu_vls_printf(gedp->ged_result_str, "\tplate_mode #/cos/nocos  - if brep is plate mode, set its thickness in current db units or toggle cos/nocos mode\n");
	bu_vls_printf(gedp->ged_result_str, "\tcsg            - convert BREP to implicit primitive CSG tree\n");
	bu_vls_printf(gedp->ged_result_str, "\tflip           - flip all faces on BREP\n");
	bu_vls_printf(gedp->ged_result_str, "\ttikz [file]    - generate a Tikz LaTeX version of the B-Rep edges\n");
	bu_vls_printf(gedp->ged_result_str, "\ttranslate SCV index i j dx dy dz - translate a surface control vertex\n");
	bu_vls_printf(gedp->ged_result_str, "\tintersect <obj2> <i> <j> [PP|PC|PS|CC|CS|SS] - BREP intersections\n");
	bu_vls_printf(gedp->ged_result_str, "\tu|i|- <obj2> <output>     - BREP boolean evaluations\n");
	bu_vls_printf(gedp->ged_result_str, "\t[brepname]                - convert the non-BREP object to BREP form\n");
	bu_vls_printf(gedp->ged_result_str, "\t --no-evaluation [suffix] - convert non-BREP comb to unevaluated BREP form\n");
	return GED_HELP;
    }

    if (argc < 2 || argc > 11) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    solid_name = argv[1];
    if ((ndp = db_lookup(gedp->ged_wdbp->dbip,  solid_name, LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Error: %s is not a solid or does not exist in database", solid_name);
	return GED_ERROR;
    } else {
	real_flag = (ndp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
    }

    if (!real_flag) {
	/* solid doesn't exist - don't kill */
	bu_vls_printf(gedp->ged_result_str, "Error: %s is not a real solid", solid_name);
	return GED_OK;
    }

    GED_DB_GET_INTERNAL(gedp, &intern, ndp, bn_mat_identity, &rt_uniresource, GED_ERROR);


    RT_CK_DB_INTERNAL(&intern);
    bi = (struct rt_brep_internal*)intern.idb_ptr;

    if (BU_STR_EQUAL(argv[2], "valid")) {
	int valid = rt_brep_valid(gedp->ged_result_str, &intern, 0);
	return (valid) ? GED_OK : GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[2], "intersect")) {
	/* handle surface-surface intersection */
	struct rt_db_internal intern2;

	/* we need exactly 6 or 7 arguments */
	if (argc != 6 && argc != 7) {
	    bu_vls_printf(gedp->ged_result_str, "There should be 6 or 7 arguments for intersection.\n");
	    bu_vls_printf(gedp->ged_result_str, "See the usage for help.\n");
	    return GED_ERROR;
	}

	/* get the other solid */
	if ((ndp = db_lookup(gedp->ged_wdbp->dbip,  argv[3], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a solid or does not exist in database", argv[3]);
	    return GED_ERROR;
	} else {
	    real_flag = (ndp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	}

	if (!real_flag) {
	    /* solid doesn't exist - don't kill */
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a real solid", argv[3]);
	    return GED_OK;
	}

	GED_DB_GET_INTERNAL(gedp, &intern2, ndp, bn_mat_identity, &rt_uniresource, GED_ERROR);

	i = atoi(argv[4]);
	j = atoi(argv[5]);
	vbp = rt_vlblock_init();

	if (argc == 6 || BU_STR_EQUAL(argv[6], "SS")) {
	    brep_intersect_surface_surface(&intern, &intern2, i, j, vbp);
	} else if (BU_STR_EQUAL(argv[6], "PP")) {
	    brep_intersect_point_point(&intern, &intern2, i, j);
	} else if (BU_STR_EQUAL(argv[6], "PC")) {
	    brep_intersect_point_curve(&intern, &intern2, i, j);
	} else if (BU_STR_EQUAL(argv[6], "PS")) {
	    brep_intersect_point_surface(&intern, &intern2, i, j);
	} else if (BU_STR_EQUAL(argv[6], "CC")) {
	    brep_intersect_curve_curve(&intern, &intern2, i, j);
	} else if (BU_STR_EQUAL(argv[6], "CS")) {
	    brep_intersect_curve_surface(&intern, &intern2, i, j);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "Invalid intersection type %s.\n", argv[6]);
	}

	_ged_cvt_vlblock_to_solids(gedp, vbp, namebuf, 0);
	bn_vlblock_free(vbp);
	vbp = (struct bn_vlblock *)NULL;

	rt_db_free_internal(&intern);
	rt_db_free_internal(&intern2);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "plate_mode")) {
	if (!rt_brep_valid(NULL, &intern, 0)) {
	    bu_vls_printf(gedp->ged_result_str, "%s is not a valid B-Rep - aborting\n", ndp->d_namep);
	    return GED_ERROR;
	}
	/* Verify brep is a plate mode brep */
	if (!rt_brep_plate_mode(&intern)) {
	    bu_vls_printf(gedp->ged_result_str, "N/A - %s is not a plate mode brep\n", ndp->d_namep);
	    return GED_ERROR;
	}

	/* we need 3 or 4 arguments */
	if (argc == 3) {
	    double thickness;
	    int nocos;
	    rt_brep_plate_mode_getvals(&thickness, &nocos, &intern);
	    thickness = gedp->ged_wdbp->dbip->dbi_base2local * thickness;
	    if (nocos) {
		bu_vls_printf(gedp->ged_result_str, "%f (NOCOS)", thickness);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "%f (COS)", thickness);
	    }
	    return GED_OK;
	} else if (argc == 4) {
	    return _ged_brep_plate_mode_set(gedp, ndp, argv[3]);
	}
	bu_vls_printf(gedp->ged_result_str, "plate_mode requires a single numerical length or a cos/nocos flag as an argument.\n");
	return GED_ERROR;
    }


    if (BU_STR_EQUAL(argv[2], "csg")) {
	/* Call csg conversion routine */
	struct bu_vls bname_csg;
	bu_vls_init(&bname_csg);
	bu_vls_sprintf(&bname_csg, "csg_%s", solid_name);
#if 0
	if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&bname_csg), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_addr(&bname_csg));
	    bu_vls_free(&bname_csg);
	    return GED_OK;
	}
#endif
	bu_vls_free(&bname_csg);
	return _ged_brep_to_csg(gedp, argv[1], 0);
    }


    if (BU_STR_EQUAL(argv[2], "bot")) {
	/* Call bot conversion routine */
	int bret = GED_ERROR;
	struct bu_vls bname_bot;
	bu_vls_init(&bname_bot);
	if (argc < 4) {
	    bu_vls_sprintf(&bname_bot, "%s.bot", solid_name);
	} else {
	    bu_vls_sprintf(&bname_bot, "%s", argv[3]);
	}

	// TODO - pass tol info down...
	bret = _ged_brep_to_bot(gedp, argv[1], bi, bu_vls_cstr(&bname_bot), (const struct bg_tess_tol *)&gedp->ged_wdbp->wdb_ttol, &gedp->ged_wdbp->wdb_tol);
	bu_vls_free(&bname_bot);
	return bret;
    }

    if (BU_STR_EQUAL(argv[2], "bots")) {
	/* Call bot conversion routine */
	int bret = GED_ERROR;
	const char **av = (const char **)bu_calloc(argc, sizeof(char *), "new argv");
	av[0] = argv[1];
	for (int iav = 3; iav < argc; iav++) {
	    av[iav-2] = argv[iav];
	}
	bret = _ged_breps_to_bots(gedp, argc-2, av, (const struct bg_tess_tol *)&gedp->ged_wdbp->wdb_ttol, &gedp->ged_wdbp->wdb_tol);
	bu_free(av, "av");
	return bret;
    }


    if (BU_STR_EQUAL(argv[2], "csgv")) {
	/* Call csg conversion routine */
	struct bu_vls bname_csg;
	bu_vls_init(&bname_csg);
	bu_vls_sprintf(&bname_csg, "csg_%s", solid_name);
	if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&bname_csg), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_addr(&bname_csg));
	    bu_vls_free(&bname_csg);
	    return GED_OK;
	}
	bu_vls_free(&bname_csg);
	return _ged_brep_to_csg(gedp, argv[1], 1);
    }


    if (BU_STR_EQUAL(argv[2], "flip")) {
	bi->brep->Flip();

	// Delete the old object
	const char *av[3];
	av[0] = "kill";
	av[1] = solid_name;
	av[2] = NULL;
	(void)ged_kill(gedp, 2, (const char **)av);

	// Make the new one
	if (mk_brep(gedp->ged_wdbp, solid_name, (void *)bi->brep)) {
	    return GED_ERROR;
	}
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "shrink-surfaces")) {
	bi->brep->ShrinkSurfaces();

	// Delete the old object
	const char *av[3];
	av[0] = "kill";
	av[1] = solid_name;
	av[2] = NULL;
	(void)ged_kill(gedp, 2, (const char **)av);

	// Make the new one
	if (mk_brep(gedp->ged_wdbp, solid_name, (void *)bi->brep)) {
	    return GED_ERROR;
	}
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "tikz")) {
	if (argc == 4) {
	    return _ged_brep_tikz(gedp, argv[1], argv[3]);
	} else {
	    return _ged_brep_tikz(gedp, argv[1], NULL);
	}
    }


    if (BU_STR_EQUAL(argv[2], "nirt")) {
	struct bu_vls log = BU_VLS_INIT_ZERO;
	vect_t xlate;
	vect_t eye;
	vect_t dir;

	GED_CHECK_VIEW(gedp, GED_ERROR);
	VSET(xlate, 0.0, 0.0, 1.0);
	MAT4X3PNT(eye, gedp->ged_gvp->gv_view2model, xlate);
	VSCALE(eye, eye, gedp->ged_wdbp->dbip->dbi_base2local);

	VMOVEN(dir, gedp->ged_gvp->gv_rotation + 8, 3);
	VSCALE(dir, dir, -1.0);

	bu_vls_sprintf(&log, "%s:\n", argv[1]);
	if (ON_Brep_Report_Faces(&log, (void *)bi->brep, eye, dir)) {
	    bu_vls_free(&log);
	    return GED_ERROR;
	}
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&log));
	bu_vls_free(&log);
	return GED_OK;
    }


    /* make sure arg isn't --no-evaluate */
    if (argc > 2 && argv[2][1] != '-') {
	op = db_str2op(argv[2]);
    }

    if (op != DB_OP_NULL) {
	/* test booleans on brep.
	 * u: union, i: intersect, -: diff, x: xor
	 */
	struct rt_db_internal intern2, intern_res;
	struct rt_brep_internal *bip;

	if (argc != 5) {
	    bu_vls_printf(gedp->ged_result_str, "Error: There should be exactly 5 params.\n");
	    return GED_ERROR;
	}

	/* get the other solid */
	if ((ndp = db_lookup(gedp->ged_wdbp->dbip,  argv[3], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a solid or does not exist in database", argv[3]);
	    return GED_ERROR;
	} else {
	    real_flag = (ndp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	}

	if (!real_flag) {
	    /* solid doesn't exist - don't kill */
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a real solid", argv[3]);
	    return GED_OK;
	}

	GED_DB_GET_INTERNAL(gedp, &intern2, ndp, bn_mat_identity, &rt_uniresource, GED_ERROR);

	rt_brep_boolean(&intern_res, &intern, &intern2, op);
	bip = (struct rt_brep_internal*)intern_res.idb_ptr;
	mk_brep(gedp->ged_wdbp, argv[4], (void *)(bip->brep));
	rt_db_free_internal(&intern);
	rt_db_free_internal(&intern2);
	rt_db_free_internal(&intern_res);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "selection")) {
	ret = selection_command(gedp, &intern, argc, argv);
	if (BU_STR_EQUAL(argv[3], "translate") && ret == 0) {
	    GED_DB_PUT_INTERNAL(gedp, ndp, &intern, &rt_uniresource, GED_ERROR);
	}
	rt_db_free_internal(&intern);

	return ret;
    }

    if (!RT_BREP_TEST_MAGIC(bi)) {
	/* The solid is not in brep form. Covert it to brep. */

	struct bu_vls bname, suffix;
	int no_evaluation = 0;

	bu_vls_init(&bname);
	bu_vls_init(&suffix);

	if (argc == 2) {
	    /* brep obj */
	    bu_vls_sprintf(&bname, "%s.brep", solid_name);
	    bu_vls_sprintf(&suffix, ".brep");
	} else if (BU_STR_EQUAL(argv[2], "--no-evaluation")) {
	    no_evaluation = 1;
	    if (argc == 3) {
		/* brep obj --no-evaluation */
		bu_vls_sprintf(&bname, "%s.brep", solid_name);
		bu_vls_sprintf(&suffix, ".brep");
	    } else if (argc == 4) {
		/* brep obj --no-evaluation suffix */
		bu_vls_sprintf(&bname, "%s", argv[3]);
		bu_vls_sprintf(&suffix, "%s", argv[3]);
	    }
	} else {
	    /* brep obj brepname/suffix */
	    bu_vls_sprintf(&bname, "%s", argv[2]);
	    bu_vls_sprintf(&suffix, "%s", argv[2]);
	}

	if (no_evaluation && intern.idb_type == ID_COMBINATION) {
	    struct bu_vls bname_suffix;
	    bu_vls_init(&bname_suffix);
	    bu_vls_sprintf(&bname_suffix, "%s%s", solid_name, bu_vls_addr(&suffix));
	    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&bname_suffix), LOOKUP_QUIET) != RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_addr(&bname_suffix));
		bu_vls_free(&bname);
		bu_vls_free(&suffix);
		bu_vls_free(&bname_suffix);
		return GED_OK;
	    }
	    brep_conversion_comb(&intern, bu_vls_addr(&bname_suffix), bu_vls_addr(&suffix), gedp->ged_wdbp, mk_conv2mm);
	    bu_vls_free(&bname_suffix);
	} else {
	    struct rt_db_internal brep_db_internal;
	    ON_Brep* brep;
	    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&bname), LOOKUP_QUIET) != RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_addr(&bname));
		bu_vls_free(&bname);
		bu_vls_free(&suffix);
		return GED_OK;
	    }
	    ret = brep_conversion(&intern, &brep_db_internal, gedp->ged_wdbp->dbip);
	    if (ret == -1) {
		bu_vls_printf(gedp->ged_result_str, "%s doesn't have a "
			"brep-conversion function yet. Type: %s", solid_name,
			intern.idb_meth->ft_label);
	    } else if (ret == -2) {
		bu_vls_printf(gedp->ged_result_str, "%s cannot be converted "
			"to brep correctly.", solid_name);
	    } else {
		brep = ((struct rt_brep_internal *)brep_db_internal.idb_ptr)->brep;
		ret = mk_brep(gedp->ged_wdbp, bu_vls_addr(&bname), brep);
		if (ret == 0) {
		    bu_vls_printf(gedp->ged_result_str, "%s is made.", bu_vls_addr(&bname));
		}
		rt_db_free_internal(&brep_db_internal);
	    }
	}
	bu_vls_free(&bname);
	bu_vls_free(&suffix);
	rt_db_free_internal(&intern);
	return GED_OK;
    }

    BU_ALLOC(stp, struct soltab);

    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n", argv[0], usage);
	bu_vls_printf(gedp->ged_result_str, "\t%s is in brep form, please input a command.", solid_name);
	return GED_HELP;
    }

    valid_command = 0;
    for (i = 0; i < num_commands; ++i) {
	if (BU_STR_EQUAL(argv[2], commands[i])) {
	    valid_command = 1;
	    break;
	}
    }

    if (!valid_command) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n", argv[0], usage);
	bu_vls_printf(gedp->ged_result_str, "\t%s is in brep form, please input a command.", solid_name);
	return GED_HELP;
    }

    if ((bs = (struct brep_specific*)stp->st_specific) == NULL) {
	BU_ALLOC(bs, struct brep_specific);
	bs->brep = bi->brep;
	bi->brep = NULL;
	stp->st_specific = (void *)bs;
    }

    vbp = rt_vlblock_init();

    if ((int)color.buc_rgb[0] == 0 && (int)color.buc_rgb[1] == 0 && (int)color.buc_rgb[2] == 0) {
	brep_command(gedp->ged_result_str, solid_name, gedp->ged_wdbp, NULL, (const struct bg_tess_tol *)&gedp->ged_wdbp->wdb_ttol, &gedp->ged_wdbp->wdb_tol, bs, bi, vbp, argc, argv, commtag);
    } else {
	brep_command(gedp->ged_result_str, solid_name, gedp->ged_wdbp, &color, (const struct bg_tess_tol *)&gedp->ged_wdbp->wdb_ttol, &gedp->ged_wdbp->wdb_tol, bs, bi, vbp, argc, argv, commtag);
    }

    if (BU_STR_EQUAL(argv[2], "translate")) {
	bi->brep = bs->brep;
	GED_DB_PUT_INTERNAL(gedp, ndp, &intern, &rt_uniresource, GED_ERROR);
    }

    snprintf(namebuf, sizeof(namebuf), "%s%s_", commtag, solid_name);
    _ged_cvt_vlblock_to_solids(gedp, vbp, namebuf, 0);
    bn_vlblock_free(vbp);
    vbp = (struct bn_vlblock *)NULL;

    rt_db_free_internal(&intern);

    return GED_OK;
}


extern "C" int
_brep_cmd_boolean(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    if (argc != 4) {
	bu_vls_printf(gb->gedp->ged_result_str, "brep <objname1> bool <op> <objname2> <output_objname>\n");
	return GED_ERROR;
    }


    // We've already looked up the first sold, get the second
    struct directory *dp2 = db_lookup(gedp->ged_wdbp->dbip, argv[2], LOOKUP_NOISY);
    if (dp2 == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, ": %s is not a solid or does not exist in database", argv[3]);
	return GED_ERROR;
    } else {
	int real_flag = (gb->dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	if (!real_flag) {
	    /* solid doesn't exist */
	    bu_vls_printf(gedp->ged_result_str, ": %s is not a real solid", argv[2]);
	    return GED_ERROR;
	}
    }
    struct rt_db_internal intern2;
    GED_DB_GET_INTERNAL(gedp, &intern2, dp2, bn_mat_identity, &rt_uniresource, GED_ERROR);
    RT_CK_DB_INTERNAL(&intern2);

    db_op_t op = DB_OP_NULL;
    op = db_str2op(argv[1]);
    if (op == DB_OP_NULL) {
	bu_vls_printf(gedp->ged_result_str, ": invalid boolean operation specified: %s", argv[1]);
	return GED_ERROR;
    }

    struct rt_db_internal intern_res;
    rt_brep_boolean(&intern_res, &gb->intern, &intern2, op);
    struct rt_brep_internal *bip = (struct rt_brep_internal *)intern_res.idb_ptr;
    mk_brep(gedp->ged_wdbp, argv[3], (void *)(bip->brep));
    rt_db_free_internal(&intern2);
    rt_db_free_internal(&intern_res);

    return GED_OK;
}

extern "C" int
_brep_cmd_bot(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    struct ged *gedp = gb->gedp;
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

    struct bg_tess_tol cdttol;
    cdttol.abs = ttol->abs;
    cdttol.rel = ttol->rel;
    cdttol.norm = ttol->norm;
    ON_Brep_CDT_State *s_cdt = ON_Brep_CDT_Create((void *)bi->brep, gb->solid_name.c_str());
    ON_Brep_CDT_Tol_Set(s_cdt, &cdttol);
    if (ON_Brep_CDT_Tessellate(s_cdt, 0, NULL) == -1) {
	bu_vls_printf(gedp->ged_result_str, "tessellation failed\n");
	ON_Brep_CDT_Destroy(s_cdt);
	bu_vls_free(&bname_bot);
	return GED_ERROR;
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
	return GED_ERROR;
    }

    bu_vls_free(&bname_bot);
    return GED_OK;
}

extern "C" int
_brep_cmd_bots(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    argc--; argv++;

    if (!argc || !argv) {
	bu_vls_printf(gedp->ged_result_str, "brep\n");
    }

    const char **obj_names = (const char **)bu_calloc(argc, sizeof(char *), "new argv");
    obj_names[0] = gb->dp->d_namep;
    for (int iav = 0; iav < argc; iav++) {
	obj_names[iav+1] = argv[iav];
    }
    int obj_cnt = argc+1;

    double ovlp_max_smallest = DBL_MAX;

    const struct bg_tess_tol *ttol = (const struct bg_tess_tol *)&gedp->ged_wdbp->wdb_ttol;
    struct bg_tess_tol cdttol;
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
	if ((dp = db_lookup(gedp->ged_wdbp->dbip, obj_names[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a solid or does not exist in database", obj_names[i]);
	    return GED_ERROR;
	}
	GED_DB_GET_INTERNAL(gedp, &intern, dp, bn_mat_identity, &rt_uniresource, GED_ERROR);
	RT_CK_DB_INTERNAL(&intern);
	bi = (struct rt_brep_internal*)intern.idb_ptr;
	if (!RT_BREP_TEST_MAGIC(bi)) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a brep solid", obj_names[i]);
	    return GED_ERROR;
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
	return GED_ERROR;
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
	    return GED_ERROR;
	}
	bu_vls_free(&bot_name);
    }

    return GED_OK;
}

extern "C" int
_brep_cmd_brep(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;
    if (gb->intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is already a brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    struct bu_vls bname, suffix;
    int no_evaluation = 0;

    bu_vls_init(&bname);
    bu_vls_init(&suffix);

    if (argc == 1) {
	/* brep obj */
	bu_vls_sprintf(&bname, "%s.brep", gb->solid_name.c_str());
	bu_vls_sprintf(&suffix, ".brep");
    } else if (BU_STR_EQUAL(argv[1], "--no-evaluation")) {
	no_evaluation = 1;
	if (argc == 2) {
	    /* brep obj --no-evaluation */
	    bu_vls_sprintf(&bname, "%s.brep", gb->solid_name.c_str());
	    bu_vls_sprintf(&suffix, ".brep");
	} else if (argc == 4) {
	    /* brep obj --no-evaluation suffix */
	    bu_vls_sprintf(&bname, "%s", argv[2]);
	    bu_vls_sprintf(&suffix, "%s", argv[2]);
	}
    } else {
	/* brep obj brepname/suffix */
	bu_vls_sprintf(&bname, "%s", argv[1]);
	bu_vls_sprintf(&suffix, "%s", argv[1]);
    }

    if (no_evaluation && gb->intern.idb_type == ID_COMBINATION) {
	struct bu_vls bname_suffix;
	bu_vls_init(&bname_suffix);
	bu_vls_sprintf(&bname_suffix, "%s%s", gb->solid_name.c_str(), bu_vls_addr(&suffix));
	if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&bname_suffix), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_addr(&bname_suffix));
	    bu_vls_free(&bname);
	    bu_vls_free(&suffix);
	    bu_vls_free(&bname_suffix);
	    return GED_OK;
	}
	brep_conversion_comb(&gb->intern, bu_vls_addr(&bname_suffix), bu_vls_addr(&suffix), gedp->ged_wdbp, mk_conv2mm);
	bu_vls_free(&bname_suffix);
    } else {
	struct rt_db_internal brep_db_internal;
	ON_Brep* brep;
	if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&bname), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_addr(&bname));
	    bu_vls_free(&bname);
	    bu_vls_free(&suffix);
	    return GED_OK;
	}
	int ret = brep_conversion(&gb->intern, &brep_db_internal, gedp->ged_wdbp->dbip);
	if (ret == -1) {
	    bu_vls_printf(gedp->ged_result_str, "%s doesn't have a "
		    "brep-conversion function yet. Type: %s", gb->solid_name.c_str(),
		    gb->intern.idb_meth->ft_label);
	} else if (ret == -2) {
	    bu_vls_printf(gedp->ged_result_str, "%s cannot be converted "
		    "to brep correctly.", gb->solid_name.c_str());
	} else {
	    brep = ((struct rt_brep_internal *)brep_db_internal.idb_ptr)->brep;
	    ret = mk_brep(gedp->ged_wdbp, bu_vls_addr(&bname), brep);
	    if (ret == 0) {
		bu_vls_printf(gedp->ged_result_str, "%s is made.", bu_vls_addr(&bname));
	    }
	    rt_db_free_internal(&brep_db_internal);
	}
    }
    bu_vls_free(&bname);
    bu_vls_free(&suffix);
    return GED_OK;
}

extern "C" int
_brep_cmd_csg(void *bs, int UNUSED(argc), const char **UNUSED(argv))
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    struct bu_vls bname_csg;
    bu_vls_init(&bname_csg);
    bu_vls_sprintf(&bname_csg, "csg_%s", gb->solid_name.c_str());
    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&bname_csg), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_addr(&bname_csg));
	bu_vls_free(&bname_csg);
	return GED_OK;
    }
    bu_vls_free(&bname_csg);

    return _ged_brep_to_csg(gedp, gb->dp->d_namep, gb->verbosity);
}


extern "C" int
_brep_cmd_flip(void *bs, int UNUSED(argc), const char **UNUSED(argv))
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gb->intern.idb_ptr;

    b_ip->brep->Flip();

    // Delete the old object
    const char *av[3];
    char *ncpy = bu_strdup(gb->solid_name.c_str());
    av[0] = "kill";
    av[1] = ncpy;
    av[2] = NULL;
    (void)ged_kill(gb->gedp, 2, (const char **)av);
    bu_free(ncpy, "free name cpy");

    // Make the new one
    if (mk_brep(gb->gedp->ged_wdbp, gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return GED_ERROR;
    }
    return GED_OK;
}

extern "C" int
_brep_cmd_info(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }
    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gb->intern.idb_ptr;

    argc--; argv++;

    // If the user doesn't specify, give an overview
    if (!argc) {
	const char *is_valid = (rt_brep_valid(gb->gedp->ged_result_str, &gb->intern, 0)) ? "YES" : "NO";
	bu_vls_printf(gedp->ged_result_str, "%s -- VALID: %s,", gb->solid_name.c_str(), is_valid);
	const char *is_solid = (b_ip->brep->IsSolid()) ? "YES" : "NO";
	bu_vls_printf(gedp->ged_result_str, " SOLID: %s,", is_solid);
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
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    struct ged *gedp = gb->gedp;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    if (argc != 4 && argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "\tintersect <obj2> <i> <j> [PP|PC|PS|CC|CS|SS]\n");
	return GED_ERROR;
    }

    int i, j;
    if (bu_opt_int(gedp->ged_result_str, 1, &argv[2], (void *)&i) < 0) {
	return GED_ERROR;
    }
    if (bu_opt_int(gedp->ged_result_str, 1, &argv[3], (void *)&j) < 0) {
	return GED_ERROR;
    }

    // We've already looked up the first sold, get the second
    struct directory *dp2 = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_NOISY);
    if (dp2 == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, ": %s is not a solid or does not exist in database", argv[3]);
	return GED_ERROR;
    } else {
	int real_flag = (gb->dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	if (!real_flag) {
	    /* solid doesn't exist */
	    bu_vls_printf(gedp->ged_result_str, ": %s is not a real solid", argv[1]);
	    return GED_ERROR;
	}
    }
    struct rt_db_internal intern2;
    GED_DB_GET_INTERNAL(gedp, &intern2, dp2, bn_mat_identity, &rt_uniresource, GED_ERROR);
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

    char namebuf[65];
    _ged_cvt_vlblock_to_solids(gedp, gb->vbp, namebuf, 0);

    rt_db_free_internal(&intern2);

    return GED_OK;
}


extern "C" int
_brep_cmd_plate_mode(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    if (argc != 1 && argc != 2) {
	bu_vls_printf(gb->gedp->ged_result_str, "brep <objname> plate_mode [val]\n");
    }

    if (argc == 1) {
	double thickness;
	int nocos;
	rt_brep_plate_mode_getvals(&thickness, &nocos, &gb->intern);
	thickness = gb->gedp->ged_wdbp->dbip->dbi_base2local * thickness;
	if (nocos) {
	    bu_vls_printf(gb->gedp->ged_result_str, "%f (NOCOS)", thickness);
	} else {
	    bu_vls_printf(gb->gedp->ged_result_str, "%f (COS)", thickness);
	}
	return GED_OK;
    }

    const char *val = argv[1];
    struct bu_attribute_value_set avs;
    double local2base = gb->gedp->ged_wdbp->dbip->dbi_local2base;

    // Make sure we can get attributes
    if (db5_get_attributes(gb->gedp->ged_wdbp->dbip, &avs, gb->dp)) {
	bu_vls_printf(gb->gedp->ged_result_str, "Error setting plate mode value\n");
	return GED_ERROR;
    };

    if (BU_STR_EQUIV(val, "cos")) {
	(void)bu_avs_add(&avs, "_plate_mode_nocos", "0");
	if (db5_replace_attributes(gb->dp, &avs, gb->gedp->ged_wdbp->dbip)) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Error setting plate mode value\n");
	    return GED_ERROR;
	} else {
	    bu_vls_printf(gb->gedp->ged_result_str, "%s", val);
	}
	return GED_OK;
    }

    if (BU_STR_EQUIV(val, "nocos")) {
	(void)bu_avs_add(&avs, "_plate_mode_nocos", "1");
	if (db5_replace_attributes(gb->dp, &avs, gb->gedp->ged_wdbp->dbip)) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Error setting plate mode value\n");
	    return GED_ERROR;
	} else {
	    bu_vls_printf(gb->gedp->ged_result_str, "%s", val);
	}
	return GED_OK;
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
    if (db5_replace_attributes(gb->dp, &avs, gb->gedp->ged_wdbp->dbip)) {
	bu_vls_printf(gb->gedp->ged_result_str, "Error setting plate mode value\n");
	return GED_ERROR;
    } else {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", val);
    }
    return GED_OK;
}


extern "C" int
_brep_cmd_plot(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    argc--; argv++;

    return brep_plot(gb, argc, argv);
}

extern "C" int
_brep_cmd_selection(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }


    if (!argc || !argv) {
	bu_vls_printf(gb->gedp->ged_result_str, "basic help\n");
    }

    return GED_OK;
}

extern "C" int
_brep_cmd_solid(void *bs, int UNUSED(argc), const char **UNUSED(argv))
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gb->intern.idb_ptr;
    int solid = (b_ip->brep->IsSolid()) ? 1 : 0;
    return (solid) ? GED_OK : GED_ERROR;
}

extern "C" int
_brep_cmd_shrink_surfaces(void *bs, int UNUSED(argc), const char **UNUSED(argv))
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gb->intern.idb_ptr;

    b_ip->brep->ShrinkSurfaces();

    // Delete the old object
    const char *av[3];
    char *ncpy = bu_strdup(gb->solid_name.c_str());
    av[0] = "kill";
    av[1] = ncpy;
    av[2] = NULL;
    (void)ged_kill(gb->gedp, 2, (const char **)av);
    bu_free(ncpy, "free name cpy");

    // Make the new one
    if (mk_brep(gb->gedp->ged_wdbp, gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return GED_ERROR;
    }
    return GED_OK;
}

extern "C" int
_brep_cmd_valid(void *bs, int UNUSED(argc), const char **UNUSED(argv))
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type brep\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    int valid = rt_brep_valid(gb->gedp->ged_result_str, &gb->intern, 0);
    return (valid) ? GED_OK : GED_ERROR;
}

extern "C" int
_brep_cmd_help(void *bs, int argc, const char **argv)
{
    struct _ged_brep_info *gb = (struct _ged_brep_info *)bs;
    if (!argc || !argv) {
	bu_vls_printf(gb->gedp->ged_result_str, "basic help\n");
    }

    return GED_OK;
}


const struct bu_cmdtab _brep_cmds[] = {
    { "?",               _brep_cmd_help},
    { "bool",            _brep_cmd_boolean},
    { "bot",             _brep_cmd_bot},
    { "bots",            _brep_cmd_bots},
    { "brep",            _brep_cmd_brep},
    { "csg",             _brep_cmd_csg},
    { "flip",            _brep_cmd_flip},
    { "help",            _brep_cmd_help},
    { "info",            _brep_cmd_info},
    { "intersect",       _brep_cmd_intersect},
    { "plate_mode",      _brep_cmd_plate_mode},
    { "plot",            _brep_cmd_plot},
    { "selection",       _brep_cmd_selection},
    { "solid",           _brep_cmd_solid},
    { "shrink_surfaces", _brep_cmd_shrink_surfaces},
    { "valid",           _brep_cmd_valid},
    { (char *)NULL,      NULL}
};

int
ged_brep2(struct ged *gedp, int argc, const char *argv[])
{
    struct _ged_brep_info gb;
    gb.gedp = gedp;
    gb.color = BU_COLOR_INIT_ZERO;
    unsigned char rgb[3] = {255, 255, 0};
    bu_color_from_rgb_chars(&gb.color, rgb);

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return GED_ERROR;
    }

    // Clear results
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the brep command - start processing args
    argc--; argv++;

    // See if we have any high level options set
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "C", "color",   "r/g/b", &bu_opt_color, &gb.color,        "Set color");
    BU_OPT(d[1], "v", "verbose", "",      NULL,          &gb.verbosity, "Verbose output");
    BU_OPT_NULL(d[2]);

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_brep_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    // Must have a subcommand
    if (cmd_pos == -1) {
	bu_vls_printf(gedp->ged_result_str, ": no valid subcommand specified\n");
	_brep_cmd_help(&gb, 0, NULL);
	return GED_ERROR;
    }


    int opt_ret = bu_opt_parse(NULL, cmd_pos, argv, d);

    if (opt_ret != 1) {
	bu_vls_printf(gedp->ged_result_str, ": no object specified before subcommand\n");
	_brep_cmd_help(&gb, 0, NULL);
	return GED_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    gb.solid_name = std::string(argv[0]);
    gb.dp = db_lookup(gedp->ged_wdbp->dbip, gb.solid_name.c_str(), LOOKUP_NOISY);
    if (gb.dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, ": %s is not a solid or does not exist in database", gb.solid_name.c_str());
	return GED_ERROR;
    } else {
	int real_flag = (gb.dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	if (!real_flag) {
	    /* solid doesn't exist */
	    bu_vls_printf(gedp->ged_result_str, ": %s is not a real solid", gb.solid_name.c_str());
	    return GED_ERROR;
	}
    }

    GED_DB_GET_INTERNAL(gedp, &gb.intern, gb.dp, bn_mat_identity, &rt_uniresource, GED_ERROR);
    RT_CK_DB_INTERNAL(&gb.intern);

    gb.vbp = rt_vlblock_init();

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

    bn_vlblock_free(gb.vbp);
    gb.vbp = (struct bn_vlblock *)NULL;
    rt_db_free_internal(&gb.intern);
    return GED_ERROR;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
