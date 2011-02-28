/*                         D D I S P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
 */
/** @file ddisp.c
 *
 * Data Display - shows doubles on a framebuffer in various ways.
 *
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include "bio.h"

#include "fb.h"

#define	MAXPTS 4096

int	Clear = 0;
int	pause_time = 0;
int	mode = 0;
#define	VERT	1
#define	BARS	2

FBIO	*fbp;
int	fbsize = 512;

void	lineout(double *dat, int n);
void	disp_inten(double *buf, int size);
void	disp_bars(double *buf, int size);

static const char usage[] = "\
Usage: ddisp [-v -b -p -c -h] [width (512)]\n";

int
main(int argc, char **argv)
{
    double	buf[MAXPTS];

    int	n, L;

    while ( argc > 1 ) {
	if ( BU_STR_EQUAL(argv[1], "-v") ) {
	    mode = VERT;
	    pause_time = 0;
	    Clear = 0;
	} else if ( BU_STR_EQUAL(argv[1], "-b") ) {
	    mode = BARS;
	} else if ( BU_STR_EQUAL(argv[1], "-p") ) {
	    pause_time = 3;
	} else if ( BU_STR_EQUAL(argv[1], "-c") ) {
	    Clear++;
	} else if ( BU_STR_EQUAL(argv[1], "-h") ) {
	    fbsize = 1024;
	} else
	    break;
	argc--;
	argv++;
    }

    if ( isatty(fileno(stdin)) ) {
	bu_exit(1, "%s", usage );
    }
    if ( (fbp = fb_open( NULL, fbsize, fbsize )) == FBIO_NULL ) {
	bu_exit(2, "Unable to open framebuffer\n");
    }

    L = (argc > 1) ? atoi(argv[1]) : 512;

    while ( (n = fread(buf, sizeof(*buf), L, stdin)) > 0 ) {
	/* XXX - width hack */
	if ( n > fb_getwidth(fbp) )
	    n = fb_getwidth(fbp);

	if ( Clear )
	    fb_clear( fbp, PIXEL_NULL );
	if ( mode == VERT )
	    disp_inten( buf, n );
	else if ( mode == BARS )
	    disp_bars( buf, n );
	else
	    lineout( buf, n );
	if ( pause_time )
	    sleep( pause_time );
    }
    fb_close(fbp);

    return 0;
}

void
lineout(double *dat, int n)
{
    static	int	y = 0;
    int	i, value;
    RGBpixel lbuf[1024*4];

    if ( n > fb_getwidth(fbp) ) n = fb_getwidth(fbp);

    for ( i = 0; i < n; i++ ) {
	/* Magnitude version */
	value = dat[i] * 255.9;
	if ( value < 0 ) value = 0;
	else if ( value > 255 ) value = 255;
	lbuf[i][RED] = lbuf[i][GRN] = lbuf[i][BLU] = value;
    }
    fb_write( fbp, 0, y, (unsigned char *)lbuf, n );

    /* Next screen position */
    y = (y + 1) % fb_getheight(fbp);
}

/*
 *  Display doubles.
 *  +/- 1.0 in, becomes +/- 128 from center Y.
 */
void
disp_inten(double *buf, int size)
{
    int	x, y;
    RGBpixel color;

/*	color.red = color.green = color.blue = 255;*/

    if ( size > fb_getwidth(fbp) ) size = fb_getwidth(fbp);

    for ( x = 0; x < size; x++ ) {
	y = buf[x] * 128;
#ifdef OVERLAY
	fb_read( fbp, x, y+255, color, 1 );
#else
	color[RED] = color[BLU] = 0;
#endif
	color[GRN] = 255;
	fb_write( fbp, x, y+255, color, 1 );
    }
}

/*
 *  Display doubles.
 *  +/- 1.0 in, becomes +/- 128 from center Y.
 */
void
disp_bars(double *buf, int size)
{
    int	x, y;
    RGBpixel color;

/*	color.red = color.green = color.blue = 255;*/

    if ( size > fb_getwidth(fbp) ) size = fb_getwidth(fbp);

    for ( x = 0; x < size; x++ ) {
	if ( buf[x] > 1.0 ) {
	    y = 128;
	} else if ( buf[x] < -1.0 ) {
	    y = -128;
	} else {
	    y = buf[x] * 128;
	}
#ifdef OVERLAY
	fb_read( fbp, x, y+255, color, 1 );
#else
	color[RED] = color[BLU] = 0;
#endif
	color[GRN] = 255;
	if ( y > 0 ) {
	    while ( y >= 0 ) {
		fb_write( fbp, x, y+255, color, 1 );
		y--;
	    }
	} else {
	    while ( y <= 0 ) {
		fb_write( fbp, x, y+255, color, 1 );
		y++;
	    }
	}
    }
}

#ifdef OLDANDCRUFTY
/* Calculate Critical Band filter weights */
if ( cflag ) {
    cbweights( &cbfilter[0], window_size, 19 );
    cbsum = 0.0;
    for ( i = 0; i < 19; i++ )
	cbsum += cbfilter[i];
}

/*
 * Scale 0 -> 65536 to 0 -> 100 then double it.
 *  (so 32767 is 100)
 */
disp_mag( buf, size )
#ifdef FHT
    double	buf[];
#else
    COMPLEX	buf[];
