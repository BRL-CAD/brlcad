/*
 *			N M G _ I N F O . C
 *
 *  A companion module to nmg_mod.c which contains routines to
 *  answer questions about n-Manifold Geometry data structures.
 *  None of these routines will modify the NMG structures in any way.
 *
 *  Authors -
 *	Michael John Muuss
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

/*
 *			N M G _ F I N D E U
 *
 *  Find an edgeuse in a shell between a given pair of vertex structs.
 *
 *  If a given shell "s" is specified, then only edgeuses in that shell
 *  will be considered, otherwise all edgeuses in the model are fair game.
 *
 *  If a particular edgeuse "eup" is specified, then that edgeuse
 *  and it's mate will not be returned as a match.
 *
 *  If "dangling_only" is true, then an edgeuse will be matched only if
 *  there are no other edgeuses on the edge, i.e. the radial edgeuse is
 *  the same as the mate edgeuse.
 *
 *  Returns -
 *	edgeuse*	Edgeuse which matches the criteria
 *	NULL		Unable to find matching edgeuse
 */
struct edgeuse *
nmg_findeu(v1, v2, s, eup, dangling_only)
struct vertex	*v1, *v2;
struct shell	*s;
struct edgeuse	*eup;
int		dangling_only;
{
	register struct vertexuse	*vu;
	register struct edgeuse		*eu;
	struct edgeuse			*eup_mate;
	int				eup_orientation;

	NMG_CK_VERTEX(v1);
	NMG_CK_VERTEX(v2);
	if(s) NMG_CK_SHELL(s);

	if(eup)  {
		NMG_CK_EDGEUSE(eup);
		eup_mate = eup->eumate_p;
		NMG_CK_EDGEUSE(eup_mate);
		if (*eup->up.magic_p != NMG_LOOPUSE_MAGIC ||
		    *eup->up.lu_p->up.magic_p != NMG_FACEUSE_MAGIC )
			rt_bomb("nmg_findeu(): eup not part of a face\n");
		eup_orientation = eup->up.lu_p->up.fu_p->orientation;
	} else {
		eup_mate = eup;			/* NULL */
		eup_orientation = OT_SAME;
	}

	if (rt_g.NMG_debug & DEBUG_FINDEU)
		rt_log("nmg_findeu() seeking eu!=%8x/%8x between (%8x, %8x) %s\n",
			eup, eup_mate, v1, v2,
			dangling_only ? "[dangling]" : "[any]" );

	for( RT_LIST_FOR( vu, vertexuse, &v1->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if (!vu->up.magic_p)
			rt_bomb("nmg_findeu() vertexuse in vu_hd list has null parent\n");

		/* Ignore self-loops and lone shell verts */
		if (*vu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
		eu = vu->up.eu_p;

		/* Ignore edgeuses which don't run between the right verts */
		if( eu->eumate_p->vu_p->v_p != v2 )  continue;

		if (rt_g.NMG_debug & DEBUG_FINDEU )  {
			rt_log("nmg_findeu: check eu=%8x vertex=(%8x, %8x)\n",
				eu, eu->vu_p->v_p,
				eu->eumate_p->vu_p->v_p);
		}

		/* Ignore the edgeuse to be excluded */
		if( eu == eup || eu->eumate_p == eup )  {
			if (rt_g.NMG_debug & DEBUG_FINDEU )
				rt_log("\tIgnoring -- excluded edgeuse\n");
			continue;
		}

		/* See if this edgeuse is in the proper shell */
		if( s )  {
			struct loopuse	*lu;
			if( *eu->up.magic_p == NMG_SHELL_MAGIC &&
			    eu->up.s_p != s )  {
			    	if (rt_g.NMG_debug & DEBUG_FINDEU)
			    		rt_log("\tIgnoring -- wire eu in wrong shell s=%x\n", eu->up.s_p);
				continue;
			}
			if( *eu->up.magic_p != NMG_LOOPUSE_MAGIC )
				rt_bomb("nmg_findeu() eu has bad up\n");
			lu = eu->up.lu_p;
			NMG_CK_LOOPUSE(lu);
			if( *lu->up.magic_p == NMG_SHELL_MAGIC &&
			    lu->up.s_p != s )  {
			    	if (rt_g.NMG_debug & DEBUG_FINDEU)
			    		rt_log("\tIgnoring -- eu of wire loop in wrong shell s=%x\n", lu->up.s_p);
				continue;
			}
			if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )
				rt_bomb("nmg_findeu() lu->up is bad\n");
			/* Edgeuse in loop in face, normal case */
			if( lu->up.fu_p->s_p != s )  {
			    	if (rt_g.NMG_debug & DEBUG_FINDEU)
		    		rt_log("\tIgnoring -- eu of lu+fu in wrong shell s=%x\n", lu->up.fu_p->s_p);
				continue;
			}
		}

		/* If it's not a dangling edge, skip on */
		if( dangling_only && eu->eumate_p != eu->radial_p) {
		    	if (rt_g.NMG_debug & DEBUG_FINDEU)  {
			    	rt_log("\tIgnoring %8x/%8x (radial=x%x)\n",
			    		eu, eu->eumate_p,
					eu->radial_p );
		    	}
			continue;
		}

	    	if (rt_g.NMG_debug & DEBUG_FINDEU)
		    	rt_log("\tFound %8x/%8x\n", eu, eu->eumate_p);

		if ( *eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
		     *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
		     eup_orientation != eu->up.lu_p->up.fu_p->orientation)
			eu = eu->eumate_p;	/* Take other orient */
		goto out;
	}
	eu = (struct edgeuse *)NULL;
out:
	if (rt_g.NMG_debug & DEBUG_FINDEU)
	    	rt_log("nmg_findeu() returns x%x\n", eu);

	return eu;
}

/*
 *			N M G _ F I N D _ V _ I N _ F A C E
 *
 *	Perform a topological search to
 *	determine if the given vertex is contained in the given faceuse.
 *	If it is, return a pointer to the vertexuse which was found in the
 *	faceuse.
 *
 *  Returns NULL if not found.
 */
struct vertexuse *
nmg_find_v_in_face(v, fu)
CONST struct vertex	*v;
CONST struct faceuse	*fu;
{
	struct vertexuse *vu;
	struct edgeuse *eu;
	struct loopuse *lu;

#define CKLU_FOR_FU(_lu, _fu, _vu) \
	if (*_lu->up.magic_p == NMG_FACEUSE_MAGIC && _lu->up.fu_p == _fu) \
		return(_vu)

	NMG_CK_VERTEX(v);

	for( RT_LIST_FOR( vu, vertexuse, &v->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC) {
			eu = vu->up.eu_p;
			if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
				lu = eu->up.lu_p;
				CKLU_FOR_FU(lu, fu, vu);
			}
		} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
			lu = vu->up.lu_p;
			CKLU_FOR_FU(lu, fu, vu);
		}
	}
	return((struct vertexuse *)NULL);
}

