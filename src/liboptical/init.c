/*                          I N I T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2021 United States Government as represented by
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
#include "raytrace.h"
#include "optical.h"

unsigned int optical_debug;	/* RT program debugging */
double AmbientIntensity = 0.4;	/* Ambient light intensity */

vect_t background = VINIT_ZERO; /* Black */

/* initialized in the app code view handler */
struct region env_region;

extern struct mfuncs camo_mfuncs[];
extern struct mfuncs light_mfuncs[];
extern struct mfuncs stk_mfuncs[];
extern struct mfuncs phg_mfuncs[];
extern struct mfuncs noise_mfuncs[];

#define MFUNCS(_name)							\
    { mlib_add_shader(headp, _name); }

#define DMFUNCS(_name)							\
    { extern struct mfuncs _name[]; mlib_add_shader(headp, _name); }


void
optical_shader_init(struct mfuncs **headp)
{
    /*
     * Connect up shader ("material") interfaces
     * Note that sh_plastic.c defines the required "default" entry.
     */
    MFUNCS(phg_mfuncs);
    MFUNCS(light_mfuncs);
    MFUNCS(camo_mfuncs);
    MFUNCS(stk_mfuncs);
    MFUNCS(noise_mfuncs);

    DMFUNCS(null_mfuncs); /* null test shader */
    DMFUNCS(cloud_mfuncs);
    DMFUNCS(spm_mfuncs);
    DMFUNCS(cook_mfuncs);
    DMFUNCS(stxt_mfuncs);
    DMFUNCS(txt_mfuncs);
    DMFUNCS(points_mfuncs);
    DMFUNCS(toyota_mfuncs);
    DMFUNCS(wood_mfuncs);
    DMFUNCS(scloud_mfuncs);
    DMFUNCS(air_mfuncs);
    DMFUNCS(rtrans_mfuncs);
    DMFUNCS(fire_mfuncs);
    DMFUNCS(brdf_mfuncs);
    DMFUNCS(gauss_mfuncs);
    DMFUNCS(prj_mfuncs);
    DMFUNCS(grass_mfuncs);
    DMFUNCS(tthrm_mfuncs);
    DMFUNCS(flat_mfuncs);
    DMFUNCS(bbd_mfuncs);
    DMFUNCS(toon_mfuncs);

#ifdef OSL_ENABLED
    /* This shader requires OSL, so it won't be compiled if this library was not enabled */
    DMFUNCS(osl_mfuncs);
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
