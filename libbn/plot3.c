/*
 *			L I B P L O T 3 . C
 *
 *  A public-domain UNIX plot library, for 2-D and 3-D plotting in
 *  16-bit VAX signed integer spaces, or 64-bit IEEE floating point.
 *
 *  These routines generate "UNIX plot" output (with the addition
 *  of 3-D commands).  They behave almost exactly like the regular
 *  libplot routines, except:
 *
 *  (1) These all take a stdio file pointer, and can thus be used to
 *      create multiple plot files simultaneously.
 *  (2) There are 3-D versions of most commands.
 *  (3) There are IEEE floating point versions of the commands.
 *  (4) The names have been changed.
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
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "plot3.h"

/* For the sake of efficiency, we trust putc() to write only one byte */
/*#define putsi(a)	putc(a&0377,plotfp); putc((a>>8)&0377,plotfp)*/
#define putsi(a)	putc(a,plotfp); putc((a>>8),plotfp)


/*
 *  These interfaces provide the standard UNIX-Plot functionality
 */

void
pl_point( plotfp, x, y )
register FILE *plotfp;
int x, y;
{
	putc( 'p', plotfp );
	putsi( x );
	putsi( y );
}

void
pl_line( plotfp, x1, y1, x2, y2 )
register FILE *plotfp;
int x1, y1, x2, y2;
{
	putc( 'l', plotfp );
	putsi( x1 );
	putsi( y1 );
	putsi( x2 );
	putsi( y2 );
}

void
pl_linmod( plotfp, s )
register FILE *plotfp;
register char *s;
{
	putc( 'f', plotfp );
	while( *s )
		putc( *s++, plotfp );
	putc( '\n', plotfp );
}

void
pl_move( plotfp, x, y )
register FILE *plotfp;
int x, y;
{
	putc( 'm', plotfp );
	putsi( x );
	putsi( y );
}

void
pl_cont( plotfp, x, y )
register FILE *plotfp;
int x, y;
{
	putc( 'n', plotfp );
	putsi( x );
	putsi( y );
}

void
pl_label( plotfp, s )
register FILE *plotfp;
register char *s;
{
	putc( 't', plotfp );
	while( *s )
		putc( *s++, plotfp );
	putc( '\n', plotfp );
}

void
pl_space( plotfp, x1, y1, x2, y2 )
register FILE *plotfp;
int x1, y1, x2, y2;
{
	putc( 's', plotfp );
	putsi( x1 );
	putsi( y1 );
	putsi( x2 );
	putsi( y2 );
}

void
pl_erase( plotfp )
register FILE *plotfp;
{
	putc( 'e', plotfp );
}

void
pl_circle( plotfp, x, y, r )
register FILE *plotfp;
int x, y, r;
{
	putc( 'c', plotfp );
	putsi( x );
	putsi( y );
	putsi( r );
}

void
pl_arc( plotfp, xc, yc, x1, y1, x2, y2 )
register FILE *plotfp;
int xc, yc, x1, y1, x2, y2;
{
	putc( 'a', plotfp );
	putsi( xc );
	putsi( yc );
	putsi( x1 );
	putsi( y1 );
	putsi( x2 );
	putsi( y2 );
}

void
pl_box( plotfp, x1, y1, x2, y2 )
register FILE *plotfp;
int x1, y1, x2, y2;
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
pl_color( plotfp, r, g, b )
register FILE *plotfp;
int r, g, b;
{
	putc( 'C', plotfp );
	putc( r, plotfp );
	putc( g, plotfp );
	putc( b, plotfp );
}

void
pl_flush( plotfp )
register FILE *plotfp;
{
	putc( 'F', plotfp );
	fflush( plotfp );
}

void
pl_3space( plotfp, x1, y1, z1, x2, y2, z2 )
register FILE *plotfp;
int x1, y1, z1, x2, y2, z2;
{
	putc( 'S', plotfp );
	putsi( x1 );
	putsi( y1 );
	putsi( z1 );
	putsi( x2 );
	putsi( y2 );
	putsi( z2 );
}

void
pl_3point( plotfp, x, y, z )
register FILE *plotfp;
int x, y, z;
{
	putc( 'P', plotfp );
	putsi( x );
	putsi( y );
	putsi( z );
}

