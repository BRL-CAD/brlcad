/*                  N M G _ R T _ I S E C T . C
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
/** @addtogroup nmg */
/** @{ */
/** @file primitives/nmg/nmg_rt_isect.c
 *
 * Support routines for raytracing an NMG.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "plot3.h"


/* Plot a faceuse and a line between pt and plane_pt */
static int plot_file_number=0;


const char *
nmg_rt_inout_str(int code)
{
    switch (code) {
	case HMG_HIT_IN_IN:
	    return "IN_IN";
	case HMG_HIT_IN_OUT:
	    return "IN_OUT";
	case HMG_HIT_OUT_IN:
	    return "OUT_IN";
	case HMG_HIT_OUT_OUT:
	    return "OUT_OUT";
	case HMG_HIT_IN_ON:
	    return "IN_ON";
	case HMG_HIT_ON_IN:
	    return "ON_IN";
	case HMG_HIT_OUT_ON:
	    return "OUT_ON";
	case HMG_HIT_ON_OUT:
	    return "ON_OUT";
	case HMG_HIT_ANY_ANY:
	    return "ANY_ANY";
    }
    return "?_?\n";
}


const char *
nmg_rt_state_str(int code)
{
    switch (code) {
	case NMG_RAY_STATE_INSIDE:
	    return "RS_INSIDE";
	case NMG_RAY_STATE_ON:
	    return "RS_ON";
	case NMG_RAY_STATE_OUTSIDE:
	    return "RS_OUTSIDE";
	case NMG_RAY_STATE_ANY:
	    return "RS_ANY";
    }
    return "RS_UNKNOWN";
}


/**
 * N M G _ C K _ H I T M I S S _ L I S T
 *
 * Ensure that the ray makes a valid set of state transitions.
 */
void
nmg_ck_hitmiss_list(const struct bu_list *hd)
{
    struct hitmiss *hmp;
    int state = NMG_RAY_STATE_OUTSIDE;
    int istate;
    int ostate;

    for (BU_LIST_FOR(hmp, hitmiss, hd)) {
	NMG_CK_HITMISS(hmp);

	/* Skip hits on non-3-manifolds */
	if (hmp->in_out == HMG_HIT_ANY_ANY) continue;

	istate = HMG_INBOUND_STATE(hmp);
	ostate = HMG_OUTBOUND_STATE(hmp);
	if (state != istate) {
	    bu_log("ray state was=%s, transition=%s (istate=%s)\n",
		   nmg_rt_state_str(state),
		   nmg_rt_inout_str(hmp->in_out),
		   nmg_rt_state_str(istate));
	    bu_bomb("nmg_ck_hitmiss_list() NMG ray-tracer bad in/out state transition\n");
	}
	state = ostate;
    }
    if (state != NMG_RAY_STATE_OUTSIDE) {
	bu_log("ray ending state was %s, should have been RS_OUT\n", nmg_rt_state_str(state));
	bu_bomb("nmg_ck_hitmiss_list() NMG ray-tracer bad ending state\n");
    }
}


HIDDEN void
nmg_rt_isect_plfu(struct faceuse *fu, fastf_t *pt, fastf_t *plane_pt)
{
    FILE *fp;
    char name[25];
    long *b;

    NMG_CK_FACEUSE(fu);

    sprintf(name, "ray%02d.pl", plot_file_number++);
    fp=fopen(name, "wb");
    if (fp == (FILE *)NULL) {
	perror(name);
	bu_bomb("unable to open file for writing");
    }

    bu_log("overlay %s\n", name);
    b = (long *)bu_calloc(fu->s_p->r_p->m_p->maxindex,
			  sizeof(long), "bit vec"),

	pl_erase(fp);
    pd_3space(fp,
	      fu->f_p->min_pt[0]-1.0,
	      fu->f_p->min_pt[1]-1.0,
	      fu->f_p->min_pt[2]-1.0,
	      fu->f_p->max_pt[0]+1.0,
	      fu->f_p->max_pt[1]+1.0,
	      fu->f_p->max_pt[2]+1.0);

    nmg_pl_fu(fp, fu, b, 255, 255, 255);

    pl_color(fp, 255, 50, 50);
    pdv_3line(fp, pt, plane_pt);

    bu_free((char *)b, "bit vec");
    fclose(fp);
}


HIDDEN void
pleu(struct edgeuse *eu, fastf_t *pt, fastf_t *plane_pt)
{
    FILE *fp;
    char name[25];
    long *b;
    point_t min_pt, max_pt;
    int i;
    struct model *m;

    sprintf(name, "ray%02d.pl", plot_file_number++);
    fp=fopen(name, "wb");
    if (fp == (FILE *)NULL) {
	perror(name);
	bu_bomb("unable to open file for writing");
    }

    bu_log("overlay %s\n", name);
    m = nmg_find_model(eu->up.magic_p);
    b = (long *)bu_calloc(m->maxindex, sizeof(long), "bit vec");

    pl_erase(fp);

    VMOVE(min_pt, eu->vu_p->v_p->vg_p->coord);

    for (i=0; i < 3; i++) {
	if (eu->eumate_p->vu_p->v_p->vg_p->coord[i] < min_pt[i]) {
	    max_pt[i] = min_pt[i];
	    min_pt[i] = eu->eumate_p->vu_p->v_p->vg_p->coord[i];
	} else {
	    max_pt[i] = eu->eumate_p->vu_p->v_p->vg_p->coord[i];
	}
    }
    pd_3space(fp,
	      min_pt[0]-1.0, min_pt[1]-1.0, min_pt[2]-1.0,
	      max_pt[0]+1.0, max_pt[1]+1.0, max_pt[2]+1.0);

    nmg_pl_eu(fp, eu, b, 255, 255, 255);
    pl_color(fp, 255, 50, 50);
    pdv_3line(fp, pt, plane_pt);
    bu_free((char *)b, "bit vec");
    fclose(fp);
}


void
nmg_rt_print_hitmiss(struct hitmiss *a_hit)
{
    NMG_CK_HITMISS(a_hit);

    bu_log("   dist:%12g pt=(%f %f %f) %s=x%x\n",
	   a_hit->hit.hit_dist,
	   a_hit->hit.hit_point[0],
	   a_hit->hit.hit_point[1],
	   a_hit->hit.hit_point[2],
	   bu_identify_magic(*(uint32_t *)a_hit->hit.hit_private),
	   a_hit->hit.hit_private
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
nmg_rt_print_hitlist(struct hitmiss *hl)
{
    struct hitmiss *a_hit;

    bu_log("nmg/ray hit list:\n");

    for (BU_LIST_FOR(a_hit, hitmiss, &(hl->l))) {
	nmg_rt_print_hitmiss(a_hit);
    }
}


/**
 * H I T _ I N S
 *
 * insert the new hit point in the correct place in the list of hits
 * so that the list is always in sorted order
 */
HIDDEN void
hit_ins(struct ray_data *rd, struct hitmiss *newhit)
{
    struct hitmiss *a_hit;

    BU_LIST_MAGIC_SET(&newhit->l, NMG_RT_HIT_MAGIC);
    newhit->hit.hit_magic = RT_HIT_MAGIC;

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	bu_log("hit_ins()\n  inserting:");
	nmg_rt_print_hitmiss(newhit);
    }

    for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)) {
	if (newhit->hit.hit_dist < a_hit->hit.hit_dist)
	    break;
    }

    /* a_hit now points to the item before which we should insert this
     * hit in the hit list.
     */

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	if (BU_LIST_NOT_HEAD(a_hit, &rd->rd_hit)) {
	    bu_log("   prior to:");
	    nmg_rt_print_hitmiss(a_hit);
	} else {
	    bu_log("\tat end of list\n");
	}
    }

    BU_LIST_INSERT(&a_hit->l, &newhit->l);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	nmg_rt_print_hitlist((struct hitmiss *)&rd->rd_hit);

}


/**
 * The ray missed this vertex.  Build the appropriate miss structure.
 */
HIDDEN struct hitmiss *
ray_miss_vertex(struct ray_data *rd, struct vertexuse *vu_p)
{
    struct hitmiss *myhit;

    NMG_CK_VERTEXUSE(vu_p);
    NMG_CK_VERTEX(vu_p->v_p);
    NMG_CK_VERTEX_G(vu_p->v_p->vg_p);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	bu_log("ray_miss_vertex(%g %g %g)\n",
	       vu_p->v_p->vg_p->coord[0],
	       vu_p->v_p->vg_p->coord[1],
	       vu_p->v_p->vg_p->coord[2]);
    }

    myhit=NMG_INDEX_GET(rd->hitmiss, vu_p->v_p);
    if (myhit) {
	/* vertex previously processed */
	if (BU_LIST_MAGIC_EQUAL((struct bu_list *)myhit, NMG_RT_HIT_MAGIC)) {
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("ray_miss_vertex(vertex previously HIT!!!!)\n");
	} else if (BU_LIST_MAGIC_EQUAL((struct bu_list *)myhit, NMG_RT_HIT_SUB_MAGIC)) {
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("ray_miss_vertex(vertex previously HIT_SUB!?!?)\n");
	}
	return myhit;
    }
    myhit=NMG_INDEX_GET(rd->hitmiss, vu_p->v_p);
    if (myhit) {
	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	    bu_log("ray_miss_vertex(vertex previously missed)\n");
	return myhit;
    }

    NMG_GET_HITMISS(myhit, rd->ap);
    NMG_INDEX_ASSIGN(rd->hitmiss, vu_p->v_p, myhit);
    myhit->outbound_use = (long *)vu_p;
    myhit->inbound_use = (long *)vu_p;
    myhit->hit.hit_private = (genptr_t)vu_p->v_p;

    /* get build_vertex_miss() to compute this */
    myhit->dist_in_plane = -1.0;

    /* add myhit to the list of misses */
    BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
    BU_LIST_INSERT(&rd->rd_miss, &myhit->l);

    return myhit;
}


/**
 * Support routine for vertex_neighborhood()
 */
