/*                       F B C O L O R . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2006 United States Government as represented by
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
 *
 */
/** @file fbcolor.c
 *
 * Function -
 *	Dynamicly show the desired color as the background,
 *	and in bargraph form, using the color map.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include "machine.h"
#include "bu.h"
#include "fb.h"
#include "libtermio.h"

int curchan = 0;	/* 0=r, 1=g, 2=b */

int col[6] = {128,128,128};		/* r,g,b h,s,v */

unsigned char buf[3*2048];
ColorMap old_map;
ColorMap cm;

static char	*framebuffer = NULL;
static FBIO	*fbp;
static int	scr_height;
static int	scr_width;

void	new_rgb(void), rgbhsv(register int *rgb, register int *hsv), hsvrgb(register int *hsv, register int *rgb);
int	pars_Argv(int argc, register char **argv);
int	doKeyPad(void);

static char usage[] = "\
Usage: fbcolor [-h] [-F framebuffer]\n\
	[-s squarescrsize] [-w scr_width] [-n scr_height]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height]\n";

int
main(int argc, char **argv)
{
	register int i;

	if( ! pars_Argv( argc, argv ) )  {
		(void)fputs(usage, stderr);
		return	1;
	}
	if( (fbp = fb_open( framebuffer, scr_width, scr_height )) == FBIO_NULL )  {
		fprintf(stderr,"fbcolor:  fb_open(%s) failure\n", framebuffer);
		return	1;
	}

	/* Get the actual screen size we were given */
	scr_width = fb_getwidth(fbp);
	scr_height = fb_getheight(fbp);

	fb_rmap( fbp, &old_map );
	fb_clear( fbp, RGBPIXEL_NULL );

	rgbhsv( col, &col[3] );

	/* Note that color 0,0,0 is special;  use 1,1,1 for black */
	/* Red */
	for( i=0; i<255; i++)  {
		buf[3*i+RED] = i;
		buf[3*i+GRN] = 1;
		buf[3*i+BLU] = 1;
	}
	for( i=0; i<99; i++ )
		fb_write( fbp, 0, i, buf, 256 );

	/* Green */
	bzero( (char *)buf, sizeof(buf) );
	for( i=0; i<255; i++) {
		buf[3*i+RED] = 1;
		buf[3*i+GRN] = i;
		buf[3*i+BLU] = 1;
	}
	for( i=100; i<199; i++ )
		fb_write( fbp, 0, i, buf, 256 );

	/* Blue */
	bzero( (char *)buf, sizeof(buf) );
	for( i=0; i<255; i++)  {
		buf[3*i+RED] = 1;
		buf[3*i+GRN] = 1;
		buf[3*i+BLU] = i;
	}
	for( i=200; i<299; i++ )
		fb_write( fbp, 0, i, buf, 256 );

	/* Set RAW mode */
	save_Tty( 0 );
	set_Raw( 0 );
	clr_Echo( 0 );

	do  {
		/* Build color map for current value */
		bzero( (char *)&cm, sizeof(cm) );
		for( i=0; i<col[RED]; i++ )
			cm.cm_red[i] = 0xFFFF;
		for( ; i<255; i++ )
			cm.cm_red[i] = 0;

		for( i=0; i<col[GRN]; i++ )
			cm.cm_green[i] = 0xFFFF;
		for( ; i<255; i++ )
			cm.cm_green[i] = 0;

		for( i=0; i<col[BLU]; i++ )
			cm.cm_blue[i] = 0xFFFF;
		for( ; i<255; i++ )
			cm.cm_blue[i] = 0;

		/* 0,0,0 is color chosen */
		cm.cm_red[0] = col[RED]<<8;
		cm.cm_green[0] = col[GRN]<<8;
		cm.cm_blue[0] = col[BLU]<<8;

		/* 1,1,1 is for black */
		cm.cm_red[1] = 0;
		cm.cm_green[1] = 0;
		cm.cm_blue[1] = 0;

		fb_wmap( fbp, &cm );

		(void) fprintf( stdout,
				"%c rgb=%3d,%3d,%3d hsv=%3d,%3d,%3d   \r",
				"RGBHSIx"[curchan],
				col[0], col[1], col[2],
				col[3], col[4], col[5]	);
		(void) fflush( stdout );
	} while( doKeyPad() );

	fb_wmap( fbp, &old_map );
	reset_Tty( 0 );
	(void) fprintf( stdout,  "\n");	/* Move off of the output line.	*/
	return	0;
}

char help[] = "\r\n\
+ .	increase by 1\r\n\
- ,	decrease by 1\r\n\
>	increase by 16\r\n\
<	decrease by 16\r\n\
r	select red\r\n\
g	select green\r\n\
b	select blue\r\n\
h	select hue\r\n\
s	select saturation\r\n\
i v	select intensity value\r\n\
q	quit\r\n\
\\n	Exit\r\n";

