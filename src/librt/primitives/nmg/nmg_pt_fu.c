/*                     N M G _ P T _ F U . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file primitives/nmg/nmg_pt_fu.c
 *
 * Routines for classifying a point with respect to a faceuse.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "plot3.h"


/* vertex/edge distance
 * Each loop geometry element (edge/vertex) has one of these computed.
 * We keep track of them for the whole face so that this value is computed
 * only once per geometry element.  This way we get consistent answers
 */
struct ve_dist {
    struct bu_list l;
    uint32_t *magic_p;	/* pointer to edge/vertex structure */
    fastf_t dist;	/* distance squared from point to edge */
    struct vertex *v1;
    struct vertex *v2;
    int status;	/* return code from bn_dist_pt3_lseg3 */
};
#define NMG_VE_DIST_MAGIC 0x102938
#define NMG_CK_VED(_p) NMG_CKMAG(_p, NMG_VE_DIST_MAGIC, "vertex/edge_dist")

/* The loop builds a list of these things so that it can figure out the point
 * classification based upon the classification of the pt vs each edge of
 * the loop.  This list is sorted in ascending "ved_p->dist" order.
 */
struct edge_info {
    struct bu_list l;
    struct ve_dist *ved_p;	  /* ptr to ve_dist for this item */
    struct edgeuse *eu_p;	  /* edgeuse pointer */
    int nmg_class;	  /* pt classification WRT this item use */
};
#define NMG_EDGE_INFO_MAGIC 0xe100
#define NMG_CK_EI(_p) NMG_CKMAG(_p, NMG_EDGE_INFO_MAGIC, "edge_info")

struct fpi {
    uint32_t magic;
    const struct bn_tol *tol;
    const struct faceuse *fu_p;
    struct bu_list ve_dh;		/* ve_dist list head */
    plane_t norm;		/* surface normal for face(use) */
    point_t pt;		/* pt in plane of face to classify */
    void (*eu_func)(struct edgeuse *, point_t, const char *);	/* call w/eu when pt on edgeuse */
    void (*vu_func)(struct vertexuse *, point_t, const char *);	/* call w/vu when pt on vertexuse */
    const char *priv;		/* caller's private data */
    int hits;		/* flag PERUSE/PERGEOM */
};
#define NMG_FPI_MAGIC 12345678 /* fpi\0 */
#define NMG_CK_FPI(_fpi) \
	NMG_CKMAG(_fpi, NMG_FPI_MAGIC, "fu_pt_info"); \
	BN_CK_TOL(_fpi->tol); \
	BU_CK_LIST_HEAD(&_fpi->ve_dh)

#define NMG_FPI_TOUCHED 27
#define NMG_FPI_MISSED  32768

static int nmg_class_pt_vu(struct fpi *fpi, struct vertexuse *vu);
static struct edge_info *nmg_class_pt_eu(struct fpi *fpi, struct edgeuse *eu, struct edge_info *edge_list, const int in_or_out_only);
static int compute_loop_class(struct fpi *fpi, const struct loopuse *lu, struct edge_info *edge_list);
static int nmg_class_pt_lu(struct loopuse *lu, struct fpi *fpi, const int in_or_out_only);
int nmg_class_pt_fu_except(const point_t pt, const struct faceuse *fu, const struct loopuse *ignore_lu, void (*eu_func)(struct edgeuse *, point_t, const char *), void (*vu_func)(struct vertexuse *, point_t, const char *), const char *priv, const int call_on_hits, const int in_or_out_only, const struct bn_tol *tol);


/**
 * Find the square of the distance from a point P to a line segment described
 * by the two endpoints A and B.
 *
 *			P
 *		       *
 *		      /.
 *		     / .
 *		    /  .
 *		   /   . (dist)
 *		  /    .
 *		 /     .
 *		*------*--------*
 *		A      PCA	B
 *
 * There are six distinct cases, with these return codes -
 *	0	P is within tolerance of lseg AB.  *dist =  0.
 *	1	P is within tolerance of point A.  *dist = 0.
 *	2	P is within tolerance of point B.  *dist = 0.
 *	3	PCA is within tolerance of A. *dist = |P-A|**2.
 *	4	PCA is within tolerance of B. *dist = |P-B|**2.
 *	5	P is "above/below" lseg AB.  *dist=|PCA-P|**2.
 *
 * This routine was formerly called bn_dist_pt_lseg().
 * This is a special version that returns the square of the distance
 * and does not actually calculate PCA.
 *
 */
int
bn_distsq_pt3_lseg3(fastf_t *dist, const fastf_t *a, const fastf_t *b, const fastf_t *p, const struct bn_tol *tol)
{
    vect_t PtoA;		/* P-A */
    vect_t PtoB;		/* P-B */
    vect_t AtoB;		/* B-A */
    fastf_t P_A_sq;		/* |P-A|**2 */
    fastf_t P_B_sq;		/* |P-B|**2 */
    fastf_t B_A_sq;		/* |B-A|**2 */
    fastf_t t;		/* distance squared along ray of projection of P */
    fastf_t dot;

    BN_CK_TOL(tol);

    /* Check proximity to endpoint A */
    VSUB2(PtoA, p, a);
    P_A_sq = MAGSQ(PtoA);
    if (P_A_sq < tol->dist_sq) {
	/* P is within the tol->dist radius circle around A */
	*dist = 0.0;
	return 1;
    }

    /* Check proximity to endpoint B */
    VSUB2(PtoB, p, b);
    P_B_sq = MAGSQ(PtoB);
    if (P_B_sq < tol->dist_sq) {
	/* P is within the tol->dist radius circle around B */
	*dist = 0.0;
	return 2;
    }

    VSUB2(AtoB, b, a);
    B_A_sq = MAGSQ(AtoB);

    /* compute distance squared (in actual units) along line to PROJECTION of
     * point p onto the line: point pca
     */
    dot = VDOT(PtoA, AtoB);
    t = dot * dot / B_A_sq;

    if (t < tol->dist_sq) {
	/* PCA is at A */
	*dist = P_A_sq;
	return 3;
    }

    if (dot < -SMALL_FASTF && t > tol->dist_sq) {
	/* P is "left" of A */
	*dist = P_A_sq;
	return 3;
    }
    if (t < (B_A_sq - tol->dist_sq)) {
	/* PCA falls between A and B */
	register fastf_t dsq;

	/* Find distance from PCA to line segment (Pythagorus) */
	dsq = P_A_sq - t;
	if (dsq < tol->dist_sq) {
	    /* PCA is on lseg */
	    *dist = 0.0;
	    return 0;
	}

	/* P is above or below lseg */
	*dist = dsq;
	return 5;
    }

    /* P is "right" of B */
    *dist = P_B_sq;
    return 4;
}


/**
 * Classify a point vs a vertex (touching/missed)
 */
