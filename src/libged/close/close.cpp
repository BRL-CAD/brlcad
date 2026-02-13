/*                     C L O S E . C P P
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
/** @file libged/close.cpp
 *
 * The close command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bv/lod.h"

#include "../ged_private.h"
#include "../dbi.h"


extern "C" int
ged_close_core(struct ged *gedp, int UNUSED(argc), const char **UNUSED(argv))
{
    // If we don't have an open database, this is a no-op
    if (!gedp || !gedp->dbip)
	return BRLCAD_OK;

    /* set result while we still have the info */
    bu_vls_sprintf(gedp->ged_result_str, "closed %s", gedp->dbip->dbi_filename);

    rt_new_material_head(MATER_NULL);

    /* Clear any geometry displayed in application views.
     * TODO - properly speaking, we should only be zapping geometry data here
     * and not clearing all scene objects... */
    const char *av[1] = {"zap"};
    ged_exec_zap(gedp, 1, (const char **)av);

    /* close current database */
    if (gedp->dbip)
	db_close(gedp->dbip);
    gedp->dbip = NULL;

    /* Clean up any old acceleration states, if present */
    if (gedp->dbi_state)
	delete (DbiState *)gedp->dbi_state;
    gedp->dbi_state = NULL;
    if (gedp->ged_lod)
	bv_mesh_lod_context_destroy(gedp->ged_lod);
    gedp->ged_lod = NULL;

    /* Terminate any ged subprocesses */
    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_subp); i++) {
	struct ged_subprocess *rrp = (struct ged_subprocess *)BU_PTBL_GET(&gedp->ged_subp, i);
	if (gedp->ged_delete_io_handler) {
	    (*gedp->ged_delete_io_handler)(rrp, BU_PROCESS_STDIN);
	    (*gedp->ged_delete_io_handler)(rrp, BU_PROCESS_STDOUT);
	    (*gedp->ged_delete_io_handler)(rrp, BU_PROCESS_STDERR);
	}
	if (!rrp->aborted) {
	    bu_pid_terminate(bu_process_pid(rrp->p));
	    rrp->aborted = 1;
	}
	bu_ptbl_rm(&gedp->ged_subp, (long *)rrp);
	    BU_PUT(rrp, struct ged_subprocess);
    }
    bu_ptbl_reset(&gedp->ged_subp);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_CLOSE_COMMANDS(X, XID) \
    X(closedb, ged_close_core, GED_CMD_DEFAULT) \
    X(close, ged_close_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_CLOSE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_close", 1, GED_CLOSE_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
