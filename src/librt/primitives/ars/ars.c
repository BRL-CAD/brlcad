/*                           A R S . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2022 United States Government as represented by
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
/** @file primitives/ars/ars.c
 *
 * Intersect a ray with an ARS (Arbitrary faceted solid).
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "bnetwork.h"

#include "bu/cv.h"
#include "vmath.h"
#include "rt/db4.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "rt/primitives/bot.h"

#include "../../librt_private.h"


#define IJ(ii, jj)	(((i+(ii))*(arip->pts_per_curve+1))+(j+(jj)))
#define ARS_PT(ii, jj)	(&arip->curves[i+(ii)][(j+(jj))*ELEMENTS_PER_VECT])
#define FIND_IJ(a, b)	\
	if (!(verts[IJ(a, b)])) { \
		verts[IJ(a, b)] = \
		nmg_find_pnt_in_shell(s, ARS_PT(a, b), tol); \
	}
#define ASSOC_GEOM(corn, a, b)	\
	if (!((*corners[corn])->vg_p)) { \
		nmg_vertex_gv(*(corners[corn]), ARS_PT(a, b)); \
	}


/* Describe algorithm here */

/* from g_bot.c */
extern int rt_bot_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip);
extern void rt_bot_ifree(struct rt_db_internal *ip);


/**
 * reads a set of ARS B records and returns a pointer to memory
 * allocated for holding the curve's fastf_t values.
 */
HIDDEN fastf_t *
ars_rd_curve(union record *rp, size_t npts, int flip)
{
    size_t lim;
    fastf_t *base;
    register fastf_t *fp;		/* pointer to temp vector */
    size_t i;
    union record *rr;
    size_t rec;

    /* Leave room for first point to be repeated */
    base = fp = (fastf_t *)bu_malloc(
	(npts+1) * sizeof(fastf_t) * ELEMENTS_PER_VECT,
	"ars curve");

    rec = 0;
    for (; npts > 0; npts -= 8) {
	rr = &rp[rec++];
	if (rr->b.b_id != ID_ARS_B) {
	    bu_log("ars_rd_curve(npts=%zu):  non-ARS_B record [%d]!\n", npts, rr->b.b_id);
	    break;
	}
	lim = (npts>8) ? 8 : npts;
	for (i = 0; i < lim; i++) {
	    vect_t vec;

	    /* cvt from dbfloat_t */
	    flip_fastf_float(vec, (&(rr->b.b_values[i*3])), 1, flip);
	    VMOVE(fp, vec);

	    fp += ELEMENTS_PER_VECT;
	}
    }
    return base;
}


/**
 * Read all the curves in as a two dimensional array.  The caller is
 * responsible for freeing the dynamic memory.
 *
 * Note that in each curve array, the first point is replicated as the
 * last point, to make processing the data easier.
 */