static int
nmg_class_pt_vu(struct fpi *fpi, struct vertexuse *vu)
{
    vect_t delta;
    struct ve_dist *ved;

    NMG_CK_VERTEXUSE(vu);

    /* see if we've classified this vertex WRT the point already */
    for (BU_LIST_FOR(ved, ve_dist, &fpi->ve_dh)) {
	NMG_CK_VED(ved);
	if (ved->magic_p == &vu->v_p->magic) {
	    goto found;
	}
    }

    /* if we get here, we didn't find the vertex in the list of
     * previously classified geometry.  Create an entry in the
     * face's list of processed geometry.
     */
    VSUB2(delta, vu->v_p->vg_p->coord, fpi->pt);

    BU_ALLOC(ved, struct ve_dist);
    ved->magic_p = &vu->v_p->magic;
    ved->dist = MAGNITUDE(delta);
    if (ved->dist < fpi->tol->dist_sq) {
	ved->status = NMG_FPI_TOUCHED;
	if (fpi->hits == NMG_FPI_PERGEOM) {
	    /* need to cast vu_func pointer for actual use as a function */
	    void (*cfp)(struct vertexuse *, point_t, const char*);
	    cfp = (void (*)(struct vertexuse *, point_t, const char *))fpi->vu_func;
	    cfp(vu, fpi->pt, fpi->priv);
	}
    } else ved->status = NMG_FPI_MISSED;

    ved->v1 = ved->v2 = vu->v_p;

    BU_LIST_MAGIC_SET(&ved->l, NMG_VE_DIST_MAGIC);
    BU_LIST_APPEND(&fpi->ve_dh, &ved->l);
 found:

    if (fpi->vu_func  &&
	ved->status == NMG_FPI_TOUCHED &&
	fpi->hits == NMG_FPI_PERUSE) {
	/* need to cast vu_func pointer for actual use as a function */
	void (*cfp)(struct vertexuse *, point_t, const char*);
	cfp = (void (*)(struct vertexuse *, point_t, const char *))fpi->vu_func;
	cfp(vu, fpi->pt, fpi->priv);
    }

    return ved->status;
}


static int
Quadrant(fastf_t x, fastf_t y)
{
    if (x > -SMALL_FASTF) {
	if (y > -SMALL_FASTF)
	    return 1;
	else
	    return 4;
    } else {
	if (y > -SMALL_FASTF)
	    return 2;
	else
	    return 3;
    }
}


int
nmg_eu_is_part_of_crack(const struct edgeuse *eu)
{
    struct loopuse *lu;
    struct edgeuse *eu_test;

    NMG_CK_EDGEUSE(eu);

    /* must be part of a loop to be a crack */
    if (*eu->up.magic_p != NMG_LOOPUSE_MAGIC)
	return 0;

    lu = eu->up.lu_p;
    NMG_CK_LOOPUSE(lu);

    for (BU_LIST_FOR(eu_test, edgeuse, &lu->down_hd)) {
	if (eu_test == eu)
	    continue;

	if (eu_test->vu_p->v_p == eu->eumate_p->vu_p->v_p &&
	    eu_test->eumate_p->vu_p->v_p == eu->vu_p->v_p)
	    return 1;
    }

    return 0;
}


/**
 * Classify a point with respect to an EU where the VU is the
 * closest to the point. The EU and its left vector form an XY
 * coordinate system in the face, with EU along the X-axis and
 * its left vector along the Y-axis. Use these to decompose the
 * direction of the prev_eu into X and Y coordinates (xo, yo) then
 * do the same for the vector to the point to be classed (xpt, ypt).
 * If (xpt, ypt) makes a smaller angle with EU than does (xo, yo),
 * then PT is in the face, otherwise it is out.
 *
 *
 * It is assumed that eu is from an OT_SAME faceuse, and that
 * the PCA of PT to EU is at eu->vu_p.
 */

