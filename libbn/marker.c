/*
 *			T P _ M A R K E R
 *
 *	Terminal Independant Graphics Display Package
 *		Mike Muuss  August 04, 1978
 *
 *	This routine places a specified character (either from
 * the ASCII set, or one of the 5 special marker characters)
 * centered about the current pen position (instead of above & to
 * the right of the current position, as characters usually go).
 * Calling sequence:
 *
 *	char c		is the character to be used for a marker,
 *			or one of the following special markers -
 *				1 = plus
 *				2 = an "x"
 *				3 = a triangle
 *				4 = a square
 *				5 = an hourglass
 */
#include <stdio.h>

tp_marker( fp, c, x, y, scale )
FILE	*fp;
register int c;
double	x, y;
double	scale;
{
	static char *mark_str = "x";

	mark_str[0] = (char)c;

	/* Draw the marker */
	tp_symbol( fp, mark_str,
		(x - scale*0.5), (y - scale*0.5),
		scale, 0.0 );
}


/*
 *  FORTRAN-IV Interface Entry
 */

fmarker( fp, c, x, y, scale )
FILE	**fp;
char	*c;
double	*x, *y;
double	*scale;
{
	tp_marker( *fp, *c, *x, *y, *scale );
}
