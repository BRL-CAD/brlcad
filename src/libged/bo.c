/*                             B O . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
#include <ctype.h>
#include <string.h>

#include "ged.h"


int
ged_bo(struct ged *gedp, int argc, const char *argv[])
{
    int c;
    unsigned int minor_type=0;
    char *obj_name;
    char *file_name;
    int input_mode=0;
    int output_mode=0;
    struct rt_binunif_internal *bip;
    struct rt_db_internal intern;
    struct directory *dp;
    const char *argv0;
    static const char *usage = "{-i major_type minor_type | -o} dest source";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    argv0 = argv[0];

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv0, usage);
	return GED_HELP;
    }

    /* check that we are using a version 5 database */
    if (db_version(gedp->ged_wdbp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str, "This is an older database version.\nIt does not support binary objects.Use \"dbupgrade\" to upgrade this database to the current version.\n");
	return GED_ERROR;
    }

    bu_optind = 1;		/* re-init bu_getopt() */
    bu_opterr = 0;          /* suppress bu_getopt()'s error message */
    while ((c=bu_getopt(argc, (char * const *)argv, "iou:")) != -1) {
	switch (c) {
	    case 'i':
		input_mode = 1;
		break;
	    case 'o':
		output_mode = 1;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Unrecognized option - %c", c);
		return GED_ERROR;

	}
    }

    if (input_mode + output_mode != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv0, usage);
	return GED_ERROR;
    }

    argc -= bu_optind;
    argv += bu_optind;

    if ((input_mode && argc != 4) || (output_mode && argc != 2)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv0, usage);
	return GED_ERROR;
    }


    if (input_mode) {
	if (argv[0][0] == 'u') {

	    if (argv[1][1] != '\0') {
		bu_vls_printf(gedp->ged_result_str, "Unrecognized minor type: %s", argv[1]);
		return GED_ERROR;
	    }

	    switch ((int)argv[1][0]) {
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
		    bu_vls_printf(gedp->ged_result_str, "Unrecognized minor type: %s", argv[1]);
		    return GED_ERROR;
	    }
	} else {
	    bu_vls_printf(gedp->ged_result_str, "Unrecognized major type: %s", argv[0]);
	    return GED_ERROR;
	}

	/* skip past major_type and minor_type */
	argc -= 2;
	argv += 2;

	if (minor_type == 0) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv0, usage);
	    return GED_ERROR;
	}

	obj_name = (char *)*argv;
	GED_CHECK_EXISTS(gedp, obj_name, LOOKUP_QUIET, GED_ERROR);

	argc--;
	argv++;

	file_name = (char *)*argv;

	/* make a binunif of the entire file */
	if (rt_mk_binunif (gedp->ged_wdbp, obj_name, file_name, minor_type, 0)) {
	    bu_vls_printf(gedp->ged_result_str, "Error creating %s", obj_name);
	    return GED_ERROR;
	}

    } else if (output_mode) {
	FILE *fp;

	file_name = (char *)*argv;

	argc--;
	argv++;

	obj_name = (char *)*argv;

	if ((dp=db_lookup(gedp->ged_wdbp->dbip, obj_name, LOOKUP_NOISY)) == RT_DIR_NULL) {
	    return GED_ERROR;
	}
	if (!(dp->d_major_type & DB5_MAJORTYPE_BINARY_MASK)) {
	    bu_vls_printf(gedp->ged_result_str, "%s is not a binary object", obj_name);
	    return GED_ERROR;
	}

	if (dp->d_major_type != DB5_MAJORTYPE_BINARY_UNIF) {
	    bu_vls_printf(gedp->ged_result_str, "source must be a uniform binary object");
	    return GED_ERROR;
	}

	fp = fopen(file_name, "w+b");
	if (fp == NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Error: cannot open file %s for writing", file_name);
	    return GED_ERROR;
	}

	if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, NULL,
			       &rt_uniresource) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Error reading %s from database", dp->d_namep);
	    fclose(fp);
	    return GED_ERROR;
	}

	RT_CK_DB_INTERNAL(&intern);

	bip = (struct rt_binunif_internal *)intern.idb_ptr;
	if (bip->count < 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s has no contents", obj_name);
	    fclose(fp);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}

	if (fwrite(bip->u.int8, bip->count * db5_type_sizeof_h_binu(bip->type),
		   1, fp) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Error writing contents to file");
	    fclose(fp);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}

	fclose(fp);
	rt_db_free_internal(&intern);

    } else {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv0, usage);
	return GED_ERROR;
    }

    return GED_OK;
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
