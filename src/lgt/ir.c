/*                            I R . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file ir.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./vecmath.h"
#include "./tree.h"
#define IR_DATA_WID	512
#define Avg_Fah(sum)	((sum)/(sample_sz))
#define Kelvin2Fah( f )	(9.0/5.0)*((f)-273.15) + 32.0
#define S_BINS		10
#define HUE_TOL		0.5

static RGBpixel	black = { 0, 0, 0 };
static int	ir_max_index = -1;
RGBpixel	*ir_table = (RGBpixel *)RGBPIXEL_NULL;

STATIC void	temp_To_RGB(unsigned char *rgb, int temp);

int
ir_Chk_Table(void)
{
	if( ir_table == (RGBpixel *)PIXEL_NULL )
		{
		get_Input( input_ln, MAX_LN, "Enter minimum temperature : " );
		if( sscanf( input_ln, "%d", &ir_min ) != 1 )
			{
			prnt_Scroll( "Could not read minimum temperature." );
			return	0;
			}
		get_Input( input_ln, MAX_LN, "Enter maximum temperature : " );
		if( sscanf( input_ln, "%d", &ir_max ) != 1 )
			{
			prnt_Scroll( "Could not read maximum temperature." );
			return	0;
			}
		if( ! init_Temp_To_RGB() )
			return	0;
		}
	return	1;
	}

STATIC int
adjust_Page(int y)
{	int	scans_per_page = fbiop->if_ppixels/fbiop->if_width;
		int	newy = y - (y % scans_per_page);
	return	newy;
	}

#define D_XPOS	(x-xmin)
void
display_Temps(int xmin, int ymin)
{	register int	x, y;
		register int	interval = ((grid_sz*3+2)/4)/(S_BINS+2);
		register int	xmax = xmin+(interval*S_BINS);
		register int	ymax;
		fastf_t		xrange = xmax - xmin;

	/* Avoid page thrashing of frame buffer.			*/
	ymin = adjust_Page( ymin );
	ymax = ymin + interval;

	/* Initialize ir_table if necessary.				*/
	if( ! ir_Chk_Table() )
		return;

	for( y = ymin; y <= ymax; y++ )
		{
		x = xmin;
		if( fb_seek( fbiop, x, y ) == -1 )
			{
			bu_log( "\"%s\"(%d) fb_seek to pixel <%d,%d> failed.\n",
				__FILE__, __LINE__, x, y
				);
			return;
			}
		for( ; x <= xmax + interval; x++ )
			{	fastf_t	percent;
				static RGBpixel	*pixel;
			percent = D_XPOS / xrange;
			if( D_XPOS % interval == 0 )
				{	int	temp = AMBIENT+percent*RANGE;
					register int	index = temp - ir_min;
				pixel = (RGBpixel *) ir_table[Min(index,ir_max_index)];
					/* LINT: this should be an &ir_table...,
						allowed by ANSI C, but not current
						compilers. */
				(void) fb_wpixel( fbiop, (unsigned char *) black );
				}
			else
				{
				(void) fb_wpixel( fbiop, (unsigned char *) pixel );
				}
			}
		}
	if( ! get_Font( (char *) NULL ) )
		{
		bu_log( "Could not load font.\n" );
		fb_flush( fbiop );
		return;
		}
	y = ymin;
	for( x = xmin; x <= xmax; x += interval )
		{	char	tempstr[4];
			fastf_t	percent = D_XPOS / xrange;
			int	temp = AMBIENT+percent*RANGE;
			int	shrinkfactor = fb_getwidth( fbiop )/grid_sz;
		(void) sprintf( tempstr, "%3d", temp );
		do_line(	x+2,
				y+(interval-(12/shrinkfactor))/2,
				tempstr
/*,shrinkfactor*/
				);
		}
	fb_flush( fbiop );
	return;
	}

STATIC int
get_IR(int x, int y, int *fahp, FILE *fp)
{
	if( fseek( fp, (long)((y*IR_DATA_WID + x) * sizeof(int)), 0 ) != 0 )
		return	0;
	else
	if( fread( (char *) fahp, (int) sizeof(int), 1, fp ) != 1 )
		return	0;
	else
		return	1;
	}
