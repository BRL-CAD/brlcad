/*                        A N N O T . C
 * BRL-CAD
 *
 * Copyright (c) 2017-2021 United States Government as represented by
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
/** @file primitives/annot/annot.c
 *
 * Provide support for 2D annotations.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "bnetwork.h"

#include "vmath.h"
#include "bu/debug.h"
#include "bu/cv.h"
#include "bu/opt.h"
#include "rt/db4.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../../librt_private.h"


int
rt_txt_pos_flag(int *pos_flag, int p_hor, int p_ver)
{
    if (!pos_flag)
	return 1;

    /* sanity bounding */
    if (p_hor < 1)
	p_hor = 1;
    if (p_hor > 3)
	p_hor = 3;
    if (p_ver < 1)
	p_ver = 1;
    if (p_ver > 3)
	p_ver = 3;

    switch (p_ver) {
	case 1:
	    switch(p_hor) {
		case 1:
		    *pos_flag = RT_TXT_POS_BL;
		    break;
		case 2:
		    *pos_flag = RT_TXT_POS_BC;
		    break;
		case 3:
		    *pos_flag = RT_TXT_POS_BR;
	    }
	    break;
	case 2:
	    switch(p_hor) {
		case 1:
		    *pos_flag = RT_TXT_POS_ML;
		    break;
		case 2:
		    *pos_flag = RT_TXT_POS_MC;
		    break;
		case 3:
		    *pos_flag = RT_TXT_POS_MR;
	    }
	    break;
	case 3:
	    switch(p_hor) {
		case 1:
		    *pos_flag = RT_TXT_POS_TL;
		    break;
		case 2:
		    *pos_flag = RT_TXT_POS_TC;
		    break;
		case 3:
		    *pos_flag = RT_TXT_POS_TR;
	    }
    }
    return 0;
}


static int
ant_check_pos(const struct txt_seg *tsg, char **rel_pos)
{
    switch (tsg->rel_pos) {
	case RT_TXT_POS_BL:
	    *rel_pos = "bottom left";
	    break;
	case RT_TXT_POS_BC:
	    *rel_pos = "bottom center";
	    break;
	case RT_TXT_POS_BR:
	    *rel_pos = "bottom right";
	    break;
	case RT_TXT_POS_ML:
	    *rel_pos = "middle left";
	    break;
	case RT_TXT_POS_MC:
	    *rel_pos = "middle center";
	    break;
	case RT_TXT_POS_MR:
	    *rel_pos = "middle right";
	    break;
	case RT_TXT_POS_TL:
	    *rel_pos = "top left";
	    break;
	case RT_TXT_POS_TC:
	    *rel_pos = "top center";
	    break;
	case RT_TXT_POS_TR:
	    *rel_pos = "top right";
	    break;
    }

    return 0;
}


static void
ant_label_dimensions(struct txt_seg* tsg, hpoint_t ref_pt, fastf_t* length, fastf_t* hight)
{
    point_t bmin, bmax;
    struct bu_list vhead;

    BU_LIST_INIT(&vhead);
    VSET(bmin, INFINITY, INFINITY, INFINITY);
    VSET(bmax, -INFINITY, -INFINITY, -INFINITY);

    bv_vlist_2string(&vhead, &RTG.rtg_vlfree, tsg->label.vls_str, ref_pt[0], ref_pt[1], 5, 0);
    bv_vlist_bbox(&vhead, &bmin, &bmax, NULL);

    *length = bmax[0] - ref_pt[0];
    *hight = bmax[1] - ref_pt[1];
}


static int
ant_pos_adjs(struct txt_seg* tsg, struct rt_annot_internal* annot_ip)
{
    point2d_t pt = V2INIT_ZERO;
    fastf_t length = 0;
    fastf_t height = 0;

    ant_label_dimensions(tsg, annot_ip->verts[tsg->ref_pt], &length, &height);

    if (tsg->rel_pos == RT_TXT_POS_BL) {
	V2MOVE(pt, annot_ip->verts[tsg->ref_pt]);
	pt[0] = pt[0] + 1;
	V2MOVE(annot_ip->verts[tsg->ref_pt], pt);
    }else if (tsg->rel_pos == RT_TXT_POS_BC) {
	V2MOVE(pt, annot_ip->verts[tsg->ref_pt]);
	pt[0] = pt[0] - (length / 2);
	V2MOVE(annot_ip->verts[tsg->ref_pt], pt);
    }else if (tsg->rel_pos == RT_TXT_POS_BR) {
	V2MOVE(pt, annot_ip->verts[tsg->ref_pt]);
	pt[0] = pt[0] - length;
	V2MOVE(annot_ip->verts[tsg->ref_pt], pt);
    }else if (tsg->rel_pos == RT_TXT_POS_ML) {
	V2MOVE(pt, annot_ip->verts[tsg->ref_pt]);
	pt[0] = pt[0] + 1;
	pt[1] = pt[1] - (height / 2);
	V2MOVE(annot_ip->verts[tsg->ref_pt], pt);
    }else if (tsg->rel_pos == RT_TXT_POS_MC) {
	V2MOVE(pt, annot_ip->verts[tsg->ref_pt]);
	pt[0] = pt[0] - (length / 2);
	pt[1] = pt[1] - (height / 2);
	V2MOVE(annot_ip->verts[tsg->ref_pt], pt);
    }else if (tsg->rel_pos == RT_TXT_POS_MR) {
	V2MOVE(pt, annot_ip->verts[tsg->ref_pt]);
	pt[1] = pt[1] - (height / 2);
	pt[0] = pt[0] - length;
	V2MOVE(annot_ip->verts[tsg->ref_pt], pt);
    }else if (tsg->rel_pos == RT_TXT_POS_TL) {
	V2MOVE(pt, annot_ip->verts[tsg->ref_pt]);
	pt[1] = pt[1] - height;
	V2MOVE(annot_ip->verts[tsg->ref_pt], pt);
    }else if (tsg->rel_pos == RT_TXT_POS_TC) {
	V2MOVE(pt, annot_ip->verts[tsg->ref_pt]);
	pt[0] = pt[0] - (length / 2);
	pt[1] = pt[1] - height;
	V2MOVE(annot_ip->verts[tsg->ref_pt], pt);
    } else {
	//this is the case of TR
	V2MOVE(pt, annot_ip->verts[tsg->ref_pt]);
	pt[0] = pt[0] - length;
	pt[1] = pt[1] - height;
	V2MOVE(annot_ip->verts[tsg->ref_pt], pt);
    }
    return 0;
}


/* FIXME: Unused? */
#if 0
static int
ant_check(const struct rt_ant *ant, const struct rt_annot_internal *annot_ip, int noisy)
{
    size_t i, j;
    int ret=0;

    /* empty annotations are invalid */
    if (ant->count == 0) {
	if (noisy)
	    bu_log("annotation is void\n");
	return 1;
    }

    for (i=0; i<ant->count; i++) {
	const struct line_seg *lsg;
	const struct carc_seg *csg;
	const struct nurb_seg *nsg;
	const struct bezier_seg *bsg;
	const struct txt_seg *tsg;
	const uint32_t *lng;

	lng = (uint32_t *)ant->segments[i];

	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		lsg = (struct line_seg *)lng;
		if ((size_t)lsg->start >= annot_ip->vert_count ||
		    (size_t)lsg->end >= annot_ip->vert_count)
		    ret++;
		break;
	    case CURVE_CARC_MAGIC:
		csg = (struct carc_seg *)lng;
		if ((size_t)csg->start >= annot_ip->vert_count ||
		    (size_t)csg->end >= annot_ip->vert_count)
		    ret++;
		break;
	    case CURVE_NURB_MAGIC:
		nsg = (struct nurb_seg *)lng;
		for (j=0; j<(size_t)nsg->c_size; j++) {
		    if ((size_t)nsg->ctl_points[j] >= annot_ip->vert_count) {
			ret++;
			break;
		    }
		}
		break;
	    case CURVE_BEZIER_MAGIC:
		bsg = (struct bezier_seg *)lng;
		for (j=0; j<=(size_t)bsg->degree; j++) {
		    if ((size_t)bsg->ctl_points[j] >= annot_ip->vert_count) {
			ret++;
			break;
		    }
		}
		break;
	    case ANN_TSEG_MAGIC:
		tsg = (struct txt_seg *)lng;
		if((size_t)tsg->ref_pt >= annot_ip->vert_count)
		    ret++;
		if((size_t)tsg->rel_pos > 9 || (size_t)tsg->rel_pos < 1)
		    ret++;
		break;
	    default:
		ret++;
		if (noisy)
		    bu_log("Unrecognized segment type in the annotation\n");
		break;
	}
    }

    if (ret && noisy)
	bu_log("annotation references non-existent vertices!\n");
    return ret;
}
#endif


/**
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid ANNOTATION, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0 ANNOTATION is OK
 * !0 Error in description
 *
 */
int
rt_annot_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    if (!stp)
	return -1;
    RT_CK_SOLTAB(stp);
    if (ip) RT_CK_DB_INTERNAL(ip);
    if (rtip) RT_CK_RTI(rtip);

    stp->st_specific = (void *)NULL;
    return 0;
}


void
rt_annot_print(const struct soltab *stp)
{
    if (stp) RT_CK_SOLTAB(stp);
}


/**
 * Intersect a ray with an annotation.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_annot_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    if (!stp || !rp || !ap || !seghead)
	return 0;

    RT_CK_SOLTAB(stp);
    RT_CK_RAY(rp);
    RT_CK_APPLICATION(ap);

    /* annotations cannot be ray traced.
     */

    return 0;			/* MISS */
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_annot_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    if (!hitp || !rp)
	return;

    RT_CK_HIT(hitp);
    if (stp) RT_CK_SOLTAB(stp);
    RT_CK_RAY(rp);

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
}


