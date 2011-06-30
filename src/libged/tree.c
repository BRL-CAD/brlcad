/*                         T R E E . C
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
/** @file libged/tree.c
 *
 * The tree command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "bio.h"

#include "cmd.h"
#include "solid.h"

#include "./ged_private.h"


/*
 * Return the object hierarchy for all object(s) specified or for all currently displayed
 *
 * Usage:
 * tree [-a] [-c] [-o outfile] [-i indentSize] [-d displayDepth] [object(s)]
 *
 */
int
ged_tree(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int j;
    unsigned flags = 0;
    int indentSize = -1;
    int displayDepth = INT_MAX;
    int c;
    FILE *fdout = NULL;
    char *buffer = NULL;
#define WHOARGVMAX 256
    char *whoargv[WHOARGVMAX+1] = {0};
    static const char *usage = "[-a] [-c] [-o outfile] [-i indentSize] [-d displayDepth] [object(s)]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Parse options */
    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "d:i:o:ca")) != -1) {
	switch (c) {
	    case 'i':
		indentSize = atoi(bu_optarg);
		break;
	    case 'a':
		flags |= _GED_TREE_AFLAG;
		break;
	    case 'c':
		flags |= _GED_TREE_CFLAG;
		break;
	    case 'o':
		if ((fdout = fopen(bu_optarg, "w+b")) == NULL) {
		    bu_vls_printf(gedp->ged_result_str, "Failed to open output file, %d", errno);
		    return GED_ERROR;
		}
		break;
	    case 'd':
		displayDepth = atoi(bu_optarg);
		if (displayDepth < 0) {
		    bu_vls_printf(gedp->ged_result_str, "Negative number supplied as depth - unsupported.");
		    return GED_ERROR;
		}
		break;
	    case '?':
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_ERROR;
	}
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    /* tree of all displayed objects */
    if (argc == 1) {
	char *whocmd[2] = {"who", NULL};
	if (ged_who(gedp, 1, (const char **)whocmd) == GED_OK) {
	    buffer = bu_strdup(bu_vls_addr(gedp->ged_result_str));
	    bu_vls_trunc(gedp->ged_result_str, 0);

	    argc += bu_argv_from_string(whoargv, WHOARGVMAX, buffer);
	}
    }

    for (j = 1; j < argc; j++) {
	const char *next = argv[j];
	if (buffer) {
	    next = whoargv[j-1];
	}

	if (j > 1)
	    bu_vls_printf(gedp->ged_result_str, "\n");
	if ((dp = db_lookup(gedp->ged_wdbp->dbip, next, LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;
	_ged_print_node(gedp, dp, 0, indentSize, 0, flags, displayDepth, 0);
    }

    if (buffer) {
	bu_free(buffer, "free who buffer");
	buffer = NULL;
    }

    if (fdout != NULL) {
	fprintf(fdout, "%s", bu_vls_addr(gedp->ged_result_str));
	fclose(fdout);
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
