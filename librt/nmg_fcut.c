/*
 *			N M G _ C O M B . C
 *
 *	Routines to combine loops
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "externs.h"
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

/*	J U S T G R A Z I N G
 *
 *	Find the relationship (grazing/crossing) of the edge/loop
 *	associated with a vertex with respect to a given plane.
 *
 *      vu is a vertexuse of an edge which is in turn a part of a loop.
 *      The vertex under vu lies on the plane "pl".  The edge whith which
 *      vu is associated may either approach the plane, or lie within the
 *      plane.  In either case the loop which this edge is a part of has
 *      verticies on the plane, and to at least one side of the plane.  If
 *      it only has verticies to one side, the loop is said to "graze" the
 *      plane (i.e. is tangential to the plane)
 *
 *	explicit Return:
 *		Zero		loop crosses plane at vu
 *		Non-Zero	loop does not cross plane at vu
 */
static int justgrazing(pl, vu, b)
plane_t pl;
struct vertexuse *vu;
struct nmg_ptbl *b;
{
	point_t pt;
	fastf_t next_class,	/* classification of nextward and lastward */
		last_class;	/* points not on given plane */
	struct edgeuse *eun, *eul;

	if (rt_g.NMG_debug & DEBUG_GRAZING)
		rt_log("justgrazing:\n");

	NMG_CK_VERTEXUSE(vu);
	if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )  {
		/* This is sometimes a loop of a single vertex */
		rt_log("justgrazing() vu->up is not edgeuse, (%s) skipping\n",
			rt_identify_magic(*vu->up.magic_p));
		return(1);
	}
	NMG_CK_EDGEUSE(vu->up.eu_p);


	if (rt_g.NMG_debug & DEBUG_GRAZING) {
		VPRINT("\tVertex", vu->v_p->vg_p->coord);
		PLPRINT("\tplane", pl);
	}

	/* We look at successively "next" edgeuses looking for one whose
	 * vertex does not lie on the given plane.
	 *
	 * First we skip over all the points which are "topologically" on 
	 * the given plane
	 */
	next_class = 0.0;
	eun = RT_LIST_PNEXT_CIRC(edgeuse, &vu->up.eu_p->l);
	while (nmg_in_or_ref(eun->vu_p, b) && eun != vu->up.eu_p) {
		eun = RT_LIST_PNEXT_CIRC(edgeuse, &eun->l);
	}

	if (eun == vu->up.eu_p) return (1);

	/* Just to be sure, we do some geometry checking to make sure the
	 * point we end up with is NOT on the plane
	 */
	VMOVE(pt, eun->vu_p->v_p->vg_p->coord);
	next_class = NMG_DIST_PT_PLANE(pt, pl);
	while (NEAR_ZERO(next_class, VDIVIDE_TOL) && eun != vu->up.eu_p) {
		eun = RT_LIST_PNEXT_CIRC(edgeuse, &eun->l);

		VMOVE(pt, eun->vu_p->v_p->vg_p->coord);
		next_class = NMG_DIST_PT_PLANE(pt, pl);
	}


	if (rt_g.NMG_debug & DEBUG_GRAZING) {
		VPRINT("\tNext_pt", pt);
		rt_log("\t\tdistance: %g\n", next_class);
	}

	if (eun == vu->up.eu_p) return (1);

	/* We look at successively "last" edgeuses looking for one whose
	 * vertex does not lie on the given plane.
	 */
	eul = RT_LIST_PLAST_CIRC(edgeuse, &vu->up.eu_p->l);
	NMG_CK_EDGEUSE(eul);
	last_class = 0.0;
	while (nmg_in_or_ref(eul->vu_p, b) && eul != vu->up.eu_p)
		eul = RT_LIST_PLAST_CIRC(edgeuse, eul);

	if (eul == vu->up.eu_p) return (1);
	VMOVE(pt, eul->vu_p->v_p->vg_p->coord);
	last_class = NMG_DIST_PT_PLANE(pt, pl);
	while (NEAR_ZERO(last_class, VDIVIDE_TOL) && eul != vu->up.eu_p) {
		eul = RT_LIST_PLAST_CIRC(edgeuse, eul);
		VMOVE(pt, eul->vu_p->v_p->vg_p->coord);
		last_class = NMG_DIST_PT_PLANE(pt, pl);
	}


	if (rt_g.NMG_debug & DEBUG_GRAZING) {
		VPRINT("\tLast_pt", pt);
		rt_log("\t\tdistance: %g\n", last_class);

		rt_log("grazecheck Next dist:%g Last dist:%g\n",
			next_class, last_class);
	}


	if (last_class <= 0.0 && next_class <= 0.0 ||
	    last_class >= 0.0 && next_class >= 0.0) {

	    	if (rt_g.NMG_debug & DEBUG_GRAZING)
		    	rt_log("\tgrazing\n");

		return(1);
	}

    	if (rt_g.NMG_debug & DEBUG_GRAZING)
	    	rt_log("\tnot grazing\n");

	return(0);
}


