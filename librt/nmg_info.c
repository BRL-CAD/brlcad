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
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

CONST struct nmg_visit_handlers	nmg_visit_handlers_null;

/************************************************************************
 *									*
 *				MODEL Routines				*
 *									*
 ************************************************************************/

/*
 *			N M G _ F I N D _ M O D E L
 *
 *  Given a pointer to the magic number in any NMG data structure,
 *  return a pointer to the model structure that contains that NMG item.
 *
 *  The reason for the register variable is to leave the argument variable
 *  unmodified;  this may aid debugging in event of a core dump.
 */
struct model *
nmg_find_model( magic_p_arg )
CONST long	*magic_p_arg;
{
	register CONST long	*magic_p = magic_p_arg;

top:
	if( magic_p == (long *)0 )  {
		rt_log("nmg_find_model(x%x) enountered null pointer\n",
			magic_p_arg );
		rt_bomb("nmg_find_model() null pointer\n");
		/* NOTREACHED */
	}

	switch( *magic_p )  {
	case NMG_MODEL_MAGIC:
		return( (struct model *)magic_p );
	case NMG_REGION_MAGIC:
		return( ((struct nmgregion *)magic_p)->m_p );
	case NMG_SHELL_MAGIC:
		return( ((struct shell *)magic_p)->r_p->m_p );
	case NMG_FACEUSE_MAGIC:
		magic_p = &((struct faceuse *)magic_p)->s_p->l.magic;
		goto top;
	case NMG_FACE_MAGIC:
		magic_p = &((struct face *)magic_p)->fu_p->l.magic;
		goto top;
	case NMG_LOOP_MAGIC:
		magic_p = ((struct loop *)magic_p)->lu_p->up.magic_p;
		goto top;
	case NMG_LOOPUSE_MAGIC:
		magic_p = ((struct loopuse *)magic_p)->up.magic_p;
		goto top;
	case NMG_EDGE_MAGIC:
		magic_p = ((struct edge *)magic_p)->eu_p->up.magic_p;
		goto top;
	case NMG_EDGEUSE_MAGIC:
		magic_p = ((struct edgeuse *)magic_p)->up.magic_p;
		goto top;
	case NMG_VERTEX_MAGIC:
		magic_p = &(RT_LIST_FIRST(vertexuse,
			&((struct vertex *)magic_p)->vu_hd)->l.magic);
		goto top;
	case NMG_VERTEXUSE_MAGIC:
		magic_p = ((struct vertexuse *)magic_p)->up.magic_p;
		goto top;

	default:
		rt_log("nmg_find_model() can't get model for magic=x%x (%s)\n",
			*magic_p, rt_identify_magic( *magic_p ) );
		rt_bomb("nmg_find_model() failure\n");
	}
	return( (struct model *)NULL );
}

/*
 *			N M G _ F I N D _ P T _ I N _ M O D E L
 *
 *  Brute force search of the entire model to find a vertex that
 *  matches this point.
 */
struct vertex *
nmg_find_pt_in_model( m, pt, tol )
CONST struct model	*m;
CONST point_t		pt;
CONST struct rt_tol	*tol;
{
	struct nmgregion	*r;
	struct shell		*s;
	struct vertex		*v;

	NMG_CK_MODEL(m);
	RT_CK_TOL(tol);

	for( RT_LIST_FOR( r, nmgregion, &m->r_hd ) )  {
		NMG_CK_REGION(r);
		for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
			NMG_CK_SHELL(s);
			if( (v = nmg_find_pt_in_shell( s, pt, tol )) )  {
				NMG_CK_VERTEX(v);
				return v;
			}
		}
	}
	return (struct vertex *)NULL;
}

/************************************************************************
 *									*
 *				SHELL Routines				*
 *									*
 ************************************************************************/

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

/*				N M G _ F I N D _ S _ O F _ L U
 *
 *	return parent shell for loopuse
 *	formerly nmg_lups().
 */
