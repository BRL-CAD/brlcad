/*                       N M G _ T R I . C
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
/** @file primitives/nmg/nmg_tri.c
 *
 * Triangulate the faces of a polygonal NMG.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "bu/parallel.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "plot3.h"


/* macros for comparing 2D points in scanline order */
/* XXX maybe should use near zero tolerance instead */
#define TOL_2D 1.0e-10
#define P_GT_V(_p, _v) \
    (((_p)->coord[Y] - (_v)->coord[Y]) > TOL_2D || (NEAR_EQUAL((_p)->coord[Y], (_v)->coord[Y], TOL_2D) && (_p)->coord[X] < (_v)->coord[X]))
#define P_LT_V(_p, _v) \
    (((_p)->coord[Y] - (_v)->coord[Y]) < (-TOL_2D) || (NEAR_EQUAL((_p)->coord[Y], (_v)->coord[Y], TOL_2D) && (_p)->coord[X] > (_v)->coord[X]))
#define P_GE_V(_p, _v) \
    (((_p)->coord[Y] - (_v)->coord[Y]) > TOL_2D || (NEAR_EQUAL((_p)->coord[Y], (_v)->coord[Y], TOL_2D) && (_p)->coord[X] <= (_v)->coord[X]))
#define P_LE_V(_p, _v) \
    (((_p)->coord[Y] - (_v)->coord[Y]) < (-TOL_2D) || (NEAR_EQUAL((_p)->coord[Y], (_v)->coord[Y], TOL_2D) && (_p)->coord[X] >= (_v)->coord[X]))

#define NMG_PT2D_MAGIC 0x2d2d2d2d
#define NMG_TRAP_MAGIC 0x1ab1ab
#define NMG_CK_PT2D(_p) NMG_CKMAG(_p, NMG_PT2D_MAGIC, "pt2d")
#define NMG_CK_TRAP(_p) {NMG_CKMAG(_p, NMG_TRAP_MAGIC, "trap");\
	if (! BU_LIST_PREV(bu_list, &(_p)->l)) {\
	    bu_log("%s %d bad prev pointer of trapezoid %p\n", \
		   __FILE__, __LINE__, (void *)&(_p)->l);	     \
	    bu_bomb("NMG_CK_TRAP: aborting");\
	} else if (! BU_LIST_NEXT(bu_list, &(_p)->l)) {\
	    bu_log("%s %d bad next pointer of trapezoid %p\n", \
		   __FILE__, __LINE__, (void *)&(_p)->l);	     \
	    bu_bomb("NMG_CL_TRAP: aborting");\
	}}

#define NMG_TBL2D_MAGIC 0x3e3e3e3e
#define NMG_CK_TBL2D(_p) NMG_CKMAG(_p, NMG_TBL2D_MAGIC, "tbl2d")

/* macros to retrieve the next/previous 2D point about loop */
#define PT2D_NEXT(tbl, pt) pt2d_pn(tbl, pt, 1)
#define PT2D_PREV(tbl, pt) pt2d_pn(tbl, pt, -1)

struct pt2d {
    struct bu_list l;		/* scanline ordered list of points */
    fastf_t coord[3];		/* point coordinates in 2-D space */
    struct vertexuse *vu_p;		/* associated vertexuse */
};


struct trap {
    struct bu_list l;
    struct pt2d *top;	   /* point at top of trapezoid */
    struct pt2d *bot;	   /* point at bottom of trapezoid */
    struct edgeuse *e_left;
    struct edgeuse *e_right;
};

struct loopuse_tree_node {
    struct bu_list l;
    struct loopuse *lu;
    struct loopuse_tree_node *parent;
    struct bu_list children_hd;
};

/* if there is an edge in this face which connects the points
 * return 1
 * else
 * return 0
 */


/* subroutine version to pass to the rt_tree functions */
int PvsV(struct trap *p, struct trap *v)
{
    NMG_CK_TRAP(p);
    NMG_CK_TRAP(v);

    if (p->top->coord[Y] > v->top->coord[Y]) return 1;
    else if (p->top->coord[Y] < v->top->coord[Y]) return -1;
    else if (p->top->coord[X] < v->top->coord[X]) return 1;
    else if (p->top->coord[X] > v->top->coord[X]) return -1;
    else return 0;
}


static struct pt2d *find_pt2d(struct bu_list *tbl2d, struct vertexuse *vu);
static FILE *plot_fp;
static int flatten_debug=1;

static struct pt2d *
find_pt2d(struct bu_list *tbl2d, struct vertexuse *vu)
{
    struct pt2d *p;
    NMG_CK_TBL2D(tbl2d);

    if (vu) {
	NMG_CK_VERTEXUSE(vu);
    }

    for (BU_LIST_FOR(p, pt2d, tbl2d)) {
	if (p->vu_p == vu) {
	    return p;
	}
    }
    return (struct pt2d *)NULL;
}


static void
nmg_tri_plfu(struct faceuse *fu, struct bu_list *tbl2d)
{
    static int file_number=0;
    FILE *fp;
    char name[25];
    char buf[80];
    long *b;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct vertexuse *vu;
    struct pt2d *p;

    NMG_CK_TBL2D(tbl2d);
    NMG_CK_FACEUSE(fu);

    sprintf(name, "tri%02d.plot3", file_number++);
    fp=fopen(name, "wb");
    if (fp == (FILE *)NULL) {
	perror(name);
	return;
    }

    bu_log("\tplotting %s\n", name);
    b = (long *)bu_calloc(fu->s_p->maxindex,
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

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	if (BU_LIST_IS_EMPTY(&lu->down_hd)) {
	    bu_log("Empty child list for loopuse %s %d\n", __FILE__, __LINE__);
	} else if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	    pdv_3move(fp, vu->v_p->vg_p->coord);
	    p=find_pt2d(tbl2d, vu);
	    if (p) {
		sprintf(buf, "%g, %g",
			p->coord[0], p->coord[1]);
		pl_label(fp, buf);
	    } else
		pl_label(fp, "X, Y (no 2D coords)");

	} else {
	    eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	    NMG_CK_EDGEUSE(eu);

	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		p=find_pt2d(tbl2d, eu->vu_p);
		if (p) {
		    pdv_3move(fp, eu->vu_p->v_p->vg_p->coord);

		    sprintf(buf, "%g, %g",
			    p->coord[0], p->coord[1]);
		    pl_label(fp, buf);
		} else
		    pl_label(fp, "X, Y (no 2D coords)");
	    }
	}
    }


    bu_free((char *)b, "plot table");
    fclose(fp);
}


/**
 * Return Prev/Next 2D pt about loop from given 2D pt.
 * if vertex is child of loopuse, return parameter 2D pt.
 */
static struct pt2d *
pt2d_pn(struct bu_list *tbl, struct pt2d *pt, int dir)
{
    struct edgeuse *eu, *eu_other;
    struct pt2d *new_pt;

    NMG_CK_TBL2D(tbl);
    NMG_CK_PT2D(pt);
    NMG_CK_VERTEXUSE((pt)->vu_p);

    if (*(pt)->vu_p->up.magic_p == NMG_EDGEUSE_MAGIC) {
	eu = (pt)->vu_p->up.eu_p;
	NMG_CK_EDGEUSE(eu);
	if (dir < 0)
	    eu_other = BU_LIST_PPREV_CIRC(edgeuse, eu);
	else
	    eu_other = BU_LIST_PNEXT_CIRC(edgeuse, eu);

	new_pt = find_pt2d(tbl, eu_other->vu_p);
	if (new_pt == (struct pt2d *)NULL) {
	    if (dir < 0)
		bu_log("can't find prev of %g %g\n",
		       pt->coord[X],
		       pt->coord[Y]);
	    else
		bu_log("can't find next of %g %g\n",
		       pt->coord[X],
		       pt->coord[Y]);
	    bu_bomb("goodbye\n");
	}
	NMG_CK_PT2D(new_pt);
	return new_pt;
    }

    if (*(pt)->vu_p->up.magic_p != NMG_LOOPUSE_MAGIC) {
	bu_log("%s %d Bad vertexuse parent (%g %g %g)\n",
	       __FILE__, __LINE__,
	       V3ARGS((pt)->vu_p->v_p->vg_p->coord));
	bu_bomb("goodbye\n");
    }

    return pt;
}


/**
 * Add a vertex to the 2D table if it isn't already there.
 */
static void
map_vu_to_2d(struct vertexuse *vu, struct bu_list *tbl2d, fastf_t *mat, struct faceuse *fu)
{
    struct vertex_g *vg;
    struct vertexuse *vu_p;
    struct vertex *vp;
    struct pt2d *p, *np;

    NMG_CK_TBL2D(tbl2d);
    NMG_CK_VERTEXUSE(vu);
    NMG_CK_FACEUSE(fu);

    /* if this vertexuse has already been transformed, we're done */
    if (find_pt2d(tbl2d, vu)) return;

    BU_ALLOC(np, struct pt2d);
    np->coord[2] = 0.0;
    np->vu_p = vu;
    BU_LIST_MAGIC_SET(&np->l, NMG_PT2D_MAGIC);

    /* if one of the other vertexuses has been mapped, use that data */
    for (BU_LIST_FOR(vu_p, vertexuse, &vu->v_p->vu_hd)) {
	p = find_pt2d(tbl2d, vu_p);
	if (p) {
	    VMOVE(np->coord, p->coord);
	    return;
	}
    }

    /* transform the 3-D vertex into a 2-D vertex */
    vg = vu->v_p->vg_p;
    MAT4X3PNT(np->coord, mat, vg->coord);


    if (RTG.NMG_debug & DEBUG_TRI && flatten_debug) bu_log(
	"Transforming %p 3D(%g, %g, %g) to 2D(%g, %g, %g)\n",
	(void *)vu, V3ARGS(vg->coord), V3ARGS(np->coord));

    /* find location in scanline ordered list for vertex */
    for (BU_LIST_FOR(p, pt2d, tbl2d)) {
	if (P_GT_V(p, np)) continue;
	break;
    }
    BU_LIST_INSERT(&p->l, &np->l);

    if (RTG.NMG_debug & DEBUG_TRI && flatten_debug)
	bu_log("transforming other vertexuses...\n");

    /* for all other uses of this vertex in this face, store the
     * transformed 2D vertex
     */
    vp = vu->v_p;

    for (BU_LIST_FOR(vu_p, vertexuse, &vp->vu_hd)) {
	register struct faceuse *fu_of_vu;
	NMG_CK_VERTEXUSE(vu_p);

	if (vu_p == vu) continue;

	fu_of_vu = nmg_find_fu_of_vu(vu_p);
	if (!fu_of_vu) continue;	/* skip vu's of wire edges */
	NMG_CK_FACEUSE(fu_of_vu);
	if (fu_of_vu != fu) continue;

	if (RTG.NMG_debug & DEBUG_TRI && flatten_debug)
	    bu_log("transform %p... ", (void *)vu_p);

	/* if vertexuse already transformed, skip it */
	if (find_pt2d(tbl2d, vu_p)) {
	    if (RTG.NMG_debug & DEBUG_TRI && flatten_debug) {
		bu_log("%p vertexuse already transformed\n", (void *)vu);
		nmg_pr_vu(vu, NULL);
	    }
	    continue;
	}

	/* add vertexuse to list */
	BU_ALLOC(p, struct pt2d);
	p->vu_p = vu_p;
	VMOVE(p->coord, np->coord);
	BU_LIST_MAGIC_SET(&p->l, NMG_PT2D_MAGIC);

	BU_LIST_APPEND(&np->l, &p->l);

	if (RTG.NMG_debug & DEBUG_TRI && flatten_debug)
	    bu_log("vertexuse transformed\n");
    }
    if (RTG.NMG_debug & DEBUG_TRI && flatten_debug)
	bu_log("Done.\n");
}


/**
 * Create the 2D coordinate table for the vertexuses of a face.
 *
 *	---------	-----------------------------------
 *	|pt2d --+-----> |     struct pt2d.{magic, coord[3]} |
 *	---------	-----------------------------------
 *			|     struct pt2d.{magic, coord[3]} |
 *			-----------------------------------
 *			|     struct pt2d.{magic, coord[3]} |
 *			-----------------------------------
 *
 * When the caller is done, nmg_free_2d_map() should be called to dispose
 * of the map
 */
struct bu_list *
nmg_flatten_face(struct faceuse *fu, fastf_t *TformMat, const struct bn_tol *tol)
{
    static const vect_t twoDspace = { 0.0, 0.0, 1.0 };
    struct bu_list *tbl2d;
    struct vertexuse *vu;
    struct loopuse *lu;
    struct edgeuse *eu;
    vect_t Normal;

    NMG_CK_FACEUSE(fu);

    BU_ALLOC(tbl2d, struct bu_list);

    /* we use the 0 index entry in the table as the head of the sorted
     * list of vertices.  This is safe since the 0 index is always for
     * the model structure
     */

    BU_LIST_INIT(tbl2d);
    BU_LIST_MAGIC_SET(tbl2d, NMG_TBL2D_MAGIC);

    /* construct the matrix that maps the 3D coordinates into 2D space */
    NMG_GET_FU_NORMAL(Normal, fu);
    bn_mat_fromto(TformMat, Normal, twoDspace, tol);

    if (RTG.NMG_debug & DEBUG_TRI && flatten_debug)
	bn_mat_print("TformMat", TformMat);


    /* convert each vertex in the face to its 2-D equivalent */
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	if (RTG.NMG_debug & DEBUG_TRI) {
	    switch (lu->orientation) {
		case OT_NONE:	bu_log("flattening OT_NONE loop\n"); break;
		case OT_SAME:	bu_log("flattening OT_SAME loop\n"); break;
		case OT_OPPOSITE:bu_log("flattening OT_OPPOSITE loop\n"); break;
		case OT_UNSPEC:	bu_log("flattening OT_UNSPEC loop\n"); break;
		case OT_BOOLPLACE:bu_log("flattening OT_BOOLPLACE loop\n"); break;
		default: bu_log("flattening bad orientation loop\n"); break;
	    }
	}
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	    if (RTG.NMG_debug & DEBUG_TRI && flatten_debug)
		bu_log("vertex loop\n");
	    map_vu_to_2d(vu, tbl2d, TformMat, fu);

	} else if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
	    if (RTG.NMG_debug & DEBUG_TRI && flatten_debug)
		bu_log("edge loop\n");
	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		vu = eu->vu_p;
		if (RTG.NMG_debug & DEBUG_TRI && flatten_debug)
		    bu_log("(%g %g %g) -> (%g %g %g)\n",
			   V3ARGS(vu->v_p->vg_p->coord),
			   V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord));
		map_vu_to_2d(vu, tbl2d, TformMat, fu);
	    }
	} else bu_bomb("bad magic of loopuse child\n");
    }

    return tbl2d;
}


static int
is_convex(struct pt2d *a, struct pt2d *b, struct pt2d *c, const struct bn_tol *tol)
{
    vect_t ab, bc, pv, N;
    double angle;

    NMG_CK_PT2D(a);
    NMG_CK_PT2D(b);
    NMG_CK_PT2D(c);

    /* invent surface normal */
    VSET(N, 0.0, 0.0, 1.0);

    /* form vector from a->b */
    VSUB2(ab, b->coord, a->coord);

    /* Form "left" vector */
    VCROSS(pv, N, ab);

    /* form vector from b->c */
    VSUB2(bc, c->coord, b->coord);

    /* find angle about normal in "pv" direction from a->b to b->c */
    angle = bn_angle_measure(bc, ab, pv);

    if (RTG.NMG_debug & DEBUG_TRI)
	bu_log("\tangle == %g tol angle: %g\n", angle, tol->perp);

    /* Since during loopuse triangulation, sometimes it is necessary
     * to allow degenerate triangles (i.e. zero area). Because of
     * this, we need to allow the definition of convex to include 0
     * and 180 degree angles.
     */
    return (angle >= -SMALL_FASTF) && (angle <= (M_PI + SMALL_FASTF));
}


static void
map_new_vertexuse(struct bu_list *tbl2d, struct vertexuse *vu_p)
{
    struct vertexuse *vu;
    struct pt2d *p, *new_pt2d;

    NMG_CK_TBL2D(tbl2d);
    NMG_CK_VERTEXUSE(vu_p);

    /* if it's already mapped we're outta here! */
    p = find_pt2d(tbl2d, vu_p);
    if (p) {
	if (RTG.NMG_debug & DEBUG_TRI)
	    bu_log("%s %d map_new_vertexuse() vertexuse already mapped!\n",
		   __FILE__, __LINE__);
	return;
    }
    /* allocate memory for new 2D point */
    BU_ALLOC(new_pt2d, struct pt2d);

    /* find another use of the same vertex that is already mapped */
    for (BU_LIST_FOR(vu, vertexuse, &vu_p->v_p->vu_hd)) {
	NMG_CK_VERTEXUSE(vu);
	p=find_pt2d(tbl2d, vu);
	if (!p)
	    continue;

	/* map parameter vertexuse */
	new_pt2d->vu_p = vu_p;
	VMOVE(new_pt2d->coord, p->coord);
	BU_LIST_MAGIC_SET(&new_pt2d->l, NMG_PT2D_MAGIC);
	BU_LIST_APPEND(&p->l, &new_pt2d->l);
	return;
    }

    bu_bomb("Couldn't find mapped vertexuse of vertex!\n");
}


/**
 * Support routine for
 * nmg_find_first_last_use_of_v_in_fu
 *
 * The object of the game here is to find uses of the given vertex whose
 * departing edges have the min/max dot product with the direction vector.
 *
 */
HIDDEN void
pick_edges(struct vertex *v, struct vertexuse **vu_first, int *min_dir, struct vertexuse **vu_last, int *max_dir, struct faceuse *fu, fastf_t *dir)


/* 1: forward -1 reverse */