HIDDEN void
get_pole_dist_to_face(struct ray_data *rd, struct vertexuse *vu, fastf_t *Pole, fastf_t *Pole_prj_pt, double *Pole_dist, fastf_t *Pole_pca, fastf_t *pointA, fastf_t *leftA, fastf_t *pointB, fastf_t *leftB, fastf_t *polar_height_vect, char *Pole_name)
{
    vect_t pca_to_pole_vect;
    vect_t VtoPole_prj;
    point_t pcaA, pcaB;
    double distA, distB;
    int code, status;

    VSETALL(pca_to_pole_vect, 0);

    /* find the points of closest approach
     * There are six distinct return values from bn_dist_pt3_lseg3():
     *
     *    Value	Condition
     *    	-----------------------------------------------------------------
     *	0	P is within tolerance of lseg AB.  *dist isn't 0: (SPECIAL!!!)
     *		  *dist = parametric dist = |PCA-A| / |B-A|.  pca=computed.
     *	1	P is within tolerance of point A.  *dist = 0, pca=A.
     *	2	P is within tolerance of point B.  *dist = 0, pca=B.
     *
     *	3	P is to the "left" of point A.  *dist=|P-A|, pca=A.
     *	4	P is to the "right" of point B.  *dist=|P-B|, pca=B.
     *	5	P is "above/below" lseg AB.  *dist=|PCA-P|, pca=computed.
     */
    code = bn_dist_pt3_lseg3(&distA, pcaA, vu->v_p->vg_p->coord, pointA,
			     Pole_prj_pt, rd->tol);
    if (code < 3) {
	/* Point is on line */
	*Pole_dist = MAGNITUDE(polar_height_vect);
	VMOVE(Pole_pca, Pole_prj_pt);
	return;
    }

    status = code << 4;
    code = bn_dist_pt3_lseg3(&distB, pcaB, vu->v_p->vg_p->coord, pointB,
			     Pole_prj_pt, rd->tol);
    if (code < 3) {
	/* Point is on line */
	*Pole_dist = MAGNITUDE(polar_height_vect);
	VMOVE(Pole_pca, Pole_prj_pt);
	return;
    }

    status |= code;

    /*	  Status
     *	codeA CodeB
     *	 3	3	Do the Tanenbaum patch thing
     *	 3	4	This should not happen.
     *	 3	5	compute dist from pole to pcaB
     *	 4	3	This should not happen.
     *	 4	4	This should not happen.
     *	 4	5	This should not happen.
     *	 5	3	compute dist from pole to pcaA
     *	 5	4	This should not happen.
     *	 5	5	pick the edge with smallest "dist"
     *	 		and compute pole-pca distance.
     */
    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("get_pole_dist_to_face(%s) status from dist_pt_lseg == 0x%02x\n", Pole_name, status);

    switch (status) {
	case 0x35: /* compute dist from pole to pcaB */

	    /* if plane point is "inside" edge B, plane point is PCA */
	    VSUB2(VtoPole_prj, Pole_prj_pt, vu->v_p->vg_p->coord);
	    if (VDOT(leftB, VtoPole_prj) > -SMALL_FASTF) {
		/* plane point is "inside" edge B */
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("\tplane point inside face\n");
		VMOVE(Pole_pca, Pole_prj_pt);
		VSUB2(pca_to_pole_vect, Pole_prj_pt, Pole);
		*Pole_dist = MAGNITUDE(pca_to_pole_vect);
		return;
	    }

	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("\tplane point outside face\n");
	    VSUB2(pca_to_pole_vect, pcaB, Pole);
	    VMOVE(Pole_pca, pcaB);
	    break;
	case 0x53: /* compute dist from pole to pcaA */

	    /* if plane point is "inside" edge A, plane point is PCA */
	    VSUB2(VtoPole_prj, Pole_prj_pt, vu->v_p->vg_p->coord);
	    if (VDOT(leftA, VtoPole_prj) > -SMALL_FASTF) {
		/* plane point is "inside" edge A */
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("\tplane point inside face\n");
		VMOVE(Pole_pca, Pole_prj_pt);
		VSUB2(pca_to_pole_vect, Pole_prj_pt, Pole);
		*Pole_dist = MAGNITUDE(pca_to_pole_vect);
		return;
	    }

	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("\tplane point outside face\n");
	    VSUB2(pca_to_pole_vect, pcaA, Pole);
	    VMOVE(Pole_pca, pcaA);
	    break;
	case 0x55:/* pick the edge with smallest "dist"
		   * and compute pole-pca distance.
		   */
	    VSUB2(VtoPole_prj, Pole_prj_pt, vu->v_p->vg_p->coord);

	    if (distA < distB) {
		VUNITIZE(VtoPole_prj);
		VUNITIZE(leftA);
		if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		    VPRINT("LeftA", leftA);
		    VPRINT("VtoPole_prj", VtoPole_prj);
		}

		if (VDOT(leftA, VtoPole_prj) > -SMALL_FASTF) {
		    /* plane point is "inside" edge A */
		    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			bu_log("\tplane point inside face\n");
		    VSUB2(pca_to_pole_vect, Pole_prj_pt, Pole);
		    *Pole_dist = MAGNITUDE(pca_to_pole_vect);
		    VMOVE(Pole_pca, Pole_prj_pt);
		    return;
		}
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("\tplane point outside face\n");
		VSUB2(pca_to_pole_vect, pcaA, Pole);
		VMOVE(Pole_pca, pcaA);
	    } else {
		VUNITIZE(VtoPole_prj);
		VUNITIZE(leftB);
		if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		    VPRINT("LeftB", leftB);
		    VPRINT("VtoPole_prj", VtoPole_prj);
		}
		if (VDOT(leftB, VtoPole_prj) > -SMALL_FASTF) {
		    /* plane point is "inside" edge B */
		    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			bu_log("\tplane point inside face\n");
		    VMOVE(Pole_pca, Pole_prj_pt);
		    VSUB2(pca_to_pole_vect, Pole_prj_pt, Pole);
		    *Pole_dist = MAGNITUDE(pca_to_pole_vect);
		    return;
		}
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("\tplane point outside face\n");
		VSUB2(pca_to_pole_vect, pcaB, Pole);
		VMOVE(Pole_pca, pcaB);
	    }
	    break;
	case 0x33: {
	    /* The point is over the vertex shared by the points, and
	     * not over one edge or the other.  We now need to figure
	     * out which edge to classify this point against.
	     *
	     * Time to do the Tanenbaum algorithm.
	     */
	    double dotA;
	    double dotB;

	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("\tplane point beyond both edges.... Doing the Tanenbaum algorithm.\n");

	    VSUB2(VtoPole_prj, Pole_prj_pt, vu->v_p->vg_p->coord);

	    dotA = VDOT(leftA, VtoPole_prj);
	    dotB = VDOT(leftB, VtoPole_prj);

	    if (dotA < dotB) {
		if (dotA > -SMALL_FASTF) {
		    /* Point is "inside" face,
		     * PCA is plane projection point.
		     */
		    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			bu_log("\tpoint inside face\n");
		    VMOVE(Pole_pca, Pole_prj_pt);
		    VSUB2(pca_to_pole_vect, Pole, Pole_prj_pt);
		} else {
		    /* Point is "outside" face, PCA is at vertex. */
		    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			bu_log("\tpoint outside face, PCA is vertex\n");

		    VSUB2(pca_to_pole_vect, pcaA, Pole);
		    VMOVE(Pole_pca, pcaA);
		}
	    } else {
		if (dotB > -SMALL_FASTF) {
		    /* Point is "inside" face,
		     * PCA is plane projection point
		     */
		    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			bu_log("\tpoint is inside face\n");
		    VMOVE(Pole_pca, Pole_prj_pt);
		    VSUB2(pca_to_pole_vect, Pole, Pole_prj_pt);
		} else {

		    /* Point is "outside" face, PCA is at vertex. */
		    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			bu_log("\tpoint is outside face, PCA is vertex\n");
		    VSUB2(pca_to_pole_vect, pcaB, Pole);
		    VMOVE(Pole_pca, pcaB);
		}
	    }
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		VPRINT("\tpca_to_pole_vect", pca_to_pole_vect);
	    *Pole_dist = MAGNITUDE(pca_to_pole_vect);
	    return;
	}
	case 0x34: /* fallthrough */
	case 0x54: /* fallthrough */
	case 0x43: /* fallthrough */
	case 0x44: /* fallthrough */
	case 0x45: /* fallthrough */
	default:
	    bu_log("%s %d: So-called 'Impossible' status codes\n",
		   __FILE__, __LINE__);
	    bu_bomb("get_pole_dist_to_face() Pretending NOT to bomb\n");
	    break;
    }

    *Pole_dist = MAGNITUDE(pca_to_pole_vect);

    return;
}


HIDDEN void
plot_neighborhood(fastf_t *North_Pole, fastf_t *North_pl_pt, fastf_t *North_pca, fastf_t *South_Pole, fastf_t *South_pl_pt, fastf_t *South_pca, fastf_t *pointA, fastf_t *pointB, fastf_t *norm, fastf_t *pt, fastf_t *leftA, fastf_t *leftB)
{
    static int plotnum=0;
    FILE *pfp;
    char name[64];
    point_t my_pt;
    vect_t ray;

    sprintf(name, "vert%03d.pl", plotnum++);
    pfp=fopen(name, "wb");
    if (pfp == (FILE *)NULL) {
	bu_log("Error opening %s\n", name);
	return;
    } else
	bu_log("overlay %s\n", name);


    /* draw the ray */
    pl_color(pfp, 255, 55, 55);
    pdv_3line(pfp, North_Pole, South_Pole);

    /* draw the area of the face */
    pl_color(pfp, 55, 255, 55);
    pdv_3move(pfp, pt);
    pdv_3cont(pfp, pointA);
    pdv_3cont(pfp, pointB);
    pdv_3cont(pfp, pt);

    /* draw the projections of the pole points */
    pl_color(pfp, 255, 255, 55);
    pdv_3line(pfp, North_Pole, North_pl_pt);
    if (! VEQUAL(North_pca, North_pl_pt)) {
	pdv_3line(pfp, North_Pole, North_pca);
	pdv_3line(pfp, North_pl_pt, North_pca);
    }
    VSUB2(ray, South_Pole, North_Pole);
    VSCALE(ray, ray, -0.125);
    VADD2(my_pt, North_Pole, ray);
    pdv_3move(pfp, my_pt);
    pl_label(pfp, "N");

    pl_color(pfp, 55, 255, 255);
    pdv_3line(pfp, South_Pole, South_pl_pt);
    if (! VEQUAL(South_pca, South_pl_pt)) {
	pdv_3line(pfp, South_Pole, South_pca);
	pdv_3line(pfp, South_pl_pt, South_pca);
    }
    VREVERSE(ray, ray);
    VADD2(my_pt, South_Pole, ray);
    pdv_3move(pfp, my_pt);
    pl_label(pfp, "S");

    pl_color(pfp, 128, 128, 128);
    VADD2(my_pt, pt, norm);
    pdv_3line(pfp, pt, my_pt);

    pl_color(pfp, 192, 192, 192);
    VADD2(my_pt, pointA, leftA);
    pdv_3line(pfp, pointA, my_pt);

    VADD2(my_pt, pointB, leftB);
    pdv_3line(pfp, pointB, my_pt);

    fclose(pfp);
}


/** V E R T E X _ N E I G H B O R H O O D
 *
 * Examine the vertex neighborhood to classify the ray as IN/ON/OUT of
 * the NMG both before and after hitting the vertex.
 *
 *
 * There is a conceptual sphere about the vertex.  For reasons
 * associated with tolerancing, the sphere has a radius equal to the
 * magnitude of the maximum dimension of the NMG bounding RPP.
 *
 * The point where the ray enters this sphere is called the "North
 * Pole" and the point where it exits the sphere is called the "South
 * Pole"
 *
 * For each face which uses this vertex we compute 2 "distance"
 * metrics:
 *
 * project the "north pole" and "south pole" down onto the plane of
 * the face:
 *
 *
 *
 *			    		    /
 *				  	   /
 *				     	  /North Pole
 *				     	 / |
 *			Face Normal  ^	/  |
 *			 	     | /   | Projection of North Pole
 *	Plane of face	    	     |/    V to plane
 *  ---------------------------------*-------------------------------
 *     		  Projection of ^   / Vertex
 *		     South Pole |  /
 *		       to plane | /
 *			    	|/
 *			    	/South Pole
 *			       /
 *			      /
 *			     /
 *			    /
 *			   /
 *			  /
 *			|/_
 *			Ray
 *
 *	If the projected polar point is inside the two edgeuses at this
 *		vertexuse, then
 *		 	the distance to the face is the projection distance.
 *
 *		else
 *			Each of the two edgeuses at this vertexuse
 *			implies an infinite ray originating at the vertex.
 *			Find the point of closest approach (PCA) of each ray
 *			to the projection point.  For whichever ray passes
 *			closer to the projection point, find the distance
 *			from the original pole point to the PCA.  This is
 *			the "distance to the face" metric for this face from
 *			the given pole point.
 *
 * We compute this metric for the North and South poles for all faces
 * at the vertex.  The face with the smallest distance to the north
 * (south) pole is used to determine the state of the ray before
 * (after) it hits the vertex.
 *
 * If the north (south) pole is "outside" the plane of the closest
 * face, then the ray state is "outside" before (after) the ray hits
 * the vertex.
 */
