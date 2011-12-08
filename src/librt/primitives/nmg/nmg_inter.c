/*                     N M G _ I N T E R . C
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
/** @file primitives/nmg/nmg_inter.c
 *
 * Routines to intersect two NMG regions.  When complete, all loops in
 * each region have a single classification w.r.t. the other region,
 * i.e. all geometric intersections of the two regions have explicit
 * topological representations.
 *
 * The intersector makes sure that all geometric intersections gets
 * recorded with explicit geometry and topology that is shared between
 * both regions. Primary examples of this are (a) the line of
 * intersection between two planes (faces), and (b) the point of
 * intersection where two edges cross.
 *
 * Entities of one region that are INSIDE, but not ON the other region
 * do not become shared during the intersection process.
 *
 * All point -vs- point comparisons should be done in 3D, for
 * consistency.
 *
 * Method -
 *
 * Find all the points of intersection between the two regions, and
 * insert vertices at those points, breaking edges on those new
 * vertices as appropriate.
 *
 * Call the face cutter to construct and delete edges and loops along
 * the line of intersection, as appropriate.
 *
 * There are no "user interface" routines in here.
 *
 */
/** @} */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "plot3.h"


#define ISECT_NONE 0
#define ISECT_SHARED_V 1
#define ISECT_SPLIT1 2
#define ISECT_SPLIT2 4

struct ee_2d_state {
    struct nmg_inter_struct *is;
    struct edgeuse *eu;
    point_t start;
    point_t end;
    vect_t dir;
};


static int nmg_isect_edge2p_face2p(struct nmg_inter_struct *is,
				   struct edgeuse *eu, struct faceuse *fu,
				   struct faceuse *eu_fu);


static struct nmg_inter_struct *nmg_hack_last_is;	/* see nmg_isect2d_final_cleanup() */

/*
 */
struct vertexuse *
nmg_make_dualvu(struct vertex *v, struct faceuse *fu, const struct bn_tol *tol)
{
    struct loopuse *lu;
    struct vertexuse *dualvu;
    struct edgeuse *new_eu;

    NMG_CK_VERTEX(v);
    NMG_CK_FACEUSE(fu);
    BN_CK_TOL(tol);

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_make_dualvu(v=x%x, fu=x%x)\n", v, fu);

    /* check for existing vu */
    dualvu=nmg_find_v_in_face(v, fu);
    if (dualvu) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tdualvu already exists (x%x)\n", dualvu);
	return dualvu;
    }

    new_eu = (struct edgeuse *)NULL;

    /* check if v lies within tolerance of an edge in face */
    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("\tLooking for an edge to split\n");
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	struct edgeuse *eu;

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    int code;
	    fastf_t dist;
	    point_t pca;

	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\tChecking eu x%x (%f %f %f) <-> (%f %f %f)\n",
		       eu,
		       V3ARGS(eu->vu_p->v_p->vg_p->coord),
		       V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord));

	    code = bn_dist_pt3_lseg3(&dist, pca,
				     eu->vu_p->v_p->vg_p->coord,
				     eu->eumate_p->vu_p->v_p->vg_p->coord,
				     v->vg_p->coord, tol);

	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("bn_dist_pt3_lseg3 returns %d, dist=%f\n", code, dist);

	    if (code > 2)
		continue;

	    /* v is within tolerance of eu */
	    if (code > 0)
		continue;

	    /* split edge */
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("nmg_make_dualvu is splitting eu x%x at v x%x\n", eu, v);
	    new_eu = nmg_esplit(v, eu, 1);
	}
    }

    if (new_eu)
	return new_eu->vu_p;

    /* need a self loop */
    lu = nmg_mlv(&fu->l.magic, v, OT_BOOLPLACE);
    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_make_dualvu is makeing a self_loop (lu=x%x, vu=x%x) for v=x%x\n", lu, BU_LIST_FIRST(vertexuse, &lu->down_hd), v);
    nmg_loop_g(lu->l_p, tol);
    return BU_LIST_FIRST(vertexuse, &lu->down_hd);
}


/**
 * N M G _ E N L I S T _ V U
 *
 * Given a vu which represents a point of intersection between shells
 * s1 and s2, insert it and its dual into lists l1 and l2.
 * First, determine whether the vu came from s1 or s2, and insert in
 * the corresponding list.
 *
 * Second, try and find a dual of that vertex
 * in the other shell's faceuse (fu1 or fu2)
 * (if the entity in the other shell is not a wire), and enlist the dual.
 * If there is no dual, make a self-loop over there, and enlist that.
 *
 * If 'dualvu' is provided, don't search, just use that.
 *
 * While it is true that in most cases the calling routine will know
 * which shell the vu came from, it's cheap to re-determine it here.
 * This "all in one" packaging, which handles both lists automaticly
 * is *vastly* superior to the previous version, which pushed 10-20
 * lines of bookkeeping up into *every* place an intersection vu was
 * created.
 *
 * Returns a pointer to vu's dual.
 *
 * "Join the Army, young vertexuse".
 */
struct vertexuse *
nmg_enlist_vu(struct nmg_inter_struct *is, const struct vertexuse *vu, struct vertexuse *dualvu, fastf_t dist)


/* vu's dual in other shell.  May be NULL */
/* distance along intersect ray for this vu */
{
    struct shell *sv;		/* shell of vu */
    struct loopuse *lu;		/* lu of new self-loop */
    struct faceuse *dualfu = (struct faceuse *)NULL; /* faceuse of vu's dual */
    struct shell *duals = (struct shell *)NULL;	/* shell of vu's dual */
    struct faceuse *fuv;		/* faceuse of vu */

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_VERTEXUSE(vu);
    if (dualvu) {
	NMG_CK_VERTEXUSE(dualvu);
	if (vu == dualvu) bu_bomb("nmg_enlist_vu() vu == dualvu\n");
    }

    if (is->mag_len <= BU_PTBL_END(is->l1) || is->mag_len <= BU_PTBL_END(is->l2))
	bu_log("Array for distances to vertexuses is too small (%d)\n", is->mag_len);

    sv = nmg_find_s_of_vu(vu);
    fuv = nmg_find_fu_of_vu(vu);

    /* First step:  add vu to corresponding list */
    if (sv == is->s1) {
	bu_ptbl_ins_unique(is->l1, (long *)&vu->l.magic);
	if (is->mag_len <= BU_PTBL_END(is->l1)) {
	    if (is->mag_len) {
		is->mag_len *= 2;
		is->mag1 = (fastf_t *)bu_realloc((char *)is->mag1, is->mag_len*sizeof(fastf_t),
						 "is->mag1");
		is->mag2 = (fastf_t *)bu_realloc((char *)is->mag2, is->mag_len*sizeof(fastf_t),
						 "is->mag2");
	    } else {
		is->mag_len = 2*(BU_PTBL_END(is->l1) + BU_PTBL_END(is->l2));
		is->mag1 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "is->mag1");
		is->mag2 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "is->mag2");
	    }

	}
	if (dist < MAX_FASTF)
	    is->mag1[bu_ptbl_locate(is->l1, (long *)&vu->l.magic)] = dist;
	duals = is->s2;		/* other shell */
	dualfu = is->fu2;
	if (is->fu1 && is->fu1->s_p != is->s1) bu_bomb("nmg_enlist_vu() fu1/s1 mismatch\n");
	if (fuv != is->fu1) {
	    bu_log("fuv=x%x, fu1=x%x, fu2=x%x\n", fuv, is->fu1, is->fu2);
	    bu_log("\tvu=x%x (x%x)\n", vu, vu->v_p);
	    bu_bomb("nmg_enlist_vu() vu/fu1 mis-match\n");
	}
    } else if (sv == is->s2) {
	bu_ptbl_ins_unique(is->l2, (long *)&vu->l.magic);
	if (is->mag_len <= BU_PTBL_END(is->l2)) {
	    if (is->mag_len) {
		is->mag_len *= 2;
		is->mag1 = (fastf_t *)bu_realloc((char *)is->mag1, is->mag_len*sizeof(fastf_t),
						 "is->mag1");
		is->mag2 = (fastf_t *)bu_realloc((char *)is->mag2, is->mag_len*sizeof(fastf_t),
						 "is->mag2");
	    } else {
		is->mag_len = 2*(BU_PTBL_END(is->l1) + BU_PTBL_END(is->l2));
		is->mag1 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "is->mag1");
		is->mag2 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "is->mag2");
	    }

	}
	if (dist < MAX_FASTF)
	    is->mag2[bu_ptbl_locate(is->l2, (long *)&vu->l.magic)] = dist;
	duals = is->s1;		/* other shell */
	dualfu = is->fu1;
	if (is->fu2 && is->fu2->s_p != is->s2) bu_bomb("nmg_enlist_vu() fu2/s2 mismatch\n");
	if (fuv != is->fu2) {
	    bu_log("fuv=x%x, fu1=x%x, fu2=x%x\n", fuv, is->fu1, is->fu2);
	    bu_log("\tvu=x%x (x%x)\n", vu, vu->v_p);
	    bu_bomb("nmg_enlist_vu() vu/fu2 mis-match\n");
	}
    } else {
	bu_log("nmg_enlist_vu(vu=x%x, dv=x%x) sv=x%x, s1=x%x, s2=x%x\n",
	       vu, dualvu, sv, is->s1, is->s2);
	bu_bomb("nmg_enlist_vu: vu is not in s1 or s2\n");
    }

    if (dualvu) {
	if (vu->v_p != dualvu->v_p) bu_bomb("nmg_enlist_vu() dual vu has different vertex\n");
	if (nmg_find_s_of_vu(dualvu) != duals) {
	    bu_log("nmg_enlist_vu(vu=x%x, dv=x%x) sv=x%x, s1=x%x, s2=x%x, sdual=x%x\n",
		   vu, dualvu,
		   sv, is->s1, is->s2, nmg_find_s_of_vu(dualvu));
	    bu_bomb("nmg_enlist_vu() dual vu shell mis-match\n");
	}
	if (dualfu && nmg_find_fu_of_vu(dualvu) != dualfu) bu_bomb("nmg_enlist_vu() dual vu has wrong fu\n");
    }

    /* Second, search for vu's dual */
    if (dualfu) {
	NMG_CK_FACEUSE(dualfu);
	if (dualfu->s_p != duals) bu_bomb("nmg_enlist_vu() dual fu's shell is not dual's shell?\n");
	if (!dualvu)
	    dualvu = nmg_make_dualvu(vu->v_p, dualfu, &(is->tol));
	else {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		bu_log("nmg_enlist_vu(vu=x%x, dv=x%x) re-using dualvu=x%x from dualfu=x%x\n",
		       vu, dualvu,
		       dualvu, dualfu);
	    }
	}
    } else {
	/* Must have come from a wire in other shell, make wire loop */
	bu_log("\tvu=x%x, %s, fu1=x%x, fu2=x%x\n", vu, (sv==is->s1)?"shell 1":"shell 2", is->fu1, is->fu2);
	bu_log("nmg_enlist_vu(): QUESTION: What do I search for wire intersections?  Making self-loop\n");
	if (!dualvu) {
	    dualvu = nmg_find_v_in_shell(vu->v_p, duals, 0);
	    if (!dualvu) {
		/* Not found, make self-loop in dual shell */
		lu = nmg_mlv(&duals->l.magic, vu->v_p, OT_BOOLPLACE);
		nmg_loop_g(lu->l_p, &(is->tol));
		dualvu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	    }
	}
    }
    NMG_CK_VERTEXUSE(dualvu);

    /* Enlist the dual onto the other list */
    if (sv == is->s1) {
	bu_ptbl_ins_unique(is->l2, (long *)&dualvu->l.magic);
	if (is->mag_len <= BU_PTBL_END(is->l2)) {
	    if (is->mag_len) {
		is->mag_len *= 2;
		is->mag1 = (fastf_t *)bu_realloc((char *)is->mag1, is->mag_len*sizeof(fastf_t),
						 "is->mag1");
		is->mag2 = (fastf_t *)bu_realloc((char *)is->mag2, is->mag_len*sizeof(fastf_t),
						 "is->mag2");
	    } else {
		is->mag_len = 2*(BU_PTBL_END(is->l1) + BU_PTBL_END(is->l2));
		is->mag1 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "is->mag1");
		is->mag2 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "is->mag2");
	    }

	}
	if (dist < MAX_FASTF)
	    is->mag2[bu_ptbl_locate(is->l2, (long *)&dualvu->l.magic)] = dist;
    } else {
	bu_ptbl_ins_unique(is->l1, (long *)&dualvu->l.magic);
	if (is->mag_len <= BU_PTBL_END(is->l1)) {
	    if (is->mag_len) {
		is->mag_len *= 2;
		is->mag1 = (fastf_t *)bu_realloc((char *)is->mag1, is->mag_len*sizeof(fastf_t),
						 "is->mag1");
		is->mag2 = (fastf_t *)bu_realloc((char *)is->mag2, is->mag_len*sizeof(fastf_t),
						 "is->mag2");
	    } else {
		is->mag_len = 2*(BU_PTBL_END(is->l1) + BU_PTBL_END(is->l2));
		is->mag1 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "is->mag1");
		is->mag2 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "is->mag2");
	    }

	}
	if (dist < MAX_FASTF)
	    is->mag1[bu_ptbl_locate(is->l1, (long *)&dualvu->l.magic)] = dist;
    }

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_enlist_vu(vu=x%x, dv=x%x) v=x%x, dist=%g (%s) ret=x%x\n",
	       vu, dualvu, vu->v_p, dist,
	       (sv == is->s1) ? "shell 1" : "shell 2",
	       dualvu);
    }

    /* Some (expensive) centralized sanity checking */
    if ((rt_g.NMG_debug & DEBUG_VERIFY) && is->fu1 && is->fu2) {
	nmg_ck_v_in_2fus(vu->v_p, is->fu1, is->fu2, &(is->tol));
    }
    return dualvu;
}


/**
 * N M G _ G E T _ 2 D _ V E R T E X
 *
 * A "lazy evaluator" to obtain the 2D projection of a vertex.
 * The lazy approach is not a luxury, since new (3D) vertices are created
 * as the edge/edge intersection proceeds, and their 2D coordinates may
 * be needed later on in the calculation.
 * The alternative would be to store the 2D projection each time a
 * new vertex is created, but that is likely to be a lot of bothersome
 * code, where one omission would be deadly.
 *
 * The return is a 3-tuple, with the Z coordinate set to 0.0 for safety.
 * This is especially useful when the projected value is printed using
 * one of the 3D print routines.
 *
 * 'assoc_use' is either a pointer to a faceuse, or an edgeuse.
 */
static void
nmg_get_2d_vertex(fastf_t *v2d, struct vertex *v, struct nmg_inter_struct *is, const uint32_t *assoc_use)
/* a 3-tuple */


/* ptr to faceuse/edgeuse associated w/2d projection */
{
    register fastf_t *pt2d;
    point_t pt;
    struct vertex_g *vg;
    uint32_t *this;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_VERTEX(v);

    /* If 2D preparations have not been made yet, do it now */
    if (!is->vert2d) {
	nmg_isect2d_prep(is, assoc_use);
    }

    if (*assoc_use == NMG_FACEUSE_MAGIC) {
	this = &((struct faceuse *)assoc_use)->f_p->l.magic;
	if (this != is->twod)
	    goto bad;
    } else if (*assoc_use == NMG_EDGEUSE_MAGIC) {
	this = &((struct edgeuse *)assoc_use)->e_p->magic;
	if (this != is->twod)
	    goto bad;
    } else {
	this = NULL;
    bad:
	bu_log("nmg_get_2d_vertex(, assoc_use=%x %s) this=x%x %s, is->twod=%x %s\n",
	       assoc_use, bu_identify_magic(*assoc_use),
	       this, bu_identify_magic(*this),
	       (unsigned long *)is->twod, bu_identify_magic(*(is->twod)));
	bu_bomb("nmg_get_2d_vertex:  2d association mis-match\n");
    }

    if (!v->vg_p) {
	bu_log("nmg_get_2d_vertex: v=x%x, assoc_use=x%x, null vg_p\n",
	       v, assoc_use);
	bu_bomb("nmg_get_2d_vertex:  vertex with no geometry!\n");
    }
    vg = v->vg_p;
    NMG_CK_VERTEX_G(vg);
    if (v->index >= is->maxindex) {
	struct model *m;
	int oldmax;
	register int i;

	oldmax = is->maxindex;
	m = nmg_find_model(&v->magic);
	NMG_CK_MODEL(m);
	bu_log("nmg_get_2d_vertex:  v=x%x, v->index=%d, is->maxindex=%d, m->maxindex=%d\n",
	       v, v->index, is->maxindex, m->maxindex);
	if (v->index >= m->maxindex) {
	    /* Really off the end */
	    VPRINT("3d vertex", vg->coord);
	    bu_bomb("nmg_get_2d_vertex:  array overrun\n");
	}
	/* Need to extend array, it's grown. */
	is->maxindex = m->maxindex * 4;
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("nmg_get_2d_vertex() extending vert2d array from %d to %d points (m max=%d)\n",
		   oldmax, is->maxindex, m->maxindex);
	}
	is->vert2d = (fastf_t *)bu_realloc((char *)is->vert2d,
					   is->maxindex * 3 * sizeof(fastf_t), "vert2d[]");

	/* Clear out the new part of the 2D vertex array, setting flag in [2] to -1 */
	for (i = (3*is->maxindex)-1-2; i >= oldmax*3; i -= 3) {
	    VSET(&is->vert2d[i], 0, 0, -1);
	}
    }
    pt2d = &is->vert2d[v->index*3];
    if (ZERO(pt2d[2])) {
	/* Flag set.  Conversion is done.  Been here before */
	v2d[0] = pt2d[0];
	v2d[1] = pt2d[1];
	v2d[2] = 0;
	return;
    }

    MAT4X3PNT(pt, is->proj, vg->coord);
    v2d[0] = pt2d[0] = pt[0];
    v2d[1] = pt2d[1] = pt[1];
    v2d[2] = pt2d[2] = 0;		/* flag */

    if (!NEAR_ZERO(pt[2], is->tol.dist)) {
	struct faceuse *fu = (struct faceuse *)assoc_use;
	plane_t n;
	fastf_t dist;
	NMG_GET_FU_PLANE(n, fu);
	dist = DIST_PT_PLANE(vg->coord, n);
	bu_log("nmg_get_2d_vertex ERROR #%d (%g %g %g) becomes (%g, %g)\n\t%g != zero, dist3d=%g, %g*tol\n",
	       v->index, V3ARGS(vg->coord), V3ARGS(pt),
	       dist, dist/is->tol.dist);
	if (!NEAR_ZERO(dist, is->tol.dist) &&
	    !NEAR_ZERO(pt[2], 10*is->tol.dist)) {
	    bu_log("nmg_get_2d_vertex(, assoc_use=%x) f=x%x, is->twod=%x\n",
		   assoc_use, fu->f_p, (unsigned long *)is->twod);
	    PLPRINT("fu->f_p N", n);
	    bu_bomb("3D->2D point projection error\n");
	}
    }

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("2d #%d (%g %g %g) becomes (%g, %g) %g\n",
	       v->index, V3ARGS(vg->coord), V3ARGS(pt));
    }
}


/**
 * N M G _ I S E C T 2 D _ P R E P
 *
 * To intersect two co-planar faces, project all vertices from those
 * faces into 2D.
 * At the moment, use a memory intensive strategy which allocates a
 * (3d) point_t for each "index" item, and subscripts the resulting
 * array by the vertices index number.
 * Since additional vertices can be created as the intersection process
 * operates, 2*maxindex items are originall allocated, as a (generous)
 * upper bound on the amount of intersecting that might happen.
 *
 * In the array, the third double of each projected vertex is set to -1 when
 * that slot has not been filled yet, and 0 when it has been.
 */
/* XXX Set this up so that it can take either an edge pointer
 * or a face pointer.  In case of edge, make edge_g_lseg->dir unit, and
 * rotate that to +X axis.  Make edge_g_lseg->pt be the origin.
 * This will allow the 2D routines to operate on wires.
 */
void
nmg_isect2d_prep(struct nmg_inter_struct *is, const uint32_t *assoc_use)
{
    struct model *m;
    struct face_g_plane *fg;
    vect_t to;
    point_t centroid;
    point_t centroid_proj;
    plane_t n;
    register int i;

    NMG_CK_INTER_STRUCT(is);

    if (*assoc_use == NMG_FACEUSE_MAGIC) {
	if (&((struct faceuse *)assoc_use)->f_p->l.magic == is->twod)
	    return;		/* Already prepped */
    } else if (*assoc_use == NMG_EDGEUSE_MAGIC) {
	if (&((struct edgeuse *)assoc_use)->e_p->magic == is->twod)
	    return;		/* Already prepped */
    } else {
	bu_bomb("nmg_isect2d_prep() bad assoc_use magic\n");
    }

    nmg_isect2d_cleanup(is);
    nmg_hack_last_is = is;

    m = nmg_find_model(assoc_use);

    is->maxindex = (2 * m->maxindex);
    is->vert2d = (fastf_t *)bu_malloc(is->maxindex * 3 * sizeof(fastf_t), "vert2d[]");

    if (*assoc_use == NMG_FACEUSE_MAGIC) {
	struct faceuse *fu1 = (struct faceuse *)assoc_use;
	struct face *f1;

	f1 = fu1->f_p;
	fg = f1->g.plane_p;
	NMG_CK_FACE_G_PLANE(fg);
	is->twod = &f1->l.magic;
	if (f1->flip) {
	    VREVERSE(n, fg->N);
	    n[W] = -fg->N[W];
	} else {
	    HMOVE(n, fg->N);
	}
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("nmg_isect2d_prep(f=x%x) flip=%d\n", f1, f1->flip);
	    PLPRINT("N", n);
	}

	/*
	 * Rotate so that f1's N vector points up +Z.
	 * This places all 2D calcuations in the XY plane.
	 * Translate so that f1's centroid becomes the 2D origin.
	 * Reasoning:  no vertex should be favored by putting it at
	 * the origin.  The "desirable" floating point space in the
	 * vicinity of the origin should be used to best advantage,
	 * by centering calculations around it.
	 */
	VSET(to, 0, 0, 1);
	bn_mat_fromto(is->proj, n, to);
	VADD2SCALE(centroid, f1->max_pt, f1->min_pt, 0.5);
	MAT4X3PNT(centroid_proj, is->proj, centroid);
	centroid_proj[Z] = n[W];	/* pull dist from origin off newZ */
	MAT_DELTAS_VEC_NEG(is->proj, centroid_proj);
    } else if (*assoc_use == NMG_EDGEUSE_MAGIC) {
	struct edgeuse *eu1 = (struct edgeuse *)assoc_use;
	struct edge *e1;
	struct edge_g_lseg *eg;

	bu_log("2d prep for edgeuse\n");
	e1 = eu1->e_p;
	NMG_CK_EDGE(e1);
	eg = eu1->g.lseg_p;
	NMG_CK_EDGE_G_LSEG(eg);
	is->twod = &e1->magic;

	/*
	 * Rotate so that eg's eg_dir vector points up +X.
	 * The choice of the other axes is arbitrary.
	 * This ensures that all calculations happen on the XY plane.
	 * Translate the edge start point to the origin.
	 */
	VSET(to, 1, 0, 0);
	bn_mat_fromto(is->proj, eg->e_dir, to);
	MAT_DELTAS_VEC_NEG(is->proj, eg->e_pt);
    } else {
	bu_bomb("nmg_isect2d_prep() bad assoc_use magic\n");
    }

    /* Clear out the 2D vertex array, setting flag in [2] to -1 */
    for (i = (3*is->maxindex)-1-2; i >= 0; i -= 3) {
	VSET(&is->vert2d[i], 0, 0, -1);
    }
}


/**
 * N M G _ I S E C T 2 D _ C L E A N U P.
 *
 * Common routine to zap 2d vertex cache, and release dynamic storage.
 */
void
nmg_isect2d_cleanup(struct nmg_inter_struct *is)
{
    NMG_CK_INTER_STRUCT(is);

    nmg_hack_last_is = (struct nmg_inter_struct *)NULL;

    if (!is->vert2d) return;
    bu_free((char *)is->vert2d, "vert2d");
    is->vert2d = NULL;
    is->twod = NULL;
}


/**
 * N M G _ I S E C T 2 D _ F I N A L _ C L E A N U P
 *
 * XXX Hack routine used for storage reclamation by G-JACK for
 * XXX calculation of the reportcard without gobbling lots of memory
 * XXX on bu_bomb() longjmp()s.
 * Can be called by the longjmp handler with impunity.
 * If a pointer to busy dynamic memory is still handy, it will be freed.
 * If not, no harm done.
 */
void
nmg_isect2d_final_cleanup(void)
{
    if (nmg_hack_last_is && nmg_hack_last_is->magic == NMG_INTER_STRUCT_MAGIC)
	nmg_isect2d_cleanup(nmg_hack_last_is);
}


/**
 * N M G _ I S E C T _ V E R T 2 P _ F A C E 2 P
 *
 * Handle the complete intersection of a vertex which lies on the
 * plane of a face.  *every* intersection is performed.
 *
 * If already part of the topology of the face, do nothing more.
 * If it intersects one of the edges of the face, break the edge there.
 * Otherwise, add a self-loop into the face as a marker.
 *
 * All vertexuse pairs are enlisted on the intersection line.
 * Assuming that there is one (is->l1 non null).
 *
 * Called by -
 * nmg_isect_3vertex_3face()
 * nmg_isect_two_face2p()
 */
void
nmg_isect_vert2p_face2p(struct nmg_inter_struct *is, struct vertexuse *vu1, struct faceuse *fu2)
{
    struct vertexuse *vu2;
    struct loopuse *lu2;
    pointp_t pt;
    int ret = 0;

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_vert2p_face2p(, vu1=x%x, fu2=x%x)\n", vu1, fu2);
    NMG_CK_INTER_STRUCT(is);
    NMG_CK_VERTEXUSE(vu1);
    NMG_CK_FACEUSE(fu2);

    pt = vu1->v_p->vg_p->coord;

    /* Prep the 2D cache, if the face changed */
    nmg_isect2d_prep(is, &fu2->l.magic);

    /* For every edge and vert, check topo AND geometric intersection */
    for (BU_LIST_FOR(lu2, loopuse, &fu2->lu_hd)) {
	struct edgeuse *eu2;

	NMG_CK_LOOPUSE(lu2);
	if (BU_LIST_FIRST_MAGIC(&lu2->down_hd) == NMG_VERTEXUSE_MAGIC) {
	    vu2 = BU_LIST_FIRST(vertexuse, &lu2->down_hd);
	    if (vu1->v_p == vu2->v_p) {

		if (is->l1) nmg_enlist_vu(is, vu1, vu2, MAX_FASTF);
		ret++;
		continue;
	    }
	    /* Use 3D comparisons for uniformity */
	    if (bn_pt3_pt3_equal(pt, vu2->v_p->vg_p->coord, &is->tol)) {
		/* Fuse the two verts together */
		nmg_jv(vu1->v_p, vu2->v_p);
		if (is->l1) nmg_enlist_vu(is, vu1, vu2, MAX_FASTF);
		ret++;
		continue;
	    }
	    continue;
	}
	for (BU_LIST_FOR(eu2, edgeuse, &lu2->down_hd)) {
	    struct edgeuse *new_eu;

	    if (eu2->vu_p->v_p == vu1->v_p) {
		if (is->l1) nmg_enlist_vu(is, vu1, eu2->vu_p, MAX_FASTF);
		ret++;
		continue;
	    }

	    new_eu = nmg_break_eu_on_v(eu2, vu1->v_p, fu2, is);
	    if (new_eu) {
		if (is->l1) nmg_enlist_vu(is, vu1, new_eu->vu_p, MAX_FASTF);
		ret++;
		continue;
	    }
	}
    }

    if (ret == 0) {
	/* The vertex lies in the face, but touches nothing.  Place marker */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    VPRINT("Making vertexloop", pt);

	lu2 = nmg_mlv(&fu2->l.magic, vu1->v_p, OT_BOOLPLACE);
	nmg_loop_g(lu2->l_p, &is->tol);
	vu2 = BU_LIST_FIRST(vertexuse, &lu2->down_hd);
	if (is->l1) nmg_enlist_vu(is, vu1, vu2, MAX_FASTF);
    }
}


/**
 * N M G _ I S E C T _ 3 V E R T E X _ 3 F A C E
 *
 * intersect a vertex with a face (primarily for intersecting
 * loops of a single vertex with a face).
 *
 * XXX It would be useful to have one of the new vu's in fu returned
 * XXX as a flag, so that nmg_find_v_in_face() wouldn't have to be called
 * XXX to re-determine what was just done.
 */
static void
nmg_isect_3vertex_3face(struct nmg_inter_struct *is, struct vertexuse *vu, struct faceuse *fu)
{
    struct vertexuse *vup;
    pointp_t pt;
    fastf_t dist;
    plane_t n;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_VERTEXUSE(vu);
    NMG_CK_VERTEX(vu->v_p);
    NMG_CK_FACEUSE(fu);

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_3vertex_3face(, vu=x%x, fu=x%x) v=x%x\n", vu, fu, vu->v_p);

    /* check the topology first */
    vup=nmg_find_v_in_face(vu->v_p, fu);
    if (vup) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT) bu_log("\tvu lies in face (topology 1)\n");
	(void)bu_ptbl_ins_unique(is->l1, (long *)&vu->l.magic);
	(void)bu_ptbl_ins_unique(is->l2, (long *)&vup->l.magic);
	return;
    }


    /* since the topology didn't tell us anything, we need to check with
     * the geometry
     */
    pt = vu->v_p->vg_p->coord;
    NMG_GET_FU_PLANE(n, fu);
    dist = DIST_PT_PLANE(pt, n);

    if (!NEAR_ZERO(dist, is->tol.dist)) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT) bu_log("\tvu not on face (geometry)\n");
	return;
    }

    /*
     * The point lies on the plane of the face, by geometry.
     * This is now a 2-D problem.
     */
    (void)nmg_isect_vert2p_face2p(is, vu, fu);
}


/**
 * N M G _ B R E A K _ 3 E D G E _ A T _ P L A N E
 *
 * Having decided that an edge(use) crosses a plane of intersection,
 * stick a vertex at the point of intersection along the edge.
 *
 * vu1_final in fu1 is BU_LIST_PNEXT_CIRC(edgeuse, eu1)->vu_p after return.
 * vu2_final is the returned value, and is in fu2.
 *
 */
static struct vertexuse *
nmg_break_3edge_at_plane(const fastf_t *hit_pt, struct faceuse *fu2, struct nmg_inter_struct *is, struct edgeuse *eu1)

/* The face that eu intersects */

/* Edge to be broken (in fu1) */
{
    struct vertexuse *vu1_final;
    struct vertexuse *vu2_final;	/* hit_pt's vu in fu2 */
    struct vertex *v2;
    struct loopuse *plu2;		/* "point" loopuse */
    struct edgeuse *eu1forw;	/* New eu, after break, forw of eu1 */
    struct vertex *v1;
    struct vertex *v1mate;
    fastf_t dist;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_EDGEUSE(eu1);

    v1 = eu1->vu_p->v_p;
    NMG_CK_VERTEX(v1);
    v1mate = eu1->eumate_p->vu_p->v_p;
    NMG_CK_VERTEX(v1mate);

    /* Intersection is between first and second vertex points.
     * Insert new vertex at intersection point.
     */
    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_break_3edge_at_plane() Splitting %g, %g, %g <-> %g, %g, %g\n",
	       V3ARGS(v1->vg_p->coord),
	       V3ARGS(v1mate->vg_p->coord));
	VPRINT("\tAt point of intersection", hit_pt);
    }

    /* Double check for bad behavior */
    if (bn_pt3_pt3_equal(hit_pt, v1->vg_p->coord, &is->tol))
	bu_bomb("nmg_break_3edge_at_plane() hit_pt equal to v1\n");
    if (bn_pt3_pt3_equal(hit_pt, v1mate->vg_p->coord, &is->tol))
	bu_bomb("nmg_break_3edge_at_plane() hit_pt equal to v1mate\n");

    {
	vect_t va, vb;
	VSUB2(va, hit_pt, eu1->vu_p->v_p->vg_p->coord);
	VSUB2(vb, eu1->eumate_p->vu_p->v_p->vg_p->coord, hit_pt);
	VUNITIZE(va);
	VUNITIZE(vb);
	if (VDOT(va, vb) <= 0.7071) {
	    bu_bomb("nmg_break_3edge_at_plane() eu1 changes direction?\n");
	}
    }
    {
	struct bn_tol t2;
	t2 = is->tol;	/* Struct copy */

	t2.dist = is->tol.dist * 4;
	t2.dist_sq = t2.dist * t2.dist;
	dist = DIST_PT_PT(hit_pt, v1->vg_p->coord);
	if (bn_pt3_pt3_equal(hit_pt, v1->vg_p->coord, &t2))
	    bu_log("NOTICE: nmg_break_3edge_at_plane() hit_pt nearly equal to v1 %g*tol\n", dist/is->tol.dist);
	dist = DIST_PT_PT(hit_pt, v1mate->vg_p->coord);
	if (bn_pt3_pt3_equal(hit_pt, v1mate->vg_p->coord, &t2))
	    bu_log("NOTICE: nmg_break_3edge_at_plane() hit_pt nearly equal to v1mate %g*tol\n", dist/is->tol.dist);
    }

    /* if we can't find the appropriate vertex in the
     * other face by a geometry search, build a new vertex.
     * Otherwise, re-use the existing one.
     * Can't just search other face, might miss relevant vert.
     */
    v2 = nmg_find_pt_in_model(fu2->s_p->r_p->m_p, hit_pt, &(is->tol));
    if (v2) {
	/* the other face has a convenient vertex for us */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("re-using vertex v=x%x from other shell\n", v2);

	eu1forw = nmg_ebreaker(v2, eu1, &(is->tol));
	vu1_final = eu1forw->vu_p;
	vu2_final = nmg_enlist_vu(is, vu1_final, 0, MAX_FASTF);
    } else {
	/* The other face has no vertex in this vicinity */
	/* If hit_pt falls outside all the loops in fu2,
	 * then there is no need to break this edge.
	 * XXX It is probably cheaper to call nmg_isect_3vertex_3face()
	 * XXX here first, causing any ON cases to be resolved into
	 * XXX shared topology first (and also cutting fu2 edges NOW),
	 * XXX and then run the classifier to answer IN/OUT.
	 * This is expensive.  For getting started, tolerate it.
	 */
	int class;
	class = nmg_class_pt_fu_except(hit_pt, fu2,
				       (struct loopuse *)NULL,
				       (void (*)())NULL, (void (*)())NULL, (char *)NULL, 0,
				       0, &is->tol);

	eu1forw = nmg_ebreaker((struct vertex *)NULL, eu1, &is->tol);
	vu1_final = eu1forw->vu_p;
	nmg_vertex_gv(vu1_final->v_p, hit_pt);
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("Made new vertex vu=x%x, v=x%x\n", vu1_final, vu1_final->v_p);

	NMG_CK_VERTEX_G(eu1->vu_p->v_p->vg_p);
	NMG_CK_VERTEX_G(eu1->eumate_p->vu_p->v_p->vg_p);
	NMG_CK_VERTEX_G(eu1forw->vu_p->v_p->vg_p);
	NMG_CK_VERTEX_G(eu1forw->eumate_p->vu_p->v_p->vg_p);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    register pointp_t p1 = eu1->vu_p->v_p->vg_p->coord;
	    register pointp_t p2 = eu1->eumate_p->vu_p->v_p->vg_p->coord;

	    bu_log("After split eu1 x%x= %g, %g, %g -> %g, %g, %g\n",
		   eu1,
		   V3ARGS(p1), V3ARGS(p2));
	    p1 = eu1forw->vu_p->v_p->vg_p->coord;
	    p2 = eu1forw->eumate_p->vu_p->v_p->vg_p->coord;
	    bu_log("\teu1forw x%x = %g, %g, %g -> %g, %g, %g\n",
		   eu1forw,
		   V3ARGS(p1), V3ARGS(p2));
	}

	switch (class) {
	    case NMG_CLASS_AinB:
		/* point inside a face loop, break edge */
		break;
	    case NMG_CLASS_AonBshared:
		/* point is on a loop boundary.  Break fu2 loop too? */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
		    bu_log("%%%%%% point is on loop boundary.  Break fu2 loop too?\n");
		nmg_isect_3vertex_3face(is, vu1_final, fu2);
		/* XXX should get new vu2 from isect_3vertex_3face! */
		vu2_final = nmg_find_v_in_face(vu1_final->v_p, fu2);
		if (!vu2_final) bu_bomb("%%%%%% missed!\n");
		NMG_CK_VERTEXUSE(vu2_final);
		nmg_enlist_vu(is, vu1_final, vu2_final, MAX_FASTF);
		return vu2_final;
	    case NMG_CLASS_AoutB:
		/* Can't optimize this, break edge anyway. */
		break;
	    default:
		bu_bomb("nmg_break_3edge_at_plane() bad classification return from nmg_class_pt_f()\n");
	}

	/* stick this vertex in the other shell
	 * and make sure it is in the other shell's
	 * list of vertices on the intersect line
	 */
	plu2 = nmg_mlv(&fu2->l.magic, vu1_final->v_p, OT_BOOLPLACE);
	vu2_final = BU_LIST_FIRST(vertexuse, &plu2->down_hd);
	NMG_CK_VERTEXUSE(vu2_final);
	nmg_loop_g(plu2->l_p, &is->tol);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("Made vertexloop in other face. lu=x%x vu=x%x on v=x%x\n",
		   plu2,
		   vu2_final, vu2_final->v_p);
	}
	vu2_final = nmg_enlist_vu(is, vu1_final, vu2_final, MAX_FASTF);
    }

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	register pointp_t p1, p2;
	p1 = eu1->vu_p->v_p->vg_p->coord;
	p2 = eu1->eumate_p->vu_p->v_p->vg_p->coord;
	bu_log("\tNow %g, %g, %g <-> %g, %g, %g\n",
	       V3ARGS(p1), V3ARGS(p2));
	p1 = eu1forw->vu_p->v_p->vg_p->coord;
	p2 = eu1forw->eumate_p->vu_p->v_p->vg_p->coord;
	bu_log("\tand %g, %g, %g <-> %g, %g, %g\n\n",
	       V3ARGS(p1), V3ARGS(p2));
    }
    return vu2_final;
}


