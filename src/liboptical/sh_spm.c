/*                        S H _ S P M . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2024 United States Government as represented by
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
/** @file liboptical/sh_spm.c
 *
 * Spherical Data Structures/Texture Maps
 *
 */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <math.h>

#include "vmath.h"
#include "bn/spm.h"
#include "raytrace.h"
#include "dm.h"
#include "optical.h"


#define SPM_NAME_LEN 128
struct spm_specific {
    char sp_file[SPM_NAME_LEN];	/* Filename */
    int sp_w;		/* Width: number of pixels around equator */
    bn_spm_map_t *sp_map;	/* stuff */
};
#define SP_NULL ((struct spm_specific *)0)
#define SP_O(m) bu_offsetof(struct spm_specific, m)

struct bu_structparse spm_parse[] = {
    {"%s",	SPM_NAME_LEN, "file",		SP_O(sp_file),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "w",		SP_O(sp_w),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "n",		SP_O(sp_w),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },	/*compat*/
    {"",	0, (char *)0,	0,		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


static int spm_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *mfp, struct rt_i *rtip);
static int spm_render(struct application *ap, const struct partition *pp, struct shadework *swp, void *dp);
static void spm_print(register struct region *rp, void *dp);
static void spm_mfree(void *cp);

struct mfuncs spm_mfuncs[] = {
    {MF_MAGIC,	"spm",		0,		MFI_UV,		0,     spm_setup,	spm_render,	spm_print,	spm_mfree },
    {0,		(char *)0,	0,		0,		0,     0,		0,		0,		0 }
};


/*
 * Given a u, v coordinate within the texture (0 <= u, v <= 1.0),
 * return a pointer to the relevant pixel.
 */
static int
spm_render(struct application *UNUSED(ap), const struct partition *UNUSED(pp), struct shadework *swp, void *dp)
{
    register struct spm_specific *spp =
	(struct spm_specific *)dp;
    int x, y;
    register unsigned char *cp;

    /** bn_spm_read(spp->sp_map, xxx); **/
    /* Limits checking? */
    y = swp->sw_uv.uv_v * spp->sp_map->ny;
    x = swp->sw_uv.uv_u * spp->sp_map->nx[y];
    cp = &(spp->sp_map->xbin[y][x*3]);
    VSET(swp->sw_color,
	 ((double)cp[RED])/256.,
	 ((double)cp[GRN])/256.,
	 ((double)cp[BLU])/256.0);
    return 1;
}


static void
spm_mfree(void *cp)
{
    struct spm_specific *spm;

    spm = (struct spm_specific *)cp;

    if (spm->sp_map)
	bn_spm_free(spm->sp_map);
    spm->sp_map = BN_SPM_MAP_NULL;
    BU_PUT(cp, struct spm_specific);
}


/*
 * Returns -
 * <0 failed
 * >0 success
 */
static int
spm_setup(register struct region *UNUSED(rp), struct bu_vls *matparm, void **dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *UNUSED(rtip))


/* New since 4.4 release */
{
    register struct spm_specific *spp;

    BU_CK_VLS(matparm);
    BU_GET(spp, struct spm_specific);
    *dpp = spp;

    spp->sp_file[0] = '\0';
    spp->sp_w = -1;
    if (bu_struct_parse(matparm, spm_parse, (char *)spp, NULL) < 0) {
	BU_PUT(spp, struct spm_specific);
	return -1;
    }
    if (spp->sp_w < 0) spp->sp_w = 512;
    if (spp->sp_file[0] == '\0')
	goto fail;
    if ((spp->sp_map = bn_spm_init(spp->sp_w, sizeof(RGBpixel))) == BN_SPM_MAP_NULL)
	goto fail;
    if (bn_spm_load(spp->sp_map, spp->sp_file) < 0)
	goto fail;
    return 1;
fail:
    spm_mfree((void *)spp);
    return -1;
}


static void
spm_print(register struct region *rp, void *dp)
{
    struct spm_specific *spm;

    spm = (struct spm_specific *)dp;

    bu_log("spm_print(rp=%p, dp = %p)\n", (void *)rp, dp);
    (void)bu_struct_print("spm_print", spm_parse, (char *)dp);
    if (spm->sp_map) bn_spm_dump(spm->sp_map, 0);
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