/**
 * Return the curvature of the annotation.
 */
void
rt_annot_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (!cvp || !hitp)
	return;

    RT_CK_HIT(hitp);
    if (stp) RT_CK_SOLTAB(stp);

    cvp->crv_c1 = cvp->crv_c2 = 0;

    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
}


void
rt_annot_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);
    if (hitp) RT_CK_HIT(hitp);
    if (!uvp)
	return;
}


void
rt_annot_free(struct soltab *stp)
{
    if (stp) RT_CK_SOLTAB(stp);
}


static int
seg_to_vlist(struct bu_list *vhead, const struct bg_tess_tol *ttol, fastf_t *V, struct rt_annot_internal *annot_ip, void *seg)
{
    int ret=0;
    int i;
    uint32_t *lng;
    struct line_seg *lsg;
    struct txt_seg *tsg;
    struct carc_seg *csg;
    struct nurb_seg *nsg;
    struct bezier_seg *bsg;
    fastf_t delta;
    point_t center = VINIT_ZERO;
    point_t start_pt = VINIT_ZERO;
    hpoint_t pt = HINIT_ZERO;
    vect_t semi_a, semi_b;
    fastf_t radius;
    vect_t norm;

    BU_CK_LIST_HEAD(vhead);

    VSETALL(semi_a, 0);
    VSETALL(semi_b, 0);
    VSETALL(center, 0);
    VSETALL(V, 0);

    lng = (uint32_t *)seg;
    switch (*lng) {
	case CURVE_LSEG_MAGIC:
	    lsg = (struct line_seg *)lng;
	    if ((size_t)lsg->start >= annot_ip->vert_count || (size_t)lsg->end >= annot_ip->vert_count) {
		ret++;
		break;
	    }
	    V2ADD2(pt, V, annot_ip->verts[lsg->start]);
	    RT_ADD_VLIST(vhead, pt, BV_VLIST_LINE_MOVE);
	    V2ADD2(pt, V, annot_ip->verts[lsg->end]);
	    RT_ADD_VLIST(vhead, pt, BV_VLIST_LINE_DRAW);
	    break;
	case ANN_TSEG_MAGIC:
	    tsg = (struct txt_seg *)lng;
	    if((size_t)tsg->ref_pt >= annot_ip->vert_count) {
		ret++;
		break;
	    }
	    if((size_t)tsg->rel_pos > 9 || (size_t)tsg->rel_pos < 1) {
		ret++;
		break;
	    }
	    ant_pos_adjs(tsg, annot_ip);
	    V2ADD2(pt, V, annot_ip->verts[tsg->ref_pt]);
	    bv_vlist_2string(vhead, &RTG.rtg_vlfree, tsg->label.vls_str, pt[0], pt[1], 5, 0);
	    break;
	case CURVE_CARC_MAGIC:
	    {
		point2d_t mid_pt, start2d, end2d, center2d, s2m, dir, new_uv;
		fastf_t s2m_len_sq, len_sq, tmp_len, cross_z;
		fastf_t start_ang, end_ang, tot_ang, cosdel, sindel;
		fastf_t oldu, oldv, newu, newv;
		int nsegs;

		csg = (struct carc_seg *)lng;
		if ((size_t)csg->start >= annot_ip->vert_count || (size_t)csg->end >= annot_ip->vert_count) {
		    ret++;
		    break;
		}

		delta = M_PI_4;
		if (csg->radius <= 0.0) {
		    V2ADD2(center, V, annot_ip->verts[csg->end]);
		    V2ADD2(pt, V, annot_ip->verts[csg->start]);

		    VSUB2(semi_a, pt, center);
		    VSET(norm, 0, 0, 1);
		    VCROSS(semi_b, norm, semi_a);
		    VUNITIZE(semi_b);
		    radius = MAGNITUDE(semi_a);
		    VSCALE(semi_b, semi_b, radius);
		} else if (csg->radius <= SMALL_FASTF) {
		    bu_log("Radius too small in annotation!\n");
		    break;
		} else {
		    radius = csg->radius;
		}

		if (ttol->abs > 0.0) {
		    fastf_t tmp_delta, ratio;

		    ratio = ttol->abs / radius;
		    if (ratio < 1.0) {
			tmp_delta = 2.0 * acos(1.0 - ratio);
			if (tmp_delta < delta)
			    delta = tmp_delta;
		    }
		}
		if (ttol->rel > 0.0 && ttol->rel < 1.0) {
		    fastf_t tmp_delta;

		    tmp_delta = 2.0 * acos(1.0 - ttol->rel);
		    if (tmp_delta < delta)
			delta = tmp_delta;
		}
		if (ttol->norm > 0.0) {
		    fastf_t normal;

		    normal = ttol->norm * DEG2RAD;
		    if (normal < delta)
			delta = normal;
		}
		if (csg->radius <= 0.0) {
		    /* this is a full circle */
		    nsegs = ceil(M_2PI / delta);
		    delta = M_2PI / (double)nsegs;
		    cosdel = cos(delta);
		    sindel = sin(delta);
		    oldu = 1.0;
		    oldv = 0.0;
		    VJOIN2(start_pt, center, oldu, semi_a, oldv, semi_b);
		    RT_ADD_VLIST(vhead, start_pt, BV_VLIST_LINE_MOVE);
		    for (i=1; i<nsegs; i++) {
			newu = oldu * cosdel - oldv * sindel;
			newv = oldu * sindel + oldv * cosdel;
			VJOIN2(pt, center, newu, semi_a, newv, semi_b);
			RT_ADD_VLIST(vhead, pt, BV_VLIST_LINE_DRAW);
			oldu = newu;
			oldv = newv;
		    }
		    RT_ADD_VLIST(vhead, start_pt, BV_VLIST_LINE_DRAW);
		    break;
		}

		/* this is an arc (not a full circle) */
		V2MOVE(start2d, annot_ip->verts[csg->start]);
		V2MOVE(end2d, annot_ip->verts[csg->end]);
		mid_pt[0] = (start2d[0] + end2d[0]) * 0.5;
		mid_pt[1] = (start2d[1] + end2d[1]) * 0.5;
		V2SUB2(s2m, mid_pt, start2d);
		dir[0] = -s2m[1];
		dir[1] = s2m[0];
		s2m_len_sq =  s2m[0]*s2m[0] + s2m[1]*s2m[1];
		if (s2m_len_sq <= SMALL_FASTF) {
		    bu_log("start and end points are too close together in circular arc of annotation\n");
		    break;
		}
		len_sq = radius*radius - s2m_len_sq;
		if (len_sq < 0.0) {
		    bu_log("Impossible radius for specified start and end points in circular arc\n");
		    break;
		}
		tmp_len = sqrt(dir[0]*dir[0] + dir[1]*dir[1]);
		dir[0] = dir[0] / tmp_len;
		dir[1] = dir[1] / tmp_len;
		tmp_len = sqrt(len_sq);
		V2JOIN1(center2d, mid_pt, tmp_len, dir);

		/* check center location */
		cross_z = (end2d[X] - start2d[X])*(center2d[Y] - start2d[Y]) -
		    (end2d[Y] - start2d[Y])*(center2d[X] - start2d[X]);
		if (!(cross_z > 0.0 && csg->center_is_left))
		    V2JOIN1(center2d, mid_pt, -tmp_len, dir);
		start_ang = atan2(start2d[Y]-center2d[Y], start2d[X]-center2d[X]);
		end_ang = atan2(end2d[Y]-center2d[Y], end2d[X]-center2d[X]);
		if (csg->orientation) {
		    /* clock-wise */
		    while (end_ang > start_ang)
			end_ang -= M_2PI;
		} else {
		    /* counter-clock-wise */
		    while (end_ang < start_ang)
			end_ang += M_2PI;
		}
		tot_ang = end_ang - start_ang;
		nsegs = ceil(tot_ang / delta);
		if (nsegs < 0)
		    nsegs = -nsegs;
		if (nsegs < 3)
		    nsegs = 3;
		delta = tot_ang / nsegs;
		cosdel = cos(delta);
		sindel = sin(delta);

		V2ADD2(center, V, center2d);
		V2ADD2(start_pt, V, start2d);
		oldu = (start2d[0] - center2d[0]);
		oldv = (start2d[1] - center2d[1]);
		RT_ADD_VLIST(vhead, start_pt, BV_VLIST_LINE_MOVE);
		for (i=0; i<nsegs; i++) {
		    newu = oldu * cosdel - oldv * sindel;
		    newv = oldu * sindel + oldv * cosdel;
		    V2SET(new_uv, newu, newv);
		    V2ADD2(pt, center, new_uv);
		    RT_ADD_VLIST(vhead, pt, BV_VLIST_LINE_DRAW);
		    oldu = newu;
		    oldv = newv;
		}
		break;
	    }
	case CURVE_NURB_MAGIC:
	    {
		struct edge_g_cnurb eg;
		int coords;
		fastf_t inv_weight;
		int num_intervals;
		fastf_t param_delta, epsilon;

		nsg = (struct nurb_seg *)lng;
		for (i=0; i<nsg->c_size; i++) {
		    if ((size_t)nsg->ctl_points[i] >= annot_ip->vert_count) {
			ret++;
			break;
		    }
		}
		if (nsg->order < 3) {
		    /* just straight lines */
		    V2ADD2(start_pt, V, annot_ip->verts[nsg->ctl_points[0]]);

		    if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
			inv_weight = 1.0/nsg->weights[0];
			VSCALE(start_pt, start_pt, inv_weight);
		    }
		    RT_ADD_VLIST(vhead, start_pt, BV_VLIST_LINE_MOVE);
		    for (i=1; i<nsg->c_size; i++) {
			V2ADD2(pt, V, annot_ip->verts[nsg->ctl_points[i]]);
			if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
			    inv_weight = 1.0/nsg->weights[i];
			    VSCALE(pt, pt, inv_weight);
			}
			RT_ADD_VLIST(vhead, pt, BV_VLIST_LINE_DRAW);
		    }
		    break;
		}
		eg.l.magic = NMG_EDGE_G_CNURB_MAGIC;
		eg.order = nsg->order;
		eg.k.k_size = nsg->k.k_size;
		eg.k.knots = nsg->k.knots;
		eg.c_size = nsg->c_size;
		coords = 3 + RT_NURB_IS_PT_RATIONAL(nsg->pt_type);
		eg.pt_type = RT_NURB_MAKE_PT_TYPE(coords, 2, RT_NURB_IS_PT_RATIONAL(nsg->pt_type));
		eg.ctl_points = (fastf_t *)bu_malloc(nsg->c_size * coords * sizeof(fastf_t), "eg.ctl_points");
		if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
		    for (i=0; i<nsg->c_size; i++) {
			V2ADD2(&eg.ctl_points[i*coords], V, annot_ip->verts[nsg->ctl_points[i]]);
			eg.ctl_points[(i+1)*coords - 1] = nsg->weights[i];
		    }
		} else {
		    for (i=0; i<nsg->c_size; i++) {
			V2ADD2(&eg.ctl_points[i*coords], V, annot_ip->verts[nsg->ctl_points[i]]);
		    }
		}
		epsilon = MAX_FASTF;
		if (ttol->abs > 0.0 && ttol->abs < epsilon)
		    epsilon = ttol->abs;
		if (ttol->rel > 0.0) {
		    point2d_t min_pt, max_pt, tmp_pt;
		    point2d_t diff;
		    fastf_t tmp_epsilon;

		    min_pt[0] = MAX_FASTF;
		    min_pt[1] = MAX_FASTF;
		    max_pt[0] = -MAX_FASTF;
		    max_pt[1] = -MAX_FASTF;

		    for (i=0; i<nsg->c_size; i++) {
			V2MOVE(tmp_pt, annot_ip->verts[nsg->ctl_points[i]]);
			if (tmp_pt[0] > max_pt[0])
			    max_pt[0] = tmp_pt[0];
			if (tmp_pt[1] > max_pt[1])
			    max_pt[1] = tmp_pt[1];
			if (tmp_pt[0] < min_pt[0])
			    min_pt[0] = tmp_pt[0];
			if (tmp_pt[1] < min_pt[1])
			    min_pt[1] = tmp_pt[1];
		    }

		    V2SUB2(diff, max_pt, min_pt);
		    tmp_epsilon = ttol->rel * sqrt(MAG2SQ(diff));
		    if (tmp_epsilon < epsilon)
			epsilon = tmp_epsilon;

		}
		param_delta = rt_cnurb_par_edge(&eg, epsilon);
		num_intervals = ceil((nsg->k.knots[nsg->k.k_size-1] - nsg->k.knots[0])/param_delta);
		if (num_intervals < 3)
		    num_intervals = 3;
		if (num_intervals > 500) {
		    bu_log("num_intervals was %d, clamped to 500\n", num_intervals);
		    num_intervals = 500;
		}
		param_delta = (nsg->k.knots[nsg->k.k_size-1] - nsg->k.knots[0])/(double)num_intervals;
		for (i=0; i<=num_intervals; i++) {
		    fastf_t t;
		    int j;

		    t = nsg->k.knots[0] + i*param_delta;
		    nmg_nurb_c_eval(&eg, t, pt);
		    if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
			for (j=0; j<coords-1; j++)
			    pt[j] /= pt[coords-1];
		    }
		    if (i == 0)
			RT_ADD_VLIST(vhead, pt, BV_VLIST_LINE_MOVE);
		    else
			RT_ADD_VLIST(vhead, pt, BV_VLIST_LINE_DRAW);
		}
		bu_free((char *)eg.ctl_points, "eg.ctl_points");
		break;
	    }
	case CURVE_BEZIER_MAGIC: {
	    struct bezier_2d_list *bezier_hd, *bz;
	    fastf_t epsilon;

	    bsg = (struct bezier_seg *)lng;

	    for (i=0; i<=bsg->degree; i++) {
		if ((size_t)bsg->ctl_points[i] >= annot_ip->vert_count) {
		    ret++;
		    break;
		}
	    }

	    if (bsg->degree < 1) {
		bu_log("g_annot: ERROR: Bezier curve with illegal degree (%d)\n",
		       bsg->degree);
		ret++;
		break;
	    }

	    if (bsg->degree == 1) {
		/* straight line */
		V2ADD2(start_pt, V, annot_ip->verts[bsg->ctl_points[0]]);
		RT_ADD_VLIST(vhead, start_pt, BV_VLIST_LINE_MOVE);
		for (i=1; i<=bsg->degree; i++) {
		    V2ADD2(pt, V, annot_ip->verts[bsg->ctl_points[i]]);
		    RT_ADD_VLIST(vhead, pt, BV_VLIST_LINE_DRAW);
		}
		break;
	    }

	    /* use tolerance to determine coarseness of plot */
	    epsilon = MAX_FASTF;
	    if (ttol->abs > 0.0 && ttol->abs < epsilon)
		epsilon = ttol->abs;
	    if (ttol->rel > 0.0) {
		point2d_t min_pt, max_pt, tmp_pt;
		point2d_t diff;
		fastf_t tmp_epsilon;

		min_pt[0] = MAX_FASTF;
		min_pt[1] = MAX_FASTF;
		max_pt[0] = -MAX_FASTF;
		max_pt[1] = -MAX_FASTF;

		for (i=0; i<=bsg->degree; i++) {
		    V2MOVE(tmp_pt, annot_ip->verts[bsg->ctl_points[i]]);
		    if (tmp_pt[0] > max_pt[0])
			max_pt[0] = tmp_pt[0];
		    if (tmp_pt[1] > max_pt[1])
			max_pt[1] = tmp_pt[1];
		    if (tmp_pt[0] < min_pt[0])
			min_pt[0] = tmp_pt[0];
		    if (tmp_pt[1] < min_pt[1])
			min_pt[1] = tmp_pt[1];
		}

		V2SUB2(diff, max_pt, min_pt);
		tmp_epsilon = ttol->rel * sqrt(MAG2SQ(diff));
		if (tmp_epsilon < epsilon)
		    epsilon = tmp_epsilon;

	    }


	    /* Create an initial bezier_2d_list */
	    BU_ALLOC(bezier_hd, struct bezier_2d_list);

	    BU_LIST_INIT(&bezier_hd->l);
	    bezier_hd->ctl = (point2d_t *)bu_calloc(bsg->degree + 1, sizeof(point2d_t),
						    "g_annot.c: bezier_hd->ctl");
	    for (i=0; i<=bsg->degree; i++) {
		V2MOVE(bezier_hd->ctl[i], annot_ip->verts[bsg->ctl_points[i]]);
	    }

	    /* now do subdivision as necessary */
	    bezier_hd = bezier_subdivide(bezier_hd, bsg->degree, epsilon, 0);

	    /* plot the results */
	    bz = BU_LIST_FIRST(bezier_2d_list, &bezier_hd->l);
	    V2ADD2(pt, V, bz->ctl[0]);
	    RT_ADD_VLIST(vhead, pt, BV_VLIST_LINE_MOVE);

	    while (BU_LIST_WHILE(bz, bezier_2d_list, &(bezier_hd->l))) {
		BU_LIST_DEQUEUE(&bz->l);
		for (i=1; i<=bsg->degree; i++) {
		    V2ADD2(pt, V, bz->ctl[i]);
		    RT_ADD_VLIST(vhead, pt, BV_VLIST_LINE_DRAW);
		}
		bu_free((char *)bz->ctl, "g_annot.c: bz->ctl");
		bu_free((char *)bz, "g_annot.c: bz");
	    }
	    bu_free((char *)bezier_hd, "g_annot.c: bezier_hd");
	    break;
	}
	default:
	    bu_log("seg_to_vlist: ERROR: unrecognized segment type!\n");
	    break;
    }

    return ret;
}


