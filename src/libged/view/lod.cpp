/*                         L O D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/view/lod.cpp
 *
 * The view lod subcommand.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/vls.h"
#include "bg/lod.h"

#include "../ged_private.h"
#include "./ged_view.h"

int
_view_cmd_lod(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] lod [subcommand] [vals]";
    const char *purpose_string = "manage Level of Detail drawing settings";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct ged *gedp = gd->gedp;
    struct bview *gvp;
    int print_help = 0;
    static const char *usage = "view lod [0|1]\n"
	"view lod cache [clear]\n"
	"view lod enabled [0|1]\n"
	"view lod scale [factor]\n"
	"view lod point_scale [factor]\n"
	"view lod curve_scale [factor]\n"
	"view lod bot_threshold [face_cnt]\n";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bu_opt_desc d[2];
    BU_OPT(d[0], "h", "help",        "",  NULL,  &print_help,      "Print help");
    BU_OPT_NULL(d[1]);

    // We know we're the lod command - start processing args
    argc--; argv++;

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage);
	return BRLCAD_HELP;
    }

    if (argc > 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage);
	return BRLCAD_ERROR;
    }

    gvp = gedp->ged_gvp;
    if (gvp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "no current view defined\n");
	return BRLCAD_ERROR;
    }

    /* Print current state if no args are supplied */
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "enabled: %d\n", gvp->gv_s->adaptive_plot);
	bu_vls_printf(gedp->ged_result_str, "scale: %g\n", gvp->gv_s->lod_scale);
	bu_vls_printf(gedp->ged_result_str, "point_scale: %g\n", gvp->gv_s->point_scale);
	bu_vls_printf(gedp->ged_result_str, "curve_scale: %g\n", gvp->gv_s->curve_scale);
	bu_vls_printf(gedp->ged_result_str, "bot_threshold: %zd\n", gvp->gv_s->bot_threshold);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUIV(argv[0], "1")) {
	if (!gvp->gv_s->adaptive_plot) {
	    gvp->gv_s->adaptive_plot = 1;
	    int rac = 1;
	    const char *rav[2] = {"redraw", NULL};
	    ged_exec(gedp, rac, (const char **)rav);
	}
	return BRLCAD_OK;
    }
    if (BU_STR_EQUIV(argv[0], "0")) {
	if (gvp->gv_s->adaptive_plot) {
	    gvp->gv_s->adaptive_plot = 0;
	    int rac = 1;
	    const char *rav[2] = {"redraw", NULL};
	    ged_exec(gedp, rac, (const char **)rav);
	}
	return BRLCAD_OK;
    }


    if (BU_STR_EQUAL(argv[0], "cache")) {
	if (argc == 1) {

	    int done = 0;
	    int total = 0;
	    for (int i = 0; i < RT_DBNHASH; i++) {
		struct directory *dp;
		for (dp = gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		    if (dp->d_addr == RT_DIR_PHONY_ADDR || dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
			continue;
		    }
		    total++;
		}
	    }

	    for (int i = 0; i < RT_DBNHASH; i++) {
		struct directory *dp;
		for (dp = gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		    if (dp->d_addr == RT_DIR_PHONY_ADDR || dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
			continue;
		    }

		    struct rt_db_internal dbintern;
		    RT_DB_INTERNAL_INIT(&dbintern);
		    struct rt_db_internal *ip = &dbintern;
		    int ret = rt_db_get_internal(ip, dp, gedp->dbip, NULL, &rt_uniresource);
		    if (ret < 0)
			continue;

		    if (ip->idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
			rt_db_free_internal(&dbintern);
			continue;
		    }
		    done++;
		    struct bu_vls pname = BU_VLS_INIT_ZERO;
		    bu_log("Caching %s (%d of %d)\n", dp->d_namep, done, total);
		    bu_vls_free(&pname);
		    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
		    RT_BOT_CK_MAGIC(bot);
		    bg_mesh_lod_cache((const point_t *)bot->vertices, bot->num_vertices, bot->faces, bot->num_faces);
		    rt_db_free_internal(&dbintern);
		}
	    }
	    bu_vls_printf(gedp->ged_result_str, "Caching complete");
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL(argv[1], "clear")) {
	    bg_mesh_lod_clear_cache(0);
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "unknown argument to cache: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }


    if (BU_STR_EQUAL(argv[0], "enabled")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%d\n", gvp->gv_s->adaptive_plot);
	    return BRLCAD_OK;
	}
	int rac = 1;
	const char *rav[2] = {"redraw", NULL};
	if (bu_str_true(argv[1])) {
	    gvp->gv_s->adaptive_plot = 1;
	    ged_exec(gedp, rac, (const char **)rav);
	    return BRLCAD_OK;
	}
	if (bu_str_false(argv[1])) {
	    gvp->gv_s->adaptive_plot = 0;
	    ged_exec(gedp, rac, (const char **)rav);
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "unknown argument to enabled: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "scale")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%g\n", gvp->gv_s->lod_scale);
	    return BRLCAD_OK;
	}
	fastf_t scale = 1.0;
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&scale) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to point_scale: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->lod_scale = scale;
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "point_scale")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%g\n", gvp->gv_s->point_scale);
	    return BRLCAD_OK;
	}
	fastf_t scale = 1.0;
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&scale) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to point_scale: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->point_scale = scale;
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "curve_scale")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%g\n", gvp->gv_s->curve_scale);
	    return BRLCAD_OK;
	}
	fastf_t scale = 1.0;
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&scale) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to curve_scale: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->curve_scale = scale;
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "bot_threshold")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%zd\n", gvp->gv_s->bot_threshold);
	    return BRLCAD_OK;
	}
	int bcnt = 0;
	if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&bcnt) != 1 || bcnt < 0) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to bot_threshold: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->bot_threshold = bcnt;
	return BRLCAD_OK;

    }

    bu_vls_printf(gedp->ged_result_str, "unknown subcommand: %s\n", argv[0]);
    return BRLCAD_ERROR;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
