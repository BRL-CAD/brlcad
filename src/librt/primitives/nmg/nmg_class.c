/*                     N M G _ C L A S S . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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
/** @file primitives/nmg/nmg_class.c
 *
 * Subroutines to classify one object with respect to another.
 * Possible classifications are AinB, AoutB, AinBshared, AinBanti.
 *
 * The first set of routines (nmg_class_pt_xxx) are used to classify
 * an arbitrary point specified by its Cartesian coordinates, against
 * various kinds of NMG elements.  nmg_class_pt_f() and
 * nmg_class_pt_s() are available to applications programmers for
 * direct use, and have no side effects.
 *
 * The second set of routines (class_xxx_vs_s) are used only to
 * support the routine nmg_class_shells() mid-way through the NMG
 * Boolean algorithm.  These routines operate with special knowledge
 * about the state of the data structures after the intersector has
 * been called, and depends on all geometric equivalences to have been
 * converted into shared topology.
 *
 */
/** @} */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "plot3.h"


#define MAX_DIR_TRYS 10

/* XXX These should go the way of the dodo bird. */
#define INSIDE 32
#define ON_SURF 64
#define OUTSIDE 128

extern int nmg_class_nothing_broken;

/* Structure for keeping track of how close a point/vertex is to
 * its potential neighbors.
 */
struct neighbor {
    union {
	const struct vertexuse *vu;
	const struct edgeuse *eu;
    } p;
    /* XXX For efficiency, this should have been dist_sq */
    fastf_t dist;	/* distance from point to neighbor */
    int nmg_class;	/* Classification of this neighbor */
};


static void nmg_class_pt_e(struct neighbor *closest,
			   const point_t pt, const struct edgeuse *eu,
			   const struct bn_tol *tol);
static void nmg_class_pt_l(struct neighbor *closest,
			   const point_t pt, const struct loopuse *lu,
			   const struct bn_tol *tol);
static int class_vu_vs_s(struct vertexuse *vu, struct shell *sB,
			 char **classlist, const struct bn_tol *tol);
static int class_eu_vs_s(struct edgeuse *eu, struct shell *s,
			 char **classlist, const struct bn_tol *tol);
static int class_lu_vs_s(struct loopuse *lu, struct shell *s,
			 char **classlist, const struct bn_tol *tol);
static void class_fu_vs_s(struct faceuse *fu, struct shell *s,
			  char **classlist, const struct bn_tol *tol);

/**
 * Convert classification status to string.
 */
const char *
nmg_class_status(int status)
{
    switch (status) {
	case INSIDE:
	    return "INSIDE";
	case OUTSIDE:
	    return "OUTSIDE";
	case ON_SURF:
	    return "ON_SURF";
    }
    return "BOGUS_CLASS_STATUS";
}


void
nmg_pr_class_status(char *prefix, int status)
{
    bu_log("%s has classification status %s\n",
	   prefix, nmg_class_status(status));
}


/**
 * The ray hit an edge.  We have to decide whether it hit the
 * edge, or missed it.
 *
 * XXX DANGER:  This routine does not work well.
 *
 * Called by -
 * nmg_class_pt_e
 */
static void
joint_hitmiss2(struct neighbor *closest, const struct edgeuse *eu, int code)
{
    const struct edgeuse *eu_rinf;

    eu_rinf = nmg_faceradial(eu);

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("joint_hitmiss2\n");
    }
    if (eu_rinf == eu) {
	bu_bomb("joint_hitmiss2: radial eu is me?\n");
    }
    /* If eu_rinf == eu->eumate_p, that's OK, this is a dangling face,
     * or a face that has not been fully hooked up yet.
     * It's OK as long as the orientations both match.
     */
    if (eu->up.lu_p->orientation == eu_rinf->up.lu_p->orientation) {
	if (eu->up.lu_p->orientation == OT_SAME) {
	    closest->nmg_class = NMG_CLASS_AonBshared;
	} else if (eu->up.lu_p->orientation == OT_OPPOSITE) {
	    closest->nmg_class = NMG_CLASS_AoutB;
	} else {
	    nmg_pr_lu_briefly(eu->up.lu_p, (char *)0);
	    bu_bomb("joint_hitmiss2: bad loop orientation\n");
	}
	closest->dist = 0.0;
	switch (code) {
	    case 0:
		/* Hit the edge */
		closest->p.eu = eu;
		break;
	    case 1:
		/* Hit first vertex */
		closest->p.vu = eu->vu_p;
		break;
	    case 2:
		/* Hit second vertex */
		closest->p.vu = BU_LIST_PNEXT_CIRC(edgeuse, eu)->vu_p;
		break;
	}
	if (RTG.NMG_debug & DEBUG_CLASSIFY) bu_log("\t\t%s\n", nmg_class_name(closest->nmg_class));
	return;
    }

    /* XXX What goes here? */
    bu_bomb("nmg_class.c/joint_hitmiss2() unable to resolve ray/edge hit\n");
    bu_log("joint_hitmiss2: NO CODE HERE, assuming miss\n");

    if (RTG.NMG_debug & (DEBUG_CLASSIFY|DEBUG_NMGRT)) {
	nmg_euprint("Hard question time", eu);
	bu_log(" eu_rinf=%p, eu->eumate_p=%p, eu=%p\n", (void *)eu_rinf, (void *)eu->eumate_p, (void *)eu);
	bu_log(" eu lu orient=%s, eu_rinf lu orient=%s\n",
	       nmg_orientation(eu->up.lu_p->orientation),
	       nmg_orientation(eu_rinf->up.lu_p->orientation));
    }
}


/**
 * XXX DANGER:  This routine does not work well.
 *
 * Given the Cartesian coordinates of a point, determine if the point
 * is closer to this edgeuse than the previous neighbor(s) as given
 * in the "closest" structure.
 * If it is, record how close the point is, and whether it is IN, ON, or OUT.
 * The neighbor's "p" element will indicate the edgeuse or vertexuse closest.
 *
 * This routine should print everything indented two tab stops.
 *
 * Implicit Return -
 * Updated "closest" structure if appropriate.
 *
 * Called by -
 * nmg_class_pt_l
 */
static void
nmg_class_pt_e(struct neighbor *closest, const fastf_t *pt, const struct edgeuse *eu, const struct bn_tol *tol)
{
    vect_t ptvec;	/* vector from lseg to pt */
    vect_t left;	/* vector left of edge -- into inside of loop */
    const fastf_t *eupt;
    const fastf_t *matept;
    point_t pca;	/* point of closest approach from pt to edge lseg */
    fastf_t dist;	/* distance from pca to pt */
    fastf_t dot;
    int code;

    NMG_CK_EDGEUSE(eu);
    NMG_CK_EDGEUSE(eu->eumate_p);
    NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
    NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);
    BN_CK_TOL(tol);
    VSETALL(left, 0);

    eupt = eu->vu_p->v_p->vg_p->coord;
    matept = eu->eumate_p->vu_p->v_p->vg_p->coord;

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	VPRINT("nmg_class_pt_e\tPt", pt);
	nmg_euprint("          \tvs. eu", eu);
    }
    /*
     * Note that "pca" can be one of the edge endpoints,
     * even if "pt" is far, far away.  This can be confusing.
     *
     * Some compilers don't get that fastf_t * and point_t are related
     * So we have to pass the whole bloody mess for the point arguments.
     */
    code = bn_dist_pt3_lseg3(&dist, pca, eu->vu_p->v_p->vg_p->coord,
			     eu->eumate_p->vu_p->v_p->vg_p->coord,
			     pt, tol);
    if (code <= 0) dist = 0;
    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("          \tcode=%d, dist: %g\n", code, dist);
	VPRINT("          \tpca:", pca);
    }

    if (dist >= closest->dist + tol->dist) {
	if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	    bu_log("\t\tskipping, earlier eu is closer (%g)\n", closest->dist);
	}
	return;
    }
    if (dist >= closest->dist - tol->dist) {
	/*
	 * Distances are very nearly the same.
	 *
	 * If closest eu and this eu are from the same lu,
	 * and the earlier result was OUT, that's all it takes
	 * to decide things.
	 */
	if (closest->p.eu && closest->p.eu->up.lu_p == eu->up.lu_p) {
	    if (closest->nmg_class == NMG_CLASS_AoutB ||
		closest->nmg_class == NMG_CLASS_AonBshared ||
		closest->nmg_class == NMG_CLASS_AonBanti) {
		if (RTG.NMG_debug & DEBUG_CLASSIFY)
		    bu_log("\t\tSkipping, earlier eu from same lu at same dist, is OUT or ON.\n");
		return;
	    }
	    if (RTG.NMG_debug & DEBUG_CLASSIFY)
		bu_log("\t\tEarlier eu from same lu at same dist, is IN, continue processing.\n");
	} else {
	    /*
	     * If this is now a new lu, it is more complicated.
	     * If "closest" result so far was a NMG_CLASS_AinB or
	     * or NMG_CLASS_AonB, then keep it,
	     * otherwise, replace that result with whatever we find
	     * here.  Logic:  Two touching loops, one concave ("A")
	     * which wraps around part of the other ("B"), with the
	     * point inside A near the contact with B.  If loop B is
	     * processed first, the closest result will be NMG_CLASS_AoutB,
	     * and when loop A is visited the distances will be exactly
	     * equal, not giving A a chance to claim its hit.
	     */
	    if (closest->nmg_class == NMG_CLASS_AinB ||
		closest->nmg_class == NMG_CLASS_AonBshared ||
		closest->nmg_class == NMG_CLASS_AonBanti) {
		if (RTG.NMG_debug & DEBUG_CLASSIFY)
		    bu_log("\t\tSkipping, earlier eu from other another lu at same dist, is IN or ON\n");
		return;
	    }
	    if (RTG.NMG_debug & DEBUG_CLASSIFY)
		bu_log("\t\tEarlier eu from other lu at same dist, is OUT, continue processing.\n");
	}
    }

    /* Plane hit point is closer to this edgeuse than previous one(s) */
    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("\t\tCLOSER dist=%g (closest=%g), tol=%g\n",
	       dist, closest->dist, tol->dist);
    }

    if (*eu->up.magic_p != NMG_LOOPUSE_MAGIC ||
	*eu->up.lu_p->up.magic_p != NMG_FACEUSE_MAGIC) {
	bu_log("Trying to classify a pt (%g, %g, %g)\n\tvs a wire edge? (%g, %g, %g -> %g, %g, %g)\n",
	       V3ARGS(pt),
	       V3ARGS(eupt),
	       V3ARGS(matept)
	    );
	return;
    }

    if (code <= 2) {
	/* code==0:  The point is ON the edge! */
	/* code==1 or 2:  The point is ON a vertex! */
	if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	    bu_log("\t\tThe point is ON the edge, calling joint_hitmiss2()\n");
	}
	joint_hitmiss2(closest, eu, code);
	return;
    } else {
	if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	    bu_log("\t\tThe point is not on the edge\n");
	}
    }

    /* The point did not lie exactly ON the edge, calculate in/out */

    /* Get vector which lies on the plane, and points
     * left, towards the interior of the loop, regardless of
     * whether it's an interior (OT_OPPOSITE) or exterior (OT_SAME) loop.
     */
    if (nmg_find_eu_leftvec(left, eu) < 0) bu_bomb("nmg_class_pt_e() bad LEFT vector\n");

    VSUB2(ptvec, pt, pca);		/* pt - pca */
    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	VPRINT("\t\tptvec unnorm", ptvec);
	VPRINT("\t\tleft", left);
    }
    VUNITIZE(ptvec);

    dot = VDOT(left, ptvec);
    if (NEAR_ZERO(dot, RT_DOT_TOL)) {
	if (RTG.NMG_debug & DEBUG_CLASSIFY)
	    bu_log("\t\tpt lies on line of edge, outside verts. Skipping this edge\n");
	goto out;
    }

    if (dot > -SMALL_FASTF) {
	closest->dist = dist;
	closest->p.eu = eu;
	closest->nmg_class = NMG_CLASS_AinB;
	if (RTG.NMG_debug & DEBUG_CLASSIFY)
	    bu_log("\t\tpt is left of edge, INSIDE loop, dot=%g\n", dot);
    } else {
	closest->dist = dist;
	closest->p.eu = eu;
	closest->nmg_class = NMG_CLASS_AoutB;
	if (RTG.NMG_debug & DEBUG_CLASSIFY)
	    bu_log("\t\tpt is right of edge, OUTSIDE loop\n");
    }