static int
ant_to_vlist(struct bu_list *vhead, const struct bg_tess_tol *ttol, fastf_t *V, struct rt_annot_internal *annot_ip, struct rt_ant *ant)
{
    size_t seg_no;
    int ret=0;

    BU_CK_LIST_HEAD(vhead);

    RT_VLIST_SET_DISP_MAT(vhead, annot_ip->V);

    for (seg_no=0; seg_no < ant->count; seg_no++) {
	ret += seg_to_vlist(vhead, ttol, V, annot_ip, ant->segments[seg_no]);
    }
    RT_VLIST_SET_MODEL_MAT(vhead);

    return ret;
}


int
rt_annot_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *UNUSED(tol), const struct bv *UNUSED(info))
{
    struct rt_annot_internal *annot_ip;
    int ret;
    int myret=0;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    annot_ip = (struct rt_annot_internal *)ip->idb_ptr;
    RT_ANNOT_CK_MAGIC(annot_ip);

    ret=ant_to_vlist(vhead, ttol, annot_ip->V, annot_ip, &annot_ip->ant);
    if (ret) {
	myret--;
	bu_log("WARNING: Errors in annotation (%d segments reference non-existent vertices)\n",
	       ret);
    }

    return myret;
}


/**
 * Import an annotation from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_annot_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_annot_internal *annot_ip;
    size_t seg_no;
    unsigned char *ptr;
    struct rt_ant *ant;
    size_t i;

    /* must be double for import and export */
    double v[ELEMENTS_PER_VECT];
    double *vp;

    if (dbip) RT_CK_DBI(dbip);
    BU_CK_EXTERNAL(ep);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_ANNOT;
    ip->idb_meth = &OBJ[ID_ANNOT];
    BU_ALLOC(ip->idb_ptr, struct rt_annot_internal);

    annot_ip = (struct rt_annot_internal *)ip->idb_ptr;
    annot_ip->magic = RT_ANNOT_INTERNAL_MAGIC;

    ptr = ep->ext_buf;
    if (mat == NULL) mat = bn_mat_identity;
    bu_cv_ntohd((unsigned char *)v, ptr, ELEMENTS_PER_VECT);
    MAT4X3PNT(annot_ip->V, mat, v);

    ptr += SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT;
    annot_ip->vert_count = ntohl(*(uint32_t *)ptr);
    ptr += SIZEOF_NETWORK_LONG;
    annot_ip->ant.count = ntohl(*(uint32_t *)ptr);
    ptr += SIZEOF_NETWORK_LONG;

    if (annot_ip->vert_count) {
	annot_ip->verts = (point2d_t *)bu_calloc(annot_ip->vert_count, sizeof(point2d_t), "annot_ip->verts");
	vp = (double *)bu_calloc(annot_ip->vert_count, sizeof(double)*ELEMENTS_PER_VECT2D, "vp");
	bu_cv_ntohd((unsigned char *)vp, ptr, annot_ip->vert_count*2);

	/* convert double to fastf_t */
	for (i=0; i<annot_ip->vert_count; i++) {
	    annot_ip->verts[i][X] = vp[(i*ELEMENTS_PER_VECT2D)+0];
	    annot_ip->verts[i][Y] = vp[(i*ELEMENTS_PER_VECT2D)+1];
	}

	bu_free(vp, "vp");
	ptr += SIZEOF_NETWORK_DOUBLE * 2 * annot_ip->vert_count;
    }

    if (annot_ip->ant.count)
	annot_ip->ant.segments = (void **)bu_calloc(annot_ip->ant.count, sizeof(void *), "segs");
    else
	annot_ip->ant.segments = (void **)NULL;
    for (seg_no=0; seg_no < annot_ip->ant.count; seg_no++) {
	uint32_t magic;
	struct line_seg *lsg;
	struct txt_seg *tsg;
	struct carc_seg *csg;
	struct nurb_seg *nsg;
	struct bezier_seg *bsg;


	/* must be double for import and export */
	double scan;
	double *scanp;

	magic = ntohl(*(uint32_t *)ptr);
	ptr += SIZEOF_NETWORK_LONG;
	switch (magic) {
	    case CURVE_LSEG_MAGIC:
		BU_ALLOC(lsg, struct line_seg);
		lsg->magic = magic;
		lsg->start = ntohl(*(uint32_t *)ptr);
		ptr += SIZEOF_NETWORK_LONG;
		lsg->end = ntohl(*(uint32_t *)ptr);
		ptr += SIZEOF_NETWORK_LONG;
		annot_ip->ant.segments[seg_no] = (void *)lsg;
		break;
	    case ANN_TSEG_MAGIC:
		BU_ALLOC(tsg, struct txt_seg);
		tsg->magic = magic;
		tsg->ref_pt = ntohl(*(uint32_t *)ptr);
		ptr += SIZEOF_NETWORK_LONG;
		tsg->rel_pos = ntohl(*(uint32_t *)ptr);
		ptr += SIZEOF_NETWORK_LONG;
		bu_vls_init(&tsg->label);
		bu_vls_strcpy(&tsg->label, (const char*)ptr);
		ptr += bu_vls_strlen(&tsg->label) + 1;
		annot_ip->ant.segments[seg_no] = (void *)tsg;
		break;
	    case CURVE_CARC_MAGIC:
		BU_ALLOC(csg, struct carc_seg);
		csg->magic = magic;
		csg->start = ntohl(*(uint32_t *)ptr);
		ptr += SIZEOF_NETWORK_LONG;
		csg->end = ntohl(*(uint32_t *)ptr);
		ptr += SIZEOF_NETWORK_LONG;
		csg->orientation = ntohl(*(uint32_t *)ptr);
		ptr += SIZEOF_NETWORK_LONG;
		csg->center_is_left = ntohl(*(uint32_t *)ptr);
		ptr += SIZEOF_NETWORK_LONG;
		bu_cv_ntohd((unsigned char *)&scan, ptr, 1);
		csg->radius = scan; /* double to fastf_t */
		ptr += SIZEOF_NETWORK_DOUBLE;
		annot_ip->ant.segments[seg_no] = (void *)csg;
		break;
	    case CURVE_NURB_MAGIC:
		BU_ALLOC(nsg, struct nurb_seg);
		nsg->magic = magic;
		nsg->order = ntohl(*(uint32_t *)ptr);
		ptr += SIZEOF_NETWORK_LONG;
		nsg->pt_type = ntohl(*(uint32_t *)ptr);
		ptr += SIZEOF_NETWORK_LONG;
		nsg->k.k_size = ntohl(*(uint32_t *)ptr);
		ptr += SIZEOF_NETWORK_LONG;

		nsg->k.knots = (fastf_t *)bu_malloc(nsg->k.k_size * sizeof(fastf_t), "nsg->k.knots");
		scanp = (double *)bu_malloc(nsg->k.k_size * sizeof(double), "scanp");
		bu_cv_ntohd((unsigned char *)scanp, ptr, nsg->k.k_size);

		/* convert double to fastf_t */
		for (i=0; i<(size_t)nsg->k.k_size; i++) {
		    nsg->k.knots[i] = scanp[i];
		}
		bu_free(scanp, "scanp");

		ptr += SIZEOF_NETWORK_DOUBLE * nsg->k.k_size;
		nsg->c_size = ntohl(*(uint32_t *)ptr);
		ptr += SIZEOF_NETWORK_LONG;
		nsg->ctl_points = (int *)bu_malloc(nsg->c_size * sizeof(int), "nsg->ctl_points");
		for (i=0; i<(size_t)nsg->c_size; i++) {
		    nsg->ctl_points[i] = ntohl(*(uint32_t *)ptr);
		    ptr += SIZEOF_NETWORK_LONG;
		}
		if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
		    nsg->weights = (fastf_t *)bu_malloc(nsg->c_size * sizeof(fastf_t), "nsg->weights");
		    scanp = (double *)bu_malloc(nsg->c_size * sizeof(double), "scanp");
		    bu_cv_ntohd((unsigned char *)scanp, ptr, nsg->c_size);

		    /* convert double to fastf_t */
		    for (i=0; i<(size_t)nsg->k.k_size; i++) {
			nsg->weights[i] = scanp[i];
		    }
		    bu_free(scanp, "scanp");

		    ptr += SIZEOF_NETWORK_DOUBLE * nsg->c_size;
		} else
		    nsg->weights = (fastf_t *)NULL;
		annot_ip->ant.segments[seg_no] = (void *)nsg;
		break;
	    case CURVE_BEZIER_MAGIC:
		BU_ALLOC(bsg, struct bezier_seg);
		bsg->magic = magic;
		bsg->degree = ntohl(*(uint32_t *)ptr);
		ptr += SIZEOF_NETWORK_LONG;
		bsg->ctl_points = (int *)bu_calloc(bsg->degree+1, sizeof(int), "bsg->ctl_points");
		for (i=0; i<=(size_t)bsg->degree; i++) {
		    bsg->ctl_points[i] = ntohl(*(uint32_t *)ptr);
		    ptr += SIZEOF_NETWORK_LONG;
		}
		annot_ip->ant.segments[seg_no] = (void *)bsg;
		break;
	    default:
		bu_bomb("rt_annot_import5: ERROR: unrecognized segment type!\n");
		break;
	}
    }

    ant = &annot_ip->ant;

    if (ant->count) {
	ant->reverse = (int *)bu_calloc(ant->count, sizeof(int), "ant->reverse");
    }

    for (i=0; i<ant->count; i++) {
	ant->reverse[i] = ntohl(*(uint32_t *)ptr);
	ptr += SIZEOF_NETWORK_LONG;
    }

    return 0;			/* OK */
}


