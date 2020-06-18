/*                           N M G . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2020 United States Government as represented by
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

#ifdef DO_LONGJMP
static jmp_buf nmg_longjump_env;
#endif

/* EDGE-FACE correlation data
 * used in edge_hit() for 3manifold case
 */
struct ef_data {
    fastf_t fdotr;	/* face vector VDOT with ray */
    fastf_t fdotl;	/* face vector VDOT with ray-left */
    fastf_t ndotr;	/* face normal VDOT with ray */
    struct edgeuse *eu;
};

HIDDEN void
segs_error(const char *str) {
    bu_log("%s\n", str);
#ifdef DO_LONGJMP
    longjmp(nmg_longjump_env, -1);}
#endif
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

HIDDEN void
visitor(uint32_t *l_p, void *tbl, int UNUSED(unused))
{
    (void)bu_ptbl_ins_unique((struct bu_ptbl *)tbl, (long *)l_p);
}

/**
 * Add an element provided by nmg_visit to a bu_ptbl struct.
 */
HIDDEN void
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
HIDDEN long *
common_topo(struct bu_ptbl *a_tbl, struct bu_ptbl *next_tbl)
{
    long **p;

    for (p = &a_tbl->buffer[a_tbl->end]; p >= a_tbl->buffer; p--) {
	if (bu_ptbl_locate(next_tbl, *p) >= 0)
	    return *p;
    }

    return (long *)NULL;
}

HIDDEN void
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





HIDDEN void
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


HIDDEN int
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



HIDDEN void
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


HIDDEN void
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


HIDDEN void
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


HIDDEN int
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


HIDDEN int
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


HIDDEN int
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


HIDDEN int
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


HIDDEN int
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

HIDDEN int
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

HIDDEN int
state5(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol)
/* intersection w/ ray */
/* The segment we're building */
/* The number of valid segments built */
/* The input hit point */

{
    return state5and6(seghead, seg_p, seg_count, a_hit, stp, ap, tol, 5);
}


HIDDEN int
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


HIDDEN int
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

#ifdef DO_LONGJMP
    if (setjmp(nmg_longjump_env) != 0) {
	return 0;
    }
#endif

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
rt_nmg_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct rt_view_info *UNUSED(info))
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


#define NMG_CK_DISKMAGIC(_cp, _magic)	\
    if (ntohl(*(uint32_t*)_cp) != _magic) { \
	bu_log("NMG_CK_DISKMAGIC: magic mis-match, got x%x, s/b x%x, file %s, line %d\n", \
	       ntohl(*(uint32_t*)_cp), _magic, __FILE__, __LINE__); \
	bu_bomb("bad magic\n"); \
    }


/* ----------------------------------------------------------------------
 *
 * Definitions for the binary, machine-independent format of the NMG
 * data structures.
 *
 * There are two special values that may be assigned to an
 * disk_index_t to signal special processing when the structure is
 * re-import4ed.
 */
#define DISK_INDEX_NULL 0
#define DISK_INDEX_LISTHEAD -1

#define DISK_MODEL_VERSION 1	/* V0 was Release 4.0 */

typedef unsigned char disk_index_t[4]; /* uint32_t buffer */
struct disk_rt_list {
    disk_index_t forw;
    disk_index_t back;
};


#define DISK_MODEL_MAGIC 0x4e6d6f64	/* Nmod */
struct disk_model {
    unsigned char magic[4];
    unsigned char version[4];	/* unused */
    struct disk_rt_list r_hd;
};


#define DISK_REGION_MAGIC 0x4e726567	/* Nreg */
struct disk_nmgregion {
    unsigned char magic[4];
    struct disk_rt_list l;
    disk_index_t m_p;
    disk_index_t ra_p;
    struct disk_rt_list s_hd;
};


#define DISK_REGION_A_MAGIC 0x4e725f61	/* Nr_a */
struct disk_nmgregion_a {
    unsigned char magic[4];
    unsigned char min_pt[3*8];
    unsigned char max_pt[3*8];
};


#define DISK_SHELL_MAGIC 0x4e73686c	/* Nshl */
struct disk_shell {
    unsigned char magic[4];
    struct disk_rt_list l;
    disk_index_t r_p;
    disk_index_t sa_p;
    struct disk_rt_list fu_hd;
    struct disk_rt_list lu_hd;
    struct disk_rt_list eu_hd;
    disk_index_t vu_p;
};


#define DISK_SHELL_A_MAGIC 0x4e735f61	/* Ns_a */
struct disk_shell_a {
    unsigned char magic[4];
    unsigned char min_pt[3*8];
    unsigned char max_pt[3*8];
};


#define DISK_FACE_MAGIC 0x4e666163	/* Nfac */
struct disk_face {
    unsigned char magic[4];
    struct disk_rt_list l;
    disk_index_t fu_p;
    disk_index_t g;
    unsigned char flip[4];
};


#define DISK_FACE_G_PLANE_MAGIC 0x4e666770	/* Nfgp */
struct disk_face_g_plane {
    unsigned char magic[4];
    struct disk_rt_list f_hd;
    unsigned char N[4*8];
};


#define DISK_FACE_G_SNURB_MAGIC 0x4e666773	/* Nfgs */
struct disk_face_g_snurb {
    unsigned char magic[4];
    struct disk_rt_list f_hd;
    unsigned char u_order[4];
    unsigned char v_order[4];
    unsigned char u_size[4];	/* u.k_size */
    unsigned char v_size[4];	/* v.k_size */
    disk_index_t u_knots;	/* u.knots subscript */
    disk_index_t v_knots;	/* v.knots subscript */
    unsigned char us_size[4];
    unsigned char vs_size[4];
    unsigned char pt_type[4];
    disk_index_t ctl_points;	/* subscript */
};


#define DISK_FACEUSE_MAGIC 0x4e667520	/* Nfu */
struct disk_faceuse {
    unsigned char magic[4];
    struct disk_rt_list l;
    disk_index_t s_p;
    disk_index_t fumate_p;
    unsigned char orientation[4];
    disk_index_t f_p;
    disk_index_t fua_p;
    struct disk_rt_list lu_hd;
};


#define DISK_LOOP_MAGIC 0x4e6c6f70	/* Nlop */
struct disk_loop {
    unsigned char magic[4];
    disk_index_t lu_p;
    disk_index_t lg_p;
};


#define DISK_LOOP_G_MAGIC 0x4e6c5f67	/* Nl_g */
struct disk_loop_g {
    unsigned char magic[4];
    unsigned char min_pt[3*8];
    unsigned char max_pt[3*8];
};


#define DISK_LOOPUSE_MAGIC 0x4e6c7520	/* Nlu */
struct disk_loopuse {
    unsigned char magic[4];
    struct disk_rt_list l;
    disk_index_t up;
    disk_index_t lumate_p;
    unsigned char orientation[4];
    disk_index_t l_p;
    disk_index_t lua_p;
    struct disk_rt_list down_hd;
};


#define DISK_EDGE_MAGIC 0x4e656467	/* Nedg */
struct disk_edge {
    unsigned char magic[4];
    disk_index_t eu_p;
    unsigned char is_real[4];
};


#define DISK_EDGE_G_LSEG_MAGIC 0x4e65676c	/* Negl */
struct disk_edge_g_lseg {
    unsigned char magic[4];
    struct disk_rt_list eu_hd2;
    unsigned char e_pt[3*8];
    unsigned char e_dir[3*8];
};


#define DISK_EDGE_G_CNURB_MAGIC 0x4e656763	/* Negc */
struct disk_edge_g_cnurb {
    unsigned char magic[4];
    struct disk_rt_list eu_hd2;
    unsigned char order[4];
    unsigned char k_size[4];	/* k.k_size */
    disk_index_t knots;		/* knot.knots subscript */
    unsigned char c_size[4];
    unsigned char pt_type[4];
    disk_index_t ctl_points;	/* subscript */
};


#define DISK_EDGEUSE_MAGIC 0x4e657520	/* Neu */
struct disk_edgeuse {
    unsigned char magic[4];
    struct disk_rt_list l;
    struct disk_rt_list l2;
    disk_index_t up;
    disk_index_t eumate_p;
    disk_index_t radial_p;
    disk_index_t e_p;
    disk_index_t eua_p;
    unsigned char orientation[4];
    disk_index_t vu_p;
    disk_index_t g;
};


#define DISK_VERTEX_MAGIC 0x4e767274	/* Nvrt */
struct disk_vertex {
    unsigned char magic[4];
    struct disk_rt_list vu_hd;
    disk_index_t vg_p;
};


#define DISK_VERTEX_G_MAGIC 0x4e765f67	/* Nv_g */
struct disk_vertex_g {
    unsigned char magic[4];
    unsigned char coord[3*8];
};


#define DISK_VERTEXUSE_MAGIC 0x4e767520	/* Nvu */
struct disk_vertexuse {
    unsigned char magic[4];
    struct disk_rt_list l;
    disk_index_t up;
    disk_index_t v_p;
    disk_index_t a;
};


#define DISK_VERTEXUSE_A_PLANE_MAGIC 0x4e767561	/* Nvua */
struct disk_vertexuse_a_plane {
    unsigned char magic[4];
    unsigned char N[3*8];
};


#define DISK_VERTEXUSE_A_CNURB_MAGIC 0x4e766163	/* Nvac */
struct disk_vertexuse_a_cnurb {
    unsigned char magic[4];
    unsigned char param[3*8];
};


#define DISK_DOUBLE_ARRAY_MAGIC 0x4e666172		/* Narr */
struct disk_double_array {
    unsigned char magic[4];
    unsigned char ndouble[4];	/* # of doubles to follow */
    unsigned char vals[1*8];	/* actually [ndouble*8] */
};


/* ---------------------------------------------------------------------- */
/* All these arrays and defines have to use the same implicit index
 * values. FIXME: this should probably be an enum.
 */
#define NMG_KIND_MODEL              0
#define NMG_KIND_NMGREGION          1
#define NMG_KIND_NMGREGION_A        2
#define NMG_KIND_SHELL              3
#define NMG_KIND_SHELL_A            4
#define NMG_KIND_FACEUSE            5
#define NMG_KIND_FACE               6
#define NMG_KIND_FACE_G_PLANE       7
#define NMG_KIND_FACE_G_SNURB       8
#define NMG_KIND_LOOPUSE            9
#define NMG_KIND_LOOP              10
#define NMG_KIND_LOOP_G            11
#define NMG_KIND_EDGEUSE           12
#define NMG_KIND_EDGE              13
#define NMG_KIND_EDGE_G_LSEG       14
#define NMG_KIND_EDGE_G_CNURB      15
#define NMG_KIND_VERTEXUSE         16
#define NMG_KIND_VERTEXUSE_A_PLANE 17
#define NMG_KIND_VERTEXUSE_A_CNURB 18
#define NMG_KIND_VERTEX            19
#define NMG_KIND_VERTEX_G          20
/* 21 through 24 are unassigned, and reserved for future use */

/** special, variable sized */
#define NMG_KIND_DOUBLE_ARRAY      25

/* number of kinds.  This number must have some extra space, for
 * upwards compatibility.
 */
#define NMG_N_KINDS                26


const int rt_nmg_disk_sizes[NMG_N_KINDS] = {
    sizeof(struct disk_model),		/* 0 */
    sizeof(struct disk_nmgregion),
    sizeof(struct disk_nmgregion_a),
    sizeof(struct disk_shell),
    sizeof(struct disk_shell_a),
    sizeof(struct disk_faceuse),
    sizeof(struct disk_face),
    sizeof(struct disk_face_g_plane),
    sizeof(struct disk_face_g_snurb),
    sizeof(struct disk_loopuse),
    sizeof(struct disk_loop),		/* 10 */
    sizeof(struct disk_loop_g),
    sizeof(struct disk_edgeuse),
    sizeof(struct disk_edge),
    sizeof(struct disk_edge_g_lseg),
    sizeof(struct disk_edge_g_cnurb),
    sizeof(struct disk_vertexuse),
    sizeof(struct disk_vertexuse_a_plane),
    sizeof(struct disk_vertexuse_a_cnurb),
    sizeof(struct disk_vertex),
    sizeof(struct disk_vertex_g),		/* 20 */
    0,
    0,
    0,
    0,
    0  /* disk_double_array, MUST BE ZERO */	/* 25: MUST BE ZERO */
};
const char rt_nmg_kind_names[NMG_N_KINDS+2][18] = {
    "model",				/* 0 */
    "nmgregion",
    "nmgregion_a",
    "shell",
    "shell_a",
    "faceuse",
    "face",
    "face_g_plane",
    "face_g_snurb",
    "loopuse",
    "loop",					/* 10 */
    "loop_g",
    "edgeuse",
    "edge",
    "edge_g_lseg",
    "edge_g_cnurb",
    "vertexuse",
    "vertexuse_a_plane",
    "vertexuse_a_cnurb",
    "vertex",
    "vertex_g",				/* 20 */
    "k21",
    "k22",
    "k23",
    "k24",
    "double_array",				/* 25 */
    "k26-OFF_END",
    "k27-OFF_END"
};


