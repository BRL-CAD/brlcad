/*
 *			M A R B L E . C
 *
 *  Author -
 *	Tom DiGiacinto
 *
 *  Status:  experimental
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./mathtab.h"
#include "./rdebug.h"

struct	marble_specific  {
	vect_t	mar_min;
	vect_t	mar_max;
};
#define MARB_NULL	((struct marble_specific *)0)
#define MARB_O(m)	offsetof(struct marble_specific, m)

struct	structparse marble_parse[] = {
	(char *)0,(char *)0,	0,		FUNC_NULL
};


HIDDEN int  marble_setup(), marble_render();
extern int	mlib_zero(), mlib_one();
extern void	mlib_void();

struct	mfuncs marble_mfuncs[] = {
	"marble",	0,		0,		MFI_HIT,
	marble_setup,	marble_render,	mlib_void,	mlib_void,

	(char *)0,	0,		0,		0,
	0,		0,		0,		0
};


#define	IPOINTS	10
HIDDEN double	n[IPOINTS+1][IPOINTS+1][IPOINTS+1];

/*
 *			M A R B L E _ S E T U P
 *
 * Initialize the table of noise points
 */
HIDDEN int
marble_setup( rp, matparm, dpp )
register struct region *rp;
struct rt_vls	*matparm;
char	**dpp;
{
	int	i, j, k;
	register struct marble_specific *mp;
	extern struct resource		rt_uniresource;
	register struct resource	*resp = &rt_uniresource;;

	RT_VLS_CHECK( matparm );
	GETSTRUCT( mp, marble_specific );
	*dpp = (char *)mp;

	if( rt_bound_tree(rp->reg_treetop,mp->mar_min,mp->mar_max) < 0 )
		return(-1);	/* FAIL */

	for( i = 0; i < IPOINTS+1; i++ )
		for( j = 0; j < IPOINTS+1; j++ )
			for( k = 0; k < IPOINTS+1; k++ )
				n[i][j][k] = rand0to1(resp->re_randptr);

	/*
	 * Duplicate the three initial faces
	 *  beyond the last three faces so we can interpolate
	 *  back to zero.  NOT DONE YET.
	 */
	return(1);
}

/*
 *	N O I S E 3
 *
 * Return a linearly interpolated noise point for
 *  0 <= x < 1 and 0 <= y < 1 and 0 <= z < 1.
 *  Noise returned is also between 0 and 1.
 */
double
noise3(x, y, z)
double x, y, z;
{
	int	xi, yi, zi;		/* Integer portions of x and y */
	double	xr, yr, zr;		/* Remainders */
	double	n1, n2, noise1, noise2, noise;	/* temps */


	xi = x * IPOINTS;
	xr = (x * IPOINTS) - xi;
	yi = y * IPOINTS;
	yr = (y * IPOINTS) - yi;
	zi = z * IPOINTS;
	zr = (z * IPOINTS) - zi;
/*rt_log("xi= %d, yi= %d, zi= %d\n",xi,yi,zi);*/
	n1 = (1 - xr) * n[xi][yi][zi] + xr * n[xi + 1][yi][zi];
	n2 = (1 - xr) * n[xi][yi + 1][zi] + xr * n[xi + 1][yi + 1][zi];
	noise1 = (1 - yr) * n1 + yr * n2;
	n1 = (1 - xr) * n[xi][yi][zi + 1] + xr * n[xi + 1][yi][zi + 1];
	n2 = (1 - xr) * n[xi][yi + 1][zi + 1] + xr * n[xi + 1][yi + 1][zi + 1];
	noise2 = (1 - yr) * n1 + yr * n2;
	noise = (1 - zr) * noise1 + zr * noise2;

/*rt_log("noise3(%g,%g,%g) = %g\n",x,y,z,noise);*/
	return( noise );
}

/*
 * Turbulence Routine
 */
#define	Pixelsize	0.005

HIDDEN double
turb3(x, y, z)
double x, y, z;
{
	double	turb, temp;
	double	scale;

	turb = 0;
	scale = 1.0;
	temp = 0.0;

	while( scale > Pixelsize ) {
		temp = ( ( noise3( x * scale, y * scale, z * scale ) - 0.5 ) * scale );
		turb += ( temp > 0 ) ? temp : - temp;
		scale /= 2.0;
	}

/*rt_log("turb(%g,%g,%g) = %g\n",x,y,z,turb);*/
	return( turb );
}

/*
 *			M A R B L E _ R E N D E R
 *
 *
 *
 */
HIDDEN
marble_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
register struct shadework	*swp;
char	*dp;
{
	register struct marble_specific *mp =
		(struct marble_specific *)dp;
	double	newx, value;
	fastf_t x, y, z;
	vect_t min, max;
	fastf_t xd, yd, zd;

	xd = mp->mar_max[0] - mp->mar_min[0] + 1.0;
	yd = mp->mar_max[1] - mp->mar_min[1] + 1.0;
	zd = mp->mar_max[2] - mp->mar_min[2] + 1.0;

	/* NORMALIZE x,y,z to [0..1) */
	x = (swp->sw_hit.hit_point[0] - mp->mar_min[0]) / xd;
	y = (swp->sw_hit.hit_point[1] - mp->mar_min[1]) / yd;
	z = (swp->sw_hit.hit_point[2] - mp->mar_min[2]) / zd;

	newx = x + turb3( x, y, z );

	value = sin(newx);

	if( value <= 0.25 )
		value *= 4.0;
	else
		if( value > 0.25 && value <= 0.5 )
			value = -((value-0.5) * 4.0);
		else
			if( value > 0.5 && value <= 0.75 )
				value = (value-0.5) * 4.0;
			else
				value = -((value-1.0) * 4.0);

	VSETALL( swp->sw_color, value );
	return(1);
}
