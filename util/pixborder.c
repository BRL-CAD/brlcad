/*
 *			P I X B O R D E R . C
 *
 *	Add a 1-pixel-wide border to regions of a specified color.
 *
 *  Author -
 *	Paul J. Tanenbaum
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1996 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "externs.h"		/* For getopt, etc */
#include "vmath.h"
#include "fb.h"

static char		*file_name;
static FILE		*infp;

static int		fileinput = 0;	/* Is input a file (not stdin)? */
static int		autosize = 0;	/* Try to guess input dimensions? */

static int		file_width = 512;
static int		file_height = 512;

static unsigned char	border_color[3];
static unsigned char	region_color[3];
static unsigned char	color_tol[3];

#define	OPT_STRING	"ab:hi:s:t:w:n:?"

#define	made_it()	fprintf(stderr, "Made it to %s:%d\n",	\
				__FILE__, __LINE__);		\
				fflush(stderr)
static char usage[] = "\
Usage: pixborder [-b 'R G B'] [-i 'R G B'] [-t 'R G B']\n\
	[-ah] [-s squaresize] [-w file_width] [-n file_height] [file.pix]\n";

/*
 *		    R E A D _ R G B ( )
 *
 *	Read in an R/G/B triple as ints and then (implicitly)
 *	cast them as unsigned chars.
 */
static int read_rgb (rgbp, buf)

unsigned char *rgbp;
char *buf;

{
    int		tmp[3];
    int		i;

    if (sscanf(buf, "%d %d %d", tmp, tmp + 1, tmp + 2) != 3)
	return (0);
    for (i = 0; i < 3; ++i)
	if ((tmp[i] < 0) || (tmp[i] > 255))
	    return (0);
    VMOVE(rgbp, tmp);
    return (1);
}

/*
 *		    G E T _ A R G S ( )
 */
static get_args (argc, argv)

int		argc;
register char **argv;

{
    register int c;

    while ((c = getopt( argc, argv, OPT_STRING)) != EOF)
    {
	switch (c)
	{
	    case 'a':
		autosize = 1;
		break;
	    case 'b':
		if (! read_rgb(border_color, optarg))
		{
		    fprintf(stderr, "Illegal color: '%s'\n", optarg);
		    return (0);
		}
		break;
	    case 'h':
		file_height = file_width = 1024;
		autosize = 0;
		break;
	    case 'i':
		if (! read_rgb(region_color, optarg))
		{
		    fprintf(stderr, "Illegal color: '%s'\n", optarg);
		    return (0);
		}
		break;
	    case 'n':
		file_height = atoi(optarg);
		autosize = 0;
		break;
	    case 's':
		file_height = file_width = atoi(optarg);
		autosize = 0;
		break;
	    case 't':
		if (! read_rgb(color_tol, optarg))
		{
		    fprintf(stderr, "Illegal color: '%s'\n", optarg);
		    return (0);
		}
		break;
	    case 'w':
		file_width = atoi(optarg);
		autosize = 0;
		break;
	    case '?':
	    default:
		return (c == '?');
	}
    }

    if (optind >= argc)
    {
	if(isatty(fileno(stdin)))
	    return(0);
	file_name = "stdin";
	infp = stdin;
    }
    else
    {
	file_name = argv[optind];
	if ((infp = fopen(file_name, "r")) == NULL)
	{
	    perror(file_name);
	    (void) fprintf(stderr, "Cannot open file '%s'\n", file_name);
	    return (0);
	}
	++fileinput;
    }

    if (argc > ++optind)
	(void) fprintf(stderr, "pixborder: excess argument(s) ignored\n");

    return (1);
}

/*
 *		    R E A D _ R O W ( )
 */
static int read_row (rp, file_width, infp)

unsigned char	*rp;
int		file_width;
FILE		*infp;

{
    if (fread(rp + 3, 3, file_width, infp) != file_width)
	return (0);
    *(rp + RED) = *(rp + GRN) = *(rp + BLU) = 0;
    *(rp + 3 * (file_width + 1) + RED) =
    *(rp + 3 * (file_width + 1) + GRN) =
    *(rp + 3 * (file_width + 1) + BLU) = 0;
    return (1);
}

/*
 *		    S A M E _ C O L O R ( )
 */
same_color (color1, color2)

unsigned char	*color1;
unsigned char	*color2;

{
    return ((abs(*(color1 + RED) - *(color2 + RED)) <= (int) color_tol[RED]) &&
	    (abs(*(color1 + GRN) - *(color2 + GRN)) <= (int) color_tol[GRN]) &&
	    (abs(*(color1 + BLU) - *(color2 + BLU)) <= (int) color_tol[BLU]));
}

/*
 *		    I S _ B O R D E R ( )
 */
