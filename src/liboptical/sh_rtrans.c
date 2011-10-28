/*                     S H _ R T R A N S . C
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
/** @file liboptical/sh_rtrans.c
 *
 * Random transparency shader. A random number from 0 to 1 is drawn
 * for each pixel rendered. If the random number is less than the threshold
 * value, the pixel is rendered as 100% transparent
 *
 */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vmath.h"
#include "raytrace.h"
#include "optical.h"


#define RTRANS_MAGIC 0x4a6f686e
struct rtrans_specific {
    uint32_t magic;
    double threshold;
    int next_rand;
};
#define CK_RTRANS_SP(_p) BU_CKMAG(_p, RTRANS_MAGIC, "rtrans_specific")

static struct rtrans_specific rtrans_defaults = {
    RTRANS_MAGIC,
    0.5,
    3
};


#define SHDR_NULL ((struct rtrans_specific *)0)
#define SHDR_O(m) bu_offsetof(struct rtrans_specific, m)
#define SHDR_AO(m) bu_offsetofarray(struct rtrans_specific, m)

struct bu_structparse rtrans_parse[] = {
    {"%f",  1, "threshold",		SHDR_O(threshold),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",  1, "t",			SHDR_O(threshold),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


HIDDEN int rtrans_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int rtrans_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp);
HIDDEN void rtrans_print(register struct region *rp, genptr_t dp);
HIDDEN void rtrans_free(genptr_t cp);

struct mfuncs rtrans_mfuncs[] = {
    {MF_MAGIC,	"rtrans",	0,		0,	0,     rtrans_setup,	rtrans_render,	rtrans_print,	rtrans_free },
    {0,		(char *)0,	0,		0,	0,     0,		0,		0,		0 }
};


/* R T R A N S _ S E T U P
 *
 * This routine is called (at prep time)
 * once for each region which uses this shader.
 * Any shader-specific initialization should be done here.
 */
HIDDEN int
rtrans_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *rtip)


/* pointer to reg_udata in *rp */

/* New since 4.4 release */
{
    register struct rtrans_specific *rtrans_sp;

    RT_CHECK_RTI(rtip);
    BU_CK_VLS(matparm);
    RT_CK_REGION(rp);
    BU_GETSTRUCT(rtrans_sp, rtrans_specific);
    *dpp = rtrans_sp;

    memcpy(rtrans_sp, &rtrans_defaults, sizeof(struct rtrans_specific));

    if (bu_struct_parse(matparm, rtrans_parse, (char *)rtrans_sp) < 0)
	return -1;

    BN_RANDSEED(rtrans_sp->next_rand, 3);

    if (rdebug&RDEBUG_SHADE)
	bu_struct_print(rp->reg_name, rtrans_parse, (char *)rtrans_sp);

    return 1;
}


/*
 * R T R A N S _ P R I N T
 */
HIDDEN void
rtrans_print(register struct region *rp, genptr_t dp)
{
    bu_struct_print(rp->reg_name, rtrans_parse, (char *)dp);
}


/*
 * R T R A N S _ F R E E
 */
HIDDEN void
rtrans_free(genptr_t cp)
{
    bu_free(cp, "rtrans_specific");
}


/*
 * R T R A N S _ R E N D E R
 *
 * This is called (from viewshade() in shade.c)
 * once for each hit point to be shaded.
 */
int
rtrans_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp)
{
    register struct rtrans_specific *rtrans_sp =
	(struct rtrans_specific *)dp;

    RT_AP_CHECK(ap);
    RT_CHECK_PT(pp);
    CK_RTRANS_SP(rtrans_sp);

    if (rdebug&RDEBUG_SHADE)
	bu_struct_print("random transparency", rtrans_parse, (char *)rtrans_sp);

    if (rtrans_sp->threshold >= 1.0 ||
	BN_RANDOM(rtrans_sp->next_rand)  < rtrans_sp->threshold)
    {
	swp->sw_transmit = 1.0;
	swp->sw_reflect = 0.0;
	swp->sw_refrac_index = 1.0;
	VSETALL(swp->sw_basecolor, 1.0);

	if (swp->sw_reflect > 0 || swp->sw_transmit > 0)
	    (void)rr_render(ap, pp, swp);
    }

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