/**
 * The name is added by the caller, in the usual place.
 */
int
rt_annot_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_annot_internal *annot_ip;
    unsigned char *cp;
    size_t seg_no;
    size_t i;

    /* must be double for import and export */
    double tmp_vec[ELEMENTS_PER_VECT];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_ANNOT) return -1;
    annot_ip = (struct rt_annot_internal *)ip->idb_ptr;
    RT_ANNOT_CK_MAGIC(annot_ip);

    BU_CK_EXTERNAL(ep);

    /* tally up size of buffer needed */
    ep->ext_nbytes =  (ELEMENTS_PER_VECT * SIZEOF_NETWORK_DOUBLE)	/* V*/
	+ 2 * SIZEOF_NETWORK_LONG		/* vert_count and count */
	+ 2 * annot_ip->vert_count * SIZEOF_NETWORK_DOUBLE	/* 2D-vertices */
	+ annot_ip->ant.count * SIZEOF_NETWORK_LONG;	/* reverse flags */

    for (seg_no=0; seg_no < annot_ip->ant.count; seg_no++) {
	uint32_t *lng;
	struct nurb_seg *nseg;
	struct bezier_seg *bseg;
	struct txt_seg *tseg;

	lng = (uint32_t *)annot_ip->ant.segments[seg_no];
	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		/* magic + start + end */
		ep->ext_nbytes += 3 * SIZEOF_NETWORK_LONG;
		break;
	    case ANN_TSEG_MAGIC:
		tseg = (struct txt_seg*)lng;
		/* magic + ref_pt + pt_rel_pos + label->vls_str length + 1 for the null terminator*/
		ep->ext_nbytes += 3 * SIZEOF_NETWORK_LONG + bu_vls_strlen(&tseg->label) + 1;
		break;
	    case CURVE_CARC_MAGIC:
		/* magic + start + end + orientation + center_is_left + (double)radius*/
		ep->ext_nbytes += 5 * SIZEOF_NETWORK_LONG + SIZEOF_NETWORK_DOUBLE;
		break;
	    case CURVE_NURB_MAGIC:
		nseg = (struct nurb_seg *)lng;
		/* magic + order + pt_type + c_size */
		ep->ext_nbytes += 4 * SIZEOF_NETWORK_LONG;
		/* (double)knots */
		ep->ext_nbytes += SIZEOF_NETWORK_LONG + nseg->k.k_size * SIZEOF_NETWORK_DOUBLE;
		/* control point count */
		ep->ext_nbytes += nseg->c_size * SIZEOF_NETWORK_LONG;
		if (RT_NURB_IS_PT_RATIONAL(nseg->pt_type))
		    /* (double)weights */
		    ep->ext_nbytes += nseg->c_size * SIZEOF_NETWORK_DOUBLE;
		break;
	    case CURVE_BEZIER_MAGIC:
		bseg = (struct bezier_seg *)lng;
		/* magic + degree */
		ep->ext_nbytes += 2 * SIZEOF_NETWORK_LONG;
		/* control points */
		ep->ext_nbytes += (bseg->degree + 1) * SIZEOF_NETWORK_LONG;
		break;
	    default:
		bu_log("rt_annot_export5: unsupported segment type (x%x)\n", *lng);
		bu_bomb("rt_annot_export5: unsupported segment type\n");
	}
    }
    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "annotation external");

    cp = (unsigned char *)ep->ext_buf;

    /* scale and export */
    VSCALE(tmp_vec, annot_ip->V, local2mm);
    bu_cv_htond(cp, (unsigned char *)tmp_vec, ELEMENTS_PER_VECT);
    cp += ELEMENTS_PER_VECT * SIZEOF_NETWORK_DOUBLE;


    *(uint32_t *)cp = htonl(annot_ip->vert_count);
    cp += SIZEOF_NETWORK_LONG;
    *(uint32_t *)cp = htonl(annot_ip->ant.count);
    cp += SIZEOF_NETWORK_LONG;

    /* convert 2D points to mm */
    for (i=0; i<annot_ip->vert_count; i++) {
	/* must be double for import and export */
	double pt2d[ELEMENTS_PER_VECT2D];

	V2SCALE(pt2d, annot_ip->verts[i], local2mm);
	bu_cv_htond(cp, (const unsigned char *)pt2d, ELEMENTS_PER_VECT2D);
	cp += 2 * SIZEOF_NETWORK_DOUBLE;
    }

    for (seg_no=0; seg_no < annot_ip->ant.count; seg_no++) {
	struct line_seg *lseg;
	struct txt_seg *tseg;
	struct carc_seg *cseg;
	struct nurb_seg *nseg;
	struct bezier_seg *bseg;
	uint32_t *lng;

	/* must be double for import and export */
	double scan;
	double *scanp;

	/* write segment type ID, and segment parameters */
	lng = (uint32_t *)annot_ip->ant.segments[seg_no];
	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		lseg = (struct line_seg *)lng;
		*(uint32_t *)cp = htonl(CURVE_LSEG_MAGIC);
		cp += SIZEOF_NETWORK_LONG;
		*(uint32_t *)cp = htonl(lseg->start);
		cp += SIZEOF_NETWORK_LONG;
		*(uint32_t *)cp = htonl(lseg->end);
		cp += SIZEOF_NETWORK_LONG;
		break;
	    case ANN_TSEG_MAGIC:
		tseg = (struct txt_seg *)lng;
		*(uint32_t *)cp = htonl(ANN_TSEG_MAGIC);
		cp += SIZEOF_NETWORK_LONG;
		*(uint32_t *)cp = htonl(tseg->ref_pt);
		cp += SIZEOF_NETWORK_LONG;
		*(uint32_t *)cp = htonl(tseg->rel_pos);
		cp += SIZEOF_NETWORK_LONG;

		bu_strlcpy((char *)cp, bu_vls_addr(&tseg->label), bu_vls_strlen(&tseg->label) + 1);

		cp += bu_vls_strlen(&tseg->label) + 1;
		break;
	    case CURVE_CARC_MAGIC:
		cseg = (struct carc_seg *)lng;
		*(uint32_t *)cp = htonl(CURVE_CARC_MAGIC);
		cp += SIZEOF_NETWORK_LONG;
		*(uint32_t *)cp = htonl(cseg->start);
		cp += SIZEOF_NETWORK_LONG;
		*(uint32_t *)cp = htonl(cseg->end);
		cp += SIZEOF_NETWORK_LONG;
		*(uint32_t *)cp = htonl(cseg->orientation);
		cp += SIZEOF_NETWORK_LONG;
		*(uint32_t *)cp = htonl(cseg->center_is_left);
		cp += SIZEOF_NETWORK_LONG;
		scan = cseg->radius * local2mm;
		bu_cv_htond(cp, (unsigned char *)&scan, 1);
		cp += SIZEOF_NETWORK_DOUBLE;
		break;
	    case CURVE_NURB_MAGIC:
		nseg = (struct nurb_seg *)lng;
		*(uint32_t *)cp = htonl(CURVE_NURB_MAGIC);
		cp += SIZEOF_NETWORK_LONG;
		*(uint32_t *)cp = htonl(nseg->order);
		cp += SIZEOF_NETWORK_LONG;
		*(uint32_t *)cp = htonl(nseg->pt_type);
		cp += SIZEOF_NETWORK_LONG;
		*(uint32_t *)cp = htonl(nseg->k.k_size);
		cp += SIZEOF_NETWORK_LONG;
		scanp = (double *)bu_malloc(nseg->k.k_size * sizeof(double), "scanp");
		/* convert fastf_t to double */
		for (i=0; i<(size_t)nseg->k.k_size; i++) {
		    scanp[i] = nseg->k.knots[i];
		}
		bu_cv_htond(cp, (const unsigned char *)nseg->k.knots, nseg->k.k_size);
		bu_free(scanp, "scanp");
		cp += nseg->k.k_size * SIZEOF_NETWORK_DOUBLE;
		*(uint32_t *)cp = htonl(nseg->c_size);
		cp += SIZEOF_NETWORK_LONG;
		for (i=0; i<(size_t)nseg->c_size; i++) {
		    *(uint32_t *)cp = htonl(nseg->ctl_points[i]);
		    cp += SIZEOF_NETWORK_LONG;
		}
		if (RT_NURB_IS_PT_RATIONAL(nseg->pt_type)) {
		    scanp = (double *)bu_malloc(nseg->c_size * sizeof(double), "scanp");
		    /* convert fastf_t to double */
		    for (i=0; i<(size_t)nseg->c_size; i++) {
			scanp[i] = nseg->weights[i];
		    }
		    bu_cv_htond(cp, (const unsigned char *)scanp, nseg->c_size);
		    bu_free(scanp, "scanp");
		    cp += SIZEOF_NETWORK_DOUBLE * nseg->c_size;
		}
		break;
	    case CURVE_BEZIER_MAGIC:
		bseg = (struct bezier_seg *)lng;
		*(uint32_t *)cp = htonl(CURVE_BEZIER_MAGIC);
		cp += SIZEOF_NETWORK_LONG;
		*(uint32_t *)cp = htonl(bseg->degree);
		cp += SIZEOF_NETWORK_LONG;
		for (i=0; i<=(size_t)bseg->degree; i++) {
		    *(uint32_t *)cp = htonl(bseg->ctl_points[i]);
		    cp += SIZEOF_NETWORK_LONG;
		}
		break;
	    default:
		bu_bomb("rt_annot_export5: ERROR: unrecognized segment type!\n");
		break;

	}
    }

    for (seg_no=0; seg_no < annot_ip->ant.count; seg_no++) {
	*(uint32_t *)cp = htonl(annot_ip->ant.reverse[seg_no]);
	cp += SIZEOF_NETWORK_LONG;
    }

    return 0;
}


