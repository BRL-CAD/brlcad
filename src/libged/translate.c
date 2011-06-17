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
    int c; /* bu_getopt return value */
    char *a_arg; /* absolute position */
    char *k_arg; /* keypoint */
    char *r_arg; /* relative position */
    const char *cmdName = argv[0];
    static const char *usage = "xpos [ypos [zpos]]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1 || argc < 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	return GED_HELP;
    }

    goto disabled; /* DISABLE DISABLE DISABLE DISABLE DISABLE DISABLE DISABLE */
    
    while ((c = bu_getopt(argc, (char * const *)argv, "a:k:r:")) != -1) {
        switch (c) {
            case 'a' :
		a_arg = bu_optarg;
                break;
            case 'k' :
		k_arg = bu_optarg;
                break;
            case 'r' :
		r_arg = bu_optarg;
                break;
	    case '?':
		switch (bu_optopt) {
		    case 'a':
		    case 'k':
		    case 'r':
			bu_vls_printf(&gedp->ged_result_str,
			              "Missing argument for option -%c",
				      bu_optopt);
			return GED_ERROR;
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
            default :
		bu_vls_printf(&gedp->ged_result_str, "Unknown error");
		return GED_ERROR;
                break;
        }
    }
   
    bu_vls_printf(&gedp->ged_result_str, "a:%s k:%s r:%s", a_arg, k_arg, r_arg);
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
