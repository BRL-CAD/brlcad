/*                             B O . C
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
/** @file libged/bo.c
 *
 * The 'bo' binary object command, used for importing and exporting
 * between files and binary objects stored in a geometry database.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/getopt.h"
#include "ged.h"


static int
bo_decode_input_type(unsigned int *input_type, const char *type_string)
{
    const char *type = type_string;
    unsigned int order = 0;
    unsigned int minor_type;
    size_t type_length = strlen(type_string);

    if (type_length == 2) {
	if (type_string[0] == 'n') {
	    order = RT_BINUNIF_NETWORK_ORDER;
	} else if (type_string[0] != 'h') {
	    return -1;
	}
	type++;
    } else if (type_length != 1) {
	return -1;
    }

    switch ((int)type[0]) {
	case 'f':
	    minor_type = DB5_MINORTYPE_BINU_FLOAT;
	    break;
	case 'd':
	    minor_type = DB5_MINORTYPE_BINU_DOUBLE;
	    break;
	case 'c':
	    minor_type = DB5_MINORTYPE_BINU_8BITINT;
	    break;
	case 's':
	    minor_type = DB5_MINORTYPE_BINU_16BITINT;
	    break;
	case 'i':
	    minor_type = DB5_MINORTYPE_BINU_32BITINT;
	    break;
	case 'l':
	    minor_type = DB5_MINORTYPE_BINU_64BITINT;
	    break;
	case 'C':
	    minor_type = DB5_MINORTYPE_BINU_8BITINT_U;
	    break;
	case 'S':
	    minor_type = DB5_MINORTYPE_BINU_16BITINT_U;
	    break;
	case 'I':
	    minor_type = DB5_MINORTYPE_BINU_32BITINT_U;
	    break;
	case 'L':
	    minor_type = DB5_MINORTYPE_BINU_64BITINT_U;
	    break;
	default:
	    return -1;
    }

    *input_type = minor_type | order;
    return 0;
}


int
ged_bo_core(struct ged *gedp, int argc, const char *argv[])
{
    int c;
    unsigned int input_type = 0;
    int input_mode = 0;
    int output_mode = 0;
    const char *argv0;
    static const char *usage = "{-i u [h|n]type | -o} dest source";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    argv0 = argv[0];

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv0, usage);
	return GED_HELP;
    }

    /* check that we are using a version 5 database */
    if (db_version(gedp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str, "This is an older database version.\nIt does not support binary objects.Use \"dbupgrade\" to upgrade this database to the current version.\n");
	return BRLCAD_ERROR;
    }

    bu_optind = 1;		/* re-init bu_getopt() */
    const int show_option_errors = bu_opterr;
    bu_opterr = 0;          /* suppress bu_getopt()'s error message */
    while ((c = bu_getopt(argc, (char * const *)argv, "io")) != -1) {
	switch (c) {
	    case 'i':
		input_mode = 1;
		break;
	    case 'o':
		output_mode = 1;
		break;
	    default:
		bu_opterr = show_option_errors;
		bu_vls_printf(gedp->ged_result_str, "Unrecognized option - %c", bu_optopt);
		return BRLCAD_ERROR;
	}
    }
    bu_opterr = show_option_errors;

    if (input_mode + output_mode != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv0, usage);
	return BRLCAD_ERROR;
    }

    argc -= bu_optind;
    argv += bu_optind;

    if ((input_mode && argc != 4) || (output_mode && argc != 2)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv0, usage);
	return BRLCAD_ERROR;
    }

    if (output_mode) {
	const enum db5_save_result status = db5_export_binunif(gedp->dbip, argv[1], argv[0], gedp->ged_result_str);
	return status == DB5_SAVE_OK || status == DB5_SAVE_SYNC_FAILED ? BRLCAD_OK : BRLCAD_ERROR;
    }

    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    if (!BU_STR_EQUAL(argv[0], "u")) {
	bu_vls_printf(gedp->ged_result_str, "Unrecognized major type: %s", argv[0]);
	return BRLCAD_ERROR;
    }
    if (bo_decode_input_type(&input_type, argv[1]) != 0) {
	bu_vls_printf(gedp->ged_result_str, "Unrecognized minor type: %s", argv[1]);
	return BRLCAD_ERROR;
    }

    const char *object = argv[2];
    GED_CHECK_EXISTS(gedp, object, LOOKUP_QUIET, BRLCAD_ERROR);
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (rt_mk_binunif(wdbp, object, argv[3], input_type, 0)) {
	bu_vls_printf(gedp->ged_result_str, "Error creating %s", object);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_BO_COMMANDS(X, XID) \
    X(bo, ged_bo_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_BO_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_bo", 1, GED_BO_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
