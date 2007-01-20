/*                         P L O T 3 . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup plot */
/*@{*/
/** @file plot3.c
 * @brief
 *  A public-domain UNIX plot library, for 2-D and 3-D plotting in
 *  16-bit VAX signed integer spaces, or 64-bit IEEE floating point.
 *
 *  These routines generate "UNIX plot" output (with the addition
 *  of 3-D commands).  They behave almost exactly like the regular
 *  libplot routines, except:
 *
 *  -# These all take a stdio file pointer, and can thus be used to create multiple plot files simultaneously.
 *  -# There are 3-D versions of most commands.
 *  -# There are IEEE floating point versions of the commands.
 *  -# The names have been changed.
 *
 *  The 3-D extensions are those of Doug Gwyn, from his System V extensions.
 *
 *  Author -
 *	Phillip Dykstra
 *	24 Sep 1986
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



#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "plot3.h"


static int pl_outputMode = PL_OUTPUT_MODE_BINARY;

/* For the sake of efficiency, we trust putc() to write only one byte */
/*#define putsi(a)	putc(a&0377,plotfp); putc((a>>8)&0377,plotfp)*/
#define putsi(a)	putc(a,plotfp); putc((a>>8),plotfp)


/*
 *  These interfaces provide the standard UNIX-Plot functionality
 */

int
pl_getOutputMode() {
    return pl_outputMode;
}

void
pl_setOutputMode(int mode) {
    pl_outputMode = mode;
}

/**
 * @brief
 * plot a point
 */
void
pl_point(register FILE *plotfp, int x, int y)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'p', plotfp );
	putsi( x );
	putsi( y );
    } else {
	fprintf(plotfp, "p %d %d\n", x, y);
    }
}

void
pl_line(register FILE *plotfp, int x1, int y1, int x2, int y2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'l', plotfp );
	putsi( x1 );
	putsi( y1 );
	putsi( x2 );
	putsi( y2 );
    } else {
	fprintf(plotfp, "l %d %d %d %d\n", x1, y1, x2, y2);
    }
}

void
pl_linmod(register FILE *plotfp, register char *s)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'f', plotfp );
	while( *s )
		putc( *s++, plotfp );
	putc( '\n', plotfp );
    } else {
	fprintf(plotfp, "f %s\n", s);
    }
}

void
pl_move(register FILE *plotfp, int x, int y)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'm', plotfp );
	putsi( x );
	putsi( y );
    } else {
	fprintf(plotfp, "m %d %d\n", x, y);
    }
}

void
pl_cont(register FILE *plotfp, int x, int y)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'n', plotfp );
	putsi( x );
	putsi( y );
    } else {
	fprintf(plotfp, "n %d %d\n", x, y);
    }
}

void
pl_label(register FILE *plotfp, register char *s)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 't', plotfp );
	while( *s )
		putc( *s++, plotfp );
	putc( '\n', plotfp );
    } else {
	fprintf(plotfp, "t %s\n", s);
    }
}

void
pl_space(register FILE *plotfp, int x1, int y1, int x2, int y2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 's', plotfp );
	putsi( x1 );
	putsi( y1 );
	putsi( x2 );
	putsi( y2 );
    } else {
	fprintf(plotfp, "s %d %d %d %d\n", x1, y1, x2, y2);
    }
}

void
pl_erase(register FILE *plotfp)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY)
	putc( 'e', plotfp );
    else
	fprintf(plotfp, "e\n");
}

void
pl_circle(register FILE *plotfp, int x, int y, int r)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'c', plotfp );
	putsi( x );
	putsi( y );
	putsi( r );
    } else {
	fprintf(plotfp, "c %d %d %d\n", x, y, r);
    }
}

void
pl_arc(register FILE *plotfp, int xc, int yc, int x1, int y1, int x2, int y2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'a', plotfp );
	putsi( xc );
	putsi( yc );
	putsi( x1 );
	putsi( y1 );
	putsi( x2 );
	putsi( y2 );
    } else {
	fprintf(plotfp, "a %d %d %d %d %d %d\n", xc, yc, x1, y1, x2, y2);
    }
}

void
pl_box(register FILE *plotfp, int x1, int y1, int x2, int y2)
{
	pl_move( plotfp, x1, y1 );
	pl_cont( plotfp, x1, y2 );
	pl_cont( plotfp, x2, y2 );
	pl_cont( plotfp, x2, y1 );
	pl_cont( plotfp, x1, y1 );
	pl_move( plotfp, x2, y2 );
}

/*
 * Here lie the BRL 3-D extensions.
 */

/* Warning: r, g, b are ints.  The output is chars. */
void
pl_color(register FILE *plotfp, int r, int g, int b)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'C', plotfp );
	putc( r, plotfp );
	putc( g, plotfp );
	putc( b, plotfp );
    } else {
	fprintf(plotfp, "C %d %d %d\n", r, g, b);
    }
}

