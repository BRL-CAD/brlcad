/*                      N M G _ I N F O . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2007 United States Government as represented by
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
/** @file nmg_info.c
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
 */
/** @} */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

/************************************************************************
 *									*
 *				MODEL Routines				*
 *									*
 ************************************************************************/

/**
 *			N M G _ F I N D _ M O D E L
 *
 *  Given a pointer to the magic number in any NMG data structure,
 *  return a pointer to the model structure that contains that NMG item.
 *
 *  The reason for the register variable is to leave the argument variable
 *  unmodified;  this may aid debugging in event of a core dump.
 */
struct model *
nmg_find_model(const long int *magic_p_arg)
{
	register const long	*magic_p = magic_p_arg;

top:
	if( magic_p == (long *)0 )  {
		bu_log("nmg_find_model(x%x) enountered null pointer\n",
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
		magic_p = &(BU_LIST_FIRST(vertexuse,
			&((struct vertex *)magic_p)->vu_hd)->l.magic);
		goto top;
	case NMG_VERTEXUSE_MAGIC:
		magic_p = ((struct vertexuse *)magic_p)->up.magic_p;
		goto top;

	default:
		bu_log("nmg_find_model() can't get model for magic=x%x (%s)\n",
			*magic_p, bu_identify_magic( *magic_p ) );
		rt_bomb("nmg_find_model() failure\n");
	}
	return( (struct model *)NULL );
}

void
nmg_model_bb(fastf_t *min_pt, fastf_t *max_pt, const struct model *m)
{
	struct nmgregion *r;
	register int i;

	NMG_CK_MODEL(m);

	min_pt[0] = min_pt[1] = min_pt[2] = MAX_FASTF;
	max_pt[0] = max_pt[1] = max_pt[2] = -MAX_FASTF;

	for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
		NMG_CK_REGION(r);
		NMG_CK_REGION_A(r->ra_p);

		for (i=0 ; i < 3 ; i++) {
			if (min_pt[i] > r->ra_p->min_pt[i])
				min_pt[i] = r->ra_p->min_pt[i];
			if (max_pt[i] < r->ra_p->max_pt[i])
				max_pt[i] = r->ra_p->max_pt[i];
		}
	}
}

/************************************************************************
 *									*
 *				SHELL Routines				*
 *									*
 ************************************************************************/

/**
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
nmg_shell_is_empty(register const struct shell *s)
{

	NMG_CK_SHELL(s);

	if( BU_LIST_NON_EMPTY( &s->fu_hd ) )  return 0;
	if( BU_LIST_NON_EMPTY( &s->lu_hd ) )  return 0;
	if( BU_LIST_NON_EMPTY( &s->eu_hd ) )  return 0;
	if( s->vu_p )  return 0;
	return 1;
}

/**				N M G _ F I N D _ S _ O F _ L U
 *
 *	return parent shell for loopuse
 *	formerly nmg_lups().
 */
struct shell *
nmg_find_s_of_lu(const struct loopuse *lu)
{
	if (*lu->up.magic_p == NMG_SHELL_MAGIC) return(lu->up.s_p);
	else if (*lu->up.magic_p != NMG_FACEUSE_MAGIC)
		rt_bomb("nmg_find_s_of_lu() bad parent for loopuse\n");

	return(lu->up.fu_p->s_p);
}

/**				N M G _ F I N D _ S _ O F _ E U
 *
 *	return parent shell of edgeuse
 *	formerly nmg_eups().
 */
struct shell *
nmg_find_s_of_eu(const struct edgeuse *eu)
{
	if (*eu->up.magic_p == NMG_SHELL_MAGIC) return(eu->up.s_p);
	else if (*eu->up.magic_p != NMG_LOOPUSE_MAGIC)
		rt_bomb("nmg_find_s_of_eu() bad parent for edgeuse\n");

	return(nmg_find_s_of_lu(eu->up.lu_p));
}

/**
 *			N M G _ F I N D _ S _ O F _ V U
 *
 *  Return parent shell of vertexuse
 */
struct shell *
nmg_find_s_of_vu(const struct vertexuse *vu)
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

/**
 *			N M G _ F I N D _ F U _ O F _ E U
 *
 *	return a pointer to the faceuse that is the super-parent of this
 *	edgeuse.  If edgeuse has no grandparent faceuse, return NULL.
 */
struct faceuse *
nmg_find_fu_of_eu(const struct edgeuse *eu)
{
	NMG_CK_EDGEUSE(eu);

	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
		*eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC)
			return eu->up.lu_p->up.fu_p;

	return (struct faceuse *)NULL;
}

/**
 *			N M G _ F I N D _ F U _ O F _ L U
 */
struct faceuse *
nmg_find_fu_of_lu(const struct loopuse *lu)
{
	switch (*lu->up.magic_p) {
	case NMG_FACEUSE_MAGIC:
		return lu->up.fu_p;
	case NMG_SHELL_MAGIC:
		return (struct faceuse *)NULL;
	default:
	    bu_log("Error at %s %d:\nInvalid loopuse parent magic (0x%x %d)\n",
		__FILE__, __LINE__, *lu->up.magic_p, *lu->up.magic_p);
	    rt_bomb("nmg_find_fu_of_lu() giving up on loopuse");
	}
	return (struct faceuse *)NULL;
}


/**	N M G _ F I N D _ F U _ O F _ V U
 *
 *	return a pointer to the parent faceuse of the vertexuse
 *	or a null pointer if vu is not a child of a faceuse.
 */
struct faceuse *
nmg_find_fu_of_vu(const struct vertexuse *vu)
{
	NMG_CK_VERTEXUSE(vu);

	switch (*vu->up.magic_p) {
	case NMG_LOOPUSE_MAGIC:
		return nmg_find_fu_of_lu( vu->up.lu_p );
	case NMG_SHELL_MAGIC:
		if (rt_g.NMG_debug & DEBUG_BASIC) bu_log("nmg_find_fu_of_vu(vu=x%x) vertexuse is child of shell, can't find faceuse\n", vu);
		return ((struct faceuse *)NULL);
	case NMG_EDGEUSE_MAGIC:
		switch (*vu->up.eu_p->up.magic_p) {
		case NMG_LOOPUSE_MAGIC:
			return nmg_find_fu_of_lu( vu->up.eu_p->up.lu_p );
		case NMG_SHELL_MAGIC:
			if (rt_g.NMG_debug & DEBUG_BASIC) bu_log("nmg_find_fu_of_vu(vu=x%x) vertexuse is child of shell/edgeuse, can't find faceuse\n", vu);
			return ((struct faceuse *)NULL);
		}
		bu_log("Error at %s %d:\nInvalid loopuse parent magic 0x%x\n",
			__FILE__, __LINE__, *vu->up.lu_p->up.magic_p);
		abort();
		break;
	default:
		bu_log("Error at %s %d:\nInvalid vertexuse parent magic 0x%x\n",
			__FILE__, __LINE__,
			*vu->up.magic_p);
		abort();
		break;
	}
	bu_log("How did I get here %s %d?\n", __FILE__, __LINE__);
	rt_bomb("nmg_find_fu_of_vu()\n");
	return ((struct faceuse *)NULL);
}
/**
 *			N M G _ F I N D _ F U _ W I T H _ F G _ I N _ S
 *
 *  Find a faceuse in shell s1 that shares the face_g_plane structure with
 *  fu2 and has the same orientation.
 *  This may be an OT_OPPOSITE faceuse, depending on orientation.
 *  Returns NULL if no such faceuse can be found in s1.
 *  fu2 may be in s1, or in some other shell.
 */
struct faceuse *
nmg_find_fu_with_fg_in_s(const struct shell *s1, const struct faceuse *fu2)
{
	struct faceuse		*fu1;
	struct face		*f2;
	register struct face_g_plane	*fg2;

	NMG_CK_SHELL(s1);
	NMG_CK_FACEUSE(fu2);

	f2 = fu2->f_p;
	NMG_CK_FACE(f2);
	fg2 = f2->g.plane_p;
	NMG_CK_FACE_G_PLANE(fg2);

	for( BU_LIST_FOR( fu1, faceuse, &s1->fu_hd ) )  {
		register struct face	*f1;
		register struct	face_g_plane	*fg1;
		int			flip1, flip2;

		NMG_CK_FACEUSE(fu1);
		f1 = fu1->f_p;
		NMG_CK_FACE(f1);
		fg1 = fu1->f_p->g.plane_p;
		NMG_CK_FACE_G_PLANE(fg1);

		if( fg1 != fg2 )  continue;

		if( fu1 == fu2 || fu1->fumate_p == fu2 )  continue;

		/* Face geometry matches, select fu1 or it's mate */
		flip1 = (fu1->orientation != OT_SAME) != (f1->flip != 0);
		flip2 = (fu2->orientation != OT_SAME) != (f2->flip != 0);
		if( flip1 == flip2 )  return fu1;
		return fu1->fumate_p;
	}
	return (struct faceuse *)NULL;
}

/**
 *			N M G _ M E A S U R E _ F U _ A N G L E
 *
 *  Return the angle in radians from the interior portion of the faceuse
 *  associated with edgeuse 'eu', measured in the coordinate system
 *  defined by xvec and yvec, which are known to be perpendicular to
 *  each other, and to the edge vector.
 *
 *  This is done by finding the "left-ward" vector for the edge in the
 *  face, which points into the interior of the face, and measuring
 *  the angle it forms relative to xvec and yvec.
 *
 *  Wire edges are indicated by always returning angle of -pi.
 *  That will be the only case for negative returns.
 */
double
nmg_measure_fu_angle(const struct edgeuse *eu, const fastf_t *xvec, const fastf_t *yvec, const fastf_t *zvec)
{
	vect_t			left;
	double			ret;

	NMG_CK_EDGEUSE(eu);
	if( *eu->up.magic_p != NMG_LOOPUSE_MAGIC )  return -bn_pi;

	if( nmg_find_eu_leftvec( left, eu ) < 0 )  return -bn_pi;

	ret = bn_angle_measure( left, xvec, yvec );
	return ret;
}

