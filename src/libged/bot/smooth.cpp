/*                  S M O O T H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2026 United States Government as represented by
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
 /** @file libged/bot/smooth.cpp
  *
  * Smooth a BoT using Jacobi-Laplace smoother.
  *
  */

#include "common.h"

#include <vector>

#include "vmath.h"
#include "bu/str.h"

#include "rt/db5.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/geom.h"
#include "rt/wdb.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#elif defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#include "../../libbg/GTE/Mathematics/BotSmooth.h"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#elif defined(__clang__)
#  pragma clang diagnostic pop
#endif

#include "ged/commands.h"
#include "ged/database.h"
#include "ged/objects.h"
#include "./ged_bot.h"


static struct rt_bot_internal *
bot_smooth(struct ged *gedp, struct rt_bot_internal *input_bot,
	   int component, int continuity,
	   double max_lerr, double max_aerr, int iterations)
{
    if (!gedp || !input_bot)
	return NULL;

    std::vector<float> inV(input_bot->num_vertices * 3);
    for (size_t i = 0; i < input_bot->num_vertices * 3; i++)
	inV[i] = (float)input_bot->vertices[i];

    std::vector<int> inF(input_bot->num_faces * 3);
    for (size_t i = 0; i < input_bot->num_faces * 3; i++)
	inF[i] = input_bot->faces[i];

    std::vector<float> outV;
    std::vector<int>   outF;

    if (!gte::gte_bot_smooth(
	    input_bot->num_vertices, inV.data(),
	    input_bot->num_faces,    inF.data(),
	    component, continuity,
	    max_lerr, max_aerr, iterations,
	    outV, outF)) {
	bu_vls_printf(gedp->ged_result_str, "WARNING: BoT smoothing failed.\n");
	return NULL;
    }

    struct rt_bot_internal *obot = NULL;
    BU_ALLOC(obot, struct rt_bot_internal);
    obot->magic      = RT_BOT_INTERNAL_MAGIC;
    obot->mode       = RT_BOT_SOLID;
    obot->orientation = RT_BOT_UNORIENTED;
    obot->thickness  = (fastf_t *)NULL;
    obot->face_mode  = (struct bu_bitv *)NULL;

    obot->num_vertices = outV.size() / 3;
    obot->vertices = (fastf_t*)bu_malloc(obot->num_vertices * 3 * sizeof(fastf_t), "vertices");
    for (size_t i = 0; i < obot->num_vertices * 3; i++)
	obot->vertices[i] = (fastf_t)outV[i];

    obot->num_faces = outF.size() / 3;
    obot->faces = (int*)bu_malloc((obot->num_faces + 1) * 3 * sizeof(int), "triangles");
    for (size_t i = 0; i < obot->num_faces * 3; i++)
	obot->faces[i] = outF[i];

    return obot;
}

static void
smooth_usage(struct bu_vls *str, const char *cmd, struct bu_opt_desc *d) {
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(str, "Usage: %s [options] input_bot [output_name]\n", cmd);
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }

    bu_vls_printf(str, "Available continuity options are:\n");
    bu_vls_printf(str, "    0 (C0: shape is continuous, but not the tangent)\n");
    bu_vls_printf(str, "    1 (C1: shape and tangent are continuous - default)\n\n");

    bu_vls_printf(str, "Available component direction options are:\n");
    bu_vls_printf(str, "    0 (smooth in tangential direction - default)\n");
    bu_vls_printf(str, "    1 (smooth in normal direction)\n");
    bu_vls_printf(str, "    2 (smooth in both tangential and normal direction)\n");
}

extern "C" int
_bot_cmd_smooth(void* bs, int argc, const char** argv)
{
    struct _ged_bot_info* gb = (struct _ged_bot_info*)bs;
    struct ged* gedp = gb->gedp;
    struct db_i *dbip = gedp->dbip;

    const char* usage_string = "bot [options] smooth [smooth_options] <objname> [output_name]";
    const char* purpose_string = "Smooth the BoT using Jacobi Laplace smoothing";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    int print_help = 0;
    int continuity = 1;
    int direction = 0;
    double max_lerror = 0;
    double max_aerror = 0;
    int iterations = 3;

    struct bu_opt_desc d[7];
    BU_OPT(d[0], "h", "help",              "",         NULL,      &print_help, "Print help");
    BU_OPT(d[1], "c", "continuity",       "#",  &bu_opt_int,      &continuity, "C0 (0) or C1 (1) continuity (default C1)");
    BU_OPT(d[2], "d", "direction",        "#",  &bu_opt_int,      &direction,  "Tangential (0), Normal (1) or both (2)");
    BU_OPT(d[3], "e", "max-local-error",  "#",  &bu_opt_fastf_t,  &max_lerror, "Maximum local error");
    BU_OPT(d[4], "E", "max-abs-error",    "#",  &bu_opt_fastf_t,  &max_aerror, "Maximum absolute error");
    BU_OPT(d[5], "I", "iterations",       "#",  &bu_opt_int,      &iterations, "Number of times to apply smoothing");
    BU_OPT_NULL(d[6]);

    // We know we're the smooth command - start processing args
    argc--; argv++;

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help || !argc) {
	smooth_usage(gedp->ged_result_str, "bot smooth", d);
	return GED_HELP;
    }

    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    const char* input_bot_name = gb->dp->d_namep;
    struct bu_vls output_bot_name = BU_VLS_INIT_ZERO;
    if (argc > 1) {
	bu_vls_sprintf(&output_bot_name, "%s", argv[1]);
    } else {
	bu_vls_sprintf(&output_bot_name, "%s-out_smooth.bot", input_bot_name);
    }

    GED_CHECK_EXISTS(gedp, bu_vls_cstr(&output_bot_name), LOOKUP_QUIET, BRLCAD_ERROR);

    struct rt_bot_internal *input_bot = (struct rt_bot_internal*)gb->intern->idb_ptr;
    RT_BOT_CK_MAGIC(input_bot);

    bu_log("INPUT BoT has %zu vertices and %zu faces\n", input_bot->num_vertices, input_bot->num_faces);

    struct rt_bot_internal *output_bot = bot_smooth(gedp, input_bot, direction, continuity, max_lerror, max_aerror, iterations);
    if (!output_bot) {
	bu_vls_free(&output_bot_name);
	return BRLCAD_ERROR;
    }

    bu_log("OUTPUT BoT has %zu vertices and %zu faces\n", output_bot->num_vertices, output_bot->num_faces);

    /* Export BOT as a new solid */
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)output_bot;

    struct directory *dp = db_diradd(dbip, bu_vls_cstr(&output_bot_name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_free(&output_bot_name);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&output_bot_name);

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

