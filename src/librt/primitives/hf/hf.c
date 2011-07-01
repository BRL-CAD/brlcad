/*                            H F . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/hf/hf.c
 *
 * Intersect a ray with a height field, where the heights are imported
 * from an external data file, and where some (or all) of the
 * parameters of that data file may be read in from an external
 * control file.
 *
 * Description of the external string description of the HF.
 *
 * There are two versions of this parse table.  The string solid in
 * the .g file can set any parameter.  The indirect control file
 * (cfile) can only set parameters relating to dfile parameters, and
 * not to the geometric position, orientation, and scale of the HF's
 * bounding RPP.
 *
 * In general, the cfile should be thought of as describing the data
 * arrangement of the dfile, and the string solid should be thought of
 * as describing the "geometry" of the height field's bounding RPP.
 *
 * The string solid is parsed first.  If a cfile is present, it is
 * parsed second, and any parameters specified in the cfile override
 * the values taken from the string solid.
 */

#include "common.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"


#define HF_O(m) bu_offsetof(struct rt_hf_internal, m)

/* All fields valid in string solid */
const struct bu_structparse rt_hf_parse[] = {
    {"%s",	128,	"cfile",	bu_offsetofarray(struct rt_hf_internal, cfile), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%s",	128,	"dfile",	bu_offsetofarray(struct rt_hf_internal, dfile), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%s",	8,	"fmt",		bu_offsetofarray(struct rt_hf_internal, fmt), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d",	1,	"w",		HF_O(w),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1,	"n",		HF_O(n),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1,	"shorts",	HF_O(shorts),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1,	"file2mm",	HF_O(file2mm),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	3,	"v",		HF_O(v[0]),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	3,	"x",		HF_O(x[0]),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	3,	"y",		HF_O(y[0]),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1,	"xlen",		HF_O(xlen),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1,	"ylen",		HF_O(ylen),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1,	"zscale",	HF_O(zscale),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0,	(char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};
/* Subset of fields found in cfile */
const struct bu_structparse rt_hf_cparse[] = {
    {"%s",	128,	"dfile",	bu_offsetofarray(struct rt_hf_internal, dfile), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%s",	8,	"fmt",		bu_offsetofarray(struct rt_hf_internal, fmt), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d",	1,	"w",		HF_O(w),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1,	"n",		HF_O(n),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1,	"shorts",	HF_O(shorts),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1,	"file2mm",	HF_O(file2mm),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0,	(char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


struct hf_specific {
    vect_t hf_V;	/* min vertex/origin of HF */
    vect_t hf_VO;	/* max vertex of HF */
    vect_t hf_X;	/* X direction vector */
    fastf_t hf_Xlen;	/* magnitude of HF in X direction */
    vect_t hf_Y;	/* Y Direction vector */
    fastf_t hf_Ylen;	/* magnitude of HF in Y direction */
    vect_t hf_N;	/* dir of elevation */
    fastf_t hf_min;	/* bounding box of hf solid */
    fastf_t hf_max;
    fastf_t hf_file2mm;	/* scale file elevation units to mm */
    int hf_w;		/* X dimension of file */
    int hf_n;		/* Y dimension of file */
    int hf_shorts;	/* Boolean: use shorts instead of double */
    struct bu_mapped_file *hf_mp;
};


/**
 * R T _ H F _ T O _ D S P
 *
 * Convert in-memory form of a height-field (HF) to a displacement-map
 * solid (DSP) in internal representation.  There is no record in the
 * V5 database for an HF.
 */
int
rt_hf_to_dsp(struct rt_db_internal *db_intern)
{
    struct rt_hf_internal *hip = (struct rt_hf_internal *)db_intern->idb_ptr;
    struct rt_dsp_internal *dsp;
    vect_t tmp;

    RT_CK_DB_INTERNAL(db_intern);
    RT_HF_CK_MAGIC(hip);

    if (! hip->shorts) {
	bu_log("cannot convert float HF to DSP\n");
	return -1;
    }

    BU_GETSTRUCT(dsp, rt_dsp_internal);
    bu_vls_init(&dsp->dsp_name);
    bu_vls_strcat(&dsp->dsp_name, hip->dfile);
    dsp->dsp_xcnt = hip->w;
    dsp->dsp_ycnt = hip->n;
    dsp->dsp_smooth = 0;
    dsp->dsp_cuttype = DSP_CUT_DIR_ADAPT;
    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("Converting from cut-style lower-left/upper-right to adaptive\n");
    }
    dsp->dsp_datasrc = RT_DSP_SRC_FILE;


    MAT_IDN(dsp->dsp_stom);
    MAT_DELTAS_VEC(dsp->dsp_stom, hip->v);	/* translate */

    /* Apply modeling transformations */
    VUNITIZE(hip->x);
    VSCALE(tmp, hip->x, hip->xlen/(fastf_t)(hip->w - 1));
    dsp->dsp_stom[0] = tmp[0];
    dsp->dsp_stom[4] = tmp[1];
    dsp->dsp_stom[8] = tmp[2];
    VUNITIZE(hip->y);
    VSCALE(tmp, hip->y, hip->ylen/(fastf_t)(hip->n - 1));
    dsp->dsp_stom[1] = tmp[0];
    dsp->dsp_stom[5] = tmp[1];
    dsp->dsp_stom[9] = tmp[2];
    VCROSS(tmp, hip->x, hip->y);
    VUNITIZE(tmp);

    /* The next line should be:
     *
     * VSCALE(tmp, tmp, hip->zscale * hip->file2mm);
     *
     * This will make the converted DSP plot in MGED agree with what
     * he HF looks like, but the HF ignores 'zscale' in the shot
     * routine.  So we choose to duplicate the raytrace behavior
     * (ignore zscale)
     */
    VSCALE(tmp, tmp, hip->file2mm);

    dsp->dsp_stom[2] = tmp[0];
    dsp->dsp_stom[6] = tmp[1];
    dsp->dsp_stom[10] = tmp[2];

    bn_mat_inv(dsp->dsp_mtos, dsp->dsp_stom);

    dsp->magic = RT_DSP_INTERNAL_MAGIC;

    rt_db_free_internal(db_intern);

    db_intern->idb_ptr = (genptr_t)dsp;
    db_intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    db_intern->idb_type = ID_DSP;
    db_intern->idb_meth = &rt_functab[ID_DSP];

    return 0;

}


/**
 * R T _ H F _ P R E P
 *
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid HF, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0 HF is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct hf_specific is created, and its address is stored in
 * stp->st_specific for use by hf_shot().
 */
int
rt_hf_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_hf_internal *hip;
    struct hf_specific *hf;
    const struct bn_tol *tol = &rtip->rti_tol;
    double dot;
    vect_t height, work;
    static int first_time=1;

    if (first_time) {
	first_time=0;
    }
    RT_CK_DB_INTERNAL(ip);
    hip = (struct rt_hf_internal *)ip->idb_ptr;
    RT_HF_CK_MAGIC(hip);

    BU_GETSTRUCT(hf, hf_specific);
    stp->st_specific = (genptr_t) hf;
    /*
     * The stuff that is given to us.
     */
    VMOVE(hf->hf_V, hip->v);
    VMOVE(hf->hf_X, hip->x);
    VUNITIZE(hf->hf_X);
    hf->hf_Xlen = hip->xlen;
    VMOVE(hf->hf_Y, hip->y);
    VUNITIZE(hf->hf_Y);
    hf->hf_Ylen = hip->ylen;
    hf->hf_file2mm = hip->file2mm;
    hf->hf_w = hip->w;
    hf->hf_n = hip->n;
    hf->hf_mp = hip->mp;
    bu_semaphore_acquire(RT_SEM_MODEL);
    ++hf->hf_mp->uses;
    bu_semaphore_release(RT_SEM_MODEL);
    hf->hf_shorts = hip->shorts;
    /*
     * From here down, we are calculating new values on a one time
     * basis.
     */

    /*
     * Start finding the location of the oposite vertex to V
     */
    VJOIN2(hf->hf_VO, hip->v, hip->xlen, hip->x, hip->ylen, hip->y);

    /*
     * get the normal.
     */
    dot = VDOT(hf->hf_X, hf->hf_Y);
    if (fabs(dot) >tol->perp) {
	/* not perpendicular, bad hf */
	bu_log("Hf(%s): X not perpendicular to Y.\n", stp->st_name);
	bu_free((genptr_t)hf, "struct hf");
	stp->st_specific = (genptr_t) 0;
	return 1;	/* BAD */
    }
    VCROSS(hf->hf_N, hf->hf_X, hf->hf_Y);
    VUNITIZE(hf->hf_N);		/* Not needed (?) */

    /*
     * Locate the min-max of the HF for use in determining VO and
     * bounding boxes et so forth.
     */
    if (hf->hf_shorts) {
	int max, min;
	int len;
	unsigned short *sp;
	int i;

	sp = (unsigned short *)hf->hf_mp->apbuf;
	min = max = *sp++;
	len = hf->hf_w * hf->hf_n;
	for (i=1; i< len; i++, sp++) {
	    if ((int)*sp > max) max=*sp;
	    if ((int)*sp < min) min=*sp;
	}
	hf->hf_min = min * hf->hf_file2mm;
	hf->hf_max = max * hf->hf_file2mm;
    } else {
	fastf_t max, min;
	int len;
	int i;
	fastf_t *fp;

	fp = (fastf_t *) hf->hf_mp->apbuf;
	min = max = *fp++;
	len = hf->hf_w * hf->hf_n;
	for (i=1; i < len; i++, fp++) {
	    if (*fp > max) max = *fp;
	    if (*fp < min) min = *fp;
	}
	hf->hf_min = min * hf->hf_file2mm;
	hf->hf_max = max * hf->hf_file2mm;
    }

    VSCALE(height, hf->hf_N, hf->hf_max);
    VADD2(hf->hf_VO, hf->hf_VO, height);

    /*
     * Now we compute the bounding box and sphere.
     */
    VMOVE(stp->st_min, hf->hf_V);
    VMOVE(stp->st_max, hf->hf_V);
    VADD2(work, hf->hf_V, height);
    VMINMAX(stp->st_min, stp->st_max, work);
    VJOIN1(work, hf->hf_V, hf->hf_Xlen, hf->hf_X);
    VMINMAX(stp->st_min, stp->st_max, work);
    VADD2(work, work, height);
    VMINMAX(stp->st_min, stp->st_max, work);
    VJOIN1(work, hf->hf_V, hf->hf_Ylen, hf->hf_Y);
    VMINMAX(stp->st_min, stp->st_max, work);
    VADD2(work, work, height);
    VMINMAX(stp->st_min, stp->st_max, work);
    VJOIN2(work, hf->hf_V, hf->hf_Xlen, hf->hf_X, hf->hf_Ylen, hf->hf_Y);
    VMINMAX(stp->st_min, stp->st_max, work);
    VADD2(work, work, height);
    VMINMAX(stp->st_min, stp->st_max, work);
    /* Now find the center and radius for a bounding sphere. */
    {
	fastf_t dx, dy, dz;
	fastf_t f;

	VADD2SCALE(stp->st_center, stp->st_max, stp->st_min, 0.5);

	dx = (stp->st_max[X] - stp->st_min[X])/2;
	dy = (stp->st_max[Y] - stp->st_min[Y])/2;
	dz = (stp->st_max[Z] - stp->st_min[Z])/2;
	f = dx;
	if (dy > f) f = dy;
	if (dz > f) f = dz;
	stp->st_aradius = f;
	stp->st_bradius = sqrt(dx*dx + dy*dy + dz*dz);
    }
    return 0;
}


/**
 * R T _ H F _ P R I N T
 */
void
rt_hf_print(const struct soltab *stp)
{
    const struct hf_specific *hf =
	(struct hf_specific *)stp->st_specific;
    VPRINT("V", hf->hf_V);
    VPRINT("X", hf->hf_X);
    VPRINT("Y", hf->hf_Y);
    VPRINT("N", hf->hf_N);
    bu_log("XL %g\n", hf->hf_Xlen);
    bu_log("YL %g\n", hf->hf_Ylen);
}


static int
hf_cell_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct hit *hitp, int xCell, int yCell)
{
    struct hf_specific *hfp =
	(struct hf_specific *)stp->st_specific;

    fastf_t dn, abs_dn, k1st=0, k2nd=0, alpha, beta;
    int dir1st, dir2nd;
    vect_t wxb, xp;
    vect_t tri_wn1st, tri_wn2nd, tri_BA1st, tri_BA2nd;
    vect_t tri_CA1st, tri_CA2nd;
    vect_t xvect, yvect, tri_A, tri_B, tri_C;
    int fnd1, fnd2;
    double hf2mm = hfp->hf_file2mm;

    if (ap) RT_CK_APPLICATION(ap);

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("hf_cell_shot(%s): %d, %d\n", stp->st_name,
	       xCell, yCell);
    }
    {
	fastf_t scale;
	scale = hfp->hf_Xlen/((double)hfp->hf_w-1);
	VSCALE(xvect, hfp->hf_X, scale);
	scale = hfp->hf_Ylen/((double)hfp->hf_n-1);
	VSCALE(yvect, hfp->hf_Y, scale);
    }
    if (hfp->hf_shorts) {
	unsigned short *sp;
	sp = (unsigned short *)hfp->hf_mp->apbuf +
	    yCell * hfp->hf_w + xCell;

	/* Get the points of one of the triangles */

	/* 0, 0 -> tri_A */
	VJOIN3(tri_A, hfp->hf_V, *sp*hf2mm, hfp->hf_N, xCell+0, xvect,
	       yCell+0, yvect);
	sp++;
	/* 1, 0 -> tri_B */
	VJOIN3(tri_B, hfp->hf_V, *sp*hf2mm, hfp->hf_N, xCell+1, xvect,
	       yCell+0, yvect);
	sp += hfp->hf_w;
	/* 1, 1 -> tri_C */
	VJOIN3(tri_C, hfp->hf_V, *sp*hf2mm, hfp->hf_N, xCell+1, xvect,
	       yCell+1, yvect);

	VSUB2(tri_CA1st, tri_C, tri_A);
	VSUB2(tri_BA1st, tri_B, tri_A);
	VCROSS(tri_wn1st, tri_BA1st, tri_CA1st);
	VMOVE(tri_B, tri_C);		/* This can optimize down. */
	--sp;

	/* 0, 1 */
	VJOIN3(tri_C, hfp->hf_V, *sp*hf2mm, hfp->hf_N, xCell+0, xvect,
	       yCell+1, yvect);
	VSUB2(tri_CA2nd, tri_C, tri_A);

	VSUB2(tri_BA2nd, tri_B, tri_A);
	VCROSS(tri_wn2nd, tri_BA2nd, tri_CA2nd);
    } else {
	double *fp;
	fp = (double *)hfp->hf_mp->apbuf +
	    yCell * hfp->hf_w + xCell;
	/* 0, 0 -> A */
	VJOIN3(tri_A, hfp->hf_V, *fp*hf2mm, hfp->hf_N, xCell+0, xvect,
	       yCell+0, yvect);
	fp++;
	/* 1, 0 */
	VJOIN3(tri_B, hfp->hf_V, *fp*hf2mm, hfp->hf_N, xCell+1, xvect,
	       yCell+0, yvect);
	fp += hfp->hf_w;
	/* 1, 1 */
	VJOIN3(tri_C, hfp->hf_V, *fp*hf2mm, hfp->hf_N, xCell+1, xvect,
	       yCell+1, yvect);
	VSUB2(tri_CA1st, tri_C, tri_A);
	VSUB2(tri_BA1st, tri_B, tri_A);
	VCROSS(tri_wn1st, tri_BA1st, tri_CA1st);
	VMOVE(tri_B, tri_C);		/* This can optimize down. */
	--fp;
	/* 0, 1 */
	VJOIN3(tri_C, hfp->hf_V, *fp*hf2mm, hfp->hf_N, xCell+0, xvect,
	       yCell+1, yvect);
	VSUB2(tri_CA2nd, tri_C, tri_A);
	VSUB2(tri_BA2nd, tri_B, tri_A);
	VCROSS(tri_wn2nd, tri_BA2nd, tri_CA2nd);
    }

    /*	0, 1		1, 1
     *	  o		o
     *	  	    _
     *	  ^          //|
     *   CA2nd|         //
     *	  |        //
     *	  |  BA2nd//
     *	  |      //
     *	  |     // CA1st
     *	  |    //
     *	  |   //
     *	  |  //
     *	  | //
     *
     *	  o  -------->  o
     *	0, 0	BA1st	1, 0
     *
     * wn1st and wn2nd are non-unit normal vectors pointing out of screen
     */

    fnd1 = fnd2 = 0;

    /* Ray triangle intersection.
     * See: "Graphics Gems" An Efficient Ray-Polygon Intersection P:390
     */

    dn = VDOT(tri_wn1st, rp->r_dir); /* wn1st points out */
    if (dn >= 0.0) {
	dir1st = 1;
	abs_dn=dn;
    } else {
	dir1st = 0;
	abs_dn = -dn;
    }

    /* make sure ray not parallel to plane of triangle */
    if (abs_dn >= SQRT_SMALL_FASTF) {
	VSUB2(wxb, tri_A, rp->r_pt);
	VCROSS(xp, wxb, rp->r_dir);

	/* alpha = dist along CA1 to isect pt */
	alpha = VDOT(tri_CA1st, xp);
	if (dn < 0.0) alpha = -alpha;

	/* if pt before CA1st or beyond end of CA1st pt is
	 * outside triangle.
	 *
	 * XXX Can someone explain the "alpha <= abs_dn" part? -- Lee
	 * I know it's supposed to be determining if the point
	 * is beyond the end of CA1st, but I don't see how the math
	 * here does that.
	 */
	if (alpha >= 0.0 && alpha <= abs_dn) {

	    /* beta = dist along BA1 to isect pt */
	    beta = VDOT(tri_BA1st, xp);
	    if (dn > 0.0) beta = -beta;

	    if (beta >= 0.0 && beta <= abs_dn) {
		if (alpha + beta <= abs_dn) {
		    k1st = VDOT(wxb, tri_wn1st) / dn;
		    fnd1 = 1;
		}
	    }
	}
    }

    /* XXX This is really hard to read.  Need to fix this like above */
    dn = VDOT(tri_wn2nd, rp->r_dir);
    if (dn >= 0.0) {
	dir2nd = 1;
	abs_dn = dn;
    } else {
	dir2nd = 0;
	abs_dn = -dn;
    }
    if (abs_dn < SQRT_SMALL_FASTF) goto leave;
    VSUB2(wxb, tri_A, rp->r_pt);
    VCROSS(xp, wxb, rp->r_dir);
    alpha = VDOT(tri_CA2nd, xp);
    if (dn <0.0) alpha = -alpha;
    if (alpha < 0.0 || alpha > abs_dn) goto leave;
    beta = VDOT(tri_BA2nd, xp);
    if (dn > 0.0) beta = -beta;
    if (beta < 0.0 || beta > abs_dn) goto leave;
    if (alpha+beta > abs_dn) goto leave;
    k2nd = VDOT(wxb, tri_wn2nd)/ dn;
    fnd2 = 1;
 leave:
    if (!fnd1 && !fnd2) return 0;

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("hf_cell_shot: hit(%d).\n", fnd1+fnd2);
    }

    /*
     * XXX - This is now wrong.
     *
     * We have now done the ray-triangle intersection.  dn1st and dn
     * tell us the direction of the normal, <0 is in and >0 is out.
     * k1st and k2nd tell us the distance from the start point.
     *
     * We are only interested in the closest hit. and that will
     * replace the out if dn>0 or the in if dn<0.
     */

    if (!fnd2) {
	hitp->hit_magic = RT_HIT_MAGIC;
	hitp->hit_dist = k1st;
	VMOVE(hitp->hit_normal, tri_wn1st);
	VUNITIZE(hitp->hit_normal);
	hitp->hit_surfno = yCell*hfp->hf_w+xCell;
	return 1;
    }
    if (!fnd1) {
	hitp->hit_magic = RT_HIT_MAGIC;
	hitp->hit_dist = k2nd;
	VMOVE(hitp->hit_normal, tri_wn2nd);
	VUNITIZE(hitp->hit_normal);
	hitp->hit_surfno = yCell*hfp->hf_w+xCell;
	return 1;
    }
    /*
     * This is the two hit situation which can cause interesting
     * problems.  Three are basicly five different cases that must be
     * dealt with and each one requires that the ray be classified
     *
     * 1) The ray has hit two different planes at two different
     *    locations (k1st != k2nd).  Return both hit points.
     * 2) The ray is going from inside to outside, return one hit point.
     * 3) The ray is going from outside to inside, return one hit point.
     * 4) The ray is going from inside to inside, return two hit points.
     * 5) The ray is going from outside to outside, return two hit points.
     */
    hitp->hit_magic = RT_HIT_MAGIC;
    hitp->hit_dist = k1st;
    VMOVE(hitp->hit_normal, tri_wn1st);
    VUNITIZE(hitp->hit_normal);
    hitp->hit_surfno = yCell*hfp->hf_w+xCell;
    hitp++;

    if ((fabs(k1st-k2nd) <= SMALL_FASTF) &&
	(dir1st + dir2nd != 1)) return 1;

    hitp->hit_magic = RT_HIT_MAGIC;
    hitp->hit_dist = k2nd;
    VMOVE(hitp->hit_normal, tri_wn2nd);
    VUNITIZE(hitp->hit_normal);
    hitp->hit_surfno = yCell*hfp->hf_w+xCell;
    return 2;
}


