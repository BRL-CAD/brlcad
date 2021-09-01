/*                          H E A L . C
 * BRL-CAD
 *
 * Copyright (c) 2016-2021 United States Government as represented by
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
/** @file heal.c
 *
 * Heal command for the mesh healing operations
 *
 */

#include "common.h"
#include "analyze.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bg/chull.h"
#include "rt/geom.h"
#include "wdb.h"
#include "../ged_private.h"


int
ged_heal_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *bot_dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    const char *cmd = argv[0];
    const char *primitive = NULL;

    double zipper_tol = 0;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if(argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s <bot_solid>", argv[0]);
	return GED_HELP;
    }

    primitive = argv[1];

    if(argc > 2)
	zipper_tol = atof(argv[2]);

    /* get bot */
    GED_DB_LOOKUP(gedp, bot_dp, primitive, LOOKUP_NOISY, GED_ERROR & GED_QUIET);
    GED_DB_GET_INTERNAL(gedp, &intern, bot_dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT solid!", cmd, primitive);
	return GED_ERROR;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    analyze_heal_bot(bot, zipper_tol);
    rt_db_put_internal(bot_dp, gedp->dbip, &intern, &rt_uniresource);

    bu_vls_printf(gedp->ged_result_str, "Healed Mesh!");

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl heal_cmd_impl = {
    "heal",
    ged_heal_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd heal_cmd = { &heal_cmd_impl };
const struct ged_cmd *heal_cmds[] = { &heal_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  heal_cmds, 1 };

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
