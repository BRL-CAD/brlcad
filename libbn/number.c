/*
 *			T P _ N U M B E R
 *
 *
 *	Terminal Independant Graphics Display Package
 *		Mike Muuss  August 01, 1978
 *
 *	This routine is used to convert a floating point number into
 * a string of ASCII characters, and then call SYMBOL(TIG) to plot
 * the number with the specified parameters  onto the output plot.
 */
#include <stdio.h>
#include <math.h>

tp_number( fp, input, x, y, cscale, theta, digits )
FILE	*fp;
double	input;		/* number to be plotted */
int	x, y;		/* first char position */
int	cscale;		/* char scale */
double	theta;		/* degrees ccw from X-axis */
int	digits;		/* # digits wide */
{
	char	fmt[32];
	char	buf[32];

	if( digits < 1 )
		digits = 1;
	sprintf( fmt, "%%%dg", digits, digits );
	sprintf( buf, fmt, input );
	tp_symbol( fp, buf, x, y, cscale, theta );
}


/*
 *	FORTRAN-IV Interface
 */
fnumber( fp, input, x, y, cscale, theta, digits )
FILE	*fp;
float	*input;
int	*x, *y;
float	*cscale;
float	*theta;
int     *digits;
{
	tp_number(fp, *input, *x, *y, *cscale, *theta, *digits);
}
