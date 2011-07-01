/*                        S H _ F B M . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2011 United States Government as represented by
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
/** @file liboptical/sh_fbm.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "optical.h"


struct fbm_specific {
    double lacunarity;
    double h_val;
    double octaves;
    double offset;
    double gain;
    double distortion;
    point_t scale;	/* scale coordinate space */
};


static struct fbm_specific fbm_defaults = {
    2.1753974,		/* lacunarity */
    1.0,		/* h_val */
    4,			/* octaves */
    0.0,		/* offset */
    0.0,		/* gain */
    1.0,		/* distortion */
    VINITALL(1.0)	/* scale */
};


#define FBM_NULL ((struct fbm_specific *)0)
#define FBM_O(m) bu_offsetof(struct fbm_specific, m)
#define FBM_AO(m) bu_offsetofarray(struct fbm_specific, m)

struct bu_structparse fbm_parse[] = {
    {"%f", 1, "lacunarity",	FBM_O(lacunarity),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "H", 		FBM_O(h_val),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "octaves", 	FBM_O(octaves),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "gain",		FBM_O(gain),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "distortion",	FBM_O(distortion),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "l",		FBM_O(lacunarity),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "o", 		FBM_O(octaves),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "g",		FBM_O(gain),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "d",		FBM_O(distortion),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 3, "scale",		FBM_AO(scale),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


HIDDEN int fbm_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int fbm_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp);
HIDDEN void fbm_print(register struct region *rp, genptr_t dp);
HIDDEN void fbm_free(genptr_t cp);

struct mfuncs fbm_mfuncs[] = {
    {MF_MAGIC,	"bump_fbm",		0,	MFI_NORMAL|MFI_HIT|MFI_UV,	0,
     fbm_setup,	 fbm_render,	fbm_print,	fbm_free },

    {0,		(char*)0,		0,				0,	0,
     0,		0,			0,				0}
};


/*
 * F B M _ S E T U P
 */
HIDDEN int
fbm_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *UNUSED(rtip))
{
    register struct fbm_specific *fbm;

    BU_CK_VLS(matparm);
    BU_GETSTRUCT(fbm, fbm_specific);
    *dpp = fbm;

    memcpy(fbm, &fbm_defaults, sizeof(struct fbm_specific));
    if (rdebug&RDEBUG_SHADE)
	bu_log("fbm_setup\n");

    if (bu_struct_parse(matparm, fbm_parse, (char *)fbm) < 0)
	return -1;

    if (rdebug&RDEBUG_SHADE)
	bu_struct_print(rp->reg_name, fbm_parse, (char *)fbm);

    return 1;
}


/*
 * F B M _ P R I N T
 */
HIDDEN void
fbm_print(register struct region *rp, genptr_t dp)
{
    bu_struct_print(rp->reg_name, fbm_parse, (char *)dp);
}


/*
 * F B M _ F R E E
 */
HIDDEN void
fbm_free(genptr_t cp)
{
    bu_free(cp, "fbm_specific");
}


/*
 * F B M _ R E N D E R
 */
int
fbm_render(struct application *UNUSED(ap), const struct partition *UNUSED(pp), struct shadework *swp, genptr_t dp)
{
    register struct fbm_specific *fbm_sp =
	(struct fbm_specific *)dp;
    vect_t v_noise;
    point_t pt;

    if (rdebug&RDEBUG_SHADE)
	bu_struct_print("foo", fbm_parse, (char *)fbm_sp);

    pt[0] = swp->sw_hit.hit_point[0] * fbm_sp->scale[0];
    pt[1] = swp->sw_hit.hit_point[1] * fbm_sp->scale[1];
    pt[2] = swp->sw_hit.hit_point[2] * fbm_sp->scale[2];

    bn_noise_vec(pt, v_noise);

    VSCALE(v_noise, v_noise, fbm_sp->distortion);

    if (rdebug&RDEBUG_SHADE)
	bu_log("fbm_render: point (%g %g %g) becomes (%g %g %g)\n\tv_noise (%g %g %g)\n",
	       V3ARGS(swp->sw_hit.hit_point),
	       V3ARGS(pt),
	       V3ARGS(v_noise));

    VADD2(swp->sw_hit.hit_normal, swp->sw_hit.hit_normal, v_noise);
    VUNITIZE(swp->sw_hit.hit_normal);

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