/*
 *			N M G _ F I N D _ V U _ I N _ F A C E
 *
 *	try to find a vertex(use) in a face wich appoximately matches the
 *	coordinates given.  
 *	
 *	This is a geometric search, not a topological one.
 */
struct vertexuse *
nmg_find_vu_in_face(pt, fu, tol)
CONST point_t		pt;
CONST struct faceuse	*fu;
CONST struct rt_tol	*tol;
{
	register struct loopuse	*lu;
	struct edgeuse		*eu;
	vect_t			delta;
	struct vertex		*v;
	register struct vertex_g *vg;
	int			magic1;

	NMG_CK_FACEUSE(fu);
	RT_CK_TOL(tol);

	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
		if (magic1 == NMG_VERTEXUSE_MAGIC) {
			struct vertexuse	*vu;
			vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
			v = vu->v_p;
			NMG_CK_VERTEX(v);
			if( !(vg = v->vg_p) )  continue;
			NMG_CK_VERTEX_G(vg);
			VSUB2(delta, vg->coord, pt);
			if ( MAGSQ(delta) < tol->dist_sq)
				return(vu);
		}
		else if (magic1 == NMG_EDGEUSE_MAGIC) {
			for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
				v = eu->vu_p->v_p;
				NMG_CK_VERTEX(v);
				if( !(vg = v->vg_p) )  continue;
				NMG_CK_VERTEX_G(vg);
				VSUB2(delta, vg->coord, pt);
				if ( MAGSQ(delta) < tol->dist_sq)
					return(eu->vu_p);
			}
		} else
			rt_bomb("nmg_find_vu_in_face() Bogus child of loop\n");
	}
	return ((struct vertexuse *)NULL);
}