/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_annot_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_annot_internal *annot_ip =
	(struct rt_annot_internal *)ip->idb_ptr;
    char *rel_pos = NULL;
    size_t i;
    size_t seg_no;
    char buf[256];
    point_t V;

    RT_ANNOT_CK_MAGIC(annot_ip);
    bu_vls_strcat(str, "2D annotations (annotation)\n");

    VSCALE(V, annot_ip->V, mm2local);

    sprintf(buf, "\tV = (%g %g %g)\n\t%lu vertices\n",
	    V3INTCLAMPARGS(V),
	    (long unsigned)annot_ip->vert_count);
    bu_vls_strcat(str, buf);

    if (!verbose)
	return 0;

    if (annot_ip->vert_count) {
	bu_vls_strcat(str, "\tVertices:\n\t");
	for (i=0; i<annot_ip->vert_count; i++) {
	    sprintf(buf, " %lu-(%g %g)", (long unsigned)i, V2INTCLAMPARGS(annot_ip->verts[i]));
	    bu_vls_strcat(str, buf);
	    if (i && (i+1)%3 == 0)
		bu_vls_strcat(str, "\n\t");
	}
    }
    bu_vls_strcat(str, "\n");

    sprintf(buf, "\n\tAnt:\n");
    bu_vls_strcat(str, buf);
    for (seg_no=0; seg_no < annot_ip->ant.count; seg_no++) {
	struct line_seg *lsg;
	struct txt_seg *tsg;
	struct carc_seg *csg;
	struct nurb_seg *nsg;
	struct bezier_seg *bsg;

	lsg = (struct line_seg *)annot_ip->ant.segments[seg_no];
	switch (lsg->magic) {
	    case CURVE_LSEG_MAGIC:
		lsg = (struct line_seg *)annot_ip->ant.segments[seg_no];
		if ((size_t)lsg->start >= annot_ip->vert_count || (size_t)lsg->end >= annot_ip->vert_count) {
		    if (annot_ip->ant.reverse[seg_no])
			sprintf(buf, "\t\tLine segment from vertex #%d to #%d\n",
				lsg->end, lsg->start);
		    else
			sprintf(buf, "\t\tLine segment from vertex #%d to #%d\n",
				lsg->start, lsg->end);
		} else {
		    if (annot_ip->ant.reverse[seg_no])
			sprintf(buf, "\t\tLine segment (%g %g) <-> (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[lsg->end]),
				V2INTCLAMPARGS(annot_ip->verts[lsg->start]));
		    else
			sprintf(buf, "\t\tLine segment (%g %g) <-> (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[lsg->start]),
				V2INTCLAMPARGS(annot_ip->verts[lsg->end]));
		}
		bu_vls_strcat(str, buf);
		break;
	    case ANN_TSEG_MAGIC:
		tsg = (struct txt_seg *)annot_ip->ant.segments[seg_no];
		if((size_t)tsg->ref_pt >= annot_ip->vert_count) {
		    sprintf(buf, "\t\tReference point vertex #%d\n", tsg->ref_pt);
		}
		else {
		    sprintf(buf, "\t\tReference point (%g %g)\n",
			    V2INTCLAMPARGS(annot_ip->verts[tsg->ref_pt]));
		}
		bu_vls_strcat(str, buf);
		ant_check_pos(tsg, &rel_pos);
		sprintf(buf, "\t\tRelative position: %s\n", rel_pos);
		bu_vls_strcat(str, buf);
		sprintf(buf, "\tLabel text: %s\n", bu_vls_addr(&tsg->label));
		bu_vls_strcat(str, buf);
		break;
	    case CURVE_CARC_MAGIC:
		csg = (struct carc_seg *)annot_ip->ant.segments[seg_no];
		if (csg->radius < 0.0) {
		    bu_vls_strcat(str, "\t\tFull Circle:\n");

		    if ((size_t)csg->end >= annot_ip->vert_count || (size_t)csg->start >= annot_ip->vert_count) {
			sprintf(buf, "\t\tcenter at vertex #%d\n",
				csg->end);
			bu_vls_strcat(str, buf);
			sprintf(buf, "\t\tpoint on circle at vertex #%d\n",
				csg->start);
		    } else {
			sprintf(buf, "\t\t\tcenter: (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[csg->end]));
			bu_vls_strcat(str, buf);
			sprintf(buf, "\t\t\tpoint on circle: (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[csg->start]));
		    }
		    bu_vls_strcat(str, buf);
		} else {
		    bu_vls_strcat(str, "\t\tCircular Arc:\n");

		    if ((size_t)csg->end >= annot_ip->vert_count || (size_t)csg->start >= annot_ip->vert_count) {
			sprintf(buf, "\t\t\tstart at vertex #%d\n",
				csg->start);
			bu_vls_strcat(str, buf);
			sprintf(buf, "\t\t\tend at vertex #%d\n",
				csg->end);
			bu_vls_strcat(str, buf);
		    } else {
			sprintf(buf, "\t\t\tstart: (%g, %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[csg->start]));
			bu_vls_strcat(str, buf);
			sprintf(buf, "\t\t\tend: (%g, %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[csg->end]));
			bu_vls_strcat(str, buf);
		    }
		    sprintf(buf, "\t\t\tradius: %g\n", csg->radius*mm2local);
		    bu_vls_strcat(str, buf);
		    if (csg->orientation)
			bu_vls_strcat(str, "\t\t\tcurve is clock-wise\n");
		    else
			bu_vls_strcat(str, "\t\t\tcurve is counter-clock-wise\n");
		    if (csg->center_is_left)
			bu_vls_strcat(str, "\t\t\tcenter of curvature is left of the line from start point to end point\n");
		    else
			bu_vls_strcat(str, "\t\t\tcenter of curvature is right of the line from start point to end point\n");
		    if (annot_ip->ant.reverse[seg_no])
			bu_vls_strcat(str, "\t\t\tarc is reversed\n");
		}
		break;
	    case CURVE_NURB_MAGIC:
		nsg = (struct nurb_seg *)annot_ip->ant.segments[seg_no];
		bu_vls_strcat(str, "\t\tNURB Curve:\n");
		if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
		    sprintf(buf, "\t\t\tCurve is rational\n");
		    bu_vls_strcat(str, buf);
		}
		sprintf(buf, "\t\t\torder = %d, number of control points = %d\n",
			nsg->order, nsg->c_size);
		bu_vls_strcat(str, buf);
		if ((size_t)nsg->ctl_points[0] >= annot_ip->vert_count ||
		    (size_t)nsg->ctl_points[nsg->c_size-1] >= annot_ip->vert_count) {
		    if (annot_ip->ant.reverse[seg_no])
			sprintf(buf, "\t\t\tstarts at vertex #%d\n\t\t\tends at vertex #%d\n",
				nsg->ctl_points[nsg->c_size-1],
				nsg->ctl_points[0]);
		    else
			sprintf(buf, "\t\t\tstarts at vertex #%d\n\t\t\tends at vertex #%d\n",
				nsg->ctl_points[0],
				nsg->ctl_points[nsg->c_size-1]);
		} else {
		    if (annot_ip->ant.reverse[seg_no])
			sprintf(buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[nsg->ctl_points[nsg->c_size-1]]),
				V2INTCLAMPARGS(annot_ip->verts[nsg->ctl_points[0]]));
		    else
			sprintf(buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[nsg->ctl_points[0]]),
				V2INTCLAMPARGS(annot_ip->verts[nsg->ctl_points[nsg->c_size-1]]));
		}
		bu_vls_strcat(str, buf);
		sprintf(buf, "\t\t\tknot values are %g to %g\n",
			INTCLAMP(nsg->k.knots[0]), INTCLAMP(nsg->k.knots[nsg->k.k_size-1]));
		bu_vls_strcat(str, buf);
		break;
	    case CURVE_BEZIER_MAGIC:
		bsg = (struct bezier_seg *)annot_ip->ant.segments[seg_no];
		bu_vls_strcat(str, "\t\tBezier segment:\n");
		sprintf(buf, "\t\t\tdegree = %d\n", bsg->degree);
		bu_vls_strcat(str, buf);
		if ((size_t)bsg->ctl_points[0] >= annot_ip->vert_count ||
		    (size_t)bsg->ctl_points[bsg->degree] >= annot_ip->vert_count) {
		    if (annot_ip->ant.reverse[seg_no]) {
			sprintf(buf, "\t\t\tstarts at vertex #%d\n\t\t\tends at vertex #%d\n",
				bsg->ctl_points[bsg->degree],
				bsg->ctl_points[0]);
		    } else {
			sprintf(buf, "\t\t\tstarts at vertex #%d\n\t\t\tends at vertex #%d\n",
				bsg->ctl_points[0],
				bsg->ctl_points[bsg->degree]);
		    }
		} else {
		    if (annot_ip->ant.reverse[seg_no])
			sprintf(buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[bsg->ctl_points[bsg->degree]]),
				V2INTCLAMPARGS(annot_ip->verts[bsg->ctl_points[0]]));
		    else
			sprintf(buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
				V2INTCLAMPARGS(annot_ip->verts[bsg->ctl_points[0]]),
				V2INTCLAMPARGS(annot_ip->verts[bsg->ctl_points[bsg->degree]]));
		}
		bu_vls_strcat(str, buf);
		break;
	    default:
		bu_bomb("rt_annot_describe: ERROR: unrecognized segment type\n");
	}
    }

    return 0;
}


