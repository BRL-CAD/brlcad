/*                        M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file mirror.c
 *
 * The mirror command.
 *
 */

#include "ged.h"


int
ged_mirror(struct ged *gedp, int argc, const char *argv[])
{
    register int k;
    point_t mirror_origin = {0.0, 0.0, 0.0};
    vect_t mirror_dir = {1.0, 0.0, 0.0};
    fastf_t mirror_pt = 0.0;
    static const char *usage = "[-h] [-d dir] [-o origin] [-p distance] [-x] [-y] [-z] old new";

    int ret;
    register struct directory *dp;
    struct rt_db_internal *ip;
    struct rt_db_internal internal;

    int early_out = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    bu_optind = 1;
    while ((k = bu_getopt(argc, (char * const *)argv, (const char *)"d:D:hHo:O:p:P:xXyYzZ")) != EOF) {
	switch (k) {
	    case 'd':
	    case 'D':
		if (sscanf(bu_optarg, "%lf %lf %lf",
			   &mirror_dir[X],
			   &mirror_dir[Y],
			   &mirror_dir[Z]) != 3) {
		    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    early_out = 1;
		}
		break;
	    case 'p':
	    case 'P':
		if (sscanf(bu_optarg, "%lf", &mirror_pt) != 1) {
		    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    early_out = 1;
		}
		break;
	    case 'o':
	    case 'O':
		if (sscanf(bu_optarg, "%lf %lf %lf",
			   &mirror_origin[X],
			   &mirror_origin[Y],
			   &mirror_origin[Z]) != 3) {
		    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    early_out = 1;
		}
		break;
	    case 'x':
	    case 'X':
		VSET(mirror_dir, 1.0, 0.0, 0.0);
		break;
	    case 'y':
	    case 'Y':
		VSET(mirror_dir, 0.0, 1.0, 0.0);
		break;
	    case 'z':
	    case 'Z':
		VSET(mirror_dir, 0.0, 0.0, 1.0);
		break;
	    case 'h':
	    case 'H':
		bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return BRLCAD_HELP;
	    default:
		bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		early_out = 1;
		break;
	}
    }

    if (early_out) {
	return BRLCAD_ERROR;
    }

    argc -= bu_optind;

    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* make sure object mirroring to does not already exist */
    if (db_lookup(gedp->ged_wdbp->dbip, argv[bu_optind+1], LOOKUP_QUIET) != DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "ERROR: %s already exists\n", argv[bu_optind+1]);
	return BRLCAD_ERROR;
    }

    /* look up the object being mirrored */
    if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[bu_optind], LOOKUP_NOISY)) == DIR_NULL) {
	return BRLCAD_ERROR;
    }

    /* get object being mirrored */
    ret = rt_db_get_internal(&internal, dp, gedp->ged_wdbp->dbip, NULL, gedp->ged_wdbp->wdb_resp);
    if (ret < 0) {
	bu_vls_printf(&gedp->ged_result_str, "ERROR: Unable to load solid [%s]\n", argv[bu_optind]);
	return BRLCAD_ERROR;
    }
    
    /* mirror the object */
    ip = rt_mirror(gedp->ged_wdbp->dbip,
		   &internal,
		   mirror_origin,
		   mirror_dir,
		   mirror_pt,
		   gedp->ged_wdbp->wdb_resp);
    if (ip == NULL) {
	bu_vls_printf(&gedp->ged_result_str, "ERROR: Unable to mirror %s", argv[0]);
	return BRLCAD_ERROR;
    }

    /* add the mirrored object to the directory */
    if ((dp = db_diradd(gedp->ged_wdbp->dbip, argv[bu_optind+1], -1L, 0, dp->d_flags, (genptr_t)&ip->idb_type)) == DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "ERROR: Unable to add [%s] to the database directory", argv[bu_optind+1]);
	return BRLCAD_ERROR;
    }
    /* save the mirrored object to disk */
    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, ip, gedp->ged_wdbp->wdb_resp) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "ERROR: Unable to store [%s] to the database", argv[bu_optind+1]);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
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