/**
 * N M G _ B R E A K _ E U _ O N _ V
 *
 * The vertex 'v2' is known to lie in the plane of eu1's face.
 * If v2 lies between the two endpoints of eu1, break eu1 and
 * return the new edgeuse pointer.
 *
 * If an edgeuse vertex is joined with v2, v2 remains as the survivor,
 * as the caller is working on it explicitly, and the edgeuse vertices
 * are dealt with implicitly (by dereferencing the eu pointers).
 * Otherwise, we will invalidate our caller's v2 pointer.
 *
 * Note that no "intersection line" stuff is done, the goal here is
 * just to get the edge appropriately broken.
 *
 * Either faceuse can be passed in, but it needs to be consistent with the
 * faceuse used to establish the 2d vertex cache.
 *
 * Returns -
 * new_eu if edge is broken
 * 0 otherwise
 */
struct edgeuse *
nmg_break_eu_on_v(struct edgeuse *eu1, struct vertex *v2, struct faceuse *fu, struct nmg_inter_struct *is)


/* for plane equation of (either) face */

{
    point_t a;
    point_t b;
    point_t p;
    int code;
    fastf_t dist;
    struct vertex *v1a;
    struct vertex *v1b;
    struct edgeuse *new_eu = (struct edgeuse *)0;

    NMG_CK_EDGEUSE(eu1);
    NMG_CK_VERTEX(v2);
    NMG_CK_FACEUSE(fu);
    NMG_CK_INTER_STRUCT(is);

    v1a = eu1->vu_p->v_p;
    v1b = BU_LIST_PNEXT_CIRC(edgeuse, eu1)->vu_p->v_p;

    /* Check for already shared topology */
    if (v1a == v2 || v1b == v2) {
	goto out;
    }

    /* Map to 2d */
    nmg_get_2d_vertex(a, v1a, is, &fu->l.magic);
    nmg_get_2d_vertex(b, v1b, is, &fu->l.magic);
    nmg_get_2d_vertex(p, v2, is, &fu->l.magic);

    dist = -INFINITY;
    code = bn_isect_pt2_lseg2(&dist, a, b, p, &(is->tol));

    switch (code) {
	case -2:
	    /* P outside AB */
	    break;
	default:
	case -1:
	    /* P not on line */
	    /* This can happen when v2 is a long way from the lseg */
	    break;
	case 1:
	    /* P is at A */
	    nmg_jv(v2, v1a);	/* v2 must be surviving vertex */
	    break;
	case 2:
	    /* P is at B */
	    nmg_jv(v2, v1b);	/* v2 must be surviving vertex */
	    break;
	case 3:
	    /* P is in the middle, break edge */
	    new_eu = nmg_ebreaker(v2, eu1, &is->tol);
	    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		bu_log("nmg_break_eu_on_v() breaking eu=x%x on v=x%x, new_eu=x%x\n",
		       eu1, v2, new_eu);
	    }
	    break;
    }

out:
    return new_eu;
}


/**
 * N M G _ B R E A K _ E G _ O N _ V
 *
 * Given a vertex 'v' which is already known to have geometry that lies
 * on the line defined by 'eg', break all the edgeuses along 'eg'
 * which cross 'v'.
 *
 * Calculation is done in 1 dimension:  parametric distance along 'eg'.
 * Edge direction vector needs to be made unit length so that tol->dist
 * makes sense.
 */
void
nmg_break_eg_on_v(const struct edge_g_lseg *eg, struct vertex *v, const struct bn_tol *tol)
{
    register struct edgeuse **eup;
    struct bu_ptbl eutab;
    vect_t dir;
    double vdist;

    NMG_CK_EDGE_G_LSEG(eg);
    NMG_CK_VERTEX(v);
    BN_CK_TOL(tol);

    VMOVE(dir, eg->e_dir);
    VUNITIZE(dir);
    vdist = bn_dist_pt3_along_line3(eg->e_pt, dir, v->vg_p->coord);

    /* This has to be a table, because nmg_ebreaker() will
     * change the list on the fly, otherwise.
     */
    nmg_edgeuse_with_eg_tabulate(&eutab, eg);

    for (eup = (struct edgeuse **)BU_PTBL_LASTADDR(&eutab);
	 eup >= (struct edgeuse **)BU_PTBL_BASEADDR(&eutab);
	 eup--
	) {
	struct vertex *va;
	struct vertex *vb;
	double a;
	double b;
	struct edgeuse *new_eu;

	NMG_CK_EDGEUSE(*eup);
	if ((*eup)->g.lseg_p != eg) bu_bomb("nmg_break_eg_on_v() eu disowns eg\n");

	va = (*eup)->vu_p->v_p;
	vb = (*eup)->eumate_p->vu_p->v_p;
	if (v == va || v == vb) continue;
	if (bn_pt3_pt3_equal(v->vg_p->coord, va->vg_p->coord, tol)) {
	    nmg_jv(v, va);
	    continue;
	}
	if (bn_pt3_pt3_equal(v->vg_p->coord, vb->vg_p->coord, tol)) {
	    nmg_jv(v, vb);
	    continue;
	}
	a = bn_dist_pt3_along_line3(eg->e_pt, dir, va->vg_p->coord);
	b = bn_dist_pt3_along_line3(eg->e_pt, dir, vb->vg_p->coord);
	if (NEAR_EQUAL(a, vdist, tol->dist)) continue;
	if (NEAR_EQUAL(b, vdist, tol->dist)) continue;
	if (!bn_between(a, vdist, b, tol)) continue;
	new_eu = nmg_ebreaker(v, *eup, tol);
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("nmg_break_eg_on_v(eg=x%x, v=x%x) new_eu=x%x\n",
		   eg, v, new_eu);
	}
    }
    bu_ptbl_free(&eutab);
}


/**
 * N M G _ I S E C T _ 2 C O L I N E A R _ E D G E 2 P
 *
 * Perform edge mutual breaking only on two colinear edgeuses.
 * This can result in 2 new edgeuses showing up in either loop (case A & D).
 * The vertexuse lists are updated to have all participating vu's and
 * their duals.
 *
 * Two colinear line segments (eu1 and eu2, or just "1" and "2" in the
 * diagram) can overlap each other in one of 9 configurations,
 * labeled A through I:
 *
 *	A	B	C	D	E	F	G	H	I
 *
 * vu1b, vu2b
 *	*	*	  *	  *	*	  *	*	  *	*=*
 *	1	1	  2	  2	1	  2	1	  2	1 2
 *	1=*	1	  2	*=2	1=*	*=2	*	  *	1 2
 *	1 2	*=*	*=*	1 2	1 2	1 2			1 2
 *	1 2	  2	1	1 2	1 2	1 2	  *	*	1 2
 *	1=*	  2	1	*=2	*=2	1=*	  2	1	1 2
 *	1	  *	*	  2	  2	1	  *	*	1 2
 *	*			  *	  *	*			*=*
 *  vu1a, vu2a
 *
 * To ensure nothing is missed, break every edgeuse on all 4 vertices.
 * If a new edgeuse is created, add it to the list of edgeuses still to be
 * broken.
 * Brute force, but *certain* not to miss anything.
 *
 * There is nothing to prevent eu1 and eu2 from being edgeuses in the same
 * loop.  This creates interesting patterns if one is NEXT of the other,
 * such as vu[1] == vu[2].  Just handle it gracefully.
 *
 * Returns the number of edgeuses that resulted,
 * which is always at least the original 2.
 *
 */
int
nmg_isect_2colinear_edge2p(struct edgeuse *eu1, struct edgeuse *eu2, struct faceuse *fu, struct nmg_inter_struct *is, struct bu_ptbl *l1, struct bu_ptbl *l2)


/* for plane equation of (either) face */

/* optional: list of new eu1 pieces */
/* optional: list of new eu2 pieces */
{
    struct edgeuse *eu[10];
    struct vertexuse *vu[4];
    register int i;
    register int j;
    int neu;	/* Number of edgeuses */

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_2colinear_edge2p(eu1=x%x, eu2=x%x) START\n",
	       eu1, eu2);
    }

    NMG_CK_EDGEUSE(eu1);
    NMG_CK_EDGEUSE(eu2);
    NMG_CK_FACEUSE(fu);	/* Don't check it, just pass it on down. */
    NMG_CK_INTER_STRUCT(is);
    if (l1) BU_CK_PTBL(l1);
    if (l2) BU_CK_PTBL(l2);

    vu[0] = eu1->vu_p;
    vu[1] = BU_LIST_PNEXT_CIRC(edgeuse, eu1)->vu_p;
    vu[2] = eu2->vu_p;
    vu[3] = BU_LIST_PNEXT_CIRC(edgeuse, eu2)->vu_p;

    eu[0] = eu1;
    eu[1] = eu2;
    neu = 2;

    for (i=0; i < neu; i++) {
	for (j=0; j<4; j++) {
	    eu[neu] = nmg_break_eu_on_v(eu[i], vu[j]->v_p, fu, is);
	    if (eu[neu]) {
		nmg_enlist_vu(is, eu[neu]->vu_p, vu[j], MAX_FASTF);
		if (l1 && eu[neu]->e_p == eu1->e_p)
		    bu_ptbl_ins_unique(l1, (long *)&eu[neu]->l.magic);
		else if (l2 && eu[neu]->e_p == eu2->e_p)
		    bu_ptbl_ins_unique(l2, (long *)&eu[neu]->l.magic);
		neu++;
	    }
	}
    }

    /* Now join 'em up */
    /* This step should no longer be necessary, as nmg_ebreaker()
     * from nmg_break_eu_on_v() should have already handled this. */
    for (i=0; i < neu-1; i++) {
	for (j=i+1; j < neu; j++) {
	    if (!NMG_ARE_EUS_ADJACENT(eu[i], eu[j])) continue;
	    nmg_radial_join_eu(eu[i], eu[j], &(is->tol));
	}
    }

    /* Enlist all four of the original endpoints */
    for (i=0; i < 4; i++) {
	for (j=0; j < 4; j++) {
	    if (i==j) continue;
	    if (vu[i] == vu[j]) continue;	/* Happens if eu2 follows eu1 in loop */
	    if (vu[i]->v_p == vu[j]->v_p) {
		nmg_enlist_vu(is, vu[i], vu[j], MAX_FASTF);
		goto next_i;
	    }
	}
	/* No match, let subroutine hunt for dual */
	nmg_enlist_vu(is, vu[i], 0, MAX_FASTF);
    next_i:		;
    }

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_2colinear_edge2p(eu1=x%x, eu2=x%x) ret #eu=%d\n",
	       eu1, eu2, neu);
    }
    return neu;
}


/**
 * N M G _ I S E C T _ E D G E 2 P _ E D G E 2 P
 *
 * Actual 2d edge/edge intersector
 *
 * One or both of the edges may be wire edges, i.e.
 * either or both of the fu1 and fu2 args may be null.
 * If so, the vert_list's are unimport4ant.
 *
 * Returns a bit vector -
 * ISECT_NONE no intersection
 * ISECT_SHARED_V intersection was at (at least one) shared vertex
 * ISECT_SPLIT1 eu1 was split at (geometric) intersection.
 * ISECT_SPLIT2 eu2 was split at (geometric) intersection.
 */
int
nmg_isect_edge2p_edge2p(struct nmg_inter_struct *is, struct edgeuse *eu1, struct edgeuse *eu2, struct faceuse *fu1, struct faceuse *fu2)


/* fu of eu1, for plane equation */
/* fu of eu2, for error checks */
{
    point_t eu1_start;
    point_t eu1_end;
    vect_t eu1_dir;
    point_t eu2_start;
    point_t eu2_end;
    vect_t eu2_dir;
    vect_t dir3d;
    fastf_t dist[2];
    int status;
    point_t hit_pt;
    struct vertexuse *vu;
    struct vertexuse *vu1a, *vu1b;
    struct vertexuse *vu2a, *vu2b;
    struct model *m;
    int ret = 0;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_EDGEUSE(eu1);
    NMG_CK_EDGEUSE(eu2);
    m = nmg_find_model(&eu1->l.magic);
    NMG_CK_MODEL(m);
    /*
     * Important note:  don't use eu1->eumate_p->vu_p here,
     * because that vu is in the opposite orientation faceuse.
     * Putting those vu's on the intersection line makes for big trouble.
     */
    vu1a = eu1->vu_p;
    vu1b = BU_LIST_PNEXT_CIRC(edgeuse, eu1)->vu_p;
    vu2a = eu2->vu_p;
    vu2b = BU_LIST_PNEXT_CIRC(edgeuse, eu2)->vu_p;
    NMG_CK_VERTEXUSE(vu1a);
    NMG_CK_VERTEXUSE(vu1b);
    NMG_CK_VERTEXUSE(vu2a);
    NMG_CK_VERTEXUSE(vu2b);

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_edge2p_edge2p(eu1=x%x, eu2=x%x) START\n\tfu1=x%x, fu2=x%x\n\tvu1a=%x vu1b=%x, vu2a=%x vu2b=%x\n\tv1a=%x v1b=%x,   v2a=%x v2b=%x\n",
	       eu1, eu2,
	       fu1, fu2,
	       vu1a, vu1b, vu2a, vu2b,
	       vu1a->v_p, vu1b->v_p, vu2a->v_p, vu2b->v_p);
    }

    /*
     * Topology check.
     * If both endpoints of both edges match, this is a trivial accept.
     */
    if (vu1a->v_p == vu2a->v_p && vu1b->v_p == vu2b->v_p) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("nmg_isect_edge2p_edge2p: shared edge topology, both ends\n");
	nmg_radial_join_eu(eu1, eu2, &is->tol);
	nmg_enlist_vu(is, vu1a, vu2a, MAX_FASTF);
	nmg_enlist_vu(is, vu1b, vu2b, MAX_FASTF);
	ret = ISECT_SHARED_V;
	goto out;		/* vu1a, vu1b already listed */
    }
    if (vu1a->v_p == vu2b->v_p && vu1b->v_p == vu2a->v_p) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("nmg_isect_edge2p_edge2p: shared edge topology, both ends, reversed.\n");
	nmg_radial_join_eu(eu1, eu2, &is->tol);
	nmg_enlist_vu(is, vu1a, vu2b, MAX_FASTF);
	nmg_enlist_vu(is, vu1b, vu2a, MAX_FASTF);
	ret = ISECT_SHARED_V;
	goto out;		/* vu1a, vu1b already listed */
    }

    /*
     * The 3D line in is->pt and is->dir is prepared by the caller.
     * is->pt is *not* one of the endpoints of this edge.
     *
     * IMPORTANT NOTE:  The edge-ray used for the edge intersection
     * calculations is colinear with the "intersection line",
     * but the edge-ray starts at vu1a and points to vu1b,
     * while the intersection line has to satisfy different constraints.
     * Don't confuse the two!
     */
    nmg_get_2d_vertex(eu1_start, vu1a->v_p, is, &fu2->l.magic);	/* 2D line */
    nmg_get_2d_vertex(eu1_end, vu1b->v_p, is, &fu2->l.magic);
    VSUB2_2D(eu1_dir, eu1_end, eu1_start);

    nmg_get_2d_vertex(eu2_start, vu2a->v_p, is, &fu2->l.magic);
    nmg_get_2d_vertex(eu2_end, vu2b->v_p, is, &fu2->l.magic);
    VSUB2_2D(eu2_dir, eu2_end, eu2_start);

    dist[0] = dist[1] = 0;	/* for clean prints, below */

    /* The "proper" thing to do is intersect two line segments.
     * However, this means that none of the intersections of edge "line"
     * with the exterior of the loop are computed, and that
     * violates the strategy assumptions of the face-cutter.
     */
    /* To pick up ALL intersection points, the source edge is a line */
    status = bn_isect_line2_lseg2(dist, eu1_start, eu1_dir,
				  eu2_start, eu2_dir, &is->tol);

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("\tbn_isect_line2_lseg2()=%d, dist: %g, %g\n",
	       status, dist[0], dist[1]);
    }

    /*
     * Whether geometry hits or misses, as long as not colinear, check topo.
     * If one endpoint matches, and edges are not colinear,
     * then accept the one shared vertex as the intersection point.
     * Can't do this before geometry check, or we might miss the
     * colinear condition, and not do the mutual intersection.
     */
    if (status != 0 &&
	(vu1a->v_p == vu2a->v_p || vu1a->v_p == vu2b->v_p ||
	 vu1b->v_p == vu2a->v_p || vu1b->v_p == vu2b->v_p)
	) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("edge2p_edge2p: non-colinear edges share one vertex (topology)\n");
	if (vu1a->v_p == vu2a->v_p)
	    nmg_enlist_vu(is, vu1a, vu2a, MAX_FASTF);
	else if (vu1a->v_p == vu2b->v_p)
	    nmg_enlist_vu(is, vu1a, vu2b, MAX_FASTF);

	if (vu1b->v_p == vu2a->v_p)
	    nmg_enlist_vu(is, vu1b, vu2a, MAX_FASTF);
	else if (vu1b->v_p == vu2b->v_p)
	    nmg_enlist_vu(is, vu1b, vu2b, MAX_FASTF);

	ret = ISECT_SHARED_V;
	goto out;		/* vu1a, vu1b already listed */
    }

    if (status < 0) {
	ret = ISECT_NONE;	/* No geometric intersection */
	goto topo;		/* Still need to list vu1a, vu2b */
    }

    if (status == 0) {
	/* Lines are co-linear and on line of intersection. */
	/* Perform full mutual intersection, and vu enlisting. */
	if (nmg_isect_2colinear_edge2p(eu1, eu2, fu2, is, (struct bu_ptbl *)0, (struct bu_ptbl *)0) > 2) {
	    /* Can't tell which edgeuse(s) got split */
	    ret = ISECT_SPLIT1 | ISECT_SPLIT2;
	} else {
	    /* XXX Can't tell if some sharing ensued.  Does it matter? */
	    /* No, not for the one place we are called. */
	    ret = ISECT_NONE;
	}
	goto out;		/* vu1a, vu1b listed by nmg_isect_2colinear_edge2p */
    }

    /* There is only one intersect point.  Break one or both edges. */


    /* The ray defined by the edgeuse line eu1 intersects the lseg eu2.
     * Tolerances have already been factored in.
     * The edge exists over values of 0 <= dist <= 1.
     */
    VSUB2(dir3d, vu1b->v_p->vg_p->coord, vu1a->v_p->vg_p->coord);
    VJOIN1(hit_pt, vu1a->v_p->vg_p->coord, dist[0], dir3d);

    if (ZERO(dist[0])) {
	/* First point of eu1 is on eu2, by geometry */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tvu=x%x vu1a is intersect point\n", vu1a);
	if (dist[1] < 0 || dist[1] > 1) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\teu1 line intersects eu2 outside vu2a...vu2b range, ignore.\n");
	    ret = ISECT_NONE;
	    goto topo;
	}

	/* Edges not colinear. Either join up with a matching vertex,
	 * or break eu2 on our vert.
	 */
	if (ZERO(dist[1])) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\tvu2a matches vu1a\n");
	    nmg_jv(vu1a->v_p, vu2a->v_p);
	    nmg_enlist_vu(is, vu1a, vu2a, MAX_FASTF);
	    ret = ISECT_SHARED_V;
	    goto topo;
	}
	if (ZERO(dist[1] - 1.0)) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\tsecond point of eu2 matches vu1a\n");
	    nmg_jv(vu1a->v_p, vu2b->v_p);
	    nmg_enlist_vu(is, vu1a, vu2b, MAX_FASTF);
	    ret = ISECT_SHARED_V;
	    goto topo;
	}
	/* Break eu2 on our first vertex */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tbreaking eu2 on vu1a\n");
	vu = nmg_ebreaker(vu1a->v_p, eu2, &is->tol)->vu_p;
	nmg_enlist_vu(is, vu1a, vu, MAX_FASTF);
	ret = ISECT_SPLIT2;	/* eu1 not broken, just touched */
	goto topo;
    }

    if (ZERO(dist[0] - 1.0)) {
	/* Second point of eu1 is on eu2, by geometry */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tvu=x%x vu1b is intersect point\n", vu1b);
	if (dist[1] < 0 || dist[1] > 1) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\teu1 line intersects eu2 outside vu2a...vu2b range, ignore.\n");
	    ret = ISECT_NONE;
	    goto topo;
	}

	/* Edges not colinear. Either join up with a matching vertex,
	 * or break eu2 on our vert.
	 */
	if (ZERO(dist[1])) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\tvu2a matches vu1b\n");
	    nmg_jv(vu1b->v_p, vu2a->v_p);
	    nmg_enlist_vu(is, vu1b, vu2a, MAX_FASTF);
	    ret = ISECT_SHARED_V;
	    goto topo;
	}
	if (ZERO(dist[1] - 1.0)) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\tsecond point of eu2 matches vu1b\n");
	    nmg_jv(vu1b->v_p, vu2b->v_p);
	    nmg_enlist_vu(is, vu1b, vu2b, MAX_FASTF);
	    ret = ISECT_SHARED_V;
	    goto topo;
	}
	/* Break eu2 on our second vertex */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tbreaking eu2 on vu1b\n");
	vu = nmg_ebreaker(vu1b->v_p, eu2, &is->tol)->vu_p;
	nmg_enlist_vu(is, vu1b, vu, MAX_FASTF);
	ret = ISECT_SPLIT2;	/* eu1 not broken, just touched */
	goto topo;
    }

    /* eu2 intersect point is on eu1 line, but not between vertices.
     * Since it crosses the line of intersection, it must be broken.
     */
    if (dist[0] < 0 || dist[0] > 1) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tIntersect point on eu2 is outside vu1a...vu1b.  Break eu2 anyway.\n");

	if (ZERO(dist[1])) {
	    nmg_enlist_vu(is, vu2a, 0, MAX_FASTF);
	    ret = ISECT_SHARED_V;		/* eu1 was not broken */
	    goto topo;
	} else if (ZERO(dist[1] - 1.0)) {
	    nmg_enlist_vu(is, vu2b, 0, MAX_FASTF);
	    ret = ISECT_SHARED_V;		/* eu1 was not broken */
	    goto topo;
	} else if (dist[1] > 0 && dist[1] < 1) {
	    /* Break eu2 somewhere in the middle */
	    struct vertexuse *new_vu2;
	    struct vertex *new_v2;
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		VPRINT("\t\tBreaking eu2 at intersect point", hit_pt);
	    new_v2 = nmg_find_pt_in_model(m, hit_pt, &(is->tol));
	    new_vu2 = nmg_ebreaker(new_v2, eu2, &is->tol)->vu_p;
	    if (!new_v2) {
		/* A new vertex was created, assign geom */
		nmg_vertex_gv(new_vu2->v_p, hit_pt);	/* 3d geom */
	    }
	    nmg_enlist_vu(is, new_vu2, 0, MAX_FASTF);
	    ret = ISECT_SPLIT2;	/* eu1 was not broken */
	    goto topo;
	}

	/* Hit point not on either eu1 or eu2, nothing to do */
	ret = ISECT_NONE;
	goto topo;
    }

    /* Intersection is in the middle of the reference edge (eu1) */
    /* dist[0] >= 0 && dist[0] <= 1) */
    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("\tintersect is in middle of eu1, breaking it\n");

    /* Edges not colinear. Either join up with a matching vertex,
     * or break eu2 on our vert.
     */
    if (ZERO(dist[1])) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\t\tintersect point is vu2a\n");
	vu = nmg_ebreaker(vu2a->v_p, eu1, &is->tol)->vu_p;
	nmg_enlist_vu(is, vu2a, vu, MAX_FASTF);
	ret |= ISECT_SPLIT1;
	goto topo;
    } else if (ZERO(dist[1] - 1.0)) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\t\tintersect point is vu2b\n");
	vu = nmg_ebreaker(vu2b->v_p, eu1, &is->tol)->vu_p;
	nmg_enlist_vu(is, vu2b, vu, MAX_FASTF);
	ret |= ISECT_SPLIT1;
	goto topo;
    } else if (dist[1] > 0 && dist[1] < 1) {
	/* Intersection is in the middle of both, split edge */
	struct vertex *new_v;
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    VPRINT("\t\tBreaking both edges at intersect point", hit_pt);
	ret = ISECT_SPLIT1 | ISECT_SPLIT2;
	new_v = nmg_e2break(eu1, eu2);
	nmg_vertex_gv(new_v, hit_pt);	/* 3d geometry */

	/* new_v is at far end of eu1 and eu2 */
	if (eu1->eumate_p->vu_p->v_p != new_v) bu_bomb("new_v 1\n");
	if (eu2->eumate_p->vu_p->v_p != new_v) bu_bomb("new_v 2\n");
	/* Can't use eumate_p here, it's in wrong orientation face */
	nmg_enlist_vu(is, BU_LIST_PNEXT_CIRC(edgeuse, eu1)->vu_p,
		      BU_LIST_PNEXT_CIRC(edgeuse, eu2)->vu_p, MAX_FASTF);
	goto topo;
    } else {
	/* Intersection is in middle of eu1, which lies on the
	 * line of intersection being computed, but is outside
	 * the endpoints of eu2.  There is no point in breaking
	 * eu1 here -- it does not connnect up with anything.
	 */
	ret = ISECT_NONE;
	goto topo;
    }

topo:
    /*
     * Listing of any vu's from eu2 will have been done above.
     *
     * The *original* vu1a and vu1b (and their duals) MUST be
     * forcibly listed on
     * the intersection line, since eu1 lies ON the line!
     *
     * This is done last, so that the intersection code (above) has
     * the opportunity to create the duals.
     * vu1a and vu1b don't have to have anything to do with eu2,
     * hence the 2nd vu argument is unspecified (0).
     * For our purposes here, we will be satisfied with *any* use
     * of the same vertex in the other face.
     */
    nmg_enlist_vu(is, vu1a, 0, MAX_FASTF);
    nmg_enlist_vu(is, vu1b, 0, MAX_FASTF);
out:
    /* By here, vu1a and vu1b MUST have been enlisted */
    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_edge2p_edge2p(eu1=x%x, eu2=x%x) END, ret=%d %s%s%s\n",
	       eu1, eu2, ret,
	       (ret&ISECT_SHARED_V)? "SHARED_V|" :
	       ((ret==0) ? "NONE" : ""),
	       (ret&ISECT_SPLIT1)? "SPLIT1|" : "",
	       (ret&ISECT_SPLIT2)? "SPLIT2" : ""
	    );
    }

    return ret;
}


/**
 * N M G _ I S E C T _ W I R E E D G E 3 P _ F A C E 3 P
 *
 * Intersect an edge eu1 with a faceuse fu2.
 * eu1 may belong to fu1, or it may be a wire edge.
 *
 * XXX It is not clear whether we need the caller to provide the
 * line equation, or if we should just create it here.
 * If done here, the start pt needs to be outside fu2 (fu1 also?)
 *
 * Returns -
 * 0 If everything went well
 * 1 If vu[] list along the intersection line needs to be re-done.
 */
static int
nmg_isect_wireedge3p_face3p(struct nmg_inter_struct *is, struct edgeuse *eu1, struct faceuse *fu2)
{
    struct vertexuse *vu1_final = (struct vertexuse *)NULL;
    struct vertexuse *vu2_final = (struct vertexuse *)NULL;
    struct vertex *v1a;		/* vertex at start of eu1 */
    struct vertex *v1b;		/* vertex at end of eu1 */
    point_t hit_pt;
    vect_t edge_vect;
    fastf_t edge_len;	/* MAGNITUDE(edge_vect) */
    fastf_t dist;		/* parametric dist to hit point */
    fastf_t dist_to_plane;	/* distance to hit point, in mm */
    int status;
    vect_t start_pt;
    struct edgeuse *eunext;
    struct faceuse *fu1;		/* fu that contains eu1 */
    plane_t n2;
    int ret = 0;

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_wireedge3p_face3p(, eu1=x%x, fu2=x%x) START\n", eu1, fu2);

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_EDGEUSE(eu1);
    NMG_CK_VERTEXUSE(eu1->vu_p);
    v1a = eu1->vu_p->v_p;
    NMG_CK_VERTEX(v1a);
    NMG_CK_VERTEX_G(v1a->vg_p);

    NMG_CK_EDGEUSE(eu1->eumate_p);
    NMG_CK_VERTEXUSE(eu1->eumate_p->vu_p);
    v1b = eu1->eumate_p->vu_p->v_p;
    NMG_CK_VERTEX(v1b);
    NMG_CK_VERTEX_G(v1b->vg_p);

    NMG_CK_FACEUSE(fu2);
    if (fu2->orientation != OT_SAME) bu_bomb("nmg_isect_wireedge3p_face3p() fu2 not OT_SAME\n");
    fu1 = nmg_find_fu_of_eu(eu1);	/* May be NULL */

    /*
     * Form a ray that starts at one vertex of the edgeuse
     * and points to the other vertex.
     */
    VSUB2(edge_vect, v1b->vg_p->coord, v1a->vg_p->coord);
    edge_len = MAGNITUDE(edge_vect);

    VMOVE(start_pt, v1a->vg_p->coord);

    {
	/* XXX HACK */
	double dot;
	dot = fabs(VDOT(is->dir, edge_vect) / edge_len) - 1;
	if (!NEAR_ZERO(dot, .01)) {
	    bu_log("HACK HACK cough cough.  Resetting is->pt, is->dir\n");
	    VPRINT("old is->pt ", is->pt);
	    VPRINT("old is->dir", is->dir);
	    VMOVE(is->pt, start_pt);
	    VMOVE(is->dir, edge_vect);
	    VUNITIZE(is->dir);
	    VPRINT("new is->pt ", is->pt);
	    VPRINT("new is->dir", is->dir);
	}
    }

    NMG_GET_FU_PLANE(n2, fu2);
    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("Testing (%g, %g, %g) -> (%g, %g, %g) dir=(%g, %g, %g)\n",
	       V3ARGS(start_pt),
	       V3ARGS(v1b->vg_p->coord),
	       V3ARGS(edge_vect));
	PLPRINT("\t", n2);
    }

    status = bn_isect_line3_plane(&dist, start_pt, edge_vect,
				  n2, &is->tol);

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	if (status >= 0)
	    bu_log("\tHit. bn_isect_line3_plane=%d, dist=%g (%e)\n",
		   status, dist, dist);
	else
	    bu_log("\tMiss. Boring status of bn_isect_line3_plane: %d\n",
		   status);
    }
    if (status == 0) {
	struct nmg_inter_struct is2;

	/*
	 * Edge (ray) lies in the plane of the other face,
	 * by geometry.  Drop into 2D code to handle all
	 * possible intersections (there may be many),
	 * and any cut/joins, then resume with the previous work.
	 */
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("nmg_isect_wireedge3p_face3p: edge lies ON face, using 2D code\n@ @ @ @ @ @ @ @ @ @ 2D CODE, START\n");
	    bu_log(" The status of the face/face intersect line, before 2d:\n");
	    nmg_pr_ptbl_vert_list("l1", is->l1, is->mag1);
	    nmg_pr_ptbl_vert_list("l2", is->l2, is->mag2);
	}

	is2 = *is;	/* make private copy */
	is2.vert2d = 0;	/* Don't use previously initialized stuff */

	ret = nmg_isect_edge2p_face2p(&is2, eu1, fu2, fu1);

	nmg_isect2d_cleanup(&is2);

	/*
	 * Because nmg_isect_edge2p_face2p() calls the face cutter,
	 * vu's in lone lu's that are listed in the current l1 or
	 * l2 lists may have been destroyed.  Its ret is ours.
	 */

	/* Only do this if list is still OK */
	if (rt_g.NMG_debug & DEBUG_POLYSECT && ret == 0) {
	    bu_log("nmg_isect_wireedge3p_face3p: @ @ @ @ @ @ @ @ @ @ 2D CODE, END, resume 3d problem.\n");
	    bu_log(" The status of the face/face intersect line, so far:\n");
	    nmg_pr_ptbl_vert_list("l1", is->l1, is->mag1);
	    nmg_pr_ptbl_vert_list("l2", is->l2, is->mag2);
	}

	/* See if start vertex is now shared */
	vu2_final=nmg_find_v_in_face(eu1->vu_p->v_p, fu2);
	if (vu2_final) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\tEdge start vertex lies on other face (2d topology).\n");
	    vu1_final = eu1->vu_p;
	    (void)bu_ptbl_ins_unique(is->l1, (long *)&vu1_final->l.magic);
	    (void)bu_ptbl_ins_unique(is->l2, (long *)&vu2_final->l.magic);
	}
	/* XXX HACK HACK -- shut off error checking */
	vu1_final = vu2_final = (struct vertexuse *)NULL;
	goto out;
    }

    /*
     * We now know that the the edge does not lie +in+ the other face,
     * so it will intersect the face in at most one point.
     * Before looking at the results of the geometric calculation,
     * check the topology.  If the topology says that starting vertex
     * of this edgeuse is on the other face, that is the hit point.
     * Enter the two vertexuses of that starting vertex in the list,
     * and return.
     *
     * XXX Lee wonders if there might be a benefit to violating the
     * XXX "only ask geom question once" rule, and doing a geom
     * XXX calculation here before the topology check.
     */
    vu2_final=nmg_find_v_in_face(v1a, fu2);
    if (vu2_final) {
	vu1_final = eu1->vu_p;
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("\tEdge start vertex lies on other face (topology).\n\tAdding vu1_final=x%x (v=x%x), vu2_final=x%x (v=x%x)\n",
		   vu1_final, vu1_final->v_p,
		   vu2_final, vu2_final->v_p);
	}
	(void)bu_ptbl_ins_unique(is->l1, (long *)&vu1_final->l.magic);
	(void)bu_ptbl_ins_unique(is->l2, (long *)&vu2_final->l.magic);
	goto out;
    }

    if (status < 0) {
	/* Ray does not strike plane.
	 * See if start point lies on plane.
	 */
	dist = VDOT(start_pt, n2) - n2[W];
	if (!NEAR_ZERO(dist, is->tol.dist))
	    goto out;		/* No geometric intersection */

	/* XXX Does this ever happen, now that geom calc is done
	 * XXX above, and there is 2D handling as well?  Lets find out.
	 */
	bu_bomb("nmg_isect_wireedge3p_face3p: Edge start vertex lies on other face (geometry)\n");

	/* Start point lies on plane of other face */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tEdge start vertex lies on other face (geometry)\n");
	dist = VSUB2DOT(v1a->vg_p->coord, start_pt, edge_vect)
	    / edge_len;
    }

    /* The ray defined by the edgeuse intersects the plane
     * of the other face.  Check to see if the distance to
     * intersection is between limits of the endpoints of
     * this edge(use).
     * The edge exists over values of 0 <= dist <= 1, ie,
     * over values of 0 <= dist_to_plane <= edge_len.
     * The tolerance, an absolute distance, can only be compared
     * to other absolute distances like dist_to_plane & edge_len.
     * The vertices are "fattened" by +/- is->tol units.
     */
    dist_to_plane = edge_len * dist;

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("\tedge_len=%g, dist=%g, dist_to_plane=%g\n",
	       edge_len, dist, dist_to_plane);

    if (dist_to_plane < -is->tol.dist) {
	/* Hit is behind first point */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tplane behind first point\n");
	goto out;
    }

    if (dist_to_plane > edge_len + is->tol.dist) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tplane beyond second point\n");
	goto out;
    }

    VJOIN1(hit_pt, start_pt, dist, edge_vect);

    /* Check hit_pt against face/face intersection line */
    {
	fastf_t ff_dist;
	ff_dist = bn_dist_line3_pt3(is->pt, is->dir, hit_pt);
	if (ff_dist > is->tol.dist) {
	    bu_log("WARNING nmg_isect_wireedge3p_face3p() hit_pt off f/f line %g*tol (%e, tol=%e)\n",
		   ff_dist/is->tol.dist,
		   ff_dist, is->tol.dist);
	    /* XXX now what? */
	}
    }

    /*
     * If the vertex on the other end of this edgeuse is on the face,
     * then make a linkage to an existing face vertex (if found),
     * and give up on this edge, knowing that we'll pick up the
     * intersection of the next edgeuse with the face later.
     */
    if (dist_to_plane < is->tol.dist) {
	/* First point is on plane of face, by geometry */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tedge starts at plane intersect\n");
	vu1_final = eu1->vu_p;
	vu2_final = nmg_enlist_vu(is, vu1_final, 0, MAX_FASTF);
	goto out;
    }

    if (dist_to_plane < edge_len - is->tol.dist) {
	/* Intersection is between first and second vertex points.
	 * Insert new vertex at intersection point.
	 */
	vu2_final = nmg_break_3edge_at_plane(hit_pt, fu2, is, eu1);
	if (vu2_final)
	    vu1_final = BU_LIST_PNEXT_CIRC(edgeuse, eu1)->vu_p;
	goto out;
    }

    /* Second point is on plane of face, by geometry */
    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("\tedge ends at plane intersect\n");

    eunext = BU_LIST_PNEXT_CIRC(edgeuse, eu1);
    NMG_CK_EDGEUSE(eunext);
    if (eunext->vu_p->v_p != v1b)
	bu_bomb("nmg_isect_wireedge3p_face3p: discontinuous eu loop\n");

    vu1_final = eunext->vu_p;
    vu2_final = nmg_enlist_vu(is, vu1_final, 0, MAX_FASTF);