#define MAXHITS 128		/* # of surfaces hit, must be even */

/**
 * For the given plane of the bounding box of the hf solid, compute
 * the hit distance and add it to the list of hits.  If the plane
 * happens to be the "Zmax" face, then the hit is really in the
 * elevation data, and we skip it.  That will be handled elsewhere.
 */
static void
axis_plane_isect(int plane, fastf_t inout, struct xray *rp, struct hf_specific *hf, double xWidth, double yWidth, struct hit **hp, int *nhits)
{
    double left, right, xx=0, xright=0, answer;
    vect_t loc;
    int CellX=0, CellY=0;

    if (plane == -6) return;

    if (plane == -3) {
	(*hp)->hit_magic = RT_HIT_MAGIC;
	(*hp)->hit_dist = inout;
	(*hp)->hit_surfno = plane;
	(*hp)++;
	if ((*nhits)++>=MAXHITS) bu_bomb("g_hf.c: too many hits.\n");
	return;
    }

    VJOIN1(loc, rp->r_pt, inout, rp->r_dir);
    VSUB2(loc, loc, hf->hf_V);

    /* find the left, right and xx */
    switch (plane) {
	case -1:
	    CellY = loc[Y]/ yWidth;
	    CellX = 0;
	    xright = yWidth;
	    xx = loc[Y] - (CellY* yWidth);
	    break;
	case -2:
	    CellY = 0;
	    CellX = loc[X]/ xWidth;
	    xright = xWidth;
	    xx = loc[X] - CellX * xWidth;
	    break;
	case -4:
	    CellY = loc[Y]/ yWidth;
	    CellX = hf->hf_n-1;
	    xright = yWidth;
	    xx = loc[Y] - (CellY* yWidth);
	    break;
	case -5:
	    CellY = hf->hf_w-1;
	    CellX = loc[X]/ xWidth;
	    xright = xWidth;
	    xx = loc[X] - CellX* xWidth;
	    break;
    }
#if 1 /* What does this indicate that it generates so much noise? */
    if (xx < 0) {
	bu_log("hf: xx < 0, plane = %d\n", plane);
    }
#endif

    if (hf->hf_shorts) {
	unsigned short *sp;
	sp = (unsigned short *)hf->hf_mp->apbuf +
	    CellY * hf->hf_w + CellX;
	left = *sp;
	if (plane == -2 || plane == -5) {
	    sp++;
	} else {
	    sp += hf->hf_w;
	}
	right = *sp;
    } else {
	double *fp;
	fp = (double *) hf->hf_mp->apbuf +
	    CellY * hf->hf_w + CellX;
	left = *fp;
	if (plane == -2 || plane == -5) {
	    fp++;
	} else {
	    fp += hf->hf_w;
	}
	right = *fp;
    }
    left *= hf->hf_file2mm;
    right *= hf->hf_file2mm;
    answer = (right-left)/xright*xx+left;

    if (loc[Z]-SQRT_SMALL_FASTF < answer) {
	(*hp)->hit_magic = RT_HIT_MAGIC;
	(*hp)->hit_dist = inout;
	(*hp)->hit_surfno = plane;
	VJOIN1((*hp)->hit_point, rp->r_pt, inout, rp->r_dir);
	(*hp)++;
	if ((*nhits)++>=MAXHITS) bu_bomb("g_hf.c: too many hits.\n");
    }
}


