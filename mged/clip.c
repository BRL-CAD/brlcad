/*	SCCSID	%W%	%E%	*/
/*
 *			G E D 1 0 . C
 *
 * Functions -
 *	clip	clip a line segment against the size of the display
 *
 * Author -
 *	This module written by Doug Kingston, 14 October 81
 *	Based on the clipping routine in "Principles of Computer
 *	Graphics" by Newman and Sproull, 1973, McGraw/Hill.
 *
 *			R E V I S I O N   H I S T O R Y
 *
 *	05/27/83  MJM	Adapted code to run on VAX;  numerous cleanups.
 *
 *	09-Sep-83 DAG	Overhauled.
 */

static int	code();

int
clip (xp1, yp1, xp2, yp2)
float *xp1, *yp1, *xp2, *yp2;
{
	char code1, code2;

	code1 = code (*xp1, *yp1);
	code2 = code (*xp2, *yp2);

	while (code1 || code2) {
		if (code1 & code2)
			return (-1);	/* No part is visible */

		/* SWAP codes, X's, and Y's */
		if (code1 == 0) {
			char ctemp;
			float temp;

			ctemp = code1;
			code1 = code2;
			code2 = ctemp;

			temp = *xp1;
			*xp1 = *xp2;
			*xp2 = temp;

			temp = *yp1;
			*yp1 = *yp2;
			*yp2 = temp;
		}

		if (code1 & 01)  {	/* Push toward left edge */
			*yp1 = *yp1 + (*yp2-*yp1)*(-2048.0-*xp1)/(*xp2-*xp1);
			*xp1 = -2048.0;
		}
		else if (code1 & 02)  {	/* Push toward right edge */
			*yp1 = *yp1 + (*yp2-*yp1)*(2047.0-*xp1)/(*xp2-*xp1);
			*xp1 = 2047.0;
		}
		else if (code1 & 04)  {	/* Push toward bottom edge */
			*xp1 = *xp1 + (*xp2-*xp1)*(-2048.0-*yp1)/(*yp2-*yp1);
			*yp1 = -2048.0;
		}
		else if (code1 & 010)  {	/* Push toward top edge */
			*xp1 = *xp1 + (*xp2-*xp1)*(2047.0-*yp1)/(*yp2-*yp1);
			*yp1 = 2047.0;
		}

		code1 = code (*xp1, *yp1);
	}

	return (0);
}

static int
code (x, y)
float x, y;
{
	int cval;

	cval = 0;
	if (x < -2048.0)
		cval |= 01;
	else if (x > 2047.0)
		cval |= 02;

	if (y < -2048.0)
		cval |= 04;
	else if (y > 2047.0)
		cval |= 010;

	return (cval);
}