out:
    /* If vu's were added to list, run some quick checks here */
    if (vu1_final && vu2_final) {
	if (vu1_final->v_p != vu2_final->v_p) bu_bomb("nmg_isect_wireedge3p_face3p() vertex mis-match\n");

	dist = bn_dist_line3_pt3(is->pt, is->dir,
				 vu1_final->v_p->vg_p->coord);
	if (dist > 100*is->tol.dist) {
	    bu_log("ERROR nmg_isect_wireedge3p_face3p() vu1=x%x point off line by %g > 100*dist_tol (%g)\n",
		   vu1_final, dist, 100*is->tol.dist);
	    VPRINT("is->pt|", is->pt);
	    VPRINT("is->dir", is->dir);
	    VPRINT(" coord ", vu1_final->v_p->vg_p->coord);
	    bu_bomb("nmg_isect_wireedge3p_face3p()\n");
	}
	if (dist > is->tol.dist) {
	    bu_log("WARNING nmg_isect_wireedge3p_face3p() vu1=x%x pt off line %g*tol (%e, tol=%e)\n",
		   vu1_final, dist/is->tol.dist,
		   dist, is->tol.dist);
	}
    }

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_wireedge3p_face3p(, eu1=x%x, fu2=x%x) ret=%d END\n", eu1, fu2, ret);
    return ret;
}


/**
 * N M G _ I S E C T _ W I R E L O O P 3 P _ F A C E 3 P
 *
 * Intersect a single loop with another face.
 * Note that it may be a wire loop.
 *
 * Returns -
 * 0 everything is ok
 * >0 vu[] list along intersection line needs to be re-done.
 */
static int
nmg_isect_wireloop3p_face3p(struct nmg_inter_struct *bs, struct loopuse *lu, struct faceuse *fu)
{
    struct edgeuse *eu;
    uint32_t magic1;
    int discards = 0;

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	plane_t n;
	bu_log("nmg_isect_wireloop3p_face3p(, lu=x%x, fu=x%x) START\n", lu, fu);
	NMG_GET_FU_PLANE(n, fu);
	HPRINT(" fg N", n);
    }

    NMG_CK_INTER_STRUCT(bs);
    NMG_CK_LOOPUSE(lu);
    NMG_CK_LOOP(lu->l_p);
    NMG_CK_LOOP_G(lu->l_p->lg_p);

    NMG_CK_FACEUSE(fu);

    magic1 = BU_LIST_FIRST_MAGIC(&lu->down_hd);
    if (magic1 == NMG_VERTEXUSE_MAGIC) {
	struct vertexuse *vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	/* this is most likely a loop inserted when we split
	 * up fu2 wrt fu1 (we're now splitting fu1 wrt fu2)
	 */
	nmg_isect_3vertex_3face(bs, vu, fu);
	return 0;
    } else if (magic1 != NMG_EDGEUSE_MAGIC) {
	bu_bomb("nmg_isect_wireloop3p_face3p() Unknown type of NMG loopuse\n");
    }

    /* Process loop consisting of a list of edgeuses.
     *
     * By going backwards around the list we avoid
     * re-processing an edgeuse that was just created
     * by nmg_isect_wireedge3p_face3p.  This is because the edgeuses
     * point in the "next" direction, and when one of
     * them is split, it inserts a new edge AHEAD or
     * "nextward" of the current edgeuse.
     */
    for (eu = BU_LIST_LAST(edgeuse, &lu->down_hd);
	 BU_LIST_NOT_HEAD(eu, &lu->down_hd);
	 eu = BU_LIST_PLAST(edgeuse, eu)) {
	NMG_CK_EDGEUSE(eu);

	if (eu->up.magic_p != &lu->l.magic) {
	    bu_bomb("nmg_isect_wireloop3p_face3p: edge does not share loop\n");
	}

	discards += nmg_isect_wireedge3p_face3p(bs, eu, fu);

	nmg_ck_lueu(lu, "nmg_isect_wireloop3p_face3p");
    }

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_wireloop3p_face3p(, lu=x%x, fu=x%x) END, discards=%d\n", lu, fu, discards);
    }
    return discards;
}


/**
 * N M G _ I S E C T _ C O N S T R U C T _ N I C E _ R A Y
 *
 * Construct a nice ray for is->pt, is->dir
 * which contains the line of intersection, is->on_eg.
 *
 * See the comment in nmg_isect_two_generics_faces() for details
 * on the constraints on this ray, and the algorithm.
 *
 * XXX Danger?
 * The ray -vs- RPP check is being done in 3D.
 * It really ought to be done in 2D, to ensure that
 * long edge lines on nearly axis-aligned faces don't
 * get discarded prematurely!
 * XXX Can't just comment out the code, I think the selection
 * XXX of is->pt is significant:
 * 1) All intersections are at positive distances on the ray,
 * 2) dir cross N will point "left".
 *
 * Returns -
 * 0 OK
 * 1 ray misses fu2 bounding box
 */
int
nmg_isect_construct_nice_ray(struct nmg_inter_struct *is, struct faceuse *fu2)
{
    struct xray line;
    vect_t invdir;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_FACEUSE(fu2);

    VMOVE(line.r_pt, is->on_eg->e_pt);			/* 3D line */
    VMOVE(line.r_dir, is->on_eg->e_dir);
    VUNITIZE(line.r_dir);
    VINVDIR(invdir, line.r_dir);

    /* nmg_loop_g() makes sure there are no 0-thickness faces */
    if (!rt_in_rpp(&line, invdir, fu2->f_p->min_pt, fu2->f_p->max_pt)) {
	/* The edge ray missed the face RPP, nothing to do. */
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    VPRINT("r_pt ", line.r_pt);
	    VPRINT("r_dir", line.r_dir);
	    VPRINT("fu2 min", fu2->f_p->min_pt);
	    VPRINT("fu2 max", fu2->f_p->max_pt);
	    bu_log("r_min=%g, r_max=%g\n", line.r_min, line.r_max);
	    bu_log("nmg_isect_construct_nice_ray() edge ray missed face bounding RPP, ret=1\n");
	}
	return 1;	/* Missed */
    }
    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	VPRINT("fu2 min", fu2->f_p->min_pt);
	VPRINT("fu2 max", fu2->f_p->max_pt);
	bu_log("r_min=%g, r_max=%g\n", line.r_min, line.r_max);
    }
    /* Start point will lie at min or max dist, outside of face RPP */
    VJOIN1(is->pt, line.r_pt, line.r_min, line.r_dir);
    if (line.r_min > line.r_max) {
	/* Direction is heading the wrong way, flip it */
	VREVERSE(is->dir, line.r_dir);
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("flipping dir\n");
    } else {
	VMOVE(is->dir, line.r_dir);
    }
    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	VPRINT("r_pt ", line.r_pt);
	VPRINT("r_dir", line.r_dir);
	VPRINT("->pt ", is->pt);
	VPRINT("->dir", is->dir);
	bu_log("nmg_isect_construct_nice_ray() ret=0\n");
    }
    return 0;
}


/**
 * N M G _ I S E C T _ E D G E 2 P _ F A C E 2 P
 *
 * Given one (2D) edge (eu1) lying in the plane of another face (fu2),
 * intersect with all the other edges of that face.
 * The line of intersection is defined by the geometry of this edgeuse.
 * Therefore, all edgeuses in fu1 which share edge geometry are,
 * by definition, ON the intersection line.  We process all edgeuses
 * which share geometry at once, followed by cutjoin operation.
 * It is up to the caller not to recall for the other edgeuses of this edge_g.
 *
 * XXX eu1 may be a wire edge, in which case there is no fu1 face!
 *
 * Note that this routine completely conducts the
 * intersection operation, so that edges may come and go, loops
 * may join or split, each time it is called.
 * This imposes special requirements on handling the march through
 * the linked lists in this routine.
 *
 * This also means that much of argument "is" is changed each call.
 *
 * It further means that vu's in lone lu's found along the edge
 * "intersection line" here may get merged in, causing the lu to
 * be killed, and the vu, which is listed in the 3D (calling)
 * routine's l1/l2 list, is now invalid.
 *
 * NOTE-
 * Since this routine calls the face cutter, *all* points of intersection
 * along the line, for *both* faces, need to be found.
 * Otherwise, the parity requirements of the face cutter will be violated.
 * This means that eu1 needs to be intersected with all of fu1 also,
 * including itself (so that the vu's at the ends of eu1 are listed).
 *
 * Returns -
 * 0 Topology is completely shared (or no sharing).  l1/l2 valid.
 * >0 Caller needs to invalidate his l1/l2 list.
 */
static int
nmg_isect_edge2p_face2p(struct nmg_inter_struct *is, struct edgeuse *eu1, struct faceuse *fu2, struct faceuse *fu1)

/* edge to be intersected w/fu2 */
/* face to be intersected w/eu1 */
/* fu that eu1 is from */
{
    struct bu_ptbl vert_list1, vert_list2;
    fastf_t *mag1,	    *mag2;
    struct vertexuse *vu1;
    struct vertexuse *vu2;
    struct edgeuse *fu2_eu;	/* use of edge in fu2 */
    int total_splits = 0;
    int ret = 0;
    struct bu_ptbl eu1_list;
    struct bu_ptbl eu2_list;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_EDGEUSE(eu1);
    NMG_CK_FACEUSE(fu2);
    if (fu1) NMG_CK_FACEUSE(fu1);	 /* fu1 may be null */

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_edge2p_face2p(eu1=x%x, fu2=x%x, fu1=x%x) START\n", eu1, fu2, fu1);

    if (fu2->orientation != OT_SAME) bu_bomb("nmg_isect_edge2p_face2p() fu2 not OT_SAME\n");
    if (fu1 && fu1->orientation != OT_SAME) bu_bomb("nmg_isect_edge2p_face2p() fu1 not OT_SAME\n");

    mag1 = (fastf_t *)NULL;
    mag2 = (fastf_t *)NULL;

    /* See if an edge exists in other face that connects these 2 verts */
    fu2_eu = nmg_find_eu_in_face(eu1->vu_p->v_p, eu1->eumate_p->vu_p->v_p,
				 fu2, (const struct edgeuse *)NULL, 0);
    if (fu2_eu != (struct edgeuse *)NULL) {
	/* There is an edge in other face that joins these 2 verts. */
	NMG_CK_EDGEUSE(fu2_eu);
	if (fu2_eu->e_p != eu1->e_p) {
	    /* Not the same edge, fuse! */
	    bu_log("nmg_isect_edge2p_face2p() fusing unshared shared edge\n");
	    nmg_radial_join_eu(eu1, fu2_eu, &is->tol);
	}
	/* Topology is completely shared */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("nmg_isect_edge2p_face2p() topology is shared\n");
	ret = 0;
	goto do_ret;
    }

    bu_ptbl_init(&vert_list1, 64, "vert_list1 buffer");
    bu_ptbl_init(&vert_list2, 64, "vert_list1 buffer");
    bu_ptbl_init(&eu1_list, 64, "eu1_list1 buffer");
    bu_ptbl_init(&eu2_list, 64, "eu2_list1 buffer");

    NMG_CK_EDGE_G_LSEG(eu1->g.lseg_p);
    is->on_eg = eu1->g.lseg_p;
    is->l1 = &vert_list1;
    is->l2 = &vert_list2;
    is->s1 = nmg_find_s_of_eu(eu1);		/* may be wire edge */
    is->s2 = fu2->s_p;
    is->fu1 = fu1;
    is->fu2 = fu2;

    if (fu1 && rt_g.NMG_debug & (DEBUG_POLYSECT|DEBUG_FCUT|DEBUG_MESH)
	&& rt_g.NMG_debug & DEBUG_PLOTEM) {
	nmg_pl_2fu("Iface%d.pl", fu2, fu1, 0);
    }

    vu1 = eu1->vu_p;
    vu2 = BU_LIST_PNEXT_CIRC(edgeuse, eu1)->vu_p;
    if (vu1->v_p == vu2->v_p) {
	bu_log("nmg_isect_edge2p_face2p(eu1=x%x) skipping 0-len edge (topology)\n", eu1);
	/* Call nmg_k0eu() ? */
	goto out;
    }

    /*
     * Construct the ray which contains the line of intersection,
     * i.e. the line that contains the edge "eu1" (is->on_eg).
     */
    if (nmg_isect_construct_nice_ray(is, fu2)) goto out;

    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_fu_touchingloops(fu2);
	if (fu1)nmg_fu_touchingloops(fu1);
	nmg_region_v_unique(is->s1->r_p, &is->tol);
	nmg_region_v_unique(is->s2->r_p, &is->tol);
    }

    /* Build list of all edgeuses in eu1/fu1 and fu2 */
    if (fu1) {
	nmg_edgeuse_tabulate(&eu1_list, &fu1->l.magic);
    } else {
	nmg_edgeuse_tabulate(&eu1_list, &eu1->l.magic);
    }
    nmg_edgeuse_tabulate(&eu2_list, &fu2->l.magic);

    is->mag_len = 2 * (BU_PTBL_END(&eu1_list) + BU_PTBL_END(&eu2_list));
    mag1 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "mag1");
    mag2 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "mag2");

    is->mag1 = mag1;
    is->mag2 = mag2;

    /* Run infinite line containing eu1 through fu2 */
    total_splits = 1;
    nmg_isect_line2_face2pNEW(is, fu2, fu1, &eu2_list, &eu1_list);

    /* If eu1 is a wire, there is no fu1 to run line through. */
    if (fu1) {
	/* We are intersecting with ourself */
	nmg_isect_line2_face2pNEW(is, fu1, fu2, &eu1_list, &eu2_list);
    }
    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_edge2p_face2p(): total_splits=%d\n", total_splits);

    if (total_splits <= 0) goto out;

    if (rt_g.NMG_debug & DEBUG_FCUT) {
	bu_log("nmg_isect_edge2p_face2p(eu1=x%x, fu2=x%x) vert_lists C:\n", eu1, fu2);
	nmg_pr_ptbl_vert_list("vert_list1", &vert_list1, mag1);
	nmg_pr_ptbl_vert_list("vert_list2", &vert_list2, mag2);
    }

    if (rt_g.NMG_debug & DEBUG_FCUT) {
	bu_log("nmg_isect_edge2p_face2p(eu1=x%x, fu2=x%x) vert_lists D:\n", eu1, fu2);
	nmg_pr_ptbl_vert_list("vert_list1", &vert_list1, mag1);
	nmg_pr_ptbl_vert_list("vert_list2", &vert_list2, mag2);
    }

    if (vert_list1.end == 0 && vert_list2.end == 0) goto out;

    /* Invoke the face cutter to snip and join loops along isect line */
    is->on_eg = nmg_face_cutjoin(&vert_list1, &vert_list2, mag1, mag2, fu1, fu2, is->pt, is->dir, is->on_eg, &is->tol);
    ret = 1;		/* face cutter was called. */

out:
    (void)bu_ptbl_free(&vert_list1);
    (void)bu_ptbl_free(&vert_list2);
    (void)bu_ptbl_free(&eu1_list);
    (void)bu_ptbl_free(&eu2_list);
    if (mag1)
	bu_free((char *)mag1, "nmg_isect_edge2p_face2p: mag1");
    if (mag2)
	bu_free((char *)mag2, "nmg_isect_edge2p_face2p: mag2");

do_ret:
    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_edge2p_face2p(eu1=x%x, fu2=x%x) ret=%d\n",
	       eu1, fu2, ret);
    }
    return ret;
}


/*
 */
void
nmg_enlist_one_vu(struct nmg_inter_struct *is, const struct vertexuse *vu, fastf_t dist)


/* distance along intersect ray for this vu */
{
    struct shell *sv;		/* shell of vu */

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_VERTEXUSE(vu);

    if (is->mag_len <= BU_PTBL_END(is->l1) || is->mag_len <= BU_PTBL_END(is->l2))
	bu_log("Array for distances to vertexuses is too small (%d)\n", is->mag_len);

    sv = nmg_find_s_of_vu(vu);

    /* First step:  add vu to corresponding list */
    if (sv == is->s1) {
	bu_ptbl_ins_unique(is->l1, (long *)&vu->l.magic);
	if (is->mag_len <= BU_PTBL_END(is->l1)) {
	    if (is->mag_len) {
		is->mag_len *= 2;
		is->mag1 = (fastf_t *)bu_realloc((char *)is->mag1, is->mag_len*sizeof(fastf_t),
						 "is->mag1");
		is->mag2 = (fastf_t *)bu_realloc((char *)is->mag2, is->mag_len*sizeof(fastf_t),
						 "is->mag2");
	    } else {
		is->mag_len = 2*(BU_PTBL_END(is->l1) + BU_PTBL_END(is->l2));
		is->mag1 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "is->mag1");
		is->mag2 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "is->mag2");
	    }

	}
	if (dist < MAX_FASTF)
	    is->mag1[bu_ptbl_locate(is->l1, (long *)&vu->l.magic)] = dist;
    } else if (sv == is->s2) {
	bu_ptbl_ins_unique(is->l2, (long *)&vu->l.magic);
	if (is->mag_len <= BU_PTBL_END(is->l2)) {
	    if (is->mag_len) {
		is->mag_len *= 2;
		is->mag1 = (fastf_t *)bu_realloc((char *)is->mag1, is->mag_len*sizeof(fastf_t),
						 "is->mag1");
		is->mag2 = (fastf_t *)bu_realloc((char *)is->mag2, is->mag_len*sizeof(fastf_t),
						 "is->mag2");
	    } else {
		is->mag_len = 2*(BU_PTBL_END(is->l1) + BU_PTBL_END(is->l2));
		is->mag1 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "is->mag1");
		is->mag2 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "is->mag2");
	    }

	}
	if (dist < MAX_FASTF)
	    is->mag2[bu_ptbl_locate(is->l2, (long *)&vu->l.magic)] = dist;
    } else {
	bu_log("nmg_enlist_one_vu(vu=x%x) sv=x%x, s1=x%x, s2=x%x\n",
	       vu, sv, is->s1, is->s2);
	bu_bomb("nmg_enlist_one_vu: vu is not in s1 or s2\n");
    }

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_enlist_one_vu(vu=x%x) v=x%x, dist=%g (%s)\n",
	       vu, vu->v_p, dist,
	       (sv == is->s1) ? "shell 1" : "shell 2");
    }

    /* Some (expensive) centralized sanity checking */
    if ((rt_g.NMG_debug & DEBUG_VERIFY) && is->fu1 && is->fu2) {
	nmg_ck_v_in_2fus(vu->v_p, is->fu1, is->fu2, &(is->tol));
    }
}


static void
nmg_coplanar_face_vertex_fuse(struct faceuse *fu1, struct faceuse *fu2, struct bn_tol *tol)
{
    struct bu_ptbl fu1_verts;
    struct bu_ptbl fu2_verts;
    int i, j;
    vect_t norm;

    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACEUSE(fu2);
    BN_CK_TOL(tol);

    NMG_GET_FU_NORMAL(norm, fu1);

    nmg_vertex_tabulate(&fu1_verts, &fu1->l.magic);
    nmg_vertex_tabulate(&fu2_verts, &fu2->l.magic);

    for (i=0; i<BU_PTBL_END(&fu1_verts); i++) {
	struct vertex *v1;

	v1 = (struct vertex *)BU_PTBL_GET(&fu1_verts, i);

	for (j=0; j<BU_PTBL_END(&fu2_verts); j++) {
	    struct vertex *v2;
	    vect_t diff;
	    vect_t diff_unit;
	    fastf_t len_sq, inv_len;
	    fastf_t dot;

	    v2 = (struct vertex *)BU_PTBL_GET(&fu2_verts, j);

	    if (v1 == v2)
		continue;

	    VSUB2(diff, v1->vg_p->coord, v2->vg_p->coord);
	    len_sq = MAGSQ(diff);
	    if (len_sq > 4.0*tol->dist_sq)
		continue;

	    inv_len = 1.0 / sqrt(len_sq);

	    VSCALE(diff_unit, diff, inv_len);

	    dot = VDOT(norm, diff_unit);
	    if (BN_VECT_ARE_PARALLEL(dot, tol)) {
		/* fuse these two vertices */
		nmg_jv(v2, v1);
		break;
	    }
	}
    }
}


static void
nmg_isect_two_face2p_jra(struct nmg_inter_struct *is, struct faceuse *fu1, struct faceuse *fu2)
{
    struct model *m;
    struct loopuse *lu;
    struct bu_ptbl eu1_list;
    struct bu_ptbl eu2_list;
    struct bu_ptbl v_list;
    struct bu_ptbl vert_list1, vert_list2;
    fastf_t *mag1, *mag2;
    int i, j;
    const fastf_t opsff = (1.0 + SMALL_FASTF); 
    const fastf_t omsff = (1.0 - SMALL_FASTF); 

    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACEUSE(fu2);
    NMG_CK_INTER_STRUCT(is);

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_two)face2p_jra: fu1=x%x, fu2=x%x\n", fu1, fu2);

    nmg_coplanar_face_vertex_fuse(fu1, fu2, &is->tol);

    m = nmg_find_model(&fu1->l.magic);
    NMG_CK_MODEL(m);

    nmg_edgeuse_tabulate(&eu1_list, &fu1->l.magic);
    nmg_edgeuse_tabulate(&eu2_list, &fu2->l.magic);

    is->mag_len = 2 * (BU_PTBL_END(&eu1_list) + BU_PTBL_END(&eu2_list));
    mag1 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "mag1");
    mag2 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "mag2");

    for (i=0; i<is->mag_len; i++) {
	mag1[i] = MAX_FASTF;
	mag2[i] = MAX_FASTF;
    }


    is->s1 = fu1->s_p;
    is->s2 = fu2->s_p;
    is->fu1 = fu1;
    is->fu2 = fu2;
    is->mag1 = mag1;
    is->mag2 = mag2;

    /* First split all edgeuses that intersect */
    for (i=0; i<BU_PTBL_END(&eu1_list); i++) {
	struct edgeuse *eu1;
	struct vertex_g *vg1a, *vg1b;
	vect_t vt1_3d;

	eu1 = (struct edgeuse *)BU_PTBL_GET(&eu1_list, i);
	NMG_CK_EDGEUSE(eu1);

	vg1a = eu1->vu_p->v_p->vg_p;
	NMG_CK_VERTEX_G(vg1a);
	vg1b = eu1->eumate_p->vu_p->v_p->vg_p;
	NMG_CK_VERTEX_G(vg1b);

	VSUB2(vt1_3d, vg1b->coord, vg1a->coord);

	for (j=0; j<BU_PTBL_END(&eu2_list); j++) {
	    struct edgeuse *eu2 = NULL;
	    struct vertex_g *vg2a = NULL;
	    struct vertex_g *vg2b = NULL;
	    int code = 0;
	    vect_t vt2_3d = VINIT_ZERO;
	    fastf_t dist[2] = V2INIT_ZERO;
	    point_t hit_pt = VINIT_ZERO;
	    int hit_no = 0;
	    int hit_count = 0;

	    eu2 = (struct edgeuse *)BU_PTBL_GET(&eu2_list, j);
	    NMG_CK_EDGEUSE(eu2);

	    vg2a = eu2->vu_p->v_p->vg_p;
	    vg2b = eu2->eumate_p->vu_p->v_p->vg_p;
	    VSUB2(vt2_3d, vg2b->coord, vg2a->coord);

	    code = bn_isect_lseg3_lseg3(dist, vg1a->coord, vt1_3d,
					vg2a->coord, vt2_3d, &is->tol);

	    if (code < 0)
		continue;

	    if (code == 0) {
		/* When there is only one hit, place the distance in
		 * dist[0] otherwise use both dist[0] and dist[1].
		 * In the description below the line p0->p1 is eu1 where
		 * p0 is the start of eu1 and p1 is the end of eu1.
		 * The same is done for eu2 with line q0->q1. 
		 * When eu1 and eu2 are colinear, the value of dist[0]
		 * returned from function 'bn_isect_lseg3_lseg3' is the
		 * scaled distance from p0->q0 and dist[1] is the scaled
		 * distance from p0->q1.
		 */
		if ((dist[0] < -SMALL_FASTF && ((dist[1] > SMALL_FASTF) &&
		   (dist[1] < omsff))) ||
		   ((dist[1] > SMALL_FASTF) && (dist[1] < omsff) 
		   && (dist[0] > opsff))) {
		    /* true when q1 is within p0->p1 */
		    hit_count = 1;
		    dist[0] = dist[1];
		    dist[1] = MAX_FASTF; /* sanity */
		} else if ((dist[0] > SMALL_FASTF) && (dist[0] < omsff) &&
			   (dist[1] > SMALL_FASTF) && (dist[1] < omsff)) {
		    /* true when both q0 and q1 is within p0->p1 */
		    hit_count = 2;
		    /* dist[0] and dist[1] are already the correct values */
		} else if (((dist[0] > 0) && (dist[0] < 1) && (dist[1] < 0)) ||
			   ((dist[0] > 0) && (dist[0] < 1) && (dist[1] > 1))) {
		    /* true when q0 is within p0->p1 */
		    hit_count = 1;
		    dist[1] = MAX_FASTF; /* sanity */
		    /* dist[0] is already the correct value */
		} else if (((dist[0] < -SMALL_FASTF) && (dist[1] > opsff)) || 
			   ((dist[0] > opsff) && (dist[1] < -SMALL_FASTF))) {
		    /* true when both p0 and p1 is within q0->q1 */
		    /* eu1 is not cut */
		    hit_count = 0; /* sanity */
		    dist[0] = dist[1] = MAX_FASTF; /* sanity */
		    continue;

		} else if ((ZERO(dist[0]) && ZERO(dist[1] - 1.0)) ||
			   (ZERO(dist[1]) && ZERO(dist[0] - 1.0))) {
		    /* true when eu1 and eu2 shared the same vertices */
		    /* eu1 is not cut */
		    hit_count = 0; /* sanity */
		    dist[0] = dist[1] = MAX_FASTF; /* sanity */
		    continue;
		} else if (((dist[0] < -SMALL_FASTF) && ZERO(dist[1])) ||
			   (ZERO(dist[0]) && (dist[1] < -SMALL_FASTF)) ||
			   (ZERO(dist[0] - 1.0) && (dist[1] > opsff)) ||
			   (ZERO(dist[1] - 1.0) && (dist[0] > opsff))) {
		    /* true when eu2 shares one of eu1 vertices and the
		     * other eu2 vertex is outside p0->p1 (i.e. eu1)
		     */
		    /* eu1 is not cut */
		    hit_count = 0; /* sanity */
		    dist[0] = dist[1] = MAX_FASTF; /* sanity */
		    continue;
		} else if ((ZERO(dist[0]) && (dist[1] > SMALL_FASTF) && 
			    (dist[1] < omsff)) || 
			   ((dist[1] > SMALL_FASTF) && 
			    (dist[1] < omsff) && 
			    ZERO(dist[0] - 1.0))) {
		    /* true when q1 is within p0->p1 and q0 = p0 or q0 = p1 */
		    hit_count = 1;
		    dist[0] = dist[1];
		    dist[1] = MAX_FASTF; /* sanity */
		} else if ((ZERO(dist[1]) && (dist[0] > SMALL_FASTF) && 
			   (dist[0] < omsff)) || 
			  ((dist[0] > SMALL_FASTF) && 
			   (dist[0] < omsff) && 
			   ZERO(dist[1] - 1.0))) {
		    /* true when q0 is within p0->p1 and q1 = p0 or q1 = p1 */
		    hit_count = 1;
		    dist[1] = MAX_FASTF; /* sanity */
		    /* dist[0] is already the correct value */
		} else if ((ZERO(dist[0]) && (dist[1] > opsff)) ||
			   (ZERO(dist[1]) && (dist[0] > opsff)) ||
			   (ZERO(dist[0] - 1.0) && (dist[1] < -SMALL_FASTF)) ||
			  ((dist[0] < -SMALL_FASTF) && ZERO(dist[1] - 1.0))) {
		    /* true when eu2 shares one of the vertices of eu1 and
		     * the other vertex in eu2 is on the far side of eu1 
		     * outside eu1 (i.e. p0->p1).
		     */
		    /* eu1 is not cut */
		    hit_count = 0; /* sanity */
		    dist[0] = dist[1] = MAX_FASTF; /* sanity */
		    continue;
		} else {
		    /* should never get here */
		    bu_log("nmg_isect_two_face2p_jra(): dist[0] = %f dist[1] = %f\n", 
			    dist[0], dist[1]);
		    bu_bomb("nmg_isect_two_face2p_jra(): unexpected condition\n");
		}
	    } else {
		if (ZERO(dist[0]) || ZERO(dist[0] - 1.0)) {
		    /* eu1 was hit on a vertex, nothing to cut */
		    continue;
	        } else if ((dist[0] < -SMALL_FASTF) || (dist[0] > opsff)) {
		    bu_bomb("nmg_isect_two_face2p_jra(): dist[0] not within 0-1\n");
		} else {
		    hit_count = 1;
		}
	    }

	    for (hit_no=0; hit_no < hit_count; hit_no++) {
		struct edgeuse *new_eu;
		struct vertex *hitv;
		struct vertexuse *hit_vu = NULL;

		if (dist[hit_no] < -SMALL_FASTF || dist[hit_no] > opsff)
		    continue;

		hitv = (struct vertex *)NULL;

		if (ZERO(dist[hit_no])) {
		    hit_vu = eu1->vu_p;
		    hitv = hit_vu->v_p;
		    VMOVE(hit_pt, hitv->vg_p->coord);
		} else if (ZERO(dist[hit_no] - 1.0)) {
		    hit_vu = eu1->eumate_p->vu_p;
		    hitv = hit_vu->v_p;
		    VMOVE(hit_pt, hitv->vg_p->coord);
		} else
		    VJOIN1(hit_pt, vg1a->coord, dist[hit_no], vt1_3d)

			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    bu_log("eus x%x and x%x intersect #%d at (%f %f %f)\n",
				   eu1, eu2, hit_no, V3ARGS(hit_pt));

		if (!hit_vu)
		    hit_vu = nmg_find_pt_in_face(fu2, hit_pt, &is->tol);

		if (!hit_vu)
		    hitv = nmg_find_pt_in_model(nmg_find_model(&fu1->l.magic), hit_pt, &is->tol);
#if 1
		if (!hit_vu && (code == 0)) {
		    bu_log("dist[0] = %f dist[1] = %f hit_no = %d\n", dist[0], dist[1], hit_no);
		    bu_bomb("nmg_isect_two_face2p_jra(): why can we not find a vertexuse\n");
		}
#endif
		if (rt_g.NMG_debug & DEBUG_POLYSECT && hitv)
		    bu_log("Found vertex (x%x) at hit_pt\n", hitv);

		if (hitv != eu1->vu_p->v_p && hitv != eu1->eumate_p->vu_p->v_p) {
		    struct edgeuse *next_eu, *prev_eu;

		    next_eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu1->l);
		    prev_eu = BU_LIST_PPREV_CIRC(edgeuse, &eu1->l);

		    if (hitv != prev_eu->vu_p->v_p && hitv != next_eu->eumate_p->vu_p->v_p) {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    bu_log("Splitting eu1 x%x\n", eu1);
			new_eu = nmg_esplit(hitv, eu1, 1);
			hitv = new_eu->vu_p->v_p;
			if (!hitv->vg_p)
			    nmg_vertex_gv(hitv, hit_pt);
			vg1b = eu1->eumate_p->vu_p->v_p->vg_p;
			VSUB2(vt1_3d, vg1b->coord, vg1a->coord);
		    }
		}
		if (code == 1 && hitv != eu2->vu_p->v_p && hitv != eu2->eumate_p->vu_p->v_p) {
		    struct edgeuse *next_eu, *prev_eu;

		    next_eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu2->l);
		    prev_eu = BU_LIST_PPREV_CIRC(edgeuse, &eu2->l);

		    if (hitv != prev_eu->vu_p->v_p && hitv != next_eu->eumate_p->vu_p->v_p) {
			if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			    vect_t tmp1, tmp2;
			    VSUB2(tmp1, hit_pt, eu2->vu_p->v_p->vg_p->coord)
				VSUB2(tmp2, hit_pt, eu2->eumate_p->vu_p->v_p->vg_p->coord)
				bu_log("Splitting eu2 x%x\n",  eu2);
			    bu_log("Distance to hit_pt = %g from vu1, %g from vu2\n",
				   MAGNITUDE(tmp1), MAGNITUDE(tmp2));
			}
			new_eu = nmg_esplit(hitv, eu2, 1);
			hitv = new_eu->vu_p->v_p;
			if (!hitv->vg_p)
			    nmg_vertex_gv(hitv, hit_pt);
			bu_ptbl_ins(&eu2_list, (long *)new_eu);
		    }
		}

		if (hitv)
		    (void)nmg_break_all_es_on_v(&m->magic, hitv, &is->tol);
	    }
	}
    }

    bu_ptbl_free(&eu1_list);
    bu_ptbl_free(&eu2_list);

    /* Make sure every vertex in fu1 has dual in fu2
     * (if they overlap)
     */
    nmg_vertex_tabulate(&v_list, &fu1->l.magic);

    for (i=0; i<BU_PTBL_END(&v_list); i++) {
	struct vertex *v;
	int class;

	v = (struct vertex *)BU_PTBL_GET(&v_list, i);
	NMG_CK_VERTEX(v);

	if (nmg_find_v_in_face(v, fu2))
	    continue;

	/* Check if this vertex is within other FU */
	class = nmg_class_pt_fu_except(v->vg_p->coord, fu2, NULL, NULL, NULL,
				       (char *)NULL, 0, 0, &is->tol);

	if (class == NMG_CLASS_AinB) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("Making dualvu of vertex x%x in fu2 x%x\n", v, fu2);
	    (void)nmg_make_dualvu(v, fu2, &is->tol);
	}
    }
    bu_ptbl_reset(&v_list);

    /* same for fu2 */
    nmg_vertex_tabulate(&v_list, &fu2->l.magic);

    for (i=0; i<BU_PTBL_END(&v_list); i++) {
	struct vertex *v;
	int class;

	v = (struct vertex *)BU_PTBL_GET(&v_list, i);
	NMG_CK_VERTEX(v);

	if (nmg_find_v_in_face(v, fu1))
	    continue;

	/* Check if this vertex is within other FU */
	class = nmg_class_pt_fu_except(v->vg_p->coord, fu1, NULL, NULL, NULL,
				       (char *)NULL, 0, 0, &is->tol);

	if (class == NMG_CLASS_AinB) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("Making dualvu of vertex x%x in fu1 x%x\n", v, fu1);
	    (void)nmg_make_dualvu(v, fu1, &is->tol);
	}
    }

    bu_ptbl_free(&v_list);

    bu_ptbl_init(&vert_list1, 64, " &vert_list1");
    bu_ptbl_init(&vert_list2, 64, " &vert_list2");
    is->l1 = &vert_list1;
    is->l2 = &vert_list2;

    for (BU_LIST_FOR(lu, loopuse, &fu1->lu_hd)) {
	struct edgeuse *eu;

	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    struct vertexuse *vu;
	    struct vertex *v1, *v2;

	    NMG_CK_EDGEUSE(eu);

	    v1 = eu->vu_p->v_p;
	    NMG_CK_VERTEX(v1);
	    v2 = eu->eumate_p->vu_p->v_p;
	    NMG_CK_VERTEX(v2);

	    if (!nmg_find_v_in_face(v1, fu2) ||
		!nmg_find_v_in_face(v2, fu2))
		continue;

	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("Making EU x%x an intersect line for face cutting\n", eu);

	    for (BU_LIST_FOR(vu, vertexuse, &v1->vu_hd)) {
		struct faceuse *fu;

		fu = nmg_find_fu_of_vu(vu);

		if (fu == fu2)
		    nmg_enlist_one_vu(is, vu, 0.0);
	    }

	    for (BU_LIST_FOR(vu, vertexuse, &v2->vu_hd)) {
		struct faceuse *fu;

		fu = nmg_find_fu_of_vu(vu);

		if (fu == fu2)
		    nmg_enlist_one_vu(is, vu, 1.0);
	    }

	    /* Now do face cutting */
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("Calling face cutter for fu2 x%x\n", fu2);
	    nmg_fcut_face_2d(is->l2, is->mag2, fu2, fu1, &is->tol);

	    bu_ptbl_reset(is->l1);
	    bu_ptbl_reset(is->l2);
	}
    }

    for (BU_LIST_FOR(lu, loopuse, &fu2->lu_hd)) {
	struct edgeuse *eu;

	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    struct vertexuse *vu;
	    struct vertex *v1, *v2;

	    NMG_CK_EDGEUSE(eu);

	    v1 = eu->vu_p->v_p;
	    NMG_CK_VERTEX(v1);
	    v2 = eu->eumate_p->vu_p->v_p;
	    NMG_CK_VERTEX(v2);

	    if (!nmg_find_v_in_face(v1, fu1) ||
		!nmg_find_v_in_face(v2, fu1))
		continue;

	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("Making EU x%x an intersect line for face cutting\n", eu);

	    for (BU_LIST_FOR(vu, vertexuse, &v1->vu_hd)) {
		struct faceuse *fu;

		fu = nmg_find_fu_of_vu(vu);

		if (fu == fu1)
		    nmg_enlist_one_vu(is, vu, 0.0);
	    }

	    for (BU_LIST_FOR(vu, vertexuse, &v2->vu_hd)) {
		struct faceuse *fu;

		fu = nmg_find_fu_of_vu(vu);

		if (fu == fu1)
		    nmg_enlist_one_vu(is, vu, 1.0);
	    }

	    /* Now do face cutting */
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("Calling face cutter for fu1 x%x\n", fu1);
	    nmg_fcut_face_2d(is->l1, is->mag1, fu1, fu2, &is->tol);

	    bu_ptbl_reset(is->l1);
	    bu_ptbl_reset(is->l2);
	}
    }
    if (mag1)
	bu_free((char *)mag1, "mag1");
    if (mag2)
	bu_free((char *)mag2, "mag2");

    bu_ptbl_free(is->l1);
    bu_ptbl_free(is->l2);
}