/*
 *			N M G _ F I N D _ V E R T E X _ I N _ E D G E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_find_vertex_in_edgelist( v, hd )
register CONST struct vertex	*v;
CONST struct rt_list		*hd;
{
	register CONST struct edgeuse	*eu;

	NMG_CK_VERTEX(v);
	for( RT_LIST_FOR( eu, edgeuse, hd ) )  {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_VERTEXUSE(eu->vu_p);
		NMG_CK_VERTEX(eu->vu_p->v_p);
		if( eu->vu_p->v_p == v )  return(1);
	}
	return(0);
}

/*
 *			N M G _ F I N D _ V E R T E X _ I N _ L O O P L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_find_vertex_in_looplist( v, hd, singletons )
register CONST struct vertex	*v;
CONST struct rt_list		*hd;
int				singletons;
{
	register CONST struct loopuse	*lu;
	long			magic1;

	NMG_CK_VERTEX(v);
	for( RT_LIST_FOR( lu, loopuse, hd ) )  {
		NMG_CK_LOOPUSE(lu);
		magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
		if( magic1 == NMG_VERTEXUSE_MAGIC )  {
			register CONST struct vertexuse	*vu;
			if( !singletons )  continue;
			vu = RT_LIST_FIRST(vertexuse, &lu->down_hd );
			NMG_CK_VERTEXUSE(vu);
			NMG_CK_VERTEX(vu->v_p);
			if( vu->v_p == v )  return(1);
		} else if( magic1 == NMG_EDGEUSE_MAGIC )  {
			if( nmg_find_vertex_in_edgelist( v, &lu->down_hd ) )
				return(1);
		} else {
			rt_bomb("nmg_find_vertex_in_loopuse() bad magic\n");
		}
	}
	return(0);
}

/*
 *			N M G _ F I N D _ V E R T E X _ I N _ F A C E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_find_vertex_in_facelist( v, hd )
register CONST struct vertex	*v;
CONST struct rt_list		*hd;
{
	register CONST struct faceuse	*fu;

	NMG_CK_VERTEX(v);
	for( RT_LIST_FOR( fu, faceuse, hd ) )  {
		NMG_CK_FACEUSE(fu);
		if( nmg_find_vertex_in_looplist( v, &fu->lu_hd, 1 ) )
			return(1);
	}
	return(0);
}

/*
 *			N M G _ F I N D _ E D G E _ I N _ E D G E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_find_edge_in_edgelist( e, hd )
CONST struct edge	*e;
CONST struct rt_list	*hd;
{
	register CONST struct edgeuse	*eu;

	NMG_CK_EDGE(e);
	for( RT_LIST_FOR( eu, edgeuse, hd ) )  {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_EDGE(eu->e_p);
		if( e == eu->e_p )  return(1);
	}
	return(0);
}

/*
 *			N M G _ F I N D _ E D G E _ I N _ L O O P L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_find_edge_in_looplist( e, hd )
CONST struct edge	*e;
CONST struct rt_list	*hd;
{
	register CONST struct loopuse	*lu;
	long			magic1;

	NMG_CK_EDGE(e);
	for( RT_LIST_FOR( lu, loopuse, hd ) )  {
		NMG_CK_LOOPUSE(lu);
		magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
		if( magic1 == NMG_VERTEXUSE_MAGIC )  {
			/* Loop of a single vertex does not have an edge */
			continue;
		} else if( magic1 == NMG_EDGEUSE_MAGIC )  {
			if( nmg_find_edge_in_edgelist( e, &lu->down_hd ) )
				return(1);
		} else {
			rt_bomb("nmg_find_edge_in_loopuse() bad magic\n");
		}
	}
	return(0);
}

