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
/** @file liboptical/init.c
 *
 * This file represents the single function exported from the
 * shader library whose "name" is known.
 * All other functions are called through the function table.
 *
 * Shaders are, of course, permitted to "upcall" into LIBRT as
 * necessary.
 *
 */

#include "common.h"

#include <stdio.h>
#include "vmath.h"
#include "bu.h"
#include "raytrace.h"
#include "optical.h"

int rt_verbosity = -1;	/* blather incesantly by default */
int rdebug;			/* RT program debugging */
double AmbientIntensity = 0.4;	/* Ambient light intensity */
struct bn_table *spectrum = NULL;

#ifdef RT_MULTISPECTRAL
struct bn_tabdata *background = NULL;	/* radiant emittance of bg */
#else
vect_t background = VINIT_ZERO; /* Black */
#endif

#define MFUNCS(_name)							\
    { extern struct mfuncs _name[]; mlib_add_shader(headp, _name); }


void
optical_shader_init(struct mfuncs **headp)
{
    /*
     * Connect up shader ("material") interfaces
     * Note that sh_plastic.c defines the required "default" entry.
     */
    MFUNCS(phg_mfuncs);
    MFUNCS(null_mfuncs); /* null test shader */
    MFUNCS(light_mfuncs);
    MFUNCS(cloud_mfuncs);
    MFUNCS(spm_mfuncs);
    MFUNCS(txt_mfuncs);
    MFUNCS(stk_mfuncs);
    MFUNCS(cook_mfuncs);
    MFUNCS(stxt_mfuncs);
    MFUNCS(points_mfuncs);
    MFUNCS(toyota_mfuncs);
    MFUNCS(wood_mfuncs);
    MFUNCS(camo_mfuncs);
    MFUNCS(scloud_mfuncs);
    MFUNCS(air_mfuncs);
    MFUNCS(rtrans_mfuncs);
    MFUNCS(fire_mfuncs);
    MFUNCS(brdf_mfuncs);
    MFUNCS(gauss_mfuncs);
    MFUNCS(noise_mfuncs);
    MFUNCS(prj_mfuncs);
    MFUNCS(grass_mfuncs);
    MFUNCS(tthrm_mfuncs);
    MFUNCS(flat_mfuncs);
    MFUNCS(bbd_mfuncs);
    MFUNCS(toon_mfuncs);

#ifdef OSL_ENABLED
    /* This shader requires OSL, so it won't be compiled if this library was not enabled */
    MFUNCS(osl_mfuncs);
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