struct shell *
nmg_find_s_of_lu(lu)
struct loopuse *lu;
{
	if (*lu->up.magic_p == NMG_SHELL_MAGIC) return(lu->up.s_p);
	else if (*lu->up.magic_p != NMG_FACEUSE_MAGIC) 
		rt_bomb("nmg_find_s_of_lu() bad parent for loopuse\n");

	return(lu->up.fu_p->s_p);
}

/*				N M G _ F I N D _ S _ O F _ E U
 *
 *	return parent shell of edgeuse
 *	formerly nmg_eups().
 */
struct shell *
nmg_find_s_of_eu(eu)
struct edgeuse *eu;
{
	if (*eu->up.magic_p == NMG_SHELL_MAGIC) return(eu->up.s_p);
	else if (*eu->up.magic_p != NMG_LOOPUSE_MAGIC)
		rt_bomb("nmg_find_s_of_eu() bad parent for edgeuse\n");

	return(nmg_find_s_of_lu(eu->up.lu_p));
}

/*
 *			N M G _ F I N D _ S _ O F _ V U
 *
 *  Return parent shell of vertexuse
 */
struct shell *
nmg_find_s_of_vu(vu)
struct vertexuse *vu;
{
	NMG_CK_VERTEXUSE(vu);

	if( *vu->up.magic_p == NMG_LOOPUSE_MAGIC )
		return nmg_find_s_of_lu( vu->up.lu_p );
	return nmg_find_s_of_eu( vu->up.eu_p );
}

/************************************************************************
 *									*
 *				FACE Routines				*
 *									*
 ************************************************************************/

/*	N M G _ F I N D _ F U _ O F _ E U
 *
 *	return a pointer to the faceuse that is the super-parent of this
 *	edgeuse.  If edgeuse has no super-parent faceuse, return NULL.
 */
struct faceuse *
nmg_find_fu_of_eu(eu)
struct edgeuse *eu;
{
	NMG_CK_EDGEUSE(eu);

	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
		*eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC)
			return eu->up.lu_p->up.fu_p;

	return (struct faceuse *)NULL;			
}

#define FACE_OF_LOOP(lu) { switch (*lu->up.magic_p) { \
	case NMG_FACEUSE_MAGIC:	return lu->up.fu_p; break; \
	case NMG_SHELL_MAGIC: return (struct faceuse *)NULL; break; \
	default: \
	    rt_log("Error at %s %d:\nInvalid loopuse parent magic (0x%x %d)\n", \
		__FILE__, __LINE__, *lu->up.magic_p, *lu->up.magic_p); \
	    rt_bomb("giving up on loopuse"); \
	} }


/*	N M G _ F I N D _ F U _ O F _ V U
 *
 *	return a pointer to the parent faceuse of the vertexuse
 *	or a null pointer if vu is not a child of a faceuse.
 */
struct faceuse *
nmg_find_fu_of_vu(vu)
struct vertexuse *vu;
{
	NMG_CK_VERTEXUSE(vu);

	switch (*vu->up.magic_p) {
	case NMG_LOOPUSE_MAGIC:
		FACE_OF_LOOP( vu->up.lu_p );
		break;
	case NMG_SHELL_MAGIC:
		rt_log("nmg_find_fu_of_vu() vertexuse is child of shell, can't find faceuse\n");
		return ((struct faceuse *)NULL);
		break;
	case NMG_EDGEUSE_MAGIC:
		switch (*vu->up.eu_p->up.magic_p) {
		case NMG_LOOPUSE_MAGIC:
			FACE_OF_LOOP( vu->up.eu_p->up.lu_p );
			break;
		case NMG_SHELL_MAGIC:
			rt_log("nmg_find_fu_of_vu() vertexuse is child of shell/edgeuse, can't find faceuse\n");
			return ((struct faceuse *)NULL);
			break;
		}
		rt_log("Error at %s %d:\nInvalid loopuse parent magic 0x%x\n",
			__FILE__, __LINE__, *vu->up.lu_p->up.magic_p);
		abort();
		break;
	default:
		rt_log("Error at %s %d:\nInvalid vertexuse parent magic 0x%x\n",
			__FILE__, __LINE__,
			*vu->up.magic_p);
		abort();
		break;
	}
	rt_log("How did I get here %s %d?\n", __FILE__, __LINE__);
	rt_bomb("nmg_find_fu_of_vu()\n");
}