/**
 * Given the magic number for an NMG structure, return the
 * manifest constant which identifies that structure kind.
 */
HIDDEN int
rt_nmg_magic_to_kind(uint32_t magic)
{
    switch (magic) {
	case NMG_MODEL_MAGIC:
	    return NMG_KIND_MODEL;
	case NMG_REGION_MAGIC:
	    return NMG_KIND_NMGREGION;
	case NMG_REGION_A_MAGIC:
	    return NMG_KIND_NMGREGION_A;
	case NMG_SHELL_MAGIC:
	    return NMG_KIND_SHELL;
	case NMG_SHELL_A_MAGIC:
	    return NMG_KIND_SHELL_A;
	case NMG_FACEUSE_MAGIC:
	    return NMG_KIND_FACEUSE;
	case NMG_FACE_MAGIC:
	    return NMG_KIND_FACE;
	case NMG_FACE_G_PLANE_MAGIC:
	    return NMG_KIND_FACE_G_PLANE;
	case NMG_FACE_G_SNURB_MAGIC:
	    return NMG_KIND_FACE_G_SNURB;
	case NMG_LOOPUSE_MAGIC:
	    return NMG_KIND_LOOPUSE;
	case NMG_LOOP_G_MAGIC:
	    return NMG_KIND_LOOP_G;
	case NMG_LOOP_MAGIC:
	    return NMG_KIND_LOOP;
	case NMG_EDGEUSE_MAGIC:
	    return NMG_KIND_EDGEUSE;
	case NMG_EDGE_MAGIC:
	    return NMG_KIND_EDGE;
	case NMG_EDGE_G_LSEG_MAGIC:
	    return NMG_KIND_EDGE_G_LSEG;
	case NMG_EDGE_G_CNURB_MAGIC:
	    return NMG_KIND_EDGE_G_CNURB;
	case NMG_VERTEXUSE_MAGIC:
	    return NMG_KIND_VERTEXUSE;
	case NMG_VERTEXUSE_A_PLANE_MAGIC:
	    return NMG_KIND_VERTEXUSE_A_PLANE;
	case NMG_VERTEXUSE_A_CNURB_MAGIC:
	    return NMG_KIND_VERTEXUSE_A_CNURB;
	case NMG_VERTEX_MAGIC:
	    return NMG_KIND_VERTEX;
	case NMG_VERTEX_G_MAGIC:
	    return NMG_KIND_VERTEX_G;
    }
    /* default */
    bu_log("magic = x%x\n", magic);
    bu_bomb("rt_nmg_magic_to_kind: bad magic");
    return -1;
}


/* ---------------------------------------------------------------------- */

struct nmg_exp_counts {
    long new_subscript;
    long per_struct_index;
    int kind;
    long first_fastf_relpos;	/* for snurb and cnurb. */
    long byte_offset;		/* for snurb and cnurb. */
};


/* XXX These are horribly non-PARALLEL, and they *must* be PARALLEL ! */
static unsigned char *rt_nmg_fastf_p;
static unsigned int rt_nmg_cur_fastf_subscript;


/**
 * Format a variable sized array of fastf_t's into external format
 * (IEEE big endian double precision) with a 2 element header.
 *
 *		+-----------+
 *		|  magic    |
 *		+-----------+
 *		|  count    |
 *		+-----------+
 *		|           |
 *		~  doubles  ~
 *		~    :      ~
 *		|           |
 *		+-----------+
 *
 * Increments the pointer to the next free byte in the external array,
 * and increments the subscript number of the next free array.
 *
 * Note that this subscript number is consistent with the rest of the
 * NMG external subscript numbering, so that the first
 * disk_double_array subscript will be one larger than the largest
 * disk_vertex_g subscript, and in the external record the array of
 * fastf_t arrays will follow the array of disk_vertex_g structures.
 *
 * Returns subscript number of this array, in the external form.
 */
int
rt_nmg_export4_fastf(const fastf_t *fp, int count, int pt_type, double scale)


/* If zero, means literal array of values */

{
    int i;
    unsigned char *cp;

    /* always write doubles to disk */
    double *scanp;

    if (pt_type)
	count *= RT_NURB_EXTRACT_COORDS(pt_type);

    cp = rt_nmg_fastf_p;
    *(uint32_t *)&cp[0] = htonl(DISK_DOUBLE_ARRAY_MAGIC);
    *(uint32_t *)&cp[4] = htonl(count);
    if (pt_type == 0 || ZERO(scale - 1.0)) {
	scanp = (double *)bu_malloc(count * sizeof(double), "scanp");
	/* convert fastf_t to double */
	for (i=0; i<count; i++) {
	    scanp[i] = fp[i];
	}
	bu_cv_htond(cp + (4+4), (unsigned char *)scanp, count);
	bu_free(scanp, "scanp");
    } else {
	/* Need to scale data by 'scale' ! */
	scanp = (double *)bu_malloc(count*sizeof(double), "scanp");
	if (RT_NURB_IS_PT_RATIONAL(pt_type)) {
	    /* Don't scale the homogeneous (rational) coord */
	    int nelem;	/* # elements per tuple */

	    nelem = RT_NURB_EXTRACT_COORDS(pt_type);
	    for (i = 0; i < count; i += nelem) {
		VSCALEN(&scanp[i], &fp[i], scale, nelem-1);
		scanp[i+nelem-1] = fp[i+nelem-1];
	    }
	} else {
	    /* Scale everything as one long array */
	    VSCALEN(scanp, fp, scale, count);
	}
	bu_cv_htond(cp + (4+4), (unsigned char *)scanp, count);
	bu_free(scanp, "rt_nmg_export4_fastf");
    }
    cp += (4+4) + count * 8;
    rt_nmg_fastf_p = cp;
    return rt_nmg_cur_fastf_subscript++;
}


fastf_t *
rt_nmg_import4_fastf(const unsigned char *base, struct nmg_exp_counts *ecnt, long int subscript, const matp_t mat, int len, int pt_type)
{
    const unsigned char *cp;

    int i;
    int count;
    fastf_t *ret;

    /* must be double for import and export */
    double *tmp;
    double *scanp;

    if (ecnt[subscript].byte_offset <= 0 || ecnt[subscript].kind != NMG_KIND_DOUBLE_ARRAY) {
	bu_log("subscript=%ld, byte_offset=%ld, kind=%d (expected %d)\n",
	       subscript, ecnt[subscript].byte_offset,
	       ecnt[subscript].kind, NMG_KIND_DOUBLE_ARRAY);
	bu_bomb("rt_nmg_import4_fastf() bad ecnt table\n");
    }


    cp = base + ecnt[subscript].byte_offset;
    if (ntohl(*(uint32_t*)cp) != DISK_DOUBLE_ARRAY_MAGIC) {
	bu_log("magic mis-match, got x%x, s/b x%x, file %s, line %d\n",
	       ntohl(*(uint32_t*)cp), DISK_DOUBLE_ARRAY_MAGIC, __FILE__, __LINE__);
	bu_log("subscript=%ld, byte_offset=%ld\n",
	       subscript, ecnt[subscript].byte_offset);
	bu_bomb("rt_nmg_import4_fastf() bad magic\n");
    }

    if (pt_type)
	len *= RT_NURB_EXTRACT_COORDS(pt_type);

    count = ntohl(*(uint32_t*)(cp + 4));
    if (count != len || count < 0) {
	bu_log("rt_nmg_import4_fastf() subscript=%ld, expected len=%d, got=%d\n",
	       subscript, len, count);
	bu_bomb("rt_nmg_import4_fastf()\n");
    }
    ret = (fastf_t *)bu_malloc(count * sizeof(fastf_t), "rt_nmg_import4_fastf[]");
    if (!mat) {
	scanp = (double *)bu_malloc(count * sizeof(double), "scanp");
	bu_cv_ntohd((unsigned char *)scanp, cp + (4+4), count);
	/* read as double, return as fastf_t */
	for (i=0; i<count; i++) {
	    ret[i] = scanp[i];
	}
	bu_free(scanp, "scanp");
	return ret;
    }

    /*
     * An amazing amount of work: transform all points by 4x4 mat.
     * Need to know width of data points, may be 3, or 4-tuples.
     * The vector times matrix calculation can't be done in place.
     */
    tmp = (double *)bu_malloc(count * sizeof(double), "rt_nmg_import4_fastf tmp[]");
    bu_cv_ntohd((unsigned char *)tmp, cp + (4+4), count);

    switch (RT_NURB_EXTRACT_COORDS(pt_type)) {
	case 3:
	    if (RT_NURB_IS_PT_RATIONAL(pt_type)) bu_bomb("rt_nmg_import4_fastf() Rational 3-tuple?\n");
	    for (count -= 3; count >= 0; count -= 3) {
		MAT4X3PNT(&ret[count], mat, &tmp[count]);
	    }
	    break;
	case 4:
	    if (!RT_NURB_IS_PT_RATIONAL(pt_type)) bu_bomb("rt_nmg_import4_fastf() non-rational 4-tuple?\n");
	    for (count -= 4; count >= 0; count -= 4) {
		MAT4X4PNT(&ret[count], mat, &tmp[count]);
	    }
	    break;
	default:
	    bu_bomb("rt_nmg_import4_fastf() unsupported # of coords in ctl_point\n");
    }

    bu_free(tmp, "rt_nmg_import4_fastf tmp[]");

    return ret;
}


/**
 * Depends on ecnt[0].byte_offset having been set to maxindex.
 *
 * There are some special values for the disk index returned here:
 * >0 normal structure index.
 * 0 substitute a null pointer when imported.
 * -1 substitute pointer to within-struct list head when imported.
 */
HIDDEN int
reindex(void *p, struct nmg_exp_counts *ecnt)
{
    long idx;
    long ret=0;	/* zero is NOT the default value, this is just to satisfy cray compilers */

    /* If null pointer, return new subscript of zero */
    if (p == 0) {
	ret = 0;
	idx = 0;	/* sanity */
    } else {
	idx = nmg_index_of_struct((uint32_t *)(p));
	if (idx == -1) {
	    ret = DISK_INDEX_LISTHEAD; /* FLAG:  special list head */
	} else if (idx < -1) {
	    bu_bomb("reindex(): unable to obtain struct index\n");
	} else {
	    ret = ecnt[idx].new_subscript;
	    if (ecnt[idx].kind < 0) {
		bu_log("reindex(p=%p), p->index=%ld, ret=%ld, kind=%d\n", p, idx, ret, ecnt[idx].kind);
		bu_bomb("reindex() This index not found in ecnt[]\n");
	    }
	    /* ret == 0 on suppressed loop_g ptrs, etc. */
	    if (ret < 0 || ret > ecnt[0].byte_offset) {
		bu_log("reindex(p=%p) %s, p->index=%ld, ret=%ld, maxindex=%ld\n",
		       p,
		       bu_identify_magic(*(uint32_t *)p),
		       idx, ret, ecnt[0].byte_offset);
		bu_bomb("reindex() subscript out of range\n");
	    }
	}
    }
/*bu_log("reindex(p=x%x), p->index=%d, ret=%d\n", p, idx, ret);*/
    return ret;
}


/* forw may never be null;  back may be null for loopuse (sigh) */
#define INDEX(o, i, elem) *(uint32_t *)(o)->elem = htonl(reindex((void *)((i)->elem), ecnt))
#define INDEXL(oo, ii, elem) {						\
	uint32_t _f = reindex((void *)((ii)->elem.forw), ecnt);	\
	if (_f == DISK_INDEX_NULL) \
	    bu_log("INTERNAL WARNING: rt_nmg_edisk() reindex forw to null?\n"); \
	*(uint32_t *)((oo)->elem.forw) = htonl(_f);			\
	*(uint32_t *)((oo)->elem.back) = htonl(reindex((void *)((ii)->elem.back), ecnt)); }