/**
 * R T _ H T F _ S H O T
 *
 * Intersect a ray with a height field.  If an intersection occurs, a
 * struct seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_hf_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct hf_specific *hf =
	(struct hf_specific *)stp->st_specific;

    struct hit hits[MAXHITS];
    struct hit *hp;
    int nhits;
    double xWidth, yWidth;

    vect_t peqn;
    fastf_t pdist=0;
    fastf_t allDist[6];	/* The hit point for all rays. */
    fastf_t cosine;

    int iplane, oplane, j;
    fastf_t in, out;
    vect_t aray, curloc;

    VSETALL(peqn, 0);

    memset(hits, 0, sizeof(hits));

    in = -INFINITY;
    out = INFINITY;
    iplane = oplane = 0;

    nhits=0;
    hp = &hits[0];


    /*
     * First step in raytracing the HF is to find the intersection of
     * the ray with the bounding box.  Since the ray might not even
     * hit the box.
     *
     * The results of this intercept can be used to find which cell of
     * the dda is the start cell.
     */
    for (j=-1; j>-7; j--) {
	fastf_t dn;	/* Direction dot Normal */
	fastf_t dxbdn;	/* distence beteen d and b * dn */
	fastf_t s;	/* actual distence in mm */
	int allIndex;

	switch (j) {
	    case -1:
		/* Xmin plane */
		VREVERSE(peqn, hf->hf_X);
		pdist = VDOT(peqn, hf->hf_V);
		break;
	    case -2:
		/* Ymin plane */
		VREVERSE(peqn, hf->hf_Y);
		pdist = VDOT(peqn, hf->hf_V);
		break;
	    case -3:
		/* Zmin plane */
		VREVERSE(peqn, hf->hf_N);
		pdist = VDOT(peqn, hf->hf_V);
		break;
	    case -4:
		/* Xmax plane */
		VMOVE(peqn, hf->hf_X);
		pdist = VDOT(peqn, hf->hf_VO);
		break;
	    case -5:
		/* Ymax plane */
		VMOVE(peqn, hf->hf_Y);
		pdist = VDOT(peqn, hf->hf_VO);
		break;
	    case -6:
		/* Zmax plane */
		VMOVE(peqn, hf->hf_N);
		pdist = VDOT(peqn, hf->hf_VO);
		break;
	}
	allIndex = abs(j)-1;

	dxbdn = VDOT(peqn, rp->r_pt) - pdist;
	dn = -VDOT(peqn, rp->r_dir);
	if (RT_G_DEBUG & DEBUG_HF) {
	    VPRINT("hf: Plane Equation", peqn);
	    bu_log("hf: dn=%g, dxbdn=%g, ", dn, dxbdn);
	}

	if (dn < -SQRT_SMALL_FASTF) {
	    /* Leaving */
	    allDist[allIndex] = s = dxbdn/dn;
	    if (out > s) {
		out = s;
		oplane = j;
	    }
	    if (RT_G_DEBUG & DEBUG_HF) {
		bu_log("s=%g out=%g\n", s, out);
	    }
	} else if (dn > SQRT_SMALL_FASTF) {
	    /* entering */
	    allDist[allIndex] = s = dxbdn/dn;
	    if (in < s) {
		in = s;
		iplane = j;
	    }
	    if (RT_G_DEBUG & DEBUG_HF) {
		bu_log("s=%g in=%g\n", s, in);
	    }
	} else {
	    /*
	     * if the ray is outside the solid, then this
	     * is a miss.
	     */
	    if (RT_G_DEBUG & DEBUG_HF) {
		bu_log("s=DIVIDE_BY_ZERO\n");
	    }
	    if (dxbdn > SQRT_SMALL_FASTF) {
		return 0; /* MISS */
	    }
	    allDist[allIndex] = INFINITY;
	}
	if (in > out) {
	    if (RT_G_DEBUG & DEBUG_HF) {
		bu_log("rt_hf_shoot(%s): in(%g) > out(%g)\n",
		       stp->st_name, in, out);
	    }
	    return 0;	/* MISS */
	}
    }

    if (iplane >= 0 || oplane >= 0) {
	bu_log("rt_hf_shoot(%s): 1 hit => MISS\n",
	       stp->st_name);
	return 0;	/* MISS */
    }

    if (fabs(in-out) <= SMALL_FASTF  || out >= INFINITY) {
	if (RT_G_DEBUG & DEBUG_HF) {
	    bu_log("rt_hf_shoot(%s): in(%g) >= out(%g) || out >= INFINITY\n",
		   stp->st_name, in, out);
	}
	return 0;
    }

    /*
     * Once that translation is done, we start a DDA to walk across
     * the field.  Checking each "cell" and when changing cells in the
     * off direction, the 2 cells filling the corner are also checked.
     */

    xWidth = hf->hf_Xlen/((double)(hf->hf_w-1));
    yWidth = hf->hf_Ylen/((double)(hf->hf_n-1));

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("hf: xWidth=%g, yWidth=%g, in=%g, out=%g\n", xWidth,
	       yWidth, in, out);
    }


    /*
     * add the sides, and bottom to the hit list.
     */
    axis_plane_isect(iplane,  in, rp, hf, xWidth, yWidth, &hp, &nhits);
    axis_plane_isect(oplane, out, rp, hf, xWidth, yWidth, &hp, &nhits);

    /*
     * Gee, we've gotten much closer, we know that we hit the the
     * solid. Now it's time to see which cell we hit.  The Key here is
     * to use a fast DDA to check ONLY the cells we are interested in.
     * The basic idea and some of the pseudo code comes from:
     *
     * Grid Tracing: Fast Ray Tracing for Height Fields By: F. Kenton
     * Musgrave.
     */

    /*
     * Now we figure out which direction we are going to be moving, X,
     * Y or Z.
     */
    {
	vect_t tmp;
	VMOVE(tmp, rp->r_dir);
	tmp[Z] = 0.0;	/* XXX Bogus?  Assumes X, Y in XY plane */
	VUNITIZE(tmp);
	cosine = VDOT(tmp, hf->hf_X);
    }
    if (fabs(cosine) <= SMALL_FASTF) {
	/* near enough to Z */
	vect_t tmp;
	int xCell, yCell, r;
	if (RT_G_DEBUG & DEBUG_HF) {
	    bu_log("hf: Vertical shoot\n");
	}
	VSUB2(tmp, rp->r_pt, hf->hf_V);
	xCell = tmp[X]/hf->hf_Xlen*hf->hf_w;
	yCell = tmp[Y]/hf->hf_Ylen*hf->hf_n;
	r = hf_cell_shot(stp, rp, ap, hp, xCell, yCell);
	if (r) {
	    nhits += r;
	    if (nhits > MAXHITS)
		bu_bomb("g_hf.c: too many hits.\n");
	    hp += r;
	}
    } else if (cosine*cosine > 0.5) {
	double tmp;
	double farZ, minZ, maxZ;
	int xCell, yCell, signX, signY;
	double highest, lowest, error, delta;
	double deltaZ;

	vect_t goesIN, goesOUT;

	VJOIN1(goesIN, rp->r_pt, allDist[3], rp->r_dir); /* Xmax plane */
	VJOIN1(goesOUT, rp->r_pt, allDist[0], rp->r_dir); /* Xmin plane */
	VSUB2(aray, goesOUT, goesIN);
	VSUB2(curloc, goesIN, hf->hf_V);


	/*
	 * We will be stepping one "cell width" in the X direction
	 * each time through the loop.  In simple case we have???
	 * cell_width = htfp->htf_Xlen/(htfp->htf_i-1); deltaX =
	 * (Xdist*cell_width)/Xdist deltaY = (Ydist*cell_width)/Xdist;
	 */
	tmp = xWidth/fabs(aray[X]);

	VSCALE(aray, aray, tmp);

	/*
	 * Some fudges here.  First, the size of the array of samples
	 * is iXj, but the size of the array of CELLS is (i-1)X(j-1),
	 * therefore number of CELLS is one less than number of
	 * samples and the scaling factor is used.  Second, the math
	 * is nice to us.  IF we are entering at the far end
	 * (curloc[X] == Xlen || curloc[Y] == Ylen) then the result we
	 * will get back is of the cell following this (out of bounds)
	 * So we add a check for that problem.
	 */
	xCell = curloc[X]/xWidth;
	tmp = curloc[Y];
	if (tmp < 0) {
	    yCell = tmp/yWidth - 1;
	} else {
	    yCell = tmp/yWidth;
	}

	signX = (aray[X] < 0.0) ? -1 : 1;
	signY = (aray[Y] < 0.0) ? -1 : 1;

	if (RT_G_DEBUG & DEBUG_HF) {
	    bu_log("hf: curloc=(%g, %g, %g) aray=(%g, %g, %g)\n", curloc[X], curloc[Y],
		   curloc[Z], aray[X], aray[Y], aray[Z]);
	    bu_log("hf: from=(%g, %g) to=(%g, %g)\n",
		   goesIN[X]/xWidth,
		   goesIN[Y]/yWidth,
		   goesOUT[X]/xWidth,
		   goesOUT[Y]/yWidth);
	}
	error = curloc[Y]-yCell*yWidth;
	error /= yWidth;

	delta = aray[Y]/fabs(aray[X]);

	if (delta < 0.0) {
	    delta = -delta;
	    error = -error;
	} else {
	    error -= 1.0;
	}

	/*
	 * if at the far end (Xlen) then we need to move one step
	 * forward along aray.
	 */
	if (xCell >= hf->hf_w-1) {
	    xCell+=signX;
	    error += delta;
	}
	if (RT_G_DEBUG & DEBUG_HF) {
	    bu_log("hf: delta=%g, error=%g, %d, %d\n",
		   delta, error, xCell, yCell);
	}


	deltaZ = (aray[Z] < 0) ? -aray[Z] : aray[Z];
	do {
	    farZ = curloc[Z] + aray[Z];
	    maxZ = (curloc[Z] > farZ) ? curloc[Z] : farZ;
	    minZ = (curloc[Z] < farZ) ? curloc[Z] : farZ;
	    if (RT_G_DEBUG & DEBUG_HF) {
		bu_log("hf: cell %d, %d [%g -- %g]",
		       xCell, yCell, minZ, maxZ);
	    }
	    /*
	     * Are we on the grid yet?  If not, then we will check for
	     * a side step and inc.
	     */
	    /* CTJ - Or maxZ < hf->hf_min then no chance to hit */
	    if (yCell < 0 || yCell > hf->hf_n-2) {
		if (error > -SQRT_SMALL_FASTF) {
		    if (yCell >= -1) goto skip_first;
		    yCell += signY;
		    error -= 1.0;
		}
		xCell += signX;
		error += delta;
		VADD2(curloc, curloc, aray);
		continue;
	    }

	    /*
	     * Get the min/max of the four corners of a given cell.
	     * Since the data in memory can be in one of two forms,
	     * unsigned short and double, we have this simple if
	     * statement around the find min/max to reference the data
	     * correctly.
	     */
	    if (hf->hf_shorts) {
		unsigned short *sp;
		sp = (unsigned short *)hf->hf_mp->apbuf +
		    yCell * hf->hf_w + xCell;
		/* 0, 0 */
		highest = lowest = *sp++;
		/* 1, 0 */
		if (lowest > (double)*sp) lowest=*sp;
		if (highest < (double)*sp) highest=*sp;
		sp+=hf->hf_w;
		/* 1, 1 */
		if (lowest > (double)*sp) lowest=*sp;
		if (highest < (double)*sp) highest=*sp;
		sp--;
		/* 0, 1 */
		if (lowest > (double)*sp) lowest = *sp;
		if (highest < (double)*sp) highest = *sp;
		lowest *= hf->hf_file2mm;
		highest *= hf->hf_file2mm;
	    } else {
		double *fp;
		fp = (double *)hf->hf_mp->apbuf +
		    yCell * hf->hf_w + xCell;
		/* 0, 0 */
		highest = lowest = *fp++;
		/* 1, 0 */
		if (lowest > *fp) lowest=*fp;
		if (highest < *fp) highest=*fp;
		fp+=hf->hf_w;
		/* 1, 1 */
		if (lowest > *fp) lowest=*fp;
		if (highest < *fp) highest=*fp;
		fp--;
		/* 0, 1 */
		if (lowest > *fp) lowest = *fp;
		if (highest < *fp) highest = *fp;
		lowest *= hf->hf_file2mm;
		highest *= hf->hf_file2mm;
	    }

	    if (RT_G_DEBUG & DEBUG_HF) {
		bu_log("lowest=%g, highest=%g\n",
		       lowest, highest);
	    }

	    /* This is the primary test.  It is designed to get all
	     * cells that the ray passes through.
	     */
	    if (maxZ+deltaZ > lowest &&
		minZ-deltaZ < highest) {
		int r = hf_cell_shot(stp, rp, ap, hp, xCell, yCell);
		if (r) {
		    nhits += r;
		    if (nhits >= MAXHITS)
			bu_bomb("g_hf.c: too many hits.\n");
		    hp += r;
		}
	    }
	    /* This is the DDA trying to fill in the corners as it
	     * walks the path.
	     */
	skip_first:
	    if (error > SQRT_SMALL_FASTF) {
		yCell += signY;
		if (RT_G_DEBUG & DEBUG_HF) {
		    bu_log("hf: cell %d, %d ", xCell, yCell);
		}
		if ((yCell < 0) || yCell > hf->hf_n-2) {
		    error -= 1.0;
		    xCell += signX;
		    error += delta;
		    VADD2(curloc, curloc, aray);
		    continue;
		}
		if (hf->hf_shorts) {
		    unsigned short *sp;
		    sp = (unsigned short *)hf->hf_mp->apbuf +
			yCell * hf->hf_w + xCell;
		    /* 0, 0 */
		    highest = lowest = *sp++;
		    /* 1, 0 */
		    if (lowest > (double)*sp) lowest=*sp;
		    if (highest < (double)*sp) highest=*sp;
		    sp+=hf->hf_w;
		    /* 1, 1 */
		    if (lowest > (double)*sp) lowest=*sp;
		    if (highest < (double)*sp) highest=*sp;
		    sp--;
		    /* 0, 1 */
		    if (lowest > (double)*sp) lowest = *sp;
		    if (highest < (double)*sp) highest = *sp;
		    lowest *= hf->hf_file2mm;
		    highest *= hf->hf_file2mm;
		} else {
		    double *fp;
		    fp = (double *)hf->hf_mp->apbuf +
			yCell * hf->hf_w + xCell;
		    /* 0, 0 */
		    highest = lowest = *fp++;
		    /* 1, 0 */
		    if (lowest > *fp) lowest=*fp;
		    if (highest < *fp) highest=*fp;
		    fp+=hf->hf_w;
		    /* 1, 1 */
		    if (lowest > *fp) lowest=*fp;
		    if (highest < *fp) highest=*fp;
		    fp--;
		    /* 0, 1 */
		    if (lowest > *fp) lowest = *fp;
		    if (highest < *fp) highest = *fp;
		    lowest *= hf->hf_file2mm;
		    highest *= hf->hf_file2mm;
		}
		if (maxZ+deltaZ > lowest &&
		    minZ-deltaZ < highest) {
		    int r = hf_cell_shot(stp, rp, ap, hp, xCell, yCell);
		    /* DO HIT */
		    if (r) {
			nhits += r;
			if (nhits >= MAXHITS)
			    bu_bomb("g_hf.c: too many hits.\n");
			hp+=r;
		    }
		}
		error -= 1.0;
	    } else if (error > -SQRT_SMALL_FASTF) {
		yCell += signY;
		error -= 1.0;
	    }
	    xCell += signX;
	    error += delta;
	    VADD2(curloc, curloc, aray);
	} while (xCell >= 0 && xCell < hf->hf_w-1);
	if (RT_G_DEBUG & DEBUG_HF) {
	    bu_log("htf: leaving loop, %d, %d, %g vs. 0--%d, 0--%d, 0.0--%g\n",
		   xCell, yCell, curloc[Z], hf->hf_w-1, hf->hf_n-1, hf->hf_max);
	}

    } else {
	/* OTHER HALF */

	double tmp;
	double farZ, minZ, maxZ;
	double deltaZ;
	int xCell, yCell, signX, signY;
	double highest, lowest, error, delta;

	vect_t goesIN, goesOUT;

	VJOIN1(goesIN, rp->r_pt, allDist[4], rp->r_dir);
	VJOIN1(goesOUT, rp->r_pt, allDist[1], rp->r_dir);
	VSUB2(aray, goesOUT, goesIN);
	VSUB2(curloc, goesIN, hf->hf_V);


	/*
	 * We will be stepping one "cell width" in the X direction
	 * each time through the loop.  In simple case we have???
	 * cell_width = htfp->htf_Xlen/(htfp->htf_i-1);
	 * deltaX = (Xdist*cell_width)/Xdist
	 * deltaY = (Ydist*cell_width)/Xdist;
	 */
	tmp = yWidth/fabs(aray[Y]);

	VSCALE(aray, aray, tmp);

	/*
	 * Some fudges here.  First, the size of the array of samples
	 * is iXj, but the size of the array of CELLS is (i-1)X(j-1),
	 * therefore number of CELLS is one less than number of
	 * samples and the scaling factor is used.  Second, the math
	 * is nice to us.  IF we are entering at the far end
	 * (curloc[X] == Xlen || curloc[Y] == Ylen) then the result we
	 * will get back is of the cell following this (out of bounds)
	 * So we add a check for that problem.
	 */
	yCell = curloc[Y]/yWidth;
	tmp = curloc[X];
	if (tmp < 0) {
	    xCell = tmp/xWidth - 1;
	} else {
	    xCell = tmp/xWidth;
	}

	signX = (aray[X] < 0.0) ? -1 : 1;
	signY = (aray[Y] < 0.0) ? -1 : 1;

	if (RT_G_DEBUG & DEBUG_HF) {
	    bu_log("hf: curloc=(%g, %g, %g) aray=(%g, %g, %g)\n", curloc[X], curloc[Y],
		   curloc[Z], aray[X], aray[Y], aray[Z]);
	    bu_log("hf: from=(%g, %g) to=(%g, %g)\n",
		   goesIN[X]/xWidth,
		   goesIN[Y]/yWidth,
		   goesOUT[X]/xWidth,
		   goesOUT[Y]/yWidth);
	}
	error = curloc[X]-xCell*xWidth;
	error /= xWidth;

/* delta = aray[X]/xWidth; */
	delta = aray[X]/fabs(aray[Y]);

	if (delta < 0.0) {
	    delta = -delta;
	    error = -error;
	} else {
	    error -= 1.0;
	}

	/*
	 * if at the far end (Ylen) then we need to move one step
	 * forward along aray.
	 */
	if (yCell >= hf->hf_n-1) {
	    yCell+=signY;
	    error += delta;
	}
	if (RT_G_DEBUG & DEBUG_HF) {
	    bu_log("hf: delta=%g, error=%g, %d, %d\n",
		   delta, error, xCell, yCell);
	}

	deltaZ = (aray[Z] < 0) ? -aray[Z] : aray[Z];
	do {
	    farZ = curloc[Z] + aray[Z];
	    maxZ = (curloc[Z] > farZ) ? curloc[Z] : farZ;
	    minZ = (curloc[Z] < farZ) ? curloc[Z] : farZ;
	    if (RT_G_DEBUG & DEBUG_HF) {
		bu_log("hf: cell %d, %d [%g -- %g] ",
		       xCell, yCell, minZ, maxZ);
	    }
	    /* CTJ - Or maxZ < hf->hf_min */
	    if (xCell < 0 || xCell > hf->hf_w-2) {
		if (error > -SQRT_SMALL_FASTF) {
		    if (xCell >= -1) goto skip_2nd;
		    xCell += signX;
		    error -= 1.0;
		}
		yCell += signY;
		error += delta;
		VADD2(curloc, curloc, aray);
		continue;
	    }

	    if (hf->hf_shorts) {
		unsigned short *sp;
		sp = (unsigned short *)hf->hf_mp->apbuf +
		    yCell * hf->hf_w + xCell;
		/* 0, 0 */
		highest = lowest = *sp++;
		/* 1, 0 */
		if (lowest > (double)*sp) lowest=*sp;
		if (highest < (double)*sp) highest=*sp;
		sp+=hf->hf_w;
		/* 1, 1 */
		if (lowest > (double)*sp) lowest=*sp;
		if (highest < (double)*sp) highest=*sp;
		sp--;
		/* 0, 1 */
		if (lowest > (double)*sp) lowest = *sp;
		if (highest < (double)*sp) highest = *sp;
		lowest *= hf->hf_file2mm;
		highest *= hf->hf_file2mm;
	    } else {
		double *fp;
		fp = (double *)hf->hf_mp->apbuf +
		    yCell * hf->hf_w + xCell;
		/* 0, 0 */
		highest = lowest = *fp++;
		/* 1, 0 */
		if (lowest > *fp) lowest=*fp;
		if (highest < *fp) highest=*fp;
		fp+=hf->hf_w;
		/* 1, 1 */
		if (lowest > *fp) lowest=*fp;
		if (highest < *fp) highest=*fp;
		fp--;
		/* 0, 1 */
		if (lowest > *fp) lowest = *fp;
		if (highest < *fp) highest = *fp;
		lowest *= hf->hf_file2mm;
		highest *= hf->hf_file2mm;
	    }


	    if (RT_G_DEBUG & DEBUG_HF) {
		bu_log("lowest=%g, highest=%g\n",
		       lowest, highest);
	    }

	    /* This is the primary test.  It is designed to get all
	     * cells that the ray passes through.
	     */
	    if (maxZ+deltaZ > lowest &&
		minZ-deltaZ < highest) {
		int r = hf_cell_shot(stp, rp, ap, hp, xCell, yCell);
		if (r) {
		    nhits += r;
		    if (nhits >= MAXHITS)
			bu_bomb("g_hf.c: too many hits.\n");
		    hp+=r;
		}
	    }
	    /* This is the DDA trying to fill in the corners as it
	     * walks the path.
	     */
	skip_2nd:
	    if (error > SQRT_SMALL_FASTF) {
		xCell += signX;
		if (RT_G_DEBUG & DEBUG_HF) {
		    bu_log("hf: cell %d, %d\n", xCell, yCell);
		}
		if ((xCell < 0) || xCell > hf->hf_w-2) {
		    error -= 1.0;
		    yCell += signY;
		    error += delta;
		    VADD2(curloc, curloc, aray);
		    continue;
		}
		if (hf->hf_shorts) {
		    unsigned short *sp;
		    sp = (unsigned short *)hf->hf_mp->apbuf +
			yCell * hf->hf_w + xCell;
		    /* 0, 0 */
		    highest = lowest = *sp++;
		    /* 1, 0 */
		    if (lowest > (double)*sp) lowest=*sp;
		    if (highest < (double)*sp) highest=*sp;
		    sp+=hf->hf_w;
		    /* 1, 1 */
		    if (lowest > (double)*sp) lowest=*sp;
		    if (highest < (double)*sp) highest=*sp;
		    sp--;
		    /* 0, 1 */
		    if (lowest > (double)*sp) lowest = *sp;
		    if (highest < (double)*sp) highest = *sp;
		    lowest *= hf->hf_file2mm;
		    highest *= hf->hf_file2mm;
		} else {
		    double *fp;
		    fp = (double *)hf->hf_mp->apbuf +
			yCell * hf->hf_w + xCell;
		    /* 0, 0 */
		    highest = lowest = *fp++;
		    /* 1, 0 */
		    if (lowest > *fp) lowest=*fp;
		    if (highest < *fp) highest=*fp;
		    fp+=hf->hf_w;
		    /* 1, 1 */
		    if (lowest > *fp) lowest=*fp;
		    if (highest < *fp) highest=*fp;
		    fp--;
		    /* 0, 1 */
		    if (lowest > *fp) lowest = *fp;
		    if (highest < *fp) highest = *fp;
		    lowest *= hf->hf_file2mm;
		    highest *= hf->hf_file2mm;
		}
		if (maxZ+deltaZ > lowest &&
		    minZ-deltaZ < highest) {
		    int r = hf_cell_shot(stp, rp, ap, hp, xCell, yCell);
		    /* DO HIT */
		    if (r) {
			nhits += r;
			if (nhits >= MAXHITS)
			    bu_bomb("g_hf.c: too many hits.\n");
			hp += r;
		    }
		}
		error -= 1.0;
	    } else if (error > -SQRT_SMALL_FASTF) {
		xCell += signX;
		error -= 1.0;
	    }
	    yCell += signY;
	    error += delta;
	    VADD2(curloc, curloc, aray);
	} while (yCell >= 0 && yCell < hf->hf_n-1);
	if (RT_G_DEBUG & DEBUG_HF) {
	    bu_log("htf: leaving loop, %d, %d, %g vs. 0--%d, 0--%d, 0.0--%g\n",
		   xCell, yCell, curloc[Z], hf->hf_w-1, hf->hf_n-1, hf->hf_max);
	}
    }

    /* Sort hits, near to Far */
    {
	int i;
	struct hit tmp;
	for (i=0; i< nhits-1; i++) {
	    for (j=i+1; j<nhits; j++) {
		if (hits[i].hit_dist <= hits[j].hit_dist) continue;
		tmp = hits[j];
		hits[j]=hits[i];
		hits[i]=tmp;
	    }
	}
    }

    if (nhits & 1) {
	int i;
	static int nerrors = 0;
	hits[nhits] = hits[nhits-1];	/* struct copy*/
	VREVERSE(hits[nhits].hit_normal, hits[nhits-1].hit_normal);
	nhits++;
	if (nerrors++ < 300) {
	    bu_log("rt_hf_shot(%s): %d hit(s)@ %d %d: ", stp->st_name, nhits-1, ap->a_x, ap->a_y);
	    for (i=0; i< nhits; i++) {
		bu_log("%f(%d), ", hits[i].hit_dist, hits[i].hit_surfno);
	    }
	    bu_log("\n");
	}
    }

    /* nhits is even, build segments */
    {
	struct seg *segp;
	int i;
	for (i=0; i< nhits; i+=2) {
	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;
	    segp->seg_in = hits[i];
	    segp->seg_out= hits[i+1];
	    BU_LIST_INSERT(&(seghead->l), &(segp->l));
	}
    }
    return nhits;	/* hits or misses */
}


