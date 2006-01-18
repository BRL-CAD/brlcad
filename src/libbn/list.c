/*                          L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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

/** \addtogroup libbn */
/*@{*/
/** @file ./libbn/list.c
 *  NOTE that tp_2list() and tp_3list() are good candidates to become
 *  intrinsic parts of plot3.c, for efficiency reasons.
 *
 *  Author -
 *	Michael John Muuss
 *	August 04, 1978
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "plot3.h"

/* Modes for internal flag */
#define	TP_MARK		1		/* Draw marks */
#define	TP_LINE		2		/* Draw lines */

/*
 *			T P _ I 2 L I S T
 *
 *  Take a set of x,y coordinates, and plot them as a
 *  polyline, ie, connect them with line segments.
 *  For markers, use tp_mlist(), below.
 *  This "C" interface expects arrays of INTs.
 */
void
tp_i2list(register FILE *fp, register int *x, register int *y, register int npoints)

            	   			/* array of points */
            	   			/* array of points */

{
	if( npoints <= 0 )
		return;

	pl_move( fp, *x++, *y++ );
	while( --npoints > 0 )
		pl_cont( fp, *x++, *y++ );
}

/*
 *			T P _ 2 L I S T
 *
 *  Take a set of x,y coordinates, and plot them as a
 *  polyline, ie, connect them with line segments.
 *  For markers, use tp_mlist(), below.
 *  This "C" interface expects arrays of DOUBLES.
 */
void
tp_2list(register FILE *fp, register double *x, register double *y, register int npoints)

               	   			/* array of points */
               	   			/* array of points */

{
	if( npoints <= 0 )
		return;

	pd_move( fp, *x++, *y++ );
	while( --npoints > 0 )
		pd_cont( fp, *x++, *y++ );
}

void
PL_FORTRAN(f2list, F2LIST)( fpp, x, y, n )
FILE		**fpp;
register float	*x;
register float	*y;
int		*n;
{
	register int npoints = *n-1;	/* FORTRAN uses 1-based subscripts */
	register FILE	*fp = *fpp;

	if( npoints <= 0 )
		return;

	pd_move( fp, *x++, *y++ );
	while( --npoints > 0 )
		pd_cont( fp, *x++, *y++ );
}

/*
 *			T P _ 3 L I S T
 */
void
tp_3list(FILE *fp, register double *x, register double *y, register double *z, register int npoints)
{
	if( npoints <= 0 )
		return;

	pd_3move( fp, *x++, *y++, *z++ );
	while( --npoints > 0 )
		pd_3cont( fp, *x++, *y++, *z++ );
}

void
PL_FORTRAN(f3list, F3LIST)( fpp, x, y, z, n )
FILE		**fpp;
register float	*x;
register float	*y;
register float	*z;
int		*n;
{
	register int npoints = *n-1;	/* FORTRAN uses 1-based subscripts */
	register FILE	*fp = *fpp;

	if( npoints <= 0 )
		return;

	pd_3move( fp, *x++, *y++, *z++ );
	while( --npoints > 0 )
		pd_3cont( fp, *x++, *y++, *z++ );
}

/*
 *			T P _ 2 M L I S T
 *
 *  Take a set of x,y co-ordinates and plots them,
 *  with a combination of connecting lines and/or place markers.
 *  It is important to note that the arrays
 *  are arrays of doubles, and express UNIX-plot coordinates in the
 *  current pl_space().
 *
 *  tp_scale(TIG) may be called first to optionally re-scale the data.
 *
 *  The 'mark' character to be used for marking points off can be any
 *  printing ASCII character, or 001 to 005 for the special marker characters.
 *
 *  In addition, the value of the 'flag' variable determines the type
 *  of line to be drawn, as follows:
 *
 *	0	Draw nothing (rather silly)
 *	1	Marks only, no connecting lines.  Suggested interval=1.
 *	2	Draw connecting lines only.
 *	3	Draw line and marks
 */
void
tp_2mlist(FILE *fp, register double *x, register double *y, int npoints, int flag, int mark, int interval, double size)


               	   			/* arrays of points */

   		     			/* TP_MARK|TP_LINE */
   		     			/* marker character to use */
   		         		/* marker drawn every N points */
      		     			/* marker size */
{
	register int i;			/* index variable */
	register int counter;		/* interval counter */

	if( npoints <= 0 )
		return;

	if( flag & TP_LINE )
		tp_2list( fp, x, y, npoints );
	if( flag & TP_MARK )  {
		tp_2marker( fp, mark, *x++, *y++, size );
		counter = 1;		/* Already plotted one */
		for( i=1; i<npoints; i++ )  {
			if( counter >= interval )  {
				tp_2marker( fp, mark, *x, *y, size );
				counter = 0;	/* We made a mark */
			}
			x++; y++;
			counter++;		/* One more point done */
		}
	}
}

/*
 *  This FORTRAN interface expects arrays of REALs (single precision).
 */
void
PL_FORTRAN(f2mlst, F2MLST)( fp, x, y, np, flag, mark, interval, size )
FILE	**fp;
float	*x;
float	*y;
int	*np;
int	*flag;		/* indicates user's mode request */
int	*mark;
int	*interval;
float	*size;
{
	register int i;			/* index variable */
	register int counter;		/* interval counter */
	register int npoints = *np-1;

	if( npoints <= 0 )
		return;

	if( *flag & TP_LINE )
		PL_FORTRAN(f2list,F2LIST)( fp, x, y, np );
	if( *flag & TP_MARK )  {
		tp_2marker( *fp, *mark, *x++, *y++, *size );
		counter = 1;			/* We already plotted one */
		for( i=1; i<npoints; i++ )  {
			if( counter >= *interval )  {
				tp_2marker( *fp, *mark, *x, *y, *size );
				counter = 0;	/* Made a mark */
			}
			x++; y++;
			counter++;		/* One more point done */
		}
	}
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
