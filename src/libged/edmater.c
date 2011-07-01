/*                       E D M A T E R . C
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
/** @file libged/edmater.c
 *
 * The edmater command.
 *
 * Relies on: rmater, editit, wmater
 *
 */

#include "common.h"

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "./ged_private.h"

int
ged_edmater(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    int i, c;
    int status;
    const char **av;
    static const char *usage = "comb(s)";
    char tmpfil[MAXPATHLEN];
    const char *editstring = NULL;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    bu_optind = 1;
    /* First, grab the editstring off of the argv list */
    while ((c = bu_getopt(argc, (char * const *)argv, "E:")) != -1) {
	switch (c) {
	    case 'E' :
		editstring = bu_optarg;
		break;
	    default :
		break;
	}
    }

    argc -= bu_optind - 1;
    argv += bu_optind - 1;


    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    fp = bu_temp_file(tmpfil, MAXPATHLEN);
    if (!fp)
	return TCL_ERROR;

    av = (const char **)bu_malloc(sizeof(char *)*(argc + 2), "f_edmater: av");
    av[0] = "wmater";
    av[1] = tmpfil;
    for (i = 2; i < argc + 1; ++i)
	av[i] = argv[i-1];

    av[i] = NULL;

    if (ged_wmater(gedp, argc, av) == TCL_ERROR) {
	(void)unlink(tmpfil);
	bu_free((genptr_t)av, "f_edmater: av");
	return TCL_ERROR;
    }

    (void)fclose(fp);

    if (_ged_editit(editstring, tmpfil)) {
	av[0] = "rmater";
	av[2] = NULL;
	status = ged_rmater(gedp, 2, av);
    } else {
	status = TCL_ERROR;
    }

    (void)unlink(tmpfil);
    bu_free((genptr_t)av, "ged_edmater: av");

    return status;
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