int
rt_ars_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_ars_internal *ari;
    union record *rp;
    register size_t i, j;
    vect_t base_vect;
    size_t currec;

    VSETALL(base_vect, 0);

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    if (rp->u_id != ID_ARS_A) {
	bu_log("rt_ars_import4: defective record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_ARS;
    ip->idb_meth = &OBJ[ID_ARS];
    BU_ALLOC(ip->idb_ptr, struct rt_ars_internal);

    ari = (struct rt_ars_internal *)ip->idb_ptr;
    ari->magic = RT_ARS_INTERNAL_MAGIC;

    if (dbip && dbip->dbi_version < 0) {
	ari->ncurves = flip_short(rp[0].a.a_m);
	ari->pts_per_curve = flip_short(rp[0].a.a_n);
    } else {
	ari->ncurves = rp[0].a.a_m;
	ari->pts_per_curve = rp[0].a.a_n;
    }

    /*
     * Read all the curves into internal form.
     */
    ari->curves = (fastf_t **)bu_malloc(
	(ari->ncurves+1) * sizeof(fastf_t *), "ars curve ptrs");

    currec = 1;
    int cflag = (dbip && dbip->dbi_version < 0) ? 1 : 0;
    for (i = 0; i < ari->ncurves; i++) {
	ari->curves[i] = ars_rd_curve(&rp[currec], ari->pts_per_curve, cflag);
	currec += (ari->pts_per_curve+7)/8;
    }

    /* Convert from vector to point notation IN PLACE by rotating
     * vectors and adding base vector.  Observe special treatment for
     * base vector.
     */
    if (mat == NULL) mat = bn_mat_identity;
    for (i = 0; i < ari->ncurves; i++) {
	register fastf_t *v;

	v = ari->curves[i];
	for (j = 0; j < ari->pts_per_curve; j++) {
	    vect_t homog;

	    if (i == 0 && j == 0) {
		/* base vector */
		VMOVE(homog, v);
		MAT4X3PNT(base_vect, mat, homog);
		VMOVE(v, base_vect);
	    } else {
		MAT4X3VEC(homog, mat, v);
		VADD2(v, base_vect, homog);
	    }
	    v += ELEMENTS_PER_VECT;
	}
	VMOVE(v, ari->curves[i]);		/* replicate first point */
    }
    return 0;
}


/**
 * The name will be added by the caller.  Generally, only libwdb will
 * set conv2mm != 1.0
 */
int
rt_ars_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_ars_internal *arip;
    union record *rec;
    point_t base_pt;
    size_t per_curve_grans;
    size_t cur; /* current curve number */
    size_t gno; /* current granule number */

    RT_CK_DB_INTERNAL(ip);
    if (dbip) RT_CK_DBI(dbip);

    if (ip->idb_type != ID_ARS) return -1;
    arip = (struct rt_ars_internal *)ip->idb_ptr;
    RT_ARS_CK_MAGIC(arip);

    per_curve_grans = (arip->pts_per_curve+7)/8;

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = (1 + per_curve_grans * arip->ncurves) *
	sizeof(union record);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "ars external");
    rec = (union record *)ep->ext_buf;

    rec[0].a.a_id = ID_ARS_A;
    rec[0].a.a_type = ARS; /* obsolete? */
    rec[0].a.a_m = (short)arip->ncurves;
    rec[0].a.a_n = (short)arip->pts_per_curve;
    rec[0].a.a_curlen = (short)per_curve_grans;
    rec[0].a.a_totlen = (short)(per_curve_grans * arip->ncurves);

    VMOVE(base_pt, &arip->curves[0][0]);
    gno = 1;
    for (cur = 0; cur < arip->ncurves; cur++) {
	register fastf_t *fp;
	size_t npts;
	size_t left;

	fp = arip->curves[cur];
	left = arip->pts_per_curve;
	for (npts = 0; npts < arip->pts_per_curve; npts+=8, left -= 8) {
	    size_t el;
	    size_t lim;
	    register struct ars_ext *bp = &rec[gno].b;

	    bp->b_id = ID_ARS_B;
	    bp->b_type = ARSCONT;	/* obsolete? */
	    bp->b_n = (short)cur+1;		/* obsolete? */
	    bp->b_ngranule = (short)((npts/8)+1); /* obsolete? */

	    lim = (left > 8) ? 8 : left;
	    for (el = 0; el < lim; el++) {
		vect_t diff;
		if (cur == 0 && npts == 0 && el == 0) {
		    VSCALE(diff, fp, local2mm);
		} else {
		    VSUB2SCALE(diff, fp, base_pt, local2mm);
		}
		/* NOTE: also type converts to dbfloat_t */
		VMOVE(&(bp->b_values[el*3]), diff);
		fp += ELEMENTS_PER_VECT;
	    }
	    gno++;
	}
    }
    return 0;
}


/**
 * Read all the curves in as a two dimensional array.  The caller is
 * responsible for freeing the dynamic memory.
 *
 * Note that in each curve array, the first point is replicated as the
 * last point, to make processing the data easier.
 */
