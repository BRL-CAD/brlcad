/*
 *			L I B O P T I C A L / I N I T . C
 *
 *  This file represents the single function exported from the
 *  shader library whose "name" is known.
 *  All other functions are called through the function table.
 *
 *  Shaders are, of course, permitted to "upcall" into LIBRT as
 *  necessary.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998-2004 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "raytrace.h"
#include "shadefuncs.h"
#include "shadework.h"
#include "rtprivate.h"

int		rdebug;			/* RT program debugging */
double		AmbientIntensity = 0.4;	/* Ambient light intensity */
vect_t		background = { 0.0, 0.0, 0.0 }; /* Black */

#define MFUNCS(_name)	\
	{ extern struct mfuncs _name[]; mlib_add_shader( headp, _name ); }


/*
 *			O P T I C A L _ S H A D E R _ I N I T
 */
void
optical_shader_init(struct mfuncs	**headp)
{
	/*
	 *  Connect up shader ("material") interfaces
	 *  Note that sh_plastic.c defines the required "default" entry.
	 */
	MFUNCS( phg_mfuncs );
	MFUNCS( null_mfuncs ); /* null test shader */
	MFUNCS( light_mfuncs );
	MFUNCS( cloud_mfuncs );
	MFUNCS( spm_mfuncs );
	MFUNCS( txt_mfuncs );
	MFUNCS( stk_mfuncs );
	MFUNCS( cook_mfuncs );
	MFUNCS( stxt_mfuncs );
	MFUNCS( points_mfuncs );
	MFUNCS( toyota_mfuncs );
	MFUNCS( wood_mfuncs );
	MFUNCS( camo_mfuncs ); 
	MFUNCS( scloud_mfuncs );
	MFUNCS( air_mfuncs );
	MFUNCS( rtrans_mfuncs );
	MFUNCS( fire_mfuncs );
	MFUNCS( brdf_mfuncs );
	MFUNCS( gauss_mfuncs );
	MFUNCS( noise_mfuncs );
	MFUNCS( prj_mfuncs );
	MFUNCS( grass_mfuncs );
	MFUNCS( tthrm_mfuncs );
	MFUNCS( flat_mfuncs );
	MFUNCS( bbd_mfuncs );
}