int
nmg_class_pt_euvu(const fastf_t *pt, struct edgeuse *eu_in, const struct bn_tol *tol)
{
    struct loopuse *lu;
    struct edgeuse *prev_eu;
    struct edgeuse *eu;
    struct vertex *v0, *v1, *v2;
    vect_t left;
    vect_t eu_dir;
    vect_t other_eudir;
    vect_t pt_dir;
    fastf_t xo, yo;
    fastf_t xpt, ypt;
    fastf_t len;
    int quado, quadpt;
    int nmg_class = NMG_CLASS_Unknown;
    int eu_is_crack = 0;
    int prev_eu_is_crack = 0;

    NMG_CK_EDGEUSE(eu_in);
    BN_CK_TOL(tol);

    eu = eu_in;

    if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
	bu_log("nmg_class_pt_euvu((%g %g %g), eu=%p)\n", V3ARGS(pt), (void *)eu);

    if (UNLIKELY(*eu->up.magic_p != NMG_LOOPUSE_MAGIC)) {
	bu_log("nmg_class_pt_euvu() called with eu (%p) that isn't part of a loop\n", (void *)eu);
	bu_bomb("nmg_class_pt_euvu() called with eu that isn't part of a loop");
    }
    lu = eu->up.lu_p;
    NMG_CK_LOOPUSE(lu);

    eu_is_crack = nmg_eu_is_part_of_crack(eu);

    prev_eu = BU_LIST_PPREV_CIRC(edgeuse, &eu->l);

    prev_eu_is_crack = nmg_eu_is_part_of_crack(prev_eu);

    /* if both EU's are cracks, we cannot classify */
    if (eu_is_crack && prev_eu_is_crack)
	return NMG_CLASS_Unknown;

    if (eu_is_crack) {
	struct edgeuse *eu_test;
	int done = 0;

	if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
	    bu_log("nmg_class_pt_euvu: eu %p is a crack\n", (void *)eu);

	/* find next eu from this vertex that is not a crack */
	eu_test = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	while (!done) {
	    while (eu_test->vu_p->v_p != eu->vu_p->v_p && eu_test != eu)
		eu_test = BU_LIST_PNEXT_CIRC(edgeuse, &eu_test->l);

	    if (eu_test == eu)
		done = 1;

	    if (!nmg_eu_is_part_of_crack(eu_test))
		done = 1;

	    if (!done)
		eu_test = BU_LIST_PNEXT_CIRC(edgeuse, &eu_test->l);
	}

	if (eu_test == eu) /* can't get away from crack */
	    return NMG_CLASS_Unknown;
	else
	    eu = eu_test;

	if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
	    bu_log("\tUsing eu %p instead\n", (void *)eu);
    }

    if (prev_eu_is_crack) {
	struct edgeuse *eu_test;
	int done = 0;

	if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
	    bu_log("nmg_class_pt_euvu: prev_eu (%p) is a crack\n", (void *)prev_eu);

	/* find previous eu ending at this vertex that is not a crack */
	eu_test = BU_LIST_PPREV_CIRC(edgeuse, &prev_eu->l);
	while (!done) {
	    while (eu_test->eumate_p->vu_p->v_p != eu->vu_p->v_p && eu_test != prev_eu)
		eu_test = BU_LIST_PPREV_CIRC(edgeuse, &eu_test->l);

	    if (eu_test == prev_eu)
		done = 1;

	    if (!nmg_eu_is_part_of_crack(eu_test))
		done = 1;

	    if (!done)
		eu_test = BU_LIST_PPREV_CIRC(edgeuse, &eu_test->l);
	}

	if (eu_test == prev_eu) /* can't get away from crack */
	    return NMG_CLASS_Unknown;
	else
	    prev_eu = eu_test;

	if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
	    bu_log("\tUsing prev_eu %p instead\n", (void *)prev_eu);
    }

    /* left is the Y-axis of our XY-coordinate system */
    if (UNLIKELY(nmg_find_eu_leftvec(left, eu))) {
	bu_log("nmg_class_pt_euvu: nmg_find_eu_leftvec() for eu=%p failed!\n", (void *)eu);
	bu_bomb("nmg_class_pt_euvu: nmg_find_eu_leftvec() failed!");
    }

    if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
	bu_log("\tprev_eu = %p, left = (%g %g %g)\n", (void *)prev_eu, V3ARGS(left));

    /* v0 is the origin of the XY-coordinate system */
    v0 = eu->vu_p->v_p;
    NMG_CK_VERTEX(v0);

    /* v1 is on the X-axis */
    v1 = eu->eumate_p->vu_p->v_p;
    NMG_CK_VERTEX(v1);

    /* v2 determines angle prev_eu makes with X-axis */
    v2 = prev_eu->vu_p->v_p;
    NMG_CK_VERTEX(v2);

    if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
	bu_log("\tv0=%p, v1=%p, v2=%p\n", (void *)v0, (void *)v1, (void *)v2);

    /* eu_dir is our X-direction */
    VSUB2(eu_dir, v1->vg_p->coord, v0->vg_p->coord);

    /* other_eudir is direction along the previous EU (from origin) */
    VSUB2(other_eudir, v2->vg_p->coord, v0->vg_p->coord);

    if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
	bu_log("\teu_dir=(%g %g %g), other_eudir=(%g %g %g)\n", V3ARGS(eu_dir), V3ARGS(other_eudir));

    /* get X and Y components for other_eu */
    xo = VDOT(eu_dir, other_eudir);
    yo = VDOT(left, other_eudir);

    /* which quadrant does this XY point lie in */
    quado = Quadrant(xo, yo);

    if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
	bu_log("\txo=%g, yo=%g, quadrant=%d\n", xo, yo, quado);

    /* get direction to PT from origin */
    VSUB2(pt_dir, pt, v0->vg_p->coord);

    if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
	bu_log("\tpt_dir=(%g %g %g)\n", V3ARGS(pt_dir));

    /* get X and Y components for PT */
    xpt = VDOT(eu_dir, pt_dir);
    ypt = VDOT(left, pt_dir);

    /* which quadrant does this XY point lie in */
    quadpt = Quadrant(xpt, ypt);

    if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
	bu_log("\txpt=%g, ypt=%g, quadrant=%d\n", xpt, ypt, quadpt);

    /* do a quadrant comparison first (cheap!!!) */
    if (quadpt < quado)
	return NMG_CLASS_AinB;

    if (quadpt > quado)
	return NMG_CLASS_AoutB;

    /* both are in the same quadrant, need to normalize the coordinates */
    len = sqrt(xo*xo + yo*yo);
    xo = xo/len;
    yo = yo/len;

    len = sqrt(xpt*xpt + ypt*ypt);
    xpt = xpt/len;
    ypt = ypt/len;

    if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
	bu_log("\tNormalized xo, yo=(%g %g), xpt, ypt=(%g %g)\n", xo, yo, xpt, ypt);

    switch (quadpt) {
	case 1:
	    if (xpt >= xo && ypt <= yo)
		nmg_class = NMG_CLASS_AinB;
	    else
		nmg_class = NMG_CLASS_AoutB;
	    break;
	case 2:
	    if (xpt >= xo && ypt >= yo)
		nmg_class = NMG_CLASS_AinB;
	    else
		nmg_class = NMG_CLASS_AoutB;
	    break;
	case 3:
	    if (xpt <= xo && ypt >= yo)
		nmg_class = NMG_CLASS_AinB;
	    else
		nmg_class = NMG_CLASS_AoutB;
	    break;
	case 4:
	    if (xpt <= xo && ypt <= yo)
		nmg_class = NMG_CLASS_AinB;
	    else
		nmg_class = NMG_CLASS_AoutB;
	    break;
	default:
	    bu_log("This can't happen (illegal quadrant %d)\n", quadpt);
	    bu_bomb("This can't happen (illegal quadrant)\n");
	    break;
    }
    if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
	bu_log("returning %s\n", nmg_class_name(nmg_class));

    return nmg_class;
}


/**
 * If there is no ve_dist structure for this edge, compute one and
 * add it to the list.
 *
 * Sort an edge_info structure into the loops list of edgeuse status
 */