out:
    /* XXX Temporary addition for chasing bug in Bradley r5 */
    /* XXX Should at least add DEBUG_PLOTEM check, later */
    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	struct faceuse *fu;
	char buf[128];
	static int num;
	FILE *fp;
	long *bits;
	point_t mid_pt;
	point_t left_pt;
	fu = eu->up.lu_p->up.fu_p;
	bits = (long *)bu_calloc(nmg_find_model(&fu->l.magic)->maxindex, sizeof(long), "bits[]");
	sprintf(buf, "faceclass%d.plot3", num++);
	if ((fp = fopen(buf, "wb")) == NULL)
	    bu_bomb(buf);
	nmg_pl_fu(fp, fu, bits, 0, 0, 255);	/* blue */
	pl_color(fp, 0, 255, 0);	/* green */
	pdv_3line(fp, pca, pt);
	pl_color(fp, 255, 0, 0);	/* red */
	VADD2SCALE(mid_pt, eupt, matept, 0.5);
	VJOIN1(left_pt, mid_pt, 500, left);
	pdv_3line(fp, mid_pt, left_pt);
	fclose(fp);
	bu_free((char *)bits, "bits[]");
	bu_log("wrote %s\n", buf);
    }
}


/**
 * XXX DANGER:  This routine does not work well.
 *
 * Given the coordinates of a point which lies on the plane of the face
 * containing a loopuse, determine if the point is IN, ON, or OUT of the
 * area enclosed by the loop.
 *
 * Implicit Return -
 * Updated "closest" structure if appropriate.
 *
 * Called by -
 * nmg_class_pt_loop()
 *		from: nmg_extrude.c / nmg_fix_overlapping_loops()
 * nmg_classify_lu_lu()
 *		from: nmg_misc.c / nmg_split_loops_handler()
 */
static void
nmg_class_pt_l(struct neighbor *closest, const fastf_t *pt, const struct loopuse *lu, const struct bn_tol *tol)
{
    vect_t delta;
    pointp_t lu_pt;
    fastf_t dist;
    struct edgeuse *eu;
    struct loop_g *lg;

    NMG_CK_LOOPUSE(lu);
    NMG_CK_LOOP(lu->l_p);
    lg = lu->l_p->lg_p;
    NMG_CK_LOOP_G(lg);

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	VPRINT("nmg_class_pt_l\tPt:", pt);
    }

    if (*lu->up.magic_p != NMG_FACEUSE_MAGIC)
	return;

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	plane_t peqn;
	nmg_pr_lu_briefly(lu, 0);
	NMG_GET_FU_PLANE(peqn, lu->up.fu_p);
	HPRINT("\tplane eqn", peqn);
    }

    if (V3PT_OUT_RPP_TOL(pt, lg->min_pt, lg->max_pt, tol->dist)) {
	if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	    bu_log("\tPoint is outside loop RPP\n");
	}
	closest->nmg_class = NMG_CLASS_AoutB;
	return;
    }
    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    nmg_class_pt_e(closest, pt, eu, tol);
	    /* If point lies ON edge, we are done */
	    if (closest->nmg_class == NMG_CLASS_AonBanti) {
		break;
	    } else if (closest->nmg_class == NMG_CLASS_AonBshared) {
		bu_bomb("nmg_class_pt_l(): nmg_class_pt_e returned AonBshared but can only return AonBanti\n");
	    }
	}
    } else if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	register struct vertexuse *vu;
	vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	lu_pt = vu->v_p->vg_p->coord;
	VSUB2(delta, pt, lu_pt);
	if ((dist = MAGNITUDE(delta)) < closest->dist) {
	    if (lu->orientation == OT_OPPOSITE) {
		closest->nmg_class = NMG_CLASS_AoutB;
	    } else if (lu->orientation == OT_SAME) {
		closest->nmg_class = NMG_CLASS_AonBanti;
	    } else {
		nmg_pr_orient(lu->orientation, "\t");
		bu_bomb("nmg_class_pt_l: bad orientation for face loopuse\n");
	    }
	    if (RTG.NMG_debug & DEBUG_CLASSIFY)
		bu_log("\t\t closer to loop pt (%g, %g, %g)\n",
		       V3ARGS(lu_pt));

	    closest->dist = dist;
	    closest->p.vu = vu;
	}
    } else {
	bu_bomb("nmg_class_pt_l() bad child of loopuse\n");
    }
    if (RTG.NMG_debug & DEBUG_CLASSIFY)
	bu_log("nmg_class_pt_l\treturning, closest=%g %s\n",
	       closest->dist, nmg_class_name(closest->nmg_class));
}


/**
 * This is intended as an internal routine to support nmg_lu_reorient().
 *
 * Given a loopuse in a face, pick one of its vertexuses, and classify
 * that point with respect to all the rest of the loopuses in the face.
 * The containment status of that point is the status of the loopuse.
 *
 * If the first vertex chosen is "ON" another loop boundary,
 * choose the next vertex and try again.  Only return an "ON"
 * status if _all_ the vertices are ON.
 *
 * The point is "A", and the face is "B".
 *
 * Returns -
 * NMG_CLASS_AinB lu is INSIDE the area of the face.
 * NMG_CLASS_AonBshared ALL of lu is ON other loop boundaries.
 * NMG_CLASS_AoutB lu is OUTSIDE the area of the face.
 *
 * Called by -
 * nmg_mod.c, nmg_lu_reorient()
 */
int
nmg_class_lu_fu(const struct loopuse *lu, const struct bn_tol *tol)
{
    const struct faceuse *fu;
    struct vertexuse *vu;
    const struct vertex_g *vg;
    struct edgeuse *eu;
    struct edgeuse *eu_first;
    fastf_t dist;
    plane_t n;
    int nmg_class;

    NMG_CK_LOOPUSE(lu);
    BN_CK_TOL(tol);

    fu = lu->up.fu_p;
    NMG_CK_FACEUSE(fu);
    NMG_CK_FACE(fu->f_p);
    NMG_CK_FACE_G_PLANE(fu->f_p->g.plane_p);

    if (RTG.NMG_debug & DEBUG_CLASSIFY)
	bu_log("nmg_class_lu_fu(lu=%p) START\n", (void *)lu);

    /* Pick first vertex in loopuse, for point */
    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	eu = (struct edgeuse *)NULL;
    } else {
	eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	NMG_CK_EDGEUSE(eu);
	vu = eu->vu_p;
    }
    eu_first = eu;
again:
    NMG_CK_VERTEXUSE(vu);
    NMG_CK_VERTEX(vu->v_p);
    vg = vu->v_p->vg_p;
    NMG_CK_VERTEX_G(vg);

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	VPRINT("nmg_class_lu_fu\tPt:", vg->coord);
    }

    /* Validate distance from point to plane */
    NMG_GET_FU_PLANE(n, fu);
    if ((dist = fabs(DIST_PT_PLANE(vg->coord, n))) > tol->dist) {
	bu_log("nmg_class_lu_fu() ERROR, point (%g, %g, %g) not on face, dist=%g\n",
	       V3ARGS(vg->coord), dist);
    }

    /* find the closest approach in this face to the projected point */
    nmg_class = nmg_class_pt_fu_except(vg->coord, fu, lu,
				       NULL, NULL, NULL, 0, 0, tol);

    /* If this vertex lies ON loop edge, must check all others. */
    if (nmg_class == NMG_CLASS_AonBshared) {
	if (!eu_first) {
	    /* was self-loop, nothing more to test */
	} else {
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    if (eu != eu_first) {
		vu = eu->vu_p;
		goto again;
	    }
	    /* all match, call it "ON" */
	}
    }

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("nmg_class_lu_fu(lu=%p) END, ret=%s\n",
	       (void *)lu,
	       nmg_class_name(nmg_class));
    }
    return nmg_class;
}


/* Ray direction vectors for Jordan curve algorithm */
static const point_t nmg_good_dirs[MAX_DIR_TRYS] = {
#if 1
    {3, 2, 1},	/* Normally the first dir */
#else
    {1, 0, 0},	/* DEBUG: Make this first dir to wring out ray-tracer */
#endif
    {1, 0, 0},
    {0, 1, 0},
    {0, 0, 1},
    {1, 1, 1},
    {-3, -2, -1},
    {-1, 0, 0},
    {0, -1, 0},
    {0, 0, -1},
    {-1, -1, -1}
};


/**
 * This is intended as a general user interface routine.
 * Given the Cartesian coordinates for a point in space,
 * return the classification for that point with respect to a shell.
 *
 * The algorithm used is to fire a ray from the point, and count
 * the number of times it crosses a face.
 *
 * The flag "in_or_out_only" specifies that the point is known to not
 * be on the shell, therefore only returns of NMG_CLASS_AinB or
 * NMG_CLASS_AoutB are acceptable.
 *
 * The point is "A", and the face is "B".
 *
 * Returns -
 * NMG_CLASS_AinB pt is INSIDE the volume of the shell.
 * NMG_CLASS_AonBshared pt is ON the shell boundary.
 * NMG_CLASS_AoutB pt is OUTSIDE the volume of the shell.
 */
int
nmg_class_pt_s(const fastf_t *pt, const struct shell *s, const int in_or_out_only, const struct bn_tol *tol)
{
    const struct faceuse *fu;
    struct model *m;
    long *faces_seen = NULL;
    vect_t region_diagonal;
    fastf_t region_diameter;
    int nmg_class = 0;
    vect_t projection_dir = VINIT_ZERO;
    int tries = 0;
    struct xray rp;
    fastf_t model_bb_max_width;
    point_t m_min_pt, m_max_pt; /* nmg model min and max points */

    m = s->r_p->m_p;
    NMG_CK_MODEL(m);
    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("nmg_class_pt_s(): pt=(%g, %g, %g), s=%p\n",
	       V3ARGS(pt), (void *)s);
    }
    if (V3PT_OUT_RPP_TOL(pt, s->sa_p->min_pt, s->sa_p->max_pt, tol->dist)) {
	if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	    bu_log("nmg_class_pt_s(): OUT, point not in RPP\n");
	}
	return NMG_CLASS_AoutB;
    }

    if (!in_or_out_only) {
	faces_seen = (long *)bu_calloc(m->maxindex, sizeof(long), "nmg_class_pt_s faces_seen[]");
	/*
	 * First pass:  Try hard to see if point is ON a face.
	 */
	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    plane_t n;

	    /* If this face processed before, skip on */
	    if (NMG_INDEX_TEST(faces_seen, fu->f_p)) {
		continue;
	    }
	    /* Only consider the outward pointing faceuses */
	    if (fu->orientation != OT_SAME) {
		continue;
	    }

	    /* See if this point lies on this face */
	    NMG_GET_FU_PLANE(n, fu);
	    if ((fabs(DIST_PT_PLANE(pt, n))) < tol->dist) {
		/* Point lies on this plane, it may be possible to
		 * short circuit everything.
		 */
		nmg_class = nmg_class_pt_fu_except(pt, fu, (struct loopuse *)0,
						   (void (*)(struct edgeuse *, point_t, const char *))NULL,
						   (void (*)(struct vertexuse *, point_t, const char *))NULL,
						   (const char *)NULL, 0, 0, tol);
		if (nmg_class == NMG_CLASS_AonBshared) {
		    bu_bomb("nmg_class_pt_s(): function nmg_class_pt_fu_except returned AonBshared when it can only return AonBanti\n");
		}
		if (nmg_class == NMG_CLASS_AonBshared) {
		    /* Point is ON face, therefore it must be
		     * ON the shell also.
		     */
		    nmg_class = NMG_CLASS_AonBshared;
		    goto out;
		}
		if (nmg_class == NMG_CLASS_AonBanti) {
		    /* Point is ON face, therefore it must be
		     * ON the shell also.
		     */
		    nmg_class = NMG_CLASS_AonBanti;
		    goto out;
		}

		if (nmg_class == NMG_CLASS_AinB) {
		    /* Point is IN face, therefor it must be
		     * ON the shell also.
		     */
		    nmg_class = NMG_CLASS_AonBanti;
		    goto out;
		}
		/* Point is OUTside face, its undecided. */
	    }

	    /* Mark this face as having been processed */
	    NMG_INDEX_SET(faces_seen, fu->f_p);
	}
    }

    /* If we got here, the point isn't ON any of the faces.
     * Time to do the Jordan Curve Theorem.  We fire an arbitrary
     * ray and count the number of crossings (in the positive direction)
     * If that number is even, we're outside the shell, otherwise we're
     * inside the shell.
     */
    NMG_CK_REGION_A(s->r_p->ra_p);
    VSUB2(region_diagonal, s->r_p->ra_p->max_pt, s->r_p->ra_p->min_pt);
    region_diameter = MAGNITUDE(region_diagonal);

    nmg_model_bb(m_min_pt, m_max_pt, m);
    model_bb_max_width = bn_dist_pt3_pt3(m_min_pt, m_max_pt);

    /* Choose an unlikely direction */
    tries = 0;
