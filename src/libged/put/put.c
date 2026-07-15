/*                         P U T . C
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
/** @file libged/put.c
 *
 * The put command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "ged.h"

static const struct bu_cmd_schema *put_schema(void);

static const struct rt_functab *
put_functab_by_label(const char *label, char normalized[16])
{
    size_t i;

    if (!label || !normalized)
	return NULL;
    for (i = 0; label[i] && i < 15; i++)
	normalized[i] = isupper((unsigned char)label[i]) ?
	    (char)tolower((unsigned char)label[i]) : label[i];
    normalized[i] = '\0';
    return rt_get_functab_by_label(normalized);
}


int
ged_put_core(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal intern;
    const struct rt_functab *ftp;
    char *name;
    char type[16];
    static const char *usage = "object type attrs";
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


    operand_index = bu_cmd_schema_parse_complete(put_schema(),
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    name = (char *)argv[1];

    if (db_lookup(gedp->dbip, argv[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s already exists", argv[1]);
	return BRLCAD_ERROR;
    }

    RT_DB_INTERNAL_INIT(&intern);

    ftp = put_functab_by_label(argv[2], type);
    if (ftp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s is an unknown object type.", type);
	return BRLCAD_ERROR;
    }

    RT_CK_FUNCTAB(ftp);

    if (ftp->ft_make) {
	ftp->ft_make(ftp, &intern);
    } else {
	rt_generic_make(ftp, &intern);
    }

    if (!ftp->ft_adjust || ftp->ft_adjust(gedp->ged_result_str, &intern, argc-3, argv+3) & BRLCAD_ERROR) {
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_put_internal(wdbp, name, &intern, 1.0) < 0) {
	bu_vls_printf(gedp->ged_result_str, "wdb_put_internal(%s)", argv[1]);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    rt_db_free_internal(&intern);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

static int
put_primitive_type_validate(struct bu_vls *msg, const char *arg)
{
    char type[16];

    if (put_functab_by_label(arg, type))
	return 0;
    if (msg)
	bu_vls_printf(msg, "%s is an unknown primitive type", type);
    return -1;
}

static int
put_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || argc < 2)
	return ret;

    /* While editing the primitive-type token, retain the semantic provider's
     * prefix completion.  The complete execution parse and every subsequent
     * operand position require a real librt primitive label. */
    if (cursor_arg == 1 && cursor_arg < argc)
	return 0;
    if (put_primitive_type_validate(NULL, argv[1]) == 0)
	return 0;

    bu_cmd_validate_result_clear(result);
    result->state = BU_CMD_VALIDATE_INVALID;
    result->token_start = 1;
    result->token_end = 1;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = BU_CMD_VALUE_KEYWORD;
    result->hint = "unknown primitive type";
    result->semantic_provider = "ged.primitive_type";
    return 0;
}

static const struct bu_cmd_operand put_schema_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_STRING, 1, 1, "New object name", NULL),
    BU_CMD_OPERAND("primitive_type", BU_CMD_VALUE_STRING, 1, 1,
	"Primitive type", "ged.primitive_type"),
    BU_CMD_OPERAND("attributes", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Primitive-specific attribute/value arguments", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema put_cmd_schema = {
    "put", "Create a database primitive", NULL, put_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {put_schema_validate}
};

static const struct bu_cmd_schema *
put_schema(void)
{
    return &put_cmd_schema;
}

#define GED_PUT_COMMANDS(X, XID) \
    X(put, ged_put_core, GED_CMD_DEFAULT, &put_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_PUT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_put", 1, GED_PUT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