int
rt_ars_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_ars_internal *ari;
    register size_t i, j;
    register unsigned char *cp;
    register fastf_t *fp;

    /* must be double for import and export */
    double tmp_pnt[ELEMENTS_PER_POINT];

    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_ARS;
    ip->idb_meth = &OBJ[ID_ARS];
    BU_ALLOC(ip->idb_ptr, struct rt_ars_internal);

    ari = (struct rt_ars_internal *)ip->idb_ptr;
    ari->magic = RT_ARS_INTERNAL_MAGIC;

    cp = (unsigned char *)ep->ext_buf;
    ari->ncurves = bu_ntohl(*(uint32_t *)cp, 0, UINT_MAX - 1);
    cp += SIZEOF_NETWORK_LONG;
    ari->pts_per_curve = bu_ntohl(*(uint32_t *)cp, 0, UINT_MAX - 1);
    cp += SIZEOF_NETWORK_LONG;

    /*
     * Read all the curves into internal form.
     */
    ari->curves = (fastf_t **)bu_calloc(
	(ari->ncurves+1), sizeof(fastf_t *), "ars curve ptrs");
    if (mat == NULL) mat = bn_mat_identity;
    for (i = 0; i < ari->ncurves; i++) {
	ari->curves[i] = (fastf_t *)bu_calloc((ari->pts_per_curve + 1) * ELEMENTS_PER_POINT,
					      sizeof(fastf_t), "ARS points");
	fp = ari->curves[i];
	for (j = 0; j < ari->pts_per_curve; j++) {
	    bu_cv_ntohd((unsigned char *)tmp_pnt, cp, ELEMENTS_PER_POINT);
	    MAT4X3PNT(fp, mat, tmp_pnt);
	    cp += ELEMENTS_PER_POINT * SIZEOF_NETWORK_DOUBLE;
	    fp += ELEMENTS_PER_POINT;
	}
	VMOVE(fp, ari->curves[i]);	/* duplicate first point */
    }
    return 0;
}


/**
 * The name will be added by the caller.  Generally, only libwdb will
 * set conv2mm != 1.0
 */
int
rt_ars_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_ars_internal *arip;
    unsigned char *cp;
    size_t cur;		/* current curve number */

    /* must be double for import and export */
    double tmp_pnt[ELEMENTS_PER_POINT];

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_ARS) return -1;
    arip = (struct rt_ars_internal *)ip->idb_ptr;
    RT_ARS_CK_MAGIC(arip);

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = 2 * SIZEOF_NETWORK_LONG + ELEMENTS_PER_POINT * arip->ncurves * arip->pts_per_curve * SIZEOF_NETWORK_DOUBLE;
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "ars external");
    cp = (unsigned char *)ep->ext_buf;

    *(uint32_t *)cp = htonl((unsigned long)arip->ncurves);
    cp += SIZEOF_NETWORK_LONG;
    *(uint32_t *)cp = htonl((unsigned long)arip->pts_per_curve);
    cp += SIZEOF_NETWORK_LONG;

    for (cur = 0; cur < arip->ncurves; cur++) {
	register fastf_t *fp;
	size_t npts;

	fp = arip->curves[cur];
	for (npts = 0; npts < arip->pts_per_curve; npts++) {
	    VSCALE(tmp_pnt, fp, local2mm);
	    bu_cv_htond(cp, (unsigned char *)tmp_pnt, ELEMENTS_PER_POINT);
	    cp += ELEMENTS_PER_POINT * SIZEOF_NETWORK_DOUBLE;
	    fp += ELEMENTS_PER_POINT;
	}
    }
    return 0;
}


