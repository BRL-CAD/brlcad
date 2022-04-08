/*                           N M G . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2022 United States Government as represented by
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
/** @file primitives/nmg/nmg.c
 *
 * Intersect a ray with an NMG solid.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bnetwork.h"

#include "vmath.h"
#include "bu/cv.h"
#include "bg/polygon.h"
#include "nmg.h"
#include "rt/db4.h"
#include "raytrace.h"
#include "../../librt_private.h"

/* rt_nmg_internal is just "model", from nmg.h */

#define NMG_SPEC_START_MAGIC 6014061
#define NMG_SPEC_END_MAGIC 7013061

#define ERR_MSG "INTERNAL ERROR:  Probably bad geometry.  Trying to continue."

#define CK_SEGP(_p) if (!(_p) || !(*(_p))) {\
	bu_log("%s[line:%d]: Bad seg_p pointer\n", __FILE__, __LINE__); \
	segs_error(ERR_MSG); }

static jmp_buf nmg_longjump_env;

/* EDGE-FACE correlation data
 * used in edge_hit() for 3manifold case
 */
struct ef_data {
    fastf_t fdotr;	/* face vector VDOT with ray */
    fastf_t fdotl;	/* face vector VDOT with ray-left */
    fastf_t ndotr;	/* face normal VDOT with ray */
    struct edgeuse *eu;
};

static void
segs_error(const char *str) {
    bu_log("%s\n", str);
    longjmp(nmg_longjump_env, -1);
}



/* This is the solid information specific to an nmg solid */
struct nmg_specific {
    uint32_t nmg_smagic;	/* STRUCT START magic number */
    struct model *nmg_model;
    char *manifolds;		/* structure 1-3manifold table */
    vect_t nmg_invdir;
    uint32_t nmg_emagic;	/* STRUCT END magic number */
};


struct tmp_v {
    point_t pt;
    struct vertex *v;
};


/**
 * Calculate the bounding box for an N-Manifold Geometry
 */
int
rt_nmg_bbox(struct rt_db_internal *ip, point_t *min, point_t * max, const struct bn_tol *UNUSED(tol)) {
    struct model *m;

    RT_CK_DB_INTERNAL(ip);
    m = (struct model *)ip->idb_ptr;
    NMG_CK_MODEL(m);

    nmg_model_bb(*min, *max, m);
    return 0;
}


/**
 * Given a pointer to a ged database record, and a transformation
 * matrix, determine if this is a valid nmg, and if so, precompute
 * various terms of the formula.
 *
 * returns 0 if nmg is ok and !0 on error in description
 *
 * implicit return - a struct nmg_specific is created, and its
 * address is stored in stp->st_specific for use by nmg_shot().
 */
int
rt_nmg_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct model *m;
    struct nmg_specific *nmg_s;
    vect_t work;

    RT_CK_DB_INTERNAL(ip);
    m = (struct model *)ip->idb_ptr;
    NMG_CK_MODEL(m);

    if (stp->st_meth->ft_bbox(ip, &(stp->st_min), &(stp->st_max), &(rtip->rti_tol))) return 1;

    VADD2SCALE(stp->st_center, stp->st_min, stp->st_max, 0.5);
    VSUB2SCALE(work, stp->st_max, stp->st_min, 0.5);
    stp->st_aradius = stp->st_bradius = MAGNITUDE(work);

    BU_GET(nmg_s, struct nmg_specific);
    stp->st_specific = (void *)nmg_s;
    nmg_s->nmg_model = m;
    ip->idb_ptr = (void *)NULL;
    nmg_s->nmg_smagic = NMG_SPEC_START_MAGIC;
    nmg_s->nmg_emagic = NMG_SPEC_END_MAGIC;

    /* build table indicating the manifold level of each sub-element
     * of NMG solid
     */
    nmg_s->manifolds = nmg_manifolds(m);

    return 0;
}


void
rt_nmg_print(const struct soltab *stp)
{
    struct model *m =
	(struct model *)stp->st_specific;

    NMG_CK_MODEL(m);
    nmg_pr_m(m);
}

static void
visitor(uint32_t *l_p, void *tbl, int UNUSED(unused))
{
    (void)bu_ptbl_ins_unique((struct bu_ptbl *)tbl, (long *)l_p);
}

/**
 * Add an element provided by nmg_visit to a bu_ptbl struct.
 */
static void
build_topo_list(uint32_t *l_p, struct bu_ptbl *tbl, struct bu_list *vlfree)
{
    struct loopuse *lu;
    struct edgeuse *eu;
    struct edgeuse *eu_p;
    struct vertexuse *vu;
    struct vertexuse *vu_p;
    int radial_not_mate=0;
    static const struct nmg_visit_handlers htab = {NULL, NULL, NULL, NULL, NULL,
						   NULL, NULL, NULL, NULL, NULL,
						   visitor, NULL, NULL, NULL, NULL,
						   NULL, NULL, NULL, visitor, NULL,
						   NULL, NULL, NULL, visitor, NULL};
    /* htab.vis_face = htab.vis_edge = htab.vis_vertex = visitor; */

    if (!l_p) {
	bu_log("%s:%d NULL l_p\n", __FILE__, __LINE__);
	segs_error(ERR_MSG);
    }

    switch (*l_p) {
	case NMG_FACEUSE_MAGIC:
	    nmg_visit(l_p, &htab, (void *)tbl, vlfree);
	    break;
	case NMG_EDGEUSE_MAGIC:
	    eu = eu_p = (struct edgeuse *)l_p;
	    do {
		/* if the parent of this edgeuse is a face loopuse
		 * add the face to the list of shared topology
		 */
		if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC)
		    bu_ptbl_ins_unique(tbl,
				       (long *)eu->up.lu_p->up.fu_p->f_p);

		if (radial_not_mate) eu = eu->radial_p;
		else eu = eu->eumate_p;
		radial_not_mate = ! radial_not_mate;
	    } while (eu != eu_p);

	    bu_ptbl_ins_unique(tbl, (long *)eu->e_p);
	    bu_ptbl_ins_unique(tbl, (long *)eu->vu_p->v_p);
	    bu_ptbl_ins_unique(tbl, (long *)eu->eumate_p->vu_p->v_p);

	    break;
	case NMG_VERTEXUSE_MAGIC:
	    vu_p = (struct vertexuse *)l_p;
	    bu_ptbl_ins_unique(tbl, (long *)vu_p->v_p);

	    for (BU_LIST_FOR(vu, vertexuse, &vu_p->v_p->vu_hd)) {
		lu = (struct loopuse *)NULL;
		switch (*vu->up.magic_p) {
		    case NMG_EDGEUSE_MAGIC:
			eu = vu->up.eu_p;
			bu_ptbl_ins_unique(tbl, (long *)eu->e_p);
			if (*eu->up.magic_p !=  NMG_LOOPUSE_MAGIC)
			    break;

			lu = eu->up.lu_p;
			/* fallthrough */

		    case NMG_LOOPUSE_MAGIC:
			if (! lu) lu = vu->up.lu_p;

			if (*lu->up.magic_p == NMG_FACEUSE_MAGIC)
			    bu_ptbl_ins_unique(tbl,
					       (long *)lu->up.fu_p->f_p);
			break;
		    case NMG_SHELL_MAGIC:
			break;
		    default:
			bu_log("Bogus vertexuse parent magic:%s.",
			       bu_identify_magic(*vu->up.magic_p));
			segs_error(ERR_MSG);
		}
	    }
	    break;
	default:
	    bu_log("Bogus magic number pointer:%s", bu_identify_magic(*l_p));
	    segs_error(ERR_MSG);
    }
}

/**
 * If a_tbl and next_tbl have an element in common, return it.
 * Otherwise return a NULL pointer.
 */
static long *
common_topo(struct bu_ptbl *a_tbl, struct bu_ptbl *next_tbl)
{
    long **p;

    for (p = &a_tbl->buffer[a_tbl->end]; p >= a_tbl->buffer; p--) {
	if (bu_ptbl_locate(next_tbl, *p) >= 0)
	    return *p;
    }

    return (long *)NULL;
}

static void
pl_ray(struct ray_data *rd)
{
    FILE *fp;
    char name[80];
    static int plot_file_number=0;
    struct hitmiss *a_hit;
    int old_state = NMG_RAY_STATE_OUTSIDE;
    int in_state;
    int out_state;
    point_t old_point;
    point_t end_point;
    int old_cond = 0;

    sprintf(name, "nmg_ray%02d.plot3", plot_file_number++);
    fp=fopen(name, "wb");
    if (fp == (FILE *)NULL) {
	perror(name);
	bu_bomb("unable to open file for writing");
    } else {
	bu_log("overlay %s\n", name);
    }

    VMOVE(old_point, rd->rp->r_pt);

    for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)) {
	NMG_CK_HITMISS(a_hit);

	in_state = HMG_INBOUND_STATE(a_hit);
	out_state = HMG_OUTBOUND_STATE(a_hit);

	if (in_state == old_state) {
	    switch (in_state) {
		case NMG_RAY_STATE_INSIDE:
		    pl_color(fp, 55, 255, 55);
		    pdv_3line(fp, old_point, a_hit->hit.hit_point);
		    break;
		case NMG_RAY_STATE_ON:
		    pl_color(fp, 155, 155, 255);
		    pdv_3line(fp, old_point, a_hit->hit.hit_point);
		    break;
		case NMG_RAY_STATE_OUTSIDE:
		    pl_color(fp, 255, 255, 255);
		    pdv_3line(fp, old_point, a_hit->hit.hit_point);
		    break;
	    }
	    old_cond = 0;
	} else {
	    if (old_cond) {
		pl_color(fp, 255, 155, 255);
		old_cond = 0;
	    } else {
		pl_color(fp, 255, 55, 255);
		old_cond = 1;
	    }
	    pdv_3line(fp, old_point, a_hit->hit.hit_point);
	}
	VMOVE(old_point, a_hit->hit.hit_point);
	old_state = out_state;
    }

    if (old_state == NMG_RAY_STATE_OUTSIDE)
	pl_color(fp, 255, 255, 255);
    else
	pl_color(fp, 255, 55, 255);

    VADD2(end_point, old_point, rd->rp->r_dir);
    pdv_3line(fp, old_point, end_point);

    fclose(fp);
}





static void
unresolved(struct hitmiss *next_hit, struct bu_ptbl *a_tbl, struct bu_ptbl *next_tbl, struct bu_list *hd, struct ray_data *rd)
{

    struct hitmiss *hm;
    register long **l_p;
    register long **b;

    bu_log("Unable to fix state transition--->\n");
    bu_log("\tray start = (%f %f %f) dir = (%f %f %f)\n",
	   V3ARGS(rd->rp->r_pt), V3ARGS(rd->rp->r_dir));
    for (BU_LIST_FOR(hm, hitmiss, hd)) {
	if (hm == next_hit) {
	    bu_log("======= ======\n");
	    rt_nmg_print_hitmiss(hm);
	    bu_log("================\n");
	} else
	    rt_nmg_print_hitmiss(hm);
    }

    bu_log("topo table A\n");
    b = &a_tbl->buffer[a_tbl->end];
    l_p = &a_tbl->buffer[0];
    for (; l_p < b; l_p ++)
	bu_log("\t%ld %s\n", **l_p, bu_identify_magic(**l_p));

    bu_log("topo table NEXT\n");
    b = &next_tbl->buffer[next_tbl->end];
    l_p = &next_tbl->buffer[0];
    for (; l_p < b; l_p ++)
	bu_log("\t%ld %s\n", **l_p, bu_identify_magic(**l_p));

    bu_log("<---Unable to fix state transition\n");
    pl_ray(rd);
    bu_log("Primitive: %s, pixel=%d %d,  lvl=%d %s\n",
	   rd->stp->st_dp->d_namep,
	   rd->ap->a_x, rd->ap->a_y, rd->ap->a_level,
	   rd->ap->a_purpose);
}


static int
check_hitstate(struct bu_list *hd, struct ray_data *rd, struct bu_list *vlfree)
{
    struct hitmiss *a_hit;
    struct hitmiss *next_hit;
    int ibs;
    int obs;
    struct bu_ptbl *a_tbl = (struct bu_ptbl *)NULL;
    struct bu_ptbl *next_tbl = (struct bu_ptbl *)NULL;
    struct bu_ptbl *tbl_p = (struct bu_ptbl *)NULL;
    long *long_ptr;

    BU_CK_LIST_HEAD(hd);

    /* find that first "OUTSIDE" point */
    a_hit = BU_LIST_FIRST(hitmiss, hd);
    NMG_CK_HITMISS(a_hit);

    if (((a_hit->in_out & 0x0f0) >> 4) != NMG_RAY_STATE_OUTSIDE ||
	nmg_debug & NMG_DEBUG_RT_SEGS) {
	bu_log("check_hitstate()\n");
	rt_nmg_print_hitlist(hd);

	bu_log("Ray: pt:(%g %g %g) dir:(%g %g %g)\n",
	       V3ARGS(rd->rp->r_pt), V3ARGS(rd->rp->r_dir));
    }

    while (BU_LIST_NOT_HEAD(a_hit, hd) &&
	   ((a_hit->in_out & 0x0f0) >> 4) != NMG_RAY_STATE_OUTSIDE) {

	NMG_CK_HITMISS(a_hit);

	/* this better be a 2-manifold face */
	bu_log("%s[%d]: This better be a 2-manifold face\n", __FILE__, __LINE__);
	bu_log("Primitive: %s, pixel=%d %d,  lvl=%d %s\n",
	       rd->stp->st_dp->d_namep,
	       rd->ap->a_x, rd->ap->a_y, rd->ap->a_level,
	       rd->ap->a_purpose);
	a_hit = BU_LIST_PNEXT(hitmiss, a_hit);
    }
    if (BU_LIST_IS_HEAD(a_hit, hd)) return 1;

    BU_ALLOC(a_tbl, struct bu_ptbl);
    bu_ptbl_init(a_tbl, 64, "a_tbl");

    BU_ALLOC(next_tbl, struct bu_ptbl);
    bu_ptbl_init(next_tbl, 64, "next_tbl");

    /* check the state transition on the rest of the hit points */
    while (BU_LIST_NOT_HEAD((next_hit = BU_LIST_PNEXT(hitmiss, &a_hit->l)), hd)) {
	NMG_CK_HITMISS(next_hit);

	ibs = HMG_INBOUND_STATE(next_hit);
	obs = HMG_OUTBOUND_STATE(a_hit);
	if (ibs != obs) {
	    /* if these two hits share some common topological
	     * element, then we can fix things.
	     *
	     * First we build the table of elements associated
	     * with each hit.
	     */

	    bu_ptbl_reset(a_tbl);
	    NMG_CK_HITMISS(a_hit);
	    build_topo_list((uint32_t *)a_hit->outbound_use, a_tbl, vlfree);

	    bu_ptbl_reset(next_tbl);
	    NMG_CK_HITMISS(next_hit);
	    build_topo_list((uint32_t *)next_hit->outbound_use, next_tbl, vlfree);


	    /* If the tables have elements in common,
	     * then resolve the conflict by
	     * morphing the two hits to match.
	     * else
	     * This is a real conflict.
	     */
	    long_ptr = common_topo(a_tbl, next_tbl);
	    if (long_ptr) {
		/* morf the two hit points */
		a_hit->in_out = (a_hit->in_out & 0x0f0) +
		    NMG_RAY_STATE_ON;
		a_hit->outbound_use = long_ptr;

		next_hit->in_out = (next_hit->in_out & 0x0f) +
		    (NMG_RAY_STATE_ON << 4);
		a_hit->inbound_use = long_ptr;

	    } else
		unresolved(next_hit, a_tbl, next_tbl, hd, rd);

	}

	/* save next_tbl as a_tbl for next iteration */
	tbl_p = a_tbl;
	a_tbl = next_tbl;
	next_tbl = tbl_p;

	a_hit = next_hit;
    }

    bu_ptbl_free(next_tbl);
    bu_ptbl_free(a_tbl);
    (void)bu_free((char *)a_tbl, "a_tbl");
    (void)bu_free((char *)next_tbl, "next_tbl");

    return 0;
}