/************************************************************************
 *									*
 *				LOOP Routines				*
 *									*
 ************************************************************************/

/**
 *			N M G _ F I N D _ L U _ O F _ V U
 */
struct loopuse *
nmg_find_lu_of_vu(const struct vertexuse *vu)
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

/**
 *			N M G _ L O O P _ I S _ A _ C R A C K
 *
 *  A "crack" is defined as a loop which has no area.
 *
 *  Example of a non-trivial "crack" loop:
 *
 *	                 <---- eu4 -----
 *	               C ############### D
 *	             | # ^ ---- eu3 --->
 *	             | # |
 *	           eu5 # eu2
 *	             | # |
 *	  <--- eu6 --V # |
 *	A ############ B
 *	  --- eu1 ---->
 *
 *
 *  Returns -
 *	 0	Loop is not a "crack"
 *	!0	Loop *is* a "crack"
 */
int
nmg_loop_is_a_crack(const struct loopuse *lu)
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
	int		ret = 0;

	NMG_CK_LOOPUSE(lu);
	if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )  {
		if (rt_g.NMG_debug & DEBUG_BASIC)  bu_log("lu up is not faceuse\n");
		ret = 0;
		goto out;
	}
	fu = lu->up.fu_p;
	NMG_CK_FACEUSE(fu);

	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )  {
		if (rt_g.NMG_debug & DEBUG_BASIC)  bu_log("lu down is not edgeuse\n");
		ret = 0;
		goto out;
	}

	/*
	 *  For every edgeuse, see if there is another edgeuse from 'lu',
	 *  radial around this edge, which is not this edgeuse's mate.
	 */
	for( BU_LIST_FOR( cur_eu, edgeuse, &lu->down_hd ) )  {
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
		for( BU_LIST_FOR( test_vu, vertexuse, &next_v->vu_hd ) )  {
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
		ret = 0;
		goto out;

		/* One edgeuse matched, all the others have to as well */
match:		;
	}
	ret = 1;
out:
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_loop_is_a_crack(lu=x%x) ret=%d\n", lu, ret );
	}
	return ret;
}

/**
 *			N M G _ L O O P _ I S _ C C W
 *
 *  Determine if loop proceeds counterclockwise (CCW) around the
 *  provided normal vector (which may be either the face normal,
 *  or the anti-normal).
 *
 *  Returns -
 *	+1	Loop is CCW, should be exterior loop.
 *	-1	Loop is CW, should be interior loop.
 *	 0	Unable to tell, error.
 *
 */
int
nmg_loop_is_ccw(const struct loopuse *lu, const fastf_t *norm, const struct bn_tol *tol)
{
	fastf_t area;
	fastf_t dot;
	plane_t pl;
	int ret;

	area = nmg_loop_plane_area( lu, pl );

	if( area <= 0.0 )
	{
		if( RT_G_DEBUG & DEBUG_MATH )
		{
			bu_log( "nmg_loop_is_ccw: Loop has no area\n" );
			nmg_pr_lu_briefly( lu, " " );
		}
		ret = 0;
		goto out;
	}

	if( NEAR_ZERO( area, tol->dist_sq ) )
	{
		if( RT_G_DEBUG & DEBUG_MATH )
		{
			bu_log( "nmg_loop_is_ccw: Loop area (%g) is less than tol->dist_sq (%g)\n", area, tol->dist_sq );
			nmg_pr_lu_briefly( lu, " " );
		}
		ret = 0;
		goto out;
	}

	dot = VDOT( norm, pl );

	if( NEAR_ZERO( dot, tol->perp ) )
	{
		if( RT_G_DEBUG & DEBUG_MATH )
		{
			bu_log( "nmg_loop_is_ccw: normal ( %g %g %g ) is in plane of loop ( %g %g %g %g ), dot = %g\n",
				V3ARGS( norm ), V4ARGS( pl ), dot );
			nmg_pr_lu_briefly( lu, " " );
		}
		ret = 0;
		goto out;
	}

	if( dot < 0.0 )
		ret = (-1 );
	else
		ret = 1;

out:
	if (rt_g.NMG_debug & DEBUG_BASIC)
		bu_log( "nmg_loop_is_ccw(lu=x%x) ret=%d\n" , lu, ret );

	return( ret );
}

/**
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
const struct vertexuse *
nmg_loop_touches_self(const struct loopuse *lu)
{
	const struct edgeuse	*eu;
	const struct vertexuse	*vu;
	const struct vertex	*v;

	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return (const struct vertexuse *)0;

	/* For each edgeuse, get vertexuse and vertex */
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
		const struct vertexuse	*tvu;

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
		for( BU_LIST_FOR( tvu, vertexuse, &v->vu_hd ) )  {
			const struct edgeuse		*teu;
			const struct loopuse		*tlu;

			if( tvu == vu )  continue;
			/*
			 *  Inline expansion of:
			 *	if(nmg_find_lu_of_vu(tvu) != lu) continue;
			 */
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
	return (const struct vertexuse *)0;
}

/************************************************************************
 *									*
 *				EDGE Routines				*
 *									*
 ************************************************************************/

/**
 *			N M G _ F I N D _ M A T C H I N G _ E U _ I N _ S
 *
 *  If shell s2 has an edge that connects the same vertices as eu1 connects,
 *  return the matching edgeuse in s2.
 *  This routine works properly regardless of whether eu1 is in s2 or not.
 *  A convenient wrapper for nmg_findeu().
 */
struct edgeuse *
nmg_find_matching_eu_in_s(const struct edgeuse *eu1, const struct shell *s2)
{
	const struct vertexuse	*vu1a, *vu1b;
	struct edgeuse		*eu2;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_SHELL(s2);

	vu1a = eu1->vu_p;
	vu1b = BU_LIST_PNEXT_CIRC( edgeuse, eu1 )->vu_p;
	NMG_CK_VERTEXUSE(vu1a);
	NMG_CK_VERTEXUSE(vu1b);
	if( (nmg_find_v_in_shell( vu1a->v_p, s2, 1 )) == (struct vertexuse *)NULL )
		return (struct edgeuse *)NULL;
	if( (nmg_find_v_in_shell( vu1b->v_p, s2, 1 )) == (struct vertexuse *)NULL )
		return (struct edgeuse *)NULL;

	/* Both vertices have vu's of eu's in s2 */

	eu2 = nmg_findeu( vu1a->v_p, vu1b->v_p, s2, eu1, 0 );
	return eu2;		/* May be NULL if no edgeuse found */
}

