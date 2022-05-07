/*                         T R A . C
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
 /** @file libged/art.c
  *
  * The art command for mged
  *
  */



#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/app.h"

#include "../ged_private.h"


int
ged_art_core(struct ged* gedp, int argc, const char* argv[])
{
	/*int i;
	int units_supplied = 0;
	char** vp;
	int args;
	char** gd_art_cmd = NULL;
	int gd_art_cmd_len = 0;
	int ret = BRLCAD_OK;

	const char* bin;
	char art[256] = { 0 };
	const char* cmd2 = getenv("GED_TEST_NEW_CMD_FORMS");

	GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
	GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
	GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
	GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

	bu_vls_trunc(gedp->ged_result_str, 0);

	if (!ged_who_argc(gedp)) {
		bu_vls_printf(gedp->ged_result_str, "no objects displayed\n");
		return BRLCAD_ERROR;
	}

	if (BU_STR_EQUAL(cmd2, "1")) {
		args = argc + 9 + 2 + ged_who_argc(gedp);
	}
	else {
		args = argc + 7 + 2 + ged_who_argc(gedp);
	}
	gd_art_cmd = (char**)bu_calloc(args, sizeof(char*), "alloc gd_art_cmd");

	bin = bu_dir(NULL, 0, BU_DIR_BIN, NULL);
	if (bin) {
		snprintf(art, 256, "%s/%s", bin, argv[0]);
	}

	vp = &gd_art_cmd[0];
	*vp++ = art;

	if (BU_STR_EQUAL(cmd2, "1")) {
		*vp++ = "-F";
		// TODO - look up dm type for this...
		*vp++ = "/dev/qtgl";
	}


	*vp++ = "-M";

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == 'u' &&
			BU_STR_EQUAL(argv[1], "-u")) {
			units_supplied = 1;
		}
		else if (argv[i][0] == '-' && argv[i][1] == '-' &&
			argv[i][2] == '\0') {
			++i;
			break;
		}
		*vp++ = (char*)argv[i];
	}


	*vp++ = gedp->dbip->dbi_filename;
	gd_art_cmd_len = vp - gd_art_cmd;

	ret = _ged_run_rt(gedp, gd_art_cmd_len, (const char**)gd_art_cmd, (argc - i), &(argv[i]));

	bu_free(gd_art_cmd, "free gd_rt_cmd");

	bu_vls_printf(gedp->ged_result_str, "Hello world");
	//int ret = _ged_run_rt(gedp, 1, (const char**)argv, 0, NULL);
	return ret;
	*/
	char** vp;
	int i;
	int units_supplied = 0;
	char pstring[32];
	int args;
	char** gd_rt_cmd = NULL;
	int gd_rt_cmd_len = 0;
	int ret = BRLCAD_OK;

	const char* bin;
	char rt[256] = { 0 };
	const char* cmd2 = getenv("GED_TEST_NEW_CMD_FORMS");

	GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
	GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
	GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
	GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

	/* initialize result */
	bu_vls_trunc(gedp->ged_result_str, 0);

	if (!ged_who_argc(gedp)) {
		bu_vls_printf(gedp->ged_result_str, "no objects displayed\n");
		return BRLCAD_ERROR;
	}

	if (BU_STR_EQUAL(cmd2, "1")) {
		args = argc + 9 + 2 + ged_who_argc(gedp);
	}
	else {
		args = argc + 7 + 2 + ged_who_argc(gedp);
	}
	gd_rt_cmd = (char**)bu_calloc(args, sizeof(char*), "alloc gd_rt_cmd");

	bin = bu_dir(NULL, 0, BU_DIR_BIN, NULL);
	if (bin) {
		snprintf(rt, 256, "%s/%s", bin, argv[0]);
	}

	vp = &gd_rt_cmd[0];
	*vp++ = rt;

	if (BU_STR_EQUAL(cmd2, "1")) {
		*vp++ = "-F";
		// TODO - look up dm type for this...
		*vp++ = "/dev/qtgl";
	}

	*vp++ = "-M";

	if (gedp->ged_gvp->gv_perspective > 0) {
		(void)sprintf(pstring, "-p%g", gedp->ged_gvp->gv_perspective);
		*vp++ = pstring;
	}

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == 'u' &&
			BU_STR_EQUAL(argv[1], "-u")) {
			units_supplied = 1;
		}
		else if (argv[i][0] == '-' && argv[i][1] == '-' &&
			argv[i][2] == '\0') {
			++i;
			break;
		}
		*vp++ = (char*)argv[i];
	}

	/* default to local units when not specified on command line */
	if (!units_supplied) {
		*vp++ = "-u";
		*vp++ = "model";
	}

	*vp++ = gedp->dbip->dbi_filename;
	gd_rt_cmd_len = vp - gd_rt_cmd;

	ret = _ged_run_rt(gedp, gd_rt_cmd_len, (const char**)gd_rt_cmd, (argc - i), &(argv[i]));

	bu_free(gd_rt_cmd, "free gd_rt_cmd");

	return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl art_cmd_impl = {
	"art",
	ged_art_core,
	GED_CMD_DEFAULT
};

const struct ged_cmd art_cmd = { &art_cmd_impl };
const struct ged_cmd* art_cmds[] = { &art_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  art_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin* ged_plugin_info()
{
	return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