/* nmg_rt_segs.c contents */



static void
print_seg_list(struct seg *seghead, int seg_count, char *s)
{
    struct seg *seg_p;

    bu_log("Segment List (%d segments) (%s):\n", seg_count, s);
    /* print debugging data before returning */
    bu_log("Seghead:\n%p magic: %08x forw:%p back:%p\n\n",
	   (void *)seghead,
	   seghead->l.magic,
	   (void *)seghead->l.forw,
	   (void *)seghead->l.back);

    for (BU_LIST_FOR(seg_p, seg, &seghead->l)) {
	bu_log("%p magic: %08x forw:%p back:%p\n",
	       (void *)seg_p,
	       seg_p->l.magic,
	       (void *)seg_p->l.forw,
	       (void *)seg_p->l.back);
	bu_log("dist %g  pt(%g, %g, %g) N(%g, %g, %g)  =>\n",
	       seg_p->seg_in.hit_dist,
	       seg_p->seg_in.hit_point[0],
	       seg_p->seg_in.hit_point[1],
	       seg_p->seg_in.hit_point[2],
	       seg_p->seg_in.hit_normal[0],
	       seg_p->seg_in.hit_normal[1],
	       seg_p->seg_in.hit_normal[2]);
	bu_log("dist %g  pt(%g, %g, %g) N(%g, %g, %g)\n",
	       seg_p->seg_out.hit_dist,
	       seg_p->seg_out.hit_point[0],
	       seg_p->seg_out.hit_point[1],
	       seg_p->seg_out.hit_point[2],
	       seg_p->seg_out.hit_normal[0],
	       seg_p->seg_out.hit_normal[1],
	       seg_p->seg_out.hit_normal[2]);
    }
}


/*
 *			Current_State
 *	Input	|  0   1     2    3    4    5    6
 *	-------------------------------------------
 *	O_N  =	| i1   1   Ci1    5    1    5    5
 *	O_N !=	| i1  1|E  Ci1   C1  Ci1  Ci1  Ci1
 *	N_N  =	|  E   1     E    3    E    1    1
 *	N_N !=	|  E   1     E    E    E    1    1
 *	N_O  =	|  E  o2    o2  ?o2    E   o2   o2
 *	N_O !=	|  E  o2    o2    E    E   o2   o2
 *	O_O  =	|io3  o2    o2    3    3    6    6
 *	O_O !=	|io3 o2|E Cio3 Cio3 Cio3 Cio3 Cio3
 *	A_A  =	|io4   1     2    3    4    5    6
 *	A_A !=	|io4   1  Cio4 Cio4 Cio4 Cio4 Cio4
 *	EOI     |  N   E    CN   CN   CN   CN   CN
 *
 *	=	-> ray dist to point within tol (along ray) of last hit point
 *	!=	-> ray dist to point outside tol (along ray) of last hit point
 *
 *	State Prefix
 *	C	segment now completed, add to list & alloc new segment
 *	i	set inpoint for current segment
 *	o	set outpoint for current segment
 *
 *	State
 *	E	Error termination
 *	N	Normal termination
 *	1	"Entering" solid
 *	2	"Leaving" solid
 *	3	O_O from outside state
 *	4	A_A from outside state
 *	5	O_O=O_N		Fuzzy state 1 or state 2 from state 3
 *	6	0_0=O_N=O_O
 */


static void
set_inpoint(struct seg **seg_p, struct hitmiss *a_hit, struct soltab *stp, struct application *ap)
/* The segment we're building */
/* The input hit point */


{
    if (!seg_p) {
	bu_log("%s[line:%d]: Null pointer to segment pointer\n", __FILE__, __LINE__);
	bu_log(ERR_MSG);
	return;
    }

    /* if we don't have a seg struct yet, get one */
    if (*seg_p == (struct seg *)NULL) {
	RT_GET_SEG(*seg_p, ap->a_resource);
	(*seg_p)->seg_stp = stp;
    }

    /* copy the "in" hit */
    memcpy(&(*seg_p)->seg_in, &a_hit->hit, sizeof(struct hit));

    /* copy the normal */
    VMOVE((*seg_p)->seg_in.hit_normal, a_hit->inbound_norm);

    if (nmg_debug & NMG_DEBUG_RT_SEGS) {
	bu_log("Set seg_in:\n\tdist %g  pt(%g, %g, %g) N(%g, %g, %g)\n",
	       (*seg_p)->seg_in.hit_dist,
	       (*seg_p)->seg_in.hit_point[0],
	       (*seg_p)->seg_in.hit_point[1],
	       (*seg_p)->seg_in.hit_point[2],
	       (*seg_p)->seg_in.hit_normal[0],
	       (*seg_p)->seg_in.hit_normal[1],
	       (*seg_p)->seg_in.hit_normal[2]);
    }
}


static void
set_outpoint(struct seg **seg_p, struct hitmiss *a_hit)
/* The segment we're building */
/* The input hit point */
{
    if (!seg_p) {
	bu_log("%s[line:%d]: Null pointer to segment pointer\n", __FILE__, __LINE__);
	bu_log(ERR_MSG);
	return;
    }

    /* if we don't have a seg struct yet, get one */
    if (*seg_p == (struct seg *)NULL)
	segs_error("bad seg pointer\n");

    /* copy the "out" hit */
    memcpy(&(*seg_p)->seg_out, &a_hit->hit, sizeof(struct hit));

    /* copy the normal */
    VMOVE((*seg_p)->seg_out.hit_normal, a_hit->outbound_norm);

    if (nmg_debug & NMG_DEBUG_RT_SEGS) {
	bu_log("Set seg_out:\n\tdist %g  pt(%g, %g, %g) N(%g, %g, %g)  =>\n",
	       (*seg_p)->seg_in.hit_dist,
	       (*seg_p)->seg_in.hit_point[0],
	       (*seg_p)->seg_in.hit_point[1],
	       (*seg_p)->seg_in.hit_point[2],
	       (*seg_p)->seg_in.hit_normal[0],
	       (*seg_p)->seg_in.hit_normal[1],
	       (*seg_p)->seg_in.hit_normal[2]);
	bu_log("\tdist %g  pt(%g, %g, %g) N(%g, %g, %g)\n",
	       (*seg_p)->seg_out.hit_dist,
	       (*seg_p)->seg_out.hit_point[0],
	       (*seg_p)->seg_out.hit_point[1],
	       (*seg_p)->seg_out.hit_point[2],
	       (*seg_p)->seg_out.hit_normal[0],
	       (*seg_p)->seg_out.hit_normal[1],
	       (*seg_p)->seg_out.hit_normal[2]);
    }
}


static int
state0(struct seg *UNUSED(seghead), struct seg **seg_p, int *UNUSED(seg_count), struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *UNUSED(tol))
/* intersection w/ ray */
/* The segment we're building */
/* The number of valid segments built */
/* The input hit point */


{
    int ret_val = -1;

    NMG_CK_HITMISS(a_hit);

    switch (a_hit->in_out) {
	case HMG_HIT_OUT_IN:
	case HMG_HIT_OUT_ON:
	    /* save the in-hit point */
	    set_inpoint(seg_p, a_hit, stp, ap);
	    ret_val = 1;
	    break;
	case HMG_HIT_ON_ON:
	case HMG_HIT_IN_IN:
	case HMG_HIT_ON_IN:
	case HMG_HIT_IN_ON:
	case HMG_HIT_IN_OUT:
	case HMG_HIT_ON_OUT:
	    /* error */
	    bu_log("%s[line:%d]: State transition error: exit without entry.\n", __FILE__, __LINE__);
	    ret_val = -2;
	    break;
	case HMG_HIT_OUT_OUT:
	    /* Save the in/out points */
	    set_inpoint(seg_p, a_hit, stp, ap);
	    set_outpoint(seg_p, a_hit);
	    ret_val = 3;
	    break;
	case HMG_HIT_ANY_ANY:
	    /* Save the in/out points */
	    set_inpoint(seg_p, a_hit, stp, ap);
	    set_outpoint(seg_p, a_hit);
	    ret_val = 4;
	    break;
	default:
	    bu_log("%s[line:%d]: bogus hit in/out status\n", __FILE__, __LINE__);
	    segs_error(ERR_MSG);
	    break;
    }

    return ret_val;
}


static int
state1(struct seg *UNUSED(seghead), struct seg **seg_p, int *UNUSED(seg_count), struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *UNUSED(tol))
/* intersection w/ ray */
/* The segment we're building */
/* The number of valid segments built */
/* The input hit point */


{
    int ret_val = -1;

    if (stp) RT_CK_SOLTAB(stp);
    if (ap) RT_CK_APPLICATION(ap);
    NMG_CK_HITMISS(a_hit);

    switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
	case HMG_HIT_IN_IN:
	case HMG_HIT_ON_IN:
	case HMG_HIT_IN_ON:
	case HMG_HIT_ON_ON:
	    ret_val = 1;
	    break;
	case HMG_HIT_ON_OUT:
	case HMG_HIT_IN_OUT:
	    set_outpoint(seg_p, a_hit);
	    ret_val = 2;
	    break;
	case HMG_HIT_OUT_OUT:
	    /* XXX possibly an error condition if not within tol */
	    set_outpoint(seg_p, a_hit);
	    ret_val = 2;
	    break;
	case HMG_HIT_ANY_ANY:
	    ret_val = 1;
	    break;
	default:
	    bu_log("%s[line:%d]: bogus hit in/out status\n", __FILE__, __LINE__);
	    segs_error(ERR_MSG);
	    break;
    }

    return ret_val;
}


static int
state2(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol)
/* intersection w/ ray */
/* The segment we're building */
/* The number of valid segments built */
/* The input hit point */


{
    int ret_val = -1;
    double delta;

    NMG_CK_HITMISS(a_hit);

    switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
	    /* Segment completed.  Insert into segment list */
	    BU_LIST_MAGIC_SET(&((*seg_p)->l), RT_SEG_MAGIC);
	    BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l));
	    (*seg_count)++;

	    /* start new segment */
	    (*seg_p) = (struct seg *)NULL;
	    set_inpoint(seg_p, a_hit, stp, ap);

	    ret_val = 1;
	    break;
	case HMG_HIT_IN_IN:
	case HMG_HIT_ON_ON:
	case HMG_HIT_ON_IN:
	case HMG_HIT_IN_ON:
	    /* Error */
	    bu_log("%s[line:%d]: State transition error.\n", __FILE__, __LINE__);
	    ret_val = -2;
	    break;
	case HMG_HIT_ON_OUT:
	case HMG_HIT_IN_OUT:
	    /* progress the out-point */
	    set_outpoint(seg_p, a_hit);
	    ret_val = 2;
	    break;
	case HMG_HIT_OUT_OUT:
	    CK_SEGP(seg_p);
	    BN_CK_TOL(tol);
	    delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
	    if (delta < tol->dist) {
		set_outpoint(seg_p, a_hit);
		ret_val = 2;
		break;
	    }
	    /* complete the segment */
	    BU_LIST_MAGIC_SET(&((*seg_p)->l), RT_SEG_MAGIC);
	    BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l));
	    (*seg_count)++;

	    /* start new segment */
	    (*seg_p) = (struct seg *)NULL;
	    set_inpoint(seg_p, a_hit, stp, ap);
	    set_outpoint(seg_p, a_hit);

	    ret_val = 3;
	    break;
	case HMG_HIT_ANY_ANY:
	    CK_SEGP(seg_p);
	    BN_CK_TOL(tol);
	    delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
	    if (delta < tol->dist) {
		set_outpoint(seg_p, a_hit);
		ret_val = 2;
		break;
	    }
	    /* complete the segment */
	    BU_LIST_MAGIC_SET(&((*seg_p)->l), RT_SEG_MAGIC);
	    BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l));
	    (*seg_count)++;

	    /* start new segment */
	    (*seg_p) = (struct seg *)NULL;
	    set_inpoint(seg_p, a_hit, stp, ap);
	    set_outpoint(seg_p, a_hit);

	    ret_val = 4;
	    break;
	default:
	    bu_log("%s[line:%d]: bogus hit in/out status\n", __FILE__, __LINE__);
	    segs_error(ERR_MSG);
	    break;
    }

    return ret_val;
}


static int
state3(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol)
/* intersection w/ ray */
/* The segment we're building */
/* The number of valid segments built */
/* The input hit point */


{
    int ret_val = -1;
    double delta;

    NMG_CK_HITMISS(a_hit);
    CK_SEGP(seg_p);
    BN_CK_TOL(tol);
    delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);

    switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
	    if (delta < tol->dist) {
		ret_val = 5;
	    } else {
		/* complete the segment */
		BU_LIST_MAGIC_SET(&((*seg_p)->l), RT_SEG_MAGIC);
		BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l));
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);

		ret_val = 1;
	    }
	    break;
	case HMG_HIT_IN_IN:
	case HMG_HIT_ON_IN:
	case HMG_HIT_IN_ON:
	case HMG_HIT_ON_ON:
	    if (delta < tol->dist) {
		ret_val = 3;
	    } else {
		/* Error */
		bu_log("%s[line:%d]: State transition error.\n", __FILE__, __LINE__);
		ret_val = -2;
	    }
	    break;
	case HMG_HIT_ON_OUT:
	case HMG_HIT_IN_OUT:
	    /*
	     * This can happen when the ray hits an edge/vertex and
	     * (due to floating point fuzz) also appears to have a
	     * hit point in the area of a face:
	     *
	     * ------->o
	     *	  / \------------>
	     *	 /   \
	     *
	     */
	    set_outpoint(seg_p, a_hit);
	    ret_val = 2;
	    break;
	case HMG_HIT_OUT_OUT:
	    if (delta > tol->dist) {
		/* complete the segment */
		BU_LIST_MAGIC_SET(&((*seg_p)->l), RT_SEG_MAGIC);
		BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l));
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);
		set_outpoint(seg_p, a_hit);
	    }
	    ret_val = 3;
	    break;
	case HMG_HIT_ANY_ANY:
	    if (delta < tol->dist) {
		ret_val = 3;
	    } else {
		/* complete the segment */
		BU_LIST_MAGIC_SET(&((*seg_p)->l), RT_SEG_MAGIC);
		BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l));
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);
		ret_val = 4;
	    }
	    break;
	default:
	    bu_log("%s[line:%d]: bogus hit in/out status\n", __FILE__, __LINE__);
	    segs_error(ERR_MSG);
	    break;
    }

    return ret_val;
}


