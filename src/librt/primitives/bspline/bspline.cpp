/*                        B S P L I N E . C P P
 * BRL-CAD
 *
 * Copyright (c) 1991-2026 United States Government as represented by
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
/** @file bspline.cpp
 *
 * Intersect a ray with a Non Uniform Rational B-Spline.
 *
 */
/** @} */

/* define to display old nurbs wireframe plot */
//#define OLD_WIREFRAME 1

/* define to display new brep wireframe plot */
#define NEW_WIREFRAME 1

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bnetwork.h"

#include "bu/cv.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/db4.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "brep/defines.h"

#include "../../librt_private.h"


#ifdef __cplusplus
extern "C" {
#endif

extern void rt_nurb_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol);

extern int rt_brep_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip);
extern void rt_brep_print(const struct soltab *stp);
extern int rt_brep_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead);
extern void rt_brep_norm(struct hit *hitp, struct soltab *stp, struct xray *rp);
extern void rt_brep_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp);
extern void rt_brep_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp);
extern void rt_brep_free(struct soltab *stp);
extern int rt_brep_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol, const struct bview *UNUSED(info));


static int
rt_nurb_grans(struct face_g_snurb *srf)
{
    int total_knots, total_points;
    int k_gran;
    int p_gran;

    total_knots = srf->u.k_size + srf->v.k_size;
    k_gran = ((total_knots * sizeof(dbfloat_t)) + sizeof(union record)-1)
	/ sizeof(union record);

    total_points = RT_NURB_EXTRACT_COORDS(srf->pt_type) *
	(srf->s_size[0] * srf->s_size[1]);
    p_gran = ((total_points * sizeof(dbfloat_t)) + sizeof(union record)-1)
	/ sizeof(union record);

    return 1 + k_gran + p_gran;
}

/**
 * Calculate the bounding RPP of a bspline
 */
int
rt_nurb_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol)) {
    int i;
    struct rt_nurb_internal *sip;

    sip = (struct rt_nurb_internal *) ip->idb_ptr;
    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    for (i = 0; i < sip->nsrf; i++) {
	struct face_g_snurb * s;
	struct bu_list bez_hd;
	BU_LIST_INIT(&bez_hd);

	/* Grind up the original surf into a list of Bezier face_g_snurbs */
	(void)nmg_nurb_bezier(&bez_hd, sip->srfs[i]);

	/* Compute bounds of each Bezier face_g_snurb */
	for (BU_LIST_FOR(s, face_g_snurb, &bez_hd)) {
	    NMG_CK_SNURB(s);
	    nmg_nurb_s_bound(s, s->min_pt, s->max_pt);
	    VMINMAX((*min), (*max), s->min_pt);
	    VMINMAX((*min), (*max), s->max_pt);
	}

	while (BU_LIST_WHILE(s, face_g_snurb, &bez_hd)) {
	    NMG_CK_SNURB(s);
	    BU_LIST_DEQUEUE(&(s->l));
	    nmg_nurb_free_snurb(s);
	}
    }
    /* zero thickness will get missed by the raytracer */
    BBOX_NONDEGEN((*min), (*max), SMALL_FASTF);

    return 0;
}


/**
 * Given a pointer of a GED database record, and a transformation
 * matrix, determine if this is a valid NURB, and if so, prepare the
 * surface so the intersections will work.
 */
int
rt_nurb_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_nurb_internal *sip;
    const struct bn_tol *tol = &rtip->rti_tol;

    sip = (struct rt_nurb_internal *) ip->idb_ptr;
    RT_NURB_CK_MAGIC(sip);

    ON_Brep *brep = ON_Brep::New();
    rt_nurb_brep(&brep, ip, tol);

    struct rt_brep_internal bi;
    bi.magic = RT_BREP_INTERNAL_MAGIC;
    bi.brep = brep;

    struct rt_db_internal di;
    RT_DB_INTERNAL_INIT(&di);
    di.idb_ptr = (void *)&bi;

    return rt_brep_prep(stp, &di, rtip);
}


void
rt_nurb_print(const struct soltab *stp)
{
    return rt_brep_print(stp);
}


/**
 * Intersect a ray with a nurb.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 *   0 MISS
 *  >0 HIT
 */
int
rt_nurb_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    return rt_brep_shot(stp, rp, ap, seghead);
}


/**
 * Baseline flat-array vshot: delegates to the scalar shot via rt_vshot_via_shot().
 */
void
rt_nurb_vshot(struct soltab *stp[], struct xray *rp[], struct seg *segp, int n, struct application *ap)
/* An array of solid pointers */
/* An array of ray pointers */
/* array of segs (results returned) */
/* Number of ray/object pairs */
{
    rt_vshot_via_shot(rt_nurb_shot, stp, rp, segp, n, ap);
}


#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_nurb_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    return rt_brep_norm(hitp, stp, rp);
}


/**
 * Return the curvature of the nurb.
 */
void
rt_nurb_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    return rt_brep_curve(cvp, hitp, stp);
}


/**
 * For a hit on the surface of an nurb, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 * u = azimuth
 * v = elevation
 */