/*
 *			N M G _ F I N D _ E D G E _ I N _ F A C E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_find_edge_in_facelist( e, hd )
CONST struct edge	*e;
CONST struct rt_list	*hd;
{
	register CONST struct faceuse	*fu;

	NMG_CK_EDGE(e);
	for( RT_LIST_FOR( fu, faceuse, hd ) )  {
		NMG_CK_FACEUSE(fu);
		if( nmg_find_edge_in_looplist( e, &fu->lu_hd ) )
			return(1);
	}
	return(0);
}

/*
 *			N M G _ F I N D _ L O O P _ I N _ F A C E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_find_loop_in_facelist( l, fu_hd )
CONST struct loop	*l;
CONST struct rt_list	*fu_hd;
{
	register CONST struct faceuse	*fu;
	register CONST struct loopuse	*lu;

	NMG_CK_LOOP(l);
	for( RT_LIST_FOR( fu, faceuse, fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
			NMG_CK_LOOPUSE(lu);
			NMG_CK_LOOP(lu->l_p);
			if( l == lu->l_p )  return(1);
		}
	}
	return(0);
}

/*
 *			N M G _ E U _ W I T H _ V U _ I N _ L U
 *
 *  Find an edgeuse starting at a given vertexuse within a loop(use).
 */
struct edgeuse *
nmg_eu_with_vu_in_lu( lu, vu )
CONST struct loopuse		*lu;
CONST struct vertexuse	*vu;
{
	register struct edgeuse	*eu;

	NMG_CK_LOOPUSE(lu);
	NMG_CK_VERTEXUSE(vu);
	if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
		rt_bomb("nmg_eu_with_vu_in_lu: loop has no edges!\n");
	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
		NMG_CK_EDGEUSE(eu);
		if( eu->vu_p == vu )  return eu;
	}
	rt_bomb("nmg_eu_with_vu_in_lu:  Unable to find vu!\n");
	/* NOTREACHED */
	return((struct edgeuse *)NULL);
}

/*
 *			N M G _ L O O P _ I S _ A _ C R A C K
 *
 *  Returns -
 *	 0	Loop is not a "crack"
 *	!0	Loop *is* a "crack"
 */
int
nmg_loop_is_a_crack( lu )
CONST struct loopuse	*lu;
{
	struct edgeuse	*cur_eu;
	struct edgeuse	*cur_eumate;
	struct vertexuse *cur_vu;
	struct vertex	*cur_v;
	struct vertexuse *next_vu;
	struct vertex	*next_v;
	struct faceuse	*fu;
	struct vertexuse *test_vu;
	struct edgeuse	*test_eu;
	struct loopuse	*test_lu;

	NMG_CK_LOOPUSE(lu);
	if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )  return 0;
	fu = lu->up.fu_p;
	NMG_CK_FACEUSE(fu);

	if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )  return 0;

	for( RT_LIST_FOR( cur_eu, edgeuse, &lu->down_hd ) )  {
		NMG_CK_EDGEUSE(cur_eu);
		cur_eumate = cur_eu->eumate_p;
		NMG_CK_EDGEUSE(cur_eumate);
		cur_vu = cur_eu->vu_p;
		NMG_CK_VERTEXUSE(cur_vu);
		cur_v = cur_vu->v_p;
		NMG_CK_VERTEX(cur_v);

		next_vu = cur_eumate->vu_p;
		NMG_CK_VERTEXUSE(next_vu);
		next_v = next_vu->v_p;
		NMG_CK_VERTEX(next_v);
		/* XXX It might be more efficient to walk the radial list */
		/* See if the next vertex has an edge pointing back to cur_v */
		for( RT_LIST_FOR( test_vu, vertexuse, &next_v->vu_hd ) )  {
			if( *test_vu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
			test_eu = test_vu->up.eu_p;
			NMG_CK_EDGEUSE(test_eu);
			if( test_eu == cur_eu )  continue;	/* skip self */
			if( test_eu == cur_eumate )  continue;	/* skip mates */
			if( *test_eu->up.magic_p != NMG_LOOPUSE_MAGIC )  continue;
			test_lu = test_eu->up.lu_p;
			if( test_lu != lu )  continue;
			/* Check departing edgeuse's NEXT vertex */
			if( test_eu->eumate_p->vu_p->v_p == cur_v )  goto match;
		}
		/* No path back, this can't be a crack, abort */
		return 0;
		
		/* One edgeuse matched, all the others have to as well */
match:		;
	}
	return 1;
}

