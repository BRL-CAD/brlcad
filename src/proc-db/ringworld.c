/*                    R I N G W O R L D . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2011 United States Government as represented by
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

/** @file proc-db/ringworld.c
 *
 * Generate a 'ringworld', as imagined by Larry Niven.
 *
 * Some urls/notes
 *
 * http://www.youtube.com/watch?v=sR2296df-bc
 * http://www.freewebs.com/knownspace/rw.htm
 * http://www.alcyone.com/max/reference/scifi/ringworld.html
 *
 * sun: 1.93e26g, 1.36e9m, 3.6e33ergs/sec
 * ring: 2e30g, 1.53e11m radius, 1.605e9m width, 1600km rim walls 200' wide at
 * top. 30m bottom plus 370m foam
 * shadow squares: 20, 1.6e6x4e6m, 4.6e7m orbit
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"

int
main(int argc, char *argv[])
{
    static const char usage[] = "Usage:\n%s [-h] [-o outfile] \n\n  -h      \tShow help\n  -o file \tFile to write out (default: ringworld.g)\n\n";

    char outfile[MAXPATHLEN] = "ringworld.g";
    int optc = 0;

    while ((optc = bu_getopt(argc, argv, "Hho:n:")) != -1) {
	switch (optc) {
	    case 'o':
		snprintf(outfile, MAXPATHLEN, "%s", bu_optarg);;
		break;
	    case 'h' :
	    case 'H' :
	    case '?' :
		printf(usage, *argv);
		return optc == '?' ? EXIT_FAILURE : EXIT_SUCCESS;
	}
    }

    if (bu_file_exists(outfile)) {
	bu_exit(EXIT_FAILURE, "ERROR: %s already exists.  Remove file and try again.", outfile);
    }

    bu_log("Writing ringworld out to [%s]\n", outfile);

    /* do things and stuff. */

    bu_log("BRL-CAD geometry database file [%s] created.\nDone.\n", outfile);

    return EXIT_SUCCESS;
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