void
rt_nurb_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    return rt_brep_uv(ap, stp, hitp, uvp);
}


void
rt_nurb_free(struct soltab *stp)
{
    return rt_brep_free(stp);
}


int
rt_nurb_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol, const struct bview *UNUSED(info))
{
    struct rt_nurb_internal *sip;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    sip = (struct rt_nurb_internal *) ip->idb_ptr;
    RT_NURB_CK_MAGIC(sip);

#ifdef OLD_WIREFRAME
    struct bu_list *vlfree = &rt_vlfree;
    for (s=0; s < sip->nsrf; s++) {
	struct face_g_snurb * n, *r, *c;
	int coords;
	fastf_t bound;
	point_t tmp_pt;
	fastf_t dtol;
	struct knot_vector tkv1,
	    tkv2;
	fastf_t tess;
	int num_knots;
	int refined_col = 0;
	int refined_row = 0;
	fastf_t rt_nurb_par_edge(const struct face_g_snurb *srf, fastf_t epsilon);

	n = (struct face_g_snurb *) sip->srfs[s];

	VSUB2(tmp_pt, n->min_pt, n->max_pt);
	bound =         MAGNITUDE(tmp_pt)/ 2.0;

	dtol = primitive_get_absolute_tolerance(ttol, 2.0 * bound);

	if (n->order[0] < 3 || n->order[1] < 3) {
	    /* cannot use rt_nurb_par_edge() in this case */
	    tess = 0.25; /* hack for now */
	} else
	    tess = (fastf_t) rt_nurb_par_edge(n, dtol);

	num_knots = (int)floor(1.0/((M_SQRT1_2 / 2.0) * tess));

	if (num_knots < 2) num_knots = 2;

	nmg_nurb_kvknot(&tkv1, n->order[0],
		       n->u.knots[0],
		       n->u.knots[n->u.k_size-1], num_knots);

	nmg_nurb_kvknot(&tkv2, n->order[1],
		       n->v.knots[0],
		       n->v.knots[n->v.k_size-1], num_knots);


	if (tkv2.k_size > n->v.k_size) {
	    r = (struct face_g_snurb *) nmg_nurb_s_refine(n, RT_NURB_SPLIT_COL, &tkv2);
	    refined_col = 1;
	} else {
	    r = n;
	}
	if (tkv1.k_size > r->u.k_size) {
	    c = (struct face_g_snurb *) nmg_nurb_s_refine(r, RT_NURB_SPLIT_ROW, &tkv1);
	    refined_row = 1;
	} else {
	    c = r;
	}

	coords = RT_NURB_EXTRACT_COORDS(n->pt_type);

	if (RT_NURB_IS_PT_RATIONAL(n->pt_type)) {
	    vp = c->ctl_points;
	    for (i= 0; i < c->s_size[0] * c->s_size[1]; i++) {
		vp[0] /= vp[3];
		vp[1] /= vp[3];
		vp[2] /= vp[3];
		vp[3] /= vp[3];
		vp += coords;
	    }
	}


	vp = c->ctl_points;
	for (i = 0; i < c->s_size[0]; i++) {
	    BV_ADD_VLIST(vlfree, vhead, vp, BV_VLIST_LINE_MOVE);
	    vp += coords;
	    for (j = 1; j < c->s_size[1]; j++) {
		BV_ADD_VLIST(vlfree, vhead, vp, BV_VLIST_LINE_DRAW);
		vp += coords;
	    }
	}

	for (j = 0; j < c->s_size[1]; j++) {
	    int stride;

	    stride = c->s_size[1] * coords;
	    vp = &c->ctl_points[j * coords];
	    BV_ADD_VLIST(vlfree, vhead, vp, BV_VLIST_LINE_MOVE);
	    for (i = 0; i < c->s_size[0]; i++) {
		BV_ADD_VLIST(vlfree, vhead, vp, BV_VLIST_LINE_DRAW);
		vp += stride;
	    }
	}
	if (refined_col) {
	    nmg_nurb_free_snurb(r);
	}
	if (refined_row) {
	    nmg_nurb_free_snurb(c);
	}
	bu_free((char *) tkv1.knots, "rt_nurb_plot:tkv1>knots");
	bu_free((char *) tkv2.knots, "rt_nurb_plot:tkv2.knots");
    }
#endif

#ifdef NEW_WIREFRAME
    ON_Brep *brep = ON_Brep::New();
    rt_nurb_brep(&brep, ip, tol);

    struct rt_brep_internal bi;
    bi.magic = RT_BREP_INTERNAL_MAGIC;
    bi.brep = brep;

    struct rt_db_internal di;
    RT_DB_INTERNAL_INIT(&di);
    di.idb_ptr = (void *)&bi;

    return rt_brep_plot(vhead, &di, ttol, tol, NULL);
#else
    return 0;
#endif /* NEW_WIREFRAME */
}


int
rt_nurb_tess(struct nmgregion **, struct model *, struct rt_db_internal *, const struct bg_tess_tol *, const struct bn_tol *)
{
    return -1;
}