/*
 *			N M G _ L O O P _ I S _ C C W
 *
 *  Returns -
 *	+1	Loop is CCW, should be exterior loop.
 *	-1	Loop is CW, should be interior loop.
 *	 0	Unable to tell, error.
 */
int
nmg_loop_is_ccw( lu, norm, tol )
CONST struct loopuse	*lu;
CONST plane_t		norm;
CONST struct rt_tol	*tol;
{
	vect_t		edge1, edge2;
	vect_t		left;
	struct edgeuse	*eu;
	struct edgeuse	*next_eu;
	struct vertexuse *this_vu, *next_vu, *third_vu;
	fastf_t		theta = 0;
	fastf_t		x,y;
	fastf_t		rad;

	NMG_CK_LOOPUSE(lu);
	RT_CK_TOL(tol);
	if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )  return 0;

	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
		next_eu = RT_LIST_PNEXT_CIRC( edgeuse, eu );
		this_vu = eu->vu_p;
		next_vu = eu->eumate_p->vu_p;
		third_vu = next_eu->eumate_p->vu_p;

		/* Skip topological 0-length edges */
		if( this_vu->v_p == next_vu->v_p )  continue;
		if( next_vu->v_p == third_vu->v_p )  continue;

		/* Skip edges with calculated edge lengths near 0 */
		VSUB2( edge1, next_vu->v_p->vg_p->coord, this_vu->v_p->vg_p->coord );
		if( MAGSQ(edge1) < tol->dist_sq )  continue;
		VSUB2( edge2, third_vu->v_p->vg_p->coord, next_vu->v_p->vg_p->coord );
		if( MAGSQ(edge2) < tol->dist_sq )  continue;
		VUNITIZE(edge1);
		VUNITIZE(edge2);

		/* Compute (loop)inward pointing "left" vector */
		VCROSS( left, norm, edge1 );
		y = VDOT( edge2, left );
		x = VDOT( edge2, edge1 );
		rad = atan2( y, x );
#if 0
		VPRINT("vu1", this_vu->v_p->vg_p->coord);
		VPRINT("vu2", next_vu->v_p->vg_p->coord);
		VPRINT("edge1", edge1);
		VPRINT("edge2", edge2);
		VPRINT("left", left);
		rt_log("atan2(%g,%g) = %g\n", y, x, rad);
#endif
		theta += rad;
	}
#if 0
	rt_log(" theta = %g (%g)\n", theta, theta / rt_twopi );
	nmg_face_lu_plot( lu, this_vu, this_vu );
	nmg_face_lu_plot( lu->lumate_p, this_vu, this_vu );
#endif

	rad = theta * rt_inv2pi;
	x = rad-1;
	/* Value is in radians, tolerance here is 1% */
	if( NEAR_ZERO( x, 0.05 ) )  {
		/* theta = two pi, loop is CCW */
		return 1;
	}
	x = rad + 1;
	if( NEAR_ZERO( x, 0.05 ) )  {
		/* theta = -two pi, loop is CW */
		return -1;
	}
	rt_log("nmg_loop_is_ccw(x%x):  unable to determine CW/CCW, theta=%g (%g)\n",
		theta, rad );
	return 0;
}

