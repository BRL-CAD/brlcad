/*                        S H _ T O O N . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2011 United States Government as represented by
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

/** @file liboptical/sh_toon.c
 *
 * uses simple binning based on the cosine between the ray and the vector from
 * the hitpoint to the light. Might be improved by using hit location curvature
 * and/or cosine between ray dir and hit point normal?
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
#include "light.h"


#define TOON_MAGIC 0x746F6F6E    /* make this a unique number for each shader */
#define CK_TOON_SP(_p) BU_CKMAG(_p, TOON_MAGIC, "toon_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct toon_specific {
    long magic;	/* magic # for memory validity check, must come 1st */
};


/* The default values for the variables in the shader specific structure */
static const
struct toon_specific toon_defaults = {
    TOON_MAGIC
};


#define SHDR_NULL ((struct toon_specific *)0)
#define SHDR_O(m) bu_offsetof(struct toon_specific, m)
#define SHDR_AO(m) bu_offsetofarray(struct toon_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse toon_print_tab[] = {
    {"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }

};
struct bu_structparse toon_parse_tab[] = {
    {"%p",	bu_byteoffset(toon_print_tab[0]), "toon_print_tab", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


HIDDEN int toon_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int toon_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp);
HIDDEN void toon_print(register struct region *rp, genptr_t dp);
HIDDEN void toon_free(genptr_t cp);

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs toon_mfuncs[] = {
    {MF_MAGIC,	"toon",	0,	MFI_NORMAL|MFI_HIT,	0,
     toon_setup,	toon_render,	toon_print,	toon_free },

    {0,		(char *)0,	0,		0,		0,
     0,		0,		0,		0 }
};


/* T O O N _ S E T U P
 *
 * This routine is called (at prep time)
 * once for each region which uses this shader.
 * Any shader-specific initialization should be done here.
 *
 * Returns:
 * 1 success
 * 0 success, but delete region
 * -1 failure
 */
HIDDEN int
toon_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *rtip)


/* pointer to reg_udata in *rp */

/* New since 4.4 release */
{
    register struct toon_specific *toon_sp;

    /* check the arguments */
    RT_CHECK_RTI(rtip);
    BU_CK_VLS(matparm);
    RT_CK_REGION(rp);

    if (rdebug&RDEBUG_SHADE)
	bu_log("toon_setup(%s)\n", rp->reg_name);

    /* Get memory for the shader parameters and shader-specific data */
    BU_GETSTRUCT(toon_sp, toon_specific);
    *dpp = toon_sp;

    /* initialize the default values for the shader */
    memcpy(toon_sp, &toon_defaults, sizeof(struct toon_specific));

    /* parse the user's arguments for this use of the shader. */
    if (bu_struct_parse(matparm, toon_parse_tab, (char *)toon_sp) < 0)
	return -1;

    if (rdebug&RDEBUG_SHADE) {
	bu_struct_print(" Parameters:", toon_print_tab, (char *)toon_sp);
    }

    return 1;
}


/*
 * T O O N _ P R I N T
 */
HIDDEN void
toon_print(register struct region *rp, genptr_t dp)
{
    bu_struct_print(rp->reg_name, toon_print_tab, (char *)dp);
}


/*
 * T O O N _ F R E E
 */
HIDDEN void
toon_free(genptr_t cp)
{
    bu_free(cp, "toon_specific");
}


/*
 * T O O N _ R E N D E R
 *
 * This is called (from viewshade() in shade.c) once for each hit point
 * to be shaded.  The purpose here is to fill in values in the shadework
 * structure.
 */
int
toon_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp)
{
    int i;
    struct toon_specific *toon_sp = (struct toon_specific *)dp;
    struct light_specific *lp;
    fastf_t cosi, scale;

    /* check the validity of the arguments we got */
    RT_AP_CHECK(ap);
    RT_CHECK_PT(pp);
    CK_TOON_SP(toon_sp);

    if (rdebug&RDEBUG_SHADE)
	bu_struct_print("toon_render Parameters:", toon_print_tab, (char *)toon_sp);

    /* if surface normal is nearly orthogonal to the ray, make a black line */
    if (VDOT(swp->sw_hit.hit_normal, ap->a_inv_dir) >= 0.8) {
	VSETALL(swp->sw_color, 0);
	return 1;
    }

    /* probably need to set some swp values here to avoid the infinite recursion
     * if specified lights exist. */
    light_obs(ap, swp, MFI_HIT);

    /* Consider effects of each light source */
    for (i=ap->a_rt_i->rti_nlights-1; i>=0; i--) {
	if ((lp = (struct light_specific *)swp->sw_visible[i]) == LIGHT_NULL)
	    continue;

	cosi = VDOT(swp->sw_hit.hit_normal, swp->sw_tolight);
	if (cosi <= 0.0) scale = 0.0;
	else if (cosi <= 0.5) scale = 0.5;
	else if (cosi <= 0.8) scale = 0.8;
	else scale = 1.0;
	VSCALE(swp->sw_color, swp->sw_color, scale);
	return 1;
    }
    
    /* no paths to light source, so just paint it black */
    VSETALL(swp->sw_color, 0);

    if (swp->sw_reflect > 0 || swp->sw_transmit > 0)
	(void)rr_render(ap, pp, swp);
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