/************************************************************************
 *									*
 *				LOOP Routines				*
 *									*
 ************************************************************************/

/*
 *			N M G _ F I N D _ L U _ O F _ V U
 */
struct loopuse *
nmg_find_lu_of_vu( vu )
struct vertexuse *vu;
{
	NMG_CK_VERTEXUSE( vu );
	if ( *vu->up.magic_p == NMG_LOOPUSE_MAGIC )
		return vu->up.lu_p;

	if ( *vu->up.magic_p == NMG_SHELL_MAGIC )
		return (struct loopuse *)NULL;

	NMG_CK_EDGEUSE( vu->up.eu_p );

	if ( *vu->up.eu_p->up.magic_p == NMG_SHELL_MAGIC )
		return (struct loopuse *)NULL;

	NMG_CK_LOOPUSE( vu->up.eu_p->up.lu_p );

	return vu->up.eu_p->up.lu_p;
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
 *
 * XXX Should the return really be a boolean?
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
	int		n_angles=0;	/* number of edge/edge angles measured */

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
		nmg_pr_eu_briefly(eu,NULL);
		VPRINT("vu1", this_vu->v_p->vg_p->coord);
		VPRINT("vu2", next_vu->v_p->vg_p->coord);
		VPRINT("edge1", edge1);
		VPRINT("edge2", edge2);
		VPRINT("left", left);
		rt_log(" e1=%g, e2=%g, n=%g, l=%g\n", MAGNITUDE(edge1),
			MAGNITUDE(edge2), MAGNITUDE(norm), MAGNITUDE(left));
		rt_log("atan2(%g,%g) = %g\n", y, x, rad);
#endif
		theta += rad;
		n_angles++;
	}
#if 0
	rt_log(" theta = %g (%g) n_angles=%d\n", theta, theta / rt_twopi, n_angles );
	nmg_face_lu_plot( lu, this_vu, this_vu );
	nmg_face_lu_plot( lu->lumate_p, this_vu, this_vu );
#endif

	if( n_angles < 3 )  {
		rt_log("nmg_loop_is_ccw():  only %d angles, can't tell\n", n_angles);
		return 0;
	}

	rad = theta * rt_inv2pi;
	x = rad-1;
	/* "rad" value is normalized -1..+1, tolerance here is 1% */
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
	nmg_pr_lu_briefly(lu, NULL);
	rt_log(" theta = %g (%g)\n", theta, theta / rt_twopi );
	nmg_face_lu_plot( lu, this_vu, this_vu );
	nmg_face_lu_plot( lu->lumate_p, this_vu, this_vu );
	rt_bomb("nmg_loop_is_ccw()\n");
	return 0;
}

/*
 *			N M G _ L O O P _ T O U C H E S _ S E L F
 *
 *  Search through all the vertices in a loop.
 *  If there are two distinct uses of one vertex in the loop,
 *  return true.
 *  This is useful for detecting "accordian pleats"
 *  unexpectedly showing up in a loop.
 *  Derrived from nmg_split_touchingloops().
 *
 *  Returns -
 *	vu	Yes, the loop touches itself at least once, at this vu.
 *	0	No, the loop does not touch itself.
 */