HIDDEN void
vertex_neighborhood(struct ray_data *rd, struct vertexuse *vu_p, struct hitmiss *myhit)
{
    struct vertexuse *vu;
    struct faceuse *fu;
    point_t South_Pole, North_Pole;
    struct faceuse *North_fu = (struct faceuse *)NULL;
    struct faceuse *South_fu = (struct faceuse *)NULL;
    struct vertexuse *North_vu = (struct vertexuse *)NULL;
    struct vertexuse *South_vu = (struct vertexuse *)NULL;
    point_t North_pl_pt, South_pl_pt;
    point_t North_pca, South_pca;
    double North_dist, South_dist;
    double North_min, South_min;
    double cos_angle;
    vect_t VtoPole;
    double scalar_dist;
    double dimen, t;
    point_t min_pt, max_pt;
    int i;
    vect_t norm, anti_norm;
    vect_t polar_height_vect;
    vect_t leftA, leftB;
    point_t pointA, pointB;
    struct edgeuse *eu;
    vect_t edge_vect;
    int found_faces;
    vect_t r_dir_unit;

    NMG_CK_VERTEXUSE(vu_p);
    NMG_CK_VERTEX(vu_p->v_p);
    NMG_CK_VERTEX_G(vu_p->v_p->vg_p);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("vertex_neighborhood\n");

    nmg_model_bb(min_pt, max_pt, nmg_find_model(vu_p->up.magic_p));
    for (dimen= -MAX_FASTF, i=3; i--;) {
	t = max_pt[i]-min_pt[i];
	if (t > dimen) dimen = t;
    }

    VMOVE(r_dir_unit, rd->rp->r_dir);
    VUNITIZE(r_dir_unit);
    VJOIN1(North_Pole, vu_p->v_p->vg_p->coord, -dimen, r_dir_unit);
    VJOIN1(South_Pole, vu_p->v_p->vg_p->coord, dimen, r_dir_unit);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	VPRINT("\tNorth Pole", North_Pole);
	VPRINT("\tSouth Pole", South_Pole);
    }

    /* There is a conceptual sphere around the vertex.  The max
     * dimension of the bounding box for the NMG defines the size.
     * The point where the ray enters this sphere is called the North
     * Pole, and the point where it exits is called the South Pole.
     */

    South_min = North_min = MAX_FASTF;
    found_faces = 0;
    myhit->in_out = 0;

    /* for every use of this vertex */
    for (BU_LIST_FOR(vu, vertexuse, &vu_p->v_p->vu_hd)) {
	/* if the parent use is an (edge/loop)use of an OT_SAME
	 * faceuse that we haven't already processed...
	 */
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_VERTEX(vu->v_p);
	NMG_CK_VERTEX_G(vu->v_p->vg_p);

	fu = nmg_find_fu_of_vu(vu);
	if (fu) {
	    found_faces = 1;
	    if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC &&
		fu->orientation == OT_SAME) {

		/* The distance from each "Pole Point" to the faceuse
		 * is the commodity in which we are interested.
		 *
		 * A pole point is projected.  This is either the
		 * distance to the plane (if the projection of the
		 * point into the plane lies within the angle of the
		 * two edgeuses at this vertex) or we take the
		 * distance from the pole to the PCA of the closest
		 * edge.
		 */
		NMG_GET_FU_NORMAL(norm, fu);
		VREVERSE(anti_norm, norm);

		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    VPRINT("\tchecking face", norm);

		/* project the north pole onto the plane */
		VSUB2(polar_height_vect, vu->v_p->vg_p->coord, North_Pole);
		scalar_dist = VDOT(norm, polar_height_vect);

		/* project the poles down onto the plane */
		VJOIN1(North_pl_pt, North_Pole, scalar_dist, norm);
		VJOIN1(South_pl_pt, South_Pole, scalar_dist, anti_norm);
		if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		    VPRINT("\tNorth Plane Point", North_pl_pt);
		    VPRINT("\tSouth Plane Point", South_pl_pt);
		}
		/* Find points on sphere in direction of edges
		 * (away from vertex along edge)
		 */
		eu = vu->up.eu_p;
		do {
		    eu = BU_LIST_PNEXT_CIRC(edgeuse, eu);
		} while ((eu->vu_p->v_p == vu->v_p) && (eu != vu->up.eu_p));
		nmg_find_eu_leftvec(leftA, vu->up.eu_p);
		VSUB2(edge_vect, eu->vu_p->v_p->vg_p->coord, vu->v_p->vg_p->coord);
		VUNITIZE(edge_vect);
		VJOIN1(pointA, vu->v_p->vg_p->coord, dimen, edge_vect)

		eu = vu->up.eu_p;
		do {
		    eu = BU_LIST_PPREV_CIRC(edgeuse, eu);
		} while ((eu->vu_p->v_p == vu->v_p) && (eu != vu->up.eu_p));

		nmg_find_eu_leftvec(leftB, eu);
		VSUB2(edge_vect, eu->vu_p->v_p->vg_p->coord, vu->v_p->vg_p->coord);
		VUNITIZE(edge_vect);
		VJOIN1(pointB, vu->v_p->vg_p->coord, dimen, edge_vect)


		    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
			VPRINT("\tLeftA", leftA);
			VPRINT("\tLeftB", leftB);
		    }
		/* find distance of face to North Pole */
		get_pole_dist_to_face(rd, vu,
				      North_Pole, North_pl_pt, &North_dist, North_pca,
				      pointA, leftA, pointB, leftB,
				      polar_height_vect, "North");

		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("\tDist north pole<=>face %g\n", North_dist);

		if (North_min > North_dist) {
		    North_min = North_dist;
		    North_fu = fu;
		    North_vu = vu;
		    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			bu_log("New North Pole Min: %g\n", North_min);
		}


		/* find distance of face to South Pole */
		get_pole_dist_to_face(rd, vu,
				      South_Pole, South_pl_pt, &South_dist, South_pca,
				      pointA, leftA, pointB, leftB,
				      polar_height_vect, "South");

		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("\tDist south pole<=>face %g\n", South_dist);

		if (South_min > South_dist) {
		    South_min = South_dist;
		    South_fu = fu;
		    South_vu = vu;
		    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			bu_log("New South Pole Min: %g\n", South_min);
		}


		if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		    plot_neighborhood(North_Pole, North_pl_pt, North_pca,
				      South_Pole, South_pl_pt, South_pca,
				      pointA, pointB, norm,
				      vu_p->v_p->vg_p->coord,
				      leftA, leftB);
		}
	    }
	} else {
	    if (!found_faces)
		South_vu = North_vu = vu;
	}
    }

    if (!found_faces) {
	/* we've found a vertex floating in space */
	myhit->outbound_use = myhit->inbound_use = (long *)North_vu;
	myhit->in_out = HMG_HIT_ANY_ANY;
	VREVERSE(myhit->hit.hit_normal, r_dir_unit);
	return;
    }


    NMG_GET_FU_NORMAL(norm, North_fu);
    VMOVE(myhit->inbound_norm, norm);
    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("North Pole Min: %g to %g %g %g\n", North_min,
	       norm[0], norm[1], norm[2]);

    /* compute status of ray as it is in-bound on the vertex */
    VSUB2(VtoPole, North_Pole, vu_p->v_p->vg_p->coord);
    cos_angle = VDOT(norm, VtoPole);
    if (BN_VECT_ARE_PERP(cos_angle, rd->tol))
	myhit->in_out |= NMG_RAY_STATE_ON << 4;
    else if (cos_angle > SMALL_FASTF)
	myhit->in_out |= NMG_RAY_STATE_OUTSIDE << 4;
    else
	myhit->in_out |= NMG_RAY_STATE_INSIDE << 4;


    /* compute status of ray as it is out-bound from the vertex */
    NMG_GET_FU_NORMAL(norm, South_fu);
    VMOVE(myhit->outbound_norm, norm);
    VSUB2(VtoPole, South_Pole, vu_p->v_p->vg_p->coord);
    cos_angle = VDOT(norm, VtoPole);
    if (BN_VECT_ARE_PERP(cos_angle, rd->tol))
	myhit->in_out |= NMG_RAY_STATE_ON;
    else if (cos_angle > SMALL_FASTF)
	myhit->in_out |= NMG_RAY_STATE_OUTSIDE;
    else
	myhit->in_out |= NMG_RAY_STATE_INSIDE;


    myhit->inbound_use = (long *)North_vu;
    myhit->outbound_use = (long *)South_vu;

    switch (myhit->in_out) {
	case HMG_HIT_ON_ON:	/* fallthrough???  -MJM??? */
	case HMG_HIT_IN_IN:	/* fallthrough */
	case HMG_HIT_OUT_OUT:	/* fallthrough */
	case HMG_HIT_IN_ON:	/* fallthrough */
	case HMG_HIT_ON_IN:	/* two hits */
	    myhit->hit.hit_private = (genptr_t)North_vu;
	    break;
	case HMG_HIT_IN_OUT:	/* one hit - outbound */
	case HMG_HIT_ON_OUT:	/* one hit - outbound */
	    myhit->hit.hit_private = (genptr_t)South_vu;
	    break;
	case HMG_HIT_OUT_IN:	/* one hit - inbound */
	case HMG_HIT_OUT_ON:	/* one hit - inbound */
	    myhit->hit.hit_private = (genptr_t)North_vu;
	    break;
	default:
	    bu_log("%s %d: vertex_neighborhood() Bad vertex in_out state = x%x\n",
		   __FILE__, __LINE__, myhit->in_out);
	    bu_bomb("vertex_neighborhood() bad vertex in_out state\n");
	    break;

    }
}


/**
 * Once it has been decided that the ray hits the vertex, this routine
 * takes care of recording that fact.
 */
HIDDEN void
ray_hit_vertex(struct ray_data *rd, struct vertexuse *vu_p, int status)
{
    struct hitmiss *myhit;
    vect_t v;
    vect_t r_dir_unit;

    VMOVE(r_dir_unit, rd->rp->r_dir);
    VUNITIZE(r_dir_unit);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("ray_hit_vertex x%x (%g %g %g) status=%d\n",
	       vu_p->v_p, V3ARGS(vu_p->v_p->vg_p->coord), status);

    myhit = NMG_INDEX_GET(rd->hitmiss, vu_p->v_p);
    if (myhit) {
	if (BU_LIST_MAGIC_EQUAL((struct bu_list *)myhit, NMG_RT_HIT_MAGIC))
	    return;
	/* oops, we have to change a MISS into a HIT */
	BU_LIST_DEQUEUE(&myhit->l);
    } else {
	NMG_GET_HITMISS(myhit, rd->ap);
	NMG_INDEX_ASSIGN(rd->hitmiss, vu_p->v_p, myhit);
	myhit->outbound_use = (long *)vu_p;
	myhit->inbound_use = (long *)vu_p;
    }

    /* v = vector from ray point to hit vertex */
    VSUB2(v, vu_p->v_p->vg_p->coord, rd->rp->r_pt);
    /* using the unit vector for ray direction */
    myhit->hit.hit_dist = VDOT(v, r_dir_unit);	  /* distance along ray */
    VMOVE(myhit->hit.hit_point, vu_p->v_p->vg_p->coord);
    myhit->hit.hit_private = (genptr_t) vu_p->v_p;

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("\tray = (%g %g %g), dir=(%g %g %g), dist=%g\n",
	       V3ARGS(rd->rp->r_pt), V3ARGS(rd->rp->r_dir), myhit->hit.hit_dist);

    BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_MAGIC);

    /* XXX need to compute neighborhood of vertex so that ray can be
     * classified as in/on/out before and after the vertex.
     */
    vertex_neighborhood(rd, vu_p, myhit);

    /* XXX we re really should temper the results of vertex_neighborhood()
     * with the knowledge of "status"
     */

    hit_ins(rd, myhit);

    /* sanity check */
    NMG_CK_HITMISS(myhit);
}


/**
 * I S E C T _ R A Y _ V E R T E X U S E
 *
 * Called in one of the following situations:
 * 1) Zero length edge
 * 2) Vertexuse child of Loopuse
 * 3) Vertexuse child of Shell
 *
 * return:
 * 1 vertex was hit
 * 0 vertex was missed
 */
HIDDEN int
isect_ray_vertexuse(struct ray_data *rd, struct vertexuse *vu_p)
{
    struct hitmiss *myhit;
    double ray_vu_dist;

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("isect_ray_vertexuse\n");

    NMG_CK_VERTEXUSE(vu_p);
    NMG_CK_VERTEX(vu_p->v_p);
    NMG_CK_VERTEX_G(vu_p->v_p->vg_p);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("nmg_isect_ray_vertexuse %g %g %g",
	       vu_p->v_p->vg_p->coord[0],
	       vu_p->v_p->vg_p->coord[1],
	       vu_p->v_p->vg_p->coord[2]);

    myhit = NMG_INDEX_GET(rd->hitmiss, vu_p->v_p);
    if (myhit) {
	if (BU_LIST_MAGIC_EQUAL((struct bu_list *)myhit, NMG_RT_HIT_MAGIC)) {
	    /* we've previously hit this vertex */
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log(" previously hit\n");
	    return 1;
	} else {
	    /* we've previously missed this vertex */
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log(" previously missed\n");
	    return 0;
	}
    }

    /* intersect ray with vertex */
    ray_vu_dist = bn_dist_line3_pt3(rd->rp->r_pt, rd->rp->r_dir,
				    vu_p->v_p->vg_p->coord);

    if (ray_vu_dist > rd->tol->dist) {
	/* ray misses vertex */
	ray_miss_vertex(rd, vu_p);
	return 0;
    }

    /* ray hits vertex */
    ray_hit_vertex(rd, vu_p, NMG_VERT_UNKNOWN);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	bu_log(" Ray hits vertex, dist %g (priv=x%x, v magic=x%x)\n",
	       myhit->hit.hit_dist,
	       myhit->hit.hit_private,
	       vu_p->v_p->magic);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	    nmg_rt_print_hitlist(rd->hitmiss[NMG_HIT_LIST]);
    }

    return 1;
}


/**
 * As the name implies, this routine is called when the ray and an
 * edge are colinear.  It handles marking the verticies as hit,
 * remembering that this is a seg_in/seg_out pair, and builds the hit
 * on the edge.
 */
