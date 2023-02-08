/*                       O P E N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/open.cpp
 *
 * The open command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bg/lod.h"

#include "../ged_private.h"


extern "C" int
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
	const char *av[2];
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
	ged_exec(gedp, 1, (const char **)av);

	new_materp = rt_material_head();

	gedp->dbip = old_dbip;
	rt_new_material_head(old_materp);

	/* close current database */
	if (gedp->dbip)
	    db_close(gedp->dbip);
	gedp->dbip = NULL;
	const char *use_dbi_state = getenv("LIBGED_DBI_STATE");
	if (use_dbi_state) {
	    if (gedp->dbi_state)
		delete gedp->dbi_state;
	}
	gedp->dbi_state = NULL;

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

	rt_new_material_head(new_materp);

	/* New database open, need to initialize reference counts */
	db_update_nref(gedp->dbip, &rt_uniresource);


	// LoD context creation (DbiState initialization can use info
	// stored here, so do this first)
	if (gedp->ged_lod)
	    bg_mesh_lod_context_destroy(gedp->ged_lod);
	gedp->ged_lod = bg_mesh_lod_context_create(argv[1]);

	// Set up the DbiState container for fast structure access
	if (use_dbi_state)
	    gedp->dbi_state = new DbiState(gedp);

	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}

extern "C" {
#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl reopen_cmd_impl = {"reopen", ged_reopen_core, GED_CMD_DEFAULT};
const struct ged_cmd reopen_cmd = { &reopen_cmd_impl };

struct ged_cmd_impl open_cmd_impl = {"open", ged_reopen_core, GED_CMD_DEFAULT};
const struct ged_cmd open_cmd = { &open_cmd_impl };


const struct ged_cmd *open_cmds[] = { &reopen_cmd, &open_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  open_cmds, 2 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

