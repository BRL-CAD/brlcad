/*                         C C . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2026 United States Government as represented by
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
/** @file libged/cc.c
 *
 * The cc (create constraint) command.
 *
 */

#include "common.h"

#include <string.h>

#include "raytrace.h"
#include "wdb.h"
#include "bu/cmd.h"
#include "ged/event_txn.h"

#include "../ged_private.h"

/*
 * List constraint objects in this database
 */
int
ged_cc_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "name constraint_expression";

    struct rt_db_internal internal;
    struct rt_constraint_internal *con_ip;
    struct directory *dp;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    GED_CHECK_EXISTS(gedp, argv[1], LOOKUP_QUIET, BRLCAD_ERROR);
    GED_CHECK_EXISTS(gedp, argv[2], LOOKUP_QUIET, BRLCAD_ERROR);

    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_CONSTRAINT;
    internal.idb_meth=&OBJ[ID_CONSTRAINT];

    BU_ALLOC(internal.idb_ptr, struct rt_constraint_internal);
    con_ip = (struct rt_constraint_internal *)internal.idb_ptr;
    con_ip->magic = RT_CONSTRAINT_MAGIC;
    con_ip->id = 324;
    con_ip->type = 4;
    bu_vls_init(&(con_ip->expression));
    bu_vls_strcat(&(con_ip->expression), argv[2]);

    int event_batch_opened = (ged_event_batch_begin(gedp) > 0);
    dp = db_diradd(gedp->dbip, argv[1], RT_DIR_PHONY_ADDR, 0,
	    RT_DIR_NON_GEOM, (void *)&internal.idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Unable to add %s to the database.",
		argv[1]);
	if (event_batch_opened)
	    ged_event_batch_end(gedp, NULL);
	rt_db_free_internal(&internal);
	return BRLCAD_ERROR;
    }
    if (rt_db_put_internal(dp, gedp->dbip, &internal) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database write failure.");
	if (event_batch_opened)
	    ged_event_batch_end(gedp, NULL);
	return BRLCAD_ERROR;
    }
    (void)ged_event_notify_object_added(gedp, argv[1], NULL);
    if (event_batch_opened)
	ged_event_batch_end(gedp, NULL);

    bu_vls_printf(gedp->ged_result_str, "Constraint saved");
    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_CC_COMMANDS(X, XID) \
    X(cc, ged_cc_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_CC_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_cc", 1, GED_CC_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
