/*
 *			F O R T R A N . C
 *
 *  A FORTRAN-callable interface to libplot3, which is
 *  a public-domain UNIX plot library, for 2-D and 3-D plotting in
 *  16-bit signed integer spaces, and in floating point.
 *
 *  Note that all routines which expect floating point parameters
 *  currently expect them to be of
 *  type "float" (single precision) so that all FORTRAN constants
 *  can be written normally, rather than having to insist on
 *  FORTRAN "double precision" parameters.
 *  This is at odds with the C routines and the meta-file format,
 *  which both operate in "C double" precision.
 *
 *  Note that on machines like the Cray,
 *	(C float == C double == FORTRAN REAL) != FORTRAN DOUBLE PRECISION
 *
 *  Also note that on the Cray, the only interface provision required
 *  is that the subroutine name be in all upper case.  Other systems
 *  may have different requirements, such as adding a leading underscore.
 *  It is not clear how to handle this in a general way.
 *
 *  Note that due to the 6-character name space required to be
 *  generally useful in the FORTRAN environment, the names have been
 *  shortened.  At the same time, a consistency of naming has been
 *  implemented;  the first character or two giving a clue as to
 *  the purpose of the subroutine:
 *
 *	I	General routines, and integer-parameter routines
 *	I2	Routines with enumerated 2-D integer parameters
 *	I3	Routines with enumerated 3-D integer parameters
 *	F2	Routines with enumerated 2-D float parameters
 *	F3	Routines with enumerated 3-D float parameters
 *	A3	Routines with arrays of 3-D float parameters
 *
 *  This name space leaves the door open for a double-precision
 *  family of routines, D, D2, and D3.
 *
 *
 *  Author -
 *	Mike Muuss
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

#include "machine.h"

/*
 *			P L _ S T R N C P Y
 *
 *  Make null-terminated copy of a string in output buffer,
 *  being careful not to exceed indicated buffer size
 *  Accept "$" as alternate string-terminator for FORTRAN Holerith constants,
 *  because getting FORTRAN to null-terminate strings is to painful
 *  (and non-portable) to contemplate.
 */
void
pl_strncpy( out, in, sz )
register char *out;
register char *in;
register int sz;
{
	register int c;

	while( --sz > 0 && (c = *in++) != '\0' && c != '$' )
		*out++ = c;
	*out++ = '\0';
}

/*
 *  These interfaces provide necessary access to C library routines
 *  from the FORTRAN environment
 */

void
IFDOPN( plotfp, fd )
FILE	**plotfp;
int	*fd;
{
	if( (*plotfp = fdopen(*fd, "w")) == NULL )
		perror("IFDOPN/fdopen");
}

void
IFOPEN( plotfp, name )
FILE	**plotfp;
char	*name;
{
	char	buf[128];

	pl_strncpy( buf, name, sizeof(buf) );
	if( (*plotfp = fopen(buf, "w")) == NULL )
		perror(buf);
}

/*
 *  These interfaces provide the standard UNIX-Plot functionality
 */

void
I2PNT( plotfp, x, y )
FILE	**plotfp;
int	*x, *y;
{
	pl_point( *plotfp, *x, *y );
}

void
I2LINE( plotfp, x1, y1, x2, y2 )
FILE	**plotfp;
int	*x1, *y1, *x2, *y2;
{
	pl_line( *plotfp, *x1, *y1, *x2, *y2 );
}

void
ILINMD( plotfp, s )
FILE	**plotfp;
char *s;
{
	char buf[32];
	pl_strncpy( buf, s, sizeof(buf) );
	pl_linmod( *plotfp, buf );
}

void
I2MOVE( plotfp, x, y )
FILE	**plotfp;
int	*x, *y;
{
	pl_move( *plotfp, *x, *y );
}

void
I2CONT( plotfp, x, y )
FILE	**plotfp;
int	*x, *y;
{
	pl_cont( *plotfp, *x, *y );
}

void
I2LABL( plotfp, s )
FILE	**plotfp;
char *s;
{
	char	buf[256];
	pl_strncpy( buf, s, sizeof(buf) );
	pl_label( *plotfp, buf );
}

void
I2SPAC( plotfp, x1, y1, x2, y2 )
FILE	**plotfp;
int	*x1, *y1, *x2, *y2;
{
	pl_space( *plotfp, *x1, *y1, *x2, *y2 );
}

void
IERASE( plotfp )
FILE	**plotfp;
{
	pl_erase( *plotfp );
}

void
I2CIRC( plotfp, x, y, r )
FILE	**plotfp;
int	*x, *y, *r;
{
	pl_circle( *plotfp, *x, *y, *r );
}

void
I2ARC( plotfp, xc, yc, x1, y1, x2, y2 )
FILE	**plotfp;
int	*xc, *yc, *x1, *y1, *x2, *y2;
{
	pl_arc( *plotfp, *xc, *yc, *x1, *y1, *x2, *y2 );
}

void
I2BOX( plotfp, x1, y1, x2, y2 )
FILE	**plotfp;
int	*x1, *y1, *x2, *y2;
{
	pl_box( *plotfp, *x1, *y1, *x2, *y2 );
}

/*
 * Here lie the BRL 3-D extensions.
 */

/* Warning: r, g, b are ints.  The output is chars. */
void
ICOLOR( plotfp, r, g, b )
FILE	**plotfp;
int	*r, *g, *b;
{
	pl_color( *plotfp, *r, *g, *b );
}

void
IFLUSH( plotfp )
FILE	**plotfp;
{
	pl_flush( *plotfp );
}

