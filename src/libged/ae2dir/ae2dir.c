/*                         A E 2 D I R . C
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
/** @file libged/ae2dir.c
 *
 * The ae2dir command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_ae2dir_core(struct ged *gedp, int argc, const char *argv[])
{
    double az, el;
    vect_t dir;
    int iflag;
    static const char *usage = "[-i] az el";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argv[1][0] == '-' && argv[1][1] == 'i' && argv[1][2] == '\0') {
	iflag = 1;
	--argc;
	++argv;
    } else
	iflag = 0;

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[1], "%lf", &az) != 1 ||
	sscanf(argv[2], "%lf", &el) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    az *= DEG2RAD;
    el *= DEG2RAD;
    V3DIR_FROM_AZEL(dir, az, el);

    if (iflag)
	VSCALE(dir, dir, -1);

    bn_encode_vect(gedp->ged_result_str, dir, 1);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl ae2dir_cmd_impl = {
    "ae2dir",
    ged_ae2dir_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd ae2dir_cmd = { &ae2dir_cmd_impl };
const struct ged_cmd *ae2dir_cmds[] = { &ae2dir_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  ae2dir_cmds, 1 };

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
