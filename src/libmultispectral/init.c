/*                          I N I T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file init.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "raytrace.h"
#include "shadefuncs.h"
#include "shadework.h"
#include "rtprivate.h"


#define MFUNCS(_name)	\
	{ extern struct mfuncs _name[]; mlib_add_shader( headp, _name ); }

/*
 *			M U L T I S P E C T R A L _ S H A D E R _ I N I T
 *
 *  Connect up shader ("material") interfaces
 *  Note that sh_plastic.c defines the required "default" entry.
 */
void
multispectral_shader_init(struct mfuncs **headp)
{
	/* multi-spectral-specific routines */
	MFUNCS( temp_mfuncs );

	/* Compiled from sources in liboptical */
	MFUNCS( phg_mfuncs );
	MFUNCS( stk_mfuncs );
	MFUNCS( light_mfuncs );
	MFUNCS( camo_mfuncs );
	MFUNCS( noise_mfuncs );

#if 0
	/* these are not included yet as they do not have RT_MULTISPECTRAL hooks */
	MFUNCS( cloud_mfuncs );
	MFUNCS( spm_mfuncs );
	MFUNCS( txt_mfuncs );
	MFUNCS( cook_mfuncs );
	MFUNCS( marble_mfuncs );
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
	MFUNCS( gravel_mfuncs );
	MFUNCS( prj_mfuncs );
	MFUNCS( grass_mfuncs );
#endif
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