static int
state4(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol)
/* intersection w/ ray */
/* The segment we're building */
/* The number of valid segments built */
/* The input hit point */


{
    int ret_val = -1;
    double delta;

    NMG_CK_HITMISS(a_hit);

    switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
	    CK_SEGP(seg_p);
	    BN_CK_TOL(tol);
	    delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
	    if (delta > tol->dist) {
		/* complete the segment */
		BU_LIST_MAGIC_SET(&((*seg_p)->l), RT_SEG_MAGIC);
		BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l));
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);
	    }
	    ret_val = 1;
	    break;
	case HMG_HIT_IN_IN:
	case HMG_HIT_ON_IN:
	case HMG_HIT_IN_ON:
	case HMG_HIT_ON_ON:
	case HMG_HIT_ON_OUT:
	case HMG_HIT_IN_OUT:
	    /* Error */
	    bu_log("%s[line:%d]: State transition error.\n", __FILE__, __LINE__);
	    ret_val = -2;
	    break;
	case HMG_HIT_OUT_OUT:
	    CK_SEGP(seg_p);
	    BN_CK_TOL(tol);
	    delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
	    if (delta > tol->dist) {
		/* complete the segment */
		BU_LIST_MAGIC_SET(&((*seg_p)->l), RT_SEG_MAGIC);
		BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l));
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);
		set_outpoint(seg_p, a_hit);
	    }
	    ret_val = 3;
	    break;
	case HMG_HIT_ANY_ANY:
	    CK_SEGP(seg_p);
	    BN_CK_TOL(tol);
	    delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
	    if (delta > tol->dist) {
		/* complete the segment */
		BU_LIST_MAGIC_SET(&((*seg_p)->l), RT_SEG_MAGIC);
		BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l));
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);
		set_outpoint(seg_p, a_hit);
	    }
	    ret_val = 4;
	    break;
	default:
	    bu_log("%s[line:%d]: bogus hit in/out status\n", __FILE__, __LINE__);
	    segs_error(ERR_MSG);
	    break;
    }

    return ret_val;
}

static int
state5and6(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol, int ret_val_7)
{
    int ret_val = -1;
    double delta;

    NMG_CK_HITMISS(a_hit);

    switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
	    CK_SEGP(seg_p);
	    BN_CK_TOL(tol);
	    delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
	    if (delta < tol->dist) {
		ret_val = 5;
	    } else {
		/* complete the segment */
		BU_LIST_MAGIC_SET(&((*seg_p)->l), RT_SEG_MAGIC);
		BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l));
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);
		ret_val = 1;
	    }
	    break;
	case HMG_HIT_IN_IN:
	case HMG_HIT_ON_IN:
	case HMG_HIT_IN_ON:
	case HMG_HIT_ON_ON:
	    ret_val = 1;
	    break;
	case HMG_HIT_ON_OUT:
	case HMG_HIT_IN_OUT:
	    set_outpoint(seg_p, a_hit);
	    ret_val = 2;
	    break;
	case HMG_HIT_OUT_OUT:
	    CK_SEGP(seg_p);
	    BN_CK_TOL(tol);
	    delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
	    if (delta < tol->dist) {
		ret_val = 6;
	    } else {
		/* complete the segment */
		BU_LIST_MAGIC_SET(&((*seg_p)->l), RT_SEG_MAGIC);
		BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l));
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);
		set_outpoint(seg_p, a_hit);
		ret_val = 3;
	    }
	    break;
	case HMG_HIT_ANY_ANY:
	    CK_SEGP(seg_p);
	    BN_CK_TOL(tol);
	    delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
	    if (delta < tol->dist) {
		ret_val = ret_val_7;
	    } else {
		/* complete the segment */
		BU_LIST_MAGIC_SET(&((*seg_p)->l), RT_SEG_MAGIC);
		BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l));
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);
		set_outpoint(seg_p, a_hit);
		ret_val = 4;
	    }
	    break;
	default:
	    bu_log("%s[line:%d]: bogus hit in/out status\n", __FILE__, __LINE__);
	    segs_error(ERR_MSG);
	    break;
    }

    return ret_val;
}

static int
state5(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol)
/* intersection w/ ray */
/* The segment we're building */
/* The number of valid segments built */
/* The input hit point */

{
    return state5and6(seghead, seg_p, seg_count, a_hit, stp, ap, tol, 5);
}


static int
state6(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol)
/* intersection w/ ray */
/* The segment we're building */
/* The number of valid segments built */
/* The input hit point */

{
    return state5and6(seghead, seg_p, seg_count, a_hit, stp, ap, tol, 6);
}

static int (*state_table[7])(struct seg *, struct seg **, int *, struct hitmiss *, struct soltab *, struct application *, struct bn_tol *) = {
    state0,
    state1,
    state2,
    state3,
    state4,
    state5,
    state6
};


static int
nmg_bsegs(struct ray_data *rd, struct application *ap, struct seg *seghead, struct soltab *stp)


/* intersection w/ ray */

{
    int ray_state = 0;
    int new_state;
    struct hitmiss *a_hit = (struct hitmiss *)NULL;
    struct hitmiss *hm = (struct hitmiss *)NULL;
    struct seg *seg_p = (struct seg *)NULL;
    int seg_count = 0;

    for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)) {
	int (*state_table_func)(struct seg *, struct seg **, int *, struct hitmiss *, struct soltab *, struct application *, struct bn_tol *);

	NMG_CK_HITMISS(a_hit);

	/* cast function pointers for use */
	state_table_func = (int (*)(struct seg *, struct seg **, int *, struct hitmiss *, struct soltab *, struct application *, struct bn_tol *))state_table[ray_state];
	new_state = state_table_func(seghead, &seg_p, &seg_count, a_hit, stp, ap, (struct bn_tol *)rd->tol);
	if (new_state < 0) {
	    /* state transition error.  Print out the hit list
	     * and indicate where we were in processing it.
	     */
	    for (BU_LIST_FOR(hm, hitmiss, &rd->rd_hit)) {
		if (hm == a_hit) {
		    bu_log("======= State %d ======\n", ray_state);
		    rt_nmg_print_hitmiss(hm);
		    bu_log("================\n");
		} else
		    rt_nmg_print_hitmiss(hm);
	    }

	    /* Now bomb off */
	    bu_log("Primitive: %s, pixel=%d %d, lvl=%d %s\n",
		   rd->stp->st_dp->d_namep,
		   rd->ap->a_x, rd->ap->a_y, rd->ap->a_level,
		   rd->ap->a_purpose);
	    bu_log("Ray: pt:(%g %g %g) dir:(%g %g %g)\n",
		   V3ARGS(rd->rp->r_pt), V3ARGS(rd->rp->r_dir));
	    segs_error(ERR_MSG);
	    break;
	}

	ray_state = new_state;
    }

    /* Check to make sure the input ran out in the right place
     * in the state table.
     */
    if (ray_state == 1) {
	bu_log("%s[line:%d]: Input ended at non-terminal FSM state\n", __FILE__, __LINE__);

	bu_log("Ray: pt:(%g %g %g) dir:(%g %g %g)\n",
	       V3ARGS(rd->rp->r_pt), V3ARGS(rd->rp->r_dir));

	bu_log("Primitive: %s, pixel=%d %d, lvl=%d %s\n",
	       stp->st_dp->d_namep,
	       ap->a_x, ap->a_y, ap->a_level,
	       ap->a_purpose);
	segs_error(ERR_MSG);
    }

    /* Insert the last segment if appropriate */
    if (ray_state > 1) {
	/* complete the segment */
	BU_LIST_MAGIC_SET(&(seg_p->l), RT_SEG_MAGIC);
	BU_LIST_INSERT(&(seghead->l), &(seg_p->l));
	seg_count++;
    }

    return seg_count;
}



/**
 * Obtain the list of ray segments which intersect with the nmg.
 * This routine does all of the "work" for rt_nmg_shot()
 *
 * Return:
 * # of segments added to list.
 */
int
nmg_ray_segs(struct ray_data *rd, struct bu_list *vlfree)
{
    struct hitmiss *a_hit;
    static int last_miss=0;

    if (setjmp(nmg_longjump_env) != 0) {
	return 0;
    }

    NMG_CK_HITMISS_LISTS(rd);

    if (BU_LIST_IS_EMPTY(&rd->rd_hit)) {

	NMG_FREE_HITLIST(&rd->rd_miss);

	if (nmg_debug & NMG_DEBUG_RT_SEGS) {
	    if (last_miss) bu_log(".");
	    else bu_log("ray missed NMG\n");
	}
	last_miss = 1;
	return 0;			/* MISS */
    } else if (nmg_debug & NMG_DEBUG_RT_SEGS) {
	int seg_count=0;

	print_seg_list(rd->seghead, seg_count, "before");

	bu_log("\n\nnmg_ray_segs(rd)\nsorted nmg/ray hit list\n");

	for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_hit))
	    rt_nmg_print_hitmiss(a_hit);
    }

    last_miss = 0;

    if (check_hitstate(&rd->rd_hit, rd, vlfree)) {
	NMG_FREE_HITLIST(&rd->rd_hit);
	NMG_FREE_HITLIST(&rd->rd_miss);
	return 0;
    }

    if (nmg_debug & NMG_DEBUG_RT_SEGS) {
	bu_log("----------morphed nmg/ray hit list---------\n");
	for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_hit))
	    rt_nmg_print_hitmiss(a_hit);

	pl_ray(rd);
    }

    {
	int seg_count = nmg_bsegs(rd, rd->ap, rd->seghead, rd->stp);


	NMG_FREE_HITLIST(&rd->rd_hit);
	NMG_FREE_HITLIST(&rd->rd_miss);


	if (nmg_debug & NMG_DEBUG_RT_SEGS) {
	    /* print debugging data before returning */
	    print_seg_list(rd->seghead, seg_count, "after");
	}
	return seg_count;
    }
}


/**
 * Intersect a ray with a nmg.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_nmg_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)

/* info about the ray */

/* intersection w/ ray */
{
    struct ray_data rd;
    int status;
    struct nmg_specific *nmg =
	(struct nmg_specific *)stp->st_specific;

    if (nmg_debug & NMG_DEBUG_NMGRT) {
	bu_log("rt_nmg_shot()\n\t");
	rt_pr_tol(&ap->a_rt_i->rti_tol);
    }

    /* check validity of nmg specific structure */
    if (nmg->nmg_smagic != NMG_SPEC_START_MAGIC)
	bu_bomb("start of NMG st_specific structure corrupted\n");

    if (nmg->nmg_emagic != NMG_SPEC_END_MAGIC)
	bu_bomb("end of NMG st_specific structure corrupted\n");

    /* Compute the inverse of the direction cosines */
    if (!ZERO(rp->r_dir[X])) {
	nmg->nmg_invdir[X]=1.0/rp->r_dir[X];
    } else {
	nmg->nmg_invdir[X] = INFINITY;
	rp->r_dir[X] = 0.0;
    }
    if (!ZERO(rp->r_dir[Y])) {
	nmg->nmg_invdir[Y]=1.0/rp->r_dir[Y];
    } else {
	nmg->nmg_invdir[Y] = INFINITY;
	rp->r_dir[Y] = 0.0;
    }
    if (!ZERO(rp->r_dir[Z])) {
	nmg->nmg_invdir[Z]=1.0/rp->r_dir[Z];
    } else {
	nmg->nmg_invdir[Z] = INFINITY;
	rp->r_dir[Z] = 0.0;
    }

    /* build the NMG per-ray data structure */
    rd.rd_m = nmg->nmg_model;
    rd.manifolds = nmg->manifolds;
    VMOVE(rd.rd_invdir, nmg->nmg_invdir);
    rd.rp = rp;
    rd.tol = &ap->a_rt_i->rti_tol;
    rd.ap = ap;
    rd.stp = stp;
    rd.seghead = seghead;
    rd.classifying_ray = 0;

    /* create a table to keep track of which elements have been
     * processed before and which haven't.  Elements in this table
     * will either be (NULL) if item not previously processed or a
     * hitmiss ptr if item was previously processed
     */
    rd.hitmiss = (struct hitmiss **)bu_calloc(rd.rd_m->maxindex,
					      sizeof(struct hitmiss *), "nmg geom hit list");

    /* initialize the lists of things that have been hit/missed */
    BU_LIST_INIT(&rd.rd_hit);
    BU_LIST_INIT(&rd.rd_miss);
    rd.magic = NMG_RAY_DATA_MAGIC;

    /* intersect the ray with the geometry (sets surfno) */
    nmg_isect_ray_model((struct nmg_ray_data *)&rd,&RTG.rtg_vlfree);

    /* build the sebgent lists */
    status = nmg_ray_segs(&rd,&RTG.rtg_vlfree);

    /* free the hitmiss table */
    bu_free((char *)rd.hitmiss, "free nmg geom hit list");

    return status;
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_nmg_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    if (!hitp || !rp)
	return;

    if (stp) RT_CK_SOLTAB(stp);
    RT_CK_RAY(rp);
    RT_CK_HIT(hitp);

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
}


/**
 * Return the curvature of the nmg.
 */
void
rt_nmg_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (!cvp || !hitp)
	return;

    RT_CK_HIT(hitp);
    if (stp) RT_CK_SOLTAB(stp);

    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
}


/**
 * For a hit on the surface of an nmg, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 *
 * u = azimuth
 * v = elevation
 */
void
rt_nmg_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);
    if (hitp) RT_CK_HIT(hitp);
    if (!uvp) return;
}


void
rt_nmg_free(struct soltab *stp)
{
    struct nmg_specific *nmg =
	(struct nmg_specific *)stp->st_specific;

    nmg_km(nmg->nmg_model);
    BU_PUT(nmg, struct nmg_specific);
    stp->st_specific = NULL; /* sanity */
}


int
rt_nmg_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    struct model *m;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    m = (struct model *)ip->idb_ptr;
    NMG_CK_MODEL(m);

    nmg_m_to_vlist(vhead, m, 0, &RTG.rtg_vlfree);

    return 0;
}