retry:
    if (tries < MAX_DIR_TRYS) {
	projection_dir[X] = nmg_good_dirs[tries][X];
	projection_dir[Y] = nmg_good_dirs[tries][Y];
	projection_dir[Z] = nmg_good_dirs[tries][Z];
    }

    if (++tries >= MAX_DIR_TRYS) {
	goto out; /* only nmg_good_dirs to try */
    }

    VUNITIZE(projection_dir);

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("nmg_class_pt_s(): Pt=(%g, %g, %g) dir=(%g, %g, %g), reg_diam=%g\n",
	       V3ARGS(pt), V3ARGS(projection_dir), region_diameter);
    }

    VMOVE(rp.r_pt, pt);

    /* give the ray a length which is at least the max
     * length of the nmg model bounding box.
     */
    VSCALE(rp.r_dir, projection_dir, model_bb_max_width * 1.25);

    /* get NMG ray-tracer to tell us if start point is inside or outside */
    nmg_class = nmg_class_ray_vs_shell(&rp, s, in_or_out_only, tol);

    if (nmg_class == NMG_CLASS_AonBshared) {
	bu_bomb("nmg_class_pt_s(): function nmg_class_ray_vs_shell returned AonBshared when it can only return AonBanti\n");
    }
    if (nmg_class == NMG_CLASS_Unknown) {
	goto retry;
    }

out:
    if (!in_or_out_only) {
	bu_free((char *)faces_seen, "nmg_class_pt_s faces_seen[]");
    }

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("nmg_class_pt_s(): returning %s, s=%p, tries=%d\n",
	       nmg_class_name(nmg_class), (void *)s, tries);
    }

    return nmg_class;
}


/**
 * Classify a loopuse/vertexuse from shell A WRT shell B.
 */
static int
class_vu_vs_s(struct vertexuse *vu, struct shell *sB, char **classlist, const struct bn_tol *tol)
{
    struct vertexuse *vup;
    pointp_t pt;
    char *reason;
    int status = 0;
    int nmg_class;
    struct vertex *sv;

    NMG_CK_VERTEXUSE(vu);
    NMG_CK_SHELL(sB);
    BN_CK_TOL(tol);

    pt = vu->v_p->vg_p->coord;

    if (RTG.NMG_debug & DEBUG_CLASSIFY)
	bu_log("class_vu_vs_s(vu=%p, v=%p) pt=(%g, %g, %g)\n", (void *)vu, (void *)vu->v_p, V3ARGS(pt));

    /* As a mandatory consistency measure, check for cached classification */
    reason = "of cached classification";
    if (NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], vu->v_p)) {
	status = INSIDE;
	goto out;
    }
    if (NMG_INDEX_TEST(classlist[NMG_CLASS_AonBshared], vu->v_p)) {
	status = ON_SURF;
	goto out;
    }
    if (NMG_INDEX_TEST(classlist[NMG_CLASS_AonBanti], vu->v_p)) {
	status = ON_SURF;
	goto out;
    }
    if (NMG_INDEX_TEST(classlist[NMG_CLASS_AoutB], vu->v_p)) {
	status = OUTSIDE;
	goto out;
    }

    /* we use topology to determine if the vertex is "ON" the
     * other shell.
     */
    for (BU_LIST_FOR(vup, vertexuse, &vu->v_p->vu_hd)) {

	if (*vup->up.magic_p == NMG_LOOPUSE_MAGIC) {
	    if (nmg_find_s_of_lu(vup->up.lu_p) != sB) {
		continue;
	    }
	    NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], vu->v_p);
	    reason = "a loopuse of vertex is on shell";
	    status = ON_SURF;
	    goto out;
	} else if (*vup->up.magic_p == NMG_EDGEUSE_MAGIC) {
	    if (nmg_find_s_of_eu(vup->up.eu_p) != sB) {
		continue;
	    }
	    NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], vu->v_p);
	    reason = "an edgeuse of vertex is on shell";
	    status = ON_SURF;
	    goto out;
	} else if (*vup->up.magic_p == NMG_SHELL_MAGIC) {
	    if (vup->up.s_p != sB) {
		continue;
	    }
	    NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], vu->v_p);
	    reason = "vertex is shell's lone vertex";
	    status = ON_SURF;
	    goto out;
	} else {
	    bu_bomb("class_vu_vs_s(): bad vertex UP pointer\n");
	}
    }

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("class_vu_vs_s(): Can't classify vertex via topology\n");
    }

    /* The topology doesn't tell us about the vertex being "on" the shell
     * so now it's time to look at the geometry to determine the vertex
     * relationship to the shell.
     *
     * The vertex should *not* lie ON any of the faces or
     * edges, since the intersection algorithm would have combined the
     * topology if that had been the case.
     */

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	if ((sv = nmg_find_pt_in_shell(sB, pt, tol))) {
	    bu_log("vu=%p, v=%p, sv=%p, pt=(%g, %g, %g)\n",
		   (void *)vu, (void *)vu->v_p, (void *)sv, V3ARGS(pt));
	    bu_bomb("class_vu_vs_s(): logic error, vertex topology not shared properly\n");
	}
    }

    reason = "of nmg_class_pt_s()";
    /* 3rd parameter '0' allows return of AonBanti instead of
     * only AinB or AoutB
     */
    nmg_class = nmg_class_pt_s(pt, sB, 0, tol);

    if (nmg_class == NMG_CLASS_AonBshared) {
	bu_bomb("class_vu_vs_s(): function nmg_class_pt_s returned AonBshared when it can only return AonBanti\n");
    }
    if (nmg_class == NMG_CLASS_AoutB) {
	NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], vu->v_p);
	status = OUTSIDE;
    }  else if (nmg_class == NMG_CLASS_AinB) {
	NMG_INDEX_SET(classlist[NMG_CLASS_AinB], vu->v_p);
	status = INSIDE;
    }  else if (nmg_class == NMG_CLASS_AonBshared) {
	NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], vu->v_p);
	status = ON_SURF;
    }  else if (nmg_class == NMG_CLASS_AonBanti) {
	NMG_INDEX_SET(classlist[NMG_CLASS_AonBanti], vu->v_p);
	status = ON_SURF;
    } else {
	bu_log("class_vu_vs_s(): nmg_class=%s\n", nmg_class_name(nmg_class));
	VPRINT("pt", pt);
	bu_bomb("class_vu_vs_s(): nmg_class_pt_s() failure\n");
    }

out:
    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("class_vu_vs_s(vu=%p) return %s because %s\n",
	       (void *)vu, nmg_class_status(status), reason);
    }
    return status;
}