#define PUTMAGIC(_magic) *(uint32_t *)d->magic = htonl(_magic)


/**
 * Export a given structure from memory to disk format
 *
 * Scale geometry by 'local2mm'
 */
HIDDEN void
rt_nmg_edisk(void *op, void *ip, struct nmg_exp_counts *ecnt, int idx, double local2mm)
/* base of disk array */
/* ptr to in-memory structure */


{
    int oindex;		/* index in op */

    oindex = ecnt[idx].per_struct_index;
    switch (ecnt[idx].kind) {
	case NMG_KIND_MODEL: {
	    struct model *m = (struct model *)ip;
	    struct disk_model *d;
	    d = &((struct disk_model *)op)[oindex];
	    NMG_CK_MODEL(m);
	    PUTMAGIC(DISK_MODEL_MAGIC);
	    *(uint32_t *)d->version = htonl(0);
	    INDEXL(d, m, r_hd);
	}
	    return;
	case NMG_KIND_NMGREGION: {
	    struct nmgregion *r = (struct nmgregion *)ip;
	    struct disk_nmgregion *d;
	    d = &((struct disk_nmgregion *)op)[oindex];
	    NMG_CK_REGION(r);
	    PUTMAGIC(DISK_REGION_MAGIC);
	    INDEXL(d, r, l);
	    INDEX(d, r, m_p);
	    INDEX(d, r, ra_p);
	    INDEXL(d, r, s_hd);
	}
	    return;
	case NMG_KIND_NMGREGION_A: {
	    struct nmgregion_a *r = (struct nmgregion_a *)ip;
	    struct disk_nmgregion_a *d;

	    /* must be double for import and export */
	    double min[ELEMENTS_PER_POINT];
	    double max[ELEMENTS_PER_POINT];

	    d = &((struct disk_nmgregion_a *)op)[oindex];
	    NMG_CK_REGION_A(r);
	    PUTMAGIC(DISK_REGION_A_MAGIC);
	    VSCALE(min, r->min_pt, local2mm);
	    VSCALE(max, r->max_pt, local2mm);
	    bu_cv_htond(d->min_pt, (unsigned char *)min, ELEMENTS_PER_POINT);
	    bu_cv_htond(d->max_pt, (unsigned char *)max, ELEMENTS_PER_POINT);
	}
	    return;
	case NMG_KIND_SHELL: {
	    struct shell *s = (struct shell *)ip;
	    struct disk_shell *d;
	    d = &((struct disk_shell *)op)[oindex];
	    NMG_CK_SHELL(s);
	    PUTMAGIC(DISK_SHELL_MAGIC);
	    INDEXL(d, s, l);
	    INDEX(d, s, r_p);
	    INDEX(d, s, sa_p);
	    INDEXL(d, s, fu_hd);
	    INDEXL(d, s, lu_hd);
	    INDEXL(d, s, eu_hd);
	    INDEX(d, s, vu_p);
	}
	    return;
	case NMG_KIND_SHELL_A: {
	    struct shell_a *sa = (struct shell_a *)ip;
	    struct disk_shell_a *d;

	    /* must be double for import and export */
	    double min[ELEMENTS_PER_POINT];
	    double max[ELEMENTS_PER_POINT];

	    d = &((struct disk_shell_a *)op)[oindex];
	    NMG_CK_SHELL_A(sa);
	    PUTMAGIC(DISK_SHELL_A_MAGIC);
	    VSCALE(min, sa->min_pt, local2mm);
	    VSCALE(max, sa->max_pt, local2mm);
	    bu_cv_htond(d->min_pt, (unsigned char *)min, ELEMENTS_PER_POINT);
	    bu_cv_htond(d->max_pt, (unsigned char *)max, ELEMENTS_PER_POINT);
	}
	    return;
	case NMG_KIND_FACEUSE: {
	    struct faceuse *fu = (struct faceuse *)ip;
	    struct disk_faceuse *d;
	    d = &((struct disk_faceuse *)op)[oindex];
	    NMG_CK_FACEUSE(fu);
	    NMG_CK_FACEUSE(fu->fumate_p);
	    NMG_CK_FACE(fu->f_p);
	    if (fu->f_p != fu->fumate_p->f_p) bu_log("faceuse export, differing faces\n");
	    PUTMAGIC(DISK_FACEUSE_MAGIC);
	    INDEXL(d, fu, l);
	    INDEX(d, fu, s_p);
	    INDEX(d, fu, fumate_p);
	    *(uint32_t *)d->orientation = htonl(fu->orientation);
	    INDEX(d, fu, f_p);
	    INDEXL(d, fu, lu_hd);
	}
	    return;
	case NMG_KIND_FACE: {
	    struct face *f = (struct face *)ip;
	    struct disk_face *d;
	    d = &((struct disk_face *)op)[oindex];
	    NMG_CK_FACE(f);
	    PUTMAGIC(DISK_FACE_MAGIC);
	    INDEXL(d, f, l);	/* face is member of fg list */
	    INDEX(d, f, fu_p);
	    *(uint32_t *)d->g = htonl(reindex((void *)(f->g.magic_p), ecnt));
	    *(uint32_t *)d->flip = htonl(f->flip);
	}
	    return;
	case NMG_KIND_FACE_G_PLANE: {
	    struct face_g_plane *fg = (struct face_g_plane *)ip;
	    struct disk_face_g_plane *d;

	    /* must be double for import and export */
	    double plane[ELEMENTS_PER_PLANE];

	    d = &((struct disk_face_g_plane *)op)[oindex];
	    NMG_CK_FACE_G_PLANE(fg);
	    PUTMAGIC(DISK_FACE_G_PLANE_MAGIC);
	    INDEXL(d, fg, f_hd);

	    /* convert fastf_t to double */
	    VMOVE(plane, fg->N);
	    plane[W] = fg->N[W] * local2mm;

	    bu_cv_htond(d->N, (unsigned char *)plane, ELEMENTS_PER_PLANE);
	}
	    return;
	case NMG_KIND_FACE_G_SNURB: {
	    struct face_g_snurb *fg = (struct face_g_snurb *)ip;
	    struct disk_face_g_snurb *d;

	    d = &((struct disk_face_g_snurb *)op)[oindex];
	    NMG_CK_FACE_G_SNURB(fg);
	    PUTMAGIC(DISK_FACE_G_SNURB_MAGIC);
	    INDEXL(d, fg, f_hd);
	    *(uint32_t *)d->u_order = htonl(fg->order[0]);
	    *(uint32_t *)d->v_order = htonl(fg->order[1]);
	    *(uint32_t *)d->u_size = htonl(fg->u.k_size);
	    *(uint32_t *)d->v_size = htonl(fg->v.k_size);
	    *(uint32_t *)d->u_knots = htonl(
		rt_nmg_export4_fastf(fg->u.knots,
				     fg->u.k_size, 0, 1.0));
	    *(uint32_t *)d->v_knots = htonl(
		rt_nmg_export4_fastf(fg->v.knots,
				     fg->v.k_size, 0, 1.0));
	    *(uint32_t *)d->us_size = htonl(fg->s_size[0]);
	    *(uint32_t *)d->vs_size = htonl(fg->s_size[1]);
	    *(uint32_t *)d->pt_type = htonl(fg->pt_type);
	    /* scale XYZ ctl_points by local2mm */
	    *(uint32_t *)d->ctl_points = htonl(
		rt_nmg_export4_fastf(fg->ctl_points,
				     fg->s_size[0] * fg->s_size[1],
				     fg->pt_type,
				     local2mm));
	}
	    return;
	case NMG_KIND_LOOPUSE: {
	    struct loopuse *lu = (struct loopuse *)ip;
	    struct disk_loopuse *d;
	    d = &((struct disk_loopuse *)op)[oindex];
	    NMG_CK_LOOPUSE(lu);
	    PUTMAGIC(DISK_LOOPUSE_MAGIC);
	    INDEXL(d, lu, l);
	    *(uint32_t *)d->up = htonl(reindex((void *)(lu->up.magic_p), ecnt));
	    INDEX(d, lu, lumate_p);
	    *(uint32_t *)d->orientation = htonl(lu->orientation);
	    INDEX(d, lu, l_p);
	    INDEXL(d, lu, down_hd);
	}
	    return;
	case NMG_KIND_LOOP: {
	    struct loop *loop = (struct loop *)ip;
	    struct disk_loop *d;
	    d = &((struct disk_loop *)op)[oindex];
	    NMG_CK_LOOP(loop);
	    PUTMAGIC(DISK_LOOP_MAGIC);
	    INDEX(d, loop, lu_p);
	    INDEX(d, loop, lg_p);
	}
	    return;
	case NMG_KIND_LOOP_G: {
	    struct loop_g *lg = (struct loop_g *)ip;
	    struct disk_loop_g *d;

	    /* must be double for import and export */
	    double min[ELEMENTS_PER_POINT];
	    double max[ELEMENTS_PER_POINT];

	    d = &((struct disk_loop_g *)op)[oindex];
	    NMG_CK_LOOP_G(lg);
	    PUTMAGIC(DISK_LOOP_G_MAGIC);

	    VSCALE(min, lg->min_pt, local2mm);
	    VSCALE(max, lg->max_pt, local2mm);

	    bu_cv_htond(d->min_pt, (unsigned char *)min, ELEMENTS_PER_POINT);
	    bu_cv_htond(d->max_pt, (unsigned char *)max, ELEMENTS_PER_POINT);
	}
	    return;
	case NMG_KIND_EDGEUSE: {
	    struct edgeuse *eu = (struct edgeuse *)ip;
	    struct disk_edgeuse *d;
	    d = &((struct disk_edgeuse *)op)[oindex];
	    NMG_CK_EDGEUSE(eu);
	    PUTMAGIC(DISK_EDGEUSE_MAGIC);
	    INDEXL(d, eu, l);
	    /* NOTE The pointers in l2 point at other l2's.
	     * nmg_index_of_struct() will point 'em back
	     * at the top of the edgeuse.  Beware on import.
	     */
	    INDEXL(d, eu, l2);
	    *(uint32_t *)d->up = htonl(reindex((void *)(eu->up.magic_p), ecnt));
	    INDEX(d, eu, eumate_p);
	    INDEX(d, eu, radial_p);
	    INDEX(d, eu, e_p);
	    *(uint32_t *)d->orientation = htonl(eu->orientation);
	    INDEX(d, eu, vu_p);
	    *(uint32_t *)d->g = htonl(reindex((void *)(eu->g.magic_p), ecnt));
	}
	    return;
	case NMG_KIND_EDGE: {
	    struct edge *e = (struct edge *)ip;
	    struct disk_edge *d;
	    d = &((struct disk_edge *)op)[oindex];
	    NMG_CK_EDGE(e);
	    PUTMAGIC(DISK_EDGE_MAGIC);
	    *(uint32_t *)d->is_real = htonl(e->is_real);
	    INDEX(d, e, eu_p);
	}
	    return;
	case NMG_KIND_EDGE_G_LSEG: {
	    struct edge_g_lseg *eg = (struct edge_g_lseg *)ip;
	    struct disk_edge_g_lseg *d;

	    /* must be double for import and export */
	    double pt[ELEMENTS_PER_POINT];
	    double dir[ELEMENTS_PER_VECT];

	    d = &((struct disk_edge_g_lseg *)op)[oindex];
	    NMG_CK_EDGE_G_LSEG(eg);
	    PUTMAGIC(DISK_EDGE_G_LSEG_MAGIC);
	    INDEXL(d, eg, eu_hd2);

	    /* convert fastf_t to double */
	    VSCALE(pt, eg->e_pt, local2mm);
	    VMOVE(dir, eg->e_dir);

	    bu_cv_htond(d->e_pt, (unsigned char *)pt, ELEMENTS_PER_POINT);
	    bu_cv_htond(d->e_dir, (unsigned char *)dir, ELEMENTS_PER_VECT);
	}
	    return;
	case NMG_KIND_EDGE_G_CNURB: {
	    struct edge_g_cnurb *eg = (struct edge_g_cnurb *)ip;
	    struct disk_edge_g_cnurb *d;
	    d = &((struct disk_edge_g_cnurb *)op)[oindex];
	    NMG_CK_EDGE_G_CNURB(eg);
	    PUTMAGIC(DISK_EDGE_G_CNURB_MAGIC);
	    INDEXL(d, eg, eu_hd2);
	    *(uint32_t *)d->order = htonl(eg->order);

	    /* If order is zero, everything else is NULL */
	    if (eg->order == 0) return;

	    *(uint32_t *)d->k_size = htonl(eg->k.k_size);
	    *(uint32_t *)d->knots = htonl(
		rt_nmg_export4_fastf(eg->k.knots,
				     eg->k.k_size, 0, 1.0));
	    *(uint32_t *)d->c_size = htonl(eg->c_size);
	    *(uint32_t *)d->pt_type = htonl(eg->pt_type);
	    /*
	     * The curve's control points are in parameter space
	     * for cnurbs on snurbs, and in XYZ for cnurbs on planar faces.
	     * UV values do NOT get transformed, XYZ values do!
	     */
	    *(uint32_t *)d->ctl_points = htonl(
		rt_nmg_export4_fastf(eg->ctl_points,
				     eg->c_size,
				     eg->pt_type,
				     RT_NURB_EXTRACT_PT_TYPE(eg->pt_type) == RT_NURB_PT_UV ?
				     1.0 : local2mm));
	}
	    return;
	case NMG_KIND_VERTEXUSE: {
	    struct vertexuse *vu = (struct vertexuse *)ip;
	    struct disk_vertexuse *d;
	    d = &((struct disk_vertexuse *)op)[oindex];
	    NMG_CK_VERTEXUSE(vu);
	    PUTMAGIC(DISK_VERTEXUSE_MAGIC);
	    INDEXL(d, vu, l);
	    *(uint32_t *)d->up = htonl(reindex((void *)(vu->up.magic_p), ecnt));
	    INDEX(d, vu, v_p);
	    if (vu->a.magic_p)NMG_CK_VERTEXUSE_A_EITHER(vu->a.magic_p);
	    *(uint32_t *)d->a = htonl(reindex((void *)(vu->a.magic_p), ecnt));
	}
	    return;
	case NMG_KIND_VERTEXUSE_A_PLANE: {
	    struct vertexuse_a_plane *vua = (struct vertexuse_a_plane *)ip;
	    struct disk_vertexuse_a_plane *d;

	    /* must be double for import and export */
	    double normal[ELEMENTS_PER_VECT];

	    d = &((struct disk_vertexuse_a_plane *)op)[oindex];
	    NMG_CK_VERTEXUSE_A_PLANE(vua);
	    PUTMAGIC(DISK_VERTEXUSE_A_PLANE_MAGIC);

	    /* Normal vectors don't scale */
	    /* This is not a plane equation here */
	    VMOVE(normal, vua->N); /* convert fastf_t to double */
	    bu_cv_htond(d->N, (unsigned char *)normal, ELEMENTS_PER_VECT);
	}
	    return;
	case NMG_KIND_VERTEXUSE_A_CNURB: {
	    struct vertexuse_a_cnurb *vua = (struct vertexuse_a_cnurb *)ip;
	    struct disk_vertexuse_a_cnurb *d;

	    /* must be double for import and export */
	    double param[3];

	    d = &((struct disk_vertexuse_a_cnurb *)op)[oindex];
	    NMG_CK_VERTEXUSE_A_CNURB(vua);
	    PUTMAGIC(DISK_VERTEXUSE_A_CNURB_MAGIC);

	    /* (u, v) parameters on curves don't scale */
	    VMOVE(param, vua->param); /* convert fastf_t to double */

	    bu_cv_htond(d->param, (unsigned char *)param, 3);
	}
	    return;
	case NMG_KIND_VERTEX: {
	    struct vertex *v = (struct vertex *)ip;
	    struct disk_vertex *d;
	    d = &((struct disk_vertex *)op)[oindex];
	    NMG_CK_VERTEX(v);
	    PUTMAGIC(DISK_VERTEX_MAGIC);
	    INDEXL(d, v, vu_hd);
	    INDEX(d, v, vg_p);
	}
	    return;
	case NMG_KIND_VERTEX_G: {
	    struct vertex_g *vg = (struct vertex_g *)ip;
	    struct disk_vertex_g *d;

	    /* must be double for import and export */
	    double pt[ELEMENTS_PER_POINT];

	    d = &((struct disk_vertex_g *)op)[oindex];
	    NMG_CK_VERTEX_G(vg);
	    PUTMAGIC(DISK_VERTEX_G_MAGIC);
	    VSCALE(pt, vg->coord, local2mm);

	    bu_cv_htond(d->coord, (unsigned char *)pt, ELEMENTS_PER_POINT);
	}
	    return;
    }
    bu_log("rt_nmg_edisk kind=%d unknown\n", ecnt[idx].kind);
}
#undef INDEX
#undef INDEXL

