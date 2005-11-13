/*                       I M G D I M S . C
 * BRL-CAD
 *
 * Copyright (C) 1997-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file imgdims.c
 *
 *			Guess the dimensions of an image
 *
 *  Author -
 *	Paul J. Tanenbaum
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"


#define	BELIEVE_NAME	0
#define	BELIEVE_STAT	1
#define	DFLT_PIXEL_SIZE	3

static char usage[] = "\
Usage: 'imgdims [-ns] [-# bytes/pixel] file_name'\n\
    or 'imgdims [-# bytes/pixel] num_bytes'\n";
#define OPT_STRING	"ns#:?"

static void print_usage (void)
{
    (void) bu_log("%s", usage);
}

static int grab_number (char *buf, int *np)
{
    char	*bp;

    for (bp = buf; *bp != '\0'; ++bp)
	if (!isdigit(*bp))
	    return (0);
    if (sscanf(buf, "%d", np) != 1)
    {
	bu_log("imgdims: grab_number(%s) failed.  This shouldn't happen\n",
	    buf);
	exit (1);
    }
    return (1);
}

static int pixel_size (char *buf)
{
    char		*ep;
    struct assoc
    {
	char	*ext;
	int	size;
    }			*ap;
    static struct assoc	a_tbl[] =
			{
			    {"bw", 1},
			    {"pix", 3},
			    {0, 0}
			};

    if ((ep = strrchr(buf, '.')) == NULL)
	return (DFLT_PIXEL_SIZE);
    else
	++ep;

    for (ap = a_tbl; ap -> ext; ++ap)
	if (strcmp(ep, ap -> ext) == 0)
	    return (ap -> size);

    return (DFLT_PIXEL_SIZE);
}

int
main (int argc, char **argv)
{
    char	*argument;		/* file name or size */
    int		bytes_per_pixel = -1;
    int		ch;
    int		how = BELIEVE_NAME;
    int		nm_bytes = -1;
    int		nm_pixels;
    unsigned long int		width;
    unsigned long int		height;
    struct stat	stat_buf;

    extern int	bu_optind;			/* index from bu_getopt(3C) */
    extern char	*bu_optarg;		/* argument from bu_getopt(3C) */

    /*
     *	Process the command line
     */
    while ((ch = bu_getopt(argc, argv, OPT_STRING)) != EOF)
	switch (ch)
	{
	    case 'n':
		how = BELIEVE_NAME;
		break;
	    case 's':
		how = BELIEVE_STAT;
		break;
	    case '#':
		if (sscanf(bu_optarg, "%d", &bytes_per_pixel) != 1)
		{
		    bu_log("Invalid pixel-size value: '%s'\n", bu_optarg);
		    print_usage();
		    exit (1);
		}
		break;
	    case '?':
	    default:
		print_usage();
		exit (ch != '?');
	}
    if (argc - bu_optind != 1)
    {
	print_usage();
	exit (1);
    }

    argument = argv[bu_optind];
    if ((stat(argument, &stat_buf) != 0)
     && (!grab_number(argument, &nm_bytes)))
    {
	bu_log("Cannot find file '%s'\n", argument);
	print_usage();
	exit (1);
    }

    /*
     *	If the user specified a file,
     *	determine its size
     */
    if (nm_bytes == -1) {
	if ((how == BELIEVE_NAME)
	 && bn_common_name_size(&width, &height, argument))
	    goto done;
	else
	{
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
    {
	bu_log("Image size (%d bytes) is not a multiple of pixel size (%d bytes)\n", nm_bytes, bytes_per_pixel);
	exit (1);
    }

    if (!bn_common_image_size(&width, &height, nm_pixels))
	exit (0);

done:
    printf("%lu %lu\n", width, height);
    exit (0);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
