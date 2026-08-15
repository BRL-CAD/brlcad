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

#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "rt/geom.h"
#include "wdb.h"
#include "../ged_private.h"

/* FIXME - we want the DSP macro for convenience here - should this be in include/rt/dsp.h ? */
#include "../librt/primitives/dsp/dsp.h"


static const struct bu_cmd_operand dsp_root_operands[] = {
    BU_CMD_OPERAND("dsp_object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"DSP object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema dsp_root_schema = {
    "dsp", "Inspect DSP height data", NULL, dsp_root_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_operand dsp_xy_operands[] = {
    BU_CMD_OPERAND_INTEGER_RANGE("x", 1, 1, 0, INT_MAX,
	"Nonnegative DSP grid X coordinate", NULL),
    BU_CMD_OPERAND_INTEGER_RANGE("y", 1, 1, 0, INT_MAX,
	"Nonnegative DSP grid Y coordinate", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema dsp_xy_schema = {
    "xy", "Report the height value at an X,Y grid coordinate", NULL,
    dsp_xy_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_operand dsp_diff_operands[] = {
    BU_CMD_OPERAND("comparison_dsp", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"DSP object to compare", "ged.db_object"),
    BU_CMD_OPERAND("minimum_difference", BU_CMD_VALUE_NUMBER, 0, 1,
	"Minimum height difference to report", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema dsp_diff_schema = {
    "diff", "Report height differences between two DSP objects", NULL,
    dsp_diff_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_tree_node dsp_subcommands[] = {
    BU_CMD_TREE_NODE(&dsp_xy_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&dsp_diff_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree dsp_tree = {
    &dsp_root_schema, dsp_subcommands, BU_CMD_TREE_CHILD_AFTER_FIXED_OPERANDS
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
    struct bu_cmd_validate_result validation = BU_CMD_VALIDATE_RESULT_NULL;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 3) {
	char *help = bu_cmd_tree_help(&dsp_tree, cmd);
	if (help) {
	    bu_vls_strcat(gedp->ged_result_str, help);
	    bu_free(help, "dsp native help");
	}
	return BRLCAD_ERROR;
    }


    if (bu_cmd_tree_validate_argv(&dsp_tree, argc - 1, argv + 1,
	argc - 1, &validation) != 0 || validation.state != BU_CMD_VALIDATE_VALID) {
	if (validation.hint)
	    bu_vls_printf(gedp->ged_result_str,
		validation.state == BU_CMD_VALIDATE_INCOMPLETE ? "%s expected\n" : "%s\n",
		validation.hint);
	bu_cmd_validate_result_clear(&validation);
	return BRLCAD_ERROR;
    }
    bu_cmd_validate_result_clear(&validation);

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

static int
ged_dsp_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &dsp_tree, input, cursor_pos, result);
}

static int
ged_dsp_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &dsp_tree, input, analysis);
}

static char *
ged_dsp_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&dsp_tree);
}

static int
ged_dsp_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&dsp_tree, msgs);
}
GED_CMD_TREE_HELP(ged_dsp_grammar_help, dsp_tree)

static const struct ged_cmd_grammar ged_dsp_grammar = {
    "dsp", "Inspect DSP height data", ged_dsp_grammar_validate,
    ged_dsp_grammar_analyze, ged_dsp_grammar_json, ged_dsp_grammar_lint, NULL,
    ged_dsp_grammar_help
};

#define GED_DSP_COMMANDS(X, XID, N, NID, G, GID) \
    G(dsp, ged_dsp_core, GED_CMD_DEFAULT, &ged_dsp_grammar) \

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_DSP_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_dsp", 1, GED_DSP_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
