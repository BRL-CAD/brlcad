/*                        M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
ged_mirror(struct rt_wdb *wdbp, int argc, const char *argv[])
{
    register int k;
    point_t mirror_origin = {0.0, 0.0, 0.0};
    vect_t mirror_dir = {1.0, 0.0, 0.0};
    fastf_t mirror_pt = 0.0;
    static const char *usage = "[-h] [-d dir] [-o origin] [-p scalar_pt] [-x] [-y] [-z] old new";

    int early_out = 0;

#if 0
    char **nargv;
    struct bu_vls vlsargv;
#endif

    GED_CHECK_DATABASE_OPEN(wdbp, GED_ERROR);
    GED_CHECK_READ_ONLY(wdbp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&wdbp->wdb_result_str, 0);
    wdbp->wdb_result = GED_RESULT_NULL;
    wdbp->wdb_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	wdbp->wdb_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&wdbp->wdb_result_str, "Usage: %s %s", argv[0], usage);
	return GED_OK;
    }

#if 1
    bu_optind = 1;
    while ((k = bu_getopt(argc, (char * const *)argv, (const char *)"d:D:hHo:O:p:P:xXyYzZ")) != EOF) {
#else
	/* get a writable copy of argv */
	bu_vls_init(&vlsargv);
	bu_vls_from_argv(&vlsargv, argc, argv);
	nargv = bu_calloc(argc+1, sizeof(char *), "calloc f_ill nargv");
	bu_argv_from_string(nargv, argc, bu_vls_addr(&vlsargv));

	bu_optind = 1;

	/* Process arguments */
	while ((k = bu_getopt(argc, nargv, "d:D:hHo:O:p:P:xXyYzZ")) != EOF) {
#endif
	switch (k) {
	case 'd':
	case 'D':
	    if (sscanf(bu_optarg, "%lf %lf %lf",
		       &mirror_dir[X],
		       &mirror_dir[Y],
		       &mirror_dir[Z]) != 3) {
		bu_vls_printf(&wdbp->wdb_result_str, "Usage: %s %s", argv[0], usage);
		early_out = 1;
	    }
	    break;
	case 'p':
	case 'P':
	    mirror_pt = atof(bu_optarg);
	    break;
	case 'o':
	case 'O':
	    if (sscanf(bu_optarg, "%lf %lf %lf",
		       &mirror_origin[X],
		       &mirror_origin[Y],
		       &mirror_origin[Z]) != 3) {
		bu_vls_printf(&wdbp->wdb_result_str, "Usage: %s %s", argv[0], usage);
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
	    wdbp->wdb_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	    bu_vls_printf(&wdbp->wdb_result_str, "Usage: %s %s", argv[0], usage);

#if 0
		    bu_free(nargv, "free f_ill nargv");
		    bu_vls_free(&vlsargv);
#endif

	    return GED_OK;
	default:
	    bu_vls_printf(&wdbp->wdb_result_str, "Usage: %s %s", argv[0], usage);
	    early_out = 1;
	    break;
	}
	}

#if 0
	bu_free(nargv, "free f_ill nargv");
	bu_vls_free(&vlsargv);
#endif

    if (early_out) {
	return GED_ERROR;
    }

    argc -= bu_optind;

    if (argc < 2) {
	bu_vls_printf(&wdbp->wdb_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* mirror the object */
    VUNITIZE(mirror_dir);

    if (rt_mirror(wdbp->dbip,
		  argv[bu_optind],
		  argv[bu_optind+1],
		  mirror_origin,
		  mirror_dir,
		  mirror_pt,
		  &rt_uniresource) == DIR_NULL) {
	bu_vls_printf(&wdbp->wdb_result_str, "%s: not able to perform the mirror", argv[0]);
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