/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_ars_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    register size_t  i, j;
    register struct rt_ars_internal *arip =
	(struct rt_ars_internal *)ip->idb_ptr;
    char buf[256];

    RT_ARS_CK_MAGIC(arip);
    bu_vls_strcat(str, "arbitrary rectangular solid (ARS)\n");

    sprintf(buf, "\t%lu curves, %lu points per curve\n", (long unsigned)arip->ncurves, (long unsigned)arip->pts_per_curve);
    bu_vls_strcat(str, buf);

    if (arip->ncurves > 0) {
	sprintf(buf, "\tV (%g, %g, %g)\n",
		INTCLAMP(arip->curves[0][X] * mm2local),
		INTCLAMP(arip->curves[0][Y] * mm2local),
		INTCLAMP(arip->curves[0][Z] * mm2local));
	bu_vls_strcat(str, buf);
    }

    if (!verbose) return 0;

    /* Print out all the points */
    for (i = 0; i < arip->ncurves; i++) {
	register fastf_t *v = arip->curves[i];

	sprintf(buf, "\tCurve %lu:\n", (long unsigned)i);
	bu_vls_strcat(str, buf);
	for (j = 0; j < arip->pts_per_curve; j++) {
	    sprintf(buf, "\t\t(%g, %g, %g)\n",
		    INTCLAMP(v[X] * mm2local),
		    INTCLAMP(v[Y] * mm2local),
		    INTCLAMP(v[Z] * mm2local));
	    bu_vls_strcat(str, buf);
	    v += ELEMENTS_PER_VECT;
	}
    }

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_ars_ifree(struct rt_db_internal *ip)
{
    register struct rt_ars_internal *arip;
    register size_t i;

    RT_CK_DB_INTERNAL(ip);
    arip = (struct rt_ars_internal *)ip->idb_ptr;
    RT_ARS_CK_MAGIC(arip);

    /*
     * Free storage for faces
     */
    for (i = 0; i < arip->ncurves; i++) {
	bu_free((char *)arip->curves[i], "ars curve");
    }
    bu_free((char *)arip->curves, "ars curve ptrs");
    arip->magic = 0;		/* sanity */
    arip->ncurves = 0;
    bu_free((char *)arip, "ars ifree");
    ip->idb_ptr = ((void *)0);	/* sanity */
}