int
doKeyPad(void)
{
	register int ch;

	if( (ch = getchar()) == EOF )
		return	0;		/* done */

	switch( ch )  {
	default :
		(void) fprintf( stdout,
				"\r\n'%c' bad -- Type ? for help\r\n",
				ch
				);
		break;
	case '?' :
		(void) fprintf( stdout, "\r\n%s", help );
		break;
	case '\r' :
	case '\n' :				/* Done, return to normal */
	case 'q' :
		return	0;
	case 'Q' :				/* Done, leave "as is" */
		return	0;

	case 'r':
		curchan = 0;
		break;
	case 'g':
		curchan = 1;
		break;
	case 'b':
		curchan = 2;
		break;
	case 'h':
		curchan = 3;
		break;
	case 's':
		curchan = 4;
		break;
	case 'v':
	case 'i':
		curchan = 5;
		break;
	case '/':
		if( ++curchan >= 6 )  curchan = 0;
		break;

	/* unit changes with -+ or ,. */
	case '+':
	case '.':
		col[curchan]++;
		new_rgb();
		break;
	case '-':
	case ',':
		col[curchan]--;
		new_rgb();
		break;

	/* big changes with <> */
	case '>':
		col[curchan]+=16;
		new_rgb();
		break;
	case '<':
		col[curchan]-=16;
		new_rgb();
		break;
	}
	return	1;		/* keep going */
}

void
new_rgb(void) {
	/* Wrap values to stay in range 0..255 */
	if( col[curchan] < 0 ) col[curchan] = 255;
	if( col[curchan] > 255 ) col[curchan] = 0;
	/* recompute either rgb or hsv from the other */
	if( curchan < 3 )
		rgbhsv( col, &col[3] );
	else
		hsvrgb( &col[3], col );
}

/*	p a r s _ A r g v ( )
 */
int
pars_Argv(int argc, register char **argv)
{
	register int	c;
	while( (c = bu_getopt( argc, argv, "F:s:S:w:W:n:N:h" )) != EOF )  {
		switch( c )  {
		case 'F':
			framebuffer = bu_optarg;
			break;
		case 'h' : /* High resolution frame buffer.	*/
			scr_height = scr_width = 1024;
			break;
		case 's':
		case 'S':
			scr_height = scr_width = atoi(bu_optarg);
			break;
		case 'w':
		case 'W':
			scr_width = atoi(bu_optarg);
			break;
		case 'n':
		case 'N':
			scr_height = atoi(bu_optarg);
			break;
		case '?' :
			return	0;
		}
	}
	return	1;
}

/* rgbhsv
 *
 * convert red green blue to hue saturation value
 */
void
rgbhsv(register int *rgb, register int *hsv)
{
        int	s, v;
        int	r, g, b;
        int	x;
	static int h;
        double dif = 0;

        r = rgb[0];
        g = rgb[1];
        b = rgb[2];
        v = ((r > g) ? r : g);
        v = ((v > b) ? v : b);
        x = ((r < g) ? r : g);
        x = ((x < b) ? x : b);
	if (v != x)  {
            dif = (double) (v - x);
            if (r != v)  {
                if (g == v)  {
                    if (b != x)
                        h = (int) (42.5 * (3. - (double)(v-b) / dif));
                    else
                        h = (int) (42.5 * (1. + (double)(v-r) / dif));
                } else {
                    if (r != x)
                        h = (int) (42.5 * (5. - (double)(v-r) / dif));
                    else
                        h = (int) (42.5 * (3. + (double)(v-g) / dif));
                }
            } else {
                if (g != x)
                    h = (int) (42.5 * (1. - (double)(v-g) / dif));
                else
                    h = (int) (42.5 * (5. + (double)(v-b) / dif));
            }
	}

	if (v != 0)
            s = (int)(255. * dif / (double)v);
        else
            s = 0;

        hsv[0] = h;
        hsv[1] = s;
        hsv[2] = v;
}

/* hsvrgb
 *
 * convert hue saturation and value to red, green, blue
 */

double modf(double, double *);

void
hsvrgb(register int *hsv, register int *rgb)
{
  	int r, g, b, m, n, k;
        double h, s, v, foo;
        double f;

	if(hsv[1] != 0)
        {
            s = (double)hsv[1] / 255.;
            h = (double)hsv[0] / 42.666;
            f = modf(h, &(foo));
            v = (double)hsv[2];
            m = (int) (v * (1. - s) + .5);
            n = (int) (v * (1. - s*f) + .5);
            k = (int) (v * (1. - (s * (1.-f))) + .5);
            switch((int) h)
            {
            case 0:
                r = hsv[2];
                g = k;
                b = m;
                break;
            case 1:
                r = n;
                g = hsv[2];
                b = m;
                break;
            case 2:
                r = m;
                g = hsv[2];
                b = k;
                break;
            case 3:
                r = m;
                g = n;
                b = hsv[2];
                break;
            case 4:
                r = k;
                g = m;
                b = hsv[2];
                break;
            default:
            case 5:
                r = hsv[2];
                g = m;
                b = n;
                break;
            }
        }
        else
            r = g = b = hsv[2];

        rgb[0] = r;
        rgb[1] = g;
        rgb[2] = b;
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
