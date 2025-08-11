/*                        E D I T I T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/editit.c
 *
 * The editit function.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#include "bio.h"
#include "bresource.h"

#include "bu/app.h"
#include "bu/file.h"
#include "bu/path.h"
#include "ged.h"
#include "../ged_private.h"


int
ged_editit_core(struct ged *gedp, int argc, const char *argv[])
{
    const char *usage = "editit [opts] <filename>";
    int ret = 0;
    int print_help = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    struct bu_vls editstring = BU_VLS_INIT_ZERO;
    struct bu_vls filename = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[5];
    BU_OPT(d[0], "h", "help",   "",              NULL,        &print_help, "Print help and exit");
    BU_OPT(d[1], "?", "",       "",              NULL,        &print_help, "");
    BU_OPT(d[0], "e",  "",      "<editstring>",  &bu_opt_vls, &editstring, "Specify edit string (deprecated)");
    BU_OPT(d[1], "f",  "",      "<file>",        &bu_opt_vls, &filename,   "Specify file to edit");
    BU_OPT_NULL(d[2]);

    argc-=(argc>0); argv+=(argc>0); /* done with command name argv[0] */

    if (!argc) {
	/* must be wanting help */
	_ged_cmd_help(gedp, usage, d);
	return GED_HELP;
    }

    /* parse standard options */
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;

    /* Only one specifier for a filename */
    if (argc && bu_vls_strlen(&filename)) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_ERROR;
    }

    /* If we got a filename without -f, populate it now */
    if (argc)
	bu_vls_sprintf(&filename, "%s", argv[0]);


    ret = _ged_editit(gedp, bu_vls_cstr(&editstring), bu_vls_cstr(&filename));
    bu_vls_free(&editstring);
    bu_vls_free(&filename);
    return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl editit_cmd_impl = {
    "editit",
    ged_editit_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd editit_cmd = { &editit_cmd_impl };
const struct ged_cmd *editit_cmds[] = { &editit_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  editit_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
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
