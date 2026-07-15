/*                         R F A R B . C
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
/** @file libged/rfarb.c
 *
 * The rfarb command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "rt/geom.h"
#include "../ged_private.h"

static const struct bu_cmd_schema *rfarb_schema(void);

static int
rfarb_thickness_validate(struct bu_vls *msg, const char *arg)
{
    char *end = NULL;
    double value;

    if (!arg || !arg[0])
	goto invalid;
    value = strtod(arg, &end);
    if (!end || *end || ZERO(value))
	goto invalid;
    return 0;

invalid:
    if (msg)
	bu_vls_printf(msg, "thickness must be nonzero");
    return -1;
}


int
ged_rfarb_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal internal;
    struct rt_arb_internal *aip;

    int i;
    int solve[3];
    vect_t norm;
    fastf_t ndotv;

    /* intentionally double for scan */
    double known_pt[3];
    double pt[3][2];
    double thick;
    double rota;
    double fba;

    static const char *usage = "name pX pY pZ rA fbA c X Y c X Y c X Y th";
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


    operand_index = bu_cmd_schema_parse_complete(rfarb_schema(),
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (db_lookup(gedp->dbip, argv[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s already exists\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &known_pt[X]) != 1 ||
	sscanf(argv[3], "%lf", &known_pt[Y]) != 1 ||
	sscanf(argv[4], "%lf", &known_pt[Z]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad value - %s %s %s",
		      argv[0], argv[2], argv[3], argv[4]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[5], "%lf", &rota) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad rotation angle - %s", argv[0], argv[5]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[6], "%lf", &fba) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad fallback angle - %s", argv[0], argv[6]);
	return BRLCAD_ERROR;
    }

    rota *= DEG2RAD;
    fba *= DEG2RAD;

    /* calculate plane defined by these angles */
    norm[0] = cos(fba) * cos(rota);
    norm[1] = cos(fba) * sin(rota);
    norm[2] = sin(fba);

    for (i = 0; i < 3; i++) {
	switch (argv[7+3*i][0]) {
	    case 'x':
		if (ZERO(norm[0])) {
		    bu_vls_printf(gedp->ged_result_str, "X not unique in this face\n");
		    return BRLCAD_ERROR;
		}
		solve[i] = X;

		if (sscanf(argv[7+3*i+1], "%lf", &pt[i][X]) != 1 ||
		    sscanf(argv[7+3*i+2], "%lf", &pt[i][Y]) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "%s: at least one bad value - %s %s",
				  argv[0], argv[7+3*i+1], argv[7+3*i+2]);
		}

		pt[i][X] *= gedp->dbip->dbi_local2base;
		pt[i][Y] *= gedp->dbip->dbi_local2base;
		break;

	    case 'y':
		if (ZERO(norm[1])) {
		    bu_vls_printf(gedp->ged_result_str, "Y not unique in this face\n");
		    return BRLCAD_ERROR;
		}
		solve[i] = Y;

		if (sscanf(argv[7+3*i+1], "%lf", &pt[i][X]) != 1 ||
		    sscanf(argv[7+3*i+2], "%lf", &pt[i][Y]) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "%s: at least one bad value - %s %s",
				  argv[0], argv[7+3*i+1], argv[7+3*i+2]);
		}

		pt[i][X] *= gedp->dbip->dbi_local2base;
		pt[i][Y] *= gedp->dbip->dbi_local2base;
		break;

	    case 'z':
		if (ZERO(norm[2])) {
		    bu_vls_printf(gedp->ged_result_str, "Z not unique in this face\n");
		    return BRLCAD_ERROR;
		}
		solve[i] = Z;

		if (sscanf(argv[7+3*i+1], "%lf", &pt[i][X]) != 1 ||
		    sscanf(argv[7+3*i+2], "%lf", &pt[i][Y]) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "%s: at least one bad value - %s %s",
				  argv[0], argv[7+3*i+1], argv[7+3*i+2]);
		}

		pt[i][X] *= gedp->dbip->dbi_local2base;
		pt[i][Y] *= gedp->dbip->dbi_local2base;
		break;

	    default:
		bu_vls_printf(gedp->ged_result_str, "coordinate must be x, y, or z\n");
		return BRLCAD_ERROR;
	}
    }

    if (sscanf(argv[7+3*3], "%lf", &thick) != 1 || ZERO(thick)) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad thickness - %s", argv[0], argv[7+3*3]);
	return BRLCAD_ERROR;
    }
    thick *= gedp->dbip->dbi_local2base;

    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_ARB8;
    internal.idb_meth = &OBJ[ID_ARB8];
    BU_ALLOC(internal.idb_ptr, struct rt_arb_internal);
    aip = (struct rt_arb_internal *)internal.idb_ptr;
    aip->magic = RT_ARB_INTERNAL_MAGIC;

    for (i = 0; i < 8; i++) {
	VSET(aip->pt[i], 0.0, 0.0, 0.0);
    }

    VSCALE(aip->pt[0], known_pt, gedp->dbip->dbi_local2base);

    ndotv = VDOT(aip->pt[0], norm);

    /* calculate the unknown coordinate for points 2, 3, 4 */
    for (i = 0; i < 3; i++) {
	int j;
	j = i+1;

	switch (solve[i]) {
	    case X:
		aip->pt[j][Y] = pt[i][0];
		aip->pt[j][Z] = pt[i][1];
		aip->pt[j][X] = (ndotv
				 - norm[1] * aip->pt[j][Y]
				 - norm[2] * aip->pt[j][Z])
		    / norm[0];
		break;
	    case Y:
		aip->pt[j][X] = pt[i][0];
		aip->pt[j][Z] = pt[i][1];
		aip->pt[j][Y] = (ndotv
				 - norm[0] * aip->pt[j][X]
				 - norm[2] * aip->pt[j][Z])
		    / norm[1];
		break;
	    case Z:
		aip->pt[j][X] = pt[i][0];
		aip->pt[j][Y] = pt[i][1];
		aip->pt[j][Z] = (ndotv
				 - norm[0] * aip->pt[j][X]
				 - norm[1] * aip->pt[j][Y])
		    / norm[2];
		break;

	    default:
		return BRLCAD_ERROR;
	}
    }

    /* calculate the remaining 4 vertices */
    for (i = 0; i < 4; i++) {
	VJOIN1(aip->pt[i+4], aip->pt[i], thick, norm);
    }

    dp = db_diradd(gedp->dbip, argv[1], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Cannot add %s to the directory\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, gedp->dbip, &internal) < 0) {
	rt_db_free_internal(&internal);
	bu_vls_printf(gedp->ged_result_str, "%s: Database write error, aborting.\n", argv[0]);
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

static const char * const rfarb_coord_keywords[] = {"x", "y", "z", NULL};
static const struct bu_cmd_operand rfarb_schema_operands[] = {
    BU_CMD_OPERAND("name", BU_CMD_VALUE_STRING, 1, 1, "New ARB name", NULL),
    BU_CMD_OPERAND("known_point", BU_CMD_VALUE_NUMBER, 3, 3, "Known X Y Z point", NULL),
    BU_CMD_OPERAND("angles", BU_CMD_VALUE_NUMBER, 2, 2, "Rotation and fallback angles", NULL),
    BU_CMD_OPERAND_KEYWORDS("coordinate_1", BU_CMD_VALUE_KEYWORD, 1, 1,
	"First coordinate to solve", NULL, rfarb_coord_keywords),
    BU_CMD_OPERAND("point_1", BU_CMD_VALUE_NUMBER, 2, 2, "First pair of known coordinates", NULL),
    BU_CMD_OPERAND_KEYWORDS("coordinate_2", BU_CMD_VALUE_KEYWORD, 1, 1,
	"Second coordinate to solve", NULL, rfarb_coord_keywords),
    BU_CMD_OPERAND("point_2", BU_CMD_VALUE_NUMBER, 2, 2, "Second pair of known coordinates", NULL),
    BU_CMD_OPERAND_KEYWORDS("coordinate_3", BU_CMD_VALUE_KEYWORD, 1, 1,
	"Third coordinate to solve", NULL, rfarb_coord_keywords),
    BU_CMD_OPERAND("point_3", BU_CMD_VALUE_NUMBER, 2, 2, "Third pair of known coordinates", NULL),
    BU_CMD_OPERAND_VALIDATE("thickness", BU_CMD_VALUE_NUMBER, 1, 1,
	rfarb_thickness_validate, "Nonzero ARB thickness", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema rfarb_cmd_schema = {
    "rfarb", "Create an ARB from a point and plane angles", NULL,
    rfarb_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};

static const struct bu_cmd_schema *
rfarb_schema(void)
{
    return &rfarb_cmd_schema;
}

#define GED_RFARB_COMMANDS(X, XID) \
    X(rfarb, ged_rfarb_core, GED_CMD_DEFAULT, &rfarb_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_RFARB_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_rfarb", 1, GED_RFARB_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