/*
 * For symmetry with export, use same macro names and arg ordering,
 * but here take from "o" (outboard) variable and put in "i" (internal).
 *
 * NOTE that the "< 0" test here is a comparison with DISK_INDEX_LISTHEAD.
 */
#define INDEX(o, i, ty, elem)	(i)->elem = (struct ty *)ptrs[ntohl(*(uint32_t*)((o)->elem))]
#define INDEXL_HD(oo, ii, elem, hd) { \
	int sub; \
	if ((sub = ntohl(*(uint32_t*)((oo)->elem.forw))) < 0) \
	    (ii)->elem.forw = &(hd); \
	else (ii)->elem.forw = (struct bu_list *)ptrs[sub]; \
	if ((sub = ntohl(*(uint32_t*)((oo)->elem.back))) < 0) \
	    (ii)->elem.back = &(hd); \
	else (ii)->elem.back = (struct bu_list *)ptrs[sub]; }

/* For use with the edgeuse l2 / edge_g eu2_hd secondary list */
/* The subscripts will point to the edgeuse, not the edgeuse's l2 rt_list */
#define INDEXL_HD2(oo, ii, elem, hd) { \
	int sub; \
	struct edgeuse *eu2; \
	if ((sub = ntohl(*(uint32_t*)((oo)->elem.forw))) < 0) { \
	    (ii)->elem.forw = &(hd); \
	} else { \
	    eu2 = (struct edgeuse *)ptrs[sub]; \
	    NMG_CK_EDGEUSE(eu2); \
	    (ii)->elem.forw = &eu2->l2; \
	} \
	if ((sub = ntohl(*(uint32_t*)((oo)->elem.back))) < 0) { \
	    (ii)->elem.back = &(hd); \
	} else { \
	    eu2 = (struct edgeuse *)ptrs[sub]; \
	    NMG_CK_EDGEUSE(eu2); \
	    (ii)->elem.back = &eu2->l2; \
	} }


/**
 * Import a given structure from disk to memory format.
 *
 * Transform geometry by given matrix.
 */
HIDDEN int
rt_nmg_idisk(void *op, void *ip, struct nmg_exp_counts *ecnt, int idx, uint32_t **ptrs, const fastf_t *mat, const unsigned char *basep)
/* ptr to in-memory structure */
/* base of disk array */