{
    struct vertexuse *vu;
    struct edgeuse *eu_next, *eu_last;
    struct vertexuse *vu_next, *vu_prev;
    double dot_max = -2.0;
    double dot_min = 2.0;
    double vu_dot;
    double eu_length_sq;
    vect_t eu_dir;
    if (RTG.NMG_debug & DEBUG_TRI)
	bu_log("\t    pick_edges(v:(%g %g %g) dir:(%g %g %g))\n",
	       V3ARGS(v->vg_p->coord), V3ARGS(dir));

    /* Look at all the uses of this vertex, and find the uses
     * associated with an edgeuse in this faceuse.
     *
     * Compute the dot product of the direction vector with the vector
     * of the edgeuse, and the PRIOR edgeuse in the loopuse.
     *
     * We're looking for the vertexuses with the min/max edgeuse
     * vector dot product.
     */
    *vu_last = *vu_first = (struct vertexuse *)NULL;
    for (BU_LIST_FOR(vu, vertexuse, &v->vu_hd)) {
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_VERTEX(vu->v_p);
	NMG_CK_VERTEX_G(vu->v_p->vg_p);

	if (vu->v_p != v)
	    bu_bomb("vertexuse does not acknowledge parents\n");

	if (nmg_find_fu_of_vu(vu) != fu ||
	    *vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
	    if (RTG.NMG_debug & DEBUG_TRI)
		bu_log("\t\tskipping irrelevant vertexuse\n");
	    continue;
	}

	NMG_CK_EDGEUSE(vu->up.eu_p);

	/* compute/compare vu/eu vector w/ ray vector */
	eu_next = BU_LIST_PNEXT_CIRC(edgeuse, vu->up.eu_p);
	NMG_CK_EDGEUSE(eu_next);
	vu_next = eu_next->vu_p;
	NMG_CK_VERTEXUSE(vu_next);
	NMG_CK_VERTEX(vu_next->v_p);
	NMG_CK_VERTEX_G(vu_next->v_p->vg_p);
	VSUB2(eu_dir, vu_next->v_p->vg_p->coord, vu->v_p->vg_p->coord);
	eu_length_sq = MAGSQ(eu_dir);
	VUNITIZE(eu_dir);

	if (RTG.NMG_debug & DEBUG_TRI)
	    bu_log("\t\tchecking forward edgeuse to %g %g %g\n",
		   V3ARGS(vu_next->v_p->vg_p->coord));

	if (eu_length_sq > SMALL_FASTF) {
	    if ((vu_dot = VDOT(eu_dir, dir)) > dot_max) {
		if (RTG.NMG_debug & DEBUG_TRI) {
		    bu_log("\t\t\teu_dir %g %g %g\n",
			   V3ARGS(eu_dir));

		    bu_log("\t\t\tnew_last/max %p %g %g %g -> %g %g %g vdot %g\n",
			   (void *)vu,
			   V3ARGS(vu->v_p->vg_p->coord),
			   V3ARGS(vu_next->v_p->vg_p->coord),
			   vu_dot);
		}
		dot_max = vu_dot;
		*vu_last = vu;
		*max_dir = 1;
	    }
	    if (vu_dot < dot_min) {
		if (RTG.NMG_debug & DEBUG_TRI) {
		    bu_log("\t\t\teu_dir %g %g %g\n", V3ARGS(eu_dir));
		    bu_log("\t\t\tnew_first/min %p %g %g %g -> %g %g %g vdot %g\n",
			   (void *)vu,
			   V3ARGS(vu->v_p->vg_p->coord),
			   V3ARGS(vu_next->v_p->vg_p->coord),
			   vu_dot);
		}

		dot_min = vu_dot;
		*vu_first = vu;
		*min_dir = 1;
	    }
	}


	/* compute/compare vu/prev_eu vector w/ ray vector */
	eu_last = BU_LIST_PPREV_CIRC(edgeuse, vu->up.eu_p);
	NMG_CK_EDGEUSE(eu_last);
	vu_prev = eu_last->vu_p;
	NMG_CK_VERTEXUSE(vu_prev);
	NMG_CK_VERTEX(vu_prev->v_p);
	NMG_CK_VERTEX_G(vu_prev->v_p->vg_p);
	/* form vector in reverse direction so that all vectors
	 * "point out" from the vertex in question.
	 */
	VSUB2(eu_dir, vu_prev->v_p->vg_p->coord, vu->v_p->vg_p->coord);
	eu_length_sq = MAGSQ(eu_dir);
	VUNITIZE(eu_dir);

	if (RTG.NMG_debug & DEBUG_TRI)
	    bu_log("\t\tchecking reverse edgeuse to %g %g %g\n",
		   V3ARGS(vu_prev->v_p->vg_p->coord));

	if (eu_length_sq > SMALL_FASTF) {
	    if ((vu_dot = VDOT(eu_dir, dir)) > dot_max) {
		if (RTG.NMG_debug & DEBUG_TRI) {
		    bu_log("\t\t\t-eu_dir %g %g %g\n",
			   V3ARGS(eu_dir));
		    bu_log("\t\t\tnew_last/max %p %g %g %g <- %g %g %g vdot %g\n",
			   (void *)vu,
			   V3ARGS(vu->v_p->vg_p->coord),
			   V3ARGS(vu_prev->v_p->vg_p->coord),
			   vu_dot);
		}
		dot_max = vu_dot;
		*vu_last = vu;
		*max_dir = -1;
	    }
	    if (vu_dot < dot_min) {
		if (RTG.NMG_debug & DEBUG_TRI) {
		    bu_log("\t\t\teu_dir %g %g %g\n", V3ARGS(eu_dir));
		    bu_log("\t\t\tnew_first/min %p %g %g %g <- %g %g %g vdot %g\n",
			   (void *)vu,
			   V3ARGS(vu->v_p->vg_p->coord),
			   V3ARGS(vu_prev->v_p->vg_p->coord),
			   vu_dot);
		}
		dot_min = vu_dot;
		*vu_first = vu;
		*min_dir = -1;
	    }
	}
    }

}


/**
 * Support routine for
 * nmg_find_first_last_use_of_v_in_fu
 *
 * Given and edgeuse and a faceuse, pick the use of the edge in the faceuse
 * whose left vector has the largest/smallest dot product with the given
 * direction vector.  The parameter "find_max" determines whether we
 * return the edgeuse with the largest (find_max != 0) or the smallest
 * (find_max == 0) left-dot-product.
 */
struct edgeuse *
pick_eu(struct edgeuse *eu_p, struct faceuse *fu, fastf_t *dir, int find_max)
{
    struct edgeuse *eu, *keep_eu=NULL, *eu_next;
    int go_radial_not_mate = 0;
    double dot_limit;
    double euleft_dot;
    vect_t left, eu_vect;

    if (RTG.NMG_debug & DEBUG_TRI)
	bu_log("\t    pick_eu(%g %g %g  <-> %g %g %g, dir:%g %g %g,  %s)\n",
	       V3ARGS(eu_p->vu_p->v_p->vg_p->coord),
	       V3ARGS(eu_p->eumate_p->vu_p->v_p->vg_p->coord),
	       V3ARGS(dir), (find_max==0?"find min":"find max"));

    if (find_max) dot_limit = -2.0;
    else dot_limit = 2.0;

    /* walk around the edge looking for uses in this face */
    eu = eu_p;
    do {
	if (nmg_find_fu_of_eu(eu) == fu) {
	    /* compute the vector for this edgeuse */
	    eu_next = BU_LIST_PNEXT_CIRC(edgeuse, eu);
	    VSUB2(eu_vect, eu_next->vu_p->v_p->vg_p->coord,
		  eu->vu_p->v_p->vg_p->coord);
	    VUNITIZE(eu_vect);

	    /* compute the "left" vector for this edgeuse */
	    if (nmg_find_eu_leftvec(left, eu)) {
		bu_log("%s %d: edgeuse no longer in faceuse?\n", __FILE__, __LINE__);
		bu_bomb("bombing");
	    }

	    euleft_dot = VDOT(left, dir);

	    if (RTG.NMG_debug & DEBUG_TRI)
		bu_log("\t\tchecking: %g %g %g -> %g %g %g left vdot:%g\n",
		       V3ARGS(eu->vu_p->v_p->vg_p->coord),
		       V3ARGS(eu_next->vu_p->v_p->vg_p->coord),
		       euleft_dot);


	    /* if this is and edgeuse we need to remember, keep
	     * track of it while we go onward
	     */
	    if (find_max) {
		if (euleft_dot > dot_limit) {
		    dot_limit = euleft_dot;
		    keep_eu = eu;
		    if (RTG.NMG_debug & DEBUG_TRI)
			bu_log("\t\tnew max\n");
		}
	    } else {
		if (euleft_dot < dot_limit) {
		    dot_limit = euleft_dot;
		    keep_eu = eu;
		    if (RTG.NMG_debug & DEBUG_TRI)
			bu_log("\t\tnew min\n");
		}
	    }
	}

	if (go_radial_not_mate) eu = eu->eumate_p;
	else eu = eu->radial_p;
	go_radial_not_mate = ! go_radial_not_mate;

    } while (eu != eu_p);

    if (RTG.NMG_debug & DEBUG_TRI) {
	if (keep_eu) {
	    bu_log("\t\tpick_eu() returns %g %g %g -> %g %g %g\n\t\t\tbecause vdot(left) = %g\n",
		   V3ARGS(keep_eu->vu_p->v_p->vg_p->coord),
		   V3ARGS(keep_eu->eumate_p->vu_p->v_p->vg_p->coord),
		   dot_limit);
	} else {
	    bu_log("pick_eu() returns NULL");
	}
    }

    return keep_eu;
}


/**
 * Given a pointer to a vertexuse in a face and a ray, find the
 * "first" and "last" uses of the vertex along the ray in the face.
 * Consider the diagram below where 4 OT_SAME loopuses meet at a single
 * vertex.  The ray enters from the upper left and proceeds to the lower
 * right.  The ray encounters vertexuse (represented by "o" below)
 * number 1 first and vertexuse 3 last.
 *
 *
 *			 edge A
 *			 |
 *		     \  ^||
 *		      \ |||
 *		       1||V2
 *		------->o|o------->
 *  edge D --------------.-------------edge B
 *		<-------o|o<------
 *		       4^||3
 *		        ||| \
 *		        |||  \
 *		        ||V   \|
 *			 |    -
 *		    edge C
 *
 * The primary purpose of this routine is to find the vertexuses
 * that should be the parameters to nmg_cut_loop() and nmg_join_loop().
 */
void
nmg_find_first_last_use_of_v_in_fu(struct vertex *v, struct vertexuse **first_vu, struct vertexuse **last_vu, fastf_t *dir, struct faceuse *fu, const struct bn_tol *UNUSED(tol))
{
    struct vertexuse *vu_first, *vu_last;
    int max_dir=0, min_dir=0;	/* 1: forward -1 reverse */
    struct edgeuse *eu_first, *eu_last, *eu_p=NULL;

    NMG_CK_VERTEX(v);
    NMG_CK_FACEUSE(fu);
    if (first_vu == (struct vertexuse **)(NULL)) {
	bu_log("%s: %d first_vu is null ptr\n", __FILE__, __LINE__);
	bu_bomb("terminating\n");
    }
    if (last_vu == (struct vertexuse **)(NULL)) {
	bu_log("%s: %d last_vu is null ptr\n", __FILE__, __LINE__);
	bu_bomb("terminating\n");
    }

    VUNITIZE(dir);

    if (RTG.NMG_debug & DEBUG_TRI)
	bu_log("\t  nmg_find_first(v:(%g %g %g) dir:(%g %g %g))\n",
	       V3ARGS(v->vg_p->coord), V3ARGS(dir));

    /* go find the edges which are "closest" to the direction vector */
    pick_edges(v, &vu_first, &min_dir, &vu_last, &max_dir, fu, dir);


    /* Now we know which 2 edges are most important to look at.
     * The question now is which vertexuse on this edge to pick.
     * For example, in the diagram above we will choose a use of edge C
     * for our "max".  Either vu3 OR vu4 could be chosen.
     *
     * For our max/last point, we choose the use for which:
     * vdot(ray, eu_left_vector)
     * is largest.
     *
     * For our min/first point, we choose the use for which:
     * vdot(ray, eu_left_vector)
     * is smallest.
     */

    /* get an edgeuse of the proper edge */
    NMG_CK_VERTEXUSE(vu_first);
    switch (min_dir) {
	case -1:
	    eu_p = BU_LIST_PPREV_CIRC(edgeuse, vu_first->up.eu_p);
	    break;
	case 1:
	    eu_p = vu_first->up.eu_p;
	    break;
	default:
	    bu_bomb("bad max_dir\n");
	    break;
    }

    NMG_CK_EDGEUSE(eu_p);
    NMG_CK_VERTEXUSE(eu_p->vu_p);

    eu_first = pick_eu(eu_p, fu, dir, 0);
    NMG_CK_EDGEUSE(eu_first);
    NMG_CK_VERTEXUSE(eu_first->vu_p);
    NMG_CK_VERTEX(eu_first->vu_p->v_p);
    NMG_CK_VERTEX_G(eu_first->vu_p->v_p->vg_p);

    if (RTG.NMG_debug & DEBUG_TRI)
	bu_log("\t   first_eu: %g %g %g -> %g %g %g\n",
	       V3ARGS(eu_first->vu_p->v_p->vg_p->coord),
	       V3ARGS(eu_first->eumate_p->vu_p->v_p->vg_p->coord));


    if (eu_first->vu_p->v_p == v)
	/* if we wound up with and edgeuse whose vertexuse is
	 * actually on the vertex "v" we're in business, we
	 * simply record the vertexuse with this edgeuse.
	 */
	*first_vu = eu_first->vu_p;
    else {
	/* It looks like we wound up choosing an edgeuse which is
	 * "before" the vertex "v" (an edgeuse that points at "v")
	 * so we need to pick the vertexuse of the NEXT edgeuse
	 */
	NMG_CK_EDGEUSE(eu_first->eumate_p);
	NMG_CK_VERTEXUSE(eu_first->eumate_p->vu_p);
	NMG_CK_VERTEX(eu_first->eumate_p->vu_p->v_p);

	if (eu_first->eumate_p->vu_p->v_p == v) {
	    eu_p = BU_LIST_PNEXT_CIRC(edgeuse, eu_first);
	    NMG_CK_EDGEUSE(eu_p);
	    NMG_CK_VERTEXUSE(eu_p->vu_p);
	    *first_vu = eu_p->vu_p;
	} else {
	    bu_log("I got eu_first: %g %g %g -> %g %g %g but...\n",
		   V3ARGS(eu_first->vu_p->v_p->vg_p->coord),
		   V3ARGS(eu_first->eumate_p->vu_p->v_p->vg_p->coord));
	    bu_bomb("I can't find the right vertex\n");
	}
    }


    NMG_CK_VERTEXUSE(vu_last);
    switch (max_dir) {
	case -1:
	    eu_p = BU_LIST_PPREV_CIRC(edgeuse, vu_last->up.eu_p);
	    break;
	case 1:
	    eu_p = vu_last->up.eu_p;
	    break;
	default:
	    bu_bomb("bad min_dir\n");
	    break;
    }

    NMG_CK_EDGEUSE(eu_p);
    NMG_CK_VERTEXUSE(eu_p->vu_p);

    eu_last = pick_eu(eu_p, fu, dir, 1);

    NMG_CK_EDGEUSE(eu_last);
    NMG_CK_VERTEXUSE(eu_last->vu_p);
    NMG_CK_VERTEX(eu_last->vu_p->v_p);
    if (RTG.NMG_debug & DEBUG_TRI)
	bu_log("\t   last_eu: %g %g %g -> %g %g %g\n",
	       V3ARGS(eu_last->vu_p->v_p->vg_p->coord),
	       V3ARGS(eu_last->eumate_p->vu_p->v_p->vg_p->coord));


    if (eu_last->vu_p->v_p == v)
	/* if we wound up with and edgeuse whose vertexuse is
	 * actually on the vertex "v" we're in business, we
	 * simply record the vertexuse with this edgeuse.
	 */
	*last_vu = eu_last->vu_p;
    else {
	/* It looks like we wound up choosing an edgeuse which is
	 * "before" the vertex "v" (an edgeuse that points at "v")
	 * so we need to pick the vertexuse of the NEXT edgeuse
	 */
	NMG_CK_EDGEUSE(eu_last->eumate_p);
	NMG_CK_VERTEXUSE(eu_last->eumate_p->vu_p);
	NMG_CK_VERTEX(eu_last->eumate_p->vu_p->v_p);

	if (eu_last->eumate_p->vu_p->v_p == v) {
	    eu_p = BU_LIST_PNEXT_CIRC(edgeuse, eu_last);
	    NMG_CK_EDGEUSE(eu_p);
	    NMG_CK_VERTEXUSE(eu_p->vu_p);
	    *last_vu = eu_p->vu_p;
	} else {
	    bu_log("I got eu_last: %g %g %g -> %g %g %g but...\n",
		   V3ARGS(eu_last->vu_p->v_p->vg_p->coord),
		   V3ARGS(eu_last->eumate_p->vu_p->v_p->vg_p->coord));
	    bu_bomb("I can't find the right vertex\n");
	}
    }


    NMG_CK_VERTEXUSE(*first_vu);
    NMG_CK_VERTEXUSE(*last_vu);
}


static void
pick_pt2d_for_cutjoin(struct bu_list *tbl2d, struct pt2d **p1, struct pt2d **p2, const struct bn_tol *tol)
{
    struct vertexuse *cut_vu1, *cut_vu2, *junk_vu;
    struct faceuse *fu;
    vect_t dir;

    NMG_CK_TBL2D(tbl2d);

    if (RTG.NMG_debug & DEBUG_TRI)
	bu_log("\tpick_pt2d_for_cutjoin()\n");

    BN_CK_TOL(tol);
    NMG_CK_PT2D(*p1);
    NMG_CK_PT2D(*p2);
    NMG_CK_VERTEXUSE((*p1)->vu_p);
    NMG_CK_VERTEXUSE((*p2)->vu_p);

    cut_vu1 = (*p1)->vu_p;
    cut_vu2 = (*p2)->vu_p;

    NMG_CK_VERTEX(cut_vu1->v_p);
    NMG_CK_VERTEX_G(cut_vu1->v_p->vg_p);
    NMG_CK_VERTEX(cut_vu2->v_p);
    NMG_CK_VERTEX_G(cut_vu2->v_p->vg_p);

    /* If the 'from' and 'to' cut vertexuse are in the same loopuse,
     * it should be safe to assume the current pt2d records for these
     * vertexuse are correct for the cut. Therefore, do not search for
     * other pt2d vertexuse records, just exit this function using the
     * vertexuse pt2d records passed into this function. If we allow
     * this function to continue and search for alternate vertexuse,
     * sometimes, depending on the number of vertexuse at the cut
     * points, and the complexity of the angles between the edges at
     * the cuts, an incorrect vertexuse is often chosen. This change
     * avoids the error prone logic when we can assume the vertexuse
     * passed into this function are the correct vertexuse for the
     * cut, i.e. when the two vertexuse are in the same loopuse.
     */
    if ((*p1)->vu_p->up.eu_p->up.lu_p == (*p2)->vu_p->up.eu_p->up.lu_p) {
	return;
    }

    /* form direction vector for the cut we want to make */
    VSUB2(dir, cut_vu2->v_p->vg_p->coord,
	  cut_vu1->v_p->vg_p->coord);

    if (RTG.NMG_debug & DEBUG_TRI)
	VPRINT("\t\tdir", dir);

    fu = nmg_find_fu_of_vu(cut_vu1);
    if (!fu) {
	bu_log("%s: %d no faceuse parent of vu\n", __FILE__, __LINE__);
	bu_bomb("Bye now\n");
    }

    nmg_find_first_last_use_of_v_in_fu((*p1)->vu_p->v_p,
				       &junk_vu, &cut_vu1, dir, fu, tol);

    NMG_CK_VERTEXUSE(junk_vu);
    NMG_CK_VERTEXUSE(cut_vu1);
    *p1 = find_pt2d(tbl2d, cut_vu1);

    if (RTG.NMG_debug & DEBUG_TRI) {
	struct pt2d *pj, *pj_n, *p1_n;

	pj = find_pt2d(tbl2d, junk_vu);
	pj_n = PT2D_NEXT(tbl2d, pj);

	p1_n = PT2D_NEXT(tbl2d, (*p1));

	bu_log("\tp1 pick %g %g -> %g %g (first)\n\t\t%g %g -> %g %g (last)\n",
	       pj->coord[0], pj->coord[1],
	       pj_n->coord[0], pj_n->coord[1],
	       (*p1)->coord[0], (*p1)->coord[1],
	       p1_n->coord[0], p1_n->coord[1]);
    }


    nmg_find_first_last_use_of_v_in_fu((*p2)->vu_p->v_p,
				       &cut_vu2, &junk_vu, dir, fu, tol);


    *p2 = find_pt2d(tbl2d, cut_vu2);
    if (RTG.NMG_debug & DEBUG_TRI) {
	struct pt2d *pj, *pj_n, *p2_n;

	pj = find_pt2d(tbl2d, junk_vu);
	pj_n = PT2D_NEXT(tbl2d, pj);

	p2_n = PT2D_NEXT(tbl2d, (*p2));
	bu_log("\tp2 pick %g %g -> %g %g (first)\n\t\t%g %g -> %g %g (last)\n",
	       (*p2)->coord[0], (*p2)->coord[1],
	       p2_n->coord[0], p2_n->coord[1],
	       pj->coord[0], pj->coord[1],
	       pj_n->coord[0], pj_n->coord[1]);
    }

}