int
rt_ars_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *tol)
{
    register size_t i;
    register size_t j;
    register size_t k;
    struct rt_ars_internal *arip;
    struct shell *s;
    struct vertex **verts;
    struct faceuse *fu;
    struct bu_ptbl kill_fus;
    int bad_ars = 0;

    RT_CK_DB_INTERNAL(ip);
    arip = (struct rt_ars_internal *)ip->idb_ptr;
    RT_ARS_CK_MAGIC(arip);

    /* Check for a legal ARS */
    for (i = 0; i < arip->ncurves-1; i++) {
	for (j = 2; j < arip->pts_per_curve; j++) {
	    fastf_t dist;
	    vect_t pca;
	    int code;

	    if (VNEAR_EQUAL(ARS_PT(0, -2), ARS_PT(0, -1), tol->dist))
		continue;

	    code = bg_dist_pnt3_lseg3(&dist, pca, ARS_PT(0, -2), ARS_PT(0, -1), ARS_PT(0, 0), tol);

	    if (code < 2) {
		bu_log("ARS curve backtracks on itself!!!\n");
		bu_log("\tCurve #%zu, points #%zu through %zu are:\n", i, j-2, j);
		bu_log("\t\t%zu (%f %f %f)\n", j-2, V3ARGS(ARS_PT(0, -2)));
		bu_log("\t\t%zu (%f %f %f)\n", j-1, V3ARGS(ARS_PT(0, -1)));
		bu_log("\t\t%zu (%f %f %f)\n", j, V3ARGS(ARS_PT(0, 0)));
		bad_ars = 1;
		j++;
	    }
	}
    }

    if (bad_ars) {
	bu_log("TESSELLATION FAILURE: This ARS solid has not been tessellated.\n\tAny result you may obtain is incorrect.\n");
	return -1;
    }

    bu_ptbl_init(&kill_fus, 64, " &kill_fus");

    /* Build the topology of the ARS.  Start by allocating storage */

    *r = nmg_mrsv(m);	/* Make region, empty shell, vertex */
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    verts = (struct vertex **)bu_calloc(arip->ncurves * (arip->pts_per_curve+1),
					sizeof(struct vertex *),
					"rt_ars_tess *verts[]");

    /*
     * Draw the "waterlines", by tracing each curve.  n+1th point is
     * first point replicated by import code.
     */
    k = arip->pts_per_curve-2;	/* next to last point on curve */
    for (i = 0; i < arip->ncurves-1; i++) {
	int double_ended;

	if (k != 1
	    && VNEAR_EQUAL(&arip->curves[i][1*ELEMENTS_PER_VECT], &arip->curves[i][k*ELEMENTS_PER_VECT], tol->dist))
	{
	    double_ended = 1;
	} else {
	    double_ended = 0;
	}

	for (j = 0; j < arip->pts_per_curve; j++) {
	    struct vertex **corners[3];


	    if (double_ended &&
		i != 0 &&
		(j == 0 || j == k || j == arip->pts_per_curve-1))
		continue;

	    /*
	     * First triangular face
	     */
	    if (bg_3pnts_distinct(ARS_PT(0, 0), ARS_PT(1, 1), ARS_PT(0, 1), tol)
		&& !bg_3pnts_collinear(ARS_PT(0, 0), ARS_PT(1, 1), ARS_PT(0, 1), tol))
	    {
		/* Locate these points, if previously mentioned */
		FIND_IJ(0, 0);
		FIND_IJ(1, 1);
		FIND_IJ(0, 1);

		/* Construct first face topology, CCW order */
		corners[0] = &verts[IJ(0, 0)];
		corners[1] = &verts[IJ(0, 1)];
		corners[2] = &verts[IJ(1, 1)];

		if ((fu = nmg_cmface(s, corners, 3)) == (struct faceuse *)0) {
		    bu_log("rt_ars_tess() nmg_cmface failed, skipping face a[%zu][%zu]\n",
			   i, j);
		}

		/* Associate vertex geometry, if new */
		ASSOC_GEOM(0, 0, 0);
		ASSOC_GEOM(1, 0, 1);
		ASSOC_GEOM(2, 1, 1);
		if (nmg_calc_face_g(fu,&RTG.rtg_vlfree)) {
		    bu_log("Degenerate face created, will kill it later\n");
		    bu_ptbl_ins(&kill_fus, (long *)fu);
		}
	    }

	    /*
	     * Second triangular face
	     */
	    if (bg_3pnts_distinct(ARS_PT(1, 0), ARS_PT(1, 1), ARS_PT(0, 0), tol)
		&& !bg_3pnts_collinear(ARS_PT(1, 0), ARS_PT(1, 1), ARS_PT(0, 0), tol))
	    {
		/* Locate these points, if previously mentioned */
		FIND_IJ(1, 0);
		FIND_IJ(1, 1);
		FIND_IJ(0, 0);

		/* Construct second face topology, CCW */
		corners[0] = &verts[IJ(1, 0)];
		corners[1] = &verts[IJ(0, 0)];
		corners[2] = &verts[IJ(1, 1)];

		if ((fu = nmg_cmface(s, corners, 3)) == (struct faceuse *)0) {
		    bu_log("rt_ars_tess() nmg_cmface failed, skipping face b[%zu][%zu]\n",
			   i, j);
		}

		/* Associate vertex geometry, if new */
		ASSOC_GEOM(0, 1, 0);
		ASSOC_GEOM(1, 0, 0);
		ASSOC_GEOM(2, 1, 1);
		if (nmg_calc_face_g(fu,&RTG.rtg_vlfree)) {
		    bu_log("Degenerate face created, will kill it later\n");
		    bu_ptbl_ins(&kill_fus, (long *)fu);
		}
	    }
	}
    }

    bu_free((char *)verts, "rt_ars_tess *verts[]");

    /* kill any degenerate faces that may have been created */
    for (i = 0; i < (size_t)BU_PTBL_LEN(&kill_fus); i++) {
	fu = (struct faceuse *)BU_PTBL_GET(&kill_fus, i);
	NMG_CK_FACEUSE(fu);
	(void)nmg_kfu(fu);
    }

    /* ARS solids are often built with incorrect face normals.  Don't
     * depend on them to be correct.
     */
    nmg_fix_normals(s, &RTG.rtg_vlfree, tol);

    /* set edge's is_real flag */
    nmg_mark_edges_real(&s->l.magic, &RTG.rtg_vlfree);

    /* Compute "geometry" for region and shell */
    nmg_region_a(*r, tol);

    nmg_shell_coplanar_face_merge(s, tol, 0, &RTG.rtg_vlfree);
    nmg_simplify_shell(s,&RTG.rtg_vlfree);

    return 0;
}


/**
 * TODO:  produces bboxes that are waaay too big.
 */
