/*                       O P T I O N S . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2022 United States Government as represented by
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
/** @file libdm/options.c
 *
 * Option processing routines.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/getopt.h"
#include "bu/str.h"
#include "vmath.h"
#include "dm.h"
#include "./include/private.h"

int
dm_processOptions(struct dm *dmp, struct bu_vls *init_proc_vls, int argc, const char **argv)
{
    int c;

    if (argc < 1 || !argv)
	return 0;

    char **av = bu_argv_dup(argc, argv);

    bu_optind = 0;	 /* re-init bu_getopt */
    bu_opterr = 0;
    while ((c = bu_getopt(argc, av, "N:S:W:s:d:i:n:t:")) != -1) {
	switch (c) {
	    case 'N':
		dmp->i->dm_height = atoi(bu_optarg);
		break;
	    case 'S':
	    case 's':
		dmp->i->dm_width = dmp->i->dm_height = atoi(bu_optarg);
		break;
	    case 'W':
		dmp->i->dm_width = atoi(bu_optarg);
		break;
	    case 'd':
		bu_vls_strcpy(&dmp->i->dm_dName, bu_optarg);
		break;
	    case 'i':
		bu_vls_strcpy(init_proc_vls, bu_optarg);
		break;
	    case 'n':
		if (*bu_optarg != '.')
		    bu_vls_printf(&dmp->i->dm_pathName, ".%s", bu_optarg);
		else
		    bu_vls_strcpy(&dmp->i->dm_pathName, bu_optarg);
		break;
	    case 't':
		dmp->i->dm_top = atoi(bu_optarg);
		break;
	    default:
		bu_log("dm_processOptions: option '%c' unknown\n", bu_optopt);
		break;
	}
    }

    bu_argv_free(argc, av);

    return bu_optind;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
