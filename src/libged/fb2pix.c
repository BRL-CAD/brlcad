/*                        F B 2 P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
/** @file libged/fb2pix.c
 *
 * fb2pix writes a framebuffer image to a .pix file.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <sys/stat.h>
#include "bio.h"

#ifdef HAVE_WINSOCK_H
#  include <winsock.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "bu.h"
#include "fb.h"
#include "fbserv_obj.h"

#include "pkg.h"
#include "ged.h"


char *file_name;
FILE *outfp;

static int crunch = 0;		/* Color map crunch? */
static int inverse = 0;		/* Draw upside-down */
int screen_height;			/* input height */
int screen_width;			/* input width */


static int
get_args(int argc, char **argv)
{
    int c;

    bu_optind = 1;
    while ((c = bu_getopt(argc, argv, "chiF:s:w:n:")) != -1) {
	switch (c) {
	    case 'c':
		crunch = 1;
		break;
	    case 'h':
		/* high-res */
		screen_height = screen_width = 1024;
		break;
	    case 'i':
		inverse = 1;
		break;
	    case 's':
		/* square size */
		screen_height = screen_width = atoi(bu_optarg);
		break;
	    case 'w':
		screen_width = atoi(bu_optarg);
		break;
	    case 'n':
		screen_height = atoi(bu_optarg);
		break;

	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdout)))
	    return 0;
	file_name = "-";
	outfp = stdout;
    } else {
	file_name = argv[bu_optind];
	if ((outfp = fopen(file_name, "wb")) == NULL) {
	    (void)fprintf(stderr,
			  "fb-pix: cannot open \"%s\" for writing\n",
			  file_name);
	    return 0;
	}
	(void)bu_fchmod(outfp, 0444);
    }

    if (argc > ++bu_optind)
	(void)fprintf(stderr, "fb-pix: excess argument(s) ignored\n");

    return 1;		/* OK */
}


int
ged_fb2pix(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    char usage[] = "Usage: fb-pix [-h -i -c] \n\
	[-s squaresize] [-w width] [-n height] [file.pix]\n";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_FBSERV(gedp, GED_ERROR);
    GED_CHECK_FBSERV_FBP(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    screen_height = screen_width = 512;		/* Defaults */

    if (!get_args(argc, (char **)argv)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

#if defined(_WIN32) && !defined(__CYGWIN__)
    setmode(fileno(stdout), O_BINARY);
#endif

    ret = fb_write_fp(gedp->ged_fbsp->fbs_fbp, outfp,
		      screen_width, screen_height,
		      crunch, inverse, gedp->ged_result_str);

    if (outfp != stdout)
	fclose(outfp);

    if (ret == BRLCAD_OK)
	return GED_OK;

    return GED_ERROR;
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
