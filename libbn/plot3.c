/*
 *			L I B P L O T 3 . C
 *
 * A public-domain UNIX plot library, for 2-D and 3-D plotting
 * in 16-bit signed integer spaces.
 *
 * These routines generate "UNIX plot" output (with the addition
 * of 3-D commands).  They behave almost exactly like the regular
 * libplot routines, except:
 *
 *  (1) These all take a stdio file pointer, and can thus be used to
 *      create multiple plot files simultaneously.
 *  (2) There are 3-D versions of most commands.
 *  (3) The names have been changed.
 *
 * The 3-D extensions are those of Doug Gwyn, from his System V extensions.
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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

/* For the sake of efficiency, we trust putc() to write only one byte */
/*#define putsi(a)	putc(a&0377,plotfp); putc((a>>8)&0377,plotfp)*/
#define putsi(a)	putc(a,plotfp); putc((a>>8),plotfp)

pl_point( plotfp, x, y )
register FILE *plotfp;
int x, y;
{
	putc( 'p', plotfp );
	putsi( x );
	putsi( y );
}

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

pl_linmod( plotfp, s )
register FILE *plotfp;
register char *s;
{
	putc( 'f', plotfp );
	while( *s != NULL )
		putc( *s++, plotfp );
	putc( '\n', plotfp );
}

pl_move( plotfp, x, y )
register FILE *plotfp;
int x, y;
{
	putc( 'm', plotfp );
	putsi( x );
	putsi( y );
}

pl_cont( plotfp, x, y )
register FILE *plotfp;
int x, y;
{
	putc( 'n', plotfp );
	putsi( x );
	putsi( y );
}

pl_label( plotfp, s )
register FILE *plotfp;
register char *s;
{
	putc( 't', plotfp );
	while( *s != NULL )
		putc( *s++, plotfp );
	putc( '\n', plotfp );
}

pl_space( plotfp, x1, y1, x2, y2 )
FILE *plotfp;
int x1, y1, x2, y2;
{
	putc( 's', plotfp );
	putsi( x1 );
	putsi( y1 );
	putsi( x2 );
	putsi( y2 );
}

pl_erase( plotfp )
FILE *plotfp;
{
	putc( 'e', plotfp );
}

pl_circle( plotfp, x, y, r )
FILE *plotfp;
int x, y, r;
{
	putc( 'c', plotfp );
	putsi( x );
	putsi( y );
	putsi( r );
}

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

/*
 * Here lie the BRL 3-D extensions.
 */

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

pl_3point( plotfp, x, y, z )
register FILE *plotfp;
int x, y, z;
{
	putc( 'P', plotfp );
	putsi( x );
	putsi( y );
	putsi( z );
}

pl_3move( plotfp, x, y, z )
register FILE *plotfp;
int x, y, z;
{
	putc( 'M', plotfp );
	putsi( x );
	putsi( y );
	putsi( z );
}

pl_3cont( plotfp, x, y, z )
register FILE *plotfp;
int x, y, z;
{
	putc( 'N', plotfp );
	putsi( x );
	putsi( y );
	putsi( z );
}

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

/*
 * XXX Warning: r, g, b are ints.  The output is chars.
 */
pl_color( plotfp, r, g, b )
register FILE *plotfp;
int r, g, b;
{
	putc( 'C', plotfp );
	putc( r, plotfp );
	putc( g, plotfp );
	putc( b, plotfp );
}

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