CONST struct vertexuse *
nmg_loop_touches_self( lu )
CONST struct loopuse	*lu;
{
	CONST struct edgeuse	*eu;
	CONST struct vertexuse	*vu;
	CONST struct vertex	*v;

	if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return (CONST struct vertexuse *)0;

	/* For each edgeuse, get vertexuse and vertex */
	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
		CONST struct vertexuse	*tvu;

		vu = eu->vu_p;
		NMG_CK_VERTEXUSE(vu);
		v = vu->v_p;
		NMG_CK_VERTEX(v);

		/*
		 *  For each vertexuse on vertex list,
		 *  check to see if it points up to the this loop.
		 *  If so, then there is a duplicated vertex.
		 *  Ordinarily, the vertex list will be *very* short,
		 *  so this strategy is likely to be faster than
		 *  a table-based approach, for most cases.
		 */
		for( RT_LIST_FOR( tvu, vertexuse, &v->vu_hd ) )  {
			CONST struct edgeuse		*teu;
			CONST struct loopuse		*tlu;
			CONST struct loopuse		*newlu;

			if( tvu == vu )  continue;
			if( *tvu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
			teu = tvu->up.eu_p;
			NMG_CK_EDGEUSE(teu);
			if( *teu->up.magic_p != NMG_LOOPUSE_MAGIC )  continue;
			tlu = teu->up.lu_p;
			NMG_CK_LOOPUSE(tlu);
			if( tlu != lu )  continue;
			/*
			 *  Repeated vertex exists.
			 */
			return vu;
		}
	}
	return (CONST struct vertexuse *)0;
}

/************************************************************************
 *									*
 *				EDGE Routines				*
 *									*
 ************************************************************************/

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
/* XXX should be CONST return */
struct edgeuse *
nmg_findeu(v1, v2, s, eup, dangling_only)
CONST struct vertex	*v1, *v2;
CONST struct shell	*s;
CONST struct edgeuse	*eup;
int		dangling_only;
{
	register CONST struct vertexuse	*vu;
	register CONST struct edgeuse	*eu;
	CONST struct edgeuse		*eup_mate;
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

	return (struct edgeuse *)eu;
}

/*
 *			N M G _ F I N D _ E U _ W I T H _ V U _ I N _ L U
 *
 *  Find an edgeuse starting at a given vertexuse within a loop(use).
 */
struct edgeuse *
nmg_find_eu_with_vu_in_lu( lu, vu )
CONST struct loopuse		*lu;
CONST struct vertexuse	*vu;
{
	register struct edgeuse	*eu;

	NMG_CK_LOOPUSE(lu);
	NMG_CK_VERTEXUSE(vu);
	if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
		rt_bomb("nmg_find_eu_with_vu_in_lu: loop has no edges!\n");
	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
		NMG_CK_EDGEUSE(eu);
		if( eu->vu_p == vu )  return eu;
	}
	rt_bomb("nmg_find_eu_with_vu_in_lu:  Unable to find vu!\n");
	/* NOTREACHED */
	return((struct edgeuse *)NULL);
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

/************************************************************************
 *									*
 *				VERTEX Routines				*
 *									*
 ************************************************************************/

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
 *			N M G _ F I N D _ P T _ I N _ F A C E
 *
 *  Conduct a geometric search for a vertex in face 'fu' which is
 *  "identical" to the given point, within the specified tolerance.
 *
 *  Returns -
 *	NULL			No vertex matched
 *	(struct vertexuse *)	A matching vertexuse from that face.
 */
struct vertexuse *
nmg_find_pt_in_face(fu, pt, tol)
CONST struct faceuse	*fu;
CONST point_t		pt;
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
		} else if (magic1 == NMG_EDGEUSE_MAGIC) {
			for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
				v = eu->vu_p->v_p;
				NMG_CK_VERTEX(v);
				if( !(vg = v->vg_p) )  continue;
				NMG_CK_VERTEX_G(vg);
				VSUB2(delta, vg->coord, pt);
				if ( MAGSQ(delta) < tol->dist_sq)
					return(eu->vu_p);
			}
		} else {
			rt_bomb("nmg_find_vu_in_face() Bogus child of loop\n");
		}
	}
	return ((struct vertexuse *)NULL);
}