/**
 * XXX This routine "destroys" the internal nmg solid.  This means
 * that once you tessellate an NMG solid, your in-memory copy becomes
 * invalid, and you can't do anything else with it until you get a new
 * copy from disk.
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_nmg_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *tol)
{
    struct model *lm;

    NMG_CK_MODEL(m);

    RT_CK_DB_INTERNAL(ip);
    lm = (struct model *)ip->idb_ptr;
    NMG_CK_MODEL(lm);

    if (BU_LIST_IS_EMPTY(&(lm->r_hd))) {
	/* No regions in imported geometry, can't give valid 'r' */
	*r = (struct nmgregion *)NULL;
	return -1;
    }

    /* XXX A big hack, just for testing ***/

    *r = BU_LIST_FIRST(nmgregion, &(lm->r_hd));
    NMG_CK_REGION(*r);
    if (BU_LIST_NEXT_NOT_HEAD(*r, &(lm->r_hd))) {
	struct nmgregion *r2;

	r2 = BU_LIST_PNEXT(nmgregion, &((*r)->l));
	while (BU_LIST_NOT_HEAD(&r2->l, &(lm->r_hd))) {
	    struct nmgregion *next_r;

	    next_r = BU_LIST_PNEXT(nmgregion, &r2->l);
	    nmg_merge_regions(*r, r2, tol);

	    r2 = next_r;
	}
    }


    /* XXX The next two lines "destroy" the internal nmg solid.  This
     * means that once you tessellate an NMG solid, your in-memory copy
     * becomes invalid, and you can't do anything else with it until
     * you get a new copy from disk.
     */
    nmg_merge_models(m, lm);
    ip->idb_ptr = ((void *)0);

    return 0;
}

/**
 * Import an NMG from the database format to the internal format.
 * Apply modeling transformations as well.
 *
 * Special subscripts are used in the disk file:
 * -1 indicates a pointer to the rt_list structure which
 * heads a linked list, and is not the first struct element.
 * 0 indicates that a null pointer should be used.
 */
static int
rt_nmg_import4_internal(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat)
{
    BU_CK_EXTERNAL(ep);
    union record *rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != DBID_NMG) {
	bu_log("rt_nmg_import4: defective record\n");
	return -1;
    }

    /*
     * Check for proper version.
     * In the future, this will be the backwards-compatibility hook.
     */
    if (rp->nmg.N_version != DISK_MODEL_VERSION) {
	bu_log("rt_nmg_import4:  expected NMG '.g' format version %d, got version %d, aborting.\n",
	       DISK_MODEL_VERSION,
	       rp->nmg.N_version);
	return -1;
    }

    struct model *m = nmg_import(ep, mat, 4);

    if (!m)
	return -1;

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_NMG;
    ip->idb_meth = &OBJ[ID_NMG];
    ip->idb_ptr = (void *)m;

    return 0;			/* OK */
}


/**
 * Import an NMG from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_nmg_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct model *m;
    union record *rp;

    BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);

    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != DBID_NMG) {
	bu_log("rt_nmg_import4: defective record\n");
	return -1;
    }

    if (rt_nmg_import4_internal(ip, ep, mat) < 0)
	return -1;

    m = (struct model *)ip->idb_ptr;
    NMG_CK_MODEL(m);

    if (RT_G_DEBUG || nmg_debug)
	nmg_vmodel(m);

    return 0;			/* OK */
}


int
rt_nmg_import5(struct rt_db_internal *ip,
	       struct bu_external *ep,
	       const mat_t mat,
	       const struct db_i *dbip)
{

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    RT_CK_DB_INTERNAL(ip);
    struct model *m = nmg_import(ep, mat, 5);

    if (RT_G_DEBUG || !nmg_debug) {
	nmg_vmodel(m);
    }

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_NMG;
    ip->idb_meth = &OBJ[ ID_NMG ];
    ip->idb_ptr = (void *)m;

    return 0;		/* OK */
}


/**
 * The name is added by the caller, in the usual place.
 */
int
rt_nmg_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct model *m;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_NMG) return -1;
    m = (struct model *)ip->idb_ptr;
    NMG_CK_MODEL(m);

    /* To ensure that a decent database is written, verify source first */
    nmg_vmodel(m);

    /* The "compact" flag is used to save space in the database */
    return nmg_export(ep, m, local2mm, sizeof(union record));
}


int
rt_nmg_export5(
    struct bu_external *ep,
    const struct rt_db_internal *ip,
    double local2mm,
    const struct db_i *dbip)
{
    struct model *m;
    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_NMG) return -1;
    m = (struct model *)ip->idb_ptr;

    BU_CK_EXTERNAL(ep);

    return nmg_export(ep, m, local2mm, 0);
}


/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_nmg_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double UNUSED(mm2local))
{
    struct model *m =
	(struct model *)ip->idb_ptr;

    NMG_CK_MODEL(m);
    bu_vls_printf(str, "n-Manifold Geometry solid (NMG) maxindex=%ld\n",
		  (long)m->maxindex);

    if (!verbose) return 0;

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_nmg_ifree(struct rt_db_internal *ip)
{
    struct model *m;

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_ptr) {
	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL(m);
	nmg_km(m);
    }

    ip->idb_ptr = ((void *)0);	/* sanity */
}


int
rt_nmg_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    struct model *m=(struct model *)intern->idb_ptr;
    struct bu_ptbl verts;
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct vertexuse *vu;
    struct vertex *v;
    struct vertex_g *vg;
    size_t i;

    NMG_CK_MODEL(m);

    if (attr == (char *)NULL) {
	bu_vls_strcpy(logstr, "nmg");
	bu_ptbl_init(&verts, 256, "nmg verts");
	nmg_vertex_tabulate(&verts, &m->magic, &RTG.rtg_vlfree);

	/* first list all the vertices */
	bu_vls_strcat(logstr, " V {");
	for (i=0; i<BU_PTBL_LEN(&verts); i++) {
	    v = (struct vertex *) BU_PTBL_GET(&verts, i);
	    NMG_CK_VERTEX(v);
	    vg = v->vg_p;
	    if (!vg) {
		bu_vls_printf(logstr, "Vertex has no geometry\n");
		bu_ptbl_free(&verts);
		return BRLCAD_ERROR;
	    }
	    bu_vls_printf(logstr, " { %.25g %.25g %.25g }", V3ARGS(vg->coord));
	}
	bu_vls_strcat(logstr, " }");

	/* use the backwards macros here so that "asc2g" will build the same structures */
	/* now all the nmgregions */
	for (BU_LIST_FOR_BACKWARDS(r, nmgregion, &m->r_hd)) {
	    /* bu_vls_strcat(logstr, " R {"); */

	    /* and all the shells */
	    for (BU_LIST_FOR_BACKWARDS(s, shell, &r->s_hd)) {
		/* bu_vls_strcat(logstr, " S {"); */

		/* all the faces */
		if (BU_LIST_NON_EMPTY(&s->fu_hd)) {
		    for (BU_LIST_FOR_BACKWARDS(fu, faceuse, &s->fu_hd)) {
			if (fu->orientation != OT_SAME)
			    continue;

			bu_vls_strcat(logstr, " F {");

			/* all the loops in this face */
			for (BU_LIST_FOR_BACKWARDS(lu, loopuse, &fu->lu_hd)) {

			    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
				vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
				bu_vls_printf(logstr, " %jd",
					      bu_ptbl_locate(&verts, (long *)vu->v_p));
			    } else {
				bu_vls_strcat(logstr, " {");
				for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
				    vu = eu->vu_p;
				    bu_vls_printf(logstr, " %jd",
						  bu_ptbl_locate(&verts, (long *)vu->v_p));
				}
				/* end of this loop */
				bu_vls_strcat(logstr, " }");
			    }
			}

			/* end of this face */
			bu_vls_strcat(logstr, " }");
		    }
		}
	    }
	    /* end of this nmgregion */
	    /* bu_vls_strcat(logstr, " }"); */
	}
	bu_ptbl_free(&verts);
    } else if (BU_STR_EQUAL(attr, "V")) {
	/* list of vertices */

	bu_ptbl_init(&verts, 256, "nmg verts");
	nmg_vertex_tabulate(&verts, &m->magic, &RTG.rtg_vlfree);
	for (i=0; i<BU_PTBL_LEN(&verts); i++) {
	    v = (struct vertex *) BU_PTBL_GET(&verts, i);
	    NMG_CK_VERTEX(v);
	    vg = v->vg_p;
	    if (!vg) {
		bu_vls_printf(logstr, "Vertex has no geometry\n");
		bu_ptbl_free(&verts);
		return BRLCAD_ERROR;
	    }
	    bu_vls_printf(logstr, " { %.25g %.25g %.25g }", V3ARGS(vg->coord));
	}
	bu_ptbl_free(&verts);
    } else {
	bu_vls_printf(logstr, "Unrecognized parameter\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


int
rt_nmg_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct model *m;
    struct nmgregion *r=NULL;
    struct shell *s=NULL;
    struct faceuse *fu=NULL;
    const char **obj_array;
    int len;
    int num_verts = 0;
    int num_loops = 0;
    int *loop;
    int loop_len;
    int i, j;
    struct tmp_v *verts;
    fastf_t *tmp;
    struct bn_tol tol;

    RT_CK_DB_INTERNAL(intern);
    m = (struct model *)intern->idb_ptr;
    NMG_CK_MODEL(m);

    verts = (struct tmp_v *)NULL;
    for (i=0; i<argc; i += 2) {
	if (BU_STR_EQUAL(argv[i], "V")) {
	    if (bu_argv_from_tcl_list(argv[i+1], &num_verts, (const char ***)&obj_array) != 0) {
		bu_vls_printf(logstr, "ERROR: failed to parse vertex list\n");
		return BRLCAD_ERROR;
	    }
	    if (num_verts == 0) {
		bu_vls_printf(logstr, "ERROR: no vertices found\n");
		return BRLCAD_ERROR;
	    }

	    verts = (struct tmp_v *)bu_calloc(num_verts,
					      sizeof(struct tmp_v),
					      "verts");
	    for (j = 0; j < num_verts; j++) {
		len = 3;
		tmp = &verts[j].pt[0];
		if (_rt_tcl_list_to_fastf_array(obj_array[j], &tmp, &len) != 3) {
		    bu_vls_printf(logstr, "ERROR: incorrect number of coordinates for vertex\n");
		    return BRLCAD_ERROR;
		}
	    }

	}
    }

    while (argc >= 2) {
	struct vertex ***face_verts;

	if (BU_STR_EQUAL(argv[0], "V")) {
	    /* vertex list handled above */
	    goto cont;
	} else if (BU_STR_EQUAL(argv[0], "F")) {
	    if (!verts) {
		bu_vls_printf(logstr,
			      "ERROR: cannot set faces without vertices\n");
		return BRLCAD_ERROR;
	    }
	    if (BU_LIST_IS_EMPTY(&m->r_hd)) {
		r = nmg_mrsv(m);
		s = BU_LIST_FIRST(shell, &r->s_hd);
	    } else {
		r = BU_LIST_FIRST(nmgregion, &m->r_hd);
		s = BU_LIST_FIRST(shell, &r->s_hd);
	    }
	    if (bu_argv_from_tcl_list(argv[1], &num_loops, (const char ***)&obj_array) != 0) {
		bu_vls_printf(logstr, "ERROR: failed to parse face list\n");
		return BRLCAD_ERROR;
	    }
	    if (num_loops == 0) {
		bu_vls_printf(logstr, "ERROR: no face loops found\n");
		return BRLCAD_ERROR;
	    }
	    for (i=0, fu=NULL; i<num_loops; i++) {
		struct vertex **loop_verts;
		/* struct faceuse fu is initialized in earlier scope */

		loop_len = 0;
		(void)_rt_tcl_list_to_int_array(obj_array[i], &loop, &loop_len);
		if (!loop_len) {
		    bu_vls_printf(logstr,
				  "ERROR: unable to parse face list\n");
		    return BRLCAD_ERROR;
		}
		if (i) {
		    loop_verts = (struct vertex **)bu_calloc(
			loop_len,
			sizeof(struct vertex *),
			"loop_verts");
		    for (i=0; i<loop_len; i++) {
			loop_verts[i] = verts[loop[i]].v;
		    }
		    fu = nmg_add_loop_to_face(s, fu,
					      loop_verts, loop_len,
					      OT_OPPOSITE);
		    for (i=0; i<loop_len; i++) {
			verts[loop[i]].v = loop_verts[i];
		    }
		} else {
		    face_verts = (struct vertex ***)bu_calloc(
			loop_len,
			sizeof(struct vertex **),
			"face_verts");
		    for (j=0; j<loop_len; j++) {
			face_verts[j] = &verts[loop[j]].v;
		    }
		    fu = nmg_cmface(s, face_verts, loop_len);
		    bu_free((char *)face_verts, "face_verts");
		}
	    }
	} else {
	    bu_vls_printf(logstr,
			  "ERROR: Unrecognized parameter, must be V or F\n");
	    return BRLCAD_ERROR;
	}
    cont:
	argc -= 2;
	argv += 2;
    }

    /* assign geometry for entire vertex list (if we have one) */
    for (i=0; i<num_verts; i++) {
	if (verts[i].v)
	    nmg_vertex_gv(verts[i].v, verts[i].pt);
    }

    /* assign face geometry */
    if (s) {
	for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	    if (fu->orientation != OT_SAME)
		continue;
	    nmg_calc_face_g(fu,&RTG.rtg_vlfree);
	}
    }

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    nmg_rebound(m, &tol);

    return BRLCAD_OK;
}


void
rt_nmg_make(const struct rt_functab *ftp, struct rt_db_internal *intern)
{
    struct model *m;

    m = nmg_mm();
    intern->idb_ptr = (void *)m;
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_NMG;
    intern->idb_meth = ftp;
}


int
rt_nmg_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


/* contains information used to analyze a polygonal face */
struct poly_face
{
    char label[5];
    size_t npts;
    point_t *pts;
    plane_t plane_eqn;
    fastf_t area;
    fastf_t vol_pyramid;
    point_t cent_pyramid;
    point_t cent;
};


static void
rt_nmg_faces_area(struct poly_face* faces, struct shell* s)
{
    struct bu_ptbl nmg_faces;
    size_t num_faces, i;
    size_t *npts;
    point_t **tmp_pts;
    plane_t *eqs;
    nmg_face_tabulate(&nmg_faces, &s->l.magic, &RTG.rtg_vlfree);
    num_faces = BU_PTBL_LEN(&nmg_faces);
    tmp_pts = (point_t **)bu_calloc(num_faces, sizeof(point_t *), "rt_nmg_faces_area: tmp_pts");
    npts = (size_t *)bu_calloc(num_faces, sizeof(size_t), "rt_nmg_faces_area: npts");
    eqs = (plane_t *)bu_calloc(num_faces, sizeof(plane_t), "rt_nmg_faces_area: eqs");

    for (i = 0; i < num_faces; i++) {
	struct face *f;
	f = (struct face *)BU_PTBL_GET(&nmg_faces, i);
	HMOVE(faces[i].plane_eqn, f->g.plane_p->N);
	VUNITIZE(faces[i].plane_eqn);
	tmp_pts[i] = faces[i].pts;
	HMOVE(eqs[i], faces[i].plane_eqn);
    }
    bg_3d_polygon_make_pnts_planes(npts, tmp_pts, num_faces, (const plane_t *)eqs);
    for (i = 0; i < num_faces; i++) {
	faces[i].npts = npts[i];
	bg_3d_polygon_sort_ccw(faces[i].npts, faces[i].pts, faces[i].plane_eqn);
	bg_3d_polygon_area(&faces[i].area, faces[i].npts, (const point_t *)faces[i].pts);
    }
    bu_free((char *)tmp_pts, "rt_nmg_faces_area: tmp_pts");
    bu_free((char *)npts, "rt_nmg_faces_area: npts");
    bu_free((char *)eqs, "rt_nmg_faces_area: eqs");
}