/*
 *			N M G _ F I N D _ P T _ I N _ S H E L L
 *
 *  Given a point in 3-space and a shell pointer, try to find a vertex
 *  anywhere in the shell which is within sqrt(tol_sq) distance of
 *  the given point.
 *
 *  The algorithm is a brute force, and should be used sparingly.
 *  Mike Gigante's NUgrid technique would work well here, if
 *  searching was going to be needed for more than a few points.
 *
 *  Returns -
 *	pointer to vertex with matching geometry
 *	NULL
 */
struct vertex *
nmg_find_pt_in_shell( s, pt, tol )
CONST struct shell	*s;
CONST point_t		pt;
CONST struct rt_tol	*tol;
{
	CONST struct faceuse	*fu;
	CONST struct loopuse	*lu;
	CONST struct edgeuse	*eu;
	CONST struct vertexuse	*vu;
	struct vertex		*v;
	CONST struct vertex_g	*vg;
	vect_t		delta;

	NMG_CK_SHELL(s);
	RT_CK_TOL(tol);

	fu = RT_LIST_FIRST(faceuse, &s->fu_hd);
	while (RT_LIST_NOT_HEAD(fu, &s->fu_hd) ) {
		/* Shell has faces */
		NMG_CK_FACEUSE(fu);
			if( (vu = nmg_find_vu_in_face( pt, fu, tol )) )
				return(vu->v_p);

			if (RT_LIST_PNEXT(faceuse, fu) == fu->fumate_p)
				fu = RT_LIST_PNEXT_PNEXT(faceuse, fu);
			else
				fu = RT_LIST_PNEXT(faceuse, fu);
	}

	for (RT_LIST_FOR(lu, loopuse, &s->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
			vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
			NMG_CK_VERTEX(vu->v_p);
			if (vg = vu->v_p->vg_p) {
				NMG_CK_VERTEX_G(vg);
				VSUB2( delta, vg->coord, pt );
				if( MAGSQ(delta) < tol->dist_sq )
					return(vu->v_p);
			}
		} else if (RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
			rt_log("in %s at %d ", __FILE__, __LINE__);
			rt_bomb("loopuse has bad child\n");
		} else {
			/* loopuse made of edgeuses */
			for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				
				NMG_CK_EDGEUSE(eu);
				NMG_CK_VERTEXUSE(eu->vu_p);
				v = eu->vu_p->v_p;
				NMG_CK_VERTEX(v);
				if( (vg = v->vg_p) )  {
					NMG_CK_VERTEX_G(vg);
					VSUB2( delta, vg->coord, pt );
					if( MAGSQ(delta) < tol->dist_sq )
						return(v);
				}
			}
		}
	}

	lu = RT_LIST_FIRST(loopuse, &s->lu_hd);
	while (RT_LIST_NOT_HEAD(lu, &s->lu_hd) ) {

			NMG_CK_LOOPUSE(lu);
			/* XXX what to do here? */
			rt_log("nmg_find_vu_in_face(): lu?\n");

			if (RT_LIST_PNEXT(loopuse, lu) == lu->lumate_p)
				lu = RT_LIST_PNEXT_PNEXT(loopuse, lu);
			else
				lu = RT_LIST_PNEXT(loopuse, lu);
	}

	for (RT_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_VERTEXUSE(eu->vu_p);
		v = eu->vu_p->v_p;
		NMG_CK_VERTEX(v);
		if( (vg = v->vg_p) )  {
			NMG_CK_VERTEX_G(vg);
			VSUB2( delta, vg->coord, pt );
			if( MAGSQ(delta) < tol->dist_sq )
				return(v);
		}
	}


	if (s->vu_p) {
		NMG_CK_VERTEXUSE(s->vu_p);
		v = s->vu_p->v_p;
		NMG_CK_VERTEX(v);
		if( (vg = v->vg_p) )  {
			NMG_CK_VERTEX_G( vg );
			VSUB2( delta, vg->coord, pt );
			if( MAGSQ(delta) < tol->dist_sq )
				return(v);
		}
	}
	return( (struct vertex *)0 );
}

