/*                         T R A N S L A T E . C
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
/** @file translate.c
 *
 * The translate command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ged.h"

int
ged_translate(struct ged *gedp, int argc, const char *argv[])
{
    int c;			/* bu_getopt return value */
    int abs_flag = 0;		/* use absolute position */
    int rel_flag = 0;		/* use relative position */
    char *kp_arg = NULL;		/* keypoint */
    const char *cmdName = argv[0];
    static const char *usage = "[-k keypoint]"
				"[[-a] | [-r]]" 
				"xpos [ypos [zpos]]"
				"objects";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	return GED_HELP;
    }

    bu_optind = 1; /* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "ak:r")) != -1) {
        switch (c) {
	case 'a':
	    abs_flag = 1;
	    break;
	case 'k':
	    kp_arg = bu_optarg;
	    break;
	case 'r':
	    rel_flag = 1;
	    break;
	default:
	    switch (bu_optopt) {
	    /* Options that require arguments */
	    case 'k':
		bu_vls_printf(&gedp->ged_result_str,
			      "Missing argument for option -%c", bu_optopt);
		return GED_ERROR;

	    /* Unknown options */
	    default:
		if (isprint(bu_optopt)) {
		    bu_vls_printf(&gedp->ged_result_str,
				  "Unknown option '-%c'", bu_optopt);
		    return GED_ERROR;
		} else {
		    bu_vls_printf(&gedp->ged_result_str,
				  "Unknown option character '\\x%x'",
				  bu_optopt);
		    return GED_ERROR;
		}
	    }
	    break;
        }
    }
    goto disabled;

    return GED_OK;

    /* Not yet working*/
    disabled:
    bu_vls_printf(&gedp->ged_result_str, "Not yet implemented");
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
