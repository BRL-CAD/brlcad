/*                          G _ Q A . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file g_qa.c
 *
 * Perform quantitative analysis checks on geometry.
 *
 * XXX need to look at gap computation
 *
 * plot the points where overlaps start/stop
 *
 * Designed to be a framework for 3d sampling of the geometry volume.
 */

#include "common.h"
#include "bio.h"

#include "cmd.h"
#include "ged.h"

static char usage[] = "Usage: %s [-A A|a|b|e|g|o|v|w] [-a az] [-d] [-e el] [-f densityFile] [-g spacing|upper, lower|upper-lower] [-G] [-n nhits] [-N nviews] [-p] [-P ncpus] [-r] [-S nsamples] [-t overlap_tol] [-U useair] [-u len_units vol_units wt_units] [-v] [-V volume_tol] [-W weight_tol] model object [object...]\n";

/*
 * M A I N
 */
int
main(int argc, char *argv[])
{
    int i, j;
    int db_index;
    int c;
    const char **av;
    struct ged *gedp;

    bu_opterr = 0;
    bu_optind = 1;

    /* Get past command line options. */
    while ((c = bu_getopt(argc, argv, "A:a:de:f:g:Gn:N:pP:rS:s:t:U:u:vV:W:")) != -1) {
	switch (c) {
	    case 'A':
	    case 'a':
	    case 'e':
	    case 'd':
	    case 'f':
	    case 'g':
	    case 'G':
	    case 'n':
	    case 'N':
	    case 'p':
	    case 'P':
	    case 'r':
	    case 'S':
	    case 't':
	    case 'v':
	    case 'V':
	    case 'W':
	    case 'U':
	    case 'u':
		break;
	    case '?':
	    case 'h':
	    default:
		bu_exit(1, usage, argv[0]);
		break;
	}
    }

    if (bu_optind >= argc) {
	bu_exit(1, usage, argv[0]);
    }

    av = (const char **)bu_calloc(argc, sizeof(char *), "av");

    db_index = bu_optind;
    for (i = j = 0; i < argc; ++i) {
	if (i == db_index)
	    continue;

	av[j] = argv[i];
	++j;
    }
    av[j] = (char *)0;

    if ((gedp = ged_open("db", argv[db_index], 1)) == GED_NULL) {
	bu_free(av, "av");
	bu_exit(1, usage, argv[0]);
    }

    bu_semaphore_reinit(GED_SEM_LAST);

    (void)ged_gqa(gedp, j, av);
    if (bu_vls_strlen(gedp->ged_result_str) > 0)
	bu_log("%s", bu_vls_addr(gedp->ged_result_str));
    ged_close(gedp);

    bu_free(av, "av");

    return 0;
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
