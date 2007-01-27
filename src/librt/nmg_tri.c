/*                       N M G _ T R I . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2007 United States Government as represented by
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
/** @file nmg_tri.c
 *
 *  Triangulate the faces of a polygonal NMG.
 *
 *  Author -
 *	Lee A. Butler
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/** @} */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "plot3.h"


/* macros for comparing 2D points in scanline order */
/* XXX maybe should use near zero tolerance instead */
#define TOL_2D	1.0e-10
#define P_GT_V(_p, _v) \
	(((_p)->coord[Y] - (_v)->coord[Y]) > TOL_2D || (NEAR_ZERO((_p)->coord[Y] - (_v)->coord[Y], TOL_2D) && (_p)->coord[X] < (_v)->coord[X]))
#define P_LT_V(_p, _v) \
	(((_p)->coord[Y] - (_v)->coord[Y]) < (-TOL_2D) || (NEAR_ZERO((_p)->coord[Y] - (_v)->coord[Y], TOL_2D) && (_p)->coord[X] > (_v)->coord[X]))
#define P_GE_V(_p, _v) \
	(((_p)->coord[Y] - (_v)->coord[Y]) > TOL_2D || (NEAR_ZERO((_p)->coord[Y] - (_v)->coord[Y], TOL_2D) && (_p)->coord[X] <= (_v)->coord[X]))
#define P_LE_V(_p, _v) \
	(((_p)->coord[Y] - (_v)->coord[Y]) < (-TOL_2D) || (NEAR_ZERO((_p)->coord[Y] - (_v)->coord[Y], TOL_2D) && (_p)->coord[X] >= (_v)->coord[X]))

#define NMG_PT2D_MAGIC	0x2d2d2d2d
#define NMG_TRAP_MAGIC  0x1ab1ab
#define	NMG_CK_PT2D(_p)	NMG_CKMAG(_p, NMG_PT2D_MAGIC, "pt2d")
#define	NMG_CK_TRAP(_p)	{NMG_CKMAG(_p, NMG_TRAP_MAGIC, "trap");\
	if ( ! BU_LIST_PREV(bu_list, &(_p)->l) ) {\
		bu_log("%s %d bad prev pointer of trapezoid 0x%08x\n",\
			__FILE__, __LINE__, &(_p)->l);\
		rt_bomb("NMG_CK_TRAP: aborting");\
	} else if (! BU_LIST_NEXT(bu_list, &(_p)->l) ) {\
		bu_log("%s %d bad next pointer of trapezoid 0x%08x\n",\
			__FILE__, __LINE__, &(_p)->l);\
		rt_bomb("NMG_CL_TRAP: aborting");\
	}}

#define NMG_TBL2D_MAGIC 0x3e3e3e3e
#define NMG_CK_TBL2D(_p) NMG_CKMAG(_p, NMG_TBL2D_MAGIC, "tbl2d")

/* macros to retrieve the next/previous 2D point about loop */
#define PT2D_NEXT(tbl, pt) pt2d_pn(tbl, pt, 1)
#define PT2D_PREV(tbl, pt) pt2d_pn(tbl, pt, -1)

struct pt2d {
	struct bu_list	l;		/* scanline ordered list of points */
	fastf_t	coord[3];		/* point coordinates in 2-D space */
	struct vertexuse *vu_p;		/* associated vertexuse */
};


struct trap {
	struct bu_list	l;
	struct pt2d	*top;	   /* point at top of trapezoid */
	struct pt2d	*bot;	   /* point at bottom of trapezoid */
	struct edgeuse	*e_left;
	struct edgeuse	*e_right;
};

/* if there is an edge in this face which connects the points
 *	return 1
 * else
 *	return 0
 */


/* subroutine version to pass to the rt_tree functions */
int PvsV(struct trap *p, struct trap *v)
{
	NMG_CK_TRAP(p);
	NMG_CK_TRAP(v);

	if (p->top->coord[Y] > v->top->coord[Y]) return(1);
	else if (p->top->coord[Y] < v->top->coord[Y]) return(-1);
	else if (p->top->coord[X] < v->top->coord[X]) return(1);
	else if (p->top->coord[X] > v->top->coord[X]) return(-1);
	else	return(0);
}


static struct pt2d *find_pt2d(struct bu_list *tbl2d, struct vertexuse *vu);
static FILE *plot_fd;

static void
print_2d_eu(char *s, struct edgeuse *eu, struct bu_list *tbl2d)
{
	struct pt2d *pt, *pt_next;
	NMG_CK_TBL2D(tbl2d);
	NMG_CK_EDGEUSE(eu);

	pt = find_pt2d(tbl2d, eu->vu_p);
	pt_next = find_pt2d(tbl2d, (BU_LIST_PNEXT_CIRC(edgeuse, eu))->vu_p);
	bu_log("%s: 0x%08x %g %g -> %g %g\n", s, eu,
		pt->coord[X], pt->coord[Y],
		pt_next->coord[X], pt_next->coord[Y]);
}


static void
print_trap(struct trap *tp, struct bu_list *tbl2d)
{
	NMG_CK_TBL2D(tbl2d);
	NMG_CK_TRAP(tp);

	bu_log("trap 0x%08x top pt2d: 0x%08x %g %g vu:0x%08x\n",
			tp,
			&tp->top, tp->top->coord[X], tp->top->coord[Y],
			tp->top->vu_p);

	if (tp->bot)
		bu_log("\t\tbot pt2d: 0x%08x %g %g vu:0x%08x\n",
			&tp->bot, tp->bot->coord[X], tp->bot->coord[Y],
			tp->bot->vu_p);
	else {
		bu_log("\tbot (nil)\n");
	}

	if (tp->e_left)
		print_2d_eu("\t\t  e_left", tp->e_left, tbl2d);

	if (tp->e_right)
		print_2d_eu("\t\t e_right", tp->e_right, tbl2d);
}
static void
print_tlist(struct bu_list *tbl2d, struct bu_list *tlist)
{
	struct trap *tp;
	NMG_CK_TBL2D(tbl2d);

	bu_log("Trapezoid list start ----------\n");
	for (BU_LIST_FOR(tp, trap, tlist)) {
		NMG_CK_TRAP(tp);
		print_trap(tp, tbl2d);
	}
	bu_log("Trapezoid list end ----------\n");
}

static int flatten_debug=1;

static struct pt2d *
find_pt2d(struct bu_list *tbl2d, struct vertexuse *vu)
{
	struct pt2d *p;
	NMG_CK_TBL2D(tbl2d);
	NMG_CK_VERTEXUSE(vu);

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
	FILE *fd;
	char name[25];
	char buf[80];
	long *b;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct vertexuse *vu;
	struct pt2d *p;

	NMG_CK_TBL2D(tbl2d);
	NMG_CK_FACEUSE(fu);

	sprintf(name, "tri%02d.pl", file_number++);
	if ((fd=fopen(name, "w")) == (FILE *)NULL) {
		perror(name);
		abort();
	}

	bu_log("\tplotting %s\n", name);
	b = (long *)bu_calloc( fu->s_p->r_p->m_p->maxindex,
		sizeof(long), "bit vec"),

	pl_erase(fd);
	pd_3space(fd,
		fu->f_p->min_pt[0]-1.0,
		fu->f_p->min_pt[1]-1.0,
		fu->f_p->min_pt[2]-1.0,
		fu->f_p->max_pt[0]+1.0,
		fu->f_p->max_pt[1]+1.0,
		fu->f_p->max_pt[2]+1.0);

	nmg_pl_fu(fd, fu, b, 255, 255, 255);

	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		if ( BU_LIST_IS_EMPTY(&lu->down_hd) ) {
			bu_log("Empty child list for loopuse %s %d\n", __FILE__, __LINE__);
		} else if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC){
			vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
			pdv_3move(fd, vu->v_p->vg_p->coord);
			if ( (p=find_pt2d(tbl2d, vu)) ) {
				sprintf(buf, "%g, %g",
					p->coord[0], p->coord[1]);
				pl_label(fd, buf);
			} else
				pl_label(fd, "X, Y (no 2D coords)");

		} else {
			eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
			NMG_CK_EDGEUSE( eu );

			for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				if ( (p=find_pt2d(tbl2d,eu->vu_p)) ) {
					pdv_3move(fd, eu->vu_p->v_p->vg_p->coord);

					sprintf(buf, "%g, %g",
						p->coord[0], p->coord[1]);
					pl_label(fd, buf);
				} else
					pl_label(fd, "X, Y (no 2D coords)");
			}
		}
	}


	bu_free((char *)b, "plot table");
	fclose(fd);
}


/**	P T 2 D _ P N
 *
 *	Return Prev/Next 2D pt about loop from given 2D pt.
 *	if vertex is child of loopuse, return parameter 2D pt.
 */
static struct pt2d *
pt2d_pn(struct bu_list *tbl, struct pt2d *pt, int dir)
{
	struct edgeuse *eu, *eu_other;
	struct pt2d *new_pt;

	NMG_CK_TBL2D(tbl);
	NMG_CK_PT2D( pt );
	NMG_CK_VERTEXUSE( (pt)->vu_p );

	if ( *(pt)->vu_p->up.magic_p == NMG_EDGEUSE_MAGIC) {
		eu = (pt)->vu_p->up.eu_p;
		NMG_CK_EDGEUSE( eu );
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
			rt_bomb("goodbye\n");
		}
		NMG_CK_PT2D( new_pt );
		return new_pt;
	}

	if ( *(pt)->vu_p->up.magic_p != NMG_LOOPUSE_MAGIC) {
		bu_log("%s %d Bad vertexuse parent (%g %g %g)\n",
			__FILE__, __LINE__,
			V3ARGS( (pt)->vu_p->v_p->vg_p->coord ) );
		rt_bomb("goodbye\n");
	}

	return pt;
}