/**
 * R T _ H F _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_hf_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    struct hf_specific *hf =
	(struct hf_specific *)stp->st_specific;
    int j;

    j = hitp->hit_surfno;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    if (j >= 0) {
	/* Normals computed in rt_htf_shot, nothing to do here. */
	return;
    }

    switch (j) {
	case -1:
	    VREVERSE(hitp->hit_normal, hf->hf_X);
	    break;
	case -2:
	    VREVERSE(hitp->hit_normal, hf->hf_Y);
	    break;
	case -3:
	    VREVERSE(hitp->hit_normal, hf->hf_N);
	    break;
	case -4:
	    VMOVE(hitp->hit_normal, hf->hf_X);
	    break;
	case -5:
	    VMOVE(hitp->hit_normal, hf->hf_Y);
	    break;
	case -6:
	    VMOVE(hitp->hit_normal, hf->hf_N);
	    break;
    }
    VUNITIZE(hitp->hit_normal);

}


/**
 * R T _ H F _ C U R V E
 *
 * Return the curvature of the hf.
 */
void
rt_hf_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    struct hf_specific *hf = (struct hf_specific *)stp->st_specific;
    if (!hf || !cvp || !hitp)
	return;
    RT_CK_HIT(hitp);

    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
}


/**
 * R T _ H F _ U V
 *
 * For a hit on the surface of an hf, return the (u, v) coordinates of
 * the hit point, 0 <= u, v <= 1.
 *
 * u = azimuth
 * v = elevation
 */