int
rt_nurb_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_nurb_internal *sip;
    union record *rp;
    int i;
    int s;

    if (dbip)
	RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    if (rp->u_id != ID_BSOLID) {
	bu_log("rt_nurb_import4: defective header record");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_BSPLINE;
    ip->idb_meth = &OBJ[ID_BSPLINE];
    BU_ALLOC(ip->idb_ptr, struct rt_nurb_internal);

    sip = (struct rt_nurb_internal *)ip->idb_ptr;
    sip->magic = RT_NURB_INTERNAL_MAGIC;

    if (dbip && dbip->i->dbi_version < 0) {
	sip->nsrf = flip_short(rp->B.B_nsurf);
    } else {
	sip->nsrf = rp->B.B_nsurf;
    }

    sip->srfs = (struct face_g_snurb **) bu_malloc(sip->nsrf * sizeof(struct face_g_snurb), "nurb srfs[]");
    rp++;

    for (s = 0; s < sip->nsrf; s++) {
	fastf_t * m;
	int coords;
	dbfloat_t *vp;
	int pt_type;
	union record d;

	if (rp->d.d_id != ID_BSURF) {
	    bu_log("rt_nurb_import4() surf %d bad ID\n", s);
	    return -1;
	}

	if (rp->d.d_geom_type == 3)
	    pt_type = RT_NURB_MAKE_PT_TYPE(3, RT_NURB_PT_XYZ, RT_NURB_PT_NONRAT);
	else
	    pt_type = RT_NURB_MAKE_PT_TYPE(4, RT_NURB_PT_XYZ, RT_NURB_PT_RATIONAL);

	/* fix endianness */
	d.d.d_id = rp->d.d_id;
	if (dbip && dbip->i->dbi_version < 0) {
	    d.d.d_order[0] = flip_short(rp->d.d_order[0]);
	    d.d.d_order[1] = flip_short(rp->d.d_order[1]);
	    d.d.d_kv_size[0] = flip_short(rp->d.d_kv_size[0]);
	    d.d.d_kv_size[1] = flip_short(rp->d.d_kv_size[1]);
	    d.d.d_ctl_size[0] = flip_short(rp->d.d_ctl_size[0]);
	    d.d.d_ctl_size[1] = flip_short(rp->d.d_ctl_size[1]);
	    d.d.d_geom_type = flip_short(rp->d.d_geom_type);
	    d.d.d_nknots = flip_short(rp->d.d_nknots);
	    d.d.d_nctls = flip_short(rp->d.d_nctls);
	} else {
	    d.d.d_order[0] = rp->d.d_order[0];
	    d.d.d_order[1] = rp->d.d_order[1];
	    d.d.d_kv_size[0] = rp->d.d_kv_size[0];
	    d.d.d_kv_size[1] = rp->d.d_kv_size[1];
	    d.d.d_ctl_size[0] = rp->d.d_ctl_size[0];
	    d.d.d_ctl_size[1] = rp->d.d_ctl_size[1];
	    d.d.d_geom_type = rp->d.d_geom_type;
	    d.d.d_nknots = rp->d.d_nknots;
	    d.d.d_nctls = rp->d.d_nctls;
	}

	sip->srfs[s] = (struct face_g_snurb *) nmg_nurb_new_snurb(
	    d.d.d_order[0], d.d.d_order[1],
	    d.d.d_kv_size[0], d.d.d_kv_size[1],
	    d.d.d_ctl_size[0], d.d.d_ctl_size[1],
	    pt_type);

	vp = (dbfloat_t *) &rp[1];

	if (dbip && dbip->i->dbi_version < 0) {
	    for (i = 0; i < d.d.d_kv_size[0]; i++) {
		sip->srfs[s]->u.knots[i] = flip_dbfloat(*vp++);
	    }
	    for (i = 0; i < d.d.d_kv_size[1]; i++) {
		sip->srfs[s]->v.knots[i] = flip_dbfloat(*vp++);
	    }
	} else {
	    for (i = 0; i < d.d.d_kv_size[0]; i++) {
		sip->srfs[s]->u.knots[i] = (fastf_t) *vp++;
	    }
	    for (i = 0; i < d.d.d_kv_size[1]; i++) {
		sip->srfs[s]->v.knots[i] = (fastf_t) *vp++;
	    }
	}

	nmg_nurb_kvnorm(&sip->srfs[s]->u);
	nmg_nurb_kvnorm(&sip->srfs[s]->v);

	vp = (dbfloat_t *) &rp[d.d.d_nknots+1];
	m = sip->srfs[s]->ctl_points;
	coords = d.d.d_geom_type;
	i = (d.d.d_ctl_size[0] * d.d.d_ctl_size[1]);

	if (mat == NULL)
	    mat = bn_mat_identity;

	if (coords == 3) {
	    for (; i> 0; i--) {
		vect_t f;

		if (dbip && dbip->i->dbi_version < 0) {
		    f[0] = flip_dbfloat(vp[0]);
		    f[1] = flip_dbfloat(vp[1]);
		    f[2] = flip_dbfloat(vp[2]);
		} else {
		    VMOVE(f, vp);
		}

		MAT4X3PNT(m, mat, f);
		m += 3;
		vp += 3;
	    }
	} else if (coords == 4) {
	    for (; i> 0; i--) {
		hvect_t f;

		if (dbip && dbip->i->dbi_version < 0) {
		    f[0] = flip_dbfloat(vp[0]);
		    f[1] = flip_dbfloat(vp[1]);
		    f[2] = flip_dbfloat(vp[2]);
		    f[3] = flip_dbfloat(vp[3]);
		} else {
		    HMOVE(f, vp);
		}

		MAT4X4PNT(m, mat, f);
		m += 4;
		vp += 4;
	    }
	} else {
	    bu_log("rt_nurb_internal: %d invalid elements per vect\n", d.d.d_geom_type);
	    return -1;
	}

	/* bound the surface for tolerancing and other bounding box tests */
	nmg_nurb_s_bound(sip->srfs[s], sip->srfs[s]->min_pt,
			sip->srfs[s]->max_pt);

	rp += 1 + d.d.d_nknots + d.d.d_nctls;
    }
    return 0;
}