/*
 *			N M G _ I S _ V E R T E X _ I N _ E D G E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_vertex_in_edgelist( v, hd )
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
 *			N M G _ I S _ V E R T E X _ I N _ L O O P L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_vertex_in_looplist( v, hd, singletons )
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
			if( nmg_is_vertex_in_edgelist( v, &lu->down_hd ) )
				return(1);
		} else {
			rt_bomb("nmg_is_vertex_in_loopuse() bad magic\n");
		}
	}
	return(0);
}

/*
 *	N M G _ I S _ V E R T E X _ A _ S E L F L O O P _ I N _ S H E L L
 *
 *  Check to see if a given vertex is used within a shell
 *  by a wire loopuse which is a loop of a single vertex.
 *  The search could either be by the shell lu_hd, or the vu_hd.
 *
 *  Returns -
 *	0	vertex is not part of that kind of loop in the shell.
 *	1	vertex is part of a selfloop in the given shell.
 */
int
nmg_is_vertex_a_selfloop_in_shell(v, s)
CONST struct vertex	*v;
CONST struct shell	*s;
{
	CONST struct vertexuse *vu;

	NMG_CK_VERTEX(v);
	NMG_CK_SHELL(s);

	/* try to find the vertex in a loopuse of this shell */
	for (RT_LIST_FOR(vu, vertexuse, &v->vu_hd)) {
		register CONST struct loopuse	*lu;
		NMG_CK_VERTEXUSE(vu);
		if (*vu->up.magic_p != NMG_LOOPUSE_MAGIC )  continue;
		lu = vu->up.lu_p;
		NMG_CK_LOOPUSE(lu);
		if( *lu->up.magic_p != NMG_SHELL_MAGIC )  continue;
		NMG_CK_SHELL(lu->up.s_p);
		if( lu->up.s_p == s)
			return 1;
	}
	return 0;
}

/*
 *			N M G _ I S _ V E R T E X _ I N _ F A C E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_vertex_in_facelist( v, hd )
register CONST struct vertex	*v;
CONST struct rt_list		*hd;
{
	register CONST struct faceuse	*fu;

	NMG_CK_VERTEX(v);
	for( RT_LIST_FOR( fu, faceuse, hd ) )  {
		NMG_CK_FACEUSE(fu);
		if( nmg_is_vertex_in_looplist( v, &fu->lu_hd, 1 ) )
			return(1);
	}
	return(0);
}

/*
 *			N M G _ I S _ E D G E _ I N _ E D G E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_edge_in_edgelist( e, hd )
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
 *			N M G _ I S _ E D G E _ I N _ L O O P L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_edge_in_looplist( e, hd )
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
			if( nmg_is_edge_in_edgelist( e, &lu->down_hd ) )
				return(1);
		} else {
			rt_bomb("nmg_is_edge_in_loopuse() bad magic\n");
		}
	}
	return(0);
}

/*
 *			N M G _ I S _ E D G E _ I N _ F A C E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_edge_in_facelist( e, hd )
CONST struct edge	*e;
CONST struct rt_list	*hd;
{
	register CONST struct faceuse	*fu;

	NMG_CK_EDGE(e);
	for( RT_LIST_FOR( fu, faceuse, hd ) )  {
		NMG_CK_FACEUSE(fu);
		if( nmg_is_edge_in_looplist( e, &fu->lu_hd ) )
			return(1);
	}
	return(0);
}

/*
 *			N M G _ I S _ L O O P _ I N _ F A C E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_loop_in_facelist( l, fu_hd )
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
			if( (vu = nmg_find_pt_in_face( fu, pt, tol )) )
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

struct vf_state {
	char		*visited;
	struct nmg_ptbl	*tabl;
};

/*
 *			N M G _ 2 R V F _ H A N D L E R
 *
 *  A private support routine for nmg_region_vertex_list().
 *  Having just visited a vertex, if this is the first time,
 *  add it to the nmg_ptbl array.
 */