void
rt_hf_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    struct hf_specific *hf = (struct hf_specific *)stp->st_specific;
    vect_t delta;
    fastf_t r = 0;

    if (ap) RT_CK_APPLICATION(ap);
    if (!hf || !hitp || !uvp || !stp)
	return;
    RT_CK_HIT(hitp);

    VSUB2(delta, hitp->hit_point, hf->hf_V);
    uvp->uv_u = delta[X] / hf->hf_Xlen;
    uvp->uv_v = delta[Y] / hf->hf_Ylen;
    r = 0.0;
    if (uvp->uv_u < 0.0) uvp->uv_u=0.0;
    if (uvp->uv_u > 1.0) uvp->uv_u=1.0;
    if (uvp->uv_v < 0.0) uvp->uv_v=0.0;
    if (uvp->uv_v > 1.0) uvp->uv_v=1.0;

    uvp->uv_du = r;
    uvp->uv_dv = r;
}


/**
 * R T _ H F _ F R E E
 */
void
rt_hf_free(struct soltab *stp)
{
    struct hf_specific *hf =
	(struct hf_specific *)stp->st_specific;

    if (hf->hf_mp) {
	bu_close_mapped_file(hf->hf_mp);
	hf->hf_mp = (struct bu_mapped_file *)0;
    }
    bu_free((char *)hf, "hf_specific");
}


