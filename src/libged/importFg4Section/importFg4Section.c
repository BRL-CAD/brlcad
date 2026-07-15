/*              I M P O R T F G 4 S E C T I O N . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file libged/importFg4Section.c
 *
 * This file calls the functions in wdb_importFg4Section.c with the
 * correct wdb, which is derived from the passed in gedp. It imports a
 * Fastgen4 section from a string. This section can only contain
 * GRIDs, CTRIs and CQUADs.
 *
 */
#include "common.h"

#include "bu/cmdschema.h"
#include "../ged_private.h"

static const struct bu_cmd_schema *importfg4_schema(void);

int
ged_importFg4Section_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "obj section";
    int operand_index;
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }


    operand_index = bu_cmd_schema_parse_complete(importfg4_schema(),
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    int ret = wdb_importFg4Section_cmd(wdbp, argc, argv);
    return ret;
}
/** @} */

#include "../include/plugin.h"

static const struct bu_cmd_operand importfg4_schema_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_STRING, 1, 1, "Imported object name", NULL),
    BU_CMD_OPERAND("section", BU_CMD_VALUE_INTEGER, 1, 1, "FASTGEN4 section", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema importfg4_cmd_schema = {
    "importFg4Section", "Import a FASTGEN4 section", NULL,
    importfg4_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};

static const struct bu_cmd_schema *
importfg4_schema(void)
{
    return &importfg4_cmd_schema;
}

#define GED_IMPORTFG4SECTION_COMMANDS(X, XID) \
    X(importFg4Section, ged_importFg4Section_core, GED_CMD_DEFAULT, &importfg4_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_IMPORTFG4SECTION_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_importFg4Section", 1, GED_IMPORTFG4SECTION_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
