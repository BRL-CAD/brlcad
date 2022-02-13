/*                       C L O S E . C
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
/** @file libged/close.c
 *
 * The close command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"

#include "../ged_private.h"


int
ged_close_core(struct ged *gedp, int UNUSED(argc), const char **UNUSED(argv))
{
    char *av[2];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    /* set result while we still have the info */
    bu_vls_sprintf(gedp->ged_result_str, "closed %s", gedp->dbip->dbi_filename);

    rt_new_material_head(MATER_NULL);

    av[0] = "zap";
    av[1] = (char *)0;
    ged_zap(gedp, 1, (const char **)av);

    /* close current database */
    if (gedp->ged_wdbp) {
	wdb_close(gedp->ged_wdbp);
    } else {
	db_close(gedp->dbip);
    }
    gedp->ged_wdbp = RT_WDB_NULL;
    gedp->dbip = NULL;

    /* Terminate any ged subprocesses */
    if (gedp != GED_NULL) {
	for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_subp); i++) {
	    struct ged_subprocess *rrp = (struct ged_subprocess *)BU_PTBL_GET(&gedp->ged_subp, i);
	    if (!rrp->aborted) {
		bu_terminate(bu_process_pid(rrp->p));
		rrp->aborted = 1;
	    }
	    bu_ptbl_rm(&gedp->ged_subp, (long *)rrp);
	    BU_PUT(rrp, struct ged_subprocess);
	}
	bu_ptbl_reset(&gedp->ged_subp);
    }

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl close_cmd_impl = {"close", ged_close_core, GED_CMD_DEFAULT};
const struct ged_cmd close_cmd = { &close_cmd_impl };

const struct ged_cmd *close_cmds[] = { &close_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  close_cmds, 1 };

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