int
rt_nurb_export4(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *dbip)
{
    int rec_ptr;
    struct rt_nurb_internal *sip;
    union record *rec;
    int s;
    int grans;
    int total_grans;
    dbfloat_t *vp;
    int n;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_BSPLINE) return -1;
    sip = (struct rt_nurb_internal *) ip->idb_ptr;
    RT_NURB_CK_MAGIC(sip);

    /* Figure out how many recs to buffer by
     * walking through the surfaces and
     * calculating the number of granules
     * needed for storage and add it to the total
     */
    total_grans = 1;	/* First gran for BSOLID record */
    for (s = 0; s < sip->nsrf; s++) {
	total_grans += rt_nurb_grans(sip->srfs[s]);
    }

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = total_grans * sizeof(union record);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "nurb external");
    rec = (union record *)ep->ext_buf;

    rec[0].B.B_id = ID_BSOLID;
    rec[0].B.B_nsurf = sip->nsrf;

    rec_ptr = 1;

    for (s = 0; s < sip->nsrf; s++) {
	struct face_g_snurb *srf = sip->srfs[s];
	NMG_CK_SNURB(srf);

	grans = rt_nurb_grans(srf);

	rec[rec_ptr].d.d_id = ID_BSURF;
	rec[rec_ptr].d.d_nknots = (short)(((srf->u.k_size + srf->v.k_size)
				    * sizeof(dbfloat_t)) + sizeof(union record)-1)/ sizeof(union record);
	rec[rec_ptr].d.d_nctls = (short)((
				      RT_NURB_EXTRACT_COORDS(srf->pt_type)
				      * (srf->s_size[0] * srf->s_size[1])
				      * sizeof(dbfloat_t)) + sizeof(union record)-1)
	    / sizeof(union record);

	rec[rec_ptr].d.d_order[0] = srf->order[0];
	rec[rec_ptr].d.d_order[1] = srf->order[1];
	rec[rec_ptr].d.d_kv_size[0] = srf->u.k_size;
	rec[rec_ptr].d.d_kv_size[1] = srf->v.k_size;
	rec[rec_ptr].d.d_ctl_size[0] = 	srf->s_size[0];
	rec[rec_ptr].d.d_ctl_size[1] = 	srf->s_size[1];
	rec[rec_ptr].d.d_geom_type =
	    RT_NURB_EXTRACT_COORDS(srf->pt_type);

	vp = (dbfloat_t *) &rec[rec_ptr +1];
	for (n = 0; n < rec[rec_ptr].d.d_kv_size[0]; n++) {
	    *vp++ = srf->u.knots[n];
	}

	for (n = 0; n < rec[rec_ptr].d.d_kv_size[1]; n++) {
	    *vp++ = srf->v.knots[n];
	}

	vp = (dbfloat_t *) &rec[rec_ptr + 1 +
				rec[rec_ptr].d.d_nknots];

	for (n = 0; n < (srf->s_size[0] * srf->s_size[1]) *
		 rec[rec_ptr].d.d_geom_type; n++)
	    *vp++ = srf->ctl_points[n];

	rec_ptr += grans;
	total_grans -= grans;
    }

    bu_log("DEPRECATED:  The 'bspline' primitive is no longer supported.  Use 'brep' NURBS instead.\n");

    return 0;
}

int
rt_nurb_bytes(struct face_g_snurb *srf)
{
    int total_bytes=0;

    total_bytes = 3 * SIZEOF_NETWORK_LONG		/* num_coords and order */
	+ 2 * SIZEOF_NETWORK_LONG		/* k_size in both knot vectors */
	+ srf->u.k_size * SIZEOF_NETWORK_DOUBLE	/* u knot vector knots */
	+ srf->v.k_size * SIZEOF_NETWORK_DOUBLE	/* v knot vector knots */
	+ 2 * SIZEOF_NETWORK_LONG		/* mesh size */
	+ RT_NURB_EXTRACT_COORDS(srf->pt_type) *
	(srf->s_size[0] * srf->s_size[1]) * SIZEOF_NETWORK_DOUBLE;	/* control point mesh */

    return total_bytes;
}