/* base of whole import record */
{
    int iindex;		/* index in ip */

    iindex = 0;
    switch (ecnt[idx].kind) {
	case NMG_KIND_MODEL: {
	    struct model *m = (struct model *)op;
	    struct disk_model *d;
	    d = &((struct disk_model *)ip)[iindex];
	    NMG_CK_MODEL(m);
	    NMG_CK_DISKMAGIC(d->magic, DISK_MODEL_MAGIC);
	    INDEXL_HD(d, m, r_hd, m->r_hd);
	}
	    return 0;
	case NMG_KIND_NMGREGION: {
	    struct nmgregion *r = (struct nmgregion *)op;
	    struct disk_nmgregion *d;
	    d = &((struct disk_nmgregion *)ip)[iindex];
	    NMG_CK_REGION(r);
	    NMG_CK_DISKMAGIC(d->magic, DISK_REGION_MAGIC);
	    INDEX(d, r, model, m_p);
	    INDEX(d, r, nmgregion_a, ra_p);
	    INDEXL_HD(d, r, s_hd, r->s_hd);
	    INDEXL_HD(d, r, l, r->m_p->r_hd); /* do after m_p */
	    NMG_CK_MODEL(r->m_p);
	}
	    return 0;
	case NMG_KIND_NMGREGION_A: {
	    struct nmgregion_a *r = (struct nmgregion_a *)op;
	    struct disk_nmgregion_a *d;
	    point_t min;
	    point_t max;

	    /* must be double for import and export */
	    double scanmin[ELEMENTS_PER_POINT];
	    double scanmax[ELEMENTS_PER_POINT];

	    d = &((struct disk_nmgregion_a *)ip)[iindex];
	    NMG_CK_REGION_A(r);
	    NMG_CK_DISKMAGIC(d->magic, DISK_REGION_A_MAGIC);
	    bu_cv_ntohd((unsigned char *)scanmin, d->min_pt, ELEMENTS_PER_POINT);
	    VMOVE(min, scanmin); /* convert double to fastf_t */
	    bu_cv_ntohd((unsigned char *)scanmax, d->max_pt, ELEMENTS_PER_POINT);
	    VMOVE(max, scanmax); /* convert double to fastf_t */
	    bn_rotate_bbox(r->min_pt, r->max_pt, mat, min, max);
	}
	    return 0;
	case NMG_KIND_SHELL: {
	    struct shell *s = (struct shell *)op;
	    struct disk_shell *d;
	    d = &((struct disk_shell *)ip)[iindex];
	    NMG_CK_SHELL(s);
	    NMG_CK_DISKMAGIC(d->magic, DISK_SHELL_MAGIC);
	    INDEX(d, s, nmgregion, r_p);
	    INDEX(d, s, shell_a, sa_p);
	    INDEXL_HD(d, s, fu_hd, s->fu_hd);
	    INDEXL_HD(d, s, lu_hd, s->lu_hd);
	    INDEXL_HD(d, s, eu_hd, s->eu_hd);
	    INDEX(d, s, vertexuse, vu_p);
	    NMG_CK_REGION(s->r_p);
	    INDEXL_HD(d, s, l, s->r_p->s_hd); /* after s->r_p */
	}
	    return 0;
	case NMG_KIND_SHELL_A: {
	    struct shell_a *sa = (struct shell_a *)op;
	    struct disk_shell_a *d;
	    point_t min;
	    point_t max;

	    /* must be double for import and export */
	    double scanmin[ELEMENTS_PER_POINT];
	    double scanmax[ELEMENTS_PER_POINT];

	    d = &((struct disk_shell_a *)ip)[iindex];
	    NMG_CK_SHELL_A(sa);
	    NMG_CK_DISKMAGIC(d->magic, DISK_SHELL_A_MAGIC);
	    bu_cv_ntohd((unsigned char *)scanmin, d->min_pt, ELEMENTS_PER_POINT);
	    VMOVE(min, scanmin); /* convert double to fastf_t */
	    bu_cv_ntohd((unsigned char *)scanmax, d->max_pt, ELEMENTS_PER_POINT);
	    VMOVE(max, scanmax); /* convert double to fastf_t */
	    bn_rotate_bbox(sa->min_pt, sa->max_pt, mat, min, max);
	}
	    return 0;
	case NMG_KIND_FACEUSE: {
	    struct faceuse *fu = (struct faceuse *)op;
	    struct disk_faceuse *d;
	    d = &((struct disk_faceuse *)ip)[iindex];
	    NMG_CK_FACEUSE(fu);
	    NMG_CK_DISKMAGIC(d->magic, DISK_FACEUSE_MAGIC);
	    INDEX(d, fu, shell, s_p);
	    INDEX(d, fu, faceuse, fumate_p);
	    fu->orientation = ntohl(*(uint32_t*)(d->orientation));
	    INDEX(d, fu, face, f_p);
	    INDEXL_HD(d, fu, lu_hd, fu->lu_hd);
	    INDEXL_HD(d, fu, l, fu->s_p->fu_hd); /* after fu->s_p */
	    NMG_CK_FACE(fu->f_p);
	    NMG_CK_FACEUSE(fu->fumate_p);
	}
	    return 0;
	case NMG_KIND_FACE: {
	    struct face *f = (struct face *)op;
	    struct disk_face *d;
	    int g_index;

	    d = &((struct disk_face *)ip)[iindex];
	    NMG_CK_FACE(f);
	    NMG_CK_DISKMAGIC(d->magic, DISK_FACE_MAGIC);
	    INDEX(d, f, faceuse, fu_p);
	    g_index = ntohl(*(uint32_t*)(d->g));
	    f->g.magic_p = (uint32_t *)ptrs[g_index];
	    f->flip = ntohl(*(uint32_t*)(d->flip));
	    /* Enroll this face on fg's list of users */
	    NMG_CK_FACE_G_EITHER(f->g.magic_p);
	    INDEXL_HD(d, f, l, f->g.plane_p->f_hd); /* after fu->fg_p set */
	    NMG_CK_FACEUSE(f->fu_p);
	}
	    return 0;
	case NMG_KIND_FACE_G_PLANE: {
	    struct face_g_plane *fg = (struct face_g_plane *)op;
	    struct disk_face_g_plane *d;
	    plane_t plane;

	    /* must be double for import and export */
	    double scan[ELEMENTS_PER_PLANE];

	    d = &((struct disk_face_g_plane *)ip)[iindex];
	    NMG_CK_FACE_G_PLANE(fg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_FACE_G_PLANE_MAGIC);
	    INDEXL_HD(d, fg, f_hd, fg->f_hd);
	    bu_cv_ntohd((unsigned char *)scan, d->N, ELEMENTS_PER_PLANE);
	    HMOVE(plane, scan); /* convert double to fastf_t */
	    bn_rotate_plane(fg->N, mat, plane);
	}
	    return 0;
	case NMG_KIND_FACE_G_SNURB: {
	    struct face_g_snurb *fg = (struct face_g_snurb *)op;
	    struct disk_face_g_snurb *d;
	    d = &((struct disk_face_g_snurb *)ip)[iindex];
	    NMG_CK_FACE_G_SNURB(fg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_FACE_G_SNURB_MAGIC);
	    INDEXL_HD(d, fg, f_hd, fg->f_hd);
	    fg->order[0] = ntohl(*(uint32_t*)(d->u_order));
	    fg->order[1] = ntohl(*(uint32_t*)(d->v_order));
	    fg->u.k_size = ntohl(*(uint32_t*)(d->u_size));
	    fg->u.knots = rt_nmg_import4_fastf(basep,
					       ecnt,
					       ntohl(*(uint32_t*)(d->u_knots)),
					       NULL,
					       fg->u.k_size,
					       0);
	    fg->v.k_size = ntohl(*(uint32_t*)(d->v_size));
	    fg->v.knots = rt_nmg_import4_fastf(basep,
					       ecnt,
					       ntohl(*(uint32_t*)(d->v_knots)),
					       NULL,
					       fg->v.k_size,
					       0);
	    fg->s_size[0] = ntohl(*(uint32_t*)(d->us_size));
	    fg->s_size[1] = ntohl(*(uint32_t*)(d->vs_size));
	    fg->pt_type = ntohl(*(uint32_t*)(d->pt_type));
	    /* Transform ctl_points by 'mat' */
	    fg->ctl_points = rt_nmg_import4_fastf(basep,
						  ecnt,
						  ntohl(*(uint32_t*)(d->ctl_points)),
						  (matp_t)mat,
						  fg->s_size[0] * fg->s_size[1],
						  fg->pt_type);
	}
	    return 0;
	case NMG_KIND_LOOPUSE: {
	    struct loopuse *lu = (struct loopuse *)op;
	    struct disk_loopuse *d;
	    int up_index;
	    int up_kind;

	    d = &((struct disk_loopuse *)ip)[iindex];
	    NMG_CK_LOOPUSE(lu);
	    NMG_CK_DISKMAGIC(d->magic, DISK_LOOPUSE_MAGIC);
	    up_index = ntohl(*(uint32_t*)(d->up));
	    lu->up.magic_p = ptrs[up_index];
	    INDEX(d, lu, loopuse, lumate_p);
	    lu->orientation = ntohl(*(uint32_t*)(d->orientation));
	    INDEX(d, lu, loop, l_p);
	    up_kind = ecnt[up_index].kind;
	    if (up_kind == NMG_KIND_FACEUSE) {
		INDEXL_HD(d, lu, l, lu->up.fu_p->lu_hd);
	    } else if (up_kind == NMG_KIND_SHELL) {
		INDEXL_HD(d, lu, l, lu->up.s_p->lu_hd);
	    } else bu_log("bad loopuse up, index=%d, kind=%d\n", up_index, up_kind);
	    INDEXL_HD(d, lu, down_hd, lu->down_hd);
	    if (lu->down_hd.forw == BU_LIST_NULL)
		bu_bomb("rt_nmg_idisk: null loopuse down_hd.forw\n");
	    NMG_CK_LOOP(lu->l_p);
	}
	    return 0;
	case NMG_KIND_LOOP: {
	    struct loop *loop = (struct loop *)op;
	    struct disk_loop *d;
	    d = &((struct disk_loop *)ip)[iindex];
	    NMG_CK_LOOP(loop);
	    NMG_CK_DISKMAGIC(d->magic, DISK_LOOP_MAGIC);
	    INDEX(d, loop, loopuse, lu_p);
	    INDEX(d, loop, loop_g, lg_p);
	    NMG_CK_LOOPUSE(loop->lu_p);
	}
	    return 0;
	case NMG_KIND_LOOP_G: {
	    struct loop_g *lg = (struct loop_g *)op;
	    struct disk_loop_g *d;
	    point_t min;
	    point_t max;

	    /* must be double for import and export */
	    double scanmin[ELEMENTS_PER_POINT];
	    double scanmax[ELEMENTS_PER_POINT];

	    d = &((struct disk_loop_g *)ip)[iindex];
	    NMG_CK_LOOP_G(lg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_LOOP_G_MAGIC);
	    bu_cv_ntohd((unsigned char *)scanmin, d->min_pt, ELEMENTS_PER_POINT);
	    VMOVE(min, scanmin); /* convert double to fastf_t */
	    bu_cv_ntohd((unsigned char *)scanmax, d->max_pt, ELEMENTS_PER_POINT);
	    VMOVE(max, scanmax); /* convert double to fastf_t */
	    bn_rotate_bbox(lg->min_pt, lg->max_pt, mat, min, max);
	}
	    return 0;
	case NMG_KIND_EDGEUSE: {
	    struct edgeuse *eu = (struct edgeuse *)op;
	    struct disk_edgeuse *d;
	    int up_index;
	    int up_kind;

	    d = &((struct disk_edgeuse *)ip)[iindex];
	    NMG_CK_EDGEUSE(eu);
	    NMG_CK_DISKMAGIC(d->magic, DISK_EDGEUSE_MAGIC);
	    up_index = ntohl(*(uint32_t*)(d->up));
	    eu->up.magic_p = ptrs[up_index];
	    INDEX(d, eu, edgeuse, eumate_p);
	    INDEX(d, eu, edgeuse, radial_p);
	    INDEX(d, eu, edge, e_p);
	    eu->orientation = ntohl(*(uint32_t*)(d->orientation));
	    INDEX(d, eu, vertexuse, vu_p);
	    up_kind = ecnt[up_index].kind;
	    if (up_kind == NMG_KIND_LOOPUSE) {
		INDEXL_HD(d, eu, l, eu->up.lu_p->down_hd);
	    } else if (up_kind == NMG_KIND_SHELL) {
		INDEXL_HD(d, eu, l, eu->up.s_p->eu_hd);
	    } else bu_log("bad edgeuse up, index=%d, kind=%d\n", up_index, up_kind);
	    eu->g.magic_p = ptrs[ntohl(*(uint32_t*)(d->g))];
	    NMG_CK_EDGE(eu->e_p);
	    NMG_CK_EDGEUSE(eu->eumate_p);
	    NMG_CK_EDGEUSE(eu->radial_p);
	    NMG_CK_VERTEXUSE(eu->vu_p);
	    if (eu->g.magic_p != NULL) {
		NMG_CK_EDGE_G_EITHER(eu->g.magic_p);

		/* Note that l2 subscripts will be for edgeuse, not l2 */
		/* g.lseg_p->eu_hd2 is a pun for g.cnurb_p->eu_hd2 also */
		INDEXL_HD2(d, eu, l2, eu->g.lseg_p->eu_hd2);
	    } else {
		eu->l2.forw = &eu->l2;
		eu->l2.back = &eu->l2;
	    }
	}
	    return 0;
	case NMG_KIND_EDGE: {
	    struct edge *e = (struct edge *)op;
	    struct disk_edge *d;
	    d = &((struct disk_edge *)ip)[iindex];
	    NMG_CK_EDGE(e);
	    NMG_CK_DISKMAGIC(d->magic, DISK_EDGE_MAGIC);
	    e->is_real = ntohl(*(uint32_t*)(d->is_real));
	    INDEX(d, e, edgeuse, eu_p);
	    NMG_CK_EDGEUSE(e->eu_p);
	}
	    return 0;
	case NMG_KIND_EDGE_G_LSEG: {
	    struct edge_g_lseg *eg = (struct edge_g_lseg *)op;
	    struct disk_edge_g_lseg *d;
	    point_t pt;
	    vect_t dir;

	    /* must be double for import and export */
	    double scanpt[ELEMENTS_PER_POINT];
	    double scandir[ELEMENTS_PER_VECT];

	    d = &((struct disk_edge_g_lseg *)ip)[iindex];
	    NMG_CK_EDGE_G_LSEG(eg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_EDGE_G_LSEG_MAGIC);
	    /* Forw subscript points to edgeuse, not edgeuse2 */
	    INDEXL_HD2(d, eg, eu_hd2, eg->eu_hd2);
	    bu_cv_ntohd((unsigned char *)scanpt, d->e_pt, ELEMENTS_PER_POINT);
	    VMOVE(pt, scanpt); /* convert double to fastf_t */
	    bu_cv_ntohd((unsigned char *)scandir, d->e_dir, ELEMENTS_PER_VECT);
	    VMOVE(dir, scandir); /* convert double to fastf_t */
	    MAT4X3PNT(eg->e_pt, mat, pt);
	    MAT4X3VEC(eg->e_dir, mat, dir);
	}
	    return 0;
	case NMG_KIND_EDGE_G_CNURB: {
	    struct edge_g_cnurb *eg = (struct edge_g_cnurb *)op;
	    struct disk_edge_g_cnurb *d;
	    d = &((struct disk_edge_g_cnurb *)ip)[iindex];
	    NMG_CK_EDGE_G_CNURB(eg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_EDGE_G_CNURB_MAGIC);
	    INDEXL_HD2(d, eg, eu_hd2, eg->eu_hd2);
	    eg->order = ntohl(*(uint32_t*)(d->order));

	    /* If order is zero, so is everything else */
	    if (eg->order == 0) return 0;

	    eg->k.k_size = ntohl(*(uint32_t*)(d->k_size));
	    eg->k.knots = rt_nmg_import4_fastf(basep,
					       ecnt,
					       ntohl(*(uint32_t*)(d->knots)),
					       NULL,
					       eg->k.k_size,
					       0);
	    eg->c_size = ntohl(*(uint32_t*)(d->c_size));
	    eg->pt_type = ntohl(*(uint32_t*)(d->pt_type));
	    /*
	     * The curve's control points are in parameter space.
	     * They do NOT get transformed!
	     */
	    if (RT_NURB_EXTRACT_PT_TYPE(eg->pt_type) == RT_NURB_PT_UV) {
		/* UV coords on snurb surface don't get xformed */
		eg->ctl_points = rt_nmg_import4_fastf(basep,
						      ecnt,
						      ntohl(*(uint32_t*)(d->ctl_points)),
						      NULL,
						      eg->c_size,
						      eg->pt_type);
	    } else {
		/* XYZ coords on planar face DO get xformed */
		eg->ctl_points = rt_nmg_import4_fastf(basep,
						      ecnt,
						      ntohl(*(uint32_t*)(d->ctl_points)),
						      (matp_t)mat,
						      eg->c_size,
						      eg->pt_type);
	    }
	}
	    return 0;
	case NMG_KIND_VERTEXUSE: {
	    struct vertexuse *vu = (struct vertexuse *)op;
	    struct disk_vertexuse *d;
	    d = &((struct disk_vertexuse *)ip)[iindex];
	    NMG_CK_VERTEXUSE(vu);
	    NMG_CK_DISKMAGIC(d->magic, DISK_VERTEXUSE_MAGIC);
	    vu->up.magic_p = ptrs[ntohl(*(uint32_t*)(d->up))];
	    INDEX(d, vu, vertex, v_p);
	    vu->a.magic_p = ptrs[ntohl(*(uint32_t*)(d->a))];
	    NMG_CK_VERTEX(vu->v_p);
	    if (vu->a.magic_p)NMG_CK_VERTEXUSE_A_EITHER(vu->a.magic_p);
	    INDEXL_HD(d, vu, l, vu->v_p->vu_hd);
	}
	    return 0;
	case NMG_KIND_VERTEXUSE_A_PLANE: {
	    struct vertexuse_a_plane *vua = (struct vertexuse_a_plane *)op;
	    struct disk_vertexuse_a_plane *d;
	    /* must be double for import and export */
	    double norm[ELEMENTS_PER_VECT];

	    d = &((struct disk_vertexuse_a_plane *)ip)[iindex];
	    NMG_CK_VERTEXUSE_A_PLANE(vua);
	    NMG_CK_DISKMAGIC(d->magic, DISK_VERTEXUSE_A_PLANE_MAGIC);
	    bu_cv_ntohd((unsigned char *)norm, d->N, ELEMENTS_PER_VECT);
	    MAT4X3VEC(vua->N, mat, norm);
	}
	    return 0;
	case NMG_KIND_VERTEXUSE_A_CNURB: {
	    struct vertexuse_a_cnurb *vua = (struct vertexuse_a_cnurb *)op;
	    struct disk_vertexuse_a_cnurb *d;

	    /* must be double for import and export */
	    double scan[3];

	    d = &((struct disk_vertexuse_a_cnurb *)ip)[iindex];
	    NMG_CK_VERTEXUSE_A_CNURB(vua);
	    NMG_CK_DISKMAGIC(d->magic, DISK_VERTEXUSE_A_CNURB_MAGIC);
	    /* These parameters are invariant w.r.t. 'mat' */
	    bu_cv_ntohd((unsigned char *)scan, d->param, 3);
	    VMOVE(vua->param, scan); /* convert double to fastf_t */
	}
	    return 0;
	case NMG_KIND_VERTEX: {
	    struct vertex *v = (struct vertex *)op;
	    struct disk_vertex *d;
	    d = &((struct disk_vertex *)ip)[iindex];
	    NMG_CK_VERTEX(v);
	    NMG_CK_DISKMAGIC(d->magic, DISK_VERTEX_MAGIC);
	    INDEXL_HD(d, v, vu_hd, v->vu_hd);
	    INDEX(d, v, vertex_g, vg_p);
	}
	    return 0;
	case NMG_KIND_VERTEX_G: {
	    struct vertex_g *vg = (struct vertex_g *)op;
	    struct disk_vertex_g *d;
	    /* must be double for import and export */
	    double pt[ELEMENTS_PER_POINT];

	    d = &((struct disk_vertex_g *)ip)[iindex];
	    NMG_CK_VERTEX_G(vg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_VERTEX_G_MAGIC);
	    bu_cv_ntohd((unsigned char *)pt, d->coord, ELEMENTS_PER_POINT);
	    MAT4X3PNT(vg->coord, mat, pt);
	}
	    return 0;
    }
    bu_log("rt_nmg_idisk kind=%d unknown\n", ecnt[idx].kind);
    return -1;
}


