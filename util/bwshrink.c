/*
 *			P I X S H R I N K . C
 *
 *	scale down a picture by a uniform factor.
 *
 *	Options
 *	h	help
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"

/* declarations to support use of getopt() system call */
char *options = "uhs:w:n:f:";

char *progname = "(noname)";
char *filename = "(stdin)";

/*	S H R I N K _ I M A G E
 *
 *	
 */
void
shrink_image(w, h, buffer, Factor)
unsigned char *buffer;
int Factor, w, h;
{
	unsigned char *finalpixel;	/* output pixel pointer */
	unsigned int p;		/* pixel sum/average */
	int facsq, x, y, px, py;

	facsq = Factor * Factor;
	finalpixel = buffer;

	for (y=0 ; y < h ; y += Factor)
		for (x=0 ; x < w ; x += Factor) {

			/* average factor by factor grid of pixels */

			p = 0;
			for (py = 0 ; py < Factor ; py++) {

				/* add pixels from scanline to average */
				for (px = 0 ; px < Factor ; px++) {
					p += buffer[y*w + x+px];
				}
			}

			/* store resultant pixel back in buffer */
			*finalpixel++ = p / facsq;
		}
}

/*
 *	Undersample image pixels
 */
void
usample_image(w, h, buffer, Factor)
unsigned char *buffer;
int Factor, w, h;
{
	register int x, y;
	register unsigned char *p;

	p = buffer;

	for (y=0 ; y < h ; y += Factor)
		for (x=0 ; x < w ; x += Factor, p++) {
			p[0] = buffer[x + y * w];
		}
}


int width = 512;
int height = 512;
int factor = 2;

#define METH_BOXCAR 1
#define METH_UNDERSAMPLE 2
int method = METH_BOXCAR;

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage()
{
	(void) fprintf(stderr,
"Usage: %s [-u] [-h] [-w width] [-n scanlines] [-s squaresize]\n\
[-f shrink_factor] [pixfile] > pixfile\n", progname);
	exit(1);
}


/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
void parse_args(ac, av)
int ac;
char *av[];
{
	int  c;

	if (!(progname = strrchr(*av, '/')))
		progname = *av;
	
	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF)
		switch (c) {
		case 'f'	: if ((c = atoi(optarg)) > 1)
					factor = c;
				  break;
		case 'h'	: width = height = 1024; break;
		case 'n'	: if ((c=atoi(optarg)) > 0)
					height = c;
				  break;
		case 'w'	: if ((c=atoi(optarg)) > 0)
					width = c;
				  break;
		case 's'	: if ((c=atoi(optarg)) > 0)
					height = width = c;
				  break;
		case 'u'	: method = METH_UNDERSAMPLE; break; 
		case '?'	:
		default		: usage(); break;
		}

	if (optind >= ac) {
		if (isatty(fileno(stdout)) )
			usage();
	}
	if (optind < ac) {
		if (freopen(av[optind], "r", stdin) == (FILE *)NULL) {
			perror(av[optind]);
			exit(-1);
		} else
			filename = av[optind];
	}
	if (optind+1 < ac)
		(void)fprintf(stderr, "%s: Excess arguments ignored\n", progname);

}




/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(ac,av)
int ac;
char *av[];
{
	unsigned char *buffer = (unsigned char *)NULL;
	int	size;
	int	c = 0;
	register int t;

	(void)parse_args(ac, av);
	if (isatty(fileno(stdin))) usage();

	/* process stdin */

	/* get buffer for image */
	size = width * height;
	if ((buffer = (unsigned char *)malloc(width*height)) == (unsigned char *)NULL) {
		(void)fprintf(stderr, "%s: cannot allocate input buffer\n", 
			progname);
		exit(-1);
	}

	/* read in entire image */
	for (t=0 ; t < size && (c=read(0, (char *)&buffer[t], size-t)) >= 0 ; 
		t += c);

	if (c < 0) {
		perror (filename);
		return(-1);
	}

	switch (method) {
	case METH_BOXCAR : shrink_image(width, height, buffer, factor); break;
	case METH_UNDERSAMPLE : usample_image(width, height, buffer, factor);
				break;
	default: return(-1);
	}

	for (t=0 ; t < size && (c=write(1, (char *)&buffer[t], size-t)) >= 0 ;
		t += c);

	if (c < 0) {
		perror("stdout");
		return(-1);
	}

	return(0);
}