/**	M A P _ V U _ T O _ 2 D
 *
 *	Add a vertex to the 2D table if it isn't already there.
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


	np = (struct pt2d *)bu_calloc(1, sizeof(struct pt2d), "pt2d struct");
	np->coord[2] = 0.0;
	np->vu_p = vu;
	BU_LIST_MAGIC_SET(&np->l, NMG_PT2D_MAGIC);

	/* if one of the other vertexuses has been mapped, use that data */
	for (BU_LIST_FOR(vu_p, vertexuse, &vu->v_p->vu_hd)) {
		if ( (p = find_pt2d(tbl2d, vu_p)) ) {
			VMOVE(np->coord, p->coord);
			return;
		}
	}

	/* transform the 3-D vertex into a 2-D vertex */
	vg = vu->v_p->vg_p;
	MAT4X3PNT(np->coord, mat, vg->coord);


	if (rt_g.NMG_debug & DEBUG_TRI && flatten_debug) bu_log(
		"Transforming 0x%x 3D(%g, %g, %g) to 2D(%g, %g, %g)\n",
		vu, V3ARGS(vg->coord), V3ARGS(np->coord) );

	/* find location in scanline ordered list for vertex */
	for ( BU_LIST_FOR(p, pt2d, tbl2d) ) {
		if (P_GT_V(p, np)) continue;
		break;
	}
	BU_LIST_INSERT(&p->l, &np->l);

	if (rt_g.NMG_debug & DEBUG_TRI && flatten_debug)
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
		if( !fu_of_vu )  continue;	/* skip vu's of wire edges */
		NMG_CK_FACEUSE(fu_of_vu);
		if (fu_of_vu != fu) continue;

		if (rt_g.NMG_debug & DEBUG_TRI && flatten_debug)
			bu_log("transform 0x%x... ", vu_p);

		/* if vertexuse already transformed, skip it */
		if (find_pt2d(tbl2d, vu_p)) {
			if (rt_g.NMG_debug & DEBUG_TRI && flatten_debug) {
				bu_log("vertexuse already transformed\n", vu);
				nmg_pr_vu(vu, NULL);
			}
			continue;
		}

		/* add vertexuse to list */
		p = (struct pt2d *)bu_calloc(1, sizeof(struct pt2d), "pt2d");
		p->vu_p = vu_p;
		VMOVE(p->coord, np->coord);
		BU_LIST_MAGIC_SET(&p->l, NMG_PT2D_MAGIC);

		BU_LIST_APPEND(&np->l, &p->l);

		if (rt_g.NMG_debug & DEBUG_TRI && flatten_debug)
			(void)bu_log( "vertexuse transformed\n");
	}
	if (rt_g.NMG_debug & DEBUG_TRI && flatten_debug)
		(void)bu_log( "Done.\n");
}

/**	N M G _ F L A T T E N _ F A C E
 *
 *	Create the 2D coordinate table for the vertexuses of a face.
 *
 *	---------	-----------------------------------
 *	|pt2d --+-----> |     struct pt2d.{magic,coord[3]} |
 *	---------	-----------------------------------
 *			|     struct pt2d.{magic,coord[3]} |
 *			-----------------------------------
 *			|     struct pt2d.{magic,coord[3]} |
 *			-----------------------------------
 *
 *	When the caller is done, nmg_free_2d_map() should be called to dispose
 *	of the map
 */
struct bu_list *
nmg_flatten_face(struct faceuse *fu, fastf_t *TformMat)
{
	static const vect_t twoDspace = { 0.0, 0.0, 1.0 };
	struct bu_list *tbl2d;
	struct vertexuse *vu;
	struct loopuse *lu;
	struct edgeuse *eu;
	vect_t Normal;

	NMG_CK_FACEUSE(fu);

	tbl2d = (struct bu_list *)bu_calloc(1, sizeof(struct bu_list),
		"2D coordinate list");

	/* we use the 0 index entry in the table as the head of the sorted
	 * list of verticies.  This is safe since the 0 index is always for
	 * the model structure
	 */

	BU_LIST_INIT( tbl2d );
	BU_LIST_MAGIC_SET(tbl2d, NMG_TBL2D_MAGIC);

	/* construct the matrix that maps the 3D coordinates into 2D space */
	NMG_GET_FU_NORMAL(Normal, fu);
	bn_mat_fromto( TformMat, Normal, twoDspace );

	if (rt_g.NMG_debug & DEBUG_TRI && flatten_debug)
		bn_mat_print( "TformMat", TformMat );


	/* convert each vertex in the face to its 2-D equivalent */
	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (rt_g.NMG_debug & DEBUG_TRI) {
			switch (lu->orientation) {
			case OT_NONE:	bu_log("flattening OT_NONE loop\n"); break;
			case OT_SAME:	bu_log("flattening OT_SAME loop\n"); break;
			case OT_OPPOSITE:bu_log("flattening OT_OPPOSITE loop\n"); break;
			case OT_UNSPEC:	bu_log("flattening OT_UNSPEC loop\n"); break;
			case OT_BOOLPLACE:bu_log("flattening OT_BOOLPLACE loop\n"); break;
			default: bu_log("flattening bad orientation loop\n"); break;
			}
		}
		if (BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC) {
			vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
			if (rt_g.NMG_debug & DEBUG_TRI && flatten_debug)
				bu_log("vertex loop\n");
			map_vu_to_2d(vu, tbl2d, TformMat, fu);

		} else if (BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_EDGEUSE_MAGIC) {
			if (rt_g.NMG_debug & DEBUG_TRI && flatten_debug)
				bu_log("edge loop\n");
			for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				vu = eu->vu_p;
				if (rt_g.NMG_debug & DEBUG_TRI && flatten_debug)
					bu_log("(%g %g %g) -> (%g %g %g)\n",
						V3ARGS(vu->v_p->vg_p->coord),
						V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord));
				map_vu_to_2d(vu, tbl2d, TformMat, fu);
			}
		} else rt_bomb("bad magic of loopuse child\n");
	}

	return(tbl2d);
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
	angle = bn_angle_measure( bc, ab, pv );

	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log("\tangle == %g tol angle: %g\n", angle, tol->perp);

	return (angle > tol->perp && angle <= M_PI-tol->perp);
}

#define POLY_SIDE 1
#define HOLE_START 2
#define POLY_START 3
#define HOLE_END 4
#define POLY_END 5
#define HOLE_POINT 6
#define POLY_POINT 7


/**
 *
 *	characterize the edges which meet at this vertex.
 *
 *	  1 	     2	       3	   4	    5	      6		7
 *
 *      /- -\	  -------		-\   /-     \---/  -------
 *     /-- --\	  ---O---	O	--\ /--      \-/   ---O---	O
 *    O--- ---O	  --/ \--      /-\	---O---       O	   -------
 *     \-- --/	  -/   \-     /---\	-------
 *      \- -/
 *
 *    Poly Side		    Poly Start 		   Poly End
 *	         Hole Start    		Hole end
 */
static int
vtype2d(struct pt2d *v, struct bu_list *tbl2d, const struct bn_tol *tol)
{
	struct pt2d *p, *n;	/* previous/this edge endpoints */
	struct loopuse *lu;

	NMG_CK_TBL2D(tbl2d);
	NMG_CK_PT2D(v);

	/* get the next/previous points relative to v */
	p = PT2D_PREV(tbl2d, v);
	n = PT2D_NEXT(tbl2d, v);


	lu = nmg_find_lu_of_vu( v->vu_p );

	if (p == n && n == v) {
		/* loopuse of vertexuse or loopuse of 1 edgeuse */
		if (lu->orientation == OT_SAME)
			return(POLY_POINT);
		else if (lu->orientation == OT_OPPOSITE)
			return(HOLE_POINT);
	}

	if (P_GT_V(n, v) && P_GT_V(p, v)) {
		/*
		 *   \   /
		 *    \ /
		 *     .
		 *
		 * if this is a convex point, this is a polygon end
		 * if it is a concave point, this is a hole end
		 */

		if (p == n) {
			if (lu->orientation == OT_OPPOSITE)
				return(HOLE_END);
			else if (lu->orientation == OT_SAME)
				return(POLY_END);
			else {
				bu_log("%s: %d loopuse is not OT_SAME or OT_OPPOSITE\n",
					__FILE__, __LINE__);
				rt_bomb("bombing\n");
			}
		}

		if (is_convex(p, v, n, tol)) return(POLY_END);
		else return(HOLE_END);

	}

	if (P_LT_V(n, v) && P_LT_V(p, v)) {
		/*      .
		 *     / \
		 *    /   \
		 *
		 * if this is a convex point, this is a polygon start
		 * if this is a concave point, this is a hole start
		 */

		if (p == n) {
			if (lu->orientation == OT_OPPOSITE)
				return(HOLE_START);
			else if (lu->orientation == OT_SAME)
				return(POLY_START);
			else {
				bu_log("%s: %d loopuse is not OT_SAME or OT_OPPOSITE\n",
					__FILE__, __LINE__);
				rt_bomb("bombing\n");
			}
		}

		if (is_convex(p, v, n, tol))
			return(POLY_START);
		else
			return(HOLE_START);
	}
	if ( (P_GT_V(n, v) && P_LT_V(p, v)) ||
	    (P_LT_V(n, v) && P_GT_V(p, v)) ) {
		/*
		 *  |
		 *  |
		 *  .
		 *   \
		 *    \
		 *
		 * This is the "side" of a polygon.
		 */
		return(POLY_SIDE);
	}
	bu_log(
		"%s %d HELP! special case:\n(%g %g)->(%g %g)\n(%g %g)->(%g %g)\n",
		__FILE__, __LINE__,
		p->coord[X], p->coord[Y],
		v->coord[X], v->coord[Y],
		n->coord[X], n->coord[Y]);

	return(0);
}

/**	Polygon point start.
 *
 *	  O
 *	 /-\
 *	/---\
 *	v
 *
 *	start new trapezoid
 */
static void
poly_start_vertex(struct pt2d *pt, struct bu_list *tbl2d, struct bu_list *tlist)
{
	struct trap *new_trap;

	NMG_CK_TBL2D(tbl2d);
	NMG_CK_PT2D(pt);
	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log( "%g %g is polygon start vertex\n",
				pt->coord[X], pt->coord[Y]);

	new_trap = (struct trap *)bu_calloc(sizeof(struct trap), 1, "new poly_start trap");
	new_trap->top = pt;
	new_trap->bot = (struct pt2d *)NULL;
	new_trap->e_left = pt->vu_p->up.eu_p;
	new_trap->e_right = BU_LIST_PPREV_CIRC(edgeuse, pt->vu_p->up.eu_p);
	BU_LIST_MAGIC_SET(&new_trap->l, NMG_TRAP_MAGIC);

	/* add new trapezoid */
	BU_LIST_APPEND(tlist, &new_trap->l);
	NMG_CK_TRAP(new_trap);
}