/**
 * N M G _ I S E C T _ T W O _ F A C E 2 P
 *
 * Manage the mutual intersection of two 3-D coplanar planar faces.
 *
 * The big challenge in this routine comes from the fact that
 * loopuses can come and go as the facecutter operates.
 * Thus, after a call to nmg_isect_edge2p_face2p(), the current
 * loopuse structure may be invalid.
 * The intersection operations being performed here never delete
 * edgeuses, only split existing ones and add new ones.
 * It might reduce complexity to unbreak edges in here, but that
 * would violate the assumption of edgeuses not vanishing.
 *
 * Called by -
 * nmg_isect_two_generic_faces()
 *
 * Call tree -
 * nmg_isect_vert2p_face2p()
 * nmg_isect_edge2p_face2p()
 *		nmg_isect_vert2p_face2p()
 *		nmg_isect_line2_face2p()
 *		nmg_purge_unwanted_intersection_points()
 *		nmg_face_cutjoin()
 */
#if 0
static void
nmg_isect_two_face2p(is, fu1, fu2)
    struct nmg_inter_struct *is;
    struct faceuse *fu1, *fu2;
{
    struct model *m;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct vertexuse *vu;
    unsigned char *tags;
    int tagsize;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACEUSE(fu2);
    m = fu1->s_p->r_p->m_p;
    NMG_CK_MODEL(m);

    is->l1 = 0;
    is->l2 = 0;
    is->fu1 = fu1;
    is->fu2 = fu2;

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_two_face2p(fu1=x%x, fu2=x%x) START\n", fu1, fu2);

    /* Allocate map of edgegeom's visited */
    tagsize = 4 * m->maxindex+1;
    tags = (unsigned char *)bu_calloc(tagsize, 1, "nmg_isect_two_face2p() tags[]");

/* XXX A vastly better strategy would be to build a list of vu's and eu's,
 * XXX and then intersect them with the other face.
 * XXX loopuses can come and go as loops get cutjoin'ed, but at this
 * XXX stage edgeuses are created, but never deleted.
 * XXX This way, the process should converge in 2 interations, rather than N.
 */

    /* For every edge in f1, intersect with f2, incl. cutjoin */
    memset((char *)tags, 0, tagsize);
f1_again:
    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_fu_touchingloops(fu1);
	nmg_fu_touchingloops(fu2);
	nmg_region_v_unique(fu1->s_p->r_p, &is->tol);
	nmg_region_v_unique(fu2->s_p->r_p, &is->tol);
    }
    for (BU_LIST_FOR(lu, loopuse, &fu1->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	    is->l1 = 0;
	    is->l2 = 0;
	    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	    if (!NMG_INDEX_FIRST_TIME(tags, vu->v_p)) continue;
	    nmg_isect_vert2p_face2p(is, vu, fu2);
	    continue;
	}
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    struct edge_g_lseg *eg;

	    NMG_CK_EDGEUSE(eu);
	    eg = eu->g.lseg_p;
	    /* If this eu's eg has been seen before, skip on. */
	    if (eg && !NMG_INDEX_FIRST_TIME(tags, eg)) continue;

	    if (nmg_isect_edge2p_face2p(is, eu, fu2, fu1)) {
		/* Face topologies have changed */
		/* This loop might have been joined into another loopuse! */
		/* XXX Might want to unbreak edges? */
		goto f1_again;
	    }
	}
    }

    /* Zap 2d cache, we are switching faces now */
    nmg_isect2d_cleanup(is);

    /* For every edge in f2, intersect with f1, incl. cutjoin */
    memset((char *)tags, 0, tagsize);
f2_again:
    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_fu_touchingloops(fu1);
	nmg_fu_touchingloops(fu2);
	nmg_region_v_unique(fu1->s_p->r_p, &is->tol);
	nmg_region_v_unique(fu2->s_p->r_p, &is->tol);
    }
    for (BU_LIST_FOR(lu, loopuse, &fu2->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	    is->l1 = 0;
	    is->l2 = 0;
	    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	    if (!NMG_INDEX_FIRST_TIME(tags, vu->v_p)) continue;
	    nmg_isect_vert2p_face2p(is, vu, fu1);
	    continue;
	}
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    struct edge_g_lseg *eg;

	    NMG_CK_EDGEUSE(eu);
	    eg = eu->g.lseg_p;
	    /* If this eu's eg has been seen before, skip on. */
	    if (eg && !NMG_INDEX_FIRST_TIME(tags, eg)) continue;

	    if (nmg_isect_edge2p_face2p(is, eu, fu1, fu2)) {
		/* Face topologies have changed */
		goto f2_again;
	    }
	}
    }
    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_fu_touchingloops(fu1);
	nmg_fu_touchingloops(fu2);
	nmg_region_v_unique(fu1->s_p->r_p, &is->tol);
	nmg_region_v_unique(fu2->s_p->r_p, &is->tol);
    }
    bu_free((char *)tags, "tags[]");
    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_two_face2p(fu1=x%x, fu2=x%x) END\n", fu1, fu2);
}
#endif
/**
 * N M G _ I S E C T _ L I N E 2 _ E D G E 2 P
 *
 * A parallel to nmg_isect_edge2p_edge2p().
 *
 * Intersect the line with eu1, from fu1.
 * The resulting vu's are added to "list", not is->l1 or is->l2.
 * fu2 is the "other" face on this intersect line, and is used only
 * when searching for existing vertex structs suitable for re-use.
 *
 * Returns -
 * Number of times edge is broken (0 or 1).
 */
int
nmg_isect_line2_edge2p(struct nmg_inter_struct *is, struct bu_ptbl *list, struct edgeuse *eu1, struct faceuse *fu1, struct faceuse *fu2)
{
    point_t eu1_start;	/* 2D */
    point_t eu1_end;	/* 2D */
    vect_t eu1_dir;	/* 2D */
    fastf_t dist[2];
    int status;
    point_t hit_pt;		/* 3D */
    struct vertexuse *vu1a, *vu1b;
    int ret = 0;

    NMG_CK_INTER_STRUCT(is);
    BU_CK_PTBL(list);
    NMG_CK_EDGEUSE(eu1);
    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACEUSE(fu2);

    /*
     * Important note:  don't use eu1->eumate_p->vu_p here,
     * because that vu is in the opposite orientation faceuse.
     * Putting those vu's on the intersection line makes for big trouble.
     */
    vu1a = eu1->vu_p;
    vu1b = BU_LIST_PNEXT_CIRC(edgeuse, eu1)->vu_p;
    NMG_CK_VERTEXUSE(vu1a);
    NMG_CK_VERTEXUSE(vu1b);

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_line2_edge2p(eu1=x%x, fu1=x%x)\n\tvu1a=%x vu1b=%x\n\tv2a=%x v2b=%x\n",
	       eu1, fu1,
	       vu1a, vu1b,
	       vu1a->v_p, vu1b->v_p);
    }

    /*
     * The 3D line in is->pt and is->dir is prepared by the caller.
     */
    nmg_get_2d_vertex(eu1_start, vu1a->v_p, is, &fu1->l.magic);
    nmg_get_2d_vertex(eu1_end, vu1b->v_p, is, &fu1->l.magic);
    VSUB2_2D(eu1_dir, eu1_end, eu1_start);

    dist[0] = dist[1] = 0;	/* for clean prints, below */

    /* Intersect the line with the edge, in 2D */
    status = bn_isect_line2_lseg2(dist, is->pt2d, is->dir2d,
				  eu1_start, eu1_dir, &is->tol);

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("\tbn_isect_line2_lseg2()=%d, dist: %g, %g\n",
	       status, dist[0], dist[1]);
    }

    if (status < 0) goto out;	/* No geometric intersection */

    if (status == 0) {
	/*
	 * The edge is colinear with the line.
	 * List both vertexuse structures, and return.
	 */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\t\tedge colinear with isect line.  Listing vu1a, vu1b\n");
	nmg_enlist_vu(is, vu1a, 0, MAX_FASTF);
	nmg_enlist_vu(is, vu1b, 0, MAX_FASTF);
	ret = 0;
	goto out;
    }

    /* There is only one intersect point.  Break the edge there. */

    VJOIN1(hit_pt, is->pt, dist[0], is->dir);	/* 3D hit */

    /* Edges not colinear. Either list a vertex,
     * or break eu1.
     */
    if (status == 1 || ZERO(dist[1])) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\t\tintersect point is vu1a\n");
	if (!bn_pt3_pt3_equal(hit_pt, vu1a->v_p->vg_p->coord, &(is->tol)))
	    bu_bomb("vu1a does not match calculated point\n");
	nmg_enlist_vu(is, vu1a, 0, MAX_FASTF);
	ret = 0;
    } else if (status == 2 || ZERO(dist[1] - 1.0)) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\t\tintersect point is vu1b\n");
	if (!bn_pt3_pt3_equal(hit_pt, vu1b->v_p->vg_p->coord, &(is->tol)))
	    bu_bomb("vu1b does not match calculated point\n");
	nmg_enlist_vu(is, vu1b, 0, MAX_FASTF);
	ret = 0;
    } else {
	/* Intersection is in the middle of eu1, split edge */
	fastf_t distance;
	struct vertexuse *vu1_final;
	struct vertex *new_v;
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    int code;
	    bu_log("\t2D: pt2d=(%g, %g), dir2d=(%g, %g)\n",
		   is->pt2d[X], is->pt2d[Y],
		   is->dir2d[X], is->dir2d[Y]);
	    bu_log("\t2D: eu1_start=(%g, %g), eu1_dir=(%g, %g)\n",
		   eu1_start[X], eu1_start[Y],
		   eu1_dir[X], eu1_dir[Y]);
	    VPRINT("\t3D: is->pt ", is->pt);
	    VPRINT("\t3D: is->dir", is->dir);
	    bu_log("\t2d: Breaking eu1 at isect.\n");
	    VPRINT(" vu1a", vu1a->v_p->vg_p->coord);
	    VPRINT("hit_pt", hit_pt);
	    VPRINT(" vu1b", vu1b->v_p->vg_p->coord);
	    /* XXX Perform a (not-so) quick check */
	    code = bn_isect_pt_lseg(&distance, vu1a->v_p->vg_p->coord,
				    vu1b->v_p->vg_p->coord,
				    hit_pt, &(is->tol));
	    bu_log("\tbn_isect_pt_lseg() dist=%g, ret=%d\n", distance, code);
	    if (code < 0) bu_bomb("3D point not on 3D lseg\n");

	    /* Ensure that the 3D hit_pt is between the end pts */
	    if (!bn_between(vu1a->v_p->vg_p->coord[X], hit_pt[X], vu1b->v_p->vg_p->coord[X], &(is->tol)) ||
		!bn_between(vu1a->v_p->vg_p->coord[Y], hit_pt[Y], vu1b->v_p->vg_p->coord[Y], &(is->tol)) ||
		!bn_between(vu1a->v_p->vg_p->coord[Z], hit_pt[Z], vu1b->v_p->vg_p->coord[Z], &(is->tol))) {
		VPRINT("vu1a", vu1a->v_p->vg_p->coord);
		VPRINT("hitp", hit_pt);
		VPRINT("vu1b", vu1b->v_p->vg_p->coord);
		bu_bomb("nmg_isect_line2_edge2p() hit point not between edge verts!\n");
	    }

	}

	/* if we can't find the appropriate vertex
	 * by a geometry search, build a new vertex.
	 * Otherwise, re-use the existing one.
	 * Can't just search other face, might miss relevant vert.
	 */
	new_v = nmg_find_pt_in_model(fu2->s_p->r_p->m_p, hit_pt, &(is->tol));
	vu1_final = nmg_ebreaker(new_v, eu1, &is->tol)->vu_p;
	ret = 1;
	if (!new_v) {
	    nmg_vertex_gv(vu1_final->v_p, hit_pt);	/* 3d geom */
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\t\tmaking new vertex vu=x%x v=x%x\n",
		       vu1_final, vu1_final->v_p);
	} else {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\t\tre-using vertex v=x%x vu=x%x\n", new_v, vu1_final);
	}
	nmg_enlist_vu(is, vu1_final, 0, MAX_FASTF);

	nmg_ck_face_worthless_edges(fu1);
    }

out:
    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_line2_edge2p(eu1=x%x, fu1=x%x) END ret=%d\n", eu1, fu1, ret);
    return ret;
}


/**
 * N M G _ I S E C T _ L I N E 2 _ V E R T E X 2
 *
 * If this lone vertex lies along the intersect line, then add it to
 * the lists.
 *
 * Called from nmg_isect_line2_face2p().
 */
void
nmg_isect_line2_vertex2(struct nmg_inter_struct *is, struct vertexuse *vu1, struct faceuse *fu1)
{

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_VERTEXUSE(vu1);
    NMG_CK_FACEUSE(fu1);

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_line2_vertex2(vu=x%x)\n", vu1);

    /* Needs to be a 3D comparison */
    if (bn_distsq_line3_pt3(is->pt, is->dir, vu1->v_p->vg_p->coord) > is->tol.dist_sq)
	return;

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_line2_vertex2(vu=x%x) line hits vertex v=x%x\n", vu1, vu1->v_p);

    nmg_enlist_vu(is, vu1, 0, MAX_FASTF);
}


/**
 *
 * Given two pointer tables filled with edgeuses representing two differentt
 * edge geometry lines, see if there is a common vertex of intersection.
 * If so, enlist the intersection.
 *
 * Returns -
 * 1 intersection found
 * 0 no intersection
 */
int
nmg_isect_two_ptbls(struct nmg_inter_struct *is, const struct bu_ptbl *t1, const struct bu_ptbl *t2)
{
    const struct edgeuse **eu1;
    const struct edgeuse **eu2;
    struct vertexuse *vu1a;
    struct vertexuse *vu1b;

    NMG_CK_INTER_STRUCT(is);
    BU_CK_PTBL(t1);

    for (eu1 = (const struct edgeuse **)BU_PTBL_LASTADDR(t1);
	 eu1 >= (const struct edgeuse **)BU_PTBL_BASEADDR(t1); eu1--
	) {
	struct vertex *v1a;
	struct vertex *v1b;

	vu1a = (*eu1)->vu_p;
	vu1b = BU_LIST_PNEXT_CIRC(edgeuse, (*eu1))->vu_p;
	NMG_CK_VERTEXUSE(vu1a);
	NMG_CK_VERTEXUSE(vu1b);
	v1a = vu1a->v_p;
	v1b = vu1b->v_p;

	for (eu2 = (const struct edgeuse **)BU_PTBL_LASTADDR(t2);
	     eu2 >= (const struct edgeuse **)BU_PTBL_BASEADDR(t2); eu2--
	    ) {
	    register struct vertexuse *vu2a;
	    register struct vertexuse *vu2b;

	    vu2a = (*eu2)->vu_p;
	    vu2b = BU_LIST_PNEXT_CIRC(edgeuse, (*eu2))->vu_p;
	    NMG_CK_VERTEXUSE(vu2a);
	    NMG_CK_VERTEXUSE(vu2b);

	    if (v1a == vu2a->v_p) {
		vu1b = vu2a;
		goto enlist;
	    }
	    if (v1a == vu2b->v_p) {
		vu1b = vu2b;
		goto enlist;
	    }
	    if (v1b == vu2a->v_p) {
		vu1a = vu1b;
		vu1b = vu2a;
		goto enlist;
	    }
	    if (v1b == vu2b->v_p) {
		vu1a = vu1b;
		vu1b = vu2b;
		goto enlist;
	    }
	}
    }
    return 0;
enlist:
    /* Two vu's are now vu1a, vu1b */
    if (nmg_find_s_of_vu(vu1a) == nmg_find_s_of_vu(vu1b)) {
	vu1b = 0;
    }

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_two_ptbls() intersection! vu=x%x, vu_dual=x%x\n",
	       vu1a, vu1b);
    }
    nmg_enlist_vu(is, vu1a, vu1b, MAX_FASTF);
    return 1;
}


/**
 * N M G _ F I N D _ E G _ O N _ L I N E
 *
 * Do a geometric search to find an edge_g_lseg on the given line.
 * If the fuser did its job, there should be only one.
 */
struct edge_g_lseg *
nmg_find_eg_on_line(const uint32_t *magic_p, const fastf_t *pt, const fastf_t *dir, const struct bn_tol *tol)
{
    struct bu_ptbl eutab;
    struct edgeuse **eup;
    struct edge_g_lseg *ret = (struct edge_g_lseg *)NULL;
    vect_t dir1, dir2;

    BN_CK_TOL(tol);

    nmg_edgeuse_on_line_tabulate(&eutab, magic_p, pt, dir, tol);

    for (eup = (struct edgeuse **)BU_PTBL_LASTADDR(&eutab);
	 eup >= (struct edgeuse **)BU_PTBL_BASEADDR(&eutab); eup--
	) {
	if (!ret) {
	    /* No edge_g_lseg found yet, use this one. */
	    ret = (*eup)->g.lseg_p;
	    continue;
	}
	if ((*eup)->g.lseg_p == ret) continue;	/* OK */

	/* Found 2 different edge_g_lseg, pick best one */
	VMOVE(dir1, ret->e_dir);
	VUNITIZE(dir1);
	VMOVE(dir2, (*eup)->g.lseg_p->e_dir);
	VUNITIZE(dir2);
	if (fabs(VDOT(dir1, dir)) > fabs(VDOT(dir2, dir))) {
	    /* ret is better, do nothing */
	} else {
	    /* *eup is better, take it instead */
	    ret = (*eup)->g.lseg_p;
	}
	bu_log("nmg_find_eg_on_line() 2 different eg's, taking better one.\n");
    }
    (void)bu_ptbl_free(&eutab);
    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("rt_find_eg_on_line(x%x) ret=x%x\n", magic_p, ret);
    }
    return ret;
}


/**
 * N M G _ K 0 E U
 *
 * Kill all 0-length edgeuses that start and end on this vertex.
 *
 * Returns -
 * 0 If none were found
 * count Number of 0-length edgeuses killed (not counting mates)
 */
int
nmg_k0eu(struct vertex *v)
{
    struct edgeuse *eu;
    struct vertexuse *vu;
    int count = 0;

    NMG_CK_VERTEX(v);
top:
    for (BU_LIST_FOR(vu, vertexuse, &v->vu_hd)) {
	NMG_CK_VERTEXUSE(vu);
	if (*vu->up.magic_p != NMG_EDGEUSE_MAGIC) continue;
	eu = vu->up.eu_p;
	NMG_CK_EDGEUSE(eu);
	if (eu->eumate_p->vu_p->v_p != v) continue;
	bu_log("nmg_k0eu(v=x%x) killing 0-len eu=x%x, mate=x%x\n",
	       v, eu, eu->eumate_p);
	nmg_pr_eu_briefly(eu, 0);
	nmg_pr_eu_briefly(eu->eumate_p, 0);
	if (nmg_keu(eu)) {
	    bu_log("nmg_k0eu() WARNING: parent now has no edgeuses\n");
	    /* XXX Now what? */
	}
	count++;
	goto top;	/* vu_hd list is altered by nmg_keu() */
    }
    return count;
}


/**
 * N M G _ R E P A I R _ V _ N E A R _ V
 *
 * Attempt to join two vertices which both claim to be the intersection
 * of two lines.  If they are close enough, repair the damage.
 *
 * Returns -
 * hit_v If repair succeeds.  vertex 'v' is now invalid.
 * NULL If repair fails.
 * If 'bomb' is non-zero, bu_bomb() is called.
 */
struct vertex *
nmg_repair_v_near_v(struct vertex *hit_v, struct vertex *v, const struct edge_g_lseg *eg1, const struct edge_g_lseg *eg2, int bomb, const struct bn_tol *tol)


/* edge_g_lseg of hit_v */
/* edge_g_lseg of v */


{
    NMG_CK_VERTEX(hit_v);
    NMG_CK_VERTEX(v);
    if (eg1) NMG_CK_EDGE_G_LSEG(eg1);	/* eg1 may be NULL */
    NMG_CK_EDGE_G_LSEG(eg2);
    BN_CK_TOL(tol);

    bu_log("nmg_repair_v_near_v(hit_v=x%x, v=x%x)\n", hit_v, v);

    VPRINT("v ", v->vg_p->coord);
    VPRINT("hit", hit_v->vg_p->coord);
    bu_log("dist v-hit=%g, equal=%d\n",
	   bn_dist_pt3_pt3(v->vg_p->coord, hit_v->vg_p->coord),
	   bn_pt3_pt3_equal(v->vg_p->coord, hit_v->vg_p->coord, tol)
	);
    if (eg1) {
	if (bn_2line3_colinear(eg1->e_pt, eg1->e_dir, eg2->e_pt, eg2->e_dir, 1e5, tol))
	    bu_bomb("ERROR: nmg_repair_v_near_v() eg1 and eg2 are colinear!\n");
	bu_log("eg1: line/ vu dist=%g, hit dist=%g\n",
	       bn_dist_line3_pt3(eg1->e_pt, eg1->e_dir, v->vg_p->coord),
	       bn_dist_line3_pt3(eg1->e_pt, eg1->e_dir, hit_v->vg_p->coord));
	bu_log("eg2: line/ vu dist=%g, hit dist=%g\n",
	       bn_dist_line3_pt3(eg2->e_pt, eg2->e_dir, v->vg_p->coord),
	       bn_dist_line3_pt3(eg2->e_pt, eg2->e_dir, hit_v->vg_p->coord));
	nmg_pr_eg(&eg1->l.magic, 0);
	nmg_pr_eg(&eg2->l.magic, 0);
    }

    if (bn_dist_pt3_pt3(v->vg_p->coord,
			hit_v->vg_p->coord) < 10 * tol->dist) {
	struct edgeuse *eu0;
	bu_log("NOTICE: The intersection of two lines has resulted in 2 different intersect points\n");
	bu_log(" Since the two points are 'close', they are being fused.\n");

	/* See if there is an edge between them */
	eu0 = nmg_findeu(hit_v, v, (struct shell *)NULL,
			 (struct edgeuse *)NULL, 0);
	if (eu0) {
	    bu_log("DANGER: a 0-length edge is being created eu0=x%x\n", eu0);
	}

	nmg_jv(hit_v, v);
	(void)nmg_k0eu(hit_v);
	goto out;
    }
    /* Separation is too great */
    if (bomb)
	bu_bomb("nmg_repair_v_near_v() separation is too great to repair.\n");
    hit_v = (struct vertex *)NULL;
out:
    bu_log("nmg_repair_v_near_v(v=x%x) ret=x%x\n", v, hit_v);
    return hit_v;
}


/**
 * Search all edgeuses referring to this vu's vertex.
 * If the vertex is used by edges on both eg1 and eg2, then it's a "hit"
 * between the two edge geometries.
 * If a new hit happens at a different vertex from a previous hit,
 * that is a fatal error.
 *
 * This routine exists only as a support routine for nmg_common_v_2eg().
 *
 * XXX This is a lame name.
 */
struct vertex *
nmg_search_v_eg(const struct edgeuse *eu, int second, const struct edge_g_lseg *eg1, const struct edge_g_lseg *eg2, register struct vertex *hit_v, const struct bn_tol *tol)

/* 2nd vu on eu, not 1st */


/* often will be NULL */

{
    struct vertex *v;
    register struct vertexuse *vu1;
    register struct edgeuse *seen1 = (struct edgeuse *)NULL;
    register struct edgeuse *seen2 = (struct edgeuse *)NULL;

    NMG_CK_EDGEUSE(eu);
    NMG_CK_EDGE_G_LSEG(eg1);
    NMG_CK_EDGE_G_LSEG(eg2);
    BN_CK_TOL(tol);

    if (second) {
	v = BU_LIST_PNEXT_CIRC(edgeuse, eu)->vu_p->v_p;
	if (v != eu->eumate_p->vu_p->v_p)
	    bu_bomb("nmg_search_v_eg() next vu not mate's vu?\n");
    } else {
	v = eu->vu_p->v_p;
    }
    NMG_CK_VERTEX(v);

    if (eu->g.lseg_p != eg1) bu_bomb("nmg_search_v_eg() eu not on eg1\n");

    /* vu lies on eg1 by topology.  Check this assertion. */
    if (bn_distsq_line3_pt3(eg1->e_pt, eg1->e_dir, v->vg_p->coord) > tol->dist_sq) {
	VPRINT("v", v->vg_p->coord);
	nmg_pr_eu(eu, (char *)NULL);
	nmg_pr_eg(&eg1->l.magic, 0);
	bu_bomb("nmg_search_v_eg() eu vertex not on eg line\n");
    }

    /* This loop accounts for 30% of the runtime of a boolean! */
    for (BU_LIST_FOR(vu1, vertexuse, &v->vu_hd)) {
	register struct edgeuse *eu1;

	if (*vu1->up.magic_p != NMG_EDGEUSE_MAGIC) continue;
	eu1 = vu1->up.eu_p;
	if (eu1->g.lseg_p == eg1) seen1 = eu1;
	if (eu1->g.lseg_p == eg2) seen2 = eu1;
	if (!seen1 || !seen2) continue;

	/* Both edge_g's have been seen at 'v', this is a hit. */
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log(" seen1=x%x, seen2=x%x, hit_v=x%x, v=x%x\n",
		   seen1, seen2, hit_v, v);
	}
	if (!hit_v) {
	    hit_v = v;
	    break;
	}

	/* Is it a different vertex than hit_v? */
	if (hit_v == v) break;

	/* Different vertices, this "can't happen" */
	bu_log("ERROR seen1=x%x, seen2=x%x, hit_v=x%x != v=x%x\n",
	       seen1, seen2, hit_v, v);
	if (nmg_repair_v_near_v(hit_v, v, eg1, eg2, 0, tol))
	    break;

	bu_bomb("nmg_search_v_eg() two different vertices for intersect point?\n");
    }
    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_search_v_eg(eu=x%x, %d, eg1=x%x, eg2=x%x) ret=x%x\n",
	       eu, second, eg1, eg2, hit_v);
    }
    return hit_v;
}


/**
 * N M G _ C O M M O N _ V _ 2 E G
 *
 * Perform a topology search for a common vertex between two edge geometry
 * lines.
 */
struct vertex *
nmg_common_v_2eg(struct edge_g_lseg *eg1, struct edge_g_lseg *eg2, const struct bn_tol *tol)
{
    struct edgeuse *eu1;
    struct vertex *hit_v = (struct vertex *)NULL;
    struct bu_list *midway;	/* &eu->l2, midway into edgeuse */

    NMG_CK_EDGE_G_LSEG(eg1);
    NMG_CK_EDGE_G_LSEG(eg2);
    BN_CK_TOL(tol);

    if (eg1 == eg2)
	bu_bomb("nmg_common_v_2eg() eg1 and eg2 are colinear\n");

    /* Scan all edgeuses in the model that use eg1 */
    for (BU_LIST_FOR(midway, bu_list, &eg1->eu_hd2)) {
	NMG_CKMAG(midway, NMG_EDGEUSE2_MAGIC, "edgeuse2 [l2]");
	eu1 = BU_LIST_MAIN_PTR(edgeuse, midway, l2);
	NMG_CK_EDGEUSE(eu1);
	if (eu1->g.lseg_p != eg1) bu_bomb("nmg_common_v_2eg() eu disavows eg\n");
	/* Both verts of eu1 lie on line eg1 */
	hit_v = nmg_search_v_eg(eu1, 0, eg1, eg2, hit_v, tol);
	hit_v = nmg_search_v_eg(eu1, 1, eg1, eg2, hit_v, tol);
    }
    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_common_v_2eg(eg1=x%x, eg2=x%x) hit_v=x%x\n",
	       eg1, eg2, hit_v);
    }
    return hit_v;
}


#define VDIST(a, b) sqrt((a[X]-b[X])*(a[X]-b[X]) + (a[Y]-b[Y])*(a[Y]-b[Y]) + (a[Z]-b[Z])*(a[Z]-b[Z]))
#define VDIST_SQ(a, b)	((a[X]-b[X])*(a[X]-b[X]) + (a[Y]-b[Y])*(a[Y]-b[Y]) + (a[Z]-b[Z])*(a[Z]-b[Z]))

int
nmg_is_vertex_on_inter(struct vertex *v, struct faceuse *fu1, struct faceuse *fu2, struct nmg_inter_struct *is)
{
    struct vertex_g *vg;
    plane_t pl1, pl2;
    int code;
    fastf_t dist;

    NMG_CK_VERTEX(v);
    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACEUSE(fu2);
    NMG_CK_INTER_STRUCT(is);

    if (nmg_find_v_in_face(v, fu1) && nmg_find_v_in_face(v, fu2))
	return 1;

    NMG_GET_FU_PLANE(pl1, fu1);
    NMG_GET_FU_PLANE(pl2, fu2);

    vg = v->vg_p;
    NMG_CK_VERTEX_G(vg);

    /* check if vertex is in plane of fu's */
    dist = fabs(DIST_PT_PLANE(vg->coord, pl1));
    if (dist > is->tol.dist)
	return 0;
    dist = fabs(DIST_PT_PLANE(vg->coord, pl2));
    if (dist > is->tol.dist)
	return 0;

    /* check if it is on intersection line */
    if (bn_distsq_line3_pt3(is->pt, is->dir, vg->coord) > is->tol.dist_sq)
	return 0;

    /* check if it is within fu's */
    code = nmg_class_pt_fu_except(vg->coord, fu1, (struct loopuse *)NULL,
				  (void (*)())NULL, (void (*)())NULL, (char *)NULL, 0, 0, &is->tol);
    if (code != NMG_CLASS_AinB)
	return 0;

    code = nmg_class_pt_fu_except(vg->coord, fu2, (struct loopuse *)NULL,
				  (void (*)())NULL, (void (*)())NULL, (char *)NULL, 0, 0, &is->tol);
    if (code != NMG_CLASS_AinB)
	return 0;

    return 1;
}


void
nmg_isect_eu_verts(struct edgeuse *eu, struct vertex_g *vg1, struct vertex_g *vg2, struct bu_ptbl *verts, struct bu_ptbl *inters, const struct bn_tol *tol)
{
    int i;
    struct vertex *v1, *v2;

    NMG_CK_EDGEUSE(eu);
    NMG_CK_VERTEX_G(vg1);
    NMG_CK_VERTEX_G(vg2);
    BU_CK_PTBL(verts);
    BU_CK_PTBL(inters);
    BN_CK_TOL(tol);

    v1 = eu->vu_p->v_p;
    v2 = eu->eumate_p->vu_p->v_p;

    for (i=0; i<BU_PTBL_END(verts); i++) {
	struct vertex *v;
	fastf_t dist;
	point_t pca;
	int code;

	v = (struct vertex *)BU_PTBL_GET(verts, i);
	if (v == v1 || v == v2) {
	    bu_ptbl_ins_unique(inters, (long *)v);
	    continue;
	}

	code = bn_dist_pt3_lseg3(&dist, pca, vg1->coord,
				 vg2->coord, v->vg_p->coord, tol);

	if (code)
	    continue;

	bu_ptbl_ins_unique(inters, (long *)v);
    }

    return;
}


void
nmg_isect_eu_eu(struct edgeuse *eu1, struct vertex_g *vg1a, struct vertex_g *vg1b, fastf_t *dir1, struct edgeuse *eu2, struct bu_ptbl *verts, struct bu_ptbl *inters, const struct bn_tol *tol)
{
    struct model *m;
    struct vertex_g *vg2a, *vg2b;
    vect_t dir2;
    fastf_t dist[2];
    int code;
    point_t hit_pt;
    vect_t diff;

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_eu_eu(eu1=x%x, eu2=x%x)\n", eu1, eu2);

    NMG_CK_EDGEUSE(eu1);
    NMG_CK_VERTEX_G(vg1a);
    NMG_CK_VERTEX_G(vg1b);
    NMG_CK_EDGEUSE(eu2);
    BN_CK_TOL(tol);
    BU_CK_PTBL(inters);
    BU_CK_PTBL(verts);

    m = nmg_find_model(&eu1->l.magic);
    NMG_CK_MODEL(m);

    vg2a = eu2->vu_p->v_p->vg_p;
    NMG_CK_VERTEX_G(vg2a);

    vg2b = eu2->eumate_p->vu_p->v_p->vg_p;
    NMG_CK_VERTEX_G(vg2b);

    VSUB2(dir2, vg2b->coord, vg2a->coord);

    code = bn_isect_lseg3_lseg3(dist, vg1a->coord, dir1, vg2a->coord, dir2, tol);

    if (code < 0) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tnmg_isect_eu_eu: No intersection\n");
	return;
    }

    if (code == 1) {
	point_t hit_pt1, hit_pt2;
	struct vertex *v=(struct vertex *)NULL;
	struct edgeuse *new_eu;

	/* normal intersection (one point) */

	if (eu1->vu_p->v_p == eu2->vu_p->v_p ||
	    eu1->vu_p->v_p == eu2->eumate_p->vu_p->v_p ||
	    eu1->eumate_p->vu_p->v_p == eu2->vu_p->v_p ||
	    eu1->eumate_p->vu_p->v_p == eu2->eumate_p->vu_p->v_p)
	    return;

	if (ZERO(dist[0]) || ZERO(dist[0] - 1.0))
	    return;

	if (ZERO(dist[1])) {
	    bu_ptbl_ins_unique(inters, (long *)eu2->vu_p->v_p);
	    return;
	}
	if (ZERO(dist[1] - 1.0)) {
	    bu_ptbl_ins_unique(inters, (long *)eu2->eumate_p->vu_p->v_p);
	    return;
	}

	VJOIN1(hit_pt1, vg1a->coord, dist[0], dir1);
	VJOIN1(hit_pt2, vg2a->coord, dist[1], dir2);

	VBLEND2(hit_pt, 0.5, hit_pt1, 0.5, hit_pt2);

	v = nmg_find_pt_in_model(m, hit_pt, tol);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("nmg_isect_eu_eu: intersection at (%g %g %g)\n", V3ARGS(hit_pt));
	    bu_log("splitting eu x%x at v=x%x\n", eu2, v);
	}
	new_eu = nmg_esplit(v, eu2, 1);
	if (!v) {
	    v = new_eu->vu_p->v_p;
	    nmg_vertex_gv(v, hit_pt);
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\tcreated new vertex x%x\n", v);
	}
	bu_ptbl_ins_unique(inters, (long *)v);
	bu_ptbl_ins_unique(verts, (long *)v);
	return;
    }

    /* code == 0, could be two intersection points
     * But there should be no vertex creation here
     */

    VSUB2(diff, vg2a->coord, vg1a->coord);
    if (VDOT(diff, dir1) > 0.0) {
	VSUB2(diff, vg1b->coord, vg2a->coord);
	if (VDOT(diff, dir1) > 0.0)
	    bu_ptbl_ins_unique(inters, (long *)eu2->vu_p->v_p);
    }

    VSUB2(diff, vg2b->coord, vg1a->coord);
    if (VDOT(diff, dir1) > 0.0) {
	VSUB2(diff, vg1b->coord, vg2b->coord);
	if (VDOT(diff, dir1) > 0.0)
	    bu_ptbl_ins_unique(inters, (long *)eu2->eumate_p->vu_p->v_p);
    }
}


