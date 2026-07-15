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
#include "bu/cmdschema.h"

#include "../ged_private.h"


static const struct bu_cmd_operand cc_operands[] = {
    BU_CMD_OPERAND("name", BU_CMD_VALUE_STRING, 1, 1,
	"New constraint name", NULL),
    BU_CMD_OPERAND("expression", BU_CMD_VALUE_RAW, 1, 1,
	"Constraint expression", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema cc_cmd_schema = {
    "cc", "Create a constraint", NULL, cc_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};

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
    int operand_index = 0;
    int parse_dummy = 0;
    const char *name = NULL;
    const char *expression = NULL;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&cc_cmd_schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0 || argc - 1 - operand_index != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    name = argv[1 + operand_index];
    expression = argv[2 + operand_index];

    GED_CHECK_EXISTS(gedp, name, LOOKUP_QUIET, BRLCAD_ERROR);
    GED_CHECK_EXISTS(gedp, expression, LOOKUP_QUIET, BRLCAD_ERROR);

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
    bu_vls_strcat(&(con_ip->expression), expression);

    GED_DB_DIRADD(gedp, dp, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_NON_GEOM , (void *)&internal.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERN(gedp, dp, &internal, BRLCAD_ERROR);

    bu_vls_printf(gedp->ged_result_str, "Constraint saved");
    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_CC_COMMANDS(X, XID) \
    X(cc, ged_cc_core, GED_CMD_DEFAULT, &cc_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_CC_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_cc", 1, GED_CC_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
