/*                         S A V E K E Y . C
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
/** @file libged/savekey.c
 *
 * The savekey command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "../ged_private.h"

static const struct bu_cmd_schema *savekey_schema(void);


/**
 * Write out the information that RT's -M option needs to show current view.
 * Note that the model-space location of the eye is a parameter,
 * as it can be computed in different ways.
 * The is the OLD format, needed only when sending to RT on a pipe,
 * due to some oddball hackery in RT to determine old -vs- new format.
 */
static void
savekey_rt_oldwrite(struct ged *gedp, FILE *fp, fastf_t *eye_model)
{
    int i;

    fprintf(fp, "%.9e\n", gedp->ged_gvp->gv_size);
    fprintf(fp, "%.9e %.9e %.9e\n",
		  eye_model[X], eye_model[Y], eye_model[Z]);
    for (i = 0; i < 16; i++) {
	fprintf(fp, "%.9e ", gedp->ged_gvp->gv_rotation[i]);
	if ((i%4) == 3)
	    fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
}


int
ged_savekey_core(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    fastf_t timearg;
    vect_t eye_model;
    vect_t temp;
    static const char *usage = "file [time]";
    int operand_index;
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }


    operand_index = bu_cmd_schema_parse_complete(savekey_schema(),
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    fp = fopen(argv[1], "a");
    if (fp == NULL) {
	perror(argv[1]);
	return BRLCAD_ERROR;
    }
    if (argc > 2) {
	timearg = (fastf_t)strtod(argv[2], NULL);
	fprintf(fp, "%f\n", timearg);
    }
    /*
     * Eye is in conventional place.
     */
    VSET(temp, 0.0, 0.0, 1.0);
    MAT4X3PNT(eye_model, gedp->ged_gvp->gv_view2model, temp);
    savekey_rt_oldwrite(gedp, fp, eye_model);
    (void)fclose(fp);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

static const struct bu_cmd_operand savekey_schema_operands[] = {
    BU_CMD_OPERAND("output_file", BU_CMD_VALUE_FILE, 1, 1, "Keyframe output file", "ged.file_path"),
    BU_CMD_OPERAND("time", BU_CMD_VALUE_NUMBER, 0, 1, "Optional keyframe time", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema savekey_cmd_schema = {
    "savekey", "Save the current view as a keyframe", NULL,
    savekey_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};

static const struct bu_cmd_schema *
savekey_schema(void)
{
    return &savekey_cmd_schema;
}

#define GED_SAVEKEY_COMMANDS(X, XID) \
    X(savekey, ged_savekey_core, GED_CMD_DEFAULT, &savekey_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_SAVEKEY_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_savekey", 1, GED_SAVEKEY_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