/**
 *		^
 *	  /-	-\
 *	 /--	--\
 *	O---	---O
 *	 \--	--/
 *	  \-	-/
 *	   v
 *
 *	finish trapezoid from vertex, start new trapezoid from vertex
 */
static void
poly_side_vertex(struct pt2d *pt, struct pt2d *tbl2d, struct bu_list *tlist)
{
	struct trap *new_trap, *tp;
	struct edgeuse *upper_edge=NULL, *lower_edge=NULL;
	struct pt2d *pnext, *plast;

	NMG_CK_TBL2D(tbl2d);
	NMG_CK_PT2D(pt);
	pnext = PT2D_NEXT(&tbl2d->l, pt);
	plast = PT2D_PREV(&tbl2d->l, pt);
	if (rt_g.NMG_debug & DEBUG_TRI) {
		bu_log( "%g %g is polygon side vertex\n",
			pt->coord[X], pt->coord[Y]);
		bu_log( "%g %g -> %g %g -> %g %g\n",
			plast->coord[X],
			plast->coord[Y],
			pt->coord[X], pt->coord[Y],
			pnext->coord[X],
			pnext->coord[Y]);
	}

	/* find upper edge */
	if (P_LT_V(plast, pt) && P_GT_V(pnext, pt)) {
		/* ascending edge */
		upper_edge = pt->vu_p->up.eu_p;
		lower_edge = plast->vu_p->up.eu_p;
	} else if (P_LT_V(pnext, pt) && P_GT_V(plast, pt)) {
		/* descending edge */
		upper_edge = plast->vu_p->up.eu_p;
		lower_edge = pt->vu_p->up.eu_p;
	}

	NMG_CK_EDGEUSE(upper_edge);
	NMG_CK_EDGEUSE(lower_edge);

	/* find the uncompleted trapezoid in the tree
	 * which contains the upper edge.  This is the trapezoid we will
	 * complete, and where we will add a new trapezoid
	 */
	for (BU_LIST_FOR(tp, trap, tlist)) {
		NMG_CK_TRAP(tp);
		NMG_CK_EDGEUSE(tp->e_left);
		NMG_CK_EDGEUSE(tp->e_right);
		if ((tp->e_left == upper_edge || tp->e_right == upper_edge) &&
		    tp->bot == (struct pt2d *)NULL) {
			break;
		    }
	}

	if (BU_LIST_MAGIC_WRONG(&tp->l, NMG_TRAP_MAGIC))
		rt_bomb ("didn't find trapezoid parent\n");

	/* complete trapezoid */
	tp->bot = pt;

	/* create new trapezoid with other (not upper) edge */
	new_trap = (struct trap *)bu_calloc(sizeof(struct trap), 1, "new side trap");
	BU_LIST_MAGIC_SET(&new_trap->l, NMG_TRAP_MAGIC);
	new_trap->top = pt;
	new_trap->bot = (struct pt2d *)NULL;
	if (tp->e_left == upper_edge) {
		new_trap->e_left = lower_edge;
		new_trap->e_right = tp->e_right;
	} else if (tp->e_right == upper_edge) {
		new_trap->e_right = lower_edge;
		new_trap->e_left = tp->e_left;
	} else	/* how did I get here? */
		rt_bomb("Why me?  Always me!\n");

	BU_LIST_INSERT(tlist, &new_trap->l);
	NMG_CK_TRAP(new_trap);
}


/**	Polygon point end.
 *
 *	     ^
 *	\---/
 *	 \-/
 *	  O
 *
 *	complete trapezoid
 */
static void
poly_end_vertex(struct pt2d *pt, struct bu_list *tbl2d, struct bu_list *tlist)
{
	struct trap *tp;
	struct edgeuse *e_left, *e_right;
	struct pt2d *pprev;

	NMG_CK_TBL2D(tbl2d);
	NMG_CK_PT2D(pt);
	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log( "%g %g is polygon end vertex\n",
			pt->coord[X], pt->coord[Y]);

	/* get the two edges which end at this point */
	pprev = PT2D_PREV(tbl2d, pt);
	if (pprev == pt)
		rt_bomb("pprev == pt!\n");

	e_left = pprev->vu_p->up.eu_p;
	e_right = pt->vu_p->up.eu_p;

	/* find the trapezoid in tree which has
	 * both edges ending at this point.
	 */
	for (BU_LIST_FOR(tp, trap, tlist)) {
		NMG_CK_TRAP(tp);
		if (tp->e_left == e_left && tp->e_right == e_right && !tp->bot) {
			goto trap_found;
		} else if (tp->e_right == e_left && tp->e_left == e_right &&
		    !tp->bot) {
			/* straighten things out for notational convenience*/
			e_right = tp->e_right;
			e_left = tp->e_left;
			goto trap_found;
		}
	}

	rt_bomb("Didn't find trapezoid to close!\n");

	/* Complete the trapezoid. */
trap_found:
	tp->bot = pt;
}


/**	Hole Start in polygon
 *
 *	-------
 *	---O---
 *	--/ \--
 *	-/   \-
 *	      v
 *
 *	Finish existing trapezoid, start 2 new ones
 */
static void
hole_start_vertex(struct pt2d *pt, struct bu_list *tbl2d, struct bu_list *tlist)
{
	struct trap *tp, *new_trap;
	vect_t pv, ev, n;
	struct pt2d *e_pt, *next_pt;

	NMG_CK_TBL2D(tbl2d);
	NMG_CK_PT2D(pt);
	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log( "%g %g is hole start vertex\n",
			pt->coord[X], pt->coord[Y]);

	/* we need to find the un-completed trapezoid which encloses this
	 * point.
	 */
	for (BU_LIST_FOR(tp, trap, tlist)) {
		NMG_CK_TRAP(tp);
		/* obviously, if the trapezoid has been completed, it's not
		 * the one we want.
		 */
		if (tp->bot) {
			if (rt_g.NMG_debug & DEBUG_TRI)
				bu_log("Trapezoid %g %g / %g %g completed... Skipping\n",
					tp->top->coord[X],
					tp->top->coord[Y],
					tp->bot->coord[X],
					tp->bot->coord[Y]);
			continue;
		}

		/* if point is at the other end of either the left edge
		 * or the right edge, we've found the trapezoid to complete.
		 *
		 * First, we check the left edge
		 */
		e_pt = find_pt2d(tbl2d, tp->e_left->vu_p);
		next_pt = find_pt2d(tbl2d,
			(BU_LIST_PNEXT_CIRC(edgeuse, tp->e_left))->vu_p);

		if (e_pt->vu_p->v_p == pt->vu_p->v_p ||
		    next_pt->vu_p->v_p == pt->vu_p->v_p)
			goto gotit;


		/* check to see if the point is at the end of the right edge
		 * of the trapezoid
		 */
		e_pt = find_pt2d(tbl2d, tp->e_right->vu_p);
		next_pt = find_pt2d(tbl2d,
			(BU_LIST_PNEXT_CIRC(edgeuse, tp->e_right))->vu_p);

		if (e_pt->vu_p->v_p == pt->vu_p->v_p ||
		    next_pt->vu_p->v_p == pt->vu_p->v_p)
			goto gotit;


		/* if point is right of left edge and left of right edge
		 * we've found the trapezoid we need to work with.
		 */

		/* form a vector from the start point of each edge to pt.
		 * if crossing this vector with the vector of the edge
		 * produces a vector with a positive Z component then the pt
		 * is "inside" the trapezoid as far as this edge is concerned
		 */
		e_pt = find_pt2d(tbl2d, tp->e_left->vu_p);
		next_pt = find_pt2d(tbl2d,
			(BU_LIST_PNEXT_CIRC(edgeuse, tp->e_left))->vu_p);
		VSUB2(pv, pt->coord, e_pt->coord);
		VSUB2(ev, next_pt->coord, e_pt->coord);
		VCROSS(n, ev, pv);
		if (n[2] <= 0.0) {
			if (rt_g.NMG_debug & DEBUG_TRI)
				bu_log("Continue #1\n");
			continue;
		}

		e_pt = find_pt2d(tbl2d, tp->e_right->vu_p);
		next_pt = find_pt2d(tbl2d,
			(BU_LIST_PNEXT_CIRC(edgeuse, tp->e_right))->vu_p);
		VSUB2(pv, pt->coord, e_pt->coord);
		VSUB2(ev, next_pt->coord, e_pt->coord);
		VCROSS(n, ev, pv);
		if (n[2] <= 0.0) {
			if (rt_g.NMG_debug & DEBUG_TRI)
				bu_log("Continue #2\n");
			continue;
		}

		goto gotit;

	}

	bu_log("didn't find trapezoid for hole-start point at:\n\t%g %g %g\n",
		V3ARGS(pt->vu_p->v_p->vg_p->coord) );

	nmg_stash_model_to_file("tri_lone_hole.g",
		nmg_find_model(&pt->vu_p->l.magic),
		"lone hole start");

	rt_bomb("bombing\n");
gotit:
	/* complete existing trapezoid */
	tp->bot = pt;
	/* create new left and right trapezoids */

	new_trap = (struct trap *)bu_calloc(sizeof(struct trap), 1, "New hole start trapezoids");
	new_trap->top = pt;
	new_trap->bot = (struct pt2d *)NULL;
	new_trap->e_left = tp->e_left;
	new_trap->e_right = BU_LIST_PPREV_CIRC(edgeuse, pt->vu_p->up.eu_p);
	BU_LIST_MAGIC_SET(&new_trap->l, NMG_TRAP_MAGIC);
	BU_LIST_APPEND(&tp->l, &new_trap->l);

	new_trap = (struct trap *)bu_calloc(sizeof(struct trap), 1, "New hole start trapezoids");
	new_trap->top = pt;
	new_trap->bot = (struct pt2d *)NULL;
	new_trap->e_left = pt->vu_p->up.eu_p;
	new_trap->e_right = tp->e_right;
	BU_LIST_MAGIC_SET(&new_trap->l, NMG_TRAP_MAGIC);
	BU_LIST_APPEND(&tp->l, &new_trap->l);
}


/**	Close hole
 *
 *	^
 *	-\   /-
 *	--\ /--
 *	---O---
 *	-------
 *
 *	complete right and left trapezoids
 *	start new trapezoid
 *
 */