/**
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
nmg_findeu(const struct vertex *v1, const struct vertex *v2, const struct shell *s, const struct edgeuse *eup, int dangling_only)
{
	register const struct vertexuse	*vu;
	register const struct edgeuse	*eu;
	const struct edgeuse		*eup_mate;
	int				eup_orientation;

	NMG_CK_VERTEX(v1);
	NMG_CK_VERTEX(v2);
	if(s) NMG_CK_SHELL(s);

	if(eup)  {
		struct faceuse	*fu;
		NMG_CK_EDGEUSE(eup);
		eup_mate = eup->eumate_p;
		NMG_CK_EDGEUSE(eup_mate);
		if( (fu = nmg_find_fu_of_eu(eup)) )
			eup_orientation = fu->orientation;
		else
			eup_orientation = OT_SAME;
	} else {
		eup_mate = eup;			/* NULL */
		eup_orientation = OT_SAME;
	}

	if (rt_g.NMG_debug & DEBUG_FINDEU)
		bu_log("nmg_findeu() seeking eu!=%8x/%8x between (%8x, %8x) %s\n",
			eup, eup_mate, v1, v2,
			dangling_only ? "[dangling]" : "[any]" );

	for( BU_LIST_FOR( vu, vertexuse, &v1->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if (!vu->up.magic_p)
			rt_bomb("nmg_findeu() vertexuse in vu_hd list has null parent\n");

		/* Ignore self-loops and lone shell verts */
		if (*vu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
		eu = vu->up.eu_p;

		/* Ignore edgeuses which don't run between the right verts */
		if( eu->eumate_p->vu_p->v_p != v2 )  continue;

		if (rt_g.NMG_debug & DEBUG_FINDEU )  {
			bu_log("nmg_findeu: check eu=%8x vertex=(%8x, %8x)\n",
				eu, eu->vu_p->v_p,
				eu->eumate_p->vu_p->v_p);
		}

		/* Ignore the edgeuse to be excluded */
		if( eu == eup || eu->eumate_p == eup )  {
			if (rt_g.NMG_debug & DEBUG_FINDEU )
				bu_log("\tIgnoring -- excluded edgeuse\n");
			continue;
		}

		/* See if this edgeuse is in the proper shell */
		if( s && nmg_find_s_of_eu(eu) != s )  {
			if (rt_g.NMG_debug & DEBUG_FINDEU)
				bu_log("\tIgnoring x%x -- eu in wrong shell s=%x\n", eu, eu->up.s_p);
			continue;
		}

		/* If it's not a dangling edge, skip on */
		if( dangling_only && eu->eumate_p != eu->radial_p) {
			if (rt_g.NMG_debug & DEBUG_FINDEU)  {
				bu_log("\tIgnoring %8x/%8x (radial=x%x)\n",
					eu, eu->eumate_p,
					eu->radial_p );
			}
			continue;
		}

		if (rt_g.NMG_debug & DEBUG_FINDEU)
			bu_log("\tFound %8x/%8x\n", eu, eu->eumate_p);

		if ( *eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
		     *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
		     eup_orientation != eu->up.lu_p->up.fu_p->orientation)
			eu = eu->eumate_p;	/* Take other orient */
		goto out;
	}
	eu = (struct edgeuse *)NULL;
out:
	if (rt_g.NMG_debug & (DEBUG_BASIC|DEBUG_FINDEU))
		bu_log("nmg_findeu() returns x%x\n", eu);

	return (struct edgeuse *)eu;
}

/**
 *			N M G _ F I N D _ E U _ I N _ F A C E
 *
 *  An analog to nmg_findeu(), only restricted to searching a faceuse,
 *  rather than to a whole shell.
 */
struct edgeuse *
nmg_find_eu_in_face(const struct vertex *v1, const struct vertex *v2, const struct faceuse *fu, const struct edgeuse *eup, int dangling_only)
{
	register const struct vertexuse	*vu;
	register const struct edgeuse	*eu;
	const struct edgeuse		*eup_mate;
	int				eup_orientation;

	NMG_CK_VERTEX(v1);
	NMG_CK_VERTEX(v2);
	if(fu) NMG_CK_FACEUSE(fu);

	if(eup)  {
		struct faceuse	*fu;
		NMG_CK_EDGEUSE(eup);
		eup_mate = eup->eumate_p;
		NMG_CK_EDGEUSE(eup_mate);
		if( (fu = nmg_find_fu_of_eu(eup)) )
			eup_orientation = fu->orientation;
		else
			eup_orientation = OT_SAME;
	} else {
		eup_mate = eup;			/* NULL */
		eup_orientation = OT_SAME;
	}

	if (rt_g.NMG_debug & DEBUG_FINDEU)
		bu_log("nmg_find_eu_in_face() seeking eu!=%8x/%8x between (%8x, %8x) %s\n",
			eup, eup_mate, v1, v2,
			dangling_only ? "[dangling]" : "[any]" );

	for( BU_LIST_FOR( vu, vertexuse, &v1->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if (!vu->up.magic_p)
			rt_bomb("nmg_find_eu_in_face() vertexuse in vu_hd list has null parent\n");

		/* Ignore self-loops and lone shell verts */
		if (*vu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
		eu = vu->up.eu_p;

		/* Ignore edgeuses which don't run between the right verts */
		if( eu->eumate_p->vu_p->v_p != v2 )  continue;

		if (rt_g.NMG_debug & DEBUG_FINDEU )  {
			bu_log("nmg_find_eu_in_face: check eu=%8x vertex=(%8x, %8x)\n",
				eu, eu->vu_p->v_p,
				eu->eumate_p->vu_p->v_p);
		}

		/* Ignore the edgeuse to be excluded */
		if( eu == eup || eu->eumate_p == eup )  {
			if (rt_g.NMG_debug & DEBUG_FINDEU )
				bu_log("\tIgnoring -- excluded edgeuse\n");
			continue;
		}

		/* See if this edgeuse is in the proper faceuse */
		if( fu && nmg_find_fu_of_eu(eu) != fu )  {
			if (rt_g.NMG_debug & DEBUG_FINDEU)
				bu_log("\tIgnoring x%x -- eu not in faceuse\n", eu);
			continue;
		}

		/* If it's not a dangling edge, skip on */
		if( dangling_only && eu->eumate_p != eu->radial_p) {
			if (rt_g.NMG_debug & DEBUG_FINDEU)  {
				bu_log("\tIgnoring %8x/%8x (radial=x%x)\n",
					eu, eu->eumate_p,
					eu->radial_p );
			}
			continue;
		}

		if (rt_g.NMG_debug & DEBUG_FINDEU)
			bu_log("\tFound %8x/%8x\n", eu, eu->eumate_p);

		if ( *eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
		     *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
		     eup_orientation != eu->up.lu_p->up.fu_p->orientation)
			eu = eu->eumate_p;	/* Take other orient */
		goto out;
	}
	eu = (struct edgeuse *)NULL;
out:
	if (rt_g.NMG_debug & (DEBUG_BASIC|DEBUG_FINDEU))
		bu_log("nmg_find_eu_in_face() returns x%x\n", eu);

	return (struct edgeuse *)eu;
}

/**
 *			N M G _ F I N D _ E
 *
 *  Find an edge between a given pair of vertices.
 *
 *  If a given shell "s" is specified, then only edges in that shell
 *  will be considered, otherwise all edges in the model are fair game.
 *
 *  If a particular edge "ep" is specified, then that edge
 *  will not be returned as a match.
 *
 *  Returns -
 *	edgeuse*	Edgeuse of an edge which matches the criteria
 *	NULL		Unable to find matching edge
 */
struct edgeuse *
nmg_find_e(const struct vertex *v1, const struct vertex *v2, const struct shell *s, const struct edge *ep)
{
	register const struct vertexuse	*vu;
	register const struct edgeuse	*eu;

	NMG_CK_VERTEX(v1);
	NMG_CK_VERTEX(v2);
	if(s) NMG_CK_SHELL(s);

	if (rt_g.NMG_debug & DEBUG_FINDEU)  {
		bu_log("nmg_find_e() seeking e!=%8x between (%8x, %8x)\n",
			ep, v1, v2 );
	}

	for( BU_LIST_FOR( vu, vertexuse, &v1->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if (!vu->up.magic_p)
			rt_bomb("nmg_find_e() vertexuse in vu_hd list has null parent\n");

		/* Ignore self-loops and lone shell verts */
		if (*vu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
		eu = vu->up.eu_p;

		/* Ignore edgeuses which don't run between the right verts */
		/* We know that this eu starts at v1 */
		if( eu->eumate_p->vu_p->v_p != v2 )  continue;

		if (rt_g.NMG_debug & DEBUG_FINDEU )  {
			bu_log("nmg_find_e: check eu=%8x vertex=(%8x, %8x)\n",
				eu, eu->vu_p->v_p,
				eu->eumate_p->vu_p->v_p);
		}

		/* Ignore the edge to be excluded */
		if( eu->e_p == ep )  {
			if (rt_g.NMG_debug & DEBUG_FINDEU )
				bu_log("\tIgnoring -- excluded edge\n");
			continue;
		}

		/* See if this edgeuse is in the proper shell */
		if( s && nmg_find_s_of_eu(eu) != s )  {
			if (rt_g.NMG_debug & DEBUG_FINDEU)
				bu_log("\tIgnoring x%x -- eu in wrong shell s=%x\n", eu, eu->up.s_p);
			continue;
		}

		if (rt_g.NMG_debug & DEBUG_FINDEU)
			bu_log("\tFound %8x/%8x\n", eu, eu->eumate_p);

		if ( *eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
		     *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
		     eu->up.lu_p->up.fu_p->orientation != OT_SAME )  {
			eu = eu->eumate_p;	/* Take other orient */
		}
		goto out;
	}
	eu = (struct edgeuse *)NULL;
out:
	if (rt_g.NMG_debug & (DEBUG_BASIC|DEBUG_FINDEU))
		bu_log("nmg_find_e() returns x%x\n", eu);

	return (struct edgeuse *)eu;
}

/**
 *			N M G _ F I N D _ E U _ O F _ V U
 *
 *  Return a pointer to the edgeuse which is the parent of this vertexuse.
 *
 *  A simple helper routine, which replaces the amazingly bad sequence of:
 * 	nmg_find_eu_with_vu_in_lu( nmg_find_lu_of_vu(vu), vu )
 *  that was being used in several places.
 */
struct edgeuse *
nmg_find_eu_of_vu(const struct vertexuse *vu)
{
	NMG_CK_VERTEXUSE(vu);
	if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
		return (struct edgeuse *)NULL;
	return vu->up.eu_p;
}

/**
 *			N M G _ F I N D _ E U _ W I T H _ V U _ I N _ L U
 *
 *  Find an edgeuse starting at a given vertexuse within a loopuse.
 */
struct edgeuse *
nmg_find_eu_with_vu_in_lu(const struct loopuse *lu, const struct vertexuse *vu)
{
	register struct edgeuse	*eu;

	NMG_CK_LOOPUSE(lu);
	NMG_CK_VERTEXUSE(vu);
	if( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
		rt_bomb("nmg_find_eu_with_vu_in_lu: loop has no edges!\n");
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
		NMG_CK_EDGEUSE(eu);
		if( eu->vu_p == vu )  return eu;
	}
	rt_bomb("nmg_find_eu_with_vu_in_lu:  Unable to find vu!\n");
	/* NOTREACHED */
	return((struct edgeuse *)NULL);
}

/**				N M G _ F A C E R A D I A L
 *
 *	Looking radially around an edge, find another edge in the same
 *	face as the current edge. (this could be the mate to the current edge)
 */
const struct edgeuse *
nmg_faceradial(const struct edgeuse *eu)
{
	const struct faceuse *fu;
	const struct edgeuse *eur;

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


/**
 *			N M G _ R A D I A L _ F A C E _ E D G E _ I N _ S H E L L
 *
 *	looking radially around an edge, find another edge which is a part
 *	of a face in the same shell
 */
const struct edgeuse *
nmg_radial_face_edge_in_shell(const struct edgeuse *eu)
{
	const struct edgeuse *eur;
	const struct shell	*s;

	NMG_CK_EDGEUSE(eu);
	s = nmg_find_s_of_eu(eu);

	eur = eu->radial_p;
	NMG_CK_EDGEUSE(eur);

	while (eur != eu->eumate_p) {
		if (*eur->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    *eur->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
		    eur->up.lu_p->up.fu_p->s_p == s) {
			break; /* found another face in shell */
		} else {
			eur = eur->eumate_p->radial_p;
			NMG_CK_EDGEUSE(eur);
		}
	}
	return(eur);
}

/**
 *			N M G _ F I N D _ E D G E _ B E T W E E N _ 2 F U
 *
 *  Perform a topology search to determine if two faces (specified by
 *  their faceuses) share an edge in common.  If so, return an edgeuse
 *  in fu1 of that edge.
 *
 *  If there are multiple edgeuses in common, ensure that they all refer
 *  to the same edge_g_lseg geometry structure.  The intersection of two planes
 *  (non-coplanar) must be a single line.
 *
 *  Calling this routine when the two faces share face geometry
 *  and have more than one edge in common gives
 *  a NULL return, as there is no unique answer.
 *
 *  NULL is also returned if no common edge could be found.
 */
const struct edgeuse *
nmg_find_edge_between_2fu(const struct faceuse *fu1, const struct faceuse *fu2, const struct bn_tol *tol)
{
	const struct loopuse	*lu1;
	const struct edgeuse	*ret = (const struct edgeuse *)NULL;
	int			coincident;

	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);
	BN_CK_TOL(tol);

	for( BU_LIST_FOR( lu1, loopuse, &fu1->lu_hd ) )  {
		const struct edgeuse	*eu1;
		NMG_CK_LOOPUSE(lu1);
		if( BU_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC )
			continue;
		for( BU_LIST_FOR( eu1, edgeuse, &lu1->down_hd ) )  {
			const struct edgeuse *eur;

			NMG_CK_EDGEUSE(eu1);
			/* Walk radially around the edge */
			eur = eu1->radial_p;
			NMG_CK_EDGEUSE(eur);

			while (eur != eu1->eumate_p) {
				if (*eur->up.magic_p == NMG_LOOPUSE_MAGIC &&
				    *eur->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
				    eur->up.lu_p->up.fu_p->f_p == fu2->f_p) {
					/* Found the other face on this edge! */
					if( !ret )  {
						/* First common edge found */
						if( eur->up.lu_p->up.fu_p == fu2)  {
							ret = eur;
						} else {
							ret = eur->eumate_p;
						}
					} else {
						/* Previous edge found, check edge_g_lseg */
						if( eur->g.lseg_p != ret->g.lseg_p )  {
							bu_log("eur=x%x, eg_p=x%x;  ret=x%x, eg_p=x%x\n",
								eur, eur->g.lseg_p,
								ret, ret->g.lseg_p );
							nmg_pr_eg( eur->g.magic_p, 0 );
							nmg_pr_eg( ret->g.magic_p, 0 );
							nmg_pr_eu_endpoints( eur, 0 );
							nmg_pr_eu_endpoints( ret, 0 );

							coincident = nmg_2edgeuse_g_coincident( eur, ret, tol );
							if( coincident )  {
								/* Change eur to use ret's eg */
								bu_log("nmg_find_edge_between_2fu() belatedly fusing e1=x%x, eg1=x%x, e2=x%x, eg2=x%x\n",
									eur->e_p, eur->g.lseg_p,
									ret->e_p, ret->g.lseg_p );
								nmg_jeg( ret->g.lseg_p, eur->g.lseg_p );
								/* See if there are any others. */
								nmg_model_fuse( nmg_find_model(&eur->l.magic), tol );
							} else {
								rt_bomb("nmg_find_edge_between_2fu() 2 faces intersect with differing edge geometries?\n");
							}
						}
					}
				}
				/* Advance to next */
				eur = eur->eumate_p->radial_p;
				NMG_CK_EDGEUSE(eur);
			}
		}
	}
	if (rt_g.NMG_debug & DEBUG_BASIC)  bu_log("nmg_find_edge_between_2fu(fu1=x%x, fu2=x%x) edgeuse=x%x\n", fu1, fu2, ret);
	return ret;

}

/**
 *  Support for nmg_find_e_nearest_pt2().
 */
struct fen2d_state {
	char		*visited;	/* array of edges already visited */
	fastf_t		mindist;	/* current min dist */
	struct edge	*ep;		/* closest edge */
	mat_t		mat;
	point_t		pt2;
	const struct bn_tol	*tol;
};

static void
nmg_find_e_pt2_handler(long int *lp, genptr_t state, int first)
{
	register struct fen2d_state	*sp = (struct fen2d_state *)state;
	register struct edge		*e = (struct edge *)lp;
	fastf_t				dist_sq;
	point_t				a2, b2;
	struct vertex			*va, *vb;
	point_t				pca;
	int				code;

	BN_CK_TOL(sp->tol);
	NMG_CK_EDGE(e);

	/* If this edge has been processed before, do nothing more */
	if( !NMG_INDEX_FIRST_TIME(sp->visited, e) )  return;

	va = e->eu_p->vu_p->v_p;
	vb = e->eu_p->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX(va);
	NMG_CK_VERTEX(vb);

	MAT4X3PNT( a2, sp->mat, va->vg_p->coord );
	MAT4X3PNT( b2, sp->mat, vb->vg_p->coord );

	code = bn_dist_pt2_lseg2( &dist_sq, pca, a2, b2, sp->pt2, sp->tol );

	if( code == 0 )  dist_sq = 0;

	if( dist_sq < sp->mindist )  {
		sp->mindist = dist_sq;
		sp->ep = e;
	}
}

/**
 *			N M G _ F I N D _ E _ N E A R E S T _ P T 2
 *
 *  A geometric search routine to find the edge that is neaest to
 *  the given point, when all edges are projected into 2D using
 *  the matrix 'mat'.
 *  Useful for finding the edge nearest a mouse click, for example.
 */
struct edge *
nmg_find_e_nearest_pt2(long int *magic_p, const fastf_t *pt2, const fastf_t *mat, const struct bn_tol *tol)

				/* 2d point */
				/* 3d to 3d xform */

{
	struct model			*m;
	struct fen2d_state		st;
	static const struct nmg_visit_handlers htab = {NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, nmg_find_e_pt2_handler, NULL,
						       NULL, NULL, NULL, NULL, NULL};
	/* htab.vis_edge = nmg_find_e_pt2_handler; */

	BN_CK_TOL(tol);
	m = nmg_find_model( magic_p );
	NMG_CK_MODEL(m);

	st.visited = (char *)bu_calloc(m->maxindex+1, sizeof(char), "visited[]");
	st.mindist = INFINITY;
	VMOVE( st.pt2, pt2 );
	MAT_COPY( st.mat, mat );
	st.ep = (struct edge *)NULL;
	st.tol = tol;

	nmg_visit( magic_p, &htab, (genptr_t)&st );

	bu_free( (char *)st.visited, "visited[]" );

	if( st.ep )  {
		NMG_CK_EDGE(st.ep);
		return st.ep;
	}
	return (struct edge *)NULL;
}

/**
 *			N M G _ E U _ 2 V E C S _ P E R P
 *
 *  Given an edgeuse, return two arbitrary unit-length vectors which
 *  are perpendicular to each other and to the edgeuse, such that
 *  they can be considered the +X and +Y axis, and the edgeuse is +Z.
 *  That is, X cross Y = Z.
 *
 *  Useful for erecting a coordinate system around an edge suitable
 *  for measuring the angles of other edges and faces with.
 */
void
nmg_eu_2vecs_perp(fastf_t *xvec, fastf_t *yvec, fastf_t *zvec, const struct edgeuse *eu, const struct bn_tol *tol)
{
	const struct vertex	*v1, *v2;
	fastf_t			len;

	NMG_CK_EDGEUSE(eu);
	v1 = eu->vu_p->v_p;
	NMG_CK_VERTEX(v1);
	v2 = eu->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX(v2);
	if( v1 == v2 )  rt_bomb("nmg_eu_2vecs_perp() start&end vertex of edge are the same!\n");
	BN_CK_TOL(tol);

	NMG_CK_VERTEX_G(v1->vg_p);
	NMG_CK_VERTEX_G(v2->vg_p);
	VSUB2( zvec, v2->vg_p->coord, v1->vg_p->coord );
	len = MAGNITUDE(zvec);
	/* See if v1 == v2, within tol */
	if( len < tol->dist )  rt_bomb("nmg_eu_2vecs_perp(): 0-length edge (geometry)\n");
	len = 1 / len;
	VSCALE( zvec, zvec, len );

	bn_vec_perp( xvec, zvec );
	VCROSS( yvec, zvec, xvec );
}

/**
 *			N M G _ F I N D _ E U _ L E F T V E C
 *
 *  Given an edgeuse, if it is part of a faceuse, return the inward pointing
 *  "left" vector which points into the interior of this loop, and
 *  lies in the plane of the face. The left vector is unitized.
 *
 *  This routine depends on the vertex ordering in an OT_SAME loopuse being
 *  properly CCW for exterior loops, and CW for interior (hole) loops.
 *
 *  Returns -
 *	-1	if edgeuse is not part of a faceuse.
 *	 0	if left vector successfully computed into caller's array.
 */
int
nmg_find_eu_leftvec(fastf_t *left, const struct edgeuse *eu)
{
	const struct loopuse	*lu;
	const struct faceuse	*fu;
	vect_t			Norm;
	vect_t			edgevect;
	fastf_t			dot;
	struct vertex_g		*vg1;
	struct vertex_g		*vg2;
	fastf_t			edge_len_sq;
	fastf_t			sin_sq;

	NMG_CK_EDGEUSE(eu);
	if( *eu->up.magic_p != NMG_LOOPUSE_MAGIC )  return -1;
	lu = eu->up.lu_p;
	NMG_CK_LOOPUSE(lu);
	if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )  return -1;
	fu = lu->up.fu_p;
	NMG_CK_FACEUSE(fu);
	NMG_CK_FACE(fu->f_p);
	NMG_CK_FACE_G_PLANE(fu->f_p->g.plane_p);

	vg1 = eu->vu_p->v_p->vg_p;
	vg2 = eu->eumate_p->vu_p->v_p->vg_p;

	/* Get unit length Normal vector for edgeuse's faceuse */
	NMG_GET_FU_NORMAL( Norm, fu );

	VSUB2( edgevect, vg2->coord, vg1->coord );
	edge_len_sq = MAGSQ( edgevect );

	dot = VDOT( edgevect, Norm );
	sin_sq = 1.0 - (dot * dot / edge_len_sq);

	if( NEAR_ZERO( sin_sq, 0.000001) )	/* we don't have a tol structure available XXX */
	{
		const struct edgeuse *eu_next;
		const struct edgeuse *eu_prev;
		vect_t next_left;
		vect_t prev_left;
		vect_t other_edge;
		int other_edge_is_parallel=1;

		bu_log( "WARNING: eu x%x (%f %f %f) parallel to normal (%f %f %f)\n", eu, V3ARGS( edgevect ), V3ARGS( Norm ) );

		eu_next = eu;
		while( other_edge_is_parallel )
		{
			eu_next = BU_LIST_PNEXT_CIRC( edgeuse, &eu_next->l );
			if( eu_next == eu )
				break;
			if( NMG_ARE_EUS_ADJACENT( eu, eu_next ) )
				continue;
			VSUB2( other_edge, eu_next->eumate_p->vu_p->v_p->vg_p->coord,
				eu_next->vu_p->v_p->vg_p->coord );
			VUNITIZE( other_edge );
			dot = 1.0 - fabs( VDOT( other_edge, Norm ) );
			if( dot < .5 )
				other_edge_is_parallel = 0;
		}
		if( other_edge_is_parallel )
		{
			bu_log( "Cannot find edge (starting eu =x%x) that is not parallel to face normal!!!\n", eu );
			nmg_pr_fu_briefly( fu, (char *)NULL );
			rt_bomb( "Cannot find edge that is not parallel to face normal!!!\n" );
		}

		VCROSS( next_left, Norm, other_edge );
		VUNITIZE( next_left );

		eu_prev = eu;
		other_edge_is_parallel = 1;
		while( other_edge_is_parallel )
		{
			eu_prev = BU_LIST_PPREV_CIRC( edgeuse, &eu_prev->l );
			if( eu_prev == eu )
				break;
			if( NMG_ARE_EUS_ADJACENT( eu, eu_prev ) )
				continue;
			VSUB2( other_edge, eu_prev->eumate_p->vu_p->v_p->vg_p->coord,
				eu_prev->vu_p->v_p->vg_p->coord );
			VUNITIZE( other_edge );
			dot = 1.0 - fabs( VDOT( other_edge, Norm ) );
			if( dot < .5 )
				other_edge_is_parallel = 0;
		}
		if( other_edge_is_parallel )
		{
			bu_log( "Cannot find edge (starting eu =x%x) that is not parallel to face normal!!!\n", eu );
			nmg_pr_fu_briefly( fu, (char *)NULL );
			rt_bomb( "Cannot find edge that is not parallel to face normal!!!\n" );
		}

		VCROSS( prev_left, Norm, other_edge );
		VUNITIZE( prev_left );

		VBLEND2( left, 0.5, next_left, 0.5, prev_left );
		VUNITIZE( left );
#if 0
		bu_log( "\t eu_prev (%g %g %g)<->(%g %g %g)\n",
			V3ARGS( eu_prev->vu_p->v_p->vg_p->coord ),
			V3ARGS( eu_prev->eumate_p->vu_p->v_p->vg_p->coord ) );
		bu_log( "\t eu_next (%g %g %g)<->(%g %g %g)\n",
			V3ARGS( eu_next->vu_p->v_p->vg_p->coord ),
			V3ARGS( eu_next->eumate_p->vu_p->v_p->vg_p->coord ) );
		bu_log( "\tprev_left=(%g %g %g), next_left=(%g %g %g)\n", V3ARGS( prev_left ), V3ARGS( next_left ) );
		bu_log( "\tUnitized left=(%f %f %f)\n", V3ARGS( left ) );

		nmg_pr_fu_briefly( fu, "" );
#endif
		return 0;
	}

	VCROSS( left, Norm, edgevect );
	if (rt_g.NMG_debug & DEBUG_MESH_EU ) {
		vect_t edge_unit;
		vect_t norm_x_edge;

		VMOVE( edge_unit, edgevect );
		VUNITIZE( edge_unit );
		VCROSS( norm_x_edge, Norm, edge_unit );
		bu_log( "for eu x%x from fu x%x v1=x%x, v2=x%x:\n", eu, fu, eu->vu_p->v_p, eu->eumate_p->vu_p->v_p );
		bu_log( "\t(%.10f %.10f %.10f) <-> (%.10f %.10f %.10f)\n", V3ARGS( eu->vu_p->v_p->vg_p->coord ), V3ARGS( eu->eumate_p->vu_p->v_p->vg_p->coord ) );
		bu_log( "\tedge dot norm = %.10f\n", VDOT( edge_unit, Norm ) );
		bu_log( "\tnorm X edge = (%.10f %.10f %.10f)\n", V3ARGS( norm_x_edge ) );
		bu_log( "\tnorm=(%.10f %.10f %.10f), edgevect=(%.10f %.10f %.10f), left=(%.10f %.10f %.10f )\n",
			V3ARGS( Norm ), V3ARGS( edgevect ), V3ARGS( left ) );
	}
	VUNITIZE( left );
	if (rt_g.NMG_debug & DEBUG_MESH_EU ) {
		bu_log( "\tUnitized left=(%f %f %f)\n", V3ARGS( left ) );
	}
	return 0;
}

/**
 *		N M G _ F I N D _ E U _ L E F T _ N O N _ U N I T
 *
 *  Given an edgeuse, if it is part of a faceuse, return the inward pointing
 *  "left" vector which points into the interior of this loop, and
 *  lies in the plane of the face. The left vector is not unitized.
 *
 *  This routine depends on the vertex ordering in an OT_SAME loopuse being
 *  properly CCW for exterior loops, and CW for interior (hole) loops.
 *
 *  Returns -
 *	-1	if edgeuse is not part of a faceuse.
 *	 0	if left vector successfully computed into caller's array.
 */
int
nmg_find_eu_left_non_unit(fastf_t *left, const struct edgeuse *eu)
{
	const struct loopuse	*lu;
	const struct faceuse	*fu;
	vect_t			Norm;
	vect_t			edgevect;
	pointp_t		p1,p2;

	NMG_CK_EDGEUSE(eu);
	if( *eu->up.magic_p != NMG_LOOPUSE_MAGIC )  return -1;
	lu = eu->up.lu_p;
	if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )  return -1;
	fu = lu->up.fu_p;

	/* Get unit length Normal vector for edgeuse's faceuse */
	NMG_GET_FU_NORMAL( Norm, fu );

	/* Get vector in direction of edge */
	p1 = eu->vu_p->v_p->vg_p->coord;
	p2 = eu->eumate_p->vu_p->v_p->vg_p->coord;
	VSUB2( edgevect, p2, p1 );

	/* left vector is cross-product of face normal and edge direction */
	VCROSS( left, Norm, edgevect );
	return 0;
}
/**
 *			N M G _ F I N D _ O T _ S A M E _ E U _ O F _ E
 *
 *  If there is an edgeuse of an OT_SAME faceuse on this edge, return it.
 *  Only return a wire edgeuse if that is all there is.
 *  Useful for selecting a "good" edgeuse to pass to nmg_eu_2vecs_perp().
 */
struct edgeuse *
nmg_find_ot_same_eu_of_e(const struct edge *e)
{
	register struct edgeuse	*eu1;
	register struct edgeuse	*eu;
	struct faceuse		*fu;

	NMG_CK_EDGE(e);
	eu = eu1 = e->eu_p;
	do  {
		fu = nmg_find_fu_of_eu(eu);
		if( fu && fu->orientation == OT_SAME )  return eu;

		fu = nmg_find_fu_of_eu(eu->eumate_p);
		if( fu && fu->orientation == OT_SAME )  return eu->eumate_p;
		eu = eu->radial_p->eumate_p;
	} while( eu != eu1 );
	return eu1;		/* All wire */
}

/************************************************************************
 *									*
 *				VERTEX Routines				*
 *									*
 ************************************************************************/

/**
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
nmg_find_v_in_face(const struct vertex *v, const struct faceuse *fu)
{
	struct vertexuse *vu;
	struct edgeuse *eu;
	struct loopuse *lu;

	NMG_CK_VERTEX(v);

	for( BU_LIST_FOR( vu, vertexuse, &v->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC) {
			eu = vu->up.eu_p;
			if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
				lu = eu->up.lu_p;
				if (*lu->up.magic_p == NMG_FACEUSE_MAGIC && lu->up.fu_p == fu)
					return(vu);
			}

		} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
			lu = vu->up.lu_p;
			if (*lu->up.magic_p == NMG_FACEUSE_MAGIC && lu->up.fu_p == fu)
				return(vu);
		}
	}
	return((struct vertexuse *)NULL);
}

/**
 *			N M G _ F I N D _ V _ I N _ S H E L L
 *
 *  Search shell "s" for a vertexuse that refers to vertex "v".
 *  For efficiency, the search is done on the uses of "v".
 *
 *  If "edges_only" is set, only a vertexuse from an edgeuse will
 *  be returned, otherwise, vu's from self-loops and lone-shell-vu's
 *  are also candidates.
 */
struct vertexuse *
nmg_find_v_in_shell(const struct vertex *v, const struct shell *s, int edges_only)
{
	struct vertexuse	*vu;

	for( BU_LIST_FOR( vu, vertexuse, &v->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);

		if( *vu->up.magic_p == NMG_LOOPUSE_MAGIC )  {
			if( edges_only )  continue;
			if( nmg_find_s_of_lu( vu->up.lu_p ) == s )
				return vu;
			continue;
		}
		if( *vu->up.magic_p == NMG_SHELL_MAGIC )  {
			if( edges_only )  continue;
			if( vu->up.s_p == s )
				return vu;
			continue;
		}
		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
			rt_bomb("nmg_find_v_in_shell(): bad vu up ptr\n");

		/* vu is being used by an edgeuse */
		if( nmg_find_s_of_eu( vu->up.eu_p ) == s )
			return vu;
	}
	return (struct vertexuse *)NULL;
}

/**
 *			N M G _ F I N D _ P T _ I N _ L U
 *
 *  Conduct a geometric search for a vertex in loopuse 'lu' which is
 *  "identical" to the given point, within the specified tolerance.
 *  The loopuse may be part of a face, or it may be wires.
 *
 *  Returns -
 *	NULL			No vertex matched
 *	(struct vertexuse *)	A matching vertexuse from that loopuse.
 */
struct vertexuse *
nmg_find_pt_in_lu(const struct loopuse *lu, const fastf_t *pt, const struct bn_tol *tol)
{
	struct edgeuse		*eu;
	vect_t			delta;
	struct vertex		*v;
	register struct vertex_g *vg;
	int			magic1;

	magic1 = BU_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		struct vertexuse	*vu;
		vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
		v = vu->v_p;
		NMG_CK_VERTEX(v);
		if( !(vg = v->vg_p) )
			return ((struct vertexuse *)NULL);
		NMG_CK_VERTEX_G(vg);
		VSUB2(delta, vg->coord, pt);
		if ( MAGSQ(delta) <= tol->dist_sq)
			return(vu);
		return ((struct vertexuse *)NULL);
	}
	if (magic1 != NMG_EDGEUSE_MAGIC) {
		rt_bomb("nmg_find_pt_in_lu() Bogus child of loop\n");
	}

	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
		v = eu->vu_p->v_p;
		NMG_CK_VERTEX(v);
		if( !(vg = v->vg_p) )  continue;
		NMG_CK_VERTEX_G(vg);
		VSUB2(delta, vg->coord, pt);
		if ( MAGSQ(delta) <= tol->dist_sq)
			return(eu->vu_p);
	}
	return ((struct vertexuse *)NULL);

}

/**
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
nmg_find_pt_in_face(const struct faceuse *fu, const fastf_t *pt, const struct bn_tol *tol)
{
	register struct loopuse	*lu;
	struct vertexuse	*vu;

	NMG_CK_FACEUSE(fu);
	BN_CK_TOL(tol);

	for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		if( (vu = nmg_find_pt_in_lu(lu, pt, tol)) )
			return vu;
	}
	return ((struct vertexuse *)NULL);
}

/**
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
 *
 *  XXX Why does this return a vertex, while it's helpers return a vertexuse?
 */
struct vertex *
nmg_find_pt_in_shell(const struct shell *s, const fastf_t *pt, const struct bn_tol *tol)
{
	const struct faceuse	*fu;
	const struct loopuse	*lu;
	const struct edgeuse	*eu;
	const struct vertexuse	*vu;
	struct vertex		*v;
	const struct vertex_g	*vg;
	vect_t		delta;

	NMG_CK_SHELL(s);
	BN_CK_TOL(tol);

	fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
	while (BU_LIST_NOT_HEAD(fu, &s->fu_hd) ) {
		/* Shell has faces */
		NMG_CK_FACEUSE(fu);
		if( (vu = nmg_find_pt_in_face( fu, pt, tol )) )
			return(vu->v_p);

		if (BU_LIST_PNEXT(faceuse, fu) == fu->fumate_p)
			fu = BU_LIST_PNEXT_PNEXT(faceuse, fu);
		else
			fu = BU_LIST_PNEXT(faceuse, fu);
	}

	/* Wire loopuses */
	lu = BU_LIST_FIRST(loopuse, &s->lu_hd);
	while (BU_LIST_NOT_HEAD(lu, &s->lu_hd) ) {
		NMG_CK_LOOPUSE(lu);
		if( (vu = nmg_find_pt_in_lu(lu, pt, tol)) )
			return vu->v_p;

		if (BU_LIST_PNEXT(loopuse, lu) == lu->lumate_p)
			lu = BU_LIST_PNEXT_PNEXT(loopuse, lu);
		else
			lu = BU_LIST_PNEXT(loopuse, lu);
	}

	/* Wire edgeuses */
	for (BU_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_VERTEXUSE(eu->vu_p);
		v = eu->vu_p->v_p;
		NMG_CK_VERTEX(v);
		if( (vg = v->vg_p) )  {
			NMG_CK_VERTEX_G(vg);
			VSUB2( delta, vg->coord, pt );
			if( MAGSQ(delta) <= tol->dist_sq )
				return(v);
		}
	}

	/* Lone vertexuse */
	if (s->vu_p) {
		NMG_CK_VERTEXUSE(s->vu_p);
		v = s->vu_p->v_p;
		NMG_CK_VERTEX(v);
		if( (vg = v->vg_p) )  {
			NMG_CK_VERTEX_G( vg );
			VSUB2( delta, vg->coord, pt );
			if( MAGSQ(delta) <= tol->dist_sq )
				return(v);
		}
	}
	return( (struct vertex *)0 );
}

/**
 *			N M G _ F I N D _ P T _ I N _ M O D E L
 *
 *  Brute force search of the entire model to find a vertex that
 *  matches this point.
 *  XXX Shouldn't this return the _closest_ match, not just the
 *  XXX first match within tolerance?
 */
struct vertex *
nmg_find_pt_in_model(const struct model *m, const fastf_t *pt, const struct bn_tol *tol)
{
	struct nmgregion	*r;
	struct shell		*s;
	struct vertex		*v;

	NMG_CK_MODEL(m);
	BN_CK_TOL(tol);

	for( BU_LIST_FOR( r, nmgregion, &m->r_hd ) )  {
		NMG_CK_REGION(r);
		for( BU_LIST_FOR( s, shell, &r->s_hd ) )  {
			NMG_CK_SHELL(s);
			if( (v = nmg_find_pt_in_shell( s, pt, tol )) )  {
				NMG_CK_VERTEX(v);
				return v;
			}
		}
	}
	return (struct vertex *)NULL;
}

/**
 *			N M G _ I S _ V E R T E X _ I N _ E D G E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_vertex_in_edgelist(register const struct vertex *v, const struct bu_list *hd)
{
	register const struct edgeuse	*eu;

	NMG_CK_VERTEX(v);
	for( BU_LIST_FOR( eu, edgeuse, hd ) )  {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_VERTEXUSE(eu->vu_p);
		NMG_CK_VERTEX(eu->vu_p->v_p);
		if( eu->vu_p->v_p == v )  return(1);
	}
	return(0);
}

/**
 *			N M G _ I S _ V E R T E X _ I N _ L O O P L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_vertex_in_looplist(register const struct vertex *v, const struct bu_list *hd, int singletons)
{
	register const struct loopuse	*lu;
	long			magic1;

	NMG_CK_VERTEX(v);
	for( BU_LIST_FOR( lu, loopuse, hd ) )  {
		NMG_CK_LOOPUSE(lu);
		magic1 = BU_LIST_FIRST_MAGIC( &lu->down_hd );
		if( magic1 == NMG_VERTEXUSE_MAGIC )  {
			register const struct vertexuse	*vu;
			if( !singletons )  continue;
			vu = BU_LIST_FIRST(vertexuse, &lu->down_hd );
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

/**
 *			N M G _ I S _ V E R T E X _ I N _ F A C E
 *
 *  Returns -
 *	vu	One use of vertex 'v' in face 'f'.
 *	NULL	If there are no uses of 'v' in 'f'.
 */
struct vertexuse *
nmg_is_vertex_in_face(const struct vertex *v, const struct face *f)
{
	struct vertexuse	*vu;

	NMG_CK_VERTEX(v);
	NMG_CK_FACE(f);

	for( BU_LIST_FOR( vu, vertexuse, &v->vu_hd ) )  {
		register const struct edgeuse	*eu;
		register const struct loopuse	*lu;
		register const struct faceuse	*fu;

		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
		if( *(eu = vu->up.eu_p)->up.magic_p != NMG_LOOPUSE_MAGIC )  continue;
		lu = eu->up.lu_p;
		if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )  continue;
		fu = lu->up.fu_p;
		if( fu->f_p != f )  continue;
		return vu;
	}
	return (struct vertexuse *)NULL;
}

