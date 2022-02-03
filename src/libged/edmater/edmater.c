/*                       E D M A T E R . C
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
/** @file libged/edmater.c
 *
 * The edmater command.
 *
 * Relies on: rmater, editit, wmater
 *
 */

#include "common.h"

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "bu/app.h"
#include "bu/file.h"
#include "bu/getopt.h"
#include "../ged_private.h"


int
ged_edmater_core(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    int i, c;
    int status;
    const char **av;
    static const char *usage = "comb(s)";
    char tmpfil[MAXPATHLEN];
    const char *editstring = NULL;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_optind = 1;
    /* First, grab the editstring off of the argv list */
    while ((c = bu_getopt(argc, (char * const *)argv, "E:")) != -1) {
	switch (c) {
	    case 'E' :
		editstring = bu_optarg;
		break;
	    default :
		break;
	}
    }

    argc -= bu_optind - 1;
    argv += bu_optind - 1;


    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    fp = bu_temp_file(tmpfil, MAXPATHLEN);
    if (!fp)
	return BRLCAD_ERROR;

    av = (const char **)bu_malloc(sizeof(char *)*(argc + 2), "f_edmater: av");
    av[0] = "wmater";
    av[1] = tmpfil;
    for (i = 2; i < argc + 1; ++i)
	av[i] = argv[i-1];

    av[i] = NULL;

    (void)fclose(fp);

    if (ged_wmater(gedp, argc, av) & BRLCAD_ERROR) {
	bu_file_delete(tmpfil);
	bu_free((void *)av, "f_edmater: av");
	return BRLCAD_ERROR;
    }

    if (_ged_editit(editstring, tmpfil)) {
	av[0] = "rmater";
	av[2] = NULL;
	status = ged_rmater(gedp, 2, av);
    } else {
	status = BRLCAD_ERROR;
    }

    bu_file_delete(tmpfil);
    bu_free((void *)av, "ged_edmater_core: av");

    return status;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl edmater_cmd_impl = {
    "edmater",
    ged_edmater_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd edmater_cmd = { &edmater_cmd_impl };
const struct ged_cmd *edmater_cmds[] = { &edmater_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  edmater_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
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