static void
hole_end_vertex(struct pt2d *pt, struct bu_list *tbl2d, struct bu_list *tlist)
{
	struct edgeuse *eunext, *euprev;
	struct trap *tp, *tpnext, *tpprev;

	NMG_CK_TBL2D(tbl2d);
	NMG_CK_PT2D(pt);
	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log( "%g %g is hole end vertex\n",
			pt->coord[X], pt->coord[Y]);

	/* find the trapezoids that will end at this vertex */
	eunext = pt->vu_p->up.eu_p;
	euprev = BU_LIST_PPREV_CIRC(edgeuse, eunext);
	tpnext = tpprev = (struct trap *)NULL;

	if (rt_g.NMG_debug & DEBUG_TRI) {
		print_2d_eu("eunext", eunext, tbl2d);
		print_2d_eu("euprev", euprev, tbl2d);
	}

	for (BU_LIST_FOR(tp, trap, tlist)) {
		NMG_CK_TRAP(tp);
		/* obviously, if the trapezoid has been completed, it's not
		 * the one we want.
		 */
		NMG_CK_TRAP(tp);

		if (tp->bot) {
#if 0
			if (rt_g.NMG_debug & DEBUG_TRI) {
				print_trap(tp, tbl2d);
				bu_log("Completed... Skipping\n");
			}
#endif
			continue;
		} else {
			if (rt_g.NMG_debug & DEBUG_TRI)
				print_trap(tp, tbl2d);
		}

		if (tp->e_left == eunext || tp->e_right == eunext) {
			if (rt_g.NMG_debug & DEBUG_TRI)
				bu_log("Found tpnext\n");
			tpnext = tp;
		}

		if (tp->e_right == euprev || tp->e_left == euprev) {
			if (rt_g.NMG_debug & DEBUG_TRI)
				bu_log("Found tpprev\n");
			tpprev = tp;
		}
		if (tpnext && tpprev)
			goto gotem;
	}

	rt_bomb("couldn't find both trapezoids of hole closing vertex\n");
gotem:
	NMG_CK_TRAP(tpnext);
	NMG_CK_TRAP(tpprev);

	/* finish off the two trapezoids */
	tpnext->bot = pt;
	tpprev->bot = pt;

	/* start one new trapezoid */

	tp = (struct trap *)bu_calloc(1, sizeof(struct pt2d), "pt2d struct");
	tp->top = pt;
	tp->bot = (struct pt2d *)NULL;
	if (tpnext->e_left == eunext) {
		tp->e_right = tpnext->e_right;
		tp->e_left = tpprev->e_left;
	} else if (tpnext->e_right == eunext) {
		tp->e_left = tpnext->e_left;
		tp->e_right = tpprev->e_right;
	} else
		rt_bomb("Which is my left and which is my right?\n");

	BU_LIST_MAGIC_SET(&tp->l, NMG_TRAP_MAGIC);
	BU_LIST_APPEND(&tpprev->l, &tp->l);
}


/**
 *	N M G _ T R A P _ F A C E
 *
 *	Produce the trapezoidal decomposition of a face from the set of
 *	2D points.
 */
static void
nmg_trap_face(struct bu_list *tbl2d, struct bu_list *tlist, const struct bn_tol *tol)
{
	struct pt2d *pt;

	NMG_CK_TBL2D(tbl2d);

	for (BU_LIST_FOR(pt, pt2d, tbl2d)) {
		NMG_CK_PT2D(pt);
		switch(vtype2d(pt, tbl2d, tol)) {
		case POLY_SIDE:
			poly_side_vertex(pt, (struct pt2d *)tbl2d, tlist);
			break;
		case HOLE_START:
			hole_start_vertex(pt, tbl2d, tlist);
			break;
		case POLY_START:
			poly_start_vertex(pt, tbl2d, tlist);
			break;
		case HOLE_END:
			hole_end_vertex(pt, tbl2d, tlist);
			break;
		case POLY_END:
			poly_end_vertex(pt, tbl2d, tlist);
			break;
		default:
			bu_log( "%g %g is UNKNOWN type vertex %s %d\n",
				pt->coord[X], pt->coord[Y],
				__FILE__, __LINE__);
			break;
		}
	}

}


static void
map_new_vertexuse(struct bu_list *tbl2d, struct vertexuse *vu_p)
{
	struct vertexuse *vu;
	struct pt2d *p, *new_pt2d;

	NMG_CK_TBL2D(tbl2d);
	NMG_CK_VERTEXUSE(vu_p);

	/* if it's already mapped we're outta here! */
	if ( (p = find_pt2d(tbl2d, vu_p)) ) {
		if (rt_g.NMG_debug & DEBUG_TRI)
		    bu_log("%s %d map_new_vertexuse() vertexuse already mapped!\n",
			__FILE__, __LINE__);
		return;
	}
	/* allocate memory for new 2D point */
	new_pt2d = (struct pt2d *)
		bu_calloc(1, sizeof(struct pt2d), "pt2d struct");

	/* find another use of the same vertex that is already mapped */
	for ( BU_LIST_FOR(vu, vertexuse, &vu_p->v_p->vu_hd) ) {
		NMG_CK_VERTEXUSE(vu);
		if (! (p=find_pt2d(tbl2d, vu)) )
			continue;

		/* map parameter vertexuse */
		new_pt2d->vu_p = vu_p;
		VMOVE(new_pt2d->coord, p->coord);
		BU_LIST_MAGIC_SET(&new_pt2d->l, NMG_PT2D_MAGIC);
		BU_LIST_APPEND(&p->l, &new_pt2d->l);
		return;
	}

	rt_bomb("Couldn't find mapped vertexuse of vertex!\n");
}

/** Support routine for
 * nmg_find_first_last_use_of_v_in_fu
 *
 * The object of the game here is to find uses of the given vertex whose
 * departing edges have the min/max dot product with the direction vector.
 *
 */