void
pl_3move( plotfp, x, y, z )
register FILE *plotfp;
int x, y, z;
{
	putc( 'M', plotfp );
	putsi( x );
	putsi( y );
	putsi( z );
}

void
pl_3cont( plotfp, x, y, z )
register FILE *plotfp;
int x, y, z;
{
	putc( 'N', plotfp );
	putsi( x );
	putsi( y );
	putsi( z );
}

void
pl_3line( plotfp, x1, y1, z1, x2, y2, z2 )
register FILE *plotfp;
int x1, y1, z1, x2, y2, z2;
{
	putc( 'L', plotfp );
	putsi( x1 );
	putsi( y1 );
	putsi( z1 );
	putsi( x2 );
	putsi( y2 );
	putsi( z2 );
}

void
pl_3box( plotfp, x1, y1, z1, x2, y2, z2 )
register FILE *plotfp;
int x1, y1, z1, x2, y2, z2;
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
pd_point( plotfp, x, y )
register FILE *plotfp;
double x, y;
{
	double	in[2];
	unsigned char	out[2*8+1];

	in[0] = x;
	in[1] = y;
	htond( &out[1], (unsigned char *)in, 2 );

	out[0] = 'x';
	fwrite( out, 1, 2*8+1, plotfp );
}

void
pd_line( plotfp, x1, y1, x2, y2 )
register FILE *plotfp;
double x1, y1, x2, y2;
{
	double	in[4];
	unsigned char	out[4*8+1];

	in[0] = x1;
	in[1] = y1;
	in[2] = x2;
	in[3] = y2;
	htond( &out[1], (unsigned char *)in, 4 );

	out[0] = 'v';
	fwrite( out, 1, 4*8+1, plotfp );
}

/* Note: no pd_linmod(), just use pl_linmod() */

void
pd_move( plotfp, x, y )
register FILE *plotfp;
double x, y;
{
	double	in[2];
	unsigned char	out[2*8+1];

	in[0] = x;
	in[1] = y;
	htond( &out[1], (unsigned char *)in, 2 );

	out[0] = 'o';
	fwrite( out, 1, 2*8+1, plotfp );
}

void
pd_cont( plotfp, x, y )
register FILE *plotfp;
double x, y;
{
	double	in[2];
	unsigned char	out[2*8+1];

	in[0] = x;
	in[1] = y;
	htond( &out[1], (unsigned char *)in, 2 );

	out[0] = 'q';
	fwrite( out, 1, 2*8+1, plotfp );
}

void
pd_space( plotfp, x1, y1, x2, y2 )
register FILE *plotfp;
double x1, y1, x2, y2;
{
	double	in[4];
	unsigned char	out[4*8+1];

	in[0] = x1;
	in[1] = y1;
	in[2] = x2;
	in[3] = y2;
	htond( &out[1], (unsigned char *)in, 4 );

	out[0] = 'w';
	fwrite( out, 1, 4*8+1, plotfp );
}

void
pd_circle( plotfp, x, y, r )
register FILE *plotfp;
double x, y, r;
{
	double	in[3];
	unsigned char	out[3*8+1];

	in[0] = x;
	in[1] = y;
	in[2] = r;
	htond( &out[1], (unsigned char *)in, 3 );

	out[0] = 'i';
	fwrite( out, 1, 3*8+1, plotfp );
}

void
pd_arc( plotfp, xc, yc, x1, y1, x2, y2 )
register FILE *plotfp;
double xc, yc, x1, y1, x2, y2;
{
	double	in[6];
	unsigned char	out[6*8+1];

	in[0] = xc;
	in[1] = yc;
	in[2] = x1;
	in[3] = y1;
	in[4] = x2;
	in[5] = y2;
	htond( &out[1], (unsigned char *)in, 6 );

	out[0] = 'r';
	fwrite( out, 1, 6*8+1, plotfp );
}

void
pd_box( plotfp, x1, y1, x2, y2 )
register FILE *plotfp;
double x1, y1, x2, y2;
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
pdv_3space( plotfp, min, max )
register FILE *plotfp;
const vect_t	min;
const vect_t	max;
{
	unsigned char	out[6*8+1];

	htond( &out[1], (unsigned char *)min, 3 );
	htond( &out[3*8+1], (unsigned char *)max, 3 );

	out[0] = 'W';
	fwrite( out, 1, 6*8+1, plotfp );
}