/**
 * Allocate storage for all the in-memory NMG structures, in
 * preparation for the importation operation, using the GET_xxx()
 * macros, so that m->maxindex, etc., are all appropriately handled.
 */
HIDDEN struct model *
rt_nmg_ialloc(uint32_t **ptrs, struct nmg_exp_counts *ecnt, int *kind_counts)
{
    struct model *m = (struct model *)0;
    int subscript;
    int kind;
    int j;

    subscript = 1;
    for (kind = 0; kind < NMG_N_KINDS; kind++) {
	if (kind == NMG_KIND_DOUBLE_ARRAY) continue;
	for (j = 0; j < kind_counts[kind]; j++) {
	    ecnt[subscript].kind = kind;
	    ecnt[subscript].per_struct_index = 0; /* unused on import */
	    switch (kind) {
		case NMG_KIND_MODEL:
		    if (m) bu_bomb("multiple models?");
		    m = nmg_mm();
		    /* Keep disk indices & new indices equal... */
		    m->maxindex++;
		    ptrs[subscript] = (uint32_t *)m;
		    break;
		case NMG_KIND_NMGREGION: {
		    struct nmgregion *r;
		    GET_REGION(r, m);
		    r->l.magic = NMG_REGION_MAGIC;
		    BU_LIST_INIT(&r->s_hd);
		    ptrs[subscript] = (uint32_t *)r;
		}
		    break;
		case NMG_KIND_NMGREGION_A: {
		    struct nmgregion_a *ra;
		    GET_REGION_A(ra, m);
		    ra->magic = NMG_REGION_A_MAGIC;
		    ptrs[subscript] = (uint32_t *)ra;
		}
		    break;
		case NMG_KIND_SHELL: {
		    struct shell *s;
		    GET_SHELL(s, m);
		    s->l.magic = NMG_SHELL_MAGIC;
		    BU_LIST_INIT(&s->fu_hd);
		    BU_LIST_INIT(&s->lu_hd);
		    BU_LIST_INIT(&s->eu_hd);
		    ptrs[subscript] = (uint32_t *)s;
		}
		    break;
		case NMG_KIND_SHELL_A: {
		    struct shell_a *sa;
		    GET_SHELL_A(sa, m);
		    sa->magic = NMG_SHELL_A_MAGIC;
		    ptrs[subscript] = (uint32_t *)sa;
		}
		    break;
		case NMG_KIND_FACEUSE: {
		    struct faceuse *fu;
		    GET_FACEUSE(fu, m);
		    fu->l.magic = NMG_FACEUSE_MAGIC;
		    BU_LIST_INIT(&fu->lu_hd);
		    ptrs[subscript] = (uint32_t *)fu;
		}
		    break;
		case NMG_KIND_FACE: {
		    struct face *f;
		    GET_FACE(f, m);
		    f->l.magic = NMG_FACE_MAGIC;
		    ptrs[subscript] = (uint32_t *)f;
		}
		    break;
		case NMG_KIND_FACE_G_PLANE: {
		    struct face_g_plane *fg;
		    GET_FACE_G_PLANE(fg, m);
		    fg->magic = NMG_FACE_G_PLANE_MAGIC;
		    BU_LIST_INIT(&fg->f_hd);
		    ptrs[subscript] = (uint32_t *)fg;
		}
		    break;
		case NMG_KIND_FACE_G_SNURB: {
		    struct face_g_snurb *fg;
		    GET_FACE_G_SNURB(fg, m);
		    fg->l.magic = NMG_FACE_G_SNURB_MAGIC;
		    BU_LIST_INIT(&fg->f_hd);
		    ptrs[subscript] = (uint32_t *)fg;
		}
		    break;
		case NMG_KIND_LOOPUSE: {
		    struct loopuse *lu;
		    GET_LOOPUSE(lu, m);
		    lu->l.magic = NMG_LOOPUSE_MAGIC;
		    BU_LIST_INIT(&lu->down_hd);
		    ptrs[subscript] = (uint32_t *)lu;
		}
		    break;
		case NMG_KIND_LOOP: {
		    struct loop *l;
		    GET_LOOP(l, m);
		    l->magic = NMG_LOOP_MAGIC;
		    ptrs[subscript] = (uint32_t *)l;
		}
		    break;
		case NMG_KIND_LOOP_G: {
		    struct loop_g *lg;
		    GET_LOOP_G(lg, m);
		    lg->magic = NMG_LOOP_G_MAGIC;
		    ptrs[subscript] = (uint32_t *)lg;
		}
		    break;
		case NMG_KIND_EDGEUSE: {
		    struct edgeuse *eu;
		    GET_EDGEUSE(eu, m);
		    eu->l.magic = NMG_EDGEUSE_MAGIC;
		    eu->l2.magic = NMG_EDGEUSE2_MAGIC;
		    ptrs[subscript] = (uint32_t *)eu;
		}
		    break;
		case NMG_KIND_EDGE: {
		    struct edge *e;
		    GET_EDGE(e, m);
		    e->magic = NMG_EDGE_MAGIC;
		    ptrs[subscript] = (uint32_t *)e;
		}
		    break;
		case NMG_KIND_EDGE_G_LSEG: {
		    struct edge_g_lseg *eg;
		    GET_EDGE_G_LSEG(eg, m);
		    eg->l.magic = NMG_EDGE_G_LSEG_MAGIC;
		    BU_LIST_INIT(&eg->eu_hd2);
		    ptrs[subscript] = (uint32_t *)eg;
		}
		    break;
		case NMG_KIND_EDGE_G_CNURB: {
		    struct edge_g_cnurb *eg;
		    GET_EDGE_G_CNURB(eg, m);
		    eg->l.magic = NMG_EDGE_G_CNURB_MAGIC;
		    BU_LIST_INIT(&eg->eu_hd2);
		    ptrs[subscript] = (uint32_t *)eg;
		}
		    break;
		case NMG_KIND_VERTEXUSE: {
		    struct vertexuse *vu;
		    GET_VERTEXUSE(vu, m);
		    vu->l.magic = NMG_VERTEXUSE_MAGIC;
		    ptrs[subscript] = (uint32_t *)vu;
		}
		    break;
		case NMG_KIND_VERTEXUSE_A_PLANE: {
		    struct vertexuse_a_plane *vua;
		    GET_VERTEXUSE_A_PLANE(vua, m);
		    vua->magic = NMG_VERTEXUSE_A_PLANE_MAGIC;
		    ptrs[subscript] = (uint32_t *)vua;
		}
		    break;
		case NMG_KIND_VERTEXUSE_A_CNURB: {
		    struct vertexuse_a_cnurb *vua;
		    GET_VERTEXUSE_A_CNURB(vua, m);
		    vua->magic = NMG_VERTEXUSE_A_CNURB_MAGIC;
		    ptrs[subscript] = (uint32_t *)vua;
		}
		    break;
		case NMG_KIND_VERTEX: {
		    struct vertex *v;
		    GET_VERTEX(v, m);
		    v->magic = NMG_VERTEX_MAGIC;
		    BU_LIST_INIT(&v->vu_hd);
		    ptrs[subscript] = (uint32_t *)v;
		}
		    break;
		case NMG_KIND_VERTEX_G: {
		    struct vertex_g *vg;
		    GET_VERTEX_G(vg, m);
		    vg->magic = NMG_VERTEX_G_MAGIC;
		    ptrs[subscript] = (uint32_t *)vg;
		}
		    break;
		default:
		    bu_log("bad kind = %d\n", kind);
		    ptrs[subscript] = (uint32_t *)0;
		    break;
	    }

	    /* new_subscript unused on import except for printf()s */
	    ecnt[subscript].new_subscript = nmg_index_of_struct(ptrs[subscript]);
	    subscript++;
	}
    }
    return m;
}


/**
 * Find the locations of all the variable-sized fastf_t arrays in the
 * input record.  Record that position as a byte offset from the very
 * front of the input record in ecnt[], indexed by subscript number.
 *
 * No storage is allocated here, that will be done by
 * rt_nmg_import4_fastf() on the fly.  A separate call to bu_malloc()
 * will be used, so that nmg_keg(), etc., can kill each array as
 * appropriate.
 */
