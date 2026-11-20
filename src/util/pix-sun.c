/*                       P I X - S U N . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file util/pix-sun.c
 *
 * Convert a BRL-CAD PIX image to a Sun rasterfile.
 */

#include "common.h"

#include <limits.h>

#include "bio.h"

#include "bu/app.h"
#include "bu/exit.h"
#include "bu/getopt.h"
#include "bu/opt.h"
#include "icv.h"

#define DEFAULT_IMAGE_DIMENSION 512

static char options[] = "s:w:n:dh?";
static char *progname;
static int dither = 0;
static size_t width = DEFAULT_IMAGE_DIMENSION;
static size_t height = DEFAULT_IMAGE_DIMENSION;

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [-s squaresize] [-w width] [-n height] [-d]", progname);
    fprintf(stderr, " < BRLpixfile > rasterfile\n");
    bu_exit(1, NULL);
}

int
main(int argc, char **argv)
{
    icv_image_t *image;
    int option;

    bu_setprogname(argv[0]);
    progname = argv[0];
    if (isatty(fileno(stdin)))
	usage();

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    bu_opterr = 0;

    while ((option = bu_getopt(argc, argv, options)) != -1) {
	switch (option) {
	    case 'd':
		dither = !dither;
		break;
	    case 'w':
		if (!bu_opt_scan_size_t_range(bu_optarg, &width, 1, SIZE_MAX, "input width"))
		    usage();
		break;
	    case 'n':
		if (!bu_opt_scan_size_t_range(bu_optarg, &height, 1, SIZE_MAX, "input height"))
		    usage();
		break;
	    case 's':
		if (!bu_opt_scan_size_t_range(bu_optarg, &width, 1, SIZE_MAX, "input size"))
		    usage();
		height = width;
		break;
	    default:
		usage();
		break;
	}
    }

    if (bu_optind < argc)
	usage();

    struct icv_context *context = icv_context_create(NULL);
    image = icv_read_with_context(context, NULL, "image/vnd.brlcad.pix", NULL, width, height, NULL);
    icv_context_destroy(context);
    if (!image)
	bu_exit(1, "%s: unable to read %zu x %zu PIX input\n", progname, width, height);
    if (icv_write_sun(image, NULL, dither) != BRLCAD_OK) {
	icv_destroy(image);
	bu_exit(1, "%s: unable to write Sun rasterfile output\n", progname);
    }
    icv_destroy(image);
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