static void join_mapped_loops(struct bu_list *tbl2d, struct pt2d *p1, struct pt2d *p2, const int *color, const struct bn_tol *tol);

/**
 *
 * Cut a loop which has a 2D mapping.  Since this entails the creation
 * of new vertexuses, it is necessary to add a 2D mapping for the new
 * vertexuses.
 *
 */
static struct pt2d *
cut_mapped_loop(struct bu_list *tbl2d, struct pt2d *p1, struct pt2d *p2, const int *color, const struct bn_tol *tol, int void_ok)
{
    struct loopuse *new_lu;
    struct loopuse *old_lu;
    struct edgeuse *eu;

    NMG_CK_TBL2D(tbl2d);
    BN_CK_TOL(tol);
    NMG_CK_PT2D(p1);
    NMG_CK_PT2D(p2);
    NMG_CK_VERTEXUSE(p1->vu_p);
    NMG_CK_VERTEXUSE(p2->vu_p);

    if (RTG.NMG_debug & DEBUG_TRI)
	bu_log("\tcutting loop @ %g %g -> %g %g\n",
	       p1->coord[X], p1->coord[Y],
	       p2->coord[X], p2->coord[Y]);

    if (p1->vu_p->up.eu_p->up.lu_p != p2->vu_p->up.eu_p->up.lu_p) {
	bu_log("parent loops are not the same %s %d\n", __FILE__, __LINE__);
	bu_bomb("cut_mapped_loop() goodnight 1\n");
    }

    pick_pt2d_for_cutjoin(tbl2d, &p1, &p2, tol);

    if (p1->vu_p->up.eu_p->up.lu_p != p2->vu_p->up.eu_p->up.lu_p) {
	if (void_ok) {
	    if (RTG.NMG_debug)
		bu_log("cut_mapped_loop() parent loops are not the same %s %d, trying join\n", __FILE__, __LINE__);
	    join_mapped_loops(tbl2d, p1, p2, color, tol);
	    return (struct pt2d *)NULL;
	} else {
	    char buf[80];
	    char name[32];
	    static int iter=0;
	    vect_t cut_vect, cut_start, cut_end;
	    FILE *fp;

	    bu_log("parent loops are not the same %s %d\n",
		   __FILE__, __LINE__);


	    sprintf(buf, "cut %g %g %g -> %g %g %g\n",
		    V3ARGS(p1->vu_p->v_p->vg_p->coord),
		    V3ARGS(p2->vu_p->v_p->vg_p->coord));

	    sprintf(name, "bad_tri_cut%d.g", iter++);
	    fp=fopen("bad_tri_cut.plot3", "wb");
	    if (fp == (FILE *)NULL)
		bu_bomb("cut_mapped_loop() goodnight 2\n");

	    VSUB2(cut_vect, p2->vu_p->v_p->vg_p->coord, p1->vu_p->v_p->vg_p->coord);
	    /* vector goes past end point by 50% */
	    VJOIN1(cut_end, p2->vu_p->v_p->vg_p->coord, 0.5, cut_vect);
	    /* vector starts before start point by 25% */
	    VJOIN1(cut_start, p1->vu_p->v_p->vg_p->coord, -0.25, cut_vect);

	    pl_color(fp, 100, 100, 100);
	    pdv_3line(fp, cut_start, p1->vu_p->v_p->vg_p->coord);
	    pl_color(fp, 255, 255, 255);
	    pdv_3line(fp, p1->vu_p->v_p->vg_p->coord, p2->vu_p->v_p->vg_p->coord);
	    pl_color(fp, 100, 100, 100);
	    pdv_3line(fp, p2->vu_p->v_p->vg_p->coord, cut_end);

	    (void)fclose(fp);
	    nmg_stash_shell_to_file("bad_tri_cut.g",
				    nmg_find_shell(&p1->vu_p->l.magic), buf);

	    bu_bomb("cut_mapped_loop() goodnight 3\n");
	}
    }

    if (plot_fp) {
	pl_color(plot_fp, V3ARGS(color));
	pdv_3line(plot_fp, p1->coord, p2->coord);
    }

    old_lu = p1->vu_p->up.eu_p->up.lu_p;
    NMG_CK_LOOPUSE(old_lu);
    new_lu = nmg_cut_loop(p1->vu_p, p2->vu_p);
    NMG_CK_LOOPUSE(new_lu);
    NMG_CK_LOOP(new_lu->l_p);
    nmg_loop_g(new_lu->l_p, tol);

    /* XXX Does anyone care about loopuse orientations at this stage?
       nmg_lu_reorient(old_lu);
       nmg_lu_reorient(new_lu);
    */

    /* get the edgeuse of the new vertexuse we just created */
    eu = BU_LIST_PPREV_CIRC(edgeuse, &new_lu->down_hd);
    NMG_CK_EDGEUSE(eu);

    /* if the original vertexuses had normals,
     * copy them to the new vertexuses.
     */
    if (p1->vu_p->a.magic_p && *p1->vu_p->a.magic_p == NMG_VERTEXUSE_A_PLANE_MAGIC) {
	struct vertexuse *vu;
	struct faceuse *fu;
	struct loopuse *lu;
	vect_t ot_same_normal;
	vect_t ot_opposite_normal;

	/* get vertexuse normal */
	VMOVE(ot_same_normal, p1->vu_p->a.plane_p->N);
	fu = nmg_find_fu_of_vu(p1->vu_p);
	NMG_CK_FACEUSE(fu);
	if (fu->orientation == OT_OPPOSITE)
	    VREVERSE(ot_same_normal, ot_same_normal);

	VREVERSE(ot_opposite_normal, ot_same_normal);

	/* look for new vertexuses in new_lu and old_lu */
	for (BU_LIST_FOR(vu, vertexuse, &p1->vu_p->v_p->vu_hd)) {
	    if (vu->a.magic_p)
		continue;

	    lu = nmg_find_lu_of_vu(vu);
	    if (lu != old_lu && lu != old_lu->lumate_p &&
		lu != new_lu && lu != new_lu->lumate_p)
		continue;

	    /* assign appropriate normal */
	    fu = nmg_find_fu_of_vu(vu);
	    if (fu->orientation == OT_SAME)
		nmg_vertexuse_nv(vu, ot_same_normal);
	    else if (fu->orientation == OT_OPPOSITE)
		nmg_vertexuse_nv(vu, ot_opposite_normal);
	}
    }
    if (p2->vu_p->a.magic_p && *p2->vu_p->a.magic_p == NMG_VERTEXUSE_A_PLANE_MAGIC) {
	struct vertexuse *vu;
	struct faceuse *fu;
	struct loopuse *lu;
	vect_t ot_same_normal;
	vect_t ot_opposite_normal;

	/* get vertexuse normal */
	VMOVE(ot_same_normal, p2->vu_p->a.plane_p->N);
	fu = nmg_find_fu_of_vu(p2->vu_p);
	NMG_CK_FACEUSE(fu);
	if (fu->orientation == OT_OPPOSITE)
	    VREVERSE(ot_same_normal, ot_same_normal);

	VREVERSE(ot_opposite_normal, ot_same_normal);

	/* look for new vertexuses in new_lu and old_lu */
	for (BU_LIST_FOR(vu, vertexuse, &p2->vu_p->v_p->vu_hd)) {
	    if (vu->a.magic_p)
		continue;

	    lu = nmg_find_lu_of_vu(vu);
	    if (lu != old_lu && lu != old_lu->lumate_p &&
		lu != new_lu && lu != new_lu->lumate_p)
		continue;

	    /* assign appropriate normal */
	    fu = nmg_find_fu_of_vu(vu);
	    if (fu->orientation == OT_SAME)
		nmg_vertexuse_nv(vu, ot_same_normal);
	    else if (fu->orientation == OT_OPPOSITE)
		nmg_vertexuse_nv(vu, ot_opposite_normal);
	}
    }

    /* map it to the 2D plane */
    map_new_vertexuse(tbl2d, eu->vu_p);

    /* now map the vertexuse on the radially-adjacent edgeuse */
    NMG_CK_EDGEUSE(eu->radial_p);
    map_new_vertexuse(tbl2d, eu->radial_p->vu_p);

    eu = BU_LIST_PPREV_CIRC(edgeuse, &(p1->vu_p->up.eu_p->l));
    return find_pt2d(tbl2d, eu->vu_p);
}


/**
 *
 * Join 2 loops (one forms a hole in the other usually)
 *
 */
static void
join_mapped_loops(struct bu_list *tbl2d, struct pt2d *p1, struct pt2d *p2, const int *color, const struct bn_tol *tol)
{
    struct vertexuse *vu, *vu1, *vu2;
    struct edgeuse *eu = (struct edgeuse *)NULL;
    struct loopuse *lu = (struct loopuse *)NULL;

    NMG_CK_TBL2D(tbl2d);
    NMG_CK_PT2D(p1);
    NMG_CK_PT2D(p2);
    BN_CK_TOL(tol);

    vu = (struct vertexuse *)NULL;
    vu1 = p1->vu_p;
    vu2 = p2->vu_p;

    NMG_CK_VERTEXUSE(vu1);
    NMG_CK_VERTEXUSE(vu2);

    if (RTG.NMG_debug & DEBUG_TRI)
	bu_log("join_mapped_loops()\n");

    if (((p1->vu_p->up.eu_p->up.lu_p->orientation != OT_OPPOSITE) &&
	 (p1->vu_p->up.eu_p->up.lu_p->orientation != OT_SAME)) ||
	((p2->vu_p->up.eu_p->up.lu_p->orientation != OT_OPPOSITE) &&
	 (p2->vu_p->up.eu_p->up.lu_p->orientation != OT_SAME))) {
	bu_bomb("join_mapped_loops(): loopuse orientation is not OT_SAME or OT_OPPOSITE\n");
    }

    if ((p1->vu_p->up.eu_p->up.lu_p->orientation == OT_OPPOSITE) &&
	(p2->vu_p->up.eu_p->up.lu_p->orientation == OT_OPPOSITE)) {
	bu_bomb("join_mapped_loops(): both loopuse can not have orientation OT_OPPOSITE (1)\n");
    }

    if (p1 == p2) {
	bu_log("join_mapped_loops(): %s %d Attempting to join loop to itself at (%g %g %g)?\n",
	       __FILE__, __LINE__,
	       V3ARGS(p1->vu_p->v_p->vg_p->coord));
	bu_bomb("join_mapped_loops(): bombing\n");
    } else if (p1->vu_p->up.eu_p->up.lu_p == p2->vu_p->up.eu_p->up.lu_p) {
	bu_log("join_mapped_loops(): parent loops are the same %s %d\n", __FILE__, __LINE__);
	bu_bomb("join_mapped_loops(): bombing\n");
    }
    /* check to see if we're joining two loops that share a vertex */
    if (p1->vu_p->v_p == p2->vu_p->v_p) {
	if (RTG.NMG_debug & DEBUG_TRI) {
	    bu_log("join_mapped_loops(): Joining two loops that share a vertex at (%g %g %g)\n",
		   V3ARGS(p1->vu_p->v_p->vg_p->coord));
	}
	vu = nmg_join_2loops(p1->vu_p,  p2->vu_p);
	goto out;
    }

    pick_pt2d_for_cutjoin(tbl2d, &p1, &p2, tol);

    if (p1->vu_p->up.eu_p->up.lu_p == p2->vu_p->up.eu_p->up.lu_p) {
	bu_bomb("join_mapped_loops(): attempting to join a loopuse to itself\n");
    }
    if (p1->vu_p == p2->vu_p) {
	bu_bomb("join_mapped_loops(): attempting to join a vertexuse to itself\n");
    }

    vu1 = p1->vu_p;
    vu2 = p2->vu_p;
    NMG_CK_VERTEXUSE(vu1);
    NMG_CK_VERTEXUSE(vu2);

    if (p1 == p2) {
	bu_log("join_mapped_loops(): %s %d trying to join a vertexuse (%g %g %g) to itself\n",
	       __FILE__, __LINE__,
	       V3ARGS(p1->vu_p->v_p->vg_p->coord));
    } else if (p1->vu_p->up.eu_p->up.lu_p == p2->vu_p->up.eu_p->up.lu_p) {
	if (RTG.NMG_debug & DEBUG_TRI) {
	    bu_log("join_mapped_loops(): parent loops are the same %s %d\n",
		   __FILE__, __LINE__);
	}
	(void)cut_mapped_loop(tbl2d, p1, p2, color, tol, 1);
	return;
    }

    if ((vu1->up.eu_p->up.lu_p->orientation == OT_OPPOSITE) &&
	(vu2->up.eu_p->up.lu_p->orientation == OT_OPPOSITE)) {
	bu_bomb("join_mapped_loops(): both loopuse can not have orientation OT_OPPOSITE (2)\n");
    }

    if (*vu2->up.magic_p != NMG_EDGEUSE_MAGIC) {
	bu_bomb("join_mapped_loops(): vertexuse vu2 must belong to an edgeuse\n");
    }

    NMG_CK_EDGEUSE(vu2->up.eu_p);

    /* need to save this so we can use it later to get
     * the new "next" edge/vertexuse
     */
    eu = BU_LIST_PPREV_CIRC(edgeuse, vu2->up.eu_p);


    if (RTG.NMG_debug & DEBUG_TRI) {
	struct edgeuse *pr1_eu;
	struct edgeuse *pr2_eu;

	pr1_eu = BU_LIST_PNEXT_CIRC(edgeuse, vu1->up.eu_p);
	pr2_eu = BU_LIST_PNEXT_CIRC(edgeuse, vu2->up.eu_p);

	bu_log("join_mapped_loops(): joining loops between:\n\t%g %g %g -> (%g %g %g)\n\tand%g %g %g -> (%g %g %g)\n",
	       V3ARGS(vu1->v_p->vg_p->coord),
	       V3ARGS(pr1_eu->vu_p->v_p->vg_p->coord),
	       V3ARGS(vu2->v_p->vg_p->coord),
	       V3ARGS(pr2_eu->vu_p->v_p->vg_p->coord));
    }

    vu = nmg_join_2loops(vu1, vu2);
    if (plot_fp) {
	pl_color(plot_fp, V3ARGS(color));
	pdv_3line(plot_fp, p1->coord,  p2->coord);
    }


    NMG_CK_VERTEXUSE(vu);

    if (vu == vu2) {
	bu_bomb("join_mapped_loops(): vu == vu2\n");
    }
    /* since we've just made some new vertexuses
     * we need to map them to the 2D plane.
     *
     * XXX This should be made more direct and efficient.  For now we
     * just go looking for vertexuses without a mapping.
     */
out:
    NMG_CK_EDGEUSE(vu->up.eu_p);
    NMG_CK_LOOPUSE(vu->up.eu_p->up.lu_p);
    lu = vu->up.eu_p->up.lu_p;
    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	if (!find_pt2d(tbl2d, eu->vu_p)) {
	    map_new_vertexuse(tbl2d, eu->vu_p);
	}
    }
}


void
nmg_plot_fu(const char *prefix, const struct faceuse *fu, const struct bn_tol *UNUSED(tol))
{
    struct loopuse *lu;
    struct edgeuse *eu;
    int edgeuse_vert_count = 0;
    int non_consec_edgeuse_vert_count = 0;
    int faceuse_loopuse_count = 0;
    struct vertex *prev_v_p = (struct vertex *)NULL;
    struct vertex *curr_v_p = (struct vertex *)NULL;
    struct vertex *first_v_p = (struct vertex *)NULL;
    struct edgeuse *curr_eu = (struct edgeuse *)NULL;
    struct edgeuse *prev_eu = (struct edgeuse *)NULL;
    struct edgeuse *first_eu = (struct edgeuse *)NULL;
    FILE *plotfp;
    struct bu_vls plot_file_name = BU_VLS_INIT_ZERO;

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {

	/* skip loops which do not contain edges */
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
	    continue;
	}
	non_consec_edgeuse_vert_count = 0;
	edgeuse_vert_count = 0;
	prev_v_p = (struct vertex *)NULL;
	curr_v_p = (struct vertex *)NULL;
	first_v_p = (struct vertex *)NULL;
	curr_eu = (struct edgeuse *)NULL;
	prev_eu = (struct edgeuse *)NULL;
	first_eu = (struct edgeuse *)NULL;

	faceuse_loopuse_count++;

	bu_vls_sprintf(&plot_file_name, "%s_faceuse_%p_loopuse_%p.pl",
		       prefix, (void *)fu, (void *)lu);
	plotfp = fopen(bu_vls_addr(&plot_file_name), "wb");

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    curr_v_p = eu->vu_p->v_p;
	    curr_eu = eu;
	    if (edgeuse_vert_count == 0) {
		first_v_p = curr_v_p;
		first_eu = eu;
	    }
	    if (edgeuse_vert_count <= 1) {
		if (first_eu->e_p->is_real) {
		    /* if 1st edgeuse is real set start edge plot color to hot pink */
		    pl_color(plotfp, 255,105,180);
		} else {
		    /* set first segment red */
		    pl_color(plotfp, 255,0,0);
		}
	    } else {
		if (prev_eu->e_p->is_real) {
		    /* set middle segments shades of yellow */
		    pl_color(plotfp, ((edgeuse_vert_count * 30) % 155) + 100,
			     ((edgeuse_vert_count * 30) % 155) + 100, 0);
		} else {
		    /* set middle segments shades of green */
		    pl_color(plotfp, 0, ((edgeuse_vert_count * 30) % 155) + 100, 0);
		}
	    }
	    edgeuse_vert_count++;
	    if (curr_v_p != prev_v_p) {
		non_consec_edgeuse_vert_count++;
	    }
	    if (edgeuse_vert_count > 1) {
		bn_dist_pt3_pt3(prev_v_p->vg_p->coord,curr_v_p->vg_p->coord);
		pdv_3line(plotfp, prev_v_p->vg_p->coord, curr_v_p->vg_p->coord);
	    }
	    prev_v_p = curr_v_p;
	    prev_eu = curr_eu;
	}

	if (curr_v_p && first_v_p) {
	    bn_dist_pt3_pt3(first_v_p->vg_p->coord,curr_v_p->vg_p->coord);
	    if (curr_eu->e_p->is_real) {
		/* set last segment if is_real to cyan */
		pl_color(plotfp, 0, 255, 255);
	    } else {
		/* set last segment blue */
		pl_color(plotfp, 0,0,255);
	    }
	    pdv_3line(plotfp, curr_v_p->vg_p->coord, first_v_p->vg_p->coord);
	}
	(void)fclose(plotfp);
	bu_vls_free(&plot_file_name);
    }
}