HIDDEN void
rt_nmg_i2alloc(struct nmg_exp_counts *ecnt, unsigned char *cp, int *kind_counts)
{
    int kind;
    int nkind;
    int subscript;
    int offset;
    int i;

    nkind = kind_counts[NMG_KIND_DOUBLE_ARRAY];
    if (nkind <= 0) return;

    /* First, find the beginning of the fastf_t arrays */
    subscript = 1;
    offset = 0;
    for (kind = 0; kind < NMG_N_KINDS; kind++) {
	if (kind == NMG_KIND_DOUBLE_ARRAY) continue;
	offset += rt_nmg_disk_sizes[kind] * kind_counts[kind];
	subscript += kind_counts[kind];
    }

    /* Should have found the first one now */
    NMG_CK_DISKMAGIC(cp + offset, DISK_DOUBLE_ARRAY_MAGIC);
    for (i=0; i < nkind; i++) {
	int ndouble;
	NMG_CK_DISKMAGIC(cp + offset, DISK_DOUBLE_ARRAY_MAGIC);
	ndouble = ntohl(*(uint32_t*)(cp + offset + 4));
	ecnt[subscript].kind = NMG_KIND_DOUBLE_ARRAY;
	/* Stored byte offset is from beginning of disk record */
	ecnt[subscript].byte_offset = offset;
	offset += (4+4) + 8*ndouble;
	subscript++;
    }
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
HIDDEN int
rt_nmg_import4_internal(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, int rebound, const struct bn_tol *tol)
{
    struct model *m;
    union record *rp;
    int kind_counts[NMG_N_KINDS];
    unsigned char *cp;
    uint32_t **real_ptrs;
    uint32_t **ptrs;
    struct nmg_exp_counts *ecnt;
    int i;
    int maxindex;
    int kind;
    static uint32_t bad_magic = 0x999;

    BU_CK_EXTERNAL(ep);
    BN_CK_TOL(tol);
    rp = (union record *)ep->ext_buf;
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

    /* Obtain counts of each kind of structure */
    maxindex = 1;
    for (kind = 0; kind < NMG_N_KINDS; kind++) {
	kind_counts[kind] = ntohl(*(uint32_t*)(rp->nmg.N_structs+4*kind));
	maxindex += kind_counts[kind];
    }

    /* Collect overall new subscripts, and structure-specific indices */
    ecnt = (struct nmg_exp_counts *)bu_calloc(maxindex+3,
					      sizeof(struct nmg_exp_counts), "ecnt[]");
    real_ptrs = (uint32_t **)bu_calloc(maxindex+3,
				       sizeof(uint32_t *), "ptrs[]");
    /* So that indexing [-1] gives an appropriately bogus magic # */
    ptrs = real_ptrs+1;
    ptrs[-1] = &bad_magic;		/* [-1] gives bad magic */
    ptrs[0] = NULL;	/* [0] gives NULL */
    ptrs[maxindex] = &bad_magic;	/* [maxindex] gives bad magic */
    ptrs[maxindex+1] = &bad_magic;	/* [maxindex+1] gives bad magic */

    /* Allocate storage for all the NMG structs, in ptrs[] */
    m = rt_nmg_ialloc(ptrs, ecnt, kind_counts);

    /* Locate the variably sized fastf_t arrays.  ecnt[] has room. */
    cp = (unsigned char *)(rp+1);	/* start at first granule in */
    rt_nmg_i2alloc(ecnt, cp, kind_counts);

    /* Import each structure, in turn */
    for (i=1; i < maxindex; i++) {
	/* If we made it to the last kind, stop.  Nothing follows */
	if (ecnt[i].kind == NMG_KIND_DOUBLE_ARRAY) break;
	if (rt_nmg_idisk((void *)(ptrs[i]), (void *)cp,
			 ecnt, i, ptrs, mat, (unsigned char *)(rp+1)) < 0)
	    return -1;	/* FAIL */
	cp += rt_nmg_disk_sizes[ecnt[i].kind];
    }

    if (rebound) {
	/* Recompute all bounding boxes in model */
	nmg_rebound(m, tol);
    } else {
	/*
	 * Need to recompute bounding boxes for the faces here.
	 * Other bounding boxes will exist and be intact if NMG
	 * exporter wrote the _a structures.
	 */
	for (i=1; i < maxindex; i++) {
	    if (ecnt[i].kind != NMG_KIND_FACE) continue;
	    nmg_face_bb((struct face *)ptrs[i], tol);
	}
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_NMG;
    ip->idb_meth = &OBJ[ID_NMG];
    ip->idb_ptr = (void *)m;

    bu_free((char *)ecnt, "ecnt[]");
    bu_free((char *)real_ptrs, "ptrs[]");

    return 0;			/* OK */
}


/**
 * The name is added by the caller, in the usual place.
 *
 * When the "compact" flag is set, bounding boxes from (at present)
 * nmgregion_a
 * shell_a
 * loop_g
 * are not converted for storage in the database.
 * They should be re-generated at import time.
 *
 * If the "compact" flag is not set, then the NMG model is saved,
 * verbatim.
 *
 * The overall layout of the on-disk NMG is like this:
 *
 *	+---------------------------+
 *	|  NMG header granule       |
 *	|    solid name             |
 *	|    # additional granules  |
 *	|    format version         |
 *	|    kind_count[] array     |
 *	+---------------------------+
 *	|                           |
 *	|                           |
 *	~     N_count granules      ~
 *	~              :            ~
 *	|              :            |
 *	|                           |
 *	+---------------------------+
 *
 * In the additional granules, all structures of "kind" 0 (model) go
 * first, followed by all structures of kind 1 (nmgregion), etc.  As
 * each structure is output, it is assigned a subscript number,
 * starting with #1 for the model structure.  All pointers are
 * converted to the matching subscript numbers.  An on-disk subscript
 * of zero indicates a corresponding NULL pointer in memory.  All
 * integers are converted to network (Big-Endian) byte order.  All
 * floating point values are stored in network (Big-Endian IEEE)
 * format.
 */
HIDDEN int
rt_nmg_export4_internal(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, int compact)
{
    struct model *m;
    union record *rp;
    struct nmg_struct_counts cntbuf;
    uint32_t **ptrs;
    struct nmg_exp_counts *ecnt;
    int i;
    int subscript;
    int kind_counts[NMG_N_KINDS];
    void *disk_arrays[NMG_N_KINDS];
    int additional_grans;
    int tot_size;
    int kind;
    char *cp;
    int double_count;
    int fastf_byte_count;

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_NMG) return -1;
    m = (struct model *)ip->idb_ptr;
    NMG_CK_MODEL(m);

    /* As a by-product, this fills in the ptrs[] array! */
    memset((char *)&cntbuf, 0, sizeof(cntbuf));
    ptrs = nmg_m_struct_count(&cntbuf, m);

    /* Collect overall new subscripts, and structure-specific indices */
    ecnt = (struct nmg_exp_counts *)bu_calloc(m->maxindex+1,
					      sizeof(struct nmg_exp_counts), "ecnt[]");
    for (i = 0; i < NMG_N_KINDS; i++)
	kind_counts[i] = 0;
    subscript = 1;		/* must be larger than DISK_INDEX_NULL */
    double_count = 0;
    fastf_byte_count = 0;
    for (i=0; i < m->maxindex; i++) {
	if (ptrs[i] == NULL) {
	    ecnt[i].kind = -1;
	    continue;
	}
	kind = rt_nmg_magic_to_kind(*(ptrs[i]));
	ecnt[i].per_struct_index = kind_counts[kind]++;
	ecnt[i].kind = kind;
	/* Handle the variable sized kinds */
	switch (kind) {
	    case NMG_KIND_FACE_G_SNURB: {
		struct face_g_snurb *fg;
		int ndouble;
		fg = (struct face_g_snurb *)ptrs[i];
		ecnt[i].first_fastf_relpos = kind_counts[NMG_KIND_DOUBLE_ARRAY];
		kind_counts[NMG_KIND_DOUBLE_ARRAY] += 3;
		ndouble =  fg->u.k_size +
		    fg->v.k_size +
		    fg->s_size[0] * fg->s_size[1] *
		    RT_NURB_EXTRACT_COORDS(fg->pt_type);
		double_count += ndouble;
		ecnt[i].byte_offset = fastf_byte_count;
		fastf_byte_count += 3*(4+4) + 8*ndouble;
	    }
		break;
	    case NMG_KIND_EDGE_G_CNURB: {
		struct edge_g_cnurb *eg;
		int ndouble;
		eg = (struct edge_g_cnurb *)ptrs[i];
		ecnt[i].first_fastf_relpos = kind_counts[NMG_KIND_DOUBLE_ARRAY];
		/* If order is zero, no knots or ctl_points */
		if (eg->order == 0) break;
		kind_counts[NMG_KIND_DOUBLE_ARRAY] += 2;
		ndouble = eg->k.k_size + eg->c_size *
		    RT_NURB_EXTRACT_COORDS(eg->pt_type);
		double_count += ndouble;
		ecnt[i].byte_offset = fastf_byte_count;
		fastf_byte_count += 2*(4+4) + 8*ndouble;
	    }
		break;
	}
    }
    if (compact) {
	kind_counts[NMG_KIND_NMGREGION_A] = 0;
	kind_counts[NMG_KIND_SHELL_A] = 0;
	kind_counts[NMG_KIND_LOOP_G] = 0;
    }

    /* Assign new subscripts to ascending guys of same kind */
    for (kind = 0; kind < NMG_N_KINDS; kind++) {
	if (compact && (kind == NMG_KIND_NMGREGION_A ||
			kind == NMG_KIND_SHELL_A ||
			kind == NMG_KIND_LOOP_G)) {
	    /*
	     * Don't assign any new subscripts for them.
	     * Instead, use DISK_INDEX_NULL, yielding null ptrs.
	     */
	    for (i=0; i < m->maxindex; i++) {
		if (ptrs[i] == NULL) continue;
		if (ecnt[i].kind != kind) continue;
		ecnt[i].new_subscript = DISK_INDEX_NULL;
	    }
	    continue;
	}
	for (i=0; i < m->maxindex; i++) {
	    if (ptrs[i] == NULL) continue;
	    if (ecnt[i].kind != kind) continue;
	    ecnt[i].new_subscript = subscript++;
	}
    }
    /* Tack on the variable sized fastf_t arrays at the end */
    rt_nmg_cur_fastf_subscript = subscript;
    subscript += kind_counts[NMG_KIND_DOUBLE_ARRAY];

    /* Sanity checking */
    for (i=0; i < m->maxindex; i++) {
	if (ptrs[i] == NULL) continue;
	if (nmg_index_of_struct(ptrs[i]) != i) {
	    bu_log("***ERROR, ptrs[%d]->index = %d\n",
		   i, nmg_index_of_struct((uint32_t *)ptrs[i]));
	}
	if (rt_nmg_magic_to_kind(*ptrs[i]) != ecnt[i].kind) {
	    bu_log("@@@ERROR, ptrs[%d] kind(%d) != %d\n",
		   i, rt_nmg_magic_to_kind(*ptrs[i]),
		   ecnt[i].kind);
	}
    }

    tot_size = 0;
    for (i = 0; i < NMG_N_KINDS; i++) {
	if (kind_counts[i] <= 0) {
	    disk_arrays[i] = ((void *)0);
	    continue;
	}
	tot_size += kind_counts[i] * rt_nmg_disk_sizes[i];
    }
    /* Account for variable sized double arrays, at the end */
    tot_size += kind_counts[NMG_KIND_DOUBLE_ARRAY] * (4+4) +
	double_count * 8;

    ecnt[0].byte_offset = subscript; /* implicit arg to reindex() */

    additional_grans = (tot_size + sizeof(union record)-1) / sizeof(union record);
    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = (1 + additional_grans) * sizeof(union record);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "nmg external");
    rp = (union record *)ep->ext_buf;
    rp->nmg.N_id = DBID_NMG;
    rp->nmg.N_version = DISK_MODEL_VERSION;
    *(uint32_t *)rp->nmg.N_count = htonl((uint32_t)additional_grans);

    /* Record counts of each kind of structure */
    for (kind = 0; kind < NMG_N_KINDS; kind++) {
	*(uint32_t *)(rp->nmg.N_structs+4*kind) = htonl(kind_counts[kind]);
    }

    cp = (char *)(rp+1);	/* advance one granule */
    for (i=0; i < NMG_N_KINDS; i++) {
	disk_arrays[i] = (void *)cp;
	cp += kind_counts[i] * rt_nmg_disk_sizes[i];
    }
    /* disk_arrays[NMG_KIND_DOUBLE_ARRAY] is set properly because it is last */
    rt_nmg_fastf_p = (unsigned char *)disk_arrays[NMG_KIND_DOUBLE_ARRAY];

    /* Convert all the structures to their disk versions */
    for (i = m->maxindex-1; i >= 0; i--) {
	if (ptrs[i] == NULL) continue;
	kind = ecnt[i].kind;
	if (kind_counts[kind] <= 0) continue;
	rt_nmg_edisk((void *)(disk_arrays[kind]),
		     (void *)(ptrs[i]), ecnt, i, local2mm);
    }

    bu_free((void *)ptrs, "ptrs[]");
    bu_free((void *)ecnt, "ecnt[]");

    return 0;
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
    struct bn_tol tol;

    BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);

    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != DBID_NMG) {
	bu_log("rt_nmg_import4: defective record\n");
	return -1;
    }

    /* XXX The bounding box routines need a tolerance.
     * XXX This is sheer guesswork here.
     * As long as this NMG is going to be turned into vlist, or
     * handed off to the boolean evaluator, any non-zero numbers are fine.
     */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    if (rt_nmg_import4_internal(ip, ep, mat, 1, &tol) < 0)
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
    struct model *m;
    struct bn_tol tol;
    int maxindex;
    int kind;
    int kind_counts[NMG_N_KINDS];
    unsigned char *dp;		/* data pointer */
    void *startdata;	/* data pointer */
    uint32_t **real_ptrs;
    uint32_t **ptrs;
    struct nmg_exp_counts *ecnt;
    int i;
    static uint32_t bad_magic = 0x999;

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    dp = (unsigned char *)ep->ext_buf;

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    {
	int version;
	version = ntohl(*(uint32_t*)dp);
	dp+= SIZEOF_NETWORK_LONG;
	if (version != DISK_MODEL_VERSION) {
	    bu_log("rt_nmg_import4: expected NMG '.g' format version %d, got %d, aborting nmg solid import\n",
		   DISK_MODEL_VERSION, version);
	    return -1;
	}
    }
    maxindex = 1;
    for (kind =0; kind < NMG_N_KINDS; kind++) {
	kind_counts[kind] = ntohl(*(uint32_t*)dp);
	dp+= SIZEOF_NETWORK_LONG;
	maxindex += kind_counts[kind];
    }

    startdata = dp;

    /* Collect overall new subscripts, and structure-specific indices */
    ecnt = (struct nmg_exp_counts *) bu_calloc(maxindex+3,
					       sizeof(struct nmg_exp_counts), "ecnt[]");
    real_ptrs = (uint32_t **)bu_calloc(maxindex+3, sizeof(uint32_t *), "ptrs[]");
    /* some safety checking.  Indexing by, -1, 0, n+1, N+2 give interesting results */
    ptrs = real_ptrs+1;
    ptrs[-1] = &bad_magic;
    ptrs[0] = NULL;
    ptrs[maxindex] = &bad_magic;
    ptrs[maxindex+1] = &bad_magic;

    m = rt_nmg_ialloc(ptrs, ecnt, kind_counts);

    rt_nmg_i2alloc(ecnt, dp, kind_counts);

    /* Now import each structure, in turn */
    for (i=1; i < maxindex; i++) {
	/* We know that the DOUBLE_ARRAY is the last thing to process */
	if (ecnt[i].kind == NMG_KIND_DOUBLE_ARRAY) break;
	if (rt_nmg_idisk((void *)(ptrs[i]), (void *)dp, ecnt,
			 i, ptrs, mat, (unsigned char *)startdata) < 0) {
	    return -1;
	}
	dp += rt_nmg_disk_sizes[ecnt[i].kind];
    }

    /* Face min_pt and max_pt are not stored, so this is mandatory. */
    nmg_rebound(m, &tol);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_NMG;
    ip->idb_meth = &OBJ[ ID_NMG ];
    ip->idb_ptr = (void *)m;
    NMG_CK_MODEL(m);
    bu_free((char *)ecnt, "ecnt[]");
    bu_free((char *)real_ptrs, "ptrs[]");

    if (RT_G_DEBUG || nmg_debug) {
	nmg_vmodel(m);
    }
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
    return rt_nmg_export4_internal(ep, ip, local2mm, 1);
}