static int
class_eu_vs_s(struct edgeuse *eu, struct shell *s, char **classlist, const struct bn_tol *tol)
{
    int euv_cl, matev_cl;
    int status = 0;
    struct edgeuse *eup;
    point_t pt;
    pointp_t eupt, matept;
    char *reason = "Unknown";
    int nmg_class;
    vect_t e_min_pt;
    vect_t e_max_pt;

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("class_eu_vs_s(eu=%p (e_p=%p, lu=%p), s=%p)\n",
	       (void *)eu, (void *)eu->e_p, (void *)eu->up.lu_p, (void *)s);
	nmg_euprint("class_eu_vs_s\t", eu);
    }

    NMG_CK_EDGEUSE(eu);
    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    /* As a mandatory consistency measure, check for cached classification */
    reason = "of cached classification";
    if (NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], eu->e_p)) {
	status = INSIDE;
	goto out;
    }
    if (NMG_INDEX_TEST(classlist[NMG_CLASS_AonBshared], eu->e_p)) {
	status = ON_SURF;
	goto out;
    }
    if (NMG_INDEX_TEST(classlist[NMG_CLASS_AonBanti], eu->e_p)) {
	status = ON_SURF;
	goto out;
    }
    if (NMG_INDEX_TEST(classlist[NMG_CLASS_AoutB], eu->e_p)) {
	status = OUTSIDE;
	goto out;
    }

    /* find the bounding box of the edge */
    VMOVE(e_min_pt, eu->vu_p->v_p->vg_p->coord);
    VMIN(e_min_pt, eu->eumate_p->vu_p->v_p->vg_p->coord);
    VMOVE(e_max_pt, eu->vu_p->v_p->vg_p->coord);
    VMAX(e_max_pt, eu->eumate_p->vu_p->v_p->vg_p->coord);

    /* if the edge and shell bounding boxes are disjoint by at least
     * distance tolerance then the edge is outside the shell. also
     * both vertices of the edge are outside the shell.
     */
    if (V3RPP_DISJOINT_TOL(e_min_pt, e_max_pt, s->sa_p->min_pt, s->sa_p->max_pt, tol->dist)) {
	NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], eu->e_p);
	NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], eu->vu_p->v_p);
	NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], eu->eumate_p->vu_p->v_p);
	status = OUTSIDE;
	goto out;
    }

    euv_cl = class_vu_vs_s(eu->vu_p, s, classlist, tol);
    matev_cl = class_vu_vs_s(eu->eumate_p->vu_p, s, classlist, tol);

    /* sanity check */
    if ((euv_cl == INSIDE && matev_cl == OUTSIDE) ||
	(euv_cl == OUTSIDE && matev_cl == INSIDE)) {
	static int num = 0;
	char buf[128];
	FILE *fp;

	sprintf(buf, "nmg_class%d.plot3", num++);
	if ((fp = fopen(buf, "wb")) == NULL) {
	    bu_bomb(buf);
	}
	nmg_pl_s(fp, s);
	/* A yellow line for the angry edge */
	pl_color(fp, 255, 255, 0);
	pdv_3line(fp, eu->vu_p->v_p->vg_p->coord,
		  eu->eumate_p->vu_p->v_p->vg_p->coord);
	fclose(fp);

	nmg_pr_class_status("class_eu_vs_s(): eu vu", euv_cl);
	nmg_pr_class_status("class_eu_vs_s(): eumate vu", matev_cl);
	if (RT_G_DEBUG || RTG.NMG_debug) {
	    /* Do them over, so we can watch */
	    bu_log("class_eu_vs_s(): Edge not cut, doing it over\n");
	    NMG_INDEX_CLEAR(classlist[NMG_CLASS_AinB], eu->vu_p);
	    NMG_INDEX_CLEAR(classlist[NMG_CLASS_AoutB], eu->vu_p);
	    NMG_INDEX_CLEAR(classlist[NMG_CLASS_AinB], eu->eumate_p->vu_p);
	    NMG_INDEX_CLEAR(classlist[NMG_CLASS_AoutB], eu->eumate_p->vu_p);
	    RTG.NMG_debug |= DEBUG_CLASSIFY;
	    (void)class_vu_vs_s(eu->vu_p, s, classlist, tol);
	    (void)class_vu_vs_s(eu->eumate_p->vu_p, s, classlist, tol);
	    nmg_euprint("class_eu_vs_s(): didn't this edge get cut?", eu);
	    nmg_pr_eu(eu, "class_eu_vs_s():");
	}

	bu_log("class_eu_vs_s(): wrote %s\n", buf);
	bu_bomb("class_eu_vs_s():  edge didn't get cut\n");
    }

    if (euv_cl == ON_SURF && matev_cl == ON_SURF) {
	vect_t eu_dir;
	int tries;

	/* check for radial uses of this edge by the shell */
	eup = eu->radial_p->eumate_p;
	do {
	    NMG_CK_EDGEUSE(eup);
	    if (nmg_find_s_of_eu(eup) == s) {
		NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], eu->e_p);
		reason = "a radial edgeuse is on shell";
		status = ON_SURF;
		goto out;
	    }
	    eup = eup->radial_p->eumate_p;
	} while (eup != eu->radial_p->eumate_p);

	/* look for another eu between these two vertices */
	if (nmg_find_matching_eu_in_s(eu, s)) {
	    NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], eu->e_p);
	    reason = "another eu between same vertices is on shell";
	    status = ON_SURF;
	    goto out;
	}

	/* although the two endpoints are "on" the shell,
	 * the edge would appear to be either "inside" or "outside",
	 * since there are no uses of this edge in the shell.
	 *
	 * So we classify the midpoint of the line WRT the shell.
	 *
	 * Consider AE as being "eu", with M as the geometric midpoint.
	 * Consider AD as being a side view of a perpendicular plane
	 * in the other shell, which AE _almost_ lies in.
	 * A problem occurs when the angle DAE is very small.
	 * Point A is ON because of shared topology.
	 * Point E is marked ON because it is within tolerance of
	 * the face, even though there is no corresponding vertex.
	 * Naturally, point M is also going to be found to be ON
	 * because it is also within tolerance.
	 *
	 *         D
	 *        /|
	 *       / |
	 *      B  |
	 *     /   |
	 *    /    |
	 *   A--M--E
	 *
	 * This would seem to be an intersector problem.
	 */
	nmg_class = NMG_CLASS_Unknown;
	eupt = eu->vu_p->v_p->vg_p->coord;
	matept = eu->eumate_p->vu_p->v_p->vg_p->coord;
	if (nmg_class == NMG_CLASS_Unknown) {
	    VSUB2(eu_dir, matept, eupt);
	}

	tries = 0;
	while (nmg_class == NMG_CLASS_Unknown && tries < 3) {
	    /* Must resort to ray trace */
	    switch (tries) {
		case 0 :
		    VJOIN1(pt, eupt, 0.5, eu_dir);
		    reason = "midpoint classification (both verts ON)";
		    break;
		case 1:
		    VJOIN1(pt, eupt, 0.1, eu_dir);
		    reason = "point near EU start classification (both verts ON)";
		    break;
		case 2:
		    VJOIN1(pt, eupt, 0.9, eu_dir);
		    reason = "point near EU end classification (both verts ON)";
		    break;
	    }
	    /* 3rd parameter of '0' allows a return of AonBanti
	     * instead of only AinB or AoutB.
	     */
	    nmg_class = nmg_class_pt_s(pt, s, 0, tol);
	    tries++;
	}

	if (nmg_class == NMG_CLASS_AonBshared) {
	    bu_bomb("class_eu_vs_s(): function nmg_class_pt_s returned AonBshared when it can only return AonBanti\n");
	}
	if (nmg_class == NMG_CLASS_AoutB) {
	    NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], eu->e_p);
	    status = OUTSIDE;
	}  else if (nmg_class == NMG_CLASS_AinB) {
	    NMG_INDEX_SET(classlist[NMG_CLASS_AinB], eu->e_p);
	    status = INSIDE;
	} else if (nmg_class == NMG_CLASS_AonBanti) {
	    NMG_INDEX_SET(classlist[NMG_CLASS_AonBanti], eu->e_p);
	    status = ON_SURF;
	} else {
	    bu_log("class_eu_vs_s(): nmg_class=%s\n", nmg_class_name(nmg_class));
	    nmg_euprint("class_eu_vs_s(): Why wasn't this edge in or out?", eu);
	    bu_bomb("class_eu_vs_s(): nmg_class_pt_s() midpoint failure\n");
	}
	goto out;
    }

    if (euv_cl == OUTSIDE || matev_cl == OUTSIDE) {
	NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], eu->e_p);
	reason = "at least one vert OUT";
	status = OUTSIDE;
	goto out;
    }
    if (euv_cl == INSIDE && matev_cl == INSIDE) {
	NMG_INDEX_SET(classlist[NMG_CLASS_AinB], eu->e_p);
	reason = "both verts IN";
	status = INSIDE;
	goto out;
    }
    if ((euv_cl == INSIDE && matev_cl == ON_SURF) ||
	(euv_cl == ON_SURF && matev_cl == INSIDE)) {
	NMG_INDEX_SET(classlist[NMG_CLASS_AinB], eu->e_p);
	reason = "one vert IN, one ON";
	status = INSIDE;
	goto out;
    }
    bu_log("class_eu_vs_s(): eu's vert is %s, mate's vert is %s\n",
	   nmg_class_status(euv_cl), nmg_class_status(matev_cl));
    bu_bomb("class_eu_vs_s(): inconsistent edgeuse\n");

out:
    if (RTG.NMG_debug & DEBUG_GRAPHCL)
	nmg_show_broken_classifier_stuff((uint32_t *)eu, classlist, nmg_class_nothing_broken, 0, (char *)NULL);
    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("class_eu_vs_s(eu=%p) return %s because %s\n",
	       (void *)eu, nmg_class_status(status), reason);
    }
    return status;
}


/* XXX move to nmg_info.c */
/**
 * Given two edgeuses in different faces that share a common edge,
 * determine if they are from identical loops or not.
 *
 * Note that two identical loops in an anti-shared pair of faces
 * (faces with opposite orientations) will also have opposite orientations.
 *
 * Returns -
 * 0 Loops not identical
 * 1 Loops identical, faces are ON-shared
 * 2 Loops identical, faces are ON-anti-shared
 * 3 Loops identical, at least one is a wire loop.
 */
int
nmg_2lu_identical(const struct edgeuse *eu1, const struct edgeuse *eu2)
{
    const struct loopuse *lu1;
    const struct loopuse *lu2;
    const struct edgeuse *eu1_first;
    const struct faceuse *fu1;
    const struct faceuse *fu2;
    int ret;

    NMG_CK_EDGEUSE(eu1);
    NMG_CK_EDGEUSE(eu2);

    if (eu1->e_p != eu2->e_p) bu_bomb("nmg_2lu_identical() differing edges?\n");

    /* get the starting vertex of each edgeuse to be the same. */
    if (eu2->vu_p->v_p != eu1->vu_p->v_p) {
	eu2 = eu2->eumate_p;
	if (eu2->vu_p->v_p != eu1->vu_p->v_p)
	    bu_bomb("nmg_2lu_identical() radial edgeuse doesn't share vertices\n");
    }

    lu1 = eu1->up.lu_p;
    lu2 = eu2->up.lu_p;

    NMG_CK_LOOPUSE(lu1);
    NMG_CK_LOOPUSE(lu2);

    /* march around the two loops to see if they
     * are the same all the way around.
     */
    eu1_first = eu1;
    do {
	if (eu1->vu_p->v_p != eu2->vu_p->v_p) {
	    ret = 0;
	    goto out;
	}
	eu1 = BU_LIST_PNEXT_CIRC(edgeuse, &eu1->l);
	eu2 = BU_LIST_PNEXT_CIRC(edgeuse, &eu2->l);
    } while (eu1 != eu1_first);

    if (*lu1->up.magic_p != NMG_FACEUSE_MAGIC ||
	*lu2->up.magic_p != NMG_FACEUSE_MAGIC) {
	ret = 3;	/* one is a wire loop */
	goto out;
    }

    fu1 = lu1->up.fu_p;
    fu2 = lu2->up.fu_p;

    if (fu1->f_p->g.plane_p != fu2->f_p->g.plane_p) {
	vect_t fu1_norm, fu2_norm;

	/* Drop back to using a geometric calculation */
	if (fu1->orientation != fu2->orientation)
	    NMG_GET_FU_NORMAL(fu2_norm, fu2->fumate_p)
	    else
		NMG_GET_FU_NORMAL(fu2_norm, fu2)

		    NMG_GET_FU_NORMAL(fu1_norm, fu1);

	if (VDOT(fu1_norm, fu2_norm) < -SMALL_FASTF)
	    ret = 2;	/* ON anti-shared */
	else
	    ret = 1;	/* ON shared */
	goto out;
    }

    if (RTG.NMG_debug & DEBUG_BASIC) {
	bu_log("---- fu1, f=%p, flip=%d\n", (void *)fu1->f_p, fu1->f_p->flip);
	nmg_pr_fu_briefly(fu1, 0);
	bu_log("---- fu2, f=%p, flip=%d\n", (void *)fu2->f_p, fu2->f_p->flip);
	nmg_pr_fu_briefly(fu2, 0);
    }

    /*
     * The two loops are identical, compare the two faces.
     * Only raw face orientations count here.
     * Loopuse and faceuse orientations do not matter.
     */
    if (fu1->f_p->flip != fu2->f_p->flip)
	ret = 2;		/* ON anti-shared */
    else
	ret = 1;		/* ON shared */
out:
    if (RTG.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_2lu_identical(eu1=%p, eu2=%p) ret=%d\n",
	       (void *)eu1, (void *)eu2, ret);
    }
    return ret;
}


/**
 * Make all the edges and vertices of a loop carry the same classification
 * as the loop.
 * There is no intrinsic way to tell if an edge is "shared" or
 * "antishared", except by reference to its loopuse, but the heritage
 * of the edgeuse makes a difference to the boolean evaluator.
 *
 * "newclass" should only be AonBshared or AonBanti.
 */
void
nmg_reclassify_lu_eu(struct loopuse *lu, char **classlist, int newclass)
{
    struct vertexuse *vu;
    struct edgeuse *eu;
    struct vertex *v;

    NMG_CK_LOOPUSE(lu);

    if (RTG.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_reclassify_lu_eu(lu=%p, classlist=%p, newclass=%s)\n",
	       (void *)lu, (void *)classlist, nmg_class_name(newclass));
    }

    if (newclass != NMG_CLASS_AonBshared && newclass != NMG_CLASS_AonBanti)
	bu_bomb("nmg_reclassify_lu_eu() bad newclass\n");

    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	NMG_CK_VERTEXUSE(vu);
	v = vu->v_p;
	NMG_CK_VERTEX(v);

	if (NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], v) ||
	    NMG_INDEX_TEST(classlist[NMG_CLASS_AoutB], v))
	    bu_bomb("nmg_reclassify_lu_eu() changing in/out vert of lone vu loop to ON?\n");

	/* Clear out other classifications */
	NMG_INDEX_CLEAR(classlist[NMG_CLASS_AonBshared], v);
	NMG_INDEX_CLEAR(classlist[NMG_CLASS_AonBanti], v);
	/* Establish new classification */
	NMG_INDEX_SET(classlist[newclass], v);
	return;
    }

    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	struct edge *e;
	NMG_CK_EDGEUSE(eu);
	e = eu->e_p;
	NMG_CK_EDGE(e);

	if (NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], e) ||
	    NMG_INDEX_TEST(classlist[NMG_CLASS_AoutB], e))
	    bu_bomb("nmg_reclassify_lu_eu() changing in/out edge to ON?\n");

	/* Clear out other edge classifications */
	NMG_INDEX_CLEAR(classlist[NMG_CLASS_AonBshared], e);
	NMG_INDEX_CLEAR(classlist[NMG_CLASS_AonBanti], e);
	/* Establish new edge classification */
	NMG_INDEX_SET(classlist[newclass], e);

	/* Same thing for the vertices.  Only need to do start vert here. */
	vu = eu->vu_p;
	NMG_CK_VERTEXUSE(vu);
	v = vu->v_p;
	NMG_CK_VERTEX(v);

	if (NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], v) ||
	    NMG_INDEX_TEST(classlist[NMG_CLASS_AoutB], v))
	    bu_bomb("nmg_reclassify_lu_eu() changing in/out vert to ON?\n");

	/* Clear out other classifications */
	NMG_INDEX_CLEAR(classlist[NMG_CLASS_AonBshared], v);
	NMG_INDEX_CLEAR(classlist[NMG_CLASS_AonBanti], v);
	/* Establish new classification */
	NMG_INDEX_SET(classlist[newclass], v);
    }
}


