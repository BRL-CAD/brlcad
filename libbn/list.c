/*
 *			L I N E . C
 *
 *	Terminal Independant Graphics Display Package
 *		Mike Muuss  August 04, 1978
 *
 */
#include <stdio.h>

/* Modes for internal flag */
#define	DO_MARK		1		/* Draw marks */
#define	DO_LINE		2		/* Draw lines */


/*
 *			T P _ L I N E
 * 
 *  Take a set of x,y coordinates, and plot them as a
 *  polyline, ie, connect them with line segments.
 *  For markers, use tp_mline(), below.
 *  This "C" interface expects arrays of DOUBLES.
 */
tp_line( fp, x, y, npoints )
FILE		*fp;
register double	*x, *y;			/* arrays of points */
register int	npoints;
{
	if( npoints <= 0 )
		return;

	pd_move( fp, *x++, *y++ );
	while( --npoints > 0 )
		pd_cont( fp, *x++, *y++ );
}

/*
 *  This FORTRAN interface expects arrays of REALs (single precision).
 */
FLINE( fpp, x, y, n )
FILE	**fpp;
float	*x, *y;
int	*n;
{
	FILE	*fp = *fpp;
	register int npoints = *n;

	if( npoints <= 0 )
		return;

	pd_move( fp, *x++, *y++ );
	while( --npoints > 0 )
		pd_cont( fp, *x++, *y++ );
}

/*
 *			T P _ M L I N E
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
 *  printing character, or 001 to 005 for the special marker characters.
 *  In addition, the sign of the mark character determines the type
 *  of line to be drawn, as follows:
 *	-	Marks only, no connecting lines.  Suggested interval=1.
 *	0	Draw connecting lines only.
 *	+	Draw line and marks
 */
tp_mline( fp, x, y, npoints, mark, interval, scale )
FILE		*fp;
register double	*x, *y;			/* arrays of points */
int		npoints;
int		mark;			/* marker character to use */
int		interval;		/* marker drawn every N points */
double		scale;			/* marker scale */
{
	register int flag;		/* indicates user's mode request */
	register int i;			/* index variable */
	register int counter;		/* interval counter */

	if( npoints <= 0 )
		return;

	/* Determine line drawing mode */
	if( mark < 0 )  {
		flag = DO_MARK;
		mark = (-mark);
	} else {
		if( mark > 0 )
			flag = DO_MARK | DO_LINE;
		else
			flag = DO_LINE;
	}

	if( flag & DO_MARK )  {
		pd_move( fp, *x, *y );
		tp_marker( fp, mark, *x, *y, scale );
	}

	x++, y++;
	counter = 1;			/* We already plotted one */
	for( i=1; i<npoints; i++ )  {
		if( flag & DO_LINE )
			pd_line( fp, x[-1], y[-1], *x, *y );

		if( flag & DO_MARK )  {
			if( counter >= interval )  {
				tp_marker( fp, mark, *x, *y, scale );
				counter = 0;	/* We made a mark */
			}
			counter++;		/* One more point done */
		}
	}
}

/*
 *  This FORTRAN interface expects arrays of REALs (single precision).
 */
FMLINE( fp, x, y, npoints, mark, interval, scale )
FILE	*fp;
double	*x, *y;
int	*npoints, *mark, *interval;
float	*scale;
{
	register int flag;		/* indicates user's mode request */
	register int i;			/* index variable */
	register int counter;		/* interval counter */
	int mk;

	if( npoints <= 0 )
		return;

	/* Determine line drawing mode */
	if( (mk = *mark) < 0 )  {
		flag = DO_MARK;
		mk = (-mk);
	} else if( mark > 0 ) {
		flag = DO_MARK | DO_LINE;
	} else {
		flag = DO_LINE;
	}

	if( flag & DO_MARK )  {
		pd_move( fp, *x, *y );
		tp_marker( fp, mk, *x, *y, scale );
	}

	x++, y++;
	counter = 1;			/* We already plotted one */
	for( i=1; i<*npoints; i++ )  {
		if( flag & DO_LINE )
			pd_line( fp, x[-1], y[-1], *x, *y );

		if( flag & DO_MARK )  {
			if( counter >= *interval )  {
				tp_marker( fp, mk, *x, *y, scale );
				counter = 0;	/* Made a mark */
			}
			counter++;		/* One more point done */
		}
	}
}