int
read_IR(FILE *fp)
{	register int	fy;
		register int	rx, ry;
		int		min, max;
	if(	fread( (char *) &min, (int) sizeof(int), 1, fp ) != 1
	     ||	fread( (char *) &max, (int) sizeof(int), 1, fp ) != 1
		)
		{
		bu_log( "Can't read minimum and maximum temperatures.\n" );
		return	0;
		}
	else
		{
		bu_log(	"IR data temperature range is %d to %d\n",
			min, max
			);
		if( ir_min == ABSOLUTE_ZERO )
			{ /* Temperature range not set.			*/
			ir_min = min;
			ir_max = max;
			}
		else 
			{ /* Merge with existing range.			*/
			ir_min = Min( ir_min, min );
			ir_max = Max( ir_max, max );
			bu_log(	"Global temperature range is %d to %d\n",
				ir_min, ir_max
				);
			}
		(void) fflush( stdout );
		}
	if( ! init_Temp_To_RGB() )
		{
		return	0;
		}
 	for( ry = 0, fy = grid_sz-1; ; ry += ir_aperture, fy-- )
		{
		if( fb_seek( fbiop, 0, fy ) == -1 )
			{
			bu_log( "\"%s\"(%d) fb_seek to pixel <%d,%d> failed.\n",
				__FILE__, __LINE__, 0, fy
				);
			return	0;
			}
		for( rx = 0 ; rx < IR_DATA_WID; rx += ir_aperture )
			{	int	fah;
				int	sum = 0;
				register int	i;
				register int	index;
				RGBpixel	*pixel;
			for( i = 0; i < ir_aperture; i++ )
				{	register int	j;
				for( j = 0; j < ir_aperture; j++ )
					{
					if( get_IR( rx+j, ry+i, &fah, fp ) )
						sum += fah < ir_min ? ir_min : fah;
					else	/* EOF */
						{
						if( ir_octree.o_temp == ABSOLUTE_ZERO )
							ir_octree.o_temp = AMBIENT - 1;
						display_Temps( grid_sz/8, 0 );
						return	1;
						}
					}
				}
			fah = Avg_Fah( sum );
			if( (index = fah-ir_min) > ir_max_index || index < 0 )
				{
				bu_log( "temperature out of range (%d)\n",
					fah
					);
				return	0;
				}
			pixel = (RGBpixel *) ir_table[index];
			(void) fb_wpixel( fbiop, (unsigned char *)pixel );
			}
		}
	}

/*	t e m p _ T o _ R G B ( )
	Map temperatures to spectrum of colors.
	This routine is extracted from the "mandel" program written by
	Douglas A. Gwyn here at BRL, and has been modified slightly
	to suit the input data.
 */
STATIC void
temp_To_RGB(unsigned char *rgb, int temp)
{	fastf_t		scale = 4.0 / RANGE;
		fastf_t		t = temp;
		fastf_t		hue = 4.0 - ((t < AMBIENT ? AMBIENT :
					      t > HOTTEST ? HOTTEST :
					      t) - AMBIENT) * scale;
		register int	h = (int) hue;	/* integral part	*/
		register int	f = (int)(256.0 * (hue - (fastf_t)h));
					/* fractional part * 256	*/
	if( t == ABSOLUTE_ZERO )
		rgb[RED] = rgb[GRN] = rgb[BLU] = 0;
	else
	switch ( h )
		{
	default:	/* 0 */
		rgb[RED] = 255;
		rgb[GRN] = f;
		rgb[BLU] = 0;
		break;
	case 1:
		rgb[RED] = 255 - f;
		rgb[GRN] = 255;
		rgb[BLU] = 0;
		break;
	case 2:
		rgb[RED] = 0;
		rgb[GRN] = 255;
		rgb[BLU] = f;
		break;
	case 3:
		rgb[RED] = 0;
		rgb[GRN] = 255 - f;
		rgb[BLU] = 255;
		break;
	case 4:
		rgb[RED] = f;
		rgb[GRN] = 0;
		rgb[BLU] = 255;
		break;
/*	case 5:
		rgb[RED] = 255;
		rgb[GRN] = 0;
		rgb[BLU] = 255 - f;
		break;
 */
		}
/*	bu_log( "temp=%d rgb=(%d %d %d)\n", temp, rgb[RED], rgb[GRN], rgb[BLU] );
 */
	return;
	}

/*	i n i t _ T e m p _ T o _ R G B ( )
	Initialize pseudo-color mapping table for the current view.  This
	color assignment will vary with each set of IR data read so as to
	map the full range of data to the full spectrum of colors.  This
	means that a given color will not necessarily have the same
	temperature mapping for different views of the vehicle, but is only
	valid for display of the current view.
 */