HIDDEN void
colinear_edge_ray(struct ray_data *rd, struct edgeuse *eu_p)
{
    struct hitmiss *vhit1, *vhit2, *myhit;


    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("\t - colinear_edge_ray\n");

    vhit1 = NMG_INDEX_GET(rd->hitmiss, eu_p->vu_p->v_p);
    vhit2 = NMG_INDEX_GET(rd->hitmiss, eu_p->eumate_p->vu_p->v_p);

    /* record the hit on each vertex, and remember that these two hits
     * should be kept together.
     */
    if (vhit1->hit.hit_dist > vhit2->hit.hit_dist) {
	ray_hit_vertex(rd, eu_p->vu_p, NMG_VERT_ENTER);
	vhit1->start_stop = NMG_HITMISS_SEG_OUT;

	ray_hit_vertex(rd, eu_p->eumate_p->vu_p, NMG_VERT_LEAVE);
	vhit2->start_stop = NMG_HITMISS_SEG_IN;
    } else {
	ray_hit_vertex(rd, eu_p->vu_p, NMG_VERT_LEAVE);
	vhit1->start_stop = NMG_HITMISS_SEG_IN;

	ray_hit_vertex(rd, eu_p->eumate_p->vu_p, NMG_VERT_ENTER);
	vhit2->start_stop = NMG_HITMISS_SEG_OUT;
    }
    vhit1->other = vhit2;
    vhit2->other = vhit1;

    NMG_GET_HITMISS(myhit, rd->ap);
    NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);
    myhit->hit.hit_private = (genptr_t)eu_p->e_p;

    BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC);
    BU_LIST_INSERT(&rd->rd_miss, &myhit->l);

    return;
}


/**
 * When a vertex at an end of an edge gets hit by the ray, this macro
 * is used to build the hit structures for the vertex and the edge.
 */
#define HIT_EDGE_VERTEX(rd, eu_p, vu_p) \
    { \
	if (rt_g.NMG_debug & DEBUG_RT_ISECT) bu_log("hit_edge_vertex\n"); \
	if (*eu_p->up.magic_p == NMG_SHELL_MAGIC || \
	    (*eu_p->up.magic_p == NMG_LOOPUSE_MAGIC && \
	     *eu_p->up.lu_p->up.magic_p == NMG_SHELL_MAGIC)) \
		ray_hit_vertex(rd, vu_p, NMG_VERT_ENTER_LEAVE); \
	else \
		ray_hit_vertex(rd, vu_p, NMG_VERT_UNKNOWN); \
	NMG_GET_HITMISS(myhit, rd->ap); \
	NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit); \
	myhit->hit.hit_private = (genptr_t)eu_p->e_p; \
	\
	BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC); \
	BU_LIST_INSERT(&rd->rd_miss, &myhit->l); \
    }


/**
 * Compute the "ray state" before and after the ray encounters a
 * hit-point on an edge.
 */
HIDDEN void
edge_hit_ray_state(struct ray_data *rd, struct edgeuse *eu, struct hitmiss *myhit)
{
    double cos_angle;
    double inb_cos_angle = 2.0;
    double outb_cos_angle = -1.0;
    struct shell *s;
    struct faceuse *fu;
    struct faceuse *inb_fu = (struct faceuse *)NULL;
    struct faceuse *outb_fu = (struct faceuse *)NULL;
    struct edgeuse *inb_eu = (struct edgeuse *)NULL;
    struct edgeuse *outb_eu = (struct edgeuse *)NULL;
    struct edgeuse *eu_p;
    struct edgeuse *fu_eu;
    vect_t edge_left;
    vect_t eu_vec;
    vect_t norm;
    int faces_found;
    vect_t r_dir_unit;

    VMOVE(r_dir_unit, rd->rp->r_dir);
    VUNITIZE(r_dir_unit);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	eu_p = BU_LIST_PNEXT_CIRC(edgeuse, eu);
	bu_log("edge_hit_ray_state(%g %g %g -> %g %g %g _vs_ %g %g %g)\n",
	       eu->vu_p->v_p->vg_p->coord[0],
	       eu->vu_p->v_p->vg_p->coord[1],
	       eu->vu_p->v_p->vg_p->coord[2],
	       eu_p->vu_p->v_p->vg_p->coord[0],
	       eu_p->vu_p->v_p->vg_p->coord[1],
	       eu_p->vu_p->v_p->vg_p->coord[2],
	       rd->rp->r_dir[0],
	       rd->rp->r_dir[1],
	       rd->rp->r_dir[2]);
    }

    myhit->in_out = 0;

    s = nmg_find_s_of_eu(eu);

    faces_found = 0;
    eu_p = eu->e_p->eu_p;
    do {
	fu=nmg_find_fu_of_eu(eu_p);
	if (fu) {
	    fu_eu = eu_p;
	    faces_found = 1;
	    if (fu->orientation == OT_OPPOSITE &&
		fu->fumate_p->orientation == OT_SAME) {
		fu = fu->fumate_p;
		fu_eu = eu_p->eumate_p;
	    }
	    if (fu->orientation != OT_SAME) {
		bu_log("%s[%d]: I can't seem to find an OT_SAME faceuse\nThis must be a `dangling' face.  I'll skip it\n", __FILE__, __LINE__);
		goto next_edgeuse;
	    }

	    if (fu->s_p != s)
		goto next_edgeuse;

	    if (nmg_find_eu_leftvec(edge_left, eu_p)) {
		bu_log("edgeuse not part of faceuse");
		goto next_edgeuse;
	    }

	    if (! (NMG_3MANIFOLD &
		   NMG_MANIFOLDS(rd->manifolds, fu->f_p))) {
		bu_log("This is not a 3-Manifold face.  I'll skip it\n");
		goto next_edgeuse;
	    }

	    cos_angle = VDOT(edge_left, r_dir_unit);

	    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		bu_log("left_vect:(%g %g %g) cos_angle:%g\n",
		       edge_left[0], edge_left[1],
		       edge_left[2], cos_angle);
	    }

	    if (cos_angle < inb_cos_angle) {
		inb_cos_angle = cos_angle;
		inb_fu = fu;
		inb_eu = fu_eu;
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("New inb cos_angle %g\n", inb_cos_angle);
	    }
	    if (cos_angle > outb_cos_angle) {
		outb_cos_angle = cos_angle;
		outb_fu = fu;
		outb_eu = fu_eu;
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("New outb cos_angle %g\n", outb_cos_angle);
	    }
	}
    next_edgeuse:	eu_p = eu_p->eumate_p->radial_p;
    } while (eu_p != eu->e_p->eu_p);

    if (!inb_fu || !outb_fu) {
	return;
    }

    if (!faces_found) {
	/* we hit a wire edge */
	myhit->in_out = HMG_HIT_ANY_ANY;
	myhit->hit.hit_private = (genptr_t)eu;
	myhit->inbound_use = myhit->outbound_use = (long *)eu;

	eu_p = BU_LIST_PNEXT_CIRC(edgeuse, eu);
	VSUB2(eu_vec, eu->vu_p->v_p->vg_p->coord,
	      eu_p->vu_p->v_p->vg_p->coord);
	VCROSS(edge_left, eu_vec, r_dir_unit);
	VCROSS(myhit->inbound_norm, eu_vec, edge_left);
	if (VDOT(myhit->inbound_norm, r_dir_unit) > SMALL_FASTF) {
	    VREVERSE(myhit->inbound_norm, myhit->inbound_norm);
	}
	VMOVE(myhit->outbound_norm, myhit->inbound_norm);

	return;
    }

    /* inb_fu is closest to ray on outbound side.
     * outb_fu is closest to ray on inbound side.
     */

    /* Compute the ray state on the inbound side */
    NMG_GET_FU_NORMAL(norm, inb_fu);
    VMOVE(myhit->inbound_norm, norm);
    if (MAGSQ(norm) < VDIVIDE_TOL)
	bu_bomb("edge_hit_ray_state() null normal!\n");

    cos_angle = VDOT(norm, r_dir_unit);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	VPRINT("\ninb face normal", norm);
	bu_log("cos_angle wrt ray direction: %g\n", cos_angle);
    }

    if (BN_VECT_ARE_PERP(cos_angle, rd->tol))
	myhit->in_out |= NMG_RAY_STATE_ON << 4;
    else if (cos_angle < -SMALL_FASTF)
	myhit->in_out |= NMG_RAY_STATE_OUTSIDE << 4;
    else /* (cos_angle > 0.0) */
	myhit->in_out |= NMG_RAY_STATE_INSIDE << 4;


    /* Compute the ray state on the outbound side */
    NMG_GET_FU_NORMAL(norm, outb_fu);
    VMOVE(myhit->outbound_norm, norm);
    cos_angle = VDOT(norm, r_dir_unit);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	VPRINT("\noutb face normal", norm);
	bu_log("cos_angle wrt ray direction: %g\n", cos_angle);
    }

    if (BN_VECT_ARE_PERP(cos_angle, rd->tol))
	myhit->in_out |= NMG_RAY_STATE_ON;
    else if (cos_angle > SMALL_FASTF)
	myhit->in_out |= NMG_RAY_STATE_OUTSIDE;
    else /* (cos_angle < 0.0) */
	myhit->in_out |= NMG_RAY_STATE_INSIDE;


    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	bu_log("myhit->in_out: 0x%02x/", myhit->in_out);
	switch (myhit->in_out) {
	    case HMG_HIT_IN_IN:
		bu_log("IN_IN\n"); break;
	    case HMG_HIT_IN_OUT:
		bu_log("IN_OUT\n"); break;
	    case HMG_HIT_OUT_IN:
		bu_log("OUT_IN\n"); break;
	    case HMG_HIT_OUT_OUT:
		bu_log("OUT_OUT\n"); break;
	    case HMG_HIT_IN_ON:
		bu_log("IN_ON\n"); break;
	    case HMG_HIT_ON_IN:
		bu_log("ON_IN\n"); break;
	    case HMG_HIT_OUT_ON:
		bu_log("OUT_ON\n"); break;
	    case HMG_HIT_ON_OUT:
		bu_log("ON_OUT\n"); break;
	    case HMG_HIT_ON_ON:
		bu_log("ON_ON\n"); break;
	    default:
		bu_log("?_?\n"); break;
	}
    }


    switch (myhit->in_out) {
	case HMG_HIT_ON_ON:	/* Another fall through?? JRA */
	case HMG_HIT_IN_IN:	/* fallthrough */
	case HMG_HIT_OUT_OUT:	/* fallthrough */
	case HMG_HIT_IN_ON:	/* fallthrough */
	case HMG_HIT_ON_IN:	/* two hits */
	    myhit->inbound_use = (long *)inb_eu;
	    myhit->outbound_use = (long *)outb_eu;
	    myhit->hit.hit_private = (genptr_t)inb_eu;
	    break;
	case HMG_HIT_IN_OUT:	/* one hit - outbound */
	case HMG_HIT_ON_OUT:	/* one hit - outbound */
	    myhit->inbound_use = (long *)outb_eu;
	    myhit->outbound_use = (long *)outb_eu;
	    myhit->hit.hit_private = (genptr_t)outb_eu;
	    break;
	case HMG_HIT_OUT_IN:	/* one hit - inbound */
	case HMG_HIT_OUT_ON:	/* one hit - inbound */
	    myhit->inbound_use = (long *)inb_eu;
	    myhit->outbound_use = (long *)inb_eu;
	    myhit->hit.hit_private = (genptr_t)inb_eu;
	    break;
	default:
	    bu_log("%s %d: Bad edge in/out state = x%x\n", __FILE__, __LINE__, myhit->in_out);
	    bu_bomb("edge_hit_ray_state() bad edge in_out state\n");
	    break;
    }
}


/**
 * record a hit on an edge.
 */
HIDDEN void
ray_hit_edge(struct ray_data *rd, struct edgeuse *eu_p, double dist_along_ray, fastf_t *pt)
{
    struct hitmiss *myhit;
    ray_miss_vertex(rd, eu_p->vu_p);
    ray_miss_vertex(rd, eu_p->eumate_p->vu_p);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) bu_log("\t - HIT edge 0x%08x (edgeuse=x%x)\n", eu_p->e_p, eu_p);

    myhit = NMG_INDEX_GET(rd->hitmiss, eu_p->e_p);
    if (myhit) {
	switch (((struct bu_list *)myhit)->magic) {
	    case NMG_RT_MISS_MAGIC:
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("\tedge previously missed, changing to hit\n");
		BU_LIST_DEQUEUE(&myhit->l);
		break;
	    case NMG_RT_HIT_SUB_MAGIC:
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("\tedge vertex previously hit\n");
		return;
	    case NMG_RT_HIT_MAGIC:
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("\tedge previously hit\n");
		return;
	    default:
		break;
	}
    } else {
	NMG_GET_HITMISS(myhit, rd->ap);
    }

    /* create hit structure for this edge */
    NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);
    myhit->outbound_use = (long *)eu_p;
    myhit->inbound_use = (long *)eu_p;

    /* build the hit structure */
    myhit->hit.hit_dist = dist_along_ray;
    VMOVE(myhit->hit.hit_point, pt);
    myhit->hit.hit_private = (genptr_t) eu_p->e_p;

    edge_hit_ray_state(rd, eu_p, myhit);

    hit_ins(rd, myhit);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	struct faceuse *fu=nmg_find_fu_of_eu(eu_p);

	if (fu)
	    nmg_rt_isect_plfu(fu, rd->rp->r_pt, myhit->hit.hit_point);
	else
	    pleu(eu_p, rd->rp->r_pt, myhit->hit.hit_point);
    }
}