/*
 * 0 = no isect (out)
 * 1 = isect edge, but not vertex (on)
 * 2 = isect vertex (shared, vertex fused)
 * 3 = isect vertex (non-shared, vertex not fused)
 * 4 = inside (on)
 */

int
nmg_isect_pt_facet(struct vertex *v, struct vertex *v0, struct vertex *v1, struct vertex *v2, const struct bn_tol *tol)
{
    fastf_t *p, *p0, *p1, *p2;
    fastf_t dp0p1, dp0p2, dp1p2; /* dist p0->p1, p0->p2, p1->p2 */
    fastf_t dpp0, dpp1, dpp2;    /* dist p->p0, p->p1, p->p2 */
    point_t bb_min, bb_max;
    vect_t p0_p2, p0_p1, p0_p;
    fastf_t p0_p2_magsq, p0_p1_magsq;
    fastf_t p0_p2__p0_p1, p0_p2__p0_p, p0_p1__p0_p;
    fastf_t u, v00, u_numerator, v_numerator, denom;
    int degen_p0p1, degen_p0p2, degen_p1p2;
    int para_p0_p1__p0_p;
    int para_p0_p2__p0_p;
    int para_p1_p2__p1_p;

    NMG_CK_VERTEX(v);
    NMG_CK_VERTEX(v0);
    NMG_CK_VERTEX(v1);
    NMG_CK_VERTEX(v2);
    NMG_CK_VERTEX_G(v->vg_p);
    NMG_CK_VERTEX_G(v0->vg_p);
    NMG_CK_VERTEX_G(v1->vg_p);
    NMG_CK_VERTEX_G(v2->vg_p);

    p = v->vg_p->coord;
    p0 = v0->vg_p->coord;
    p1 = v1->vg_p->coord;
    p2 = v2->vg_p->coord;

    degen_p0p1 = degen_p0p2 = degen_p1p2 = 0;
    if (v0 == v1) {
	degen_p0p1 = 1;
    }
    if (v0 == v2) {
	degen_p0p2 = 1;
    }
    if (v1 == v2) {
	degen_p1p2 = 1;
    }

    if (v == v0 || v == v1 || v == v2) {
	return 2; /* isect vertex (shared, vertex fused) */
    }

    /* find facet bounding box */
    VSETALL(bb_min, INFINITY);
    VSETALL(bb_max, -INFINITY);
    VMINMAX(bb_min, bb_max, p0);
    VMINMAX(bb_min, bb_max, p1);
    VMINMAX(bb_min, bb_max, p2);

    if (V3PT_OUT_RPP_TOL(p, bb_min, bb_max, tol->dist)) {
	return 0; /* no isect */
    }

    dp0p1 = bn_dist_pt3_pt3(p0, p1);
    dp0p2 = bn_dist_pt3_pt3(p0, p2);
    dp1p2 = bn_dist_pt3_pt3(p1, p2);

    if (!degen_p0p1 && (dp0p1 < tol->dist)) {
	degen_p0p1 = 2;
    }
    if (!degen_p0p2 && (dp0p2 < tol->dist)) {
	degen_p0p2 = 2;
    }
    if (!degen_p1p2 && (dp1p2 < tol->dist)) {
	degen_p1p2 = 2;
    }

    dpp0 = bn_dist_pt3_pt3(p, p0);
    dpp1 = bn_dist_pt3_pt3(p, p1);
    dpp2 = bn_dist_pt3_pt3(p, p2);

    if (dpp0 < tol->dist || dpp1 < tol->dist || dpp2 < tol->dist) {
	return 3; /* isect vertex (non-shared, vertex not fused) */
    }

    para_p0_p1__p0_p = para_p0_p2__p0_p = para_p1_p2__p1_p = 0;
    /* test p against edge p0->p1 */
    if (!degen_p0p1 && bn_lseg3_lseg3_parallel(p0, p1, p0, p, tol)) {
	/* p might be on edge p0->p1 */
	para_p0_p1__p0_p = 1;
	if (NEAR_EQUAL(dpp0 + dpp1, dp0p1, tol->dist)) {
	    /* p on edge p0->p1 */
	    return 1; /* isect edge, but not vertex (on) */
	}
    }
    /* test p against edge p0->p2 */
    if (!degen_p0p2 && bn_lseg3_lseg3_parallel(p0, p2, p0, p, tol)) {
	/* p might be on edge p0->p2 */
	para_p0_p2__p0_p = 1;
	if (NEAR_EQUAL(dpp0 + dpp2, dp0p2, tol->dist)) {
	    /* p on edge p0->p2 */
	    return 1; /* isect edge, but not vertex (on) */
	}
    }
    /* test p against edge p1->p2 */
    if (!degen_p1p2 && bn_lseg3_lseg3_parallel(p1, p2, p1, p, tol)) {
	/* p might be on edge p1->p2 */
	para_p1_p2__p1_p = 1;
	if (NEAR_EQUAL(dpp1 + dpp2, dp1p2, tol->dist)) {
	    /* p on edge p1->p2 */
	    return 1; /* isect edge, but not vertex (on) */
	}
    }

    if (degen_p0p1 || degen_p0p2 || degen_p1p2) {
	return 0; /* no isect (out) */
    }
    if (para_p0_p1__p0_p || para_p0_p2__p0_p || para_p1_p2__p1_p) {
	return 0; /* no isect (out) */
    }

    VSUB2(p0_p2, p2, p0);
    VSUB2(p0_p1, p1, p0);
    VSUB2(p0_p, p, p0);

    p0_p2_magsq = MAGSQ(p0_p2);
    p0_p1_magsq = MAGSQ(p0_p1);
    p0_p2__p0_p1 = VDOT(p0_p2, p0_p1);
    p0_p2__p0_p = VDOT(p0_p2, p0_p);
    p0_p1__p0_p = VDOT(p0_p1, p0_p);

    u_numerator = ( p0_p1_magsq * p0_p2__p0_p - p0_p2__p0_p1 * p0_p1__p0_p );
    v_numerator = ( p0_p2_magsq * p0_p1__p0_p - p0_p2__p0_p1 * p0_p2__p0_p );
    denom = ( p0_p2_magsq * p0_p1_magsq - p0_p2__p0_p1 * p0_p2__p0_p1 );

    if (ZERO(denom)) {
	denom = 1.0;
    }

    if (NEAR_ZERO(u_numerator, tol->dist)) {
	u_numerator = 0.0;
	u = 0.0;
    } else {
	u = u_numerator / denom;
    }

    if (NEAR_ZERO(v_numerator, tol->dist)) {
	v_numerator = 0.0;
	v00 = 0.0;
    } else {
	v00 = v_numerator / denom;
    }

    if ((u > SMALL_FASTF) && (v00 > SMALL_FASTF) && ((u + v00) < (1.0 - SMALL_FASTF))) {
	return 4; /* inside (on) */
    }

    return 0; /* no isect (out) */
}


/**
 * Given the faceuse 'fu' test if the potential cut, defined by an
 * edgeuse from 'eu1->vu_p' to 'eu2->vu_p', intersects any edgeuse of
 * the faceuse.
 *
 * Return 1 if intersect found otherwise return 0.
 */
int
nmg_isect_potcut_fu(struct edgeuse *eu1, struct edgeuse *eu2, struct faceuse *fu, const struct bn_tol *tol)
{
    point_t p1, p2, q1, q2;
    vect_t  pdir, qdir;
    fastf_t dist[2];
    int status;
    int hit = 0;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct vertexuse *vu1b, *vu2b;
    struct vertexuse *vu1, *vu2;

    vu1 = eu1->vu_p;
    vu1b = (BU_LIST_PNEXT_CIRC(edgeuse, &eu1->eumate_p->l))->vu_p;
    vu2 = eu2->vu_p;
    vu2b = (BU_LIST_PNEXT_CIRC(edgeuse, &eu2->eumate_p->l))->vu_p;

    /* vu1b and vu2b are the vertexuse of the faceuse mate that shares
     * the same vertex as vu1 and vu2, respectively.
     */

    BN_CK_TOL(tol);
    NMG_CK_FACEUSE(fu);

    /* Loop thru all loopuse, looking for intersections */
    hit = 0;
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	NMG_CK_LOOPUSE(lu);

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {

	    /* skip testing eu1 and eu2 */
	    if (eu->vu_p == vu1 || eu->vu_p == vu1b || eu->vu_p == vu2 || eu->vu_p == vu2b) {
		continue;
	    }

	    NMG_CK_EDGEUSE(eu);

	    NMG_CK_VERTEXUSE(vu2);
	    NMG_CK_VERTEXUSE(vu1);
	    NMG_CK_VERTEXUSE(eu->vu_p);
	    NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);

	    NMG_CK_VERTEX(vu2->v_p);
	    NMG_CK_VERTEX(vu1->v_p);
	    NMG_CK_VERTEX(eu->vu_p->v_p);
	    NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);

	    NMG_CK_VERTEX_G(vu2->v_p->vg_p);
	    NMG_CK_VERTEX_G(vu1->v_p->vg_p);
	    NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
	    NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);

	    /* Test if edge is zero length, if so bomb since this
	     * should not happen.
	     */
	    if (eu->vu_p->v_p->vg_p->coord == eu->eumate_p->vu_p->v_p->vg_p->coord) {
		bu_bomb("nmg_isect_lseg3_eu(): found zero length edge\n");
	    }

	    VMOVE(p1, vu2->v_p->vg_p->coord);
	    VMOVE(p2, vu1->v_p->vg_p->coord);
	    VSUB2(pdir, p2, p1);
	    VMOVE(q1, eu->vu_p->v_p->vg_p->coord);
	    VMOVE(q2, eu->eumate_p->vu_p->v_p->vg_p->coord);
	    VSUB2(qdir, q2, q1);

	    status = bn_isect_lseg3_lseg3(dist, p1, pdir, q1, qdir, tol);

	    if (status == 0) {  /* colinear and overlapping */
		/* Hit because, can only skip if hit on end point only
		 * and overlapping indicates more than an end point
		 * was hit.
		 */
		hit = 1;
	    } else if (status == 1) {  /* normal intersection */

		/* When either (p1 = q1) or (p2 = q1) meaning either
		 * the start or end vertex of pot-cut (potential cut
		 * between hole-loopuse and non-hole-loopuse) is the
		 * same as the 1st vertex of the current edgeuse being
		 * tested then the corresponding vertex pointers
		 * should also be equal, verify this is true, if not,
		 * bomb so problem can be investigated.
		 */

		if (((NEAR_ZERO(dist[0], SMALL_FASTF) && NEAR_ZERO(dist[1], SMALL_FASTF))
		     != (vu2->v_p == eu->vu_p->v_p)) ||
		    ((NEAR_EQUAL(dist[0], 1.0, SMALL_FASTF) && NEAR_ZERO(dist[1], SMALL_FASTF))
		     != (vu1->v_p == eu->vu_p->v_p))) {
		    bu_bomb("nmg_isect_lseg3_eu(): logic error possibly in 'bn_isect_lseg3_lseg3'\n");
		}

		/* True when either (p1 = q1) or (p2 = q1) or
		 * (p1 = q2) or (p2 = q2) meaning either the start or
		 * end vertex of pot-cut (potential cut between
		 * hole-loopuse and non-hole-loopuse) is the same as
		 * the 1st vertex of the current edgeuse being tested.
		 * When this is true this is ok because we already
		 * know of these intersections, we are testing
		 * everywhere else for intersections.
		 */
		if ((NEAR_ZERO(dist[0], SMALL_FASTF) && NEAR_ZERO(dist[1], SMALL_FASTF)) ||
		    (NEAR_EQUAL(dist[0], 1.0, SMALL_FASTF) && NEAR_ZERO(dist[1], SMALL_FASTF)) ||
		    (NEAR_ZERO(dist[0], SMALL_FASTF) && NEAR_EQUAL(dist[1], 1.0, SMALL_FASTF)) ||
		    (NEAR_EQUAL(dist[0], 1.0, SMALL_FASTF) && NEAR_EQUAL(dist[1], 1.0, SMALL_FASTF))) {
		} else {
		    /* Set hit true so elsewhere logic can skip this
		     * pot-cut and try the next one.
		     */
		    hit = 1;
		}
	    }

	    if (hit) {
		/* Break from eu for-loop since a hit was found and
		 * we need to stop testing the current pot-cut and
		 * start testing the next.
		 */
		break;  /* Break from eu for-loop */
	    }
	} /* End of eu for-loop */

	if (hit) {
	    /* Break from lu for-loop since a hit was found and we
	     * need to stop testing the current pot-cut and start
	     * testing the next.
	     */
	    break;  /* Break from lu for-loop */
	}

    } /* End of lu for-loop */

    return hit;
}


void
nmg_triangulate_rm_holes(struct faceuse *fu, struct bu_list *tbl2d, const struct bn_tol *tol)
{
    vect_t N;
    int hit;
    struct loopuse *lu1 = 0, *lu2, *lu_tmp;
    struct edgeuse *eu1, *eu2, *eu_tmp;
    struct vertexuse *vu1;
    struct pt2d *pt2d_cut_to = (struct pt2d *)NULL;
    struct pt2d *pt2d_cut_from = (struct pt2d *)NULL;
    static const int cut_color[3] = { 90, 255, 90};
    int fast_exit = 0;
    int holes = 0;

    VSETALL(N, 0);

    BN_CK_TOL(tol);
    NMG_CK_FACEUSE(fu);

    fast_exit = 0;
    holes = 0;

    for (BU_LIST_FOR(lu_tmp, loopuse, &fu->lu_hd)) {
	if (lu_tmp->orientation == OT_OPPOSITE) {
	    holes = 1;
	    lu1 = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	    break;
	}
    }

    while (holes) {
	NMG_CK_LOOPUSE(lu1);
	fast_exit = 0;
	if (lu1->orientation == OT_OPPOSITE) {

	    /* Test lu1 to determine if any of the vertex is shared
	     * with a outer loopuse.
	     */

	    /* Loop thru each eu of lu1 hole loopuse. */
	    for (BU_LIST_FOR(eu1, edgeuse, &lu1->down_hd)) {
		for (BU_LIST_FOR(vu1, vertexuse, &eu1->vu_p->v_p->vu_hd)) {
		    /* Ensure same faceuse */
		    if (lu1->up.fu_p == vu1->up.eu_p->up.lu_p->up.fu_p) {
			if (vu1->up.eu_p->up.lu_p->orientation == OT_SAME) {
			    /* Non-hole edgeuse */
			    pt2d_cut_from = find_pt2d(tbl2d, vu1);
			    /* Hole edgeuse */
			    pt2d_cut_to = find_pt2d(tbl2d, eu1->vu_p);

			    if ( !pt2d_cut_from || !pt2d_cut_to ) {
				bu_bomb("nmg_triangulate_rm_holes(): failed to find vu in tbl2d table\n");
			    }

			    NMG_CK_PT2D(pt2d_cut_from);
			    NMG_CK_PT2D(pt2d_cut_to);
			    join_mapped_loops(tbl2d, pt2d_cut_from, pt2d_cut_to, cut_color, tol);
			    fast_exit = 1;
			    break;
			}
		    }
		}
		if (fast_exit) {
		    break;
		}
	    }

	    if (!fast_exit) {
		/* Loop thru each eu of hole loopuse */
		for (BU_LIST_FOR(eu1, edgeuse, &lu1->down_hd)) {

		    /* Loop thru each non-hole loopuse to create pot-cut. */
		    for (BU_LIST_FOR(lu2, loopuse, &fu->lu_hd)) {
			NMG_CK_LOOPUSE(lu2);
			if (lu2->orientation != OT_OPPOSITE) {

			    NMG_CK_EDGEUSE(eu1);
			    /* Loop thru each eu of non-hole loopuse,
			     * vu1 of pot-cut.
			     */
			    for (BU_LIST_FOR(eu2, edgeuse, &lu2->down_hd)) {
				NMG_CK_EDGEUSE(eu2);
				/* The pot-cut should be from non-hole
				 * loopuse to a hole-loopuse it is
				 * assumed that vertices are fused.
				 */

				/* Test if edge is zero length, if so
				 * skip this edge.
				 */
				if (eu2->vu_p->v_p->vg_p->coord == eu1->vu_p->v_p->vg_p->coord) {
				    continue;
				}

				/* eu1 part of hole lu */
				/* eu2 part of non-hole lu */
				hit = nmg_isect_potcut_fu(eu1, eu2, fu, tol);

				if (!hit) {
				    /* perform cut */
				    fast_exit = 1;

				    /* p1, non-hole edgeuse */
				    pt2d_cut_from = find_pt2d(tbl2d, eu2->vu_p);
				    /* p2, hole edgeuse */
				    pt2d_cut_to = find_pt2d(tbl2d, eu1->vu_p);

				    if ( !pt2d_cut_from || !pt2d_cut_to ) {
					bu_bomb("nmg_triangulate_rm_holes(): failed to find vu in tbl2d table\n");
				    }

				    NMG_CK_PT2D(pt2d_cut_from);
				    NMG_CK_PT2D(pt2d_cut_to);
				    join_mapped_loops(tbl2d, pt2d_cut_from, pt2d_cut_to, cut_color, tol);

				    if (fast_exit) {
					break; /* Break from eu2 */
				    }
				}

			    } /* End of for-loop eu2 traversing all
			       * possible pot-cut v1 of current
			       * non-hole-loopuse.
			       */

			} /* End of if-statement making sure lu2 is
			   * NOT a hole-loopuse, makes sure the start vertex
			   * of pot-cut is on a non-hole-loopuse.
			   */

			if (fast_exit) break;

		    } /* End of lu2 for-loop. */

		    if (fast_exit) {
			break;
		    }

		} /* End of eu1 for-loop. */
	    }
	}

	if (fast_exit) {
	    lu1 = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	} else {
	    lu1 = BU_LIST_PNEXT_CIRC(loopuse, lu1);
	}

	holes = 0;
	for (BU_LIST_FOR(lu_tmp, loopuse, &fu->lu_hd)) {
	    if (lu_tmp->orientation == OT_OPPOSITE) {
		holes = 1;
		break;
	    }
	}
    }

    /* When we get to this point, all hole-loopuse should be removed
     * by joining them with a non-hole-loopuse, which converts them to
     * a non-hole-loopuse.
     */

    /* This is at the end of processing of the current hole-loopuse,
     * if a cut was not made for this hole-loopuse then bomb. Maybe
     * enhance process to create a vertex to cut into since could not
     * find an existing vertex.
     */
    if (0) {
	bu_log("nmg_triangulate_rm_holes(): hole-loopuse lu1 0x%lx no cut found\n", (unsigned long)lu1);
	for (BU_LIST_FOR(lu_tmp, loopuse, &fu->lu_hd)) {
	    for (BU_LIST_FOR(eu_tmp, edgeuse, &lu_tmp->down_hd)) {
		bu_log("nmg_triangulate_rm_holes(): lu_p = 0x%lx lu_orient = %d fu_p = 0x%lx vu_p = 0x%lx eu_p = 0x%lx v_p = 0x%lx vg_p = 0x%lx 3D coord = %g %g %g\n",
		       (unsigned long)lu_tmp, lu_tmp->orientation, (unsigned long)lu_tmp->up.fu_p,
		       (unsigned long)eu_tmp->vu_p, (unsigned long)eu_tmp, (unsigned long)eu_tmp->vu_p->v_p,
		       (unsigned long)eu_tmp->vu_p->v_p->vg_p, V3ARGS(eu_tmp->vu_p->v_p->vg_p->coord));

	    }
	}
	bu_bomb("nmg_triangulate_rm_holes(): could not find cut for current hole-loopuse\n");
    }

#ifdef DEBUG_TRI_P
    {
	struct model *my_m2;
	struct edgeuse *myeu2;
	lu_tmp = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	myeu2 = BU_LIST_FIRST(edgeuse, &(lu_tmp->down_hd));
	nmg_plot_lu_around_eu("holes_removed", myeu2, tol);

	my_m2 = nmg_find_model(lu_tmp->up.magic_p);
	NMG_CK_MODEL(my_m2);
	nmg_stash_model_to_file("holes_removed.g", my_m2, "holes_removed");
    }
#endif

    return;
}

