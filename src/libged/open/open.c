/*                         O P E N . C
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
/** @file libged/open.c
 *
 * The open command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"

#include "../ged_private.h"


int
ged_reopen_core(struct ged *gedp, int argc, const char *argv[])
{
    struct db_i *new_dbip;
    static const char *usage = "[filename]";

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get database filename */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", gedp->dbip->dbi_filename);
	return BRLCAD_OK;
    }

    /* set database filename */
    if (argc == 2) {
	char *av[2];
	struct db_i *old_dbip = gedp->dbip;
	struct mater *old_materp = rt_material_head();
	struct mater *new_materp;

	// TODO - need to look more carefully at material_head and what
	// its expected behavior is through a dbip change...
	rt_new_material_head(MATER_NULL);

	if ((new_dbip = _ged_open_dbip(argv[1], 0)) == DBI_NULL) {
	    /* Restore RT's material head */
	    rt_new_material_head(old_materp);

	    bu_vls_printf(gedp->ged_result_str, "ged_reopen_core: failed to open %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}

	av[0] = "zap";
	av[1] = (char *)0;
	ged_zap(gedp, 1, (const char **)av);

	new_materp = rt_material_head();

	gedp->dbip = old_dbip;
	if (gedp->ged_wdbp) {
	    gedp->ged_wdbp->dbip = gedp->dbip;
	}
	rt_new_material_head(old_materp);

	/* close current database */
	if (gedp->ged_wdbp) {
	    wdb_close(gedp->ged_wdbp);
	} else {
	    if (gedp->dbip)
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

	gedp->dbip = new_dbip;
	gedp->ged_wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DISK);

	rt_new_material_head(new_materp);

	/* New database open, need to initialize reference counts */
	db_update_nref(gedp->dbip, &rt_uniresource);

	bu_vls_printf(gedp->ged_result_str, "%s", gedp->dbip->dbi_filename);
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl reopen_cmd_impl = {"reopen", ged_reopen_core, GED_CMD_DEFAULT};
const struct ged_cmd reopen_cmd = { &reopen_cmd_impl };

struct ged_cmd_impl open_cmd_impl = {"open", ged_reopen_core, GED_CMD_DEFAULT};
const struct ged_cmd open_cmd = { &open_cmd_impl };


const struct ged_cmd *open_cmds[] = { &reopen_cmd, &open_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  open_cmds, 2 };

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
