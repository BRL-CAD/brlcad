/*                         E R A S E . C
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
/** @file libged/erase.c
 *
 * The erase command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmdschema.h"

#include "../ged_private.h"


static void
erase_legacy_show_help(struct ged *gedp, const char *command)
{
    char *option_help = bu_cmd_schema_describe(&ged_erase_legacy_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: %s [-r] object ... | %s [-o] -A attribute value [attribute value ...]",
	command, command);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "\nOptions:\n%s", option_help);
	bu_free(option_help, "erase legacy native option help");
    }
}


extern int ged_erase2_core(struct ged *gedp, int argc, const char **argv);
/*
 * Erase objects from the display.
 *
 */
int
ged_erase_core(struct ged *gedp, int argc, const char *argv[])
{
    if (gedp->new_cmd_forms)
	return ged_erase2_core(gedp, argc, argv);

    size_t i;
    int operand_index;
    int operand_count;
    struct ged_erase_legacy_args args = {0, 0, 0};

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	erase_legacy_show_help(gedp, argv[0]);
	return GED_HELP;
    }


    operand_index = bu_cmd_schema_parse_complete(&ged_erase_legacy_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	erase_legacy_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }
    operand_count = argc - 1 - operand_index;
    argv += operand_index + 1;
    argc = operand_count;

    if (args.recursive) {
	for (i = 0; i < (size_t)argc; ++i)
	    _dl_eraseAllPathsFromDisplay(gedp, argv[i], 0);
	return BRLCAD_OK;
    }

    if (args.attributes) {
	/* args are attribute name/value pairs */
	struct bu_attribute_value_set avs;
	struct bu_ptbl *tbl;

	if (!gedp->dbip) {
	    bu_vls_printf(gedp->ged_result_str, "Error: -A option requires an open database\n");
	    return BRLCAD_ERROR;
	}

	bu_avs_init(&avs, argc/2, "ged_erase_core avs");
	i = 0;
	while (i < (size_t)argc) {
	    if (args.match_any) {
		bu_avs_add_nonunique(&avs, argv[i], argv[i+1]);
	    } else {
		bu_avs_add(&avs, argv[i], argv[i+1]);
	    }
	    i += 2;
	}

	tbl = db_lookup_by_attr(gedp->dbip, RT_DIR_REGION | RT_DIR_SOLID | RT_DIR_COMB,
		&avs, args.match_any ? 2 : 1);
	bu_avs_free(&avs);
	if (!tbl) {
	    bu_log("Error: db_lookup_by_attr() failed!!\n");
	    return BRLCAD_ERROR;
	}
	if (BU_PTBL_LEN(tbl) < 1) {
	    /* nothing matched, just return */
	    bu_ptbl_free(tbl);
	    bu_free((char *)tbl, "ged_erase_core ptbl");
	    return BRLCAD_OK;
	}
	for (i = 0; i < BU_PTBL_LEN(tbl); i++) {
	    struct directory *dp;

	    dp = (struct directory *)BU_PTBL_GET(tbl, i);
	    dl_erasePathFromDisplay(gedp, dp->d_namep, 1);
	}

	bu_ptbl_free(tbl);
	bu_free((char *)tbl, "ged_erase_core ptbl");
    } else {
	for (i = 0; i < (size_t)argc; ++i)
	    dl_erasePathFromDisplay(gedp, argv[i], 1);
    }

    return BRLCAD_OK;
}

#include "../include/plugin.h"

extern GED_EXPORT const struct ged_cmd_grammar ged_erase_grammar;

#define GED_ERASE_COMMANDS(X, XID) \
    X(erase, ged_erase_core, GED_CMD_DEFAULT, &ged_erase_grammar) \
    X(d, ged_erase_core, GED_CMD_DEFAULT, &ged_erase_grammar) \

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_ERASE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_erase", 1, GED_ERASE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
