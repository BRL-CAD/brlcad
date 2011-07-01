/*                       I M G D I M S . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2011 United States Government as represented by
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
/** @file util/imgdims.c
 *
 * Guess the dimensions of an image
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "fb.h"


#define BELIEVE_NAME 0
#define BELIEVE_STAT 1
#define DFLT_PIXEL_SIZE 3

static char usage[] = "\
Usage: 'imgdims [-ns] [-# bytes/pixel] file_name'\n\
    or 'imgdims [-# bytes/pixel] num_bytes'\n";
#define OPT_STRING "ns#:?"


static void print_usage (void)
{
    bu_exit(1, "%s", usage);
}


static int grab_number (char *buf, int *np)
{
    char *bp;

    for (bp = buf; *bp != '\0'; ++bp)
	if (!isdigit(*bp))
	    return 0;
    if (sscanf(buf, "%d", np) != 1)
	bu_exit (1, "imgdims: grab_number(%s) failed.  This shouldn't happen\n", buf);
    return 1;
}


static int pixel_size (char *buf)
{
    char *ep;
    struct assoc {
	char *ext;
	int size;
    }			*ap;
    static struct assoc a_tbl[] = {
	{"bw", 1},
	{"pix", 3},
	{0, 0}
    };

    if ((ep = strrchr(buf, '.')) == NULL)
	return DFLT_PIXEL_SIZE;
    else
	++ep;

    for (ap = a_tbl; ap->ext; ++ap)
	if (BU_STR_EQUAL(ep, ap->ext))
	    return ap->size;

    return DFLT_PIXEL_SIZE;
}


int
main (int argc, char **argv)
{
    char *argument;		/* file name or size */
    int bytes_per_pixel = -1;
    int ch;
    int how = BELIEVE_NAME;
    int nm_bytes = -1;
    int nm_pixels = 0;
    size_t width;
    size_t height;
    struct stat stat_buf;

    /*
     * Process the command line
     */
    while ((ch = bu_getopt(argc, argv, OPT_STRING)) != -1)
	switch (ch) {
	    case 'n':
		how = BELIEVE_NAME;
		break;
	    case 's':
		how = BELIEVE_STAT;
		break;
	    case '#':
		if (sscanf(bu_optarg, "%d", &bytes_per_pixel) != 1) {
		    bu_log("Invalid pixel-size value: '%s'\n", bu_optarg);
		    print_usage();
		}
		break;
	    case '?':
	    default:
		print_usage();
	}
    if (argc - bu_optind != 1) {
	print_usage();
    }

    argument = argv[bu_optind];
    if ((stat(argument, &stat_buf) != 0)
	&& (!grab_number(argument, &nm_bytes)))
    {
	bu_log("Cannot find file '%s'\n", argument);
	print_usage();
    }

    /*
     * If the user specified a file,
     * determine its size
     */
    if (nm_bytes == -1) {
	if ((how == BELIEVE_NAME)
	    && fb_common_name_size(&width, &height, argument))
	    goto done;
	else {
	    nm_bytes = (int)stat_buf.st_size;
	    if (bytes_per_pixel == -1)
		bytes_per_pixel = pixel_size(argument);
	}
    }
    if (bytes_per_pixel == -1)
	bytes_per_pixel = DFLT_PIXEL_SIZE;

    if (nm_bytes % bytes_per_pixel == 0)
	nm_pixels = nm_bytes / bytes_per_pixel;
    else
	bu_exit (1, "Image size (%d bytes) is not a multiple of pixel size (%d bytes)\n", nm_bytes, bytes_per_pixel);

    if (!fb_common_image_size(&width, &height, nm_pixels))
	bu_exit (0, NULL);

 done:
    bu_log("%zu %zu\n", width, height);
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