void
pl_flush(register FILE *plotfp)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'F', plotfp );
    } else {
	fprintf(plotfp, "F\n");
    }

    fflush( plotfp );
}

void
pl_3space(register FILE *plotfp, int x1, int y1, int z1, int x2, int y2, int z2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'S', plotfp );
	putsi( x1 );
	putsi( y1 );
	putsi( z1 );
	putsi( x2 );
	putsi( y2 );
	putsi( z2 );
    } else {
	fprintf(plotfp, "S %d %d %d %d %d %d\n", x1, y1, z1, x2, y2, z2);
    }
}

void
pl_3point(register FILE *plotfp, int x, int y, int z)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'P', plotfp );
	putsi( x );
	putsi( y );
	putsi( z );
    } else {
	fprintf(plotfp, "P %d %d %d\n", x, y, z);
    }
}

void
pl_3move(register FILE *plotfp, int x, int y, int z)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'M', plotfp );
	putsi( x );
	putsi( y );
	putsi( z );
    } else {
	fprintf(plotfp, "M %d %d %d\n", x, y, z);
    }
}

void
pl_3cont(register FILE *plotfp, int x, int y, int z)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'N', plotfp );
	putsi( x );
	putsi( y );
	putsi( z );
    } else {
	fprintf(plotfp, "N %d %d %d\n", x, y, z);
    }
}

void
pl_3line(register FILE *plotfp, int x1, int y1, int z1, int x2, int y2, int z2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( 'L', plotfp );
	putsi( x1 );
	putsi( y1 );
	putsi( z1 );
	putsi( x2 );
	putsi( y2 );
	putsi( z2 );
    } else {
	fprintf(plotfp, "L %d %d %d %d %d %d\n", x1, y1, z1, x2, y2, z2);
    }
}

void
pl_3box(register FILE *plotfp, int x1, int y1, int z1, int x2, int y2, int z2)
{
	pl_3move( plotfp, x1, y1, z1 );
	/* first side */
	pl_3cont( plotfp, x1, y2, z1 );
	pl_3cont( plotfp, x1, y2, z2 );
	pl_3cont( plotfp, x1, y1, z2 );
	pl_3cont( plotfp, x1, y1, z1 );
	/* across */
	pl_3cont( plotfp, x2, y1, z1 );
	/* second side */
	pl_3cont( plotfp, x2, y2, z1 );
	pl_3cont( plotfp, x2, y2, z2 );
	pl_3cont( plotfp, x2, y1, z2 );
	pl_3cont( plotfp, x2, y1, z1 );
	/* front edge */
	pl_3move( plotfp, x1, y2, z1 );
	pl_3cont( plotfp, x2, y2, z1 );
	/* bottom back */
	pl_3move( plotfp, x1, y1, z2 );
	pl_3cont( plotfp, x2, y1, z2 );
	/* top back */
	pl_3move( plotfp, x1, y2, z2 );
	pl_3cont( plotfp, x2, y2, z2 );
}

/*
 * Double floating point versions
 */

void
pd_point(register FILE *plotfp, double x, double y)
{
    double	in[2];
    unsigned char	out[2*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x;
	in[1] = y;
	htond( &out[1], (unsigned char *)in, 2 );

	out[0] = 'x';
	fwrite( out, 1, 2*8+1, plotfp );
    } else {
	fprintf(plotfp, "x %g %g\n", x, y);
    }
}

void
pd_line(register FILE *plotfp, double x1, double y1, double x2, double y2)
{
    double	in[4];
    unsigned char	out[4*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x1;
	in[1] = y1;
	in[2] = x2;
	in[3] = y2;
	htond( &out[1], (unsigned char *)in, 4 );

	out[0] = 'v';
	fwrite( out, 1, 4*8+1, plotfp );
    } else {
	fprintf(plotfp, "v %g %g %g %g\n", x1, y1, x2, y2);
    }
}

/* Note: no pd_linmod(), just use pl_linmod() */

void
pd_move(register FILE *plotfp, double x, double y)
{
    double	in[2];
    unsigned char	out[2*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x;
	in[1] = y;
	htond( &out[1], (unsigned char *)in, 2 );

	out[0] = 'o';
	fwrite( out, 1, 2*8+1, plotfp );
    } else {
	fprintf(plotfp, "o %g %g\n", x, y);
    }
}

void
pd_cont(register FILE *plotfp, double x, double y)
{
    double	in[2];
    unsigned char	out[2*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x;
	in[1] = y;
	htond( &out[1], (unsigned char *)in, 2 );

	out[0] = 'q';
	fwrite( out, 1, 2*8+1, plotfp );
    } else {
	fprintf(plotfp, "q %g %g\n", x, y);
    }
}