static struct edge_info *
nmg_class_pt_eu(struct fpi *fpi, struct edgeuse *eu, struct edge_info *edge_list, const int in_or_out_only)
{
    struct bn_tol tmp_tol;
    struct edgeuse *next_eu;
    struct ve_dist *ved, *ed;
    struct edge_info *ei_p;
    struct edge_info *ei;
    pointp_t eu_pt;
    vect_t left;
    vect_t v_to_pt;
    int found_data = 0;

    NMG_CK_FPI(fpi);
    BN_CK_TOL(fpi->tol);

    if (RTG.NMG_debug & DEBUG_PT_FU) {
	bu_log("pt (%g %g %g) vs_edge (%g %g %g) (%g %g %g) (eu=%p)\n",
	       V3ARGS(fpi->pt),
	       V3ARGS(eu->vu_p->v_p->vg_p->coord),
	       V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord), (void *)eu);
    }

    /* we didn't find a ve_dist structure for this edge, so we'll
     * have to do the calculations.
     */
    tmp_tol = (*fpi->tol);
    if (in_or_out_only) {
	tmp_tol.dist = 0.0;
	tmp_tol.dist_sq = 0.0;
    }

    BU_ALLOC(ved, struct ve_dist);
    ved->magic_p = &eu->e_p->magic;
    ved->status = bn_distsq_pt3_lseg3(&ved->dist,
				      eu->vu_p->v_p->vg_p->coord,
				      eu->eumate_p->vu_p->v_p->vg_p->coord,
				      fpi->pt,
				      &tmp_tol);
    ved->v1 = eu->vu_p->v_p;
    ved->v2 = eu->eumate_p->vu_p->v_p;
    BU_LIST_MAGIC_SET(&ved->l, NMG_VE_DIST_MAGIC);
    BU_LIST_APPEND(&fpi->ve_dh, &ved->l);
    eu_pt = ved->v1->vg_p->coord;

    if (RTG.NMG_debug & DEBUG_PT_FU) {
	bu_log("nmg_class_pt_eu: status for eu %p (%g %g %g)<->(%g %g %g) vs pt (%g %g %g) is %d\n",
	       (void *)eu, V3ARGS(eu->vu_p->v_p->vg_p->coord),
	       V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord),
	       V3ARGS(fpi->pt), ved->status);
	bu_log("\tdist = %g\n", ved->dist);
    }


    /* Add a struct for this edgeuse to the loop's list of dist-sorted
     * edgeuses.
     */
    BU_ALLOC(ei, struct edge_info);
    ei->ved_p = ved;
    ei->eu_p = eu;
    BU_LIST_MAGIC_SET(&ei->l, NMG_EDGE_INFO_MAGIC);

    /* compute the status (ei->status) of the point WRT this edge */

    switch (ved->status) {
	case 0: /* pt is on the edge(use) */
	    ei->nmg_class = NMG_CLASS_AonBshared;
	    if (fpi->eu_func &&
		(fpi->hits == NMG_FPI_PERUSE ||
		 (fpi->hits == NMG_FPI_PERGEOM && !found_data))) {
		/* need to cast eu_func pointer for actual use as a function */
		void (*cfp)(struct edgeuse *, point_t, const char*);
		cfp = (void (*)(struct edgeuse *, point_t, const char *))fpi->eu_func;
		cfp(eu, fpi->pt, fpi->priv);
	    }
	    break;
	case 1:	/* within tolerance of endpoint at ved->v1 */
	    ei->nmg_class = NMG_CLASS_AonBshared;
	    /* add an entry for the vertex in the edge list so that
	     * other uses of this vertex will claim the point is within
	     * tolerance without re-computing
	     */
	    BU_ALLOC(ed, struct ve_dist);
	    ed->magic_p = &ved->v1->magic;
	    ed->status = ved->status;
	    ed->v1 = ed->v2 = ved->v1;

	    BU_LIST_MAGIC_SET(&ed->l, NMG_VE_DIST_MAGIC);
	    BU_LIST_APPEND(&fpi->ve_dh, &ed->l);

	    if (fpi->vu_func &&
		(fpi->hits == NMG_FPI_PERUSE ||
		 (fpi->hits == NMG_FPI_PERGEOM && !found_data))) {
		/* need to cast vu_func pointer for actual use as a function */
		void (*cfp)(struct vertexuse *, point_t, const char*);
		cfp = (void (*)(struct vertexuse *, point_t, const char *))fpi->vu_func;
		cfp(eu->vu_p, fpi->pt, fpi->priv);
	    }

	    break;
	case 2:	/* within tolerance of endpoint at ved->v2 */
	    ei->nmg_class = NMG_CLASS_AonBshared;
	    /* add an entry for the vertex in the edge list so that
	     * other uses of this vertex will claim the point is within
	     * tolerance without re-computing
	     */
	    BU_ALLOC(ed, struct ve_dist);
	    ed->magic_p = &ved->v2->magic;
	    ed->status = ved->status;
	    ed->v1 = ed->v2 = ved->v2;

	    BU_LIST_MAGIC_SET(&ed->l, NMG_VE_DIST_MAGIC);
	    BU_LIST_APPEND(&fpi->ve_dh, &ed->l);
	    if (fpi->vu_func &&
		(fpi->hits == NMG_FPI_PERUSE ||
		 (fpi->hits == NMG_FPI_PERGEOM && !found_data))) {
		/* need to cast vu_func pointer for actual use as a function */
		void (*cfp)(struct vertexuse *, point_t, const char*);
		cfp = (void (*)(struct vertexuse *, point_t, const char *))fpi->vu_func;
		cfp(eu->eumate_p->vu_p, fpi->pt, fpi->priv);
	    }
	    break;

	case 3: /* PCA of pt on line is within tolerance of ved->v1 of segment */
	    ei->nmg_class = nmg_class_pt_euvu(fpi->pt, eu, fpi->tol);
	    if (ei->nmg_class == NMG_CLASS_Unknown)
		ei->ved_p->dist = MAX_FASTF;
	    break;
	case 4: /* PCA of pt on line is within tolerance of ved->v2 of segment */
	    next_eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    ei->nmg_class = nmg_class_pt_euvu(fpi->pt, next_eu, fpi->tol);
	    if (ei->nmg_class == NMG_CLASS_Unknown)
		ei->ved_p->dist = MAX_FASTF;
	    break;

	case 5: /* PCA is along length of edge, but point is NOT on edge. */
	    if (nmg_find_eu_left_non_unit(left, eu))
		bu_bomb("can't find left vector\n");
	    /* take dot product of v->pt vector with left to determine
	     * if pt is inside/left of edge
	     */
	    VSUB2(v_to_pt, fpi->pt, eu_pt);
	    if (VDOT(v_to_pt, left) > -SMALL_FASTF)
		ei->nmg_class = NMG_CLASS_AinB;
	    else
		ei->nmg_class = NMG_CLASS_AoutB;
	    break;
	default:
	    bu_log("%s:%d status = %d\n", __FILE__, __LINE__, ved->status);
	    bu_bomb("Why did this happen?");
	    break;
    }


    if (RTG.NMG_debug & DEBUG_PT_FU) {
	bu_log("pt @ dist %g from edge classed %s vs edge\n",
	       ei->ved_p->dist, nmg_class_name(ei->nmg_class));
/* pl_pt_e(fpi, ei); */
    }

    /* now that it's complete, add ei to the edge list */
    for (BU_LIST_FOR(ei_p, edge_info, &edge_list->l)) {
	/* if the distance to this edge is smaller, or
	 * if the distance is the same & the edge is the same
	 * Insert edge_info struct here in list
	 */
	if (ved->dist < ei_p->ved_p->dist
	    || (ZERO(ved->dist - ei_p->ved_p->dist)
		&& ei_p->ved_p->magic_p == ved->magic_p))
	{
	    break;
	}
    }

    BU_LIST_INSERT(&ei_p->l, &ei->l);
    return ei;
}


/**
 * Make a list of all edgeuses which are at the same distance as the
 * first element on the list.  Toss out opposing pairs of edgeuses of the
 * same edge.
 *
 */