/**
 * R T _ H F _ C L A S S
 */
int
rt_hf_class(void)
{
    return 0;
}


/**
 * R T _ H F _ P L O T
 */
int
rt_hf_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *UNUSED(tol))
{
    struct rt_hf_internal *xip;
    unsigned short *sp = (unsigned short *)NULL;
    double *dp;
    vect_t xbasis;
    vect_t ybasis;
    vect_t zbasis;
    point_t start;
    point_t cur;
    size_t x;
    size_t y;
    int cmd;
    int step;
    int half_step;
    int goal;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    xip = (struct rt_hf_internal *)ip->idb_ptr;
    RT_HF_CK_MAGIC(xip);

    VSCALE(xbasis, xip->x, xip->xlen / (xip->w - 1));
    VSCALE(ybasis, xip->y, xip->ylen / (xip->n - 1));
    VCROSS(zbasis, xip->x, xip->y);
    VSCALE(zbasis, zbasis, xip->zscale * xip->file2mm);

    /* XXX This should be set from the tessellation tolerance */
    goal = 20000;

    /* Draw the 4 corners of the base plate */
    RT_ADD_VLIST(vhead, xip->v, BN_VLIST_LINE_MOVE);

    VJOIN1(start, xip->v, xip->xlen, xip->x);
    RT_ADD_VLIST(vhead, start, BN_VLIST_LINE_DRAW);

    VJOIN2(start, xip->v, xip->xlen, xip->x, xip->ylen, xip->y);
    RT_ADD_VLIST(vhead, start, BN_VLIST_LINE_DRAW);

    VJOIN1(start, xip->v, xip->ylen, xip->y);
    RT_ADD_VLIST(vhead, start, BN_VLIST_LINE_DRAW);

    RT_ADD_VLIST(vhead, xip->v, BN_VLIST_LINE_DRAW);
    goal -= 5;

#define HF_GET(_p, _x, _y)	((_p)[(_y)*xip->w+(_x)])
    /*
     * Draw the four "ridge lines" at full resolution, for edge matching.
     */
    if (xip->shorts) {
	/* X direction, Y=0, with edges down to base */
	RT_ADD_VLIST(vhead, xip->v, BN_VLIST_LINE_MOVE);
	sp = &HF_GET((unsigned short *)xip->mp->apbuf, 0, 0);
	for (x = 0; x < xip->w; x++) {
	    VJOIN2(cur, xip->v, x, xbasis, *sp, zbasis);
	    RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);
	    sp++;
	}
	VJOIN1(cur, xip->v, xip->xlen, xip->x);
	RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);

	/* X direction, Y=n-1, with edges down to base */
	VJOIN1(start, xip->v, xip->ylen, xip->y);
	RT_ADD_VLIST(vhead, start, BN_VLIST_LINE_MOVE);
	sp = &HF_GET((unsigned short *)xip->mp->apbuf, 0, xip->n - 1);
	VJOIN1(start, xip->v, xip->ylen, xip->y);
	for (x = 0; x < xip->w; x++) {
	    VJOIN2(cur, start, x, xbasis, *sp, zbasis);
	    RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);
	    sp++;
	}
	VJOIN2(cur, xip->v, xip->xlen, xip->x, xip->ylen, xip->y);
	RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);

	/* Y direction, X=0 */
	cmd = BN_VLIST_LINE_MOVE;
	sp = &HF_GET((unsigned short *)xip->mp->apbuf, 0, 0);
	for (y = 0; y < xip->n; y++) {
	    VJOIN2(cur, xip->v, y, ybasis, *sp, zbasis);
	    RT_ADD_VLIST(vhead, cur, cmd);
	    cmd = BN_VLIST_LINE_DRAW;
	    sp += xip->w;
	}

	/* Y direction, X=w-1 */
	cmd = BN_VLIST_LINE_MOVE;
	sp = &HF_GET((unsigned short *)xip->mp->apbuf, xip->w - 1, 0);
	VJOIN1(start, xip->v, xip->xlen, xip->x);
	for (y = 0; y < xip->n; y++) {
	    VJOIN2(cur, start, y, ybasis, *sp, zbasis);
	    RT_ADD_VLIST(vhead, cur, cmd);
	    cmd = BN_VLIST_LINE_DRAW;
	    sp += xip->w;
	}
    } else {
	/* X direction, Y=0, with edges down to base */
	RT_ADD_VLIST(vhead, xip->v, BN_VLIST_LINE_MOVE);
	dp = &HF_GET((double *)xip->mp->apbuf, 0, 0);
	for (x = 0; x < xip->w; x++) {
	    VJOIN2(cur, xip->v, x, xbasis, *dp, zbasis);
	    RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);
	    dp++;
	}
	VJOIN1(cur, xip->v, xip->xlen, xip->x);
	RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);

	/* X direction, Y=n-1, with edges down to base */
	VJOIN1(start, xip->v, xip->ylen, xip->y);
	RT_ADD_VLIST(vhead, start, BN_VLIST_LINE_MOVE);
	dp = &HF_GET((double *)xip->mp->apbuf, 0, xip->n - 1);
	VJOIN1(start, xip->v, xip->ylen, xip->y);
	for (x = 0; x < xip->w; x++) {
	    VJOIN2(cur, start, x, xbasis, *dp, zbasis);
	    RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);
	    dp++;
	}
	VJOIN2(cur, xip->v, xip->xlen, xip->x, xip->ylen, xip->y);
	RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);

	/* Y direction, X=0 */
	cmd = BN_VLIST_LINE_MOVE;
	dp = &HF_GET((double *)xip->mp->apbuf, 0, 0);
	for (y = 0; y < xip->n; y++) {
	    VJOIN2(cur, xip->v, y, ybasis, *dp, zbasis);
	    RT_ADD_VLIST(vhead, cur, cmd);
	    cmd = BN_VLIST_LINE_DRAW;
	    sp += xip->w;
	}

	/* Y direction, X=w-1 */
	cmd = BN_VLIST_LINE_MOVE;
	dp = &HF_GET((double *)xip->mp->apbuf, xip->w - 1, 0);
	VJOIN1(start, xip->v, xip->xlen, xip->x);
	for (y = 0; y < xip->n; y++) {
	    VJOIN2(cur, start, y, ybasis, *dp, zbasis);
	    RT_ADD_VLIST(vhead, cur, cmd);
	    cmd = BN_VLIST_LINE_DRAW;
	    dp += xip->w;
	}
    }
    goal -= 4 + 2 * (xip->w + xip->n);

    /* Apply relative tolerance, if specified */
    if (!ZERO(ttol->rel)) {
	size_t rstep;
	rstep = xip->w;
	V_MAX(rstep, xip->n);
	step = ttol->rel * rstep;
    } else {
	/* No relative tol specified, limit drawing to 'goal' # of vectors */
	if (goal <= 0) return 0;		/* no vectors for interior */

	/* Compute data stride based upon producing no more than 'goal' vectors */
	step = ceil(sqrt(2*(xip->w-1)*(xip->n-1) / (double)goal));
    }
    if (step < 1) step = 1;
    if ((half_step = step/2) < 1) half_step = 1;

    /* Draw the contour lines in W (x) direction.  Don't redo ridges. */
    for (y = half_step; y < xip->n-half_step; y += step) {
	VJOIN1(start, xip->v, y, ybasis);
	x = 0;
	if (xip->shorts) {
	    sp = &HF_GET((unsigned short *)xip->mp->apbuf, x, y);
	    VJOIN2(cur, start, x, xbasis, *sp, zbasis);
	    RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_MOVE);
	    x += half_step;
	    sp = &HF_GET((unsigned short *)xip->mp->apbuf, x, y);
	    for (; x < xip->w; x += step) {
		VJOIN2(cur, start, x, xbasis, *sp, zbasis);
		RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);
		sp += step;
	    }
	    if (x != step+xip->w-1+step) {
		x = xip->w - 1;
		sp = &HF_GET((unsigned short *)xip->mp->apbuf, x, y);
		VJOIN2(cur, start, x, xbasis, *sp, zbasis);
		RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);
	    }
	} else {
	    /* doubles */
	    dp = &HF_GET((double *)xip->mp->apbuf, x, y);
	    VJOIN2(cur, start, x, xbasis, *dp, zbasis);
	    RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_MOVE);
	    x += half_step;
	    dp = &HF_GET((double *)xip->mp->apbuf, x, y);
	    for (; x < xip->w; x+=step) {
		VJOIN2(cur, start, x, xbasis, *dp, zbasis);
		RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);
		dp += step;
	    }
	    if (x != step+xip->w-1+step) {
		x = xip->w - 1;
		dp = &HF_GET((double *)xip->mp->apbuf, x, y);
		VJOIN2(cur, start, x, xbasis, *dp, zbasis);
		RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);
	    }
	}
    }

    /* Draw the contour lines in the N (y) direction */
    if (xip->shorts) {
	for (x = half_step; x < xip->w-half_step; x += step) {
	    VJOIN1(start, xip->v, x, xbasis);
	    y = 0;
	    sp = &HF_GET((unsigned short *)xip->mp->apbuf, x, y);
	    VJOIN2(cur, start, y, ybasis, *sp, zbasis);
	    RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_MOVE);
	    y += half_step;
	    for (; y < xip->n; y += step) {
		sp = &HF_GET((unsigned short *)xip->mp->apbuf, x, y);
		VJOIN2(cur, start, y, ybasis, *sp, zbasis);
		RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);
	    }
	    if (y != step+xip->n-1+step) {
		y = xip->n - 1;
		sp = &HF_GET((unsigned short *)xip->mp->apbuf, x, y);
		VJOIN2(cur, start, y, ybasis, *sp, zbasis);
		RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);
	    }
	}
    } else {
	/* doubles */
	for (x = half_step; x < xip->w-half_step; x += step) {
	    VJOIN1(start, xip->v, x, xbasis);
	    y = 0;
	    dp = &HF_GET((double *)xip->mp->apbuf, x, y);
	    VJOIN2(cur, start, y, ybasis, *dp, zbasis);
	    RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_MOVE);
	    y += half_step;
	    for (; y < xip->n; y += step) {
		dp = &HF_GET((double *)xip->mp->apbuf, x, y);
		VJOIN2(cur, start, y, ybasis, *dp, zbasis);
		RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);
	    }
	    if (y != step+xip->n-1+step) {
		y = xip->n - 1;
		dp = &HF_GET((double *)xip->mp->apbuf, x, y);
		VJOIN2(cur, start, y, ybasis, *dp, zbasis);
		RT_ADD_VLIST(vhead, cur, BN_VLIST_LINE_DRAW);
	    }
	}
    }
    return 0;
}