void
rt_nmg_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    struct model *m;
    struct nmgregion* r;

    /*Iterate through all regions and shells */
    m = (struct model *)ip->idb_ptr;
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	struct shell* s;

	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    struct bu_ptbl nmg_faces;
	    size_t num_faces, i;
	    struct poly_face *faces;

	    /*get faces of this shell*/
	    nmg_face_tabulate(&nmg_faces, &s->l.magic, &RTG.rtg_vlfree);
	    num_faces = BU_PTBL_LEN(&nmg_faces);
	    faces = (struct poly_face *)bu_calloc(num_faces, sizeof(struct poly_face), "rt_nmg_surf_area: faces");

	    for (i = 0; i < num_faces; i++) {
		/* allocate array of pt structs, max number of verts per faces = (# of faces) - 1 */
		faces[i].pts = (point_t *)bu_calloc(num_faces - 1, sizeof(point_t), "rt_nmg_surf_area: pts");
	    }
	    rt_nmg_faces_area(faces, s);
	    for (i = 0; i < num_faces; i++) {
		*area += faces[i].area;
	    }
	    for (i = 0; i < num_faces; i++) {
		bu_free((char *)faces[i].pts, "rt_nmg_surf_area: pts");
	    }
	    bu_free((char *)faces, "rt_nmg_surf_area: faces");
	}
    }
}


void
rt_nmg_centroid(point_t *cent, const struct rt_db_internal *ip)
{
    struct model *m = NULL;
    struct nmgregion* r = NULL;
    struct shell* s = NULL;
    struct poly_face *faces = NULL;
    struct bu_ptbl nmg_faces = BU_PTBL_INIT_ZERO;
    fastf_t volume = 0.0;
    point_t arbit_point = VINIT_ZERO;
    size_t num_faces = 0;
    size_t i = 0;

    *cent[0] = 0.0;
    *cent[1] = 0.0;
    *cent[2] = 0.0;
    m = (struct model *)ip->idb_ptr;
    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    s = BU_LIST_FIRST(shell, &r->s_hd);

    /*get faces*/
    nmg_face_tabulate(&nmg_faces, &s->l.magic, &RTG.rtg_vlfree);
    num_faces = BU_PTBL_LEN(&nmg_faces);

    /* If we have no faces, there's nothing to do */
    if (!num_faces)
	return;

    faces = (struct poly_face *)bu_calloc(num_faces, sizeof(struct poly_face), "rt_nmg_centroid: faces");

    for (i = 0; i < num_faces; i++) {
	/* allocate array of pt structs, max number of verts per faces = (# of faces) - 1 */
	faces[i].pts = (point_t *)bu_calloc(num_faces - 1, sizeof(point_t), "rt_nmg_centroid: pts");
    }
    rt_nmg_faces_area(faces, s);
    for (i = 0; i < num_faces; i++) {
	bg_3d_polygon_centroid(&faces[i].cent, faces[i].npts, (const point_t *) faces[i].pts);
	VADD2(arbit_point, arbit_point, faces[i].cent);
    }
    VSCALE(arbit_point, arbit_point, (1/num_faces));

    for (i = 0; i < num_faces; i++) {
	vect_t tmp = VINIT_ZERO;

	/* calculate volume */
	volume = 0.0;
	VSCALE(tmp, faces[i].plane_eqn, faces[i].area);
	faces[i].vol_pyramid = (VDOT(faces[i].pts[0], tmp)/3);
	volume += faces[i].vol_pyramid;
	/*Vector from arbit_point to centroid of face, results in h of pyramid */
	VSUB2(faces[i].cent_pyramid, faces[i].cent, arbit_point);
	/*centroid of pyramid is 1/4 up from the bottom */
	VSCALE(faces[i].cent_pyramid, faces[i].cent_pyramid, 0.75f);
	/* now cent_pyramid is back in the polyhedron */
	VADD2(faces[i].cent_pyramid, faces[i].cent_pyramid, arbit_point);
	/* weight centroid of pyramid by pyramid's volume */
	VSCALE(faces[i].cent_pyramid, faces[i].cent_pyramid, faces[i].vol_pyramid);
	/* add cent_pyramid to the centroid of the polyhedron */
	VADD2(*cent, *cent, faces[i].cent_pyramid);
    }
    /* reverse the weighting */
    VSCALE(*cent, *cent, (1/volume));
    for (i = 0; i < num_faces; i++) {
	bu_free((char *)faces[i].pts, "rt_nmg_centroid: pts");
    }
    bu_free((char *)faces, "rt_nmg_centroid: faces");
}


void
rt_nmg_volume(fastf_t *volume, const struct rt_db_internal *ip)
{
    struct model *m;
    struct nmgregion* r;

    /*Iterate through all regions and shells */
    m = (struct model *)ip->idb_ptr;
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	struct shell* s;

	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    struct bu_ptbl nmg_faces;
	    size_t num_faces, i;
	    struct poly_face *faces;

	    /*get faces of this shell*/
	    nmg_face_tabulate(&nmg_faces, &s->l.magic, &RTG.rtg_vlfree);
	    num_faces = BU_PTBL_LEN(&nmg_faces);
	    faces = (struct poly_face *)bu_calloc(num_faces, sizeof(struct poly_face), "rt_nmg_volume: faces");

	    for (i = 0; i < num_faces; i++) {
		/* allocate array of pt structs, max number of verts per faces = (# of faces) - 1 */
		faces[i].pts = (point_t *)bu_calloc(num_faces - 1, sizeof(point_t), "rt_nmg_volume: pts");
	    }
	    rt_nmg_faces_area(faces, s);
	    for (i = 0; i < num_faces; i++) {
		vect_t tmp = VINIT_ZERO;

		/* calculate volume of pyramid*/
		VSCALE(tmp, faces[i].plane_eqn, faces[i].area);
		*volume = (VDOT(faces[i].pts[0], tmp)/3);
	    }
	    for (i = 0; i < num_faces; i++) {
		bu_free((char *)faces[i].pts, "rt_nmg_volume: pts");
	    }
	    bu_free((char *)faces, "rt_nmg_volume: faces");
	}
    }
}

/**
 * Store an NMG model as a separate .g file, for later examination.
 * Don't free the model, as the caller may still have uses for it.
 *
 * NON-PARALLEL because of rt_uniresource.
 */
void
nmg_stash_model_to_file(const char *filename, const struct model *m, const char *title)
{
    struct rt_wdb *fp;
    struct rt_db_internal intern;
    struct bu_external ext;
    int flags;
    char *name="error.s";

    bu_log("nmg_stash_model_to_file('%s', %p, %s)\n", filename, (void *)m, title);

    NMG_CK_MODEL(m);
    nmg_vmodel(m);

    if ((fp = wdb_fopen(filename)) == NULL) {
	perror(filename);
	return;
    }

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_NMG;
    intern.idb_meth = &OBJ[ID_NMG];
    intern.idb_ptr = (void *)m;

    if (db_version(fp->dbip) < 5) {
	BU_EXTERNAL_INIT(&ext);
	int ret = intern.idb_meth->ft_export4(&ext, &intern, 1.0, fp->dbip, &rt_uniresource);
	if (ret < 0) {
	    bu_log("rt_db_put_internal(%s):  solid export failure\n",
		   name);
	    goto out;
	}
	db_wrap_v4_external(&ext, name);
    } else {
	if (rt_db_cvt_to_external5(&ext, name, &intern, 1.0, fp->dbip, &rt_uniresource, intern.idb_major_type) < 0) {
	    bu_log("wdb_export4(%s): solid export failure\n",
		   name);
	    goto out;
	}
    }
    BU_CK_EXTERNAL(&ext);

    flags = db_flags_internal(&intern);
    if (wdb_export_external(fp, &ext, name, flags, intern.idb_type) < 0)
	bu_log("wdb_export_external failure(%s)\n", name);

out:
    bu_free_external(&ext);
    wdb_close(fp);

    bu_log("nmg_stash_model_to_file(): wrote error.s to '%s'\n",
	   filename);
}


/* moved from nmg_bool.c - not sure where it should ultimately end up... */


/**
 * Called from db_walk_tree() each time a tree leaf is encountered.
 * The primitive solid, in external format, is provided in 'ep', and
 * the type of that solid (e.g. ID_ELL) is in 'id'.  The full tree
 * state including the accumulated transformation matrix and the
 * current tolerancing is in 'tsp', and the full path from root to
 * leaf is in 'pathp'.
 *
 * Import the solid, tessellate it into an NMG, stash a pointer to the
 * tessellation in a new tree structure (union), and return a pointer
 * to that.
 *
 * Usually given as an argument to, and called from db_walk_tree().
 *
 * This routine must be prepared to run in parallel.
 */
union tree *
nmg_booltree_leaf_tess(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, void *UNUSED(client_data))
{
    struct model *m;
    struct nmgregion *r1 = (struct nmgregion *)NULL;
    union tree *curtree;
    struct directory *dp;

    if (!tsp || !pathp || !ip)
	return TREE_NULL;

    RT_CK_DB_INTERNAL(ip);
    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);
    RT_CK_DIR(dp);

    if (!ip->idb_meth || !ip->idb_meth->ft_tessellate) {
	bu_log("ERROR(%s): tessellation support not available\n", dp->d_namep);
	return TREE_NULL;
    }

    NMG_CK_MODEL(*tsp->ts_m);
    BN_CK_TOL(tsp->ts_tol);
    BG_CK_TESS_TOL(tsp->ts_ttol);
    RT_CK_RESOURCE(tsp->ts_resp);

    m = nmg_mm();

    if (ip->idb_meth->ft_tessellate(&r1, m, ip, tsp->ts_ttol, tsp->ts_tol) < 0) {
	bu_log("ERROR(%s): tessellation failure\n", dp->d_namep);
	return TREE_NULL;
    }

    NMG_CK_REGION(r1);
    if (nmg_debug & NMG_DEBUG_VERIFY) {
	nmg_vshell(&r1->s_hd, r1);
    }

    BU_GET(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NMG_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_r = r1;

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK)
	bu_log("nmg_booltree_leaf_tess(%s) OK\n", dp->d_namep);

    return curtree;
}


/**
 * Called from db_walk_tree() each time a tree leaf is encountered.
 * The primitive solid, in external format, is provided in 'ep', and
 * the type of that solid (e.g. ID_ELL) is in 'id'.  The full tree
 * state including the accumulated transformation matrix and the
 * current tolerancing is in 'tsp', and the full path from root to
 * leaf is in 'pathp'.
 *
 * Import the solid, convert it into an NMG using t-NURBS, stash a
 * pointer in a new tree structure (union), and return a pointer to
 * that.
 *
 * Usually given as an argument to, and called from db_walk_tree().
 *
 * This routine must be prepared to run in parallel.
 */
union tree *
nmg_booltree_leaf_tnurb(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, void *UNUSED(client_data))
{
    struct nmgregion *r1;
    union tree *curtree;
    struct directory *dp;

    NMG_CK_MODEL(*tsp->ts_m);
    BN_CK_TOL(tsp->ts_tol);
    BG_CK_TESS_TOL(tsp->ts_ttol);
    RT_CK_DB_INTERNAL(ip);
    RT_CK_RESOURCE(tsp->ts_resp);

    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);
    RT_CK_DIR(dp);

    if (ip->idb_meth->ft_tnurb(
	    &r1, *tsp->ts_m, ip, tsp->ts_tol) < 0) {
	bu_log("nmg_booltree_leaf_tnurb(%s): CSG to t-NURB conversation failure\n", dp->d_namep);
	return TREE_NULL;
    }

    NMG_CK_REGION(r1);
    if (nmg_debug & NMG_DEBUG_VERIFY) {
	nmg_vshell(&r1->s_hd, r1);
    }

    BU_GET(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NMG_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_r = r1;

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK)
	bu_log("nmg_booltree_leaf_tnurb(%s) OK\n", dp->d_namep);

    return curtree;
}


/* quell the output of nmg_booltree_evaluate() to bu_log. */
int nmg_bool_eval_silent=0;

/**
 * Given a tree of leaf nodes tessellated earlier by
 * nmg_booltree_leaf_tess(), use recursion to do a depth-first
 * traversal of the tree, evaluating each pair of boolean operations
 * and reducing that result to a single nmgregion.
 *
 * Usually called from a do_region_end() handler from db_walk_tree().
 * For an example of several, see mged/dodraw.c.
 *
 * Returns an OP_NMG_TESS union tree node, which will contain the
 * resulting region and its name, as a dynamic string.  The caller is
 * responsible for releasing the string, and the node, by calling
 * db_free_tree() on the node.
 *
 * It is *essential* that the caller call nmg_model_fuse() before
 * calling this subroutine.
 *
 * Returns NULL if there is no geometry to return.
 *
 * Typical calls will be of this form:
 * (void)nmg_model_fuse(m, tol);
 * curtree = nmg_booltree_evaluate(curtree, tol);
 */