HIDDEN void make_near_list(struct edge_info *edge_list, struct bu_list *near1, const struct bn_tol *tol)
{
    struct edge_info *ei;
    struct edge_info *ei_p;
    struct edge_info *tmp;
    fastf_t dist;

    BU_CK_LIST_HEAD(&edge_list->l);
    BU_CK_LIST_HEAD(near1);

    /* toss opposing pairs of uses of the same edge from the list */
    ei = BU_LIST_FIRST(edge_info, &edge_list->l);
    while (BU_LIST_NOT_HEAD(&ei->l, &edge_list->l)) {
	NMG_CK_EI(ei);
	ei_p = BU_LIST_FIRST(edge_info, &edge_list->l);
	while (BU_LIST_NOT_HEAD(&ei_p->l, &edge_list->l)) {
	    NMG_CK_EI(ei_p);
	    NMG_CK_VED(ei_p->ved_p);

	    /* if we've found an opposing use of the same
	     * edge toss the pair of them
	     */
	    if (ei_p->ved_p->magic_p == ei->ved_p->magic_p &&
		ei_p->eu_p->eumate_p->vu_p->v_p == ei->eu_p->vu_p->v_p &&
		ei_p->eu_p->vu_p->v_p == ei->eu_p->eumate_p->vu_p->v_p) {
		if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU)) {
		    bu_log("tossing edgeuse pair:\n");
		    bu_log("(%g %g %g) -> (%g %g %g)\n",
			   V3ARGS(ei->eu_p->vu_p->v_p->vg_p->coord),
			   V3ARGS(ei->eu_p->eumate_p->vu_p->v_p->vg_p->coord));
		    bu_log("(%g %g %g) -> (%g %g %g)\n",
			   V3ARGS(ei_p->eu_p->vu_p->v_p->vg_p->coord),
			   V3ARGS(ei_p->eu_p->eumate_p->vu_p->v_p->vg_p->coord));
		}

		tmp = ei_p;
		ei_p = BU_LIST_PLAST(edge_info, &ei_p->l);
		BU_LIST_DEQUEUE(&tmp->l);
		bu_free((char *)tmp, "edge info struct");
		tmp = ei;
		ei = BU_LIST_PLAST(edge_info, &ei->l);
		BU_LIST_DEQUEUE(&tmp->l);
		bu_free((char *)tmp, "edge info struct");
		break;
	    }
	    ei_p = BU_LIST_PNEXT(edge_info, &ei_p->l);
	}
	ei = BU_LIST_PNEXT(edge_info, &ei->l);
    }

    if (BU_LIST_IS_EMPTY(&edge_list->l))
	return;

    ei = BU_LIST_FIRST(edge_info, &edge_list->l);
    NMG_CK_EI(ei);
    NMG_CK_VED(ei->ved_p);
    dist = ei->ved_p->dist;

    /* create "near" list with all ei's at this dist */
    for (BU_LIST_FOR(ei, edge_info, &edge_list->l)) {
	NMG_CK_EI(ei);
	NMG_CK_VED(ei->ved_p);
	if (NEAR_EQUAL(ei->ved_p->dist, dist, tol->dist_sq)) {
	    ei_p = BU_LIST_PLAST(edge_info, &ei->l);
	    BU_LIST_DEQUEUE(&ei->l);
	    BU_LIST_APPEND(near1, &ei->l);
	    ei = ei_p;
	}
    }

    if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU)) {
	bu_log("dist %g near list\n", dist);
	for (BU_LIST_FOR(ei, edge_info, near1)) {
	    bu_log("\t(%g %g %g) -> (%g %g %g)\n",
		   V3ARGS(ei->eu_p->vu_p->v_p->vg_p->coord),
		   V3ARGS(ei->eu_p->eumate_p->vu_p->v_p->vg_p->coord));
	    bu_log("\tdist:%g class:%s status:%d\n\t\tv1(%g %g %g) v2(%g %g %g)\n",
		   ei->ved_p->dist, nmg_class_name(ei->nmg_class),
		   ei->ved_p->status,
		   V3ARGS(ei->ved_p->v1->vg_p->coord),
		   V3ARGS(ei->ved_p->v2->vg_p->coord));
	    bu_log("\tei->ved_p->magic_p=%p, ei->eu_p->vu_p=%p, ei->eu_p->eumate_p->vu_p=%p\n",
		   (void *)ei->ved_p->magic_p, (void *)ei->eu_p->vu_p, (void *)ei->eu_p->eumate_p->vu_p);
	}
    }
}


static void
pl_pt_lu(struct fpi *fpi, const struct loopuse *lu, struct edge_info *ei)
{
    FILE *fp;
    char name[25];
    long *b;
    static int plot_file_number = 0;
    int i;
    point_t p1, p2;
    point_t pca;
    fastf_t dist;
    struct bn_tol tmp_tol;

    NMG_CK_FPI(fpi);
    NMG_CK_FACEUSE(fpi->fu_p);
    NMG_CK_LOOPUSE(lu);
    NMG_CK_EI(ei);

    sprintf(name, "pt_lu%02d.plot3", plot_file_number++);
    fp = fopen(name, "wb");
    if (fp == (FILE *)NULL) {
	perror(name);
	bu_bomb("unable to open file for writing");
    }

    bu_log("\toverlay %s\n", name);
    b = (long *)bu_calloc(fpi->fu_p->s_p->r_p->m_p->maxindex,
			  sizeof(long), "bit vec"),

	pl_erase(fp);
    pd_3space(fp,
	      fpi->fu_p->f_p->min_pt[0]-1.0,
	      fpi->fu_p->f_p->min_pt[1]-1.0,
	      fpi->fu_p->f_p->min_pt[2]-1.0,
	      fpi->fu_p->f_p->max_pt[0]+1.0,
	      fpi->fu_p->f_p->max_pt[1]+1.0,
	      fpi->fu_p->f_p->max_pt[2]+1.0);

    nmg_pl_lu(fp, lu, b, 255, 255, 255);

    tmp_tol.magic = BN_TOL_MAGIC;
    tmp_tol.dist = 0.0005;
    tmp_tol.dist_sq = tmp_tol.dist * tmp_tol.dist;
    tmp_tol.perp = 1e-6;
    tmp_tol.para = 1 - tmp_tol.perp;

    (void)bn_dist_pt3_lseg3(&dist, pca, ei->eu_p->vu_p->v_p->vg_p->coord,
			    ei->eu_p->eumate_p->vu_p->v_p->vg_p->coord, fpi->pt, &tmp_tol);

    pl_color(fp, 255, 255, 50);
    pdv_3line(fp, pca, fpi->pt);

    pl_color(fp, 255, 64, 255);

    /* make a nice axis-cross at the point in question */
    for (i = 0; i < 3; i++) {
	VMOVE(p1, fpi->pt);
	p1[i] -= 1.0;
	VMOVE(p2, fpi->pt);
	p2[i] += 1.0;
	pdv_3line(fp, p1, p2);
    }

    bu_free((char *)b, "bit vec");
    fclose(fp);
}


/**
 * Given a list of edge_info structures for the edges of a loop,
 * determine what the classification for the loop should be.
 *
 * If passed a "crack" loop, will produce random results.
 */

static int
compute_loop_class(struct fpi *fpi,
		   const struct loopuse *lu,
		   struct edge_info *edge_list)
{
    struct edge_info *ei;
    struct bu_list near1;
    int lu_class = NMG_CLASS_Unknown;