void
nmg_isect_eu_fu(struct nmg_inter_struct *is, struct bu_ptbl *verts, struct edgeuse *eu, struct faceuse *fu)
{
    struct model *m;
    struct vertex_g *vg1, *vg2;
    struct loopuse *lu;
    plane_t pl;
    fastf_t dist;
    fastf_t eu_len;
    fastf_t one_over_len;
    point_t hit_pt;
    vect_t edir;
    vect_t dir;
    struct bu_ptbl inters;
    fastf_t *inter_dist;
    int i;

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_eu_fu: eu=x%x, fu=x%x START\n", eu, fu);

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_FACEUSE(fu);
    NMG_CK_EDGEUSE(eu);
    BU_CK_PTBL(verts);

    if (nmg_find_fu_of_eu(eu) == fu) {
	bu_log("nmg_isect_eu_fu() called with eu (x%x) from its own fu (x%x)\n", eu, fu);
	bu_bomb("nmg_isect_eu_fu() called with eu from its own fu");
    }

    m = nmg_find_model(&fu->l.magic);
    NMG_CK_MODEL(m);
    if (nmg_find_model(&eu->l.magic) != m) {
	bu_log("nmg_isect_eu_fu() called with EU (x%x) from model (x%x)\n", eu, nmg_find_model(&eu->l.magic));
	bu_log("\tand FU (x%x) from model (x%x)\n", fu, m);
	bu_bomb("nmg_isect_eu_fu() called with EU and FU from different models");
    }

    vg1 = eu->vu_p->v_p->vg_p;
    NMG_CK_VERTEX_G(vg1);
    vg2 = eu->eumate_p->vu_p->v_p->vg_p;
    NMG_CK_VERTEX_G(vg2);

    VSUB2(dir, vg2->coord, vg1->coord);
    VMOVE(edir, dir);
    eu_len = MAGNITUDE(dir);
    if (eu_len < is->tol.dist || eu_len <= SMALL_FASTF) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tnmg_isec_eu_fu: 0 length edge\n");
	return;
    }

    one_over_len = 1.0/eu_len;
    VSCALE(dir, dir, one_over_len);

    NMG_GET_FU_PLANE(pl, fu);
    /* check if edge line intersects plane of fu */
    if (bn_isect_line3_plane(&dist, vg1->coord, dir, pl, &is->tol) < 1) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tnmg_isec_eu_fu: no intersection\n");
	return;
    }

    VJOIN1(hit_pt, vg1->coord, dist, dir);

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("\tintersection point at (%g %g %g)\n", V3ARGS(hit_pt));

    /* create a list of intersection vertices */
    bu_ptbl_init(&inters, 64, " &inters");

    /* add vertices from fu to list */
    nmg_isect_eu_verts(eu, vg1, vg2, verts, &inters, &is->tol);

    /* break FU EU's that intersect our eu, and put vertices on list */
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	struct edgeuse *eu_fu;

	NMG_CK_LOOPUSE(lu);

	/* vertices of FU are handled above */
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu_fu, edgeuse, &lu->down_hd)) {
	    NMG_CK_EDGEUSE(eu_fu);

	    nmg_isect_eu_eu(eu, vg1, vg2, edir, eu_fu, verts, &inters, &is->tol);

	}
    }

    /* Now break eu at every vertex in the "inters" list */

    if (BU_PTBL_END(&inters) == 0) {
	struct vertex *v=(struct vertex *)NULL;
	int class;
	fastf_t dist_to_plane;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("\tNo intersection points found\n");

	/* check if EU endpoints are within tolerance of FU
	 * If so, the endpoint is the intersection and nothing to do
	 */
	dist_to_plane = DIST_PT_PLANE(vg1->coord, pl);
	if (dist_to_plane < 0.0)
	    dist_to_plane = (-dist_to_plane);
	if (dist_to_plane <= is->tol.dist) {
	    /* check if hit point is within fu */
	    class = nmg_class_pt_fu_except(vg1->coord, fu, (struct loopuse *)NULL,
					   0, 0, (char *)NULL, 0, 0, &is->tol);
	    if (class != NMG_CLASS_AinB)
		goto out;

	    v = eu->vu_p->v_p;
	    if (v && !nmg_find_v_in_face(v, fu)) {
		struct vertexuse *new_vu;

		new_vu = nmg_make_dualvu(v, fu, &is->tol);
		bu_ptbl_ins_unique(verts, (long *)new_vu->v_p);
	    }
	    goto out;
	}

	dist_to_plane = DIST_PT_PLANE(vg2->coord, pl);
	if (dist_to_plane < 0.0)
	    dist_to_plane = (-dist_to_plane);
	if (dist_to_plane <= is->tol.dist) {
	    /* check if hit point is within fu */
	    class = nmg_class_pt_fu_except(vg2->coord, fu, (struct loopuse *)NULL,
					   0, 0, (char *)NULL, 0, 0, &is->tol);
	    if (class != NMG_CLASS_AinB)
		goto out;

	    v = eu->eumate_p->vu_p->v_p;
	    if (v && !nmg_find_v_in_face(v, fu)) {
		struct vertexuse *new_vu;

		new_vu = nmg_make_dualvu(v, fu, &is->tol);
		bu_ptbl_ins_unique(verts, (long *)new_vu->v_p);
	    }
	    goto out;
	}

	/* no vertices found, we might need to create a self loop */

	/* make sure intersection is within limits of eu */
	if (dist < (-is->tol.dist) || dist > eu_len+is->tol.dist) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\tnmg_isec_eu_fu: intersection beyond ends of EU\n");
	    goto out;
	}

	/* check if hit_point is within tolerance of an end of eu */
	if (VDIST_SQ(hit_pt, vg1->coord) <= is->tol.dist_sq) {
	    v = eu->vu_p->v_p;
	    VMOVE(hit_pt, vg1->coord);
	} else if (VDIST_SQ(hit_pt, vg2->coord) <= is->tol.dist_sq) {
	    v = eu->eumate_p->vu_p->v_p;
	    VMOVE(hit_pt, vg2->coord);
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("\tHit point is not within tolerance of eu endpoints\n");
	    bu_log("\t\thit_pt=(%g %g %g), eu=(%g %g %g)<->(%g %g %g)\n",
		   V3ARGS(hit_pt), V3ARGS(vg1->coord), V3ARGS(vg2->coord));
	}

	/* check if hit point is within fu */
	class = nmg_class_pt_fu_except(hit_pt, fu, (struct loopuse *)NULL,
				       0, 0, (char *)NULL, 0, 0, &is->tol);

	if (class == NMG_CLASS_AinB) {
	    struct edgeuse *new_eu;

	    /* may need to split eu */
	    if (!v)
		v = nmg_find_pt_in_model(m, hit_pt, &is->tol);
	    if (v != eu->vu_p->v_p && v != eu->eumate_p->vu_p->v_p) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
		    bu_log("\tsplitting eu (x%x) at hit_pt (v=x%x)\n", eu, v);

		new_eu = nmg_esplit(v, eu, 1);
		if (!v) {
		    v = new_eu->vu_p->v_p;
		    if (rt_g.NMG_debug & DEBUG_POLYSECT)
			bu_log("\tnew vertex at hit point is x%x\n", v);
		    nmg_vertex_gv(v, hit_pt);
		}
	    }

	    if (v && !nmg_find_v_in_face(v, fu)) {
		struct vertexuse *new_vu;

		new_vu = nmg_make_dualvu(v, fu, &is->tol);
		bu_ptbl_ins_unique(verts, (long *)new_vu->v_p);
	    }

	}
	goto out;
    }

    if (BU_PTBL_END(&inters) == 1) {
	struct vertex *v;

	/* only one vertex, just split */
	v = (struct vertex *)BU_PTBL_GET(&inters, 0);
	NMG_CK_VERTEX(v);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("Only one intersect vertex (x%x), just split all EU's at (x%x)\n", v, eu);

	if (v == eu->vu_p->v_p || v == eu->eumate_p->vu_p->v_p)
	    goto out;

	(void)nmg_break_all_es_on_v(&m->magic, v, &is->tol);

	goto out;
    }

    /* must do them in order from furthest to nearest */
    inter_dist = (fastf_t *)bu_calloc(BU_PTBL_END(&inters), sizeof(fastf_t),
				      "nmg_isect_eu_fu: inter_dist");

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("%d intersect vertices along eu (x%x)\n", BU_PTBL_END(&inters), eu);

    for (i=0; i<BU_PTBL_END(&inters); i++) {
	struct vertex *v;
	struct vertex_g *vg;
	vect_t diff;

	v = (struct vertex *)BU_PTBL_GET(&inters, i);
	NMG_CK_VERTEX(v);
	vg = v->vg_p;
	NMG_CK_VERTEX_G(vg);

	VSUB2(diff, vg->coord, vg1->coord);
	if (VDOT(diff, dir) < -SMALL_FASTF) {
	    bu_bomb("nmg_isect_eu_fu: intersection point not on eu\n");
	}
	inter_dist[i] = MAGSQ(diff);
    }

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("Intersect vertices along eu x%x:\n", eu);
	for (i=0; i<BU_PTBL_END(&inters); i++)
	    bu_log("%d x%x %g\n", i+1, BU_PTBL_GET(&inters, i), inter_dist[i]);
    }

    while (1) {
	struct vertex *v;
	fastf_t max_dist;
	int index_at_max;

	max_dist = (-1.0);
	index_at_max = (-1);

	for (i=0; i<BU_PTBL_END(&inters); i++) {
	    if (inter_dist[i] > max_dist) {
		max_dist = inter_dist[i];
		index_at_max = i;
	    }
	}

	if (index_at_max < 0)
	    break;

	v = (struct vertex *)BU_PTBL_GET(&inters, index_at_max);
	NMG_CK_VERTEX(v);

	if (v != eu->vu_p->v_p && v != eu->eumate_p->vu_p->v_p) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("Breaking edges at vertex #%d, dist=%g, v=x%x\n", i+1, inter_dist[i], v);
	    (void)nmg_break_all_es_on_v(&m->magic, v, &is->tol);
	}

	inter_dist[index_at_max] = (-10.0);
    }

    bu_free((char *)inter_dist, "nmg_isect_eu_fu: inter_dist");

out:
    bu_ptbl_free(&inters);

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_eu_fu: eu=x%x, fu=x%x END\n", eu, fu);

}


void
nmg_isect_fu_jra(struct nmg_inter_struct *is, struct faceuse *fu1, struct faceuse *fu2, struct bu_ptbl *eu1_list, struct bu_ptbl *eu2_list)
{
    struct model *m;
    struct bu_ptbl verts1, verts2;
    struct loopuse *lu;
    int i;

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_fu_jra(fu1=x%x, fu2=x%x) START\n", fu1, fu2);

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACEUSE(fu2);
    BU_CK_PTBL(eu1_list);
    BU_CK_PTBL(eu2_list);

    m = nmg_find_model(&fu1->l.magic);
    NMG_CK_MODEL(m);

    nmg_vertex_tabulate(&verts2, &fu2->l.magic);

    /* Intersect fu1 edgeuses */
    for (BU_LIST_FOR(lu, loopuse, &fu1->lu_hd)) {
	struct edgeuse *eu;

	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    NMG_CK_EDGEUSE(eu);

	    nmg_isect_eu_fu(is, &verts2, eu, fu2);
	}
    }

    bu_ptbl_free(&verts2);
    nmg_vertex_tabulate(&verts1, &fu1->l.magic);

    /* now intersect fu2 edgeuses */
    for (BU_LIST_FOR(lu, loopuse, &fu2->lu_hd)) {
	struct edgeuse *eu;

	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    NMG_CK_EDGEUSE(eu);

	    nmg_isect_eu_fu(is, &verts1, eu, fu1);
	}
    }

    /* check for existing vertices along intersection */
    bu_ptbl_free(&verts1);

    /* XXXX this is the second time this tabulate is being done,
     * but for now it's safer this way
     */
    nmg_vertex_tabulate(&verts1, &fu1->l.magic);
    nmg_vertex_tabulate(&verts2, &fu2->l.magic);

    /* merge the two lists */
    for (i=0; i<BU_PTBL_END(&verts2); i++) {
	struct vertex *v;

	v = (struct vertex *)BU_PTBL_GET(&verts2, i);
	NMG_CK_VERTEX(v);

	bu_ptbl_ins_unique(&verts1, (long *)v);
    }
    bu_ptbl_free(&verts2);

    for (i=0; i< BU_PTBL_END(&verts1); i++) {
	struct vertex *v;
	struct vertexuse *vu;
	fastf_t dist;

	v = (struct vertex *)BU_PTBL_GET(&verts1, i);
	NMG_CK_VERTEX(v);

	if (!nmg_is_vertex_on_inter(v, fu1, fu2, is))
	    continue;

	/* calculate distance along intersect ray */
	dist = VDIST(is->pt, v->vg_p->coord);

	/* this vertex is on the intersection line
	 * add all uses from fu1 and fu2 to intersection list
	 */
	for (BU_LIST_FOR(vu, vertexuse, &v->vu_hd)) {
	    struct faceuse *fu_tmp;

	    fu_tmp = nmg_find_fu_of_vu(vu);
	    if (fu_tmp == fu1 || fu_tmp == fu2) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
		    bu_log("\tenlisting vu x%x (x%x) from fu (x%x)\n", vu, v, fu_tmp);
		nmg_enlist_one_vu(is, vu, dist);
	    }
	}
    }

    bu_ptbl_free(&verts1);

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_fu_jra(fu1=x%x, fu2=x%x) END\n", fu1, fu2);
}


/**
 * N M G _ I S E C T _ L I N E 2 _ F A C E 2 P
 *
 * HEART
 *
 * For each distinct edge_g_lseg LINE on the face (composed of potentially many
 * edgeuses and many different edges), intersect with the edge_g_lseg LINE
 * which represents the face/face intersection line.
 *
 * Note that the geometric intersection of the two faces is
 * stored in is->pt and is->dir.
 * If is->on_eg is set, it is the callers' responsibility to make sure
 * it is not much different than the original geometric one.
 *
 * Go to great pains to ensure that two non-colinear lines intersect
 * at either 0 or 1 points, and no more.
 *
 * Called from -
 * nmg_isect_edge2p_face2p()
 * nmg_isect_two_face3p()
 */
void
nmg_isect_line2_face2pNEW(struct nmg_inter_struct *is, struct faceuse *fu1, struct faceuse *fu2, struct bu_ptbl *eu1_list, struct bu_ptbl *eu2_list)
{
    struct bu_ptbl eg_list;
    struct edge_g_lseg **eg1;
    struct edgeuse *eu1;
    struct edgeuse *eu2;
    fastf_t dist[2];
    int code;
    point_t eg_pt2d;	/* 2D */
    vect_t eg_dir2d;	/* 2D */
    struct loopuse *lu1;
    point_t hit3d;
    point_t hit2d;		/* 2D */
    struct edgeuse *new_eu;
    int eu1_index;
    int eu2_index;
    int class;
    fastf_t distance;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACEUSE(fu2);
    BU_CK_PTBL(eu1_list);
    BU_CK_PTBL(eu2_list);

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_line2_face2pNEW(, fu1=x%x, fu2=x%x) on_eg=x%x\n", fu1, fu2, is->on_eg);

    /* Project the intersect line into 2D.  Build matrix first. */
    nmg_isect2d_prep(is, &fu1->l.magic);
    /* XXX Need subroutine for this!! */
    MAT4X3PNT(is->pt2d, is->proj, is->pt);
    MAT4X3VEC(is->dir2d, is->proj, is->dir);

re_tabulate:
    /* Build list of all edge_g_lseg's in fu1 */
    /* XXX This could be more cheaply done by cooking down eu1_list */
    nmg_edge_g_tabulate(&eg_list, &fu1->l.magic);

    /* Process each distinct line in the face fu1 */
    for (eg1 = (struct edge_g_lseg **)BU_PTBL_LASTADDR(&eg_list);
	 eg1 >= (struct edge_g_lseg **)BU_PTBL_BASEADDR(&eg_list); eg1--
	) {
	struct vertex *hit_v = (struct vertex *)NULL;

	NMG_CK_EDGE_G_LSEG(*eg1);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("\tChecking eg=x%x\n", *eg1);
	}

	if (*eg1 == is->on_eg) {
	    point_t pca;
	    struct edgeuse *eu_end;
	    struct vertex_g *vg;
	    plane_t pl1, pl2;

	colinear:
	    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		bu_log("\tThis edge_geom generated the line.  Enlisting.\n");
	    }

	    NMG_GET_FU_PLANE(pl1, is->fu1);
	    NMG_GET_FU_PLANE(pl2, is->fu2);
	    for (eu1_index=0; eu1_index < BU_PTBL_END(eu1_list); eu1_index++) {
		eu1 = (struct edgeuse *)BU_PTBL_GET(eu1_list, eu1_index);
		NMG_CK_EDGEUSE(eu1);
		if (eu1->g.lseg_p != is->on_eg) continue;
		/* eu1 is from fu1 */

		vg = eu1->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G(vg);
		(void)bn_dist_pt3_line3(&distance, pca, is->pt, is->dir, vg->coord, &(is->tol));
		if (distance <= is->tol.dist) {
		    /* vertex is on intersection line */

		    if (DIST_PT_PLANE(vg->coord, pl2) <= is->tol.dist) {
			/* and in plane of fu2 */
			if (nmg_class_pt_fu_except(vg->coord, fu2, (struct loopuse *)NULL, NULL, NULL, (char *)NULL, 0, 0, &(is->tol)) != NMG_CLASS_AoutB) {
			    /* and within fu2 */
			    distance = VDIST(is->pt, vg->coord);
			    nmg_enlist_vu(is, eu1->vu_p, 0, distance);
			}
		    }
		}
		eu_end = BU_LIST_PNEXT_CIRC(edgeuse, &eu1->l);
		vg = eu_end->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G(vg);
		code = bn_dist_pt3_line3(&distance, pca, is->pt, is->dir, eu_end->vu_p->v_p->vg_p->coord, &(is->tol));
		if (distance <= is->tol.dist) {
		    /* vertex is on intersection line */

		    if (DIST_PT_PLANE(vg->coord, pl2) <= is->tol.dist) {
			/* and in plane of fu2 */
			if (nmg_class_pt_fu_except(vg->coord, fu2, (struct loopuse *)NULL, NULL, NULL, (char *)NULL, 0, 0, &(is->tol)) != NMG_CLASS_AoutB) {
			    /* and within fu2 */
			    distance = VDIST(is->pt, vg->coord);
			    nmg_enlist_vu(is, eu_end->vu_p, 0, distance);
			}
		    }
		}
	    }

	    for (eu2_index=0; eu2_index < BU_PTBL_END(eu2_list); eu2_index++) {
		eu2 = (struct edgeuse *)BU_PTBL_GET(eu2_list, eu2_index);
		NMG_CK_EDGEUSE(eu2);

		if (eu2->g.lseg_p != is->on_eg) continue;
		/* eu2 is from fu2 */

		vg = eu2->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G(vg);
		code = bn_dist_pt3_line3(&distance, pca, is->pt, is->dir, vg->coord, &(is->tol));
		if (distance <= is->tol.dist) {
		    /* vertex is on intersection line */

		    if (DIST_PT_PLANE(vg->coord, pl1) <= is->tol.dist) {
			/* and in plane of fu1 */
			if (nmg_class_pt_fu_except(vg->coord, fu1, (struct loopuse *)NULL, NULL, NULL, (char *)NULL, 0, 0, &(is->tol)) != NMG_CLASS_AoutB) {
			    /* and within fu1 */
			    distance = VDIST(is->pt, vg->coord);
			    nmg_enlist_vu(is, eu2->vu_p, 0, distance);
			}
		    }
		}
		eu_end = BU_LIST_PNEXT_CIRC(edgeuse, &eu2->l);
		vg = eu_end->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G(vg);
		code = bn_dist_pt3_line3(&distance, pca, is->pt, is->dir, eu_end->vu_p->v_p->vg_p->coord, &(is->tol));
		if (distance <= is->tol.dist) {
		    /* vertex is on intersection line */

		    if (DIST_PT_PLANE(vg->coord, pl1) <= is->tol.dist) {
			/* and in plane of fu1 */
			if (nmg_class_pt_fu_except(vg->coord, fu1, (struct loopuse *)NULL, NULL, NULL, (char *)NULL, 0, 0, &(is->tol)) != NMG_CLASS_AoutB) {
			    /* and within fu1 */
			    distance = VDIST(is->pt, vg->coord);
			    nmg_enlist_vu(is, eu_end->vu_p, 0, distance);
			}
		    }
		}
	    }
	    continue;
	}

	/*
	 * eg1 is now known to be NOT colinear with on_eg.
	 * From here on, only 0 or 1 points of intersection are possible.
	 */

	/* The 3D line in is->pt and is->dir is prepared by our caller. */
	MAT4X3PNT(eg_pt2d, is->proj, (*eg1)->e_pt);
	MAT4X3VEC(eg_dir2d, is->proj, (*eg1)->e_dir);

	/* Calculate 2D geometric intersection, but don't look at answer yet */
	dist[0] = dist[1] = -INFINITY;
	code = bn_isect_line2_line2(dist, is->pt2d, is->dir2d,
				    eg_pt2d, eg_dir2d, &(is->tol));

	/* Do this check before topology search */
	if (code == 0) {
	    /* Geometry says lines are colinear.  Egads! This can't be! */
	    if (is->on_eg) {
		bu_log("nmg_isect_line2_face2pNEW() edge_g not shared, geometry says lines are colinear.\n");
		goto fixup;
	    }
	    /* on_eg wasn't set, use it and continue on */
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("NOTICE: setting on_eg to eg1 and continuing with colinear case.\n");
	    is->on_eg = (*eg1);
	    goto colinear;
	}

	/* Double check */
	if (is->on_eg && bn_2line3_colinear(
		(*eg1)->e_pt, (*eg1)->e_dir,
		is->on_eg->e_pt, is->on_eg->e_dir, 1e5, &(is->tol))) {
	fixup:
	    nmg_pr_eg(&(*eg1)->l.magic, 0);
	    nmg_pr_eg(&is->on_eg->l.magic, 0);
	    bu_log("nmg_isect_line2_face2pNEW() eg1 colinear to on_eg?\n");

	    /* fuse eg1 with on_eg, handle as colinear */
	    bu_log("fusing eg1 with on_eg, handling as colinear\n");
	    nmg_jeg(is->on_eg, *eg1);
	    goto colinear;
	}

	/* If on_eg was specified, do a search for topology intersection */
	if (is->on_eg && !hit_v) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		bu_log("non-colinear.  Searching for topological intersection between on_eg and eg1\n");
	    }
	    /* See if any vu along eg1 is used by edge from on_eg */
	    hit_v = nmg_common_v_2eg(*eg1, is->on_eg, &(is->tol));
	    /*
	     * Note that while eg1 contains an eu from fu1,
	     * the intersection vertex just found may occur
	     * well outside *either* faceuse, but lie on some
	     * other face that shares this face geometry.
	     */
	    if (hit_v) {
		fastf_t dist1, dist2;
		plane_t n1, n2;

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		    static int num=0;
		    char buf[128];
		    FILE *fp;
		    sprintf(buf, "Itopo%d.pl", num++);
		    if ((fp=fopen(buf, "wb")) != NULL) {
			pl_color(fp, 255, 0, 0);
			pdv_3ray(fp, is->on_eg->e_pt, is->on_eg->e_dir, 1.0);
			pdv_3cont(fp, hit_v->vg_p->coord);
			pl_color(fp, 255, 255, 0);
			pdv_3ray(fp, (*eg1)->e_pt, (*eg1)->e_dir, 1.0);
			pdv_3cont(fp, hit_v->vg_p->coord);
			fclose(fp);
			bu_log("overlay %s red is on_eg, yellow is eg1\n", buf);
		    } else perror(buf);
		    bu_log("\tTopology intersection.  hit_v=x%x (%g, %g, %g)\n",
			   hit_v,
			   V3ARGS(hit_v->vg_p->coord));
		}

		/*
		 * If the hit point is outside BOTH faces
		 * bounding RPP, then it can be ignored.
		 * Otherwise, it is needed for generating
		 * accurate state transitions in the
		 * face cutter.
		 */
		if (!V3PT_IN_RPP(hit_v->vg_p->coord, fu1->f_p->min_pt, fu1->f_p->max_pt) &&
		    !V3PT_IN_RPP(hit_v->vg_p->coord, fu2->f_p->min_pt, fu2->f_p->max_pt)
		    ) {
		    /* Lines intersect outside bounds of both faces. */
		    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			VPRINT("\t\tisect pt outside fu1 & fu2 RPP:", hit_v->vg_p->coord);
			bu_log("\t\tfu1 RPP: (%g %g %g) <-> (%g %g %g)\n",
			       V3ARGS(fu1->f_p->min_pt), V3ARGS(fu1->f_p->max_pt));
			bu_log("\t\tfu2 RPP: (%g %g %g) <-> (%g %g %g)\n",
			       V3ARGS(fu2->f_p->min_pt), V3ARGS(fu2->f_p->max_pt));
		    }
		    continue;
		}

		/*
		 * Geometry check.
		 * Since on_eg is the line of intersection
		 * between fu1 and fu2, and since eg1
		 * lies in the plane of fu1,
		 * and since hit_v lies on eg1
		 * at the point where eg1 intersects on_eg,
		 * it's pretty hard to see how hit_v
		 * could fail to lie in the plane of fu2.
		 *
		 * However, in BigWedge r1 (m14.r) this
		 * case occurs:  hit_v is off f2 by -60mm!
		 * These two fu's don't overlap at all.
		 *
		 * Rather than letting nmg_ck_v_in_2fus()
		 * blow its mind over this, catch it here
		 * and discard the point.
		 * (Hopefully the RPP checks above will have
		 * already discarded it).
		 */
		NMG_GET_FU_PLANE(n1, fu1);
		NMG_GET_FU_PLANE(n2, fu2);
		dist1 = DIST_PT_PLANE(hit_v->vg_p->coord, n1);
		dist2 = DIST_PT_PLANE(hit_v->vg_p->coord, n2);

		if (!NEAR_ZERO(dist1, is->tol.dist) || !NEAR_ZERO(dist2, is->tol.dist)) {
		    continue;
		}
		if ((class=nmg_class_pt_fu_except(hit_v->vg_p->coord, fu1, (struct loopuse *)NULL, NULL, NULL, (char *)NULL, 0, 0, &(is->tol))) == NMG_CLASS_AoutB) {
		    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			VPRINT("\t\tisect pt outside face fu1 (nmg_class_pt_fu_except):", hit_v->vg_p->coord);
		    }
		    continue;
		} else if ((class=nmg_class_pt_fu_except(hit_v->vg_p->coord, fu2, (struct loopuse *)NULL, NULL, NULL, (char *)NULL, 0, 0, &(is->tol))) == NMG_CLASS_AoutB) {
		    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			VPRINT("\t\tisect pt outside face fu2 (nmg_class_pt_fu_except):", hit_v->vg_p->coord);
		    }
		    continue;
		}
		/* Ignore further geometry checking, isect is good.
		 * Head right to searching for vertexuses
		 */
		goto force_isect;
	    }
	}

	/* Now compare results of topology and geometry calculations */

	if (code < 0) {
	    /* Geometry says lines are parallel, no intersection */
	    if (hit_v) {
		bu_log("NOTICE: geom/topo mis-match, enlisting topo vu, hit_v=x%x\n", hit_v);
		VPRINT("hit_v", hit_v->vg_p->coord);
		nmg_pr_eg(&(*eg1)->l.magic, 0);
		nmg_pr_eg(&is->on_eg->l.magic, 0);
		bu_log(" dist to eg1=%e, dist to on_eg=%e\n",
		       bn_dist_line3_pt3((*eg1)->e_pt, (*eg1)->e_dir, hit_v->vg_p->coord),
		       bn_dist_line3_pt3(is->on_eg->e_pt, is->on_eg->e_dir, hit_v->vg_p->coord));
		VPRINT("is->pt2d ", is->pt2d);
		VPRINT("is->dir2d", is->dir2d);
		VPRINT("eg_pt2d ", eg_pt2d);
		VPRINT("eg_dir2d ", eg_dir2d);
		bu_log(" 3d line isect, code=%d\n",
		       bn_isect_line3_line3(&dist[0], &dist[1],
					    is->pt, is->dir,
					    (*eg1)->e_pt,
					    (*eg1)->e_dir,
					    &(is->tol)
			   ));
		goto force_isect;
	    }
	    continue;
	}
	/* Geometry says 2 lines intersect at a point */
	VJOIN1(hit3d, is->pt, dist[0], is->dir);
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    VPRINT("\t2 lines intersect at", hit3d);
	}

	if (!V3PT_IN_RPP(hit3d, fu1->f_p->min_pt, fu1->f_p->max_pt)) {
	    /* Lines intersect outside bounds of this face. */
	    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		VPRINT("\t\tisect pt outside fu1 face RPP:", hit3d);
		bu_log("\t\tface RPP: (%g %g %g) <-> (%g %g %g)\n",
		       V3ARGS(fu1->f_p->min_pt), V3ARGS(fu1->f_p->max_pt));
	    }
	    continue;
	}

	if ((class=nmg_class_pt_fu_except(hit3d, fu1, (struct loopuse *)NULL, NULL, NULL, (char *)NULL, 0, 0, &(is->tol))) == NMG_CLASS_AoutB) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		VPRINT("\t\tisect pt outside face fu1 (nmg_class_pt_fu_except):", hit3d);
	    }
	    continue;
	} else if ((class=nmg_class_pt_fu_except(hit3d, fu2, (struct loopuse *)NULL, NULL, NULL, (char *)NULL, 0, 0, &(is->tol))) == NMG_CLASS_AoutB) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		VPRINT("\t\tisect pt outside face fu2 (nmg_class_pt_fu_except):", hit3d);
	    }
	    continue;
	} else if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("\t\tnmg_class_pt_fu_except(fu1) returns %s\n", nmg_class_name(class));
	}

	VJOIN1_2D(hit2d, is->pt2d, dist[0], is->dir2d);

	/* Consistency check between geometry, and hit_v. */
	if (hit_v) {
	force_isect:
	    /* Force things to be consistent, use geom from hit_v */
	    VMOVE(hit3d, hit_v->vg_p->coord);
	    nmg_get_2d_vertex(hit2d, hit_v, is, &fu1->l.magic);
	    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		bu_log("hit_v=x%x\n", hit_v);
		VPRINT("hit3d", hit3d);
		V2PRINT("hit2d", hit2d);
	    }
	}

    eu_search:
	/* Search all eu's on eg1 for vu's to enlist.
	 * There may be many, and the list may grow.
	 */
	for (eu1_index=0; eu1_index < BU_PTBL_END(eu1_list); eu1_index++) {
	    struct vertexuse *vu1a, *vu1b;
	    struct vertexuse *vu1_midpt;
	    fastf_t ldist;
	    point_t eu1_pt2d;	/* 2D */
	    point_t eu1_end2d;	/* 2D */
	    double tmp_dist_sq;

	    eu1 = (struct edgeuse *)BU_PTBL_GET(eu1_list, eu1_index);

	    /* This EU may have been killed by nmg_repair_v_near_v() */
	    if (eu1->l.magic != NMG_EDGEUSE_MAGIC)
		continue;

	    NMG_CK_EDGEUSE(eu1);

	    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		bu_log("\tChecking eu x%x\n", eu1);
	    }

	    if (eu1->g.lseg_p != *eg1) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		    bu_log("\t\teg x%x is not eg1=x%x\n", eu1->g.lseg_p, *eg1);
		}
		continue;
	    }
	    vu1a = eu1->vu_p;
	    vu1b = BU_LIST_PNEXT_CIRC(edgeuse, eu1)->vu_p;

	    /* First, a topology check of both endpoints */
	    if (vu1a->v_p == hit_v) {
	    hit_a:
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		    bu_log("\tlisting intersect point at vu1a=x%x\n", vu1a);
		}
		/* Note that the distance dist[0] may not actually
		 * be the exact distance to vu1a, but it is the distance
		 * to the actual intersection point. This is an
		 * attempt to get the correct ordering of vertices
		 * on the intersection list, since using the
		 * actual distance can get them reversed when
		 * a VU is chosen over the actual interection
		 * point.
		 */
		nmg_enlist_vu(is, vu1a, 0, dist[0]);
	    }
	    if (vu1b->v_p == hit_v) {
	    hit_b:
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		    bu_log("\tlisting intersect point at vu1b=x%x\n", vu1b);
		}
		/* see above note about dist[0] */
		nmg_enlist_vu(is, vu1b, 0, dist[0]);
	    }
	    if (vu1a->v_p == hit_v || vu1b->v_p == hit_v) continue;

	    /* Second, a geometry check on the edgeuse ENDPOINTS
	     * -vs- the line segment.  This is 3D, for consistency
	     * with comparisons elsewhere.
	     */
	    tmp_dist_sq = bn_distsq_line3_pt3(is->pt, is->dir, vu1a->v_p->vg_p->coord);
	    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		bu_log("\tvu1a is sqrt(%g) from the intersect line\n", tmp_dist_sq);
	    }
	    if (tmp_dist_sq <= is->tol.dist_sq) {
		if (!hit_v) {
		    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			bu_log("\tnmg_isect_line2_face2pNEW: using nearby vu1a vertex x%x from eu x%x\n", vu1a->v_p, eu1);
		    }
		    hit_v = vu1a->v_p;
		    goto hit_a;
		}
		if (hit_v == vu1a->v_p) goto hit_a;

		/* Fall through to bn_isect_pt2_lseg2() */
	    }
	    tmp_dist_sq = bn_distsq_line3_pt3(is->pt, is->dir, vu1b->v_p->vg_p->coord);
	    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		bu_log("\tvu1b is sqrt(%g) from the intersect line\n", tmp_dist_sq);
	    }
	    if (tmp_dist_sq <= is->tol.dist_sq) {
		if (!hit_v) {
		    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			bu_log("\tnmg_isect_line2_face2pNEW: using nearby vu1b vertex x%x from eu x%x\n", vu1b->v_p, eu1);
		    }
		    hit_v = vu1b->v_p;
		    goto hit_b;
		}
		if (hit_v == vu1b->v_p) goto hit_b;

		/* Fall through to bn_isect_pt2_lseg2() */
	    }

	    /* Third, a geometry check of the HITPT -vs- the line segment */
	    nmg_get_2d_vertex(eu1_pt2d, vu1a->v_p, is, &fu1->l.magic);
	    nmg_get_2d_vertex(eu1_end2d, vu1b->v_p, is, &fu1->l.magic);
	    ldist = 0;
	    code = bn_isect_pt2_lseg2(&ldist, eu1_pt2d, eu1_end2d, hit2d, &(is->tol));
	    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		bu_log("\tbn_isect_pt2_lseg2() returned %d, ldist=%g\n", code, ldist);
	    }
	    switch (code) {
		case -2:
		    continue;	/* outside lseg AB pts */
		default:
		case -1:
		    continue;	/* Point not on lseg */
		case 1:
		    /* Point is at A (vu1a) by geometry */
		    if (hit_v && hit_v != vu1a->v_p) {
			nmg_repair_v_near_v(hit_v, vu1a->v_p,
					    is->on_eg, *eg1, 1, &(is->tol));
			bu_ptbl_free(&eg_list);
			goto re_tabulate;
		    }
		    hit_v = vu1a->v_p;
		    if (rt_g.NMG_debug & DEBUG_POLYSECT) bu_log("\thit_v = x%x (vu1a)\n", hit_v);
		    goto hit_a;
		case 2:
		    /* Point is at B (vu1b) by geometry */
		    if (hit_v && hit_v != vu1b->v_p) {
			nmg_repair_v_near_v(hit_v, vu1b->v_p,
					    is->on_eg, *eg1, 1, &(is->tol));
			bu_ptbl_free(&eg_list);
			goto re_tabulate;
		    }
		    hit_v = vu1b->v_p;
		    if (rt_g.NMG_debug & DEBUG_POLYSECT) bu_log("\thit_v = x%x (vu1b)\n", hit_v);
		    goto hit_b;
		case 3:
		    /* Point hits the line segment amidships! Split edge!
		     * If we don't have a hit vertex yet,
		     * search for one in whole model.
		     */
		    if (!hit_v) {
			hit_v = nmg_find_pt_in_model(fu1->s_p->r_p->m_p,
						     hit3d, &(is->tol));
			if (hit_v == vu1a->v_p || hit_v == vu1b->v_p)
			    bu_bomb("About to make 0-length edge!\n");
		    }
		    new_eu = nmg_ebreaker(hit_v, eu1, &is->tol);
		    bu_ptbl_ins_unique(eu1_list, (long *)&new_eu->l.magic);
		    /* "eu1" must now be considered invalid */
		    vu1_midpt = new_eu->vu_p;
		    if (!hit_v) {
			hit_v = vu1_midpt->v_p;
			nmg_vertex_gv(hit_v, hit3d);
			if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			    bu_log("\tmaking new vertex vu=x%x hit_v=x%x\n",
				   vu1_midpt, hit_v);
			}
			/* Before we loose track of the fact
			 * that this vertex lies on *both*
			 * lines, break any edges in the
			 * intersection line that cross it.
			 */
			if (is->on_eg) {
			    nmg_break_eg_on_v(is->on_eg,
					      hit_v, &(is->tol));
			}
		    } else {
			if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			    bu_log("\tre-using hit_v=x%x, vu=x%x\n", hit_v, vu1_midpt);
			}
			if (hit_v != vu1_midpt->v_p) bu_bomb("hit_v changed?\n");
		    }
		    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			bu_log("Faceuses after nmg_ebreaker() call\n");
			bu_log("fu1:\n");
			nmg_pr_fu_briefly(fu1, "\t");
			bu_log("fu2:\n");
			nmg_pr_fu_briefly(fu2, "\t");
		    }
		    nmg_enlist_vu(is, vu1_midpt, 0, dist[0]);
		    /* Neither old nor new edgeuse need further handling */
		    /* Because "eu1" is now invalid, restart loop. */
		    goto eu_search;
	    }
	}
    }

    /* Don't forget to do self loops with no edges */
    for (BU_LIST_FOR(lu1, loopuse, &fu1->lu_hd)) {
	struct vertexuse *vu1;

	if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) != NMG_VERTEXUSE_MAGIC) continue;

	/* Intersect line with lone vertex vu1 */
	vu1 = BU_LIST_FIRST(vertexuse, &lu1->down_hd);
	NMG_CK_VERTEXUSE(vu1);

	/* Needs to be a 3D comparison */
	if (bn_distsq_line3_pt3(is->pt, is->dir,
				vu1->v_p->vg_p->coord) > is->tol.dist_sq)
	    continue;

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("\tself-loop vu=x%x lies on line of intersection\n", vu1);
	}

	/* Break all edgeuses in the model on line on_eg, at vu1 */
	if (is->on_eg) {
	    nmg_break_eg_on_v(is->on_eg, vu1->v_p, &(is->tol));
	    nmg_enlist_vu(is, vu1, 0, MAX_FASTF);
	}

	/* FIXME: This can probably be removed */
	/* recent OLD WAY: */
	/* To prevent an OT_BOOLPLACE from being overlooked,
	 * break *both* sets of eu's
	 */

	/* Break all edges from fu1 list */
	for (eu1_index=0; eu1_index < BU_PTBL_END(eu1_list); eu1_index++) {
	    eu1 = (struct edgeuse *)BU_PTBL_GET(eu1_list, eu1_index);

	    /* This EU may have been killed by nmg_repair_v_near_v() */
	    if (eu1->l.magic != NMG_EDGEUSE_MAGIC)
		continue;

	    NMG_CK_EDGEUSE(eu1);
	    if (eu1->g.lseg_p != is->on_eg) continue;
	    /* eu1 is from fu1 and on the intersection line */
	    new_eu = nmg_break_eu_on_v(eu1, vu1->v_p, fu1, is);
	    if (!new_eu) continue;
	    bu_ptbl_ins_unique(eu1_list, (long *)&new_eu->l.magic);
	    nmg_enlist_vu(is, new_eu->vu_p, 0, MAX_FASTF);
	}

	/* Break all edges from fu2 list */
	for (eu2_index=0; eu2_index < BU_PTBL_END(eu2_list); eu2_index++) {
	    eu2 = (struct edgeuse *)BU_PTBL_GET(eu2_list, eu2_index);

	    /* This EU may have been killed by nmg_repair_v_near_v() */
	    if (eu2->l.magic != NMG_EDGEUSE_MAGIC)
		continue;

	    NMG_CK_EDGEUSE(eu2);
	    if (eu2->g.lseg_p != is->on_eg) continue;
	    /* eu2 is from fu2 and on the intersection line */
	    new_eu = nmg_break_eu_on_v(eu2, vu1->v_p, fu1, is);
	    if (!new_eu) continue;
	    bu_ptbl_ins_unique(eu2_list, (long *)&new_eu->l.magic);
	    nmg_enlist_vu(is, new_eu->vu_p, 0, MAX_FASTF);
	}

    }

    bu_ptbl_free(&eg_list);
}


