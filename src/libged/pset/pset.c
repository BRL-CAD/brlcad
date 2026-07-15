/*                         P S E T . C
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
/** @file libged/pset.c
 *
 * The pset command.
 */

#include "common.h"

#include <string.h>

#include "bu/cmdschema.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../ged_private.h"


static const struct bu_cmd_operand pset_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_PATH, 1, 1,
	"Primitive object", "ged.db_path"),
    BU_CMD_OPERAND("attribute", BU_CMD_VALUE_STRING, 1, 1,
	"Primitive attribute", NULL),
    BU_CMD_OPERAND("value", BU_CMD_VALUE_NUMBER, 1, 1,
	"New attribute value", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema pset_cmd_schema = {
    "pset", "Set a primitive parameter", NULL, pset_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};

static int
_ged_set_metaball(struct ged *gedp, struct rt_metaball_internal *mbip, const char *attribute, fastf_t sf)
{
    RT_METABALL_CK_MAGIC(mbip);

    switch (attribute[0]) {
	case 'm':
	case 'M':
	    if (sf <= METABALL_METABALL)
		mbip->method = METABALL_METABALL;
	    else if (sf >= METABALL_BLOB)
		mbip->method = METABALL_BLOB;
	    else
		mbip->method = (int)sf;

	    break;
	case 't':
	case 'T':
	    if (sf < 0)
		mbip->threshold = -sf;
	    else
		mbip->threshold = sf;

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad metaball attribute - %s", attribute);
	    return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
ged_pset_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    struct rt_db_internal intern;

    /* intentionally double for scan */
    double val;

    const char *last;
    struct directory *dp;
    static const char *usage = "obj attribute val";
    int operand_index = 0;
    int parse_dummy = 0;
    const char *object = NULL;
    const char *attribute = NULL;
    const char *value = NULL;

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

	operand_index = bu_cmd_schema_parse_complete(&pset_cmd_schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0 || argc - 1 - operand_index != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    object = argv[1 + operand_index];
    attribute = argv[2 + operand_index];
    value = argv[3 + operand_index];

	if (sscanf(value, "%lf", &val) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad value - %s", argv[0], value);
	return BRLCAD_ERROR;
    }

	if ((last = strrchr(object, '/')) == NULL)
	last = object;
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], object);
	return BRLCAD_ERROR;
    }

    if ((dp = db_lookup(gedp->dbip, last, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s not found", argv[0], object);
	return BRLCAD_ERROR;
    }

    GED_DB_GET_INTERN(gedp, &intern, dp, (matp_t)NULL, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&intern);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	bu_vls_printf(gedp->ged_result_str, "%s: Object not eligible for scaling.", argv[0]);
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    switch (intern.idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_METABALL:
	    ret = _ged_set_metaball(gedp, (struct rt_metaball_internal *)intern.idb_ptr, attribute, val);
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "%s: Object not yet supported.", argv[0]);
	    rt_db_free_internal(&intern);

	    return BRLCAD_ERROR;
    }

    if (ret == BRLCAD_OK) {
	GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);
    } else if (ret & BRLCAD_ERROR) {
	rt_db_free_internal(&intern);
    }

    return ret;
}


#include "../include/plugin.h"

#define GED_PSET_COMMANDS(X, XID) \
    X(pset, ged_pset_core, GED_CMD_DEFAULT, &pset_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_PSET_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_pset", 1, GED_PSET_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