void
I3SPAC( plotfp, x1, y1, z1, x2, y2, z2 )
FILE	**plotfp;
int	*x1, *y1, *z1, *x2, *y2, *z2;
{
	pl_3space( *plotfp, *x1, *y1, *z1, *x2, *y2, *z2 );
}

void
I3PNT( plotfp, x, y, z )
FILE	**plotfp;
int	*x, *y, *z;
{
	pl_3point( *plotfp, *x, *y, *z );

}

void
I3MOVE( plotfp, x, y, z )
FILE	**plotfp;
int	*x, *y, *z;
{
	pl_3move( *plotfp, *x, *y, *z );
}

void
I3CONT( plotfp, x, y, z )
FILE	**plotfp;
int	*x, *y, *z;
{
	pl_3cont( *plotfp, *x, *y, *z );
}

void
I3LINE( plotfp, x1, y1, z1, x2, y2, z2 )
FILE	**plotfp;
int	*x1, *y1, *z1, *x2, *y2, *z2;
{
	pl_3line( *plotfp, *x1, *y1, *z1, *x2, *y2, *z2 );
}

void
I3BOX( plotfp, x1, y1, z1, x2, y2, z2 )
FILE	**plotfp;
int	*x1, *y1, *z1, *x2, *y2, *z2;
{
	pl_3box( *plotfp, *x1, *y1, *z1, *x2, *y2, *z2 );
}

/*
 *  Floating point routines.
 *
 *
 */

void
F2PNT( plotfp, x, y )
FILE	**plotfp;
float	*x, *y;
{
	pd_point( *plotfp, *x, *y );
}

void
F2LINE( plotfp, x1, y1, x2, y2 )
FILE	**plotfp;
float	*x1, *y1, *x2, *y2;
{
	pd_line( *plotfp, *x1, *y1, *x2, *y2 );
}

void
F2MOVE( plotfp, x, y )
FILE	**plotfp;
float	*x, *y;
{
	pd_move( *plotfp, *x, *y );
}

void
F2CONT( plotfp, x, y )
FILE	**plotfp;
float	*x, *y;
{
	pd_cont( *plotfp, *x, *y );
}

void
F2SPAC( plotfp, x1, y1, x2, y2 )
FILE	**plotfp;
float	*x1, *y1, *x2, *y2;
{
	pd_space( *plotfp, *x1, *y1, *x2, *y2 );
}

void
F2CIRC( plotfp, x, y, r )
FILE	**plotfp;
float	*x, *y, *r;
{
	pd_circle( *plotfp, *x, *y, *r );
}

void
F2ARC( plotfp, xc, yc, x1, y1, x2, y2 )
FILE	**plotfp;
float	*xc, *yc, *x1, *y1, *x2, *y2;
{
	pd_arc( *plotfp, *xc, *yc, *x1, *y1, *x2, *y2 );
}

void
F2BOX( plotfp, x1, y1, x2, y2 )
FILE	**plotfp;
float	*x1, *y1, *x2, *y2;
{
	pd_box( *plotfp, *x1, *y1, *x2, *y2 );
}

/*
 *  Floating-point 3-D, both in array (vector) and enumerated versions.
 *  The same remarks about float/double apply as above.
 */

void
A3SPAC( plotfp, min, max )
FILE	**plotfp;
float	min[3];
float	max[3];
{
	pd_3space( *plotfp, min[0], min[1], min[2], max[0], max[1], max[2] );
}

void
F3SPAC( plotfp, x1, y1, z1, x2, y2, z2 )
FILE	**plotfp;
float	*x1, *y1, *z1, *x2, *y2, *z2;
{
	pd_3space( *plotfp, *x1, *y1, *z1, *x2, *y2, *z2 );
}

void
A3PNT( plotfp, pt )
FILE	**plotfp;
float	pt[3];
{
	pd_3point( *plotfp, pt[0], pt[1], pt[2] );
}

void
F3PNT( plotfp, x, y, z )
FILE	**plotfp;
float	*x, *y, *z;
{
	pd_3point( *plotfp, *x, *y, *z );
}

void
A3MOVE( plotfp, pt )
FILE	**plotfp;
float	pt[3];
{
	pd_3move( *plotfp, pt[0], pt[1], pt[2] );
}

void
F3MOVE( plotfp, x, y, z )
FILE	**plotfp;
float	*x, *y, *z;
{
	pd_3move( *plotfp, *x, *y, *z );
}

void
A3CONT( plotfp, pt )
FILE	**plotfp;
float	pt[3];
{
	pd_3cont( *plotfp, pt[0], pt[1], pt[2] );
}

void
F3CONT( plotfp, x, y, z )
FILE	**plotfp;
float	*x, *y, *z;
{
	pd_3cont( *plotfp, *x, *y, *z );
}

void
A3LINE( plotfp, a, b )
FILE	**plotfp;
float	a[3], b[3];
{
	pd_3line( *plotfp, a[0], a[1], a[2], b[0], b[1], b[2] );
}

void
F3LINE( plotfp, x1, y1, z1, x2, y2, z2 )
FILE	**plotfp;
float	*x1, *y1, *z1, *x2, *y2, *z2;
{
	pd_3line( *plotfp, *x1, *y1, *z1, *x2, *y2, *z2 );
}

void
A3BOX( plotfp, a, b )
FILE	**plotfp;
float	a[3], b[3];
{
	pd_3box( *plotfp, a[0], a[1], a[2], b[0], b[1], b[2] );
}

void
F3BOX( plotfp, x1, y1, z1, x2, y2, z2 )
FILE	**plotfp;
float	*x1, *y1, *z1, *x2, *y2, *z2;
{
	pd_3box( *plotfp, *x1, *y1, *z1, *x2, *y2, *z2 );
}