/*
 * return 1 when faceuse is empty, otherwise return 0
 *
 */
int
nmg_triangulate_rm_degen_loopuse(struct faceuse *fu, const struct bn_tol *tol)
{
    struct loopuse *lu;
    struct edgeuse *eu;
    int loopuse_count_tmp = 0;
    int edgeuse_vert_count = 0;
    struct loopuse *lu1, *lu_tmp;
    int lu_done = 0;

    size_t *book_keeping_array, *book_keeping_array_tmp;
    int book_keeping_array_alloc_cnt = 40;  /* initial allocated record count */
    int unique_vertex_cnt = 0;
    int idx = 0;
    int done = 0;
    int match = 0;
    int killed_lu = 0;
    int ret = 0;

    BN_CK_TOL(tol);
    NMG_CK_FACEUSE(fu);

    book_keeping_array = (size_t *)bu_calloc(book_keeping_array_alloc_cnt, sizeof(size_t),
					     "book_keeping_array");

    /* remove loopuse with < 3 vertices */
    lu_done = 0;
    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
    while (!lu_done) {

	if (BU_LIST_IS_HEAD(lu, &fu->lu_hd)) {
	    lu_done = 1;
	} else {
	    NMG_CK_LOOPUSE(lu);
	    lu_tmp = lu;
	    killed_lu = 0;
	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		edgeuse_vert_count = 0;
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);
		    edgeuse_vert_count++;
		}
		if (edgeuse_vert_count < 3) {
		    nmg_klu(lu);
		    killed_lu = 1;
		    if (RTG.NMG_debug & DEBUG_TRI) {
			bu_log("nmg_triangulate_rm_degen_loopuse(): faceuse 0x%lx -- killed loopuse 0x%lx with %d vertices\n",
			       (unsigned long)fu, (unsigned long)lu_tmp, edgeuse_vert_count);
		    }
		}
	    } else if ((BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) &&
		       (BU_LIST_FIRST_MAGIC(&lu->lumate_p->down_hd) == NMG_VERTEXUSE_MAGIC)) {
		nmg_kvu(BU_LIST_FIRST(vertexuse, &lu->down_hd));
		nmg_kvu(BU_LIST_FIRST(vertexuse, &lu->lumate_p->down_hd));
		nmg_klu(lu);
		killed_lu = 1;
		if (RTG.NMG_debug & DEBUG_TRI) {
		    bu_log("nmg_triangulate_rm_degen_loopuse(): faceuse 0x%lx -- killed single vertex loopuse 0x%lx\n",
			   (unsigned long)fu, (unsigned long)lu_tmp);
		}
	    } else {
		bu_log("nmg_triangulate_rm_degen_loopuse(): faceuse 0x%lx -- unknown loopuse content\n",
		       (unsigned long)fu);
		bu_bomb("nmg_triangulate_rm_degen_loopuse(): unknown loopuse content\n");
	    }

	    if (killed_lu) {
		loopuse_count_tmp = 0;
		for (BU_LIST_FOR(lu1, loopuse, &fu->lu_hd)) {
		    loopuse_count_tmp++;
		}

		if (RTG.NMG_debug & DEBUG_TRI) {
		    bu_log("nmg_triangulate_rm_degen_loopuse(): faceuse 0x%lx -- %d loopuse remain in faceuse after killing loopuse 0x%lx\n",
			   (unsigned long)fu, loopuse_count_tmp, (unsigned long)lu_tmp);
		}
		if (loopuse_count_tmp < 1) {
		    if (RTG.NMG_debug & DEBUG_TRI) {
			bu_log("nmg_triangulate_rm_degen_loopuse(): faceuse 0x%lx -- contains no loopuse\n",
			       (unsigned long)fu);
		    }
		    ret = 1;
		    goto out;
		}

		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	    } else {
		/* test for at least 3 unique vertices */
		unique_vertex_cnt = 0;
		done = 0;
		eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
		while (!done) {
		    if (BU_LIST_IS_HEAD(eu, &lu->down_hd)) {
			done = 1;
			if (unique_vertex_cnt == 0) {
			    bu_bomb("nmg_triangulate_rm_degen_loopuse(): empty loopuse\n");
			}
		    } else {
			NMG_CK_EDGEUSE(eu);
			if (unique_vertex_cnt == 0) {
			    /* insert first record */
			    book_keeping_array[unique_vertex_cnt] = (size_t)eu->vu_p->v_p->vg_p;
			    unique_vertex_cnt++;
			} else {
			    match = 0;
			    for (idx = 0 ; idx < unique_vertex_cnt ; idx++) {
				if (book_keeping_array[idx] == (size_t)eu->vu_p->v_p->vg_p) {
				    match = 1;
				    {
					struct edgeuse *eu1;
					int cnt = 0;
					for (BU_LIST_FOR(eu1, edgeuse, &lu->down_hd)) {
					    cnt++;
					    bu_log("%d -- vu_p = %p vg_p = %p dup_vu_p = %p dup_vg_p = %p\n",
						   cnt,
						   (void *)eu1->vu_p, (void *)eu1->vu_p->v_p->vg_p,
						   (void *)eu->vu_p, (void *)eu->vu_p->v_p->vg_p);
					}
				    }
				    bu_bomb("nmg_triangulate_rm_degen_loopuse(): found duplicate vertex\n");
				    break;
				}
			    }
			    if (!match) {
				book_keeping_array[unique_vertex_cnt] = (size_t)eu->vu_p->v_p->vg_p;
				unique_vertex_cnt++;
				if (unique_vertex_cnt >= book_keeping_array_alloc_cnt) {
				    book_keeping_array_alloc_cnt = unique_vertex_cnt;
				    book_keeping_array_alloc_cnt += 10;
				    book_keeping_array_tmp = (size_t *)bu_realloc((void *)book_keeping_array,
										  book_keeping_array_alloc_cnt * sizeof(size_t),
										  "book_keeping_array realloc");
				    book_keeping_array = book_keeping_array_tmp;
				}
			    }
			}
		    }
		    eu = BU_LIST_PNEXT(edgeuse, eu);
		} /* end of while-not-done loop */

		if (unique_vertex_cnt < 3) {
		    nmg_klu(lu);
		    if (RTG.NMG_debug & DEBUG_TRI) {
			bu_log("killed loopuse 0x%lx with %d vertices (i.e. < 3 unique vertices)\n",
			       (unsigned long)lu_tmp, edgeuse_vert_count);

		    }
		    loopuse_count_tmp = 0;
		    for (BU_LIST_FOR(lu1, loopuse, &fu->lu_hd)) {
			loopuse_count_tmp++;
		    }

		    if (RTG.NMG_debug & DEBUG_TRI) {
			bu_log("nmg_triangulate_rm_degen_loopuse(): %d remaining loopuse in faceuse after killing loopuse 0x%lx\n", loopuse_count_tmp, (unsigned long)lu_tmp);
		    }
		    if (loopuse_count_tmp < 1) {
			if (RTG.NMG_debug & DEBUG_TRI) {
			    bu_log("nmg_triangulate_rm_degen_loopuse(): faceuse contains no loopuse\n");
			}
			ret = 1;
			goto out;
		    }

		    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);

		} else {
		    lu = BU_LIST_PNEXT(loopuse, lu);
		}
	    }
	}
    }

out:
    bu_free(book_keeping_array, "book_keeping_array");
    return ret;
}


void
nmg_dump_shell(struct shell *s)
{
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct vertex_g *vg;
    FILE *fp;
    size_t fu_cnt = 0;
    size_t lu_cnt = 0;
    size_t eu_cnt = 0;

    if ((fp = fopen("nmg_vertex_dump.txt", "a")) == NULL) {
	bu_bomb("nmg_vertex_dump, open failed\n");
    }

    NMG_CK_SHELL(s);
    fu_cnt = 0;
    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	fu_cnt++;
	NMG_CK_FACEUSE(fu);
	lu_cnt = 0;
	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    lu_cnt++;
	    NMG_CK_LOOPUSE(lu);
	    eu_cnt = 0;
	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		eu_cnt++;
		NMG_CK_EDGEUSE(eu);
		NMG_CK_VERTEXUSE(eu->vu_p);
		NMG_CK_VERTEX(eu->vu_p->v_p);
		NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
		vg = eu->vu_p->v_p->vg_p;
		fprintf(fp, "%ld %ld %ld %g %g %g\n",
			(unsigned long)fu_cnt,
			(unsigned long)lu_cnt,
			(unsigned long)eu_cnt,
			V3ARGS(vg->coord));
	    }
	}
    }

    fclose(fp);
    return;
}


HIDDEN void
nmg_tri_kill_accordions(struct loopuse *lu, struct bu_list *tbl2d)
{
    struct edgeuse *eu_curr, *eu_prev, *eu_next;
    struct pt2d *tmp = (struct pt2d *)NULL;
    int vert_cnt = 0;

    NMG_CK_LOOPUSE(lu);

    eu_curr = eu_prev = eu_next = (struct edgeuse *)NULL;

    if (tbl2d) {
	NMG_CK_TBL2D(tbl2d);
    }

    if (BU_LIST_IS_EMPTY(&lu->down_hd)) {
	return;
    }

    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
	return;
    }

    for (BU_LIST_FOR(eu_curr, edgeuse, &lu->down_hd)) {
	vert_cnt++;
    }

    eu_curr = BU_LIST_FIRST(edgeuse, &lu->down_hd);
    while (BU_LIST_NOT_HEAD(&eu_curr->l, &lu->down_hd) && vert_cnt > 2) {

	eu_prev = BU_LIST_PPREV_CIRC(edgeuse, &eu_curr->l);
	eu_next = BU_LIST_PNEXT_CIRC(edgeuse, &eu_curr->l);

	if ((eu_prev->vu_p->v_p == eu_next->vu_p->v_p) && (eu_curr != eu_prev)) {
	    if (eu_prev != eu_next) {
		if ((RTG.NMG_debug & DEBUG_BASIC) || (RTG.NMG_debug & DEBUG_CUTLOOP)) {
		    bu_log("nmg_tri_kill_accordions(): killing jaunt in accordion eu's %p and %p\n",
			   (void *)eu_curr, (void *)eu_prev);
		}
		if (tbl2d) {
		    if ((tmp = find_pt2d(tbl2d, eu_curr->vu_p))) {
			tmp->vu_p = (struct vertexuse *)NULL;
		    } else {
			bu_bomb("nmg_tri_kill_accordions(): could not find eu_curr->vu_p in tbl2d table (1)\n");
		    }
		    if ((tmp = find_pt2d(tbl2d, eu_prev->vu_p))) {
			tmp->vu_p = (struct vertexuse *)NULL;
		    } else {
			bu_bomb("nmg_tri_kill_accordions(): could not find eu_prev->vu_p in tbl2d table\n");
		    }
		}
		(void)nmg_keu(eu_curr);
		(void)nmg_keu(eu_prev);
		vert_cnt -= 2;
	    } else {
		if ((RTG.NMG_debug & DEBUG_BASIC) || (RTG.NMG_debug & DEBUG_CUTLOOP)) {
		    bu_log("nmg_tri_kill_accordions(): killing jaunt in accordion eu %p\n", (void *)eu_curr);
		}
		if (tbl2d) {
		    if ((tmp = find_pt2d(tbl2d, eu_curr->vu_p))) {
			tmp->vu_p = (struct vertexuse *)NULL;
		    } else {
			bu_bomb("nmg_tri_kill_accordions(): could not find eu_curr->vu_p in tbl2d table (2)\n");
		    }
		}
		(void)nmg_keu(eu_curr);
		vert_cnt--;
	    }
	    eu_curr = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	} else {
	    eu_curr = BU_LIST_PNEXT(edgeuse, eu_curr);
	}
    }
}


HIDDEN void
validate_tbl2d(const char *str, struct bu_list *tbl2d, struct faceuse *fu)
{
    struct loopuse *lu = (struct loopuse *)NULL;
    struct edgeuse *eu = (struct edgeuse *)NULL;
    struct pt2d *pt = (struct pt2d *)NULL;
    int error_cnt = 0;

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
	    continue;
	}

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    NMG_CK_EDGEUSE(eu);
	    if (!(pt = find_pt2d(tbl2d, eu->vu_p))) {
		error_cnt++;
		bu_log("validate_tbl2d(): %d: fu = %p fu_orient = %d lu = %p lu_orient = %d missing vu = %p coord = %f %f %f \n",
		       error_cnt,
		       (void *)fu, fu->orientation,
		       (void *)lu, lu->orientation,
		       (void *)eu->vu_p, V3ARGS(eu->vu_p->v_p->vg_p->coord));
	    } else {
		NMG_CK_VERTEXUSE(pt->vu_p);
		NMG_CK_VERTEX(pt->vu_p->v_p);
		NMG_CK_VERTEX_G(pt->vu_p->v_p->vg_p);
		NMG_CK_EDGEUSE(pt->vu_p->up.eu_p);
		NMG_CK_LOOPUSE(pt->vu_p->up.eu_p->up.lu_p);
		NMG_CK_FACEUSE(pt->vu_p->up.eu_p->up.lu_p->up.fu_p);
	    }
	}
    }
    if (error_cnt != 0) {
	bu_log("validate_tbl2d(): %s\n", str);
	bu_bomb("validate_tbl2d(): missing entries in tbl2d table\n");
    }
}


/**
 * Given a unimonotone loopuse, triangulate it into multiple loopuses
 */