/* XXX move to nmg_info.c */
/*
 * N M G _ I S _ E U _ O N _ L I N E 3
 */
int
nmg_is_eu_on_line3(const struct edgeuse *eu, const fastf_t *UNUSED(pt), const fastf_t *dir, const struct bn_tol *tol)
{
    struct edge_g_lseg *eg;

    NMG_CK_EDGEUSE(eu);
    BN_CK_TOL(tol);

    eg = eu->g.lseg_p;
    NMG_CK_EDGE_G_LSEG(eg);

    /* Ensure direction vectors are generally parallel */
    /* These are not unit vectors */
    /* tol->para and RT_DOT_TOL are too tight a tolerance.  0.1 is 5 degrees */
    if (fabs(VDOT(eg->e_dir, dir)) <
	0.9 * MAGNITUDE(eg->e_dir) * MAGNITUDE(dir)) return 0;

    /* XXX: this does not take pt+dir into account, a bug? */

    /* Ensure that vertices on edge are within tol of line */
    if (bn_distsq_line3_pt3(eg->e_pt, eg->e_dir,
			    eu->vu_p->v_p->vg_p->coord) > tol->dist_sq) return 0;
    if (bn_distsq_line3_pt3(eg->e_pt, eg->e_dir,
			    eu->eumate_p->vu_p->v_p->vg_p->coord) > tol->dist_sq) return 0;


    return 1;
}


/* XXX Move to nmg_info.c */
/**
 * N M G _ F I N D _ E G _ B E T W E E N _ 2 F G
 *
 * Perform a topology search to determine if two face geometries (specified
 * by their faceuses) share an edge geometry in common.
 * The edge_g is returned, even if there are no existing uses of it
 * in *either* fu1 or fu2.  It still represents the intersection line
 * between the two face geometries, found topologically.
 *
 * If there are multiple edgeuses in common, ensure that they all refer
 * to the same edge_g geometry structure.  The intersection of two planes
 * (non-coplanar) must be a single line.
 *
 * Calling this routine when the two faces share face geometry
 * is illegal.
 *
 * NULL is returned if no common edge geometry could be found.
 */
struct edge_g_lseg *
nmg_find_eg_between_2fg(const struct faceuse *ofu1, const struct faceuse *fu2, const struct bn_tol *tol)
{
    const struct faceuse *fu1;
    const struct loopuse *lu1;
    const struct face_g_plane *fg1;
    const struct face_g_plane *fg2;
    const struct face *f1;
    struct edgeuse *ret = (struct edgeuse *)NULL;
    int coincident;

    NMG_CK_FACEUSE(ofu1);
    NMG_CK_FACEUSE(fu2);
    BN_CK_TOL(tol);

    fg1 = ofu1->f_p->g.plane_p;
    fg2 = fu2->f_p->g.plane_p;
    NMG_CK_FACE_G_PLANE(fg1);
    NMG_CK_FACE_G_PLANE(fg2);

    if (fg1 == fg2) bu_bomb("nmg_find_eg_between_2fg() face_g_plane shared, infinitely many results\n");

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	nmg_pr_fus_in_fg(&fg1->magic);
	nmg_pr_fus_in_fg(&fg2->magic);
    }

    /* For all faces using fg1 */
    for (BU_LIST_FOR(f1, face, &fg1->f_hd)) {
	NMG_CK_FACE(f1);

	/* Arbitrarily pick one of the two fu's using f1 */
	fu1 = f1->fu_p;
	NMG_CK_FACEUSE(fu1);

	for (BU_LIST_FOR(lu1, loopuse, &fu1->lu_hd)) {
	    const struct edgeuse *eu1;
	    NMG_CK_LOOPUSE(lu1);
	    if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC)
		continue;
	    if (rt_g.NMG_debug & DEBUG_BASIC) {
		bu_log(" visiting lu1=x%x, fu1=x%x, fg1=x%x\n",
		       lu1, fu1, fg1);
	    }
	restart:
	    for (BU_LIST_FOR(eu1, edgeuse, &lu1->down_hd)) {
		struct edgeuse *eur;

		NMG_CK_EDGEUSE(eu1);
		/* Walk radially around the edge */
		for (
		    eur = eu1->radial_p;
		    eur != eu1->eumate_p;
		    eur = eur->eumate_p->radial_p
		    ) {
		    const struct faceuse *tfu;

		    if (*eur->up.magic_p != NMG_LOOPUSE_MAGIC) continue;
		    if (*eur->up.lu_p->up.magic_p != NMG_FACEUSE_MAGIC) continue;
		    tfu = eur->up.lu_p->up.fu_p;
		    if (tfu->f_p->g.plane_p != fg2) continue;
		    NMG_CK_EDGE_G_EITHER(eur->g.lseg_p);

		    /* Found the other face on this edge! */
		    if (rt_g.NMG_debug & DEBUG_BASIC) {
			bu_log(" Found shared edge, eur=x%x, eg=x%x\n", eur, eur->g.lseg_p);
			nmg_pr_eu_briefly(eur, (char *)NULL);
			nmg_pr_eu_briefly(eur->eumate_p, (char *)NULL);
			nmg_pr_eg(eur->g.magic_p, 0);
			nmg_pr_fu_around_eu(eur, tol);
		    }

		    if (!ret) {
			/* First common edge found */
			if (rt_g.NMG_debug & DEBUG_BASIC) {
			    nmg_pl_lu_around_eu(eur);
			}
			ret = eur;
			continue;
		    }

		    /* Previous edge found, check edge_g */
		    if (eur->g.lseg_p == ret->g.lseg_p) continue;

		    /* Edge geometry differs. vu's same? */
		    if (NMG_ARE_EUS_ADJACENT(eur, ret)) {
			if (rt_g.NMG_debug & DEBUG_BASIC) {
			    bu_log("nmg_find_eg_between_2fg() joining edges eur=x%x, ret=x%x\n",
				   eur, ret);
			}
			nmg_radial_join_eu(ret, eur, tol);
			goto restart;
		    }

		    /* This condition "shouldn't happen" */
		    bu_log("eur=x%x, eg_p=x%x;  ret=x%x, eg_p=x%x\n",
			   eur, eur->g.lseg_p,
			   ret, ret->g.lseg_p);
		    nmg_pr_eg(eur->g.magic_p, 0);
		    nmg_pr_eg(ret->g.magic_p, 0);
		    nmg_pr_eu_endpoints(eur, 0);
		    nmg_pr_eu_endpoints(ret, 0);

		    coincident = nmg_2edgeuse_g_coincident(eur, ret, tol);
		    if (coincident) {
			/* Change eur to use ret's eg */
			bu_log("nmg_find_eg_between_2fg() belatedly fusing e1=x%x, eg1=x%x, e2=x%x, eg2=x%x\n",
			       eur->e_p, eur->g.lseg_p,
			       ret->e_p, ret->g.lseg_p);
			nmg_jeg(ret->g.lseg_p, eur->g.lseg_p);
			/* See if there are any others. */
			nmg_model_fuse(nmg_find_model(&eur->l.magic), tol);
		    } else {
			bu_bomb("nmg_find_eg_between_2fg() 2 faces intersect with differing edge geometries?\n");
		    }
		    goto restart;
		}
	    }
	}
    }
    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_find_eg_between_2fg(fu1=x%x, fu2=x%x) edge_g=x%x\n",
	       ofu1, fu2, ret ? ret->g.lseg_p : 0);
    }
    if (ret)
	return ret->g.lseg_p;
    return (struct edge_g_lseg *)NULL;
}


/* XXX Move to nmg_info.c */
/**
 * N M G _ D O E S _ F U _ U S E _ E G
 *
 * See if any edgeuse in the given faceuse
 * lies on the indicated edge geometry (edge_g).
 * This is a topology check only.
 *
 * Returns -
 * NULL No
 * eu Yes, here is one edgeuse that does.  There may be more.
 */
struct edgeuse *
nmg_does_fu_use_eg(const struct faceuse *fu1, const uint32_t *eg)
{
    const struct loopuse *lu1;
    register struct edgeuse *eu1;

    NMG_CK_FACEUSE(fu1);
    NMG_CK_EDGE_G_EITHER(eg);

    for (BU_LIST_FOR(lu1, loopuse, &fu1->lu_hd)) {
	NMG_CK_LOOPUSE(lu1);
	if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC)
	    continue;
	if (rt_g.NMG_debug & DEBUG_BASIC) {
	    bu_log(" visiting lu1=x%x, fu1=x%x\n",
		   lu1, fu1);
	}
	for (BU_LIST_FOR(eu1, edgeuse, &lu1->down_hd)) {
	    if (eu1->g.magic_p == eg) goto out;
	}
    }
    eu1 = (struct edgeuse *)NULL;
out:
    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_does_fu_use_eg(fu1=x%x, eg=x%x) eu1=x%x\n",
	       fu1, eg, eu1);
    }
    return eu1;
}


/* XXX move to plane.c */
/**
 * R T _ L I N E _ O N _ P L A N E
 *
 * Returns -
 * 1 line is on plane, within tol
 * 0 line does not lie on the plane
 */
int
rt_line_on_plane(const fastf_t *pt, const fastf_t *dir, const fastf_t *plane, const struct bn_tol *tol)
{
    vect_t unitdir;
    fastf_t dist;

    BN_CK_TOL(tol);

    dist = DIST_PT_PLANE(pt, plane);
    if (!NEAR_ZERO(dist, tol->dist)) return 0;

    VMOVE(unitdir, dir);
    VUNITIZE(unitdir);
/* XXX This is *way* too tight TOO_STRICT */
    if (fabs(VDOT(unitdir, plane)) >= tol->para) {
	/* Vectors are parallel */
	/* ray parallel to plane, and point is on it */
	return 1;
    }
    return 0;
}


/**
 * N M G _ I S E C T _ T W O _ F A C E 3 P
 *
 * Handle the complete mutual intersection of
 * two 3-D non-coplanar planar faces,
 * including cutjoin and meshing.
 *
 * The line of intersection has already been computed.
 * Handle as two 2-D line/face intersection problems
 *
 * This is the HEART of the intersection code.
 */
static void
nmg_isect_two_face3p(struct nmg_inter_struct *is, struct faceuse *fu1, struct faceuse *fu2)
{
    struct bu_ptbl vert_list1, vert_list2;
    struct bu_ptbl eu1_list;	/* all eu's in fu1 */
    struct bu_ptbl eu2_list;	/* all eu's in fu2 */
    fastf_t *mag1=(fastf_t *)NULL;
    fastf_t *mag2=(fastf_t *)NULL;
    int i;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACEUSE(fu2);

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_two_face3p(fu1=x%x, fu2=x%x) START12\n", fu1, fu2);
	VPRINT("isect ray is->pt ", is->pt);
	VPRINT("isect ray is->dir", is->dir);
    }

    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_vfu(&fu1->s_p->fu_hd, fu1->s_p);
	nmg_vfu(&fu2->s_p->fu_hd, fu2->s_p);
	nmg_fu_touchingloops(fu1);
	nmg_fu_touchingloops(fu2);
    }

    /* Verify that line is within tolerance of both planes */
#ifdef TOO_STRICT
    NMG_GET_FU_PLANE(n1, fu1);
    if (!rt_line_on_plane(is->pt, is->dir, n1, &(is->tol)))
	bu_log("WARNING: intersect line not on plane of fu1\n");
    NMG_GET_FU_PLANE(n2, fu2);
    if (!rt_line_on_plane(is->pt, is->dir, n2, &(is->tol)))
	bu_log("WARNING: intersect line not on plane of fu2\n");
#endif

    if (rt_g.NMG_debug & (DEBUG_POLYSECT|DEBUG_FCUT|DEBUG_MESH)
	&& rt_g.NMG_debug & DEBUG_PLOTEM) {
	nmg_pl_2fu("Iface%d.pl", fu1, fu2, 0);
    }

    bu_ptbl_init(&vert_list1, 64, "vert_list1 buffer");
    bu_ptbl_init(&vert_list2, 64, "vert_list2 buffer");

    /* Build list of all edgeuses in fu1 and fu2 */
    nmg_edgeuse_tabulate(&eu1_list, &fu1->l.magic);
    nmg_edgeuse_tabulate(&eu2_list, &fu2->l.magic);

    is->mag_len = 2 * (BU_PTBL_END(&eu1_list) + BU_PTBL_END(&eu2_list));
    mag1 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "mag1");
    mag2 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "mag2");

    for (i=0; i<is->mag_len; i++) {
	mag1[i] = MAX_FASTF;
	mag2[i] = MAX_FASTF;
    }


    is->l1 = &vert_list1;
    is->l2 = &vert_list2;
    is->s1 = fu1->s_p;
    is->s2 = fu2->s_p;
    is->fu1 = fu1;
    is->fu2 = fu2;
    is->mag1 = mag1;
    is->mag2 = mag2;

    /*
     * Now do the intersection with the other face.
     */
    is->l2 = &vert_list1;
    is->l1 = &vert_list2;
    is->s2 = fu1->s_p;
    is->s1 = fu2->s_p;
    is->fu2 = fu1;
    is->fu1 = fu2;
    is->mag1 = mag2;
    is->mag2 = mag1;
/* nmg_isect_line2_face2pNEW(is, fu2, fu1, &eu2_list, &eu1_list); */
    is->on_eg = (struct edge_g_lseg *)NULL;
    nmg_isect_fu_jra(is, fu1, fu2, &eu1_list, &eu2_list);

    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_fu_touchingloops(fu1);
	nmg_fu_touchingloops(fu2);
	nmg_vfu(&fu1->s_p->fu_hd, fu1->s_p);
	nmg_vfu(&fu2->s_p->fu_hd, fu2->s_p);
    }

    if (rt_g.NMG_debug & DEBUG_FCUT) {
	bu_log("nmg_isect_two_face3p(fu1=x%x, fu2=x%x) vert_lists B:\n", fu1, fu2);
	nmg_pr_ptbl_vert_list("vert_list1", &vert_list1, mag1);
	nmg_pr_ptbl_vert_list("vert_list2", &vert_list2, mag2);
    }

    if (vert_list1.end == 0 && vert_list2.end == 0) {
	/* there were no intersections */
	goto out;
    }

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_two_face3p(fu1=x%x, fu2=x%x) MIDDLE\n", fu1, fu2);
    }

    is->on_eg = nmg_face_cutjoin(&vert_list1, &vert_list2, mag1, mag2, fu1, fu2, is->pt, is->dir, is->on_eg, &is->tol);

    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_fu_touchingloops(fu1);
	nmg_fu_touchingloops(fu2);
	nmg_region_v_unique(fu1->s_p->r_p, &is->tol);
	nmg_region_v_unique(fu2->s_p->r_p, &is->tol);
	nmg_vfu(&fu1->s_p->fu_hd, fu1->s_p);
	nmg_vfu(&fu2->s_p->fu_hd, fu2->s_p);
    }

    nmg_mesh_faces(fu1, fu2, &is->tol);
    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_fu_touchingloops(fu1);
	nmg_fu_touchingloops(fu2);
    }

out:
    (void)bu_ptbl_free(&vert_list1);
    (void)bu_ptbl_free(&vert_list2);
    (void)bu_ptbl_free(&eu1_list);
    (void)bu_ptbl_free(&eu2_list);
    if (mag1)
	bu_free((char *)mag1, "nmg_isect_two_face3p: mag1");
    if (mag2)
	bu_free((char *)mag2, "nmg_isect_two_face3p: mag2");


    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_vfu(&fu1->s_p->fu_hd, fu1->s_p);
	nmg_vfu(&fu2->s_p->fu_hd, fu2->s_p);
    }
    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_two_face3p(fu1=x%x, fu2=x%x) END\n", fu1, fu2);
	VPRINT("isect ray is->pt ", is->pt);
	VPRINT("isect ray is->dir", is->dir);
    }
}


void
nmg_cut_lu_into_coplanar_and_non(struct loopuse *lu, fastf_t *pl, struct nmg_inter_struct *is)
{
    struct model *m;
    struct edgeuse *eu;
    struct vertex_g *vg2;
    struct vertex *vcut1, *vcut2;
    struct bu_ptbl cut_list;
    fastf_t dist, dist2;
    int class1, class2;
    int in=0;
    int on=0;
    int out=0;
    int i;

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_cut_lu_into_coplanar_and_non(lu=x%x, pl=%g %g %g %g)\n", lu, V4ARGS(pl));

    NMG_CK_LOOPUSE(lu);
    NMG_CK_INTER_STRUCT(is);

    m = nmg_find_model(&is->fu1->l.magic);

    /* check if this loop even needs to be considered */
    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	return;

    bu_ptbl_init(&cut_list, 64, " &cut_list");

    eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
    vg2 = eu->vu_p->v_p->vg_p;
    NMG_CK_VERTEX_G(vg2);
    dist2 = DIST_PT_PLANE(vg2->coord, pl);
    if (dist2 > is->tol.dist) {
	class2 = NMG_CLASS_AoutB;
	out++;
    } else if (dist2 < (-is->tol.dist)) {
	class2 = NMG_CLASS_AinB;
	in++;
    } else {
	class2 = NMG_CLASS_AonBshared;
	on++;
    }

    vcut1 = (struct vertex *)NULL;
    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	class1 = class2;

	vg2 = eu->eumate_p->vu_p->v_p->vg_p;
	dist2 = DIST_PT_PLANE(vg2->coord, pl);

	if (dist2 > is->tol.dist) {
	    class2 = NMG_CLASS_AoutB;
	    out++;
	} else if (dist2 < (-is->tol.dist)) {
	    class2 = NMG_CLASS_AinB;
	    in++;
	} else {
	    class2 = NMG_CLASS_AonBshared;
	    on++;
	}

	if (class1 == NMG_CLASS_AonBshared && class2 != class1) {
	    if (!vcut1) {
		vcut1 = eu->vu_p->v_p;
	    } else if (vcut1 != eu->vu_p->v_p) {
		bu_ptbl_ins(&cut_list, (long *)vcut1);
		bu_ptbl_ins(&cut_list, (long *)eu->vu_p->v_p);
		vcut1 = (struct vertex *)NULL;
	    }
	} else if (class2 == NMG_CLASS_AonBshared && class1 != class2) {
	    if (!vcut1) {
		vcut1 = eu->eumate_p->vu_p->v_p;
	    } else if (vcut1 != eu->eumate_p->vu_p->v_p) {
		bu_ptbl_ins(&cut_list, (long *)vcut1);
		bu_ptbl_ins(&cut_list, (long *)eu->eumate_p->vu_p->v_p);
		vcut1 = (struct vertex *)NULL;
	    }
	}
    }

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("\t pl=(%g %g %g %g)\n", V4ARGS(pl));
	bu_log("\tcut_lists=%d, on=%d, in=%d, out=%d\n", BU_PTBL_END(&cut_list), on, in, out);
	if (BU_PTBL_END(&cut_list)) {
	    bu_log("\tcut_lists:\n");
	    for (i=0; i<BU_PTBL_END(&cut_list); i++) {
		struct vertex *v;

		v = (struct vertex *)BU_PTBL_GET(&cut_list, i);
		bu_log("\t\t%d, x%x\n", i+1, v);
	    }
	}
    }

    if (!on)
	return;

    if (BU_PTBL_END(&cut_list) < 2) {
	bu_ptbl_free(&cut_list);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("No loops need cutting\n");
	return;
    }

    if (nmg_loop_is_a_crack(lu)) {
	struct bu_ptbl lus;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("Loop is a crack\n");

	i = 0;
	while (i < BU_PTBL_END(&cut_list)) {
	    struct vertexuse *vu;

	    vcut1 = (struct vertex *)BU_PTBL_GET(&cut_list, i);
	    for (BU_LIST_FOR(vu, vertexuse, &vcut1->vu_hd)) {
		if (nmg_find_lu_of_vu(vu) != lu)
		    continue;

		eu = vu->up.eu_p;
		if (NMG_ARE_EUS_ADJACENT(eu, BU_LIST_PNEXT_CIRC(edgeuse, &eu->l))) {
		    i--;
		    bu_ptbl_rm(&cut_list, (long *)vcut1);
		} else if (NMG_ARE_EUS_ADJACENT(eu, BU_LIST_PPREV_CIRC(edgeuse, &eu->l))) {
		    i--;
		    bu_ptbl_rm(&cut_list, (long *)vcut1);
		}
	    }
	    i++;
	}

	if (BU_PTBL_END(&cut_list) == 0) {
	    bu_ptbl_free(&cut_list);

	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("no loops need cutting\n");
	    return;
	}

	bu_ptbl_init(&lus, 64, " &lus");
	bu_ptbl_ins(&lus, (long *)lu);
	for (i=0; i<BU_PTBL_END(&cut_list); i++) {
	    int j;

	    vcut1 = (struct vertex *)BU_PTBL_GET(&cut_list, i);

	    for (j=0; j<BU_PTBL_END(&lus); j++) {
		int did_split=0;
		struct loopuse *lu1;
		struct vertexuse *vu1;
		struct loopuse *new_lu;

		lu1 = (struct loopuse *)BU_PTBL_GET(&lus, j);

		for (BU_LIST_FOR(vu1, vertexuse, &vcut1->vu_hd)) {
		    if (nmg_find_lu_of_vu(vu1) == lu1) {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    bu_log("Splitting lu x%x at vu x%x\n", lu1, vu1);
			new_lu = nmg_split_lu_at_vu(lu1, vu1);
			nmg_lu_reorient(lu1);
			nmg_lu_reorient(new_lu);
			nmg_loop_g(new_lu->l_p, &is->tol);
			nmg_loop_g(lu1->l_p, &is->tol);
			bu_ptbl_ins(&lus, (long *)new_lu);
			did_split = 1;
			break;
		    }
		}
		if (did_split)
		    break;
	    }
	}
	bu_ptbl_free(&lus);
	bu_ptbl_free(&cut_list);
	return;
    }

    if (BU_PTBL_END(&cut_list)%2) {
	bu_log("Uneven number (%d) of vertices on cut list\n", BU_PTBL_END(&cut_list));
	bu_bomb("Uneven number of vertices on cut list");
    }

    /* Sort vertices on cut list into some order */
    if (BU_PTBL_END(&cut_list) > 2) {
	struct vertex *v1=(struct vertex *)NULL;
	struct vertex *v2=(struct vertex *)NULL;
	struct vertex *end1 = NULL, *end2 = NULL;
	fastf_t max_dist = 0.0;
	vect_t diff;
	fastf_t *dist_array;
	int done;

	/* find longest distance between two vertices */
	for (i=0; i<BU_PTBL_END(&cut_list); i++) {
	    int j;

	    v1 = (struct vertex *)BU_PTBL_GET(&cut_list, i);

	    for (j=i; j<BU_PTBL_END(&cut_list); j++) {
		fastf_t tmp_dist;

		v2 = (struct vertex *)BU_PTBL_GET(&cut_list, j);
		VSUB2(diff, v1->vg_p->coord, v2->vg_p->coord)
		    tmp_dist = MAGSQ(diff);
		if (tmp_dist > max_dist) {
		    max_dist = tmp_dist;
		    end1 = v1;
		    end2 = v2;
		}
	    }
	}
	if (!end1 || !end2) {
	    bu_log("nmg_cut_lu_into_coplanar_and_non: Cannot find endpoints\n");
	    bu_bomb("nmg_cut_lu_into_coplanar_and_non: Cannot find endpoints\n");
	}

	/* create array of distances (SQ) along the line from end1 to end2 */
	dist_array = (fastf_t *)bu_calloc(sizeof(fastf_t), BU_PTBL_END(&cut_list), "distance array");
	for (i=0; i<BU_PTBL_END(&cut_list); i++) {
	    v1 = (struct vertex *)BU_PTBL_GET(&cut_list, i);
	    if (v1 == end1) {
		dist_array[i] = 0.0;
		continue;
	    }
	    if (v1 == end2) {
		dist_array[i] = max_dist;
		continue;
	    }

	    VSUB2(diff, v1->vg_p->coord, end1->vg_p->coord)
		dist_array[i] = MAGSQ(diff);
	}

	/* sort vertices according to distance array */
	done = 0;
	while (!done) {
	    fastf_t tmp_dist;
	    long *tmp_v;

	    done = 1;
	    for (i=1; i<BU_PTBL_END(&cut_list); i++) {
		if (dist_array[i-1] <= dist_array[i])
		    continue;

		/* swap distances in array */
		tmp_dist = dist_array[i];
		dist_array[i] = dist_array[i-1];
		dist_array[i-1] = tmp_dist;

		/* swap vertices */
		tmp_v = cut_list.buffer[i];
		cut_list.buffer[i] = cut_list.buffer[i-1];
		cut_list.buffer[i-1] = tmp_v;

		done = 0;
	    }
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("After sorting:\n");
	    for (i=0; i<BU_PTBL_END(&cut_list); i++)
		bu_log("v=x%x, dist=%g\n", BU_PTBL_GET(&cut_list, i), dist_array[i]);
	}

	bu_free((char *)dist_array, "distance array");
    }

    for (i=0; i<BU_PTBL_END(&cut_list); i += 2) {
	struct loopuse *lu1;
	struct vertexuse *vu;
	vect_t dir;
	point_t hit_pt;
	struct vertex *hit_v;
	struct vertexuse *hit_vu;
	struct edgeuse *new_eu;
	fastf_t len;
	fastf_t inv_len;
	int skip=0;

	vcut1 = (struct vertex *)BU_PTBL_GET(&cut_list, i);
	vcut2 = (struct vertex *)BU_PTBL_GET(&cut_list, i+1);

	if (vcut1 == vcut2)
	    continue;

	/* make sure these are not the ends of an edge of lu */
	for (BU_LIST_FOR(vu, vertexuse, &vcut1->vu_hd)) {
	    if (nmg_find_lu_of_vu(vu) != lu)
		continue;

	    eu = vu->up.eu_p;
	    if (eu->eumate_p->vu_p->v_p == vcut2) {
		/* already an edge here */
		skip = 1;
		break;
	    }
	}
	if (skip)
	    continue;

	/* need to cut face along line from vcut1 to vcut2 */
	VMOVE(is->pt, vcut1->vg_p->coord);
	VSUB2(dir, vcut2->vg_p->coord, vcut1->vg_p->coord);
	len = MAGNITUDE(dir);
	if (len <= is->tol.dist)
	    continue;

	inv_len = 1.0/len;
	VSCALE(is->dir, dir, inv_len);

	/* add vertexuses to intersect list */
	bu_ptbl_reset(is->l1);

	/* add uses of vcut1 */
	for (BU_LIST_FOR(vu, vertexuse, &vcut1->vu_hd)) {
	    if (nmg_find_fu_of_vu(vu) == is->fu1)
		nmg_enlist_one_vu(is, vu, 0.0);
	}

	/* add uses of vcut2 */
	for (BU_LIST_FOR(vu, vertexuse, &vcut2->vu_hd)) {
	    if (nmg_find_fu_of_vu(vu) == is->fu1)
		nmg_enlist_one_vu(is, vu, len);
	}

	/* look for other edges that may intersect this line */
	for (BU_LIST_FOR(lu1, loopuse, &is->fu1->lu_hd)) {
	    struct edgeuse *eu1;

	    NMG_CK_LOOPUSE(lu1);

	    if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;

	    for (BU_LIST_FOR(eu1, edgeuse, &lu1->down_hd)) {
		int code;
		fastf_t dists[2];
		vect_t dir2;

		NMG_CK_EDGEUSE(eu1);

		VSUB2(dir2, eu1->eumate_p->vu_p->v_p->vg_p->coord, eu1->vu_p->v_p->vg_p->coord);
		code = bn_isect_lseg3_lseg3(dists, is->pt, dir,
					    eu1->vu_p->v_p->vg_p->coord, dir2, &is->tol);
		if (code < 0)
		    continue;

		if (code == 0) {
		    if (dists[0] > 0.0 && dists[0] < 1.0) {
			dist = dists[0]*len;
			for (BU_LIST_FOR(vu, vertexuse, &eu1->vu_p->v_p->vu_hd)) {
			    if (nmg_find_fu_of_vu(vu) == is->fu1)
				nmg_enlist_one_vu(is, vu, dist);
			}
		    }
		    if (dists[1] > 0.0 && dists[1] < 1.0) {
			dist = dists[1]*len;
			for (BU_LIST_FOR(vu, vertexuse, &eu1->eumate_p->vu_p->v_p->vu_hd)) {
			    if (nmg_find_fu_of_vu(vu) == is->fu1)
				nmg_enlist_one_vu(is, vu, len);
			}
		    }
		    continue;
		}

		/* normal intersection, may need to split eu1 */
		if (dists[0] <= 0.0 || dists[0] >= 1.0)
		    continue;

		if (dists[1] <= 0.0 || dists[1] >= 1.0)
		    continue;

		VJOIN1(hit_pt, is->pt, dists[0], dir);
		hit_vu = (struct vertexuse *)NULL;
		hit_v = (struct vertex *)NULL;

		hit_vu = nmg_find_pt_in_face(is->fu1, hit_pt, &is->tol);
		if (hit_vu)
		    hit_v = hit_vu->v_p;

		if (!hit_v)
		    hit_v = nmg_find_pt_in_model(m, hit_pt, &is->tol);
		new_eu = nmg_esplit(hit_v, eu1, 1);
		hit_v = new_eu->vu_p->v_p;
		if (!hit_v->vg_p)
		    nmg_vertex_gv(hit_v, hit_pt);

		/* add uses of hit_v to intersection list */
		dist = dists[0]*len;
		for (BU_LIST_FOR(vu, vertexuse, &hit_v->vu_hd)) {
		    if (nmg_find_fu_of_vu(vu) == is->fu1)
			nmg_enlist_one_vu(is, vu, dist);
		}
	    }
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("nmg_cut_lu_into_coplanar_and_non: calling face cutter\n");
	bu_ptbl_reset(is->l2);
	(void)nmg_face_cutjoin(is->l1, is->l2, is->mag1, is->mag2, is->fu1,
			       is->fu2, is->pt, is->dir, is->on_eg, &is->tol);

	vcut1 = (struct vertex *)NULL;
	vcut2 = (struct vertex *)NULL;
    }

    bu_ptbl_free(&cut_list);
}


/* nmg_isect_coplanar_edges is disabled because it is unused since
 * it was only called by nmg_isect_nearly_coplanar_faces which has
 * been disabled due to it no longer being used.
 */