void
isect_ray_lseg(struct ray_data *rd, struct edgeuse *eu_p)
{
    int status;
    struct hitmiss *myhit;
    int vhit1, vhit2;
    double dist_along_ray;
    vect_t r_dir_unit;

    VMOVE(r_dir_unit, rd->rp->r_dir);
    VUNITIZE(r_dir_unit);

    status = bn_isect_line_lseg(&dist_along_ray,
				rd->rp->r_pt, rd->rp->r_dir,
				eu_p->vu_p->v_p->vg_p->coord,
				eu_p->eumate_p->vu_p->v_p->vg_p->coord,
				rd->tol);

    switch (status) {
	case -4 :
	    /* Zero length edge.  The routine bn_isect_line_lseg()
	     * can't help us.  Intersect the ray with each vertex.  If
	     * either vertex is hit, then record that the edge has
	     * sub-elements which where hit.  Otherwise, record the
	     * edge as being missed.
	     */

	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("\t - Zero length edge\n");

	    vhit1 = isect_ray_vertexuse(rd, eu_p->vu_p);
	    vhit2 = isect_ray_vertexuse(rd, eu_p->eumate_p->vu_p);

	    NMG_GET_HITMISS(myhit, rd->ap);
	    NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);

	    myhit->hit.hit_private = (genptr_t)eu_p->e_p;

	    if (vhit1 || vhit2) {
		/* we hit the vertex */
		BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC);
	    } else {
		/* both vertecies were missed, so edge is missed */
		BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
	    }
	    BU_LIST_INSERT(&rd->rd_miss, &myhit->l);

	    break;
	case -3 :	/* fallthrough */
	case -2 :
	    /* The ray misses the edge segment, but hits the infinite
	     * line of the edge to one end of the edge segment.  This
	     * is an exercise in tabulating the nature of the miss.
	     */

	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("\t - Miss edge, \"hit\" line\n");
	    /* record the fact that we missed each vertex */
	    (void)ray_miss_vertex(rd, eu_p->vu_p);
	    (void)ray_miss_vertex(rd, eu_p->eumate_p->vu_p);

	    /* record the fact that we missed the edge */
	    NMG_GET_HITMISS(myhit, rd->ap);
	    NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);
	    myhit->hit.hit_private = (genptr_t)eu_p->e_p;

	    BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
	    BU_LIST_INSERT(&rd->rd_miss, &myhit->l);

	    break;
	case -1 : /* just plain missed the edge/line */
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("\t - Miss edge/line\n");

	    ray_miss_vertex(rd, eu_p->vu_p);
	    ray_miss_vertex(rd, eu_p->eumate_p->vu_p);

	    NMG_GET_HITMISS(myhit, rd->ap);
	    NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);
	    myhit->hit.hit_private = (genptr_t)eu_p->e_p;

	    BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
	    BU_LIST_INSERT(&rd->rd_miss, &myhit->l);

	    break;
	case 0 :  /* oh joy.  Lines are co-linear */
	    HIT_EDGE_VERTEX(rd, eu_p, eu_p->vu_p);
	    HIT_EDGE_VERTEX(rd, eu_p, eu_p->eumate_p->vu_p);
	    colinear_edge_ray(rd, eu_p);
	    break;
	case 1 :
	    HIT_EDGE_VERTEX(rd, eu_p, eu_p->vu_p);
	    break;
	case 2 :
	    HIT_EDGE_VERTEX(rd, eu_p, eu_p->eumate_p->vu_p);
	    break;
	case 3 : {
	    /* a hit on an edge */
	    point_t pt;

	    VJOIN1(pt, rd->rp->r_pt, dist_along_ray, r_dir_unit);
	    ray_hit_edge(rd, eu_p, dist_along_ray, pt);

	    break;
	}
    }
}


/**
 * I S E C T _ R A Y _ E D G E U S E
 *
 * Intersect ray with edgeuse.  If they pass within tolerance, a hit
 * is generated.
 */
HIDDEN void
isect_ray_edgeuse(struct ray_data *rd, struct edgeuse *eu_p)
{
    struct hitmiss *myhit;

    NMG_CK_EDGEUSE(eu_p);
    NMG_CK_EDGEUSE(eu_p->eumate_p);
    NMG_CK_EDGE(eu_p->e_p);
    NMG_CK_VERTEXUSE(eu_p->vu_p);
    NMG_CK_VERTEXUSE(eu_p->eumate_p->vu_p);
    NMG_CK_VERTEX(eu_p->vu_p->v_p);
    NMG_CK_VERTEX(eu_p->eumate_p->vu_p->v_p);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	bu_log("isect_ray_edgeuse (%g %g %g) -> (%g %g %g)",
	       eu_p->vu_p->v_p->vg_p->coord[0],
	       eu_p->vu_p->v_p->vg_p->coord[1],
	       eu_p->vu_p->v_p->vg_p->coord[2],
	       eu_p->eumate_p->vu_p->v_p->vg_p->coord[0],
	       eu_p->eumate_p->vu_p->v_p->vg_p->coord[1],
	       eu_p->eumate_p->vu_p->v_p->vg_p->coord[2]);
    }

    if (eu_p->e_p != eu_p->eumate_p->e_p)
	bu_bomb("isect_ray_edgeuse() edgeuse mate has step-father\n");

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("\n\tLooking for previous hit on edge 0x%08x ...\n",
	       eu_p->e_p);

    myhit = NMG_INDEX_GET(rd->hitmiss, eu_p->e_p);
    if (myhit) {
	if (BU_LIST_MAGIC_EQUAL((struct bu_list *)myhit, NMG_RT_HIT_MAGIC)) {
	    /* previously hit vertex or edge */
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("\tedge previously hit\n");
	    return;
	} else if (BU_LIST_MAGIC_EQUAL((struct bu_list *)myhit, NMG_RT_HIT_SUB_MAGIC)) {
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("\tvertex of edge previously hit\n");
	    return;
	} else if (BU_LIST_MAGIC_EQUAL((struct bu_list *)myhit, NMG_RT_MISS_MAGIC)) {
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("\tedge previously missed\n");
	    return;
	} else {
	    nmg_bu_bomb(rd, "what happened?\n");
	}
    }

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("\t No previous hit\n");

    if (!eu_p->g.magic_p)
	isect_ray_lseg(rd, eu_p);
    else {
	switch (*eu_p->g.magic_p) {
	    case NMG_EDGE_G_LSEG_MAGIC:
		isect_ray_lseg(rd, eu_p);
		break;
	    case NMG_EDGE_G_CNURB_MAGIC:
		/* not implemented */
		break;
	}
    }
}


/**
 * I S E C T _ R A Y _ L O O P U S E
 */
HIDDEN void
isect_ray_loopuse(struct ray_data *rd, struct loopuse *lu_p)
{
    struct edgeuse *eu_p;

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("isect_ray_loopuse(0x%08x, loop:0x%08x)\n", rd, lu_p->l_p);

    NMG_CK_LOOPUSE(lu_p);
    NMG_CK_LOOP(lu_p->l_p);
    NMG_CK_LOOP_G(lu_p->l_p->lg_p);

    if (BU_LIST_FIRST_MAGIC(&lu_p->down_hd) == NMG_EDGEUSE_MAGIC) {
	for (BU_LIST_FOR(eu_p, edgeuse, &lu_p->down_hd)) {
	    isect_ray_edgeuse(rd, eu_p);
	}
	return;

    } else if (BU_LIST_FIRST_MAGIC(&lu_p->down_hd)!=NMG_VERTEXUSE_MAGIC) {
	bu_log("in %s at %d", __FILE__, __LINE__);
	nmg_bu_bomb(rd, " bad loopuse child magic");
    }

    /* loopuse child is vertexuse */

    (void) isect_ray_vertexuse(rd, BU_LIST_FIRST(vertexuse, &lu_p->down_hd));
}


HIDDEN void
eu_touch_func(struct edgeuse *eu, fastf_t *pt, char *priv)
{
    struct edgeuse *eu_next;
    struct ray_data *rd;

    NMG_CK_EDGEUSE(eu);
    NMG_CK_EDGEUSE(eu->eumate_p);
    NMG_CK_EDGE(eu->e_p);
    NMG_CK_VERTEXUSE(eu->vu_p);
    NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
    NMG_CK_VERTEX(eu->vu_p->v_p);
    NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);

    eu_next = BU_LIST_PNEXT_CIRC(edgeuse, eu);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("eu_touch(%g %g %g -> %g %g %g\n",
	       eu->vu_p->v_p->vg_p->coord[0],
	       eu->vu_p->v_p->vg_p->coord[1],
	       eu->vu_p->v_p->vg_p->coord[2],
	       eu_next->vu_p->v_p->vg_p->coord[0],
	       eu_next->vu_p->v_p->vg_p->coord[1],
	       eu_next->vu_p->v_p->vg_p->coord[2]);

    rd = (struct ray_data *)priv;
    rd->face_subhit = 1;

    ray_hit_edge(rd, eu, rd->ray_dist_to_plane, pt);
}


HIDDEN void
vu_touch_func(struct vertexuse *vu, fastf_t *UNUSED(pt), char *priv)
{
    struct ray_data *rd;

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("vu_touch(%g %g %g)\n",
	       vu->v_p->vg_p->coord[0],
	       vu->v_p->vg_p->coord[1],
	       vu->v_p->vg_p->coord[2]);

    rd = (struct ray_data *)priv;

    rd->face_subhit = 1;
    ray_hit_vertex(rd, vu, NMG_VERT_UNKNOWN);
}


HIDDEN void
record_face_hit(struct ray_data *rd, struct hitmiss *myhit, fastf_t *plane_pt, double dist, struct faceuse *fu_p, fastf_t *norm)
{
    double cos_angle;
    vect_t r_dir_unit;

    BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_MAGIC);
    myhit->outbound_use = (long *)fu_p;
    myhit->inbound_use = (long *)fu_p;


    VMOVE(myhit->hit.hit_point, plane_pt);

    /* also rd->ray_dist_to_plane */
    myhit->hit.hit_dist = dist;
    myhit->hit.hit_private = (genptr_t)fu_p->f_p;


    /* compute what the ray-state is before and after this
     * encountering this hit point.
     */
    VMOVE(r_dir_unit, rd->rp->r_dir);
    VUNITIZE(r_dir_unit);
    cos_angle = VDOT(norm, r_dir_unit);
    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	VPRINT("face Normal", norm);
	bu_log("cos_angle wrt ray direction: %g\n", cos_angle);
	bu_log("fu x%x manifoldness = %d\n", fu_p, NMG_MANIFOLDS(rd->manifolds, fu_p));
    }


    if (! (NMG_MANIFOLDS(rd->manifolds, fu_p) & NMG_3MANIFOLD)) {
	myhit->in_out = HMG_HIT_ANY_ANY;

	if (cos_angle < rd->tol->perp) {
	    VMOVE(myhit->inbound_norm, norm);
	    VREVERSE(myhit->outbound_norm, norm);
	    myhit->inbound_use = (long *)fu_p;
	    myhit->outbound_use = (long *)fu_p->fumate_p;
	} else {
	    VREVERSE(myhit->inbound_norm, norm);
	    VMOVE(myhit->outbound_norm, norm);
	    myhit->inbound_use = (long *)fu_p->fumate_p;
	    myhit->outbound_use = (long *)fu_p;
	}

	return;
    }


    switch (fu_p->orientation) {
	case OT_SAME:
	    if (BN_VECT_ARE_PERP(cos_angle, rd->tol)) {
		/* perpendicular? */
		bu_log("%s[%d]: Ray is in plane of face?\n",
		       __FILE__, __LINE__);
		bu_bomb("record_face_hit() I quit\n");
	    } else if (cos_angle > SMALL_FASTF) {
		myhit->in_out = HMG_HIT_IN_OUT;
		VREVERSE(myhit->outbound_norm, norm);
		myhit->outbound_use = (long *)fu_p;
		myhit->inbound_use = (long *)fu_p;
	    } else {
		myhit->in_out = HMG_HIT_OUT_IN;
		VMOVE(myhit->inbound_norm, norm);
		myhit->inbound_use = (long *)fu_p;
		myhit->outbound_use = (long *)fu_p;
	    }
	    break;
	case OT_OPPOSITE:
	    if (BN_VECT_ARE_PERP(cos_angle, rd->tol)) {
		/* perpendicular? */
		bu_log("%s[%d]: Ray is in plane of face?\n",
		       __FILE__, __LINE__);
		bu_bomb("record_face_hit() I quit\n");
	    } else if (cos_angle > SMALL_FASTF) {
		myhit->in_out = HMG_HIT_OUT_IN;
		VREVERSE(myhit->inbound_norm, norm);
		myhit->inbound_use = (long *)fu_p;
		myhit->outbound_use = (long *)fu_p;
	    } else {
		myhit->in_out = HMG_HIT_IN_OUT;
		VMOVE(myhit->outbound_norm, norm);
		myhit->inbound_use = (long *)fu_p;
		myhit->outbound_use = (long *)fu_p;
	    }
	    break;
	default:
	    bu_log("%s %d:face orientation not SAME/OPPOSITE\n",
		   __FILE__, __LINE__);
	    bu_bomb("record_face_hit() Crash and burn\n");
    }

    hit_ins(rd, myhit);
    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	nmg_rt_isect_plfu(fu_p, rd->rp->r_pt, myhit->hit.hit_point);

    /* sanity check */
    NMG_CK_HITMISS(myhit);
}


