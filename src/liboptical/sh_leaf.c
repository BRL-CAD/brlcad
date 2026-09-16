/*                       S H _ L E A F . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file liboptical/sh_leaf.c
 *
 * Procedural color shader for volumetric leaf and canopy solids.
 */

#include "common.h"

#include <stddef.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "optical.h"

#define leaf_MAGIC 0x1ea7
#define CK_leaf_SP(_p) BU_CKMAG(_p, leaf_MAGIC, "leaf_specific")
#define LEAF_O(m) bu_offsetof(struct leaf_specific, m)

struct leaf_specific {
    uint32_t magic;
    point_t base;
    point_t vein;
    point_t mottle;
    double noise_lacunarity;
    double noise_h_val;
    double noise_octaves;
    double noise_size;
    double vein_spacing;
    double vein_strength;
    double mottle_strength;
    vect_t noise_delta;
    mat_t xform;
};

static int leaf_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *mfp, struct rt_i *rtip);
static int leaf_render(struct application *ap, const struct partition *pp, struct shadework *swp, void *dp);
static void leaf_print(register struct region *rp, void *dp);
static void leaf_free(void *cp);
static void leaf_color_fix(const struct bu_structparse *sdp, const char *name, void *base, const char *value, void *data);

static const struct leaf_specific leaf_defaults = {
    leaf_MAGIC,
    {0.188, 0.463, 0.243},
    {0.420, 0.650, 0.270},
    {0.090, 0.280, 0.110},
    2.1753974,
    1.0,
    4.0,
    350.0,
    0.075,
    0.32,
    0.24,
    {1001.6, 1020.5, 1300.4},
    MAT_INIT_IDN
};

static struct bu_structparse leaf_parse[] = {
    {"%f", 3, "base", LEAF_O(base), leaf_color_fix, NULL, NULL},
    {"%f", 3, "rgb", LEAF_O(base), leaf_color_fix, NULL, NULL},
    {"%f", 3, "vein", LEAF_O(vein), leaf_color_fix, NULL, NULL},
    {"%f", 3, "mottle", LEAF_O(mottle), leaf_color_fix, NULL, NULL},
    {"%g", 1, "lacunarity", LEAF_O(noise_lacunarity), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "H", LEAF_O(noise_h_val), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "octaves", LEAF_O(noise_octaves), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "size", LEAF_O(noise_size), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "spacing", LEAF_O(vein_spacing), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "vein_strength", LEAF_O(vein_strength), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%g", 1, "mottle_strength", LEAF_O(mottle_strength), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 3, "delta", LEAF_O(noise_delta), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"", 0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};

struct mfuncs leaf_mfuncs[] = {
    {MF_MAGIC, "leaf", 0, MFI_HIT | MFI_NORMAL, 0, leaf_setup, leaf_render, leaf_print, leaf_free},
    {0, (char *)0, 0, 0, 0, 0, 0, 0, 0}
};

static double
leaf_clamp(double val, double lo, double hi)
{
    if (val < lo)
	return lo;
    if (val > hi)
	return hi;
    return val;
}

static void
leaf_color_fix(const struct bu_structparse *sdp, const char *UNUSED(name), void *base, const char *UNUSED(value), void *UNUSED(data))
{
    double *p = (double *)((char *)base + sdp->sp_offset);
    int normalized = 1;

    for (size_t i = 0; i < sdp->sp_count; i++) {
	if (p[i] < 0.0 || p[i] > 1.0) {
	    normalized = 0;
	    break;
	}
    }
    if (!normalized) {
	for (size_t i = 0; i < sdp->sp_count; i++)
	    p[i] /= 255.0;
    }
    for (size_t i = 0; i < sdp->sp_count; i++)
	p[i] = leaf_clamp(p[i], 0.0, 1.0);
}

static int
leaf_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *rtip)
{
    struct leaf_specific *leaf_sp;
    mat_t model_to_region;
    mat_t tmp;

    RT_CHECK_RTI(rtip);
    BU_CK_VLS(matparm);
    RT_CK_REGION(rp);

    BU_GET(leaf_sp, struct leaf_specific);
    *dpp = leaf_sp;
    memcpy(leaf_sp, &leaf_defaults, sizeof(struct leaf_specific));

    if (rp->reg_mater.ma_color_valid)
	VMOVE(leaf_sp->base, rp->reg_mater.ma_color);

    if (bu_struct_parse(matparm, leaf_parse, (char *)leaf_sp, NULL) < 0)
	return -1;

    if (leaf_sp->noise_size <= SMALL_FASTF)
	leaf_sp->noise_size = leaf_defaults.noise_size;
    if (leaf_sp->vein_spacing <= SMALL_FASTF)
	leaf_sp->vein_spacing = leaf_defaults.vein_spacing;

    db_region_mat(model_to_region, rtip->rti_dbip, rp->reg_name);
    MAT_IDN(tmp);
    tmp[0] = tmp[5] = tmp[10] = 1.0 / leaf_sp->noise_size;
    bn_mat_mul(leaf_sp->xform, tmp, model_to_region);

    MAT_IDN(tmp);
    tmp[MDX] = leaf_sp->noise_delta[X];
    tmp[MDY] = leaf_sp->noise_delta[Y];
    tmp[MDZ] = leaf_sp->noise_delta[Z];
    bn_mat_mul2(tmp, leaf_sp->xform);

    if (optical_debug&OPTICAL_DEBUG_SHADE)
	bu_struct_print(rp->reg_name, leaf_parse, (char *)leaf_sp);

    return 1;
}

static int
leaf_render(struct application *ap, const struct partition *pp, struct shadework *swp, void *dp)
{
    struct leaf_specific *leaf_sp = (struct leaf_specific *)dp;
    point_t pt;
    point_t color;
    double noise;
    double vein;
    double shade;
    double normal_lift;

    RT_AP_CHECK(ap);
    RT_CHECK_PT(pp);
    CK_leaf_SP(leaf_sp);

    MAT4X3PNT(pt, leaf_sp->xform, swp->sw_hit.hit_point);
    noise = 0.5 + 0.5 * bn_noise_fbm(pt, leaf_sp->noise_h_val, leaf_sp->noise_lacunarity, leaf_sp->noise_octaves);
    noise = leaf_clamp(noise, 0.0, 1.0);

    vein = fabs(sin((pt[X] + 0.35 * sin(pt[Y] * 0.71)) / leaf_sp->vein_spacing));
    vein = pow(1.0 - vein, 12.0);
    vein *= leaf_clamp(leaf_sp->vein_strength, 0.0, 1.0);

    VMOVE(color, leaf_sp->base);
    VBLEND2(color, 1.0 - leaf_sp->mottle_strength * noise, color, leaf_sp->mottle_strength * noise, leaf_sp->mottle);
    VBLEND2(color, 1.0 - vein, color, vein, leaf_sp->vein);

    normal_lift = 0.5 + 0.5 * fabs(swp->sw_hit.hit_normal[Z]);
    shade = 0.84 + 0.20 * normal_lift;
    VSCALE(swp->sw_color, color, shade);

    return 1;
}

static void
leaf_print(register struct region *rp, void *dp)
{
    bu_struct_print(rp->reg_name, leaf_parse, (char *)dp);
}

static void
leaf_free(void *cp)
{
    BU_PUT(cp, struct leaf_specific);
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