int
init_Temp_To_RGB(void)
{	register int	temp, i;
		RGBpixel	rgb;
	if( (ir_aperture = fb_getwidth( fbiop )/grid_sz) < 1 )
		{
		bu_log( "Grid too large for IR application, max. is %d.\n",
			IR_DATA_WID
			);
		return	0;
		}
	sample_sz = Sqr( ir_aperture );
	if( ir_table != (RGBpixel *)RGBPIXEL_NULL )
		/* Table already initialized presumably from another view,
			since range may differ we must create a different
			table of color assignment, so free storage and re-
			initialize.
		 */
		free( (char *) ir_table );
	ir_table = (RGBpixel *) malloc( (unsigned)(sizeof(RGBpixel)*((ir_max-ir_min)+1)) );
	if( ir_table == (RGBpixel *)RGBPIXEL_NULL )
		{
		Malloc_Bomb(sizeof(RGBpixel)*((ir_max-ir_min)+1));
		fatal_error = TRUE;
		return	0;
		}
	for( temp = ir_min, i = 0; temp <= ir_max; temp++, i++ )
		{
		temp_To_RGB( rgb, temp );
		COPYRGB( ir_table[i], rgb );
		}
	ir_max_index = i - 1;
	return	1;
	}

int
same_Hue(register RGBpixel (*pixel1p), register RGBpixel (*pixel2p))
{	fastf_t	rval1, gval1, bval1;
		fastf_t	rval2, gval2, bval2;
		fastf_t	rratio, gratio, bratio;
	if(	(*pixel1p)[RED] == (*pixel2p)[RED]
	    &&	(*pixel1p)[GRN] == (*pixel2p)[GRN]
	    &&	(*pixel1p)[BLU] == (*pixel2p)[BLU]
		)
		return	1;
	rval1 = (*pixel1p)[RED];
	gval1 = (*pixel1p)[GRN];
	bval1 = (*pixel1p)[BLU];
	rval2 = (*pixel2p)[RED];
	gval2 = (*pixel2p)[GRN];
	bval2 = (*pixel2p)[BLU];
	if( rval1 == 0.0 )
		{
		if( rval2 != 0.0 )
			return	0;
		else /* Both red values are zero. */
			rratio = 0.0;
		}
	else
	if( rval2 == 0.0 )
		return	0;
	else /* Neither red value is zero. */
		rratio = rval1/rval2;
	if( gval1 == 0.0 )
		{
		if( gval2 != 0.0 )
			return	0;
		else /* Both green values are zero. */
			gratio = 0.0;
		}
	else
	if( gval2 == 0.0 )
		return	0;
	else /* Neither green value is zero. */
		gratio = gval1/gval2;
	if( bval1 == 0.0 )
		{
		if( bval2 != 0.0 )
			return	0;
		else /* Both blue values are zero. */
			bratio = 0.0;
		}
	else
	if( bval2 == 0.0 )
		return	0;
	else /* Neither blue value is zero. */
		bratio = bval1/bval2;
	if( rratio == 0.0 )
		{
		if( gratio == 0.0 )
			return	1;
		else
		if( bratio == 0.0 )
			return	1;
		else
		if( AproxEq( gratio, bratio, HUE_TOL ) )
			return	1;
		else
			return	0;
		}
	else
	if( gratio == 0.0 )
		{
		if( bratio == 0.0 )
			return	1;
		else
		if( AproxEq( bratio, rratio, HUE_TOL ) )
			return	1;
		else
			return	0;
		}
	else
	if( bratio == 0.0 )
		{
		if( AproxEq( rratio, gratio, HUE_TOL ) )
			return	1;
		else
			return	0;
		}
	else
		{
		if(	AproxEq( rratio, gratio, HUE_TOL )
		    &&	AproxEq( gratio, bratio, HUE_TOL )
			)
			return	1;
		else
			return	0;
		}
	}