#endif
    int size;
{
    int	i, j, x;
    int	mag;
    double	value, sum;
    double	lin[513], logout[513];
    RGBpixel mcolor;

    if ( size > 1024 ) size = 1024;

    /* Put magnitudes in linear buffer */
/*	lin[0] = buf[0]/256.0;  NO DC ON LOG SCALE! */
    for (i = 1; i < size/2; i++) {
#ifdef FHT
	value = 2.0*sqrt((buf[i]*buf[i]
			  +buf[512-i]*buf[512-i])/2.0) / 256.0;
#else
	value = hypot( buf[i].re, buf[i].im );
#endif
/*printf("mag = %f, ", value );*/
	if ( value < 0.6 )
	    value = 0.0;
	else
	    value = 20.0 * log10( value / 65535.0 ) + 100.0;
/*printf("value = %f\n", value );*/
	lin[i-1] = value;
    }
#ifdef FHT
    lin[size/2-1] = buf[size/2]/256.0;
#else
    lin[size/2-1] = buf[size/2].re/256.0;
#endif
    /* Interp to Log scale */
    if ( lflag ) {
	LintoLog( lin, logout, size/2 );
    } else {
	for ( i = 0; i < size/2; i++ )
	    logout[i] = lin[i];	/* yeah, this does suck. */
    }

    /* Critical Band Filter */
    if ( cflag ) {
	for ( i = 0; i < size/2; i++ )
	    lin[i] = logout[i];	/* Borrow lin */
	for ( i = 0+9; i < size/2-9; i++ ) {
	    sum = 0.0;
	    for ( j = -9; j <= 9; j++ )
		sum += lin[i+j] * cbfilter[j+9];
	    logout[i] = sum / cbsum;
	}
    }

    /* Plot log values */
    for ( i = 0; i < size/2 + 1; i++ ) {
	mag = 2.0*logout[i] + 0.5;	/* 200 point range */
	if ( size > 512 ) x = i;
	else x = 2*i;
#ifdef OVERLAY
	fb_read( fbp, x, mag+255, mcolor, 1 );
#else
	mcolor[RED] = mcolor[GRN] = 0;
#endif
	mcolor[BLU] = 255;
	fb_write( fbp, x, mag+255, mcolor, 1 );
	if ( size <= 512 ) {
#ifdef OVERLAY
	    fb_read( fbp, x+1, mag+255, mcolor, 1 );
#else
	    mcolor[RED] = mcolor[GRN] = 0;
#endif
	    mcolor[BLU] = 255;
	    fb_write( fbp, x+1, mag+255, mcolor, 1 );
	}
    }
}

/*
 * -PI -> PI becomes -128 -> 128
 */
disp_phase( buf, size )
#ifdef FHT
    double	buf[];
#else
    COMPLEX	buf[];
#endif
    int size;
{
    int	i, x;
    int	mag;
    double	angle;
    RGBpixel mcolor;

    if ( size > 1024 ) size = 1024;

#ifdef OVERLAY
    fb_read( fbp, 0, 255, mcolor, 1 );
#else
    mcolor[GRN] = mcolor[BLU] = 0;
#endif
    mcolor[RED] = 255;
    fb_write( fbp, 0, 255, mcolor, 1 );
    for (i = 1; i < size/2; i++) {
#ifdef FHT
	if ( fabs(buf[i]+buf[size-i]) < 0.0001 )
	    angle = M_PI_2;
	else
	    angle = atan( (buf[i]-buf[size-i])/
			  (buf[i]+buf[size-i]) );
#else
	/* four quadrant arctan.  THIS NEEDS WORK - XXX */
/*fprintf( stderr, "%3d: (%10f,%10f) -> ", i, buf[i].re, buf[i].im );*/
	if ( fabs( buf[i].re ) < 1.0e-10 ) {
	    /* XXX - check for im equally small */
	    if ( fabs( buf[i].im ) < 1.0e-10 )
		angle = 0.0;
	    else
		angle = (buf[i].im > 0.0) ? M_PI_2 : -M_PI_2;
	} else {
	    angle = atan( buf[i].im / buf[i].re );
	    if ( buf[i].re < 0.0 )
		angle += (buf[i].im > 0.0) ? PI : -PI;
	}
/*fprintf( stderr, "%10f Deg\n", RtoD(angle) );*/
#endif
	mag = (128.0/M_PI)*angle + 0.5;
#ifdef DEBUG
	printf("(%6.3f,%6.3f): angle = %7.3f (%6.2f), mag = %d\n",
	       buf[i].re, buf[i].im, angle, RtoD( angle ), mag );
#endif /* DEBUG */
	if ( size > 512 ) x = i;
	else x = 2*i;
#ifdef OVERLAY
	fb_read( fbp, x, mag+255, mcolor, 1 );
#else
	mcolor[GRN] = mcolor[BLU] = 0;
#endif
	mcolor[RED] = 255;
	fb_write( fbp, x, mag+255, mcolor, 1 );
	if ( size <= 512 ) {
#ifdef OVERLAY
	    fb_read( fbp, x+1, mag+255, mcolor, 1 );
#else
	    mcolor[GRN] = mcolor[BLU] = 0;
#endif
	    mcolor[RED] = 255;
	    fb_write( fbp, x+1, mag+255, mcolor, 1 );
	}
    }
#ifdef OVERLAY
    fb_read( fbp, size/2, 255, mcolor, 1 );
#else
    mcolor[GRN] = mcolor[BLU] = 0;
#endif
    mcolor[RED] = 255;
    fb_write( fbp, size/2, 255, mcolor, 1 );
}
#endif /* OLDANDCRUFTY */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
