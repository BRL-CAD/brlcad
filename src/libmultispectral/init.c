/*                          I N I T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libmultispectral/init.c
 *
 *  This file represents the single function exported from the
 *  shader library whose "name" is known.
 *  All other functions are called through the function table.
 *
 *  Shaders are, of course, permitted to "upcall" into LIBRT as
 *  necessary.
 *
 */

#include "common.h"

#include <stdio.h>

#include "vmath.h"
#include "raytrace.h"
#include "shadefuncs.h"
#include "shadework.h"
#include "optical.h"


#define MFUNCS(_name)	\
	{ mlib_add_shader( headp, _name ); }

#define DMFUNCS(_name)	\
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
    DMFUNCS( temp_mfuncs );

    /* Compiled from sources in liboptical */
    MFUNCS( phg_mfuncs );
    MFUNCS( stk_mfuncs );
    MFUNCS( light_mfuncs );
    MFUNCS( camo_mfuncs );
    MFUNCS( noise_mfuncs );

#if 0
    /* these are not included yet as they do not have RT_MULTISPECTRAL hooks */
    DMFUNCS( cloud_mfuncs );
    DMFUNCS( spm_mfuncs );
    DMFUNCS( txt_mfuncs );
    DMFUNCS( cook_mfuncs );
    DMFUNCS( marble_mfuncs );
    DMFUNCS( stxt_mfuncs );
    DMFUNCS( points_mfuncs );
    DMFUNCS( toyota_mfuncs );
    DMFUNCS( wood_mfuncs );
    DMFUNCS( camo_mfuncs );
    DMFUNCS( scloud_mfuncs );
    DMFUNCS( air_mfuncs );
    DMFUNCS( rtrans_mfuncs );
    DMFUNCS( fire_mfuncs );
    DMFUNCS( brdf_mfuncs );
    DMFUNCS( gauss_mfuncs );
    DMFUNCS( gravel_mfuncs );
    DMFUNCS( prj_mfuncs );
    DMFUNCS( grass_mfuncs );
#endif
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
