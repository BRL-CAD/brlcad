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
 *  Take a set of integer x,y coordinates, and plot them as a
 *  polyline, ie, connect them with line segments.  For markers,
 *  use tp_mline(), below.
 */
tp_line( fp, x, y, npoints )
FILE		*fp;
register int	*x, *y;			/* arrays of integer points */
int		npoints;		/* numbers of points */
{
	register int i;

	if( npoints <= 0 )
		return;

	pl_move( fp, *x++, *y++ );
	for( i=1; i<npoints; i++ )
		pl_cont( fp, *x++, *y++ );
}

/*
 *			T P _ M L I N E
 *
 *  Take a set of integer x,y co-ordinates and plots them,
 *  with a combination of connecting lines and/or place markers.
 *  It is important to note that the arrays
 *  are arrays of integers, and express UNIX-plot coordinates in the
 *  current pl_space(). For most uses, SCALE(TIG) should be called first to
 *  appropriately scale the data.
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
register int	*x, *y;			/* arrays of integer points */
int		npoints;		/* numbers of points */
int		mark;			/* marker characters */
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
		pl_move( fp, *x, *y );
		tp_marker( fp, mark, *x, *y, scale );
	}

	x++, y++;
	counter = 1;			/* We already plotted one */
	for( i=1; i<npoints; i++ )  {
		if( flag & DO_LINE )
			pl_line( fp, x[-1], y[-1], *x, *y );

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
 *	FORTRAN-IV Interface Entries
 */

fline( fp, x, y, npoints )
FILE	*fp;
int	*x, *y, *npoints;
{
	/* Need to offset array by 1 for fortran 1-based arrays? */
	tp_line( fp, x, y, *npoints );
}

fmline( fp, x, y, npoints, mark, interval, scale )
FILE	*fp;
int	*x, *y, *npoints, *mark, *interval;
float	*scale;
{
	tp_mline( fp, x, y, *npoints, *mark, *interval, *scale );
}