void
rt_ant_free(struct rt_ant *ant)
{
    size_t i;

    if (ant->count)
	bu_free((char *)ant->reverse, "ant->reverse");
    for (i=0; i<ant->count; i++) {
	uint32_t *lng;
	struct nurb_seg *nsg;
	struct bezier_seg *bsg;
	struct txt_seg *tsg;

	lng = (uint32_t *)ant->segments[i];
	switch (*lng) {
	    case CURVE_NURB_MAGIC:
		nsg = (struct nurb_seg *)lng;
		bu_free((char *)nsg->ctl_points, "nsg->ctl_points");
		if (nsg->weights)
		    bu_free((char *)nsg->weights, "nsg->weights");
		bu_free((char *)nsg->k.knots, "nsg->k.knots");
		bu_free((char *)lng, "annotation segment");
		break;
	    case CURVE_BEZIER_MAGIC:
		bsg = (struct bezier_seg *)lng;
		bu_free((char *)bsg->ctl_points, "bsg->ctl_points");
		bu_free((char *)lng, "annotation segment");
		break;
	    case CURVE_LSEG_MAGIC:
		bu_free((char *)lng, "annotation segment");
		break;
	    case ANN_TSEG_MAGIC:
		tsg = (struct txt_seg *)lng;
		if (BU_VLS_IS_INITIALIZED(&tsg->label))
		    bu_vls_free(&tsg->label);
		bu_free((char *)lng, "annotation segment");
		break;
	    case CURVE_CARC_MAGIC:
		bu_free((char *)lng, "annotation segment");
		break;
	    default:
		bu_log("ERROR: rt_annot_free: unrecognized annotation segment type!\n");
		break;
	}
    }

    if (ant->count > 0)
	bu_free((char *)ant->segments, "ant->segments");

    ant->count = 0;
    ant->reverse = (int *)NULL;
    ant->segments = (void **)NULL;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_annot_ifree(struct rt_db_internal *ip)
{
    struct rt_annot_internal *annot_ip;
    struct rt_ant *ant;

    RT_CK_DB_INTERNAL(ip);

    annot_ip = (struct rt_annot_internal *)ip->idb_ptr;
    RT_ANNOT_CK_MAGIC(annot_ip);
    annot_ip->magic = 0;			/* sanity */

    if (annot_ip->verts)
	bu_free((char *)annot_ip->verts, "annot_ip->verts");

    ant = &annot_ip->ant;

    rt_ant_free(ant);

    bu_free((char *)annot_ip, "annotation ifree");
    ip->idb_ptr = ((void *)0);	/* sanity */
}


static void
ant_copy(struct rt_ant *ant_out, const struct rt_ant *ant_in)
{
    size_t i, j;

    ant_out->count = ant_in->count;
    if (ant_out->count) {
	ant_out->reverse = (int *)bu_calloc(ant_out->count, sizeof(int), "ant->reverse");
	ant_out->segments = (void **)bu_calloc(ant_out->count, sizeof(void *), "ant->segments");
    }

    for (j=0; j<ant_out->count; j++) {
	uint32_t *lng;
	struct line_seg *lsg_out, *lsg_in;
	struct txt_seg *tsg_out, *tsg_in;
	struct carc_seg *csg_out, *csg_in;
	struct nurb_seg *nsg_out, *nsg_in;
	struct bezier_seg *bsg_out, *bsg_in;

	ant_out->reverse[j] = ant_in->reverse[j];
	lng = (uint32_t *)ant_in->segments[j];
	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		lsg_in = (struct line_seg *)lng;
		BU_ALLOC(lsg_out, struct line_seg);
		ant_out->segments[j] = (void *)lsg_out;
		*lsg_out = *lsg_in;
		break;
	    case ANN_TSEG_MAGIC:
		tsg_in = (struct txt_seg *)lng;
		BU_ALLOC(tsg_out, struct txt_seg);
		ant_out->segments[j] = (void *)tsg_out;
		*tsg_out = *tsg_in;
		break;
	    case CURVE_CARC_MAGIC:
		csg_in = (struct carc_seg *)lng;
		BU_ALLOC(csg_out, struct carc_seg);
		ant_out->segments[j] = (void *)csg_out;
		*csg_out = *csg_in;
		break;
	    case CURVE_NURB_MAGIC:
		nsg_in = (struct nurb_seg *)lng;
		BU_ALLOC(nsg_out, struct nurb_seg);
		ant_out->segments[j] = (void *)nsg_out;
		*nsg_out = *nsg_in;
		nsg_out->ctl_points = (int *)bu_calloc(nsg_in->c_size, sizeof(int), "nsg_out->ctl_points");
		for (i=0; i<(size_t)nsg_out->c_size; i++)
		    nsg_out->ctl_points[i] = nsg_in->ctl_points[i];
		if (RT_NURB_IS_PT_RATIONAL(nsg_in->pt_type)) {
		    nsg_out->weights = (fastf_t *)bu_malloc(nsg_out->c_size * sizeof(fastf_t), "nsg_out->weights");
		    for (i=0; i<(size_t)nsg_out->c_size; i++)
			nsg_out->weights[i] = nsg_in->weights[i];
		} else
		    nsg_out->weights = (fastf_t *)NULL;
		nsg_out->k.knots = (fastf_t *)bu_malloc(nsg_in->k.k_size * sizeof(fastf_t), "nsg_out->k.knots");
		for (i=0; i<(size_t)nsg_in->k.k_size; i++)
		    nsg_out->k.knots[i] = nsg_in->k.knots[i];
		break;
	    case CURVE_BEZIER_MAGIC:
		bsg_in = (struct bezier_seg *)lng;
		BU_ALLOC(bsg_out, struct bezier_seg);
		ant_out->segments[j] = (void *)bsg_out;
		*bsg_out = *bsg_in;
		bsg_out->ctl_points = (int *)bu_calloc(bsg_out->degree + 1,
						       sizeof(int), "bsg_out->ctl_points");
		for (i=0; i<=(size_t)bsg_out->degree; i++) {
		    bsg_out->ctl_points[i] = bsg_in->ctl_points[i];
		}
		break;
	    default:
		bu_bomb("ERROR: unrecognized segment type enountered while copying annotation\n");
	}
    }

}


