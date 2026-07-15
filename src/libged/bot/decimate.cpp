/*                   D E C I M A T E  . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022-2026 United States Government as represented by
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
#include "bu/cmdschema.h"
#include "bu/malloc.h"
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

struct bot_decimate_subcommand_args {
    int print_help;
    double feature_size;
    fastf_t max_chord_error;
    fastf_t max_normal_error;
    fastf_t min_edge_length;
    double merge_tol;
};

static int
bot_decimate_subcommand_method_count(const struct bot_decimate_subcommand_args *args)
{
    int old_method = (args->max_chord_error > 0.0 ||
	args->max_normal_error > 0.0 || args->min_edge_length > 0.0) ? 1 : 0;
    int simple_method = (args->merge_tol > -FLT_MAX) ? 1 : 0;
    int gct_method = (NEAR_EQUAL(args->feature_size, 1.0, VUNITIZE_TOL)) ? 0 : 1;

    return old_method + simple_method + gct_method;
}

static int
bot_decimate_subcommand_schema_validate(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    struct bot_decimate_subcommand_args args = {0, 1.0, -1.0, -1.0, -1.0, -FLT_MAX};
    std::vector<const char *> parse_argv(argv, argv + argc);
    int help;
    int ret;

    help = bu_cmd_schema_option_present(schema, argc, argv, "help");
    flat.validation.custom_validate = NULL;
    if (help)
	flat.operands = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state != BU_CMD_VALIDATE_VALID || help)
	return ret;

    if (bu_cmd_schema_parse(schema, &args, NULL, (int)argc,
		parse_argv.data()) < 0)
	return BRLCAD_ERROR;
    if (bot_decimate_subcommand_method_count(&args) > 1) {
	bu_cmd_validate_result_clear(result);
	result->state = BU_CMD_VALIDATE_INVALID;
	result->token_start = cursor_arg < argc ? cursor_arg : argc;
	result->token_end = result->token_start;
	result->expected = BU_CMD_EXPECT_OPTION;
	result->completion_type = BU_CMD_VALUE_NUMBER;
	result->hint = "select one decimation method";
    }

    return 0;
}

static const struct bu_cmd_option bot_decimate_subcommand_options[] = {
    BU_CMD_FLAG("h", "help", struct bot_decimate_subcommand_args, print_help,
	"Print command help"),
    BU_CMD_NUMBER("c", "max-cord-error", struct bot_decimate_subcommand_args,
	max_chord_error, "error", "Maximum chord error length"),
    BU_CMD_NUMBER("n", "max-normal-error", struct bot_decimate_subcommand_args,
	max_normal_error, "error", "Maximum normal error"),
    BU_CMD_NUMBER("e", "min-edge-length", struct bot_decimate_subcommand_args,
	min_edge_length, "length", "Minimum edge length"),
    BU_CMD_NUMBER("f", "feature-size", struct bot_decimate_subcommand_args,
	feature_size, "size", "Feature size for the GCT decimator"),
    BU_CMD_NUMBER("t", "merge-tol", struct bot_decimate_subcommand_args,
	merge_tol, "tolerance", "Tolerance for merging vertices"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bot_decimate_subcommand_operands[] = {
    BU_CMD_OPERAND("input_bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Input BoT object", "ged.db_object"),
    BU_CMD_OPERAND("output_name", BU_CMD_VALUE_STRING, 0, 1,
	"Optional output BoT name", NULL),
    BU_CMD_OPERAND_NULL
};
extern "C" const struct bu_cmd_schema ged_bot_decimate_subcommand_schema = {
    "decimate", "Reduce BoT triangle count", bot_decimate_subcommand_options,
    bot_decimate_subcommand_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_decimate_subcommand_schema_validate, NULL)
};

static void
decimate_usage(struct bu_vls *str, const char *cmd)
{
    char *option_help = bu_cmd_schema_describe(&ged_bot_decimate_subcommand_schema);
    bu_vls_sprintf(str, "Usage: %s [options] input_bot [output_name]\n", cmd);
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
    bu_vls_printf(str, "Available decimation methods are:\n");
    bu_vls_printf(str, "   GCT (default) - method triggered by -f option\n");
    bu_vls_printf(str, "   Classical - triggered with -c, -n, or -e options\n");
    bu_vls_printf(str, "   Simple - triggered with -t option\n");
}

extern "C" int
_bot_cmd_decimate(void* bs, int argc, const char** argv)
{
    struct _ged_bot_info* gb = (struct _ged_bot_info*)bs;
    struct ged* gedp = gb->gedp;
    struct db_i *dbip = gedp->dbip;

    const char* usage_string = "bot [options] decimate [decimate_options] <objname> [output_name]";
    const char* purpose_string = "Decimate the BoT";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct bot_decimate_subcommand_args args = {0, 1.0, -1.0, -1.0, -1.0, -FLT_MAX};
    int operand_count;
    int operand_index;
    const char **operands;

    // We know we're the decimate command - start processing args
    argc--; argv++;

	if (!argc) {
	decimate_usage(gedp->ged_result_str, "bot decimate");
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&ged_bot_decimate_subcommand_schema,
	&args, gedp->ged_result_str, argc, argv);
    if (operand_index < 0)
	return BRLCAD_ERROR;
    operand_count = argc - operand_index;
    operands = argv + operand_index;
    if (args.print_help) {
	decimate_usage(gedp->ged_result_str, "bot decimate");
	return GED_HELP;
	}

    /* Sanity checks */
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    // Setup
    if (_bot_obj_setup(gb, operands[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    const char* input_bot_name = gb->dp->d_namep;
    struct bu_vls output_bot_name = BU_VLS_INIT_ZERO;
    if (operand_count > 1) {
	bu_vls_sprintf(&output_bot_name, "%s", operands[1]);
    } else {
	bu_vls_sprintf(&output_bot_name, "%s-out_decimate.bot", input_bot_name);
    }

    GED_CHECK_EXISTS(gedp, bu_vls_cstr(&output_bot_name), LOOKUP_QUIET, BRLCAD_ERROR);

    struct rt_bot_internal *input_bot = (struct rt_bot_internal*)gb->intern->idb_ptr;
    RT_BOT_CK_MAGIC(input_bot);


    /* Old method */
	if (args.max_chord_error > 0 || args.max_normal_error > 0 ||
	args.min_edge_length > 0) {
	/* convert maximum error, edge length, and feature size to mm */
	if (args.max_chord_error > 0.0)
	    args.max_chord_error = args.max_chord_error * gedp->dbip->dbi_local2base;

	if (args.min_edge_length > 0.0)
	    args.min_edge_length = args.min_edge_length * gedp->dbip->dbi_local2base;

	/* use the old decimation routine */
	if (rt_bot_decimate(input_bot, args.max_chord_error, args.max_normal_error,
		args.min_edge_length) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "rt_bot_decimate error\n");
	    return BRLCAD_ERROR;
	}

	struct directory *dp = db_diradd(dbip, bu_vls_cstr(&output_bot_name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&gb->intern->idb_type);
	if (dp == RT_DIR_NULL) {
	    bu_vls_free(&output_bot_name);
	    return BRLCAD_ERROR;
	}
	if (rt_db_put_internal(dp, dbip, gb->intern) < 0) {
	    bu_log("Failed to write %s to database\n", bu_vls_cstr(&output_bot_name));
	    bu_vls_free(&output_bot_name);
	    return BRLCAD_ERROR;
	}
	bu_vls_free(&output_bot_name);

	return BRLCAD_OK;
    }


    if (args.merge_tol > -FLT_MAX) {

	bu_log("INPUT BoT has %zu vertices and %zu faces, merge_tol = %f\n", input_bot->num_vertices, input_bot->num_faces, args.merge_tol);

	int *ofaces = NULL;
	int n_ofaces = 0;
	struct bg_trimesh_decimation_settings s= BG_TRIMESH_DECIMATION_SETTINGS_INIT;
	s.feature_size = args.merge_tol;
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
	if (rt_db_put_internal(dp, dbip, &intern) < 0) {
	    bu_free(gcfaces, "gcfaces");
	    bu_free(opnts , "opnts");
	    bu_log("Failed to write %s to database\n", bu_vls_cstr(&output_bot_name));
	    rt_db_free_internal(&intern);
	    bu_vls_free(&output_bot_name);
	    return BRLCAD_ERROR;
	}
	bu_vls_free(&output_bot_name);

	return BRLCAD_OK;
    }

    bu_log("INPUT BoT has %zu vertices and %zu faces, feature_size = %f\n", input_bot->num_vertices, input_bot->num_faces, args.feature_size);

    struct directory *dp = db_diradd(dbip, bu_vls_cstr(&output_bot_name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&gb->intern->idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_free(&output_bot_name);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&output_bot_name);

    if (rt_db_put_internal(dp, dbip, gb->intern) < 0) {
	return BRLCAD_ERROR;
    }
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    GED_DB_GET_INTERN(gedp, &intern, dp, NULL, BRLCAD_ERROR);
    struct rt_bot_internal *obot = (struct rt_bot_internal*)intern.idb_ptr;

    // Decimate with GCT
    size_t edges_removed;
    args.feature_size *= gedp->dbip->dbi_local2base;
    edges_removed = rt_bot_decimate_gct(obot, args.feature_size);
    bu_log("[GCT] OUTPUT BoT has %zu vertices and %zu faces (%zu edges removed)\n", obot->num_vertices, obot->num_faces, edges_removed);

    // Write decimation to disk
    if (rt_db_put_internal(dp, dbip, &intern) < 0) {
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
