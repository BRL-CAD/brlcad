/*                       B O T _ D U M P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
 *
 */
/** @file conv/bot_dump.c
 *
 */

#include "common.h"

#include "bu/cmd.h"
#include "bu/getopt.h"
#include "ged.h"

static const char *usage =
    "[-b] [-n] [-m directory] [-o file] [-t dxf|obj|sat|stl] [-u units] geom.g [bot1 bot2 ...]\n";

static void
print_usage(const char *progname)
{
    bu_exit(1, "Usage: %s %s", progname, usage);
}

int
main(int argc, char *argv[])
{
    int i, j;
    int db_index;
    int c;
    const char **av;
    struct ged *gedp;

    bu_setprogname(argv[0]);

    bu_optind = 1;

    /* Get past command line options. */
    while ((c = bu_getopt(argc, argv, "bno:m:t:u:h?")) != -1) {
	switch (c) {
	    case 'b':
	    case 'n':
	    case 'm':
	    case 'o':
	    case 't':
	    case 'u':
		break;
	    default:
		print_usage(argv[0]);
		break;
	}
    }

    if (bu_optind >= argc) {
	print_usage(argv[0]);
    }

    av = (const char **)bu_calloc(argc, sizeof(char *), "alloc argv copy");

    db_index = bu_optind;
    for (i = j = 0; i < argc; ++i) {
	if (i == db_index)
	    continue;

	av[j] = argv[i];
	++j;
    }
    av[j] = (char *)0;

    if ((gedp = ged_open("db", argv[db_index], 1)) == GED_NULL) {
	print_usage(argv[0]);
    }

    (void)ged_bot_dump(gedp, j, av);
    if (bu_vls_strlen(gedp->ged_result_str) > 0)
	bu_log("%s", bu_vls_addr(gedp->ged_result_str));
    ged_close(gedp);
    if (gedp)
	BU_PUT(gedp, struct ged);
    bu_free((void *)av, "free argv copy");

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