int
rt_nmg_export5(
    struct bu_external *ep,
    const struct rt_db_internal *ip,
    double local2mm,
    const struct db_i *dbip)
{
    struct model *m;
    unsigned char *dp;
    uint32_t **ptrs;
    struct nmg_struct_counts cntbuf;
    struct nmg_exp_counts *ecnt;
    int kind_counts[NMG_N_KINDS];
    void *disk_arrays[NMG_N_KINDS];
    int tot_size;
    int kind;
    int double_count;
    int i;
    int subscript, fastf_byte_count;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_NMG) return -1;
    m = (struct model *)ip->idb_ptr;
    NMG_CK_MODEL(m);

    memset((char *)&cntbuf, 0, sizeof(cntbuf));
    ptrs = nmg_m_struct_count(&cntbuf, m);

    ecnt = (struct nmg_exp_counts *)bu_calloc(m->maxindex+1,
					      sizeof(struct nmg_exp_counts), "ecnt[]");
    for (i=0; i<NMG_N_KINDS; i++) {
	kind_counts[i] = 0;
    }
    subscript = 1;
    double_count = 0;
    fastf_byte_count = 0;
    for (i=0; i< m->maxindex; i++) {
	if (ptrs[i] == NULL) {
	    ecnt[i].kind = -1;
	    continue;
	}

	kind = rt_nmg_magic_to_kind(*(ptrs[i]));
	ecnt[i].per_struct_index = kind_counts[kind]++;
	ecnt[i].kind = kind;

	/*
	 * SNURB and CNURBS are variable sized and as such need
	 * special handling
	 */
	if (kind == NMG_KIND_FACE_G_SNURB) {
	    struct face_g_snurb *fg;
	    int ndouble;
	    fg = (struct face_g_snurb *)ptrs[i];
	    ecnt[i].first_fastf_relpos = kind_counts[NMG_KIND_DOUBLE_ARRAY];
	    kind_counts[NMG_KIND_DOUBLE_ARRAY] += 3;
	    ndouble = fg->u.k_size +
		fg->v.k_size +
		fg->s_size[0] * fg->s_size[1] *
		RT_NURB_EXTRACT_COORDS(fg->pt_type);
	    double_count += ndouble;
	    ecnt[i].byte_offset = fastf_byte_count;
	    fastf_byte_count += 3*(4*4) + 89*ndouble;
	} else if (kind == NMG_KIND_EDGE_G_CNURB) {
	    struct edge_g_cnurb *eg;
	    int ndouble;
	    eg = (struct edge_g_cnurb *)ptrs[i];
	    ecnt[i].first_fastf_relpos =
		kind_counts[NMG_KIND_DOUBLE_ARRAY];
	    if (eg->order != 0) {
		kind_counts[NMG_KIND_DOUBLE_ARRAY] += 2;
		ndouble = eg->k.k_size +eg->c_size *
		    RT_NURB_EXTRACT_COORDS(eg->pt_type);
		double_count += ndouble;
		ecnt[i].byte_offset = fastf_byte_count;
		fastf_byte_count += 2*(4+4) + 8*ndouble;
	    }
	}
    }
    /* Compacting wanted */
    kind_counts[NMG_KIND_NMGREGION_A] = 0;
    kind_counts[NMG_KIND_SHELL_A] = 0;
    kind_counts[NMG_KIND_LOOP_G] = 0;

    /* Assign new subscripts to ascending struts of the same kind */
    for (kind=0; kind < NMG_N_KINDS; kind++) {
	/* Compacting */
	if (kind == NMG_KIND_NMGREGION_A ||
	    kind == NMG_KIND_SHELL_A ||
	    kind == NMG_KIND_LOOP_G) {
	    for (i=0; i<m->maxindex; i++) {
		if (ptrs[i] == NULL) continue;
		if (ecnt[i].kind != kind) continue;
		ecnt[i].new_subscript = DISK_INDEX_NULL;
	    }
	    continue;
	}

	for (i=0; i< m->maxindex;i++) {
	    if (ptrs[i] == NULL) continue;
	    if (ecnt[i].kind != kind) continue;
	    ecnt[i].new_subscript = subscript++;
	}
    }
    /* Tack on the variable sized fastf_t arrays at the end */
    rt_nmg_cur_fastf_subscript = subscript;
    subscript += kind_counts[NMG_KIND_DOUBLE_ARRAY];

    /* Now do some checking to make sure the world is not totally mad */
    for (i=0; i<m->maxindex; i++) {
	if (ptrs[i] == NULL) continue;

	if (nmg_index_of_struct(ptrs[i]) != i) {
	    bu_log("***ERROR, ptrs[%d]->index = %d\n",
		   i, nmg_index_of_struct(ptrs[i]));
	}
	if (rt_nmg_magic_to_kind(*ptrs[i]) != ecnt[i].kind) {
	    bu_log("***ERROR, ptrs[%d] kind(%d) != %d\n",
		   i, rt_nmg_magic_to_kind(*ptrs[i]),
		   ecnt[i].kind);
	}

    }

    tot_size = 0;
    for (i=0; i< NMG_N_KINDS; i++) {
	if (kind_counts[i] <= 0) {
	    disk_arrays[i] = ((void *)0);
	    continue;
	}
	tot_size += kind_counts[i] * rt_nmg_disk_sizes[i];
    }

    /* Account for variable sized double arrays, at the end */
    tot_size += kind_counts[NMG_KIND_DOUBLE_ARRAY] * (4+4) +
	double_count*8;

    ecnt[0].byte_offset = subscript; /* implicit arg to reindex() */
    tot_size += SIZEOF_NETWORK_LONG*(NMG_N_KINDS + 1); /* one for magic */
    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = tot_size;
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "nmg external5");
    dp = ep->ext_buf;
    *(uint32_t *)dp = htonl(DISK_MODEL_VERSION);
    dp+=SIZEOF_NETWORK_LONG;

    for (kind=0; kind <NMG_N_KINDS; kind++) {
	*(uint32_t *)dp = htonl(kind_counts[kind]);
	dp+=SIZEOF_NETWORK_LONG;
    }
    for (i=0; i< NMG_N_KINDS; i++) {
	disk_arrays[i] = dp;
	dp += kind_counts[i] * rt_nmg_disk_sizes[i];
    }
    rt_nmg_fastf_p = (unsigned char*)disk_arrays[NMG_KIND_DOUBLE_ARRAY];

    for (i = m->maxindex-1;i >=0; i--) {
	if (ptrs[i] == NULL) continue;
	kind = ecnt[i].kind;
	if (kind_counts[kind] <= 0) continue;
	rt_nmg_edisk((void *)(disk_arrays[kind]),
		     (void *)(ptrs[i]), ecnt, i, local2mm);
    }

    bu_free((char *)ptrs, "ptrs[]");
    bu_free((char *)ecnt, "ecnt[]");
    return 0;		/* OK */
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


HIDDEN void
rt_nmg_faces_area(struct poly_face* faces, struct shell* s)
{
    struct bu_ptbl nmg_faces;
    unsigned int num_faces, i;
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
	    unsigned int num_faces, i;
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
    struct model *m;
    struct nmgregion* r;
    struct shell* s;
    struct poly_face *faces;
    struct bu_ptbl nmg_faces;
    fastf_t volume = 0.0;
    point_t arbit_point = VINIT_ZERO;
    size_t num_faces, i;

    *cent[0] = 0.0;
    *cent[1] = 0.0;
    *cent[2] = 0.0;
    m = (struct model *)ip->idb_ptr;
    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    s = BU_LIST_FIRST(shell, &r->s_hd);

    /*get faces*/
    nmg_face_tabulate(&nmg_faces, &s->l.magic, &RTG.rtg_vlfree);
    num_faces = BU_PTBL_LEN(&nmg_faces);
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
	    unsigned int num_faces, i;
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
    int ret;
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
	ret = intern.idb_meth->ft_export4(&ext, &intern, 1.0, fp->dbip, &rt_uniresource);
	if (ret < 0) {
	    bu_log("rt_db_put_internal(%s):  solid export failure\n",
		   name);
	    ret = -1;
	    goto out;
	}
	db_wrap_v4_external(&ext, name);
    } else {
	if (rt_db_cvt_to_external5(&ext, name, &intern, 1.0, fp->dbip, &rt_uniresource, intern.idb_major_type) < 0) {
	    bu_log("wdb_export4(%s): solid export failure\n",
		   name);
	    ret = -2;
	    goto out;
	}
    }
    BU_CK_EXTERNAL(&ext);

    flags = db_flags_internal(&intern);
    ret = wdb_export_external(fp, &ext, name, flags, intern.idb_type);
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

HIDDEN int
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

    return BU_PTBL_LEN(tab);


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


HIDDEN int
_nmg_shell_append(struct rt_bot_internal *bot, struct bu_ptbl *nmg_vertices, struct bu_ptbl *nmg_faces, int *voffset, int *foffset)
{
    unsigned int i;
    unsigned int vo = (voffset) ? (unsigned int)*voffset : 0;
    unsigned int fo = (foffset) ? (unsigned int)*foffset : 0;
    int face_no;
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

HIDDEN void
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
    int vert_cnt = 0;
    int face_cnt = 0;

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
