/*
 *			A D C . C
 *
 * Functions -
 *	adcursor	implement the angle/distance cursor
 *
 * Authors -
 *	Gary Steven Moss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <math.h>
#include "./machine.h"	/* special copy */
#include "../h/vmath.h"
#include "ged.h"
#include "dm.h"

/*
 * These variables are global for the benefit of
 * the display portion of dotitles.
 */
float	curs_x;		/* cursor X position */
float	curs_y;		/* cursor Y position */
float	c_tdist;		/* Cursor tick distance */
float	angle1;		/* Angle to solid wiper */
float	angle2;		/* Angle to dashed wiper */

/*
 *			A D C U R S O R
 *
 * Compute and display the angle/distance cursor.
 */
void
adcursor( tick_dist )
{
	static double	pi = 3.14159265358979323264;
	static float	x1, Y1;	/* not "y1", due to conflict with math lib */
	static float	x2, y2;
	static float	x3, y3;
	static float	x4, y4;
	static float	d1, d2;
	static float	t1, t2;
	static long	idxy[2];

	/*
	 * Calculate a-d cursor displacement.
	 */
#define MINVAL	-2048
#define MAXVAL	 2047
	idxy[0] = dm_values.dv_xadc;
	idxy[1] = dm_values.dv_yadc;
	idxy[0] = (idxy[0] < MINVAL ? MINVAL : idxy[0]);
	idxy[0] = (idxy[0] > MAXVAL ? MAXVAL : idxy[0]);
	idxy[1] = (idxy[1] < MINVAL ? MINVAL : idxy[1]);
	idxy[1] = (idxy[1] > MAXVAL ? MAXVAL : idxy[1]);

	dmp->dmr_2d_line( MINVAL, idxy[1], MAXVAL, idxy[1], 0 ); /* Horiz */
	dmp->dmr_2d_line( idxy[0], MAXVAL, idxy[0], MINVAL, 0);  /* Vert */

	curs_x = (float) (idxy[0]);
	curs_y = (float) (idxy[1]);

	/*
	 * Calculate a-d cursor rotation.
	 */
	/* - to make rotation match knob direction */
	idxy[0] = -dm_values.dv_1adc;	/* solid line */
	idxy[1] = -dm_values.dv_2adc;	/* dashed line */
	angle1 = ((2047.0 + (float) (idxy[0])) * pi) / (4.0 * 2047.0);
	angle2 = ((2047.0 + (float) (idxy[1])) * pi) / (4.0 * 2047.0);

	/* sin for X and cos for Y to reverse sense of knob */
	d1 = cos (angle1) * 8000.0;
	d2 = sin (angle1) * 8000.0;
	x1 = curs_x + d1;
	Y1 = curs_y + d2;
	x2 = curs_x - d1;
	y2 = curs_y - d2;
	(void)clip ( &x1, &Y1, &x2, &y2 );

	x3 = curs_x + d2;
	y3 = curs_y - d1;
	x4 = curs_x - d2;
	y4 = curs_y + d1;
	(void)clip ( &x3, &y3, &x4, &y4 );

	dmp->dmr_2d_line( (int)x1, (int)Y1, (int)x2, (int)y2, 0 );
	dmp->dmr_2d_line( (int)x3, (int)y3, (int)x4, (int)y4, 0 );

	d1 = cos (angle2) * 8000.0;
	d2 = sin (angle2) * 8000.0;
	x1 = curs_x + d1;
	Y1 = curs_y + d2;
	x2 = curs_x - d1;
	y2 = curs_y - d2;
	(void)clip ( &x1, &Y1, &x2, &y2 );

	x3 = curs_x + d2;
	y3 = curs_y - d1;
	x4 = curs_x - d2;
	y4 = curs_y + d1;
	(void)clip ( &x3, &y3, &x4, &y4 );

	/* Dashed lines */
	dmp->dmr_2d_line( (int)x1, (int)Y1, (int)x2, (int)y2, 1 );
	dmp->dmr_2d_line( (int)x3, (int)y3, (int)x4, (int)y4, 1 );

	/*
	 * Position tic marks from dial 9.
	 */
	/* map -2048 - 2047 into 0 - 4096 * sqrt (2) */
	/* Tick distance */
	c_tdist = ((float)(dm_values.dv_distadc) + 2047.0) * 1.4142136;

	d1 = c_tdist * cos (angle1);
	d2 = c_tdist * sin (angle1);
	t1 = 20.0 * sin (angle1);
	t2 = 20.0 * cos (angle1);

	/* Quadrant 1 */
	x1 = curs_x + d1 + t1;
	Y1 = curs_y + d2 - t2;
	x2 = curs_x + d1 -t1;
	y2 = curs_y + d2 + t2;
	if (clip ( &x1, &Y1, &x2, &y2 ) == 0) {
		dmp->dmr_2d_line( (int)x1, (int)Y1, (int)x2, (int)y2, 0 );
	}

	/* Quadrant 2 */
	x1 = curs_x - d2 + t2;
	Y1 = curs_y + d1 + t1;
	x2 = curs_x - d2 - t2;
	y2 = curs_y + d1 - t1;
	if (clip (&x1, &Y1, &x2, &y2) == 0) {
		dmp->dmr_2d_line( (int)x1, (int)Y1, (int)x2, (int)y2, 0 );
	}

	/* Quadrant 3 */
	x1 = curs_x - d1 - t1;
	Y1 = curs_y - d2 + t2;
	x2 = curs_x - d1 + t1;
	y2 = curs_y - d2 - t2;
	if (clip (&x1, &Y1, &x2, &y2) == 0) {
		dmp->dmr_2d_line( (int)x1, (int)Y1, (int)x2, (int)y2, 0 );
	}

	/* Quadrant 4 */
	x1 = curs_x + d2 - t2;
	Y1 = curs_y - d1 - t1;
	x2 = curs_x + d2 + t2;
	y2 = curs_y - d1 + t1;
	if (clip (&x1, &Y1, &x2, &y2) == 0) {
		dmp->dmr_2d_line( (int)x1, (int)Y1, (int)x2, (int)y2, 0 );
	}
}