int
rt_nurb_export5(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *dbip)
{
    struct rt_nurb_internal *sip;
    int s;
    unsigned char *cp;
    int coords;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_BSPLINE) return -1;
    sip = (struct rt_nurb_internal *) ip->idb_ptr;
    RT_NURB_CK_MAGIC(sip);

    /* Figure out how many bytes are needed by
     * walking through the surfaces and
     * calculating the number of bytes
     * needed for storage and add it to the total
     */
    BU_EXTERNAL_INIT(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_LONG;	/* number of surfaces */
    for (s = 0; s < sip->nsrf; s++) {
	ep->ext_nbytes += rt_nurb_bytes(sip->srfs[s]);
    }

    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "nurb external");
    cp = (unsigned char *)ep->ext_buf;

    *(uint32_t *)cp = htonl(sip->nsrf);
    cp += SIZEOF_NETWORK_LONG;

    for (s = 0; s < sip->nsrf; s++) {
	int i;
	struct face_g_snurb *srf = sip->srfs[s];

	/* must be double for import and export */
	double *uknots;
	double *vknots;
	double *points;

	NMG_CK_SNURB(srf);

	coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);
	*(uint32_t *)cp = htonl(coords);
	cp += SIZEOF_NETWORK_LONG;
	*(uint32_t *)cp = htonl(srf->order[0]);
	cp += SIZEOF_NETWORK_LONG;
	*(uint32_t *)cp = htonl(srf->order[1]);
	cp += SIZEOF_NETWORK_LONG;
	*(uint32_t *)cp = htonl(srf->u.k_size);
	cp += SIZEOF_NETWORK_LONG;
	*(uint32_t *)cp = htonl(srf->v.k_size);
	cp += SIZEOF_NETWORK_LONG;
	*(uint32_t *)cp = htonl(srf->s_size[0]);
	cp += SIZEOF_NETWORK_LONG;
	*(uint32_t *)cp = htonl(srf->s_size[1]);
	cp += SIZEOF_NETWORK_LONG;

	/* allocate for export */
	uknots = (double *)bu_malloc(srf->u.k_size * sizeof(double), "uknots");
	vknots = (double *)bu_malloc(srf->v.k_size * sizeof(double), "vknots");
	points = (double *)bu_malloc(coords * srf->s_size[0] * srf->s_size[1] * sizeof(double), "points");

	/* convert fastf_t to double */
	for (i=0; i<srf->u.k_size; i++) {
	    uknots[i] = srf->u.knots[i];
	}
	for (i=0; i<srf->v.k_size; i++) {
	    vknots[i] = srf->v.knots[i];
	}
	for (i=0; i<coords * srf->s_size[0] * srf->s_size[1]; i++) {
	    points[i] = srf->ctl_points[i];
	}

	/* serialize */
	bu_cv_htond(cp, (unsigned char *)uknots, srf->u.k_size);
	cp += srf->u.k_size * SIZEOF_NETWORK_DOUBLE;
	bu_cv_htond(cp, (unsigned char *)vknots, srf->v.k_size);
	cp += srf->v.k_size * SIZEOF_NETWORK_DOUBLE;
	bu_cv_htond(cp, (unsigned char *)srf->ctl_points,
	      coords * srf->s_size[0] * srf->s_size[1]);
	cp += coords * srf->s_size[0] * srf->s_size[1] * SIZEOF_NETWORK_DOUBLE;

	/* release our arrays */
	bu_free(uknots, "uknots");
	bu_free(vknots, "vknots");
	bu_free(points, "points");
    }

    bu_log("DEPRECATED:  The 'bspline' primitive is no longer supported.  Use 'brep' NURBS instead.\n");

    return 0;
}

int
rt_nurb_mat(struct rt_db_internal *rop, const mat_t mat, const struct rt_db_internal *ip)
{
    if (!rop || !ip || !mat)
	return BRLCAD_OK;

    struct rt_nurb_internal *tip = (struct rt_nurb_internal *)ip->idb_ptr;
    RT_NURB_CK_MAGIC(tip);
    struct rt_nurb_internal *top = (struct rt_nurb_internal *)rop->idb_ptr;
    RT_NURB_CK_MAGIC(top);

    if (tip->nsrf != top->nsrf)
	return BRLCAD_ERROR;

    fastf_t tmp_vec[4];
    for (int s = 0; s < tip->nsrf; s++) {
	struct face_g_snurb *srf = tip->srfs[s];
	struct face_g_snurb *osrf = top->srfs[s];
	if (srf->s_size[0] != osrf->s_size[0])
	    return BRLCAD_ERROR;
	if (srf->s_size[1] != osrf->s_size[1])
	    return BRLCAD_ERROR;

	for (int i=0; i < srf->s_size[0] * srf->s_size[1]; i++) {
	    int coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);
	    if (coords == 3) {
		VMOVE(tmp_vec, &srf->ctl_points[i*coords]);
		MAT4X3PNT(&osrf->ctl_points[i*coords], mat, tmp_vec);
	    } else if (coords == 4) {
		HMOVE(tmp_vec, &srf->ctl_points[i*coords]);
		MAT4X4PNT(&osrf->ctl_points[i*coords], mat, tmp_vec);
	    } else {
		bu_log("rt_nurb_mat: %d invalid elements per vect\n", coords);
		return -1;
	    }
	}

	/* bound the surface for tolerancing and other bounding box tests */
	nmg_nurb_s_bound(top->srfs[s], top->srfs[s]->min_pt, top->srfs[s]->max_pt);
    }

    return BRLCAD_OK;
}