int
rt_ars_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol))
{
    register size_t i;
    register size_t j;
    struct rt_ars_internal *arip;

    RT_CK_DB_INTERNAL(ip);
    arip = (struct rt_ars_internal *)ip->idb_ptr;
    RT_ARS_CK_MAGIC(arip);

    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    /*
     * Iterate over the curves.
     */
    for (i = 0; i < arip->ncurves; i++) {
	register fastf_t *v1;

	v1 = arip->curves[i];
	VMINMAX((*min), (*max), v1);
	v1 += ELEMENTS_PER_VECT;
	for (j = 1; j <= arip->pts_per_curve; j++, v1 += ELEMENTS_PER_VECT)
	    VMINMAX((*min), (*max), v1);
    }

    return 0;
}


/**
 * This routine is used to prepare a list of planar faces for being
 * shot at by the ars routines.
 *
 * Process an ARS, which is represented as a vector from the origin to
 * the first point, and many vectors from the first point to the
 * remaining points.
 *
 * This routine is unusual in that it has to read additional database
 * records to obtain all the necessary information.
 */
int
rt_ars_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct model *m;
    struct nmgregion *r;
    struct shell *s;
    int ret;

    /*point_t min, max;*/
    /*if (rt_ars_bbox(ip, &min, &max, &rtip->rti_tol)) return -1;*/

    m = nmg_mm();
    r = BU_LIST_FIRST(nmgregion, &m->r_hd);

    if (rt_ars_tess(&r, m, ip, &rtip->rti_ttol, &rtip->rti_tol)) {
	bu_log("Failed to tessellate ARS (%s)\n", stp->st_dp->d_namep);
	nmg_km(m);
	return -1;
    }

    s = BU_LIST_FIRST(shell, &r->s_hd);
    bot = nmg_bot(s, &RTG.rtg_vlfree, &rtip->rti_tol);

    if (!bot) {
	bu_log("Failed to convert ARS to BOT (%s)\n", stp->st_dp->d_namep);
	nmg_km(m);
	return -1;
    }

    nmg_km(m);

    intern.idb_magic = RT_DB_INTERNAL_MAGIC;
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)bot;
    bu_avs_init(&intern.idb_avs, 0, "ARS to a BOT for prep");

    ret = rt_bot_prep(stp, &intern, rtip);

    rt_bot_ifree(&intern);

    /* Use the ars bbox results, rather than the BoT results */

    return ret;
}


