/*
 *			T P _ A X I S
 *
 *	Terminal Independant Graphics Display Package
 *		Mike Muuss  August 01, 1978
 *
 *	This routine is used to generate an axis for a graph.
 * It draws an axis with a linear scale, places tic marks every inch,
 * labels the tics, and uses the supplied title for the axis.
 *
 *	The strategy behind this routine is to split the axis
 * into SEGMENTS, which run from one tick to the next.  The
 * origin of the first segment (x,y), the origin of the bottom
 * of the first tick (xbott,ybott), and the origin of the first
 * tick label (xnum,ynum) are computed along with the delta x
 * and delta y (xincr,yincr) which describes the interval to
 * the start of the next tick.
 */
#include <stdio.h>
#include <math.h>

#define	TICK_YLEN	(cscale)	/* tick is 1 character height */
#define	NUM_YOFF	(3*cscale)	/* numbers offset from line */
#define	TITLE_YOFF	(5*cscale)	/* title offset from line */

/* Rotation macros */
#define	X(a,b)	((a)*xrot - (b)*yrot)
#define Y(a,b)	((a)*yrot + (b)*xrot)
#define	ROT(a,b)	temp=X(a-x,b-y) + x;\
			b=Y(a-x,b-y) + y;\
			a=temp


tp_axis( fp, string, x, y, length, theta, ndigits, minval, incr, unit, cscale )
FILE		*fp;
char		*string;	/* label for axis */
register int	x,y;		/* start coordinates for axis */
int		length;		/* length of axis */
double		theta;		/* rotation off X-axis, in degrees */
int		ndigits;	/* # digits wide */
double		minval;		/* minimum value on axis */
double		incr;		/* increment for each tick */
int		unit;		/* distance between ticks */
double		cscale;		/* character scale (size) */
{
	register int i;				/* index  variable */
	double	xrot, yrot;			/* direction & rotation */
	int	temp;				/* Work area for ROT macro */
	int	xincr, yincr;			/* increments for rotation */
	int	xbott, ybott;			/* address of bottom of tick */
	int	xnum, ynum;			/* addr of ticks number */
	int	xtitle, ytitle;			/* addr of axis title */
	int	direction;			/* 1=clockwise, -1=counter */
	int	n;
	int	label_width;
	int	space;
	int	xlast, ylast;			/* last number pos */

	/* Determine direction for ticks */
	direction = 1;				/* normal is clockwise ticks */
	if( length < 0 )  {
		direction = (-1);		/* he wants counterclockwise */
		length = (-length);
	}

	xrot = cos( 0.0174533 * theta );
	yrot = sin( 0.0174533 * theta );

	/* Compute the bottom of the first tick */
	xbott = x;
	ybott = y - TICK_YLEN * direction;
	ROT( xbott, ybott );

	/* Compute the start of this tick's label */
	xnum = x;
	ynum = y - NUM_YOFF * direction;
	ROT( xnum, ynum );

	/* Center the title */
	xtitle = x + ( length - strlen( string ) * cscale ) / 2;
	ytitle = y - TITLE_YOFF * direction;
	ROT( xtitle, ytitle );

	/* Determine the increment between ticks */
	xincr = X( unit, 0 );
	yincr = Y( unit, 0 );

	/* Draw the title */
	tp_symbol(fp, string, xtitle, ytitle, cscale, theta);

	length = (length+unit-1)/unit;		/* number of ticks to do */

	/*
	 *  Draw the axis & label as we go.
	 *  Do left-most tick, then repeat:
	 *  across, down, label.
	 */
	pl_move( fp, xbott, ybott );
	pl_cont( fp, x, y );

	/* initial label */
	if( ndigits > 0 )
		tp_number( fp, minval, xnum, ynum, cscale, theta, ndigits );
	xlast = ndigits * cscale;
	ylast = ndigits * cscale;
	ROT( xlast, ylast );
	xlast += xnum;
	ylast += ynum;

	for( i=0; i<length; i++) {
		pl_move( fp, x, y );

		/* advance x & y for next segment */
		x += xincr;
		xbott += xincr;
		xnum += xincr;
		y += yincr;
		ybott += yincr;
		ynum += yincr;

		minval += incr;

		pl_cont( fp, x, y );		/* draw segment */

		/*
		 *  Draw and label this tick if it is beyond
		 *  the last label.
		 */
		if( ( (xincr >= 0) ? (x <= xlast) : (x >= xlast) ) ||
		    ( (yincr >= 0) ? (y <= ylast) : (y >= ylast) ) )
			continue;

		pl_cont( fp, xbott, ybott );	/* draw tick */
		if( ndigits > 0 )
			tp_number( fp, minval, xnum, ynum, cscale, theta, ndigits );
	}
}