/**
 * R T _ H F _ T E S S
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_hf_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
{
    struct rt_hf_internal *xip;

    RT_CK_DB_INTERNAL(ip);
    xip = (struct rt_hf_internal *)ip->idb_ptr;
    RT_HF_CK_MAGIC(xip);

    if (r) *r = NULL;
    if (m) NMG_CK_MODEL(m);

    return -1;
}


/**
 * R T _ H F _ I M P O R T
 *
 * Import an HF from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_hf_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_hf_internal *xip;
    union record *rp;
    struct bu_vls str;
    struct bu_mapped_file *mp;
    vect_t tmp;
    int in_cookie;	/* format cookie */
    int in_len;
    int out_cookie;
    size_t count;
    size_t got;

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != DBID_STRSOL) {
	bu_log("rt_hf_import4: defective record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_HF;
    ip->idb_meth = &rt_functab[ID_HF];
    ip->idb_ptr = bu_calloc(1, sizeof(struct rt_hf_internal), "rt_hf_internal");
    xip = (struct rt_hf_internal *)ip->idb_ptr;
    xip->magic = RT_HF_INTERNAL_MAGIC;

    /* Provide defaults.  Only non-defaulted fields are dfile, w, n */
    xip->shorts = 1;		/* for now */
    xip->file2mm = 1.0;
    VSETALL(xip->v, 0);
    VSET(xip->x, 1, 0, 0);
    VSET(xip->y, 0, 1, 0);
    xip->xlen = 1000;
    xip->ylen = 1000;
    xip->zscale = 1;
    bu_strlcpy(xip->fmt, "nd", sizeof(xip->fmt));

    /* Process parameters found in .g file */
    bu_vls_init(&str);
    bu_vls_strcpy(&str, rp->ss.ss_args);
    if (bu_struct_parse(&str, rt_hf_parse, (char *)xip) < 0) {
	bu_vls_free(&str);
    err1:
	bu_free((char *)xip, "rt_hf_import4: xip");
	ip->idb_type = ID_NULL;
	ip->idb_ptr = (genptr_t)NULL;
	return -2;
    }
    bu_vls_free(&str);

    /* If "cfile" was specified, process parameters from there */
    if (xip->cfile[0]) {
	FILE *fp;

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fp = fopen(xip->cfile, "rb");
	bu_semaphore_release(BU_SEM_SYSCALL);
	if (!fp) {
	    perror(xip->cfile);
	    bu_log("rt_hf_import4() unable to open cfile=%s\n", xip->cfile);
	    goto err1;
	}
	bu_vls_init(&str);
	while (bu_vls_gets(&str, fp) >= 0)
	    bu_vls_strcat(&str, " ");
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fclose(fp);
	bu_semaphore_release(BU_SEM_SYSCALL);
	if (bu_struct_parse(&str, rt_hf_cparse, (char *)xip) < 0) {
	    bu_log("rt_hf_import4() parse error in cfile input '%s'\n",
		   bu_vls_addr(&str));
	    bu_vls_free(&str);
	    goto err1;
	}
    }

    /* Check for reasonable values */
    if (!xip->dfile[0]) {
	/* XXX Should create 2x2 data file instead, for positioning use (FPO) */
	bu_log("rt_hf_import4() no dfile specified\n");
	goto err1;
    }
    if (xip->w < 2 || xip->n < 2) {
	bu_log("rt_hf_import4() w=%zu, n=%zu too small\n", xip->w, xip->n);
	goto err1;
    }
    if (xip->xlen <= 0 || xip->ylen <= 0) {
	bu_log("rt_hf_import4() xlen=%g, ylen=%g too small\n", xip->xlen, xip->ylen);
	goto err1;
    }

    /* Apply modeling transformations */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(tmp, mat, xip->v);
    VMOVE(xip->v, tmp);
    MAT4X3VEC(tmp, mat, xip->x);
    VMOVE(xip->x, tmp);
    MAT4X3VEC(tmp, mat, xip->y);
    VMOVE(xip->y, tmp);
    xip->xlen /= mat[15];
    xip->ylen /= mat[15];
    xip->zscale /= mat[15];

    VUNITIZE(xip->x);
    VUNITIZE(xip->y);

    /* Prepare for cracking input file format */
    if ((in_cookie = bu_cv_cookie(xip->fmt)) == 0) {
	bu_log("rt_hf_import4() fmt='%s' unknown\n", xip->fmt);
	goto err1;
    }
    in_len = bu_cv_itemlen(in_cookie);

    /*
     * Load data file, and transform to internal format
     */
    mp = bu_open_mapped_file(xip->dfile, "hf");
    if (!mp) {
	bu_log("rt_hf_import4() unable to open '%s'\n", xip->dfile);
	goto err1;
    }
    xip->mp = mp;
    count = mp->buflen / in_len;

    /* If this data has already been mapped, all done */
    if (mp->apbuf) return 0;		/* OK */

    /* Transform external data to internal format -- short or double */
    if (xip->shorts) {
	mp->apbuflen = sizeof(unsigned short) * count;
	out_cookie = bu_cv_cookie("hus");
    } else {
	mp->apbuflen = sizeof(double) * count;
	out_cookie = bu_cv_cookie("hd");
    }

    if (bu_cv_optimize(in_cookie) == bu_cv_optimize(out_cookie)) {
	/* Don't replicate the data, just re-use the pointer */
	mp->apbuf = mp->buf;
	return 0;		/* OK */
    }

    mp->apbuf = (genptr_t)bu_malloc(mp->apbuflen, "rt_hf_import4 apbuf[]");
    got = bu_cv_w_cookie(mp->apbuf, out_cookie, mp->apbuflen,
			 mp->buf, in_cookie, count);
    if (got != count) {
	bu_log("rt_hf_import4(%s) bu_cv_w_cookie count=%zu, got=%zu\n",
	       xip->dfile, count, got);
    }

    return 0;			/* OK */
}


/**
 * R T _ H F _ E X P O R T
 *
 * The name is added by the caller, in the usual place.
 *
 * The meaning of the export here is slightly different than that of
 * most other solids.  The cfile and dfile are not modified, only
 * changes to the string solid parameters are placed back into the .g
 * file.  Note that any parameters taken from a cfile are included in
 * the new string solid.  This isn't a problem, because if the cfile
 * is changed (perhaps to substitute a different resolution height
 * field of the same location in space), its new parameters will
 * override those stored in the string solid (including the dfile
 * name).
 */
int
rt_hf_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_hf_internal *xip;
    union record *rec;
    struct bu_vls str;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_HF) return -1;
    xip = (struct rt_hf_internal *)ip->idb_ptr;
    RT_HF_CK_MAGIC(xip);

    /* Apply any scale transformation */
    xip->xlen /= local2mm;
    xip->ylen /= local2mm;
    xip->zscale /= local2mm;

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record) * DB_SS_NGRAN;
    ep->ext_buf = (genptr_t)bu_calloc(1, ep->ext_nbytes, "hf external");
    rec = (union record *)ep->ext_buf;

    bu_vls_init(&str);
    bu_vls_struct_print(&str, rt_hf_parse, (char *)xip);

    /* Any changes made by solid editing affect .g file only, and not
     * the cfile, if specified.
     */

    rec->s.s_id = DBID_STRSOL;
    bu_strlcpy(rec->ss.ss_keyword, "hf", sizeof(rec->ss.ss_keyword));
    bu_strlcpy(rec->ss.ss_args, bu_vls_addr(&str), DB_SS_LEN);
    bu_vls_free(&str);

    return 0;
}