static void
pick_edges(struct vertex *v, struct vertexuse **vu_first, int *min_dir, struct vertexuse **vu_last, int *max_dir, struct faceuse *fu, const struct bn_tol *tol, fastf_t *dir)


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
	if (rt_g.NMG_debug & DEBUG_TRI)
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
			rt_bomb("vertexuse does not acknoledge parents\n");

		if (nmg_find_fu_of_vu(vu) != fu ||
		    *vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
			if (rt_g.NMG_debug & DEBUG_TRI)
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

		if (rt_g.NMG_debug & DEBUG_TRI)
			bu_log("\t\tchecking forward edgeuse to %g %g %g\n",
				V3ARGS(vu_next->v_p->vg_p->coord) );

		if (eu_length_sq >= SMALL_FASTF) {
			if ((vu_dot = VDOT(eu_dir, dir)) > dot_max) {
				if (rt_g.NMG_debug & DEBUG_TRI) {
					bu_log("\t\t\teu_dir %g %g %g\n",
						V3ARGS(eu_dir));

					bu_log("\t\t\tnew_last/max 0x%08x %g %g %g -> %g %g %g vdot %g\n",
						vu,
						V3ARGS(vu->v_p->vg_p->coord),
						V3ARGS(vu_next->v_p->vg_p->coord),
						vu_dot);
				}
				dot_max = vu_dot;
				*vu_last = vu;
				*max_dir = 1;
			}
			if (vu_dot < dot_min) {
				if (rt_g.NMG_debug & DEBUG_TRI) {
					bu_log("\t\t\teu_dir %g %g %g\n", V3ARGS(eu_dir));
					bu_log("\t\t\tnew_first/min 0x%08x %g %g %g -> %g %g %g vdot %g\n",
						vu,
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

		if (rt_g.NMG_debug & DEBUG_TRI)
			bu_log("\t\tchecking reverse edgeuse to %g %g %g\n",
				V3ARGS(vu_prev->v_p->vg_p->coord) );

		if (eu_length_sq >= SMALL_FASTF) {
			if ((vu_dot = VDOT(eu_dir, dir)) > dot_max) {
				if (rt_g.NMG_debug & DEBUG_TRI) {
					bu_log("\t\t\t-eu_dir %g %g %g\n",
						V3ARGS(eu_dir));
					bu_log("\t\t\tnew_last/max 0x%08x %g %g %g <- %g %g %g vdot %g\n",
						vu,
						V3ARGS(vu->v_p->vg_p->coord),
						V3ARGS(vu_prev->v_p->vg_p->coord),
						vu_dot);
				}
				dot_max = vu_dot;
				*vu_last = vu;
				*max_dir = -1;
			}
			if (vu_dot < dot_min) {
				if (rt_g.NMG_debug & DEBUG_TRI) {
					bu_log("\t\t\teu_dir %g %g %g\n", V3ARGS(eu_dir));
					bu_log("\t\t\tnew_first/min 0x%08x %g %g %g <- %g %g %g vdot %g\n",
						vu,
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

/** Support routine for
 * nmg_find_first_last_use_of_v_in_fu
 *
 * Given and edgeuse and a faceuse, pick the use of the edge in the faceuse
 * 	whose left vector has the largest/smallest dot product with the given
 *	direction vector.  The parameter "find_max" determines whether we
 *	return the edgeuse with the largest (find_max != 0) or the smallest
 *	(find_max == 0) left-dot-product.
 */
struct edgeuse *
pick_eu(struct edgeuse *eu_p, struct faceuse *fu, fastf_t *dir, int find_max)
{
	struct edgeuse *eu, *keep_eu=NULL, *eu_next;
	int go_radial_not_mate = 0;
	double dot_limit;
	double euleft_dot;
	vect_t left, eu_vect;

	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log("\t    pick_eu(%g %g %g  <-> %g %g %g, dir:%g %g %g,  %s)\n",
			V3ARGS(eu_p->vu_p->v_p->vg_p->coord ),
			V3ARGS(eu_p->eumate_p->vu_p->v_p->vg_p->coord ),
			V3ARGS(dir), (find_max==0?"find min":"find max") );

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
				rt_bomb("bombing");
			}

			euleft_dot = VDOT(left, dir);

			if (rt_g.NMG_debug & DEBUG_TRI)
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
					if (rt_g.NMG_debug & DEBUG_TRI)
						bu_log("\t\tnew max\n");
				}
			} else {
				if (euleft_dot < dot_limit) {
					dot_limit = euleft_dot;
					keep_eu = eu;
					if (rt_g.NMG_debug & DEBUG_TRI)
						bu_log("\t\tnew min\n");
				}
			}
		}

		if (go_radial_not_mate) eu = eu->eumate_p;
		else eu = eu->radial_p;
		go_radial_not_mate = ! go_radial_not_mate;

	} while ( eu != eu_p );

	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log("\t\tpick_eu() returns %g %g %g -> %g %g %g\n\t\t\tbecause vdot(left) = %g\n",
			V3ARGS(keep_eu->vu_p->v_p->vg_p->coord),
			V3ARGS(keep_eu->eumate_p->vu_p->v_p->vg_p->coord),
			dot_limit);

	return keep_eu;
}


/**
 *	Given a pointer to a vertexuse in a face and a ray, find the
 *	"first" and "last" uses of the vertex along the ray in the face.
 *	Consider the diagram below where 4 OT_SAME loopuses meet at a single
 *	vertex.  The ray enters from the upper left and proceeds to the lower
 *	right.  The ray encounters vertexuse (represented by "o" below)
 *	number 1 first and vertexuse 3 last.
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
 *	The primary purpose of this routine is to find the vertexuses
 *	that should be the parameters to nmg_cut_loop() and nmg_join_loop().
 */
void
nmg_find_first_last_use_of_v_in_fu(struct vertex *v, struct vertexuse **first_vu, struct vertexuse **last_vu, fastf_t *dir, struct faceuse *fu, const struct bn_tol *tol)
{
	struct vertexuse *vu_first, *vu_last;
	int max_dir, min_dir;	/* 1: forward -1 reverse */
	struct edgeuse *eu_first, *eu_last, *eu_p=NULL;

	NMG_CK_VERTEX(v);
	NMG_CK_FACEUSE(fu);
	if (first_vu == (struct vertexuse **)(NULL)) {
		bu_log("%s: %d first_vu is null ptr\n", __FILE__, __LINE__);
		rt_bomb("terminating\n");
	}
	if (last_vu == (struct vertexuse **)(NULL)) {
		bu_log("%s: %d last_vu is null ptr\n", __FILE__, __LINE__);
		rt_bomb("terminating\n");
	}

	VUNITIZE(dir);

	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log("\t  nmg_find_first(v:(%g %g %g) dir:(%g %g %g))\n",
			V3ARGS(v->vg_p->coord), V3ARGS(dir));

	/* go find the edges which are "closest" to the direction vector */
	pick_edges(v, &vu_first, &min_dir, &vu_last, &max_dir, fu, tol, dir);


	/* Now we know which 2 edges are most important to look at.
	 * The question now is which vertexuse on this edge to pick.
	 * For example, in the diagram above we will choose a use of edge C
	 * for our "max".  Either vu3 OR vu4 could be chosen.
	 *
	 * For our max/last point, we choose the use for which:
	 * 		vdot(ray, eu_left_vector)
	 * is largest.
	 *
	 * For our min/first point, we choose the use for which:
	 * 		vdot(ray, eu_left_vector)
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
		rt_bomb("bad max_dir\n");
		break;
	}

	NMG_CK_EDGEUSE(eu_p);
	NMG_CK_VERTEXUSE(eu_p->vu_p);

	eu_first = pick_eu(eu_p, fu, dir, 0);
	NMG_CK_EDGEUSE(eu_first);
	NMG_CK_VERTEXUSE(eu_first->vu_p);
	NMG_CK_VERTEX(eu_first->vu_p->v_p);
	NMG_CK_VERTEX_G(eu_first->vu_p->v_p->vg_p);

	if (rt_g.NMG_debug & DEBUG_TRI)
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
			rt_bomb("I can't find the right vertex\n");
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
		rt_bomb("bad min_dir\n");
		break;
	}

	NMG_CK_EDGEUSE(eu_p);
	NMG_CK_VERTEXUSE(eu_p->vu_p);

	eu_last = pick_eu(eu_p, fu, dir, 1);

	NMG_CK_EDGEUSE(eu_last);
	NMG_CK_VERTEXUSE(eu_last->vu_p);
	NMG_CK_VERTEX(eu_last->vu_p->v_p);
	if (rt_g.NMG_debug & DEBUG_TRI)
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
			rt_bomb("I can't find the right vertex\n");
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

	if (rt_g.NMG_debug & DEBUG_TRI)
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

	/* form direction vector for the cut we want to make */
	VSUB2(dir, cut_vu2->v_p->vg_p->coord,
		   cut_vu1->v_p->vg_p->coord);

	if (rt_g.NMG_debug & DEBUG_TRI)
		VPRINT("\t\tdir", dir);

	if ( ! (fu = nmg_find_fu_of_vu(cut_vu1)) ) {
		bu_log("%s: %d no faceuse parent of vu\n", __FILE__, __LINE__);
		rt_bomb("Bye now\n");
	}

	nmg_find_first_last_use_of_v_in_fu((*p1)->vu_p->v_p,
		&junk_vu, &cut_vu1, dir, fu, tol);

	NMG_CK_VERTEXUSE(junk_vu);
	NMG_CK_VERTEXUSE(cut_vu1);
	*p1 = find_pt2d(tbl2d, cut_vu1);

	if (rt_g.NMG_debug & DEBUG_TRI) {
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
	if (rt_g.NMG_debug & DEBUG_TRI) {
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
 *  Cut a loop which has a 2D mapping.  Since this entails the creation
 *  of new vertexuses, it is necessary to add a 2D mapping for the new
 *  vertexuses.
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

	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log("\tcutting loop @ %g %g -> %g %g\n",
			p1->coord[X], p1->coord[Y],
			p2->coord[X], p2->coord[Y]);

	if (p1->vu_p->up.eu_p->up.lu_p != p2->vu_p->up.eu_p->up.lu_p) {
		bu_log("parent loops are not the same %s %d\n", __FILE__, __LINE__);
		rt_bomb("cut_mapped_loop() goodnight 1\n");
	}

	pick_pt2d_for_cutjoin(tbl2d, &p1, &p2, tol);

	if (p1->vu_p->up.eu_p->up.lu_p != p2->vu_p->up.eu_p->up.lu_p) {
		if (void_ok) {
			if(rt_g.NMG_debug)
				bu_log("cut_mapped_loop() parent loops are not the same %s %d, trying join\n", __FILE__, __LINE__);
			join_mapped_loops(tbl2d, p1, p2, color, tol);
			return (struct pt2d *)NULL;
		} else {
			char buf[80];
			char name[32];
			static int iter=0;
			vect_t cut_vect, cut_start, cut_end;
			FILE *fd;

			bu_log("parent loops are not the same %s %d\n",
				__FILE__, __LINE__);


			sprintf(buf, "cut %g %g %g -> %g %g %g\n",
				V3ARGS(p1->vu_p->v_p->vg_p->coord),
				V3ARGS(p2->vu_p->v_p->vg_p->coord) );

			sprintf(name, "bad_tri_cut%d.g", iter++);
			if ((fd=fopen("bad_tri_cut.pl", "w")) == (FILE *)NULL)
				rt_bomb("cut_mapped_loop() goodnight 2\n");

			VSUB2(cut_vect, p2->vu_p->v_p->vg_p->coord, p1->vu_p->v_p->vg_p->coord);
			/* vector goes past end point by 50% */
			VJOIN1(cut_end, p2->vu_p->v_p->vg_p->coord, 0.5, cut_vect);
			/* vector starts before start point by 25% */
			VJOIN1(cut_start, p1->vu_p->v_p->vg_p->coord, -0.25, cut_vect);

			pl_color(fd, 100, 100, 100);
			pdv_3line(fd, cut_start, p1->vu_p->v_p->vg_p->coord);
			pl_color(fd, 255, 255, 255);
			pdv_3line(fd, p1->vu_p->v_p->vg_p->coord, p2->vu_p->v_p->vg_p->coord);
			pl_color(fd, 100, 100, 100);
			pdv_3line(fd, p2->vu_p->v_p->vg_p->coord, cut_end);

			(void)fclose(fd);
			nmg_stash_model_to_file( "bad_tri_cut.g",
						 nmg_find_model(&p1->vu_p->l.magic), buf );

			rt_bomb("cut_mapped_loop() goodnight 2\n");
		}
	}

	if (plot_fd) {
		pl_color(plot_fd, V3ARGS(color) );
		pdv_3line(plot_fd, p1->coord, p2->coord);
	}

	old_lu = p1->vu_p->up.eu_p->up.lu_p;
	NMG_CK_LOOPUSE(old_lu);
	new_lu = nmg_cut_loop(p1->vu_p, p2->vu_p);
	NMG_CK_LOOPUSE(new_lu);
	NMG_CK_LOOP(new_lu->l_p);
	nmg_loop_g(new_lu->l_p, tol);

	/* XXX Does anyone care about loopuse orientations at this stage?
	nmg_lu_reorient( old_lu );
	nmg_lu_reorient( new_lu );
	 */

	/* get the edgeuse of the new vertexuse we just created */
	eu = BU_LIST_PPREV_CIRC(edgeuse, &new_lu->down_hd);
	NMG_CK_EDGEUSE(eu);

	/* if the original vertexuses had normals,
	 * copy them to the new vertexuses.
	 */
	if( p1->vu_p->a.magic_p && *p1->vu_p->a.magic_p == NMG_VERTEXUSE_A_PLANE_MAGIC )
	{
		struct vertexuse *vu;
		struct faceuse *fu;
		struct loopuse *lu;
		vect_t ot_same_normal;
		vect_t ot_opposite_normal;

		/* get vertexuse normal */
		VMOVE( ot_same_normal , p1->vu_p->a.plane_p->N );
		fu = nmg_find_fu_of_vu( p1->vu_p );
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_OPPOSITE )
			VREVERSE( ot_same_normal , ot_same_normal )

		VREVERSE( ot_opposite_normal , ot_same_normal );

		/* look for new vertexuses in new_lu and old_lu */
		for( BU_LIST_FOR( vu , vertexuse , &p1->vu_p->v_p->vu_hd ) )
		{
			if( vu->a.magic_p )
				continue;

			lu = nmg_find_lu_of_vu( vu );
			if( lu != old_lu && lu != old_lu->lumate_p &&
			    lu != new_lu && lu != new_lu->lumate_p )
				continue;

			/* assign appropriate normal */
			fu = nmg_find_fu_of_vu( vu );
			if( fu->orientation == OT_SAME )
				nmg_vertexuse_nv( vu , ot_same_normal );
			else if( fu->orientation == OT_OPPOSITE )
				nmg_vertexuse_nv( vu , ot_opposite_normal );
		}
	}
	if( p2->vu_p->a.magic_p && *p2->vu_p->a.magic_p == NMG_VERTEXUSE_A_PLANE_MAGIC )
	{
		struct vertexuse *vu;
		struct faceuse *fu;
		struct loopuse *lu;
		vect_t ot_same_normal;
		vect_t ot_opposite_normal;

		/* get vertexuse normal */
		VMOVE( ot_same_normal , p2->vu_p->a.plane_p->N );
		fu = nmg_find_fu_of_vu( p2->vu_p );
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_OPPOSITE )
			VREVERSE( ot_same_normal , ot_same_normal )

		VREVERSE( ot_opposite_normal , ot_same_normal );

		/* look for new vertexuses in new_lu and old_lu */
		for( BU_LIST_FOR( vu , vertexuse , &p2->vu_p->v_p->vu_hd ) )
		{
			if( vu->a.magic_p )
				continue;

			lu = nmg_find_lu_of_vu( vu );
			if( lu != old_lu && lu != old_lu->lumate_p &&
			    lu != new_lu && lu != new_lu->lumate_p )
				continue;

			/* assign appropriate normal */
			fu = nmg_find_fu_of_vu( vu );
			if( fu->orientation == OT_SAME )
				nmg_vertexuse_nv( vu , ot_same_normal );
			else if( fu->orientation == OT_OPPOSITE )
				nmg_vertexuse_nv( vu , ot_opposite_normal );
		}
	}

	/* map it to the 2D plane */
	map_new_vertexuse(tbl2d, eu->vu_p);

	/* now map the vertexuse on the radially-adjacent edgeuse */
	NMG_CK_EDGEUSE(eu->radial_p);
	map_new_vertexuse(tbl2d, eu->radial_p->vu_p);

	eu = BU_LIST_PPREV_CIRC( edgeuse, &(p1->vu_p->up.eu_p->l));
	return find_pt2d(tbl2d, eu->vu_p);
}

/**
 *
 *	Join 2 loops (one forms a hole in the other usually )
 *
 */
static void
join_mapped_loops(struct bu_list *tbl2d, struct pt2d *p1, struct pt2d *p2, const int *color, const struct bn_tol *tol)
{
	struct vertexuse *vu1, *vu2;
	struct vertexuse *vu;
	struct edgeuse *eu;
	struct loopuse *lu;

	NMG_CK_TBL2D(tbl2d);
	NMG_CK_PT2D(p1);
	NMG_CK_PT2D(p2);
	BN_CK_TOL(tol);

	vu1 = p1->vu_p;
	vu2 = p2->vu_p;

	NMG_CK_VERTEXUSE(vu1);
	NMG_CK_VERTEXUSE(vu2);

	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log("join_mapped_loops()\n");


	if (p1 == p2) {
		bu_log("%s %d: Attempting to join loop to itself at (%g %g %g)?\n",
			__FILE__, __LINE__,
			V3ARGS(p1->vu_p->v_p->vg_p->coord) );
		rt_bomb("bombing\n");
	} else if (p1->vu_p->up.eu_p->up.lu_p == p2->vu_p->up.eu_p->up.lu_p) {
		bu_log("parent loops are the same %s %d\n", __FILE__, __LINE__);
		rt_bomb("goodnight\n");
	}
	/* check to see if we're joining two loops that share a vertex */
	if (p1->vu_p->v_p == p2->vu_p->v_p) {
#if 1
		if (rt_g.NMG_debug & DEBUG_TRI)
			bu_log("Joining two loops that share a vertex at (%g %g %g)\n",
				V3ARGS(p1->vu_p->v_p->vg_p->coord) );
		(void)nmg_join_2loops(p1->vu_p,  p2->vu_p);
#else
		if (rt_g.NMG_debug & DEBUG_TRI)
			bu_log("NOT Joining two loops that share a vertex at (%g %g %g)\n",
				V3ARGS(p1->vu_p->v_p->vg_p->coord) );
#endif
		return;
	}

	pick_pt2d_for_cutjoin(tbl2d, &p1, &p2, tol);

	vu1 = p1->vu_p;
	vu2 = p2->vu_p;
	NMG_CK_VERTEXUSE(vu1);
	NMG_CK_VERTEXUSE(vu2);

	if (p1 == p2) {
		bu_log("%s: %d I'm a fool...\n\ttrying to join a vertexuse (%g %g %g) to itself\n",
			__FILE__, __LINE__,
			V3ARGS(p1->vu_p->v_p->vg_p->coord) );
	} else if (p1->vu_p->up.eu_p->up.lu_p == p2->vu_p->up.eu_p->up.lu_p) {
		if (rt_g.NMG_debug & DEBUG_TRI) {
			bu_log("parent loops are the same %s %d\n",
				__FILE__, __LINE__);
		}
		(void)cut_mapped_loop(tbl2d, p1, p2, color, tol, 1);
		return;
	}


	/* XXX nmg_join_2loops() requires that the two loops not be BOTH
	 *	OT_OPPOSITE.  We should check for this here.
	 *
	 * XXX what if vu2 is a vertex loop?
	 */

	NMG_CK_EDGEUSE(vu2->up.eu_p);

	/* need to save this so we can use it later to get
	 * the new "next" edge/vertexuse
	 */
	eu = BU_LIST_PPREV_CIRC(edgeuse, vu2->up.eu_p);


	if (rt_g.NMG_debug & DEBUG_TRI) {
		struct edgeuse *pr1_eu;
		struct edgeuse *pr2_eu;

		pr1_eu = BU_LIST_PNEXT_CIRC(edgeuse, vu1->up.eu_p);
		pr2_eu = BU_LIST_PNEXT_CIRC(edgeuse, vu2->up.eu_p);

		bu_log("joining loops between:\n\t%g %g %g -> (%g %g %g)\n\tand%g %g %g -> (%g %g %g)\n",
			V3ARGS(vu1->v_p->vg_p->coord),
			V3ARGS(pr1_eu->vu_p->v_p->vg_p->coord),
			V3ARGS(vu2->v_p->vg_p->coord),
			V3ARGS(pr2_eu->vu_p->v_p->vg_p->coord) );
	}

	vu = nmg_join_2loops(vu1, vu2);
	if (plot_fd) {
		pl_color(plot_fd, V3ARGS(color) );
		pdv_3line(plot_fd, p1->coord,  p2->coord);
	}


	NMG_CK_VERTEXUSE(vu);

	if (vu == vu2)
		return;

	/* since we've just made some new vertexuses
	 * we need to map them to the 2D plane.
	 *
	 * XXX This should be made more direct and efficient.  For now we
	 * just go looking for vertexuses without a mapping.
	 */
	NMG_CK_EDGEUSE(vu->up.eu_p);
	NMG_CK_LOOPUSE(vu->up.eu_p->up.lu_p);
	lu = vu->up.eu_p->up.lu_p;
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		if (! find_pt2d(tbl2d, eu->vu_p))
			map_new_vertexuse(tbl2d, eu->vu_p);
	}
}
/**
 *  Check to see if the edge between the top/bottom of the trapezoid
 * already exists.
 */
static int
skip_cut(struct bu_list *tbl2d, struct pt2d *top, struct pt2d *bot)
{
	struct vertexuse *vu_top;
	struct vertexuse *vu_bot;
	struct vertexuse *vu;
	struct vertex *v;
	struct faceuse *fu;
	struct edgeuse *eu;
	struct edgeuse *eu_next;
	struct pt2d *top_next, *bot_next;

	NMG_CK_TBL2D(tbl2d);
	NMG_CK_PT2D(top);
	NMG_CK_PT2D(bot);


	top_next = PT2D_NEXT(tbl2d, top);
	bot_next = PT2D_NEXT(tbl2d, bot);

	if (top_next == bot || bot_next == top) {
		return 1;
	}

	vu_top = top->vu_p;
	vu_bot = bot->vu_p;
	NMG_CK_VERTEXUSE(vu_top);
	NMG_CK_VERTEXUSE(vu_bot);

	v = vu_top->v_p;
	NMG_CK_VERTEX(v);
	NMG_CK_VERTEX(vu_bot->v_p);

	fu = nmg_find_fu_of_vu(vu_top);

	for (BU_LIST_FOR(vu, vertexuse, &v->vu_hd)) {
		/* if parent is edge of this loop/face, and next
		 * vertex around loop is the one for vu_bot, don't
		 * make the cut.
		 */
		if (nmg_find_fu_of_vu(vu) != fu) continue;
		if (!vu->up.magic_p) {
			bu_log("NULL vertexuse up %s %d\n",
				__FILE__, __LINE__);
			rt_bomb("");
		}
		if (*vu->up.magic_p != NMG_EDGEUSE_MAGIC) continue;
		eu = vu->up.eu_p;
		eu_next = BU_LIST_PNEXT_CIRC(edgeuse, eu);

		/* if the edge exists, don't try to re-cut it */
		if (eu_next->vu_p->v_p == vu_bot->v_p)
			return 1;
	}

	fu = nmg_find_fu_of_vu(vu_bot);
	v = vu_bot->v_p;
	for (BU_LIST_FOR(vu, vertexuse, &v->vu_hd)) {
		/* if parent is edge of this loop/face, and next
		 * vertex around loop is the one for vu_bot, don't
		 * make the cut.
		 */
		if (nmg_find_fu_of_vu(vu) != fu) continue;
		if (!vu->up.magic_p) {
			bu_log("NULL vertexuse up %s %d\n",
				__FILE__, __LINE__);
			rt_bomb("");
		}
		if (*vu->up.magic_p != NMG_EDGEUSE_MAGIC) continue;
		eu = vu->up.eu_p;
		eu_next = BU_LIST_PNEXT_CIRC(edgeuse, eu);

		/* if the edge exists, don't try to re-cut it */
		if (eu_next->vu_p->v_p == vu_top->v_p)
			return 1;
	}
	return 0;
}

static void
cut_diagonals(struct bu_list *tbl2d, struct bu_list *tlist, const struct faceuse *fu, const struct bn_tol *tol)
{
	struct trap *tp;
	int cut_count=0;

	static const int cut_color[3] = {255, 80, 80};
	static const int join_color[3] = {80, 80, 255};

	extern struct loopuse *nmg_find_lu_of_vu(const struct vertexuse *vu);
	struct loopuse *toplu, *botlu;
	struct loopuse *lu;

	NMG_CK_TBL2D(tbl2d);
	BN_CK_TOL(tol);

	/* Convert trap list to unimonotone polygons */
	for (BU_LIST_FOR(tp, trap, tlist)) {
		/* if top and bottom points are not on same edge of
		 * trapezoid, we cut across the trapezoid with a new edge.
		 */

		/* If the edge already exists in the face, don't bother
		 * to add it.
		 */
		if( !tp->top || !tp->bot )
		{
			bu_log( "tp->top and/or tp->bot is/are NULL!!!!!!!\n" );
			if (rt_g.NMG_debug & DEBUG_TRI)
			{
				nmg_pr_fu_briefly( fu, "" );
				if( tp->top )
					bu_log( "tp->top is (%g %g %g)\n", V3ARGS( tp->top->vu_p->v_p->vg_p->coord ) );
				if( tp->bot )
					bu_log( "tp->bot is (%g %g %g)\n", V3ARGS( tp->bot->vu_p->v_p->vg_p->coord ) );
			}
			rt_bomb( "tp->top and/or tp->bot is/are NULL" );
		}
		if (nmg_find_eu_in_face(tp->top->vu_p->v_p, tp->bot->vu_p->v_p, fu,
		    (struct edgeuse *)NULL, 0) != (struct edgeuse *)NULL) {
			if (rt_g.NMG_debug & DEBUG_TRI)
				bu_log("skipping %g %g/%g %g ... edge exists\n",
					tp->top->coord[X],
					tp->top->coord[Y],
					tp->bot->coord[X],
					tp->bot->coord[Y]);
			continue;
		}


		if (skip_cut(tbl2d, tp->top, tp->bot)) {
			if (rt_g.NMG_debug & DEBUG_TRI)
				bu_log("skipping %g %g/%g %g ... pts on same edge\n",
					tp->top->coord[X],
					tp->top->coord[Y],
					tp->bot->coord[X],
					tp->bot->coord[Y]);
			continue;
		}

		if (rt_g.NMG_debug & DEBUG_TRI) {
			bu_log("trying to cut ...\n");
			print_trap(tp, tbl2d);
		}

		/* top/bottom points are not on same side of trapezoid. */

		toplu = nmg_find_lu_of_vu(tp->top->vu_p);
		botlu = nmg_find_lu_of_vu(tp->bot->vu_p);
		NMG_CK_VERTEXUSE(tp->top->vu_p);
		NMG_CK_VERTEXUSE(tp->bot->vu_p);
		NMG_CK_LOOPUSE(toplu);
		NMG_CK_LOOPUSE(botlu);

		if (toplu == botlu){

			/* if points are the same, this is a split-loop op */
			if (tp->top->vu_p->v_p == tp->bot->vu_p->v_p) {

				int touching_jaunt=0;
				struct edgeuse *eu1, *eu2, *eu1_prev, *eu2_prev;

				eu1 = tp->top->vu_p->up.eu_p;
				eu2 = tp->bot->vu_p->up.eu_p;

				eu1_prev = BU_LIST_PPREV_CIRC( edgeuse, &eu1->l );
				eu2_prev = BU_LIST_PPREV_CIRC( edgeuse, &eu2->l );
				if( NMG_ARE_EUS_ADJACENT( eu1, eu1_prev ) )
					touching_jaunt = 1;
				else if( NMG_ARE_EUS_ADJACENT( eu2, eu2_prev ) )
					touching_jaunt = 1;

				if( touching_jaunt )
				{
					if (rt_g.NMG_debug & DEBUG_TRI)
						bu_log("splitting self-touching jaunt loop at (%g %g %g)\n",
						V3ARGS(tp->bot->vu_p->v_p->vg_p->coord) );

					nmg_loop_split_at_touching_jaunt(toplu, tol);
				}
				else
				{
					if (rt_g.NMG_debug & DEBUG_TRI)
						bu_log("splitting self-touching loop at (%g %g %g)\n",
						V3ARGS(tp->bot->vu_p->v_p->vg_p->coord) );

					nmg_split_touchingloops(toplu, tol);
				}
				for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))
					nmg_lu_reorient(lu);

				if (rt_g.NMG_debug & DEBUG_TRI)
				{
					char fname[32];

					sprintf( fname, "split%d.g", cut_count );
					nmg_stash_model_to_file( fname,
						nmg_find_model( &toplu->l.magic ),
						"after split_touching_loop()" );
					cut_count++;
				}

			} else {

				/* points are in same loop.  Cut the loop */

				(void)cut_mapped_loop(tbl2d, tp->top,
					tp->bot, cut_color, tol, 1);

				if (rt_g.NMG_debug & DEBUG_TRI)
				{
					char fname[32];

					sprintf( fname, "cut%d.g", cut_count );
					nmg_stash_model_to_file( fname,
						nmg_find_model( &toplu->l.magic ),
						"after cut_mapped_loop()" );
					cut_count++;
				}

			}

#if 0
			/* if the bottom vertexuse is on a rising edge and
			 * is a top vertex of another trapezoid then
			 * 	replace the occurrance of the old bottom
			 *	vertexuse with the new one in trapezoid "top"
			 *	locations.
			 */
			bot_next = PT2D_NEXT(tbl2d, tp->bot );

			if ( P_LT_V( tp->bot, bot_next ) ) {
				register struct pt2d *new_pt;
				struct trap *trp;

				/* find the new vertexuse of this vertex */
				new_pt = PT2D_PREV(tbl2d, tp->top);

				/* replace all "top" uses of tp->bot with
				 * new_pt
				 */
				for (BU_LIST_FOR(trp, trap, tlist)) {
					if (trp->top == tp->bot) {
						trp->top = new_pt;
					}
				}

				/* clean up old trapezoid so that top/bot
				 * are in same loop
				 */
				tp->top = PT2D_PREV( tbl2d, tp->bot );
			}
#endif
		} else {

			/* points are in different loops, join the
			 * loops together.
			 */

			if (toplu->orientation == OT_OPPOSITE &&
				botlu->orientation == OT_OPPOSITE)
					rt_bomb("trying to join 2 interior loops in triangulator?\n");

			if (rt_g.NMG_debug & DEBUG_TRI)
				bu_log("joining 2 loops @ %g %g -> %g %g\n",
					tp->top->coord[X],
					tp->top->coord[Y],
					tp->bot->coord[X],
					tp->bot->coord[Y]);

			join_mapped_loops(tbl2d, tp->top, tp->bot, join_color, tol);
			NMG_CK_LOOPUSE(toplu);

			if (rt_g.NMG_debug & DEBUG_TRI)
			{
				char fname[32];

				sprintf( fname, "join%d.g", cut_count );
				nmg_stash_model_to_file( fname,
					nmg_find_model( &toplu->l.magic ),
					"after join_mapped_loop()" );
				cut_count++;
			}

		}

		if (rt_g.NMG_debug & DEBUG_TRI) {
			nmg_tri_plfu( nmg_find_fu_of_vu(tp->top->vu_p),  tbl2d );
			print_tlist(tbl2d, tlist);
		}
	}

}


/**	C U T _ U N I M O N O T O N E
 *
 *	Given a unimonotone loopuse, triangulate it into multiple loopuses
 */
static void
cut_unimonotone(struct bu_list *tbl2d, struct bu_list *tlist, struct loopuse *lu, const struct bn_tol *tol)
{
	struct pt2d *min, *max, *new, *first=NULL, *prev, *next, *current;
	struct edgeuse *eu;
	int verts=0;
	int vert_count_sq;	/* XXXXX Hack for catching infinite loop */
	int loop_count=0;	/* See above */
	static const int cut_color[3] = { 90, 255, 90};

	NMG_CK_TBL2D(tbl2d);
	BN_CK_TOL(tol);
	NMG_CK_LOOPUSE(lu);

	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log("cutting unimonotone:\n");

	min = max = (struct pt2d *)NULL;

	/* find min/max points & count vertex points */
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		new = find_pt2d(tbl2d, eu->vu_p);
		if (!new) {
			bu_log("why can't I find a 2D point for %g %g %g?\n",
			V3ARGS(eu->vu_p->v_p->vg_p->coord));
			rt_bomb("bombing\n");
		}

		if (rt_g.NMG_debug & DEBUG_TRI)
			bu_log("%g %g\n", new->coord[X], new->coord[Y]);

		verts++;

		if (!min || P_LT_V(new, min))
			min = new;
		if (!max || P_GT_V(new, max))
			max = new;
	}
	vert_count_sq = verts * verts;

	/* pick the pt which does NOT have the other as a "next" pt in loop
	 * as the place from which we start marching around the uni-monotone
	 */
	if (PT2D_NEXT(tbl2d, max) == min)
		first = min;
	else if (PT2D_NEXT(tbl2d, min) == max)
		first = max;
	else {
		bu_log("is this a unimonotone loop of just 2 points?:\t%g %g %g and %g %g %g?\n",
			V3ARGS(min->vu_p->v_p->vg_p->coord),
			V3ARGS(max->vu_p->v_p->vg_p->coord) );

		rt_bomb("aborting\n");
	}

	/* */
	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log("%d verts in unimonotone, Min: %g %g  Max: %g %g first:%g %g 0x%08x\n", verts,
			min->coord[X], min->coord[Y],
			max->coord[X], max->coord[Y],
			first->coord[X], first->coord[Y], first);

	current = PT2D_NEXT(tbl2d, first);

	while (verts > 3) {

		loop_count++;
		if( loop_count > vert_count_sq )
		{
			bu_log( "Cut_unimontone is in an infinite loop!!!\n" );
			rt_bomb( "Cut_unimontone is in an infinite loop" );
		}

		prev = PT2D_PREV(tbl2d, current);
		next = PT2D_NEXT(tbl2d, current);

		if (rt_g.NMG_debug & DEBUG_TRI)
			bu_log("%g %g -> %g %g -> %g %g ...\n",
				prev->coord[X],
				prev->coord[Y],
				current->coord[X],
				current->coord[Y],
				next->coord[X],
				next->coord[Y]);

		if (is_convex(prev, current, next, tol)) {
			struct pt2d *t;
			/* cut a triangular piece off of the loop to
			 * create a new loop.
			 */
			NMG_CK_LOOPUSE(lu);
			if (rt_g.NMG_debug & DEBUG_TRI)
			{
				bu_log( "Before cut loop:\n" );
				nmg_pr_fu_briefly( lu->up.fu_p, "" );
			}
			current = cut_mapped_loop(tbl2d, next, prev, cut_color, tol, 0);
			if (rt_g.NMG_debug & DEBUG_TRI)
			{
				bu_log( "After cut loop:\n" );
				nmg_pr_fu_briefly( lu->up.fu_p, "" );
			}
			verts--;
			NMG_CK_LOOPUSE(lu);

			if (rt_g.NMG_debug & DEBUG_TRI)
				nmg_tri_plfu( lu->up.fu_p, tbl2d );

			if (current->vu_p->v_p == first->vu_p->v_p) {
				t = PT2D_NEXT(tbl2d, first);
				if (rt_g.NMG_debug & DEBUG_TRI)
					bu_log("\tfirst(0x%08x -> %g %g\n", first, t->coord[X], t->coord[Y]);
				t = PT2D_NEXT(tbl2d, current);

				if (rt_g.NMG_debug & DEBUG_TRI)
					bu_log("\tcurrent(0x%08x) -> %g %g\n", current, t->coord[X], t->coord[Y]);

				current = PT2D_NEXT(tbl2d, current);
				if (rt_g.NMG_debug & DEBUG_TRI)
					bu_log("\tcurrent(0x%08x) -> %g %g\n", current, t->coord[X], t->coord[Y]);
			}
		} else {
			if (rt_g.NMG_debug & DEBUG_TRI)
				bu_log("\tConcave, moving ahead\n");
			current = next;
		}
	}
}


