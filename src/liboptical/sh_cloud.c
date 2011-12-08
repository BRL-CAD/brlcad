/*                      S H _ C L O U D . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file liboptical/sh_cloud.c
 *
 * An attempt at 2D Geoffrey Gardner style cloud texture map
 *
 */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <math.h>

#include "vmath.h"
#include "raytrace.h"
#include "optical.h"

struct cloud_specific {
    fastf_t cl_thresh;
    fastf_t cl_range;
};
#define CL_NULL ((struct cloud_specific *)0)
#define CL_O(m) bu_offsetof(struct cloud_specific, m)

struct bu_structparse cloud_parse[] = {
    {"%f",	1, "thresh",	CL_O(cl_thresh),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1, "range",	CL_O(cl_range),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


HIDDEN int cloud_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int cloud_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp);
HIDDEN void cloud_print(register struct region *rp, genptr_t dp);
HIDDEN void cloud_free(genptr_t cp);

struct mfuncs cloud_mfuncs[] = {
    {MF_MAGIC,	"cloud",	0,		MFI_UV,		0,
     cloud_setup,	cloud_render,	cloud_print,	cloud_free },

    {0,		(char *)0,	0,		0,		0,
     0,		0,		0,		0 }
};


#define NUMSINES 4

/*
 * C L O U D _ T E X T U R E
 *
 * Returns the texture value for a plane point
 */
double
cloud_texture(register fastf_t x, register fastf_t y, fastf_t Contrast, fastf_t initFx, fastf_t initFy)
{
    register int i;
    fastf_t Px, Py, Fx, Fy, C;
    fastf_t t1, t2, k;

    t1 = t2 = 0;

    /*
     * Compute initial Phases and Frequencies
     * Freq "1" goes through 2Pi as x or y go thru 0.0 -> 1.0
     */
    Fx = bn_twopi * initFx;
    Fy = bn_twopi * initFy;
    Px = bn_halfpi * bn_tab_sin(0.5 * Fy * y);
    Py = bn_halfpi * bn_tab_sin(0.5 * Fx * x);
    C = 1.0;	/* ??? */

    for (i = 0; i < NUMSINES; i++) {
	/*
	 * Compute one term of each summation.
	 */
	t1 += C * bn_tab_sin(Fx * x + Px) + Contrast;
	t2 += C * bn_tab_sin(Fy * y + Py) + Contrast;

	/*
	 * Compute the new phases and frequencies.
	 * N.B. The phases shouldn't vary the same way!
	 */
	Px = bn_halfpi * bn_tab_sin(Fy * y);
	Py = bn_halfpi * bn_tab_sin(Fx * x);
	Fx *= 2.0;
	Fy *= 2.0;
	C *= 0.707;
    }

    /* Choose a magic k! */
    /* Compute max possible summation */
    k =  NUMSINES * 2 * NUMSINES;

    return t1 * t2 / k;
}


/*
 * C L O U D _ S E T U P
 */
HIDDEN int
cloud_setup(register struct region *UNUSED(rp), struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *UNUSED(rtip))
{
    register struct cloud_specific *cp;

    BU_CK_VLS(matparm);
    BU_GET(cp, struct cloud_specific);
    *dpp = cp;

    cp->cl_thresh = 0.35;
    cp->cl_range = 0.3;
    if (bu_struct_parse(matparm, cloud_parse, (char *)cp) < 0)
	return -1;

    return 1;
}


/*
 * C L O U D _ P R I N T
 */
HIDDEN void
cloud_print(register struct region *rp, genptr_t dp)
{
    bu_struct_print(rp->reg_name, cloud_parse, (char *)dp);
}


/*
 * C L O U D _ F R E E
 */
HIDDEN void
cloud_free(genptr_t cp)
{
    bu_free(cp, "cloud_specific");
}


/*
 * C L O U D _ R E N D E R
 *
 * Return a sky color with translucency control.
 * Threshold is the intensity below which it is completely translucent.
 * Range in the range on intensities over which translucence varies
 * from 0 to 1.
 * thresh=0.35, range=0.3 for decent clouds.
 */
int
cloud_render(struct application *UNUSED(ap), const struct partition *UNUSED(pp), struct shadework *swp, genptr_t dp)
{
    register struct cloud_specific *cp =
	(struct cloud_specific *)dp;
    double intensity;
    fastf_t TR;

    intensity = cloud_texture(swp->sw_uv.uv_u, swp->sw_uv.uv_v,
			      1.0, 2.0, 1.0);

    /* Intensity is normalized - check bounds */
    if (intensity > 1.0)
	intensity = 1.0;
    else if (intensity < 0.0)
	intensity = 0.0;

    /* Compute Translucency Function */
    TR = 1.0 - (intensity - cp->cl_thresh) / cp->cl_range;
    if (TR < 0.0)
	TR = 0.0;
    else if (TR > 1.0)
	TR = 1.0;

    swp->sw_color[0] = ((1-TR) * intensity + (TR * .31));	/* Red */
    swp->sw_color[1] = ((1-TR) * intensity + (TR * .31));	/* Green */
    swp->sw_color[2] = ((1-TR) * intensity + (TR * .78));	/* Blue */
    return 1;
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
