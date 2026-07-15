/*                        W M A T E R . C
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
/** @file libged/wmater.c
 *
 * The wmater command.
 *
 */

#include "ged.h"

#include "bu/cmdschema.h"


static const struct bu_cmd_operand wmater_schema_operands[] = {
    BU_CMD_OPERAND("output_file", BU_CMD_VALUE_FILE, 1, 1,
	"Material output file", "ged.file_path"),
    BU_CMD_OPERAND("combinations", BU_CMD_VALUE_DB_OBJECT, 1,
	BU_CMD_COUNT_UNLIMITED, "Combinations to write", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema wmater_cmd_schema = {
    "wmater", "Write combination material properties", NULL,
    wmater_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_wmater_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int status = BRLCAD_OK;
    FILE *fp;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    static const char *usage = "filename comb1 [comb2 ...]";
    const char *command;
    int operand_index;
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);
    command = argv[0];

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }


    operand_index = bu_cmd_schema_parse_complete(&wmater_cmd_schema,
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    fp = fopen(argv[0], "a");
    if (fp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Failed to open file - %s", command, argv[0]);
	return BRLCAD_ERROR;
    }


    for (i = 1; i < argc; ++i) {
	if ((dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s: Failed to find %s", command, argv[i]);
	    status = BRLCAD_ERROR;
	    continue;
	}
	if ((dp->d_flags & RT_DIR_COMB) == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%s: %s is not a combination", command, dp->d_namep);
	    status = BRLCAD_ERROR;
	    continue;
	}
	if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "%s: Unable to read %s from database", command, argv[i]);
	    status = BRLCAD_ERROR;
	    continue;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	fprintf(fp, "\"%s\"\t\"%s\"\t%d\t%d\t%d\t%d\t%d\n", argv[i],
		bu_vls_strlen(&comb->shader) > 0 ?
		bu_vls_addr(&comb->shader) : "-",
		comb->rgb[0], comb->rgb[1], comb->rgb[2],
		comb->rgb_valid, comb->inherit);
	rt_db_free_internal(&intern);
    }

    (void)fclose(fp);
    return status;
}


#include "../include/plugin.h"

#define GED_WMATER_COMMANDS(X, XID) \
    X(wmater, ged_wmater_core, GED_CMD_DEFAULT, &wmater_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_WMATER_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_wmater", 1, GED_WMATER_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