static void
nmg_plot_flat_face(struct faceuse *fu, struct bu_list *tbl2d)
{
	struct loopuse *lu;
	struct edgeuse *eu;
	char buf[80];
	vect_t pt;
	struct pt2d *p, *pn;

	NMG_CK_TBL2D(tbl2d);
	NMG_CK_FACEUSE(fu);

	if (!plot_fd && (plot_fd = fopen("triplot.pl", "w")) == (FILE *)NULL) {
		bu_log( "cannot open triplot.pl\n");
	}

	pl_erase(plot_fd);
	pd_3space(plot_fd,
		fu->f_p->min_pt[0]-1.0,
		fu->f_p->min_pt[1]-1.0,
		fu->f_p->min_pt[2]-1.0,
		fu->f_p->max_pt[0]+1.0,
		fu->f_p->max_pt[1]+1.0,
		fu->f_p->max_pt[2]+1.0);

	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC) {
			register struct vertexuse *vu;

			vu = BU_LIST_FIRST( vertexuse, &lu->down_hd );
			if (rt_g.NMG_debug & DEBUG_TRI)
				bu_log( "lone vert @ %g %g %g\n",
					V3ARGS(vu->v_p->vg_p->coord) );

			pl_color(plot_fd, 200, 200, 100);

			if (! (p=find_pt2d(tbl2d, vu)) )
				rt_bomb("didn't find vertexuse in list!\n");

			pdv_3point(plot_fd, p->coord);
			sprintf(buf, "%g, %g", p->coord[0], p->coord[1]);
			pl_label(plot_fd, buf);

			continue;
		}

		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd ) ) {
			register struct edgeuse *eu_pnext;

			eu_pnext = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);