static void
nmg_2rvf_handler( vp, state, first )
long		*vp;
genptr_t	state;
int		first;
{
	register struct vf_state *sp = (struct vf_state *)state;
	register struct vertex	*v = (struct vertex *)vp;

	NMG_CK_VERTEX(v);
	/* If this vertex has been processed before, do nothing more */
	if( !NMG_INDEX_FIRST_TIME(sp->visited, v) )  return;

	nmg_tbl( sp->tabl, TBL_INS, vp );
}

/*
 *			N M G _ R E G I O N _ V E R T E X _ L I S T
 *
 *  Given an nmgregion, build an nmg_ptbl list which has each vertex
 *  pointer in the region listed exactly once.
 */
void
nmg_region_vertex_list( tab, r )
struct nmg_ptbl		*tab;
struct nmgregion	*r;
{
	struct model	*m;
	struct vf_state	st;
	struct nmg_visit_handlers	handlers;

	NMG_CK_REGION(r);
	m = r->m_p;
	NMG_CK_MODEL(m);

	st.visited = (char *)rt_calloc(m->maxindex+1, sizeof(char), "visited[]");
	st.tabl = tab;

	(void)nmg_tbl( tab, TBL_INIT, 0 );

	handlers = nmg_visit_handlers_null;		/* struct copy */
	handlers.vis_vertex = nmg_2rvf_handler;
	nmg_visit( &r->l.magic, &handlers, (genptr_t)&st );

	rt_free( (char *)st.visited, "visited[]");
}

/*
 *			N M G _ M O D E L _ V E R T E X _ L I S T
 *
 *  Given an nmgmodel, build an nmg_ptbl list which has each vertex
 *  pointer in the model listed exactly once.
 */
void
nmg_model_vertex_list( tab, m )
struct nmg_ptbl		*tab;
struct model		*m;
{
	struct vf_state	st;
	struct nmg_visit_handlers	handlers;

	NMG_CK_MODEL(m);

	st.visited = (char *)rt_calloc(m->maxindex+1, sizeof(char), "visited[]");
	st.tabl = tab;

	(void)nmg_tbl( tab, TBL_INIT, 0 );

	handlers = nmg_visit_handlers_null;		/* struct copy */
	handlers.vis_vertex = nmg_2rvf_handler;
	nmg_visit( &m->magic, &handlers, (genptr_t)&st );

	rt_free( (char *)st.visited, "visited[]");
}

/*
 *			N M G _ 2 R E F _ H A N D L E R
 *
 *  A private support routine for nmg_region_edge_list().
 *  Having just visited an edge, if this is the first time,
 *  add it to the nmg_ptbl array.
 */
static void
nmg_2ref_handler( ep, state, first )
long		*ep;
genptr_t	state;
int		first;
{
	register struct vf_state *sp = (struct vf_state *)state;
	register struct edge	*e = (struct edge *)ep;

	NMG_CK_EDGE(e);
	/* If this edge has been processed before, do nothing more */
	if( !NMG_INDEX_FIRST_TIME(sp->visited, e) )  return;

	nmg_tbl( sp->tabl, TBL_INS, ep );
}

/*
 *                      N M G _ R E G I O N _ E D G E _ L I S T
 *
 *  Given an nmgregion, build an nmg_ptbl list which has each edge
 *  pointer in the region listed exactly once.
 */

void
nmg_region_edge_list( tab , r )
struct nmg_ptbl	*tab;
struct nmgregion *r;
{
	struct model    *m;
	struct vf_state st;
	struct nmg_visit_handlers       handlers;

	NMG_CK_REGION(r);
	m = r->m_p;
	NMG_CK_MODEL(m);


	st.visited = (char *)rt_calloc(m->maxindex+1, sizeof(char), "visited[]");
	st.tabl = tab;

	(void)nmg_tbl( tab, TBL_INIT, 0 );

	handlers = nmg_visit_handlers_null;             /* struct copy */
	handlers.vis_edge = nmg_2ref_handler;
	nmg_visit( &r->l.magic, &handlers, (genptr_t)&st );

	rt_free( (char *)st.visited, "visited[]");
}
