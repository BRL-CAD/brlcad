/*                        I N S I D E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/inside.c
 *
 * The inside command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>


#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "rt/db4.h"
#include "bu/cmdschema.h"

#include "../ged_private.h"

static const struct bu_cmd_schema *inside_schema(void);

int
ged_inside_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *outdp;
    struct rt_db_internal intern;
    int arg = 1;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    RT_DB_INTERNAL_INIT(&intern);

    if (argc < arg+1) {
	bu_vls_printf(gedp->ged_result_str, "Enter name of outside solid: ");
	return GED_MORE;
    }

    if (bu_cmd_schema_parse_complete(inside_schema(), NULL,
	gedp->ged_result_str, argc - 1, argv + 1) < 0)
	return BRLCAD_ERROR;

    if ((outdp = db_lookup(gedp->dbip,  argv[arg], LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s not found", argv[0], argv[arg]);
	return BRLCAD_ERROR;
    }
    ++arg;

    if (rt_db_get_internal(&intern, outdp, gedp->dbip, bn_mat_identity) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return BRLCAD_ERROR;
    }

    return ged_inside_internal(gedp, &intern, argc, argv, arg, outdp->d_namep);
}

#include "../include/plugin.h"

static const struct bu_cmd_operand inside_schema_operands[] = {
    BU_CMD_OPERAND("outside_object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Primitive to hollow", "ged.db_object"),
    BU_CMD_OPERAND("inside_arguments", BU_CMD_VALUE_RAW, 0,
	BU_CMD_COUNT_UNLIMITED, "Primitive-specific inside object arguments", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema inside_cmd_schema = {
    "inside", "Create an inside primitive", NULL, inside_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};

static const struct bu_cmd_schema *
inside_schema(void)
{
    return &inside_cmd_schema;
}

#define GED_INSIDE_COMMANDS(X, XID) \
    X(inside, ged_inside_core, GED_CMD_DEFAULT, &inside_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_INSIDE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_inside", 1, GED_INSIDE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