union tree *
nmg_booltree_evaluate(register union tree *tp, struct bu_list *vlfree, const struct bn_tol *tol, struct resource *resp)
{
    union tree *tl;
    union tree *tr;
    struct nmgregion *reg;
    int op = NMG_BOOL_ADD;      /* default value */
    const char *op_str = " u "; /* default value */
    size_t rem;
    char *name;

    RT_CK_TREE(tp);
    BN_CK_TOL(tol);
    RT_CK_RESOURCE(resp);

    switch (tp->tr_op) {
	case OP_NOP:
	    return TREE_NULL;
	case OP_NMG_TESS:
	    /* Hit a tree leaf */
	    if (nmg_debug & NMG_DEBUG_VERIFY) {
		nmg_vshell(&tp->tr_d.td_r->s_hd, tp->tr_d.td_r);
	    }
	    return tp;
	case OP_UNION:
	    op = NMG_BOOL_ADD;
	    op_str = " u ";
	    break;
	case OP_INTERSECT:
	    op = NMG_BOOL_ISECT;
	    op_str = " + ";
	    break;
	case OP_SUBTRACT:
	    op = NMG_BOOL_SUB;
	    op_str = " - ";
	    break;
	default:
	    bu_bomb("nmg_booltree_evaluate(): bad op\n");
    }

    /* Handle a boolean operation node.  First get its leaves. */
    tl = nmg_booltree_evaluate(tp->tr_b.tb_left, vlfree, tol, resp);
    tr = nmg_booltree_evaluate(tp->tr_b.tb_right, vlfree, tol, resp);

    if (tl) {
	RT_CK_TREE(tl);
	if (tl != tp->tr_b.tb_left) {
	    bu_bomb("nmg_booltree_evaluate(): tl != tp->tr_b.tb_left\n");
	}
    }
    if (tr) {
	RT_CK_TREE(tr);
	if (tr != tp->tr_b.tb_right) {
	    bu_bomb("nmg_booltree_evaluate(): tr != tp->tr_b.tb_right\n");
	}
    }

    if (!tl && !tr) {
	/* left-r == null && right-r == null */
	RT_CK_TREE(tp);
	db_free_tree(tp->tr_b.tb_left, resp);
	db_free_tree(tp->tr_b.tb_right, resp);
	tp->tr_op = OP_NOP;
	return TREE_NULL;
    }

    if (tl && !tr) {
	/* left-r != null && right-r == null */
	RT_CK_TREE(tp);
	db_free_tree(tp->tr_b.tb_right, resp);
	if (op == NMG_BOOL_ISECT) {
	    /* OP_INTERSECT '+' */
	    RT_CK_TREE(tp);
	    db_free_tree(tl, resp);
	    tp->tr_op = OP_NOP;
	    return TREE_NULL;
	} else {
	    /* copy everything from tl to tp no matter which union type
	     * could probably have done a mem-copy
	     */
	    tp->tr_op = tl->tr_op;
	    tp->tr_b.tb_regionp = tl->tr_b.tb_regionp;
	    tp->tr_b.tb_left = tl->tr_b.tb_left;
	    tp->tr_b.tb_right = tl->tr_b.tb_right;

	    /* null data from tl so only to free this node */
	    tl->tr_b.tb_regionp = (struct region *)NULL;
	    tl->tr_b.tb_left = TREE_NULL;
	    tl->tr_b.tb_right = TREE_NULL;

	    db_free_tree(tl, resp);
	    return tp;
	}
    }

    if (!tl && tr) {
	/* left-r == null && right-r != null */
	RT_CK_TREE(tp);
	db_free_tree(tp->tr_b.tb_left, resp);
	if (op == NMG_BOOL_ADD) {
	    /* OP_UNION 'u' */
	    /* copy everything from tr to tp no matter which union type
	     * could probably have done a mem-copy
	     */
	    tp->tr_op = tr->tr_op;
	    tp->tr_b.tb_regionp = tr->tr_b.tb_regionp;
	    tp->tr_b.tb_left = tr->tr_b.tb_left;
	    tp->tr_b.tb_right = tr->tr_b.tb_right;

	    /* null data from tr so only to free this node */
	    tr->tr_b.tb_regionp = (struct region *)NULL;
	    tr->tr_b.tb_left = TREE_NULL;
	    tr->tr_b.tb_right = TREE_NULL;

	    db_free_tree(tr, resp);
	    return tp;

	} else if ((op == NMG_BOOL_SUB) || (op == NMG_BOOL_ISECT)) {
	    /* for sub and intersect, if left-hand-side is null, result is null */
	    RT_CK_TREE(tp);
	    db_free_tree(tr, resp);
	    tp->tr_op = OP_NOP;
	    return TREE_NULL;

	} else {
	    bu_bomb("nmg_booltree_evaluate(): error, unknown operation\n");
	}
    }

    if (tl->tr_op != OP_NMG_TESS) {
	bu_bomb("nmg_booltree_evaluate(): bad left tree\n");
    }
    if (tr->tr_op != OP_NMG_TESS) {
	bu_bomb("nmg_booltree_evaluate(): bad right tree\n");
    }

    if (!nmg_bool_eval_silent) {
	bu_log(" {%s}%s{%s}\n", tl->tr_d.td_name, op_str, tr->tr_d.td_name);
    }

    NMG_CK_REGION(tr->tr_d.td_r);
    NMG_CK_REGION(tl->tr_d.td_r);

    if (nmg_ck_closed_region(tr->tr_d.td_r, tol)) {
	bu_bomb("nmg_booltree_evaluate(): ERROR, non-closed shell (r)\n");
    }
    if (nmg_ck_closed_region(tl->tr_d.td_r, tol)) {
	bu_bomb("nmg_booltree_evaluate(): ERROR, non-closed shell (l)\n");
    }

    nmg_r_radial_check(tr->tr_d.td_r, vlfree, tol);
    nmg_r_radial_check(tl->tr_d.td_r, vlfree, tol);

    if (nmg_debug & NMG_DEBUG_BOOL) {
	bu_log("Before model fuse\nShell A:\n");
	nmg_pr_s_briefly(BU_LIST_FIRST(shell, &tl->tr_d.td_r->s_hd), "");
	bu_log("Shell B:\n");
	nmg_pr_s_briefly(BU_LIST_FIRST(shell, &tr->tr_d.td_r->s_hd), "");
    }

    /* move operands into the same model */
    if (tr->tr_d.td_r->m_p != tl->tr_d.td_r->m_p) {
	nmg_merge_models(tl->tr_d.td_r->m_p, tr->tr_d.td_r->m_p);
    }

    /* input r1 and r2 are destroyed, output is new region */
    reg = nmg_do_bool(tl->tr_d.td_r, tr->tr_d.td_r, op, vlfree, tol);

    /* build string of result name */
    rem = strlen(tl->tr_d.td_name) + 3 + strlen(tr->tr_d.td_name) + 2 + 1;
    name = (char *)bu_calloc(rem, sizeof(char), "nmg_booltree_evaluate name");
    snprintf(name, rem, "(%s%s%s)", tl->tr_d.td_name, op_str, tr->tr_d.td_name);

    /* clean up child tree nodes */
    tl->tr_d.td_r = (struct nmgregion *)NULL;
    tr->tr_d.td_r = (struct nmgregion *)NULL;
    db_free_tree(tl, resp);
    db_free_tree(tr, resp);


    if (reg) {
	/* convert argument binary node into a result node */
	NMG_CK_REGION(reg);
	nmg_r_radial_check(reg, vlfree, tol);
	tp->tr_op = OP_NMG_TESS;
	tp->tr_d.td_r = reg;
	tp->tr_d.td_name = name;

	if (nmg_debug & NMG_DEBUG_VERIFY) {
	    nmg_vshell(&reg->s_hd, reg);
	}
	return tp;

    } else {
	/* resulting region was null */
	tp->tr_op = OP_NOP;
	return TREE_NULL;
    }

}

#if 0
/**
 * This function iterates over every nmg face in r and shifts it slightly.
 */
void
nmg_perturb_region(struct nmgregion *r)
{
    struct shell *s;
    struct faceuse *fu;
    struct vertex **pt;
    vect_t offset;
    int rand_ind = -1;

    /* Iterate over all faces in the NMG */
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    struct bu_ptbl vert_table;
	    const struct face_g_plane *fg;
	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation != OT_SAME) continue;

	    rand_ind = (rand_ind > BN_RAND_TABSIZE) ? 0 : rand_ind + 1;

	    /* calculate offset vector from plane */
	    fg = fu->f_p->g.plane_p;
	    VSCALE(offset, fg->N, 0.01 * BN_TOL_DIST * BN_RANDOM(rand_ind));

	    /* VADD the offset to each vertex */
	    nmg_tabulate_face_g_verts(&vert_table, fg);
	    for (BU_PTBL_FOR(pt, (struct vertex **), &vert_table)) {
		VADD2((*pt)->vg_p->coord, (*pt)->vg_p->coord, offset);
	    }

	    bu_ptbl_free(&vert_table);
	}
    }
}

void
nmg_find_leaves(register union tree *tp, struct bu_ptbl *regions)
{
    switch (tp->tr_op) {
	case OP_NOP:
	    return;
	case OP_NMG_TESS:
	    /* Hit a tree leaf */
	    bu_ptbl_ins_unique(regions, (long *)tp->tr_d.td_r);
	    return;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    nmg_find_leaves(tp->tr_b.tb_left, regions);
	    nmg_find_leaves(tp->tr_b.tb_right, regions);
	    break;
	default:
	    bu_bomb("nmg_booltree_evaluate(): bad op\n");
    }
}

/**
 * This function iterates over every nmg face in every nmg in the
 * tree and shifts it slightly.
 */
void
nmg_perturb_tree(union tree *tp)
{
    unsigned int i;
    struct bu_ptbl regions = BU_PTBL_INIT_ZERO;
    nmg_find_leaves(tp, &regions);
    if (BU_PTBL_LEN(&regions) > 0) {
	bu_log("Found %d regions\n", BU_PTBL_LEN(&regions));
	for (i = 0; i < BU_PTBL_LEN(&regions); i++) {
	    struct nmgregion *r = (struct nmgregion *)BU_PTBL_GET(&regions, i);
	    nmg_perturb_region(r);
	}
    } else {
	bu_log("Found NO regions??\n");
    }
}
#endif

/**
 * This is the main application interface to the NMG Boolean
 * Evaluator.
 *
 * This routine has the opportunity to do "once only" operations
 * before and after the boolean tree is walked.
 *
 * Returns -
 * 0 Boolean went OK.  Result region is in tp->tr_d.td_r
 * !0 Boolean produced null result.
 *
 * The caller is responsible for freeing 'tp' in either case,
 * typically with db_free_tree(tp);
 */
int
nmg_boolean(union tree *tp, struct model *m, struct bu_list *vlfree, const struct bn_tol *tol, struct resource *resp)
{
    union tree *result;
    int ret;

    RT_CK_TREE(tp);
    NMG_CK_MODEL(m);
    BN_CK_TOL(tol);
    RT_CK_RESOURCE(resp);

    if (nmg_debug & (NMG_DEBUG_BOOL|NMG_DEBUG_BASIC)) {
	bu_log("\n\nnmg_boolean(tp=%p, m=%p) START\n",
	       (void *)tp, (void *)m);
    }

    /* The nmg_model_fuse function was removed from this point in the
     * boolean process since not all geometry that is to be processed is
     * always within the single 'm' nmg model structure passed into this
     * function. In some cases the geometry resides in multiple nmg model
     * structures within the 'tp' tree that is passed into this function.
     * Running nmg_model_fuse is still required but is done later, i.e.
     * within the nmg_booltree_evaluate function just before the nmg_do_bool
     * function is called which is when the geometry, in which the boolean
     * to be performed, is always in a single nmg model structure.
     */

#if 0
    /* TODO - this doesn't seem to help any... */
    nmg_perturb_tree(tp);
#endif

    /*
     * Evaluate the nodes of the boolean tree one at a time, until
     * only a single region remains.
     */
    result = nmg_booltree_evaluate(tp, vlfree, tol, resp);

    if (result == TREE_NULL) {
	bu_log("nmg_boolean(): result of nmg_booltree_evaluate() is NULL\n");
	rt_pr_tree(tp, 0);
	ret = 1;
	goto out;
    }

    if (result != tp) {
	bu_bomb("nmg_boolean(): result of nmg_booltree_evaluate() isn't tp\n");
    }

    RT_CK_TREE(result);

    if (tp->tr_op != OP_NMG_TESS) {
	bu_log("nmg_boolean(): result of nmg_booltree_evaluate() op != OP_NMG_TESS\n");
	rt_pr_tree(tp, 0);
	ret = 1;
	goto out;
    }

    if (tp->tr_d.td_r == (struct nmgregion *)NULL) {
	/* Pointers are all OK, but boolean produced null set */
	ret = 1;
	goto out;
    }

    /* move result into correct model */
    nmg_merge_models(m, tp->tr_d.td_r->m_p);
    ret = 0;

out:
    if (nmg_debug & (NMG_DEBUG_BOOL|NMG_DEBUG_BASIC)) {
	bu_log("nmg_boolean(tp=%p, m=%p) END, ret=%d\n\n",
	       (void *)tp, (void *)m, ret);
    }

    return ret;
}

static int
Shell_is_arb(struct shell *s, struct bu_ptbl *tab, struct bu_list *vlfree)
{
    struct faceuse *fu;
    struct face *f;
    size_t four_verts=0;
    size_t three_verts=0;
    size_t face_count=0;
    size_t loop_count;

    NMG_CK_SHELL(s);

    nmg_vertex_tabulate(tab, &s->l.magic, vlfree);

    if (BU_PTBL_LEN(tab) > 8 || BU_PTBL_LEN(tab) < 4)
	goto not_arb;

    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	struct loopuse *lu;
	vect_t fu_norm;

	NMG_CK_FACEUSE(fu);

	if (fu->orientation != OT_SAME)
	    continue;

	f = fu->f_p;
	NMG_CK_FACE(f);

	if (*f->g.magic_p != NMG_FACE_G_PLANE_MAGIC)
	    goto not_arb;

	NMG_GET_FU_NORMAL(fu_norm, fu);

	loop_count = 0;
	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    struct edgeuse *eu;

	    NMG_CK_LOOPUSE(lu);

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		goto not_arb;

	    loop_count++;

	    /* face must be a single loop */
	    if (loop_count > 1)
		goto not_arb;

	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		struct edgeuse *eu_radial;
		struct faceuse *fu_radial;
		struct face *f_radial;
		vect_t norm_radial;
		vect_t eu_dir;
		vect_t cross;
		fastf_t dot;

		NMG_CK_EDGEUSE(eu);

		eu_radial = nmg_next_radial_eu(eu, s, 0);

		/* Can't have any dangling faces */
		if (eu_radial == eu || eu_radial == eu->eumate_p)
		    goto not_arb;

		fu_radial = nmg_find_fu_of_eu(eu_radial);
		NMG_CK_FACEUSE(fu_radial);

		if (fu_radial->orientation != OT_SAME)
		    fu_radial = fu_radial->fumate_p;

		f_radial = fu_radial->f_p;
		NMG_CK_FACE(f_radial);

		/* faces must be planar */
		if (*f_radial->g.magic_p != NMG_FACE_G_PLANE_MAGIC)
		    goto not_arb;


		/* Make sure shell is convex by checking that edgeuses
		 * run in direction fu_norm X norm_radial
		 */
		NMG_GET_FU_NORMAL(norm_radial, fu_radial);

		dot = VDOT(norm_radial, fu_norm);

		if (!NEAR_EQUAL(dot, 1.0, 0.00001)) {

		    VCROSS(cross, fu_norm, norm_radial);

		    if (eu->orientation == OT_NONE) {
			VSUB2(eu_dir, eu->vu_p->v_p->vg_p->coord, eu->eumate_p->vu_p->v_p->vg_p->coord);
			if (eu->orientation != OT_SAME) {
			    VREVERSE(eu_dir, eu_dir);
			}
		    } else {
			VMOVE(eu_dir, eu->g.lseg_p->e_dir);
		    }

		    if (eu->orientation == OT_SAME || eu->orientation == OT_NONE) {
			if (VDOT(cross, eu_dir) < 0.0)
			    goto not_arb;
		    } else {
			if (VDOT(cross, eu_dir) > 0.0)
			    goto not_arb;
		    }
		}
	    }
	}
    }

    /* count face types */
    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	struct loopuse *lu;
	int vert_count=0;

	if (fu->orientation != OT_SAME)
	    continue;

	face_count++;
	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    struct edgeuse *eu;

	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		vert_count++;
	}

	if (vert_count == 3)
	    three_verts++;
	else if (vert_count == 4)
	    four_verts++;
    }

    /* must have only three and four vertices in each face */
    if (face_count != three_verts + four_verts)
	goto not_arb;

    /* which type of arb is this?? */
    switch (BU_PTBL_LEN(tab)) {
	case 4:		/* each face must have 3 vertices */
	    if (three_verts != 4 || four_verts != 0)
		goto not_arb;
	    break;
	case 5:		/* one face with 4 verts, four with 3 verts */
	    if (four_verts != 1 || three_verts != 4)
		goto not_arb;
	    break;
	case 6:		/* three faces with 4 verts, two with 3 verts */
	    if (three_verts != 2 || four_verts != 3)
		goto not_arb;
	    break;
	case 7:		/* four faces with 4 verts, two with 3 verts */
	    if (four_verts != 4 || three_verts != 2)
		goto not_arb;
	    break;
	case 8:		/* each face must have 4 vertices */
	    if (four_verts != 6 || three_verts != 0)
		goto not_arb;
	    break;
    }

    return (int)BU_PTBL_LEN(tab);


