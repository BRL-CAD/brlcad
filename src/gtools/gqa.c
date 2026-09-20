/*                          G _ Q A . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2026 United States Government as represented by
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
 * plot the points where overlaps start/stop
 *
 * Designed to be a framework for 3d sampling of the geometry volume.
 */

#include "common.h"

#include "analyze/gqa.h"
#include "bu/app.h"
#include "bu/cmd.h"
#include "bu/getopt.h"
#include "ged.h"

static char usage[] = "Usage: %s [-A A|a|b|e|g|o|v|w] [-a az] [-d] [-e el] [-f densityFile] [-g spacing|upper, lower|upper-lower] [-G] [-n nhits] [-N nviews] [-p plotPrefix] [-P ncpus] [-q] [-r] [-S nsamples] [-t overlap_tol] [-U useair] [-u len_units vol_units wt_units] [-v] [-V volume_tol] [-W weight_tol] model object [objects...]\n";

int
main(int argc, char *argv[])
{
    int i, j;
    int db_index;
    int c;
    const char **av;
    struct ged *gedp;

    bu_setprogname(argv[0]);

    bu_opterr = 0;
    bu_optind = 1;

    if (argc > 1 && bu_strcmp(argv[1], "--analyze") == 0) {
        int model_index = analyze_gqa_model_argument(argc,
            (const char **)argv);
        if (model_index < 0 || model_index + 1 >= argc)
            bu_exit(1, "Usage: %s --analyze [options] model object [objects...]\n", argv[0]);
        av = (const char **)bu_calloc(argc, sizeof(char *), "analysis argv");
        for (i = j = 0; i < argc; i++) {
            if (i != model_index)
                av[j++] = argv[i];
        }
        av[0] = "gqa";
        gedp = ged_open("db", argv[model_index], 1);
        if (gedp == GED_NULL) {
            bu_free((void *)av, "analysis argv");
            bu_exit(1, "Cannot open %s\n", argv[model_index]);
        }
        c = ged_exec_gqa(gedp, j, av);
        if (bu_vls_strlen(gedp->ged_result_str) > 0)
            bu_log("%s", bu_vls_addr(gedp->ged_result_str));
        ged_close(gedp);
        bu_free((void *)av, "analysis argv");
        return c == BRLCAD_OK ? 0 : 1;
    }

    /* Get past command line options. */
    while ((c = bu_getopt(argc, argv, "A:a:de:f:g:Gn:N:p:P:qrS:t:U:u:vV:W:h?")) != -1) {
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
	    case 'q':
	    case 'r':
	    case 'S':
	    case 't':
	    case 'v':
	    case 'V':
	    case 'W':
	    case 'U':
	    case 'u':
		break;
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
    av[0] = "gqa";
    av[j] = (char *)0;

    if ((gedp = ged_open("db", argv[db_index], 1)) == GED_NULL) {
	bu_free((void *)av, "av");
	bu_exit(1, usage, argv[0]);
    }

    (void)ged_exec_gqa(gedp, j, av);
    if (bu_vls_strlen(gedp->ged_result_str) > 0)
	bu_log("%s", bu_vls_addr(gedp->ged_result_str));
    ged_close(gedp);

    bu_free((void *)av, "av");

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
