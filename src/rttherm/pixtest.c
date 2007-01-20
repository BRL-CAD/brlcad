/*                       P I X T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file pixtest.c
 *
 *  Take an RGB .pix file, convert it to spectral form, then sample it back
 *  to RGB and output it as a .pix file.
 *  A tool for testing the spectral conversion routines and the
 *  underlying libraries.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"




#include <stdio.h>
#include <math.h>

#include "machine.h"

#include "bu.h"
#include "bn.h"
#include "vmath.h"
#include "spectrum.h"


struct rt_tabdata *curve;

#if 0
/* Not many samples in visible part of spectrum */
int	nsamp = 100;
double	min_nm = 380;
double	max_nm = 12000;
#else
int	nsamp = 20;
double	min_nm = 340;
double	max_nm = 760;
#endif

struct rt_tabdata	*cie_x;
struct rt_tabdata	*cie_y;
struct rt_tabdata	*cie_z;

mat_t			xyz2rgb;

/*
 *			M A I N
 */
main(void)
{
	unsigned char	rgb[4];
	point_t		src;
	point_t		dest;
	point_t		xyz;

	spectrum = rt_table_make_uniform( nsamp, min_nm, max_nm );
	RT_GET_TABDATA( curve, spectrum );

	rt_spect_make_CIE_XYZ( &cie_x, &cie_y, &cie_z, spectrum );
	rt_make_ntsc_xyz2rgb( xyz2rgb );

	for(;;)  {
		if( fread(rgb, 1, 3, stdin) != 3 )  break;
		if( feof(stdin) )  break;

		VSET( src, rgb[0]/255., rgb[1]/255., rgb[2]/255. );

		rt_spect_reflectance_rgb( curve, src );

		rt_spect_curve_to_xyz( xyz, curve, cie_x, cie_y, cie_z );

		MAT3X3VEC( dest, xyz2rgb, xyz );

		if( dest[0] > 1 || dest[1] > 1 || dest[2] > 1 ||
		    dest[0] < 0 || dest[1] < 0 || dest[2] < 0 )  {
#if 0
bu_log("curve:\n");rt_pr_table_and_tabdata( "/dev/tty", curve );
#endif
			VPRINT("src ", src);
			VPRINT("dest", dest);
#if 0
		    	break;
#endif
		}

		if( dest[0] > 1 )  dest[0] = 1;
		if( dest[1] > 1 )  dest[1] = 1;
		if( dest[2] > 1 )  dest[2] = 1;
		if( dest[0] < 0 )  dest[0] = 0;
		if( dest[1] < 0 )  dest[1] = 0;
		if( dest[2] < 0 )  dest[2] = 0;

		VSCALE( rgb, dest, 255.0 );

		fwrite( rgb, 1, 3, stdout );
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
