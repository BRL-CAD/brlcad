/*                       S H _ S T X T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
/** @file liboptical/sh_stxt.c
 *
 * Routines to implement solid (ie, 3-D) texture maps.
 *
 * XXX Solid texturing is still preliminary.
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


HIDDEN int stxt_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int brick_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp);
HIDDEN int mbound_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp);
HIDDEN int rbound_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp);
HIDDEN void stxt_print(register struct region *rp, genptr_t dp);
HIDDEN void stxt_free(genptr_t cp);
HIDDEN void stxt_transp_hook(struct bu_structparse *ptab, char *name, char *cp, char *value);

#define STX_NAME_LEN 128
struct stxt_specific {
    int stx_transp[3];	/* RGB for transparency */
    char stx_file[STX_NAME_LEN];	/* Filename */
    uint32_t stx_magic;
    int stx_w;		/* Width of texture in pixels */
    int stx_fw;		/* File width of texture in pixels */
    int stx_n;		/* Number of scanlines */
    int stx_d;		/* Depth of texture (Num pix files)*/
    vect_t stx_min;
    vect_t stx_max;
    char *stx_pixels;	/* Pixel holding area */
    int trans_valid;	/* boolean: has stx_transp been set? */
};
#define STXT_MAGIC 0xfeedbaad
#define SOL_NULL ((struct stxt_specific *)0)
#define SOL_O(m) bu_offsetof(struct stxt_specific, m)

