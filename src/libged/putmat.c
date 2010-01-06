/*                         P U T M A T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2010 United States Government as represented by
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
/** @file putmat.c
 *
 * The putmat command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


/*
 *	Replace the matrix on an arc in the database from the command line,
 *	when NOT in an edit state.  Used mostly to facilitate writing shell
 *	scripts.  There are two valid syntaxes, each of which is implemented
 *	as an appropriate call to f_arced.  Commands of the form
 *
 *		    putmat a/b m0 m1 ... m15
 *
 *	are converted to
 *
 *		arced a/b matrix rarc m0 m1 ... m15,
 *
 *	while commands of the form
 *
 *			    putmat a/b I
 *
 *	are converted to
 *
 *	    arced a/b matrix rarc 1 0 0 0   0 1 0 0   0 0 1 0   0 0 0 1.
 *
 */
int
ged_putmat(struct ged *gedp, int argc, const char *argv[])
{
    int			result = GED_OK;	/* Return code */
    char		*newargv[20+2];
    struct bu_vls	*avp;
    int			got;
    static const char *usage = "a/b I|m0 m1 ... m15";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 18 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (!strchr( argv[1], '/')) {
	bu_vls_printf(&gedp->ged_result_str, "%s: bad path spec '%s'\n", argv[0], argv[1]);
	return GED_ERROR;
    }
    switch (argc) {
    case 18:
	avp = bu_vls_vlsinit();
	bu_vls_from_argv(avp, 16, (const char **)argv + 2);
	break;
    case 3:
	if ((argv[2][0] == 'I') && (argv[2][1] == '\0')) {
	    avp = bu_vls_vlsinit();
	    bu_vls_printf(avp, "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 ");
	    break;
	}
	/* Sometimes the matrix is sent through Tcl as one long string.
	 * Copy it so we can crack it, below.
	 */
	avp = bu_vls_vlsinit();
	bu_vls_strcat(avp, argv[2]);
	break;
    default:
	bu_vls_printf(&gedp->ged_result_str, "%s: error in matrix specification (wrong number of args)\n", argv[0]);
	return GED_ERROR;
    }
    newargv[0] = "arced";
    newargv[1] = (char *)argv[1];
    newargv[2] = "matrix";
    newargv[3] = "rarc";

    got = bu_argv_from_string( &newargv[4], 16, bu_vls_addr(avp) );
    if (got != 16)  {
	bu_vls_printf(&gedp->ged_result_str, "%s: %s:%d: bad matrix, only got %d elements\n",
		      argv[0], __FILE__, __LINE__, got);
	result = GED_ERROR;
    }

    if (result != GED_ERROR)
	result = ged_arced(gedp, 20, (const char **)newargv);

    bu_vls_vlsfree(avp);
    return result;
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