/*	S K I P P T S
 *
 *	Skip points which are sole children of loopuses.
 *	this is a support routine for the routine "combine"
 */
static int skippts(i, b, plane_eq)
int i;
struct nmg_ptbl *b;
plane_t plane_eq;
{
	union {
		struct vertexuse **v;
		long **l;
	} vu;
	struct edgeuse *eu_forw, *eu_back;

	vu.l = b->buffer;

	for (; i < b->end ; ++i)
		if (*vu.v[i]->up.magic_p == NMG_LOOPUSE_MAGIC ) {

			/* mark loopuse for later deletion */
			vu.v[i]->up.lu_p->lumate_p->orientation =
				vu.v[i]->up.lu_p->orientation = OT_BOOLPLACE;

			if (rt_g.NMG_debug & DEBUG_COMBINE)
				rt_log("\tBoolplace'ing: #%d (%g, %g, %g)\n", i,
					vu.v[i]->v_p->vg_p->coord[0],
					vu.v[i]->v_p->vg_p->coord[1],
					vu.v[i]->v_p->vg_p->coord[2]);

			continue;
		}else if (*vu.v[i]->up.magic_p == NMG_SHELL_MAGIC) {
			rt_log("in %s at %d splitting edge of shell??\n",
				__FILE__, __LINE__);
			rt_bomb("BAD NMG pointer");
		} else if (*vu.v[i]->up.magic_p == NMG_EDGEUSE_MAGIC) {
			/* if we are just grazing a single point here
			 * we can skip this point.  Otherwise, this is
			 * our terminal condition.
			 */

			/* if the verticies of the last and next edgeuse
			 * are not in the list, and we are grazing at this
			 * point, we can skip this point. Example:
			 *
			 *    *---------
			 *     \
			 *  __*_\
			 *  __*_>\
			 *	 /
			 *	/
			 *     /
			 *    *---------
			 */
			eu_forw = RT_LIST_PNEXT_CIRC(edgeuse,
				&vu.v[i]->up.eu_p->l);

			eu_back = RT_LIST_PNEXT_CIRC(edgeuse,
				&vu.v[i]->up.eu_p->l);

			if (nmg_in_or_ref(eu_back->vu_p, b) || 
			    nmg_in_or_ref(eu_forw->vu_p, b) ||
			    ! justgrazing(plane_eq, vu.v[i], b) )
				break;

			if (rt_g.NMG_debug & DEBUG_COMBINE)
				VPRINT("Skippt: skipping \"graze\" edgevert",
					vu.v[i]->v_p->vg_p->coord);
		} else
			rt_bomb("Bogus vertexuse parent");
	return(i);
}

/*	N I T Z		(another wonderful function name! ;-) )
 *
 *	This function divides a loop between a pair of vertexuses/edgeuse
 *	causing the loop to become two loops with a co-incident edge.
 */
