/*
 *			F B G A M M A M O D . C
 *
 *  Program to rapidly compute per-color gamma ramps and linear corrections.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif


#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "fb.h"

char *options = "F:";
/* externs.h includes externs for getopt and associated variables */

static char usage[] = "\
Usage: fbgammamod [-f in_file] [-F framebuffer] \
	r+ r* r_gam g+ g* g_gam b+ b* b_gam global+ global* global_gam\n";

char	*framebuffer = (char *)NULL;
char	*input_file = NULL;

FBIO	*fbp;

ColorMap map;

double	ra, rm, rg;		/* addition, multiply, gamma */
double	ga, gm, gg;
double	ba, bm, bg;
double	add, mul, gam;	/* globals */

/*
 *			M A I N
 */
main( argc, argv )
int	argc;
char	**argv;
{
	double	rexp, gexp, bexp;
	double	radd, gadd, badd;
	double	rmul, gmul, bmul;
	int	i;

	/* check for flags */
	opterr = 0;
	while ((i=getopt(argc, argv, options)) != EOF) {
		switch(i) {
		case 'F':
			framebuffer = optarg;
			break;
		case 'f':
			input_file = optarg;
			break;
		default:
			break;
		}
	}

	if( optind != argc - 12 )  {
		fprintf( stderr, usage );
		exit(1);
	}

	/* Gobble 12 positional args */
	ra = atof( argv[optind+0] );
	rm = atof( argv[optind+1] );
	rg = atof( argv[optind+2] );

	ga = atof( argv[optind+3] );
	gm = atof( argv[optind+4] );
	gg = atof( argv[optind+5] );

	ba = atof( argv[optind+6] );
	bm = atof( argv[optind+7] );
	bg = atof( argv[optind+8] );

	add = atof( argv[optind+9] );
	mul = atof( argv[optind+10] );
	gam = atof( argv[optind+11] );

	if( (fbp = fb_open( framebuffer, 0, 0 )) == FBIO_NULL ) {
		exit( 2 );
	}

	/* Build the color map, per specifications */
	rexp = 1.0 / ( gam + rg - 1 );
	gexp = 1.0 / ( gam + gg - 1 );
	bexp = 1.0 / ( gam + bg - 1 );

	radd = ra + add;
	gadd = ga + add;
	badd = ba + add;

	rmul = rm * mul;
	gmul = gm * mul;
	bmul = bm * mul;

	for( i=0; i<256; i++ )  {
		register double	t;
		register int	val;

		if( (t = (i + radd) * rmul) < 0 )
			t = 0;
		else if( t > 255 )
			t = 255;
		if( (val = (int)(65535 * pow( t / 255, rexp ))) < 0 )
			val = 0;
		else if( val > 65535 )
			val = 65535;
		map.cm_red[i] = val;

		if( (t = (i + gadd) * gmul) < 0 )
			t = 0;
		else if( t > 255 )
			t = 255;
		if( (val = (int)(65535 * pow( t / 255, gexp ))) < 0 )
			val = 0;
		else if( val > 65535 )
			val = 65535;
		map.cm_green[i] = val;

		if( (t = (i + badd) * bmul) < 0 )
			t = 0;
		else if( t > 255 )
			t = 255;
		if( (val = (int)(65535 * pow( t / 255, bexp ))) < 0 )
			val = 0;
		else if( val > 65535 )
			val = 65535;
		map.cm_blue[i] = val;
	}

	if( fb_wmap( fbp, &map ) < 0 )
		fprintf( stderr, "fbgammamod: unable to write color map\n");
	fb_close(fbp);
	exit(0);
}