/**
 * Classify LU w.r.t. LU_REF.
 *
 * Pre-requisites, required (but not checked for here):
 *
 * LU shares all its edges with LU_REF
 * LU and LU_REF are loopuses from faceuses
 * LU and LU_REF are from different shells
 * LU and LU_REF must both be loops of edges
 *
 * LU is classified w.r.t. LU_REF by the following algorithm:
 *
 * 1. Select any EU from LU.
 *
 * 2. Find EU_REF from LU_REF that is shared with EU.
 *
 * 3. If left vector of EU is opposite left vector of EU_REF, then
 * classification is either NMG_CLASS_AoutB or NMG_CLASS_AinB.  (This
 * is the case where one lu exactly fills a hole that the other lu
 * creates in a face).
 *
 * 4. If the left vectors agree, then the loops are shared - If the
 * direction of EU agrees with the direction of EU_REF, then classify
 * as NMG_CLASS_AonBshared.  Otherwise classify as NMG_CLASS_AonBanti.
 *
 * returns:
 * NMG_CLASS_AonBshared
 * NMG_CLASS_AonBanti
 * NMG_CLASS_AoutB
 */
static int
class_shared_lu(const struct loopuse *lu, const struct loopuse *lu_ref, const struct bn_tol *tol)
{
    struct shell *s_ref;
    struct edgeuse *eu;
    struct edgeuse *eu_ref;
    struct edgeuse *eu_start, *eu_tmp;
    struct faceuse *fu_of_lu;
    vect_t left;
    vect_t left_ref;

    if (RTG.NMG_debug & DEBUG_CLASSIFY)
	bu_log("class_shared_lu: classifying lu %p w.r.t. lu_ref %p\n",
	       (void *)lu, (void *)lu_ref);

    NMG_CK_LOOPUSE(lu);
    NMG_CK_LOOPUSE(lu_ref);
    BN_CK_TOL(tol);

    eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
    for (BU_LIST_FOR(eu_ref, edgeuse, &lu_ref->down_hd)) {
	if (eu_ref->e_p == eu->e_p)
	    break;
    }

    if (eu_ref->e_p != eu->e_p) {
	bu_log("class_shared_lu() couldn't find a shared EU between LU's %p and %p\n",
	       (void *)lu, (void *)lu_ref);
	bu_bomb("class_shared_lu() couldn't find a shared EU between LU's\n");
    }

    if (nmg_find_eu_leftvec(left, eu)) {
	bu_log("class_shared_lu() couldn't get a left vector for EU %p\n", (void *)eu);
	bu_bomb("class_shared_lu() couldn't get a left vector for EU\n");
    }
    if (nmg_find_eu_leftvec(left_ref, eu_ref)) {
	bu_log("class_shared_lu() couldn't get a left vector for EU %p\n", (void *)eu_ref);
	bu_bomb("class_shared_lu() couldn't get a left vector for EU\n");
    }

    if (VDOT(left, left_ref) > SMALL_FASTF) {
	if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	    bu_log("eu %p goes from v %p to v %p\n",
		   (void *)eu, (void *)eu->vu_p->v_p, (void *)eu->eumate_p->vu_p->v_p);
	    bu_log("eu_ref %p goes from v %p to v %p\n",
		   (void *)eu_ref, (void *)eu_ref->vu_p->v_p, (void *)eu_ref->eumate_p->vu_p->v_p);
	}

	/* loop is either shared or anti */
	if (eu->vu_p->v_p == eu_ref->vu_p->v_p) {
	    if (RTG.NMG_debug & DEBUG_CLASSIFY)
		bu_log("class_shared_lu returning NMG_CLASS_AonBshared\n");
	    return NMG_CLASS_AonBshared;
	} else {
	    if (RTG.NMG_debug & DEBUG_CLASSIFY)
		bu_log("class_shared_lu returning NMG_CLASS_AonBanti\n");
	    return NMG_CLASS_AonBanti;
	}
    }

    /* loop is either in or out
     * look at next radial edge from lu_ref shell if that faceuse
     * is OT_OPPOSITE, then we are "in", else "out"
     */
    s_ref = nmg_find_s_of_eu(eu_ref);
    fu_of_lu = nmg_find_fu_of_lu(lu);

    for (BU_LIST_FOR(eu_start, edgeuse, &lu->down_hd)) {
	int use_this_eu = 1;

	eu_tmp = eu_start->eumate_p->radial_p;
	while (eu_tmp != eu_start) {
	    struct faceuse *fu_tmp;
	    struct loopuse *lu_tmp;
	    int nmg_class;

	    fu_tmp = nmg_find_fu_of_eu(eu_tmp);
	    if (fu_tmp != fu_of_lu && fu_tmp->fumate_p != fu_of_lu) {
		eu_tmp = eu_tmp->eumate_p->radial_p;
		continue;
	    }

	    /* radial edge is part of same face
	     * make sure it is part of a loop */
	    if (*eu_tmp->up.magic_p != NMG_LOOPUSE_MAGIC) {
		eu_tmp = eu_tmp->eumate_p->radial_p;
		continue;
	    }
	    lu_tmp = eu_tmp->up.lu_p;
	    if (fu_tmp->fumate_p == fu_of_lu)
		lu_tmp = lu_tmp->lumate_p;

	    if (lu_tmp == lu) {
		/* cannot classify based on this edgeuse */
		use_this_eu = 0;
		break;
	    }

	    nmg_class = nmg_classify_lu_lu(lu_tmp, lu, tol);
	    if (lu->orientation == OT_OPPOSITE && nmg_class == NMG_CLASS_AoutB) {
		/* cannot classify based on this edgeuse */
		use_this_eu = 0;
		break;
	    } else if (lu->orientation == OT_SAME && nmg_class == NMG_CLASS_AinB) {
		/* cannot classify based on this edgeuse */
		use_this_eu = 0;
		break;
	    }

	    eu_tmp = eu_tmp->eumate_p->radial_p;
	}
	if (!use_this_eu)
	    continue;

	/* O.K. we can classify using this eu, look radially for a faceuse
	 * from the reference shell
	 */
	eu_tmp = eu_start->eumate_p->radial_p;
	while (eu_tmp != eu_start) {
	    struct faceuse *fu_tmp;

	    if (nmg_find_s_of_eu(eu_tmp) == s_ref) {
		fu_tmp = nmg_find_fu_of_eu(eu_tmp);
		if (fu_tmp->orientation == OT_SAME)
		    return NMG_CLASS_AoutB;
		else
		    return NMG_CLASS_AinB;
	    }

	    eu_tmp = eu_tmp->eumate_p->radial_p;
	}
    }

    if (RTG.NMG_debug & DEBUG_CLASSIFY)
	bu_log("class_shared_lu returning NMG_CLASS_Unknown at end\n");
    return NMG_CLASS_Unknown;
}


/**
 * Called by -
 * class_fu_vs_s
 */