static void nitz(i, b, vu, plane_eq)
int *i;
struct nmg_ptbl *b;
struct vertexuse **vu;
plane_t plane_eq;
{				
	int j = *i + 1;
	register struct edgeuse *eu_next,
				*eu_last,
				*eu;

	if (rt_g.NMG_debug & DEBUG_COMBINE)
		rt_log("nitz\n");

	eu = vu[*i]->up.eu_p;
	eu_next = RT_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	eu_last = RT_LIST_PLAST_CIRC(edgeuse, &eu->l);

	if (vu[j] == eu_next->vu_p 
	    || vu[j] == eu_last->vu_p
	    || vu[j] == eu->eumate_p->vu_p 
	    || vu[j] == eu_last->eumate_p->vu_p) {
		/* both vertices are a part of the
		 * same linear edge, so we just move forward
		 * along the edge.
		 */
		if (rt_g.NMG_debug & DEBUG_COMBINE)
	    		rt_log("\tBoth verts are part of same linear edge\n");
	    	*i = j;
		return;
	}

	/* classify this loop intersect as
	 * either grazing or cutting accross the plane of intersection
	 */
	if (justgrazing(plane_eq, vu[*i], b)) {
		/* We're just grazing this loop.
		 * so we skip to next point
		 */

		if (rt_g.NMG_debug & DEBUG_COMBINE)
			rt_log("\t a graze\n");

		*i = j;
	} else {
		/* We're cutting accross this
		 * loop
		 * we must divide it in twain
		 * from vu[*i] to vu[j]
		 */
		nmg_cut_loop(vu[*i], vu[j]);
		*i = j + 1;
	}
}
/*	T R Y _ S W I T C H
 *
 * check to see if a little HARMLESS re-ordering of the vertex list 
 * will allow us to do a "simple loop division" instead of a more complex
 * loop-join or a stupid loopouse-point insert
 */
static int try_switch(i, j, lim, vul)
int i, j, lim;
struct vertexuse **vul;
{
	int next;
	struct vertexuse *tvu;
	pointp_t p1;

	/* while not past end of list && coords still ~= */
	p1 = vul[j]->v_p->vg_p->coord;