int
pixel_To_Temp(register RGBpixel (*pixelp))
{
#ifdef CRAY2 /* Compiler bug, register pointers don't always compile. */
		RGBpixel	*p;
		RGBpixel	*q = (RGBpixel *) ir_table[ir_max-ir_min];
#else
		register RGBpixel *p;
		register RGBpixel *q = (RGBpixel *) ir_table[ir_max-ir_min];
#endif	
		register int	temp = ir_min;
	for( p = (RGBpixel *) ir_table[0]; p <= q; p++, temp++ )
		{
		if( same_Hue( p, pixelp ) )
			return	temp;
		}
/*	prnt_Scroll( "Pixel=(%d,%d,%d): not assigned a temperature.\n",
		(int)(*pixelp)[RED],
		(int)(*pixelp)[GRN],
		(int)(*pixelp)[BLU]
		);*/
	return	ABSOLUTE_ZERO;
	}
int
f_IR_Model(register struct application *ap, Octree *op)
{	fastf_t		octnt_min[3], octnt_max[3];
		fastf_t		delta = modl_radius / pow_Of_2( ap->a_level );
		fastf_t		point[3]; /* Intersection point.	*/
		fastf_t		norml[3]; /* Unit normal at point.	*/
	/* Push ray origin along ray direction to intersection point.	*/
	VJOIN1( point, ap->a_ray.r_pt, ap->a_uvec[0], ap->a_ray.r_dir );

	/* Compute octant RPP.						*/
	octnt_min[X] = op->o_points->c_point[X] - delta;
	octnt_min[Y] = op->o_points->c_point[Y] - delta;
	octnt_min[Z] = op->o_points->c_point[Z] - delta;
	octnt_max[X] = op->o_points->c_point[X] + delta;
	octnt_max[Y] = op->o_points->c_point[Y] + delta;
	octnt_max[Z] = op->o_points->c_point[Z] + delta;

	if( AproxEq( point[X], octnt_min[X], EPSILON ) )
		/* Intersection point lies on plane whose normal is the
			negative X-axis.
		 */
		{
		norml[X] = -1.0;
		norml[Y] =  0.0;
		norml[Z] =  0.0;
		}
	else
	if( AproxEq( point[X], octnt_max[X], EPSILON ) )
		/* Intersection point lies on plane whose normal is the
			positive X-axis.
		 */
		{
		norml[X] = 1.0;
		norml[Y] = 0.0;
		norml[Z] = 0.0;
		}
	else
	if( AproxEq( point[Y], octnt_min[Y], EPSILON ) )
		/* Intersection point lies on plane whose normal is the
			negative Y-axis.
		 */
		{
		norml[X] =  0.0;
		norml[Y] = -1.0;
		norml[Z] =  0.0;
		}
	else
	if( AproxEq( point[Y], octnt_max[Y], EPSILON ) )
		/* Intersection point lies on plane whose normal is the
			positive Y-axis.
		 */
		{
		norml[X] = 0.0;
		norml[Y] = 1.0;
		norml[Z] = 0.0;
		}
	else
	if( AproxEq( point[Z], octnt_min[Z], EPSILON ) )
		/* Intersection point lies on plane whose normal is the
			negative Z-axis.
		 */
		{
		norml[X] =  0.0;
		norml[Y] =  0.0;
		norml[Z] = -1.0;
		}
	else
	if( AproxEq( point[Z], octnt_max[Z], EPSILON ) )
		/* Intersection point lies on plane whose normal is the
			positive Z-axis.
		 */
		{
		norml[X] = 0.0;
		norml[Y] = 0.0;
		norml[Z] = 1.0;
		}

	{	/* Factor in reflectance from "ambient" light source.	*/
		fastf_t	intensity = Dot( norml, lgts[0].dir );
		/* Calculate index into false-color table.		*/
		register int	index = op->o_temp - ir_min;
	if( index > ir_max_index )
		{
		bu_log( "Temperature (%d) above range of data.\n", op->o_temp );
		return	-1;
		}
	if( index < 0 )
		/* Un-assigned octants get colored grey.		*/
		ap->a_color[0] = ap->a_color[1] = ap->a_color[2] = intensity;
	else	/* Lookup false-coloring for octant's temperature.	*/
		{
		intensity *= RGB_INVERSE;
		ap->a_color[0] = (fastf_t) (ir_table[index][RED]) * intensity;
		ap->a_color[1] = (fastf_t) (ir_table[index][GRN]) * intensity;
		ap->a_color[2] = (fastf_t) (ir_table[index][BLU]) * intensity;
		}
	}
	return	1;
	}
int
f_IR_Backgr(register struct application *ap)
{
	VMOVE( ap->a_color, bg_coefs );
	return	0;
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
