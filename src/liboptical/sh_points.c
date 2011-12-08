/*                     S H _ P O I N T S . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2011 United States Government as represented by
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
/** @file liboptical/sh_points.c
 *
 * Reads a file of u, v point locations and associated RGB color
 * values.  For each u, v texture mapping cell, this routine fills in
 * the color of the "brightest" point contained in that cell (if any).
 *
 * This routine was born in order to environment map the Yale Bright
 * Star catalog data without under or over sampling the point sources.
 * It was soon realized that making it "star" specific limited its
 * usefulness.
 *
 */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "spm.h"
#include "optical.h"


#define PT_NAME_LEN 128
struct points_specific {
    char pt_file[PT_NAME_LEN];	/* Filename */
    int pt_size;	/* number of bins around equator */
    bn_spm_map_t *pt_map;	/* stuff */
};
#define POINTS_NULL ((struct points_specific *)0)
#define POINTS_O(m) bu_offsetof(struct points_specific, m)

struct bu_structparse points_parse[] = {
    {"%s",	PT_NAME_LEN, "file", bu_offsetofarray(struct points_specific, pt_file),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "size",		POINTS_O(pt_size),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "w",			POINTS_O(pt_size),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


HIDDEN int points_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int points_render(struct application *ap, const struct partition *partp, struct shadework *swp, genptr_t dp);
HIDDEN void points_print(register struct region *rp, genptr_t dp);
HIDDEN void points_mfree(genptr_t cp);

struct mfuncs points_mfuncs[] = {
    {MF_MAGIC,	"points",	0,		MFI_UV,		0,     points_setup,	points_render,	points_print,	points_mfree },
    {0,		(char *)0,	0,		0,		0,     0,		0,		0,		0 }
};


struct points {
    fastf_t u;			/* u location */
    fastf_t v;			/* v location */
    vect_t color;			/* color of point */
    struct points *next;		/* next point in list */
};


/*
 * P O I N T S _ S E T U P
 *
 * Returns -
 * <0 failed
 * >0 success
 */
HIDDEN int
points_setup(register struct region *UNUSED(rp), struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *UNUSED(rtip))


/* New since 4.4 release */
{
    register struct points_specific *ptp;
    char buf[513];
    FILE *fp;

    BU_CK_VLS(matparm);
    BU_GET(ptp, struct points_specific);
    *dpp = ptp;

    /* get or default shader parameters */
    ptp->pt_file[0] = '\0';
    ptp->pt_size = -1;
    if (bu_struct_parse(matparm, points_parse, (char *)ptp) < 0) {
	bu_free((genptr_t)ptp, "points_specific");
	return -1;
    }
    if (ptp->pt_size < 0)
	ptp->pt_size = 512;
    if (ptp->pt_file[0] == '\0')
	bu_strlcpy(ptp->pt_file, "points.ascii", sizeof(ptp->pt_file));

    /* create a spherical data structure to bin point lists into */
    if ((ptp->pt_map = bn_spm_init(ptp->pt_size, sizeof(struct points))) == BN_SPM_MAP_NULL)
	goto fail;

    /* read in the data */
    if ((fp = fopen(ptp->pt_file, "rb")) == NULL) {
	bu_log("points_setup: can't open \"%s\"\n", ptp->pt_file);
	goto fail;
    }
    while (bu_fgets(buf, 512, fp) != NULL) {
	double u, v, mag;
	struct points *headp, *pp;

	if (buf[0] == '#')
	    continue;		/* comment */

	pp = (struct points *)bu_calloc(1, sizeof(struct points), "point");
	sscanf(buf, "%lf%lf%lf", &u, &v, &mag);
	pp->u = u;
	pp->v = v;
	pp->color[0] = mag;
	pp->color[1] = mag;
	pp->color[2] = mag;

	/* find a home for it */
	headp = (struct points *)bn_spm_get(ptp->pt_map, u, v);
	pp->next = headp->next;
	headp->next = pp;
    }
    (void)fclose(fp);

    return 1;
fail:
    bu_free((genptr_t)ptp, "points_specific");
    return -1;
}


/*
 * P O I N T S _ R E N D E R
 *
 * Given a u, v coordinate within the texture (0 <= u, v <= 1.0),
 * and a "size" of the pixel being rendered (du, dv), fill in the
 * color of the "brightest" point (if any) within that region.
 */
HIDDEN int
points_render(struct application *ap, const struct partition *UNUSED(partp), struct shadework *swp, genptr_t dp)
{
    register struct points_specific *ptp =
	(struct points_specific *)dp;
    register bn_spm_map_t *mapp;
    fastf_t umin, umax, vmin, vmax;
    int xmin, xmax, ymin, ymax;
    register int x, y;
    register struct points *pp;
    fastf_t mag;

    swp->sw_uv.uv_du = ap->a_diverge;
    swp->sw_uv.uv_dv = ap->a_diverge;
    /*bu_log("du, dv = %g %g\n", swp->sw_uv.uv_du, swp->sw_uv.uv_dv);*/

    /* compute and clip bounds in u, v space */
    umin = swp->sw_uv.uv_u - swp->sw_uv.uv_du;
    umax = swp->sw_uv.uv_u + swp->sw_uv.uv_du;
    vmin = swp->sw_uv.uv_v - swp->sw_uv.uv_dv;
    vmax = swp->sw_uv.uv_v + swp->sw_uv.uv_dv;
    if (umin < 0) umin = 0;
    if (vmin < 0) vmin = 0;
    if (umax > 1) umax = 1;
    if (vmax > 1) vmax = 1;

    mapp = ptp->pt_map;

    mag = 0;
    ymin = vmin * mapp->ny;
    ymax = vmax * mapp->ny;
    /* for each latitude band */
    for (y = ymin; y < ymax; y++) {
	xmin = umin * mapp->nx[y];
	xmax = umax * mapp->nx[y];
	/* for each bin spanned in that band */
	for (x = xmin; x < xmax; x++) {
	    pp = (struct points *)&(mapp->xbin[y][x*mapp->elsize]);
	    while (pp != NULL) {
		if (pp->u < umax && pp->u >= umin
		    && pp->v < vmax && pp->v >= vmin
		    && pp->color[0] > mag) {
		    mag = pp->color[0];
		}
		pp = pp->next;
	    }
	}
    }

    /*bu_log("points_render ([%g %g][%g %g]) = %g\n",
      umin, umax, vmin, vmax, mag);*/

    if (ZERO(mag)) {
	VSET(swp->sw_color, 0, 0, 0);
    } else {
	VSET(swp->sw_color, mag/255.0, mag/255.0, mag/255.0);
    }

    return 1;
}


/*
 * P O I N T S _ P R I N T
 */
HIDDEN void
points_print(register struct region *UNUSED(rp), genptr_t dp)
{
    bu_struct_print("points_setup", points_parse, (char *)dp);
    /* Should be more here */
}


HIDDEN void
points_mfree(genptr_t cp)
{
    /* XXX - free linked lists in every bin! */
    bn_spm_free((bn_spm_map_t *)cp);
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