#if 0
			if (rt_g.NMG_debug & DEBUG_TRI)
				bu_log( "eu vert @ %g %g %g\n",
					V3ARGS(eu->vu_p->v_p->vg_p->coord) );

#endif
			if (! (p=find_pt2d(tbl2d, eu->vu_p)) )
				rt_bomb("didn't find vertexuse in list!\n");

			if (! (pn=find_pt2d(tbl2d, eu_pnext->vu_p)) )
				rt_bomb("didn't find vertexuse in list!\n");


			VSUB2(pt, pn->coord, p->coord);

			VSCALE(pt, pt, 0.80);
			VADD2(pt, p->coord, pt);

			pl_color(plot_fd, 200, 200, 200);
			pdv_3line(plot_fd, p->coord, pt);
			pd_3move(plot_fd, V3ARGS(p->coord));

			sprintf(buf, "%g, %g", p->coord[0], p->coord[1]);
			pl_label(plot_fd, buf);
		}
	}
}


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

	BN_CK_TOL(tol);
	NMG_CK_FACEUSE(fu);

	if (rt_g.NMG_debug & DEBUG_TRI) {
		NMG_GET_FU_NORMAL(N, fu);
		bu_log("---------------- Triangulate face %g %g %g\n",
			V3ARGS(N));
	}


	/* make a quick check to see if we need to bother or not */
	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (lu->orientation != OT_SAME) {
			if (rt_g.NMG_debug & DEBUG_TRI)
				bu_log("faceuse has non-OT_SAME orientation loop\n");
			goto triangulate;
		}
		vert_count = 0;
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd ))
			if (++vert_count > 3) {
				if (rt_g.NMG_debug & DEBUG_TRI)
					bu_log("loop has more than 3 verticies\n");
				goto triangulate;
			}
	}

	if (rt_g.NMG_debug & DEBUG_TRI) {
		bu_log("---------------- face %g %g %g already triangular\n",
			V3ARGS(N));

		for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))
			for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd ))
				VPRINT("pt", eu->vu_p->v_p->vg_p->coord );

	}
	return;

