/*
 *				I M G D I M S
 *
 *			Guess the dimensions of an image
 *
 *  Author -
 *	Paul J. Tanenbaum
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1997 by the United States Army.
 *	All rights reserved.
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include "machine.h"
#include "externs.h"		/* For getopt(), etc. */
#include "bu.h"

#define	BELIEVE_NAME	0
#define	BELIEVE_STAT	1
#define	DFLT_PIXEL_SIZE	3

static char usage[] = "\
Usage: 'imgdims [-ns] [-# bytes/pixel] file_name'\n\
    or 'imgdims [-# bytes/pixel] num_bytes'\n";
#define OPT_STRING	"ns#:?"

static void print_usage ()
{
    (void) bu_log("%s", usage);
}

static int grab_number (buf, np)

char	*buf;
int	*np;

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

static int pixel_size (buf)

char	*buf;

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

main (argc, argv)

int argc;
char *argv[];

{
    char	*argument;		/* file name or size */
    int		bytes_per_pixel = -1;
    int		ch;
    int		height;
    int		how = BELIEVE_NAME;
    int		nm_bytes = -1;
    int		nm_pixels;
    int		width;
    struct stat	stat_buf;

    extern int	optind;			/* index from getopt(3C) */
    extern char	*optarg;		/* argument from getopt(3C) */

    /*
     *	Process the command line
     */
    while ((ch = getopt(argc, argv, OPT_STRING)) != EOF)
	switch (ch)
	{
	    case 'n':
		how = BELIEVE_NAME;
		break;
	    case 's':
		how = BELIEVE_STAT;
		break;
	    case '#':
		if (sscanf(optarg, "%d", &bytes_per_pixel) != 1)
		{
		    bu_log("Invalid pixel-size value: '%s'\n", optarg);
		    print_usage();
		    exit (1);
		}
		break;
	    case '?':
	    default:
		print_usage();
		exit (ch != '?');
	}
    if (argc - optind != 1)
    {
	print_usage();
	exit (1);
    }

    argument = argv[optind];
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
    if (nm_bytes == -1)
	if ((how == BELIEVE_NAME)
	 && bn_common_name_size(&width, &height, argument))
	    goto done;
	else
	{
	    nm_bytes = stat_buf.st_size;
	    if (bytes_per_pixel == -1)
		bytes_per_pixel = pixel_size(argument);
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
    printf("%d %d\n", width, height);
    exit (0);
}