void
pd_space(register FILE *plotfp, double x1, double y1, double x2, double y2)
{
    double	in[4];
    unsigned char	out[4*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x1;
	in[1] = y1;
	in[2] = x2;
	in[3] = y2;
	htond( &out[1], (unsigned char *)in, 4 );

	out[0] = 'w';
	fwrite( out, 1, 4*8+1, plotfp );
    } else {
	fprintf(plotfp, "w %g %g %g %g\n", x1, y1, x2, y2);
    }
}

void
pd_circle(register FILE *plotfp, double x, double y, double r)
{
    double	in[3];
    unsigned char	out[3*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x;
	in[1] = y;
	in[2] = r;
	htond( &out[1], (unsigned char *)in, 3 );

	out[0] = 'i';
	fwrite( out, 1, 3*8+1, plotfp );
    } else {
	fprintf(plotfp, "i %g %g %g\n", x, y, r);
    }
}

void
pd_arc(register FILE *plotfp, double xc, double yc, double x1, double y1, double x2, double y2)
{
    double	in[6];
    unsigned char	out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = xc;
	in[1] = yc;
	in[2] = x1;
	in[3] = y1;
	in[4] = x2;
	in[5] = y2;
	htond( &out[1], (unsigned char *)in, 6 );

	out[0] = 'r';
	fwrite( out, 1, 6*8+1, plotfp );
    } else {
	fprintf(plotfp, "r %g %g %g %g %g %g\n", xc, yc, x1, y1, x2, y2);
    }
}

void
pd_box(register FILE *plotfp, double x1, double y1, double x2, double y2)
{
	pd_move( plotfp, x1, y1 );
	pd_cont( plotfp, x1, y2 );
	pd_cont( plotfp, x2, y2 );
	pd_cont( plotfp, x2, y1 );
	pd_cont( plotfp, x1, y1 );
	pd_move( plotfp, x2, y2 );
}

/* Double 3-D, both in vector and enumerated versions */
void
pdv_3space(register FILE *plotfp, const fastf_t *min, const fastf_t *max)
{
    unsigned char	out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	htond( &out[1], (unsigned char *)min, 3 );
	htond( &out[3*8+1], (unsigned char *)max, 3 );

	out[0] = 'W';
	fwrite( out, 1, 6*8+1, plotfp );
    } else {
	fprintf(plotfp, "W %g %g %g %g %g %g\n", V3ARGS(min), V3ARGS(max));
    }
}

void
pd_3space(register FILE *plotfp, double x1, double y1, double z1, double x2, double y2, double z2)
{
    double	in[6];
    unsigned char	out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x1;
	in[1] = y1;
	in[2] = z1;
	in[3] = x2;
	in[4] = y2;
	in[5] = z2;
	htond( &out[1], (unsigned char *)in, 6 );

	out[0] = 'W';
	fwrite( out, 1, 6*8+1, plotfp );
    } else {
	fprintf(plotfp, "W %g %g %g %g %g %g\n", x1, y1, z1, x2, y2, z2);
    }
}

void
pdv_3point(register FILE *plotfp, const fastf_t *pt)
{
    unsigned char	out[3*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	htond( &out[1], (unsigned char *)pt, 3 );

	out[0] = 'X';
	fwrite( out, 1, 3*8+1, plotfp );
    } else {
	fprintf(plotfp, "X %g %g %g\n", V3ARGS(pt));
    }
}

void
pd_3point(register FILE *plotfp, double x, double y, double z)
{
    double	in[3];
    unsigned char	out[3*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x;
	in[1] = y;
	in[2] = z;
	htond( &out[1], (unsigned char *)in, 3 );

	out[0] = 'X';
	fwrite( out, 1, 3*8+1, plotfp );
    } else {
	fprintf(plotfp, "X %g %g %g\n", x, y, z);
    }
}

void
pdv_3move(register FILE *plotfp, const fastf_t *pt)
{
    unsigned char	out[3*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	htond( &out[1], (unsigned char *)pt, 3 );

	out[0] = 'O';
	fwrite( out, 1, 3*8+1, plotfp );
    } else {
	fprintf(plotfp, "O %g %g %g\n", V3ARGS(pt));
    }
}

void
pd_3move(register FILE *plotfp, double x, double y, double z)
{
    double	in[3];
    unsigned char	out[3*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x;
	in[1] = y;
	in[2] = z;
	htond( &out[1], (unsigned char *)in, 3 );

	out[0] = 'O';
	fwrite( out, 1, 3*8+1, plotfp );
    } else {
	fprintf(plotfp, "O %g %g %g\n", x, y, z);
    }
}

