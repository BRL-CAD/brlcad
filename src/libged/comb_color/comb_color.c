/*                        C O M B _ C O L O R . C
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
/** @file libged/comb_color.c
 *
 * The comb_color command.
 *
 */

#include "ged.h"

#include "bu/color.h"
#include "bu/cmdschema.h"


static const struct bu_cmd_operand comb_color_schema_operands[] = {
    BU_CMD_OPERAND("combination", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Combination to color", "ged.db_object"),
    BU_CMD_OPERAND_VALIDATE("red", BU_CMD_VALUE_INTEGER, 1, 1,
	bu_rgb_channel_validate, "Red channel, 0 through 255", NULL),
    BU_CMD_OPERAND_VALIDATE("green", BU_CMD_VALUE_INTEGER, 1, 1,
	bu_rgb_channel_validate, "Green channel, 0 through 255", NULL),
    BU_CMD_OPERAND_VALIDATE("blue", BU_CMD_VALUE_INTEGER, 1, 1,
	bu_rgb_channel_validate, "Blue channel, 0 through 255", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema comb_color_cmd_schema = {
    "comb_color", "Set a combination's RGB color", NULL,
    comb_color_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_comb_color_core(struct ged *gedp, int argc, const char *argv[])
{
    unsigned char rgb[3] = {0, 0, 0};
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    static const char *usage = "combination R G B";
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


    operand_index = bu_cmd_schema_parse_complete(&comb_color_cmd_schema,
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argv += operand_index + 1;

    GED_DB_LOOKUP(gedp, dp, argv[0], LOOKUP_NOISY, BRLCAD_ERROR);
    GED_CHECK_COMB(gedp, dp, BRLCAD_ERROR);
    GED_DB_GET_INTERN(gedp, &intern, dp, (fastf_t *)NULL, BRLCAD_ERROR);

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (bu_rgb_from_argv(rgb, 3, argv + 1) != 3)
	return BRLCAD_ERROR;
    VMOVE(comb->rgb, rgb);

    comb->rgb_valid = 1;
    GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_COMB_COLOR_COMMANDS(X, XID) \
    X(comb_color, ged_comb_color_core, GED_CMD_DEFAULT, &comb_color_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_COMB_COLOR_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_comb_color", 1, GED_COMB_COLOR_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