#if 0
static void
nmg_isect_coplanar_edges(struct nmg_inter_struct *is, struct bu_ptbl *eu1_list, struct bu_ptbl *eu2_list)
{
    struct model *m;
    struct loopuse *lu;
    struct bu_ptbl v_list;
    int i, j;
    plane_t pl1, pl2;

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_coplanar_edges START\n");

    NMG_CK_INTER_STRUCT(is);
    BU_CK_PTBL(eu1_list);
    BU_CK_PTBL(eu2_list);

    m = nmg_find_model(&is->fu1->l.magic);

    NMG_GET_FU_PLANE(pl1, is->fu1);
    NMG_GET_FU_PLANE(pl2, is->fu2);

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("pl1 = %g %g %g %g\n", V4ARGS(pl1));
	bu_log("pl2 = %g %g %g %g\n", V4ARGS(pl2));
    }

    /* First split all edgeuses that intersect */
    for (i=0; i<BU_PTBL_END(eu1_list); i++) {
	struct edgeuse *eu1;
	double len_vt1;
	vect_t vt1;
	struct vertex_g *vg1a, *vg1b;

	eu1 = (struct edgeuse *)BU_PTBL_GET(eu1_list, i);
	NMG_CK_EDGEUSE(eu1);

	vg1a = eu1->vu_p->v_p->vg_p;
	NMG_CK_VERTEX_G(vg1a);
	vg1b = eu1->eumate_p->vu_p->v_p->vg_p;
	NMG_CK_VERTEX_G(vg1b);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("Considering EU x%x (%g %g %g) <-> (%g %g %g)\n",
		   eu1, V3ARGS(vg1a->coord), V3ARGS(vg1b->coord));

	VSUB2(vt1, vg1b->coord, vg1a->coord);
	len_vt1 = MAGNITUDE(vt1);
	VSCALE(vt1, vt1, 1.0/len_vt1);

	for (j=0; j<BU_PTBL_END(eu2_list); j++) {
	    struct edgeuse *eu2 = NULL;
	    struct vertex_g *vg2a = NULL;
	    struct vertex_g *vg2b = NULL;
	    int code = 0;
	    vect_t vt2 = VINIT_ZERO;
	    double len_vt2 = 0.0;
	    fastf_t dist[2] = V2INIT_ZERO;
	    point_t hit_pt = VINIT_ZERO;
	    int hit_no = 0;
	    int hit_count = 0;

	    struct vertex *hitv = NULL;
	    struct vertexuse *hit_vu = NULL;

	    eu2 = (struct edgeuse *)BU_PTBL_GET(eu2_list, j);
	    NMG_CK_EDGEUSE(eu2);

	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\tConsidering EU2 x%x (%g %g %g) <-> (%g %g %g)\n",
		       eu2, V3ARGS(eu2->vu_p->v_p->vg_p->coord), V3ARGS(eu2->eumate_p->vu_p->v_p->vg_p->coord));

	    /* if these edges are radial, nothing to do */
	    if (eu1->vu_p->v_p == eu2->vu_p->v_p &&
		eu1->eumate_p->vu_p->v_p == eu2->eumate_p->vu_p->v_p)
		continue;

	    if (eu1->vu_p->v_p == eu2->eumate_p->vu_p->v_p &&
		eu1->eumate_p->vu_p->v_p == eu2->vu_p->v_p)
		continue;

	    vg2a = eu2->vu_p->v_p->vg_p;
	    NMG_CK_VERTEX_G(vg2a);
	    vg2b = eu2->eumate_p->vu_p->v_p->vg_p;
	    NMG_CK_VERTEX_G(vg2b);
	    VSUB2(vt2, vg2b->coord, vg2a->coord);
	    len_vt2 = MAGNITUDE(vt2);
	    VSCALE(vt2, vt2, 1.0/len_vt2);

	    code = bn_dist_line3_line3(dist, vg1a->coord, vt1,
				       vg2a->coord, vt2, &is->tol);

	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\tcode = %d\n", code);

	    if (code == (-2) || code == 1)
		continue;

	    if (code == (-1)) {
		fastf_t tmp;
		vect_t tmp_vect;
		fastf_t dist1[2];

		hit_count = 0;

		/* lines are colinear */
		VSUB2(tmp_vect, vg2a->coord, vg1a->coord);
		dist1[0] = VDOT(tmp_vect, vt1);
		if (NEAR_ZERO(dist1[0], is->tol.dist))
		    dist1[0] = 0.0;
		else if (NEAR_EQUAL(dist1[0], len_vt1, is->tol.dist))
		    dist1[0] = len_vt1;
		VSUB2(tmp_vect, vg2b->coord, vg1a->coord);
		dist1[1] = VDOT(tmp_vect, vt1);
		if (NEAR_ZERO(dist1[1], is->tol.dist))
		    dist1[1] = 0.0;
		else if (NEAR_EQUAL(dist1[1], len_vt1, is->tol.dist))
		    dist1[1] = len_vt1;

		if ((dist1[0] >= 0.0 && dist1[0] <= len_vt1)) {
		    dist[hit_count] = dist1[0];
		    hit_count++;
		}
		if ((dist1[1] >= 0.0 && dist1[1] <= len_vt1)) {
		    dist[hit_count] = dist1[1];
		    hit_count++;
		}

		if (hit_count == 0)
		    continue;

		if (hit_count == 2 && dist[0] < dist[1]) {
		    tmp = dist[0];
		    dist[0] = dist[1];
		    dist[1] = tmp;
		}
	    } else {
		if (NEAR_ZERO(dist[0], is->tol.dist))
		    dist[0] = 0.0;
		else if (NEAR_EQUAL(dist[0], len_vt1, is->tol.dist))
		    dist[0] = len_vt1;
		if (NEAR_ZERO(dist[1], is->tol.dist))
		    dist[1] = 0.0;
		else if (NEAR_EQUAL(dist[1], len_vt2, is->tol.dist))
		    dist[1] = len_vt2;
		if (dist[0] < 0.0 || dist[0] > len_vt1)
		    continue;
		if (dist[1] < 0.0 || dist[1] > len_vt2)
		    continue;
		hit_count = 1;
	    }

	    for (hit_no=0; hit_no < hit_count; hit_no++) {
		struct edgeuse *new_eu;

		if (rt_g.NMG_debug & DEBUG_POLYSECT)
		    bu_log("\tdist[%d] = %g\n", hit_no, dist[hit_no]);

		hitv = (struct vertex *)NULL;
		hit_vu = (struct vertexuse *)NULL;

		if (ZERO(dist[hit_no])) {
		    hit_vu = eu1->vu_p;
		    hitv = hit_vu->v_p;
		    VMOVE(hit_pt, hitv->vg_p->coord);
		} else if (ZERO(dist[hit_no] - len_vt1)) {
		    hit_vu = eu1->eumate_p->vu_p;
		    hitv = hit_vu->v_p;
		    VMOVE(hit_pt, hitv->vg_p->coord);
		} else
		    VJOIN1(hit_pt, vg1a->coord, dist[hit_no], vt1)

			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    bu_log("eus x%x and x%x intersect #%d at (%g %g %g)\n",
				   eu1, eu2, hit_no, V3ARGS(hit_pt));

		if (!hit_vu)
		    hit_vu = nmg_find_pt_in_face(is->fu2, hit_pt, &is->tol);

		if (!hit_vu)
		    hitv = nmg_find_pt_in_model(nmg_find_model(&is->fu1->l.magic), hit_pt, &is->tol);
		else
		    hitv = hit_vu->v_p;

		if (rt_g.NMG_debug & DEBUG_POLYSECT && hitv)
		    bu_log("Found vertex (x%x) at hit_pt\n", hitv);

		if (hitv != eu1->vu_p->v_p && hitv != eu1->eumate_p->vu_p->v_p) {
		    if (rt_g.NMG_debug & DEBUG_POLYSECT)
			bu_log("Splitting eu1 x%x\n", eu1);
		    new_eu = nmg_esplit(hitv, eu1, 1);
		    hitv = new_eu->vu_p->v_p;
		    if (!hitv->vg_p)
			nmg_vertex_gv(hitv, hit_pt);
		    vg1b = eu1->eumate_p->vu_p->v_p->vg_p;
		    VSUB2(vt1, vg1b->coord, vg1a->coord);
		    len_vt1 = MAGNITUDE(vt1);
		    VSCALE(vt1, vt1, 1.0/len_vt1);
		    bu_ptbl_ins(eu1_list, (long *)new_eu);
		}
		if (code == 0 && hitv != eu2->vu_p->v_p && hitv != eu2->eumate_p->vu_p->v_p) {
		    if (rt_g.NMG_debug & DEBUG_POLYSECT)
			bu_log("Splitting eu2 x%x at hitv = x%x\n",  eu2, hitv);
		    new_eu = nmg_esplit(hitv, eu2, 1);
		    hitv = new_eu->vu_p->v_p;
		    if (!hitv->vg_p)
			nmg_vertex_gv(hitv, hit_pt);
		    bu_ptbl_ins(eu2_list, (long *)new_eu);
		}

		if (hitv)
		    (void)nmg_break_all_es_on_v(&m->magic, hitv, &is->tol);
	    }
	}
    }

    bu_ptbl_free(eu1_list);
    bu_ptbl_free(eu2_list);

    /* Make sure every vertex in fu1 has dual in fu2
     * (if they overlap)
     */
    nmg_vertex_tabulate(&v_list, &is->fu1->l.magic);

    for (i=0; i<BU_PTBL_END(&v_list); i++) {
	struct vertex *v;
	int class;

	v = (struct vertex *)BU_PTBL_GET(&v_list, i);
	NMG_CK_VERTEX(v);

	if (nmg_find_v_in_face(v, is->fu2))
	    continue;

	/* Check if this vertex is within other FU */
	if (!NEAR_ZERO(DIST_PT_PLANE(v->vg_p->coord, pl2), is->tol.dist))
	    continue;

	class = nmg_class_pt_fu_except(v->vg_p->coord, is->fu2, NULL, NULL, NULL,
				       (char *)NULL, 0, 0, &is->tol);

	if (class == NMG_CLASS_AinB) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("Making dualvu of vertex x%x in is->fu2 x%x\n", v, is->fu2);
	    (void)nmg_make_dualvu(v, is->fu2, &is->tol);
	}
    }
    bu_ptbl_reset(&v_list);

    /* same for fu2 */
    nmg_vertex_tabulate(&v_list, &is->fu2->l.magic);

    for (i=0; i<BU_PTBL_END(&v_list); i++) {
	struct vertex *v;
	int class;

	v = (struct vertex *)BU_PTBL_GET(&v_list, i);
	NMG_CK_VERTEX(v);

	if (nmg_find_v_in_face(v, is->fu1))
	    continue;

	/* Check if this vertex is within other FU */
	if (!NEAR_ZERO(DIST_PT_PLANE(v->vg_p->coord, pl1), is->tol.dist))
	    continue;

	class = nmg_class_pt_fu_except(v->vg_p->coord, is->fu1, NULL, NULL, NULL,
				       (char *)NULL, 0, 0, &is->tol);

	if (class == NMG_CLASS_AinB) {
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("Making dualvu of vertex x%x in fu1 x%x\n", v, is->fu1);
	    (void)nmg_make_dualvu(v, is->fu1, &is->tol);
	}
    }

    bu_ptbl_free(&v_list);

    bu_ptbl_reset(is->l1);
    bu_ptbl_reset(is->l2);

    for (BU_LIST_FOR(lu, loopuse, &is->fu1->lu_hd)) {
	struct edgeuse *eu;

	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    struct vertexuse *vu;
	    struct vertex *v1, *v2;

	    NMG_CK_EDGEUSE(eu);

	    v1 = eu->vu_p->v_p;
	    NMG_CK_VERTEX(v1);
	    v2 = eu->eumate_p->vu_p->v_p;
	    NMG_CK_VERTEX(v2);

	    if (!NEAR_ZERO(DIST_PT_PLANE(v1->vg_p->coord, pl2), is->tol.dist))
		continue;
	    if (!NEAR_ZERO(DIST_PT_PLANE(v2->vg_p->coord, pl2), is->tol.dist))
		continue;

	    if (!nmg_find_v_in_face(v1, is->fu2) ||
		!nmg_find_v_in_face(v2, is->fu2))
		continue;

	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("Making EU x%x an intersect line for face cutting\n", eu);

	    for (BU_LIST_FOR(vu, vertexuse, &v1->vu_hd)) {
		struct faceuse *fu;

		fu = nmg_find_fu_of_vu(vu);

		if (fu == is->fu2)
		    nmg_enlist_one_vu(is, vu, 0.0);
	    }

	    for (BU_LIST_FOR(vu, vertexuse, &v2->vu_hd)) {
		struct faceuse *fu;

		fu = nmg_find_fu_of_vu(vu);

		if (fu == is->fu2)
		    nmg_enlist_one_vu(is, vu, 1.0);
	    }

	    /* Now do face cutting */
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("Calling face cutter for fu2 x%x\n", is->fu2);
	    nmg_fcut_face_2d(is->l2, is->mag2, is->fu2, is->fu1, &is->tol);

	    bu_ptbl_reset(is->l1);
	    bu_ptbl_reset(is->l2);
	}
    }

    for (BU_LIST_FOR(lu, loopuse, &is->fu2->lu_hd)) {
	struct edgeuse *eu;

	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    struct vertexuse *vu;
	    struct vertex *v1, *v2;

	    NMG_CK_EDGEUSE(eu);

	    v1 = eu->vu_p->v_p;
	    NMG_CK_VERTEX(v1);
	    v2 = eu->eumate_p->vu_p->v_p;
	    NMG_CK_VERTEX(v2);

	    if (!NEAR_ZERO(DIST_PT_PLANE(v1->vg_p->coord, pl1), is->tol.dist))
		continue;
	    if (!NEAR_ZERO(DIST_PT_PLANE(v2->vg_p->coord, pl1), is->tol.dist))
		continue;

	    if (!nmg_find_v_in_face(v1, is->fu1) ||
		!nmg_find_v_in_face(v2, is->fu1))
		continue;

	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("Making EU x%x an intersect line for face cutting\n", eu);

	    for (BU_LIST_FOR(vu, vertexuse, &v1->vu_hd)) {
		struct faceuse *fu;

		fu = nmg_find_fu_of_vu(vu);

		if (fu == is->fu1)
		    nmg_enlist_one_vu(is, vu, 0.0);
	    }

	    for (BU_LIST_FOR(vu, vertexuse, &v2->vu_hd)) {
		struct faceuse *fu;

		fu = nmg_find_fu_of_vu(vu);

		if (fu == is->fu1)
		    nmg_enlist_one_vu(is, vu, 1.0);
	    }

	    /* Now do face cutting */
	    if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("Calling face cutter for fu1 x%x\n", is->fu1);
	    nmg_fcut_face_2d(is->l1, is->mag1, is->fu1, is->fu2, &is->tol);

	    bu_ptbl_reset(is->l1);
	    bu_ptbl_reset(is->l2);
	}
    }
}
#endif


#define MAX_FACES 200
void
nmg_check_radial_angles(char *str, struct shell *s, const struct bn_tol *tol)
{
    struct bu_ptbl edges;
    vect_t xvec, yvec, zvec;
    int i, j;
    double angle[MAX_FACES];
    struct faceuse *fus[MAX_FACES];
    int face_count;
    int increasing;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    bu_ptbl_init(&edges, 64, " &edges");
    nmg_edge_tabulate(&edges, &s->l.magic);

    for (i=0; i<BU_PTBL_END(&edges); i++) {
	struct edge *e;
	struct edgeuse *eu_start;
	struct edgeuse *eu;
	struct faceuse *fu;
	int start;

	e = (struct edge *)BU_PTBL_GET(&edges,  i);
	NMG_CK_EDGE(e);

	eu_start = e->eu_p;
	NMG_CK_EDGEUSE(eu_start);

	eu = eu_start;
	do {
	    fu = nmg_find_fu_of_eu(eu);
	} while (!fu && eu != eu_start);

	if (!fu)
	    continue;

	eu_start = eu;
	if (nmg_find_eu_leftvec(xvec, eu))
	    bu_bomb("nmg_check_radial_angles() eu not part of a face!!");

	VSUB2(zvec, eu->eumate_p->vu_p->v_p->vg_p->coord, eu->vu_p->v_p->vg_p->coord);
	VUNITIZE(zvec);
	VCROSS(yvec, zvec, xvec);

	face_count = 0;

	eu = eu_start;
	do {
	    fu = nmg_find_fu_of_eu(eu);
	    if (fu) {
		if (face_count >= MAX_FACES) {
		    bu_log("Too many faces in nmg_check_radial_angles (%d)\n", face_count);
		    bu_bomb("Too many faces in nmg_check_radial_angles\n");
		}
		angle[face_count] = nmg_measure_fu_angle(eu, xvec, yvec, zvec);
		fus[face_count] = fu;
		face_count++;
	    }
	    eu = eu->eumate_p->radial_p;
	} while (eu != eu_start);

	/* list of angles should be monotonically increasing or decreasing */
	increasing =  (-1);
	start = 1;
	for (j=2; j<=face_count; j++) {
	    if (ZERO(angle[j]) || ZERO(angle[j] - bn_twopi))
		continue;
	    if (ZERO(angle[j] - angle[j-1]))
		continue;
	    else if (angle[j] > angle[j-1]) {
		start = j;
		increasing = 1;
		break;
	    } else {
		start = j;
		increasing = 0;
		break;
	    }
	}

	if (increasing == (-1))
	    continue;

	for (j=start+1; j<face_count; j++) {
	    if ((increasing && angle[j] < angle[j-1]) ||
		(!increasing && angle[j] > angle[j-1])) {
		bu_log("%s", str);
		bu_log("nmg_check_radial_angles(): angles not monotonically increasing or decreasing\n");
		bu_log("start=%d, increasing = %d\n", start, increasing);
		bu_log("\tfaces around eu x%x\n", eu_start);
		for (j=0; j<face_count; j++)
		    bu_log("\t\tfu=x%x, angle=%g\n", fus[j], angle[j]*180.0/bn_pi);
		bu_bomb("nmg_check_radial_angles(): angles not monotonically increasing or decreasing\n");
	    }
	}
    }
    bu_ptbl_free(&edges);
}


#if 0
/* unused due to change to function 'nmg_isect_two_generic_faces' */
/** N M G _ I S E C T _ N E A R L Y _ C O P L A N A R _ F A C E S
 *
 * The two faceuses passed are expected to be parallel and distinct or coplanar
 * according to bn_isect_2planes(). Also, some (but not all) of the vertices in
 * one faceuse are within tolerance of the other faceuse. This case is singled
 * out in nmg_isect_two_generic_faces().
 *
 * The algorithm is:
 *		1. split any edges that pass from beyond tolerance on one side
 *		   of the other faceuse to beyond tolerance on the other side.
 *		2. cut all loops in each faceuse such that the resulting loops are
 *		   either entirely within tolerance of the other faceuse, or share
 *		   only one "line of intersection" with the other faceuse.
 *		3. intersect coplanar loops same as done in nmg_isect_two_face2p_jra().
 */
static void
nmg_isect_nearly_coplanar_faces(struct nmg_inter_struct *is, struct faceuse *fu1, struct faceuse *fu2)
{
    int i;
    struct model *m;
    struct edgeuse *eu;
    struct loopuse *lu;
    struct bu_ptbl loops;
    plane_t pl1;
    plane_t pl2;
    fastf_t *mag1, *mag2;
    struct bu_ptbl eu1_list;
    struct bu_ptbl vert_list1;
    struct bu_ptbl eu2_list;
    struct bu_ptbl vert_list2;
    struct bu_ptbl verts;

    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACEUSE(fu2);
    NMG_CK_INTER_STRUCT(is);

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_nearly_coplanar_faces(fu1=x%x, fu2=x%x)\n", fu1, fu2);

    m = nmg_find_model(&fu1->l.magic);
    NMG_CK_MODEL(m);

    NMG_GET_FU_PLANE(pl1, fu1);
    NMG_GET_FU_PLANE(pl2, fu2);

    nmg_edgeuse_tabulate(&eu1_list, &fu1->l.magic);
    nmg_edgeuse_tabulate(&eu2_list, &fu2->l.magic);

    is->mag_len = 2 * (BU_PTBL_END(&eu1_list) + BU_PTBL_END(&eu2_list));
    mag1 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "mag1");
    mag2 = (fastf_t *)bu_calloc(is->mag_len, sizeof(fastf_t), "mag2");

    for (i=0; i<is->mag_len; i++) {
	mag1[i] = MAX_FASTF;
	mag2[i] = MAX_FASTF;
    }

    bu_ptbl_init(&vert_list1, 64, " &vert_list1");
    bu_ptbl_init(&vert_list2, 64, " &vert_list2");

    is->s1 = fu1->s_p;
    is->s2 = fu2->s_p;
    is->fu1 = fu1;
    is->fu2 = fu2;
    is->l1 = &vert_list1;
    is->l2 = &vert_list2;
    is->mag1 = mag1;
    is->mag2 = mag2;
    is->on_eg = (struct edge_g_lseg *)NULL;

    /* split any edges that pass through the plane of the other faceuse */
    for (BU_LIST_FOR(lu, loopuse, &fu1->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    int code;
	    int class1, class2;
	    fastf_t dist;
	    struct vertex_g *vg1, *vg2;
	    vect_t dir;
	    point_t hit_pt;
	    struct vertexuse *hit_vu;
	    struct vertex *hit_v=(struct vertex *)NULL;
	    struct edgeuse *new_eu;

	    NMG_CK_EDGEUSE(eu);

	    vg1 = eu->vu_p->v_p->vg_p;
	    vg2 = eu->eumate_p->vu_p->v_p->vg_p;

	    dist = DIST_PT_PLANE(vg1->coord, pl2);
	    if (dist > is->tol.dist)
		class1 = NMG_CLASS_AoutB;
	    else if (dist < (-is->tol.dist))
		class1 = NMG_CLASS_AinB;
	    else
		class1 = NMG_CLASS_AonBshared;

	    dist = DIST_PT_PLANE(vg2->coord, pl2);
	    if (dist > is->tol.dist)
		class2 = NMG_CLASS_AoutB;
	    else if (dist < (-is->tol.dist))
		class2 = NMG_CLASS_AinB;
	    else
		class2 = NMG_CLASS_AonBshared;

	    if (class1 == class2)
		continue;

	    if (class1 == NMG_CLASS_AonBshared || class2 == NMG_CLASS_AonBshared)
		continue;

	    /* need to split this edge at plane pl2 */
	    VSUB2(dir, vg2->coord, vg1->coord)

		code = bn_isect_line3_plane(&dist, vg1->coord, dir, pl2, &is->tol);
	    if (code < 1) {
		bu_log("nmg_isect_nearly_coplanar_faces: EU (x%x) goes from %s to %s\n",
		       eu, nmg_class_name(class1), nmg_class_name(class2));
		bu_log("But bn_isect_line3_plane() returns %d\n", code);
		bu_log("pl2 = (%g %g %g %g)\n", V4ARGS(pl2));
		nmg_pr_lu_briefly(lu, "");
		bu_bomb("nmg_isect_nearly_coplanar_faces: BAD EU");
	    }

	    if (dist <= 0.0 || dist >= 1.0) {
		bu_log("nmg_isect_nearly_coplanar_faces: EU (x%x) goes from %s to %s\n",
		       eu, nmg_class_name(class1), nmg_class_name(class2));
		bu_log("But bn_isect_line3_plane() returns %d and dist=%g\n", code, dist);
		bu_log("pl2 = (%g %g %g %g)\n", V4ARGS(pl2));
		nmg_pr_lu_briefly(lu, "");
		bu_bomb("nmg_isect_nearly_coplanar_faces: BAD EU");
	    }

	    VJOIN1(hit_pt, vg1->coord, dist, dir);

	    hit_vu = nmg_find_pt_in_face(fu2, hit_pt, &is->tol);
	    if (!hit_vu)
		hit_v = nmg_find_pt_in_model(m, hit_pt, &is->tol);
	    else
		hit_v = hit_vu->v_p;

	    new_eu = nmg_esplit(hit_v, eu, 1);
	    hit_v = new_eu->vu_p->v_p;
	    if (!hit_v->vg_p)
		nmg_vertex_gv(hit_v, hit_pt);

	    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		bu_log("nmg_cut_lu_into_coplanar_and_non:\n");
		bu_log("\tsplitting eu x%x at v=x%x (%g %g %g)\n",
		       eu, hit_v, V3ARGS(hit_v->vg_p->coord));
	    }

	}
    }

    /* get a list of all the loops in this faceuse */
    bu_ptbl_init(&loops, 64, " &loops");
    for (BU_LIST_FOR(lu, loopuse, &fu1->lu_hd))
	bu_ptbl_ins(&loops, (long *)lu);

    /* cut each loop so that every loop ends up either entirely on the other
     * face, or with only a "line of intersection" in common
     */
    for (i=0; i<BU_PTBL_END(&loops); i++) {
	lu = (struct loopuse *)BU_PTBL_GET(&loops, i);
	NMG_CK_LOOPUSE(lu);

	nmg_cut_lu_into_coplanar_and_non(lu, pl2, is);
    }

    /* same for fu2 */
    for (BU_LIST_FOR(lu, loopuse, &fu2->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    int code;
	    int class1, class2;
	    fastf_t dist;
	    struct vertex_g *vg1, *vg2;
	    vect_t dir;
	    point_t hit_pt;
	    struct vertexuse *hit_vu;
	    struct vertex *hit_v=(struct vertex *)NULL;
	    struct edgeuse *new_eu;

	    NMG_CK_EDGEUSE(eu);

	    vg1 = eu->vu_p->v_p->vg_p;
	    vg2 = eu->eumate_p->vu_p->v_p->vg_p;

	    dist = DIST_PT_PLANE(vg1->coord, pl1);
	    if (dist > is->tol.dist)
		class1 = NMG_CLASS_AoutB;
	    else if (dist < (-is->tol.dist))
		class1 = NMG_CLASS_AinB;
	    else
		class1 = NMG_CLASS_AonBshared;

	    dist = DIST_PT_PLANE(vg2->coord, pl1);
	    if (dist > is->tol.dist)
		class2 = NMG_CLASS_AoutB;
	    else if (dist < (-is->tol.dist))
		class2 = NMG_CLASS_AinB;
	    else
		class2 = NMG_CLASS_AonBshared;

	    if (class1 == class2)
		continue;

	    if (class1 == NMG_CLASS_AonBshared || class2 == NMG_CLASS_AonBshared)
		continue;

	    /* need to split this edge at plane pl1 */
	    VSUB2(dir, vg2->coord, vg1->coord)

		code = bn_isect_line3_plane(&dist, vg1->coord, dir, pl1, &is->tol);
	    if (code < 1) {
		bu_log("nmg_isect_nearly_coplanar_faces: EU (x%x) goes from %s to %s\n",
		       eu, nmg_class_name(class1), nmg_class_name(class2));
		bu_log("But bn_isect_line3_plane() returns %d\n", code);
		bu_log("pl1 = (%g %g %g %g)\n", V4ARGS(pl1));
		nmg_pr_lu_briefly(lu, "");
		bu_bomb("nmg_isect_nearly_coplanar_faces: BAD EU");
	    }

	    if (dist <= 0.0 || dist >= 1.0) {
		bu_log("nmg_isect_nearly_coplanar_faces: EU (x%x) goes from %s to %s\n",
		       eu, nmg_class_name(class1), nmg_class_name(class2));
		bu_log("But bn_isect_line3_plane() returns %d and dist=%g\n", code, dist);
		bu_log("pl1 = (%g %g %g %g)\n", V4ARGS(pl1));
		nmg_pr_lu_briefly(lu, "");
		bu_bomb("nmg_isect_nearly_coplanar_faces: BAD EU");
	    }
	    VJOIN1(hit_pt, vg1->coord, dist, dir);

	    hit_vu = nmg_find_pt_in_face(fu2, hit_pt, &is->tol);
	    if (!hit_vu)
		hit_v = nmg_find_pt_in_model(m, hit_pt, &is->tol);
	    else
		hit_v = hit_vu->v_p;

	    new_eu = nmg_esplit(hit_v, eu, 1);
	    hit_v = new_eu->vu_p->v_p;
	    if (!hit_v->vg_p)
		nmg_vertex_gv(hit_v, hit_pt);
	}
    }

    /* get a list of all the loops in this faceuse */
    bu_ptbl_reset(&loops);
    for (BU_LIST_FOR(lu, loopuse, &fu2->lu_hd))
	bu_ptbl_ins(&loops, (long *)lu);

    /* cut each loop so that every loop ends up either entirely on the other
     * face, or with only a "line of intersection" in common
     */
    for (i=0; i<BU_PTBL_END(&loops); i++) {
	lu = (struct loopuse *)BU_PTBL_GET(&loops, i);
	NMG_CK_LOOPUSE(lu);

	nmg_cut_lu_into_coplanar_and_non(lu, pl1, is);
    }

    /* Need to break edges in faces on vertices that may lie on new edges formed by cuts */
    nmg_vertex_tabulate(&verts, &m->magic);
    /* split new edges in fu1 */
    for (BU_LIST_FOR(lu, loopuse, &fu1->lu_hd)) {
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    if (bu_ptbl_locate(&eu1_list, (long *)eu) != (-1))
		continue;

	    /* this is a new edgeuse, check if it should be split */
	    for (i=0; i<BU_PTBL_END(&verts); i++) {
		struct vertex *v;
		int code;
		fastf_t dist[2];
		point_t pca;

		v = (struct vertex *)BU_PTBL_GET(&verts, i);

		if (v == eu->vu_p->v_p)
		    continue;

		if (v == eu->eumate_p->vu_p->v_p)
		    continue;

		code = bn_dist_pt3_lseg3(dist, pca, eu->vu_p->v_p->vg_p->coord,
					 eu->eumate_p->vu_p->v_p->vg_p->coord, v->vg_p->coord, &is->tol);

		if (code > 2)
		    continue;

		if (code == 1)
		    bu_log("nmg_isect_nearly_coplanar_faces: vertices should have been fused x%x and x%x\n", v, eu->vu_p->v_p);
		else if (code == 2)
		    bu_log("nmg_isect_nearly_coplanar_faces: vertices should have been fused x%x and x%x\n", v, eu->eumate_p->vu_p->v_p);
		else {
		    /* need to split EU at V */
		    (void)nmg_esplit(v, eu, 1);
		}
	    }
	}
    }

    /* split new edges in fu2 */
    for (BU_LIST_FOR(lu, loopuse, &fu2->lu_hd)) {
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    if (bu_ptbl_locate(&eu1_list, (long *)eu) != (-1))
		continue;

	    /* this is a new edgeuse, check if it should be split */
	    for (i=0; i<BU_PTBL_END(&verts); i++) {
		struct vertex *v;
		int code;
		fastf_t dist[2];
		point_t pca;

		v = (struct vertex *)BU_PTBL_GET(&verts, i);

		if (v == eu->vu_p->v_p)
		    continue;

		if (v == eu->eumate_p->vu_p->v_p)
		    continue;

		code = bn_dist_pt3_lseg3(dist, pca, eu->vu_p->v_p->vg_p->coord,
					 eu->eumate_p->vu_p->v_p->vg_p->coord, v->vg_p->coord, &is->tol);

		if (code > 2)
		    continue;

		if (code == 1)
		    bu_log("nmg_isect_nearly_coplanar_faces: vertices should have been fused x%x and x%x\n", v, eu->vu_p->v_p);
		else if (code == 2)
		    bu_log("nmg_isect_nearly_coplanar_faces: vertices should have been fused x%x and x%x\n", v, eu->eumate_p->vu_p->v_p);
		else {
		    /* need to split EU at V */
		    (void)nmg_esplit(v, eu, 1);
		}
	    }
	}
    }

    bu_ptbl_free(&loops);
    bu_ptbl_free(&verts);

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	plane_t plane1, plane2;
	fastf_t dist;

	bu_log("After splitting loops into coplanar and non:\n");
	nmg_pr_fu_briefly(fu1, "");
	nmg_pr_fu_briefly(fu2, "");

	NMG_GET_FU_PLANE(plane1, fu1);
	NMG_GET_FU_PLANE(plane2, fu2);

	for (BU_LIST_FOR(lu, loopuse, &fu1->lu_hd)) {
	    int in=0, on=0, out=0;

	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		struct vertex_g *vg;

		vg = eu->vu_p->v_p->vg_p;

		dist = DIST_PT_PLANE(vg->coord, plane2);

		if (dist > is->tol.dist)
		    out++;
		else if (dist < (-is->tol.dist))
		    in++;
		else
		    on++;
	    }

	    if (in && out)
		bu_log("lu x%x is in and out of fu x%x\n", lu, fu2);
	    else if (in)
		bu_log("lu x%x is inside of fu x%x\n", lu, fu2);
	    else if (out)
		bu_log("lu x%x is outside of fu x%x\n", lu, fu2);
	    else if (on)
		bu_log("lu x%x is on of fu x%x\n", lu, fu2);
	    else
		bu_log("Can't figure lu x%x w.r.t fu x%x, on=%d, in=%d, out=%d\n",
		       lu, fu2, on, in, out);
	}

	for (BU_LIST_FOR(lu, loopuse, &fu2->lu_hd)) {
	    int in=0, on=0, out=0;

	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		struct vertex_g *vg;

		vg = eu->vu_p->v_p->vg_p;

		dist = DIST_PT_PLANE(vg->coord, plane1);

		if (dist > is->tol.dist)
		    out++;
		else if (dist < (-is->tol.dist))
		    in++;
		else
		    on++;
	    }

	    if (in && out)
		bu_log("lu x%x is in and out of fu x%x\n", lu, fu1);
	    else if (in)
		bu_log("lu x%x is inside of fu x%x\n", lu, fu1);
	    else if (out)
		bu_log("lu x%x is outside of fu x%x\n", lu, fu1);
	    else if (on)
		bu_log("lu x%x is on of fu x%x\n", lu, fu1);
	    else
		bu_log("Can't figure lu x%x w.r.t fu x%x, on=%d, in=%d, out=%d\n",
		       lu, fu1, on, in, out);
	}
    }

    /* now intersect only EU's that lie in the plane of the other faceuse */
    bu_ptbl_reset(&eu1_list);
    bu_ptbl_reset(&eu2_list);
    nmg_edgeuse_tabulate(&eu1_list, &fu1->l.magic);
    nmg_edgeuse_tabulate(&eu2_list, &fu2->l.magic);
    nmg_isect_coplanar_edges(is, &eu1_list, &eu2_list);

    if (mag1)
	bu_free((char *)mag1, "mag1");
    if (mag2)
	bu_free((char *)mag2, "mag2");

    bu_ptbl_free(is->l1);
    bu_ptbl_free(is->l2);

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	plane_t plane1, plane2;
	fastf_t dist;

	bu_log("After intersection nearly coplanar faces:\n");
	nmg_pr_fu_briefly(fu1, "");
	nmg_pr_fu_briefly(fu2, "");

	NMG_GET_FU_PLANE(plane1, fu1);
	NMG_GET_FU_PLANE(plane2, fu2);

	for (BU_LIST_FOR(lu, loopuse, &fu1->lu_hd)) {
	    int in=0, on=0, out=0;

	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		struct vertex_g *vg;

		vg = eu->vu_p->v_p->vg_p;

		dist = DIST_PT_PLANE(vg->coord, plane2);

		if (dist > is->tol.dist)
		    out++;
		else if (dist < (-is->tol.dist))
		    in++;
		else
		    on++;
	    }

	    if (in && out)
		bu_log("lu x%x is in and out of fu x%x\n", lu, fu2);
	    else if (in)
		bu_log("lu x%x is inside of fu x%x\n", lu, fu2);
	    else if (out)
		bu_log("lu x%x is outside of fu x%x\n", lu, fu2);
	    else if (on)
		bu_log("lu x%x is on of fu x%x\n", lu, fu2);
	    else
		bu_log("Can't figure lu x%x w.r.t fu x%x, on=%d, in=%d, out=%d\n",
		       lu, fu2, on, in, out);
	}

	for (BU_LIST_FOR(lu, loopuse, &fu2->lu_hd)) {
	    int in=0, on=0, out=0;

	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		struct vertex_g *vg;

		vg = eu->vu_p->v_p->vg_p;

		dist = DIST_PT_PLANE(vg->coord, plane1);

		if (dist > is->tol.dist)
		    out++;
		else if (dist < (-is->tol.dist))
		    in++;
		else
		    on++;
	    }

	    if (in && out)
		bu_log("lu x%x is in and out of fu x%x\n", lu, fu1);
	    else if (in)
		bu_log("lu x%x is inside of fu x%x\n", lu, fu1);
	    else if (out)
		bu_log("lu x%x is outside of fu x%x\n", lu, fu1);
	    else if (on)
		bu_log("lu x%x is on of fu x%x\n", lu, fu1);
	    else
		bu_log("Can't figure lu x%x w.r.t fu x%x, on=%d, in=%d, out=%d\n",
		       lu, fu1, on, in, out);
	}
    }

}
#endif


/** N M G _ F A C E S _ C A N _ B E _ I N T E R S E C T E D
 *
 * Check if two faceuses can be intersected normally, by looking at the line
 * of intersection and determining if the vertices from each face are all
 * above the other face on one side of the intersection line and below it
 * on the other side of the interection line.
 *
 * return:
 * 1 - faceuses meet criteria and can be intersected normally
 * 0 - must use nmg_isect_nearly_coplanar_faces
 */
int
nmg_faces_can_be_intersected(struct nmg_inter_struct *bs, const struct faceuse *fu1, const struct faceuse *fu2, const struct bn_tol *tol)
{
    plane_t pl1, pl2;
    point_t min_pt;
    struct face *f1, *f2;
    double dir_len_sq;
    double one_over_dir_len;
    plane_t tmp_pl;
    vect_t left;
    struct bu_ptbl verts;
    int on_line, above_left, below_left, on_left, above_right, below_right, on_right;
    int i;

    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACEUSE(fu2);
    BN_CK_TOL(tol);

    NMG_GET_FU_PLANE(pl1, fu1);
    NMG_GET_FU_PLANE(pl2, fu2);

    f1 = fu1->f_p;
    f2 = fu2->f_p;

    NMG_CK_FACE(f1);
    NMG_CK_FACE(f2);

    VMOVE(min_pt, f1->min_pt);
    VMIN(min_pt, f2->min_pt);

    VCROSS(bs->dir, pl1, pl2);
    dir_len_sq = MAGSQ(bs->dir);
    if (dir_len_sq <= SMALL_FASTF)
	return 0;

    one_over_dir_len = 1.0/sqrt(dir_len_sq);
    VSCALE(bs->dir, bs->dir, one_over_dir_len);
    VMOVE(tmp_pl, bs->dir);
    tmp_pl[W] = VDOT(tmp_pl, min_pt);

    if (bn_mkpoint_3planes(bs->pt, tmp_pl, pl1, pl2))
	return 0;

    VCROSS(left, pl1, bs->dir);

    /* check vertices from fu1 versus plane of fu2 */
    nmg_vertex_tabulate(&verts, &fu1->l.magic);
    on_line = 0;
    above_left = 0;
    below_left = 0;
    on_left = 0;
    above_right = 0;
    below_right = 0;
    on_right = 0;
    for (i=0; i<BU_PTBL_END(&verts); i++) {
	struct vertex *v;
	point_t pca;
	fastf_t dist;
	int code;
	vect_t to_v;

	v = (struct vertex *)BU_PTBL_GET(&verts, i);

	code = bn_dist_pt3_line3(&dist, pca, bs->pt, bs->dir, v->vg_p->coord, tol);

	if (code == 0 || code == 1) {
	    on_line++;
	    continue;
	}

	VSUB2(to_v, v->vg_p->coord, pca);
	dist = DIST_PT_PLANE(v->vg_p->coord, pl2);
	if (VDOT(to_v, left) > 0.0) {
	    /* left of intersection line */
	    if (dist > tol->dist)
		above_left++;
	    else if (dist < (-tol->dist))
		below_left++;
	    else
		on_left++;
	} else {
	    /* right of intersction line */
	    if (dist > tol->dist)
		above_right++;
	    else if (dist < (-tol->dist))
		below_right++;
	    else
		on_right++;
	}
    }
    bu_ptbl_free(&verts);

    if (above_left && below_left)
	return 0;
    if (on_left)
	return 0;
    if (above_right && below_right)
	return 0;
    if (on_right)
	return 0;

    /* check vertices from fu2 versus plane of fu1 */
    nmg_vertex_tabulate(&verts, &fu2->l.magic);
    on_line = 0;
    above_left = 0;
    below_left = 0;
    on_left = 0;
    above_right = 0;
    below_right = 0;
    on_right = 0;
    for (i=0; i<BU_PTBL_END(&verts); i++) {
	struct vertex *v;
	point_t pca;
	fastf_t dist;
	int code;
	vect_t to_v;

	v = (struct vertex *)BU_PTBL_GET(&verts, i);

	code = bn_dist_pt3_line3(&dist, pca, bs->pt, bs->dir, v->vg_p->coord, tol);

	if (code == 0 || code == 1) {
	    on_line++;
	    continue;
	}

	VSUB2(to_v, v->vg_p->coord, pca);
	dist = DIST_PT_PLANE(v->vg_p->coord, pl1);
	if (VDOT(to_v, left) > 0.0) {
	    /* left of intersection line */
	    if (dist > tol->dist)
		above_left++;
	    else if (dist < (-tol->dist))
		below_left++;
	    else
		on_left++;
	} else {
	    /* right of intersction line */
	    if (dist > tol->dist)
		above_right++;
	    else if (dist < (-tol->dist))
		below_right++;
	    else
		on_right++;
	}
    }
    bu_ptbl_free(&verts);

    if (above_left && below_left)
	return 0;
    if (on_left)
	return 0;
    if (above_right && below_right)
	return 0;
    if (on_right)
	return 0;

    return 1;
}


/**
 * N M G _ I S E C T _ T W O _ G E N E R I C _ F A C E S
 *
 * Intersect a pair of faces
 */