HIDDEN void
cut_unimonotone(struct bu_list *tbl2d, struct loopuse *lu, const struct bn_tol *tol)
{
    int verts = 0;
    int excess_loop_count = 0;
    int loop_count = 0;
    int status;
    int ccw_result;
    int tmp_vert_cnt = 0;

    /* boolean variables */
    int inside_triangle = 0;
    int isect_vertex = 0;

    vect_t fu_normal;
    vect_t v0, v1, v2;
    fastf_t dot00, dot01, dot02, dot11, dot12;
    fastf_t invDenom, u, v;
    fastf_t dist;

    struct pt2d *min, *max, *newpt, *first, *prev, *next, *current, *tmp;
    struct pt2d *prev_orig, *next_orig, *pt, *t;

    struct shell *s;
    struct faceuse *fu;
    struct loopuse *lu2, *orig_lu_p;
    struct edgeuse *eu;
    struct vertex_g *prev_vg_p = NULL;

    static const int cut_color[3] = { 90, 255, 90};

    if (RTG.NMG_debug & DEBUG_TRI) {
	bu_log("cutting unimonotone:\n");
    }

    NMG_CK_TBL2D(tbl2d);
    BN_CK_TOL(tol);
    NMG_CK_LOOPUSE(lu);

    min = max = newpt = first = prev = next = current = (struct pt2d *)NULL;
    prev_orig = next_orig = pt = t = tmp = (struct pt2d *)NULL;

    orig_lu_p = lu;

    if (RTG.NMG_debug & DEBUG_TRI) {
	validate_tbl2d("start of function cut_unimonotone()", tbl2d, lu->up.fu_p);
    }

    /* find min/max points & count vertex points */
    verts = 0;
    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	NMG_CK_EDGEUSE(eu);
	newpt = find_pt2d(tbl2d, eu->vu_p);
	if (!newpt) {
	    bu_log("cut_unimonotone(): can not find a 2D point for %g %g %g\n",
		   V3ARGS(eu->vu_p->v_p->vg_p->coord));
	    bu_bomb("cut_unimonotone(): can not find a 2D point\n");
	}

	if (RTG.NMG_debug & DEBUG_TRI) {
	    bu_log("%g %g\n", newpt->coord[X], newpt->coord[Y]);
	}

	if (!min || P_LT_V(newpt, min)) {
	    min = newpt;
	}
	if (!max || P_GT_V(newpt, max)) {
	    max = newpt;
	}
	verts++;
    }

    first = max;

    if (RTG.NMG_debug & DEBUG_TRI) {
	bu_log("cut_unimonotone(): %d verts, min: %g %g  max: %g %g first:%g %g %p\n", verts,
	       min->coord[X], min->coord[Y], max->coord[X], max->coord[Y],
	       first->coord[X], first->coord[Y], (void *)first);
    }

    excess_loop_count = verts * verts;
    current = PT2D_NEXT(tbl2d, first);
    while (verts > 3) {
	loop_count++;
	if (loop_count > excess_loop_count) {
	    if (RTG.NMG_debug & DEBUG_TRI) {
		eu = BU_LIST_FIRST(edgeuse, &(current->vu_p->up.eu_p->up.lu_p->down_hd));
		nmg_plot_lu_around_eu("cut_unimonotone_infinite_loopuse", eu, tol);
		s = nmg_find_shell(current->vu_p->up.eu_p->up.lu_p->up.magic_p);
		nmg_stash_shell_to_file("cut_unimonotone_infinite_model.g", s, "cut_unimonotone_infinite_model");
		nmg_pr_lu(current->vu_p->up.eu_p->up.lu_p, "cut_unimonotone_loopuse");
		nmg_plot_fu("cut_unimonotone_infinite_loopuse", current->vu_p->up.eu_p->up.lu_p->up.fu_p, tol);
	    }
	    bu_log("cut_unimonotone(): infinite loop %p\n", (void *)current->vu_p->up.eu_p->up.lu_p);
	    bu_bomb("cut_unimonotone(): infinite loop\n");
	}

	prev = PT2D_PREV(tbl2d, current);
	next = PT2D_NEXT(tbl2d, current);

	VSETALL(v0, 0.0);
	VSETALL(v1, 0.0);
	VSETALL(v2, 0.0);
	dot00 = dot01 = dot02 = dot11 = dot12 = 0.0;
	invDenom = u = v = 0.0;
	prev_vg_p = (struct vertex_g *)NULL;

	/* test if any of the loopuse vertices are within the triangle
	 * to be created. it is assumed none of the loopuse within the
	 * faceuse intersects any of the other loopuse with the exception
	 * of having a common edge or vertex
	 */
	inside_triangle = 0;
	for (BU_LIST_FOR(pt, pt2d, tbl2d)) {

	    if (inside_triangle) {
		break;
	    }

	    if (pt->vu_p == (struct vertexuse *)NULL) {
		/* skip tbl2d entries with a null vertexuse */
		continue;
	    }

	    if (pt->vu_p->up.eu_p == (struct edgeuse *)NULL) {
		/* skip tbl2d entries with a null edgeuse */
		continue;
	    }

	    NMG_CK_EDGEUSE(pt->vu_p->up.eu_p);
	    NMG_CK_LOOPUSE(pt->vu_p->up.eu_p->up.lu_p);

	    if (pt->vu_p->up.eu_p->up.lu_p == current->vu_p->up.eu_p->up.lu_p) {
		/* true when the vertexuse to be tested is in the
		 * same loopuse as the potential cut
		 */

		/* skips processing the same vertex */
		if (prev_vg_p != pt->vu_p->v_p->vg_p) {
		    VSUB2(v0, next->coord, prev->coord);
		    VSUB2(v1, current->coord, prev->coord);
		    VSUB2(v2, pt->coord, prev->coord);
		    dot00 = VDOT(v0, v0);
		    dot01 = VDOT(v0, v1);
		    dot02 = VDOT(v0, v2);
		    dot11 = VDOT(v1, v1);
		    dot12 = VDOT(v1, v2);
		    invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
		    u = (dot11 * dot02 - dot01 * dot12) * invDenom;
		    v = (dot00 * dot12 - dot01 * dot02) * invDenom;

		    if ((u > SMALL_FASTF) && (v > SMALL_FASTF) && ((u + v) < (1.0 - SMALL_FASTF))) {
			/* true if point inside triangle */
			if (RTG.NMG_debug & DEBUG_TRI) {
			    bu_log("cut_unimonotone(): point inside triangle, lu_p = %p, point = %g %g %g\n",
				   (void *)lu, V3ARGS(pt->vu_p->v_p->vg_p->coord));
			}
			inside_triangle = 1;
		    } else {
			if (RTG.NMG_debug & DEBUG_TRI) {
			    bu_log("cut_unimonotone(): outside triangle, lu_p = %p, 3Dprev = %g %g %g 3Dcurr = %g %g %g 3Dnext = %g %g %g 3Dpoint = %g %g %g is_convex = %d\n",
				   (void *)lu, V3ARGS(prev->vu_p->v_p->vg_p->coord),
				   V3ARGS(current->vu_p->v_p->vg_p->coord),
				   V3ARGS(next->vu_p->v_p->vg_p->coord),
				   V3ARGS(pt->vu_p->v_p->vg_p->coord),
				   is_convex(prev, current, next, tol));
			}
		    }
		}
		prev_vg_p = pt->vu_p->v_p->vg_p;
	    } /* end of if-statement to skip testing vertices not in the current loopuse */
	}

	/* test if the potential cut would intersect a vertex which
	 * would not belong to the resulting triangle to be created
	 * by the cut
	 */
	isect_vertex = 0;
	for (BU_LIST_FOR(eu, edgeuse, &next->vu_p->up.eu_p->up.lu_p->down_hd)) {

	    if (isect_vertex) {
		break;
	    }

	    if ((prev->vu_p->v_p != eu->vu_p->v_p) &&
		(current->vu_p->v_p != eu->vu_p->v_p) &&
		(next->vu_p->v_p != eu->vu_p->v_p)) {
		/* true when the vertex tested would not belong to
		 * the resulting triangle
		 */
		status = bn_isect_pt_lseg(&dist, prev->vu_p->v_p->vg_p->coord,
					  next->vu_p->v_p->vg_p->coord,
					  eu->vu_p->v_p->vg_p->coord, tol);
		if (status == 3) {
		    /* true when the potential cut intersects a vertex */
		    isect_vertex = 1;
		    if (RTG.NMG_debug & DEBUG_TRI) {
			bu_log("cut_unimonotone(): cut prev %g %g %g -> next %g %g %g point = %g %g %g\n",
			       V3ARGS(prev->vu_p->v_p->vg_p->coord),
			       V3ARGS(next->vu_p->v_p->vg_p->coord),
			       V3ARGS(eu->vu_p->v_p->vg_p->coord));
			bu_log("cut_unimonotone(): cut and vertex intersect\n");
		    }
		}
	    }
	}

	if (is_convex(prev, current, next, tol) && !inside_triangle && !isect_vertex) {
	    /* true if the angle is convex and there are no vertices
	     * from this loopuse within the triangle to be created
	     * and the potential cut does not intersect any vertices,
	     * from this loopuse, which are not part of the triangle
	     * to be created. convex here is between 0 and 180 degrees
	     * including 0 and 180.
	     */
	    if (RTG.NMG_debug & DEBUG_TRI) {
		bu_log("cut_unimonotone(): before cut loop:\n");
		nmg_pr_fu_briefly(lu->up.fu_p, "");
	    }

	    if (RTG.NMG_debug & DEBUG_TRI) {
		bu_log("cut_unimonotone(): before cut orig_lu_p = %p prev lu_p = %p 3D %g %g %g curr lu_p = %p 3D %g %g %g next lu_p = %p 3D %g %g %g\n",
		       (void *)orig_lu_p, (void *)prev->vu_p->up.eu_p->up.lu_p,
		       V3ARGS(prev->vu_p->v_p->vg_p->coord),
		       (void *)current->vu_p->up.eu_p->up.lu_p,
		       V3ARGS(current->vu_p->v_p->vg_p->coord),
		       (void *)next->vu_p->up.eu_p->up.lu_p,
		       V3ARGS(next->vu_p->v_p->vg_p->coord));
	    }


	    if (next->vu_p == prev->vu_p) {
		bu_bomb("cut_unimonotone(): trying to cut to/from same vertexuse\n");
	    }

	    if (RTG.NMG_debug & DEBUG_TRI) {
		validate_tbl2d("before cut_unimonotone -- cut_mapped_loop",
			       tbl2d, current->vu_p->up.eu_p->up.lu_p->up.fu_p);
	    }

	    prev_orig = prev;
	    next_orig = next;

	    /* cut a triangular piece off of the loop to create a new loop */
	    current = cut_mapped_loop(tbl2d, next, prev, cut_color, tol, 0);

	    prev = prev_orig;
	    next = next_orig;

	    if (!current) {
		bu_bomb("cut_unimonotone(): function cut_mapped_loop returned null\n");
	    }
	    if (current == next) {
		bu_bomb("cut_unimonotone(): current == next\n");
	    }

	    if (RTG.NMG_debug & DEBUG_TRI) {
		validate_tbl2d("after cut_unimonotone -- cut_mapped_loop",
			       tbl2d, current->vu_p->up.eu_p->up.lu_p->up.fu_p);
	    }

	    if (BU_LIST_FIRST_MAGIC(&next->vu_p->up.eu_p->up.lu_p->down_hd) != NMG_EDGEUSE_MAGIC) {
		bu_bomb("cut_unimonotone(): next loopuse contains no edgeuse\n");
	    }

	    /* check direction (cw/ccw) of loopuse after cut */
	    NMG_GET_FU_NORMAL(fu_normal, next->vu_p->up.eu_p->up.lu_p->up.fu_p);
	    ccw_result = nmg_loop_is_ccw(next->vu_p->up.eu_p->up.lu_p, fu_normal, tol);

	    if (ccw_result == 0) {
		/* true if could not determine direction */

		/* need to save the lu pointer since the vu pointer
		 * may become invalid after nmg_tri_kill_accordions
		 * removes the accordions
		 */
		lu2 = next->vu_p->up.eu_p->up.lu_p;

		if (RTG.NMG_debug & DEBUG_TRI) {
		    validate_tbl2d("cut_unimonotone() before nmg_tri_kill_accordions",
				   tbl2d, lu2->up.fu_p);
		}

		nmg_tri_kill_accordions(lu2, tbl2d);

		if (BU_LIST_FIRST_MAGIC(&lu2->down_hd) != NMG_EDGEUSE_MAGIC) {
		    bu_bomb("cut_unimonotone(): after nmg_tri_kill_accordions, loopuse has no edgeuse\n");
		}

		if (RTG.NMG_debug & DEBUG_TRI) {
		    validate_tbl2d("cut_unimonotone() after nmg_tri_kill_accordions",
				   tbl2d, lu2->up.fu_p);
		}

		NMG_CK_LOOPUSE(lu2);
		/* find the pt2d table entry for the first vu within
		 * the resulting lu
		 */
		eu = BU_LIST_FIRST(edgeuse, &lu2->down_hd);
		if (!(next = find_pt2d(tbl2d, eu->vu_p))) {
		    bu_bomb("cut_unimonotone(): unable to find next vertexuse\n");
		}

		/* count the number of vertexuse in the 'next' loopuse */
		for (BU_LIST_FOR(eu, edgeuse, &next->vu_p->up.eu_p->up.lu_p->down_hd)) {
		    tmp_vert_cnt++;
		}

		/* set the vertexuse counter (i.e. verts) to the current
		 * number of vertexuse in the loopuse. this is needed
		 * since nmg_tri_kill_accordions, if it kills any
		 * accordions in the loopuse, will reduce the number of
		 * vertexuse in the loopuse
		 */
		verts = tmp_vert_cnt;

		if (verts > 3) {
		    bu_bomb("cut_unimonotone(): can not determine loopuse cw/ccw and loopuse contains > 3 vertices\n");
		}

	    } else if (ccw_result == -1) {
		/* true if the loopuse has a cw rotation */
		bu_log("cut_unimonotone(): after cut_mapped_loop, next loopuse was cw.\n");

		if (RTG.NMG_debug & DEBUG_TRI) {
		    validate_tbl2d("cut_unimonotone() before flip direction",
				   tbl2d, next->vu_p->up.eu_p->up.lu_p->up.fu_p);
		}
		fu = next->vu_p->up.eu_p->up.lu_p->up.fu_p;
		lu2 = next->vu_p->up.eu_p->up.lu_p;

		NMG_CK_LOOPUSE(lu2);
		NMG_CK_FACEUSE(fu);
		if (*lu2->up.magic_p == NMG_FACEUSE_MAGIC) {
		    /* loop is in wrong direction, exchange lu and lu_mate */
		    BU_LIST_DEQUEUE(&lu2->l);
		    BU_LIST_DEQUEUE(&lu2->lumate_p->l);
		    BU_LIST_APPEND(&fu->lu_hd, &lu2->lumate_p->l);
		    lu2->lumate_p->up.fu_p = fu;
		    BU_LIST_APPEND(&fu->fumate_p->lu_hd, &lu2->l);
		    lu2->up.fu_p = fu->fumate_p;
		} else if (*lu2->up.magic_p == NMG_SHELL_MAGIC) {
		    bu_bomb("cut_unimonotone(): loopuse parent is shell\n");
		} else {
		    bu_bomb("cut_unimonotone(): loopuse parent is unknown\n");
		}

		/* add the new vertexuse to the pt2d table. these new
		 * vertexuse are from loopuse-mate before the switch.
		 */
		lu2 = BU_LIST_FIRST(loopuse, &fu->lu_hd);

		for (BU_LIST_FOR(eu, edgeuse, &lu2->down_hd)) {
		    NMG_CK_EDGEUSE(eu);
		    map_new_vertexuse(tbl2d, eu->vu_p);
		    if (RTG.NMG_debug & DEBUG_TRI) {
			tmp = find_pt2d(tbl2d, eu->vu_p);
			if (!(tmp)) {
			    bu_bomb("cut_unimonotone(): vertexuse not added to tbl2d table\n");
			}
		    }
		}

		eu = BU_LIST_FIRST(edgeuse, &lu2->down_hd);
		if (!(next = find_pt2d(tbl2d, eu->vu_p))) {
		    bu_bomb("cut_unimonotone(): unable to find next vertexuse in tbl2d table\n");
		}

		/* make sure loopuse orientation is marked OT_SAME */
		nmg_set_lu_orientation(lu2, 0);

		current = PT2D_PREV(tbl2d, next);
		prev = PT2D_PREV(tbl2d, current);

		orig_lu_p = next->vu_p->up.eu_p->up.lu_p;

		if (RTG.NMG_debug & DEBUG_TRI) {
		    validate_tbl2d("cut_unimonotone() after flip direction",
				   tbl2d, next->vu_p->up.eu_p->up.lu_p->up.fu_p);
		}
	    } else if (ccw_result == 1) {
		/* make sure loopuse orientation is marked OT_SAME */
		nmg_set_lu_orientation(next->vu_p->up.eu_p->up.lu_p, 0);
	    } else {
		bu_bomb("cut_unimonotone(): function 'nmg_loop_is_ccw' returned an invalid result\n");
	    }

	    if (RTG.NMG_debug & DEBUG_TRI) {
		bu_log("cut_unimonotone(): after cut orig_lu_p = %p lu_p = %p 3D %g %g %g --> lu_p = %p 3D %g %g %g\n",
		       (void *)orig_lu_p, (void *)prev->vu_p->up.eu_p->up.lu_p,
		       V3ARGS(prev->vu_p->v_p->vg_p->coord),
		       (void *)next->vu_p->up.eu_p->up.lu_p,
		       V3ARGS(next->vu_p->v_p->vg_p->coord));
	    }

	    if (orig_lu_p != next->vu_p->up.eu_p->up.lu_p) {
		bu_bomb("cut_unimonotone(): next loopuse to cut is not the original loopuse\n");
	    }

	    if (RTG.NMG_debug & DEBUG_TRI) {
		bu_log("cut_unimonotone(): after cut loop:\n");
		nmg_pr_fu_briefly(lu->up.fu_p, "");
	    }
	    if (next->vu_p->v_p == prev->vu_p->v_p) {
		/* the vert count must be decremented 2 when
		 * the cut was between the same vertex
		 */
		verts -= 2;
	    } else {
		verts--;
	    }

	    NMG_CK_LOOPUSE(lu);

	    if (RTG.NMG_debug & DEBUG_TRI) {
		nmg_tri_plfu(lu->up.fu_p, tbl2d);
	    }

	    if (current->vu_p == first->vu_p) {
		t = PT2D_NEXT(tbl2d, first);
		if (RTG.NMG_debug & DEBUG_TRI) {
		    bu_log("cut_unimonotone(): first(%p -> %g %g\n",
			   (void *)first, t->coord[X], t->coord[Y]);
		}

		t = PT2D_NEXT(tbl2d, current);
		if (RTG.NMG_debug & DEBUG_TRI) {
		    bu_log("cut_unimonotone(): current(%p) -> %g %g\n",
			   (void *)current, t->coord[X], t->coord[Y]);
		}

		current = PT2D_NEXT(tbl2d, current);
		if (RTG.NMG_debug & DEBUG_TRI) {
		    bu_log("cut_unimonotone(): current(%p) -> %g %g\n",
			   (void *)current, t->coord[X], t->coord[Y]);
		}
	    }
	} else {
	    if (RTG.NMG_debug & DEBUG_TRI) {
		bu_log("cut_unimonotone(): not-cut orig_lu_p = %p lu_p = %p 3D %g %g %g --> lu_p = %p 3D %g %g %g\n",
		       (void *)orig_lu_p, (void *)prev->vu_p->up.eu_p->up.lu_p,
		       V3ARGS(prev->vu_p->v_p->vg_p->coord),
		       (void *)next->vu_p->up.eu_p->up.lu_p,
		       V3ARGS(next->vu_p->v_p->vg_p->coord));
	    }

	    if (RTG.NMG_debug & DEBUG_TRI) {
		bu_log("cut_unimonotone(): cut not performed, moving to next potential cut\n");
	    }
	    current = next;
	}
    }
}


void
print_loopuse_tree(struct bu_list *head, struct loopuse_tree_node *parent, const struct bn_tol *tol)
{
    struct loopuse_tree_node *node, *node_first;
    struct bu_vls plot_file_desc = BU_VLS_INIT_ZERO;

    if (head->magic != BU_LIST_HEAD_MAGIC) {
	bu_bomb("print_loopuse_tree(): head not bu_list head\n");
    }

    if (BU_LIST_IS_EMPTY(head)) {
	bu_bomb("print_loopuse_tree(): empty bu_list\n");
    }

    node_first = BU_LIST_FIRST(loopuse_tree_node, head);
    NMG_CK_LOOPUSE(node_first->lu);

    bu_vls_sprintf(&plot_file_desc, "parent_%p_", (void *)parent);
    nmg_plot_fu(bu_vls_addr(&plot_file_desc), node_first->lu->up.fu_p, tol);
    bu_vls_free(&plot_file_desc);

    for (BU_LIST_FOR(node, loopuse_tree_node, head)) {
	bu_log("print_loopuse_tree() parent ptr %p head ptr = %p siblings node %p lu %p %d child_hd ptr %p\n",
	       (void *)parent, (void *)head, (void *)node, (void *)node->lu, node->lu->orientation,
	       (void *)&(node->children_hd));
	if (BU_LIST_NON_EMPTY(&(node->children_hd))) {
	    print_loopuse_tree(&(node->children_hd), node, tol);
	}
    }
}