not_arb:
    bu_ptbl_free(tab);
    return 0;
}



/**
 * Converts an NMG to an ARB, if possible.
 *
 * NMG must have been coplanar face merged and simplified
 *
 * Returns:
 * 	1 - Equivalent ARB was constructed
 * 	0 - Cannot construct an equivalent ARB
 *
 * The newly constructed arb is in "arb_int"
 */
int
nmg_to_arb(const struct model *m, struct rt_arb_internal *arb_int)
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct vertex *v;
    struct edgeuse *eu_start;
    struct faceuse *fu1;
    struct bu_ptbl tab = BU_PTBL_INIT_ZERO;
    int face_verts;
    int i, j;
    int found;
    int ret_val = 0;

    NMG_CK_MODEL(m);

    r = BU_LIST_FIRST(nmgregion, &m->r_hd);

    /* must be a single region */
    if (BU_LIST_NEXT_NOT_HEAD(&r->l, &m->r_hd))
	return 0;

    s = BU_LIST_FIRST(shell, &r->s_hd);
    NMG_CK_SHELL(s);

    /* must be a single shell */
    if (BU_LIST_NEXT_NOT_HEAD(&s->l, &r->s_hd))
	return 0;

    switch (Shell_is_arb(s, &tab, &RTG.rtg_vlfree)) {
	case 0:
	    ret_val = 0;
	    break;
	case 4:
	    v = (struct vertex *)BU_PTBL_GET(&tab, 0);
	    NMG_CK_VERTEX(v);
	    VMOVE(arb_int->pt[0], v->vg_p->coord);
	    v = (struct vertex *)BU_PTBL_GET(&tab, 1);
	    NMG_CK_VERTEX(v);
	    VMOVE(arb_int->pt[1], v->vg_p->coord);
	    v = (struct vertex *)BU_PTBL_GET(&tab, 2);
	    NMG_CK_VERTEX(v);
	    VMOVE(arb_int->pt[2], v->vg_p->coord);
	    VMOVE(arb_int->pt[3], v->vg_p->coord);
	    v = (struct vertex *)BU_PTBL_GET(&tab, 3);
	    NMG_CK_VERTEX(v);
	    VMOVE(arb_int->pt[4], v->vg_p->coord);
	    VMOVE(arb_int->pt[5], v->vg_p->coord);
	    VMOVE(arb_int->pt[6], v->vg_p->coord);
	    VMOVE(arb_int->pt[7], v->vg_p->coord);

	    bu_ptbl_free(&tab);
	    ret_val = 1;
	    break;
	case 5:
	    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
	    face_verts = 0;
	    while (face_verts != 4) {
		face_verts = 0;
		fu = BU_LIST_PNEXT_CIRC(faceuse, &fu->l);
		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		    face_verts++;
	    }
	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation != OT_SAME)
		fu = fu->fumate_p;
	    NMG_CK_FACEUSE(fu);

	    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	    j = 0;
	    eu_start = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		VMOVE(arb_int->pt[j], eu->vu_p->v_p->vg_p->coord);
		j++;
	    }

	    eu = eu_start->radial_p;
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = eu->eumate_p;
	    for (i=0; i<4; i++) {
		VMOVE(arb_int->pt[j], eu->vu_p->v_p->vg_p->coord);
		j++;
	    }

	    bu_ptbl_free(&tab);
	    ret_val = 1;
	    break;
	case 6:
	    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
	    face_verts = 0;
	    while (face_verts != 3) {
		face_verts = 0;
		fu = BU_LIST_PNEXT_CIRC(faceuse, &fu->l);
		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		    face_verts++;
	    }
	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation != OT_SAME)
		fu = fu->fumate_p;
	    NMG_CK_FACEUSE(fu);

	    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);

	    eu_start = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	    eu = eu_start;
	    VMOVE(arb_int->pt[1], eu->vu_p->v_p->vg_p->coord);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    VMOVE(arb_int->pt[0], eu->vu_p->v_p->vg_p->coord);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    VMOVE(arb_int->pt[4], eu->vu_p->v_p->vg_p->coord);
	    VMOVE(arb_int->pt[5], eu->vu_p->v_p->vg_p->coord);

	    eu = eu_start->radial_p;
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = eu->radial_p->eumate_p;
	    VMOVE(arb_int->pt[2], eu->vu_p->v_p->vg_p->coord);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    VMOVE(arb_int->pt[3], eu->vu_p->v_p->vg_p->coord);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    VMOVE(arb_int->pt[6], eu->vu_p->v_p->vg_p->coord);
	    VMOVE(arb_int->pt[7], eu->vu_p->v_p->vg_p->coord);

	    bu_ptbl_free(&tab);
	    ret_val = 1;
	    break;
	case 7:
	    found = 0;
	    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
	    while (!found) {
		int verts4=0, verts3=0;

		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		    struct loopuse *lu1;
		    struct edgeuse *eu1;
		    int vert_count=0;

		    fu1 = nmg_find_fu_of_eu(eu->radial_p);
		    lu1 = BU_LIST_FIRST(loopuse, &fu1->lu_hd);
		    for (BU_LIST_FOR (eu1, edgeuse, &lu1->down_hd))
			vert_count++;

		    if (vert_count == 4)
			verts4++;
		    else if (vert_count == 3)
			verts3++;
		}

		if (verts4 == 2 && verts3 == 2)
		    found = 1;
	    }
	    if (fu->orientation != OT_SAME)
		fu = fu->fumate_p;
	    NMG_CK_FACEUSE(fu);

	    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	    j = 0;
	    eu_start = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		VMOVE(arb_int->pt[j], eu->vu_p->v_p->vg_p->coord);
		j++;
	    }

	    eu = eu_start->radial_p;
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = eu->radial_p->eumate_p;
	    fu1 = nmg_find_fu_of_eu(eu);
	    if (nmg_faces_are_radial(fu, fu1)) {
		eu = eu_start->radial_p;
		eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
		eu = eu->radial_p->eumate_p;
	    }
	    for (i=0; i<4; i++) {
		VMOVE(arb_int->pt[j], eu->vu_p->v_p->vg_p->coord);
		j++;
		eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    }

	    bu_ptbl_free(&tab);
	    ret_val = 1;
	    break;
	case 8:
	    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation != OT_SAME)
		fu = fu->fumate_p;
	    NMG_CK_FACEUSE(fu);

	    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	    j = 0;
	    eu_start = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		VMOVE(arb_int->pt[j], eu->vu_p->v_p->vg_p->coord);
		j++;
	    }

	    eu = eu_start->radial_p;
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = eu->radial_p->eumate_p;
	    for (i=0; i<4; i++) {
		VMOVE(arb_int->pt[j], eu->vu_p->v_p->vg_p->coord);
		j++;
		eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    }

	    bu_ptbl_free(&tab);
	    ret_val = 1;
	    break;
	default:
	    bu_bomb("Shell_is_arb screwed up");
	    break;
    }
    if (ret_val)
	arb_int->magic = RT_ARB_INTERNAL_MAGIC;
    return ret_val;
}


/**
 * Converts an NMG to a TGC, if possible.
 *
 *
 * NMG must have been coplanar face merged and simplified
 *
 * Returns:
 * 	1 - Equivalent TGC was constructed
 * 	0 - Cannot construct an equivalent TGC
 *
 * The newly constructed tgc is in "tgc_int"
 *
 * Currently only supports RCC, and creates circumscribed RCC
 */
int
nmg_to_tgc(
    const struct model *m,
    struct rt_tgc_internal *tgc_int,
    const struct bn_tol *tol)
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct faceuse *fu_base=(struct faceuse *)NULL;
    struct faceuse *fu_top=(struct faceuse *)NULL;
    int three_vert_faces=0;
    int four_vert_faces=0;
    int many_vert_faces=0;
    int base_vert_count=0;
    int top_vert_count=0;
    point_t sum = VINIT_ZERO;
    int vert_count=0;
    fastf_t one_over_vert_count;
    point_t base_center;
    fastf_t min_base_r_sq;
    fastf_t max_base_r_sq;
    fastf_t sum_base_r_sq;
    fastf_t ave_base_r_sq;
    fastf_t base_r;
    point_t top_center;
    fastf_t min_top_r_sq;
    fastf_t max_top_r_sq;
    fastf_t sum_top_r_sq;
    fastf_t ave_top_r_sq;
    fastf_t top_r;
    plane_t top_pl = HINIT_ZERO;
    plane_t base_pl = HINIT_ZERO;
    vect_t plv_1, plv_2;

    NMG_CK_MODEL(m);

    BN_CK_TOL(tol);

    r = BU_LIST_FIRST(nmgregion, &m->r_hd);

    /* must be a single region */
    if (BU_LIST_NEXT_NOT_HEAD(&r->l, &m->r_hd))
	return 0;

    s = BU_LIST_FIRST(shell, &r->s_hd);
    NMG_CK_SHELL(s);

    /* must be a single shell */
    if (BU_LIST_NEXT_NOT_HEAD(&s->l, &r->s_hd))
	return 0;

    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	int lu_count=0;

	NMG_CK_FACEUSE(fu);
	if (fu->orientation != OT_SAME)
	    continue;

	vert_count = 0;

	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {

	    NMG_CK_LOOPUSE(lu);

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		return 0;

	    lu_count++;
	    if (lu_count > 1)
		return 0;

	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		vert_count++;
	}

	if (vert_count < 3)
	    return 0;

	if (vert_count == 4)
	    four_vert_faces++;
	else if (vert_count == 3)
	    three_vert_faces++;
	else {
	    many_vert_faces++;
	    if (many_vert_faces > 2)
		return 0;

	    if (many_vert_faces == 1) {
		fu_base = fu;
		base_vert_count = vert_count;
		NMG_GET_FU_PLANE(base_pl, fu_base);
	    } else if (many_vert_faces == 2) {
		fu_top = fu;
		top_vert_count = vert_count;
		NMG_GET_FU_PLANE(top_pl, fu_top);
	    }
	}
    }
    /* if there are any three vertex faces,
     * there must be an even number of them
     */
    if (three_vert_faces%2)
	return 0;

    /* base and top must have same number of vertices */
    if (base_vert_count != top_vert_count)
	return 0;

    /* Must have correct number of side faces */
    if ((base_vert_count != four_vert_faces) &&
	(base_vert_count*2 != three_vert_faces))
	return 0;

    if (!NEAR_EQUAL(VDOT(top_pl, base_pl), -1.0, tol->perp))
	return 0;

    /* This looks like a good candidate,
     * Calculate center of base and top faces
     */

    if (fu_base) {
	vert_count = 0;
	VSETALL(sum, 0.0);
	lu = BU_LIST_FIRST(loopuse, &fu_base->lu_hd);
	for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
	    struct vertex_g *vg;

	    NMG_CK_EDGEUSE(eu);

	    NMG_CK_VERTEXUSE(eu->vu_p);
	    NMG_CK_VERTEX(eu->vu_p->v_p);
	    vg = eu->vu_p->v_p->vg_p;
	    NMG_CK_VERTEX_G(vg);

	    VADD2(sum, sum, vg->coord);
	    vert_count++;
	}

	one_over_vert_count = 1.0/(fastf_t)vert_count;
	VSCALE(base_center, sum, one_over_vert_count);

	/* Calculate Average Radius */
	min_base_r_sq = MAX_FASTF;
	max_base_r_sq = (-min_base_r_sq);
	sum_base_r_sq = 0.0;
	lu = BU_LIST_FIRST(loopuse, &fu_base->lu_hd);
	for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
	    struct vertex_g *vg;
	    vect_t rad_vect;
	    fastf_t r_sq;

	    vg = eu->vu_p->v_p->vg_p;

	    VSUB2(rad_vect, vg->coord, base_center);
	    r_sq = MAGSQ(rad_vect);
	    if (r_sq > max_base_r_sq)
		max_base_r_sq = r_sq;
	    if (r_sq < min_base_r_sq)
		min_base_r_sq = r_sq;

	    sum_base_r_sq += r_sq;
	}

	ave_base_r_sq = sum_base_r_sq * one_over_vert_count;

	base_r = sqrt(max_base_r_sq);

	if (!NEAR_ZERO((max_base_r_sq - ave_base_r_sq)/ave_base_r_sq, 0.001) ||
	    !NEAR_ZERO((min_base_r_sq - ave_base_r_sq)/ave_base_r_sq, 0.001))
	    return 0;

	VSETALL(sum, 0.0);
	lu = BU_LIST_FIRST(loopuse, &fu_top->lu_hd);
	for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
	    struct vertex_g *vg;

	    NMG_CK_EDGEUSE(eu);

	    NMG_CK_VERTEXUSE(eu->vu_p);
	    NMG_CK_VERTEX(eu->vu_p->v_p);
	    vg = eu->vu_p->v_p->vg_p;
	    NMG_CK_VERTEX_G(vg);

	    VADD2(sum, sum, vg->coord);
	}

	VSCALE(top_center, sum, one_over_vert_count);

	/* Calculate Average Radius */
	min_top_r_sq = MAX_FASTF;
	max_top_r_sq = (-min_top_r_sq);
	sum_top_r_sq = 0.0;
	lu = BU_LIST_FIRST(loopuse, &fu_top->lu_hd);
	for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
	    struct vertex_g *vg;
	    vect_t rad_vect;
	    fastf_t r_sq;

	    vg = eu->vu_p->v_p->vg_p;

	    VSUB2(rad_vect, vg->coord, top_center);
	    r_sq = MAGSQ(rad_vect);
	    if (r_sq > max_top_r_sq)
		max_top_r_sq = r_sq;
	    if (r_sq < min_top_r_sq)
		min_top_r_sq = r_sq;

	    sum_top_r_sq += r_sq;
	}

	ave_top_r_sq = sum_top_r_sq * one_over_vert_count;
	top_r = sqrt(max_top_r_sq);

	if (!NEAR_ZERO((max_top_r_sq - ave_top_r_sq)/ave_top_r_sq, 0.001) ||
	    !NEAR_ZERO((min_top_r_sq - ave_top_r_sq)/ave_top_r_sq, 0.001))
	    return 0;


	VMOVE(tgc_int->v, base_center);
	VSUB2(tgc_int->h, top_center, base_center);

	bn_vec_perp(plv_1, top_pl);
	VCROSS(plv_2, top_pl, plv_1);
	VUNITIZE(plv_1);
	VUNITIZE(plv_2);
	VSCALE(tgc_int->a, plv_1, base_r);
	VSCALE(tgc_int->b, plv_2, base_r);
	VSCALE(tgc_int->c, plv_1, top_r);
	VSCALE(tgc_int->d, plv_2, top_r);

	tgc_int->magic = RT_TGC_INTERNAL_MAGIC;

    }

    return 1;
}

