/*
 *		    T E X T U R E S C A L E . C
 *
 *	Scale a PIX(5) stream to map onto a curved solid
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

#define	SPHERE		0
#define	TORUS		1

static char		*file_name;
static FILE		*infp;

static int		fileinput = 0;	/* Is input a file (not stdin)? */
static int		autosize = 0;	/* Try to guess input dimensions? */
static int		tol_using_rgb = 1; /* Compare via RGB, not HSV? */

static int		file_width = 512;
static int		file_height = 512;

static int		solid_type = SPHERE;
static fastf_t		r1, r2;		/* radii */

#define	COLORS_NEITHER	0
#define	COLORS_INTERIOR	1
#define	COLORS_EXTERIOR	2
#define	COLORS_BOTH	(COLORS_INTERIOR | COLORS_EXTERIOR)
static int		colors_specified = COLORS_NEITHER;

static unsigned char	border_rgb[3];
static unsigned char	exterior_rgb[3];
static unsigned char	interior_rgb[3];
static unsigned char	rgb_tol[3];

fastf_t			border_hsv[3];
fastf_t			exterior_hsv[3];
fastf_t			interior_hsv[3];
fastf_t			hsv_tol[3];

#define	OPT_STRING	"ahn:s:w:ST:?"

#define	made_it()	(void) fprintf(stderr, "Made it to %s:%d\n",	\
				__FILE__, __LINE__);			\
				fflush(stderr)
static char usage[] = "\
Usage: texturescale [-T 'r1 r2' | -S]\n\
                 [-ah] [-s squaresize] [-w file_width] [-n file_height]\n\
                 [file.pix]\n";

/*
 *		    R E A D _ R G B ( )
 *
 *	Read in an RGB triple as ints and then (implicitly)
 *	cast them as unsigned chars.
 */
static int read_rgb (rgbp, buf)

unsigned char	*rgbp;
char		*buf;

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
 *		    R E A D _ R A D I I ( )
 *
 *	Read in the radii for a torus
 */
static int read_radii (r1p, r2p, buf)

fastf_t *r1p;
fastf_t *r2p;
char	*buf;

{
    double	tmp[2];
    int		i;

    if (sscanf(buf, "%lf %lf", tmp, tmp + 1) != 2)
	return (0);
    if ((tmp[0] <= 0.0) || (tmp[1] <= 0.0))
	    return (0);
    *r1p = tmp[0];
    *r2p = tmp[1];
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
 *		    S A M E _ R G B ( )
 */
static int same_rgb (color1, color2)

unsigned char	*color1;
unsigned char	*color2;

{
    return ((abs(color1[RED] - color2[RED]) <= (int) rgb_tol[RED]) &&
	    (abs(color1[GRN] - color2[GRN]) <= (int) rgb_tol[GRN]) &&
	    (abs(color1[BLU] - color2[BLU]) <= (int) rgb_tol[BLU]));
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
	    case 'h':
		file_height = file_width = 1024;
		autosize = 0;
		break;
	    case 'n':
		file_height = atoi(optarg);
		autosize = 0;
		break;
	    case 's':
		file_height = file_width = atoi(optarg);
		autosize = 0;
		break;
	    case 'w':
		file_width = atoi(optarg);
		autosize = 0;
		break;
	    case 'S':
		solid_type = SPHERE;
		break;
	    case 'T':
		if (! read_radii(&r1, &r2, optarg))
		{
		    (void) fprintf(stderr,
			"Illegal torus radii: '%s'\n", optarg);
		    return (0);
		}
		solid_type = TORUS;
		break;
	    case '?':
		(void) fputs(usage, stderr);
		exit (0);
	    default:
		return (0);
	}
    }

    if (optind >= argc)
    {
	if(isatty(fileno(stdin)))
	{
	    (void) fprintf(stderr, "texturescale: cannot read from tty\n");
	    return(0);
	}
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
	(void) fprintf(stderr, "texturescale: excess argument(s) ignored\n");

    return (1);
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

    if (!get_args( argc, argv ))
    {
	(void) fputs(usage, stderr);
	exit (1);
    }

#if 1
    if (solid_type == TORUS)
	(void) fprintf(stderr,
	    "We'll produce a torus with r1=%g and r2=%g\n", r1, r2);
    else if (solid_type == SPHERE)
	(void) fprintf(stderr, "We'll produce a sphere\n");
    else
	(void) fprintf(stderr, "Funny type %d\n", solid_type);
}
#else

    /*
     *	Autosize the input if appropriate
     */
    if (fileinput && autosize)
    {
	int	w, h;

	if (bn_common_file_size(&w, &h, file_name, 3))
	{
	    file_width = w;
	    file_height = h;
	}
	else
	    (void) fprintf(stderr, "texturescale: unable to autosize\n");
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
	(void) fprintf(stderr, "texturescale:  fread() error\n");
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
	if ((row_nm < bottom_edge) || (row_nm > top_edge))
	{
	    if (fwrite(inrow[this_row] + 3, 3, file_width, stdout)
		!= file_width)
	    {
		perror("stdout");
		exit(2);
	    }
	}
	else
	{
	    for (col_nm = 0; col_nm < file_width; ++col_nm)
	    {
		unsigned char	*color_ptr;

		if ((col_nm >= left_edge) && (col_nm <= right_edge)
		 && is_border(inrow[prev_row], inrow[this_row],
			    inrow[next_row], col_nm))
		    color_ptr = border_rgb;
		else
		    color_ptr = inrow[this_row] + (col_nm + 1) * 3;

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
		(void) fprintf(stderr, "texturescale:  fread() error\n");
		exit(1);
	    }
	}
	else
	    for (i = 0; i < 3 * (file_width + 2); ++i)
		*(inrow[next_row] + i) = 0;
    }

    exit (1);
}
#endif
