/*
 *			F B G A M M A . C
 *
 *  Load a gamma correcting colormap into a framebuffer.
 *
 *  Author -
 *	Phillip Dykstra
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "fb.h"

char *options = "ihoF:";
/* externs.h includes externs for getopt and associated variables */

void	checkgamma();

unsigned char rampval[10] = { 255, 128, 64, 32, 16, 8, 4, 2, 1, 0 };
int x, y, scr_width, scr_height, patch_width, patch_height;
unsigned char	*altline;
unsigned char	*line;
char *framebuffer = (char *)NULL;
int image = 0;

static char usage[] = "\
Usage: fbgamma [-h -o -i] [-F framebuffer] val [gval bval]\n";

void mk_ramp(fb, r, g, b, n)
FBIO *fb;
int r, g, b, n;
{

	/* grey ramp */
	for (x=0 ; x < scr_width ; ++x) {
		if (r) line[x*3] = rampval[x / patch_width + 1];
		else line[x*3] = 0;
		if (g) line[x*3+1] = rampval[x / patch_width + 1];
		else line[x*3+1] = 0;
		if (b) line[x*3+2] = rampval[x / patch_width + 1];
		else line[x*3+2] = 0;
	}
	for (y=patch_height*n ; y < patch_height*(n+1) && y < scr_height ; ++y) {
		fb_write(fb, 0, y, line, scr_width);
	}

	for (x=0 ; x < scr_width ; ++x) {
		if (r) line[x*3] = rampval[x / patch_width];
		else line[x*3] = 0;
		if (g) line[x*3+1] = rampval[x / patch_width];
		else line[x*3+1] = 0;
		if (b) line[x*3+2] = rampval[x / patch_width];
		else line[x*3+2] = 0;
	}
	for (y=patch_height*(n+1) ; y < patch_height*(n+2) && y < scr_height ; y += 2) {
		fb_write(fb, 0, y, altline, scr_width);
		fb_write(fb, 0, y+1, line, scr_width);
	}
}

void disp_image(fb)
FBIO *fb;
{

	scr_width = fb_getwidth(fb);
	scr_height = fb_getheight(fb);

	patch_width = scr_width / 8;
	patch_height = scr_height / 14;

	if ((line = (unsigned char *) malloc(scr_width*3)) == (unsigned char *)NULL) {
		exit(-1);
	}

	if ((altline = (unsigned char *) malloc(scr_width*3)) == (unsigned char *)NULL) {
		exit(-1);
	} else {
		bzero( (char *)altline, scr_width*3);
	}

	mk_ramp(fb, 1, 1, 1, 0);
	mk_ramp(fb, 1, 0, 0, 2);
	mk_ramp(fb, 1, 1, 0, 4);
	mk_ramp(fb, 0, 1, 0, 6);
	mk_ramp(fb, 0, 1, 1, 8);
	mk_ramp(fb, 0, 0, 1, 10);
	mk_ramp(fb, 1, 0, 1, 12);

	(void)free(line);
	(void)free(altline);
}


int
main( argc, argv )
int argc; char **argv;
{
	int	i;
	int	onegamma = 0;
	int	fbsize = 512;
	int	overlay = 0;
	double	gamr = 0, gamg = 0, gamb = 0;	/* gamma's */
	double	f;
	ColorMap cm;
	FBIO	*fbp;

	onegamma = 0;

	/* check for flags */
	opterr = 0;
	while ((i=getopt(argc, argv, options)) != EOF) {
		switch(i) {
		case 'h'	: fbsize = 1024; break;
		case 'o'	: overlay++; break;
		case 'i'	: image = !image; break;
		case 'F'	: framebuffer = optarg; break;
		default		: break;
		}
	}

	if (optind == argc - 1) {
		/* single value for all channels */
		f = atof( argv[optind] );
		checkgamma( f );
		gamr = gamg = gamb = 1.0 / f;
		onegamma++;
	} else if (optind == argc - 4 ) {
		/* different RGB values */
		f = atof( argv[optind] );
		checkgamma( f );
		gamr = 1.0 / f;
		f = atof( argv[optind+1] );
		checkgamma( f );
		gamg = 1.0 / f;
		f = atof( argv[optind+2] );
		checkgamma( f );
		gamb = 1.0 / f;
	} else {
		fprintf( stderr, usage );
		exit( 1 );
	}

	if( (fbp = fb_open( framebuffer, fbsize, fbsize )) == FBIO_NULL ) {
		exit( 2 );
	}

	/* draw the gamma image if requested */
	if (image) disp_image(fbp);

	/* get the starting map */
	if( overlay ) {
		fb_rmap( fbp, &cm );
	} else {
		/* start with a linear map */
		for( i = 0; i < 256; i++ ) {
			cm.cm_red[i] = cm.cm_green[i]
			= cm.cm_blue[i] = i << 8;
		}
	}

	/* apply the gamma(s) */
	for( i = 0; i < 256; i++ ) {
		if( gamr < 0 )
			cm.cm_red[i] = 65535 * pow( (double)cm.cm_red[i] / 65535.0, -1.0/gamr );
		else
			cm.cm_red[i] = 65535 * pow( (double)cm.cm_red[i] / 65535.0, gamr );
		if( onegamma && (overlay == 0) ) {
			cm.cm_green[i] = cm.cm_red[i];
			cm.cm_blue[i]  = cm.cm_red[i];
		} else {
			if( gamg < 0 )
				cm.cm_green[i] = 65535 * pow( (double)cm.cm_green[i] / 65535.0, -1.0/gamg );
			else
				cm.cm_green[i] = 65535 * pow( (double)cm.cm_green[i] / 65535.0, gamg );
			if( gamb < 0 )
				cm.cm_blue[i]  = 65535 * pow( (double)cm.cm_blue[i] / 65535.0, -1.0/gamb );
			else
				cm.cm_blue[i]  = 65535 * pow( (double)cm.cm_blue[i] / 65535.0, gamb );
		}
	}

	fb_wmap( fbp, &cm );
	fb_close( fbp );
	return(0);
}

void
checkgamma( g )
double g;
{
	if( fabs(g) < 1.0e-10 ) {
		fprintf( stderr, "fbgamma: gamma too close to zero\n" );
		fprintf( stderr, usage );
		exit( 3 );
	}
}
