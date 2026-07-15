/*                         R O T A T E _ A B O U T . C
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
/** @file libged/rotate_about.c
 *
 * The rotate_about command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"

#include "../ged_private.h"

static const char * const rot_about_keywords[] = {"e", "k", "m", "v", NULL};
static const struct bu_cmd_operand rot_about_schema_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("pivot", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Eye, keypoint, model origin, or view center", NULL, rot_about_keywords),
    BU_CMD_OPERAND_NULL
};
GED_EXPORT const struct bu_cmd_schema ged_rot_about_cmd_schema = {
    "rot_about", "Query or set the rotation pivot", NULL,
    rot_about_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_rotate_about_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[e|k|m|v]";
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get "rotate about" point */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%c", gedp->ged_gvp->gv_rotate_about);
	return BRLCAD_OK;
    }

    if (bu_cmd_schema_parse_complete(&ged_rot_about_cmd_schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
	}

    /* Set rotate_about */
    if (argc == 2 && argv[1][1] == '\0') {
	switch (argv[1][0]) {
	    case 'e':
	    case 'k':
	    case 'm':
	    case 'v':
		gedp->ged_gvp->gv_rotate_about = argv[1][0];
		return BRLCAD_OK;
	}
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
