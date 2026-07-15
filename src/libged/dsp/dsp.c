/*                         D S P . C
 * BRL-CAD
 *
 * Copyright (c) 2017-2026 United States Government as represented by
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
/** @file libged/dsp.c
 *
 * DSP command for displacement map operations.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "rt/geom.h"
#include "wdb.h"
#include "../ged_private.h"

/* FIXME - we want the DSP macro for convenience here - should this be in include/rt/dsp.h ? */
#include "../librt/primitives/dsp/dsp.h"


static const char * const dsp_command_keywords[] = {"xy", "diff", NULL};
static const struct bu_cmd_operand dsp_schema_operands[] = {
    BU_CMD_OPERAND("dsp_object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"DSP object", "ged.db_object"),
    BU_CMD_OPERAND_KEYWORDS("command", BU_CMD_VALUE_KEYWORD, 1, 1,
	"DSP query command", NULL, dsp_command_keywords),
    BU_CMD_OPERAND("command_arguments", BU_CMD_VALUE_RAW, 1, 2,
	"Command-specific arguments", NULL),
    BU_CMD_OPERAND_NULL
};


static int
dsp_validation_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, bu_cmd_value_t type,
	const char *hint, const char *provider)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = type;
    result->hint = hint;
    result->semantic_provider = provider;
    return 0;
}


static int
dsp_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    int ret;
    int xy;
    size_t required;
    size_t maximum;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || argc < 2)
	return ret;

    xy = BU_STR_EQUAL(argv[1], "xy");
    required = xy ? 4 : 3;
    maximum = xy ? 4 : 4;
    if (argc > maximum)
	return dsp_validation_result(result, BU_CMD_VALIDATE_INVALID, maximum,
	    BU_CMD_VALUE_RAW, "too many command arguments", NULL);
    if (argc < required) {
	bu_cmd_value_t expected = xy ? BU_CMD_VALUE_INTEGER :
	    (argc == 2 ? BU_CMD_VALUE_DB_OBJECT : BU_CMD_VALUE_NUMBER);
	return dsp_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, argc,
	    expected, xy ? "DSP grid coordinate expected" :
	    (argc == 2 ? "comparison DSP object expected" : "minimum difference expected"),
	    !xy && argc == 2 ? "ged.db_object" : NULL);
    }
    if (xy) {
	int value;
	for (size_t i = 2; i < 4; i++) {
	    if (!bu_cmd_integer_from_str(&value, argv[i]) || value < 0)
		return dsp_validation_result(result, BU_CMD_VALIDATE_INVALID, i,
		    BU_CMD_VALUE_INTEGER, "nonnegative DSP grid coordinate expected", NULL);
	}
	return dsp_validation_result(result, BU_CMD_VALIDATE_VALID,
	    cursor_arg < argc ? cursor_arg : argc, BU_CMD_VALUE_INTEGER,
	    "DSP grid coordinate", NULL);
    }
    if (argc == 4) {
	fastf_t value;
	if (!bu_cmd_number_from_str(&value, argv[3]))
	    return dsp_validation_result(result, BU_CMD_VALIDATE_INVALID, 3,
		BU_CMD_VALUE_NUMBER, "finite minimum difference expected", NULL);
    }
    return dsp_validation_result(result, BU_CMD_VALIDATE_VALID,
	cursor_arg < argc ? cursor_arg : argc,
	cursor_arg == 2 ? BU_CMD_VALUE_DB_OBJECT : BU_CMD_VALUE_NUMBER,
	cursor_arg == 2 ? "comparison DSP object" : "minimum difference",
	cursor_arg == 2 ? "ged.db_object" : NULL);
}


static const struct bu_cmd_schema dsp_cmd_schema = {
    "dsp", "Inspect DSP height data", NULL, dsp_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(dsp_schema_validate, NULL)
};