/**
 * this is the UV-space tolerance for the NURB intersector, a larger
 * number will improve speed, but too large will produce errors.
 * Perhaps a routine to calculate an appropriate UV_TOL could be
 * constructed.
 */
#define UV_TOL 1.0e-6

void
isect_ray_snurb_face(struct ray_data *rd, struct faceuse *fu, struct face_g_snurb *fg)
{
    plane_t pl, pl1, pl2;
    struct rt_nurb_uv_hit *hp;
    struct bu_list bezier;
    struct bu_list hit_list;
    struct face_g_snurb *srf;
    struct face *f;

    NMG_CK_RD(rd);
    NMG_CK_FACE_G_SNURB(fg);
    NMG_CK_FACEUSE(fu);

    f = fu->f_p;

    /* calculate two orthogonal planes that intersect along ray */
    bn_vec_ortho(pl2, rd->rp->r_dir);
    VCROSS(pl1, pl2, rd->rp->r_dir);
    pl1[W] = VDOT(rd->rp->r_pt, pl1);
    pl2[W] = VDOT(rd->rp->r_pt, pl2);

    BU_LIST_INIT(&bezier);
    BU_LIST_INIT(&hit_list);

    rt_nurb_bezier(&bezier, fg, rd->ap->a_resource);

    while (BU_LIST_NON_EMPTY(&bezier)) {
	point_t srf_min, srf_max;
	int planar;
	point_t hit;
	fastf_t dist = (fastf_t)0.0;

	VSETALL(hit, 0);

	srf = BU_LIST_FIRST(face_g_snurb,  &bezier);
	BU_LIST_DEQUEUE(&srf->l);

	/* calculate intersection points on NURB surface (in uv space) */
	/* check if NURB surface is a simple planar surface */
	if (srf->order[0] == 2 && srf->order[1] ==2 && srf->s_size[0] ==2 && srf->s_size[1] == 2)
	    planar = 1;
	else
	    planar = 0;

	if (planar) {
	    vect_t u_dir, v_dir;
	    point_t ctl_pt[4];
	    vect_t hit_dir;
	    int i, j;
	    int rational;
	    int coords;
	    fastf_t *pt;
	    fastf_t u, v;

	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("isect_ray_snurb_face: face is planar\n");

	    rational = RT_NURB_IS_PT_RATIONAL(srf->pt_type);
	    coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);

	    pt = srf->ctl_points;
	    for (i=0; i<4; i++) {
		for (j=0; j<coords; j++) {
		    ctl_pt[i][j] = *pt;
		    pt++;
		}
		if (rational) {
		    for (j=0; j<coords-1; j++)
			ctl_pt[i][j] = ctl_pt[i][j]/ctl_pt[i][coords-1];
		}
	    }
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		for (i=0; i<4; i++)
		    bu_log("\tctl_point[%d] = (%g %g %g)\n", i, V3ARGS(ctl_pt[i]));
		bu_log("uv range (%g %g) <-> (%g %g)\n", srf->u.knots[0], srf->v.knots[0], srf->u.knots[srf->u.k_size-1], srf->v.knots[srf->v.k_size-1]);
	    }

	    VSUB2(u_dir, ctl_pt[1], ctl_pt[0]);
	    VSUB2(v_dir, ctl_pt[2], ctl_pt[0]);
	    VCROSS(pl, u_dir, v_dir);
	    VUNITIZE(pl);
	    pl[W] = VDOT(pl, ctl_pt[0]);
	    hp = (struct rt_nurb_uv_hit *)NULL;
	    if (bn_isect_line3_plane(&dist,  rd->rp->r_pt,  rd->rp->r_dir, pl, rd->tol) <= 0) {
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("\tNo intersection\n");

		rt_nurb_free_snurb(srf, rd->ap->a_resource);
		continue;
	    }

	    VJOIN1(hit, rd->rp->r_pt, dist, rd->rp->r_dir);
	    VSUB2(hit_dir, hit, ctl_pt[0]);
	    u = srf->u.knots[0] + (srf->u.knots[srf->u.k_size-1] - srf->u.knots[0]) * VDOT(hit_dir, u_dir) / MAGSQ(u_dir);
	    v = srf->v.knots[0] + (srf->v.knots[srf->v.k_size-1] - srf->v.knots[0]) * VDOT(hit_dir, v_dir) / MAGSQ(v_dir);

	    if (u >= srf->u.knots[0] && u <= srf->u.knots[srf->u.k_size-1] &&
		v >= srf->v.knots[0] && v <= srf->v.knots[srf->v.k_size-1])
	    {
		hp = (struct rt_nurb_uv_hit *)bu_calloc(1, sizeof(struct rt_nurb_uv_hit), "struct rt_nurb_uv_hit");
		hp->next = (struct rt_nurb_uv_hit *)NULL;
		hp->sub = 0;
		hp->u = u;
		hp->v = v;

		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("\thit at uv=(%g %g), xyz=(%g %g %g)\n", hp->u, hp->v, V3ARGS(hit));
	    }
	} else {
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("isect_ray_snurb_face: using planes (%g %g %g %g) (%g %g %g %g)\n",
		       V4ARGS(pl1), V4ARGS(pl2));
	    (void)rt_nurb_s_bound(srf, srf_min, srf_max);
	    if (!rt_in_rpp(rd->rp, rd->rd_invdir, srf_min, srf_max)) {
		rt_nurb_free_snurb(srf, rd->ap->a_resource);
		continue;
	    }
	    hp = rt_nurb_intersect(srf, pl1, pl2, UV_TOL, rd->ap->a_resource, NULL);
	}

	/* process each hit point */
	while (hp != (struct rt_nurb_uv_hit *)NULL) {
	    struct rt_nurb_uv_hit *next;
	    struct hitmiss *myhit;
	    vect_t to_hit;
	    fastf_t dot;
	    fastf_t homo_hit[4];
	    int ot_sames, ot_opps;
	    struct loopuse *lu;

	    next = hp->next;

	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("\tintersect snurb surface at uv=(%g %g)\n", hp->u, hp->v);

	    /* check if point is in face (trimming curves) */
	    ot_sames = 0;
	    ot_opps = 0;

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		if (lu->orientation == OT_SAME)
		    ot_sames += nmg_uv_in_lu(hp->u, hp->v, lu);
		else if (lu->orientation == OT_OPPOSITE)
		    ot_opps += nmg_uv_in_lu(hp->u, hp->v, lu);
		else {
		    bu_log("isect_ray_snurb_face: lu orientation = %s!!\n",
			   nmg_orientation(lu->orientation));
		    bu_bomb("isect_ray_snurb_face: bad lu orientation\n");
		}
	    }

	    if (ot_sames == 0 || ot_opps == ot_sames) {
		/* not a hit */

		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		    bu_log("\tNot a hit\n");

		bu_free((char *)hp, "hit");
		hp = next;
		continue;
	    }

	    NMG_GET_HITMISS(myhit, rd->ap);
	    NMG_INDEX_ASSIGN(rd->hitmiss, fu->f_p, myhit);
	    myhit->hit.hit_private = (genptr_t)fu->f_p;
	    myhit->inbound_use = myhit->outbound_use = (long *)&fu->l.magic;

	    /* calculate actual hit point (x y z) */
	    if (planar) {
		VMOVE(myhit->hit.hit_point, hit);
		myhit->hit.hit_dist = dist;
		VMOVE(myhit->hit.hit_normal, pl);
	    } else {
		rt_nurb_s_eval(srf, hp->u, hp->v, homo_hit);
		if (RT_NURB_IS_PT_RATIONAL(srf->pt_type)) {
		    fastf_t inv_homo;

		    inv_homo = 1.0/homo_hit[W];
		    VSCALE(myhit->hit.hit_point, homo_hit, inv_homo);
		} else
		    VMOVE(myhit->hit.hit_point, homo_hit);

		VSUB2(to_hit, myhit->hit.hit_point, rd->rp->r_pt);
		myhit->hit.hit_dist = VDOT(to_hit, rd->rp->r_dir);

		/* get surface normal */
		rt_nurb_s_norm(srf, hp->u, hp->v, myhit->hit.hit_normal);
	    }

	    /* may need to reverse it */
	    if (f->flip)
		VREVERSE(myhit->hit.hit_normal, myhit->hit.hit_normal);

	    dot = VDOT(myhit->hit.hit_normal, rd->rp->r_dir);

	    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		bu_log("\thit at dist = %g (%g %g %g), norm = (%g %g %g)\n",
		       myhit->hit.hit_dist,
		       V3ARGS(myhit->hit.hit_point),
		       V3ARGS(myhit->hit.hit_normal));
		if (dot > SMALL_FASTF)
		    bu_log("\t\tdot = %g (exit point)\n", dot);
		else
		    bu_log("\t\tdot = %g (entrance point)\n", dot);
	    }

	    switch (fu->orientation) {
		case OT_SAME:
		    if (BN_VECT_ARE_PERP(dot, rd->tol)) {
			/* perpendicular? */
			bu_log("%s[%d]: Ray is in plane of face?\n",
			       __FILE__, __LINE__);
			bu_bomb("record_face_hit() I quit\n");
		    } else if (dot > SMALL_FASTF) {
			myhit->in_out = HMG_HIT_IN_OUT;
			VMOVE(myhit->outbound_norm, myhit->hit.hit_normal);
			myhit->outbound_use = (long *)fu;
			myhit->inbound_use = (long *)fu;
		    } else {
			myhit->in_out = HMG_HIT_OUT_IN;
			VMOVE(myhit->inbound_norm, myhit->hit.hit_normal);
			myhit->inbound_use = (long *)fu;
			myhit->outbound_use = (long *)fu;
		    }
		    break;
		case OT_OPPOSITE:
		    if (BN_VECT_ARE_PERP(dot, rd->tol)) {
			/* perpendicular? */
			bu_log("%s[%d]: Ray is in plane of face?\n",
			       __FILE__, __LINE__);
			bu_bomb("record_face_hit() I quit\n");
		    } else if (dot > SMALL_FASTF) {
			myhit->in_out = HMG_HIT_OUT_IN;
			VREVERSE(myhit->inbound_norm, myhit->hit.hit_normal);
			myhit->inbound_use = (long *)fu;
			myhit->outbound_use = (long *)fu;
		    } else {
			myhit->in_out = HMG_HIT_IN_OUT;
			VREVERSE(myhit->outbound_norm, myhit->hit.hit_normal);
			myhit->inbound_use = (long *)fu;
			myhit->outbound_use = (long *)fu;
		    }
		    break;
		default:
		    bu_log("%s %d:face orientation not SAME/OPPOSITE\n",
			   __FILE__, __LINE__);
		    bu_bomb("record_face_hit() Crash and burn\n");
	    }

	    hit_ins(rd, myhit);

	    bu_free((char *)hp, "hit");
	    hp = next;
	}
	rt_nurb_free_snurb(srf, rd->ap->a_resource);
    }
}


