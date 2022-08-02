/*                         C C . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2022 United States Government as represented by
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
	return BRLCAD_HELP;
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

    GED_DB_DIRADD(gedp, dp, argv[1], RT_DIR_PHONY_ADDR, 0, RT_DIR_NON_GEOM , (void *)&internal.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, BRLCAD_ERROR);

    bu_vls_printf(gedp->ged_result_str, "Constraint saved");
    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl cc_cmd_impl = {
    "cc",
    ged_cc_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd cc_cmd = { &cc_cmd_impl };
const struct ged_cmd *cc_cmds[] = { &cc_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  cc_cmds, 1 };

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