	next = j + 1;

#if 1
	while (next < lim && vul[next]->v_p == vul[j]->v_p) {
#else
	while (next < lim && (vul[next]->v_p == vul[j]->v_p ||
	    VAPPROXEQUAL(p1, vul[next]->v_p->vg_p->coord, tol)) {
#endif
		/* if next vertexuse is under edgeuse of same loop */
		if (*vul[next]->up.magic_p == NMG_EDGEUSE_MAGIC &&
		    vul[next]->up.eu_p->up.magic_p ==
		    vul[i]->up.eu_p->up.magic_p) {

		    	/* switch the vertexuses in the list "vul" */
			tvu = vul[i+1];
			vul[i+1] = vul[next];
			vul[next] = tvu;
			return(1);
		    }
		++next;
	}
	return(0);
}

/*	A B S O R B _ P T
 *
 *	We're going to deal with the case where we link a lone vertex
 *	(a vertexuse of a loopuse) into a face/loop/edge.  This is to support
 *	the "combine" function.
 *
 *	Explicit Return:	indication of what next point in combine
 *				proceedure should be.
 *
 *		1	new first point is first edge point past vu[i+1]
 *		0	new first point is vu[i+1]
 */
static int absorb_pt(i, b, vu, fu2)
int *i;
struct nmg_ptbl *b;
struct vertexuse **vu;
struct faceuse *fu2;
{
	plane_t pl;
	struct edgeuse *eu;
	struct edgeuse	*eu_next;
	struct edgeuse	*eu_last;
	int j = *i + 1;

	VMOVEN(pl, fu2->f_p->fg_p->N, 4);
	/* check to see if a little harmless
	 * re-ordering of the vertex list would turn
	 * this into a "simple loop division"
	 */
	if (try_switch(*i, j, b->end, vu)) {
		if (*vu[*i]->up.magic_p == NMG_LOOPUSE_MAGIC &&
			(struct vertexuse *)vu[*i]->up.lu_p->down_hd.forw != vu[*i])
			rt_bomb("I'm not taking this shit!\n");
		nitz(i, b, vu, pl);
		return(1);
	}

	if (rt_g.NMG_debug & DEBUG_COMBINE) {
		rt_log("absorb_pt\n");
		if (*vu[*i]->up.magic_p == NMG_LOOPUSE_MAGIC &&
			(struct vertexuse *)vu[*i]->up.lu_p->down_hd.forw != vu[*i])
			rt_bomb("I'm still not taking this shit!\n");
	}

	/* if point is along edge from p.vu[i]		.____,_____.
	 *	we split that edge with the point
	 *	and start again from vu[j]
	 * else if point is inside loop		      \
	 *	we insert a "jaunt" to that point	._.
	 *	and start again from vu[j]		|,|
	 *						!_!
	 * else	(point is outside loop)			   \
	 *	discard vu[j] and p.vu[i] as candidates
	 *     \
	 *  .___.	._.
	 *  |	|	| |
	 *  |	| ,	| |
	 *  |	!__.____! |
	 *  |	    \	  |
	 *  !________.____!	
	 *	      \
	 *
	 * Note: Comma is vu[j], Period/Exclamation-base is other (vu[i]?)
	 *	vertex
	 *
	 *
	 * We determine if vu[j] is along one of the edges from vu[i] by
	 * looking for the other endpoints of the edges in the intersection
	 * list.  If one of these points come after vu[j] then we know that
	 * the edge runs along the plane of intersection
	 */
	eu = vu[*i]->up.eu_p;
	eu_next = RT_LIST_PNEXT_CIRC(edgeuse, eu);
	eu_last = RT_LIST_PLAST_CIRC(edgeuse, eu);

	if (nmg_tbl(b, TBL_LOC, &eu_next->vu_p->l.magic) >= j ) {
		/* splitting in the "next" direction */
		(void)nmg_esplit(vu[j]->v_p, eu->e_p);

		/* replace the loop vertex in the list
		 * with the one we just created.
		 */
		nmg_klu(vu[j]->up.lu_p);
		eu_next = RT_LIST_PNEXT_CIRC(edgeuse, eu);
		vu[j] = eu_next->vu_p;
	}
	else if (nmg_tbl(b, TBL_LOC, &eu_last->vu_p->l.magic) >= j) {
		/* splitting in the "last" direction */
		(void)nmg_esplit(vu[j]->v_p, eu_last->e_p);

		/* replace the loop vertex in the list
		 * with the one we just created. 
		 */
		nmg_klu(vu[j]->up.lu_p);
		eu_last = RT_LIST_PLAST_CIRC(edgeuse, eu);
		vu[j] = eu_last->vu_p;
	}
	else if (!justgrazing(pl, vu[*i], b)) {
		/* point does NOT lie along an existing edge, but DOES lie on
		 * the interior of the loop, so we will
		 * have to build a "jaunt" or "spike" into the edge of the
		 * current loop.
		 */
		if (((struct vertexuse *)(vu[j]->up.lu_p->down_hd.forw)) != vu[j])
			rt_bomb("vertexuse has lost parent 1\n");

		eu = nmg_eins(eu);		  /* insert 0 length edge */

		eu = nmg_eusplit(vu[j]->v_p, eu); /* split it to new point */

		eu_last = RT_LIST_PLAST_CIRC(edgeuse, eu);
		/* make uses share edge */
		nmg_moveeu(eu, eu_last);

		nmg_klu(vu[j]->up.lu_p);
		vu[j] = eu->vu_p;
	} else {
		/* point lies on exterior of loop */
		*i = j;
		return(1);
	}

	return(0);
}

/*	C O M B _ L O O P S
 *
 *	Two loops (presumably an interior and an exterior) are being joined
 *	along a (new) edge that runs between them.
 *
 *	This routine and the routine to join loops of the same face which
 *	are the only uses of a common edge should be coded and tested at the
 *	same time.
 */
static int comb_loops(plane_eq, vu1, vu2, b)
plane_t plane_eq;
struct vertexuse *vu1, *vu2;
struct nmg_ptbl *b;
{
	/* if ( We are cutting accross both loops )
	 *	join loops at intersection points
	 *	start next line at j+1
	 * else ( cutting first and tangenting second)
	 *	1) join loops, start next line at j
	 */


	if (rt_g.NMG_debug & DEBUG_COMBINE)
		rt_log("comb_loops\n");

	/* if we're just grazing the first loop, we don't
	 * really want to join the loops together. 
	 */
	if (justgrazing(plane_eq, vu1, b)) return(0);
	else if (justgrazing(plane_eq, vu2, b)) {
		/* we're crossing one loop, but grazing the inside one.
		 * We join them together & continue from the second point
		 * onward.
		 */
		rt_bomb("Can't do this yet");
		return(0);
	} else {
		/* we're crossing both loops.  We join them together and
		 * then continue from the point AFTER the second point.
		 */
		rt_bomb("Can't do this yet.  Shouldn't need to");
		return(1);
	}
}

/*	C H E C K _ V E R T L I N K
 *
 *	The two verticies starting at *i_p in the table share the same vertex.
 *	pick which one we should ignore.
 */
static void check_vertlink(i_p, b)
int *i_p;
struct nmg_ptbl *b;
{
	int i, j, k;
	struct loopuse *lu_i, *lu_j;
	struct loopuse *lu_k = (struct loopuse *)0;
	struct edgeuse *eu_next;
	struct vertexuse *tvu;
	struct edgeuse	*eu_jnext;
	struct edgeuse	*eu_inext;
	union {
		long **l;
		struct vertexuse **v;
	} vul;

	if (rt_g.NMG_debug & DEBUG_COMBINE)
		rt_log("check_vertlink (%d)\n", *i_p);

	vul.l = b->buffer;
	j = (i = *i_p) +1;

	eu_jnext = RT_LIST_PNEXT_CIRC(edgeuse, &vul.v[j]->up.eu_p->l);
	eu_inext = RT_LIST_PNEXT_CIRC(edgeuse, &vul.v[i]->up.eu_p->l);

	/* if the "j" vertexuse is for edgeuse and the vertexuse at the
	 * other end of the edgeuse is in the list "after" j, then we toss
	 * out the "i" vertexuse and go onward
	 */
	if (*vul.v[j]->up.magic_p == NMG_EDGEUSE_MAGIC &&
	    nmg_tbl(b, TBL_LOC, (long *)eu_jnext->vu_p) > j) {
		/* ignore i */
		if (rt_g.NMG_debug & DEBUG_COMBINE)
			rt_log("\tignore i (%d) for j (%d)\n", i, j);

	    	/* if "i" was a "temporary" loopuse, take care of
	    	 * tagging it as "not-in-use"
	    	 */
	    	if (*vul.v[i]->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    vul.v[i]->up.lu_p->orientation == OT_UNSPEC) {
			lu_i = vul.v[i]->up.lu_p;
			lu_i->orientation = 
			lu_i->lumate_p->orientation = OT_BOOLPLACE;

		    	if (rt_g.NMG_debug & DEBUG_COMBINE)
				rt_log("\tBoolplace'ing: #%d (%g, %g, %g)\n", i,
					vul.v[i]->v_p->vg_p->coord[0],
					vul.v[i]->v_p->vg_p->coord[1],
					vul.v[i]->v_p->vg_p->coord[2]);
	    	}
	    		
	    	*i_p = j;
	}

	/* if the "i" vertexuse is for edgeuse and the vertexuse at the
	 * other end of the edgeuse is in the list "after" j, then we toss
	 * out the "j" vertexuse and go onward
	 */
	if (*vul.v[i]->up.magic_p == NMG_EDGEUSE_MAGIC &&
	    nmg_tbl(b, TBL_LOC, (long *)eu_inext->vu_p) > j) {
		/* switch i & j to ignore j */
		if (rt_g.NMG_debug & DEBUG_COMBINE)
			rt_log("\tswitch i(%d) & j(%d) to ignore j\n", i, j);
		tvu = vul.v[j];
		vul.v[j] = vul.v[i];
		vul.v[i] = tvu;

	    	/* if this was a "temporary" loopuse, take care of
	    	 * tagging it as "not-in-use"
	    	 */
	    	if (*vul.v[i]->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    vul.v[i]->up.lu_p->orientation == OT_UNSPEC) {
			lu_i = vul.v[i]->up.lu_p;
			lu_i->orientation = 
			lu_i->lumate_p->orientation = OT_BOOLPLACE;

		    	if (rt_g.NMG_debug & DEBUG_COMBINE)
				rt_log("\tBoolplace'ing: #%d (%g, %g, %g)\n", i,
					vul.v[i]->v_p->vg_p->coord[0],
					vul.v[i]->v_p->vg_p->coord[1],
					vul.v[i]->v_p->vg_p->coord[2]);
	    	}


		*i_p = j;

	} else {
		/* neither edgeuse has the "next" vertexuse in the list of
		 * points on the intersection line
		 */

		lu_i = nmg_lu_of_vu(vul.v[i]);
		lu_j = nmg_lu_of_vu(vul.v[j]);

		/* look forward for a vertex of either loop */
		for (k = j; k < b->end ; ++k) {
		    	lu_k = nmg_lu_of_vu(vul.v[k]);
			if (lu_k == lu_i || lu_k == lu_j) break;
			else if (lu_k != lu_i && lu_k != lu_j &&
			    RT_LIST_FIRST_MAGIC(&lu_k->down_hd) != NMG_VERTEXUSE_MAGIC) {
				rt_bomb("ought to be restarting edge joins on new loop\n");
			}
	    	}

		/* if there's another vertex in the loop of "i" before there's
		 *  another vertex in the loop of "j", we use the "i"
		 * vertexuse, and skip the "j" vertexuse
		 */
		if (lu_k == lu_i) {
			if (rt_g.NMG_debug & DEBUG_COMBINE)
				rt_log("\tuse to i's (%d) loop (discard j:%d)", i, j);
			tvu = vul.v[j];
			vul.v[j] = vul.v[i];
			vul.v[i] = tvu;

		    	/* if this was a "temporary" loopuse, take care of
		    	 * tagging it as "not-in-use"
	    		 */
		    	if (*vul.v[i]->up.magic_p == NMG_LOOPUSE_MAGIC &&
			    vul.v[i]->up.lu_p->orientation == OT_UNSPEC) {
				lu_i = vul.v[i]->up.lu_p;
				lu_i->orientation = 
				lu_i->lumate_p->orientation = OT_BOOLPLACE;
			    	if (rt_g.NMG_debug & DEBUG_COMBINE)
					rt_log("\tBoolplace'ing: #%d (%g, %g, %g)\n", i,
						vul.v[i]->v_p->vg_p->coord[0],
						vul.v[i]->v_p->vg_p->coord[1],
						vul.v[i]->v_p->vg_p->coord[2]);
		    	}

			*i_p = j;

		} else if (lu_k == lu_j) {
			if (rt_g.NMG_debug & DEBUG_COMBINE)
				rt_log("\t(discard i:%d)use to j's (%d)loop\n", i, j);

		    	/* if this was a "temporary" loopuse, take care of
		    	 * tagging it as "not-in-use"
	    		 */
		    	if (*vul.v[i]->up.magic_p == NMG_LOOPUSE_MAGIC &&
			    vul.v[i]->up.lu_p->orientation == OT_UNSPEC) {
				lu_i = vul.v[i]->up.lu_p;
				lu_i->orientation = 
				lu_i->lumate_p->orientation = OT_BOOLPLACE;
			    	if (rt_g.NMG_debug & DEBUG_COMBINE)
					rt_log("\tBoolplace'ing: #%d (%g, %g, %g)\n", i,
						vul.v[i]->v_p->vg_p->coord[0],
						vul.v[i]->v_p->vg_p->coord[1],
						vul.v[i]->v_p->vg_p->coord[2]);
		    	}

			*i_p = j;

		} else
			/* neither the "i" nor "j" loops have another vertex
			 * in the list after "j"
			 */
			rt_bomb("I don't know how to doooooo this\n");
	}

	return;
}

/*
 *	T B L _ V S O R T
 *	sort list of vertices in fu1 on plane of fu2 
 *
 *	W A R N I N G:
 *		This function makes gross assumptions about the contents
 *	and structure of an nmg_ptbl list!  If I could figure a way of folding
 *	this into the nmg_tbl routine, I would do it.
 */
static void tbl_vsort(b, fu1, fu2, pt, dir)
struct nmg_ptbl *b;		/* table of vertexuses on intercept line */
struct faceuse	*fu1, *fu2;
point_t		pt;
vect_t		dir;
{
	union {
		struct vertexuse **vu;
		long **magic_p;
	} p;
	fastf_t	*mag;
	register int i, j;

	mag = (fastf_t *)rt_calloc(b->end, sizeof(fastf_t),
					"vector magnitudes for sort");

	p.magic_p = b->buffer;
	/* check vertexuses and compute distance from start of line */
	for(i = 0 ; i < b->end ; ++i) {
		vect_t		vect;
		NMG_CK_VERTEXUSE(p.vu[i]);

		VSUB2(vect, pt, p.vu[i]->v_p->vg_p->coord);
		mag[i] = VDOT( vect, dir );
	}

	/* a trashy bubble-head sort, because I hope this list is never
	 * very long.
	 */
	for(i=0 ; i < b->end - 1 ; ++i) {
		for (j=i+1; j < b->end ; ++j) {
			if (mag[i] > mag[j]) {
				register struct vertexuse *tvu;
				register fastf_t	tmag;

				tvu = p.vu[i];
				p.vu[i] = p.vu[j];
				p.vu[j] = tvu;

				tmag = mag[i];
				mag[i] = mag[j];
				mag[j] = tmag;
			}
		}
	}

	rt_free((char *)mag, "vector magnitudes");
}

/*
 *			N M G _ F A C E _ C O M B I N E
 *
 *	collapse loops,vertices within face fu1 (relative to fu2)
 *
 *	At this point, we've got faces with loops on a single vertex, and
 *	we've got loops which have had edges cut in two (with a new vertex
 *	inserted in the edge) and it is time to put edges between the new
 *	vertices, thus subdividing loops and combining single-vertex loops
 *	with loops containing edges.
 */
void nmg_face_combine(b, fu1, fu2, pt, dir)
struct nmg_ptbl *b;		/* table of vertexuses in fu1 on intercept line */
struct faceuse	*fu1;		/* Face being worked */
struct faceuse	*fu2;		/* fu2 here for plane equation */
point_t		pt;		/* line of intersection */
vect_t		dir;
{
	plane_t plane_eq;	/* the plane of the other face */
	int i;
	union {
		struct vertexuse **vu;
		long **magic_p;
	} p;
	pointp_t	p1, p2;
	long		*tab;
	FILE *fp;
	static int pn=0;
	static char name[32];
	int vn=0;

	if (rt_g.NMG_debug & DEBUG_COMBINE) {
		rt_log("nmg_face_combine\n");
		if (rt_g.NMG_debug & DEBUG_PLOTEM) {
			(void)sprintf(name, "comb%d.pl", ++pn);
			rt_log("plotting %s\n", name);
			if ((fp=fopen(name, "w")) == (FILE *)NULL) {
				(void)perror(name);
				exit(-1);
			}
			tab = (long *)rt_calloc( fu1->s_p->r_p->m_p->maxindex,
				sizeof(long),
				"nmg_face_combine flags[]" );
		/*	nmg_pl_fu(fp, fu1, &tbl, 200, 20, 200);
			nmg_pl_fu(fp, fu2, &tbl, 20, 200, 200); */
		}
	}

	/* Sort vertexuse list */
	tbl_vsort(b, fu1, fu2, pt, dir);

	/* validate vertexuse list, and maybe print some useful diagnostics
	 */
	VMOVEN(plane_eq, fu2->f_p->fg_p->N, 4);
	p.magic_p = b->buffer;
	for (i=0 ; i < b->end ; ++i) {
		register struct vertexuse	*vu;
		vu = p.vu[i];
		NMG_CK_VERTEXUSE(vu);
		NMG_CK_VERTEX(vu->v_p);
		NMG_CK_VERTEX_G(vu->v_p->vg_p);

		if (rt_g.NMG_debug & DEBUG_COMBINE) {
			rt_log("%d\t%g, %g, %g\t", i,
				vu->v_p->vg_p->coord[X],
				vu->v_p->vg_p->coord[Y],
				vu->v_p->vg_p->coord[Z]);
			if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC) {
				register struct vertex_g *tmpvg;

				NMG_CK_EDGEUSE(vu->up.eu_p->eumate_p);
				NMG_CK_VERTEXUSE(vu->up.eu_p->eumate_p->vu_p);
				NMG_CK_VERTEX(vu->up.eu_p->eumate_p->vu_p->v_p);

				tmpvg = vu->up.eu_p->eumate_p->vu_p->v_p->vg_p;

				NMG_CK_VERTEX_G(tmpvg);
				rt_log("EDGEUSE -> %g, %g, %g\n",
					tmpvg->coord[0],
					tmpvg->coord[1],
					tmpvg->coord[2]);

				nmg_pl_eu(fp, vu->up.eu_p, tab, 180, 180, 180);

			} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
				rt_log("LOOPUSE\n");
				nmg_pl_lu(fp, vu->up.lu_p, tab, 80, 180, 100);

				if ((struct vertexuse *)vu->up.lu_p->down_hd.forw !=
					vu) {
					rt_log("vertexuse's parent disowns us!\n");
						if (((struct vertexuse *)(vu->up.lu_p->lumate_p->down_hd.forw))->l.magic == NMG_VERTEXUSE_MAGIC)
							rt_bomb("lumate has vertexuse\n");
						else
							rt_bomb("lumate has garbage\n");
				}
			} else
				rt_log("UNKNOWN\n");
		}

	}