HIDDEN void
isect_ray_planar_face(struct ray_data *rd, struct faceuse *fu_p)
{
    plane_t norm;
    fastf_t dist;
    struct hitmiss *myhit;
    point_t plane_pt;
    struct loopuse *lu_p;
    int pt_class;

    /* the geometric intersection of the ray with the plane of the
     * face has already been done by isect_ray_faceuse().
     */
    NMG_GET_FU_PLANE(norm, fu_p);

    /* ray hits plane:
     *
     * Get the ray/plane intersection point.  Then compute whether
     * this point lies within the area of the face.
     */

    VMOVE(plane_pt, rd->plane_pt);
    dist = rd->ray_dist_to_plane;

    if (DIST_PT_PLANE(plane_pt, norm) > rd->tol->dist) {
	bu_log("%s:%d plane_pt (%g %g %g) @ dist (%g)out of tolerance\n",
	       __FILE__, __LINE__, V3ARGS(plane_pt), dist);
	bu_bomb("isect_ray_planar_face() dist out of tol\n");
    }

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	double new_dist;
	bu_log("\tray (%16.10e %16.10e %16.10e) (-> %16.10e %16.10e %16.10e)\n",
	       rd->rp->r_pt[0],
	       rd->rp->r_pt[1],
	       rd->rp->r_pt[2],
	       rd->rp->r_dir[0],
	       rd->rp->r_dir[1],
	       rd->rp->r_dir[2]);
	bu_log("\tplane/ray intersection point (%16.10e %16.10e %16.10e)\n",
	       V3ARGS(plane_pt));
	bu_log("\tdistance along ray to intersection point %16.10e\n", dist);

	new_dist=DIST_PT_PLANE(plane_pt, norm);

	bu_log("\tDIST_PT_PLANE(%16.10e) 0x%08lx 0x%08lx\n", new_dist, plane_pt, norm);

	bn_isect_line3_plane(&new_dist, plane_pt, rd->rp->r_dir,
			     norm, rd->tol);

	bu_log("Normal %16.10e %16.10e %16.10e %16.10e)\n",
	       V4ARGS(norm));
	bu_log("recalculated plane/pt dist as %16.10e 0x%08lx 0x%08lx\n", new_dist, plane_pt, norm);
	bu_log("distance tol = %16.10e\n", rd->tol->dist);
    }


    /* determine if the plane point is in or out of the face, and if
     * it is within tolerance of any of the elements of the faceuse.
     *
     * The value of "rd->face_subhit" will be set non-zero if either
     * eu_touch_func or vu_touch_func is called.  They will be called
     * when an edge/vertex of the face is within tolerance of
     * plane_pt.
     */
    rd->face_subhit = 0;
    rd->ray_dist_to_plane = dist;
    if (rd->classifying_ray)
	pt_class = nmg_class_pt_fu_except(plane_pt, fu_p, (struct loopuse *)NULL,
					  0, 0, (char *)rd, NMG_FPI_PERGEOM, 1,
					  rd->tol);
    else
	pt_class = nmg_class_pt_fu_except(plane_pt, fu_p, (struct loopuse *)NULL,
					  eu_touch_func, vu_touch_func, (char *)rd, NMG_FPI_PERGEOM, 0,
					  rd->tol);


    NMG_GET_HITMISS(myhit, rd->ap);
    NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
    myhit->hit.hit_private = (genptr_t)fu_p->f_p;
    myhit->hit.hit_surfno = fu_p->f_p->index;
    myhit->inbound_use = myhit->outbound_use = (long *)&fu_p->l.magic;


    switch (pt_class) {
	case NMG_CLASS_Unknown	:
	    bu_log("%s[line:%d] ray/plane intercept point cannot be classified wrt face\n",
		   __FILE__, __LINE__);
	    bu_bomb("isect_ray_planar_face() class unknown\n");
	    break;
	case NMG_CLASS_AinB	:
	case NMG_CLASS_AonBshared :
	    /* if a sub-element of the face was within tolerance of
	     * the plane intercept, then a hit has already been
	     * recorded for that element, and we do NOT need to
	     * generate one for the face.
	     */
	    if (rd->face_subhit) {
		BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC);
		VMOVE(myhit->hit.hit_point, plane_pt);
		/* also rd->ray_dist_to_plane */
		myhit->hit.hit_dist = dist;

		myhit->hit.hit_private = (genptr_t)fu_p->f_p;
		BU_LIST_INSERT(&rd->rd_miss, &myhit->l);
		NMG_CK_HITMISS(myhit);
	    } else {
		/* The plane_pt was NOT within tolerance of a
		 * sub-element, but it WAS within the area of the
		 * face.  We need to record a hit on the face
		 */
		record_face_hit(rd, myhit, plane_pt, dist, fu_p, norm);
	    }
	    break;
	case NMG_CLASS_AoutB	:
	    BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
	    BU_LIST_INSERT(&rd->rd_miss, &myhit->l);

	    break;
	default	:
	    bu_log("%s[line:%d] BIZZARE ray/plane intercept point classification\n",
		   __FILE__, __LINE__);
	    bu_bomb("isect_ray_planar_face() Bizz\n");
    }

    /* intersect the ray with the edges/verticies of the face */
    for (BU_LIST_FOR(lu_p, loopuse, &fu_p->lu_hd))
	isect_ray_loopuse(rd, lu_p);
}


/**
 * I S E C T _ R A Y _ F A C E U S E
 *
 * check to see if ray hits face.
 */
HIDDEN void
isect_ray_faceuse(struct ray_data *rd, struct faceuse *fu_p)
{

    struct hitmiss *myhit;
    struct face *fp;
    struct face_g_plane *fgp;
    vect_t r_dir_unit;

    VMOVE(r_dir_unit, rd->rp->r_dir);
    VUNITIZE(r_dir_unit);

    if (fu_p->orientation == OT_OPPOSITE) return;

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("isect_ray_faceuse(0x%08x, faceuse:0x%08x/face:0x%08x)\n",
	       rd, fu_p, fu_p->f_p);

    NMG_CK_FACEUSE(fu_p);
    NMG_CK_FACEUSE(fu_p->fumate_p);
    fp = fu_p->f_p;
    NMG_CK_FACE(fp);


    /* if this face already processed, we are done. */
    myhit = NMG_INDEX_GET(rd->hitmiss, fp);
    if (myhit) {
	if (BU_LIST_MAGIC_EQUAL((struct bu_list *)myhit,
			     NMG_RT_HIT_MAGIC)) {
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log(" previously hit\n");
	} else if (BU_LIST_MAGIC_EQUAL((struct bu_list *)myhit,
				    NMG_RT_HIT_SUB_MAGIC)) {
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log(" previously hit sub-element\n");
	} else if (BU_LIST_MAGIC_EQUAL((struct bu_list *)myhit,
				    NMG_RT_MISS_MAGIC)) {
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log(" previously missed\n");
	} else {
	    bu_log("%s %d:\n\tBad magic %ld (0x%08x) for hitmiss struct for faceuse 0x%08x\n",
		   __FILE__, __LINE__,
		   myhit->l.magic, myhit->l.magic, fu_p);
	    nmg_bu_bomb(rd, "Was I hit or not?\n");
	}
	return;
    }


    /* bounding box intersection */
    if (*fp->g.magic_p == NMG_FACE_G_PLANE_MAGIC) {
	int code;
	fastf_t dist;
	point_t hit_pt;

	fgp = fu_p->f_p->g.plane_p;
	NMG_CK_FACE_G_PLANE(fgp);

	code = bn_isect_line3_plane(&dist, rd->rp->r_pt, rd->rp->r_dir, fgp->N, rd->tol);
	if (code < 1) {
	    NMG_GET_HITMISS(myhit, rd->ap);
	    NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
	    myhit->hit.hit_private = (genptr_t)fu_p->f_p;
	    myhit->hit.hit_surfno = fu_p->f_p->index;
	    BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
	    BU_LIST_INSERT(&rd->rd_miss, &myhit->l);

	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("missed bounding box\n");
	    return;
	}
        dist *= MAGNITUDE(rd->rp->r_dir);
	VJOIN1(hit_pt, rd->rp->r_pt, dist, r_dir_unit);
	if (V3PT_OUT_RPP_TOL(hit_pt, fp->min_pt, fp->max_pt, rd->tol)) {
	    NMG_GET_HITMISS(myhit, rd->ap);
	    NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
	    myhit->hit.hit_private = (genptr_t)fu_p->f_p;
	    myhit->hit.hit_surfno = fu_p->f_p->index;
	    BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
	    BU_LIST_INSERT(&rd->rd_miss, &myhit->l);

	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("hit point not within face bounding box\n");
	    return;
	}

	VMOVE(rd->plane_pt, hit_pt);
	rd->ray_dist_to_plane = dist;
    } else if (!rt_in_rpp(rd->rp, rd->rd_invdir,
			  fu_p->f_p->min_pt, fu_p->f_p->max_pt)) {
	NMG_GET_HITMISS(myhit, rd->ap);
	NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
	myhit->hit.hit_private = (genptr_t)fu_p->f_p;
	myhit->hit.hit_surfno = fu_p->f_p->index;
	BU_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
	BU_LIST_INSERT(&rd->rd_miss, &myhit->l);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	    bu_log("missed bounding box\n");
	return;
    }

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) bu_log(" hit bounding box \n");

    switch (*fu_p->f_p->g.magic_p) {
	case NMG_FACE_G_PLANE_MAGIC:
	    isect_ray_planar_face(rd, fu_p);
	    break;
	case NMG_FACE_G_SNURB_MAGIC:
	    isect_ray_snurb_face(rd, fu_p, fu_p->f_p->g.snurb_p);
	    break;
    }
}


/** I S E C T _ R A Y _ S H E L L
 *
 * Implicit return: adds hit points to the hit-list "hl"
 */
HIDDEN void
nmg_isect_ray_shell(struct ray_data *rd, const struct shell *s_p)
{
    struct faceuse *fu_p;
    struct loopuse *lu_p;
    struct edgeuse *eu_p;

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("nmg_isect_ray_shell(0x%08x, 0x%08x)\n", rd, s_p);

    NMG_CK_SHELL(s_p);
    NMG_CK_SHELL_A(s_p->sa_p);

    /* does ray isect shell rpp ?  if not, we can just return.  there
     * is no need to record the miss for the shell, as there is only
     * one "use" of a shell.
     */
    if (!rt_in_rpp(rd->rp, rd->rd_invdir,
		   s_p->sa_p->min_pt, s_p->sa_p->max_pt)) {
	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	    bu_log("nmg_isect_ray_shell(no RPP overlap) 0x%08x, 0x%08x\n", rd, s_p);
	return;
    }

    /* ray intersects shell, check sub-objects */

    for (BU_LIST_FOR(fu_p, faceuse, &(s_p->fu_hd)))
	isect_ray_faceuse(rd, fu_p);

    for (BU_LIST_FOR(lu_p, loopuse, &(s_p->lu_hd)))
	isect_ray_loopuse(rd, lu_p);

    for (BU_LIST_FOR(eu_p, edgeuse, &(s_p->eu_hd)))
	isect_ray_edgeuse(rd, eu_p);

    if (s_p->vu_p)
	(void)isect_ray_vertexuse(rd, s_p->vu_p);

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("nmg_isect_ray_shell(done) 0x%08x, 0x%08x\n", rd, s_p);
}


/**
 * N M G _ I S E C T _ R A Y _ M O D E L
 */
void
nmg_isect_ray_model(struct ray_data *rd)
{
    struct nmgregion *r_p;
    struct shell *s_p;

    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
	bu_log("isect_ray_nmg: Pnt(%g %g %g) Dir(%g %g %g)\n",
	       rd->rp->r_pt[0],
	       rd->rp->r_pt[1],
	       rd->rp->r_pt[2],
	       rd->rp->r_dir[0],
	       rd->rp->r_dir[1],
	       rd->rp->r_dir[2]);

    NMG_CK_MODEL(rd->rd_m);
    NMG_CK_HITMISS_LISTS(rd);

    /* Caller has assured us that the ray intersects the nmg model,
     * check ray for intersecion with rpp's of nmgregion's
     */
    for (BU_LIST_FOR(r_p, nmgregion, &rd->rd_m->r_hd)) {
	NMG_CK_REGION(r_p);
	NMG_CK_REGION_A(r_p->ra_p);

	/* does ray intersect nmgregion rpp? */
	if (! rt_in_rpp(rd->rp, rd->rd_invdir,
			r_p->ra_p->min_pt, r_p->ra_p->max_pt))
	    continue;

	/* ray intersects region, check shell intersection */
	for (BU_LIST_FOR(s_p, shell, &r_p->s_hd)) {
	    nmg_isect_ray_shell(rd, s_p);
	}
    }

    if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
	if (BU_LIST_IS_EMPTY(&rd->rd_hit)) {
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		bu_log("ray missed NMG\n");
	} else {
	    if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		nmg_rt_print_hitlist((struct hitmiss*)&rd->rd_hit);
	}
    }

    /* sanity check */
    NMG_CK_HITMISS_LISTS(rd);
}


/**
 * N M G _ P L _ H I T M I S S _ L I S T
 */
void
nmg_pl_hitmiss_list(const char *str, int num, const struct bu_list *hd, const struct xray *rp)
{
    FILE *fp;
    char buf[128];
    struct hitmiss *hmp;
    int count = 0;

    snprintf(buf, 128, "%s%d.pl", str, num);

    if (bu_list_len(hd) <= 0) {
	bu_log("nmg_pl_hitmiss_list(): empty list, no %s written\n", buf);
	return;
    }

    fp = fopen(buf, "wb");
    if (fp == (FILE *)NULL) {
	perror(buf);
	return;
    }

    pl_color(fp, 210, 210, 210);		/* grey ray */

    for (BU_LIST_FOR(hmp, hitmiss, hd)) {
	point_t pt;

	NMG_CK_HITMISS(hmp);

	VJOIN1(pt, rp->r_pt, hmp->hit.hit_dist, rp->r_dir);
	if (count++)
	    pdv_3cont(fp, pt);
	else
	    pdv_3move(fp, pt);
    }
    fclose(fp);
    bu_log("overlay %s\n", buf);
}