void
pd_3space( plotfp, x1, y1, z1, x2, y2, z2 )
register FILE *plotfp;
double x1, y1, z1, x2, y2, z2;
{
	double	in[6];
	unsigned char	out[6*8+1];

	in[0] = x1;
	in[1] = y1;
	in[2] = z1;
	in[3] = x2;
	in[4] = y2;
	in[5] = z2;
	htond( &out[1], (unsigned char *)in, 6 );

	out[0] = 'W';
	fwrite( out, 1, 6*8+1, plotfp );
}

void
pdv_3point( plotfp, pt )
register FILE *plotfp;
const vect_t	pt;
{
	unsigned char	out[3*8+1];

	htond( &out[1], (unsigned char *)pt, 3 );

	out[0] = 'X';
	fwrite( out, 1, 3*8+1, plotfp );
}

void
pd_3point( plotfp, x, y, z )
register FILE *plotfp;
double x, y, z;
{
	double	in[3];
	unsigned char	out[3*8+1];

	in[0] = x;
	in[1] = y;
	in[2] = z;
	htond( &out[1], (unsigned char *)in, 3 );

	out[0] = 'X';
	fwrite( out, 1, 3*8+1, plotfp );
}

void
pdv_3move( plotfp, pt )
register FILE *plotfp;
const vect_t	pt;
{
	unsigned char	out[3*8+1];

	htond( &out[1], (unsigned char *)pt, 3 );

	out[0] = 'O';
	fwrite( out, 1, 3*8+1, plotfp );
}

void
pd_3move( plotfp, x, y, z )
register FILE *plotfp;
double x, y, z;
{
	double	in[3];
	unsigned char	out[3*8+1];

	in[0] = x;
	in[1] = y;
	in[2] = z;
	htond( &out[1], (unsigned char *)in, 3 );

	out[0] = 'O';
	fwrite( out, 1, 3*8+1, plotfp );
}

void
pdv_3cont( plotfp, pt )
register FILE *plotfp;
const vect_t	pt;
{
	unsigned char	out[3*8+1];

	htond( &out[1], (unsigned char *)pt, 3 );

	out[0] = 'Q';
	fwrite( out, 1, 3*8+1, plotfp );
}

void
pd_3cont( plotfp, x, y, z )
register FILE *plotfp;
double x, y, z;
{
	double	in[3];
	unsigned char	out[3*8+1];

	in[0] = x;
	in[1] = y;
	in[2] = z;
	htond( &out[1], (unsigned char *)in, 3 );

	out[0] = 'Q';
	fwrite( out, 1, 3*8+1, plotfp );
}

void
pdv_3line( plotfp, a, b )
register FILE *plotfp;
const vect_t	a, b;
{
	unsigned char	out[6*8+1];

	htond( &out[1], (unsigned char *)a, 3 );
	htond( &out[3*8+1], (unsigned char *)b, 3 );

	out[0] = 'V';
	fwrite( out, 1, 6*8+1, plotfp );
}

void
pd_3line( plotfp, x1, y1, z1, x2, y2, z2 )
register FILE *plotfp;
double x1, y1, z1, x2, y2, z2;
{
	double	in[6];
	unsigned char	out[6*8+1];

	in[0] = x1;
	in[1] = y1;
	in[2] = z1;
	in[3] = x2;
	in[4] = y2;
	in[5] = z2;
	htond( &out[1], (unsigned char *)in, 6 );

	out[0] = 'V';
	fwrite( out, 1, 6*8+1, plotfp );
}

void
pdv_3box( plotfp, a, b )
register FILE *plotfp;
const vect_t	a, b;
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
pd_3box( plotfp, x1, y1, z1, x2, y2, z2 )
register FILE *plotfp;
double x1, y1, z1, x2, y2, z2;
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

/*
 *  Draw a ray
 */
void
pdv_3ray( fp, pt, dir, t )
FILE		*fp;
const point_t	pt;
const vect_t	dir;
double		t;
{
	point_t	tip;

	VJOIN1( tip, pt, t, dir );
	pdv_3move( fp, pt );
	pdv_3cont( fp, tip );
}