static int is_border (prp, trp, nrp, col_nm, interior_color)

unsigned char	*prp;	/* Previous row */
unsigned char	*trp;	/* Current (this) row */
unsigned char	*nrp;	/* Next row */
int		col_nm;
unsigned char	*interior_color;

{
    unsigned char	pixel[3];

    VMOVE(pixel, trp + (col_nm + 1) * 3);

    /*
     *	Ensure that this pixel is in a region of interest
     */
    if (! same_color(pixel, interior_color))
	return (0);
    
    /*
     *	Check its left and right neighbors
     */
     VMOVE(pixel, trp + (col_nm + 0) * 3);
     if (! same_color(pixel, interior_color))
	return (1);
     VMOVE(pixel, trp + (col_nm + 2) * 3);
     if (! same_color(pixel, interior_color))
	return (1);
    
    /*
     *	Check its upper and lower neighbors
     */
     VMOVE(pixel, prp + (col_nm + 1) * 3);
     if (! same_color(pixel, interior_color))
	return (1);
     VMOVE(pixel, nrp + (col_nm + 1) * 3);
     if (! same_color(pixel, interior_color))
	return (1);
    
    /*
     *	All four of its neighbors are also in the region
     */
     return (0);
}

/*
 *			M A I N ( )
 */
main (argc, argv)

int	argc;
char	*argv[];

{
    char		*outbuf;
    unsigned char	*inrow[3];
    int			col_nm;
    int			i;
    int			next_row;
    int			prev_row;
    int			row_nm;
    int			this_row;

    border_color[RED] = border_color[GRN] = border_color[BLU] = 1;
    region_color[RED] = region_color[GRN] = region_color[BLU] = 255;
    color_tol[RED] = color_tol[GRN] = color_tol[BLU] = 0;

    if (!get_args( argc, argv ))
    {
	(void) fputs(usage, stderr);
	exit (1);
    }

    fprintf(stderr, "OK, file '%s' has size %d by %d\n",
	file_name, file_width, file_height);
    fprintf(stderr,
	"We'll put a border of %d/%d/%d around regions of %d/%d/%d\n",
	border_color[RED], border_color[GRN], border_color[BLU],
	region_color[RED], region_color[GRN], region_color[BLU]);

    /*
     *	Autosize the input if appropriate
     */
    if (fileinput && autosize)
    {
	int	w, h;

	if (fb_common_file_size(&w, &h, file_name, 3))
	{
	    file_width = w;
	    file_height = h;
	}
	else
	    fprintf(stderr, "pixhalve: unable to autosize\n");
    }

    /*
     *	Allocate a 1-scanline output buffer
     *	and a circular input buffer of 3 scanlines
     */
    outbuf = malloc(3*file_width);
    for (i = 0; i < 3; ++i)
	inrow[i] = malloc(3*(file_width + 2));
    prev_row = 0;
    this_row = 1;
    next_row = 2;

    /*
     *	Initialize previous-row buffer
     */
    for (i = 0; i < 3 * (file_width + 2); ++i)
	*(inrow[prev_row] + i) = 0;

    /*
     *	Initialize current- and next-row buffers
     */
    if ((! read_row(inrow[this_row], file_width, infp))
     || (! read_row(inrow[next_row], file_width, infp)))
    {
	perror(file_name);
	fprintf(stderr, "pixborder:  fread() error\n");
	exit(1);
    }

    /*
     *		Do the filtering
     */
    for (row_nm = 0; row_nm < file_height; ++row_nm)
    {
	/*
	 *	Fill the output-scanline buffer
	 */
	for (col_nm = 0; col_nm < file_width; ++col_nm)
	{
	    unsigned char	*color_ptr;

	    color_ptr = is_border(inrow[prev_row], inrow[this_row],
				    inrow[next_row], col_nm, region_color)
		    ? border_color
		    : inrow[this_row] + col_nm * 3;

	    VMOVE(outbuf + col_nm * 3, color_ptr);
	}

	/*
	 *	Write the output scanline
	 */
	if (fwrite(outbuf, 3, file_width, stdout) != file_width)
	{
	    perror("stdout");
	    exit(2);
	}

	/*
	 *	Advance the circular input buffer
	 */
	prev_row = this_row;
	this_row = next_row;
	next_row = ++next_row % 3;

	/*
	 *	Grab the next input scanline
	 */
	if (row_nm < file_height - 2)
	{
	    if (! read_row(inrow[next_row], file_width, infp))
	    {
		perror(file_name);
		fprintf(stderr, "pixborder:  fread() error\n");
		exit(1);
	    }
	}
	else
	    for (i = 0; i < 3 * (file_width + 2); ++i)
		*(inrow[next_row] + i) = 0;
    }

    exit (1);
}