void
nmg_isect_two_generic_faces(struct faceuse *fu1, struct faceuse *fu2, const struct bn_tol *tol)
{
    struct nmg_inter_struct bs;
    plane_t pl1, pl2;
    struct face *f1, *f2;
    int status;

    /* sanity */
    memset(&bs, 0, sizeof(struct nmg_inter_struct));

    BN_CK_TOL(tol);
    bs.magic = NMG_INTER_STRUCT_MAGIC;
    bs.vert2d = (fastf_t *)NULL;
    bs.tol = *tol;		/* struct copy */

    NMG_CK_FACEUSE(fu1);
    f1 = fu1->f_p;
    NMG_CK_FACE(f1);
    NMG_CK_FACE_G_PLANE(f1->g.plane_p);

    NMG_CK_FACEUSE(fu2);
    f2 = fu2->f_p;
    NMG_CK_FACE(f2);
    NMG_CK_FACE_G_PLANE(f2->g.plane_p);

    NMG_GET_FU_PLANE(pl1, fu1);
    NMG_GET_FU_PLANE(pl2, fu2);

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("\nnmg_isect_two_generic_faces(fu1=x%x, fu2=x%x)\n", fu1, fu2);
	bu_log("Planes\t%gx + %gy + %gz = %g\n\t%gx + %gy + %gz = %g\n",
	       pl1[X], pl1[Y], pl1[Z], pl1[W],
	       pl2[X], pl2[Y], pl2[Z], pl2[W]);
	bu_log("Cosine of angle between planes = %g\n", VDOT(pl1, pl2));
	bu_log("fu1:\n");
	nmg_pr_fu_briefly(fu1, "\t");
	bu_log("fu2:\n");
	nmg_pr_fu_briefly(fu2, "\t");
	nmg_fu_touchingloops(fu1);
	nmg_fu_touchingloops(fu2);
    }

    status = 10;
    if (f1->g.plane_p == f2->g.plane_p) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("co-planar faces (shared fg)\n");
	}
	status = (-1);
    }

    if (!V3RPP_OVERLAP_TOL(f2->min_pt, f2->max_pt, 
			   f1->min_pt, f1->max_pt, &bs.tol)) {
	return;
    }

    /*
     * The extents of face1 overlap the extents of face2.
     * Construct a ray which contains the line of intersection.
     * There are two choices for direction, and an infinite number
     * of candidate points.
     *
     * The correct choice of this ray is very important, so that:
     * 1) All intersections are at positive distances on the ray,
     * 2) dir cross N will point "left".
     *
     * These two conditions can be satisfied by intersecting the
     * line with the face's bounding RPP.  This will give two
     * points A and B, where A is closer to the min point of the RPP
     * and B is closer to the max point of the RPP.
     * Let bs.pt be A, and let bs.dir point from A towards B.
     * This choice will satisfy both constraints, above.
     *
     * NOTE:  These conditions must be enforced in the 2D code, also.
     */
    if (status == 10) {
	status = nmg_isect_2faceuse(bs.pt, bs.dir, fu1, fu2, tol);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    bu_log("\tnmg_isect_two_generic_faces: intersect ray start (%f, %f, %f)\n\t\tin direction (%f, %f, %f)\n",
		   bs.pt[X], bs.pt[Y], bs.pt[Z], bs.dir[X], bs.dir[Y], bs.dir[Z]);
	}
    }

    switch (status) {
	case 0:
	    bs.coplanar = 0;
	    nmg_isect_two_face3p(&bs, fu1, fu2);
	    break;
	case -1:
	    /* co-planar faces */
	    bs.coplanar = 1;
	    nmg_isect_two_face2p_jra(&bs, fu1, fu2);
	    break;
	case -2:
	    /* no intersection, faceuse are parallel but not coplanar */
	    return;
	case -3:
	    bu_bomb("nmg_isect_two_generic_faces(): faceuse should have intersection but could not find it\n");
	default:
	    /* internal error */
	    bu_bomb("nmg_isect_two_generic_faces(): invalid return code from function nmg_isect_2faceuse\n");
    }

    nmg_isect2d_cleanup(&bs);

    /* Eliminate stray vertices that were added along edges in this step */
    (void)nmg_unbreak_region_edges(&fu1->l.magic);
    (void)nmg_unbreak_region_edges(&fu2->l.magic);

    if (fu1 && rt_g.NMG_debug & (DEBUG_POLYSECT|DEBUG_FCUT|DEBUG_MESH)
	&& rt_g.NMG_debug & DEBUG_PLOTEM) {
	static int nshell = 1;
	char name[32];
	FILE *fp;

	/* Both at once */
	nmg_pl_2fu("Iface%d.pl", fu2, fu1, 0);

	/* Each in its own file */
	nmg_face_plot(fu1);
	nmg_face_plot(fu2);

	sprintf(name, "shellA%d.pl", nshell);
	if ((fp = fopen(name, "wb")) != NULL) {
	    bu_log("overlay %s\n", name);
	    nmg_pl_s(fp, fu1->s_p);
	    fclose(fp);
	}

	sprintf(name, "shellB%d.pl", nshell++);
	if ((fp = fopen(name, "wb")) != NULL) {
	    bu_log("overlay %s\n", name);
	    nmg_pl_s(fp, fu2->s_p);
	    fclose(fp);
	}
    }

    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_region_v_unique(fu1->s_p->r_p, &bs.tol);
	nmg_region_v_unique(fu2->s_p->r_p, &bs.tol);
	nmg_fu_touchingloops(fu1);
	nmg_fu_touchingloops(fu2);
	nmg_ck_face_worthless_edges(fu1);
	nmg_ck_face_worthless_edges(fu2);
    }
}


/**
 * N M G _ I S E C T _ E D G E 3 P _ E D G E 3 P
 *
 * Intersect one edge with another.  At least one is a wire edge;
 * thus there is no face context or intersection line.
 * If the edges are non-colinear, there will be at most one point of isect.
 * If the edges are colinear, there may be two.
 *
 * Called from nmg_isect_edge3p_shell()
 */
static void
nmg_isect_edge3p_edge3p(struct nmg_inter_struct *is, struct edgeuse *eu1, struct edgeuse *eu2)
{
    struct vertexuse *vu1a;
    struct vertexuse *vu1b;
    struct vertexuse *vu2a;
    struct vertexuse *vu2b;
    vect_t eu1_dir;
    vect_t eu2_dir;
    fastf_t dist[2];
    int status;
    struct vertex *new_v;
    point_t hit_pt;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_EDGEUSE(eu1);
    NMG_CK_EDGEUSE(eu2);

    vu1a = eu1->vu_p;
    vu1b = BU_LIST_PNEXT_CIRC(edgeuse, eu1)->vu_p;
    vu2a = eu2->vu_p;
    vu2b = BU_LIST_PNEXT_CIRC(edgeuse, eu2)->vu_p;
    NMG_CK_VERTEXUSE(vu1a);
    NMG_CK_VERTEXUSE(vu1b);
    NMG_CK_VERTEXUSE(vu2a);
    NMG_CK_VERTEXUSE(vu2b);

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_edge3p_edge3p(eu1=x%x, eu2=x%x)\n\tvu1a=%x vu1b=%x, vu2a=%x vu2b=%x\n\tv1a=%x v1b=%x,   v2a=%x v2b=%x\n",
	       eu1, eu2,
	       vu1a, vu1b, vu2a, vu2b,
	       vu1a->v_p, vu1b->v_p, vu2a->v_p, vu2b->v_p);

    /*
     * Topology check.
     * If both endpoints of both edges match, this is a trivial accept.
     */
    if ((vu1a->v_p == vu2a->v_p && vu1b->v_p == vu2b->v_p) ||
	(vu1a->v_p == vu2b->v_p && vu1b->v_p == vu2a->v_p)) {
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    bu_log("nmg_isect_edge3p_edge3p: shared edge topology, both ends\n");
	if (eu1->e_p != eu2->e_p)
	    nmg_radial_join_eu(eu1, eu2, &is->tol);
	return;
    }
    VSUB2(eu1_dir, vu1b->v_p->vg_p->coord, vu1a->v_p->vg_p->coord);
    VSUB2(eu2_dir, vu2b->v_p->vg_p->coord, vu2a->v_p->vg_p->coord);

    dist[0] = dist[1] = 0.0;	/* for clean prints, below */

    status = bn_isect_lseg3_lseg3(dist,
				  vu1a->v_p->vg_p->coord, eu1_dir,
				  vu2a->v_p->vg_p->coord, eu2_dir, &is->tol);

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("\trt_isect_line3_lseg3()=%d, dist: %g, %g\n",
	       status, dist[0], dist[1]);
    }

    if (status < 0) {
	/* missed */
	return;
    }

    if (status == 0) {
	/* lines are colinear */
	bu_log("nmg_isect_edge3p_edge3p() colinear case.  Untested waters.\n");
	/* Initialize 2D vertex cache with EDGE info. */
	nmg_isect2d_prep(is, &eu1->l.magic);
	/* 3rd arg has to be a faceuse.  Tried to send it eu1->e_p */
	/* XXX This will bu_bomb() when faceuse is checked. */
	(void)nmg_isect_2colinear_edge2p(eu1, eu2, (struct faceuse *)NULL, is, (struct bu_ptbl *)0, (struct bu_ptbl *)0);
	return;
    }

    /* XXX There is an intersection point.  This could just be
     * reformulated as a 2D problem here, and passed off to
     * the 2D routine that this was based on.
     */

    /* dist[0] is distance along eu1 */
    if (ZERO(dist[0])) {
	/* Hit is at vu1a */
	if (ZERO(dist[1])) {
	    /* Hit is at vu2a */
	    nmg_jv(vu1a->v_p, vu2a->v_p);
	    return;
	} else if (ZERO(dist[1] - 1.0)) {
	    /* Hit is at vu2b */
	    nmg_jv(vu1a->v_p, vu2b->v_p);
	    return;
	}
	/* Break eu2 on vu1a */
	nmg_ebreaker(vu1a->v_p, eu2, &is->tol);
	return;
    } else if (ZERO(dist[0] - 1.0)) {
	/* Hit is at vu1b */
	if (ZERO(dist[1])) {
	    /* Hit is at vu2a */
	    nmg_jv(vu1b->v_p, vu2a->v_p);
	    return;
	} else if (ZERO(dist[1] - 1.0)) {
	    /* Hit is at vu2b */
	    nmg_jv(vu1b->v_p, vu2b->v_p);
	    return;
	}
	/* Break eu2 on vu1b */
	nmg_ebreaker(vu1b->v_p, eu2, &is->tol);
	return;
    } else {
	/* Hit on eu1 is between vu1a and vu1b */
	if (dist[1] < 0 || dist[1] > 1) return;	/* Don't bother breaking eu1, it doesn't touch eu2. */

	if (ZERO(dist[1])) {
	    /* Hit is at vu2a */
	    nmg_ebreaker(vu2a->v_p, eu1, &is->tol);
	    return;
	} else if (ZERO(dist[1] - 1.0)) {
	    /* Hit is at vu2b */
	    nmg_ebreaker(vu2b->v_p, eu1, &is->tol);
	    return;
	}
	/* Hit is amidships on both eu1 and eu2. */
	new_v = nmg_e2break(eu1, eu2);

	VJOIN1(hit_pt, vu2a->v_p->vg_p->coord, dist[1], eu2_dir);
	nmg_vertex_gv(new_v, hit_pt);
    }
}


/**
 * N M G _ I S E C T _ V E R T E X 3 _ E D G E 3 P
 *
 * Intersect a lone vertex from s1 with a single edge from s2.
 */
static void
nmg_isect_vertex3_edge3p(struct nmg_inter_struct *is, struct vertexuse *vu1, struct edgeuse *eu2)
{
    fastf_t dist;
    int code;
    struct vertexuse *vu2 = (struct vertexuse *)NULL;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_VERTEXUSE(vu1);
    NMG_CK_EDGEUSE(eu2);

    code = bn_isect_pt_lseg(&dist, eu2->vu_p->v_p->vg_p->coord,
			    eu2->vu_p->v_p->vg_p->coord,
			    vu1->v_p->vg_p->coord, &is->tol);

    if (code < 0) return;		/* Not on line */
    switch (code) {
	case 1:
	    /* Hit is at A */
	    vu2 = eu2->vu_p;
	    break;
	case 2:
	    /* Hit is at B */
	    vu2 = BU_LIST_NEXT(edgeuse, &eu2->l)->vu_p;
	    break;
	case 3:
	    /* Hit is in the span AB somewhere, break edge */
	    vu2 = nmg_ebreaker(vu1->v_p, eu2, &is->tol)->vu_p;
	    break;
	default:
	    bu_bomb("nmg_isect_vertex3_edge3p()\n");
    }
    /* Make sure verts are shared at hit point. They _should_ already be. */
    nmg_jv(vu1->v_p, vu2->v_p);
    (void)bu_ptbl_ins_unique(is->l1, (long *)&vu1->l.magic);
    (void)bu_ptbl_ins_unique(is->l2, (long *)&vu2->l.magic);
}


/**
 * N M G _ I S E C T _ E D G E 3 P _ S H E L L
 *
 * Intersect one edge with all of another shell.
 * There is no face context for this edge, because
 *
 * At present, this routine is used for only one purpose:
 * 1) Handling wire edge -vs- shell intersection
 *
 * The edge will be fully intersected with the shell, potentially
 * getting trimmed down in the process as crossings of s2 are found.
 * The caller is responsible for re-calling with the extra edgeuses.
 *
 * If both vertices of eu1 are on s2 (the other shell), and
 * there is no edge in s2 between them, we need to determine
 * whether this is an interior or exterior edge, and
 * perhaps add a loop into s2 connecting those two verts.
 *
 * We can't use the face cutter, because s2 has no
 * appropriate face containing this edge.
 *
 * If this edge is split, we have to
 * trust nmg_ebreak() to insert new eu's ahead in the eu list,
 * so caller will see them.
 *
 * Lots of junk will be put on the vert_list's in 'is';  the caller
 * should just free the lists without using them.
 *
 * Called by nmg_crackshells().
 */
static void
nmg_isect_edge3p_shell(struct nmg_inter_struct *is, struct edgeuse *eu1, struct shell *s2)
{
    struct faceuse *fu2;
    struct loopuse *lu2;
    struct edgeuse *eu2;
    struct vertexuse *vu2;
    point_t midpt;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_EDGEUSE(eu1);
    NMG_CK_SHELL(s2);

    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_edge3p_shell(, eu1=x%x, s2=x%x) START\n",
	       eu1, s2);
    }

    eu2 = nmg_find_matching_eu_in_s(eu1, s2);
    if (eu2) {
	/* XXX Is the fact that s2 has a corresponding edge good enough? */
	nmg_radial_join_eu(eu1, eu2, &is->tol);
	return;
    }

    /* Note the ray that contains this edge.  For debug in nmg_isect_wireedge3p_face3p() */
    VMOVE(is->pt, eu1->vu_p->v_p->vg_p->coord);
    VSUB2(is->dir, eu1->eumate_p->vu_p->v_p->vg_p->coord, is->pt);
    VUNITIZE(is->dir);

    /* Check eu1 of s1 against all faces in s2 */
    for (BU_LIST_FOR(fu2, faceuse, &s2->fu_hd)) {
	NMG_CK_FACEUSE(fu2);
	if (fu2->orientation != OT_SAME) continue;
	is->fu2 = fu2;

	/* We aren't interested in the vert_list's, ignore return */
	(void)nmg_isect_wireedge3p_face3p(is, eu1, fu2);
    }

    /* Check eu1 of s1 against all wire loops in s2 */
    is->fu2 = (struct faceuse *)NULL;
    for (BU_LIST_FOR(lu2, loopuse, &s2->lu_hd)) {
	NMG_CK_LOOPUSE(lu2);
	/* Really, it's just a bunch of wire edges, in a loop. */
	if (BU_LIST_FIRST_MAGIC(&lu2->down_hd) == NMG_VERTEXUSE_MAGIC) {
	    /* XXX Can there be lone-vertex wire loops here? */
	    vu2 = BU_LIST_FIRST(vertexuse, &lu2->down_hd);
	    NMG_CK_VERTEXUSE(vu2);
	    nmg_isect_vertex3_edge3p(is, vu2, eu1);
	    continue;
	}
	for (BU_LIST_FOR(eu2, edgeuse, &lu2->down_hd)) {
	    NMG_CK_EDGEUSE(eu2);
	    nmg_isect_edge3p_edge3p(is, eu1, eu2);
	}
    }

    /* Check eu1 of s1 against all wire edges in s2 */
    for (BU_LIST_FOR(eu2, edgeuse, &s2->eu_hd)) {
	NMG_CK_EDGEUSE(eu2);
	nmg_isect_edge3p_edge3p(is, eu1, eu2);
    }

    /* Check eu1 of s1 against vert of s2 */
    if (s2->vu_p) {
	nmg_isect_vertex3_edge3p(is, s2->vu_p, eu1);
    }

    /*
     * The edge has been fully intersected with the other shell.
     * It may have been trimmed in the process;  the caller is
     * responsible for re-calling us with the extra edgeuses.
     * If both vertices of eu1 are on s2 (the other shell), and
     * there is no edge in s2 between them, we need to determine
     * whether this is an interior or exterior edge, and
     * perhaps add a loop into s2 connecting those two verts.
     */
    eu2 = nmg_find_matching_eu_in_s(eu1, s2);
    if (eu2) {
	/* We can't fuse wire edges */
	goto out;
    }
    /* Can't use the face cutter, because s2 has no associated face!
     * Call the geometric classifier on the midpoint.
     * If it's INSIDE or ON the other shell, add a wire loop
     * that connects the two vertices.
     */
    VADD2SCALE(midpt, eu1->vu_p->v_p->vg_p->coord,
	       eu1->eumate_p->vu_p->v_p->vg_p->coord,  0.5);
    if (nmg_class_pt_s(midpt, s2, 0, &is->tol) == NMG_CLASS_AoutB)
	goto out;		/* Nothing more to do */

    /* Add a wire loop in s2 connecting the two vertices */
    lu2 = nmg_mlv(&s2->l.magic, eu1->vu_p->v_p, OT_UNSPEC);
    NMG_CK_LOOPUSE(lu2);
    {
	struct edgeuse *neu1, *neu2;

	neu1 = nmg_meonvu(BU_LIST_FIRST(vertexuse, &lu2->down_hd));
	neu2 = nmg_eusplit(eu1->eumate_p->vu_p->v_p, neu1, 0);
	NMG_CK_EDGEUSE(eu1);
	/* Attach both new edges in s2 to original edge in s1 */
	nmg_use_edge_g(neu1, eu1->g.magic_p);
	nmg_use_edge_g(neu2, eu1->g.magic_p);
	nmg_radial_join_eu(eu1, neu2, &is->tol);
	nmg_radial_join_eu(eu1, neu1, &is->tol);
    }
    nmg_loop_g(lu2->l_p, &is->tol);
    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_edge3p_shell(, eu1=x%x, s2=x%x) Added wire lu=x%x\n",
	       eu1, s2, lu2);
    }

out:
    if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	bu_log("nmg_isect_edge3p_shell(, eu1=x%x, s2=x%x) END\n",
	       eu1, s2);
    }
    return;
}


/**
 * N M G _ C R A C K S H E L L S
 *
 * Split the components of two shells wherever they may intersect,
 * in preparation for performing boolean operations on the shells.
 */
void
nmg_crackshells(struct shell *s1, struct shell *s2, const struct bn_tol *tol)
{
    struct bu_ptbl vert_list1, vert_list2;
    struct nmg_inter_struct is;
    struct shell_a *sa1, *sa2;
    struct face *f1, *f2;
    struct faceuse *fu1, *fu2;
    struct loopuse *lu1;
    struct loopuse *lu2;
    struct edgeuse *eu1;
    struct edgeuse *eu2;
    char *flags1, *flags2;
    long flag_len1, flag_len2;
    point_t isect_min_pt, isect_max_pt;

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_crackshells(s1=x%x, s2=x%x)\n", s1, s2);

    BN_CK_TOL(tol);
    NMG_CK_SHELL(s1);
    sa1 = s1->sa_p;
    NMG_CK_SHELL_A(sa1);

    NMG_CK_SHELL(s2);
    sa2 = s2->sa_p;
    NMG_CK_SHELL_A(sa2);

    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_ck_vs_in_region(s1->r_p, tol);
	nmg_ck_vs_in_region(s2->r_p, tol);
    }

    /* test if shell s1 and s2 overlap */
    if (!V3RPP_OVERLAP_TOL(sa1->min_pt, sa1->max_pt,
			    sa2->min_pt, sa2->max_pt, tol)) {
	return;
    }

    /* create a new bounding box which is the intersection
     * of the shell s1 and s2 bounding boxes
     */
    VMOVE(isect_min_pt, sa1->min_pt);
    VMAX(isect_min_pt, sa2->min_pt);
    VMOVE(isect_max_pt, sa1->max_pt);
    VMIN(isect_max_pt, sa2->max_pt);

    /* All the non-face/face isect subroutines need are tol, l1, and l2 */
    is.magic = NMG_INTER_STRUCT_MAGIC;
    is.vert2d = (fastf_t *)NULL;
    is.tol = *tol;		/* struct copy */
    is.l1 = &vert_list1;
    is.l2 = &vert_list2;
    is.s1 = s1;
    is.s2 = s2;
    is.fu1 = (struct faceuse *)NULL;
    is.fu2 = (struct faceuse *)NULL;
    (void)bu_ptbl_init(&vert_list1, 64, "&vert_list1");
    (void)bu_ptbl_init(&vert_list2, 64, "&vert_list2");

    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_vshell(&s1->r_p->s_hd, s1->r_p);
	nmg_vshell(&s2->r_p->s_hd, s2->r_p);
    }

    flag_len1 = s1->r_p->m_p->maxindex * 10;
    flags1 = (char *)bu_calloc(flag_len1, sizeof(char),
			      "nmg_crackshells flags1[]");
    flag_len2 = s2->r_p->m_p->maxindex * 10;
    flags2 = (char *)bu_calloc(flag_len2, sizeof(char),
			      "nmg_crackshells flags2[]");

    /*
     * Check each of the faces in shell 1 to see
     * if they overlap the extent of shell 2
     */
    for (BU_LIST_FOR(fu1, faceuse, &s1->fu_hd)) {
	if (s1->r_p->m_p->maxindex >= flag_len1) bu_bomb("nmg_crackshells() flag_len1 overrun\n");
	NMG_CK_FACEUSE(fu1);
	f1 = fu1->f_p;
	NMG_CK_FACE(f1);

	if (fu1->orientation != OT_SAME) continue;
	if (NMG_INDEX_IS_SET(flags1, f1)) continue;
	NMG_CK_FACE_G_PLANE(f1->g.plane_p);

        /* test if the face f1 bounding box and isect bounding box 
         * are disjoint
         */
        if (V3RPP_DISJOINT_TOL(f1->min_pt, f1->max_pt, isect_min_pt, isect_max_pt, tol)) {
	    NMG_INDEX_SET(flags1, f1);
	    continue;
        }

	is.fu1 = fu1;

	/*
	 * Now, check the face f1 from shell 1
	 * against each of the faces of shell 2
	 */
	for (BU_LIST_FOR(fu2, faceuse, &s2->fu_hd)) {
	    if (s2->r_p->m_p->maxindex >= flag_len2) bu_bomb("nmg_crackshells() flag_len2 overrun\n");
	    NMG_CK_FACEUSE(fu2);
	    NMG_CK_FACE(fu2->f_p);

	    f2 = fu2->f_p;
	    NMG_CK_FACE(f2);

	    if (fu2->orientation != OT_SAME) continue;
	    if (NMG_INDEX_IS_SET(flags2, f2)) continue;

            /* test if the face f1 bounding box and isect bounding box 
             * are disjoint
             */
            if (V3RPP_DISJOINT_TOL(f2->min_pt, f2->max_pt, isect_min_pt, isect_max_pt, tol)) {
	        NMG_INDEX_SET(flags2, f2);
	        continue;
            }
	    nmg_isect_two_generic_faces(fu1, fu2, tol);
	}

	/*
	 * Because the rest of the shell elements are wires,
	 * there is no need to invoke the face cutter;
	 * calculating the intersection points (vertices)
	 * is sufficient.
	 * XXX Is this true?  What about a wire edge cutting
	 * XXX clean across fu1?  fu1 ought to be cut!
	 *
	 * If coplanar, need to cut face.
	 * If non-coplanar, can only hit at one point.
	 */
	is.fu2 = (struct faceuse *)NULL;

	/* Check f1 from s1 against wire loops of s2 */
	for (BU_LIST_FOR(lu2, loopuse, &s2->lu_hd)) {
	    NMG_CK_LOOPUSE(lu2);
	    /* Not interested in vert_list here */
	    (void)nmg_isect_wireloop3p_face3p(&is, lu2, fu1);
	}

	/* Check f1 from s1 against wire edges of s2 */
	for (BU_LIST_FOR(eu2, edgeuse, &s2->eu_hd)) {
	    NMG_CK_EDGEUSE(eu2);

	    nmg_isect_wireedge3p_face3p(&is, eu2, fu1);
	}

	/* Check f1 from s1 against lone vert of s2 */
	if (s2->vu_p) {
	    nmg_isect_3vertex_3face(&is, s2->vu_p, fu1);
	}

	NMG_INDEX_SET(flags1, f1);

	if (rt_g.NMG_debug & DEBUG_VERIFY) {
	    nmg_vshell(&s1->r_p->s_hd, s1->r_p);
	    nmg_vshell(&s2->r_p->s_hd, s2->r_p);
	}
    }

    /* Check each wire loop of shell 1 against non-faces of shell 2. */
    is.fu1 = (struct faceuse *)NULL;
    is.fu2 = (struct faceuse *)NULL;
    for (BU_LIST_FOR(lu1, loopuse, &s1->lu_hd)) {
	NMG_CK_LOOPUSE(lu1);
	/* XXX Can there be lone-vertex loops here? (yes, need an intersector) */
	if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;
	/* Really, it's just a bunch of wire edges, in a loop. */
	for (BU_LIST_FOR(eu1, edgeuse, &lu1->down_hd)) {
	    NMG_CK_EDGEUSE(eu1);
	    /* Check eu1 against all of shell 2 */
	    nmg_isect_edge3p_shell(&is, eu1, s2);
	}
    }

    /* Check each wire edge of shell 1 against all of shell 2. */
    for (BU_LIST_FOR(eu1, edgeuse, &s1->eu_hd)) {
	NMG_CK_EDGEUSE(eu1);
	nmg_isect_edge3p_shell(&is, eu1, s2);
    }

    /* Check each lone vert of s1 against shell 2 */
    if (s1->vu_p) {
	/* Check vert of s1 against all faceuses in s2 */
	for (BU_LIST_FOR(fu2, faceuse, &s2->fu_hd)) {
	    NMG_CK_FACEUSE(fu2);
	    if (fu2->orientation != OT_SAME) continue;
	    nmg_isect_3vertex_3face(&is, s1->vu_p, fu2);
	}
	/* Check vert of s1 against all wire loops of s2 */
	for (BU_LIST_FOR(lu2, loopuse, &s2->lu_hd)) {
	    NMG_CK_LOOPUSE(lu2);
	    /* Really, it's just a bunch of wire edges, in a loop. */
	    /* XXX Can there be lone-vertex loops here? */
	    for (BU_LIST_FOR(eu2, edgeuse, &lu2->down_hd)) {
		NMG_CK_EDGEUSE(eu2);
		nmg_isect_vertex3_edge3p(&is, s1->vu_p, eu2);
	    }
	}
	/* Check vert of s1 against all wire edges of s2 */
	for (BU_LIST_FOR(eu2, edgeuse, &s2->eu_hd)) {
	    NMG_CK_EDGEUSE(eu2);
	    nmg_isect_vertex3_edge3p(&is, s1->vu_p, eu2);
	}

	/* Check vert of s1 against vert of s2 */
	/* Unnecessary: already done by vertex fuser */
    }
    if (s1->r_p->m_p->maxindex >= flag_len1) bu_bomb("nmg_crackshells() flag_len1 overrun by end\n");

    /* Release storage from bogus isect line */
    (void)bu_ptbl_free(&vert_list1);
    (void)bu_ptbl_free(&vert_list2);

    bu_free((char *)flags1, "nmg_crackshells flags1[]");
    bu_free((char *)flags2, "nmg_crackshells flags2[]");

    /* Eliminate stray vertices that were added along edges in this step */
    (void)nmg_unbreak_region_edges(&s1->l.magic);
    (void)nmg_unbreak_region_edges(&s2->l.magic);

    nmg_isect2d_cleanup(&is);

    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_vshell(&s1->r_p->s_hd, s1->r_p);
	nmg_vshell(&s2->r_p->s_hd, s2->r_p);
	nmg_ck_vs_in_region(s1->r_p, tol);
	nmg_ck_vs_in_region(s2->r_p, tol);
    }
}


/**
 * N M G _ F U _ T O U C H I N G L O O P S
 */
int
nmg_fu_touchingloops(const struct faceuse *fu)
{
    const struct loopuse *lu;
    const struct vertexuse *vu;

    NMG_CK_FACEUSE(fu);
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	vu = nmg_loop_touches_self(lu);
	if (vu) {
	    NMG_CK_VERTEXUSE(vu);

	    /* Right now, this routine is used for debugging ONLY,
	     * so if this condition exists, blather.
	     * However, note that this condition happens a lot
	     * for valid reasons, too.
	     */
	    if (rt_g.NMG_debug & DEBUG_MANIF) {
		bu_log("nmg_fu_touchingloops(lu=x%x, vu=x%x, v=x%x)\n", lu, vu, vu->v_p);
		nmg_pr_lu_briefly(lu, 0);
	    }

	    return 1;
	}
    }
    return 0;
}


/**
 * B N _ I S E C T _ 2 F A C E U S E
 *@brief
 * Given two faceuse, find the line of intersection between them, if
 * one exists.  The line of intersection is returned in parametric
 * line (point & direction vector) form.
 *
 * @return 0	OK, line of intersection stored in `pt' and `dir'.
 * @return -1	FAIL, faceuse are coplanar
 * @return -2	FAIL, faceuse are parallel but not coplanar
 * @return -3	FAIL, unable to find line of intersection
 *
 * @param[out]	pt	Starting point of line of intersection
 * @param[out]	dir	Direction vector of line of intersection (unit length)
 * @param[in]	fu1	faceuse 1
 * @param[in]	fu2	faceuse 2
 * @param[in]	tol	tolerance values
 */
int
nmg_isect_2faceuse(point_t pt,
		   vect_t dir,
		   struct faceuse *fu1,
		   struct faceuse *fu2,
		   const struct bn_tol *tol)
{
    vect_t abs_dir;
    plane_t pl;
    point_t rpp_min;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct face *f1, *f2;
    plane_t f1_pl, f2_pl;
    int parallel = 0;
    int coplanar = 0;
    int cnt = 0;
    fastf_t avg_dist = 0.0;
    fastf_t tot_dist = 0.0;
    fastf_t dist = 0.0;

    VSETALL(pt, 0.0);  /* sanity */
    VSETALL(dir, 0.0); /* sanity */

    if (fu1->orientation != OT_SAME && fu1->orientation != OT_OPPOSITE) {
        bu_bomb("nmg_isect_2faceuse(): invalid fu1 orientation\n");
    }
    if (fu2->orientation != OT_SAME && fu2->orientation != OT_OPPOSITE) {
        bu_bomb("nmg_isect_2faceuse(): invalid fu2 orientation\n");
    }

    NMG_CK_FACEUSE(fu1);
    f1 = fu1->f_p;
    NMG_CK_FACE(f1);
    NMG_CK_FACE_G_PLANE(f1->g.plane_p);

    NMG_CK_FACEUSE(fu2);
    f2 = fu2->f_p;
    NMG_CK_FACE(f2);
    NMG_CK_FACE_G_PLANE(f2->g.plane_p);

    if (f1->g.plane_p == f2->g.plane_p) {
	/* intersection is not possible */
	return -1; /* FAIL, faceuse are coplanar */
    }

    /* need to use this macro to retrieve the planes
     * since this macro takes into account the flip
     * normal flag
     */
    NMG_GET_FU_PLANE(f1_pl, fu1);
    NMG_GET_FU_PLANE(f2_pl, fu2);

    /* test for parallel using distance from the plane of
     * the other faceuse
     */

    /* test fu1 against f2 */
    parallel = 1;
    cnt = 0;
    avg_dist = 0.0;
    tot_dist = 0.0;
    dist = 0.0;
    for (BU_LIST_FOR(lu, loopuse, &fu1->lu_hd)) {

	if (parallel == 0) {
	    break;
	}

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
	    continue;
	}
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    NMG_CK_EDGEUSE(eu);
	    cnt++;
	    dist = DIST_PT_PLANE(eu->vu_p->v_p->vg_p->coord, f2_pl);
	    tot_dist += dist;
	    /* the current distance is included in the average
             * that the current distance is compared against
             */
	    avg_dist = tot_dist / cnt;
	    if (NEAR_ZERO(dist - avg_dist, tol->dist)) {
	    } else {
		/* not parallel */
		parallel = 0;
		break;
	    }
	}
    }

    /* test fu2 against f1 */
    if (parallel == 1) {
	cnt = 0;
	avg_dist = 0.0;
	tot_dist = 0.0;
	dist = 0.0;
	for (BU_LIST_FOR(lu, loopuse, &fu2->lu_hd)) {
	    if (parallel == 0) {
		break;
	    }
	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
		continue;
	    }
	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		NMG_CK_EDGEUSE(eu);
		cnt++;
		dist = DIST_PT_PLANE(eu->vu_p->v_p->vg_p->coord, f1_pl);
		tot_dist += dist;
	        /* the current distance is included in the average
                 * that the current distance is compared against
                 */
		avg_dist = tot_dist / cnt;
		if (!NEAR_ZERO(dist - avg_dist, tol->dist)) {
		    /* not parallel */
		    parallel = 0;
		    break;
		}
	    }
	}
    }

    coplanar = 0;
    if (!nmg_ck_fu_verts(fu2, f1, tol) && !nmg_ck_fu_verts(fu1, f2, tol) &&
	NEAR_ZERO(fabs(f1_pl[W] - f2_pl[W]), tol->dist)) {
	/* true when fu1 and fu2 are coplanar, i.e. all vertices 
	 * of faceuse (fu1) are within distance tolarance of 
	 * face (f2) and vice-versa. 
	 */
	coplanar = 1;
    } 
 
    if (coplanar && !parallel) {
	bu_bomb("nmg_isect_2faceuse(): logic error, coplanar but not parallel\n");
    }

    if (!coplanar && parallel) {
	/* intersection is not possible */
	return -2; /* FAIL, faceuse are parallel and distinct */
    }

    if (coplanar) {
	/* intersection is not possible */
	return -1; /* FAIL, planes are identical (co-planar) */
    }

    /* at this point it should be possible to find an intersection */

    /* loopuse have their bounding boxes padded, we need to
     * have a more precise minimum point.
     */
    VSETALL(rpp_min, MAX_FASTF);

    for (BU_LIST_FOR(lu, loopuse, &fu1->lu_hd)) {
        NMG_CK_LOOPUSE(lu);
        if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
            continue;
        }
        for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
            NMG_CK_EDGEUSE(eu);
            VMIN(rpp_min, eu->vu_p->v_p->vg_p->coord);
        }
    }
    for (BU_LIST_FOR(lu, loopuse, &fu2->lu_hd)) {
        NMG_CK_LOOPUSE(lu);
        if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
            continue;
        }
        for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
            NMG_CK_EDGEUSE(eu);
            VMIN(rpp_min, eu->vu_p->v_p->vg_p->coord);
        }
    }

    /* Direction vector for ray is perpendicular to both plane
     * normals.
     */
    VCROSS(dir, f1_pl, f2_pl);

    /* Select an axis-aligned plane which has its normal pointing
     * along the same axis as the largest magnitude component of the
     * direction vector.  If the largest magnitude component is
     * negative, reverse the direction vector, so that model is "in
     * front" of start point.
     */
    abs_dir[X] = fabs(dir[X]);
    abs_dir[Y] = fabs(dir[Y]);
    abs_dir[Z] = fabs(dir[Z]);

    if (ZERO(abs_dir[X])) {
        abs_dir[X] = 0.0;
    }
    if (ZERO(abs_dir[Y])) {
        abs_dir[Y] = 0.0;
    }
    if (ZERO(abs_dir[Z])) {
        abs_dir[Z] = 0.0;
    }

    if (abs_dir[X] >= abs_dir[Y]) {
	if (abs_dir[X] >= abs_dir[Z]) {
	    VSET(pl, 1, 0, 0);	/* X */
	    pl[W] = rpp_min[X];
	    if (dir[X] < -SMALL_FASTF) {
		VREVERSE(dir, dir);
	    }
	} else {
	    VSET(pl, 0, 0, 1);	/* Z */
	    pl[W] = rpp_min[Z];
	    if (dir[Z] < -SMALL_FASTF) {
		VREVERSE(dir, dir);
	    }
	}
    } else {
	if (abs_dir[Y] >= abs_dir[Z]) {
	    VSET(pl, 0, 1, 0);	/* Y */
	    pl[W] = rpp_min[Y];
	    if (dir[Y] < -SMALL_FASTF) {
		VREVERSE(dir, dir);
	    }
	} else {
	    VSET(pl, 0, 0, 1);	/* Z */
	    pl[W] = rpp_min[Z];
	    if (dir[Z] < -SMALL_FASTF) {
		VREVERSE(dir, dir);
	    }
	}
    }

    /* Intersection of the 3 planes defines ray start point */
    if (bn_mkpoint_3planes(pt, pl, f1_pl, f2_pl) < 0) {
	return -3;	/* FAIL -- no intersection */
    }

    return 0;		/* OK */
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