static int
class_lu_vs_s(struct loopuse *lu, struct shell *s, char **classlist, const struct bn_tol *tol)
{
    int nmg_class;
    unsigned int in, outside, on;
    struct edgeuse *eu, *p;
    struct loopuse *q_lu;
    struct vertexuse *vu;
    uint32_t magic1;
    char *reason = "Unknown";
    int seen_error = 0;
    int status = 0;

    if (RTG.NMG_debug & DEBUG_CLASSIFY)
	bu_log("class_lu_vs_s(lu=%p, s=%p)\n", (void *)lu, (void *)s);

    NMG_CK_LOOPUSE(lu);
    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    if (nmg_find_s_of_lu(lu) == s) {
	bu_log("class_lu_vs_s() is trying to classify lu %p vs its own shell (%p)\n",
	       (void *)lu, (void *)s);
	bu_bomb("class_lu_vs_s() is trying to classify lu vs its own shell");
    }

    /* check to see if loop is already in one of the lists */
    if (NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], lu->l_p)) {
	reason = "of classlist";
	nmg_class = NMG_CLASS_AinB;
	status = INSIDE;
	goto out;
    }

    if (NMG_INDEX_TEST(classlist[NMG_CLASS_AonBshared], lu->l_p)) {
	reason = "of classlist";
	nmg_class = NMG_CLASS_AonBshared;
	status = ON_SURF;
	goto out;
    }
    if (NMG_INDEX_TEST(classlist[NMG_CLASS_AonBanti], lu->l_p)) {
	reason = "of classlist";
	nmg_class = NMG_CLASS_AonBanti;
	status = ON_SURF;
	goto out;
    }

    if (NMG_INDEX_TEST(classlist[NMG_CLASS_AoutB], lu->l_p)) {
	reason = "of classlist";
	nmg_class = NMG_CLASS_AoutB;
	status = OUTSIDE;
	goto out;
    }

    magic1 = BU_LIST_FIRST_MAGIC(&lu->down_hd);
    if (magic1 == NMG_VERTEXUSE_MAGIC) {
	/* Loop of a single vertex */
	reason = "of vertex classification";
	vu = BU_LIST_PNEXT(vertexuse, &lu->down_hd);
	NMG_CK_VERTEXUSE(vu);
	nmg_class = class_vu_vs_s(vu, s, classlist, tol);
	switch (nmg_class) {
	    case INSIDE:
		NMG_INDEX_SET(classlist[NMG_CLASS_AinB], lu->l_p);
		break;
	    case OUTSIDE:
		NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], lu->l_p);
		break;
	    case ON_SURF:
		NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], lu->l_p);
		break;
	    default:
		bu_bomb("class_lu_vs_s(): bad vertexloop classification\n");
	}
	status = nmg_class;
	goto out;
    } else if (magic1 != NMG_EDGEUSE_MAGIC) {
	bu_bomb("class_lu_vs_s(): bad child of loopuse\n");
    }

    /* loop is collection of edgeuses */
    seen_error = 0;

    do {

	in = outside = on = 0;
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    /* Classify each edgeuse */
	    nmg_class = class_eu_vs_s(eu, s, classlist, tol);
	    switch (nmg_class) {
		case INSIDE		: ++in;
		    break;
		case OUTSIDE	: ++outside;
		    break;
		case ON_SURF	: ++on;
		    break;
		default		: bu_bomb("class_lu_vs_s(): bad class for edgeuse\n");
	    }
	}

	if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	    bu_log("class_lu_vs_s: Loopuse edges in:%d on:%d out:%d\n", in, on, outside);
	}

	if (in > 0 && outside > 0) {
	    FILE *fp;

	    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
		char buf[128];
		static int num;
		long *b;
		struct model *m;

		m = nmg_find_model(lu->up.magic_p);
		b = (long *)bu_calloc(m->maxindex, sizeof(long), "nmg_pl_lu flag[]");
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    if (NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], eu->e_p))
			nmg_euprint("In:  edgeuse", eu);
		    else if (NMG_INDEX_TEST(classlist[NMG_CLASS_AoutB], eu->e_p))
			nmg_euprint("Out: edgeuse", eu);
		    else if (NMG_INDEX_TEST(classlist[NMG_CLASS_AonBshared], eu->e_p))
			nmg_euprint("OnShare:  edgeuse", eu);
		    else if (NMG_INDEX_TEST(classlist[NMG_CLASS_AonBanti], eu->e_p))
			nmg_euprint("OnAnti:  edgeuse", eu);
		    else
			nmg_euprint("BAD: edgeuse", eu);
		}
		sprintf(buf, "badloop%d.plot3", num++);
		if ((fp=fopen(buf, "wb")) != NULL) {
		    nmg_pl_lu(fp, lu, b, 255, 255, 255);
		    nmg_pl_s(fp, s);
		    fclose(fp);
		    bu_log("wrote %s\n", buf);
		}
		nmg_pr_lu(lu, "");
		nmg_stash_model_to_file("class.g", nmg_find_model((uint32_t *)lu), "class_ls_vs_s: loop transits plane of shell/face?");
		bu_free((char *)b, "nmg_pl_lu flag[]");
	    }

	    if (seen_error > 3) {
		bu_bomb("class_lu_vs_s(): loop transits plane of shell/face?\n");
	    }
	    seen_error++;
	    continue;
	}

	/* in <=0 || outside <= 0 so were good */
	break;

    } while (1);

    if (outside > 0) {
	NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], lu->l_p);
	reason = "edgeuses were OUT and ON";
	nmg_class = NMG_CLASS_AoutB;
	status = OUTSIDE;
	goto out;
    } else if (in > 0) {
	NMG_INDEX_SET(classlist[NMG_CLASS_AinB], lu->l_p);
	reason = "edgeuses were IN and ON";
	nmg_class = NMG_CLASS_AinB;
	status = INSIDE;
	goto out;
    } else if (on == 0) {
	bu_bomb("class_lu_vs_s(): missing edgeuses\n");
    }

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("class_lu_vs_s(): All edgeuses of loop are ON\n");
    }

    /* since all of the edgeuses of this loop are "on" the other shell,
     * we need to see if this loop is "on" the other shell
     *
     * if we're a wire edge loop, simply having all edges "on" is
     * sufficient.
     *
     * foreach radial edgeuse
     * if edgeuse vertex is same and edgeuse parent shell is the one
     * desired, then....
     *
     * p = edgeuse, q = radial edgeuse
     * while p's vertex equals q's vertex and we
     *			haven't come full circle
     *			move p and q forward
     * if we made it full circle, loop is on
     */

    if (*lu->up.magic_p == NMG_SHELL_MAGIC) {
	NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], lu->l_p);
	reason = "loop is a wire loop in the shell";
	nmg_class = NMG_CLASS_AonBshared;
	status = ON_SURF;
	goto out;
    }

    NMG_CK_FACEUSE(lu->up.fu_p);

    nmg_class = NMG_CLASS_Unknown;
    eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
    for (
	eu = eu->radial_p->eumate_p;
	eu != BU_LIST_FIRST(edgeuse, &lu->down_hd);
	eu = eu->radial_p->eumate_p)
    {
	struct faceuse *fu_qlu;
	struct edgeuse *eu1;
	struct edgeuse *eu2;
	int found_match;

	/* if the radial edge is a part of a loop which is part of
	 * a face, then it's one that we might be "on"
	 */
	if (*eu->up.magic_p != NMG_LOOPUSE_MAGIC) {
	    continue;
	}

	q_lu = eu->up.lu_p;
	if (*q_lu->up.magic_p != NMG_FACEUSE_MAGIC) {
	    continue;
	}

	fu_qlu = q_lu->up.fu_p;
	NMG_CK_FACEUSE(fu_qlu);

	if (q_lu == lu) {
	    continue;
	}

	/* Only consider faces from shell 's' */
	if (q_lu->up.fu_p->s_p != s) {
	    continue;
	}

	if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	    bu_log("\tfound radial lu (%p), check for match\n", (void *)q_lu);
	}

	/* now check if eu's match in both LU's */
	eu1 = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	if (eu1->vu_p->v_p == eu->vu_p->v_p) {
	    /* true when eu1-lu and eu-lu are opposite i.e. cw -vs- ccw */
	    eu2 = eu;
	} else if (eu1->vu_p->v_p == eu->eumate_p->vu_p->v_p) {
	    /* true when eu1-lu and eu-lu are same i.e. cw or ccw */
	    eu2 = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	} else {
	    eu2 = BU_LIST_PPREV_CIRC(edgeuse, &eu->l);
	}

	found_match = 1;
	do {
	    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
		bu_log("\t\tcompare vertex %p to vertex %p\n", (void *)eu1->vu_p->v_p, (void *)eu2->vu_p->v_p);
	    }
	    if (eu1->vu_p->v_p != eu2->vu_p->v_p) {
		if (RTG.NMG_debug & DEBUG_CLASSIFY) {
		    bu_log("\t\t\tnot a match\n");
		}
		found_match = 0;
		break;
	    }
	    eu1 = BU_LIST_PNEXT_CIRC(edgeuse, &eu1->l);
	    eu2 = BU_LIST_PNEXT_CIRC(edgeuse, &eu2->l);
	} while (eu1 != BU_LIST_FIRST(edgeuse, &lu->down_hd));

	if (!found_match) {
	    /* check opposite direction */
	    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
		bu_log("\tChecking for match in opposite direction\n");
	    }
	    eu1 = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	    if (eu1->vu_p->v_p == eu->vu_p->v_p) {
		eu2 = eu;
	    } else if (eu1->vu_p->v_p == eu->eumate_p->vu_p->v_p) {
		eu2 = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    } else {
		eu2 = BU_LIST_PPREV_CIRC(edgeuse, &eu->l);
	    }

	    found_match = 1;
	    do {
		if (RTG.NMG_debug & DEBUG_CLASSIFY)
		    bu_log("\t\tcompare vertex %p to vertex %p\n",
			   (void *)eu1->vu_p->v_p, (void *)eu2->vu_p->v_p);
		if (eu1->vu_p->v_p != eu2->vu_p->v_p) {
		    if (RTG.NMG_debug & DEBUG_CLASSIFY)
			bu_log("\t\t\tnot a match\n");
		    found_match = 0;
		    break;
		}
		eu1 = BU_LIST_PNEXT_CIRC(edgeuse, &eu1->l);
		eu2 = BU_LIST_PPREV_CIRC(edgeuse, &eu2->l);
	    } while (eu1 != BU_LIST_FIRST(edgeuse, &lu->down_hd));
	}

	if (found_match) {
	    int test_class = NMG_CLASS_Unknown;

	    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
		bu_log("\tFound a matching LU's %p and %p\n", (void *)lu, (void *)q_lu);
	    }

	    if (fu_qlu->orientation == OT_SAME) {
		test_class = class_shared_lu(lu, q_lu, tol);
	    } else if (fu_qlu->orientation == OT_OPPOSITE) {
		test_class = class_shared_lu(lu, q_lu->lumate_p, tol);
	    } else {
		bu_log("class_lu_vs_s: FU %p for lu %p matching lu %p has bad orientation (%s)\n",
		       (void *)fu_qlu, (void *)lu, (void *)q_lu, nmg_orientation(fu_qlu->orientation));
		bu_bomb("class_lu_vs_s: FU has bad orientation\n");
	    }

	    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
		bu_log("\tclass_shared_lu says %s\n", nmg_class_name(test_class));
	    }

	    if (nmg_class == NMG_CLASS_Unknown) {
		nmg_class = test_class;
	    } else if (test_class == NMG_CLASS_AonBshared || test_class == NMG_CLASS_AonBanti) {
		nmg_class = test_class;
	    }

	    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
		bu_log("\tclass set to %s\n",  nmg_class_name(nmg_class));
	    }

	    if (nmg_class == NMG_CLASS_AonBshared) {
		break;
	    }
	}
    }

    if (nmg_class != NMG_CLASS_Unknown) {
	if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	    bu_log("Final nmg_class = %s\n", nmg_class_name(nmg_class));
	}
	NMG_INDEX_SET(classlist[nmg_class], lu->l_p);
	if (nmg_class == NMG_CLASS_AonBanti) {
	    nmg_reclassify_lu_eu(lu, classlist, NMG_CLASS_AonBanti);
	}
	reason = "edges identical with radial face";
	status = ON_SURF;
	goto out;
    }

    /* OK, the edgeuses are all "on", but the loop isn't.  Time to
     * decide if the loop is "out" or "in".  To do this, we look for
     * an edgeuse radial to one of the edgeuses in the loop which is
     * a part of a face in the other shell.  If/when we find such a
     * radial edge, we check the direction (in/out) of the faceuse normal.
     * if the faceuse normal is pointing out of the shell, we are outside.
     * if the faceuse normal is pointing into the shell, we are inside.
     */

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("Checking radial faces:\n");
	nmg_pr_fu_around_eu(eu, tol);
    }

    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	p = eu->radial_p;
	do {
	    if (*p->up.magic_p == NMG_LOOPUSE_MAGIC &&
		*p->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
		p->up.lu_p->up.fu_p->s_p == s) {

		if (p->up.lu_p->up.fu_p->orientation == OT_OPPOSITE) {
		    NMG_INDEX_SET(classlist[NMG_CLASS_AinB], lu->l_p);
		    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
			bu_log("Loop is INSIDE of fu %p\n", (void *)p->up.lu_p->up.fu_p);
		    }
		    reason = "radial faceuse is OT_OPPOSITE";
		    nmg_class = NMG_CLASS_AinB;
		    status = INSIDE;
		    goto out;
		} else if (p->up.lu_p->up.fu_p->orientation == OT_SAME) {
		    NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], lu->l_p);
		    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
			bu_log("Loop is OUTSIDEof fu %p\n", (void *)p->up.lu_p->up.fu_p);
		    }
		    reason = "radial faceuse is OT_SAME";
		    nmg_class = NMG_CLASS_AoutB;
		    status = OUTSIDE;
		    goto out;
		} else {
		    bu_bomb("class_lu_vs_s(): bad fu orientation\n");
		}
	    }
	    p = p->eumate_p->radial_p;
	} while (p != eu->eumate_p);
    }

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("Loop is OUTSIDE 'cause it isn't anything else\n");
    }

    /* Since we didn't find any radial faces to classify ourselves against
     * and we already know that the edges are all "on" that must mean that
     * the loopuse is "on" a wireframe portion of the shell.
     */

    NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], lu->l_p);
    reason = "loopuse is ON a wire loop in the shell";
    nmg_class = NMG_CLASS_AoutB;
    status = OUTSIDE;

out:
    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("class_lu_vs_s(lu=%p) return %s (%s) because %s\n",
	       (void *)lu, nmg_class_status(status), nmg_class_name(nmg_class), reason);
    }

    return status;
}


/**
 * Called by -
 *	nmg_class_shells()
 */
static void
class_fu_vs_s(struct faceuse *fu, struct shell *s, char **classlist, const struct bn_tol *tol)
{
    struct loopuse *lu;
    int nmg_class, in, out, on;
    plane_t n;

    NMG_CK_FACEUSE(fu);
    NMG_CK_SHELL(s);

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	NMG_GET_FU_PLANE(n, fu);
	PLPRINT("\nclass_fu_vs_s plane equation:", n);
    }

    in = out = on = 0;
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	nmg_class = class_lu_vs_s(lu, s, classlist, tol);
	switch (nmg_class) {
	    case INSIDE	        : ++in;
		break;
	    case OUTSIDE        : ++out;
		break;
	    case ON_SURF        : ++on;
		break;
	    default             : bu_bomb("class_fu_vs_s: bad class for faceuse\n");
	}
    }

    if (in == 0 && out == 0 && on > 0) {
	NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], fu->f_p);
    } else if (in == 0 && out > 0 && on == 0) {
	NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], fu->f_p);
    } else {
	NMG_INDEX_SET(classlist[NMG_CLASS_AinB], fu->f_p);
    }

    if (RTG.NMG_debug & DEBUG_CLASSIFY)
	bu_log("class_fu_vs_s() END\n");
}


/**
 * Classify one shell WRT the other shell
 *
 * Implicit return -
 * Each element's classification will be represented by a
 * SET entry in the appropriate classlist[] array.
 *
 * Called by -
 * nmg_bool.c
 */
