/*                         G D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file libged/gdiff.c
 *
 * The gdiff command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"

HIDDEN const char *
gdiff_usage()
{
    return NULL;
}

int
ged_gdiff(struct ged *gedp, int argc, const char *argv[])
{
    int left_dbip_specified = 0;
    int right_dbip_specified = 0;
    int c = 0;
    struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", gdiff_usage());
	return GED_ERROR;
    }

    /* skip command name */
    bu_optind = 1;
    bu_opterr = 1;

    /* parse args */
    while ((c=bu_getopt(argc, (char * const *)argv, "O:N:vh?")) != -1) {
	if (bu_optopt == '?')
	    c='h';
	switch (c) {
	    case 'O' :
		left_dbip_specified = 1;
		bu_vls_sprintf(&tmpstr, "%s", bu_optarg);
		bu_log("Have origin database: %s", bu_vls_addr(&tmpstr));
		break;
	    case 'N' :
		right_dbip_specified = 1;
		bu_vls_sprintf(&tmpstr, "%s", bu_optarg);
		bu_log("Have new database: %s", bu_vls_addr(&tmpstr));
		break;
	    case 'v' :
		bu_log("Reporting mode is verbose");
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s", gdiff_usage());
		return GED_ERROR;
	}
    }

    /* There are possible convention-based interpretations of 1, 2, 3, 4 and n args
     * beyond those used as options.  For the shortest cases, the interpretation depends
     * on whether one or two .g files are known:
     *
     * a) No .g file specified
     * 1 - objname (error)
     * 1 - file.g (error)
     * 2 - file.g objname (error)
     * 2 - obj1 obj2 (error)
     * 3 - file.g obj1 obj2 (diff two objects in file.g)
     *
     * b) Current .g file known
     * 1 - objname (error)
     * 1 - file.g (diff full .g file contents)
     * 2 - file.g objname (diff obj between current.g and file.g)
     * 2 - obj1 obj2 (diff two objects in current .g)
     * 3 - file.g obj1 obj2 (diff all listed objects between current.g and file.g)
     *
     * .g file args must always come first.
     *
     * A maximum of two .g files can be specified - any addiitonal specification
     * of .g files is an error.
     *
     * When only one environment is specified, all other args must define pairs of
     * objects to compare.
     *
     * When two environments are known, all args will be compared by their instances
     * in the first environment and the second, not against each other in either
     * environment.
     *
     * When there is a current .g environment and two additional .g files are
     * specified, the argv environments will override use of the "current" .g environment.

     if ((argc - bu_optind) == 2) {
     bu_log("left: %s", argv[bu_optind]);
     bu_log("right: %s", argv[bu_optind+1]);
     } else {
     if ((argc - bu_optind) == 1) {
     if (left_dbip_specified || right_dbip_specified)
     bu_log("obj_name: %s", argv[bu_optind]);
     } else {
     bu_vls_printf(gedp->ged_result_str, "Usage: %s", gdiff_usage());
     return GED_ERROR;
     }
     }
     */

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