int
nmg_classify_pt_loop_new(const struct vertex *line1_pt1_v_ptr, const struct loopuse *lu, const struct bn_tol *tol)
{
    struct edgeuse *eu1, *eu2;
    int status;
    int hit_cnt = 0;
    int in_cnt = 0;
    int out_cnt = 0;
    int on_cnt = 0;
    int done = 0;
    double angle1 = 0;
    plane_t N = HINIT_ZERO;
    vect_t  x_dir = VINIT_ZERO;
    vect_t  y_dir = VINIT_ZERO;
    vect_t  vec1 = VINIT_ZERO;
    vect_t  vec2 = VINIT_ZERO;
    vect_t  line1_dir = VINIT_ZERO;
    vect_t  line2_dir = VINIT_ZERO;
    fastf_t *line1_pt1, *line1_pt2, *line2_pt1, *line2_pt2;
    fastf_t line1_dist = 0.0;
    fastf_t line2_dist = 0.0;
    fastf_t vec2_mag = 0.0;

    vect_t  min_pt = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
    vect_t  max_pt = {-MAX_FASTF, -MAX_FASTF, -MAX_FASTF};

    bu_log("\nnmg_classify_pt_loop_new(): START ==========================================\n\n");

    line1_pt1 = line1_pt1_v_ptr->vg_p->coord;
    NMG_CK_LOOPUSE(lu);

    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
	bu_bomb("nmg_classify_pt_loop_new(): loopuse contains no edgeuse\n");
    }


    for (BU_LIST_FOR(eu1, edgeuse, &lu->down_hd)) {
	NMG_CK_EDGEUSE(eu1);
	if (eu1->vu_p->v_p->vg_p->coord[X] > max_pt[X]) {
	    max_pt[X] = eu1->vu_p->v_p->vg_p->coord[X];
	}
	if (eu1->vu_p->v_p->vg_p->coord[Y] > max_pt[Y]) {
	    max_pt[Y] = eu1->vu_p->v_p->vg_p->coord[Y];
	}
	if (eu1->vu_p->v_p->vg_p->coord[Z] > max_pt[Z]) {
	    max_pt[Z] = eu1->vu_p->v_p->vg_p->coord[Z];
	}
	if (eu1->vu_p->v_p->vg_p->coord[X] < min_pt[X]) {
	    min_pt[X] = eu1->vu_p->v_p->vg_p->coord[X];
	}
	if (eu1->vu_p->v_p->vg_p->coord[Y] < min_pt[Y]) {
	    min_pt[Y] = eu1->vu_p->v_p->vg_p->coord[Y];
	}
	if (eu1->vu_p->v_p->vg_p->coord[Z] < min_pt[Z]) {
	    min_pt[Z] = eu1->vu_p->v_p->vg_p->coord[Z];
	}
    }

    if (V3PT_OUT_RPP_TOL(line1_pt1, min_pt, max_pt, tol->dist)) {
	/* True when the point is outside the loopuse bounding box.
	 * Considering distance tolerance, the point is also not on
	 * the bounding box and therefore the point can not be on the
	 * loopuse.
	 */
	bu_log("pt = %g %g %g min_pt = %g %g %g max_pt = %g %g %g\n",
	       V3ARGS(line1_pt1), V3ARGS(min_pt), V3ARGS(max_pt));
	bu_log("nmg_classify_pt_loop_new(): pt outside loopuse bb\n");
	bu_log("\nnmg_classify_pt_loop_new(): END ==========================================\n\n");
	return NMG_CLASS_AoutB;
    }

    for (BU_LIST_FOR(eu1, edgeuse, &lu->down_hd)) {
	NMG_CK_EDGEUSE(eu1);
	if (eu1->vu_p->v_p == line1_pt1_v_ptr) {
	    bu_log("nmg_classify_pt_loop_new(): pt %g %g %g is same as a loopuse vertex\n",
		   V3ARGS(eu1->vu_p->v_p->vg_p->coord));
	    bu_log("\nnmg_classify_pt_loop_new(): END ==========================================\n\n");
	    return NMG_CLASS_AonBshared;
	} else {
	    if (bn_pt3_pt3_equal(eu1->vu_p->v_p->vg_p->coord, line1_pt1_v_ptr->vg_p->coord, tol)) {
		bu_bomb("nmg_classify_pt_loop_new(): found unfused vertex\n");
	    }
	}
    }

    NMG_GET_FU_NORMAL(N, lu->up.fu_p);

    for (BU_LIST_FOR(eu1, edgeuse, &lu->down_hd)) {
	NMG_CK_EDGEUSE(eu1);
	if (eu1->eumate_p->vu_p->v_p->vg_p == eu1->vu_p->v_p->vg_p) {
	    bu_bomb("nmg_classify_pt_loop_new(): zero length edge\n");
	} else if (bn_pt3_pt3_equal(eu1->eumate_p->vu_p->v_p->vg_p->coord, eu1->vu_p->v_p->vg_p->coord, tol)) {
	    bu_bomb("nmg_classify_pt_loop_new(): found unfused vertex, zero length edge\n");
	}

	line1_pt2 = eu1->vu_p->v_p->vg_p->coord;
	VSUB2(line1_dir, line1_pt2, line1_pt1);

	hit_cnt = 0;
	done = 0;
	eu2 = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	while (!done) {
	    if (BU_LIST_IS_HEAD(eu2, &lu->down_hd)) {
		done = 1;
	    } else {
		line2_pt1 = eu2->vu_p->v_p->vg_p->coord;
		line2_pt2 = eu2->eumate_p->vu_p->v_p->vg_p->coord;
		VSUB2(line2_dir, line2_pt2, line2_pt1);

		status = bn_isect_line3_line3(&line1_dist, &line2_dist,
					      line1_pt1, line1_dir, line2_pt1, line2_dir, tol);

		if ( status == 1 ) {
		    int on_vertex = 0;
		    VSUB2(vec2, line2_pt2, line2_pt1);
		    vec2_mag = MAGNITUDE(vec2);
		    if ((line1_dist > -(tol->dist)) && (line2_dist > -(tol->dist)) &&
			(line2_dist < (vec2_mag + tol->dist))) {
			/* true when intercept is on the ray and on the edgeuse */

			if (NEAR_ZERO(line1_dist, tol->dist)) {
			    /* true when start point of ray is within distance tolerance of edgeuse */
			    bu_log("normal intercept, pt on loopuse but not vertexuse, lu_ptr = %p pt = %g %g %g line1_dist = %g line2_dist = %g edgeuse pt1 %g %g %g pt2 %g %g %g\n",
				   (void *)lu, V3ARGS(line1_pt1), line1_dist, line2_dist, V3ARGS(line2_pt1), V3ARGS(line2_pt2));
			    bu_bomb("normal intercept, pt on loopuse but not vertexuse\n");
			    return NMG_CLASS_AonBshared;
			}

			if (NEAR_ZERO(line2_dist, tol->dist)) {
			    /* true when hit is on 1st vertex of edgeuse */
			    VSUB2(x_dir, line2_pt1, line1_pt1);
			    VCROSS(y_dir, N, x_dir);
			    VSUB2(vec1, line2_pt2, line1_pt1);
			    angle1 = bn_angle_measure(vec1, x_dir, y_dir);
			    on_vertex = 1;
			}

			if (NEAR_ZERO(line2_dist - vec2_mag, tol->dist)) {
			    /* true when hit is on 2st vertex of edgeuse */
			    VSUB2(x_dir, line2_pt2, line1_pt1);
			    VCROSS(y_dir, N, x_dir);
			    VSUB2(vec1, line2_pt1, line1_pt1);
			    angle1 = bn_angle_measure(vec1, x_dir, y_dir);
			    on_vertex = 1;
			}

			if (on_vertex) {
			    if (angle1 > M_PI) {
				/* count hit only when non-hit vertex of edgeuse lies below ray */
				bu_log("hit-on-edgeuse-vertex ... ray = %g %g %g -> %g %g %g edge = %g %g %g -> %g %g %g edge lu_ptr = %p ang1 = %g vec1 = %g %g %g N = %g %g %g %g, x_dir = %g %g %g y_dir = %g %g %g\n",
				       V3ARGS(line1_pt1), V3ARGS(line1_pt2), V3ARGS(line2_pt1),
				       V3ARGS(line2_pt2), (void *)lu, angle1, V3ARGS(vec1), V4ARGS(N),
				       V3ARGS(x_dir), V3ARGS(y_dir));
				hit_cnt++;
			    }
			} else {
			    bu_log("hit-on-edgeuse-non-vertex ray = %g %g %g -> %g %g %g edge = %g %g %g -> %g %g %g edge lu_ptr = %p N = %g %g %g %g\n",
				   V3ARGS(line1_pt1), V3ARGS(line1_pt2), V3ARGS(line2_pt1),
				   V3ARGS(line2_pt2), (void *)lu, V4ARGS(N));
			    hit_cnt++;
			}
		    }
		    eu2 = BU_LIST_PNEXT(edgeuse, eu2);
		}

		if (status == 0) {
		    bu_log("nmg_classify_pt_loop_new(): co-linear, ray = %g %g %g -> %g %g %g edge = %g %g %g -> %g %g %g lu_ptr = %p line1_dist = %g line2_dist = %g\n",
			   V3ARGS(line1_pt1), V3ARGS(line1_pt2), V3ARGS(line2_pt1),
			   V3ARGS(line2_pt2), (void *)lu, line1_dist, line2_dist);
		    VSUB2(line1_dir, line1_pt2, line1_pt1);
		    VSUB2(line2_dir, line2_pt2, line2_pt1);

		    /* test if ray start point is on edgeuse */
		    if (((line1_dist > SMALL_FASTF) && (line2_dist < -SMALL_FASTF)) ||
			((line1_dist < -SMALL_FASTF) && (line2_dist > SMALL_FASTF))) {
			/* true when the sign of the two distances are opposite */
			/* if the point was on one of the loopuse vertices, it would
			 * have been determined in the earlier logic. no need to
			 * consider any of the in/out results because they are
			 * inconclusive if the point is on the edgeuse.
			 */

			bu_log("co-linear, pt on loopuse but not vertexuse, lu_ptr = %p pt = %g %g %g line1_dist = %g line2_dist = %g edgeuse pt1 %g %g %g pt2 %g %g %g\n",
			       (void *)lu, V3ARGS(line1_pt1), line1_dist, line2_dist,
			       V3ARGS(line2_pt1), V3ARGS(line2_pt2));
			bu_bomb("co-linear, pt on loopuse but not vertexuse\n");
			return NMG_CLASS_AonBshared;

		    } else if ((line1_dist > SMALL_FASTF) && (line2_dist > SMALL_FASTF)) {
			/* true when both intercepts are on the ray
			 * this is not a hit but we need to keep track of these
			 * future hits on these vertices are inconclusive
			 */
			bu_log("nmg_classify_pt_loop_new(): co-linear ON RAY, ray = %g %g %g -> %g %g %g edge = %g %g %g -> %g %g %g lu_ptr = %p line1_dist = %g line2_dist = %g\n",
			       V3ARGS(line1_pt1), V3ARGS(line1_pt2), V3ARGS(line2_pt1),
			       V3ARGS(line2_pt2), (void *)lu, line1_dist, line2_dist);
			eu2 = BU_LIST_PNEXT(edgeuse, eu2);
		    } else {
			eu2 = BU_LIST_PNEXT(edgeuse, eu2);
		    }
		} /* end of status = 0 if statement */

		if (status != 1 && status != 0) {
		    eu2 = BU_LIST_PNEXT(edgeuse, eu2);
		}
	    }
	} /* end of while-loop */

	if (hit_cnt != 0) {
	    if (NEAR_ZERO((hit_cnt/2.0) - floor(hit_cnt/2.0), SMALL_FASTF)) {
		/* true when hit_cnt is even */
		bu_log("nmg_classify_pt_loop_new(): hit_cnt = %d %f %f EVEN\n",
		       hit_cnt, (hit_cnt/2.0), floor(hit_cnt/2.0));
		out_cnt++;
	    } else {
		bu_log("nmg_classify_pt_loop_new(): hit_cnt = %d %f %f ODD\n",
		       hit_cnt, (hit_cnt/2.0), floor(hit_cnt/2.0));
		in_cnt++;
	    }
	}
    }

    if (((out_cnt > 0) && (in_cnt != 0)) || ((in_cnt > 0) && (out_cnt != 0))) {
	bu_log("lu ptr = %p pt = %g %g %g in_cnt = %d out_cnt = %d on_cnt = %d\n",
	       (void *)lu, V3ARGS(line1_pt1), in_cnt, out_cnt, on_cnt);
	bu_log("nmg_classify_pt_loop_new(): inconsistent result, point both inside and outside loopuse\n");
    }

    if (out_cnt > 0) {
	bu_log("\nnmg_classify_pt_loop_new(): END ==========================================\n\n");
	return NMG_CLASS_AoutB;
    }
    if (in_cnt > 0) {
	bu_log("\nnmg_classify_pt_loop_new(): END ==========================================\n\n");
	return NMG_CLASS_AinB;
    }

    bu_log("in_cnt = %d out_cnt = %d on_cnt = %d\n", in_cnt, out_cnt, on_cnt);
    bu_bomb("nmg_classify_pt_loop_new(): should not be here\n");

    return NMG_CLASS_Unknown; /* error */
}


int
nmg_classify_lu_lu_new(const struct loopuse *lu1, const struct loopuse *lu2, const struct bn_tol *tol)
{
    struct edgeuse *eu;
    int status;
    int in_cnt = 0;
    int out_cnt = 0;
    int on_cnt = 0;

    NMG_CK_LOOPUSE(lu1);
    NMG_CK_LOOPUSE(lu2);

    if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) != NMG_EDGEUSE_MAGIC) {
	bu_bomb("nmg_classify_lu_lu_new(): loopuse lu1 contains no edgeuse\n");
    }

    for (BU_LIST_FOR(eu, edgeuse, &lu1->down_hd)) {

	status = nmg_classify_pt_loop_new(eu->vu_p->v_p, lu2, tol);

	if (status == NMG_CLASS_AoutB) {
	    out_cnt++;
	}
	if (status == NMG_CLASS_AinB) {
	    in_cnt++;
	}
	if (status == NMG_CLASS_AonBshared) {
	    on_cnt++;
	}
    }

    if (((out_cnt > 0) && (in_cnt != 0)) || ((in_cnt > 0) && (out_cnt != 0))) {
	bu_log("in_cnt = %d out_cnt = %d on_cnt = %d\n", in_cnt, out_cnt, on_cnt);
	bu_log("nmg_classify_lu_lu_new(): inconsistent result, point both inside and outside loopuse\n");
    }

    if (out_cnt > 0) {
	return NMG_CLASS_AoutB;
    }
    if (in_cnt > 0) {
	return NMG_CLASS_AinB;
    }

    bu_log("lu1_ptr = %p lu2_ptr = %p in_cnt = %d out_cnt = %d on_cnt = %d\n",
	   (void *)lu1, (void *)lu2, in_cnt, out_cnt, on_cnt);
    bu_bomb("nmg_classify_lu_lu_new(): should not be here\n");

    return NMG_CLASS_Unknown; /* error */
}


void
insert_above(struct loopuse *lu, struct loopuse_tree_node *node, const struct bn_tol *tol)
{
    struct loopuse_tree_node *new_node, *node_tmp, *node_idx;
    int done = 0;
    int result;

    if (node->l.magic == BU_LIST_HEAD_MAGIC) {
	bu_bomb("insert_above(): was passed bu_list head, should have received bu_list node\n");
    }

    NMG_CK_LOOPUSE(lu);
    /* XXX -  where is this released? */
    BU_ALLOC(new_node, struct loopuse_tree_node);
    BU_LIST_INIT(&(new_node->l));
    new_node->l.magic = 0;
    new_node->lu = lu;
    new_node->parent = node->parent;
    BU_LIST_INIT(&(new_node->children_hd));
    new_node->children_hd.magic = BU_LIST_HEAD_MAGIC;
    node_idx = BU_LIST_PNEXT(loopuse_tree_node, &(node->l));
    BU_LIST_DEQUEUE(&(node->l));
    node->parent = new_node;
    BU_LIST_APPEND(&(new_node->children_hd), &(node->l));


    bu_log("insert_above(): lu_p = %p node ptr = %p new_node ptr = %p\n",
	   (void *)lu, (void *)node, (void *)new_node);

    if (node_idx->l.magic == BU_LIST_HEAD_MAGIC) {
	return;
    }

    while (!done) {
	if (node_idx->l.magic == BU_LIST_HEAD_MAGIC) {
	    done = 1;
	} else {
	    NMG_CK_LOOPUSE(node_idx->lu);
	    result = nmg_classify_lu_lu(node_idx->lu, lu, tol);

	    if (result == NMG_CLASS_AinB) {
		node_tmp = BU_LIST_PNEXT(loopuse_tree_node, node_idx);
		BU_LIST_DEQUEUE(&(node_idx->l));
		node_idx->parent = new_node;
		BU_LIST_APPEND(&(new_node->children_hd), &(node_idx->l));
		bu_log("insert_above(): adjust lu_p = %p node ptr = %p new_node ptr = %p\n",
		       (void *)lu, (void *)node_idx, (void *)new_node);
		node_idx = node_tmp;
	    } else {
		node_idx = BU_LIST_PNEXT(loopuse_tree_node, node_idx);
	    }
	}
    }
    BU_LIST_APPEND(&(new_node->parent->children_hd), &(new_node->l));

    return;
}

void
insert_node(struct loopuse *lu, struct bu_list *head,
	    struct loopuse_tree_node *parent, const struct bn_tol *tol)
{
    /* when 'insert_node' is first called, the level must be '0' */
    int found = 0;
    int result1 = 0; /* AinB */
    int result2 = 0; /* BinA */
    int result_tmp = 0;
    struct loopuse_tree_node *node = 0;
    struct loopuse_tree_node *new_node = 0;
    int orientation_tmp_a, orientation_tmp_b;

    NMG_CK_LOOPUSE(lu);

    if (BU_LIST_NON_EMPTY(head)) {
	for (BU_LIST_FOR(node, loopuse_tree_node, head)) {
	    NMG_CK_LOOPUSE(node->lu);

	    orientation_tmp_a = lu->orientation;
	    orientation_tmp_b = node->lu->orientation;
	    lu->orientation = 1;
	    node->lu->orientation = 1;
	    result1 = nmg_classify_lu_lu(lu, node->lu, tol);
	    lu->orientation = orientation_tmp_a;
	    node->lu->orientation = orientation_tmp_b;
	    bu_log("---- lu %p %d vs lu %p %d result1 = %d\n",
		   (void *)lu, lu->orientation,
		   (void *)node->lu, node->lu->orientation, result1);

	    orientation_tmp_a = node->lu->orientation;
	    orientation_tmp_b = lu->orientation;
	    node->lu->orientation = 1;
	    lu->orientation = 1;
	    result_tmp = nmg_classify_lu_lu_new(node->lu, lu, tol);
	    bu_log("NEW FUNCTION ---- lu %p lu-orient = %d vs lu %p lu-orient = %d result_tmp = %d\n",
		   (void *)node->lu, node->lu->orientation,
		   (void *)lu, lu->orientation, result_tmp);
	    result2 = nmg_classify_lu_lu(node->lu, lu, tol);

	    node->lu->orientation = orientation_tmp_a;
	    lu->orientation = orientation_tmp_b;
	    bu_log("OLD FUNCTION ---- lu %p lu-orient = %d vs lu %p lu-orient = %d ...result2 = %d\n",
		   (void *)node->lu, node->lu->orientation,
		   (void *)lu, lu->orientation, result2);

	    if (result_tmp != result2) {
		bu_log("nmg_classify_lu_lu_new != nmg_classify_lu_lu\n");
	    }

	    if (result1 == NMG_CLASS_AinB) {
		/* insert new node below current node */
		found = 1;
		bu_log("lu %p in lu %p\n", (void *)lu, (void *)node->lu);
		insert_node(lu, &(node->children_hd), parent, tol);
		break;
	    }
	    if (result2 == NMG_CLASS_AinB) {
		/* insert new node above current node */
		found = 1;
		bu_log("lu %p in lu %p\n", (void *)node->lu, (void *)lu);
		break;
	    }
	}
    }