void
nmg_class_shells(struct shell *sA, struct shell *sB, char **classlist, const struct bn_tol *tol)
{
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;

    NMG_CK_SHELL(sA);
    NMG_CK_SHELL(sB);
    BN_CK_TOL(tol);

    if (RTG.NMG_debug & DEBUG_CLASSIFY &&
	BU_LIST_NON_EMPTY(&sA->fu_hd))
	bu_log("nmg_class_shells - doing faces\n");

    fu = BU_LIST_FIRST(faceuse, &sA->fu_hd);
    while (BU_LIST_NOT_HEAD(fu, &sA->fu_hd)) {

	class_fu_vs_s(fu, sB, classlist, tol);

	if (BU_LIST_PNEXT(faceuse, fu) == fu->fumate_p)
	    fu = BU_LIST_PNEXT_PNEXT(faceuse, fu);
	else
	    fu = BU_LIST_PNEXT(faceuse, fu);
    }

    if (RTG.NMG_debug & DEBUG_CLASSIFY &&
	BU_LIST_NON_EMPTY(&sA->lu_hd))
	bu_log("nmg_class_shells - doing loops\n");

    lu = BU_LIST_FIRST(loopuse, &sA->lu_hd);
    while (BU_LIST_NOT_HEAD(lu, &sA->lu_hd)) {

	(void)class_lu_vs_s(lu, sB, classlist, tol);

	if (BU_LIST_PNEXT(loopuse, lu) == lu->lumate_p)
	    lu = BU_LIST_PNEXT_PNEXT(loopuse, lu);
	else
	    lu = BU_LIST_PNEXT(loopuse, lu);
    }

    if (RTG.NMG_debug & DEBUG_CLASSIFY &&
	BU_LIST_NON_EMPTY(&sA->eu_hd))
	bu_log("nmg_class_shells - doing edges\n");

    eu = BU_LIST_FIRST(edgeuse, &sA->eu_hd);
    while (BU_LIST_NOT_HEAD(eu, &sA->eu_hd)) {

	(void)class_eu_vs_s(eu, sB, classlist, tol);

	if (BU_LIST_PNEXT(edgeuse, eu) == eu->eumate_p)
	    eu = BU_LIST_PNEXT_PNEXT(edgeuse, eu);
	else
	    eu = BU_LIST_PNEXT(edgeuse, eu);
    }

    if (sA->vu_p) {
	if (RTG.NMG_debug)
	    bu_log("nmg_class_shells - doing vertex\n");
	(void)class_vu_vs_s(sA->vu_p, sB, classlist, tol);
    }
}


/**
 * A generally available interface to nmg_class_pt_l
 *
 * returns the classification from nmg_class_pt_l
 * or a (-1) on error
 *
 * Called by -
 * nmg_extrude.c / nmg_fix_overlapping_loops()
 *
 * XXX DANGER:  Calls nmg_class_pt_l(), which does not work well.
 */
int
nmg_classify_pt_loop(const point_t pt,
		     const struct loopuse *lu,
		     const struct bn_tol *tol)
{
    struct neighbor closest;
    struct faceuse *fu;
    plane_t n;
    fastf_t dist;

    NMG_CK_LOOPUSE(lu);
    BN_CK_TOL(tol);

    bu_log("DANGER: nmg_classify_pt_loop() is calling nmg_class_pt_l(), which does not work well\n");
    if (*lu->up.magic_p != NMG_FACEUSE_MAGIC) {
	bu_log("nmg_classify_pt_loop: lu not part of a faceuse!!\n");
	return -1;
    }

    fu = lu->up.fu_p;

    /* Validate distance from point to plane */
    NMG_GET_FU_PLANE(n, fu);
    if ((dist = fabs(DIST_PT_PLANE(pt, n))) > tol->dist) {
	bu_log("nmg_classify_pt_l() ERROR, point (%g, %g, %g) not on face, dist=%g\n",
	       V3ARGS(pt), dist);
	return -1;
    }


    /* find the closest approach in this face to the projected point */
    closest.dist = MAX_FASTF;
    closest.p.eu = (struct edgeuse *)NULL;
    closest.nmg_class = NMG_CLASS_AoutB;	/* default return */

    nmg_class_pt_l(&closest, pt, lu, tol);

    return closest.nmg_class;
}


/**
 * Find any point that is interior to LU
 *
 * Returns:
 * 0 - All is well
 * 1 - Loop is not part of a faceuse
 * 2 - Loop is a single vertexuse
 * 3 - Loop is a crack
 * 4 - Just plain can't find an interior point
 */
int
nmg_get_interior_pt(fastf_t *pt, const struct loopuse *lu, const struct bn_tol *tol)
{
    struct edgeuse *eu;
    fastf_t point_count = 0.0;
    double one_over_count;
    point_t test_pt;
    point_t average_pt;
    int i;

    NMG_CK_LOOPUSE(lu);
    BN_CK_TOL(tol);

    if (*lu->up.magic_p != NMG_FACEUSE_MAGIC)
	return 1;

    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	return 2;

    if (nmg_loop_is_a_crack(lu))
	return 3;

    /* first try just averaging all the vertices */
    VSETALL(average_pt, 0.0);
    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	struct vertex_g *vg;
	NMG_CK_EDGEUSE(eu);

	vg = eu->vu_p->v_p->vg_p;
	NMG_CK_VERTEX_G(vg);

	VADD2(average_pt, average_pt, vg->coord);
	point_count++;
    }

    one_over_count = 1.0/point_count;
    VSCALE(average_pt, average_pt, one_over_count);
    VMOVE(test_pt, average_pt);

    if (nmg_class_pt_lu_except(test_pt, lu, (struct edge *)NULL, tol) == NMG_CLASS_AinB) {
	VMOVE(pt, test_pt);
	return 0;
    }

    for (i = 0; i < 3; i++) {

	double tol_mult;

	/* Try moving just a little left of an edge */
	switch (i) {
	    case 0:
		tol_mult = 5.0 * tol->dist;
		break;
	    case 1:
		tol_mult = 1.5 * tol->dist;
		break;
	    case 2:
		tol_mult = 1.005 * tol->dist;
		break;
	    default:
		/* sanity check */
		tol_mult = 1;
		break;
	}
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    vect_t left;
	    struct vertex_g *vg1, *vg2;

	    (void)nmg_find_eu_leftvec(left, eu);

	    vg1 = eu->vu_p->v_p->vg_p;
	    vg2 = eu->eumate_p->vu_p->v_p->vg_p;

	    VADD2(test_pt, vg1->coord, vg2->coord);
	    VSCALE(test_pt, test_pt, 0.5);

	    VJOIN1(test_pt, test_pt, tol_mult, left);
	    if (nmg_class_pt_lu_except(test_pt, lu, (struct edge *)NULL, tol) == NMG_CLASS_AinB) {
		VMOVE(pt, test_pt);
		return 0;
	    }
	}
    }

    if (RTG.NMG_debug & DEBUG_CLASSIFY) {
	bu_log("nmg_get_interior_pt: Couldn't find interior point for lu %p\n", (void *)lu);
	nmg_pr_lu_briefly(lu, "");
    }

    VMOVE(pt, average_pt);
    return 4;
}


/**
 * Generally available classifier for
 * determining if one loop is within another
 *
 * returns classification based on nmg_class_pt_l results
 *
 * Called by -
 * nmg_misc.c / nmg_split_loops_handler()
 *
 */
