/*
 *			P I X F A D E . C
 *
 *  Fade a pixture
 *
 * pixfade will darken a pix by a certen percentage or do an integer
 * max pixel value.  It runs in two modes, truncate which will cut any
 * channel greater than param to param, and scale which will change
 * a channel to param percent of its orignal value (limited by 0-255)
 *
 *  entry:
 *	-m	integer max value
 *	-f	fraction to fade
 *	-p	percentage of fade (fraction = percentage/100)
 *	file	a pixture file.
 *	<stdin>	a pixture file if file is not given.
 *
 *  Exit:
 *	<stdout>	the faded pixture.
 *
 *  Calls:
 *	get_args
 *
 *  Method:
 *	straight-forward.
 *
 *  Author:
 *	Christopher T. Johnson - 88/12/27
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "externs.h"		/* For getopt */
#include "bu.h"
#include "bn.h"

int	max = 255;
double	multiplier = 1.0;
FILE	*inp;

static char usage[] = "\
Usage: pixfade [-m max] [-p percent] [-f fraction] [pix-file]\n";

int
get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "m:p:f:" )) != EOF )  {
		switch( c )  {
		case 'm':
			max = atoi(optarg);
			if ((max < 0) || (max > 255)) {
				fprintf(stderr,"pixfade: max out of range");
				exit(1);
			}
			break;
		case 'p':
			multiplier = atof(optarg) / 100.0;
			if (multiplier < 0.0) {
				fprintf(stderr,"pixfade: percent is negitive");
				exit(1);
			}
			break;
		case 'f':
			multiplier = atof(optarg);
			if (multiplier < 0.0) {
				fprintf(stderr,"pixfade: fraction is negitive");
				exit(1);
			}
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  {
		if( isatty(fileno(stdin)) )  {
			fprintf(stderr,"pixfade: stdin is a tty\n");
			return(0);
		}
		inp = stdin;
	} else {
		if( (inp = fopen(argv[optind], "r")) == NULL )  {
			(void)fprintf( stderr,
				"pixfade: cannot open \"%s\" for reading\n",
				argv[optind] );
			return(0);
		}
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "pixfade: excess argument(s) ignored\n" );

	if( isatty(fileno(stdout)) )  {
		fprintf(stderr,"pixfade: stdout is a tty\n");
		return(0);
	}

	return(1);		/* OK */
}

int
main( argc, argv )
int argc;
char *argv[];
{
	register float	*randp;
	struct color_rec {
		unsigned char red,green,blue;
	} cur_color;

	bn_rand_init( randp, 0);

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

/* fprintf(stderr,"pixfade: max = %d, multiplier = %f\n",max,multiplier); */


	for(;;)  {
		register double	t;

		if( fread(&cur_color,1,3,inp) != 3 )  break;
		if( feof(inp) )  break;

		t = cur_color.red * multiplier + bn_rand_half(randp);
		if (t > max)
			cur_color.red   = max;
		else
			cur_color.red = t;

		t = cur_color.green * multiplier + bn_rand_half(randp);
		if (t > max)
			cur_color.green = max;
		else
			cur_color.green = t;

		t = cur_color.blue * multiplier + bn_rand_half(randp);
		if (t > max)
			cur_color.blue  = max;
		else
			cur_color.blue = t;

		fwrite(&cur_color,1,3,stdout);
	}
	return 0;
}