    if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU)) {
	bu_log("compute_loop_class()\n");
	for (BU_LIST_FOR(ei, edge_info, &edge_list->l)) {
	    bu_log("dist:%g class:%s status:%d\n\tv1(%g %g %g) v2(%g %g %g)\n",
		   ei->ved_p->dist, nmg_class_name(ei->nmg_class),
		   ei->ved_p->status,
		   V3ARGS(ei->ved_p->v1->vg_p->coord),
		   V3ARGS(ei->ved_p->v2->vg_p->coord));
	}
    }

    BU_CK_LIST_HEAD(&edge_list->l);
    BU_LIST_INIT(&near1);

    /* get a list of "closest/useful" edges to use in classifying
     * the pt WRT the loop
     */
    while (BU_LIST_IS_EMPTY(&near1) && BU_LIST_NON_EMPTY(&edge_list->l)) {
	make_near_list(edge_list, &near1, fpi->tol);
    }

    if (BU_LIST_IS_EMPTY(&near1)) {
	/* This was a "crack" or "snake" loop */

	if (lu->orientation == OT_SAME) {
	    lu_class = NMG_CLASS_AoutB;
	} else if (lu->orientation == OT_OPPOSITE) {
	    lu_class = NMG_CLASS_AinB;
	} else
	    bu_bomb("bad lu orientation\n");

	if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU)) {
	    bu_log("list was empty, so class is %s\n",
		   nmg_class_name(lu_class));
	}

	return lu_class;
    }

    for (BU_LIST_FOR(ei, edge_info, &near1)) {
	int done = 0;
	NMG_CK_EI(ei);
	NMG_CK_VED(ei->ved_p);
	switch (ei->ved_p->status) {
	    case 0: /* pt is on edge */
	    case 1: /* pt is on ei->ved_p->v1 */
	    case 2: /* pt is on ei->ved_p->v2 */
		lu_class = NMG_CLASS_AonBshared;
		if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
		    pl_pt_lu(fpi, lu, ei);
		done = 1;
		break;
	    case 3: /* pt pca is v1 */
	    case 4: /* pt pca is v2 */
	    case 5: /* pt pca between v1 and v2 */
		lu_class = ei->nmg_class;
		if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU)) {
		    bu_log("found status 5 edge, loop class is %s\n",
			   nmg_class_name(lu_class));
		}
		if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU))
		    pl_pt_lu(fpi, lu, ei);
		done = 1;
		break;
	    default:
		bu_log("%s:%d status = %d\n",
		       __FILE__, __LINE__, ei->ved_p->status);
		bu_bomb("Why did this happen?");
		break;
	}
	if (done) {
	    break;
	}
    }

    /* the caller will get whatever is left of the edge_list, but
     * we need to free up the "near" list
     */
    while (BU_LIST_WHILE(ei, edge_info, &near1)) {
	BU_LIST_DEQUEUE(&ei->l);
	bu_free((char *)ei, "edge_info struct");
    }

    if (UNLIKELY(RTG.NMG_debug & DEBUG_PT_FU)) {
	bu_log("compute_loop_class() returns %s\n",
	       nmg_class_name(lu_class));
    }

    return lu_class;
}
/**
 *
 * For each edgeuse, compute an edge_info structure.
 *
 * if min_dist == 0 return ON
 *
 * For dist min -> max
 *	if even # of uses of edge in this loop
 *		"ignore" all uses of this edge since we can't answer the
 *		    "spike" problem:	*---------*
 *					| .	  |
 *					*---*	  |  .
 *					|	  *----*
 *					|	  |
 *					*---------*
 *	else (odd # of uses of edge in this loop)
 *		"ignore" consecutive uses of the same edge to avoid the
 *		    "accordian pleat" problem:	*-------*
 *						|  .	|
 *						*----*	|
 *						*----*	|
 *						*----*	|
 *						*----*--*
 *		classify pt WRT remaining edgeuse
 *
 * The "C-clamp" problem:
 *	*---------------*
 *	|		|
 *	|  *----*	|
 *	|  |	|   .	|
 *	|  |	*-------*
 *	|  |	|	|
 *	|  *----*	|
 *	|		|
 *	*---------------*
 *
 */
static int
nmg_class_pt_lu(struct loopuse *lu, struct fpi *fpi, const int in_or_out_only)
{
    int lu_class = NMG_CLASS_Unknown;

    NMG_CK_FPI(fpi);
    NMG_CK_LOOPUSE(lu);
    NMG_CK_LOOP(lu->l_p);
    NMG_CK_LOOP_G(lu->l_p->lg_p);


    if (V3PT_OUT_RPP_TOL(fpi->pt, lu->l_p->lg_p->min_pt, lu->l_p->lg_p->max_pt, fpi->tol->dist)) {
	/* point is not in RPP of loop */

	if (RTG.NMG_debug & DEBUG_PT_FU) {
	    bu_log("nmg_class_pt_lu(pt(%g %g %g) outside loop RPP\n",
		   V3ARGS(fpi->pt));
	    bu_log("   lu RPP: (%g %g %g) <-> (%g %g %g)\n",
		   V3ARGS(lu->l_p->lg_p->min_pt), V3ARGS(lu->l_p->lg_p->max_pt));
	}

	if (lu->orientation == OT_SAME)
	    return NMG_CLASS_AoutB;
	else if (lu->orientation == OT_OPPOSITE)
	    return NMG_CLASS_AinB;
	else if (lu->orientation == OT_UNSPEC)
	    return NMG_CLASS_Unknown;

    }

    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
	register struct edgeuse *eu;
	struct edge_info edge_list;
	struct edge_info *ei;
	int is_crack;

	is_crack = nmg_loop_is_a_crack(lu);
	if (lu->orientation == OT_OPPOSITE && is_crack) {
	    /*  Even if point lies on a crack, if it's an
	     * OT_OPPOSITE crack loop, it subtracts nothing.
	     * Just ignore it.
	     */
	    lu_class = NMG_CLASS_AinB;
	    if (RTG.NMG_debug & DEBUG_PT_FU)
		bu_log("nmg_class_pt_lu() ignoring OT_OPPOSITE crack loop\n");
	    goto out;
	}

	BU_LIST_INIT(&edge_list.l);

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    ei = nmg_class_pt_eu(fpi, eu, &edge_list, in_or_out_only);
	    NMG_CK_EI(ei);
	    NMG_CK_VED(ei->ved_p);
	    if (!in_or_out_only && ei->ved_p->dist < fpi->tol->dist_sq) {
		lu_class = NMG_CLASS_AinB;
		break;
	    }
	}
	/* */
	if (lu_class == NMG_CLASS_Unknown) {
	    /* pt does not touch any edge or vertex */
	    if (is_crack) {
		/* orientation here is OT_SAME */
		lu_class = NMG_CLASS_AoutB;
	    } else {
		lu_class = compute_loop_class(fpi, lu, &edge_list);
	    }
	} else {
	    /* pt touches edge or vertex */
	    if (RTG.NMG_debug & DEBUG_PT_FU)
		bu_log("loop class already known (pt must touch edge)\n");
	}

	/* free up the edge_list elements */
	while (BU_LIST_WHILE(ei, edge_info, &edge_list.l)) {
	    BU_LIST_DEQUEUE(&ei->l);
	    bu_free((char *)ei, "edge info struct");
	}
    } else if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	register struct vertexuse *vu;
	int v_class;

	vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	v_class = nmg_class_pt_vu(fpi, vu);

	switch (lu->orientation) {
	    case OT_BOOLPLACE:
	    case OT_SAME:
		if (v_class == NMG_FPI_TOUCHED)
		    lu_class = NMG_CLASS_AinB;
		else
		    lu_class = NMG_CLASS_AoutB;
		break;
	    case OT_OPPOSITE:
		/* Why even call nmg_class_pt_vu() here, if return isn't used? */
		lu_class = NMG_CLASS_AinB;
		break;
	    case OT_UNSPEC:
		lu_class = NMG_CLASS_Unknown;
		break;
	    default:
		bu_log("nmg_class_pt_lu() hit %s loop at vu=%p\n",
		       nmg_orientation(lu->orientation), (void *)vu);
		bu_bomb("nmg_class_pt_lu() Loop orientation error\n");
		break;
	}
    } else {
	bu_log("%s:%d bad child of loopuse\n", __FILE__, __LINE__);
	bu_bomb("nmg_class_pt_lu() crash and burn\n");
    }


 out:
    if (RTG.NMG_debug & DEBUG_PT_FU)
	bu_log("nmg_class_pt_lu() pt classed %s vs loop\n", nmg_class_name(lu_class));

    return lu_class;
}