int
rt_nurb_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{

    struct rt_nurb_internal *sip;
    int i;
    int s;
    unsigned char *cp;

    if (dbip) RT_CK_DBI(dbip);
    BU_CK_EXTERNAL(ep);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_BSPLINE;
    ip->idb_meth = &OBJ[ID_BSPLINE];
    BU_ALLOC(ip->idb_ptr, struct rt_nurb_internal);

    sip = (struct rt_nurb_internal *)ip->idb_ptr;
    sip->magic = RT_NURB_INTERNAL_MAGIC;

    cp = (unsigned char *)ep->ext_buf;

    sip->nsrf = ntohl(*(uint32_t *)cp);
    cp += SIZEOF_NETWORK_LONG;

    if (sip->nsrf > 0)
	sip->srfs = (struct face_g_snurb **) bu_calloc(sip->nsrf, sizeof(struct face_g_snurb *), "nurb srfs[]");

    for (s = 0; s < sip->nsrf; s++) {
	struct face_g_snurb *srf;
	int coords;
	int pt_type;
	int order[2], u_size, v_size;
	int s_size[2];

	/* must be double for import and export */
	double *uknots;
	double *vknots;
	double *points;

	pt_type = ntohl(*(uint32_t *)cp);
	cp += SIZEOF_NETWORK_LONG;
	order[0] = ntohl(*(uint32_t *)cp);
	cp += SIZEOF_NETWORK_LONG;
	order[1] = ntohl(*(uint32_t *)cp);
	cp += SIZEOF_NETWORK_LONG;
	u_size = ntohl(*(uint32_t *)cp);
	cp += SIZEOF_NETWORK_LONG;
	v_size = ntohl(*(uint32_t *)cp);
	cp += SIZEOF_NETWORK_LONG;
	s_size[0] = ntohl(*(uint32_t *)cp);
	cp += SIZEOF_NETWORK_LONG;
	s_size[1] = ntohl(*(uint32_t *)cp);
	cp += SIZEOF_NETWORK_LONG;
	if (pt_type == 3)
	    pt_type = RT_NURB_MAKE_PT_TYPE(3, RT_NURB_PT_XYZ, RT_NURB_PT_NONRAT);
	else
	    pt_type = RT_NURB_MAKE_PT_TYPE(4, RT_NURB_PT_XYZ, RT_NURB_PT_RATIONAL);

	sip->srfs[s] = (struct face_g_snurb *) nmg_nurb_new_snurb(
	    order[0], order[1],
	    u_size, v_size,
	    s_size[0], s_size[1],
	    pt_type);

	srf = sip->srfs[s];
	coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);

	uknots = (double *)bu_malloc(srf->u.k_size * sizeof(double), "uknots");
	vknots = (double *)bu_malloc(srf->v.k_size * sizeof(double), "vknots");

	bu_cv_ntohd((unsigned char *)uknots, cp, srf->u.k_size);
	cp += srf->u.k_size * SIZEOF_NETWORK_DOUBLE;
	bu_cv_ntohd((unsigned char *)vknots, cp, srf->v.k_size);
	cp += srf->v.k_size * SIZEOF_NETWORK_DOUBLE;

	/* convert double to fastf_t */
	for (i=0; i<srf->u.k_size; i++) {
	    srf->u.knots[i] = uknots[i];
	}
	for (i=0; i<srf->v.k_size; i++) {
	    srf->v.knots[i] = vknots[i];
	}

	bu_free(uknots, "uknots");
	bu_free(vknots, "vknots");

	nmg_nurb_kvnorm(&srf->u);
	nmg_nurb_kvnorm(&srf->v);

	points = (double *)bu_malloc(coords * srf->s_size[0] * srf->s_size[1] * sizeof(double), "points");

	bu_cv_ntohd((unsigned char *)points, cp, coords * srf->s_size[0] * srf->s_size[1]);

	/* convert double to fastf_t */
	for (i=0; i < coords * srf->s_size[0] * srf->s_size[1]; i++) {
	    srf->ctl_points[i] = points[i];
	}

	bu_free(points, "points");

	cp += coords * srf->s_size[0] * srf->s_size[1] * SIZEOF_NETWORK_DOUBLE;

    }

    if (mat == NULL) mat = bn_mat_identity;
    return rt_nurb_mat(ip, mat, ip);
}