void
pdv_3cont(register FILE *plotfp, const fastf_t *pt)
{
    unsigned char	out[3*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	htond( &out[1], (unsigned char *)pt, 3 );

	out[0] = 'Q';
	fwrite( out, 1, 3*8+1, plotfp );
    } else {
	fprintf(plotfp, "Q %g %g %g\n", V3ARGS(pt));
    }
}

void
pd_3cont(register FILE *plotfp, double x, double y, double z)
{
    double	in[3];
    unsigned char	out[3*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x;
	in[1] = y;
	in[2] = z;
	htond( &out[1], (unsigned char *)in, 3 );

	out[0] = 'Q';
	fwrite( out, 1, 3*8+1, plotfp );
    } else {
	fprintf(plotfp, "Q %g %g %g\n", x, y, z);
    }
}

void
pdv_3line(register FILE *plotfp, const fastf_t *a, const fastf_t *b)
{
    unsigned char	out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	htond( &out[1], (unsigned char *)a, 3 );
	htond( &out[3*8+1], (unsigned char *)b, 3 );

	out[0] = 'V';
	fwrite( out, 1, 6*8+1, plotfp );
    } else {
	fprintf(plotfp, "V %g %g %g %g %g %g\n", V3ARGS(a), V3ARGS(b));
    }
}

void
pd_3line(register FILE *plotfp, double x1, double y1, double z1, double x2, double y2, double z2)
{
    double	in[6];
    unsigned char	out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x1;
	in[1] = y1;
	in[2] = z1;
	in[3] = x2;
	in[4] = y2;
	in[5] = z2;
	htond( &out[1], (unsigned char *)in, 6 );

	out[0] = 'V';
	fwrite( out, 1, 6*8+1, plotfp );
    } else {
	fprintf(plotfp, "V %g %g %g %g %g %g\n", x1, y1, z1, x2, y2, z2);
    }
}

void
pdv_3box(register FILE *plotfp, const fastf_t *a, const fastf_t *b)
{
	pd_3move( plotfp, a[X], a[Y], a[Z] );
	/* first side */
	pd_3cont( plotfp, a[X], b[Y], a[Z] );
	pd_3cont( plotfp, a[X], b[Y], b[Z] );
	pd_3cont( plotfp, a[X], a[Y], b[Z] );
	pd_3cont( plotfp, a[X], a[Y], a[Z] );
	/* across */
	pd_3cont( plotfp, b[X], a[Y], a[Z] );
	/* second side */
	pd_3cont( plotfp, b[X], b[Y], a[Z] );
	pd_3cont( plotfp, b[X], b[Y], b[Z] );
	pd_3cont( plotfp, b[X], a[Y], b[Z] );
	pd_3cont( plotfp, b[X], a[Y], a[Z] );
	/* front edge */
	pd_3move( plotfp, a[X], b[Y], a[Z] );
	pd_3cont( plotfp, b[X], b[Y], a[Z] );
	/* bottom back */
	pd_3move( plotfp, a[X], a[Y], b[Z] );
	pd_3cont( plotfp, b[X], a[Y], b[Z] );
	/* top back */
	pd_3move( plotfp, a[X], b[Y], b[Z] );
	pd_3cont( plotfp, b[X], b[Y], b[Z] );
}

void
pd_3box(register FILE *plotfp, double x1, double y1, double z1, double x2, double y2, double z2)
{
	pd_3move( plotfp, x1, y1, z1 );
	/* first side */
	pd_3cont( plotfp, x1, y2, z1 );
	pd_3cont( plotfp, x1, y2, z2 );
	pd_3cont( plotfp, x1, y1, z2 );
	pd_3cont( plotfp, x1, y1, z1 );
	/* across */
	pd_3cont( plotfp, x2, y1, z1 );
	/* second side */
	pd_3cont( plotfp, x2, y2, z1 );
	pd_3cont( plotfp, x2, y2, z2 );
	pd_3cont( plotfp, x2, y1, z2 );
	pd_3cont( plotfp, x2, y1, z1 );
	/* front edge */
	pd_3move( plotfp, x1, y2, z1 );
	pd_3cont( plotfp, x2, y2, z1 );
	/* bottom back */
	pd_3move( plotfp, x1, y1, z2 );
	pd_3cont( plotfp, x2, y1, z2 );
	/* top back */
	pd_3move( plotfp, x1, y2, z2 );
	pd_3cont( plotfp, x2, y2, z2 );
}

/**
 *  Draw a ray
 */
void
pdv_3ray(FILE *fp, const fastf_t *pt, const fastf_t *dir, double t)
{
	point_t	tip;

	VJOIN1( tip, pt, t, dir );
	pdv_3move( fp, pt );
	pdv_3cont( fp, tip );
}

/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