static void
plot_parity_error(const struct faceuse *fu, const fastf_t *pt)
{
    long *b;
    FILE *fp;
    point_t p1, p2;
    int i;

    NMG_CK_FACEUSE(fu);

    fp=fopen("pt_fu_parity_error.plot3", "wb");
    if (!fp)
	bu_bomb("error opening pt_fu_parity_error.plot3\n");

    bu_log("overlay pt_fu_parity_error.plot3\n");

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

    nmg_pl_fu(fp, fu, b, 200, 200, 200);


    /* make a nice axis-cross at the point in question */
    for (i = 0; i < 3; i++) {
	VMOVE(p1, pt);
	p1[i] -= 1.0;
	VMOVE(p2, pt);
	p2[i] += 1.0;
	pdv_3line(fp, p1, p2);
    }

    bu_free((char *)b, "plot table");
    fclose(fp);

}


/**
 *
 * Classify a point on a face's plane as being inside/outside the area
 * of the face.
 *
 * For each loopuse, compute IN/ON/OUT
 *
 * if any loop has pt classified as "ON" return "ON" (actually returns "IN" -jra)
 *
 * ignore all OT_SAME loops w/pt classified as "OUT"
 * ignore all OT_OPPOSITE loops w/pt classified as "IN"
 * If (# OT_SAME loops == # OT_OPPOSITE loops)
 *	pt is "OUT"
 * else if (# OT_SAME loops - 1 == # OT_OPPOSITE loops)
 *	pt is "IN"
 * else
 *	Error! panic!
 *
 *
 * Values for "call_on_hits"
 *	1	find all elements pt touches, call user routine for each geom.
 *	2	find all elements pt touches, call user routine for each use
 *
 * in_or_out_only:
 *	non-zero	pt is known NOT to be on an EU of FU
 *	   0		pt may be on an EU of FU
 *
 * Returns -
 *	NMG_CLASS_AonB, etc...
 */
int
nmg_class_pt_fu_except(const fastf_t *pt, const struct faceuse *fu, const struct loopuse *ignore_lu,
		       void (*eu_func) (struct edgeuse *, point_t, const char *), void (*vu_func) (struct vertexuse *, point_t, const char *), const char *priv,
		       const int call_on_hits, const int in_or_out_only, const struct bn_tol *tol)

    /* func to call when pt on edgeuse */
    /* func to call when pt on vertexuse*/
    /* private data for [ev]u_func */


{
    struct fpi fpi;
    struct loopuse *lu;
    int ot_same_in = 0;
    int ot_opposite_out = 0;
    int ot_same[4];
    int ot_opposite[4];
    int lu_class;
    int fu_class = NMG_CLASS_Unknown;
    double dist;
    struct ve_dist *ved_p;
    int i;

    if (RTG.NMG_debug & DEBUG_PT_FU)
	bu_log("nmg_class_pt_fu_except(pt=(%g %g %g), fu=%p)\n", V3ARGS(pt), (void *)fu);

    if (fu->orientation != OT_SAME) bu_bomb("nmg_class_pt_fu_except() not OT_SAME\n");

    NMG_CK_FACEUSE(fu);
    NMG_CK_FACE(fu->f_p);
    NMG_CK_FACE_G_PLANE(fu->f_p->g.plane_p);
    if (ignore_lu) NMG_CK_LOOPUSE(ignore_lu);
    BN_CK_TOL(tol);

    /* Validate distance from point to plane */
    NMG_GET_FU_PLANE(fpi.norm, fu);
    if ((dist=fabs(DIST_PT_PLANE(pt, fpi.norm))) > tol->dist) {
	bu_log("nmg_class_pt_fu_except() ERROR, point (%g, %g, %g)\nnot on face %g %g %g %g, \ndist=%g\n",
	       V3ARGS(pt), V4ARGS(fpi.norm), dist);
    }

    if (V3PT_OUT_RPP_TOL(pt, fu->f_p->min_pt, fu->f_p->max_pt, tol->dist)) {
	/* point is not in RPP of face, so there's NO WAY this point
	 * is anything but OUTSIDE
	 */
	if (RTG.NMG_debug & DEBUG_PT_FU)
	    bu_log("nmg_class_pt_fu_except((%g %g %g) outside face RPP\n",
		   V3ARGS(pt));

	return NMG_CLASS_AoutB;
    }

    for (i = 0; i < 4; i++) {
	ot_same[i] = ot_opposite[i] = 0;
    }

    fpi.fu_p = fu;
    fpi.tol = tol;
    BU_LIST_INIT(&fpi.ve_dh);
    VMOVE(fpi.pt, pt);
    fpi.eu_func = eu_func;
    fpi.vu_func = vu_func;
    fpi.priv = priv;
    fpi.hits = call_on_hits;
    fpi.magic = NMG_FPI_MAGIC;

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	if (ignore_lu && (ignore_lu==lu || ignore_lu==lu->lumate_p))
	    continue;

	/* Ignore OT_BOOLPLACE, etc. */
	if (lu->orientation != OT_SAME && lu->orientation != OT_OPPOSITE) {
	    if (lu->orientation != OT_BOOLPLACE) {
		bu_bomb("nmg_class_pt_fu_except() lu orientation is not OT_SAME, OT_OPPOSITE or OT_BOOLPLACE\n");
	    }
	    continue;
	}

	lu_class = nmg_class_pt_lu(lu, &fpi, in_or_out_only);
	if (RTG.NMG_debug & DEBUG_PT_FU)
	    bu_log("loop %s says pt is %s\n",
		   nmg_orientation(lu->orientation),
		   nmg_class_name(lu_class));

	if (lu_class < 0 || lu_class > 3) {
	    bu_log("nmg_class_pt_fu_except() lu_class=%s %d\n",
		   nmg_class_name(lu_class), lu_class);
	    bu_bomb("nmg_class_pt_fu_except() bad lu_class\n");
	}

	if (lu->orientation == OT_OPPOSITE) {
	    ot_opposite[lu_class]++;
	    if (lu_class == NMG_CLASS_AoutB) {
		ot_opposite_out++;
	    }
	} else if (lu->orientation == OT_SAME) {
	    ot_same[lu_class]++;
	    if (lu_class == NMG_CLASS_AinB
		|| lu_class == NMG_CLASS_AonBshared
		|| lu_class == NMG_CLASS_AonBanti)
	    {
		ot_same_in++;
	    }
	}
    }

    if (RTG.NMG_debug & DEBUG_PT_FU) {
	bu_log("loops ot_same_in:%d ot_opposite_out:%d\n",
	       ot_same_in, ot_opposite_out);
	bu_log("loops in/onS/onA/out ot_same=%d/%d/%d/%d ot_opp=%d/%d/%d/%d\n",
	       ot_same[0], ot_same[1], ot_same[2], ot_same[3],
	       ot_opposite[0], ot_opposite[1], ot_opposite[2], ot_opposite[3]);
    }

    if (ot_same_in == ot_opposite_out) {
	/* All the holes cancel out the solid loops */
	fu_class = NMG_CLASS_AoutB;
    } else if (ot_same_in > ot_opposite_out) {
	/* XXX How can this difference be > 1 ? */
	fu_class = NMG_CLASS_AinB;
    } else {
	/* Panic time!  How did I get a parity mis-match? */
	bu_log("loops in/onS/onA/out ot_same=%d/%d/%d/%d ot_opp=%d/%d/%d/%d\n",
	       ot_same[0], ot_same[1], ot_same[2], ot_same[3],
	       ot_opposite[0], ot_opposite[1], ot_opposite[2], ot_opposite[3]);
	bu_log("nmg_class_pt_fu_except(%g %g %g)\nParity error @ %s:%d ot_same_in:%d ot_opposite_out:%d\n",
	       V3ARGS(pt), __FILE__, __LINE__,
	       ot_same_in, ot_opposite_out);
	bu_log("fu=%p\n",  (void *)fu);
	nmg_pr_fu_briefly(fu, "");

	plot_parity_error(fu, pt);

	bu_bomb("nmg_class_pt_fu_except() loop classification parity error\n");
    }

    while (BU_LIST_WHILE(ved_p, ve_dist, &fpi.ve_dh)) {
	BU_LIST_DEQUEUE(&ved_p->l);
	bu_free((char *)ved_p, "ve_dist struct");
    }


    if (RTG.NMG_debug & DEBUG_PT_FU)
	bu_log("nmg_class_pt_fu_except() returns %s\n",
	       nmg_class_name(fu_class));

    return fu_class;
}