    if (!found) {
	bu_log("lu %p in lu %p\n", (void *)lu, (void *)parent->lu);
    }

    if (!found || (result2 == NMG_CLASS_AinB)) {
	/* XXX -  where is this released? */
	BU_ALLOC(new_node, struct loopuse_tree_node);
	BU_LIST_INIT(&(new_node->l));
	/* unset magic from BU_LIST_HEAD_MAGIC to zero since this node
	 * is not going to be a head
	 */
	new_node->l.magic = 1;
	new_node->lu = lu;
	new_node->parent = parent;
	BU_LIST_INIT(&(new_node->children_hd));  /* also sets magic to BU_LIST_HEAD_MAGIC */
	BU_LIST_APPEND(head, &(new_node->l));
	bu_log("insert-node, insert-node ptr = %p, lu_p = %p parent ptr = %p\n",
	       (void *)&(new_node->l), (void *)new_node->lu, (void *)new_node->parent);
    }

    if (found) {
	if (result2 == NMG_CLASS_AinB) {
	    BU_LIST_DEQUEUE(&(node->l));
	    BU_LIST_APPEND(&(new_node->children_hd), &(node->l));
	}
    }

    return;
}


void
nmg_build_loopuse_tree(struct faceuse *fu, struct loopuse_tree_node **root, const struct bn_tol *tol)
{

    struct loopuse *lu;
    int loopuse_cnt = 0;
    NMG_CK_FACEUSE(fu);

    /* create initial head node */
    /* XXX -  where is this released? */
    BU_ALLOC(*root, struct loopuse_tree_node);
    BU_LIST_INIT(&((*root)->l));
    BU_LIST_INIT(&((*root)->children_hd));
    (*root)->parent = (struct loopuse_tree_node *)NULL;
    (*root)->lu = (struct loopuse *)NULL;
    bu_log("root node addr = %p\n", (void *)&((*root)->l));

    loopuse_cnt = 0;
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	loopuse_cnt++;
	bu_log("nmg_build_loopuse_tree(): %d fu_p = %p lu_p = %p\n",
	       loopuse_cnt, (void *)fu, (void *)lu);
    }

    loopuse_cnt = 0;
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	loopuse_cnt++;
	bu_log("nmg_build_loopuse_tree(): %d fu_p = %p lu_p = %p\n",
	       loopuse_cnt, (void *)fu, (void *)lu);
	bu_log("nmg_build_loopuse_tree(): root child ptr = %p root ptr = %p\n",
	       (void *)&((*root)->children_hd), (void *)*root);
	insert_node(lu, &((*root)->children_hd), *root, tol);
    }
}

/*
 * return 1 when faceuse is empty, otherwise return 0.
 *
 */
int
nmg_triangulate_fu(struct faceuse *fu, const struct bn_tol *tol)
{
    int ccw_result;
    int vert_count = 0;
    int ret = 0;

    /* boolean variables */
    int cut = 0;
    int need_triangulation = 0;

    struct bu_list *tbl2d;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct pt2d *pt;

    vect_t fu_normal;
    mat_t TformMat;

    BN_CK_TOL(tol);
    NMG_CK_FACEUSE(fu);

    NMG_GET_FU_NORMAL(fu_normal, fu);

    if (RTG.NMG_debug & DEBUG_TRI) {
	bu_log("---------------- Triangulate face fu_p = 0x%lx fu Normal = %g %g %g\n",
	       (unsigned long)fu, V3ARGS(fu_normal));
    }

    if (RTG.NMG_debug & DEBUG_TRI) {
	nmg_plot_fu("nmg_triangulate_fu_unprocessed_faceuse", fu, tol);
    }

    /* test if this faceuse needs triangulation. this for-loop is
     * to prevent extra processing on simple faceuse. after
     * initial processing of more complex faceuse we will need to
     * test the faceuse again if it needs to be triangulated
     */
    need_triangulation = 0;
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
	    continue;
	}
	if (!((lu->orientation == OT_SAME) || (lu->orientation == OT_OPPOSITE))) {
	    bu_bomb("nmg_triangulate_fu(): loopuse orientation must be OT_SAME or OT_OPPOSITE\n");
	}
	if (lu->orientation == OT_OPPOSITE) {
	    /* the faceuse must contain only exterior loopuse
	     * to possibly skip triangulation
	     */
	    need_triangulation++;
	} else {
	    vert_count = 0;
	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		NMG_CK_EDGEUSE(eu);
		vert_count++;
	    }
	    if (vert_count != 3) {
		/* we want to process the loopuse if any have more
		 * or less than 3 vertices. if less than 3, we want
		 * to remove these loopuse, if greater than 3 we
		 * want to triangulate them
		 */
		need_triangulation++;
	    }
	}
    }
    if (!need_triangulation) {
	goto out2;
    }

    /* do some cleanup before anything else */
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	(void)nmg_loop_split_at_touching_jaunt(lu, tol);
    }
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	nmg_split_touchingloops(lu, tol);
    }
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	nmg_lu_reorient(lu);
    }

    /* remove loopuse with < 3 vertices i.e. degenerate loopuse */
    if (nmg_triangulate_rm_degen_loopuse(fu, tol)) {
	/* true when faceuse is empty */
	ret = 1;
	goto out2;
    }

    if (RTG.NMG_debug & DEBUG_TRI) {
	nmg_plot_fu("nmg_triangulate_fu_after_cleanup_after_degen_loopuse_killed", fu, tol);
    }

    if (RTG.NMG_debug & DEBUG_TRI) {
	bu_log("nmg_triangulate_fu(): proceeding to triangulate face %g %g %g\n", V3ARGS(fu_normal));
    }

    /* convert 3D face to face in the X-Y plane */
    tbl2d = nmg_flatten_face(fu, TformMat, tol);

    if (RTG.NMG_debug & DEBUG_TRI) {
	validate_tbl2d("before nmg_triangulate_rm_holes", tbl2d, fu);
    }

    /* remove holes from faceuse. this is done by converting inner
     * loopuse (i.e. holes, loopuse with cw rotation, loopuse with
     * OT_OPPOSITE orientation) by joining/connecting these inner
     * loopuse with outer loopuse so the inner loopuse becomes part
     * of the outer loopuse
     */
    nmg_triangulate_rm_holes(fu, tbl2d, tol);

    if (RTG.NMG_debug & DEBUG_TRI) {
	validate_tbl2d("after nmg_triangulate_rm_holes", tbl2d, fu);
    }

    /* this 'while loop' goes through each loopuse within the faceuse
     * and performs ear-clipping (i.e. triangulation). the 'while loop'
     * ends when we can go through all the loopuse within the faceuse
     * and no cuts need to be performed. each time a cut is performed
     * we restart at the beginning of the list of loopuse. the reason
     * for this is we can not be sure of the ordering of the loopuse
     * within the faceuse after loopuse are cut.
     */
    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
    vert_count = 0;
    while (BU_LIST_NOT_HEAD(lu, &fu->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	cut = 0;
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
	    /* true when loopuse contains edgeuse */
	    vert_count = 0;
	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		NMG_CK_EDGEUSE(eu);
		vert_count++;
	    }
	    /* only ear-clip loopuse with > 3 vertices */
	    if (vert_count > 3) {
		/* test loopuse rotation */
		ccw_result = nmg_loop_is_ccw(lu, fu_normal, tol);
		if (ccw_result == 1 || ccw_result == 0) {
		    /* true when loopuse rotation is ccw */
		    if (lu->orientation != OT_SAME) {
			/* set loopuse orientation to OT_SAME */
			nmg_set_lu_orientation(lu, 0);
		    }
		    /* ear clip loopuse */
		    cut_unimonotone(tbl2d, lu, tol);
		    cut = 1;
		} else if (ccw_result == -1) {
		    /* true when loopuse rotation is cw or unknown */
		    bu_log("nmg_triangulate_fu(): lu = 0x%lx ccw_result = %d\n",
			   (unsigned long)lu, ccw_result);
		    vert_count = 0;
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			vert_count++;
			bu_log("nmg_triangulate_fu(): %d ccw_result = %d lu = %p coord = %g %g %g\n",
			       vert_count, ccw_result,
			       (void *)lu, V3ARGS(eu->vu_p->v_p->vg_p->coord));
		    }
		    bu_bomb("nmg_triangulate_fu(): attempted to cut problem loopuse\n");
		} else {
		    bu_bomb("nmg_triangulate_fu(): function nmg_loop_is_ccw returned invalid result\n");
		}
	    }
	}
	if (cut) {
	    /* since a cut was made, continue with the first
	     * loopuse in the faceuse
	     */
	    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	} else {
	    /* since no cut was made, continue with the next
	     * loopuse in the faceuse
	     */
	    lu = BU_LIST_PNEXT(loopuse, lu);
	}
    } /* close of 'loopuse while loop' */

    if (1 || RTG.NMG_debug & DEBUG_TRI) {
	/* sanity check */
	/* after triangulation, verify all loopuse contains <= 3 vertices
	 * and those loopuse with 3 vertices do not have a cw rotation
	 */
	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		vert_count = 0;
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    vert_count++;
		    if (vert_count > 3) {
			bu_bomb("nmg_triangulate_fu(): after ear-clipping found loopuse with > 3 vertices\n");
		    } else if (vert_count == 3) {
			ccw_result = nmg_loop_is_ccw(lu, fu_normal, tol);
			if (ccw_result == -1) {
			    bu_bomb("nmg_triangulate_fu(): after ear-clipping found loopuse with cw rotation\n");
			}
		    }
		}
	    }
	}
    }

    if (RTG.NMG_debug & DEBUG_TRI) {
	validate_tbl2d("nmg_triangulate_fu() after triangulation, before lu_reorient", tbl2d, fu);
    }

    /* removes loopuse with < 3 vertices i.e. degenerate loopuse */
    if (nmg_triangulate_rm_degen_loopuse(fu, tol)) {
	/* true when faceuse is empty */
	ret = 1;
	goto out1;
    }

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	/* set the loopuse orientation to OT_SAME */
	nmg_set_lu_orientation(lu, 0);
    }

    if (RTG.NMG_debug & DEBUG_TRI) {
	validate_tbl2d("nmg_triangulate_fu() after triangulation, after lu_reorient", tbl2d, fu);
    }

out1:
    while (BU_LIST_WHILE(pt, pt2d, tbl2d)) {
	BU_LIST_DEQUEUE(&pt->l);
	bu_free((char *)pt, "pt2d free");
    }

    bu_free((char *)tbl2d, "discard tbl2d");

out2:
    return ret;
}


/* Disable the old version of function nmg_triangulate_fu */
#if 0
void
nmg_triangulate_fu(struct faceuse *fu, const struct bn_tol *tol)
{
    mat_t TformMat;
    struct bu_list *tbl2d;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct vertexuse *vu;
    struct bu_list tlist;
    struct trap *tp;
    struct pt2d *pt;
    int vert_count;
    static int iter=0;
    static int monotone=0;
    vect_t N;

    char db_name[32];

    VSETALL(N, 0);

    BN_CK_TOL(tol);
    NMG_CK_FACEUSE(fu);

    if (RTG.NMG_debug & DEBUG_TRI) {
	NMG_GET_FU_NORMAL(N, fu);
	bu_log("---------------- Triangulate face %g %g %g\n",
	       V3ARGS(N));
    }


    /* make a quick check to see if we need to bother or not */
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	if (lu->orientation != OT_SAME) {
	    if (RTG.NMG_debug & DEBUG_TRI)
		bu_log("faceuse has non-OT_SAME orientation loop\n");
	    goto triangulate;
	}
	vert_count = 0;
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd))
	    if (++vert_count > 3) {
		if (RTG.NMG_debug & DEBUG_TRI)
		    bu_log("loop has more than 3 vertices\n");
		goto triangulate;
	    }
    }

    if (RTG.NMG_debug & DEBUG_TRI) {
	bu_log("---------------- face %g %g %g already triangular\n",
	       V3ARGS(N));

	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))
	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd))
		VPRINT("pt", eu->vu_p->v_p->vg_p->coord);

    }
    return;

triangulate:
    if (RTG.NMG_debug & DEBUG_TRI) {
	vect_t normal;
	NMG_GET_FU_NORMAL(normal, fu);
	bu_log("---------------- proceeding to triangulate face %g %g %g\n", V3ARGS(normal));
    }


    /* convert 3D face to face in the X-Y plane */
    tbl2d = nmg_flatten_face(fu, TformMat);

    /* avoid having a hole start as the first point */
    {
	struct pt2d *pt1, *pt2;

	pt1 = BU_LIST_FIRST(pt2d, tbl2d);
	pt2 = BU_LIST_PNEXT(pt2d, &pt1->l);

	if (vtype2d(pt1, tbl2d, tol) == HOLE_START
	    && pt1->vu_p->v_p == pt2->vu_p->v_p)
	{
	    /* swap first and second points */
	    if (RTG.NMG_debug & DEBUG_TRI)
		bu_log("Swapping first two points on vertex list (first one was a HOLE_START)\n");

	    BU_LIST_DEQUEUE(&pt1->l);
	    BU_LIST_APPEND(&pt2->l, &pt1->l);
	}
    }

    if (RTG.NMG_debug & DEBUG_TRI) {
	struct pt2d *point;
	bu_log("Face Flattened\n");
	bu_log("Vertex list:\n");
	for (BU_LIST_FOR(point, pt2d, tbl2d)) {
	    bu_log("\tpt2d %26.20e %26.20e\n", point->coord[0], point->coord[1]);
	}

	nmg_tri_plfu(fu, tbl2d);
	nmg_plot_flat_face(fu, tbl2d);
	bu_log("Face plotted\n\tmaking trapezoids...\n");
    }


    BU_LIST_INIT(&tlist);
    nmg_trap_face(tbl2d, &tlist, tol);

    if (RTG.NMG_debug & DEBUG_TRI) {
	print_tlist(tbl2d, &tlist);

	bu_log("Cutting diagonals ----------\n");
    }
    cut_diagonals(tbl2d, &tlist, fu, tol);
    if (RTG.NMG_debug & DEBUG_TRI)
	bu_log("Diagonals are cut ----------\n");

    if (RTG.NMG_debug & DEBUG_TRI) {
	sprintf(db_name, "uni%d.g", iter);
	nmg_stash_model_to_file(db_name,
				nmg_find_model(&fu->s_p->l.magic),
				"triangles and unimonotones");
    }

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))
	(void)nmg_loop_split_at_touching_jaunt(lu, tol);

    if (RTG.NMG_debug & DEBUG_TRI) {
	sprintf(db_name, "uni_sj%d.g", iter);
	nmg_stash_model_to_file(db_name,
				nmg_find_model(&fu->s_p->l.magic),
				"after split_at_touching_jaunt");
    }

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))
	nmg_split_touchingloops(lu, tol);

    if (RTG.NMG_debug & DEBUG_TRI) {
	sprintf(db_name, "uni_split%d.g", iter++);
	nmg_stash_model_to_file(db_name,
				nmg_find_model(&fu->s_p->l.magic),
				"split triangles and unimonotones");
    }

    /* now we're left with a face that has some triangle loops and some
     * uni-monotone loops.  Find the uni-monotone loops and triangulate.
     */
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);

	    bu_log("How did I miss this vertex loop %g %g %g?\n%s\n",
		   V3ARGS(vu->v_p->vg_p->coord),
		   "I'm supposed to be dealing with unimonotone loops now");
	    bu_bomb("aborting\n");

	} else if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
	    vert_count = 0;
	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		if (++vert_count > 3) {
		    cut_unimonotone(tbl2d, lu, tol);

		    if (RTG.NMG_debug & DEBUG_TRI) {
			sprintf(db_name, "uni_mono%d.g", monotone++);
			nmg_stash_model_to_file(db_name,
						nmg_find_model(&fu->s_p->l.magic),
						"newly cut unimonotone");
		    }

		    break;
		}
	    }
	}
    }


    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))
	nmg_lu_reorient(lu);

    if (RTG.NMG_debug & DEBUG_TRI)
	nmg_tri_plfu(fu, tbl2d);

    while (BU_LIST_WHILE(tp, trap, &tlist)) {
	BU_LIST_DEQUEUE(&tp->l);
	bu_free((char *)tp, "trapezoid free");
    }

    while (BU_LIST_WHILE(pt, pt2d, tbl2d)) {
	BU_LIST_DEQUEUE(&pt->l);
	bu_free((char *)pt, "pt2d free");
    }
    bu_free((char *)tbl2d, "discard tbl2d");

    return;
}
#endif
/* This is the endif to disable the old version of function
 * nmg_triangulate_fu.
 */


void
nmg_triangulate_shell(struct shell *s, const struct bn_tol *tol)
{
    struct faceuse *fu, *fu_next;

    BN_CK_TOL(tol);
    NMG_CK_SHELL(s);

    if (UNLIKELY(RTG.NMG_debug & DEBUG_TRI)) {
	bu_log("nmg_triangulate_shell(): Triangulating NMG shell.\n");
    }

    (void)nmg_edge_g_fuse(&s->magic, tol);
    (void)nmg_unbreak_shell_edges(&s->magic);

    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
    while (BU_LIST_NOT_HEAD(fu, &s->fu_hd)) {
	NMG_CK_FACEUSE(fu);
	fu_next = BU_LIST_PNEXT(faceuse, &fu->l);

	if (UNLIKELY(fu->orientation != OT_SAME && fu->orientation != OT_OPPOSITE)) {
	    /* sanity check */
	    bu_bomb("nmg_triangulate_shell(): Invalid faceuse orientation. (1)\n");
	}

	if (fu->orientation == OT_SAME) {
	    if (fu_next == fu->fumate_p) {
		/* Make sure that fu_next is not the mate of fu
		 * because if fu is killed so will its mate
		 * resulting in an invalid fu_next.
		 */
		fu_next = BU_LIST_PNEXT(faceuse, &fu_next->l);
	    }
	    if (UNLIKELY(fu->fumate_p->orientation != OT_OPPOSITE)) {
		/* sanity check */
		bu_bomb("nmg_triangulate_shell(): Invalid faceuse orientation. (2)\n");
	    }
	    if (nmg_triangulate_fu(fu, tol)) {
		/* true when faceuse is empty */
		if (nmg_kfu(fu)) {
		    bu_bomb("nmg_triangulate_shell(): Shell contains no faceuse.\n");
		}
	    }
	}
	fu = fu_next;
    }

    nmg_vsshell(s);

    if (UNLIKELY(RTG.NMG_debug & DEBUG_TRI)) {
	bu_log("nmg_triangulate_shell(): Triangulating NMG shell completed.\n");
    }
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