struct bu_structparse stxt_parse[] = {
    {"%d",	1, "transp",	bu_offsetofarray(struct stxt_specific, stx_transp),	stxt_transp_hook, NULL, NULL },
    {"%s",	STX_NAME_LEN, "file",	bu_offsetofarray(struct stxt_specific, stx_file),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "w",			SOL_O(stx_w),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "n",			SOL_O(stx_n),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "d",			SOL_O(stx_d),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "fw",		SOL_O(stx_fw),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "trans_valid",	SOL_O(trans_valid),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


struct mfuncs stxt_mfuncs[] = {
    {MF_MAGIC,	"brick",	0,		MFI_HIT,	0,     stxt_setup,	brick_render,	stxt_print,	stxt_free },
    {MF_MAGIC,	"mbound",	0,		MFI_HIT,	0,     stxt_setup,	mbound_render,	stxt_print,	stxt_free },
    {MF_MAGIC,	"rbound",	0,		MFI_HIT,	0,     stxt_setup,	rbound_render,	stxt_print,	stxt_free },
    {0,		(char *)0,	0,		0,		0,     0,		0,		0,		0 }
};


/*
 * S T X T _ T R A N S P _ H O O K
 *
 * Hooked function, called by bu_structparse.
 */
HIDDEN void
stxt_transp_hook(struct bu_structparse *ptab, char *name, char *cp, char *UNUSED(value))
{
    register struct stxt_specific *stp =
	(struct stxt_specific *)cp;

    if (BU_STR_EQUAL(name, stxt_parse[0].sp_name) && ptab == stxt_parse) {
	stp->trans_valid = 1;
    } else {
	bu_log("file:%s, line:%d stxt_transp_hook name:(%s) instead of (%s)\n",
	       __FILE__, __LINE__, name, stxt_parse[0].sp_name);
    }
}


/*
 * S T X T _ R E A D
 *
 * Load the texture into memory.
 * Returns 0 on failure, 1 on success.
 */
HIDDEN int
stxt_read(register struct stxt_specific *stp)
{
    char *linebuf;
    register FILE *fp;
    register int i;
    char name[256];
    int frame, ln;
    int rd, rdd;

    /*** MEMORY HOG ***/
    stp->stx_pixels = bu_malloc(
	stp->stx_w * stp->stx_n * stp->stx_d * 3,
	stp->stx_file);

    ln = 0;
    rdd = 0;
    rd = 0;

    /* LOOP: through list of basename.n files */
    for (frame=0; frame <= stp->stx_d-1; frame++) {

	snprintf(name, 256, "%s.%d", stp->stx_file, frame);

	if ((fp = fopen(name, "rb")) == NULL) {
	    bu_log("stxt_read(%s):  can't open\n", name);
	    stp->stx_file[0] = '\0';
	    return 0;
	}
	linebuf = bu_malloc(stp->stx_fw*3, "texture file line");

	for (i = 0; i < stp->stx_n; i++) {
	    if ((rd = (int)fread(linebuf, 1, stp->stx_fw*3, fp)) != stp->stx_fw*3) {
		bu_log("stxt_read: read error on %s\n", name);
		stp->stx_file[0] = '\0';
		(void)fclose(fp);
		bu_free(linebuf, "file line, error");
		return 0;
	    }
	    memcpy(stp->stx_pixels + ln*stp->stx_w*3, linebuf, stp->stx_w*3);
	    ln++;
	    rdd += rd;
	}
	(void)fclose(fp);
	bu_free(linebuf, "texture file line");
    }
    return 1;	/* OK */
}


/*
 * S T X T _ S E T U P
 */
HIDDEN int
stxt_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *UNUSED(rtip))


/* New since 4.4 release */
{
    register struct stxt_specific *stp;

    BU_GET(stp, struct stxt_specific);
    *dpp = stp;

    /** Set up defaults **/
    stp->stx_magic = STXT_MAGIC;
    stp->stx_file[0] = '\0';
    stp->stx_w = stp->stx_fw = stp->stx_n = stp->stx_d = -1;

    if (rt_bound_tree(rp->reg_treetop, stp->stx_min, stp->stx_max) < 0)
	return -1;

    /** Get input values **/
    if (bu_struct_parse(matparm, stxt_parse, (char *)stp) < 0) {
	bu_free((genptr_t)stp, "stxt_specific");
	return -1;
    }
    /*** DEFAULT SIZE OF STXT FILES ***/
    if (stp->stx_w < 0) stp->stx_w = 512;
    if (stp->stx_n < 0) stp->stx_n = stp->stx_w;

    /** Defaults to an orthogonal projection??  **/
    if (stp->stx_d < 0) stp->stx_d = 1;

    if (stp->stx_fw < 0) stp->stx_fw = stp->stx_w;
    stp->stx_pixels = (char *)0;
    if (stp->trans_valid)
	rp->reg_transmit = 1;

    /** Read in texture file/s **/
    return stxt_read(stp);
}


/*
 * S T X T _ F R E E
 */
HIDDEN void
stxt_free(genptr_t cp)
{
    register struct stxt_specific *stp =
	(struct stxt_specific *)cp;

    if (stp->stx_magic != STXT_MAGIC) bu_log("stxt_free(): bad magic\n");

    if (stp->stx_pixels)
	bu_free(stp->stx_pixels, "solid texture pixel array");
    bu_free(cp, "stx_specific");
}


/*
 * S T X T _ P R I N T
 */
HIDDEN void
stxt_print(register struct region *rp, genptr_t dp)
{
    bu_struct_print(rp->reg_name, stxt_parse, (char *)dp);
}


HIDDEN int
brick_render(struct application *UNUSED(ap), const struct partition *UNUSED(pp), struct shadework *swp, genptr_t dp)
{
    register struct stxt_specific *stp =
	(struct stxt_specific *)dp;
    vect_t lx, ly, lz;	/* local coordinate axis */
    fastf_t f;
    double iptr;
    fastf_t sx, sy, sz;
    int tx, ty, tz;
    register long r, g, b;
    int u1, u2, u3;
    register unsigned char *cp;

    if (stp->stx_magic != STXT_MAGIC) bu_log("brick_render(): bad magic\n");

    /*
     * If no texture file present, or if
     * texture isn't and can't be read, give debug colors
     */
    if (stp->stx_file[0] == '\0'  ||
	(stp->stx_pixels == (char *)0 && stxt_read(stp) == 0)) {
	VSET(swp->sw_color, 1, 0, 1);
	return 1;
    }

    /** Local Coordinate Axis **/
    VSET(lx, 1, 0, 0);
    VSET(ly, 0, 1, 0);
    VSET(lz, 0, 0, 1);

    f = VDOT(swp->sw_hit.hit_point, lx) / (float)stp->stx_w;
    if (f < 0) f = -f;
    f = modf(f, &iptr);
    sx=f;
/********************************
 *	if (f < 0.5)
 *		sx = 2 * f;
 *	else
 *		sx = 2 * (1 - f);
 *********************************/

    f = VDOT(swp->sw_hit.hit_point, ly) / (float)stp->stx_n;
    if (f < 0) f = -f;
    f = modf(f, &iptr);
    sy=f;
/*********************************
 *	if (f < 0.5)
 *		sy = 2 * f;
 *	else
 *		sy = 2 * (1 - f);
 **********************************/

    f = VDOT(swp->sw_hit.hit_point, lz) / (float)stp->stx_d;
    if (f < 0) f = -f;
    f = modf(f, &iptr);
    sz=f;
/*********************************
 *	if (f < 0.5)
 *		sz = 2 * f;
 *	else
 *		sz = 2 * (1 - f);
 **********************************/

/*bu_log("sx = %f\tsy = %f\tsz = %f\n", sx, sy, sz);*/

    /* Index into TEXTURE SPACE */
    tx = sx * (stp->stx_w-1);
    ty = sy * (stp->stx_n-1);
    tz = sz * (stp->stx_d-1);

    u1 = (int)tz * stp->stx_n * stp->stx_w * 3.0;
    u2 = (int)ty * stp->stx_w * 3.0;
    u3 = (int)tx * 3.0;

    cp = (unsigned char *)(stp->stx_pixels + u1 + u2 + u3);

    r = *cp++;
    g = *cp++;
    b = *cp++;

    VSET(swp->sw_color,
	 (r+0.5) * bn_inv255,
	 (g+0.5) * bn_inv255,
	 (b+0.5) * bn_inv255);

    return 1;
}


/*
 * R B O U N D _ R E N D E R
 *
 * Use region RPP to bound solid texture (rbound).
 */
HIDDEN int
rbound_render(struct application *UNUSED(ap), const struct partition *UNUSED(pp), struct shadework *swp, genptr_t dp)
{
    register struct stxt_specific *stp =
	(struct stxt_specific *)dp;
    fastf_t sx, sy, sz;
    int tx, ty, tz;
    register long r, g, b;

    int u1, u2, u3;
    register unsigned char *cp;

    if (stp->stx_magic != STXT_MAGIC) bu_log("rbound_render(): bad magic\n");

    /*
     * If no texture file present, or if
     * texture isn't and can't be read, give debug colors
     */
    if (stp->stx_file[0] == '\0'  ||
	(stp->stx_pixels == (char *)0 && stxt_read(stp) == 0)) {
	VSET(swp->sw_color, 1, 0, 1);
	return 1;
    }

    /* NORMALIZE x, y, z to [0..1) */
#if 1
    /* XXX hack hack, permute axes, for vertical letters.  -M */
    sz = (swp->sw_hit.hit_point[0] - stp->stx_min[0]) /
	(stp->stx_max[0] - stp->stx_min[0] + 1.0);
    sx = (swp->sw_hit.hit_point[1] - stp->stx_min[1]) /
	(stp->stx_max[1] - stp->stx_min[1] + 1.0);
    sy = (swp->sw_hit.hit_point[2] - stp->stx_min[2]) /
	(stp->stx_max[2] - stp->stx_min[2] + 1.0);
#else
    sx = (swp->sw_hit.hit_point[0] - stp->stx_min[0]) /
	(stp->stx_max[0] - stp->stx_min[0] + 1.0);
    sy = (swp->sw_hit.hit_point[1] - stp->stx_min[1]) /
	(stp->stx_max[1] - stp->stx_min[1] + 1.0);
    sz = (swp->sw_hit.hit_point[2] - stp->stx_min[2]) /
	(stp->stx_max[2] - stp->stx_min[2] + 1.0);
#endif

    /* Index into TEXTURE SPACE */
    tx = sx * (stp->stx_w-1);
    ty = sy * (stp->stx_n-1);
    tz = sz * (stp->stx_d-1);

    u1 = (int)tz * stp->stx_n * stp->stx_w * 3.0;
    u2 = (int)ty * stp->stx_w * 3.0;
    u3 = (int)tx * 3.0;

    cp = (unsigned char *)(stp->stx_pixels + u1 + u2 + u3);

    r = *cp++;
    g = *cp++;
    b = *cp++;

    VSET(swp->sw_color,
	 (r+0.5) * bn_inv255,
	 (g+0.5) * bn_inv255,
	 (b+0.5) * bn_inv255);

    return 1;
}


/*
 * M B O U N D _ R E N D E R
 *
 * Use model RPP as solid texture bounds.  (mbound).
 */
HIDDEN int
mbound_render(struct application *ap, const struct partition *UNUSED(pp), struct shadework *swp, genptr_t dp)
{
    register struct stxt_specific *stp =
	(struct stxt_specific *)dp;
    fastf_t sx, sy, sz;
    int tx, ty, tz;
    register long r, g, b;
    int u1, u2, u3;
    register unsigned char *cp;

    if (stp->stx_magic != STXT_MAGIC) bu_log("mbound_render(): bad magic\n");

    /*
     * If no texture file present, or if
     * texture isn't and can't be read, give debug colors
     */
    if (stp->stx_file[0] == '\0'  ||
	(stp->stx_pixels == (char *)0 && stxt_read(stp) == 0)) {
	VSET(swp->sw_color, 1, 0, 1);
	return 1;
    }

    /* NORMALIZE x, y, z to [0..1) */
    sx = (swp->sw_hit.hit_point[0] - ap->a_rt_i->mdl_min[0]) /
	(ap->a_rt_i->mdl_max[0] - ap->a_rt_i->mdl_min[0] + 1.0);
    sy = (swp->sw_hit.hit_point[1] - ap->a_rt_i->mdl_min[1]) /
	(ap->a_rt_i->mdl_max[1] - ap->a_rt_i->mdl_min[1] + 1.0);
    sz = (swp->sw_hit.hit_point[2] - ap->a_rt_i->mdl_min[2]) /
	(ap->a_rt_i->mdl_max[2] - ap->a_rt_i->mdl_min[2] + 1.0);

    /* Index into TEXTURE SPACE */
    tx = sx * (stp->stx_w-1);
    ty = sy * (stp->stx_n-1);
    tz = sz * (stp->stx_d-1);

    u1 = (int)tz * stp->stx_n * stp->stx_w * 3;
    u2 = (int)ty * stp->stx_w * 3;
    u3 = (int)tx * 3;

    cp = (unsigned char *)(stp->stx_pixels + u1 + u2 + u3);

    r = *cp++;
    g = *cp++;
    b = *cp++;

    VSET(swp->sw_color,
	 (r+0.5) * bn_inv255,
	 (g+0.5) * bn_inv255,
	 (b+0.5) * bn_inv255);

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
