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
 *	This software is Copyright (C) 1997 by the United States Army
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

static int		file_width = 512;
static int		file_height = 512;

static int		solid_type = SPHERE;
static fastf_t		r1, r2;		/* radii */

#define	OPT_STRING	"ahn:s:w:ST:?"

#define	made_it()	(void) fprintf(stderr, "Made it to %s:%d\n",	\
				__FILE__, __LINE__);			\
				fflush(stderr)
static char usage[] = "\
Usage: texturescale [-T 'r1 r2' | -S]\n\
                 [-ah] [-s squaresize] [-w file_width] [-n file_height]\n\
                 [file.pix]\n";

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
    char		*inbuf;		/* The input scanline */
    char		*outbuf;	/*  "  output    "    */
    char		*in, *out;	/* Pointers into inbuf and outbuf */
    fastf_t		twice_r1r2;
    fastf_t		squares;
    fastf_t		scale_fac;
    fastf_t		theta;
    fastf_t		x;		/* Scale factor for pixel blending */
    int			i;		/* Pixel index in inbuf */
    int			j;		/*   "     "    " outbuf */
    int			row;
    int			row_width;

    if (!get_args( argc, argv ))
    {
	(void) fputs(usage, stderr);
	exit (1);
    }

    if (solid_type == SPHERE)
    {
	(void) fprintf(stderr, "Sphere scaling not yet implemented\n");
	exit (1);
    }
    else if (solid_type != TORUS)
    {
	(void) fprintf(stderr, "Illegal solid type %d\n", solid_type);
	exit (0);
    }

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
     *	Initialize some runtime constants
     */
    twice_r1r2 = 2 * r1 * r2;
    squares = r1 * r1 + r2 * r2;
    scale_fac = file_width / (r1 + r2);

    /*
     *	Allocate 1-scanline buffers for input and output
     */
    outbuf = malloc(3*file_width);
    inbuf  = malloc(3*file_width);

    /*
     *	Do the filtering
     */
    for (row = 0; row < file_height; ++row)
    {
	/*
	 *	Read an input scanline
	 */
	if (! read_row(inbuf, file_width, infp))
	{
	    perror(file_name);
	    (void) fprintf(stderr, "texturescale:  fread() error\n");
	    exit(1);
	}

	/*
	 *	Determine how much of the input scanline we want
	 */
	theta = 2 * M_PI * row / file_height;
	row_width = scale_fac * sqrt(squares - twice_r1r2 * cos(theta));
	in = inbuf + ((file_width - row_width) / 2) * 3;
	out = outbuf;

	/*
	 *	Scale the input scanline into the output scanline
	 */
	for (i = j = 1; j <= file_width; ++j)
	{
	    if (i * file_width < j * row_width)
	    {
		x = j - (i * file_width) / row_width;
		VBLEND2(out, (1.0 - x), in, x, in + 3);
		++i;
		in += 3;
	    }
	    else
		VMOVE(out, in);
	    out += 3;
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

    exit (1);
}