	if (rt_g.NMG_debug & DEBUG_COMBINE)  {
		rt_free( (char *)tab, "nmg_face_combine flags[]" );
		tab = (long *)0;		/* sanity */
		(void)fclose(fp);
	}

	/* We don't want to start an edge on a loop of one vertex
	 * becuase such a vertex will lie outside of the face.  Thus
	 * we go looking for the first vertex which is part of an
	 * edge-loop.		p.vu, b->end
	 */
	for (i=0 ; (i = skippts(i, b, plane_eq)) < b->end-1 ; ) {

		/* At this point we have a plausible first vertex from which
		 * to work.  Now we determine whether that
		 * vertex needs only to be inserted in an existing edge,
		 * or if we need to make a new edge and what that edge
		 * connects.
		 */
		for ( ; i < b->end-1 ; ++i) {

			if (rt_g.NMG_debug & DEBUG_COMBINE) {

			    p1 = p.vu[i]->v_p->vg_p->coord;
			    p2 = p.vu[i+1]->v_p->vg_p->coord;
			    rt_log("linking (%d) %g, %g, %g -> (%d) %g, %g, %g\n",
					i, p1[X], p1[Y], p1[Z],
					i+1, p2[X], p2[Y], p2[Z]);
			}
			if( rt_g.NMG_debug & (DEBUG_PLOTEM|DEBUG_PL_ANIM) )
				nmg_pl_comb_fu( pn, vn++, fu1 );

			if (p.vu[i]->v_p == p.vu[i+1]->v_p) {
				/* We may be cutting a face on an
				 * intersection line which is already
				 * the result of a previous plane
				 * intersection.
				 */
				check_vertlink(&i, b);
				if (rt_g.NMG_debug & DEBUG_COMBINE)
					rt_log("\tNew i: %d\n", i);
				if (i >= b->end-1) break;
			}


			if (*p.vu[i+1]->up.magic_p == NMG_LOOPUSE_MAGIC) {
				/* we're linking in a loopuse point
				 */
				if (absorb_pt(&i, b, p.vu, fu2))
					break;
			}
			else if (p.vu[i+1]->up.eu_p->up.magic_p ==
			    p.vu[i]->up.eu_p->up.magic_p) {
				/* the two edges (of the two vertexuses) are
				 * a part of the same parent (loop)
				 */
				nitz(&i, b, p.vu, plane_eq);
				break;
			} else {
				/* we're joining 2 different edge-loops
				 * together.
				 */

				/* check to see if a little harmless
				 * re-ordering of the vertex list would turn
				 * this into a "simple loop division"
				 */
				if (try_switch(i, i+1, b->end, p.vu)) {
					nitz(&i, b, p.vu, plane_eq);
					break;
				} else if (comb_loops(plane_eq, p.vu[i],
				    p.vu[i+1], b) ) {
					break;
				}
			}
		} /* for ( ; i < b->end-1 ; ++i) */
	} /* for all vertices in the list */

	if( rt_g.NMG_debug & (DEBUG_PLOTEM|DEBUG_PL_ANIM) )
		nmg_pl_comb_fu( pn, vn++, fu1 );

	nmg_face_bb(fu1->f_p);
}
                                                                                                                                      