triangulate:
	if (rt_g.NMG_debug & DEBUG_TRI) {
		vect_t N;
		NMG_GET_FU_NORMAL(N, fu);
		bu_log("---------------- proceeding to triangulate face %g %g %g\n", V3ARGS(N));
	}


	/* convert 3D face to face in the X-Y plane */
	tbl2d = nmg_flatten_face(fu, TformMat);

	/* avoid having a hole start as the first point */
	{
		struct pt2d *pt1, *pt2;

		pt1 = BU_LIST_FIRST( pt2d, tbl2d );
		pt2 = BU_LIST_PNEXT( pt2d, &pt1->l );

		if( vtype2d( pt1, tbl2d, tol ) == HOLE_START &&
		    pt1->vu_p->v_p == pt2->vu_p->v_p )
		{
			/* swap first and second points */
			if (rt_g.NMG_debug & DEBUG_TRI)
				bu_log( "Swapping first two points on vertex list (first one was a HOLE_START)\n" );

			BU_LIST_DEQUEUE( &pt1->l );
			BU_LIST_APPEND( &pt2->l, &pt1->l );
		}
	}

	if (rt_g.NMG_debug & DEBUG_TRI) {
		struct pt2d *pt;
		bu_log( "Face Flattened\n");
		bu_log( "Vertex list:\n");
		for (BU_LIST_FOR(pt, pt2d, tbl2d)) {
			bu_log("\tpt2d %26.20e %26.20e\n", pt->coord[0], pt->coord[1]);
		}

		nmg_tri_plfu( fu, tbl2d );
		nmg_plot_flat_face(fu, tbl2d);
		bu_log( "Face plotted\n\tmaking trapezoids...\n");
	}


	BU_LIST_INIT(&tlist);
	nmg_trap_face(tbl2d, &tlist, tol);

	if (rt_g.NMG_debug & DEBUG_TRI){
		print_tlist(tbl2d, &tlist);

		bu_log("Cutting diagonals ----------\n");
	}
	cut_diagonals(tbl2d, &tlist, fu, tol);
	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log("Diagonals are cut ----------\n");

	if (rt_g.NMG_debug & DEBUG_TRI) {
		sprintf(db_name, "uni%d.g", iter);
		nmg_stash_model_to_file(db_name,
			nmg_find_model(&fu->s_p->l.magic),
			"trangles and unimonotones");
	}

	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))
		(void)nmg_loop_split_at_touching_jaunt(lu, tol);

	if (rt_g.NMG_debug & DEBUG_TRI) {
		sprintf(db_name, "uni_sj%d.g", iter);
		nmg_stash_model_to_file(db_name,
			nmg_find_model(&fu->s_p->l.magic),
			"after split_at_touching_jaunt");
	}

	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))
		nmg_split_touchingloops( lu, tol );

	if (rt_g.NMG_debug & DEBUG_TRI) {
		sprintf(db_name, "uni_split%d.g", iter++);
		nmg_stash_model_to_file(db_name,
			nmg_find_model(&fu->s_p->l.magic),
			"split trangles and unimonotones");
	}

	/* now we're left with a face that has some triangle loops and some
	 * uni-monotone loops.  Find the uni-monotone loops and triangulate.
	 */
	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {

		if (BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC) {
			vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);

			bu_log("How did I miss this vertex loop %g %g %g?\n%s\n",
				V3ARGS(vu->v_p->vg_p->coord),
				"I'm supposed to be dealing with unimonotone loops now");
			rt_bomb("aborting\n");

		} else if (BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_EDGEUSE_MAGIC) {
			vert_count = 0;
			for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd )) {
				if (++vert_count > 3) {
					cut_unimonotone(tbl2d, &tlist, lu, tol);

					if (rt_g.NMG_debug & DEBUG_TRI) {
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

	if (rt_g.NMG_debug & DEBUG_TRI)
		nmg_tri_plfu( fu, tbl2d );

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

void
nmg_triangulate_shell(struct shell *s, const struct bn_tol *tol)
{
	struct faceuse *fu;

	BN_CK_TOL(tol);
	NMG_CK_SHELL( s );

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		NMG_CK_FACEUSE(fu);
		if (fu->orientation == OT_SAME)
			nmg_triangulate_fu(fu, tol);
	}
}

void
nmg_triangulate_model(struct model *m, const struct bn_tol *tol)
{
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;

	BN_CK_TOL(tol);
	NMG_CK_MODEL(m);
	nmg_vmodel(m);

	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log("Triangulating NMG\n");

	(void)nmg_model_edge_g_fuse( m, tol );

	(void)nmg_unbreak_region_edges( &m->magic );

	for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
		NMG_CK_REGION(r);
		for (BU_LIST_FOR(s, shell, &r->s_hd)) {
			NMG_CK_SHELL(s);
			nmg_s_split_touchingloops(s, tol);

			for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
				NMG_CK_FACEUSE(fu);
				if (fu->orientation == OT_SAME)
					nmg_triangulate_fu(fu, tol);
			}
		}
	}
	nmg_vmodel(m);

	if (rt_g.NMG_debug & DEBUG_TRI)
		bu_log("Triangulation completed\n");
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