HIDDEN int
guess_class_from_hitlist_max(struct ray_data *rd, int *hari_kari, int in_or_out_only)
{
    struct hitmiss *a_hit;
    struct hitmiss *plus_hit = (struct hitmiss *)NULL;
    int pt_class;

    *hari_kari = 0;

    NMG_CK_RD(rd);
    if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
	bu_log("plus guessing\n");

    for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)) {
	NMG_CK_HITMISS(a_hit);

	if (!in_or_out_only) {
	    /* if we've got a zero distance hit, that clinches it */
            if (NEAR_ZERO(a_hit->hit.hit_dist, rd->tol->dist)) {
		if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
		    bu_log("guess_class_from_hitlist_min() returns NMG_CLASS_AonBshared for 0 dist hit\n");
		return NMG_CLASS_AonBanti;
	    }

	    if (a_hit->hit.hit_dist < -rd->tol->dist)
		continue;
	} else if (a_hit->hit.hit_dist < rd->tol->dist)
	    continue;

	if (a_hit->in_out == HMG_HIT_ANY_ANY)
	    continue;

	if (plus_hit == (struct hitmiss *)NULL) {
	    if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
		bu_log("plus hit = %g (%s)\n", a_hit->hit.hit_dist,
		       nmg_rt_inout_str(a_hit->in_out));
	    plus_hit = a_hit;
	    *hari_kari = 0;
	} else if (a_hit->hit.hit_dist < plus_hit->hit.hit_dist) {
	    if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
		bu_log("plus hit = %g (%s)\n", a_hit->hit.hit_dist,
		       nmg_rt_inout_str(a_hit->in_out));
	    plus_hit = a_hit;
	    *hari_kari = 0;
	} else if (ZERO(a_hit->hit.hit_dist - plus_hit->hit.hit_dist)) {
	    *hari_kari = 1;
	}
    }

    /* XXX This needs to be resolved with parity */
    if (*hari_kari) {
	if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
	    bu_log("Contemplating Hari Kari\n");
	return NMG_CLASS_Unknown;
    }
    /* figure out what the status is from plus_hit */
    if (plus_hit) {
	switch (plus_hit->in_out) {
	    case HMG_HIT_IN_IN:
	    case HMG_HIT_IN_OUT:
	    case HMG_HIT_IN_ON:
		pt_class = NMG_CLASS_AinB;
		break;
	    case HMG_HIT_OUT_IN:
	    case HMG_HIT_OUT_OUT:
	    case HMG_HIT_OUT_ON:
		pt_class = NMG_CLASS_AoutB;
		break;
	    case HMG_HIT_ON_IN:
	    case HMG_HIT_ON_ON:
	    case HMG_HIT_ON_OUT:
		pt_class = NMG_CLASS_AonBanti;
		break;
	    default:
		pt_class = NMG_CLASS_Unknown;
		break;
	}
    } else {
	/* since we didn't hit anything in the positive direction,
	 * we've got to be outside, since we don't allow infinite
	 * NMG's
	 */
	if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
	    bu_log("Nothing in the plus direction\n");
	pt_class = NMG_CLASS_AoutB;
    }

    return pt_class;
}


HIDDEN int
guess_class_from_hitlist_min(struct ray_data *rd, int *hari_kari, int in_or_out_only)
{
    struct hitmiss *a_hit;
    struct hitmiss *minus_hit = (struct hitmiss *)NULL;
    int pt_class;

    *hari_kari = 0;

    NMG_CK_RD(rd);
    if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
	bu_log("minus guessing\n");

    for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)) {
	NMG_CK_HITMISS(a_hit);

	if (!in_or_out_only) {
	    /* if we've got a zero distance hit, that clinches it */
            if (NEAR_ZERO(a_hit->hit.hit_dist, rd->tol->dist)) {
		if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
		    bu_log("guess_class_from_hitlist_min() returns NMG_CLASS_AonBshared for 0 dist hit\n");
		return NMG_CLASS_AonBanti;
	    }

	    if (a_hit->hit.hit_dist > rd->tol->dist)
		continue;
	} else if (a_hit->hit.hit_dist > -(rd->tol->dist))
	    continue;

	if (a_hit->in_out == HMG_HIT_ANY_ANY)
	    continue;

	if (minus_hit == (struct hitmiss *)NULL) {
	    if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
		bu_log("minus hit = %g (%s)\n", a_hit->hit.hit_dist,
		       nmg_rt_inout_str(a_hit->in_out));
	    minus_hit = a_hit;
	    *hari_kari = 0;
	} else if (a_hit->hit.hit_dist > minus_hit->hit.hit_dist) {
	    if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
		bu_log("minus hit = %g (%s)\n", a_hit->hit.hit_dist,
		       nmg_rt_inout_str(a_hit->in_out));
	    minus_hit = a_hit;
	    *hari_kari = 0;
	} else if (ZERO(a_hit->hit.hit_dist - minus_hit->hit.hit_dist)) {
	    *hari_kari = 1;
	}
    }

    /* XXX This needs to be resolved with parity */
    if (*hari_kari) {
	if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
	    bu_log("Contemplating Hari Kari\n");
	return NMG_CLASS_Unknown;
    }

    /* figure out what the status is from plus_hit */
    if (minus_hit) {
	switch (minus_hit->in_out) {
	    case HMG_HIT_IN_IN:
	    case HMG_HIT_OUT_IN:
	    case HMG_HIT_ON_IN:
		pt_class = NMG_CLASS_AinB;
		break;
	    case HMG_HIT_OUT_OUT:
	    case HMG_HIT_IN_OUT:
	    case HMG_HIT_ON_OUT:
		pt_class = NMG_CLASS_AoutB;
		break;
	    case HMG_HIT_ON_ON:
	    case HMG_HIT_OUT_ON:
	    case HMG_HIT_IN_ON:
		pt_class = NMG_CLASS_AonBanti;
		break;
	    default:
		pt_class = NMG_CLASS_Unknown;
		break;
	}
    } else {
	/* since we didn't hit anything in this direction,
	 * we've got to be outside, since we don't allow infinite
	 * NMG's
	 */
	if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
	    bu_log("Nothing in the minus direction\n");
	pt_class = NMG_CLASS_AoutB;
    }

    return pt_class;
}


/**
 * N M G _ R A Y _ I S E C T _ S H E L L
 *
 * Intended as a support routine for nmg_class_pt_s() in nmg_class.c
 *
 * Intersect a ray with a shell, and return whether the ray start
 * point is inside or outside or ON the shell.  Count the number of
 * crossings (hit points) along the ray, both in the negative and
 * positive directions.  If an even number, point is outside, if an
 * odd number point is inside.  If the negative-going and
 * positive-going assessments don't agree, this is a problem.
 *
 * If "in_or_out_only" is non-zero, then we will not look for a
 * classification of "ON" the shell.
 *
 * The caller must be prepared for a return of NMG_CLASS_Unknown, in
 * which case it should pick a less difficult ray direction to fire
 * and try again.
 *
 * Returns NMG_CLASS_Unknown if we can't tell or NMG_CLASS_xxx for the
 * classification of the pt w.r.t. the shell.
 */
int
nmg_class_ray_vs_shell(struct xray *rp, const struct shell *s, const int in_or_out_only, const struct bn_tol *tol)
{
    struct ray_data rd;
    struct application ap;
    struct hitmiss *a_hit;
    int minus_class, plus_class;
    int hari_kari_minus, hari_kari_plus;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT)) {
	bu_log("nmg_class_ray_vs_shell(pt(%g %g %g), dir(%g %g %g))\n",
	       V3ARGS(rp->r_pt), V3ARGS(rp->r_dir));
    }

    RT_APPLICATION_INIT(&ap);
    ap.a_resource = &rt_uniresource;

    if (!BU_LIST_IS_INITIALIZED(&rt_uniresource.re_nmgfree))
	BU_LIST_INIT(&rt_uniresource.re_nmgfree);

    rd.rd_m = nmg_find_model(&s->l.magic);
    rd.manifolds = nmg_manifolds(rd.rd_m);

    if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT)) {
	struct faceuse *fu;

	bu_log("Manifoldness for shell FU's\n");
	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    if (fu->orientation != OT_SAME)
		continue;

	    bu_log("fu x%x: %d\n", fu, NMG_MANIFOLDS(rd.manifolds, fu));
	}
    }

    /* Compute the inverse of the direction cosines */
    if (!ZERO(rp->r_dir[X])) {
	rd.rd_invdir[X]=1.0/rp->r_dir[X];
    } else {
	rd.rd_invdir[X] = INFINITY;
	rp->r_dir[X] = 0.0;
    }
    if (!ZERO(rp->r_dir[Y])) {
	rd.rd_invdir[Y]=1.0/rp->r_dir[Y];
    } else {
	rd.rd_invdir[Y] = INFINITY;
	rp->r_dir[Y] = 0.0;
    }
    if (!ZERO(rp->r_dir[Z])) {
	rd.rd_invdir[Z]=1.0/rp->r_dir[Z];
    } else {
	rd.rd_invdir[Z] = INFINITY;
	rp->r_dir[Z] = 0.0;
    }

    rd.rp = rp;
    rd.tol = tol;
    rd.ap = &ap;
    rd.stp = (struct soltab *)NULL;
    rd.seghead = (struct seg *)NULL;
    rd.magic = NMG_RAY_DATA_MAGIC;
    rd.hitmiss = (struct hitmiss **)bu_calloc(rd.rd_m->maxindex,
					      sizeof(struct hitmiss *), "nmg geom hit list");
    rd.classifying_ray = 1;

    /* initialize the lists of things that have been hit/missed */
    BU_LIST_INIT(&rd.rd_hit);
    BU_LIST_INIT(&rd.rd_miss);

    nmg_isect_ray_shell(&rd, s);
    NMG_FREE_HITLIST(&rd.rd_miss, &ap);

    /* count the number of hits */
    if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT)) {
	bu_log("%s[%d]: shell Hits:\n", __FILE__, __LINE__);
	for (BU_LIST_FOR(a_hit, hitmiss, &rd.rd_hit)) {
	    if (a_hit->hit.hit_dist > -SMALL_FASTF)
		bu_log("Positive dist hit\n");
	    else
		bu_log("Negative dist hit\n");
	    nmg_rt_print_hitmiss(a_hit);
	}
    }

    if ((rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT)) &&
	(rt_g.NMG_debug & (DEBUG_PLOTEM))) {
	static int num=0;
	nmg_pl_hitmiss_list("shell-ray", num++, &rd.rd_hit, rp);
    }

    minus_class = guess_class_from_hitlist_min(&rd, &hari_kari_minus, in_or_out_only);
    plus_class = guess_class_from_hitlist_max(&rd, &hari_kari_plus, in_or_out_only);

    if (minus_class != plus_class) {
	bu_log("minus_class = (%s)\n", nmg_class_name(minus_class));
	bu_log("plus_class = (%s)\n", nmg_class_name(plus_class));
    }

    /*
     * Rather than blowing up, or guessing, just report that it didn't
     * work, and let caller try another direction.
     */
    if (hari_kari_minus || hari_kari_plus) {
	if (rt_g.NMG_debug)
	    bu_log("hari_kari = %d, %d\n", hari_kari_minus, hari_kari_plus);
	plus_class = NMG_CLASS_Unknown;
	goto out;
    }
    if (plus_class != minus_class || plus_class == NMG_CLASS_Unknown ||
	minus_class == NMG_CLASS_Unknown) {
	plus_class = NMG_CLASS_Unknown;
    }
 out:

    NMG_FREE_HITLIST(&rd.rd_hit, &ap);

    /* free the hitmiss freelist, filled during NMG_FREE_HITLIST */
    if (BU_LIST_IS_INITIALIZED(&rt_uniresource.re_nmgfree)) {
	struct hitmiss *hitp;
	while (BU_LIST_WHILE(hitp, hitmiss, &rt_uniresource.re_nmgfree)) {
	    NMG_CK_HITMISS(hitp);
	    BU_LIST_DEQUEUE((struct bu_list *)hitp);
	    bu_free((genptr_t)hitp, "struct hitmiss");
	}
    }

    /* free the hitmiss table */
    bu_free((char *)rd.hitmiss, "free nmg geom hit list");

    /* free the manifold table */
    bu_free((char *)rd.manifolds, "free manifolds table");

    if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
	bu_log("nmg_class_ray_vs_shell() returns %s(%d)\n",
	       nmg_class_name(plus_class), plus_class);


    return plus_class;
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