int
rt_hf_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);
    if (!ep || !mat) return -1;
    if (dbip) RT_CK_DBI(dbip);

    bu_log("As of release 6.0 the HF primitive is superceded by the DSP primitive.\n");
    bu_log("\tTo convert HF primitives to DSP, use 'dbupgrade'.\n");
    /* The rt_hf_to_dsp() routine can also be used */
    return -1;
}


int
rt_hf_export5(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *dbip)
{
    if (!ep) return -1;
    if (ip) RT_CK_DB_INTERNAL(ip);
    if (dbip) RT_CK_DBI(dbip);

    bu_log("As of release 6.0 the HF primitive is superceded by the DSP primitive.\n");
    bu_log("\tTo convert HF primitives to DSP, use 'dbupgrade'.\n");
    /* The rt_hf_to_dsp() routine can also be used */
    return -1;
}


/**
 * R T _ H F _ D E S C R I B E
 *
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_hf_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_hf_internal *xip;

    BU_CK_VLS(str);

    xip = (struct rt_hf_internal *)ip->idb_ptr;
    RT_HF_CK_MAGIC(xip);

    bu_vls_printf(str, "Height Field (HF) mm2local=%g\n", mm2local);
    if (!verbose)
	return 0;

    bu_vls_struct_print(str, rt_hf_parse, ip->idb_ptr);
    bu_vls_strcat(str, "\n");

    return 0;
}


/**
 * R T _ H F _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_hf_ifree(struct rt_db_internal *ip)
{
    struct rt_hf_internal *xip;

    RT_CK_DB_INTERNAL(ip);
    xip = (struct rt_hf_internal *)ip->idb_ptr;
    RT_HF_CK_MAGIC(xip);
    xip->magic = 0;			/* sanity */

    if (xip->mp) {
	BU_CK_MAPPED_FILE(xip->mp);
	bu_close_mapped_file(xip->mp);
    }

    bu_free((char *)xip, "hf ifree");
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
}
/**
 * R T _ H F _ P A R A M S
 *
 */
int
rt_hf_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
