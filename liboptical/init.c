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
 *	This software is Copyright (C) 1998 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "raytrace.h"
#include "shadefuncs.h"
#include "shadework.h"
#include "../rt/rdebug.h"

/*
 *			O P T I C A L _ S H A D E R _ I N I T
 */
void
optical_shader_init(headp)
struct mfuncs	**headp;
{
	/*
	 *  Connect up shader ("material") interfaces
	 *  Note that sh_plastic.c defines the required "default" entry.
	 */
	{
		extern struct mfuncs phg_mfuncs[];
		extern struct mfuncs light_mfuncs[];
		extern struct mfuncs cloud_mfuncs[];
		extern struct mfuncs spm_mfuncs[];
		extern struct mfuncs txt_mfuncs[];
		extern struct mfuncs stk_mfuncs[];
		extern struct mfuncs cook_mfuncs[];
		extern struct mfuncs marble_mfuncs[];
		extern struct mfuncs stxt_mfuncs[];
		extern struct mfuncs points_mfuncs[];
		extern struct mfuncs toyota_mfuncs[];
		extern struct mfuncs wood_mfuncs[];
		extern struct mfuncs camo_mfuncs[]; 
		extern struct mfuncs scloud_mfuncs[];
		extern struct mfuncs air_mfuncs[];
		extern struct mfuncs rtrans_mfuncs[];
		extern struct mfuncs fire_mfuncs[];
		extern struct mfuncs brdf_mfuncs[];
		extern struct mfuncs gauss_mfuncs[];
		extern struct mfuncs gravel_mfuncs[];
		extern struct mfuncs prj_mfuncs[];
		extern struct mfuncs grass_mfuncs[];

		mlib_add_shader( headp, phg_mfuncs );
		mlib_add_shader( headp, light_mfuncs );
		mlib_add_shader( headp, cloud_mfuncs );
		mlib_add_shader( headp, spm_mfuncs );
		mlib_add_shader( headp, txt_mfuncs );
		mlib_add_shader( headp, stk_mfuncs );
		mlib_add_shader( headp, cook_mfuncs );
		mlib_add_shader( headp, marble_mfuncs );
		mlib_add_shader( headp, stxt_mfuncs );
		mlib_add_shader( headp, points_mfuncs );
		mlib_add_shader( headp, toyota_mfuncs );
		mlib_add_shader( headp, wood_mfuncs );
		mlib_add_shader( headp, camo_mfuncs );
		mlib_add_shader( headp, scloud_mfuncs );
		mlib_add_shader( headp, air_mfuncs );
		mlib_add_shader( headp, rtrans_mfuncs );
		mlib_add_shader( headp, fire_mfuncs );
		mlib_add_shader( headp, brdf_mfuncs );
		mlib_add_shader( headp, gauss_mfuncs );
		mlib_add_shader( headp, gravel_mfuncs );
		mlib_add_shader( headp, prj_mfuncs );
		mlib_add_shader( headp, grass_mfuncs );
	}
}
