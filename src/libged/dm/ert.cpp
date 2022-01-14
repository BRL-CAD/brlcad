/*                         E R T . C P P
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
/** @file libged/dm/ert.c
 *
 * Raytrace the current view and display the results in the current
 * embedded fb.
 *
 */

#include "common.h"

#include <vector>
#include <string>

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include "bresource.h"

#include "bu/app.h"
#include "bu/process.h"
#include "raytrace.h"
#include "dm.h"

#include "../ged_private.h"


extern "C" int
ged_ert_core(struct ged *gedp, int argc, const char *argv[])
{

    int ret = GED_OK;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct dm *dmp = (struct dm *)gedp->ged_dmp;
    if (!dmp) {
	bu_vls_printf(gedp->ged_result_str, "no current display manager set\n");
	return GED_ERROR;
    }

    struct fb *fbp = dm_get_fb(dmp);
    if (!fbp) {
    	bu_vls_printf(gedp->ged_result_str, "attached display manager has no embedded framebuffer\n");
	return GED_ERROR;
    }

    if (!ged_who_argc(gedp)) {
	bu_vls_printf(gedp->ged_result_str, "no objects displayed\n");
	return GED_ERROR;
    }

    // Have a framebuffer to target and objects to raytrace.  Next we need a
    // framebuffer server.
    struct fbserv_obj *fbs = gedp->ged_fbs;
    fbs->fbs_fbp = fbp;
    fbs->fbs_is_listening = gedp->fbs_is_listening;
    fbs->fbs_listen_on_port = gedp->fbs_listen_on_port;
    fbs->fbs_open_server_handler = gedp->fbs_open_server_handler;
    fbs->fbs_close_server_handler = gedp->fbs_close_server_handler;
    fbs->fbs_open_client_handler = gedp->fbs_open_client_handler;
    fbs->fbs_close_client_handler = gedp->fbs_close_client_handler;
    if (!fbs->fbs_is_listening || fbs_open(fbs, 0) != BRLCAD_OK) {
    	bu_vls_printf(gedp->ged_result_str, "could not open fb server\n");
	return GED_ERROR;
    }

    // Assemble the arguments
    struct bu_vls wstr = BU_VLS_INIT_ZERO;
    std::vector<std::string> args;

    char rt[MAXPATHLEN] = {0};
    bu_dir(rt, MAXPATHLEN, BU_DIR_BIN, "rt", BU_DIR_EXT, NULL);
    args.push_back(std::string(rt));
    args.push_back(std::string("-F"));
    args.push_back(std::to_string(fbs->fbs_listener.fbsl_port));
    args.push_back(std::string("-M"));

    int width = dm_get_width((struct dm *)gedp->ged_dmp);
    int height = dm_get_height((struct dm *)gedp->ged_dmp);

    args.push_back(std::string("-w"));
    args.push_back(std::to_string(width));
    args.push_back(std::string("-n"));
    args.push_back(std::to_string(height));
    args.push_back(std::string("-V"));
    double aspect = (double)width/(double)height;
    bu_vls_sprintf(&wstr, "%.14e", aspect);
    args.push_back(std::string(bu_vls_cstr(&wstr)));
    if (gedp->ged_gvp->gv_perspective > 0) {
	args.push_back(std::string("-p"));
	bu_vls_sprintf(&wstr, "%.14e", gedp->ged_gvp->gv_perspective);
	args.push_back(std::string(bu_vls_cstr(&wstr)));
    }

    // Everything before the "--" argument is incorporated into the
    // initial command args
    int units_supplied = 0;
    int i = 0;
    for (i = 1; i < argc; i++) {
	if (BU_STR_EQUAL(argv[1], "-u")) {
	    units_supplied=1;
	} else if (argv[i][0] == '-' && argv[i][1] == '-' && argv[i][2] == '\0') {
	    ++i;
	    break;
	}
	args.push_back(std::string(argv[i]));
    }

    /* default to local units when not specified on command line */
    if (!units_supplied) {
	args.push_back(std::string("-u"));
	args.push_back(std::string("model"));
    }

    args.push_back(std::string(gedp->dbip->dbi_filename));

    int gd_rt_cmd_len = args.size();
    char **gd_rt_cmd = (char **)bu_calloc(gd_rt_cmd_len + ged_who_argc(gedp), sizeof(char *), "alloc gd_rt_cmd");
    for (size_t j = 0; j < args.size(); j++) {
	gd_rt_cmd[j] = bu_strdup(args[j].c_str());
    }

    ret = _ged_run_rt(gedp, gd_rt_cmd_len, (const char **)gd_rt_cmd, (argc - i), &(argv[i]));

    bu_vls_cstr(&wstr);
    for (size_t j = 0; j < args.size(); j++) {
	bu_free(gd_rt_cmd[j], "free gd_rt_cmd arg");
    }
    bu_free(gd_rt_cmd, "free gd_rt_cmd");

    return ret;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
