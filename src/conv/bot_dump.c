/*                       B O T _ D U M P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
#include "bio.h"

#include "cmd.h"
#include "ged.h"

static char usage[] = "\
Usage: %s [-b] [-n] [-m directory] [-o file] [-t dxf|obj|sat|stl] [-u units] geom.g [bot1 bot2 ...]\n";


/*
 *	M A I N
 *
 */
int
main(int argc, char *argv[])
{
    int i, j;
    int db_index;
    char c;
    const char **av;
    struct ged *gedp;

    bu_optind = 1;

    /* Get past command line options. */
    while ((c = bu_getopt(argc, argv, "bno:m:t:u:")) != -1) {
	switch (c) {
	    case 'b':
	    case 'n':
	    case 'm':
	    case 'o':
	    case 't':
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

    av = bu_calloc(argc, sizeof(char *), "alloc argv copy");

    db_index = bu_optind;
    for (i = j = 0; i < argc; ++i) {
	if (i == db_index)
	    continue;

	av[j] = argv[i];
	++j;
    }
    av[j] = (char *)0;

    if ((gedp = ged_open("db", argv[db_index], 1)) == GED_NULL) {
	bu_exit(1, usage, argv[0]);
    }

    (void)ged_bot_dump(gedp, j, av);
    if (bu_vls_strlen(gedp->ged_result_str) > 0)
	bu_log("%s", bu_vls_addr(gedp->ged_result_str));
    ged_close(gedp);
    bu_free(av, "free argv copy");

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