/**
 * Classify a point as being in/on/out of the area bounded by a loop,
 * ignoring any uses of a particular edge in the loop.
 *
 * This routine must be called with a face-loop of edges!
 * It will not work properly on crack loops.
 */
int
nmg_class_pt_lu_except(fastf_t *pt, const struct loopuse *lu, const struct edge *e_p, const struct bn_tol *tol)
{
    register struct edgeuse *eu;
    struct edge_info edge_list;
    struct edge_info *ei;
    struct fpi fpi;
    int lu_class = NMG_CLASS_Unknown;
    struct ve_dist *ved_p;
    double dist;

    if (RTG.NMG_debug & DEBUG_PT_FU) {
	bu_log("nmg_class_pt_lu_except((%g %g %g) %p ", V3ARGS(pt), (void *)e_p);
	if (e_p)
	    bu_log(" e_p=(%g %g %g) <-> (%g %g %g))\n",
		   V3ARGS(e_p->eu_p->vu_p->v_p->vg_p->coord),
		   V3ARGS(e_p->eu_p->eumate_p->vu_p->v_p->vg_p->coord));
	else
	    bu_log(" e_p=(NULL))\n");
    }

    NMG_CK_LOOPUSE(lu);

    if (e_p) NMG_CK_EDGE(e_p);

    NMG_CK_FACEUSE(lu->up.fu_p);

    /* Validate distance from point to plane */
    NMG_GET_FU_PLANE(fpi.norm, lu->up.fu_p);
    if ((dist=fabs(DIST_PT_PLANE(pt, fpi.norm))) > tol->dist) {
	bu_log("nmg_class_pt_lu_except() ERROR, point (%g, %g, %g)\nnot on face %g %g %g %g, \ndist=%g\n",
	       V3ARGS(pt), V4ARGS(fpi.norm), dist);
    }

    if (V3PT_OUT_RPP_TOL(pt, lu->l_p->lg_p->min_pt, lu->l_p->lg_p->max_pt, tol->dist)) {
	/* point is not in RPP of loop */

	if (RTG.NMG_debug & DEBUG_PT_FU)
	    bu_log("nmg_class_pt_lu_except(pt(%g %g %g) outside loop RPP\n",
		   V3ARGS(pt));

	if (lu->orientation == OT_SAME) return NMG_CLASS_AoutB;
	else if (lu->orientation == OT_OPPOSITE) return NMG_CLASS_AinB;
	else {
	    bu_log("What kind of loop is this anyway? %s?\n",
		   nmg_orientation(lu->orientation));
	    bu_bomb("");
	}
    }

    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	bu_log("%s:%d Improper use of nmg_class_pt_lu_except(pt(%g %g %g), vu)\n",
	       __FILE__, __LINE__, V3ARGS(pt));
	bu_bomb("giving up\n");
    }

    BU_LIST_INIT(&edge_list.l);
    fpi.fu_p = lu->up.fu_p;

    fpi.tol = tol;
    BU_LIST_INIT(&fpi.ve_dh);
    VMOVE(fpi.pt, pt);
    fpi.eu_func = (void (*)(struct edgeuse *, point_t, const char *))NULL;
    fpi.vu_func = (void (*)(struct vertexuse *, point_t, const char *))NULL;
    fpi.priv = (char *)NULL;
    fpi.hits = 0;
    fpi.magic = NMG_FPI_MAGIC;

    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	if (eu->e_p == e_p) {
	    if (RTG.NMG_debug & DEBUG_PT_FU)
		bu_log("skipping edgeuse (%g %g %g) -> (%g %g %g) on \"except\" edge\n",
		       V3ARGS(eu->vu_p->v_p->vg_p->coord),
		       V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord));

	    continue;
	}

	ei = nmg_class_pt_eu(&fpi, eu, &edge_list, 0);
	NMG_CK_EI(ei);
	NMG_CK_VED(ei->ved_p);
	if (ei->ved_p->dist < tol->dist_sq) {
	    lu_class = NMG_CLASS_AonBshared;
	    break;
	}
    }
    if (lu_class == NMG_CLASS_Unknown)
	lu_class = compute_loop_class(&fpi, lu, &edge_list);
    else if (RTG.NMG_debug & DEBUG_PT_FU)
	bu_log("loop class already known (pt must touch edge)\n");

    /* free up the edge_list elements */
    while (BU_LIST_WHILE(ei, edge_info, &edge_list.l)) {
	BU_LIST_DEQUEUE(&ei->l);
	bu_free((char *)ei, "edge info struct");
    }

    while (BU_LIST_WHILE(ved_p, ve_dist, &fpi.ve_dh)) {
	BU_LIST_DEQUEUE(&ved_p->l);
	bu_free((char *)ved_p, "ve_dist struct");
    }

    if (RTG.NMG_debug & DEBUG_PT_FU)
	bu_log("nmg_class_pt_lu_except() returns %s\n",
	       nmg_class_name(lu_class));

    return lu_class;
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