int
ged_dsp_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dsp_dp;
    struct rt_db_internal intern;
    struct rt_dsp_internal *dsp;
    const char *cmd = argv[0];
    const char *sub = NULL;
    const char *primitive = NULL;
    static const char *usage = "<obj> [command]\n";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	bu_vls_printf(gedp->ged_result_str, "commands:\n");
	bu_vls_printf(gedp->ged_result_str, "\txy x y                  - report the height value at (x,y)\n");
	bu_vls_printf(gedp->ged_result_str, "\tdiff obj [min_diff_val] - report height differences at x,y coordinates between two dsp objects\n");
	return BRLCAD_ERROR;
    }

    if (bu_cmd_schema_parse_complete(&dsp_cmd_schema, NULL,
	gedp->ged_result_str, argc - 1, argv + 1) < 0)
	return BRLCAD_ERROR;

    /* get dsp */
    primitive = argv[1];
    GED_DB_LOOKUP(gedp, dsp_dp, primitive, LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
    GED_DB_GET_INTERN(gedp, &intern, dsp_dp, bn_mat_identity, BRLCAD_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_DSP) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a DSP solid!", cmd, primitive);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    dsp = (struct rt_dsp_internal *)intern.idb_ptr;
    RT_DSP_CK_MAGIC(dsp);

    /* execute subcommand */
    sub = argv[2];
    if (BU_STR_EQUAL(sub, "xy")) {
	unsigned short elev;
	int gx = 0;
	int gy = 0;
	if (!bu_cmd_integer_from_str(&gx, argv[3]) || gx < 0 ||
		!bu_cmd_integer_from_str(&gy, argv[4]) || gy < 0 ||
		(unsigned int)gx >= dsp->dsp_xcnt || (unsigned int)gy >= dsp->dsp_ycnt) {
	    bu_vls_printf(gedp->ged_result_str, "Error - xy coordinate (%d,%d) is outside max data bounds of dsp: (%d,%d)", gx, gy, dsp->dsp_xcnt, dsp->dsp_ycnt);
	    rt_db_free_internal(&intern);
	    return BRLCAD_ERROR;
	} else {
	    elev = DSP(dsp, gx, gy);
	    bu_vls_printf(gedp->ged_result_str, "%d", elev);
	}
	rt_db_free_internal(&intern);
	return BRLCAD_OK;
    }
    if (BU_STR_EQUAL(sub, "diff")) {
	struct directory *dsp_dp2;
	struct rt_db_internal intern2;
	struct rt_dsp_internal *dsp2;
	fastf_t min_diff = 0.0;
	if (argc == 5 && !bu_cmd_number_from_str(&min_diff, argv[4])) {
	    bu_vls_printf(gedp->ged_result_str, "Error - invalid minimum difference: %s", argv[4]);
	    rt_db_free_internal(&intern);
	    return BRLCAD_ERROR;
	}
	GED_DB_LOOKUP(gedp, dsp_dp2, argv[3], LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
	GED_DB_GET_INTERN(gedp, &intern2, dsp_dp2, bn_mat_identity, BRLCAD_ERROR);

	if (intern2.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern2.idb_minor_type != DB5_MINORTYPE_BRLCAD_DSP) {
	    bu_vls_printf(gedp->ged_result_str, "%s: %s is not a DSP solid!", cmd, argv[3]);
	    rt_db_free_internal(&intern);
	    rt_db_free_internal(&intern2);
	    return BRLCAD_ERROR;
	}

	dsp2 = (struct rt_dsp_internal *)intern2.idb_ptr;
	RT_DSP_CK_MAGIC(dsp2);

	if (dsp->dsp_xcnt != dsp2->dsp_xcnt || dsp->dsp_ycnt != dsp2->dsp_ycnt) {
	    bu_vls_printf(gedp->ged_result_str, "%s xy grid size (%d,%d) differs from that of %s: (%d,%d)", dsp_dp2->d_namep, dsp2->dsp_xcnt, dsp2->dsp_ycnt, dsp_dp->d_namep, dsp->dsp_xcnt, dsp->dsp_ycnt);
	    rt_db_free_internal(&intern);
	    rt_db_free_internal(&intern2);
	    return BRLCAD_OK;
	} else {
	    uint32_t i, j;
	    for (i = 0; i < dsp->dsp_xcnt; i++) {
		for (j = 0; j < dsp->dsp_ycnt; j++) {
		    unsigned short e1 = DSP(dsp, i, j);
		    unsigned short e2 = DSP(dsp2, i, j);
			if (e1 != e2) {
			    unsigned short delta = (e1 > e2) ? e1 - e2 : e2 - e1;
			    if ((fastf_t)delta >= min_diff)
				bu_vls_printf(gedp->ged_result_str, "(%d,%d): %d\n", i, j, delta);
		    }
		}
	    }
	}

	rt_db_free_internal(&intern);
	rt_db_free_internal(&intern2);
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Error - unknown dsp subcommand: %s", sub);
    rt_db_free_internal(&intern);
    return BRLCAD_ERROR;
}

#include "../include/plugin.h"

#define GED_DSP_COMMANDS(X, XID) \
    X(dsp, ged_dsp_core, GED_CMD_DEFAULT, &dsp_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_DSP_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_dsp", 1, GED_DSP_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