/*
 *			N M G _ S H E L L _ I S _ E M P T Y
 *
 *  See if this is an invalid shell
 *  i.e., one that has absolutely nothing in it,
 *  not even a lone vertexuse.
 *
 *  Returns -
 *	1	yes, it is completely empty
 *	0	no, not empty
 */
int
nmg_shell_is_empty(s)
register CONST struct shell *s;
{

	NMG_CK_SHELL(s);

	if( RT_LIST_NON_EMPTY( &s->fu_hd ) )  return 0;
	if( RT_LIST_NON_EMPTY( &s->lu_hd ) )  return 0;
	if( RT_LIST_NON_EMPTY( &s->eu_hd ) )  return 0;
	if( s->vu_p )  return 0;
	return 1;
}

/*				N M G _ L U _ O F _ V U 
 *
 *	Given a vertexuse, return the loopuse somewhere above
 */
struct loopuse *
nmg_lu_of_vu(vu)
struct vertexuse *vu;
{
	NMG_CK_VERTEXUSE(vu);
	
	if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC &&
		*vu->up.eu_p->up.magic_p == NMG_LOOPUSE_MAGIC)
			return(vu->up.eu_p->up.lu_p);
	else if (*vu->up.magic_p != NMG_LOOPUSE_MAGIC)
		rt_bomb("NMG vertexuse has no loopuse ancestor\n");

	return(vu->up.lu_p);		
}

/*				N M G _ L U P S
 *
 *	return parent shell for loopuse
 */
struct shell *
nmg_lups(lu)
struct loopuse *lu;
{
	if (*lu->up.magic_p == NMG_SHELL_MAGIC) return(lu->up.s_p);
	else if (*lu->up.magic_p != NMG_FACEUSE_MAGIC) 
		rt_bomb("bad parent for loopuse\n");

	return(lu->up.fu_p->s_p);
}

/*				N M G _ E U P S 
 *
 *	return parent shell of edgeuse
 */
struct shell *
nmg_eups(eu)
struct edgeuse *eu;
{
	if (*eu->up.magic_p == NMG_SHELL_MAGIC) return(eu->up.s_p);
	else if (*eu->up.magic_p != NMG_LOOPUSE_MAGIC)
		rt_bomb("bad parent for edgeuse\n");

	return(nmg_lups(eu->up.lu_p));
}

/*				N M G _ F A C E R A D I A L
 *
 *	Looking radially around an edge, find another edge in the same
 *	face as the current edge. (this could be the mate to the current edge)
 */
CONST struct edgeuse *
nmg_faceradial(eu)
CONST struct edgeuse *eu;
{
	CONST struct faceuse *fu;
	CONST struct edgeuse *eur;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_LOOPUSE(eu->up.lu_p);
	fu = eu->up.lu_p->up.fu_p;
	NMG_CK_FACEUSE(fu);

	eur = eu->radial_p;

	while (*eur->up.magic_p != NMG_LOOPUSE_MAGIC ||
	    *eur->up.lu_p->up.magic_p != NMG_FACEUSE_MAGIC ||
	    eur->up.lu_p->up.fu_p->f_p != fu->f_p)
	    	eur = eur->eumate_p->radial_p;

	return(eur);
}


/*
 *			N M G _ R A D I A L _ F A C E _ E D G E _ I N _ S H E L L
 *
 *	looking radially around an edge, find another edge which is a part
 *	of a face in the same shell
 */
struct edgeuse *
nmg_radial_face_edge_in_shell(eu)
struct edgeuse *eu;
{
	struct edgeuse *eur;
	struct faceuse *fu;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_LOOPUSE(eu->up.lu_p);
	NMG_CK_FACEUSE(eu->up.lu_p->up.fu_p);

	fu = eu->up.lu_p->up.fu_p;
	eur = eu->radial_p;
	NMG_CK_EDGEUSE(eur);

	while (eur != eu->eumate_p) {
		if (*eur->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    *eur->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
		    eur->up.lu_p->up.fu_p->s_p == fu->s_p)
			break; /* found another face in shell */
		else {
			eur = eur->eumate_p->radial_p;
			NMG_CK_EDGEUSE(eur);
		}
	}
	return(eur);
}