/**
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
nmg_is_vertex_a_selfloop_in_shell(const struct vertex *v, const struct shell *s)
{
	const struct vertexuse *vu;

	NMG_CK_VERTEX(v);
	NMG_CK_SHELL(s);

	/* try to find the vertex in a loopuse of this shell */
	for (BU_LIST_FOR(vu, vertexuse, &v->vu_hd)) {
		register const struct loopuse	*lu;
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

/**
 *			N M G _ I S _ V E R T E X _ I N _ F A C E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_vertex_in_facelist(register const struct vertex *v, const struct bu_list *hd)
{
	register const struct faceuse	*fu;

	NMG_CK_VERTEX(v);
	for( BU_LIST_FOR( fu, faceuse, hd ) )  {
		NMG_CK_FACEUSE(fu);
		if( nmg_is_vertex_in_looplist( v, &fu->lu_hd, 1 ) )
			return(1);
	}
	return(0);
}

/**
 *			N M G _ I S _ E D G E _ I N _ E D G E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_edge_in_edgelist(const struct edge *e, const struct bu_list *hd)
{
	register const struct edgeuse	*eu;

	NMG_CK_EDGE(e);
	for( BU_LIST_FOR( eu, edgeuse, hd ) )  {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_EDGE(eu->e_p);
		if( e == eu->e_p )  return(1);
	}
	return(0);
}

/**
 *			N M G _ I S _ E D G E _ I N _ L O O P L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_edge_in_looplist(const struct edge *e, const struct bu_list *hd)
{
	register const struct loopuse	*lu;
	long			magic1;

	NMG_CK_EDGE(e);
	for( BU_LIST_FOR( lu, loopuse, hd ) )  {
		NMG_CK_LOOPUSE(lu);
		magic1 = BU_LIST_FIRST_MAGIC( &lu->down_hd );
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

/**
 *			N M G _ I S _ E D G E _ I N _ F A C E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_edge_in_facelist(const struct edge *e, const struct bu_list *hd)
{
	register const struct faceuse	*fu;

	NMG_CK_EDGE(e);
	for( BU_LIST_FOR( fu, faceuse, hd ) )  {
		NMG_CK_FACEUSE(fu);
		if( nmg_is_edge_in_looplist( e, &fu->lu_hd ) )
			return(1);
	}
	return(0);
}

/**
 *			N M G _ I S _ L O O P _ I N _ F A C E L I S T
 *
 *  Returns -
 *	1	If found
 *	0	If not found
 */
int
nmg_is_loop_in_facelist(const struct loop *l, const struct bu_list *fu_hd)
{
	register const struct faceuse	*fu;
	register const struct loopuse	*lu;

	NMG_CK_LOOP(l);
	for( BU_LIST_FOR( fu, faceuse, fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
			NMG_CK_LOOPUSE(lu);
			NMG_CK_LOOP(lu->l_p);
			if( l == lu->l_p )  return(1);
		}
	}
	return(0);
}

/************************************************************************
 *									*
 *				Tabulation Routines			*
 *									*
 ************************************************************************/

struct vf_state {
	char		*visited;
	struct bu_ptbl	*tabl;
};

/**
 *			N M G _ 2 R V F _ H A N D L E R
 *
 *  A private support routine for nmg_vertex_tabulate().
 *  Having just visited a vertex, if this is the first time,
 *  add it to the bu_ptbl array.
 */
static void
nmg_2rvf_handler(long int *vp, genptr_t state, int first)
{
	register struct vf_state *sp = (struct vf_state *)state;
	register struct vertex	*v = (struct vertex *)vp;

	NMG_CK_VERTEX(v);
	/* If this vertex has been processed before, do nothing more */
	if( !NMG_INDEX_FIRST_TIME(sp->visited, v) )  return;

	bu_ptbl_ins( sp->tabl, vp );
}

/**
 *			N M G _ V E R T E X _ T A B U L A T E
 *
 *  Given a pointer to any nmg data structure,
 *  build an bu_ptbl list which has every vertex
 *  pointer from there on "down" in the model, each one listed exactly once.
 */
void
nmg_vertex_tabulate(struct bu_ptbl *tab, const long int *magic_p)
{
	struct model		*m;
	struct vf_state		st;
	static const struct nmg_visit_handlers	handlers = {NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, nmg_2rvf_handler, NULL};
	/* handlers.vertex = nmg_2rvf_handler; */

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL(m);

	st.visited = (char *)bu_calloc(m->maxindex+1, sizeof(char), "visited[]");
	st.tabl = tab;

	(void)bu_ptbl_init( tab, 64, " tab");

	nmg_visit( magic_p, &handlers, (genptr_t)&st );

	bu_free( (char *)st.visited, "visited[]");
}

/**
 *			N M G _ V E R T _ A _ H A N D L E R
 *
 *  A private support routine for nmg_vertexuse_normal_tabulate().
 *  Having just visited a vertexuse-a, if this is the first time,
 *  add it to the bu_ptbl array.
 */
static void
nmg_vert_a_handler(long int *vp, genptr_t state, int first)
{
	register struct vf_state *sp = (struct vf_state *)state;
	register struct vertexuse_a_plane	*va;

	if( *vp != NMG_VERTEXUSE_A_PLANE_MAGIC )
		return;

	va = (struct vertexuse_a_plane *)vp;
	/* If this vertex normal has been processed before, do nothing more */
	if( !NMG_INDEX_FIRST_TIME(sp->visited, va) )  return;

	bu_ptbl_ins( sp->tabl, vp );
}

/**
 *			N M G _ V E R T E X U S E_ N O R M A L _ T A B U L A T E
 *
 *  Given a pointer to any nmg data structure,
 *  build an bu_ptbl list which has every vertexuse normal
 *  pointer from there on "down" in the model, each one listed exactly once.
 */
void
nmg_vertexuse_normal_tabulate(struct bu_ptbl *tab, const long int *magic_p)
{
	struct model		*m;
	struct vf_state		st;
	static const struct nmg_visit_handlers	handlers = {NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, nmg_vert_a_handler, NULL, NULL};
	/* handlers.vis_vertexuse_a = nmg_vert_a_handler; */

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL(m);

	st.visited = (char *)bu_calloc(m->maxindex+1, sizeof(char), "visited[]");
	st.tabl = tab;

	(void)bu_ptbl_init( tab, 64, " tab");

	nmg_visit( magic_p, &handlers, (genptr_t)&st );

	bu_free( (char *)st.visited, "visited[]");
}

/**
 *			N M G _ 2 E D G E U S E _ H A N D L E R
 *
 *  A private support routine for nmg_edgeuse_tabulate().
 *  Having just visited a edgeuse, if this is the first time,
 *  add it to the bu_ptbl array.
 */
static void
nmg_2edgeuse_handler(long int *eup, genptr_t state, int first)
{
	register struct vf_state *sp = (struct vf_state *)state;
	register struct edgeuse	*eu = (struct edgeuse *)eup;

	NMG_CK_EDGEUSE(eu);
	/* If this edgeuse has been processed before, do nothing more */
	if( !NMG_INDEX_FIRST_TIME(sp->visited, eu) )  return;

	bu_ptbl_ins( sp->tabl, eup );
}

/**
 *			N M G _ E D G E U S E _ T A B U L A T E
 *
 *  Given a pointer to any nmg data structure,
 *  build an bu_ptbl list which has every edgeuse
 *  pointer from there on "down" in the model, each one listed exactly once.
 */
void
nmg_edgeuse_tabulate(struct bu_ptbl *tab, const long int *magic_p)
{
	struct model		*m;
	struct vf_state		st;
	static const struct nmg_visit_handlers	handlers = {NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, nmg_2edgeuse_handler, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL};
	/* handlers.bef_edgeuse = nmg_2edgeuse_handler; */

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL(m);

	st.visited = (char *)bu_calloc(m->maxindex+1, sizeof(char), "visited[]");
	st.tabl = tab;

	(void)bu_ptbl_init( tab, 64, " tab");

	nmg_visit( magic_p, &handlers, (genptr_t)&st );

	bu_free( (char *)st.visited, "visited[]");
}

/**
 *			N M G _ 2 E D G E _ H A N D L E R
 *
 *  A private support routine for nmg_edge_tabulate().
 *  Having just visited a edge, if this is the first time,
 *  add it to the bu_ptbl array.
 */
static void
nmg_2edge_handler(long int *ep, genptr_t state, int first)
{
	register struct vf_state *sp = (struct vf_state *)state;
	register struct edge	*e = (struct edge *)ep;

	NMG_CK_EDGE(e);
	/* If this edge has been processed before, do nothing more */
	if( !NMG_INDEX_FIRST_TIME(sp->visited, e) )  return;

	bu_ptbl_ins( sp->tabl, ep );
}

/**
 *			N M G _ E D G E _ T A B U L A T E
 *
 *  Given a pointer to any nmg data structure,
 *  build an bu_ptbl list which has every edge
 *  pointer from there on "down" in the model, each one listed exactly once.
 */
void
nmg_edge_tabulate(struct bu_ptbl *tab, const long int *magic_p)
{
	struct model		*m;
	struct vf_state		st;
	static const struct nmg_visit_handlers	handlers = {NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, nmg_2edge_handler, NULL,
							    NULL, NULL, NULL, NULL, NULL};
	/* handlers.vis_edge = nmg_2edge_handler; */

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL(m);

	st.visited = (char *)bu_calloc(m->maxindex+1, sizeof(char), "visited[]");
	st.tabl = tab;

	(void)bu_ptbl_init( tab, 64, " tab");

	nmg_visit( magic_p, &handlers, (genptr_t)&st );

	bu_free( (char *)st.visited, "visited[]");
}

/**
 *			N M G _ E D G E _ G _ H A N D L E R
 *
 *  A private support routine for nmg_edge_g_tabulate().
 *  Having just visited an edge_g_lseg, if this is the first time,
 *  add it to the bu_ptbl array.
 */
static void
nmg_edge_g_handler(long int *ep, genptr_t state, int first)
{
	register struct vf_state *sp = (struct vf_state *)state;

	NMG_CK_EDGE_G_EITHER(ep);

	/* If this edge has been processed before, do nothing more */
	switch( *ep )  {
	case NMG_EDGE_G_LSEG_MAGIC:
		if( !NMG_INDEX_FIRST_TIME(sp->visited, ((struct edge_g_lseg *)ep)) )
			return;
		break;
	case NMG_EDGE_G_CNURB_MAGIC:
		if( !NMG_INDEX_FIRST_TIME(sp->visited, ((struct edge_g_cnurb *)ep)) )
			return;
		break;
	}

	bu_ptbl_ins( sp->tabl, ep );
}

/**
 *			N M G _ E D G E _ G _ T A B U L A T E
 *
 *  Given a pointer to any nmg data structure,
 *  build an bu_ptbl list which has every edge
 *  pointer from there on "down" in the model, each one listed exactly once.
 */
void
nmg_edge_g_tabulate(struct bu_ptbl *tab, const long int *magic_p)
{
	struct model		*m;
	struct vf_state		st;
	static const struct nmg_visit_handlers	handlers = {NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, nmg_edge_g_handler,
							    NULL, NULL, NULL, NULL, NULL};
	/* handlers.vis_edge_g = nmg_edge_g_handler; */

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL(m);

	st.visited = (char *)bu_calloc(m->maxindex+1, sizeof(char), "visited[]");
	st.tabl = tab;

	(void)bu_ptbl_init( tab, 64, " tab");

	nmg_visit( magic_p, &handlers, (genptr_t)&st );

	bu_free( (char *)st.visited, "visited[]");
}

/**
 *			N M G _ 2 F A C E _ H A N D L E R
 *
 *  A private support routine for nmg_face_tabulate().
 *  Having just visited a face, if this is the first time,
 *  add it to the bu_ptbl array.
 */
static void
nmg_2face_handler(long int *fp, genptr_t state, int first)
{
	register struct vf_state *sp = (struct vf_state *)state;
	register struct face	*f = (struct face *)fp;

	NMG_CK_FACE(f);
	/* If this face has been processed before, do nothing more */
	if( !NMG_INDEX_FIRST_TIME(sp->visited, f) )  return;

	bu_ptbl_ins( sp->tabl, fp );
}

/**
 *			N M G _ F A C E _ T A B U L A T E
 *
 *  Given a pointer to any nmg data structure,
 *  build an bu_ptbl list which has every face
 *  pointer from there on "down" in the model, each one listed exactly once.
 */
void
nmg_face_tabulate(struct bu_ptbl *tab, const long int *magic_p)
{
	struct model		*m;
	struct vf_state		st;
	static const struct nmg_visit_handlers	handlers = {NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    nmg_2face_handler, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL};
	/* handlers.vis_face = nmg_2face_handler; */

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL(m);

	st.visited = (char *)bu_calloc(m->maxindex+1, sizeof(char), "visited[]");
	st.tabl = tab;

	(void)bu_ptbl_init( tab, 64, " tab");

	nmg_visit( magic_p, &handlers, (genptr_t)&st );

	bu_free( (char *)st.visited, "visited[]");
}

/**
 *			N M G _ E D G E U S E _ W I T H _ E G _ T A B U L A T E
 *
 *  Build an bu_ptbl list which cites every edgeuse
 *  pointer that uses edge geometry "eg".
 */
void
nmg_edgeuse_with_eg_tabulate(struct bu_ptbl *tab, const struct edge_g_lseg *eg)

					/* can also be edge_g_cnurb */
{
	struct bu_list	*midway;	/* &eu->l2, midway into edgeuse */
	struct edgeuse	*eu;

	NMG_CK_EDGE_G_EITHER(eg);
	(void)bu_ptbl_init( tab, 64, " tab");

	for( BU_LIST_FOR( midway, bu_list, &eg->eu_hd2 ) )  {
		NMG_CKMAG(midway, NMG_EDGEUSE2_MAGIC, "edgeuse2 [l2]");
		eu = BU_LIST_MAIN_PTR( edgeuse, midway, l2 );
		NMG_CK_EDGEUSE(eu);
		if( eu->g.lseg_p != eg )  rt_bomb("nmg_edgeuse_with_eg_tabulate() eu disavows eg\n");
		bu_ptbl_ins_unique( tab, (long *)eu );
	}
}

struct edge_line_state {
	char			*visited;
	struct bu_ptbl		*tabl;
	point_t			pt;
	vect_t			dir;
	struct bn_tol		tol;
};

/**
 *			N M G _ L I N E _ H A N D L E R
 *
 *  A private support routine for nmg_edgeuse_on_line_tabulate.
 *  Having just visited an edgeuse, if this is the first time,
 *  and both vertices of this edgeuse lie within tol of the line,
 *  add it to the bu_ptbl array.
 */
static void
nmg_line_handler(long int *longp, genptr_t state, int first)
{
	register struct edge_line_state *sp = (struct edge_line_state *)state;
	register struct edgeuse	*eu = (struct edgeuse *)longp;

	NMG_CK_EDGEUSE(eu);
	/* If this edgeuse has been processed before, do nothing more */
	if( !NMG_INDEX_FIRST_TIME(sp->visited, eu) )  return;

	/*  If the lines are not generally parallel, don't bother with
	 *  checking the endpoints.  This helps reject very short edges
	 *  which are colinear only by virtue of being very small.
	 */
	BN_CK_TOL(&sp->tol);
	NMG_CK_EDGE_G_LSEG(eu->g.lseg_p);

	/* sp->tol.para and RT_DOT_TOL are too tight. 0.1 is 5 degrees */
	/* These are not unit vectors. */
	if( fabs( VDOT( eu->g.lseg_p->e_dir, sp->dir ) ) <
	    0.9 * MAGNITUDE(eu->g.lseg_p->e_dir) * MAGNITUDE(sp->dir) )
		return;

	if( bn_distsq_line3_pt3( sp->pt, sp->dir, eu->vu_p->v_p->vg_p->coord )
	    > sp->tol.dist_sq )
		return;
	if( bn_distsq_line3_pt3( sp->pt, sp->dir, eu->eumate_p->vu_p->v_p->vg_p->coord )
	    > sp->tol.dist_sq )
		return;

	/* Both points are within tolerance, add edgeuse to the list */
	bu_ptbl_ins( sp->tabl, longp );
}

/**
 *			N M G _ E D G E U S E _ O N _ L I N E _ T A B U L A T E
 *
 *  Given a pointer to any nmg data structure,
 *  build an bu_ptbl list which cites every edgeuse
 *  pointer from there on "down" in the model
 *  that has both vertices within tolerance of the given line.
 *
 *  XXX This routine is a potential source of major trouble.
 *  XXX If there are "nearby" edges that "should" be on the list but
 *  XXX don't make it, then the intersection calculations might
 *  XXX miss important intersections.
 *  As an admittedly grubby workaround, use 10X the distance tol here,
 *  just to get more candidates onto the list.
 *  The caller will have to wrestle with the added fuzz.
 */
void
nmg_edgeuse_on_line_tabulate(struct bu_ptbl *tab, const long int *magic_p, const fastf_t *pt, const fastf_t *dir, const struct bn_tol *tol)
{
	struct model		*m;
	struct edge_line_state		st;
	static const struct nmg_visit_handlers	handlers = {NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, nmg_line_handler, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL};
	/* handlers.bef_edgeuse = nmg_line_handler; */

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL(m);
	BN_CK_TOL(tol);

	st.visited = (char *)bu_calloc(m->maxindex+1, sizeof(char), "visited[]");
	st.tabl = tab;
	VMOVE( st.pt, pt );
	VMOVE( st.dir, dir );

	st.tol = *tol;					/* struct copy */
#if 0
	/* Use larger tolerance */
	st.tol.dist = 10 * tol->dist;
	st.tol.dist_sq = st.tol.dist * st.tol.dist;
#endif

	(void)bu_ptbl_init( tab, 64, " tab");

	nmg_visit( magic_p, &handlers, (genptr_t)&st );

	bu_free( (char *)st.visited, "visited[]");
}

struct e_and_v_state  {
	char		*visited;
	struct bu_ptbl	*edges;
	struct bu_ptbl	*verts;
};

/**
 *			N M G _ E _ H A N D L E R
 *
 *  A private support routine for nmg_e_and_v_tabulate().
 *
 *  Note that an edgeUSE is put on the list, to save one memory dereference
 *  in the eventual application.
 */
static void
nmg_e_handler(long int *longp, genptr_t state, int first)
{
	register struct e_and_v_state	*sp = (struct e_and_v_state *)state;
	register struct edge		*e  = (struct edge *)longp;

	NMG_CK_EDGE(e);
	/* If this edge has been processed before, do nothing more */
	if( !NMG_INDEX_FIRST_TIME(sp->visited, e) )  return;

	/* Add to list */
	bu_ptbl_ins( sp->edges, (long *)e->eu_p );
}

/**
 *			N M G _ V _ H A N D L E R
 *
 *  A private support routine for nmg_e_and_v_tabulate().
 */
static void
nmg_v_handler(long int *longp, genptr_t state, int first)
{
	register struct e_and_v_state	*sp = (struct e_and_v_state *)state;
	register struct vertex		*v  = (struct vertex *)longp;

	NMG_CK_VERTEX(v);
	/* If this vertex has been processed before, do nothing more */
	if( !NMG_INDEX_FIRST_TIME(sp->visited, v) )  return;

	/* Add to list */
	bu_ptbl_ins( sp->verts, longp );
}

/**
 *			N M G _ E _ A N D _ V _ T A B U L A T E
 *
 *  Build lists of all edges (represented by one edgeuse on that edge)
 *  and all vertices found underneath the
 *  NMG entity indicated by magic_p.
 */
void
nmg_e_and_v_tabulate(struct bu_ptbl *eutab, struct bu_ptbl *vtab, const long int *magic_p)
{
	struct model			*m;
	struct e_and_v_state		st;
	static const struct nmg_visit_handlers	handlers = {NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, NULL, NULL,
							    NULL, NULL, NULL, nmg_e_handler, NULL,
							    NULL, NULL, NULL, nmg_v_handler, NULL};
	/* handlers.vis_edge = nmg_e_handler; */
	/* handlers.vis_vertex = nmg_v_handler; */

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL(m);

	st.visited = (char *)bu_calloc(m->maxindex+1, sizeof(char), "visited[]");
	st.edges = eutab;
	st.verts = vtab;

	(void)bu_ptbl_init( eutab, 64, " eutab");
	(void)bu_ptbl_init( vtab, 64, " vtab");

	nmg_visit( magic_p, &handlers, (genptr_t)&st );

	bu_free( (char *)st.visited, "visited[]");
}

/**
 *			N M G _ 2 E D G E U S E _ G _ C O I N C I D E N T
 *
 *  Given two edgeuses, determine if they share the same edge geometry,
 *  either topologically, or within tolerance.
 *
 *  Returns -
 *	0	two edge geometries are not coincident
 *	1	edges geometries are everywhere coincident.
 *		(For linear edge_g_lseg, the 2 are the same line, within tol.)
 */
int
nmg_2edgeuse_g_coincident(const struct edgeuse *eu1, const struct edgeuse *eu2, const struct bn_tol *tol)
{
	struct edge_g_lseg	*eg1;
	struct edge_g_lseg	*eg2;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	BN_CK_TOL(tol);

	eg1 = eu1->g.lseg_p;
	eg2 = eu2->g.lseg_p;
	NMG_CK_EDGE_G_LSEG(eg1);
	NMG_CK_EDGE_G_LSEG(eg2);

	if( eg1 == eg2 )  return 1;

	/* Ensure direction vectors are generally parallel */
	/* These are not unit vectors */
	/* tol->para and RT_DOT_TOL are too tight a tolerance.  0.1 is 5 degrees */
	if( fabs(VDOT(eg1->e_dir, eg2->e_dir)) <
	    0.9 * MAGNITUDE(eg1->e_dir) * MAGNITUDE(eg2->e_dir)  )  return 0;

	/* Ensure that vertices on edge 2 are within tol of e1 */
	if( bn_distsq_line3_pt3( eg1->e_pt, eg1->e_dir,
	    eu2->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;
	if( bn_distsq_line3_pt3( eg1->e_pt, eg1->e_dir,
	    eu2->eumate_p->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;

	/* Ensure that vertices of both edges are within tol of other eg */
	if( bn_distsq_line3_pt3( eg2->e_pt, eg2->e_dir,
	    eu1->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;
	if( bn_distsq_line3_pt3( eg2->e_pt, eg2->e_dir,
	    eu1->eumate_p->vu_p->v_p->vg_p->coord ) > tol->dist_sq )  goto trouble;

	/* Perhaps check for ultra-short edges (< 10*tol->dist)? */

	/* Do not use bn_isect_line3_line3() -- it's MUCH too strict */

	return 1;
trouble:
	if( !bn_2line3_colinear( eg1->e_pt, eg1->e_dir, eg2->e_pt, eg2->e_dir,
	     1e5, tol ) )
		return 0;

	/* XXX debug */
	nmg_pr_eg( &eg1->l.magic, 0 );
	nmg_pr_eg( &eg2->l.magic, 0 );
	bu_log("nmg_2edgeuse_g_coincident() lines colinear, vertex check fails, calling colinear anyway.\n");
	return 1;
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
