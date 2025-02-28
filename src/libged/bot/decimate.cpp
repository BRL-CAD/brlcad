/*                   D E C I M A T E  . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022-2025 United States Government as represented by
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
 /** @file libged/bot/decimate.cpp
  *
  * Decimate a BoT
  *
  */

#include "common.h"

#include <vector>
#include <unordered_map>

#include "vmath.h"
#include "bu/str.h"
#include "bg/trimesh.h"
#include "rt/db5.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/geom.h"
#include "rt/wdb.h"

#include "ged/commands.h"
#include "ged/database.h"
#include "ged/objects.h"
#include "./ged_bot.h"

static void
decimate_usage(struct bu_vls *str, const char *cmd, struct bu_opt_desc *d) {
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(str, "Usage: %s [options] input_bot [output_name]\n", cmd);
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
    bu_vls_printf(str, "Available decimation methods are:\n");
    bu_vls_printf(str, "   GCT (default) - specify with feature-size option\n");
    bu_vls_printf(str, "   Simple - specify with merge-tol option\n");
}

extern "C" int
_bot_cmd_decimate(void* bs, int argc, const char** argv)
{
    struct _ged_bot_info* gb = (struct _ged_bot_info*)bs;
    struct ged* gedp = gb->gedp;
    struct db_i *dbip = gedp->dbip;
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    const char* usage_string = "bot [options] decimate [decimate_options] <objname> [output_name]";
    const char* purpose_string = "Decimate the BoT";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    int print_help = 0;
    double feature_size = 1.0;
    double merge_tol = -FLT_MAX;

    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h",      "help",     "",            NULL, &print_help,   "Print help");
    BU_OPT(d[1], "f", "feature-size", "#", &bu_opt_fastf_t, &feature_size, "Feature size");
    BU_OPT(d[2], "t", "merge-tol",    "#", &bu_opt_fastf_t, &merge_tol, "Tolerance for merging vertices");
    BU_OPT_NULL(d[3]);

    // We know we're the decimate command - start processing args
    argc--; argv++;

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help || !argc) {
	decimate_usage(gedp->ged_result_str, "bot decimate", d);
	return GED_HELP;
    }

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    const char* input_bot_name = gb->dp->d_namep;
    struct bu_vls output_bot_name = BU_VLS_INIT_ZERO;
    if (argc > 1) {
	bu_vls_sprintf(&output_bot_name, "%s", argv[1]);
    } else {
	bu_vls_sprintf(&output_bot_name, "%s-out_decimate.bot", input_bot_name);
    }

    GED_CHECK_EXISTS(gedp, bu_vls_cstr(&output_bot_name), LOOKUP_QUIET, BRLCAD_ERROR);
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    struct rt_bot_internal *input_bot = (struct rt_bot_internal*)gb->intern->idb_ptr;
    RT_BOT_CK_MAGIC(input_bot);


    if (merge_tol > -FLT_MAX) {

	bu_log("INPUT BoT has %zu vertices and %zu faces, merge_tol = %f\n", input_bot->num_vertices, input_bot->num_faces, merge_tol);

	int *ofaces = NULL;
	int n_ofaces = 0;
	struct bg_trimesh_decimation_settings s= BG_TRIMESH_DECIMATION_SETTINGS_INIT;
	s.feature_size = merge_tol;
	int ret = bg_trimesh_decimate(&ofaces, &n_ofaces, input_bot->faces, (int)input_bot->num_faces, (point_t *)input_bot->vertices, (int)input_bot->num_vertices, &s);
	if (bu_vls_strlen(&s.msgs)) {
	    bu_log("%s", bu_vls_cstr(&s.msgs));
	}
	bu_vls_free(&s.msgs);
	if (ret != BRLCAD_OK) {
	    bu_free(ofaces, "ofaces");
	    return BRLCAD_ERROR;
	}

	int *gcfaces = NULL;
	point_t *opnts = NULL;
	int n_opnts;

	// Trim out any unused points
	int n_gcfaces = bg_trimesh_3d_gc(&gcfaces, &opnts, &n_opnts, ofaces, n_ofaces, (point_t *)input_bot->vertices);
	if (n_gcfaces != n_ofaces) {
	    bu_free(gcfaces, "gcfaces");
	    bu_free(opnts , "opnts");
	    bu_free(ofaces, "ofaces");
	    bu_vls_free(&output_bot_name);
	    return BRLCAD_ERROR;
	}

	bu_log("OUTPUT BoT has %d vertices and %d faces, merge_tol = %f\n", n_opnts, n_gcfaces, s.feature_size);

	// Indices may be updated after gc, so the old array is obsolete
	bu_free(ofaces, "ofaces");

	// New bot time
	struct rt_bot_internal *nbot;
	BU_ALLOC(nbot, struct rt_bot_internal);
	nbot->magic = RT_BOT_INTERNAL_MAGIC;
	nbot->mode = input_bot->mode;
	// We're not mapping plate mode thickness info at the moment, so we
	// can't persist a plate mode type
	if (nbot->mode == RT_BOT_PLATE || nbot->mode == RT_BOT_PLATE_NOCOS)
	    nbot->mode = RT_BOT_SURFACE;
	nbot->orientation = input_bot->orientation;
	nbot->thickness = NULL; // TODO
	nbot->face_mode = NULL; // TODO
	nbot->num_faces = n_ofaces;
	nbot->num_vertices = n_opnts;
	nbot->faces = gcfaces;
	nbot->vertices = (fastf_t *)opnts;

	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_meth = &OBJ[ID_BOT];
	intern.idb_ptr = (void *)nbot;

	struct directory *dp = db_diradd(dbip, bu_vls_cstr(&output_bot_name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&gb->intern->idb_type);
	if (dp == RT_DIR_NULL) {
	    bu_free(gcfaces, "gcfaces");
	    bu_free(opnts , "opnts");
	    bu_vls_free(&output_bot_name);
	    return BRLCAD_ERROR;
	}
	bu_vls_free(&output_bot_name);

	if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	    bu_free(gcfaces, "gcfaces");
	    bu_free(opnts , "opnts");
	    bu_log("Failed to write %s to database\n", bu_vls_cstr(&output_bot_name));
	    rt_db_free_internal(&intern);
	    bu_vls_free(&output_bot_name);
	    return BRLCAD_ERROR;
	}

	return BRLCAD_OK;
    }

    bu_log("INPUT BoT has %zu vertices and %zu faces, feature_size = %f\n", input_bot->num_vertices, input_bot->num_faces, feature_size);

    struct directory *dp = db_diradd(dbip, bu_vls_cstr(&output_bot_name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&gb->intern->idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_free(&output_bot_name);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&output_bot_name);

    if (rt_db_put_internal(dp, dbip, gb->intern, &rt_uniresource) < 0) {
	return BRLCAD_ERROR;
    }
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, NULL, wdbp->wdb_resp, BRLCAD_ERROR);
    struct rt_bot_internal *obot = (struct rt_bot_internal*)intern.idb_ptr;

    // Decimate with GCT
    size_t edges_removed;
    feature_size *= gedp->dbip->dbi_local2base;
    edges_removed = rt_bot_decimate_gct(obot, feature_size);
    bu_log("[GCT] OUTPUT BoT has %zu vertices and %zu faces (%zu edges removed)\n", obot->num_vertices, obot->num_faces, edges_removed);

    // Write decimation to disk
    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