void
rt_nurb_ifree(struct rt_db_internal *ip)
{
    struct rt_nurb_internal *sip;
    int i;

    RT_CK_DB_INTERNAL(ip);

    sip = (struct rt_nurb_internal *) ip->idb_ptr;
    RT_NURB_CK_MAGIC(sip);

    /* Free up storage for the nurb surfaces */
    for (i = 0; i < sip->nsrf; i++) {
	nmg_nurb_free_snurb(sip->srfs[i]);
    }
    sip->magic = 0;
    sip->nsrf = 0;
    bu_free(sip->srfs, "nurb surfs[]");
    sip->srfs = (struct face_g_snurb**)((void *)0);

    bu_free(ip->idb_ptr, "sip ifree");
    ip->idb_ptr = ((void *)0);
}


int
rt_nurb_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    int j;
    struct rt_nurb_internal *sip =
	(struct rt_nurb_internal *) ip->idb_ptr;
    int i;
    int surf;

    RT_NURB_CK_MAGIC(sip);
    bu_vls_strcat(str, "Non-Uniform Rational B-Spline (NURBS) solid\n");

    bu_vls_printf(str, "\t%d surfaces\n", sip->nsrf);
    if (verbose < 2)  return 0;

    for (surf = 0; surf < sip->nsrf; surf++) {
	struct face_g_snurb *np;
	fastf_t *mp;
	int ncoord;

	np = sip->srfs[surf];
	NMG_CK_SNURB(np);
	mp = np->ctl_points;
	ncoord = RT_NURB_EXTRACT_COORDS(np->pt_type);

	bu_vls_printf(str,
		      "\tSurface %d: order %d x %d, mesh %d x %d\n",
		      surf, np->order[0], np->order[1],
		      np->s_size[0], np->s_size[1]);

	bu_vls_printf(str, "\t\tVert (%g, %g, %g)\n",
		      INTCLAMP(mp[X] * mm2local),
		      INTCLAMP(mp[Y] * mm2local),
		      INTCLAMP(mp[Z] * mm2local));

	if (verbose < 3) continue;

	/* Print out the knot vectors */
	bu_vls_printf(str, "\tU: ");
	for (i=0; i < np->u.k_size; i++)
	    bu_vls_printf(str, "%g, ", INTCLAMP(np->u.knots[i]));
	bu_vls_printf(str, "\n\tV: ");
	for (i=0; i < np->v.k_size; i++)
	    bu_vls_printf(str, "%g, ", INTCLAMP(np->v.knots[i]));
	bu_vls_printf(str, "\n");

	/* print out all the points */
	for (i=0; i < np->s_size[0]; i++) {
	    bu_vls_printf(str, "\tRow %d:\n", i);
	    for (j = 0; j < np->s_size[1]; j++) {
		if (ncoord == 3) {
		    bu_vls_printf(str, "\t\t(%g, %g, %g)\n",
				  INTCLAMP(mp[X] * mm2local),
				  INTCLAMP(mp[Y] * mm2local),
				  INTCLAMP(mp[Z] * mm2local));
		} else {
		    bu_vls_printf(str, "\t\t(%g, %g, %g, %g)\n",
				  INTCLAMP(mp[X] * mm2local),
				  INTCLAMP(mp[Y] * mm2local),
				  INTCLAMP(mp[Z] * mm2local),
				  INTCLAMP(mp[W]));
		}
		mp += ncoord;
	    }
	}
    }
    return 0;
}


int
rt_nurb_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    struct rt_nurb_internal *nurb=(struct rt_nurb_internal *)intern->idb_ptr;
    struct face_g_snurb *srf;
    int i, j, k;
    int coords;

    RT_NURB_CK_MAGIC(nurb);

    if (attr == (char *)NULL) {
	bu_vls_strcpy(logstr, "bspline");
	bu_vls_printf(logstr, " N %d S {", nurb->nsrf);
	for (i=0; i<nurb->nsrf; i++) {
	    srf = nurb->srfs[i];
	    bu_vls_printf(logstr, " { O {%d %d} s {%d %d} T %d u {",
			  srf->order[0], srf->order[1],
			  srf->s_size[0], srf->s_size[1],
			  srf->pt_type/* !!! -- export this?, srf->u.k_size */);
	    for (j=0; j<srf->u.k_size; j++) {
		bu_vls_printf(logstr, " %.25G", srf->u.knots[j]);
	    }
	    bu_vls_printf(logstr, "} v {"/* !!! -- export this?, srf->v.k_size */);
	    for (j=0; j<srf->v.k_size; j++) {
		bu_vls_printf(logstr, " %.25G", srf->v.knots[j]);
	    }
	    bu_vls_strcat(logstr, "} P {");

	    coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);
	    for (j=0; j<srf->s_size[0]*srf->s_size[1]; j++) {
		for (k=0; k<coords; k++) {
		    bu_vls_printf(logstr, " %.25G",
				  srf->ctl_points[j*coords + k]);
		}
	    }
	    bu_vls_strcat(logstr, " } }");
	}
	bu_vls_printf(logstr, " }");
	return BRLCAD_OK;
    }

    bu_vls_printf(logstr, "Nurb has no attribute '%s'", attr);
    return BRLCAD_ERROR;
}