struct rt_annot_internal *
rt_copy_annot(const struct rt_annot_internal *annot_ip)
{
    struct rt_annot_internal *out;
    size_t i;
    struct rt_ant *ant_out;

    RT_ANNOT_CK_MAGIC(annot_ip);

    BU_ALLOC(out, struct rt_annot_internal);
    *out = *annot_ip;	/* struct copy */

    if (out->vert_count)
	out->verts = (point2d_t *)bu_calloc(out->vert_count, sizeof(point2d_t), "out->verts");

    for (i=0; annot_ip->verts && i<out->vert_count; i++) {
	V2MOVE(out->verts[i], annot_ip->verts[i]);
    }

    ant_out = &out->ant;
    if (ant_out)
	ant_copy(ant_out, &annot_ip->ant);

    return out;
}


static int
ant_to_tcl_list(struct bu_vls *vls, struct rt_ant *ant)
{
    size_t i, j;
    char *rel_pos = NULL;

    bu_vls_printf(vls, " SL {");
    for (j=0; j<ant->count; j++) {
	switch ((*(uint32_t *)ant->segments[j])) {
	    case CURVE_LSEG_MAGIC:
		{
		    struct line_seg *lsg = (struct line_seg *)ant->segments[j];
		    bu_vls_printf(vls, " { line S %d E %d }", lsg->start, lsg->end);
		}
		break;
	    case ANN_TSEG_MAGIC:
		{
		    struct txt_seg *tsg = (struct txt_seg *)ant->segments[j];
		    ant_check_pos(tsg, &rel_pos);
		    bu_vls_printf(vls, " { label %s ref_pt %d position %s }", bu_vls_addr(&tsg->label), tsg->ref_pt, rel_pos);
		}
		break;
	    case CURVE_CARC_MAGIC:
		{
		    struct carc_seg *csg = (struct carc_seg *)ant->segments[j];
		    bu_vls_printf(vls, " { carc S %d E %d R %.25g L %d O %d }",
				  csg->start, csg->end, csg->radius,
				  csg->center_is_left, csg->orientation);
		}
		break;
	    case CURVE_BEZIER_MAGIC:
		{
		    struct bezier_seg *bsg = (struct bezier_seg *)ant->segments[j];
		    bu_vls_printf(vls, " { bezier D %d P {", bsg->degree);
		    for (i=0; i<=(size_t)bsg->degree; i++)
			bu_vls_printf(vls, " %d", bsg->ctl_points[i]);
		    bu_vls_printf(vls, " } }");
		}
		break;
	    case CURVE_NURB_MAGIC:
		{
		    size_t k;
		    struct nurb_seg *nsg = (struct nurb_seg *)ant->segments[j];
		    bu_vls_printf(vls, " { nurb O %d T %d K {",
				  nsg->order, nsg->pt_type);
		    for (k=0; k<(size_t)nsg->k.k_size; k++)
			bu_vls_printf(vls, " %.25g", nsg->k.knots[k]);
		    bu_vls_strcat(vls, "} P {");
		    for (k=0; k<(size_t)nsg->c_size; k++)
			bu_vls_printf(vls, " %d", nsg->ctl_points[k]);
		    if (nsg->weights) {
			bu_vls_strcat(vls, "} W {");
			for (k=0; k<(size_t)nsg->c_size; k++)
			    bu_vls_printf(vls, " %.25g", nsg->weights[k]);
		    }
		    bu_vls_strcat(vls, "} }");
		}
		break;
	}
    }
    bu_vls_strcat(vls, " }");	/* end of segment list */

    return 0;
}


int
rt_annot_form(struct bu_vls *logstr, const struct rt_functab *ftp)
{
    BU_CK_VLS(logstr);
    RT_CK_FUNCTAB(ftp);

    bu_vls_printf(logstr, "V {%%f %%f %%f} VL {{%%f %%f} {%%f %%f} ...} SL {{segment_data} {segment_data}}");

    return BRLCAD_OK;
}