int
rt_ars_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    register size_t i;
    register size_t j;
    struct rt_ars_internal *arip;
    struct bu_list *vlfree = &RTG.rtg_vlfree;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    arip = (struct rt_ars_internal *)ip->idb_ptr;
    RT_ARS_CK_MAGIC(arip);

    /*
     * Draw the "waterlines", by tracing each curve.  n+1th point is
     * first point replicated by code above.
     */
    for (i = 0; i < arip->ncurves; i++) {
	register fastf_t *v1;

	v1 = arip->curves[i];
	BV_ADD_VLIST(vlfree, vhead, v1, BV_VLIST_LINE_MOVE);
	v1 += ELEMENTS_PER_VECT;
	for (j = 1; j <= arip->pts_per_curve; j++, v1 += ELEMENTS_PER_VECT)
	    BV_ADD_VLIST(vlfree, vhead, v1, BV_VLIST_LINE_DRAW);
    }

    /* Connect the Ith points on each curve, to make a mesh.  */
    for (i = 0; i < arip->pts_per_curve; i++) {
	BV_ADD_VLIST(vlfree, vhead, &arip->curves[0][i*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
	for (j = 1; j < arip->ncurves; j++)
	    BV_ADD_VLIST(vlfree, vhead, &arip->curves[j][i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    return 0;
}


int
rt_ars_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    register struct rt_ars_internal *ars = (struct rt_ars_internal *)intern->idb_ptr;
    size_t i, j;

    RT_ARS_CK_MAGIC(ars);

    if (attr == (char *)NULL) {
	bu_vls_strcpy(logstr, "ars");
	bu_vls_printf(logstr, " NC %zu PPC %zu", ars->ncurves, ars->pts_per_curve);
	for (i = 0; i < ars->ncurves; i++) {
	    bu_vls_printf(logstr, " C%zu {", i);
	    for (j = 0; j < ars->pts_per_curve; j++) {
		bu_vls_printf(logstr, " { %.25g %.25g %.25g }",
			      V3ARGS(&ars->curves[i][j*3]));
	    }
	    bu_vls_printf(logstr, " }");
	}
    } else if (BU_STR_EQUAL(attr, "NC")) {
	bu_vls_printf(logstr, "%zu", ars->ncurves);
    } else if (BU_STR_EQUAL(attr, "PPC")) {
	bu_vls_printf(logstr, "%zu", ars->pts_per_curve);
    } else if (attr[0] == 'C') {
	const char *ptr;

	if (attr[1] == '\0') {
	    /* all the curves */
	    for (i = 0; i < ars->ncurves; i++) {
		bu_vls_printf(logstr, " C%zu {", i);
		for (j = 0; j < ars->pts_per_curve; j++) {
		    bu_vls_printf(logstr, " { %.25g %.25g %.25g }",
				  V3ARGS(&ars->curves[i][j*3]));
		}
		bu_vls_printf(logstr, " }");
	    }
	} else if (!isdigit((int)attr[1])) {
	    bu_vls_printf(logstr,
			  "ERROR: illegal argument, must be NC, PPC, C, C#, or C#P#\n");
	    return BRLCAD_ERROR;
	}

	ptr = strchr(attr, 'P');
	if (ptr) {
	    /* a specific point on a specific curve */
	    if (!isdigit((int)*(ptr+1))) {
		bu_vls_printf(logstr,
			      "ERROR: illegal argument, must be NC, PPC, C, C#, or C#P#\n");
		return BRLCAD_ERROR;
	    }
	    j = atoi((ptr+1));
	    /* FIXME: is this necessary? modifying a const char is illegal!
	    *ptr = '\0';
	    */
	    i = atoi(&attr[1]);
	    bu_vls_printf(logstr, "%.25g %.25g %.25g",
			  V3ARGS(&ars->curves[i][j*3]));
	} else {
	    /* the entire curve */
	    i = atoi(&attr[1]);
	    for (j = 0; j < ars->pts_per_curve; j++) {
		bu_vls_printf(logstr, " { %.25g %.25g %.25g }",
			      V3ARGS(&ars->curves[i][j*3]));
	    }
	}
    }

    return BRLCAD_OK;
}


int
rt_ars_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_ars_internal *ars;
    size_t i, j, k;
    int len;
    fastf_t *array;

    RT_CK_DB_INTERNAL(intern);

    ars = (struct rt_ars_internal *)intern->idb_ptr;
    RT_ARS_CK_MAGIC(ars);

    while (argc >= 2) {
	if (BU_STR_EQUAL(argv[0], "NC")) {
	    /* change number of curves */
	    i = atoi(argv[1]);
	    if (i < ars->ncurves) {
		for (j=i; j<ars->ncurves; j++)
		    bu_free((char *)ars->curves[j], "ars->curves[j]");
		ars->curves = (fastf_t **)bu_realloc(ars->curves,
						     i*sizeof(fastf_t *), "ars->curves");
		ars->ncurves = i;
	    } else if (i > ars->ncurves) {
		ars->curves = (fastf_t **)bu_realloc(ars->curves,
						     i*sizeof(fastf_t *), "ars->curves");
		if (ars->pts_per_curve) {
		    /* new curves are duplicates of the last */
		    for (j=ars->ncurves; j<i; j++) {
			ars->curves[j] = (fastf_t *)bu_malloc(
			    ars->pts_per_curve * 3 * sizeof(fastf_t),
			    "ars->curves[j]");
			for (k = 0; k < ars->pts_per_curve; k++) {
			    if (j) {
				VMOVE(&ars->curves[j][k*3],
				      &ars->curves[j-1][k*3]);
			    } else {
				VSETALL(&ars->curves[j][k*3], 0.0);
			    }
			}
		    }
		} else {
		    for (j=ars->ncurves; j<i; j++) {
			ars->curves[j] = NULL;
		    }
		}
		ars->ncurves = i;
	    }
	} else if (BU_STR_EQUAL(argv[0], "PPC")) {
	    /* change the number of points per curve */
	    i = atoi(argv[1]);
	    if (i < 3) {
		bu_vls_printf(logstr,
			      "ERROR: must have at least 3 points per curve\n");
		return BRLCAD_ERROR;
	    }
	    if (i < ars->pts_per_curve) {
		for (j = 0; j < ars->ncurves; j++) {
		    ars->curves[j] = (fastf_t *)bu_realloc(ars->curves[j],
						i * 3 * sizeof(fastf_t),
						"ars->curves[j]");
		}
		ars->pts_per_curve = i;
	    } else if (i > ars->pts_per_curve) {
		for (j = 0; j < ars->ncurves; j++) {
		    ars->curves[j] = (fastf_t *)bu_realloc(ars->curves[j],
						i * 3 * sizeof(fastf_t),
						"ars->curves[j]");
		    /* new points are duplicates of last */
		    for (k = ars->pts_per_curve; k < i; k++) {
			if (k) {
			    VMOVE(&ars->curves[j][k*3],
				  &ars->curves[j][(k-1)*3]);
			} else {
			    VSETALL(&ars->curves[j][k*3], 0);
			}
		    }
		}
		ars->pts_per_curve = i;
	    }
	} else if (argv[0][0] == 'C') {
	    if (isdigit((int)argv[0][1])) {
		const char *ptr;

		/* a specific curve */
		ptr = strchr(argv[0], 'P');
		if (ptr) {
		    /* a specific point on this curve */
		    i = atoi(&argv[0][1]);
		    j = atoi(ptr+1);
		    len = 3;
		    array = &ars->curves[i][j*3];
		    if (_rt_tcl_list_to_fastf_array(argv[1], &array, &len)!= len) {
			bu_vls_printf(logstr, "WARNING: incorrect number of parameters provided for a point\n");
		    }
		} else {
		    char *dupstr;
		    char *ptr2;

		    /* one complete curve */
		    i = atoi(&argv[0][1]);
		    len = (int)ars->pts_per_curve * 3;
		    dupstr = bu_strdup(argv[1]);
		    ptr2 = dupstr;
		    while (*ptr2) {
			if (*ptr2 == '{' || *ptr2 == '}')
			    *ptr2 = ' ';
			ptr2++;
		    }
		    if (!ars->curves[i]) {
			ars->curves[i] = (fastf_t *)bu_calloc(ars->pts_per_curve * 3, sizeof(fastf_t), "ars->curves[i]");
		    }
		    if (_rt_tcl_list_to_fastf_array(dupstr, &ars->curves[i], &len) != len) {
			bu_vls_printf(logstr, "WARNING: incorrect number of parameters provided for a curve\n");
		    }
		    bu_free(dupstr, "bu_strdup ars curve");
		}
	    } else {
		bu_vls_printf(logstr, "ERROR: Illegal argument, must be NC, PPC, C#, or C#P#\n");
		return BRLCAD_ERROR;
	    }
	} else {
	    bu_vls_printf(logstr, "ERROR: Illegal argument, must be NC, PPC, C#, or C#P#\n");
	    return BRLCAD_ERROR;
	}
	argc -= 2;
	argv += 2;
    }

    return BRLCAD_OK;
}


int
rt_ars_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}

void
rt_ars_labels(struct bv_scene_obj *ps, const struct rt_db_internal *ip, struct bview *v)
{
    if (!ps || !ip)
	return;

    /*XXX Needs work - doesn't really represent the ARS data well... */

    struct rt_ars_internal *ars = (struct rt_ars_internal *)ip->idb_ptr;
    RT_ARS_CK_MAGIC(ars);

    // Set up the containers
    struct bv_label *l[2];
    int lcnt = 1;
    for (int i = 0; i < lcnt; i++) {
	struct bv_scene_obj *s = bv_obj_get_child(ps);
	struct bv_label *la;
	BU_GET(la, struct bv_label);
	s->s_i_data = (void *)la;
	s->s_v = v;

	BU_LIST_INIT(&(s->s_vlist));
	VSET(s->s_color, 255, 255, 0);
	s->s_type_flags |= BV_DBOBJ_BASED;
	s->s_type_flags |= BV_LABELS;
	BU_VLS_INIT(&la->label);

	l[i] = la;
    }

    bu_vls_sprintf(&l[0]->label, "V");
    VMOVE(l[0]->p, ars->curves[0]);

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