/**
 * XXX This routine is deprecated in favor of BoTs
 */
int
nmg_to_poly(const struct model *m, struct rt_pg_internal *poly_int, struct bu_list *vlfree, const struct bn_tol *tol)
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct model *dup_m;
    struct nmgregion *dup_r;
    struct shell *dup_s;
    int max_count;
    int count_npts;
    int face_count=0;

    NMG_CK_MODEL(m);

    BN_CK_TOL(tol);

    for (BU_LIST_FOR (r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR (s, shell, &r->s_hd)) {
	    if (nmg_check_closed_shell(s, tol))
		return 0;
	}
    }

    dup_m = nmg_mm();
    dup_r = nmg_mrsv(dup_m);
    dup_s = BU_LIST_FIRST(shell, &dup_r->s_hd);

    for (BU_LIST_FOR (r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR (s, shell, &r->s_hd)) {
	    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
		if (fu->orientation != OT_SAME)
		    continue;
		(void)nmg_dup_face(fu, dup_s);
	    }
	}
    }

    for (BU_LIST_FOR (dup_r, nmgregion, &dup_m->r_hd)) {
	for (BU_LIST_FOR (dup_s, shell, &dup_r->s_hd)) {
	    for (BU_LIST_FOR (fu, faceuse, &dup_s->fu_hd)) {
		NMG_CK_FACEUSE(fu);

		/* only do OT_SAME faces */
		if (fu->orientation != OT_SAME)
		    continue;

		/* count vertices in loops */
		max_count = 0;
		for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
		    NMG_CK_LOOPUSE(lu);
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
			continue;

		    if (lu->orientation != OT_SAME) {
			/* triangulate holes */
			max_count = 6;
			break;
		    }

		    count_npts = 0;
		    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
			count_npts++;

		    if (count_npts > 5) {
			max_count = count_npts;
			break;
		    }
		    if (!nmg_lu_is_convex(lu, vlfree, tol)) {
			/* triangulate non-convex faces */
			max_count = 6;
			break;
		    }
		}

		/* if any loop has more than 5 vertices, triangulate the face */
		if (max_count > 5) {
		    if (nmg_debug & NMG_DEBUG_BASIC)
			bu_log("nmg_to_poly: triangulating fu %p\n", (void *)fu);
		    nmg_triangulate_fu(fu, vlfree, tol);
		}

		for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
		    NMG_CK_LOOPUSE(lu);
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
			continue;

		    face_count++;
		}
	    }
	}
    }
    poly_int->npoly = face_count;
    poly_int->poly = (struct rt_pg_face_internal *)bu_calloc(face_count,
							     sizeof(struct rt_pg_face_internal), "nmg_to_poly: poly");

    face_count = 0;
    for (BU_LIST_FOR (dup_r, nmgregion, &dup_m->r_hd)) {
	for (BU_LIST_FOR (dup_s, shell, &dup_r->s_hd)) {
	    for (BU_LIST_FOR (fu, faceuse, &dup_s->fu_hd)) {
		vect_t norm;

		NMG_CK_FACEUSE(fu);

		/* only do OT_SAME faces */
		if (fu->orientation != OT_SAME)
		    continue;

		NMG_GET_FU_NORMAL(norm, fu);

		for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
		    int pt_no=0;

		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
			continue;

		    /* count vertices in loop */
		    count_npts = 0;
		    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
			count_npts++;

		    poly_int->poly[face_count].npts = count_npts;
		    poly_int->poly[face_count].verts = (fastf_t *) bu_calloc(3*count_npts, sizeof(fastf_t), "nmg_to_poly: verts");
		    poly_int->poly[face_count].norms = (fastf_t *) bu_calloc(3*count_npts, sizeof(fastf_t), "nmg_to_poly: norms");

		    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
			struct vertex_g *vg;

			vg = eu->vu_p->v_p->vg_p;
			NMG_CK_VERTEX_G(vg);

			VMOVE(&(poly_int->poly[face_count].verts[pt_no*3]), vg->coord);
			VMOVE(&(poly_int->poly[face_count].norms[pt_no*3]), norm);

			pt_no++;
		    }
		    face_count++;
		}
	    }
	}
    }

    poly_int->magic = RT_PG_INTERNAL_MAGIC;
    nmg_km(dup_m);

    return 1;
}


static int
_nmg_shell_append(struct rt_bot_internal *bot, struct bu_ptbl *nmg_vertices, struct bu_ptbl *nmg_faces, size_t *voffset, size_t *foffset)
{
    size_t i;
    size_t vo = (voffset) ? (unsigned int)*voffset : 0;
    size_t fo = (foffset) ? (unsigned int)*foffset : 0;
    size_t face_no;
    struct vertex *v;

    /* fill in the vertices */
    for (i = 0; i < BU_PTBL_LEN(nmg_vertices); i++) {
	struct vertex_g *vg;

	v = (struct vertex *)BU_PTBL_GET(nmg_vertices, i);
	NMG_CK_VERTEX(v);

	vg = v->vg_p;
	NMG_CK_VERTEX_G(vg);

	VMOVE(&bot->vertices[(i + vo)*3], vg->coord);
    }
    if (voffset) (*voffset) += (int)BU_PTBL_LEN(nmg_vertices);

    /* fill in the faces */
    face_no = 0;
    for (i = 0; i < BU_PTBL_LEN(nmg_faces); i++) {
	struct face *f;
	struct faceuse *fu;
	struct loopuse *lu;

	f = (struct face *)BU_PTBL_GET(nmg_faces, i);
	NMG_CK_FACE(f);

	fu = f->fu_p;

	if (fu->orientation != OT_SAME) {
	    fu = fu->fumate_p;
	    if (fu->orientation != OT_SAME) {
		bu_log("nmg_bot(): Face has no OT_SAME use!\n");
		return -1;
	    }
	}

	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    struct edgeuse *eu;
	    size_t vertex_no=0;

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;

	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		if (vertex_no > 2) {
		    bu_log("nmg_bot(): Face has not been triangulated!\n");
		    return -1;
		}

		v = eu->vu_p->v_p;
		NMG_CK_VERTEX(v);

		bot->faces[ (face_no + fo)*3 + vertex_no ] = bu_ptbl_locate(nmg_vertices, (long *)v);

		vertex_no++;
	    }

	    face_no++;
	}
    }

    if (foffset) (*foffset) += face_no;

    return 0;
}


size_t
_nmg_tri_count(struct bu_ptbl *nmg_faces)
{
    size_t i;
    size_t num_faces = 0;
    /* count the number of triangles */
    for (i=0; i<BU_PTBL_LEN(nmg_faces); i++) {
	struct face *f;
	struct faceuse *fu;
	struct loopuse *lu;

	f = (struct face *)BU_PTBL_GET(nmg_faces, i);
	NMG_CK_FACE(f);

	fu = f->fu_p;

	if (fu->orientation != OT_SAME) {
	    fu = fu->fumate_p;
	    if (fu->orientation != OT_SAME) {
		return 0;
	    }
	}

	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;
	    num_faces++;
	}
    }
    return num_faces;
}

/**
 * Convert an NMG to a BOT solid
 */
struct rt_bot_internal *
nmg_bot(struct shell *s, struct bu_list *vlfree, const struct bn_tol *tol)
{
    struct rt_bot_internal *bot;
    struct bu_ptbl nmg_vertices;
    struct bu_ptbl nmg_faces;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    /* first convert the NMG to triangles */
    (void)nmg_triangulate_shell(s, vlfree, tol);

    /* make a list of all the vertices */
    nmg_vertex_tabulate(&nmg_vertices, &s->l.magic, vlfree);

    /* and a list of all the faces */
    nmg_face_tabulate(&nmg_faces, &s->l.magic, vlfree);

    /* now build the BOT */
    BU_ALLOC(bot, struct rt_bot_internal);

    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags = 0;

    bot->num_vertices = BU_PTBL_LEN(&nmg_vertices);
    bot->num_faces = 0;

    /* count the number of triangles */
    bot->num_faces = _nmg_tri_count(&nmg_faces);

    bot->faces = (int *)bu_calloc(bot->num_faces * 3, sizeof(int), "BOT faces");
    bot->vertices = (fastf_t *)bu_calloc(bot->num_vertices * 3, sizeof(fastf_t), "BOT vertices");

    bot->thickness = (fastf_t *)NULL;
    bot->face_mode = (struct bu_bitv *)NULL;

    if (_nmg_shell_append(bot, &nmg_vertices, &nmg_faces, NULL, NULL) < 0) {
	bu_log("nmg_bot(): Failed to process shell!\n");
	return (struct rt_bot_internal *)NULL;
    }

    bu_ptbl_free(&nmg_vertices);
    bu_ptbl_free(&nmg_faces);

    return bot;
}

static void
_nmg_shell_tabulate(struct bu_ptbl *va, struct bu_ptbl *fa, struct shell *s, struct bu_list *vlfree)
{
    struct bu_ptbl *nmg_vertices, *nmg_faces;

    NMG_CK_SHELL(s);

    BU_ALLOC(nmg_vertices, struct bu_ptbl);
    BU_ALLOC(nmg_faces, struct bu_ptbl);

    /* make a list of all the vertices */
    nmg_vertex_tabulate(nmg_vertices, &s->l.magic, vlfree);
    bu_ptbl_ins(va, (long *)nmg_vertices);
    /* and a list of all the faces */
    nmg_face_tabulate(nmg_faces, &s->l.magic, vlfree);

    bu_ptbl_ins(fa, (long *)nmg_faces);
}


/**
 * Convert all shells of an NMG to a BOT solid
 */
struct rt_bot_internal *
nmg_mdl_to_bot(struct model *m, struct bu_list *vlfree, const struct bn_tol *tol)
{
    unsigned int i = 0;
    struct nmgregion *r;
    struct shell *s;
    struct rt_bot_internal *bot;
    struct bu_ptbl vert_arrays = BU_PTBL_INIT_ZERO;
    struct bu_ptbl face_arrays = BU_PTBL_INIT_ZERO;
    size_t vert_cnt = 0;
    size_t face_cnt = 0;

    BN_CK_TOL(tol);
    NMG_CK_MODEL(m);

    /* first convert the NMG to triangles */
    nmg_triangulate_model(m, vlfree, tol);

    /* For each shell, tabulate faces and vertices */
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	NMG_CK_REGION(r);
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    _nmg_shell_tabulate(&vert_arrays, &face_arrays, s, vlfree);
	}
    }

    /* Count up the verts */
    for (i = 0; i < BU_PTBL_LEN(&vert_arrays); i++) {
	struct bu_ptbl *vac = (struct bu_ptbl *)BU_PTBL_GET(&vert_arrays, i);
	vert_cnt += BU_PTBL_LEN(vac);
    }

    /* Count up the faces */
    for (i = 0; i < BU_PTBL_LEN(&face_arrays); i++) {
	struct bu_ptbl *nfaces = (struct bu_ptbl *)BU_PTBL_GET(&face_arrays, i);
	face_cnt += _nmg_tri_count(nfaces);
    }

    /* now build the BOT */
    BU_ALLOC(bot, struct rt_bot_internal);

    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags = 0;

    bot->num_vertices = vert_cnt;
    bot->vertices = (fastf_t *)bu_calloc(bot->num_vertices * 3, sizeof(fastf_t), "BOT vertices");
    bot->num_faces = face_cnt;
    bot->faces = (int *)bu_calloc(bot->num_faces * 3, sizeof(int), "BOT faces");

    bot->thickness = (fastf_t *)NULL;
    bot->face_mode = (struct bu_bitv *)NULL;

    vert_cnt = 0;
    face_cnt = 0;
    for (i = 0; i < BU_PTBL_LEN(&vert_arrays); i++) {
	struct bu_ptbl *va = (struct bu_ptbl *)BU_PTBL_GET(&vert_arrays, i);
	struct bu_ptbl *fa = (struct bu_ptbl *)BU_PTBL_GET(&face_arrays, i);
	if (_nmg_shell_append(bot, va, fa, &vert_cnt, &face_cnt) < 0) {
	    bu_log("nmg_mdl_to_bot(): Failed to process shell!\n");
	    bu_free((char *)bot->vertices, "BOT vertices");
	    bu_free((char *)bot->faces, "BOT faces");
	    bu_free((char *)bot, "BOT");
	    return (struct rt_bot_internal *)NULL;
	}
    }

    for (i = 0; i < BU_PTBL_LEN(&vert_arrays); i++) {
	struct bu_ptbl *va = (struct bu_ptbl *)BU_PTBL_GET(&vert_arrays, i);
	struct bu_ptbl *fa = (struct bu_ptbl *)BU_PTBL_GET(&face_arrays, i);
	bu_ptbl_free(va);
	bu_ptbl_free(fa);
	bu_free(va, "va container");
	bu_free(fa, "va container");
    }
    bu_ptbl_free(&vert_arrays);
    bu_ptbl_free(&face_arrays);
    return bot;
}



void
rt_nmg_print_hitmiss(struct hitmiss *a_hit)
{
    NMG_CK_HITMISS(a_hit);

    bu_log("   dist:%12g pt=(%f %f %f) %s=%p\n",
	   a_hit->hit.hit_dist,
	   a_hit->hit.hit_point[0],
	   a_hit->hit.hit_point[1],
	   a_hit->hit.hit_point[2],
	   bu_identify_magic(*(uint32_t *)a_hit->hit.hit_private),
	   (void *)a_hit->hit.hit_private
	);
    bu_log("\tstate:%s", nmg_rt_inout_str(a_hit->in_out));

    switch (a_hit->start_stop) {
	case NMG_HITMISS_SEG_IN:	bu_log(" SEG_START"); break;
	case NMG_HITMISS_SEG_OUT:	bu_log(" SEG_STOP"); break;
    }

    VPRINT("\n\tin_normal", a_hit->inbound_norm);
    VPRINT("\tout_normal", a_hit->outbound_norm);
}
void
rt_nmg_print_hitlist(struct bu_list *hd)
{
    struct hitmiss *a_hit;

    bu_log("nmg/ray hit list:\n");

    for (BU_LIST_FOR(a_hit, hitmiss, hd)) {
	rt_nmg_print_hitmiss(a_hit);
    }
}

void
rt_isect_ray_model(struct ray_data *rd, struct bu_list *vlfree)
{
    nmg_isect_ray_model((struct nmg_ray_data *)rd, vlfree);
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