int
rt_nurb_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_nurb_internal *nurb;
    int srf_no;
    const char **srf_array = NULL;
    const char **srf_param_array = NULL;
    struct face_g_snurb *srf;
    int i;
    const char *key;
    int len;

    RT_CK_DB_INTERNAL(intern);
    nurb = (struct rt_nurb_internal *)intern->idb_ptr;
    RT_NURB_CK_MAGIC(nurb);

    while (argc >= 2) {

	if (bu_argv_from_tcl_list(argv[1], &len, (const char ***)&srf_array) != 0) {
	    return BRLCAD_ERROR;
	}

	if (BU_STR_EQUAL(argv[0], "N")) {
	    if (nurb->srfs) {
		for (i=0; i<nurb->nsrf; i++)
		    nmg_nurb_free_snurb(nurb->srfs[i]);
		bu_free((char *)nurb->srfs, "nurb surfaces");
	    }
	    nurb->nsrf = atoi(argv[1]);
	    nurb->srfs = (struct face_g_snurb **) bu_calloc(
		nurb->nsrf, sizeof(struct face_g_snurb *), "nurb srfs[]");
	} else if (BU_STR_EQUAL(argv[0], "S")) {
	    for (srf_no=0; srf_no < nurb->nsrf; srf_no++) {
		int n_params=0;
		int *order=NULL, *s_size=NULL, u_size=0, v_size=0, pt_type=0;
		fastf_t *u_pts=NULL, *v_pts=NULL;

		(void)bu_argv_from_tcl_list(srf_array[srf_no], &n_params, (const char ***)&srf_param_array);

		if (!srf_param_array)
		    continue;

		for (i=0; i<n_params; i+= 2) {
		    int tmp_len;

		    key = srf_param_array[i];
		    if (BU_STR_EQUAL(key, "O")) {
			tmp_len = 0;
			if (_rt_tcl_list_to_int_array(srf_param_array[i+1], &order, &tmp_len) != 2) {
			    bu_vls_printf(logstr,
					  "ERROR: unable to parse surface\n");
			    return BRLCAD_ERROR;
			}
		    } else if (BU_STR_EQUAL(key, "s")) {
			tmp_len = 0;
			if (_rt_tcl_list_to_int_array(srf_param_array[i+1], &s_size, &tmp_len) != 2) {
			    bu_vls_printf(logstr,
					  "ERROR: unable to parse surface\n");
			    return BRLCAD_ERROR;
			}
		    } else if (BU_STR_EQUAL(key, "T")) {
			pt_type = atoi(srf_param_array[i+1]);
		    } else if (BU_STR_EQUAL(key, "u")) {
			(void)_rt_tcl_list_to_fastf_array(srf_param_array[i+1], &u_pts, &u_size);
		    } else if (BU_STR_EQUAL(key, "v")) {
			(void)_rt_tcl_list_to_fastf_array(srf_param_array[i+1], &v_pts, &v_size);
		    } else if (BU_STR_EQUAL(key, "P")) {
			int tmp2;

			if (!order || !s_size || !u_pts || !v_pts ||
			    u_size == 0 || v_size == 0 || pt_type == 0) {
			    bu_vls_printf(logstr, "ERROR: Need all other details set before ctl points\n");
			    bu_free((char *)srf_array, "srf_array");
			    bu_free((char *)srf_param_array, "srf_param_array");
			    return BRLCAD_ERROR;
			}
			nurb->srfs[srf_no] = (struct face_g_snurb *) nmg_nurb_new_snurb(order[0], order[1], u_size, v_size, s_size[0], s_size[1], pt_type);
			srf = nurb->srfs[srf_no];
			bu_free((char *)order, "order");
			order = NULL;
			bu_free((char *)s_size, "s_size");
			s_size = NULL;
			(void)memcpy(srf->u.knots, u_pts, srf->u.k_size * sizeof(fastf_t));
			(void)memcpy(srf->v.knots, v_pts, srf->v.k_size * sizeof(fastf_t));
			bu_free((char *)u_pts, "u_pts");
			u_pts = NULL;
			bu_free((char *)v_pts, "v_pts");
			v_pts = NULL;
			tmp_len = srf->s_size[0] * srf->s_size[1] * RT_NURB_EXTRACT_COORDS(srf->pt_type);
			tmp2 = tmp_len;
			if (_rt_tcl_list_to_fastf_array(srf_param_array[i+1], &srf->ctl_points, &tmp_len) != tmp2) {
			    bu_vls_printf(logstr, "ERROR: unable to parse surface\n");
			    bu_free((char *)srf_array, "srf_array");
			    bu_free((char *)srf_param_array, "srf_param_array");
			    return BRLCAD_ERROR;
			}
		    }
		}
		bu_free((char *)srf_param_array, "srf_param_array");
		srf_param_array = NULL;
	    }
	}

	bu_free((char *)srf_array, "srf_array");

	argc -= 2;
	argv += 2;
    }

    return BRLCAD_OK;
}


int
rt_nurb_params(struct pc_pc_set *, const struct rt_db_internal *)
{
    return 0;			/* OK */
}

#ifdef __cplusplus
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