int
nmg_classify_lu_lu(const struct loopuse *lu1, const struct loopuse *lu2, const struct bn_tol *tol)
{
    struct faceuse *fu1, *fu2;
    struct edgeuse *eu;
    int share_edges;
    int lu1_eu_count = 0;
    int lu2_eu_count = 0;

    NMG_CK_LOOPUSE(lu1);
    NMG_CK_LOOPUSE(lu2);
    BN_CK_TOL(tol);

    if (RTG.NMG_debug & DEBUG_CLASSIFY)
	bu_log("nmg_classify_lu_lu(lu1=%p, lu2=%p)\n", (void *)lu1, (void *)lu2);

    if (lu1 == lu2 || lu1 == lu2->lumate_p)
	return NMG_CLASS_AonBshared;

    if (*lu1->up.magic_p != NMG_FACEUSE_MAGIC) {
	bu_log("nmg_classify_lu_lu: lu1 not part of a faceuse\n");
	return -1;
    }

    if (*lu2->up.magic_p != NMG_FACEUSE_MAGIC) {
	bu_log("nmg_classify_lu_lu: lu2 not part of a faceuse\n");
	return -1;
    }

    fu1 = lu1->up.fu_p;
    NMG_CK_FACEUSE(fu1);
    fu2 = lu2->up.fu_p;
    NMG_CK_FACEUSE(fu2);

    if (fu1->f_p != fu2->f_p) {
	bu_log("nmg_classify_lu_lu: loops are not in same face\n");
	return -1;
    }

    /* do simple check for two loops of the same vertices */
    if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_EDGEUSE_MAGIC &&
	BU_LIST_FIRST_MAGIC(&lu2->down_hd) == NMG_EDGEUSE_MAGIC) {
	struct edgeuse *eu1_start, *eu2_start;
	struct edgeuse *eu1, *eu2;

	/* count EU's in lu1 */
	for (BU_LIST_FOR(eu, edgeuse, &lu1->down_hd))
	    lu1_eu_count++;

	/* count EU's in lu2 */
	for (BU_LIST_FOR(eu, edgeuse, &lu2->down_hd))
	    lu2_eu_count++;

	share_edges = 1;
	eu1_start = BU_LIST_FIRST(edgeuse, &lu1->down_hd);
	NMG_CK_EDGEUSE(eu1_start);
	eu2_start = BU_LIST_FIRST(edgeuse, &lu2->down_hd);
	NMG_CK_EDGEUSE(eu2_start);
	while (BU_LIST_NOT_HEAD(eu2_start, &lu2->down_hd) &&
	       eu2_start->e_p != eu1_start->e_p)
	{
	    NMG_CK_EDGEUSE(eu2_start);
	    eu2_start = BU_LIST_PNEXT(edgeuse, &eu2_start->l);
	}

	if (BU_LIST_NOT_HEAD(eu2_start, &lu2->down_hd) &&
	    eu1_start->e_p == eu2_start->e_p)
	{
	    /* check the rest of the loop */
	    share_edges = 1;
	    eu1 = eu1_start;
	    eu2 = eu2_start;
	    do {
		if (eu1->e_p != eu2->e_p) {
		    share_edges = 0;
		    break;
		}
		eu1 = BU_LIST_PNEXT_CIRC(edgeuse, &eu1->l);
		eu2 = BU_LIST_PNEXT_CIRC(edgeuse, &eu2->l);
	    } while (eu1 != eu1_start);

	    if (!share_edges) {
		/* maybe the other way round */
		share_edges = 1;
		eu1 = eu1_start;
		eu2 = eu2_start;
		do {
		    if (eu1->e_p != eu2->e_p) {
			share_edges = 0;
			break;
		    }
		    eu1 = BU_LIST_PNEXT_CIRC(edgeuse, &eu1->l);
		    eu2 = BU_LIST_PPREV_CIRC(edgeuse, &eu2->l);
		} while (eu1 != eu1_start);
	    }

	    if (share_edges && lu1_eu_count == lu2_eu_count) {
		if (RTG.NMG_debug & DEBUG_CLASSIFY)
		    bu_log("nmg_classify_lu_lu returning NMG_CLASS_AonBshared\n");
		return NMG_CLASS_AonBshared;
	    } else {
		vect_t inward1, inward2;

		/* not all edges are shared,
		 * try to fnd a vertex in lu1
		 * that is not in lu2
		 */
		for (BU_LIST_FOR(eu, edgeuse, &lu1->down_hd)) {
		    struct vertex_g *vg;
		    int nmg_class;

		    NMG_CK_EDGEUSE(eu);

		    vg = eu->vu_p->v_p->vg_p;
		    NMG_CK_VERTEX_G(vg);

		    if (nmg_find_vertex_in_lu(eu->vu_p->v_p, lu2) != NULL)
			continue;

		    nmg_class = nmg_class_pt_lu_except(vg->coord, lu2, (struct edge *)NULL, tol);
		    if (nmg_class != NMG_CLASS_AonBshared && nmg_class != NMG_CLASS_AonBanti) {
			if (lu2->orientation == OT_SAME) {
			    if (RTG.NMG_debug & DEBUG_CLASSIFY)
				bu_log("nmg_classify_lu_lu returning %s\n", nmg_class_name(nmg_class));
			    return nmg_class;
			} else {
			    if (nmg_class == NMG_CLASS_AinB) {
				if (RTG.NMG_debug & DEBUG_CLASSIFY)
				    bu_log("nmg_classify_lu_lu returning NMG_CLASS_AoutB\n");
				return NMG_CLASS_AoutB;
			    }
			    if (nmg_class == NMG_CLASS_AoutB) {
				if (RTG.NMG_debug & DEBUG_CLASSIFY)
				    bu_log("nmg_classify_lu_lu returning NMG_CLASS_AinB\n");
				return NMG_CLASS_AinB;
			    }
			}
		    }
		}

		/* Some of lu1 edges are on lu2, but loops are not shared.
		 * Compare inward vectors of a shared edge.
		 */

		nmg_find_eu_leftvec(inward1, eu1_start);
		if (lu1->orientation == OT_OPPOSITE)
		    VREVERSE(inward1, inward1);

		nmg_find_eu_leftvec(inward2, eu2_start);
		if (lu2->orientation == OT_OPPOSITE)
		    VREVERSE(inward2, inward2);

		if (VDOT(inward1, inward2) < -SMALL_FASTF)
		    return NMG_CLASS_AoutB;
		else
		    return NMG_CLASS_AinB;
	    }
	} else {
	    /* no matching edges, classify by vertices */
	    for (BU_LIST_FOR(eu, edgeuse, &lu1->down_hd)) {
		struct vertex_g *vg;
		int nmg_class;

		NMG_CK_EDGEUSE(eu);

		vg = eu->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G(vg);

		if (nmg_find_vertex_in_lu(eu->vu_p->v_p, lu2) != NULL)
		    continue;

		nmg_class = nmg_class_pt_lu_except(vg->coord, lu2, (struct edge *)NULL, tol);
		if (nmg_class != NMG_CLASS_AonBshared && nmg_class != NMG_CLASS_AonBanti) {
		    if (lu2->orientation == OT_SAME) {
			if (RTG.NMG_debug & DEBUG_CLASSIFY)
			    bu_log("nmg_classify_lu_lu returning %s\n", nmg_class_name(nmg_class));
			return nmg_class;
		    } else {
			if (nmg_class == NMG_CLASS_AinB) {
			    if (RTG.NMG_debug & DEBUG_CLASSIFY)
				bu_log("nmg_classify_lu_lu returning NMG_CLASS_AoutB\n");
			    return NMG_CLASS_AoutB;
			}
			if (nmg_class == NMG_CLASS_AoutB) {
			    if (RTG.NMG_debug & DEBUG_CLASSIFY)
				bu_log("nmg_classify_lu_lu returning NMG_CLASS_AinB\n");
			    return NMG_CLASS_AinB;
			}
		    }
		}
	    }

	    /* if we get here, all vertices are shared,
	     * but no edges are!!!!! Check the midpoint of edges.
	     */
	    for (BU_LIST_FOR(eu, edgeuse, &lu1->down_hd)) {
		struct vertex_g *vg1, *vg2;
		int nmg_class;
		point_t mid_pt;

		vg1 = eu->vu_p->v_p->vg_p;
		vg2 = eu->eumate_p->vu_p->v_p->vg_p;

		VADD2(mid_pt, vg1->coord, vg2->coord);
		VSCALE(mid_pt, mid_pt, 0.5);

		nmg_class = nmg_class_pt_lu_except(mid_pt, lu2, (struct edge *)NULL, tol);
		if (nmg_class != NMG_CLASS_AonBshared && nmg_class != NMG_CLASS_AonBanti) {
		    if (lu2->orientation == OT_SAME) {
			if (RTG.NMG_debug & DEBUG_CLASSIFY)
			    bu_log("nmg_classify_lu_lu returning %s\n", nmg_class_name(nmg_class));
			return nmg_class;
		    } else {
			if (nmg_class == NMG_CLASS_AinB) {
			    if (RTG.NMG_debug & DEBUG_CLASSIFY)
				bu_log("nmg_classify_lu_lu returning NMG_CLASS_AoutB\n");
			    return NMG_CLASS_AoutB;
			}
			if (nmg_class == NMG_CLASS_AoutB) {
			    if (RTG.NMG_debug & DEBUG_CLASSIFY)
				bu_log("nmg_classify_lu_lu returning NMG_CLASS_AinB\n");
			    return NMG_CLASS_AinB;
			}
		    }
		}
	    }

	    /* Should never get here */
	    bu_log("nmg_classify_lu_lu: Cannot classify lu %p w.r.t. lu %p\n",
		   (void *)lu1, (void *)lu2);
	    return NMG_CLASS_Unknown;
	}
    } else if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC &&
	       BU_LIST_FIRST_MAGIC(&lu2->down_hd) == NMG_VERTEXUSE_MAGIC)
    {
	struct vertexuse *vu1, *vu2;

	vu1 = BU_LIST_FIRST(vertexuse, &lu1->down_hd);
	vu2 = BU_LIST_FIRST(vertexuse, &lu2->down_hd);

	if (vu1->v_p == vu2->v_p) {
	    if (RTG.NMG_debug & DEBUG_CLASSIFY)
		bu_log("nmg_classify_lu_lu returning NMG_CLASS_AonBshared\n");
	    return NMG_CLASS_AonBshared;
	} else {
	    if (RTG.NMG_debug & DEBUG_CLASSIFY)
		bu_log("nmg_classify_lu_lu returning NMG_CLASS_AoutB\n");
	    return NMG_CLASS_AoutB;
	}
    }

    if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC) {
	struct vertexuse *vu;
	struct vertex_g *vg;
	int nmg_class;

	vu = BU_LIST_FIRST(vertexuse, &lu1->down_hd);
	NMG_CK_VERTEXUSE(vu);
	vg = vu->v_p->vg_p;
	NMG_CK_VERTEX_G(vg);

	nmg_class = nmg_class_pt_lu_except(vg->coord, lu2,
					   (struct edge *)NULL, tol);

	if (lu2->orientation == OT_OPPOSITE) {
	    if (nmg_class == NMG_CLASS_AoutB)
		nmg_class = NMG_CLASS_AinB;
	    else if (nmg_class == NMG_CLASS_AinB)
		nmg_class = NMG_CLASS_AoutB;
	}
	if (RTG.NMG_debug & DEBUG_CLASSIFY)
	    bu_log("nmg_classify_lu_lu returning %s\n", nmg_class_name(nmg_class));
	return nmg_class;
    } else if (BU_LIST_FIRST_MAGIC(&lu2->down_hd) == NMG_VERTEXUSE_MAGIC)
	return NMG_CLASS_AoutB;

    bu_log("nmg_classify_lu_lu: ERROR, Should not get here!!!\n");
    bu_bomb("nmg_classify_lu_lu: ERROR, Should not get here!!!\n");

    return -1; /* to make the compilers happy */
}


/**
 * Classify one shell (s2) with respect to another (s).
 *
 * returns:
 * NMG_CLASS_AinB if s2 is inside s
 * NMG_CLASS_AoutB is s2 is outside s
 * NMG_CLASS_Unknown if we can't tell
 *
 * Assumes (but does not verify) that these two shells do not
 * overlap, but are either entirely separate or entirely within
 * one or the other.
 */
int
nmg_classify_s_vs_s(struct shell *s2, struct shell *s, const struct bn_tol *tol)
{
    int i;
    int nmg_class;
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    point_t pt_in_s2;
    struct bu_ptbl verts;

    if (!V3RPP1_IN_RPP2(s2->sa_p->min_pt, s2->sa_p->max_pt, s->sa_p->min_pt, s->sa_p->max_pt))
	return NMG_CLASS_AoutB;

    /* shell s2 may be inside shell s
       Get a point from s2 to classify vs s */

    if (BU_LIST_NON_EMPTY(&s2->fu_hd)) {
	fu = BU_LIST_FIRST(faceuse, &s2->fu_hd);
	lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	VMOVE(pt_in_s2, eu->vu_p->v_p->vg_p->coord);
	nmg_class = nmg_class_pt_s(pt_in_s2, s, 0, tol);
	if (nmg_class == NMG_CLASS_AinB)
	    return NMG_CLASS_AinB;		/* shell s2 is inside shell s */
	else if (nmg_class == NMG_CLASS_AoutB)
	    return NMG_CLASS_AoutB;		/* shell s2 is not inside shell s */

	/* try other end of this EU */
	VMOVE(pt_in_s2, eu->eumate_p->vu_p->v_p->vg_p->coord);
	nmg_class = nmg_class_pt_s(pt_in_s2, s, 0, tol);
	if (nmg_class == NMG_CLASS_AinB)
	    return NMG_CLASS_AinB;		/* shell s2 is inside shell s */
	else if (nmg_class == NMG_CLASS_AoutB)
	    return NMG_CLASS_AoutB;		/* shell s2 is not inside shell s */
    }

    if (BU_LIST_NON_EMPTY(&s2->lu_hd)) {
	lu = BU_LIST_FIRST(loopuse, &s2->lu_hd);
	eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	VMOVE(pt_in_s2, eu->vu_p->v_p->vg_p->coord);
	nmg_class = nmg_class_pt_s(pt_in_s2, s, 0, tol);
	if (nmg_class == NMG_CLASS_AinB)
	    return NMG_CLASS_AinB;		/* shell s2 is inside shell s */
	else if (nmg_class == NMG_CLASS_AoutB)
	    return NMG_CLASS_AoutB;		/* shell s2 is not inside shell s */

	/* try other end of this EU */
	VMOVE(pt_in_s2, eu->eumate_p->vu_p->v_p->vg_p->coord);
	nmg_class = nmg_class_pt_s(pt_in_s2, s, 0, tol);
	if (nmg_class == NMG_CLASS_AinB)
	    return NMG_CLASS_AinB;		/* shell s2 is inside shell s */
	else if (nmg_class == NMG_CLASS_AoutB)
	    return NMG_CLASS_AoutB;		/* shell s2 is not inside shell s */
    }

    if (BU_LIST_NON_EMPTY(&s2->eu_hd)) {
	eu = BU_LIST_FIRST(edgeuse, &s2->eu_hd);
	VMOVE(pt_in_s2, eu->vu_p->v_p->vg_p->coord);
	nmg_class = nmg_class_pt_s(pt_in_s2, s, 0, tol);
	if (nmg_class == NMG_CLASS_AinB)
	    return NMG_CLASS_AinB;		/* shell s2 is inside shell s */
	else if (nmg_class == NMG_CLASS_AoutB)
	    return NMG_CLASS_AoutB;		/* shell s2 is not inside shell s */

	/* try other end of this EU */
	VMOVE(pt_in_s2, eu->eumate_p->vu_p->v_p->vg_p->coord);
	nmg_class = nmg_class_pt_s(pt_in_s2, s, 0, tol);
	if (nmg_class == NMG_CLASS_AinB)
	    return NMG_CLASS_AinB;		/* shell s2 is inside shell s */
	else if (nmg_class == NMG_CLASS_AoutB)
	    return NMG_CLASS_AoutB;		/* shell s2 is not inside shell s */
    }

    if (s2->vu_p && s2->vu_p->v_p->vg_p) {
	VMOVE(pt_in_s2, s2->vu_p->v_p->vg_p->coord);
	nmg_class = nmg_class_pt_s(pt_in_s2, s, 0, tol);
	if (nmg_class == NMG_CLASS_AinB)
	    return NMG_CLASS_AinB;		/* shell s2 is inside shell s */
	else if (nmg_class == NMG_CLASS_AoutB)
	    return NMG_CLASS_AoutB;		/* shell s2 is not inside shell s */
    }

    /* classification returned NMG_CLASS_AonB, so need to try other points */
    nmg_vertex_tabulate(&verts, &s2->l.magic);
    for (i = 0; i < BU_PTBL_END(&verts); i++) {
	struct vertex *v;

	v = (struct vertex *)BU_PTBL_GET(&verts, i);

	VMOVE(pt_in_s2, v->vg_p->coord);
	nmg_class = nmg_class_pt_s(pt_in_s2, s, 0, tol);
	if (nmg_class == NMG_CLASS_AinB) {
	    bu_ptbl_free(&verts);
	    return NMG_CLASS_AinB;	/* shell s2 is inside shell s */
	} else if (nmg_class == NMG_CLASS_AoutB) {
	    bu_ptbl_free(&verts);
	    return NMG_CLASS_AoutB;	/* shell s2 is not inside shell s */
	}
    }
    bu_ptbl_free(&verts);

    /* every point of s2 is on s !!!!!!! */
    return NMG_CLASS_Unknown;
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
