/*                    F B G A M M A M O D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2009 United States Government as represented by
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
 *
 */
/** @file fbgammamod.c
 *
 * Program to rapidly compute per-color gamma ramps and linear
 * corrections.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "fb.h"
#include "pkg.h"

char *options = "vf:F:";

static char usage[] = "\
Usage: fbgammamod [-v] [-f in_file] [-F framebuffer] \
	r+ r* r_gam g+ g* g_gam b+ b* b_gam global_pre_gam global+ global* global_post_gam\n";

int	verbose = 0;
char	*framebuffer = (char *)NULL;
char	*input_file = NULL;

ColorMap map;

double	ra, rm, rg;		/* addition, multiply, gamma */
double	ga, gm, gg;
double	ba, bm, bg;
double	add, mul, pre_gam, post_gam;	/* globals */

/*
 *			D O _ F I L E
 */
void
do_file(void)
{
    char	*output_file;
    FILE	*ifp, *ofp;
    int	i;

    if ( (ifp = fopen( input_file, "rb" )) == NULL )  {
	perror(input_file);
	bu_exit(1, NULL);
    }
    output_file = (char *)bu_malloc( strlen(input_file)+10, "output_file" );
    snprintf(output_file, strlen(input_file)+9, "MOD_%s", input_file);

    if ( (ofp = fopen( output_file, "wb" )) == NULL )  {
	perror(output_file);
	bu_exit(2, NULL);
    }
    bu_free(output_file, "output_file");

    /* Shift cmap to be more useful */
    for ( i=0; i<256; i++ )  {
	map.cm_red[i] >>= 8;
	map.cm_green[i] >>= 8;
	map.cm_blue[i] >>= 8;
    }

    while ( !feof(ifp) )  {
	i = map.cm_red[getc(ifp)];
	putc( i, ofp );

	i = map.cm_green[getc(ifp)];
	putc( i, ofp );

	i = map.cm_blue[getc(ifp)];
	putc( i, ofp );
    }
}

/*
 *			D O _ F B
 */
void
do_fb(void)
{
    FBIO	*fbp;

    if ( (fbp = fb_open( framebuffer, 0, 0 )) == FBIO_NULL ) {
	bu_exit( 2, "Unable to open framebuffer\n" );
    }
    if ( fb_wmap( fbp, &map ) < 0 )
	fprintf( stderr, "fbgammamod: unable to write color map\n");
    fb_close(fbp);
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
    double	rexp, gexp, bexp;
    double	radd, gadd, badd;
    double	rmul, gmul, bmul;
    double	pre_exp;
    int	i;

    /* check for flags */
    bu_opterr = 0;
    while ((i=bu_getopt(argc, argv, options)) != EOF) {
	switch (i) {
	    case 'v':
		verbose++;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 'f':
		input_file = bu_optarg;
		break;
	    default:
		bu_exit(2, "fbgammamod: Unrecognized option '%c'\n%s",
			i, usage);
	}
    }

    if ( bu_optind != argc - 13 )  {
	bu_exit(1, "%s", usage );
    }

    /* Gobble 13 positional args */
    ra = atof( argv[bu_optind+0] );
    rm = atof( argv[bu_optind+1] );
    rg = atof( argv[bu_optind+2] );

    ga = atof( argv[bu_optind+3] );
    gm = atof( argv[bu_optind+4] );
    gg = atof( argv[bu_optind+5] );

    ba = atof( argv[bu_optind+6] );
    bm = atof( argv[bu_optind+7] );
    bg = atof( argv[bu_optind+8] );

    pre_gam = atof( argv[bu_optind+9] );
    add = atof( argv[bu_optind+10] );
    mul = atof( argv[bu_optind+11] );
    post_gam = atof( argv[bu_optind+12] );

    if ( verbose )  {
	fprintf(stderr, "r+ = %g, r* = %g, r gam=%g\n", ra, rm, rg);
	fprintf(stderr, "g+ = %g, g* = %g, g gam=%g\n", ga, gm, gg);
	fprintf(stderr, "b+ = %g, b* = %g, b gam=%g\n", ba, bm, bg);
	fprintf(stderr, "pre_gam = %g, + = %g, * = %g, post_gam = %g\n",
		pre_gam, add, mul, post_gam );
    }

    /* Build the color map, per specifications */
    pre_exp = 1.0 / pre_gam;
    rexp = 1.0 / ( post_gam + rg - 1 );
    gexp = 1.0 / ( post_gam + gg - 1 );
    bexp = 1.0 / ( post_gam + bg - 1 );

    radd = (ra + add) / 255.;
    gadd = (ga + add) / 255.;
    badd = (ba + add) / 255.;

    rmul = rm * mul;
    gmul = gm * mul;
    bmul = bm * mul;

    for ( i=0; i<256; i++ )  {
	double	t;
	int	val;

	if ( (t = (pow( i/255.0, pre_exp) + radd) * rmul) < 0 )
	    t = 0;
	else if ( t > 1 )
	    t = 1;
	if ( (val = (int)(65535 * pow( t, rexp ))) < 0 )
	    val = 0;
	else if ( val > 65535 )
	    val = 65535;
	map.cm_red[i] = val;

	if ( (t = (pow( i/255.0, pre_exp) + gadd) * gmul) < 0 )
	    t = 0;
	else if ( t > 1 )
	    t = 1;
	if ( (val = (int)(65535 * pow( t, gexp ))) < 0 )
	    val = 0;
	else if ( val > 65535 )
	    val = 65535;
	map.cm_green[i] = val;

	if ( (t = (pow( i/255.0, pre_exp) + badd) * bmul) < 0 )
	    t = 0;
	else if ( t > 1 )
	    t = 1;
	if ( (val = (int)(65535 * pow( t, bexp ))) < 0 )
	    val = 0;
	else if ( val > 65535 )
	    val = 65535;
	map.cm_blue[i] = val;

	/* use cmap-fb format */
	if ( verbose )
	    fprintf(stderr, "%d	%4x %4x %4x\n", i,
		    map.cm_red[i], map.cm_green[i], map.cm_blue[i] );
    }

    if (!input_file) {
	do_fb();
    } else {
	do_file();
    }

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