int
rt_annot_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    struct rt_annot_internal *ann=(struct rt_annot_internal *)intern->idb_ptr;
    size_t i;
    struct rt_ant *ant;

    BU_CK_VLS(logstr);
    RT_ANNOT_CK_MAGIC(ann);

    if (attr == (char *)NULL) {
	bu_vls_strcpy(logstr, "annotation");
	bu_vls_printf(logstr, " V {%.25g %.25g %.25g}", V3ARGS(ann->V));
	bu_vls_strcat(logstr, " VL {");
	for (i=0; i<ann->vert_count; i++)
	    bu_vls_printf(logstr, " {%.25g %.25g}", V2ARGS(ann->verts[i]));
	bu_vls_strcat(logstr, " }");

	ant = &ann->ant;
	if (ant_to_tcl_list(logstr, ant)) {
	    return BRLCAD_ERROR;
	}
    } else if (BU_STR_EQUAL(attr, "V")) {
	bu_vls_printf(logstr, "%.25g %.25g %.25g", V3ARGS(ann->V));
    } else if (BU_STR_EQUAL(attr, "VL")) {
	for (i=0; i<ann->vert_count; i++)
	    bu_vls_printf(logstr, " {%.25g %.25g}", V2ARGS(ann->verts[i]));
    } else if (BU_STR_EQUAL(attr, "SL")) {
	ant = &ann->ant;
	if (ant_to_tcl_list(logstr, ant)) {
	    return BRLCAD_ERROR;
	}
    } else if (*attr == 'V') {
	long lval = atol((attr+1));
	if (lval < 0 || (size_t)lval >= ann->vert_count) {
	    bu_vls_printf(logstr, "ERROR: Illegal vertex number\n");
	    return BRLCAD_ERROR;
	}
	bu_vls_printf(logstr, "%.25g %.25g", V2ARGS(ann->verts[lval]));
    } else {
	/* unrecognized attribute */
	bu_vls_printf(logstr, "ERROR: Unknown attribute, choices are V, VL, SL, or V#\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


static int
ant_get_tcl(struct bu_vls *logstr, struct rt_ant *ant, const char *argv1)
{
    int count;
    int j;
    const char **seg_list = NULL;

    /* split initial list */
    if (bu_argv_from_tcl_list(argv1, &count, (const char ***)&seg_list) != 0) {
	return -1;
    }

    if (count) {
	ant->count = count;
	ant->reverse = (int *)bu_calloc(count, sizeof(int), "ant->reverse");
	ant->segments = (void **)bu_calloc(count, sizeof(void *), "ant->segments");
    }

    /* loop through all the segments */
    for (j = 0; j < count; j++) {
	int seg_argc;
	const char **seg_argv = NULL;
	const char *elem, *sval;
	int k;

	/* get the next segment */
	if (bu_argv_from_tcl_list(seg_list[j], &seg_argc, (const char ***)&seg_argv) != 0) {
	    return -1;
	}

	if (seg_argc < 1) return 0;

	/* get the next segment */
	if (BU_STR_EQUAL(seg_argv[0], "line")) {
	    struct line_seg *lsg;

	    BU_ALLOC(lsg, struct line_seg);
	    for (k=1; k<seg_argc; k += 2) {
		elem = seg_argv[k];
		sval = seg_argv[k+1];
		switch (*elem) {
		    case 'S':
			(void)bu_opt_int(NULL, 1, &sval, (void *)&lsg->start);
			break;
		    case 'E':
			(void)bu_opt_int(NULL, 1, &sval, (void *)&lsg->end);
			break;
		}
	    }
	    lsg->magic = CURVE_LSEG_MAGIC;
	    ant->segments[j] = (void *)lsg;
	} else if (BU_STR_EQUAL(seg_argv[0], "txt")) {
	    struct txt_seg *tsg;

	    BU_ALLOC(tsg, struct txt_seg);
	    for (k=1; k<seg_argc; k+= 2) {
		elem = seg_argv[k];
		sval = seg_argv[k+1];
		switch (*elem) {
		    case 'R': /* ref point */
			(void)bu_opt_int(NULL, 1, &sval, (void *)&tsg->ref_pt);
			break;
		    case 'P': /* position relative */
			(void)bu_opt_int(NULL, 1, &sval, (void *)&tsg->rel_pos);
			break;
		    case 'L': /* label text */
			(void)bu_opt_vls(NULL, 1, &sval, (void *)&tsg->label);
			break;
		}
	    }
	    tsg->magic = ANN_TSEG_MAGIC;
	    ant->segments[j] = (void *)tsg;
	} else if (BU_STR_EQUAL(seg_argv[0], "bezier")) {
	    struct bezier_seg *bsg;
	    int num_points;

	    BU_ALLOC(bsg, struct bezier_seg);
	    for (k=1; k<seg_argc; k+= 2) {
		elem = seg_argv[k];
		sval = seg_argv[k+1];
		switch (*elem) {
		    case 'D': /* degree */
			(void)bu_opt_int(NULL, 1, &sval, (void *)&bsg->degree);
			break;
		    case 'P': /* list of control points */
			num_points = 0;
			(void)_rt_tcl_list_to_int_array(sval, &bsg->ctl_points, &num_points);
			if (num_points != bsg->degree + 1) {
			    bu_vls_printf(logstr, "ERROR: degree and number of control points disagree for a Bezier segment\n");
			    bu_free((char *)seg_argv, "free seg argv");
			    bu_free((char *)seg_list, "free seg argv");
			    return 1;
			}
		}
	    }
	    bsg->magic = CURVE_BEZIER_MAGIC;
	    ant->segments[j] = (void *)bsg;
	} else if (BU_STR_EQUAL(seg_argv[0], "carc")) {
	    struct carc_seg *csg;

	    BU_ALLOC(csg, struct carc_seg);
	    for (k=1; k<seg_argc; k += 2) {
		elem = seg_argv[k];
		sval = seg_argv[k+1];
		switch (*elem) {
		    case 'S':
			(void)bu_opt_int(NULL, 1, &sval, (void *)&csg->start);
			break;
		    case 'E':
			(void)bu_opt_int(NULL, 1, &sval, (void *)&csg->end);
			break;
		    case 'R':
			(void)bu_opt_fastf_t(NULL, 1, &sval, (void *)&csg->radius);
			break;
		    case 'L' :
			(void)bu_opt_bool(NULL, 1, &sval, (void *)&csg->center_is_left);
			break;
		    case 'O':
			(void)bu_opt_bool(NULL, 1, &sval, (void *)&csg->orientation);
			break;
		}
	    }
	    csg->magic = CURVE_CARC_MAGIC;
	    ant->segments[j] = (void *)csg;
	} else if (BU_STR_EQUAL(seg_argv[0], "nurb")) {
	    struct nurb_seg *nsg;

	    BU_ALLOC(nsg, struct nurb_seg);
	    for (k=1; k<seg_argc; k += 2) {
		elem = seg_argv[k];
		sval = seg_argv[k+1];
		switch (*elem) {
		    case 'O':
			(void)bu_opt_int(NULL, 1, &sval, (void *)&nsg->order);
			break;
		    case 'T':
			(void)bu_opt_int(NULL, 1, &sval, (void *)&nsg->pt_type);
			break;
		    case 'K':
			(void)_rt_tcl_list_to_fastf_array(sval, &nsg->k.knots, &nsg->k.k_size);
			break;
		    case 'P' :
			(void)_rt_tcl_list_to_int_array(sval, &nsg->ctl_points, &nsg->c_size);
			break;
		    case 'W':
			(void)_rt_tcl_list_to_fastf_array(sval, &nsg->weights, &nsg->c_size);
			break;
		}
	    }
	    nsg->magic = CURVE_NURB_MAGIC;
	    ant->segments[j] = (void *)nsg;
	} else {
	    bu_vls_sprintf(logstr, "ERROR: Unrecognized segment type: %s\n", seg_argv[0]);
	    bu_free((char *)seg_argv, "free seg argv");
	    bu_free((char *)seg_list, "free seg argv");
	    return 1;
	}

	bu_free((char *)seg_argv, "free seg argv");
    }

    bu_free((char *)seg_list, "free seg argv");

    return 0;
}


int
rt_annot_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_annot_internal *annot_ip;
    int ret, array_len;
    fastf_t *newval;

    RT_CK_DB_INTERNAL(intern);
    annot_ip = (struct rt_annot_internal *)intern->idb_ptr;
    RT_ANNOT_CK_MAGIC(annot_ip);

    while (argc >= 2) {
	if (BU_STR_EQUAL(argv[0], "V")) {
	    newval = annot_ip->V;
	    array_len = 3;
	    if (_rt_tcl_list_to_fastf_array(argv[1], &newval, &array_len) !=
		array_len) {
		bu_vls_printf(logstr, "ERROR: Incorrect number of coordinates for vertex\n");
		return BRLCAD_ERROR;
	    }
	} else if (BU_STR_EQUAL(argv[0], "VL")) {
	    fastf_t *new_verts=(fastf_t *)NULL;
	    int len;
	    char *ptr;
	    char *dupstr;

	    /* the vertex list is a list of lists (each element is a
	     * list of two coordinates) so eliminate all the '{' and
	     * '}' chars in the list
	     */
	    dupstr = bu_strdup(argv[1]);

	    ptr = dupstr;
	    while (*ptr != '\0') {
		if (*ptr == '{' || *ptr == '}')
		    *ptr = ' ';
		ptr++;
	    }

	    len = 0;
	    (void)_rt_tcl_list_to_fastf_array(dupstr, &new_verts, &len);
	    bu_free(dupstr, "annotation adjust strdup");

	    if (len%2) {
		bu_vls_printf(logstr, "ERROR: Incorrect number of coordinates for vertices\n");
		return BRLCAD_ERROR;
	    }

	    if (annot_ip->verts)
		bu_free((char *)annot_ip->verts, "verts");
	    annot_ip->verts = (point2d_t *)new_verts;
	    annot_ip->vert_count = len / 2;
	} else if (BU_STR_EQUAL(argv[0], "SL")) {
	    /* the entire segment list */
	    struct rt_ant *ant;

	    ant = &annot_ip->ant;
	    ant->count = 0;
	    ant->reverse = (int *)NULL;
	    ant->segments = (void **)NULL;

	    if ((ret=ant_get_tcl(logstr, ant, argv[1])) != 0)
		return ret;
	} else if (*argv[0] == 'V' && isdigit((int)*(argv[0]+1))) {
	    /* changing a specific vertex */
	    long vert_no;
	    fastf_t *new_vert;

	    vert_no = atol(argv[0] + 1);
	    new_vert = annot_ip->verts[vert_no];
	    if (vert_no < 0 || (size_t)vert_no > annot_ip->vert_count) {
		bu_vls_printf(logstr, "ERROR: Illegal vertex number\n");
		return BRLCAD_ERROR;
	    }
	    array_len = 2;
	    if (_rt_tcl_list_to_fastf_array(argv[1], &new_vert, &array_len) != array_len) {
		bu_vls_printf(logstr, "ERROR: Incorrect number of coordinates for vertex\n");
		return BRLCAD_ERROR;
	    }
	}

	argc -= 2;
	argv += 2;
    }

    return BRLCAD_OK;
}


int
rt_annot_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
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
