/*                      N M G _ M I S C . C
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
/** @file nmg_misc.c
 *
 *	As the name implies, these are miscellaneous routines that work with
 *	the NMG structures.
 *
 *
 *  Authors -
 *	John R. Anderson
 *	Lee A. Butler
 *	Michael John Muuss
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


#include <math.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"

#include "db.h"		/* for debugging stuff at bottom */

/**
 *	N M G _ S N U R B _ C A L C _ L U _ U V _ O R I E N T
 */
int
nmg_snurb_calc_lu_uv_orient(const struct loopuse *lu)
{
	struct edgeuse *eu;
	int edge_count=0;
	int edge_no;
	vect_t area;
	point_t *pts;

	NMG_CK_LOOPUSE( lu );

	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		rt_bomb( "nmg_snurb_calc_lu_uv_orient: LU has no edges\n" );

	if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )
		rt_bomb( "nmg_snurb_calc_lu_uv_orient: LU is not part of a faceuse\n" );

	NMG_CK_FACEUSE( lu->up.fu_p );
	NMG_CK_FACE( lu->up.fu_p->f_p );

	if( *lu->up.fu_p->f_p->g.magic_p != NMG_FACE_G_SNURB_MAGIC )
		rt_bomb( "nmg_snurb_calc_lu_uv_orient: LU is not part of a SNURB face\n" );

	/* count "psuedo-vertices" in loop */
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct edge_g_cnurb *eg;

		NMG_CK_EDGEUSE( eu );

		if( *eu->g.magic_p != NMG_EDGE_G_CNURB_MAGIC )
			rt_bomb( "nmg_snurb_calc_lu_uv_orient: EU on NURB face does not have edge_g_cnurb geometry\n" );

		eg = eu->g.cnurb_p;
		NMG_CK_EDGE_G_CNURB( eg );

		if( eg->order <= 0 )
			edge_count++;
		else
			edge_count += 5;
	}

	/* allocate memory for "psuedo-vertices" */
	pts = (point_t *)bu_calloc( edge_count, sizeof( point_t ), "Orient_nurb_face_loops: pts" );

	/* Assign uv geometry to each "psuedo-vertex" */
	edge_no = 0;
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct edge_g_cnurb *eg;
		struct vertexuse *vu;
		struct vertexuse_a_cnurb *vg1;

		eg = eu->g.cnurb_p;

		if( eg->order <= 0 )
		{
			vu = eu->vu_p;
			NMG_CK_VERTEXUSE( vu );
			if( *vu->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC )
				rt_bomb( "Orient_nurb_face_loops: vertexuse in face_g_snurb faceuse doesn't have edge_g_cnurb attribute\n" );
			vg1 = vu->a.cnurb_p;
			VMOVE( pts[edge_no], vg1->param )
			edge_no++;
		}
		else
		{
			fastf_t t1,t2;
			hpoint_t crv_pt;
			int coords;
			int i;

			t1 = eg->k.knots[0];
			t2 = eg->k.knots[eg->k.k_size-1];
			coords = RT_NURB_EXTRACT_COORDS( eg->pt_type );

			for( i=0 ; i<5 ; i++ )
			{
				fastf_t t;

				t = t1 + (t2 - t1)*0.2*(fastf_t)i;

				VSETALLN( crv_pt, 0.0, coords )
				rt_nurb_c_eval( eg, t, crv_pt );
				if( RT_NURB_IS_PT_RATIONAL( eg->pt_type ) )
					VSCALE( pts[edge_no], crv_pt, crv_pt[coords-1] )
				else
					VMOVE( pts[edge_no], crv_pt )
				edge_no++;
			}
		}
	}

	/* translate loop such that pts[0] is at (0,0,0) */
	for( edge_no=1 ; edge_no<edge_count ; edge_no++ )
	{
		VSUB2( pts[edge_no], pts[edge_no], pts[0] )
		pts[edge_no][Z] = 0.0;
	}
	VSETALL( pts[0], 0.0 )

	/* calculate area of loop in uv-space */
	VSETALL( area, 0.0 );
	for( edge_no=1 ; edge_no<edge_count-1 ; edge_no++ )
	{
		vect_t cross;

		VCROSS( cross, pts[edge_no], pts[edge_no+1] );
		VADD2( area, area, cross );
	}

	bu_free( (char *)pts, "nmg_snurb_calc_lu_uv_orient: pts" );

	if( area[Z] > 0.0 )
		return( OT_SAME );
	if( area[Z] < 0.0 )
		return( OT_OPPOSITE );

	return( OT_NONE );
}

void
nmg_snurb_fu_eval(const struct faceuse *fu, const fastf_t u, const fastf_t v, fastf_t *pt_on_srf)
{
	struct face *f;
	hpoint_t tmp_pt;

	NMG_CK_FACEUSE( fu );

	f = fu->f_p;
	NMG_CK_FACE( f );
	if( !f->g.magic_p )
	{
		bu_log( "nmg_snurb_fu_get_norm: face has no geometry (x%x)\n", f );
		rt_bomb( "nmg_snurb_fu_get_norm: bad face\n" );
	}
	if( *f->g.magic_p != NMG_FACE_G_SNURB_MAGIC )
	{
		bu_log( "nmg_snurb_fu_get_norm: face is not a NURB face (x%x)\n", f );
		rt_bomb( "nmg_snurb_fu_get_norm: bad face\n" );
	}

	VSETALLN( tmp_pt, 0.0, 4 );
	rt_nurb_s_eval( f->g.snurb_p, u, v, tmp_pt );

	if( RT_NURB_IS_PT_RATIONAL(f->g.snurb_p->pt_type) )
	{
		double f;

		f = 1.0 / tmp_pt[3];
		VSCALE( pt_on_srf, tmp_pt, f );
	}
	else
		VMOVE( pt_on_srf, tmp_pt )
}

void
nmg_snurb_fu_get_norm(const struct faceuse *fu, const fastf_t u, const fastf_t v, fastf_t *norm)
{
	struct face *f;

	NMG_CK_FACEUSE( fu );

	f = fu->f_p;
	NMG_CK_FACE( f );
	if( !f->g.magic_p )
	{
		bu_log( "nmg_snurb_fu_get_norm: face has no geometry (x%x)\n", f );
		rt_bomb( "nmg_snurb_fu_get_norm: bad face\n" );
	}
	if( *f->g.magic_p != NMG_FACE_G_SNURB_MAGIC )
	{
		bu_log( "nmg_snurb_fu_get_norm: face is not a NURB face (x%x)\n", f );
		rt_bomb( "nmg_snurb_fu_get_norm: bad face\n" );
	}

	rt_nurb_s_norm( f->g.snurb_p, u, v, norm );

	if( (fu->orientation != OT_SAME) != (f->flip != 0 ) )
		VREVERSE( norm, norm )
}

void
nmg_snurb_fu_get_norm_at_vu(const struct faceuse *fu, const struct vertexuse *vu, fastf_t *norm)
{
	struct vertexuse_a_cnurb *va;

	NMG_CK_FACEUSE( fu );
	NMG_CK_VERTEXUSE( vu );

	if( !vu->a.magic_p )
	{
		bu_log( "nmg_snurb_fu_get_norm_at_vu: vertexuse does not have an attribute (x%x)\n", vu );
		rt_bomb( "nmg_snurb_fu_get_norm_at_vu: bad VU\n" );
	}

	if( *vu->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC )
	{
		bu_log( "nmg_snurb_fu_get_norm_at_vu: vertexuse does not have a cnurb attribute (x%x)\n", vu );
		rt_bomb( "nmg_snurb_fu_get_norm_at_vu: bad VU\n" );
	}

	va = vu->a.cnurb_p;
	NMG_CK_VERTEXUSE_A_CNURB( va );

	nmg_snurb_fu_get_norm( fu, va->param[0], va->param[1], norm );

}

void
nmg_find_zero_length_edges(const struct model *m)
{
	struct bu_ptbl eu_tab;
	struct edgeuse *eu;
	int i;

	bu_ptbl_init( &eu_tab, 64, " &eu_tab");

	nmg_edgeuse_tabulate( &eu_tab, &m->magic );

	for( i=0 ; i<BU_PTBL_END( &eu_tab ) ; i++ )
	{
		struct loopuse *lu;

		eu = (struct edgeuse *)BU_PTBL_GET( &eu_tab, i );
		NMG_CK_EDGEUSE( eu );

		if( eu->vu_p->v_p != eu->eumate_p->vu_p->v_p )
			continue;

		/* found a zero length edge */

		bu_log( "Edgeuse x%x (vp %x to vp %x)\n" , eu, eu->vu_p->v_p, eu->eumate_p->vu_p->v_p );
		if( *eu->up.magic_p != NMG_LOOPUSE_MAGIC )
		{
			bu_log( "\tThis is a wire edge\n" );
			continue;
		}

		lu = eu->up.lu_p;

		nmg_pr_lu_briefly( lu, "" );
	}

	bu_ptbl_free( &eu_tab);
}

/**
 *	N M G _ F I N D _ T O P _ F A C E _ I N _ D I R
 *
 *	Finds the topmost face in a shell (in given direction).
 *	Expects to have a translation table (variable "flags") for
 *	the model, and will ignore face structures that have their
 *	flag set in the table.
 *
 *	dir must be X,Y, or Z
 */
struct face *
nmg_find_top_face_in_dir(const struct shell *s, int dir, long int *flags)
{
	fastf_t extreme_value=(-MAX_FASTF);
	fastf_t extreme_slope=(-MAX_FASTF);
	vect_t edge;
	vect_t normal;
	struct face *f_top=(struct face *)NULL;
	struct edge *e_top=(struct edge *)NULL;
	struct vertex *vp_top=(struct vertex *)NULL;
	struct loopuse *lu;
	struct faceuse *fu;
	struct edgeuse *eu,*eu1;
	struct vertexuse *vu;
	struct vertex *v1,*v2;
	int bottommost=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_find_top_face_in_dir( s = x%x , dir=%d , flags = x%x )\n" , s, dir , flags );

	NMG_CK_SHELL( s );

	if( dir < X || dir > Z )
	{
		bu_log( "nmg_find_top_face_in_dir: illegal direction: %d\n", dir );
		return( (struct face *)NULL );
	}

#if 0
	if( dir < 0 )
	{
		bottommost = 1;
		dir = (-dir);
		extreme_value = MAX_FASTF;
		extreme_slope = MAX_FASTF;
	}
#endif
	/* find extreme vertex */
	for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );

		/* skip flagged faceuses */
		if( NMG_INDEX_TEST( flags , fu->f_p ) )
			continue;
		for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );
			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_EDGEUSE_MAGIC )
			{
				for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );
					if( bottommost )
					{
						if( eu->vu_p->v_p->vg_p->coord[dir] < extreme_value )
						{
							extreme_value = eu->vu_p->v_p->vg_p->coord[dir];
							vp_top = eu->vu_p->v_p;
						}
					}
					else
					{
						if( eu->vu_p->v_p->vg_p->coord[dir] > extreme_value )
						{
							extreme_value = eu->vu_p->v_p->vg_p->coord[dir];
							vp_top = eu->vu_p->v_p;
						}
					}
				}
			}
		}
	}
	if( vp_top == (struct vertex *)NULL )
	{
		bu_log( "Find_top_face_in_dir: Could not find extreme vertex" );
		return( (struct face *)NULL );
	}

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "top vertex is x%x at ( %g %g %g )\n",
			vp_top, V3ARGS( vp_top->vg_p->coord ) );

	/* find edge from vp_top with extreme slope in "dir" direction */
	for( BU_LIST_FOR( vu , vertexuse , &vp_top->vu_hd ) )
	{
		struct vertexuse *vu1;

		NMG_CK_VERTEXUSE( vu );

		/* only consider edgeuses */
		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
			continue;

		eu = vu->up.eu_p;
		NMG_CK_EDGEUSE( eu );

		if( rt_g.NMG_debug & DEBUG_BASIC )
		{
			bu_log( "Checking edge (%g %g %g)<->(%g %g %g)\n",
				V3ARGS( eu->vu_p->v_p->vg_p->coord ),
				V3ARGS( eu->eumate_p->vu_p->v_p->vg_p->coord ) );
		}

		/* skip wire edges */
		if( *eu->up.magic_p != NMG_LOOPUSE_MAGIC )
			continue;

		/* skip wire loops */
		if( *eu->up.lu_p->up.magic_p != NMG_FACEUSE_MAGIC )
			continue;

		/* skip finished faces */
		if( NMG_INDEX_TEST( flags , eu->up.lu_p->up.fu_p->f_p ) )
			continue;

		/* skip edges from other shells */
		if( nmg_find_s_of_eu( eu ) != s )
			continue;

		/* skip zero length edges */
		if( eu->eumate_p->vu_p->v_p == vp_top )
			continue;

		/* get vertex at other end of this edge */
		vu1 = eu->eumate_p->vu_p;
		NMG_CK_VERTEXUSE( vu1 );

		/* make a unit vector in direction of edgeuse */
		VSUB2( edge , vu1->v_p->vg_p->coord , vu->v_p->vg_p->coord );
		VUNITIZE( edge );

		if( rt_g.NMG_debug & DEBUG_BASIC )
		{
			bu_log( "Checking edge (%g %g %g)<->(%g %g %g)\n",
				V3ARGS( eu->vu_p->v_p->vg_p->coord ),
				V3ARGS( eu->eumate_p->vu_p->v_p->vg_p->coord ) );
			bu_log( "\tedge direction = (%g %g %g)\n", V3ARGS( edge ) );
			bu_log( "\t\textreme slope = %g\n", extreme_slope );
		}

		/* check against current maximum slope */
		if( bottommost )
		{
			if( edge[dir] < extreme_slope )
			{
				extreme_slope = edge[dir];
				e_top = eu->e_p;
			}
		}
		else
		{
			if( edge[dir] > extreme_slope )
			{
				if( rt_g.NMG_debug & DEBUG_BASIC )
					bu_log( "New top edge!\n" );
				extreme_slope = edge[dir];
				e_top = eu->e_p;
			}
		}
	}
	if( e_top == (struct edge *)NULL )
	{
		bu_log( "Fix_normals: Could not find uppermost edge" );
		return( (struct face *)NULL );
	}

	eu = e_top->eu_p;

#if 0
	/* if the top edge is a free edge, don't use it */
	if( eu->eumate_p == eu->radial_p )
		return( (struct face *)NULL );
#endif
	v1 = eu->vu_p->v_p;
	NMG_CK_VERTEX( v1 );
	v2 = eu->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX( v2 );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "top EU is x%x (%g %g %g) <-> (%g %g %g)\n",
			eu, V3ARGS( v1->vg_p->coord ),
			V3ARGS( v2->vg_p->coord ) );

	/* now find the face containing edge between v1 nad v2
	 with "left-pointing vector" having the most extreme slope */
	if( bottommost )
		extreme_slope = MAX_FASTF;
	else
		extreme_slope = (-MAX_FASTF);

	for( BU_LIST_FOR( vu, vertexuse, &v1->vu_hd ) )
	{
		vect_t left;
		vect_t edge_dir;

		NMG_CK_VERTEXUSE( vu );
		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
			continue;

		eu1 = vu->up.eu_p;
		NMG_CK_EDGEUSE( eu1 );

		/* don't bother with anything but faces */
		if( *eu1->up.magic_p != NMG_LOOPUSE_MAGIC )
			continue;

		/* skip edges not between correct vertices */
		if( eu1->eumate_p->vu_p->v_p != v2 )
			continue;

		lu = eu1->up.lu_p;
		NMG_CK_LOOPUSE( lu );
		if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )
			continue;

		/* fu is a faceuse containing "eu1" */
		fu = lu->up.fu_p;
		NMG_CK_FACEUSE( fu );

		/* skip faces from other shells and flagged faceuses */
		if( fu->s_p != s || NMG_INDEX_TEST( flags, fu->f_p ) )
			continue;

		/* make a vector in the direction of "eu1" */
		if( rt_g.NMG_debug & DEBUG_BASIC )
			bu_log( "test EU is x%x (%g %g %g) <-> (%g %g %g)\n",
				eu, V3ARGS( eu->vu_p->v_p->vg_p->coord ),
				V3ARGS( eu->eumate_p->vu_p->v_p->vg_p->coord ) );

		VSUB2( edge_dir , eu1->eumate_p->vu_p->v_p->vg_p->coord , eu1->vu_p->v_p->vg_p->coord );

		if( rt_g.NMG_debug & DEBUG_BASIC )
			bu_log( "edge_dir is ( %g %g %g )\n", V3ARGS( edge_dir ) );

		/* find the normal for this faceuse */
		if( *fu->f_p->g.magic_p == NMG_FACE_G_PLANE_MAGIC )
			NMG_GET_FU_NORMAL( normal, fu )
		else if( *fu->f_p->g.magic_p == NMG_FACE_G_SNURB_MAGIC )
			nmg_snurb_fu_get_norm_at_vu( fu, eu1->vu_p, normal );

		if( rt_g.NMG_debug & DEBUG_BASIC )
			bu_log( "fu normal is ( %g %g %g )\n" , V3ARGS( normal ) );

		/* normal cross edge direction gives vector in face */
		VCROSS( left , normal , edge_dir );

		/* unitize to get slope */
		VUNITIZE( left );
		if( rt_g.NMG_debug & DEBUG_BASIC )
		{
			bu_log( "left vector is ( %g %g %g )\n", V3ARGS( left ) );
			bu_log( "\textreme slope in %d direction is %g\n", dir, extreme_slope );
		}

		/* check against current most extreme slope */
		if( bottommost )
		{
			if( left[dir] < extreme_slope )
			{
				extreme_slope = left[dir];
				f_top = fu->f_p;
			}
		}
		else
		{
			if( left[dir] > extreme_slope )
			{
				if( rt_g.NMG_debug & DEBUG_BASIC )
					bu_log( "new f_top\n" );
				extreme_slope = left[dir];
				f_top = fu->f_p;
			}
		}
	}

	if( f_top == (struct face *)NULL )
	{
		bu_log( "Nmg_find_top_face_in_dir: Could not find uppermost face" );
		return( (struct face *)NULL );
	}

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_find_top_face_in_dir: top face = x%x, dir = %d, top vertex = x%x ( %g %g %g )\n",
			f_top, dir, vp_top, V3ARGS( vp_top->vg_p->coord ) );

	return( f_top );
}

/**
 *	N M G _ F I N D _ T O P _ F A C E
 *
 *	Finds the topmost face in a shell (in some direction).
 *	Expects to have a translation table (variable "flags") for
 *	the model, and will ignore face structures that have their
 *	flag set in the table.
 *
 * returns the top face in some direction.
 *
 * dir will be set to X, Y, or Z to indicate which top face was found.
 */

struct face *
nmg_find_top_face(const struct shell *s, int *dir, long int *flags)
{
	struct face *top_face;

	for( *dir=X ; *dir<=Z ; (*dir)++ )
		if( (top_face=nmg_find_top_face_in_dir( s, *dir, flags )) != (struct face *)NULL )
			return( top_face );

	/* give up!! */
	bu_log( "Nmg_find_top_face: Cannot find a top face\n" );
	*dir = (-32000); /* will hopefully cause an error if used */
	return( (struct face *)NULL );

}

/**	N M G _ A S S O C _ V O I D _ S H E L L S
 *
 * Passed an bu_ptbl structure containing one shell, this routine
 * examines the other shells in the region to determine if any are void shells
 * within the shell on the bu_ptbl list. Any such void shells found are added
 * to the bu_ptbl list. The final result is a ptbl list of shells where the
 * first shell on the list is the outer shell, and any additional shells one
 * the list are void shells within that outer shell. This is a support routine
 * for "nmg_find_outer_and_void_shells" and gets called for every outer shell
 * in the region
 */
struct top_face
{
	struct shell *s;
	struct face *f;
	int dir;
	vect_t normal;
};

static void
nmg_assoc_void_shells(const struct nmgregion *r, struct bu_ptbl *shells, const struct bn_tol *ttol)
{
	struct shell *outer_shell,*void_s,*s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	long *flags;
	struct top_face *top_faces;
	int total_shells=0;
	int i;
	int dir;

	NMG_CK_REGION( r );
	BU_CK_PTBL( shells );
	BN_CK_TOL( ttol );

	outer_shell = (struct shell *)BU_PTBL_GET( shells , 0 );
	NMG_CK_SHELL( outer_shell );

	/* count shells in region */
	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
		total_shells++;

	/* make an array of shells and top faces */
	top_faces = (struct top_face *)bu_calloc( total_shells , sizeof( struct top_face ) , "nmg_assoc_void_shells: top_faces" );

	/* make flags array for use by "nmg_find_top_face" */
	flags = (long *)bu_calloc( r->m_p->maxindex , sizeof( long ) , "nmg_find_outer_and_void_shells: flags" );

	top_faces[0].s = outer_shell;
	top_faces[0].f = nmg_find_top_face( outer_shell, &dir , flags );
	top_faces[0].dir = dir;
	fu = top_faces[0].f->fu_p;
	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;
	NMG_GET_FU_NORMAL( top_faces[0].normal , fu );

	/* fill in top_faces array */
	i = 0;
	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
	{
		if( s == outer_shell )
			continue;

		top_faces[++i].s = s;
		top_faces[i].f = nmg_find_top_face( s, &dir , flags );
		top_faces[i].dir = dir;
		if( top_faces[i].f == (struct face *)NULL )
			bu_log( "WARNING: nmg_assoc_void_shells() could not find top face for shell x%x\n", s );
		else
		{
			fu = top_faces[i].f->fu_p;
			if( fu->orientation != OT_SAME )
				fu = fu->fumate_p;
			NMG_GET_FU_NORMAL( top_faces[i].normal , fu );
		}
	}

	/* look for voids */
	for( BU_LIST_FOR( void_s , shell , &r->s_hd ) )
	{
		struct face *void_f;
		int wrong_void=0;
		vect_t normal;

		if( void_s == outer_shell )
			continue;

		NMG_CK_SHELL( void_s );

		void_f = (struct face *)NULL;
		for( i=0 ; i<total_shells ; i++ )
		{
			if( top_faces[i].s == void_s )
			{
				void_f = top_faces[i].f;
				dir = top_faces[i].dir;
				VMOVE( normal , top_faces[i].normal );
				break;
			}
		}
		if( void_f == (struct face *)NULL )
			rt_bomb( "nmg_assoc_void_shells: no top face for a shell\n" );

		if( normal[dir] < 0.0)
		{
			/* this is a void shell */
			struct face *int_f;
			struct shell *test_s;
			int breakout=0;
			int not_in_this_shell=0;

			/* this is a void shell
			 * but does it belong with outer_shell */
			if( !V3RPP1_IN_RPP2( void_s->sa_p->min_pt , void_s->sa_p->max_pt , outer_shell->sa_p->min_pt , outer_shell->sa_p->max_pt ) )
			{
				continue;
			}

			for( BU_LIST_FOR( fu , faceuse , &void_s->fu_hd ) )
			{
				for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
				{
					if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
						continue;
					for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
					{
						int class;

						class = nmg_class_pt_s( eu->vu_p->v_p->vg_p->coord , outer_shell, 0 , ttol );

						if( class == NMG_CLASS_AoutB )
						{
							breakout = 1;
							not_in_this_shell = 1;
							break;
						}
					}
					if( breakout )
						break;
				}
				if( breakout )
					break;
			}

			if( not_in_this_shell )
				continue;

			int_f = (struct face *)NULL;
			for( i=0 ; i<total_shells ; i++ )
			{
				if( top_faces[i].s == void_s )
				{
					int_f = top_faces[i].f;
					break;
				}
			}
			if( int_f == (struct face *)NULL )
				rt_bomb( "nmg_assoc_void_shells: no top face for a shell\n" );

			/* Make sure there are no other external shells between these two */
			for( BU_LIST_FOR( test_s , shell , &r->s_hd ) )
			{
				vect_t test_norm;
				struct face *test_f;
				int test_dir = 0;

				/* don't check against the outer shell or the candidate void shell */
				if( test_s == void_s || test_s == outer_shell )
					continue;

				/* find top face for the test shell */
				test_f = (struct face *)NULL;
				for( i=0 ; i<total_shells ; i++ )
				{
					if( top_faces[i].s == test_s )
					{
						test_f = top_faces[i].f;
						test_dir = top_faces[i].dir;
						VMOVE( test_norm , top_faces[i].normal );
						break;
					}
				}
				if( test_f == (struct face *)NULL )
					rt_bomb( "nmg_assoc_void_shells: no top face for a shell\n" );

				/* skip test shells that are void shells */
				if( test_norm[test_dir] < 0.0)
					continue;

				/* if the void shell is not within the test shell, continue */
				if( !V3RPP1_IN_RPP2( void_s->sa_p->min_pt , void_s->sa_p->max_pt , test_s->sa_p->min_pt , test_s->sa_p->max_pt ) )
					continue;

				/* the void shell may be within this shell */
				/* XXXX Need code here to check if candidate void shell (void_s)
				 * XXXX is within test shell (test_s) and test shell is
				 * XXXX is within outer shell (outer_shell)
				if( void_s in test_s and test_s in outer_shell )
				{
					wrong_void = 1;
					break;
				}
				*/
			}
			if( wrong_void )
			{
				continue;
			}

			/* This void shell belongs with shell outer_s
			 * add it to the list of shells */
			bu_ptbl_ins( shells , (long *)void_s );
		}
	}
	bu_free( (char *)flags , "nmg_assoc_void_shells: flags" );
}

/**	N M G _ F I N D _ O U T E R _ A N D _ V O I D _ S H E L L S
 *
 * This routine takes a region and constructs an array of bu_ptbl lists.
 * A list is created for each outer shell, and that shell is the first item
 * on the list. Additional shells on any list are void shells within that
 * lists outer shell. This routine calls "nmg_decompose_shell" for every
 * shell in the region, so the original region topology may be changed
 * to accomplish this. No geometry is altered.
 */
int
nmg_find_outer_and_void_shells(struct nmgregion *r, struct bu_ptbl ***shells, const struct bn_tol *tol)
{
	struct bu_ptbl *outer_shells;
	struct shell *s;
	int i;
	int total_shells=0;
	int outer_shell_count;
	int re_bound=0;
	int dir;
	long *flags;

	NMG_CK_REGION( r );
	BN_CK_TOL( tol );

	/* Decompose shells */
	outer_shells = (struct bu_ptbl *)bu_malloc( sizeof( struct bu_ptbl ) , "nmg_find_outer_and_void_shells: outer_shells" );
	bu_ptbl_init( outer_shells , 64, " outer_shells ");
	for (BU_LIST_FOR(s, shell, &r->s_hd))
	{
		NMG_CK_SHELL( s );
		bu_ptbl_ins( outer_shells , (long *)s );
	}
	for( i=0 ; i<BU_PTBL_END( outer_shells ) ; i++ )
	{
		s = (struct shell *)BU_PTBL_GET( outer_shells , i );
		if( nmg_decompose_shell( s , tol ) > 1 )
			re_bound = 1;
	}
	bu_ptbl_reset( outer_shells );

	if( re_bound )
		nmg_region_a( r , tol );

	for (BU_LIST_FOR(s, shell, &r->s_hd))
		total_shells++;

	flags = (long *)bu_calloc( r->m_p->maxindex , sizeof( long ) , "nmg_find_outer_and_void_shells: flags" );

	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
	{
		struct face *f;
		struct faceuse *fu;
		vect_t normal;

		f = (struct face *)NULL;
		for( dir = X ; dir <= Z ; dir++ ) {
			if( (f = nmg_find_top_face_in_dir( s, dir , flags )) == (struct face *)NULL )
				continue;

			fu = f->fu_p;
			if( fu->orientation != OT_SAME )
				fu = fu->fumate_p;
			if( fu->orientation != OT_SAME )
				rt_bomb( "nmg_find_outer_and_void_shells: Neither faceuse nor mate have OT_SAME orient\n" );

			NMG_GET_FU_NORMAL( normal , fu );
			if( normal[dir] >= 0.0)	{
				bu_ptbl_ins( outer_shells , (long *)s );	/* outer shell */
				break;
			}
		}

		if( f == (struct face *)NULL ) {
			bu_bomb( "nmg_find_outer_and_void_shells: cannot find top face in a shell\n" );
		}
	}

	/* outer_shells is now a list of all the outer shells in the region */
	outer_shell_count = BU_PTBL_END( outer_shells );

	*shells = (struct bu_ptbl **)bu_calloc( BU_PTBL_END( outer_shells ) , sizeof( struct bu_ptbl *) ,
			"nmg_find_outer_and_void_shells: shells" );
	for( i=0 ; i<BU_PTBL_END( outer_shells ) ; i++ )
	{
		(*shells)[i] = (struct bu_ptbl *)bu_malloc( sizeof( struct bu_ptbl ) ,
			"nmg_find_outer_and_void_shells: shells[]" );

		bu_ptbl_init( (*shells)[i] , 64, "(*shells)[i]");
		BU_CK_PTBL( (*shells)[i] );
		bu_ptbl_ins( (*shells)[i] , BU_PTBL_GET( outer_shells , i ) );
		if( outer_shell_count != total_shells ) /* must be some void shells */
			nmg_assoc_void_shells( r , (*shells)[i] , tol );
	}

	bu_free( (char *)flags , "nmg_find_outer_and_void_shells: flags" );
	bu_ptbl_free( outer_shells );
	return( outer_shell_count );
}

/**	N M G _ M A R K _ E D G E S _ R E A L
 *
 * Sets the "is_real" flag on all edges at or below the
 * pointer passed. Returns the number of flags set.
 */
int
nmg_mark_edges_real(const long int *magic_p)
{
	struct bu_ptbl edges;
	int i,count;

	nmg_edge_tabulate( &edges , magic_p );

	count = BU_PTBL_END( &edges );
	for( i=0 ; i<count ; i++ )
	{
		struct edge *e;

		e = (struct edge *)BU_PTBL_GET( &edges , i );
		NMG_CK_EDGE( e );

		e->is_real = 1;
	}

	bu_ptbl_free( &edges );

	return( count );
}

/**	N M G _ T A B U L A T E _ F A C E _ G _ V E R T S
 *
 * Tabulates all vertices in faces that use fg
 */

void
nmg_tabulate_face_g_verts(struct bu_ptbl *tab, const struct face_g_plane *fg)
{
	struct face *f;

	NMG_CK_FACE_G_PLANE( fg );

	bu_ptbl_init( tab, 64, " tab");

	/* loop through all faces using fg */
	for( BU_LIST_FOR( f , face , &fg->f_hd ) )
	{
		struct faceuse *fu;
		struct loopuse *lu;

		NMG_CK_FACE( f );

		/* get one of the two uses of this face */
		fu = f->fu_p;
		NMG_CK_FACEUSE( fu );

		/* Visit each loop in this faceuse */
		for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );

			/* include loops of a single vertex */
			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
			{
				struct vertexuse *vu;
				struct vertex *v;

				vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
				NMG_CK_VERTEXUSE( vu );
				v = vu->v_p;
				NMG_CK_VERTEX( v );

				/* insert vertex into table */
				bu_ptbl_ins_unique( tab , (long *)v );
			}
			else
			{
				struct edgeuse *eu;

				/* visit each edgeuse in the loop */
				for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					struct vertexuse *vu;
					struct vertex *v;

					NMG_CK_EDGEUSE( eu );
					vu = eu->vu_p;
					NMG_CK_VERTEXUSE( vu );
					v = vu->v_p;
					NMG_CK_VERTEX( v );

					/* insert vertex into table */
					bu_ptbl_ins_unique( tab , (long *)v );
				}
			}
		}
	}
}

/** 	N M G _ I S E C T _ S H E L L _ S E L F
 *
 * Intersects all faces in a shell with all other faces in the same shell
 * Intended for use after extrusion
 *
 */
void
nmg_isect_shell_self(struct shell *s, const struct bn_tol *tol)
{
	struct model *m;
	struct nmgregion *r;
	struct shell *s_fu;
	struct faceuse *fu;
	struct bu_ptbl fus;
	int fu_no;
	int fu2_no;

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	m = nmg_find_model( &s->l.magic );
	NMG_CK_MODEL( m );

	nmg_vmodel( m );

	r = s->r_p;
	NMG_CK_REGION( r );

	s_fu = nmg_msv( r );
	NMG_CK_SHELL( s_fu );

	bu_ptbl_init( &fus , 64, " &fus ");

	for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );

		if( fu->orientation == OT_SAME )
			bu_ptbl_ins( &fus , (long *)fu );
	}

	/* intersect each face with every other face in the shell */
	for( fu_no=0 ; fu_no < BU_PTBL_END( &fus ) ; fu_no ++ )
	{
		struct faceuse *fu2;

		fu = (struct faceuse *)BU_PTBL_GET( &fus , fu_no );

		NMG_CK_FACEUSE( fu );

		/* move fu to another shell to avoid radial edge problems */
		nmg_mv_fu_between_shells( s_fu, s, fu );

		/* consider intersection this faceuse with all the faceuses
		 * after it in the list
		 */
		for( fu2_no=fu_no+1 ; fu2_no < BU_PTBL_END( &fus ) ; fu2_no++ )
		{
			struct face *f,*f2;

			fu2 = (struct faceuse *)BU_PTBL_GET( &fus , fu2_no );

			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "nmg_extrude_cleanup: fu=x%x, fu2=x%x\n" , fu , fu2 );

			/* skip faceuses radial to fu or not OT_SAME */
			if( fu2->orientation != OT_SAME || nmg_faces_are_radial( fu , fu2 ) )
				continue;

			f = fu->f_p;
			f2 = fu2->f_p;

			/* skip faceuse pairs that don't have overlapping BB's */
			if( !V3RPP_OVERLAP( f->min_pt , f->max_pt , f2->min_pt , f2->max_pt ) )
				continue;

			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "nmg_extrude_cleanup: calling nmg_isect_two_generic_faces( fu=x%x , fu2=x%x )\n" , fu , fu2 );

			nmg_isect_two_generic_faces( fu , fu2 , tol );
		}
		/* move fu back where it belongs */
		while( BU_LIST_NON_EMPTY( &s_fu->fu_hd ) )
		{
			struct faceuse *fu_tmp;

			fu_tmp = BU_LIST_FIRST( faceuse , &s_fu->fu_hd );
			NMG_CK_FACEUSE( fu_tmp );

			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "nmg_extrude_cleanup: moving fu x%x back\n" , fu_tmp );

			nmg_mv_fu_between_shells( s, s_fu, fu_tmp );
		}
	}

	/* get rid of the temporary shell */
	nmg_ks( s_fu );

	bu_ptbl_free( &fus);
}

/**	N M G _ N E X T _ R A D I A L _ E D G E U S E
 *
 * Traverse radial edgeuse around specified edgeuse looking for
 * one that meets optional restrictions. If a shell is specified
 * only edgeuse from that shell will be considered. If wires is
 * non-zero, wire edges will be considered, otherwise, wire edges
 * are ignored.
 *
 * returns:
 *	radial edgeuse
 */

struct edgeuse *
nmg_next_radial_eu(const struct edgeuse *eu, const struct shell *s, const int wires)
{
	struct edgeuse *ret_eu;

	NMG_CK_EDGEUSE( eu );
	if( s )
		NMG_CK_SHELL( s );

	if( s && nmg_find_s_of_eu( eu ) != s )
		rt_bomb( "nmg_find_radial_eu: eu is not in specified shell\n" );

	if( !wires && !nmg_find_fu_of_eu( eu ) )
		rt_bomb( "nmg_find_radial_eu: wire edges not specified, but eu is a wire!!\n" );

	ret_eu = eu->eumate_p->radial_p;
	while(
	      (!wires & (nmg_find_fu_of_eu(ret_eu) == (struct faceuse *)NULL))
	      ||
	      ( (s != (struct shell *)NULL) &&
		nmg_find_s_of_eu(ret_eu) != s  )
	      )
			ret_eu = ret_eu->eumate_p->radial_p;

	return( ret_eu );
}

/**	N M G _ P R E V _ R A D I A L _ E D G E U S E
 *
 * Traverse radial edgeuse around specified edgeuse in opposite
 * direction from nmg_next_radial_eu, looking for
 * one that meets optional restrictions. If a shell is specified
 * only edgeuse from that shell will be considered. If wires is
 * non-zero, wire edges will be considered, otherwise, wire edges
 * are ignored.
 *
 * returns:
 *	radial edgeuse
 */

struct edgeuse *
nmg_prev_radial_eu(const struct edgeuse *eu, const struct shell *s, const int wires)
{
	struct edgeuse *ret_eu;

	NMG_CK_EDGEUSE( eu );
	if( s )
		NMG_CK_SHELL( s );

	if( s && nmg_find_s_of_eu( eu ) != s )
		rt_bomb( "nmg_find_radial_eu: eu is not in specified shell\n" );

	if( !wires && !nmg_find_fu_of_eu( eu ) )
		rt_bomb( "nmg_find_radial_eu: wire edges not specified, but eu is a wire!!\n" );

	ret_eu = eu->radial_p->eumate_p;
	while( ( !wires & (nmg_find_fu_of_eu( ret_eu ) == (struct faceuse *)NULL)) ||
		( (s != (struct shell *)NULL) && nmg_find_s_of_eu( ret_eu ) != s  ) )
			ret_eu = ret_eu->radial_p->eumate_p;

	return( ret_eu );
}

/**	N M G _ R A D I A L _ F A C E _ C O U N T
 *
 * Counts the number of faces (actually, the number of radial edgeuse/mate pairs)
 * around eu. If s is specified, only edgeuses in shell s are counted. Wire
 * edgeuses are not counted.
 *
 * returns:
 *	number of edgeuse/mate pairs radiall around eu that meet restrictions
 */
int
nmg_radial_face_count(const struct edgeuse *eu, const struct shell *s)
{
	int face_count=1;
	struct edgeuse *eu1;

	NMG_CK_EDGEUSE( eu );
	if( s )
		NMG_CK_SHELL( s );

	/* count radial faces on this edge */
	eu1 = eu->eumate_p->radial_p;
	while( eu1 != eu && eu1 != eu->eumate_p )
	{
		/* ignore other shells and don't count wires */
		if( (!s || nmg_find_s_of_eu( eu1 ) == s) &&
			nmg_find_fu_of_eu( eu1 ) != (struct faceuse *)NULL )
				face_count++;
		eu1 = eu1->eumate_p->radial_p;
	}

	return( face_count );
}

/**	N M G _ C H E C K _ C L O S E D _ S H E L L
 *
 *	Looks at every eu in OT_SAME fu's. If any eu
 *	has no radials, then it must be the edge of a
 *	dangling face and therfore the edge of an opening.
 *
 *  returns:
 *	0 - O.K.
 *	1 - found a hole
 */
int
nmg_check_closed_shell(const struct shell *s, const struct bn_tol *tol)
{
	struct faceuse *fu;

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		struct loopuse *lu;

		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			continue;

		for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			struct edgeuse *eu;

			NMG_CK_LOOPUSE( lu );

			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
				continue;

			for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				struct edgeuse *next_eu;

				next_eu = nmg_next_radial_eu( eu, s , 0 );
				if( next_eu == eu || next_eu == eu->eumate_p )
					return( 1 );
			}
		}
	}

	return( 0 );
}

/**	N M G _ M O V E _ L U _ B E T W E E N _ F U S
 *
 * Moves lu from src faceuse to dest faceuse
 *
 * returns:
 *	0 - All is well
 *	1 - src faceuse is now empty
 */
int
nmg_move_lu_between_fus(struct faceuse *dest, struct faceuse *src, struct loopuse *lu)
{
	struct loopuse *lumate;
	int src_is_empty;

	NMG_CK_FACEUSE( dest );
	NMG_CK_FACEUSE( dest->fumate_p );
	NMG_CK_FACEUSE( src );
	NMG_CK_FACEUSE( src->fumate_p );
	NMG_CK_LOOPUSE( lu );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_move_lu_between_fus( dest=x%x, src=x%x, lu=x%x)\n", dest, src, lu );

	if( lu->up.fu_p != src )
	{
		bu_log( "nmg_move_lu_between_fus( dest=x%x, src=x%x, lu=x%x)\n", dest, src, lu );
		rt_bomb( "\tlu is not in src faceuse\n" );
	}

	if( dest == src )
		return( 0 );

	lumate = lu->lumate_p;
	NMG_CK_LOOPUSE( lumate );

	/* remove lu from src faceuse */
	BU_LIST_DEQUEUE( &lu->l );
	src_is_empty = BU_LIST_IS_EMPTY( &src->lu_hd );

	/* remove lumate from src faceuse mate */
	BU_LIST_DEQUEUE( &lumate->l );
	if( src_is_empty != BU_LIST_IS_EMPTY( &src->fumate_p->lu_hd ) )
	{
		bu_log( "nmg_move_lu_between_fus( dest=x%x, src=x%x, lu=x%x)\n", dest, src, lu );
		if( src_is_empty )
			rt_bomb( "\tsrc faceuse contains only lu, but src->fumate_p has more!!\n" );
		rt_bomb( "\tsrc->fumate_p faceuse contains only lu->lumate_p, but src has more!!\n" );
	}

	/* add lu to dest faceuse */
	BU_LIST_INSERT( &dest->lu_hd, &lu->l );

	/* add lumate to dest mate */
	BU_LIST_INSERT( &dest->fumate_p->lu_hd, &lumate->l );

	/* adjust up pointers */
	lu->up.fu_p = dest;
	lumate->up.fu_p = dest->fumate_p;

	return( src_is_empty );
}

/**	N M G _ L O O P _ P L A N E _ N E W E L L
 *
 *	Calculate the plane equation of a loop using Newell's Method
 *	(See "Graphics Gems III", David Kirk editor, Academic Press, Inc. 1992)
 *
 *	If the loop orientation is OT_OPPOSITE, the normal of the plane
 *	is reversed.
 */
void
nmg_loop_plane_newell(const struct loopuse *lu, fastf_t *pl)
{
	struct edgeuse *eu;
	double hmin,hmax;
	double pl_tmp[3];

	NMG_CK_LOOPUSE( lu );

	VSETALL( pl, 0.0 );
	pl[H] = 0.0;

	VSETALL( pl_tmp, 0.0 );

	/* make sure we have a loop of edges */
	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return;

	/* check if this loop is a crack */
	if( nmg_loop_is_a_crack( lu ) )
		return;

	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return;

	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct edgeuse *eu_next;
		struct vertex_g *vg;
		struct vertex_g *vg_next;

		vg = eu->vu_p->v_p->vg_p;
		eu_next = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
		vg_next = eu_next->vu_p->v_p->vg_p;

		pl_tmp[X] += ( vg->coord[Y] - vg_next->coord[Y]) * ( vg->coord[Z] + vg_next->coord[Z] );
		pl_tmp[Y] += ( vg->coord[Z] - vg_next->coord[Z]) * ( vg->coord[X] + vg_next->coord[X] );
		pl_tmp[Z] += ( vg->coord[X] - vg_next->coord[X]) * ( vg->coord[Y] + vg_next->coord[Y] );
	}

	VUNITIZE( pl_tmp );
	VMOVE( pl, pl_tmp );

	hmin = MAX_FASTF;
	hmax = (-hmin);

	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct vertex_g *vg;
		fastf_t htest;

		vg = eu->vu_p->v_p->vg_p;
		htest = VDOT( vg->coord, pl );
		if( htest > hmax )
			hmax = htest;
		if( htest < hmin )
			hmin = htest;
	}

	pl[H] = (hmax + hmin)/2.0;

	if( lu->orientation == OT_OPPOSITE )
		HREVERSE( pl, pl );
}

/**	N M G _ L O O P _ P L A N E _ A R E A
 *
 *  Calculates a plane equation and the area of a loop
 *
 *  returns:
 *	the area of the loop
 *	less than zero indicates an error
 *
 *  pl is assigned the plane equation for the loop
 *
 */

fastf_t
nmg_loop_plane_area(const struct loopuse *lu, fastf_t *pl)
{
	fastf_t area;
	fastf_t vect_mag;
	fastf_t vect_mag_inv;
	fastf_t pt_dot_plane=0.0;
	fastf_t pt_count=0.0;
	plane_t plane;
	struct edgeuse *eu;
	vect_t trans;

	NMG_CK_LOOPUSE( lu );

	/* make sure we have a loop of edges */
	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return( (fastf_t)(-1.0) );

	/* check if this loop is a crack */
	if( nmg_loop_is_a_crack( lu ) )
		return( (fastf_t)(-1.0) );

	/* calculate a translation to put one vertex at the origin
	 * not necessary, but good for accuracy.
	 * Also, origin must be in plane of loop for this
	 * method to work
	 */
	eu = BU_LIST_FIRST( edgeuse , &lu->down_hd );
	NMG_CK_VERTEXUSE( eu->vu_p );
	NMG_CK_VERTEX( eu->vu_p->v_p );
	NMG_CK_VERTEX_G( eu->vu_p->v_p->vg_p );
	VMOVE( trans , eu->vu_p->v_p->vg_p->coord );

	VSET( plane , 0.0 , 0.0 , 0.0 );

	/* Calculate area and plane normal.
	 * Cross product of each pair of vertices gives twice
	 * the area of the triangle formed by the origin and
	 * the two vertices. (positive if counter-clockwise,
	 * negative if clockwise). In counter_clockwise case,
	 * sum of all cross products around loop adds area for
	 * edges away from origin and subtracts area for edges
	 * near origin, leaving twice the area of the polygon.
	 */
	for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
	{
		struct edgeuse *next_eu;
		struct vertex *vp,*next_vp;
		vect_t cross;
		point_t p1,p2;

		vp = eu->vu_p->v_p;

		next_eu = BU_LIST_PNEXT_CIRC( edgeuse , &eu->l );
		NMG_CK_EDGEUSE( next_eu );
		NMG_CK_VERTEXUSE( next_eu->vu_p );

		next_vp = next_eu->vu_p->v_p;
		NMG_CK_VERTEX( next_vp );
		NMG_CK_VERTEX_G( next_vp->vg_p );

		VSUB2( p1 , vp->vg_p->coord , trans );
		VSUB2( p2 , next_vp->vg_p->coord , trans );
		VCROSS( cross , p1 , p2 );
		VADD2( plane , plane , cross );
	}

	vect_mag = MAGNITUDE( plane );

	/* Error if the area is too small to unitize the normal vector */
	if( vect_mag < SMALL_FASTF )
		return( (fastf_t)(-1.0) );

	area = 0.5 * vect_mag;
	vect_mag_inv = 1.0/vect_mag;

	VSCALE( plane , plane , vect_mag_inv );

	/* calculate plane[3] as average distance to plane */
	for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
	{
		pt_dot_plane += VDOT( plane , eu->vu_p->v_p->vg_p->coord );
		pt_count++;
	}

	/* Error if we don't have at least 3 vertices in this loop */
	if( pt_count < 3 )
		return( (fastf_t)(-1.0) );

	plane[3] = pt_dot_plane/pt_count;
	HMOVE( pl , plane );

	return( area );
}

/**	N M G _ C A L C _ F A C E _ P L A N E
 *
 * Calculate face geometry using a least squares fit or Newell's method.
 *
 * If fu does not already have a face_g_plane associated, only
 * vertices in fu will participate, and if it has only one loop
 * Newell's method will be used rather than a least square fit.
 *
 * if fu has a face_g_plane, then all vertices in any face that
 * references the same face_g_plane will participate in the
 * fit for the face plane.
 *
 * Returns:
 *	0 - All is well
 *	1 - Failed to calculate face geometry
 *
 */

int
nmg_calc_face_plane(struct faceuse *fu_in, fastf_t *pl)
{
	struct faceuse *fu;
	struct bu_ptbl verts;
	plane_t old_pl;
	struct face *f;
	struct face_g_plane *fg;
	struct loopuse *lu;
	mat_t matrix;
	mat_t inverse;
	fastf_t det;
	double one_over_vertex_count;
	vect_t vsum;
	fastf_t min_dist=MAX_FASTF;
	fastf_t max_dist=(-MAX_FASTF);
	int i;
	int got_dir=0;
	int failed=0;
	int loop_count=0;

	fu = fu_in;
	NMG_CK_FACEUSE( fu );

	/* find an OT_SAME loop to use for calculating general direction of normal */
	for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		if( !got_dir && BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_EDGEUSE_MAGIC )
		{
			/* get general direction for face normal */
			nmg_loop_plane_newell( lu , old_pl );
			if( old_pl[X] != 0.0 || old_pl[Y] != 0.0 || old_pl[Z] != 0.0 )
				got_dir = 1;
		}

		loop_count++;
	}

	if( !got_dir )
		return( 1 );

	f = fu->f_p;
	NMG_CK_FACE( f );
	fg = f->g.plane_p;
	if( fg )
	{
		struct face *f1;

		NMG_CK_FACE_G_PLANE( fg );

		/* count loops using this face geometry */
		loop_count = 0;
		for( BU_LIST_FOR( f1 , face, &fg->f_hd ) )
		{
			for( BU_LIST_FOR( lu, loopuse, &f1->fu_p->lu_hd ) )
				loop_count++;
		}

		/* if this face geometry only has one loop using it, just use Newell's method */
		if( loop_count < 2 )
		{
			HMOVE( pl, old_pl );
			return( 0 );
		}

		nmg_tabulate_face_g_verts( &verts , fg );
	}
	else		/* just use vertices from this faceuse */
	{
		/* If this faceuse only has one loop, just use Newell's method */
		if( loop_count < 2 )
		{
			HMOVE( pl, old_pl );
			return( 0 );
		}

		nmg_vertex_tabulate( &verts , &fu->l.magic );
	}

	/* Get the direction for the plane normal in "old_pl".
	 * Make sure we are dealing with OT_SAME or OT_UNSPEC faceuse.
	 */
	if( fu->orientation != OT_UNSPEC )
	{
		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;
		if( fu->orientation != OT_SAME )
		{
			bu_log( "nmg_calc_face_plane: fu x%x has no OT_SAME use\n" , fu );
			bu_ptbl_free( &verts );
			return( 1 );
		}
	}

	/* build matrix */
	MAT_ZERO( matrix );
	VSET( vsum , 0.0 , 0.0 , 0.0 );

	one_over_vertex_count = 1.0/(double)(BU_PTBL_END( &verts ));

	for( i=0 ; i<BU_PTBL_END( &verts ) ; i++ )
	{
		struct vertex *v;
		struct vertex_g *vg;

		v = (struct vertex *)BU_PTBL_GET( &verts , i );
		vg = v->vg_p;

		matrix[0] += vg->coord[X] * vg->coord[X];
		matrix[1] += vg->coord[X] * vg->coord[Y];
		matrix[2] += vg->coord[X] * vg->coord[Z];
		matrix[5] += vg->coord[Y] * vg->coord[Y];
		matrix[6] += vg->coord[Y] * vg->coord[Z];
		matrix[10] += vg->coord[Z] * vg->coord[Z];

		vsum[X] += vg->coord[X];
		vsum[Y] += vg->coord[Y];
		vsum[Z] += vg->coord[Z];
	}
	matrix[4] = matrix[1];
	matrix[8] = matrix[2];
	matrix[9] = matrix[6];
	matrix[15] = 1.0;


	/* Check that we don't have a singular matrix */
	det = bn_mat_determinant( matrix );

	if( !NEAR_ZERO( det , SMALL_FASTF ) )
	{
		fastf_t inv_len_pl;

		/* invert matrix */
		bn_mat_inv( inverse , matrix );

		/* get normal vector */
		MAT4X3PNT( pl , inverse , vsum );

		/* unitize direction vector */
		inv_len_pl = 1.0/(MAGNITUDE( pl ));
		HSCALE( pl , pl , inv_len_pl );

		/* get average vertex coordinates */
		VSCALE( vsum, vsum, one_over_vertex_count );

		/* get distance from plane to orgin */
		pl[H] = VDOT( pl , vsum );

		/* make sure it points in the correct direction */
		if( VDOT( pl , old_pl ) < 0.0 )
			HREVERSE( pl , pl );
	}
	else
	{
		struct vertex *v,*v0;
		int x_same=1;
		int y_same=1;
		int z_same=1;

		/* singular matrix, may occur if all vertices have the same zero
		 * component.
		 */
		v0 = (struct vertex *)BU_PTBL_GET( &verts , 0 );
		for( i=1 ; i<BU_PTBL_END( &verts ) ; i++ )
		{
			v = (struct vertex *)BU_PTBL_GET( &verts , i );

			if( v->vg_p->coord[X] != v0->vg_p->coord[X] )
				x_same = 0;
			if( v->vg_p->coord[Y] != v0->vg_p->coord[Y] )
				y_same = 0;
			if( v->vg_p->coord[Z] != v0->vg_p->coord[Z] )
				z_same = 0;

			if( !x_same && !y_same && !z_same )
				break;
		}

		if( x_same )
		{
			VSET( pl , 1.0 , 0.0 , 0.0 );
		}
		else if( y_same )
		{
			VSET( pl , 0.0 , 1.0 , 0.0 );
		}
		else if( z_same )
		{
			VSET( pl , 0.0 , 0.0 , 1.0 );
		}

		if( x_same || y_same || z_same )
		{
			/* get average vertex coordinates */
			VSCALE( vsum, vsum, one_over_vertex_count );

			/* get distance from plane to orgin */
			pl[H] = VDOT( pl , vsum );

			/* make sure it points in the correct direction */
			if( VDOT( pl , old_pl ) < 0.0 )
				HREVERSE( pl , pl );
		}
		else
		{
			bu_log( "nmg_calc_face_plane: Cannot calculate plane for fu x%x\n" , fu );
			nmg_pr_fu_briefly( fu , (char *)NULL );
			bu_log( "%d verts\n" , BU_PTBL_END( &verts ) );
			failed = 1;
		}
	}

	/* make sure plane is at center of range of vertices */
	for( i=0 ; i<BU_PTBL_END( &verts ) ; i++ )
	{
		struct vertex *v;
		struct vertex_g *vg;
		fastf_t dist;

		v = (struct vertex *)BU_PTBL_GET( &verts, i );
		vg = v->vg_p;

		dist = DIST_PT_PLANE( vg->coord, pl );
		if( dist > max_dist )
			max_dist = dist;
		if( dist < min_dist )
			min_dist = dist;
	}

	pl[H] += (max_dist + min_dist)/2.0;

	bu_ptbl_free( &verts );
	return( failed );
}

/**	N M G _ C A L C _ F A C E _ G
 *
 * interface to nmg_calc_face_plane(), calls nmg_face_g with the
 * resulting plane
 *
 */
int
nmg_calc_face_g(struct faceuse *fu)
{
	plane_t pl;
	int ret_val;

	ret_val = nmg_calc_face_plane( fu, pl );

	if( !ret_val )
		nmg_face_g( fu, pl );

	return( ret_val );
}

/**
 *			N M G _ F A C E U S E _ A R E A
 *
 *	The following routines calculate surface area of
 *	NMG objects. Note that this includes all surfaces,
 *	not just external surfaces, i.e., an NMG object consisting
 *	of two adjacent cubes with a coincident face will have a
 *	surface area of 12*s*s (s is length of one side)
 */

fastf_t
nmg_faceuse_area(const struct faceuse *fu)
{
	struct loopuse *lu;
	plane_t plane;
	fastf_t area=0.0;
	fastf_t tmp_area;

	NMG_CK_FACEUSE( fu );

	for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		tmp_area = nmg_loop_plane_area( lu , plane );
		if( tmp_area < 0.0 )
			continue;

		if( lu->orientation == OT_SAME )
			area += tmp_area;
		else if( lu->orientation == OT_OPPOSITE )
			area -= tmp_area;
		else
		{
			bu_log( "nmg_faceuse_area: Cannot calculate area (lu with %s orientation)\n",
				nmg_orientation( lu->orientation ) );
			return( (fastf_t)-1.0 );
		}
	}

	return( area );
}

fastf_t
nmg_shell_area(const struct shell *s)
{
	fastf_t area=0.0;
	fastf_t tmp_area;
	struct faceuse *fu;

	NMG_CK_SHELL( s );

	for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		if( fu->orientation != OT_SAME )
			continue;

		tmp_area = nmg_faceuse_area( fu );
		if( tmp_area < 0.0 )
			continue;

		area += tmp_area;
	}

	return( area );
}

fastf_t
nmg_region_area(const struct nmgregion *r)
{
	struct shell *s;
	fastf_t area=0.0;

	NMG_CK_REGION( r );

	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
		area += nmg_shell_area( s );

	return( area );
}

fastf_t
nmg_model_area(const struct model *m)
{
	struct nmgregion *r;
	fastf_t area=0.0;

	NMG_CK_MODEL( m );

	for( BU_LIST_FOR( r , nmgregion , &m->r_hd ) )
		area += nmg_region_area( r );

	return( area );
}

/**	R T _ D I S T _ L I N E 3 _ L I N E 3
 *
 *  Calculate closest approach of two lines
 *
 *  returns:
 *	-2 -> lines are parallel and do not intersect
 *	-1 -> lines are parallel and collinear
 *	 0 -> lines intersect
 *	 1 -> lines do not intersect
 *  For return values less than zero, dist is not set.
 *  For return valuse of 0 or 1, dist[0] is the distance
 *  from p1 in the d1 direction to the point of closest
 *  approach for that line.  Similar for the second line.
 *  d1 and d2 must be unit direction vectors.
XXX How is this different from bn_isect_line3_line3() ?
XXX Why are the calling sequences just slightly different?
XXX Can we pick the better one, and get rid of the other one?
XXX If not, can we document how they differ?
 */

int
rt_dist_line3_line3(fastf_t *dist, const fastf_t *p1, const fastf_t *d1, const fastf_t *p2, const fastf_t *d2, const struct bn_tol *tol)
{
	fastf_t d1_d2;
	point_t a1,a2;
	vect_t a1_to_a2;
	vect_t p2_to_p1;
	fastf_t min_dist;
	fastf_t tol_dist_sq;
	fastf_t tol_dist;

	BN_CK_TOL( tol );

	if( tol->dist > 0.0 )
		tol_dist = tol->dist;
	else
		tol_dist = 0.005;

	if( tol->dist_sq > 0.0 )
		tol_dist_sq = tol->dist_sq;
	else
		tol_dist_sq = tol_dist * tol_dist;

	if( !NEAR_ZERO( MAGSQ( d1 ) - 1.0 , tol_dist_sq ) )
	{
		bu_log( "rt_dist_line3_line3: non-unit length direction vector ( %f %f %f )\n" , V3ARGS( d1 ) );
		rt_bomb( "rt_dist_line3_line3\n" );
	}

	if( !NEAR_ZERO( MAGSQ( d2 ) - 1.0 , tol_dist_sq ) )
	{
		bu_log( "rt_dist_line3_line3: non-unit length direction vector ( %f %f %f )\n" , V3ARGS( d2 ) );
		rt_bomb( "rt_dist_line3_line3\n" );
	}

	d1_d2 = VDOT( d1 , d2 );

	if( BN_VECT_ARE_PARALLEL( d1_d2 , tol ) )
	{
		if( bn_dist_line3_pt3( p1 , d1 , p2 ) > tol_dist )
			return( -2 ); /* parallel, but not collinear */
		else
			return( -1 ); /* parallel and collinear */
	}

	VSUB2( p2_to_p1 , p1 , p2 );
	dist[0] = (d1_d2 * VDOT( p2_to_p1 , d2 ) - VDOT( p2_to_p1 , d1 ))/(1.0 - d1_d2 * d1_d2 );
	dist[1] = dist[0] * d1_d2 + VDOT( p2_to_p1 , d2 );

	VJOIN1( a1 , p1 , dist[0] , d1 );
	VJOIN1( a2 , p2 , dist[1] , d2 );

	VSUB2( a1_to_a2 , a2 , a1 );
	min_dist = MAGNITUDE( a1_to_a2 );
	if( min_dist < tol_dist )
		return( 0 );
	else
		return( 1 );
}

/**	R T _ D I S T _ L I N E 3 _ L S E G 3
 *
 *  calculate intersection or closest approach of
 *  a line and a line segement.
 *
 *  returns:
 *	-2 -> line and line segment are parallel and collinear.
 *	-1 -> line and line segment are parallel, not collinear.
 *	 0 -> intersection between points a and b.
 *	 1 -> intersection outside a and b.
 *	 2 -> closest approach is between a and b.
 *	 3 -> closest approach is outside a and b.
 *
 *  dist[0] is actual distance from p in d direction to
 *  closest portion of segment.
 *  dist[1] is ratio of distance from a to b (0.0 at a, and 1.0 at b),
 *  dist[1] may be less than 0 or greater than 1.
 *  For return values less than 0, closest approach is defined as
 *  closest to point p.
 *  Direction vector, d, must be unit length.
XXX This should be moved to libbn/plane.c
XXX probably renamed as bn_dist_line3_line3().
 */

int
rt_dist_line3_lseg3(fastf_t *dist, const fastf_t *p, const fastf_t *d, const fastf_t *a, const fastf_t *b, const struct bn_tol *tol)
{
	vect_t a_to_b;
	vect_t a_dir;
	fastf_t len_ab;
	int outside_segment;
	int ret;

	BN_CK_TOL( tol );

	VSUB2( a_to_b , b , a );
	len_ab = MAGNITUDE( a_to_b );
	VSCALE( a_dir , a_to_b , (1.0/len_ab) );

	ret = rt_dist_line3_line3( dist , p , d , a , a_dir , tol );

	if( ret < 0 )
	{
		vect_t to_a,to_b;
		fastf_t dist_to_a,dist_to_b;

		VSUB2( to_a , a , p );
		VSUB2( to_b , b , p );
		dist_to_a = VDOT( to_a , d );
		dist_to_b = VDOT( to_b , d );

		if( dist_to_a <= dist_to_b )
		{
			dist[0] = dist_to_a;
			dist[1] = 0.0;
		}
		else
		{
			dist[0] = dist_to_b;
			dist[1] = 1.0;
		}
		return( ret );
	}

	if( dist[1] >= (-tol->dist) && dist[1] <= len_ab + tol->dist )
	{
		/* intersect or closest approach between a and b */
		outside_segment = 0;
		dist[1] = dist[1]/len_ab;
		if( dist[1] < 0.0 )
			dist[1] = 0.0;
		if( dist[1] > 1.0 )
			dist[1] = 1.0;
	}
	else
	{
		outside_segment = 1;
		dist[1] = dist[1]/len_ab;
	}

	return( 2*ret + outside_segment );
}


/** N M G _ P U R G E _ U N W A N T E D _ I N T E R S E C T I O N _ P O I
 *  N T S
 *
 *	Make sure that the list of intersection points doesn't contain
 *	any vertexuses from loops whose bounding boxes don;t overlap the
 *	bounding box of a loop in the given faceuse.
 *
 *	This is really a special purpose routine to help the intersection
 *	operations of the boolean process.  The only reason it's here instead
 *	of nmg_inter.c is that it knows too much about the format and contents
 *	of an bu_ptbl structure.
 */
void
nmg_purge_unwanted_intersection_points(struct bu_ptbl *vert_list, fastf_t *mag_list, const struct faceuse *fu, const struct bn_tol *tol)
{
	int			i;
	int			j;
	struct vertexuse	*vu;
	struct loopuse		*lu;
	const struct loop_g	*lg;
	const struct loopuse	*fu2lu;
	const struct loop_g	*fu2lg = (const struct loop_g *)NULL;
	int			overlap = 0;

	NMG_CK_FACEUSE(fu);
	BN_CK_TOL(tol);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	{
		bu_log("nmg_purge_unwanted_intersection_points(0x%08x, 0x%08x)\n", vert_list, fu);
		bu_log("\t%d vertexuses in list\n", vert_list->end );
	}

	for (i=0 ; i < vert_list->end ; i++) {
		vu = (struct vertexuse *)vert_list->buffer[i];
		if( vu->l.magic != NMG_VERTEXUSE_MAGIC )
		{
			/* vertexuse probably killed by nmg_repair_v_near_v() */
			/* delete the entry from the vertex list */
			for (j=i ; j < vert_list->end ; j++)
			{
				vert_list->buffer[j] = vert_list->buffer[j+1];
				mag_list[j] = mag_list[j+1];
			}

			--(vert_list->end);
			vert_list->buffer[vert_list->end] = (long *)NULL;
			mag_list[vert_list->end] = MAX_FASTF;
			--i;
			continue;
		}
		NMG_CK_VERTEXUSE(vu);
		lu = nmg_find_lu_of_vu( vu );
		NMG_CK_LOOPUSE(lu);
		lg = lu->l_p->lg_p;
		NMG_CK_LOOP_G(lg);

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			bu_log("vu[%d]: 0x%08x (%g %g %g) lu: 0x%08x %s\n",
				i, vu, V3ARGS(vu->v_p->vg_p->coord),
				lu, nmg_orientation(lu->orientation) );
			bu_log("\tlu BBox: (%g %g %g) (%g %g %g)\n",
				V3ARGS(lg->min_pt), V3ARGS(lg->max_pt) );
		}
		if (lu->up.fu_p->f_p == fu->f_p)
			bu_log("I'm checking against my own face?\n");

		/* If the bounding box of a loop doesn't intersect the
		 * bounding box of a loop in the other face, it shouldn't
		 * be on the list of intersecting loops.
		 */
		overlap = 0;
		for (BU_LIST_FOR(fu2lu, loopuse, &fu->lu_hd )){
			NMG_CK_LOOPUSE(fu2lu);
			NMG_CK_LOOP(fu2lu->l_p);

			switch(fu2lu->orientation)  {
			case OT_BOOLPLACE:
				/*  If this loop is destined for removal
				 *  by sanitize(), skip it.
				 */
				continue;
			case OT_UNSPEC:
				/* If this is a loop of a lone vertex,
				 * it was deposited into the
				 * other loop as part of an intersection
				 * operation, and is quite important.
				 */
				if( BU_LIST_FIRST_MAGIC(&fu2lu->down_hd) != NMG_VERTEXUSE_MAGIC )
					bu_log("nmg_purge_unwanted_intersection_points() non self-loop OT_UNSPEC vertexuse in fu2?\n");
				break;
			case OT_SAME:
			case OT_OPPOSITE:
				break;
			default:
				bu_log("nmg_purge_unwanted_intersection_points encountered %s loop in fu2\n",
					nmg_orientation(fu2lu->orientation));
				/* Process it anyway */
				break;
			}

			fu2lg = fu2lu->l_p->lg_p;
			NMG_CK_LOOP_G(fu2lg);

			if (rt_g.NMG_debug & DEBUG_POLYSECT) {
				bu_log("\tfu2lu BBox: (%g %g %g)  (%g %g %g) %s\n",
					V3ARGS(fu2lg->min_pt), V3ARGS(fu2lg->max_pt),
					nmg_orientation(fu2lu->orientation) );
			}

			if (V3RPP_OVERLAP_TOL(fu2lg->min_pt, fu2lg->max_pt,
			    lg->min_pt, lg->max_pt, tol)) {
				overlap = 1;
				break;
			}
		}
		if (!overlap) {
			/* why is this vertexuse in the list? */
			if (rt_g.NMG_debug & DEBUG_POLYSECT) {
				bu_log("nmg_purge_unwanted_intersection_points This little bugger slipped in somehow.  Deleting it from the list.\n");
				nmg_pr_vu_briefly(vu, (char *)NULL);
			}
			if( BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC &&
			    lu->orientation == OT_UNSPEC )  {
				/* Drop this loop of a single vertex in sanitize() */
				if (rt_g.NMG_debug & DEBUG_POLYSECT)
					bu_log("nmg_purge_unwanted_intersection_points() remarking as OT_BOOLPLACE\n");
				lu->orientation =
				  lu->lumate_p->orientation = OT_BOOLPLACE;
			}

			/* delete the entry from the vertex list */
			for (j=i ; j < vert_list->end ; j++)
			{
				vert_list->buffer[j] = vert_list->buffer[j+1];
				mag_list[j] = mag_list[j+1];
			}

			--(vert_list->end);
			vert_list->buffer[vert_list->end] = (long *)NULL;
			mag_list[vert_list->end] = MAX_FASTF;
			--i;
		}
	}
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		bu_log("\tAt end of nmg_purge_unwanted_intersection_points, %d vertexuses in list\n",
				vert_list->end );
}


/**				N M G _ I N _ O R _ R E F
 *
 *	if the given vertexuse "vu" is in the table given by "b" or if "vu"
 *	references a vertex which is refernced by a vertexuse in the table,
 *	then we return 1.  Otherwise, we return 0.
 */
int
nmg_in_or_ref(struct vertexuse *vu, struct bu_ptbl *b)
{
	union {
		struct vertexuse **vu;
		long **magic_p;
	} p;
	int i;

	p.magic_p = b->buffer;
	for (i=0 ; i < b->end ; ++i) {
		if (p.vu[i] && *p.magic_p[i] == NMG_VERTEXUSE_MAGIC &&
		    (p.vu[i] == vu || p.vu[i]->v_p == vu->v_p))
			return(1);
	}
	return(0);
}

/**
 *			N M G _ R E B O U N D
 *
 *  Re-compute all the bounding boxes in the NMG model.
 *  Bounding boxes are presently found in these structures:
 *	loop_g
 *	face_g
 *	shell_a
 *	nmgregion_a
 *  The re-bounding must be performed in a bottom-up manner,
 *  computing the loops first, and working up to the nmgregions.
 */
void
nmg_rebound(struct model *m, const struct bn_tol *tol)
{
	struct nmgregion	*r;
	struct shell		*s;
	struct faceuse		*fu;
	struct face		*f;
	struct loopuse		*lu;
	struct loop		*l;
	register int		*flags;

	NMG_CK_MODEL(m);
	BN_CK_TOL(tol);

	flags = (int *)bu_calloc( m->maxindex*2, sizeof(int), "rebound flags[]" );

	for( BU_LIST_FOR( r, nmgregion, &m->r_hd ) )  {
		NMG_CK_REGION(r);
		for( BU_LIST_FOR( s, shell, &r->s_hd ) )  {
			NMG_CK_SHELL(s);

			/* Loops in faces in shell */
			for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				/* Loops in face */
				for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
					NMG_CK_LOOPUSE(lu);
					l = lu->l_p;
					NMG_CK_LOOP(l);
					if( NMG_INDEX_FIRST_TIME(flags,l) )
						nmg_loop_g(l, tol);
				}
			}
			/* Faces in shell */
			for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				f = fu->f_p;
				NMG_CK_FACE(f);

				/* Rebound the face */
				if( NMG_INDEX_FIRST_TIME(flags,f) )
					nmg_face_bb( f, tol );
			}

			/* Wire loops in shell */
			for( BU_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
				NMG_CK_LOOPUSE(lu);
				l = lu->l_p;
				NMG_CK_LOOP(l);
				if( NMG_INDEX_FIRST_TIME(flags,l) )
					nmg_loop_g(l, tol);
			}

			/*
			 *  Rebound the shell.
			 *  This routine handles wire edges and lone vertices.
			 */
			if( NMG_INDEX_FIRST_TIME(flags,s) )
				nmg_shell_a( s, tol );
		}

		/* Rebound the nmgregion */
		nmg_region_a( r, tol );
	}

	bu_free( (char *)flags, "rebound flags[]" );
}

/**
 *			N M G _ C O U N T _ S H E L L _ K I D S
 */
void
nmg_count_shell_kids(const struct model *m, long unsigned int *total_faces, long unsigned int *total_wires, long unsigned int *total_points)
{
	short *tbl;

	const struct nmgregion *r;
	const struct shell *s;
	const struct faceuse *fu;
	const struct loopuse *lu;
	const struct edgeuse *eu;

	NMG_CK_MODEL(m);

	tbl = (short *)bu_calloc(m->maxindex+1, sizeof(char),
		"face/wire/point counted table");

	*total_faces = *total_wires = *total_points = 0;
	for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
		for (BU_LIST_FOR(s, shell, &r->s_hd)) {
			if (s->vu_p) {
				total_points++;
				continue;
			}
			for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
				if (NMG_INDEX_TEST_AND_SET(tbl, fu->f_p))
					(*total_faces)++;
			}
			for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
				if (NMG_INDEX_TEST_AND_SET(tbl, lu->l_p))
					(*total_wires)++;
			}
			for (BU_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
				if (NMG_INDEX_TEST_AND_SET(tbl, eu->e_p))
					(*total_wires)++;
			}
		}
	}

	bu_free((char *)tbl, "face/wire/point counted table");
}

/**
 *	O R D E R _ T B L
 *
 *	private support routine for nmg_close_shell
 *	creates an array of indices into a table of edgeuses, ordered
 *	end-to-end. This may or may not create an actual loop.
 *
 *	Arguments:
 *	tbl is the table (provided by caller)
 *	index is the array of indices created by order_tbl
 *	tbl_size is the size of the table (provided by caller)
 *	loop_size is the number of edgeuses in the loop (calculated by order_tbl)
 */
static void
order_tbl(struct bu_ptbl *tbl, int start_index, int **index, int tbl_size, int *loop_size)
{
	int i,j,k;
	int found;
	struct edgeuse *eu,*eu1;
	struct vertex *start_v;

	/* create an index into the table, ordered to create a loop */
	if( *index == NULL )
		(*index) = (int *)bu_calloc( tbl_size , sizeof( int ) , "Table index" );

	for( i=0 ; i<tbl_size ; i++ )
		(*index)[i] = (-1);

	/* start the loop at index = start_index */
	(*index)[0] = start_index;
	*loop_size = 1;
	eu = (struct edgeuse *)BU_PTBL_GET( tbl , start_index );
	start_v = eu->vu_p->v_p;
	i = 0;
	found = 1;
	while( eu->eumate_p->vu_p->v_p != start_v && found )
	{
		found = 0;

		/* Look for edgeuse that starts where "eu" ends */
		for( j=0 ; j<tbl_size ; j++ )
		{
			int already_used = 0;

			eu1 = (struct edgeuse *)BU_PTBL_GET( tbl , j );

			/* don't use same edgeuse twice!!! */
			for( k=0 ; k<(*loop_size) ; k++ )
			{
				if( eu1 == (struct edgeuse *)BU_PTBL_GET( tbl, (*index)[k] ) )
				{
					already_used = 1;
					break;
				}
			}
			if( already_used )
				continue;
			if( eu1->vu_p->v_p == eu->eumate_p->vu_p->v_p )
			{
				/* Found it */
				found = 1;
				(*index)[++i] = j;
				(*loop_size)++;
				eu = eu1;
				break;
			}
		}
	}
}

/**
 *	N M G _ C L O S E _ S H E L L
 *
 *	Examines the passed shell and, if there are holes, closes them
 *	note that not much care is taken as to how the holes are closed
 *	so the results are not entirely predictable.
 *	A list of free edges is created (edges bounding only one face).
 *	New faces are constructed by taking two consecutive edges
 *	and making a face. The newly created edge is added to the list
 *	of free edges and the two used ones are removed.
 *
 */
void
nmg_close_shell(struct shell *s, const struct bn_tol *tol)
{
	struct bu_ptbl eu_tbl;		/* table of free edgeuses from shell */
	struct bu_ptbl vert_tbl;	/* table of vertices for use in nmg_cface */
	int *index;			/* array of indices into eu_tbl, ordered to form a loop */
	int loop_size;			/* number of edgeueses in loop */
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	int start_loop;
	int i;
	int found;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_close_shell: s = x%x\n" , s );

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	index = NULL;

	/* construct a table of free edges */
	(void)bu_ptbl_init( &eu_tbl , 64, " &eu_tbl ");

	/* loop through all the faces in the shell */
	for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		/* only look at OT_SAME faces */
		if( fu->orientation == OT_SAME )
		{
			/* loop through each loopuse in the face */
			for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				/* ignore loops that are just a vertex */
				if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) ==
					NMG_VERTEXUSE_MAGIC )
						continue;

				/* loop through all the edgeuses in the loop */
				for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );
					/* if this edgeuse is a free edge, add its mate to the list */
					if( eu->radial_p == eu->eumate_p )
						(void)bu_ptbl_ins( &eu_tbl , (long *) eu->eumate_p );
				}
			}
		}
	}

	/* if there is nothing in our list of free edges, the shell is already closed */
	if( BU_PTBL_END( &eu_tbl ) == 0 )
	{
		bu_ptbl_free( &eu_tbl );
		return;
	}

	/* create a table of vertices */
	(void)bu_ptbl_init( &vert_tbl , 64, " &vert_tbl ");

	while( BU_PTBL_END( &eu_tbl ) )
	{
		int give_up_on_face=0;

		/* Create an index into the table that orders the edgeuses into a loop */
		start_loop = -1;
		loop_size = 0;
		while( loop_size < 2 )
		{
			if( ++start_loop > BU_PTBL_END( &eu_tbl ) - 3 )
				break;
			order_tbl( &eu_tbl, start_loop , &index , BU_PTBL_END( &eu_tbl ) , &loop_size );
		}

		/* Create new faces to close the shell */
		while( loop_size > 3 )
		{
			struct edgeuse *eu1, *eu2=NULL, *eu_new = NULL;
			struct edgeuse **eu_used;	/* array of edgueses used, for deletion */
			int edges_used=0;			/* number of edges used in making a face */
			int found_face=0;		/* flag - indicates that a face with the correct normal will be created */
			int start_index,end_index;	/* start and stop index for loop */
			int coplanar;			/* flag - indicates entire loop is coplanar */
			plane_t pl1,pl2;		/* planes for checking coplanarity of loop */
			point_t pt[3];			/* points for calculating planes */

			/* Look for an easy way out, maybe this loop is planar */
			/* first, calculate a plane from the first three non-collinear vertices */
			start_index = 0;
			end_index = start_index + 3;

			for( i=start_index ; i<end_index ; i++ )
			{
				eu = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[i] );
				VMOVE( pt[i-start_index] , eu->vu_p->v_p->vg_p->coord );
			}
			while( bn_mk_plane_3pts( pl1 , pt[0] , pt[1] , pt[2] , tol ) && end_index<loop_size )
			{
				start_index++;
				end_index++;
				for( i=start_index ; i<end_index ; i++ )
				{
					eu = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[i] );
					VMOVE( pt[i-start_index] , eu->vu_p->v_p->vg_p->coord );
				}
			}
			if( end_index == loop_size )
			{
				if( BU_PTBL_END( &eu_tbl ) > loop_size )
				{
					int old_loop_size=loop_size;

					loop_size = 0;
					while( loop_size < 3 )
					{
						found = 1;
						for( start_loop=index[0]+1 ; start_loop < BU_PTBL_END( &eu_tbl )-3 ; start_loop++ )
						{
							found = 0;
							for( i=0 ; i<old_loop_size ; i++ )
							{
								if( index[i] == start_loop )
								{
									found = 1;
									break;
								}
							}
							if( !found )
								break;
						}
						if( found )
						{
							/* Could not even make a plane, this is some serious screw-up */
							bu_log( "nmg_close_shell: cannot make any planes from loop, old_loop_size = %d\n", old_loop_size );
							for( i=0 ; i<old_loop_size ; i++ )
							{
								eu= (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[i] );
								bu_log( "(%g %g %g) <-> (%g %g %g)\n",
									V3ARGS( eu->vu_p->v_p->vg_p->coord ),
									V3ARGS( eu->eumate_p->vu_p->v_p->vg_p->coord ) );
							}
							bu_bomb( "nmg_close_shell: cannot make any planes from loop\n" );
						}
						order_tbl( &eu_tbl, start_loop , &index , BU_PTBL_END( &eu_tbl ) , &loop_size );
					}
				}
			}

			/* now we have one plane, let's check the others */
			coplanar = 1;
			while( end_index < loop_size && coplanar )
			{
				end_index +=3;
				if( end_index > loop_size )
					end_index = loop_size;
				start_index = end_index - 3;

				for( i=start_index ; i<end_index ; i++ )
				{
					eu = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[i] );
					VMOVE( pt[i-start_index] , eu->vu_p->v_p->vg_p->coord );
				}

				/* if these three points make a plane, is it coplanar with
				 * our first one??? */
				if( !bn_mk_plane_3pts( pl2 , pt[0] , pt[1] , pt[2] , tol ) )
				{
					if( (i=bn_coplanar( pl1 , pl2 , tol )) < 1 )
						coplanar = 0;
				}
			}

			if( coplanar )	/* excellent! - just make one big face */
			{
				struct edgeuse *eu_tmp;

				/* put vertices in table */
				bu_ptbl_reset( &vert_tbl );
				for( i=0 ; i<loop_size ; i++ )
				{
					eu = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[i] );
					bu_ptbl_ins( &vert_tbl , (long *)&eu->vu_p->v_p );
				}

				/* make face */
				fu = nmg_cmface( s , (struct vertex ***)BU_PTBL_BASEADDR(&vert_tbl) , loop_size );

				/* face geometry */
				if( nmg_loop_plane_area( BU_LIST_FIRST( loopuse , &fu->lu_hd ) , pl2 ) < 0.0 )
				{
					bu_log( "Failed planeeq\n" );
					nmg_kfu( fu );
				}
				else
				{
					nmg_face_g( fu , pl2 );

					/* Calculate face bounding box */
					nmg_face_bb( fu->f_p , tol );
				}

				/* now eliminate loop from table */
				eu_used = (struct edgeuse **)bu_calloc( loop_size , sizeof( struct edguse *) , "edges used list" );
				for( i=0 ; i<loop_size ; i++ )
					eu_used[i] = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[i] );

				for( i=0 ; i<loop_size ; i++ )
					bu_ptbl_rm( &eu_tbl , (long *)eu_used[i] );

				bu_free( (char *)eu_used , "edge used list" );

				/* may need to remove one more edgeuse from table */
				eu_tmp = nmg_find_e( (*(struct vertex **)BU_PTBL_GET( &vert_tbl, 0 )), (*(struct vertex **)BU_PTBL_GET( &vert_tbl, loop_size-1 )), (struct shell *)NULL, (struct edge *)NULL );
				if( eu_tmp )
				{
					if( eu_tmp->radial_p != eu_tmp->eumate_p )
					{
						for( i=0 ; i<BU_PTBL_END( &eu_tbl ) ; i++ )
						{
							struct edgeuse *eu2;

							eu2 = (struct edgeuse *)BU_PTBL_GET( &eu_tbl, i );
							if( EDGESADJ( eu2, eu_tmp ) )
							{
								bu_ptbl_rm( &eu_tbl , (long *)eu2 );
								break;
							}
						}
					}
				}

				/* set some flags to get us back to start of loop */
				found_face = 1;
				give_up_on_face = 1;
			}

			/* OK, so we have to do this one little-by-little */
			start_index = -1;
			end_index = -1;
			while( !found_face )
			{
				/* refresh the vertex list */
				(void)bu_ptbl_reset( &vert_tbl );

				if( end_index == 0 )
				{
					give_up_on_face = 1;
					break;
				}
				start_index++;
				end_index = start_index + 1;
				if( end_index == loop_size )
					end_index = 0;

				/* Get two edgeuses from the loop */
				eu1 = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[start_index] );
				bu_ptbl_ins( &vert_tbl , (long *)&eu1->vu_p->v_p );

				eu2 = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[end_index] );
				bu_ptbl_ins( &vert_tbl , (long *)&eu2->vu_p->v_p );

				edges_used = 2;
				/* if the edges are collinear, we can't make a face */
				while( bn_3pts_collinear(
					eu1->vu_p->v_p->vg_p->coord,
					eu2->vu_p->v_p->vg_p->coord,
					eu2->eumate_p->vu_p->v_p->vg_p->coord,
					tol ) && edges_used < loop_size )
				{
					/* So, add another edge */
					end_index++;
					if( end_index == loop_size )
						end_index = 0;
					eu1 = eu2;
					eu2 = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[end_index]);
					bu_ptbl_ins( &vert_tbl , (long *)&eu2->vu_p->v_p );
					edges_used++;
				}

				found_face = 1;
#if 0
				/* check that newly created edge isn't already a winged edge */
				vfirst = (*(struct vertex **)BU_PTBL_GET( &vert_tbl, 0 ));
				vlast = (*(struct vertex **)BU_PTBL_GET( &vert_tbl, edges_used-1 ));
				eu_tmp = nmg_find_e( vfirst, vlast, (struct shell *)NULL, (struct edge *)NULL );
				if( eu_tmp )
				{
					if( eu_tmp->radial_p != eu_tmp->eumate_p )
						found_face = 0;
				}
#endif

			}

			if( give_up_on_face )
			{
				loop_size = 0;
				start_loop = -1;
				break;
			}

			/* add last vertex to table */
			bu_ptbl_ins( &vert_tbl , (long *)&eu2->eumate_p->vu_p->v_p );

			/* save list of used edges to be removed later */
			eu_used = (struct edgeuse **)bu_calloc( edges_used , sizeof( struct edguse *) , "edges used list" );
			for( i=0 ; i<edges_used ; i++ )
				eu_used[i] = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[i] );

			/* make a face */
			fu = nmg_cmface( s , (struct vertex ***)BU_PTBL_BASEADDR(&vert_tbl) , edges_used+1 );
			/* out with the old, in with the new */
			for( i=0 ; i<edges_used ; i++ )
				bu_ptbl_rm( &eu_tbl , (long *)eu_used[i] );

			if( nmg_loop_plane_area( BU_LIST_FIRST( loopuse , &fu->lu_hd ) , pl2 ) < 0.0 )
			{
				bu_log( "Failed planeeq\n" );
				nmg_kfu( fu );
			}
			else
			{
				nmg_face_g( fu , pl2 );

				/* find the newly created edgeuse */
				found = 0;
				for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
				{
					NMG_CK_LOOPUSE( lu );
					if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) ==
						NMG_VERTEXUSE_MAGIC )
							continue;
					for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
					{
						NMG_CK_EDGEUSE( eu );
						if( eu->vu_p->v_p == (*(struct vertex **)BU_PTBL_GET( &vert_tbl , 0 ))
						&& eu->eumate_p->vu_p->v_p == (*(struct vertex **)BU_PTBL_GET( &vert_tbl , edges_used) ) )
						{
							eu_new = eu;
							found = 1;
							break;
						}
						else if( eu->vu_p->v_p == (*(struct vertex **)BU_PTBL_GET( &vert_tbl , edges_used))
						&& eu->eumate_p->vu_p->v_p == (*(struct vertex **)BU_PTBL_GET( &vert_tbl , 0 ) ) )
						{
							eu_new = eu->eumate_p;
							found = 1;
							break;
						}

					}
					if( found )
						break;
				}

				if( eu_new->radial_p == eu_new->eumate_p )
					bu_ptbl_ins( &eu_tbl , (long *)eu_new );
				else
				{
					struct edgeuse *eu_tmp;

					/* find third eu to be removed from eu_tbl */
					for( i=0 ; i<BU_PTBL_END( &eu_tbl ) ; i++ )
					{
						eu_tmp = (struct edgeuse *)BU_PTBL_GET( &eu_tbl, i );
						if( eu_tmp->vu_p->v_p == eu_new->vu_p->v_p &&
						    eu_tmp->eumate_p->vu_p->v_p == eu_new->eumate_p->vu_p->v_p )
						{
							bu_ptbl_rm( &eu_tbl , (long *)eu_tmp );
							break;
						}
					}
				}
			}

			bu_free( (char *)eu_used , "edge used list" );

			/* re-order loop */
			loop_size = 0;
			start_loop = -1;
			while( loop_size < 3 && start_loop < BU_PTBL_END( &eu_tbl )-3 )
				order_tbl( &eu_tbl, ++start_loop , &index , BU_PTBL_END( &eu_tbl ) , &loop_size );
		}

		if( give_up_on_face )
			continue;

		if( loop_size == 2 )
		{
			bu_ptbl_reset( &vert_tbl );
			eu = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[0] );
			(void)bu_ptbl_ins( &vert_tbl , (long *)&eu->vu_p->v_p);
			eu = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[1] );
			(void)bu_ptbl_ins( &vert_tbl , (long *)&eu->vu_p->v_p);
			(void)bu_ptbl_ins( &vert_tbl , (long *)&eu->eumate_p->vu_p->v_p);
			if( !bn_3pts_collinear(
				((*(struct vertex **)BU_PTBL_GET( &vert_tbl , 0 )))->vg_p->coord,
				((*(struct vertex **)BU_PTBL_GET( &vert_tbl , 1 )))->vg_p->coord,
				((*(struct vertex **)BU_PTBL_GET( &vert_tbl , 2 )))->vg_p->coord,
				tol ) )
			{
				fu = nmg_cmface( s , (struct vertex ***)BU_PTBL_BASEADDR(&vert_tbl) , 3 );
				if( nmg_calc_face_g( fu ) )
				{
					bu_log( "Failed planeeq\n" );
					nmg_kfu( fu );
				}
			}
			else
				bu_log( "Not makeing face, edges are collinear!\n" );

			loop_size = 0;
			continue;
		}
		else if( loop_size < 2 )
		{
			loop_size = 0;
			continue;
		}

		/* if the last 3 vertices are collinear, then don't make the last face */
		bu_ptbl_reset( &vert_tbl );
		for( i=0 ; i<3 ; i++ )
		{
			eu = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[i] );
			(void)bu_ptbl_ins( &vert_tbl , (long *)&eu->vu_p->v_p);
		}

		if( !bn_3pts_collinear(
			((*(struct vertex **)BU_PTBL_GET( &vert_tbl , 0 )))->vg_p->coord,
			((*(struct vertex **)BU_PTBL_GET( &vert_tbl , 1 )))->vg_p->coord,
			((*(struct vertex **)BU_PTBL_GET( &vert_tbl , 2 )))->vg_p->coord,
			tol ) )
		{

			/* Create last face from remaining 3 edges */
			fu = nmg_cmface( s , (struct vertex ***)BU_PTBL_BASEADDR(&vert_tbl) , 3 );
			if( nmg_calc_face_g( fu ) )
				bu_log( "Failed planeeq\n" );

		}
		else
			bu_log( "Not makeing face, edges are collinear!\n" );

		/* remove the last three edges from the table */
		{
			struct edgeuse *eu1,*eu2,*eu3;

			eu1 = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[0] );
			eu2 = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[1] );
			eu3 = (struct edgeuse *)BU_PTBL_GET( &eu_tbl , index[2] );
			bu_ptbl_rm( &eu_tbl , (long *)eu1 );
			bu_ptbl_rm( &eu_tbl , (long *)eu2 );
			bu_ptbl_rm( &eu_tbl , (long *)eu3 );
		}
	}

	/* Free up all the memory */
	bu_free( (char *)index , "index" );
	bu_ptbl_free( &eu_tbl );
	bu_ptbl_free( &vert_tbl );

}

/**
 *	N M G _ D U P _ S H E L L
 *
 *	Duplicate a shell and return the new copy. New shell is
 *	in the same region.
 *
 *  The vertex geometry is copied from the source faces into topologically
 *  distinct (new) vertex and vertex_g structs.
 *  They will start out being geometricly coincident, but it is anticipated
 *  that the caller will modify the geometry, e.g. as in an extrude operation.
 *
 *  NOTE: This routine creates a translation table that gives the
 *  correspondence between old and new structures, the caller is responsible
 *  for freeing this memory. Warning - NOT EVERY structure is assigned a
 *  correspondence.
 */
struct shell *
nmg_dup_shell(struct shell *s, long int ***trans_tbl, const struct bn_tol *tol)
{

	struct model *m;
	struct shell *new_s;
	struct faceuse *fu;
	struct loopuse *lu,*new_lu;
	struct edgeuse *eu;
	struct faceuse *new_fu;
	struct bu_ptbl faces;
	int tbl_size;

	if( rt_g.NMG_debug & DEBUG_BASIC)
		bu_log( "nmg_dup_shell( s = x%x , trans_tbl = x%x )\n" , s , trans_tbl );

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	m = nmg_find_model( (long *)s );

	/* create translation table double size to accomodate both copies */
	tbl_size = m->maxindex * 3;
	(*trans_tbl) = (long **)bu_calloc(tbl_size, sizeof(long *),
		"nmg_dup_shell trans_tbl" );

	bu_ptbl_init( &faces , 64, " &faces ");

	new_s = nmg_msv( s->r_p );
	if( s->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
	NMG_INDEX_ASSIGN( (*trans_tbl) , s , (long *)new_s );
	if( new_s->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
	NMG_INDEX_ASSIGN( (*trans_tbl) , new_s , (long *)s );

	/* copy face uses */
	for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
		{
			new_fu = (struct faceuse *)NULL;
			for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				if( new_fu )
				{
					new_lu = nmg_dup_loop( lu , &new_fu->l.magic , (*trans_tbl) );
					if( lu->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
					NMG_INDEX_ASSIGN( (*trans_tbl) , lu , (long *)new_lu );
					if( new_lu->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
					NMG_INDEX_ASSIGN( (*trans_tbl) , new_lu , (long *)lu );
				}
				else
				{
					new_lu = nmg_dup_loop( lu , &new_s->l.magic , (*trans_tbl) );
					if( new_lu->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
					NMG_INDEX_ASSIGN( (*trans_tbl) , lu , (long *)new_lu );
					if( lu->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
					NMG_INDEX_ASSIGN( (*trans_tbl) , new_lu , (long *)lu );
					new_fu = nmg_mf( new_lu );
					if( lu->orientation == OT_OPPOSITE )
					{
						/* nmg_mf forces loops to OT_SAME */
						new_lu->orientation = OT_OPPOSITE;
						new_lu->lumate_p->orientation = OT_OPPOSITE;
					}
					if( fu->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
					NMG_INDEX_ASSIGN( (*trans_tbl) , fu , (long *)new_fu );
					if( new_fu->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
					NMG_INDEX_ASSIGN( (*trans_tbl) , new_fu , (long *)fu );
					if( fu->fumate_p->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
					NMG_INDEX_ASSIGN( (*trans_tbl) , fu->fumate_p , (long *)new_fu->fumate_p );
					if( new_fu->fumate_p->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
					NMG_INDEX_ASSIGN( (*trans_tbl) , new_fu->fumate_p , (long *)fu->fumate_p );
					if( fu->f_p->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
					NMG_INDEX_ASSIGN( (*trans_tbl) , fu->f_p , (long *)new_fu->f_p );
					if( new_fu->f_p->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
					NMG_INDEX_ASSIGN( (*trans_tbl) , new_fu->f_p , (long *)fu->f_p );
				}
			}
			if (fu->f_p->g.plane_p)
			{
#if 1
				/* Do it this way if you expect to change the normals */
				plane_t	n;
				NMG_GET_FU_PLANE( n, fu );
				nmg_face_g(new_fu, n);
#else
				/* Do it this way to share fu's geometry struct */
				nmg_jfg( fu, new_fu );
#endif

				/* XXX Perhaps this should be new_fu->f_p->g.plane_p ? */
				if( fu->f_p->g.plane_p->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
				NMG_INDEX_ASSIGN( (*trans_tbl) , fu->f_p->g.plane_p , (long *)new_fu->f_p->g.plane_p );
				if( new_fu->f_p->g.plane_p->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
				NMG_INDEX_ASSIGN( (*trans_tbl) , new_fu->f_p->g.plane_p , (long *)fu->f_p->g.plane_p );
			}
			new_fu->orientation = fu->orientation;
			new_fu->fumate_p->orientation = fu->fumate_p->orientation;
			bu_ptbl_ins( &faces , (long *)new_fu );
		}
	}

	/* glue new faces */
	nmg_gluefaces( (struct faceuse **)BU_PTBL_BASEADDR( &faces) , BU_PTBL_END( &faces ), tol );
	bu_ptbl_free( &faces );

	/* copy wire loops */
	for( BU_LIST_FOR( lu , loopuse , &s->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );
		new_lu = nmg_dup_loop( lu , &new_s->l.magic , (*trans_tbl) );
		if( lu->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
		NMG_INDEX_ASSIGN( (*trans_tbl) , lu , (long *)new_lu );
		if( new_lu->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
		NMG_INDEX_ASSIGN( (*trans_tbl) , new_lu , (long *)lu );
	}

	/* copy wire edges */
	for( BU_LIST_FOR( eu , edgeuse , &s->eu_hd ) )
	{
		struct vertex *old_v1,*old_v2,*new_v1,*new_v2;
		struct edgeuse *new_eu;

		NMG_CK_EDGEUSE( eu );
		NMG_CK_VERTEXUSE( eu->vu_p );
		NMG_CK_VERTEX( eu->vu_p->v_p );
		NMG_CK_EDGEUSE( eu->eumate_p );
		NMG_CK_VERTEXUSE( eu->eumate_p->vu_p );
		NMG_CK_VERTEX( eu->eumate_p->vu_p->v_p );

		old_v1 = eu->vu_p->v_p;
		new_v1 = NMG_INDEX_GETP(vertex, (*trans_tbl), old_v1);
		old_v2 = eu->eumate_p->vu_p->v_p;
		new_v2 = NMG_INDEX_GETP(vertex, (*trans_tbl), old_v2);

		/* make the wire edge */
		new_eu = nmg_me( new_v1 , new_v2 , new_s );
		if( eu->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
		NMG_INDEX_ASSIGN( (*trans_tbl) , eu , (long *)new_eu );
		if( new_eu->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
		NMG_INDEX_ASSIGN( (*trans_tbl) , new_eu , (long *)eu );

		new_v1 = new_eu->vu_p->v_p;
		if( old_v1->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
		NMG_INDEX_ASSIGN( (*trans_tbl) , old_v1 , (long *)new_v1 );
		if( new_v1->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
		NMG_INDEX_ASSIGN( (*trans_tbl) , new_v1 , (long *)old_v1 );
		if( !new_v1->vg_p )
		{
			nmg_vertex_gv( new_v1 , old_v1->vg_p->coord );
			if( old_v1->vg_p->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
			NMG_INDEX_ASSIGN( (*trans_tbl) , old_v1->vg_p , (long *)new_v1->vg_p );
			if( new_v1->vg_p->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
			NMG_INDEX_ASSIGN( (*trans_tbl) , new_v1->vg_p , (long *)old_v1->vg_p );
		}

		new_v2 = new_eu->eumate_p->vu_p->v_p;
		if( old_v2->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
		NMG_INDEX_ASSIGN( (*trans_tbl) , old_v2 , (long *)new_v2 );
		if( new_v2->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
		NMG_INDEX_ASSIGN( (*trans_tbl) , new_v2 , (long *)old_v2 );
		if( !new_v2->vg_p )
		{
			nmg_vertex_gv( new_v2 , old_v2->vg_p->coord );
			if( old_v2->vg_p->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
			NMG_INDEX_ASSIGN( (*trans_tbl) , old_v2->vg_p , (long *)new_v2->vg_p );
			if( new_v2->vg_p->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
			NMG_INDEX_ASSIGN( (*trans_tbl) , new_v2->vg_p , (long *)old_v2->vg_p );
		}

	}

#if 0
	/* XXX for this to work nmg_mvu and nmg_mvvu must not be private
	 *     perhaps there is another way???? */
	/* copy vertex use
	 * This must be done last, since other routines may steal it */
	if( s->vu_p )
	{
		old_vu = s->vu_p;
		NMG_CK_VERTEXUSE( old_vu );
		old_v = old_vu->v_p;
		NMG_CK_VERTEX( old_v );
		new_v = NMG_INDEX_GETP(vertex, (*trans_tbl), old_v);
		if( new_v )
		{
			/* already copied vertex, just need a use */
			if( new_s->vu_p )
				(void )nmg_kvu( new_s->vu_p );
			new_s->vu_p = nmg_mvu( new_v , (long *)new_s , m );
		}
		else
		{
			/* make a new vertex and use */
			new_s->vu_p = nmg_mvvu( (long *)new_s , m );
			new_v = new_s->vu_p->v_p;

			/* put entry in table */
			if( old_v->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
			NMG_INDEX_ASSIGN( (*trans_tbl) , old_v , (long *)new_v );
			if( new_v->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
			NMG_INDEX_ASSIGN( (*trans_tbl) , new_v , (long *)old_v );

			/* assign the same geometry as the old copy */
			nmg_vertex_gv( new_v , old_v->vg_p->coord );
			if( old_v->vg_p->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
			NMG_INDEX_ASSIGN( (*trans_tbl) , old_v->vg_p , (long *)new_v->vg_p );
			if( new_v->vg_p->index >= tbl_size ) rt_bomb( "nmg_dup_shell: trans table exceeded\n" );
			NMG_INDEX_ASSIGN( (*trans_tbl) , new_v->vg_p , (long *)old_v->vg_p );
		}
	}
#endif

	return( new_s );
}

/*	Routines to use the bu_ptbl structures as a stack of edgeuse structures */

#define	NMG_PUSH( _ptr , _stack )	bu_ptbl_ins( _stack , (long *) _ptr );

struct edgeuse
*nmg_pop_eu(struct bu_ptbl *stack)
{
	struct edgeuse *eu;

	/* return a NULL if stack is empty */
	if( BU_PTBL_END( stack ) == 0 )
		return( (struct edgeuse *)NULL );

	/* get last edgeuse on the stack */
	eu = (struct edgeuse *)BU_PTBL_GET( stack , BU_PTBL_END( stack )-1 );

	/* remove that edgeuse from the stack */
	bu_ptbl_rm( stack , (long *)eu );

	return( eu );
}

void
nmg_reverse_radials(struct faceuse *fu, const struct bn_tol *tol)
{
	struct loopuse *lu;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_reverse_radials( fu = x%x )\n" , fu );

	NMG_CK_FACEUSE( fu );
	BN_CK_TOL( tol );

	for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	{
		struct edgeuse *eu;
		struct edgeuse *eu_radial;
		struct edgeuse *eumate;
		struct edgeuse *eumate_radial;

		NMG_CK_LOOPUSE( lu );

		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		{
			eu_radial = eu->radial_p;
			eumate = eu->eumate_p;
			eumate_radial = eumate->radial_p;

			/* if no radials continue to next edgeuse in loop */
			if( eu_radial == eumate )
				continue;

			/* Note: there is no problem if this loop is radial to another
			 * loop in the same face, the radials will get switched as we process
			 * the first loop, then switched back as we process the second
			 */

			eu_radial->radial_p = eumate;
			eu->radial_p = eumate_radial;
			eumate_radial->radial_p = eu;
			eumate->radial_p = eu_radial;
		}
	}
}

/* XXX Don't use this, use nmg_s_radial_harmonize() at the right time. */
/**	N M G _ R E V E R S E _ F A C E _ A N D _ R A D I A L S
 *
 *	This routine calls "nmg_reverse_face" and also makes the radial
 *	pointers connect faces of like orientation (i.e., OT_SAME to OT_SAME and
 *	OT_OPPOSITE to OT_OPPOSITE).
 */

void
nmg_reverse_face_and_radials(struct faceuse *fu, const struct bn_tol *tol)
{
	struct loopuse *lu;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_reverse_face_and_radials( fu = x%x )\n" , fu );

	NMG_CK_FACEUSE( fu );
	BN_CK_TOL( tol );

	/* reverse face */
	nmg_reverse_face( fu );

	for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	{
		struct edgeuse *eu;
		struct edgeuse *eu_radial;
		struct edgeuse *eumate;
		struct edgeuse *eumate_radial;

		NMG_CK_LOOPUSE( lu );

		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		{
			eu_radial = eu->radial_p;
			eumate = eu->eumate_p;
			eumate_radial = eumate->radial_p;

			/* if no radials continue to next edgeuse in loop */
			if( eu_radial == eumate )
				continue;

			/* Note: there is no problem if this loop is radial to another
			 * loop in the same face, the radials will get switched as we process
			 * the first loop, then switched back as we process the second
			 */

			eu_radial->radial_p = eumate;
			eu->radial_p = eumate_radial;
			eumate_radial->radial_p = eu;
			eumate->radial_p = eu_radial;
		}
	}
}

/**	N M G _ S H E L L _ I S _ V O I D
 *
 * determines if the shell is a void shell or an exterior shell
 * by finding the topmost face (in some direction) and looking at
 * the component of the OT_SAME faceuse normal in that direction.
 *
 * returns:
 *	0 - shell is exterior shell
 *	1 - shell is a void shell
 *	-1 - cannot determine
 *
 * It is expected that this shell is the result of nmg_decompose_shells.
 */
int
nmg_shell_is_void(const struct shell *s)
{
	struct model *m;
	struct face *f;
	struct faceuse *fu;
	vect_t normal;
	int dir;
	long *flags;

	NMG_CK_SHELL( s );

	m = nmg_find_model( &s->l.magic );
	NMG_CK_MODEL( m );

	flags = (long *)bu_calloc( m->maxindex , sizeof( long ) , "nmg_shell_is_void: flags " );

	f = nmg_find_top_face( s, &dir , flags );

	bu_free( (char *)flags , "nmg_shell_is_void: flags" );

	if( f == (struct face *)NULL )
		return( -1 );

	NMG_CK_FACE( f );
	NMG_CK_FACE_G_PLANE( f->g.plane_p );
	fu = f->fu_p;
	NMG_CK_FACEUSE( fu );

	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;
	if( fu->orientation != OT_SAME )
		return( -1 );

	NMG_GET_FU_NORMAL( normal , fu );

	if( normal[dir] == 0.0 )
		return( -1 );
	if( normal[dir] < 0.0)
		return( 1 );
	else
		return( 0 );
}

/**	N M G _ P R O P A G A T E _ N O R M A L S
 *
 *	This routine expects "fu_in" to have a correctly oriented normal.
 *	It then checks all faceuses in the same shell it can reach via radial structures, and
 *	reverses faces and modifies radial structures as needed to result in
 *	a consistent NMG shell structure. The "flags" variable is a translation table
 *	for the model, and as each face is checked, its flag is set. Faces with flags
 *	that have already been set will not be checked by this routine.
 */

void
nmg_propagate_normals(struct faceuse *fu_in, long int *flags, const struct bn_tol *tol)
{
	struct bu_ptbl stack;
	struct loopuse *lu;
	struct edgeuse *eu,*eu1;
	struct faceuse *fu;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_propagate_normals( fu_in = x%x , flags = x%x )\n" , fu_in , flags );

	NMG_CK_FACEUSE( fu_in );
	BN_CK_TOL( tol );

	fu = fu_in;
	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;
	if( fu->orientation != OT_SAME )
	{
		bu_log( "nmg_propagate_normals: Could not find OT_SAME orientation of faceuse x%x\n" , fu_in );
		return;
	}

	/* set flag for this face since we know this one is OK */
	NMG_INDEX_SET( flags , fu->f_p );

	/* Use the ptbl structure as a stack */
	bu_ptbl_init( &stack , 64, " &stack ");

	/* push all edgeuses of "fu" onto the stack */
	for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );
		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;
		for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			/* don't push free edges on the stack */
			if( eu->radial_p->eumate_p != eu )
				NMG_PUSH( eu , &stack );
		}
	}

	/* now pop edgeuses from stack, go to radial face, fix its normal,
	 * and push its edgeuses onto the stack */

	while( (eu1 = nmg_pop_eu( &stack )) != (struct edgeuse *)NULL )
	{
		/* eu1 is an edgeuse on an OT_SAME face, so its radial
		 * should be in an OT_SAME also */

		NMG_CK_EDGEUSE( eu1 );

		eu = eu1->radial_p;
		NMG_CK_EDGEUSE( eu );

		/* find the face that contains this edgeuse */
		fu = nmg_find_fu_of_eu( eu );
		if( !fu )
			continue;

		NMG_CK_FACEUSE( fu );

		/* if this face has already been processed, skip it */
		if( NMG_INDEX_TEST_AND_SET( flags , fu->f_p ) )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "nmg_propagate_normals: checking fu x%x, eu = x%x, eu1 = x%x\n" , fu, eu, eu1 );

			if( fu->orientation == OT_SAME )
			{
				if( eu1->vu_p->v_p == eu->vu_p->v_p &&
					eu1->eumate_p->vu_p->v_p == eu->eumate_p->vu_p->v_p )
				{
					/* edge direction is wrong, need to reverse
					 * both the face and the radials
					 */
					nmg_reverse_face_and_radials( fu , tol );
				}
				/* else - this is the way it should be */
			}
			else if( fu->orientation == OT_OPPOSITE )
			{
				if( eu1->vu_p->v_p == eu->vu_p->v_p &&
					eu1->eumate_p->vu_p->v_p == eu->eumate_p->vu_p->v_p )
				{
					/* just swap radial pointer around */
					nmg_reverse_radials( fu , tol );
				}
				else
				{
					/* just reverse the face */
					nmg_reverse_face( fu );
				}
			}
			else
			{
				bu_log( "nmg_propagate_normals: found an unoriented face!!!!\n" );
				nmg_pr_fu_briefly( fu, "" );
				rt_bomb( "nmg_propagate_normals: found an unoriented face!!!!\n" );
			}

			/* make sure we are dealing with an OT_SAME faceuse */
			if( fu->orientation != OT_SAME )
				fu = fu->fumate_p;

			/* push all edgeuses of "fu" onto the stack */
			for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
					continue;
				for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					/* don't push free edges on the stack */
					if( eu->radial_p->eumate_p != eu )
						NMG_PUSH( eu , &stack );
				}
			}
		}
	}

	/* free the stack */
	bu_ptbl_free( &stack );
}


/**		N M G _ D I S C O N N E C T _ S H E L L S
 *
 *	looks for edges that have uses in more than one shell in region.
 *	creates new edges so that there is no sharing of edges among shells
 */
static void
nmg_disconnect_shells(struct nmgregion *r)
{
	struct shell *s1;
	struct bu_ptbl edges;
	int count=0;
	int i;

	NMG_CK_REGION( r );

	/* count number of shells in this region */
	for( BU_LIST_FOR( s1, shell, &r->s_hd ) )
		count++;

	/* if there is less than two shells, nothing to do */
	if( count < 2 )
		return;

	/* get a list of all edges in this region */
	nmg_edge_tabulate( &edges, &r->l.magic );

	/* look at every edge in region */
	for( i=0 ; i<BU_PTBL_END( &edges ) ; i++ )
	{
		struct edge *e;
		struct edgeuse *eu;
		struct edgeuse *eu1;
		struct shell *needs_disconnect=(struct shell *)NULL;

		e = (struct edge *)BU_PTBL_GET( &edges, i );
		NMG_CK_EDGE( e );

		/* find shell of a use of this edge */
		eu = e->eu_p;
		s1 = nmg_find_s_of_eu( eu );

		/* check if any other use of this edge is from a different shell */
		eu1 = eu->eumate_p->radial_p;
		while( eu1 != eu &&  eu1 != eu->eumate_p )
		{
			struct shell *s2;

			if( (s2 = nmg_find_s_of_eu( eu1 )) != s1 )
			{
				needs_disconnect = s2;
				break;
			}
			eu1 = eu1->eumate_p->radial_p;
		}

		while( needs_disconnect )
		{
			struct edgeuse *eu2;
			struct edgeuse *start_eu;
			struct edgeuse *last;
			int last_orientation;

			/* disconnect first use of this edge in shell 'needs_disconnect' */
			start_eu = eu1->radial_p;
			nmg_unglueedge( eu1 );
			last = eu1;
			last_orientation = (nmg_find_fu_of_eu( eu1 ))->orientation;

			/* now disconnect all other uses, reconnecting them to eu1 as we go  */
			while( nmg_find_s_of_eu( start_eu ) == needs_disconnect )
				start_eu = start_eu->eumate_p->radial_p;
			eu2 = start_eu;
			do
			{
				struct edgeuse *next_eu;
				struct faceuse *fu2;

				/* find uses in 'needs_disconnect' shell */
				next_eu = eu2->eumate_p->radial_p;
				if( nmg_find_s_of_eu( eu2 ) == needs_disconnect )
				{

					/* disconnect this use */
					nmg_unglueedge( eu2 );
					fu2 = nmg_find_fu_of_eu( eu2 );

					/* reconnect it to 'needs_disconnect' shell */
					if( fu2->orientation == last_orientation)
					{
						nmg_je( last, eu2 );
						last = eu2->eumate_p;
						last_orientation = fu2->fumate_p->orientation;
					}
					else
					{
						nmg_je( last, eu2->eumate_p );
						last_orientation = fu2->orientation;
						last = eu2;
					}
				}
				eu2 = next_eu;
			} while( eu2 != start_eu && eu2->eumate_p != start_eu );

			/* now check remaining uses */
			eu1 = eu->eumate_p->radial_p;
			needs_disconnect = (struct shell *)NULL;
			while( eu1 != eu &&  eu1 != eu->eumate_p )
			{
				struct shell *s2;

				if( (s2 = nmg_find_s_of_eu( eu1 )) != s1 )
				{
					needs_disconnect = s2;
					break;
				}
				eu1 = eu1->eumate_p->radial_p;
			}
		}

	}
}

/**		N M G _ C O N N E C T _ S A M E _ F U _ O R I E N T S
 *
 *	looks for radially connected faceuses that have disagreeing orientations.
 *	if such a condiftion is found, the radial pointers are rearranged to make
 *	the radial faces agree in orientation.
 */
void
nmg_connect_same_fu_orients(struct shell *s)
{
	struct faceuse *fu;

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		struct loopuse *lu;

		if( fu->orientation != OT_SAME )
			continue;

		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		{
			struct edgeuse *eu;

			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
				continue;

			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				struct faceuse *fu2;
				struct faceuse *fu3;
				struct edgeuse *eu2;

				eu2 = eu->radial_p;

				if( eu2 == eu->eumate_p )
					continue;

				fu2 = nmg_find_fu_of_eu( eu2 );
				if( fu2->orientation == OT_SAME )
					continue;

				fu3 = nmg_find_fu_of_eu( eu->eumate_p->radial_p );
				if( fu3->orientation == OT_OPPOSITE )
					continue;
			}
		}
	}
}

/**		N M G _ F I X _ D E C O M P O S E D_ S H E L L _ N O R M A L S
 *
 *	Routine to set all OT_SAME faceuse normals to outward direction.
 *	Assumes that there are no other shells sharing edges with this one.
 */

void
nmg_fix_decomposed_shell_normals(struct shell *s, const struct bn_tol *tol)
{
	struct model *m;
	struct face *f_top;
	struct faceuse *fu;
	vect_t normal;
	int dir;
	int missed_faces;
	long *flags;

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	m = s->r_p->m_p;
	flags = (long *)bu_calloc( m->maxindex , sizeof( long ) , "nmg_fix_decomposed_shell_normals: flags" );

missed:
	/* find the top face */
	f_top = nmg_find_top_face( s, &dir , flags );
	if( f_top == (struct face *)NULL )
	{
		bu_log( "nmg_fix_decomposed_shell_normals: Could not get a top face from nmg_find_top_face()\n" );
		bu_log( "\tWARNING: continuing without fixing normals!!!!\n" );
		bu_free( (char *)flags, "nmg_fix_decomposed_shell_normals: flags" );
		return;
	}
	if( *f_top->g.magic_p != NMG_FACE_G_PLANE_MAGIC )
	{
		NMG_INDEX_SET( flags, f_top );
		goto missed;
	}

	if( NMG_INDEX_TEST( flags, f_top ) )
		bu_log(" nmg_find_top_face returned a flagged face %x\n" , f_top );

	NMG_CK_FACE( f_top );
	fu = f_top->fu_p;
	NMG_CK_FACEUSE( fu );
	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;
	if( fu->orientation != OT_SAME )
	{
		bu_log( "nmg_fix_decomposed_shell_normals: no OT_SAME use of top face\n" );
		bu_free( (char *)flags , "nmg_fix_decomposed_shell_normals: flags" );
		return;
	}
	NMG_GET_FU_NORMAL( normal , fu );

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		bu_log( "\tnmg_fix_decomposed_shell_normals: top face is x%x in %d direction, OT_SAME use is x%x\n", f_top, dir, fu );
		bu_log( "\toutward normal = ( %g %g %g )\n" , V3ARGS( normal ) );
	}

	/* f_top is the topmost face (in the "dir" direction), so its OT_SAME use should have a
	 * normal with component in the "dir" direction */
	if( normal[dir] < 0.0 )
	{
		if( rt_g.NMG_debug & DEBUG_BASIC )
			bu_log( "nmg_fix_decomposed_shell_normals: reversing fu x%x\n" , fu );

		nmg_reverse_face_and_radials( fu, tol );
	}

	/* get OT_SAME use of top face */
	fu = f_top->fu_p;
	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;

	NMG_CK_FACEUSE( fu );

	/* fu is now known to be a correctly oriented faceuse,
	 * propagate this throughout the shell, face by face, by
	 * traversing the shell using the radial edge structure */

	nmg_propagate_normals( fu , flags , tol );

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		vect_t new_norm;

		NMG_GET_FU_NORMAL( new_norm , fu );
		bu_log( "nmg_fix_decomposed_shell_normals: After propagation top faceuse normal is ( %g %g %g )\n",
			V3ARGS( new_norm ) );
	}

	/* check if all the faces have been processed */
	missed_faces = 0;
	for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
		{
			if( !NMG_INDEX_TEST( flags , fu->f_p ) )
				missed_faces++;
		}
	}

	if( missed_faces )
	{
		bu_log( "nmg_fix_decomposed_shell_normals: missed %d faces in shell x%x (was it decomposed?)\n",
			missed_faces, s );
		rt_bomb( "nmg_fix_decomposed_shell_normals: missed faces in shell (was it decomposed?)\n" );
	}

	bu_free( (char *)flags, "flags" );
}

/**		N M G _ M K _ M O D E L _ F R O M _ R E G I O N
 *
 *	Creates a new model from an existing nmgregion.
 *	Will refuse to create new model if the passed nmgregion has
 *	children with uses in another nmgregion.
 *
 *	The reindex flag indicates if the new model should be reindexed.
 *		If non-zero, nmg_m_reindex() is called.
 *		If zero, the maxindex field of the new model is filled by finding
 *			the largest index value in the new model and adding one.
 *		This is useful if the indices in the region are being used for table look-up.
 */
struct model *
nmg_mk_model_from_region(struct nmgregion *r, int reindex)
{
	struct model *m;
	struct bu_ptbl tbl;
	int i;
	int other_uses=0;

	NMG_CK_REGION( r );


	/* check if anything in this region has uses in another region */
	nmg_vertex_tabulate( &tbl, &r->l.magic );

	for( i=0 ; i<BU_PTBL_END( &tbl ) ; i++ )
	{
		struct vertex *v;
		struct vertexuse *vu;

		v = (struct vertex *)BU_PTBL_GET( &tbl, i );
		NMG_CK_VERTEX( v );

		for( BU_LIST_FOR( vu, vertexuse, &v->vu_hd ) )
		{
			if( (nmg_find_s_of_vu( vu ))->r_p != r )
			{
				bu_log( "vertexuse x%x (v=x%x) at (%g %g %g) has use in another region\n",
					vu, v, V3ARGS( v->vg_p->coord ) );
				other_uses++;
			}
		}
	}
	bu_ptbl_free( &tbl );

	nmg_edge_tabulate( &tbl, &r->l.magic );
	for( i=0 ; i<BU_PTBL_END( &tbl ) ; i++ )
	{
		struct edge *e;
		struct edgeuse *eu;
		struct edgeuse *eu1;

		e = (struct edge *)BU_PTBL_GET( &tbl, i );
		NMG_CK_EDGE( e );

		eu = e->eu_p;
		NMG_CK_EDGEUSE( eu );

		eu1 = eu->radial_p->eumate_p;
		while( eu1 != eu )
		{
			if( (nmg_find_s_of_eu( eu1 ))->r_p != r )
			{
				bu_log( "edgeuse x%x (e=x%x) at (%g %g %g)<->(%g %g %g0 has use in another region\n",
					eu, e, V3ARGS( eu->vu_p->v_p->vg_p->coord ), V3ARGS( eu->eumate_p->vu_p->v_p->vg_p->coord ) );
				other_uses++;
			}
			eu1 = eu1->radial_p->eumate_p;
		}
	}
	bu_ptbl_free( &tbl );

	if( other_uses )
	{
		return( (struct model *)NULL );
	}

	m = nmg_mm();

	BU_LIST_DEQUEUE( &r->l );

	BU_LIST_INSERT( &m->r_hd, &r->l );

	r->m_p = m;

	if( reindex )
		nmg_m_reindex( m , 0 );
	else
		m->maxindex = nmg_find_max_index( m ) + 1;

	return( m );
}

/**		N M G _ F I X _ N O R M A L S
 *
 *	Routine to set faceuse normlas to correct direction.
 *
 *	Method:
 *		1. Make a  copy of the shell in another model.
 *		2. Decompose the copy into constituent shells.
 *		3. Set all normals of constituent shells outward (no void shells)
 *		4. Use nmg_classify_s_vs_s() for every pair of shells.
 *		5. Mark any shell that is inside an odd number of other shells as a void shell.
 *		6. Compare original faceuses with the results in the copy and adjust normals as needed.
 *		7. Destroy the copy model.
 */
void
nmg_fix_normals(struct shell *s_orig, const struct bn_tol *tol)
{
	struct model *tmp_m;
	struct model *m;
	struct shell *dup_s;
	struct shell *s1;
	struct nmgregion *tmp_r;
	struct faceuse *fu;
	struct bu_ptbl reverse;
	int shell_count;
	long **trans_tbl;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_fix_normals( s = x%x )\n" , s_orig );

	NMG_CK_SHELL( s_orig );
	BN_CK_TOL( tol );

	/* Currently we can only fix normals for planar faces
	 * check that there are no TNURB faces
	 */
	for( BU_LIST_FOR( fu, faceuse, &s_orig->fu_hd ) )
	{
		struct face *f;

		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			continue;

		f = fu->f_p;

		if( !f->g.magic_p )
		{
			bu_log( "nmg_fix_normals failed, found a face with no geometry (x%x)\n", f );
			return;
		}

		if( *f->g.magic_p != NMG_FACE_G_PLANE_MAGIC )
		{
			bu_log( "nmg_fix_normals: non-planar face found (x%x)\n", f );
			bu_log( "	cannot fix normals\n" );
			return;
		}
	}

	m = s_orig->r_p->m_p;

	/* make a temporary nmgregion for us to work in */
	tmp_r = nmg_mrsv( m );

	/* get rid of the automatically created shell */
	(void)nmg_ks( BU_LIST_FIRST( shell, &tmp_r->s_hd ) );

	/* make a copy of the shell of interest */
	dup_s = nmg_dup_shell( s_orig, &trans_tbl, tol );

	/* move the copy to our work area */
	nmg_mv_shell_to_region( dup_s, tmp_r );

	/* move duplicate shell to another model */
	tmp_m = nmg_mk_model_from_region( tmp_r, 0 ); /* don't reindex, We need the old indices */
	nmg_rebound( tmp_m, tol );

	/* decompose the shell */
	shell_count = nmg_decompose_shell( dup_s, tol );

	if( shell_count == 1 )
	{
		/* just one shell, so fix it and return */
		(void)nmg_km( tmp_m );
		bu_free( (char *)trans_tbl, "translate table" );
		nmg_connect_same_fu_orients( s_orig );
		nmg_fix_decomposed_shell_normals( s_orig, tol );
		return;
	}

	/* make sure the shells don't share any edges */
	nmg_disconnect_shells( tmp_r );

	/* Make sure all OT_SAME faceuses are radial to OT_SAME faceuses */
	for( BU_LIST_FOR( s1, shell, &tmp_r->s_hd ) )
		nmg_connect_same_fu_orients( s1 );

	/* Decomposed into more than one shell.
	 * Need to check for inner void shells.
	 * Start by making all the shells look like solids (no voids).
	 */
	for( BU_LIST_FOR( s1, shell, &tmp_r->s_hd ) )
		nmg_fix_decomposed_shell_normals( s1, tol );

	/* initialize a list of shells to be reversed */
	bu_ptbl_init( &reverse, 8, "Ptbl for nmg_fix_normals" );

	/* now check which shells are inside others */
	for( BU_LIST_FOR( s1, shell, &tmp_r->s_hd ) )
	{
		struct shell *s2;
		int inner_count=0;

		for( BU_LIST_FOR( s2, shell, &tmp_r->s_hd ) )
		{
			int class;

			if( s1 == s2 )
				continue;

			class = nmg_classify_s_vs_s( s1, s2, tol );
			if( class == NMG_CLASS_AinB )
				inner_count++;
			else if( class == NMG_CLASS_Unknown )
			{
				bu_log( "nmg_fix_normals: nmg_classify_s_vs_s() failed for shells x%x and x%x\n", s1, s2 );
				bu_log( "   Continueing anyway (shell is likely to have incorrectly oriented normals)\n" );
			}
		}

		if( inner_count % 2 )
		{
			/* shell s1 is inside an odd number of shells, so it must be a void */
			bu_ptbl_ins( &reverse, (long *)s1 );
		}
	}

	/* now set faces in orignal shell to match our calculations */
	nmg_connect_same_fu_orients( s_orig );

	for( BU_LIST_FOR( s1, shell, &tmp_r->s_hd ) )
	{
		int reversed;

		if( bu_ptbl_locate( &reverse, (long *)s1 ) == (-1 ))
			reversed = 0;
		else
			reversed = 1;

		for( BU_LIST_FOR( fu, faceuse, &s1->fu_hd ) )
		{
			struct faceuse *fu_in_s;
			vect_t normal;
			vect_t normal_in_s;

			if( fu->orientation != OT_SAME )
				continue;

			fu_in_s = NMG_INDEX_GETP( faceuse, trans_tbl, fu );
			if( !fu_in_s )
			{
				bu_log( "fu x%x does not have corrrespondence in original shell\n", fu );
				nmg_pr_fu_briefly( fu, "" );
				continue;
			}
			if( fu_in_s->orientation != OT_SAME  )
				fu_in_s = fu_in_s->fumate_p;

			NMG_GET_FU_NORMAL( normal, fu );
			if( reversed )
				VREVERSE( normal, normal )

			NMG_GET_FU_NORMAL( normal_in_s, fu_in_s );

			if( VDOT( normal, normal_in_s ) < 0.0 )
			{
				nmg_reverse_face_and_radials( fu_in_s, tol );
			}
		}
	}

	bu_ptbl_free( &reverse );
	bu_free( (char *)trans_tbl, "translation table" );

	nmg_km( tmp_m );
}

/**	N M G _ B R E A K _ L O N G _ E D G E S
 *
 *	This codes looks for situations as illustrated:
 *
 *    *------->*-------->*--------->*
 *    *<----------------------------*
 *
 *	where one long edgeuse (the bottom one above) and two or more
 *	shorter edgeusess (the tops ones) are collinear and have the same
 *	start and end vertices.  The code breaks the longer edgeuse into
 *	ones that can be radials of the shorter ones.
 *	Returns the number of splits performed.
 */

int
nmg_break_long_edges(struct shell *s, const struct bn_tol *tol)
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	int split_count=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_break_long_edges( s = x%x )\n" , s );

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	/* look at every edgeuse in the shell */
	for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );

		for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );

			/* skip loops of a single vertex */
			if( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
				continue;

			for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				struct vertexuse *vu;

				NMG_CK_EDGEUSE( eu );

				/* look for an edgeuse that terminates on this vertex */
				for( BU_LIST_FOR( vu , vertexuse , &eu->vu_p->v_p->vu_hd ) )
				{
					struct edgeuse *eu1;

					NMG_CK_VERTEXUSE( vu );

					/* skip vertexuses that are not part of an edge */
					if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
						continue;

					eu1 = vu->up.eu_p;

					/* don't consider the edge we already found!!! */
					if( eu1 == eu )
						continue;

					/* if it terminates at the same vertex as eu, skip it */
					if( eu1->eumate_p->vu_p->v_p == eu->eumate_p->vu_p->v_p )
						continue;

					/* get the mate (so it terminates at "vu") */
					eu1 = eu1->eumate_p;

					/* make sure it is collinear with "eu" */
					if( bn_3pts_collinear( eu->vu_p->v_p->vg_p->coord ,
						eu->eumate_p->vu_p->v_p->vg_p->coord ,
						eu1->vu_p->v_p->vg_p->coord , tol ) )
					{
						/* make sure we break the longer edge
						 * and that the edges are in opposite directions */
						vect_t v0,v1;

						VSUB2( v0 , eu->eumate_p->vu_p->v_p->vg_p->coord , eu->vu_p->v_p->vg_p->coord );
						VSUB2( v1 , eu1->eumate_p->vu_p->v_p->vg_p->coord , eu1->vu_p->v_p->vg_p->coord );

						if (MAGSQ( v0 ) > MAGSQ( v1 ) && VDOT( v0 , v1 ) < 0.0 )
						{
							if( rt_g.NMG_debug & DEBUG_BASIC )
							{
								bu_log( "Breaking edge from ( %f %f %f ) to ( %f %f %f ) at ( %f %f %f )\n",
									V3ARGS( eu->vu_p->v_p->vg_p->coord ),
									V3ARGS( eu->eumate_p->vu_p->v_p->vg_p->coord ),
									V3ARGS( eu1->vu_p->v_p->vg_p->coord ) );
							}
							(void) nmg_ebreak(eu1->vu_p->v_p, eu);
							split_count++;
						}
						else if( rt_g.NMG_debug & DEBUG_BASIC )
						{
							bu_log( "Not splitting collinear edges x%x and x%x:\n", eu , eu1 );
							bu_log( "\t( %f %f %f ) -> ( %f %f %f )\n",
								V3ARGS( eu->vu_p->v_p->vg_p->coord ),
								V3ARGS( eu->eumate_p->vu_p->v_p->vg_p->coord ) );
							bu_log( "\t( %f %f %f ) -> ( %f %f %f )\n",
								V3ARGS( eu1->vu_p->v_p->vg_p->coord ),
								V3ARGS( eu1->eumate_p->vu_p->v_p->vg_p->coord ) );
						}
					}
				}
			}
		}
	}
	return( split_count );
}

/**	N M G _ M K _ N E W _ F A C E _ F R O M _ L O O P
 *
 *  Remove a loopuse from an existing face and construct a new face
 *  from that loop
 *
 *  Returns new faceuse as built by nmg_mf()
 *
 */
struct faceuse *
nmg_mk_new_face_from_loop(struct loopuse *lu)
{
	struct shell *s;
	struct faceuse *fu;
	struct loopuse *lu1;
	struct loopuse *lu_mate;
	int ot_same_loops=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_mk_new_face_from_loop( lu = x%x )\n" , lu );

	NMG_CK_LOOPUSE( lu );

	if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )
	{
		bu_log( "nmg_mk_new_face_from_loop: loopuse is not in a faceuse\n" );
		return( (struct faceuse *)NULL );
	}

	fu = lu->up.fu_p;
	NMG_CK_FACEUSE( fu );

	s = fu->s_p;
	NMG_CK_SHELL( s );

	/* Count the number of exterior loops in this faceuse */
	for( BU_LIST_FOR( lu1 , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu1 );
		if( lu1->orientation == OT_SAME )
			ot_same_loops++;
	}

	if( ot_same_loops == 1 && lu->orientation == OT_SAME )
	{
		bu_log( "nmg_mk_new_face_from_loop: cannot remove only exterior loop from faceuse\n" );
		return( (struct faceuse *)NULL );
	}

	lu_mate = lu->lumate_p;

	/* remove loopuse from faceuse */
	BU_LIST_DEQUEUE( &lu->l );

	/* remove its mate from faceuse mate */
	BU_LIST_DEQUEUE( &lu_mate->l );

	/* insert these loops in the shells list of wire loops
	 * put the original loopuse at the head of the list
	 * so that it will end up as the returned faceuse from "nmg_mf"
	 */
	BU_LIST_INSERT( &s->lu_hd , &lu_mate->l );
	BU_LIST_INSERT( &s->lu_hd , &lu->l );

	/* set the "up" pointers to the shell */
	lu->up.s_p = s;
	lu_mate->up.s_p = s;

	/* Now make the new face */
	return( nmg_mf( lu ) );
}

/* state for nmg_split_loops_into_faces */
struct nmg_split_loops_state
{
	long		*flags;		/* index based array of flags for model */
	const struct bn_tol	*tol;
	int		split;		/* count of faces split */
};

void
nmg_split_loops_handler(long int *fu_p, genptr_t sl_state, int after)
{
	struct faceuse *fu;
	struct nmg_split_loops_state *state;
	struct loopuse *lu;
	const struct bn_tol *tol;
	int otsame_loops=0;
	int otopp_loops=0;

	fu = (struct faceuse *)fu_p;
	NMG_CK_FACEUSE( fu );

	state = (struct nmg_split_loops_state *)sl_state;
	tol = state->tol;

	if( fu->orientation != OT_SAME )
		return;

	if( !NMG_INDEX_TEST_AND_SET( state->flags , fu ) )  return;

	NMG_INDEX_SET( state->flags , fu->fumate_p );

	for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );

		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		if( lu->orientation == OT_SAME )
			otsame_loops++;
		else if( lu->orientation == OT_OPPOSITE )
			otopp_loops++;
		else
		{
			bu_log( "nmg_split_loops_into_faces: facuse (x%x) with %s loopuse (x%x)\n",
				fu , nmg_orientation( lu->orientation ) , lu );
			return;
		}
	}

	/* if there is only one OT_SAME loop in this faceuse, nothing to do */
	if( otsame_loops == 1 )
		return;

	if( otopp_loops && otsame_loops )
	{
		struct bu_ptbl inside_loops;

		bu_ptbl_init( &inside_loops , 64, " &inside_loops ");

		lu = BU_LIST_FIRST( loopuse , &fu->lu_hd );
		while( BU_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			struct faceuse *new_fu;
			struct loopuse *lu_next;
			struct loopuse *lu1;
			plane_t plane;
			int index;

			lu_next = BU_LIST_PNEXT( loopuse , &lu->l );

			if( otsame_loops == 1 )
			{
				/* done */
				bu_ptbl_free( &inside_loops );
				return;
			}

			if( lu->orientation != OT_SAME )
			{
				lu = lu_next;
				continue;
			}

			/* find OT_OPPOSITE loops for this exterior loop */
			for( BU_LIST_FOR( lu1 , loopuse , &fu->lu_hd ) )
			{
				struct loopuse *lu2;
				int hole_in_lu=1;

				if( lu1 == lu )
					continue;

				/* skip loops that are not OT_OPPOSITE */
				if( lu1->orientation != OT_OPPOSITE )
					continue;

				/* skip loops that are not within lu */
				if( nmg_classify_lu_lu( lu1 , lu , tol ) != NMG_CLASS_AinB )
					continue;

				/* lu1 is an OT_OPPOSITE loopuse within the OT_SAME lu
				 * but lu1 may be within yet another loopuse that is
				 * also within lu (nested loops)
				 */

				/* look for another OT_SAME loopuse within lu */
				for( BU_LIST_FOR( lu2 , loopuse , &fu->lu_hd ) )
				{

					if( lu2 == lu || lu2 == lu1 )
						continue;

					if( lu2->orientation != OT_SAME )
						continue;

					if( nmg_classify_lu_lu( lu2 , lu , tol ) == NMG_CLASS_AinB )
					{
						/* lu2 is within lu, does it contain lu1?? */
						if( nmg_classify_lu_lu( lu1 , lu2 , tol ) == NMG_CLASS_AinB )
						{
							/* Yes, lu1 is within lu2, so lu1 is not
							 * a hole in lu
							 */
							hole_in_lu = 0;;
							break;
						}
					}
				}

				if( hole_in_lu )
					bu_ptbl_ins( &inside_loops , (long *)lu1 );
			}

			NMG_GET_FU_PLANE( plane , fu );

			new_fu = nmg_mk_new_face_from_loop( lu );
			nmg_face_g( new_fu , plane );

			for( index=0 ; index<BU_PTBL_END( &inside_loops ) ; index++ )
			{
				lu1 = (struct loopuse *)BU_PTBL_GET( &inside_loops , index );
				nmg_move_lu_between_fus( new_fu , fu , lu1 );
				otopp_loops--;
			}
			nmg_face_bb( new_fu->f_p, tol );
			bu_ptbl_reset( &inside_loops );
			otsame_loops--;
			lu = lu_next;
		}
		bu_ptbl_free( &inside_loops );
	}
	else if( otsame_loops )
	{
		/* all loops are of the same orientation, just make a face for every loop */
		int first=1;
		struct faceuse *new_fu;

		lu = BU_LIST_FIRST( loopuse , &fu->lu_hd );
		while( BU_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			struct loopuse *next_lu;

			next_lu = BU_LIST_PNEXT( loopuse , &lu->l );

			if( first )
				first = 0;
			else
			{
				plane_t plane;

				if( lu->orientation == OT_SAME )
				{
					NMG_GET_FU_PLANE( plane , fu );
				}
				else
				{
					NMG_GET_FU_PLANE( plane , fu->fumate_p );
				}
				new_fu = nmg_mk_new_face_from_loop( lu );
				nmg_face_g( new_fu , plane );
				nmg_face_bb( new_fu->f_p, tol );
			}

			lu = next_lu;
		}
	}
	else
	{
		/* faceuse has only OT_OPPOSITE loopuses */
		bu_log( "nmg_split_loops_into_faces: fu (x%x) has only OT_OPPOSITE loopuses, ignored\n" , fu );
	}
}

/**	N M G _ S P L I T _ L O O P S _ I N T O _ F A C E S
 *
 *	Visits each faceuse and splits disjoint loops into
 *	seperate faces.
 *
 *	Returns the number of faces modified.
 */
int
nmg_split_loops_into_faces(long int *magic_p, const struct bn_tol *tol)
{
	struct model *m;
	struct nmg_split_loops_state sl_state;
	int count;
	static const struct nmg_visit_handlers htab = {NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, NULL, nmg_split_loops_handler,
						       NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, NULL, NULL};
	/* htab.aft_faceuse = nmg_split_loops_handler; */

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_split_loops_into_faces( magic_p = x%x )\n" , magic_p );

	BN_CK_TOL( tol );

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL( m );

	BN_CK_TOL( tol );

	sl_state.split = 0;
	sl_state.flags = (long *)bu_calloc( m->maxindex*2 , sizeof( long ) , "nmg_split_loops_into_faces: flags" );
	sl_state.tol = tol;

	nmg_visit( magic_p , &htab , (genptr_t)&sl_state );

	count = sl_state.split;

	bu_free( (char *)sl_state.flags , "nmg_split_loops_into_faces: flags" );

	return( count );
}

/**	N M G _ D E C O M P O S E _ S H E L L
 *
 *	Accepts one shell and breaks it to the minimum number
 *	of disjoint shells.
 *
 *	explicit returns:
 *		# of resulting shells ( 1 indicates that nothing was done )
 *
 *	implicit returns:
 *		additional shells in the passed in shell's region.
 */
int
nmg_decompose_shell(struct shell *s, const struct bn_tol *tol)
{
	int missed_faces;
	int no_of_shells=1;
	int shell_no=1;
	int i,j;
	struct model *m;
	struct nmgregion *r;
	struct shell *new_s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct edgeuse *eu1;
	struct faceuse *missed_fu = NULL;
	struct bu_ptbl stack;
	struct bu_ptbl shared_edges;
	long *flags;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_decompose_shell( s = x%x ) START\n" , s );

	NMG_CK_SHELL( s );

	BN_CK_TOL( tol );

	/* Make an index table to insure we visit each face once and only once */
	r = s->r_p;
	NMG_CK_REGION( r );
	m = r->m_p;
	NMG_CK_MODEL( m );
	flags = (long *)bu_calloc( m->maxindex*2 , sizeof( long ) , "nmg_decompose_shell: flags" );

	bu_ptbl_init( &stack , 64, " &stack ");
	bu_ptbl_init( &shared_edges , 64, " &shared_edges ");

	/* Need to be sure that every face has just one OT_SAME loop */
	(void)nmg_split_loops_into_faces( &s->l.magic , tol );

	/* get first faceuse from shell */
	fu = BU_LIST_FIRST( faceuse , &s->fu_hd );
	NMG_CK_FACEUSE( fu );
	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;
	if( fu->orientation != OT_SAME )
		rt_bomb( "First face in shell has no OT_SAME uses!!!!\n" );

	/* put all edguses of first faceuse on the stack */
	for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );
		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			/* build two lists, one of winged edges, the other not */
			if( nmg_radial_face_count( eu , s ) > 2 )
				bu_ptbl_ins_unique( &shared_edges , (long *)eu );
			else
				bu_ptbl_ins_unique( &stack , (long *)eu );
		}
	}

	/* Mark first faceuse and mate with shell number */
	NMG_INDEX_ASSIGN( flags , fu , shell_no );
	NMG_INDEX_ASSIGN( flags , fu->fumate_p , shell_no );

	/* now pop edgeuse of the stack and visit faces radial to edgeuse */
	while( (eu1 = nmg_pop_eu( &stack )) != (struct edgeuse *)NULL )
	{
		NMG_CK_EDGEUSE( eu1 );

		/* move to the radial */
		eu = eu1->radial_p;
		NMG_CK_EDGEUSE( eu );

		/* make sure we stay within the intended shell */
		while( nmg_find_s_of_eu( eu ) != s && eu != eu1 && eu != eu1->eumate_p )
			eu = eu->eumate_p->radial_p;

		if( eu == eu1 || eu == eu1->eumate_p )
			continue;

		fu = nmg_find_fu_of_eu( eu );
		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;

		/* if this faceuse has already been visited, skip it */
		if( !NMG_INDEX_TEST( flags , fu ) )
		{
			/* push all edgeuses of "fu" onto the stacks */
			for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );

				if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
					continue;

				for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					/* build two lists, one of winged edges, the other not */
					if( nmg_radial_face_count( eu , s ) > 2 )
						bu_ptbl_ins_unique( &shared_edges , (long *)eu );
					else
						bu_ptbl_ins_unique( &stack , (long *)eu );
				}
			}
			/* Mark this faceuse and its mate with a shell number */
			NMG_INDEX_ASSIGN( flags , fu , shell_no );
			NMG_INDEX_ASSIGN( flags , fu->fumate_p , shell_no );
		}
	}

	/* count number of faces that were not visited */
	missed_faces = 0;
	for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
		{
			if( !NMG_INDEX_TEST( flags , fu ) )
			{
				missed_faces++;
				missed_fu = fu;
			}
		}
	}

	if( !missed_faces )	/* nothing to do, just one shell */
	{
		bu_free( (char *)flags , "nmg_decompose_shell: flags " );
		bu_ptbl_free( &stack );
		bu_ptbl_free( &shared_edges );
		return( no_of_shells );
	}

	while( missed_faces )
	{
		struct edgeuse *unassigned_eu = NULL;
		int *shells_at_edge;
		int new_shell_no=0;

		bu_ptbl_reset( &stack );

		/* Look at the list of shared edges to see if anything can be deduced */
		shells_at_edge = (int *)bu_calloc( no_of_shells+1 , sizeof( int ) , "nmg_decompose_shell: shells_at_edge" );

		for( i=0 ; i<BU_PTBL_END( &shared_edges ) ; i++ )
		{
			int faces_at_edge=0;
			int k;

			/* Construct a list of shells for this edge.
			 * shells_at_edge[i] is the number of edgeuses of this
			 * edge that have been assigned to shell number i.
			 * shells_at_edge[0] is the number of uses of this edge
			 * that have not been asigned to any shell yet
			 */
			for( k=0 ; k<=no_of_shells ; k++ )
				shells_at_edge[k] = 0;

			unassigned_eu = NULL;

			eu = (struct edgeuse *)BU_PTBL_GET( &shared_edges , i );
			NMG_CK_EDGEUSE( eu );

			eu1 = eu;
			do
			{
				struct faceuse *fu_of_eu;

				fu_of_eu = nmg_find_fu_of_eu( eu1 );

				faces_at_edge++;
				if( !NMG_INDEX_TEST( flags , fu_of_eu ) )
					unassigned_eu = eu1;
				shells_at_edge[ NMG_INDEX_GET( flags , fu_of_eu ) ]++;

				eu1 = nmg_next_radial_eu( eu1 , s , 0 );
			}
			while( eu1 != eu );

			if( shells_at_edge[0] == 1 && unassigned_eu )
			{
				/* Only one edgeuse at this edge is unassigned, should be
				 * able to determine which shell it belongs in */

				for( j=1 ; j<=no_of_shells ; j++ )
				{
					if( shells_at_edge[j] == 1 )
					{
						/* unassigned edgeuse should belong to shell j */
						new_shell_no = j;
						break;
					}
				}
			}
			else if( shells_at_edge[0] == 0 )
			{
				/* all uses of this edge have been assigned
				 * it can be deleted from the table
				 */
				bu_ptbl_rm( &shared_edges , (long *)eu );
			}
			if( new_shell_no )
				break;
		}

		if( !new_shell_no )
		{
			/* Couldn't find a definite edgeuse to start a new shell
			 * so use radial parity to pick an edgeuse that should not be
			 * part of the same shell as ones already assigned
			 */
			for( i=0 ; i<BU_PTBL_END( &shared_edges ) ; i++ )
			{
				struct faceuse *fu_of_eu1;
				int found_missed_face=0;

				eu = (struct edgeuse *)BU_PTBL_GET( &shared_edges , i );
				NMG_CK_EDGEUSE( eu );

				eu1 = eu;
				do
				{
					/* look for unassigned edgeuses */
					fu_of_eu1 = nmg_find_fu_of_eu( eu1 );
					NMG_CK_FACEUSE( fu_of_eu1 );
					if( !NMG_INDEX_TEST( flags , fu_of_eu1 ) )
					{
						struct edgeuse *eu2;
						struct faceuse *fu_of_eu2;

						/* look for a neighboring edgeuse that
						 * has been assigned
						 */
						eu2 = nmg_prev_radial_eu( eu1 , s , 0 );
						fu_of_eu2 = nmg_find_fu_of_eu( eu2 );
						NMG_CK_FACEUSE( fu_of_eu2 );
						if( NMG_INDEX_TEST( flags , fu_of_eu2 ) )
						{
							/* eu2 has been assigned
							 * compare orientation parity
							 */
							if( fu_of_eu2->orientation ==
								fu_of_eu1->orientation )
							{
								/* These should not be in the same
								 * shell, so start a new shell
								 * at this faceuse
								 */
								missed_fu = fu_of_eu1;
								found_missed_face = 1;
							}
						}
						if( found_missed_face )
							break;

						eu2 = nmg_next_radial_eu( eu1 , s , 0 );
						fu_of_eu2 = nmg_find_fu_of_eu( eu2 );
						NMG_CK_FACEUSE( fu_of_eu2 );
						if( NMG_INDEX_TEST( flags , fu_of_eu2 ) )
						{
							/* eu2 has been assigned
							 * compare orientation parity
							 */
							if( fu_of_eu2->orientation ==
								fu_of_eu1->orientation )
							{
								/* These should not be in the same
								 * shell, so start a new shell
								 * at this faceuse
								 */
								missed_fu = fu_of_eu1;
								found_missed_face = 1;
							}
						}

					}
					if( found_missed_face )
						break;
					eu1 = nmg_next_radial_eu( eu1 , s , 0 );
				}
				while( eu1 != eu );

				if( found_missed_face )
					break;
			}
		}

		bu_free( (char *)shells_at_edge , "nmg_decompose_shell: shells_at_edge" );

		/* make a new shell number */
		if( new_shell_no )
		{
			shell_no = new_shell_no;
			fu = nmg_find_fu_of_eu( unassigned_eu );
		}
		else
		{
			shell_no = (++no_of_shells);
			NMG_CK_FACEUSE( missed_fu );
			fu = missed_fu;
		}

		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;

		if( !NMG_INDEX_TEST( flags , fu ) )
		{
			/* move this missed face to the new shell */

			/* push all edgeuses of "fu" onto the stack */
			for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );

				if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
					continue;

				for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					/* build two lists, one of winged edges, the other not */
					if( nmg_radial_face_count( eu , s ) > 2 )
						bu_ptbl_ins_unique( &shared_edges , (long *)eu );
					else
						bu_ptbl_ins_unique( &stack , (long *)eu );
				}
			}

			/* Mark this faceuse with a shell number */
			NMG_INDEX_ASSIGN( flags , fu , shell_no );
			NMG_INDEX_ASSIGN( flags , fu->fumate_p , shell_no );

		}
		else
			rt_bomb( "nmg_decompose_shell: Missed face wasn't missed???\n" );

		/* now pop edgeuse of the stack and visit faces radial to edgeuse */
		while( (eu1 = nmg_pop_eu( &stack )) != (struct edgeuse *)NULL )
		{
			NMG_CK_EDGEUSE( eu1 );

			/* move to the radial */
			eu = eu1->radial_p;
			NMG_CK_EDGEUSE( eu );

			/* stay within the original shell */
			while( nmg_find_s_of_eu( eu ) != s && eu != eu1 && eu != eu1->eumate_p )
				eu = eu->eumate_p->radial_p;

			if( eu == eu1 || eu == eu1->eumate_p )
				continue;

			fu = nmg_find_fu_of_eu( eu );
			NMG_CK_FACEUSE( fu );

			/* if this face has already been visited, skip it */
			if( !NMG_INDEX_TEST( flags , fu ) )
			{
				/* push all edgeuses of "fu" onto the stack */
				for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
				{
					NMG_CK_LOOPUSE( lu );
					if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
						continue;
					for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
					{
						/* build two lists, one of winged edges, the other not */
						if( nmg_radial_face_count( eu , s ) > 2 )
							bu_ptbl_ins_unique( &shared_edges , (long *)eu );
						else
							bu_ptbl_ins_unique( &stack , (long *)eu );
					}
				}

				/* Mark this faceuse with a shell number */
				NMG_INDEX_ASSIGN( flags , fu , shell_no );
				NMG_INDEX_ASSIGN( flags , fu->fumate_p , shell_no );

			}
		}

		/* count number of faces that were not visited */
		missed_faces = 0;
		for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
		{
			NMG_CK_FACEUSE( fu );
			if( fu->orientation == OT_SAME )
			{
				if( !NMG_INDEX_TEST( flags , fu ) )
				{
					missed_faces++;
					missed_fu = fu;
				}
			}
		}
	}

	/* Now build the new shells */
	for( shell_no=2 ; shell_no<=no_of_shells ; shell_no++ )
	{
		struct bu_ptbl faces;

		bu_ptbl_init( &faces, 64, "Faces ptbl for nmg_decompose_shell" );

		/* Make a shell */
		new_s = nmg_msv( r );
		(void)nmg_kvu( new_s->vu_p );

		/* Move faces marked with this shell_no to this shell */
		fu = BU_LIST_FIRST( faceuse , &s->fu_hd );
		while( BU_LIST_NOT_HEAD( fu , &s->fu_hd ) )
		{
			struct faceuse *next_fu;

			next_fu = BU_LIST_NEXT( faceuse , &fu->l );
			while( BU_LIST_NOT_HEAD( next_fu , &s->fu_hd ) && next_fu->orientation != OT_SAME )
				next_fu = BU_LIST_NEXT( faceuse , &next_fu->l );

			if( fu->orientation != OT_SAME )
			{
				fu = next_fu;
				continue;
			}

			if( NMG_INDEX_GET( flags , fu ) == shell_no )
			{
				nmg_mv_fu_between_shells( new_s , s , fu );
				bu_ptbl_ins( &faces, (long *)fu );
			}

			fu = next_fu;
		}

		nmg_gluefaces( (struct faceuse **)BU_PTBL_BASEADDR( &faces ), BU_PTBL_END( &faces ), tol );
		bu_ptbl_free( &faces );
		nmg_shell_a( new_s, tol );
	}
	bu_free( (char *)flags , "nmg_decompose_shell: flags " );
	bu_ptbl_free( &stack );
	bu_ptbl_free( &shared_edges );

	nmg_rebound( m ,tol );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_decompose_shell END (%d shells)\n" , no_of_shells );

	return( no_of_shells );
}

/**
 *			N M G _ S T A S H _ M O D E L _ T O _ F I L E
 *
 *  Store an NMG model as a separate .g file, for later examination.
 *  Don't free the model, as the caller may still have uses for it.
 *
 *  NON-PARALLEL because of rt_uniresource.
 */
void
nmg_stash_model_to_file(const char *filename, const struct model *m, const char *title)
{
	struct rt_wdb		*fp;
	struct rt_db_internal	intern;
	struct bu_external	ext;
	int			ret;
	int			flags;
	char			*name="error.s";

	bu_log("nmg_stash_model_to_file('%s', x%x, %s)\n", filename, m, title);

	NMG_CK_MODEL(m);
	nmg_vmodel(m);

	if( (fp = wdb_fopen(filename)) == NULL )  {
		perror(filename);
		return;
	}

	RT_INIT_DB_INTERNAL(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_NMG;
	intern.idb_meth = &rt_functab[ID_NMG];
	intern.idb_ptr = (genptr_t)m;

	if( fp->dbip->dbi_version <= 4 )  {
		BU_INIT_EXTERNAL( &ext );
		ret = intern.idb_meth->ft_export( &ext, &intern, 1.0, fp->dbip, &rt_uniresource );
		if( ret < 0 )  {
			bu_log("rt_db_put_internal(%s):  solid export failure\n",
				name);
			ret = -1;
			goto out;
		}
		db_wrap_v4_external( &ext, name );
	} else {
		if( rt_db_cvt_to_external5( &ext, name, &intern, 1.0, fp->dbip, &rt_uniresource, intern.idb_major_type ) < 0 )  {
			bu_log("wdb_export(%s): solid export failure\n",
				name );
			ret = -2;
			goto out;
		}
	}
	BU_CK_EXTERNAL( &ext );

	flags = db_flags_internal( &intern );
	ret = wdb_export_external( fp, &ext, name, flags, intern.idb_type );
out:
	bu_free_external( &ext );
	wdb_close( fp );

	bu_log("nmg_stash_model_to_file(): wrote error.s to '%s'\n",
		filename);
}

/* state for nmg_unbreak_edge */
struct nmg_unbreak_state
{
	long		*flags;		/* index based array of flags for model */
	int		unbroken;	/* count of edges mended */
};

/**
 *			N M G _ U N B R E A K _ H A N D L E R
 *
 *	edgeuse visit routine for nmg_unbreak_region_edges.
 *
 *	checks if edgeuse "eu" and its successor are candidates	to be unbroken.
 *	looks for two consectutive edgeuses sharing the same
 *	edge geometry. Checks that the middle vertex has no
 *	other uses, and,  if so, kills the second edgeuse.
 *	Also moves the vu of the first edgeuse mate to the vu
 *	of the killed edgeuse mate.
 */
void
nmg_unbreak_handler(long int *eup, genptr_t state, int after)
{
	struct edgeuse *eu1,*eu2;
	struct edge *e;
	struct edge_g_lseg *eg;
	struct nmg_unbreak_state *ub_state;
	struct vertex	*vb;
	int		ret;

	eu1 = (struct edgeuse *)eup;
	NMG_CK_EDGEUSE( eu1 );

	ub_state = (struct nmg_unbreak_state *)state;

	/* there is a temptation to do a NMG_INDEX_SET( ub_state->flags , eu1->eumate_p )
	 * here to avoid looking at this edgeuse's mate, but since we are only looking
	 * forward, we must look at ALL edgeuses
	 */

	/* make sure we only visit this edgeuse once */
	if( !NMG_INDEX_TEST_AND_SET( ub_state->flags , eu1 ) )  return;

	e = eu1->e_p;
	NMG_CK_EDGE( e );

	eg = eu1->g.lseg_p;
	if( !eg )  {
		bu_log( "nmg_unbreak_handler: no geomtry for edge x%x\n" , e );
		return;
	}
	NMG_CK_EDGE_G_EITHER(eg);

	/* if the edge geometry doesn't have at least two uses, this
	 * is not a candidate for unbreaking */
	if( bu_list_len( &eg->eu_hd2 ) < 2*2 )  {
		/* bu_log("nmg_unbreak_handler: usage < 4\n"); */
		return;
	}

	/* Check for two consecutive uses, by looking forward. */
	eu2 = BU_LIST_PNEXT_CIRC( edgeuse , eu1 );
	NMG_CK_EDGEUSE( eu2 );
	if( eu2->g.lseg_p != eg )
	{
		/* Can't look backward here, or nmg_unbreak_edge()
		 * will be asked to kill *this* edgeuse, which
		 * will blow our caller's mind.
		 */
		/* bu_log("nmg_unbreak_handler: eu1 edge geom not shared with eu2\n"); */
		return;
	}
	vb = eu2->vu_p->v_p;
	NMG_CK_VERTEX(vb);

	/* at this point, the situation is:

		     eu1          eu2
		*----------->*----------->*
		A------------B------------C
		*<-----------*<-----------*
		    eu1mate      eu2mate
	*/
	ret = nmg_unbreak_edge( eu1 );
	if( ret != 0 )  return;

	/* keep a count of unbroken edges */
	ub_state->unbroken++;
}

/**
 *			N M G _ U N B R E A K _ R E G I O N _ E D G E S
 *
 *	Uses the visit handler to call nmg_unbreak_handler for
 *	each edgeuse below the region (or any other NMG element).
 *
 *	returns the number of edges mended
 */
int
nmg_unbreak_region_edges(long int *magic_p)
{
	struct model *m;
	struct nmg_unbreak_state ub_state;
	int count;
	static const struct nmg_visit_handlers htab = {NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, nmg_unbreak_handler, NULL, NULL,
						       NULL, NULL, NULL, NULL, NULL};
	/* htab.aft_edgeuse = nmg_unbreak_handler; */

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_unbreak_region_edges( magic_p = x%x )\n" , magic_p );

	m = nmg_find_model( magic_p );
	NMG_CK_MODEL( m );

	ub_state.unbroken = 0;
	ub_state.flags = (long *)bu_calloc( m->maxindex*2 , sizeof( long ) , "nmg_unbreak_region_edges: flags" );

	nmg_visit( magic_p , &htab , (genptr_t)&ub_state );

	count = ub_state.unbroken;

	bu_free( (char *)ub_state.flags , "nmg_unbreak_region_edges: flags" );

	return( count );
}

/**
 *			R T _ D I S T _ P T 3 _ L I N E 3
 *
 *  Find the distance from a point P to a line described
 *  by the endpoint A and direction dir, and the point of closest approach (PCA).
 *
 *			P
 *		       *
 *		      /.
 *		     / .
 *		    /  .
 *		   /   . (dist)
 *		  /    .
 *		 /     .
 *		*------*-------->
 *		A      PCA	dir
 *
 *  There are three distinct cases, with these return codes -
 *	0	P is within tolerance of point A.  *dist = 0, pca=A.
 *	1	P is within tolerance of line.  *dist = 0, pca=computed.
 *	2	P is "above/below" line.  *dist=|PCA-P|, pca=computed.
 *
 *
 * XXX For efficiency, a version of this routine that provides the
 * XXX distance squared would be faster.
XXX This should be moved into libbn/plane.c,
XXX probably named bn_dist_pt3_line3().
 */
int
rt_dist_pt3_line3(fastf_t *dist, fastf_t *pca, const fastf_t *a, const fastf_t *dir, const fastf_t *p, const struct bn_tol *tol)
{
	vect_t	AtoP;		/* P-A */
	vect_t	unit_dir;	/* unitized dir vector */
	fastf_t	A_P_sq;		/* |P-A|**2 */
	fastf_t	t;		/* distance along ray of projection of P */
	fastf_t	dsq;		/* sqaure of distance from p to line */

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "rt_dist_pt3_line3( a=( %f %f %f ), dir=( %f %f %f ), p=( %f %f %f )\n" ,
			V3ARGS( a ) , V3ARGS( dir ) , V3ARGS( p ) );

	BN_CK_TOL(tol);

	/* Check proximity to endpoint A */
	VSUB2(AtoP, p, a);
	if( (A_P_sq = MAGSQ(AtoP)) < tol->dist_sq )  {
		/* P is within the tol->dist radius circle around A */
		VMOVE( pca, a );
		*dist = 0.0;
		return( 0 );
	}

	VMOVE( unit_dir , dir );
	VUNITIZE( unit_dir );

	/* compute distance (in actual units) along line to PROJECTION of
	 * point p onto the line: point pca
	 */
	t = VDOT(AtoP, unit_dir);

	VJOIN1( pca , a , t , unit_dir );
	if( (dsq = A_P_sq - t*t) < tol->dist_sq )
	{
		/* P is within tolerance of the line */
		*dist = 0.0;
		return( 1 );
	}
	else
	{
		/* P is off line */
		*dist = sqrt( dsq );
		return( 2 );
	}
}

/**
 *	N M G _ M V _ S H E L L _ T O _ R E G I O N
 *
 *  Move a shell from one nmgregion to another.
 *  Will bomb if shell and region aren't in the same model.
 *
 *  returns:
 *	0 - all is well
 *	1 - nmgregion that gave up the shell is now empty!!!!
 *
 */
int
nmg_mv_shell_to_region(struct shell *s, struct nmgregion *r)
{
	int ret_val;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_mv_shell_to_region( s=x%x , r=x%x )\n" , s , r );

	NMG_CK_SHELL( s );
	NMG_CK_REGION( r );

	if( s->r_p == r )
	{
		bu_log( "nmg_mv_shell_to_region: Attempt to move shell to region it is already in\n" );
		return( 0 );
	}

	if( nmg_find_model( &s->l.magic ) != nmg_find_model( &r->l.magic ) )
		rt_bomb( "nmg_mv_shell_to_region: Cannot move shell to a different model\n" );

	BU_LIST_DEQUEUE( &s->l );
	if( BU_LIST_IS_EMPTY( &s->r_p->s_hd ) )
		ret_val = 1;
	else
		ret_val = 0;

	BU_LIST_APPEND( &r->s_hd , &s->l );

	s->r_p = r;

	return( ret_val );
}

/**	N M G _ F I N D _ I S E C T _ F A C E S
 *
 *	Find all faces that contain vertex "new_v"
 *	Put them in a bu_ptbl "faces"
 *
 *	returns:
 *		the number of faces that contain the vertex
 *
 *	and fills in the table with the faces.
 *	Counts edges at this vertex where radial is mate (free_edges)
 */
int
nmg_find_isect_faces(const struct vertex *new_v, struct bu_ptbl *faces, int *free_edges, const struct bn_tol *tol)
{
	struct faceuse *fu;
	struct face_g_plane *fg;
	struct vertexuse *vu;
	int i;
	int unique;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_find_isect_faces( new_v=x%x , faces=x%x )\n" , new_v , faces );

	NMG_CK_VERTEX( new_v );
	BN_CK_TOL( tol );
	BU_CK_PTBL( faces );

	/* loop through vertex's vu list */
	for( BU_LIST_FOR( vu , vertexuse , &new_v->vu_hd ) )
	{
		NMG_CK_VERTEXUSE( vu );
		fu = nmg_find_fu_of_vu( vu );
		if( fu->orientation != OT_SAME )
			continue;;

		NMG_CK_FACEUSE( fu );
		fg = fu->f_p->g.plane_p;

		/* check if this face is different from the ones on list */
		unique = 1;
		for( i=0 ; i<BU_PTBL_END( faces ) ; i++ )
		{
			struct face *fp;

			fp = (struct face *)BU_PTBL_GET( faces , i );
			if( fp->g.plane_p == fg || bn_coplanar( fg->N , fp->g.plane_p->N , tol ) > 0 )
			{
				unique = 0;
				break;
			}
		}

		/* if it is not already on the list, add it */
		if( unique )
		{
			struct edgeuse *eu1;

			bu_ptbl_ins( faces , (long *)fu->f_p );
			/* Count the number of free edges containing new_v */

			if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
				continue;

			eu1 = vu->up.eu_p;
			if( eu1->eumate_p == eu1->radial_p )
				(*free_edges)++;
			else
			{
				eu1 = BU_LIST_PPREV_CIRC( edgeuse , eu1 );
				if( eu1->eumate_p == eu1->radial_p )
					(*free_edges)++;
			}
		}
	}
	return( BU_PTBL_END( faces ) );
}

/**	N M G _ S I M P L E _ V E R T E X _ S O L V E
 *
 *	given a vertex and a list of faces (not more than three)
 *	that should intersect at the vertex, calculate a new
 *	location for the vertex.
 *
 *	returns:
 *		0 - if everything is OK
 *		1 - failure
 *
 *	and modifies the geometry of the vertex to the new location
 */
int
nmg_simple_vertex_solve(struct vertex *new_v, const struct bu_ptbl *faces, const struct bn_tol *tol)
{
	struct vertex_g *vg;
	int failed=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		struct face *f;
		struct faceuse *fu;
		plane_t pl;
		int i;

		bu_log( "nmg_simple_vertex_solve( new_v=x%x , %d faces )\n" , new_v , BU_PTBL_END( faces ));

		for( i=0 ; i<BU_PTBL_END( faces ) ; i++ )
		{
			f = (struct face *)BU_PTBL_GET( faces, i );
			fu = f->fu_p;
			if( fu->orientation != OT_SAME )
				fu = fu->fumate_p;
			if( fu->orientation != OT_SAME )
				bu_log( "\tface (x%x) has no OT_SAME use\n", f );
			NMG_GET_FU_PLANE( pl, fu );
			bu_log( "\t#%d: %g %g %g %g\n", i, V4ARGS( pl ) );
		}
	}

	NMG_CK_VERTEX( new_v );
	BU_CK_PTBL( faces );
	BN_CK_TOL( tol );

	vg = new_v->vg_p;
	NMG_CK_VERTEX_G( vg );

	switch( BU_PTBL_END( faces ) )
	{
		struct face *fp1,*fp2,*fp3;
		plane_t pl1;
		fastf_t	vert_move_len;
		fastf_t pl_dot;

		case 0:
			bu_log( "nmg_simple_vertex_solve: vertex not in any face planes!!!\n" );
			failed = 1;
			break;

		case 1:		/* just move the vertex to the plane */
			fp1 = (struct face *)BU_PTBL_GET( faces , 0 );
			vert_move_len = DIST_PT_PLANE( vg->coord , fp1->g.plane_p->N );
			VJOIN1( vg->coord , vg->coord , -vert_move_len , fp1->g.plane_p->N );
			break;

		case 2:
			fp1 = (struct face *)BU_PTBL_GET( faces , 0 );
			fp2 = (struct face *)BU_PTBL_GET( faces , 1 );

			pl_dot = VDOT( fp1->g.plane_p->N, fp2->g.plane_p->N );
			if( NEAR_ZERO( pl_dot - 1.0 , tol->perp ) || NEAR_ZERO( pl_dot + 1.0, tol->perp) )
			{
				vect_t move_vect;

				/* treat as a single plane */
				vert_move_len = (DIST_PT_PLANE( vg->coord , fp1->g.plane_p->N )
						+ DIST_PT_PLANE( vg->coord , fp2->g.plane_p->N ))/2.0;
				VADD2( move_vect, fp1->g.plane_p->N, fp2->g.plane_p->N );
				VUNITIZE( move_vect );
				VJOIN1( vg->coord, vg->coord, -vert_move_len, move_vect );
			}
			else
			{
				/* create a third plane perpendicular to first two */

				VCROSS( pl1 , fp1->g.plane_p->N , fp2->g.plane_p->N );

				VUNITIZE( pl1 );
				pl1[3] = VDOT( vg->coord , pl1 );
				if( bn_mkpoint_3planes( vg->coord , fp1->g.plane_p->N , fp2->g.plane_p->N , pl1 ) )
				{
					bu_log( "nmg_simple_vertex_solve: Cannot find new coords for two planes\n" );
					bu_log( "\tplanes are ( %f %f %f %f ) and ( %f %f %f %f )\n",
						V4ARGS( fp1->g.plane_p->N ) ,
						V4ARGS( fp2->g.plane_p->N ) );
					bu_log( "\tcalculated third plane is ( %f %f %f %f )\n" , V4ARGS( pl1 ) );
					failed = 1;
					break;
				}
			}
			break;

		case 3:		/* just intersect the three planes */
			fp1 = (struct face *)BU_PTBL_GET( faces , 0 );
			fp2 = (struct face *)BU_PTBL_GET( faces , 1 );
			fp3 = (struct face *)BU_PTBL_GET( faces , 2 );
			if( bn_mkpoint_3planes( vg->coord , fp1->g.plane_p->N , fp2->g.plane_p->N , fp3->g.plane_p->N ) )
			{
				bu_log( "nmg_simple_vertex_solve: failed for 3 planes:\n" );
				bu_log( "\t( %f %f %f %f )\n" , V4ARGS( fp1->g.plane_p->N ) );
				bu_log( "\t( %f %f %f %f )\n" , V4ARGS( fp2->g.plane_p->N ) );
				bu_log( "\t( %f %f %f %f )\n" , V4ARGS( fp3->g.plane_p->N ) );
				failed = 1;
				break;
			}
			break;
		default:
			failed = 1;
			bu_log( "nmg_simple_vertex_solve: Called for a complex vertex\n" );
			break;
	}

	if( failed )
		bu_log( "nmg_simple_vertex_solve: Failed to determine new coordinates for vertex at ( %f %f %f )\n",
			V3ARGS( new_v->vg_p->coord ) );
	else if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_simple_vertex_solve: new coords = ( %f %f %f )\n",
			V3ARGS( new_v->vg_p->coord ) );

	return( failed );
}

/**	N M G _ C K _ V E R T _ O N _ F U S
 *
 *  Check all uses of a vertex to see if it lies within tolerance
 *  of all faces where it is used
 *
 * returns:
 *	0 - All is well
 *	1 - vertex is off face plane by at least tol->dist for at least one face
 */
int
nmg_ck_vert_on_fus(const struct vertex *v, const struct bn_tol *tol)
{
	struct vertexuse *vu;
	fastf_t max_dist=0.0;
	int ret_val=0;

	NMG_CK_VERTEX( v );
	BN_CK_TOL( tol );

	NMG_CK_VERTEX_G( v->vg_p );

	for( BU_LIST_FOR( vu , vertexuse , &v->vu_hd ) )
	{
		struct faceuse *fu;
		fastf_t dist;

		fu = nmg_find_fu_of_vu( vu );
		if( !fu )
			continue;

		NMG_CK_FACEUSE( fu );
		NMG_CK_FACE( fu->f_p );
		NMG_CK_FACE_G_PLANE( fu->f_p->g.plane_p );
		dist = DIST_PT_PLANE( v->vg_p->coord , fu->f_p->g.plane_p->N );
		dist = (dist < 0.0 ? (-dist) : dist);
		if( dist > tol->dist )
		{
			ret_val = 1;

			if( dist > max_dist )
				max_dist = dist;

			bu_log( "nmg_ck_vert_on_fus: v=x%x vu=x%x ( %f %f %f ) is %g from\n\tfaceuse x%x f x%x\n" , v , vu , V3ARGS( v->vg_p->coord ) , dist , fu , fu->f_p );
		}
	}

	if( ret_val )
		bu_log( "nmg_ck_vert_on_fus: v=x%x ( %f %f %f ) max distance of %g from faceuses\n" , v , V3ARGS( v->vg_p->coord ) , max_dist );

	return( ret_val );
}

/* struct used by nmg_complex_vertex_solve
 * to store info about one edge
 * that contains the vertex in question
 */
struct intersect_fus
{
	struct faceuse *fu[2];	/* fu's that intersect at this edge */
	struct edgeuse *eu;	/* edgeuse in fu[0] that emanates from vertex */
	point_t start;		/* calculated start point of edge line */
	vect_t dir;		/* calculated direction of edge line */
	point_t pt;		/* a point on the edge a small distance from the vertex */
	int got_pt;		/* flag indicating that the above point has been obtained */
	int free_edge;		/* flag indicating that this is a free edge */
	struct vertex *vp;	/* a vertex pointer for above point */
};

/**	N M G _ P R _ I N T E R
 *
 * debug printing of the table of intersect_fus structs used by extruder
 */

static void
nmg_pr_inter(const struct vertex *new_v, const struct bu_ptbl *int_faces)
{
	int i;
	struct bn_tol tol;

	NMG_CK_VERTEX( new_v );
	BU_CK_PTBL( int_faces );

	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	bu_log( "\nint_faces at vertex x%x ( %f %f %f )\n" , new_v , V3ARGS( new_v->vg_p->coord ) );
	for( i=0 ; i<BU_PTBL_END( int_faces ) ; i++ )
	{
		struct intersect_fus *i_fus;
		struct face *fp1,*fp2;
		plane_t pl;

		i_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , i );

		bu_log( "edge number %d, x%x\n" , i , i_fus );
		if( i_fus->fu[0] )
			fp1 = i_fus->fu[0]->f_p;
		else
			fp1 = NULL;
		if( i_fus->fu[1] )
		{
			fp2 = i_fus->fu[1]->f_p;
			NMG_GET_FU_PLANE( pl , i_fus->fu[1] );
		}
		else
			fp2 = NULL;

		if( i_fus->fu[1] )
			bu_log( "\tfu1 = x%x (face=x%x), fu2 = x%x (face=x%x) ( %f %f %f %f )\n" , i_fus->fu[0] , fp1 , i_fus->fu[1] , fp2 , V4ARGS( pl ) );
		else
			bu_log( "\tfu1 = x%x (face=x%x), fu2 = x%x (face=x%x)\n" , i_fus->fu[0] , fp1 , i_fus->fu[1] , fp2 );

		if( i_fus->eu == NULL )
			bu_log( "\teu = NULL\n" );
		else if( i_fus->eu->vu_p == NULL )
			bu_log( "\teu = x%x , vu_p = NULL\n" , i_fus->eu );
		else
		{
			struct faceuse *fu;

			bu_log( "\teu = x%x, from x%x ( %f %f %f ) to x%x ( %f %f %f )\n" , i_fus->eu,
				i_fus->eu->vu_p->v_p , V3ARGS( i_fus->eu->vu_p->v_p->vg_p->coord ),
				i_fus->eu->eumate_p->vu_p->v_p , V3ARGS( i_fus->eu->eumate_p->vu_p->v_p->vg_p->coord ) );
			if( i_fus->fu[0] )
			{
				fu = nmg_find_fu_of_eu( i_fus->eu );
				if( fu != i_fus->fu[0] )
					bu_log( "****ERROR**** eu is not in fu1 it's in x%x\n" , fu );
			}
			else
			{
				fu = nmg_find_fu_of_eu( i_fus->eu );
				if( fu != i_fus->fu[1] )
					bu_log( "****ERROR**** eu is not in fu2, it's in x%x\n" , fu );
			}
#if 0
			/* XXXX sometimes this gives a zero length edge error in spite of the check!! */
			if( !bn_pt3_pt3_equal( i_fus->eu->vu_p->v_p->vg_p->coord, i_fus->eu->eumate_p->vu_p->v_p, &tol ) )
				nmg_pr_fu_around_eu( i_fus->eu , &tol );
#endif
		}

		bu_log( "\tstart = ( %f %f %f ) , dir = ( %f %f %f )\n" , V3ARGS( i_fus->start ) , V3ARGS( i_fus->dir ) );
		bu_log( "\tpt = ( %f %f %f )\n" , V3ARGS( i_fus->pt ) );
		bu_log( "\tfree_edge = %d\n" , i_fus->free_edge );
		if( i_fus->eu && i_fus->eu->vu_p )
		{
			if( i_fus->eu->eumate_p != i_fus->eu->radial_p &&
			    i_fus->free_edge )
				bu_log( "****ERROR**** this is NOT a free edge\n" );
			if( i_fus->eu->eumate_p == i_fus->eu->radial_p &&
			    !i_fus->free_edge )
				bu_log( "****ERROR**** This is a free edge\n" );
		}
		if( i_fus->vp )
			bu_log( "\tvp = x%x ( %f %f %f )\n" , i_fus->vp , V3ARGS( i_fus->vp->vg_p->coord ) );
		else
			bu_log( "\tvp = NULL\n" );
	}
}

/**	N M G _ G E T _ E D G E _ L I N E S
 *
 * Fill in the intersect_fus structures for edges around
 * new_v. Does not fill in "pt" or "vp".
 *
 *	returns:
 *		0 - All is well
 *		1 - Failure
 */

static int
nmg_get_edge_lines(struct vertex *new_v, struct bu_ptbl *int_faces, const struct bn_tol *tol)
{
	struct vertex_g *vg;
	struct vertexuse *vu;
	struct edgeuse *eu,*eu1;
	struct faceuse *fu;
	struct model *m;
	struct nmgregion *r;
	struct bn_tol tol_tmp;
	int done=0;
	int edge_no;

	NMG_CK_VERTEX( new_v );
	vg = new_v->vg_p;
	NMG_CK_VERTEX_G( vg );
	BN_CK_TOL( tol );
	BU_CK_PTBL( int_faces );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_get_edge_lines( new_v=x%x , int_faces=x%x )\n" , new_v , int_faces );

	/* A temporary tolerance struct for times when I don't want tolerancing */
	tol_tmp.magic = BN_TOL_MAGIC;
	tol_tmp.dist = 0.0;
	tol_tmp.dist_sq = 0.0;
	tol_tmp.perp = 0.0;
	tol_tmp.para = 1.0;

	m = nmg_find_model( &new_v->magic );
	NMG_CK_MODEL( m );
	r = BU_LIST_FIRST( nmgregion , &m->r_hd );
	NMG_CK_REGION( r );
	NMG_CK_REGION_A( r->ra_p );

	/* look for a dangling edge emanating from this vertex */
	eu1 = (struct edgeuse *)NULL;
	for( BU_LIST_FOR( vu , vertexuse , &new_v->vu_hd ) )
	{
		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
			continue;

		eu = vu->up.eu_p->eumate_p;
		fu = nmg_find_fu_of_eu( eu );
		if( !fu )
			continue;

		if( fu->orientation != OT_SAME )
			continue;

		if( eu->eumate_p == eu->radial_p )
		{
			/* found a dangling edge, start processing here */
			plane_t pl;
			struct intersect_fus *i_fus;

			/* create and initialize an intersect_fus struct for this edge */
			i_fus = (struct intersect_fus *)bu_malloc( sizeof( struct intersect_fus ) , "nmg_get_edge_lines: i_fus" );
			i_fus->fu[0] = NULL;
			i_fus->fu[1] = fu;
			i_fus->eu = eu;
			i_fus->vp = (struct vertex *)NULL;
			VSET( i_fus->pt , 0.0 , 0.0 , 0.0 );
			i_fus->got_pt = 0;
			i_fus->free_edge = 1;
			eu1 = BU_LIST_PNEXT_CIRC( edgeuse , &eu->l );

			VSUB2( i_fus->dir , eu->vu_p->v_p->vg_p->coord , eu->eumate_p->vu_p->v_p->vg_p->coord );
			VUNITIZE( i_fus->dir );
			NMG_GET_FU_PLANE( pl , fu );
			VJOIN1( i_fus->start , vg->coord , (-DIST_PT_PLANE( vg->coord , pl )) , pl );

			/* Save this info in the int_faces table */
			bu_ptbl_ins( int_faces , (long *)i_fus );

			break;
		}
	}

	if( !eu1 )
	{
		int found_start=0;

		/* get the an edgeuse emanating from new_v */
		for( BU_LIST_FOR( vu , vertexuse , &new_v->vu_hd ) )
		{
			NMG_CK_VERTEXUSE( vu );
			if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
				continue;

			eu1 = vu->up.eu_p;

			fu = nmg_find_fu_of_eu( eu1 );
			NMG_CK_FACEUSE( fu );

			if( fu->orientation == OT_SAME )
			{
				found_start = 1;
				break;
			}
		}
		if( !found_start )
		{
			bu_log( "Cannot find edgeuse in OT_SAME faceuse starting at ( %f %f %f )\n",
				V3ARGS( new_v->vg_p->coord ) );
			return( 1 );
		}
	}

	eu = eu1;

	/* loop through all the edges emanating from new_v */
	while( !done )
	{
		fastf_t dist;
		point_t start;
		vect_t dir;
		vect_t eu_dir;
		int ret_val;
		struct intersect_fus *i_fus;
		struct faceuse *fu1,*fu2;

		NMG_CK_EDGEUSE( eu );

		if( eu->vu_p->v_p != new_v )
		{
			/* This can happen if the faces of the shell are not properly
			 * oriented such as might happen when an object is incorrectly
			 * modelled in FASTGEN and run through the patch-g converter
			 */
			bu_log( "nmg_get_edge_lines: Bad solid!!!\n" );
			for( edge_no=0 ; edge_no<BU_PTBL_END( int_faces ) ; edge_no++ )
			{
				struct intersect_fus *i_fus;

				i_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , edge_no );

				bu_free( (char *)i_fus , "nmg_get_edge_lines: i_fus" );
			}
			return( 1 );
		}

		/* get the direction of the original edge (away from the vertex) */
		VSUB2( eu_dir , eu->eumate_p->vu_p->v_p->vg_p->coord , eu->vu_p->v_p->vg_p->coord );

		/* get the two faces that intersect along this edge */
		fu1 = nmg_find_fu_of_eu( eu );
		fu2 = nmg_find_fu_of_eu( eu->radial_p );

		/* initialize the intersect structure for this edge */
		i_fus = (struct intersect_fus *)bu_malloc( sizeof( struct intersect_fus ) , "nmg_inside_vert: intersection list" );
		i_fus->fu[0] = fu1;
		if( eu->radial_p == eu->eumate_p )
		{
			i_fus->fu[1] = (struct faceuse *)NULL;
			i_fus->free_edge = 1;
			done = 1;
		}
		else
		{
			i_fus->fu[1] = fu2;
			i_fus->free_edge = 0;
		}
		i_fus->eu = eu;
		i_fus->vp = (struct vertex *)NULL;
		VSET( i_fus->pt , 0.0 , 0.0 , 0.0 );
		i_fus->got_pt = 0;
		VSET( i_fus->start , 0.0 , 0.0 , 0.0 );
		VSET( i_fus->dir , 0.0 , 0.0 , 0.0 );

		/* if edge is between loops of same face , don't calculate an edge line */
		if( fu1->f_p != fu2->f_p )
		{
			/* find the new edge line at the intersection of these two faces
			 * the line is defined by start and dir */

			ret_val = bn_isect_2planes( start, dir,
						    fu1->f_p->g.plane_p->N,
						    fu2->f_p->g.plane_p->N,
						    new_v->vg_p->coord,
						    &tol_tmp );
			if( ret_val )
			{
				/* Cannot find line for this edge */
				bu_log( "nmg_inside_vert: Cannot find new edge between two planes\n" );
				bu_log( "return from bn_isect_2planes is %d\n" , ret_val );
				bu_log( "\tplanes are ( %f %f %f %f ) and ( %f %f %f %f )\n" ,
					V4ARGS( fu1->f_p->g.plane_p->N ),
					V4ARGS( fu2->f_p->g.plane_p->N ) );
				bu_log( "\tfus x%x and x%x, faces x%x and x%x\n" ,
					fu1, fu2, fu1->f_p, fu2->f_p );
				nmg_pr_fu_briefly( fu1 , "fu1: " );
				nmg_pr_fu_briefly( fu2 , "fu2: " );
				rt_bomb( "Can't find plane intersection\n" );
			}
			/* Make the start point at closest approach to old vertex */
			(void)rt_dist_pt3_line3( &dist , start , start , dir , new_v->vg_p->coord , tol );

			/* Make sure the calculated direction is away from the vertex */
			if( VDOT( eu_dir , dir ) < 0.0 )
				VREVERSE( dir , dir );
			VMOVE( i_fus->start , start );
			VMOVE( i_fus->dir , dir );
		}
		else if( i_fus->free_edge )
		{
			plane_t pl;

			/* for a dangling edge, use the same direction as the original edge
			 * just move the start point to the new plane
			 */

			NMG_GET_FU_PLANE( pl , fu1 );

			VMOVE( i_fus->dir , eu_dir );
			VUNITIZE( i_fus->dir );

			VJOIN1( i_fus->start , vg->coord , (-DIST_PT_PLANE( vg->coord , pl )) , pl );

		}

		/* Save this info in the int_faces table */
		bu_ptbl_ins( int_faces , (long *)i_fus );

		if( !done )
		{
			/* move on to the next edge emanating from new_v */
			eu = eu->radial_p;
			eu = BU_LIST_PNEXT_CIRC( edgeuse , eu );
			if( eu == eu1 )
				done = 1;
		}
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		bu_log( "After getting edge lines:\n" );
		nmg_pr_inter( new_v , int_faces );
	}

	return( 0 );
}

/**	N M G _ G E T _ M A X _ E D G E _ I N T E R S
 *
 * Fill in the "pt" portion of the "intersect_fus" structure
 * for edges around new_v by calculating the intersection with neighboring
 * edges and selecting the furthest one from new_v.
 */
static int
nmg_get_max_edge_inters(const struct vertex *new_v, struct bu_ptbl *int_faces, const struct bu_ptbl *faces, const struct bn_tol *tol)
{
	struct model *m;
	struct nmgregion *r;
	int edge_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_get_max_edge_inters( new_v = x%x , %d intersect_fus structs , %d faces )\n" , new_v , BU_PTBL_END( int_faces ) , BU_PTBL_END( faces ) );

	NMG_CK_VERTEX( new_v );
	BN_CK_TOL( tol );
	BU_CK_PTBL( int_faces );

	m = nmg_find_model( &new_v->magic );
	NMG_CK_MODEL( m );
	r = BU_LIST_FIRST( nmgregion , &m->r_hd );
	NMG_CK_REGION( r );
	NMG_CK_REGION_A( r->ra_p );

	/* loop through edges departing from new_v */
	for( edge_no=0 ; edge_no<BU_PTBL_END( int_faces ) ; edge_no++ )
	{
		struct intersect_fus *edge_fus,*other_fus;
		fastf_t max_dist,dist[2];
		int next_edge_no,prev_edge_no;
		int other_index;

		edge_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , edge_no );

		/* don't calculate intersect point for edge between two loops of same face */
		if( edge_fus->fu[0] && edge_fus->fu[1] &&
			 edge_fus->fu[0]->f_p == edge_fus->fu[1]->f_p )
				continue;

		/* Find intersections with neighboring edges and keep the one
		 * furthest up the edge
		 */
		max_dist = (-MAX_FASTF);

		/* start with next edge */
		next_edge_no = edge_no + 1;
		if( next_edge_no == BU_PTBL_END( int_faces ) )
			next_edge_no = 0;

		other_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , next_edge_no );

		/* skip over edges betwen loops of same face */
		while( other_fus->fu[0] == other_fus->fu[1] && other_fus != edge_fus )
		{
			next_edge_no++;
			if( next_edge_no == BU_PTBL_END( int_faces ) )
				next_edge_no = 0;

			other_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , next_edge_no );

		}

		/* if we found another edge, calculate its intersection with the edge */
		if( other_fus != edge_fus )
		{
			if( !rt_dist_line3_line3( dist , edge_fus->start , edge_fus->dir , other_fus->start , other_fus->dir , tol ) )
			{
				if( rt_g.NMG_debug & DEBUG_BASIC )
					bu_log( "Edge #%d intersects edge #%d at dist = %f\n" , edge_no , next_edge_no , dist[0] );
				if( NEAR_ZERO( dist[0] , tol->dist ) )
					dist[0] = 0.0;
				if( dist[0] > max_dist )
					max_dist = dist[0];
			}
		}

		/* now check the previous neighboring edge */
		prev_edge_no = edge_no - 1;
		if( prev_edge_no < 0 )
			prev_edge_no = BU_PTBL_END( int_faces ) - 1;

		other_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , prev_edge_no );

		while( other_fus->fu[0] == other_fus->fu[1] && other_fus != edge_fus )
		{
			prev_edge_no--;
			if( prev_edge_no < 0 )
				prev_edge_no = BU_PTBL_END( int_faces ) - 1;

			other_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , prev_edge_no );
		}

		if( other_fus != edge_fus )
		{
			if( rt_dist_line3_line3( dist , edge_fus->start , edge_fus->dir , other_fus->start , other_fus->dir , tol ) >= 0 )
			{
				if( rt_g.NMG_debug & DEBUG_BASIC )
					bu_log( "Edge #%d intersects edge #%d at dist = %f\n" , edge_no , prev_edge_no , dist[0] );
				if( NEAR_ZERO( dist[0] , tol->dist ) )
					dist[0] = 0.0;
				if( dist[0] > max_dist )
					max_dist = dist[0];
			}
		}

		if( max_dist < 0.0 )
		{
			/* Now check for intersections with other planes */
			for( other_index=0 ; other_index<BU_PTBL_END( int_faces ) ; other_index ++ )
			{
				struct face *f;

				if( other_index == edge_no )
					continue;

				other_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , other_index );

				if( !other_fus->fu[0] )
					continue;

				NMG_CK_FACEUSE( other_fus->fu[0] );
				f = other_fus->fu[0]->f_p;

				if( edge_fus->fu[0] && f == edge_fus->fu[0]->f_p )
					continue;

				if( edge_fus->fu[1] && f == edge_fus->fu[1]->f_p )
					continue;

				/* Do not intersect with a plane that this edge is parallel to */
				if( NEAR_ZERO( VDOT( f->g.plane_p->N , edge_fus->dir ) , tol->perp ) )
					continue;

				if( bn_isect_line3_plane( &dist[0] , edge_fus->start , edge_fus->dir , f->g.plane_p->N , tol ) > 1 )
					continue;

				if( rt_g.NMG_debug & DEBUG_BASIC )
					bu_log( "Edge #%d intersects fu[0] from edge #%d at dist = %f\n" , edge_no , other_index , dist[0] );

				if( NEAR_ZERO( dist[0] , tol->dist ) )
					dist[0] = 0.0;

				if( dist[0] > max_dist )
					max_dist = dist[0];
			}
		}

		/* if any intersections have been found, save the point in edge_fus->pt */
		if( max_dist > (-MAX_FASTF) )
		{
			VJOIN1( edge_fus->pt , edge_fus->start , max_dist , edge_fus->dir );
			edge_fus->got_pt = 1;
		}
	}

	/* if no intersection was found, just use the edge-line start point */
	for( edge_no=0 ; edge_no < BU_PTBL_END( int_faces ) ; edge_no++ )
	{
		struct intersect_fus *edge_fus;

		edge_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , edge_no );
		if( !edge_fus->got_pt )
			VMOVE( edge_fus->pt , edge_fus->start )
	}

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		bu_log( "After nmg_get_max_edge_inters:\n" );
		nmg_pr_inter( new_v , int_faces );
	}

	return( 0 );
}

/**	N M G _ F U S E _ I N T E R S
 *
 * eliminate "j_fus" from the table "int_faces" and
 * adjust the info in "i_fus".
 * This is done when the "vp" vertices of the two structures
 * have been joined.
 */
static void
nmg_fuse_inters(struct intersect_fus *i_fus, struct intersect_fus *j_fus, struct bu_ptbl *int_faces, const struct bn_tol *tol)
{
	struct edgeuse *radial_eu;
	struct edgeuse *prev_eu;

	BU_CK_PTBL( int_faces );
	BN_CK_TOL( tol );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_fuse_inters: i_fus=x%x, j_fus=x%x, %d edges\n" , i_fus, j_fus, BU_PTBL_END( int_faces ) );

	/* remember the radial edge of the structure to be deleted */
	radial_eu = j_fus->eu->radial_p;

	/* if the vertices have been joined prev_eu and j_fus->eu should be adjacent */
	prev_eu = BU_LIST_PPREV_CIRC( edgeuse , &j_fus->eu->l );

	if( EDGESADJ( prev_eu , j_fus->eu ) )
	{
		nmg_keu( prev_eu );
		nmg_keu( j_fus->eu );
	}
	else
		bu_log( "nmg_fuse_inter_verts: ERROR: can't find adjacent edges to kill\n" );

	/* the other face for this edge is now j_fus->fu[1] */
	i_fus->fu[1] = j_fus->fu[1];

	/* if there are other faces along the edges that have been brought together
	 * do a radial join
	 */
	if( i_fus->fu[0] && j_fus->fu[1] )
	{
		if( rt_g.NMG_debug & DEBUG_BASIC )
		{
			bu_log( "radial join of eu's x%x and x%x\n" , i_fus->eu , radial_eu );
			bu_log( "\tx%x to x%x and x%x to x%x\n" ,
				i_fus->eu->vu_p->v_p, i_fus->eu->eumate_p->vu_p->v_p,
				radial_eu->vu_p->v_p, radial_eu->eumate_p->vu_p->v_p );
		}
		nmg_radial_join_eu( i_fus->eu , radial_eu , tol );
	}

	/* If this is a dangling edge, need to adjust the eu pointer */
	if( !i_fus->fu[0] )
		i_fus->eu = radial_eu;
	NMG_CK_EDGEUSE( i_fus->eu );

	/* if the deleted structure was for a dangling edge,
	 * then this edge is now dangling
	 */
	if( j_fus->free_edge )
		i_fus->free_edge = 1;

	bu_ptbl_rm( int_faces , (long *)j_fus );
	bu_free( (char *)j_fus , "nmg_split_edges_at_pts: j_fus " );

}

/**	N M G _ S P L I T _ E D G E S _ A T _ P T S
 *
 * Using the info in the table of intersect_fus structs,
 * split the edgeuse (eu) in each struct at the point (pt)
 * store the new vertices in the structure (vp) and assign
 * the geometry.
 *
 */
static void
nmg_split_edges_at_pts(const struct vertex *new_v, struct bu_ptbl *int_faces, const struct bn_tol *tol)
{
	int edge_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_split_edges_at_pts( new_v = x%x , %d intersect_fus structs)\n" , new_v , BU_PTBL_END( int_faces ) );

	BN_CK_TOL( tol );
	BU_CK_PTBL( int_faces );
	NMG_CK_VERTEX( new_v );

	/* loop through all edges departing from new_v */
	for( edge_no=0 ; edge_no < BU_PTBL_END( int_faces ) ; edge_no++ )
	{
		struct intersect_fus *i_fus;
		struct edgeuse *new_eu;

		i_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , edge_no );

		/* skip edges between two loops of same face, for now */
		if( i_fus->fu[0] && i_fus->fu[1] && i_fus->fu[0]->f_p == i_fus->fu[1]->f_p )
			continue;

		if( bn_pt3_pt3_equal( new_v->vg_p->coord , i_fus->pt , tol ) )
		{
			/* if pt is within tolerance of new_v, don't split the edge */
			i_fus->vp = (struct vertex *)NULL;
			VMOVE( i_fus->pt , new_v->vg_p->coord );
			VMOVE( i_fus->start , new_v->vg_p->coord );
			VSUB2( i_fus->dir , i_fus->eu->eumate_p->vu_p->v_p->vg_p->coord , i_fus->eu->vu_p->v_p->vg_p->coord );
			VUNITIZE( i_fus->dir );
			continue;
		}
		new_eu = nmg_esplit( i_fus->vp , i_fus->eu, 0 );
		i_fus->vp = new_eu->vu_p->v_p;

		/* Need to keep track of correct eu in this case */
		if( i_fus->free_edge && !i_fus->fu[0] )
			i_fus->eu = new_eu;

		/* Assign geometry to the new vertex */
		if( i_fus && !i_fus->vp->vg_p )
			nmg_vertex_gv( i_fus->vp , i_fus->pt );
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		bu_log( "After splitting edges:\n" );
		nmg_pr_inter( new_v , int_faces );
	}

	/* Now take care of edges between two loops of same face */
	edge_no = 0;
	while( edge_no < BU_PTBL_END( int_faces ) )
	{
		int next_edge_no;
		struct intersect_fus *i_fus,*j_fus;

		next_edge_no = edge_no + 1;
		if( next_edge_no == BU_PTBL_END( int_faces ) )
			next_edge_no = 0;

		i_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , edge_no );
		j_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , next_edge_no );

		/* look at all edges in the same face as i_fus->fu[1] */
		while( j_fus->fu[0] && j_fus->fu[1] &&
		       j_fus->fu[0]->f_p == j_fus->fu[1]->f_p &&
		       j_fus != i_fus )
		{
			/* if both edges are dangling, there is nothing to do */
			if( i_fus->free_edge && j_fus->free_edge )
				break;

			/* if we haven't assigned a vertex, skip this edge */
			if( !i_fus->vp )
				break;

			/* split the neighbor at the first structure's "vp"
			 * this moves the neighboring edge's endpoint to
			 * fall on the first edge.
			 */
			(void) nmg_esplit( i_fus->vp , j_fus->eu, 0 );

			/* now we can ignore this edge */
			nmg_fuse_inters( i_fus , j_fus , int_faces , tol );

			/* go to the next edge */
			if( next_edge_no == BU_PTBL_END( int_faces ) )
				next_edge_no = 0;

			j_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , next_edge_no );

		}
		edge_no++;
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		bu_log( "After loops of same face\n" );
		nmg_pr_inter( new_v , int_faces );
	}
}

/**	N M G _ R E M O V E _ S H O R T _ E U S _ I N T E R
 *
 * kill all zero length edgeuses in faces around new_v
 *
 */
static void
nmg_remove_short_eus_inter(struct vertex *new_v, struct bu_ptbl *int_faces, const struct bn_tol *tol)
{
	int edge_no;
	struct vertexuse *vu;

	NMG_CK_VERTEX( new_v );
	BU_CK_PTBL( int_faces );
	BN_CK_TOL( tol );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_remove_short_eus: new_v=x%x ( %f %f %f ), %d edges\n" , new_v, V3ARGS( new_v->vg_p->coord ), BU_PTBL_END( int_faces ) );

	/* first join any of the "vp" in the intersect_fus structs that are
	 * within tolerance of new-v
	 */
	for( edge_no=0 ; edge_no<BU_PTBL_END( int_faces ) ; edge_no++ )
	{
		struct intersect_fus *edge_fus;

		edge_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , edge_no );

		if( !edge_fus->vp )
			continue;

		if( !bn_pt3_pt3_equal( new_v->vg_p->coord , edge_fus->vp->vg_p->coord , tol ) )
			continue;

		nmg_jv( new_v , edge_fus->vp );
		edge_fus->vp = new_v;
	}

	/* look at all faces around new_v */
	vu = BU_LIST_FIRST( vertexuse , &new_v->vu_hd );
	while( BU_LIST_NOT_HEAD( vu , &new_v->vu_hd ) )
	{
		struct vertexuse *vu_next;
		struct faceuse *fu;
		struct loopuse *lu;
		struct faceuse *bad_fu=(struct faceuse *)NULL;
		int bad_loop=0;

		NMG_CK_VERTEXUSE( vu );

		vu_next = BU_LIST_PNEXT( vertexuse , &vu->l );

		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
		{
			vu = vu_next;
			continue;
		}

		fu = nmg_find_fu_of_vu( vu );
		NMG_CK_FACEUSE( fu );

		/* look at all loops in these faces */
		lu = BU_LIST_FIRST( loopuse , &fu->lu_hd );
		while( BU_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			struct loopuse *lu_next;
			struct edgeuse *eu;

			NMG_CK_LOOPUSE( lu );

			lu_next = BU_LIST_PNEXT( loopuse , &lu->l );

			eu = BU_LIST_FIRST( edgeuse , &lu->down_hd );
			while( BU_LIST_NOT_HEAD( eu , &lu->down_hd ) )
			{
				struct edgeuse *eu_next;

				NMG_CK_EDGEUSE( eu );

				eu_next = BU_LIST_PNEXT( edgeuse , &eu->l );

				/* kill edges that are to/from same vertex */
				if( eu->vu_p->v_p == eu->eumate_p->vu_p->v_p )
				{
					while( (vu_next == eu->vu_p || vu_next == eu->eumate_p->vu_p ) &&
						BU_LIST_NOT_HEAD( vu_next , &new_v->vu_hd ) )
							vu_next = BU_LIST_PNEXT( vertexuse , &vu_next->l );
					while( (eu_next == eu || eu_next == eu->eumate_p) &&
						BU_LIST_NOT_HEAD( eu_next , &lu->down_hd ) )
							eu_next = BU_LIST_PNEXT( edgeuse , &eu_next->l );

					if( rt_g.NMG_debug & DEBUG_BASIC )
						bu_log( "\tkilling eu x%x (x%x)\n" , eu , eu->eumate_p );

					bad_loop = nmg_keu( eu );
				}
				/* kill edges with length less than tol->dist */
				else if( bn_pt3_pt3_equal( eu->vu_p->v_p->vg_p->coord , eu->eumate_p->vu_p->v_p->vg_p->coord , tol ) )
				{
					struct edgeuse *prev_eu;

					prev_eu = BU_LIST_PPREV_CIRC( edgeuse , &eu->l );
					NMG_CK_EDGEUSE( prev_eu );

					prev_eu->eumate_p->vu_p->v_p = eu->eumate_p->vu_p->v_p;

					while( (vu_next == eu->vu_p || vu_next == eu->eumate_p->vu_p ) &&
						BU_LIST_NOT_HEAD( vu_next , &new_v->vu_hd ) )
							vu_next = BU_LIST_PNEXT( vertexuse , &vu_next->l );
					while( (eu_next == eu || eu_next == eu->eumate_p) &&
						BU_LIST_NOT_HEAD( eu_next , &lu->down_hd ) )
							eu_next = BU_LIST_PNEXT( edgeuse , &eu_next->l );

					if( rt_g.NMG_debug & DEBUG_BASIC )
						bu_log( "\tkilling eu x%x (x%x)\n" , eu , eu->eumate_p );

					bad_loop = nmg_keu( eu );
				}

				if( bad_loop )
				{
					/* emptied a loop, so kill it */
					while( (lu_next == lu || lu_next == lu->lumate_p) &&
						BU_LIST_NOT_HEAD( lu_next , &fu->lu_hd ) )
							lu_next = BU_LIST_PNEXT( loopuse , &lu_next->l );

					bad_fu = nmg_find_fu_of_lu( lu );
					if( !nmg_klu( lu ) )
						bad_fu = (struct faceuse *)NULL;

					break;
				}

				eu = eu_next;
			}
			if( bad_fu )
			{
				/* emptied a faceuse, so kill it */
				if( nmg_kfu( bad_fu ) )
				{
					/* I can't believe I emptied the whole thing!! */
					bu_log( "nmg_remove_short_eus_inter: nmg_kfu emptied shell!!!\n" );
					break;
				}
			}
			lu = lu_next;
		}

		vu = vu_next;
	}
}

/**	N M G _ S I M P L I F Y _ I N T E R
 *
 * Eliminates adjacent intersect_fus structs with collinear edges
 *
 */
static void
nmg_simplify_inter(const struct vertex *new_v, struct bu_ptbl *int_faces, const struct bn_tol *tol)
{
	int edge_no=0;
	int next_edge_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_simplify_inter( new_v=x%x ( %f %f %f ), int_faces=x%x)\n",
			new_v, V3ARGS( new_v->vg_p->coord ) , int_faces );

	NMG_CK_VERTEX( new_v );
	BU_CK_PTBL( int_faces );
	BN_CK_TOL( tol );

	while( BU_PTBL_END( int_faces ) > 1 && edge_no < BU_PTBL_END( int_faces ) )
	{
		struct intersect_fus *i_fus;
		struct intersect_fus *j_fus;

		/* get two consectutive structures */
		i_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , edge_no );
		next_edge_no = edge_no+1;
		if( next_edge_no == BU_PTBL_END( int_faces ) )
			 next_edge_no = 0;
		j_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , next_edge_no );

		/* skip open space */
		if( (i_fus->free_edge || j_fus->free_edge) && next_edge_no == 0 )
		{
			edge_no++;
			continue;
		}

		/* Don't fuse free edges together */
		if( i_fus->free_edge && j_fus->free_edge )
		{
			edge_no++;
			continue;
		}

		/* if either vertex or edgeuse is null, skip */
		if( i_fus->vp == NULL || j_fus->vp == NULL ||
		    i_fus->eu == NULL || j_fus->eu == NULL )
		{
			edge_no++;
			continue;
		}

		/* If either vertex is new_v, skip */
		if( i_fus->vp == new_v || j_fus->vp == new_v )
		{
			edge_no++;
			continue;
		}

		NMG_CK_VERTEX( i_fus->vp );
		NMG_CK_VERTEX( j_fus->vp );
		NMG_CK_EDGEUSE( i_fus->eu );
		NMG_CK_EDGEUSE( j_fus->eu );

		/* if the two vertices are within tolerance,
		 * fuse them
		 */
		if( i_fus->vp == j_fus->vp )
		{
			nmg_fuse_inters( i_fus , j_fus , int_faces , tol );
			continue;
		}
		else if( bn_pt3_pt3_equal( i_fus->vp->vg_p->coord , j_fus->vp->vg_p->coord , tol ) )
		{
			nmg_jv( i_fus->vp , j_fus->vp );
			nmg_fuse_inters( i_fus , j_fus , int_faces , tol );
			continue;
		}
		else if( bn_3pts_collinear( i_fus->vp->vg_p->coord , j_fus->vp->vg_p->coord , new_v->vg_p->coord , tol ) )
		{
			fastf_t i_dist,j_dist;
			vect_t i_dist_to_new_v,j_dist_to_new_v;

			/* all three points are collinear,
			 * may need to split edges
			 */

			VSUB2( i_dist_to_new_v , new_v->vg_p->coord , i_fus->vp->vg_p->coord );
			VSUB2( j_dist_to_new_v , new_v->vg_p->coord , j_fus->vp->vg_p->coord );

			if( VDOT( i_dist_to_new_v , j_dist_to_new_v ) < 0.0 )
			{
				/* points are collinear with new_v, but in opoosit directions */
				edge_no++;
				continue;
			}

			i_dist = MAGSQ( i_dist_to_new_v );
			j_dist = MAGSQ( j_dist_to_new_v );

			if( i_dist < tol->dist_sq || j_dist < tol->dist_sq )
				rt_bomb( "nmg_simplify_inter: vertex within tolerance of new_v\n" );

			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "\tCollinear vertices x%x, x%x, and x%x\n",
						new_v , i_fus->vp , j_fus->vp );

			if( i_dist > j_dist && j_dist > tol->dist_sq )
			{
				/* j point is closer to new_v than i point
				 * split edge at j point
				 */

				if( rt_g.NMG_debug & DEBUG_BASIC )
					bu_log( "\tSplitting i_fus->eu x%x at vertex x%x\n" , i_fus->eu , j_fus->vp );

				(void)nmg_esplit( j_fus->vp , i_fus->eu, 0 );
				i_fus->vp = j_fus->vp;
				nmg_fuse_inters( i_fus , j_fus , int_faces , tol );

				continue;
			}
			else if( j_dist > i_dist && i_dist > tol->dist_sq )
			{
				/* i point is closer to new_v than j point
				 * split edge at i point
				 */

				if( rt_g.NMG_debug & DEBUG_BASIC )
					bu_log( "\tSplitting j_fus->eu x%x at vertex x%x\n" , j_fus->eu , i_fus->vp );

				(void)nmg_esplit( i_fus->vp , j_fus->eu, 0 );
				nmg_fuse_inters( i_fus , j_fus , int_faces , tol );
				continue;
			}
		}
		edge_no++;
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		bu_log( "\nAfter nmg_simplify_inter:\n" );
		nmg_pr_inter( new_v , int_faces );
	}
}

/**	N M G _ M A K E _ F A C E S _ A T _ V E R T
 *
 * Make new faces around vertex new_v using info in
 * the table of intersect_fu structures. Each structure
 * contains a vertex on an edge departing new_v.
 * Vertices from two consecutive edges are combined with
 * new_v to form triangular faces around new_v
 *
 */
void
nmg_make_faces_at_vert(struct vertex *new_v, struct bu_ptbl *int_faces, const struct bn_tol *tol)
{
	struct loopuse *old_lu;
	int edge_no=0,next_edge_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_make_faces_at_vert( x%x , %d intersect_fus structs)\n" , new_v , BU_PTBL_END( int_faces ) );

	NMG_CK_VERTEX( new_v );
	BU_CK_PTBL( int_faces );
	BN_CK_TOL( tol );

	if( BU_PTBL_END( int_faces ) == 1 )
	{
		struct intersect_fus *i_fus;

		/* only one intersect point is left, move new_v to it
		 * and don't make any faces
		 */
		i_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , 0 );
		if( i_fus->vp )
		{
			i_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , 0 );

			VMOVE( new_v->vg_p->coord , i_fus->vp->vg_p->coord );
			nmg_jv( new_v , i_fus->vp );
		}
		return;
	}

	if( BU_PTBL_END( int_faces ) == 2 )
	{
		struct intersect_fus *i_fus,*j_fus;
		point_t center_pt;

		/* only two intersect points left, if they are not on free edges,
		 *  move new_v to the center of the connecting line. No new faces needed
		 */
		i_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , 0 );
		j_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , 1 );

		if( i_fus->vp && j_fus->vp && !i_fus->free_edge && !j_fus->free_edge )
		{
			VCOMB2( center_pt , 0.5 , i_fus->vp->vg_p->coord , 0.5 , j_fus->vp->vg_p->coord );
			VMOVE( new_v->vg_p->coord , center_pt );
		}
		return;
	}

	/* Need to make new faces.
	 * loop around the vertex, looking at
	 * pairs of adjacent edges and deciding
	 * if a new face needs to be constructed
	 * from the two intersect vertices and new_v
	 */
	while( edge_no < BU_PTBL_END( int_faces ) )
	{
		struct intersect_fus *i_fus;
		struct intersect_fus *j_fus;
		struct vertexuse *vu1,*vu2;
		struct edgeuse *eu;
		struct loopuse *lu;
		struct loopuse *new_lu;
		struct faceuse *new_fu;
		struct faceuse *fu;

		/* get two consectutive structures */
		i_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , edge_no );
		next_edge_no = edge_no+1;
		if( next_edge_no == BU_PTBL_END( int_faces ) )
			 next_edge_no = 0;
		j_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , next_edge_no );

		/* Don't construct a new face across open space */
		if( (i_fus->free_edge || j_fus->free_edge) && next_edge_no == 0 )
		{
			edge_no++;
			continue;
		}

		/* if the two vertices are the same, no face needed */
		if( i_fus->vp == j_fus->vp )
		{
			edge_no++;
			continue;
		}

		/* if either vertex is null, no face needed */
		if( i_fus->vp == NULL || j_fus->vp == NULL || i_fus->eu == NULL || j_fus->eu == NULL )
		{
			edge_no++;
			continue;
		}

		/* Don't make faces with two vertices the same */
		if( i_fus->vp == new_v || j_fus->vp == new_v )
		{
			edge_no++;
			continue;
		}

		NMG_CK_VERTEX( i_fus->vp );
		NMG_CK_VERTEX( j_fus->vp );
		NMG_CK_EDGEUSE( i_fus->eu );
		NMG_CK_EDGEUSE( j_fus->eu );

		/* don't make degenerate faces */
		if( bn_3pts_collinear( i_fus->vp->vg_p->coord , j_fus->vp->vg_p->coord , new_v->vg_p->coord , tol ) )
		{
			edge_no++;
			continue;
		}

		/* O.K., here is where we actually start making faces.
		 * Find uses of the two vertices in the same loopuse
		 */
		old_lu = j_fus->eu->up.lu_p;
		vu1 = (struct vertexuse *)NULL;
		vu2 = (struct vertexuse *)NULL;
		for( BU_LIST_FOR( eu , edgeuse , &old_lu->down_hd ) )
		{
			if( eu->vu_p->v_p == i_fus->vp )
				vu1 = eu->vu_p;
			else if( eu->vu_p->v_p == j_fus->vp )
				vu2 = eu->vu_p;
		}

		if( vu1 == NULL || vu2 == NULL )
		{
			bu_log( "nmg_make_faces_at_vert: ERROR: Can't find loop containing vertices x%x and x%x\n" , i_fus->vp, j_fus->vp );
			bu_log( "\t( %f %f %f ) and ( %f %f %f )\n" , V3ARGS( i_fus->vp->vg_p->coord ) , V3ARGS( j_fus->vp->vg_p->coord ) );
			edge_no++;
			continue;
		}

		/* make sure the two vertices have a third between,
		 * otherwise, don't cut the loop
		 */
		eu = vu1->up.eu_p;
		if( eu->eumate_p->vu_p == vu2 )
		{
			edge_no++;
			continue;
		}
		eu = vu2->up.eu_p;
		if( eu->eumate_p->vu_p == vu1 )
		{
			edge_no++;
			continue;
		}

		/* cut the face loop across the two vertices */
		new_lu = nmg_cut_loop( vu1 , vu2 );

		/* Fix orientations.
		 * We will never be cutting an OT_OPPOSITE loop
		 * so the will always be OT_SAME
		 */
		new_lu->orientation = OT_SAME;
		new_lu->lumate_p->orientation = OT_SAME;
		old_lu->orientation = OT_SAME;
		old_lu->lumate_p->orientation = OT_SAME;

		/* find which loopuse contains new_v
		 * this will be the one to become a new face
		 */
		lu = NULL;

		/* first check old_lu */
		for( BU_LIST_FOR( eu , edgeuse , &old_lu->down_hd ) )
		{
			if( eu->vu_p->v_p == new_v )
			{
				lu = old_lu;
				break;
			}
		}

		/* if not found check new_lu */
		if( lu == NULL )
		{
			for( BU_LIST_FOR( eu , edgeuse , &new_lu->down_hd ) )
			{
				if( eu->vu_p->v_p == new_v )
				{
					lu = old_lu;
					break;
				}
			}
		}

		if( lu == NULL )
		{
			fu = old_lu->up.fu_p;
			bu_log( "nmg_make_faces_at_vert: can't find loop for new face\n" );
			bu_log( "vu1 = x%x (x%x) vu2 = x%x (x%x)\n" , vu1 , vu1->v_p , vu2 , vu2->v_p );
			bu_log( "new_v = x%x\n" , new_v );
			bu_log( "old_lu = x%x , new_lu = x%x\n" , old_lu , new_lu );
			nmg_pr_fu_briefly( fu , (char *)NULL );
			rt_bomb( "nmg_make_faces_at_vert: can't find loop for new face\n" );
		}

		/* make the new face from the new loop */
		new_fu = nmg_mk_new_face_from_loop( lu );

		/* update the intersect_fus structs (probably not necessary at this point) */
		j_fus->fu[0] = new_fu;
		i_fus->fu[1] = new_fu;

		NMG_CK_FACEUSE( new_fu );

		/* calculate a plane equation for the new face */
		if( nmg_calc_face_g( new_fu ) )
		{
			bu_log( "nmg_make_faces_at_vert: Failed to calculate plane eqn for face:\n " );
			bu_log( "\tnew_v is x%x at ( %f %f %f )\n" , new_v , V3ARGS( new_v->vg_p->coord ) );
			if( bn_3pts_collinear( new_v->vg_p->coord,
			    vu1->v_p->vg_p->coord, vu2->v_p->vg_p->coord,
			    tol ) )
				bu_log( "\tPoints are collinear\n" );
			nmg_pr_fu_briefly( new_fu , " " );
		}
		nmg_face_bb( new_fu->f_p , tol );

		edge_no++;
	}
}

/**	N M G _ K I L L _ C R A C K S _ A T _ V E R T E X
 *
 * Look at all faces around vertex new_v and kill any two
 * consecutive eu's that go from a vertex to a second then back
 * to the original vertex
 */
void
nmg_kill_cracks_at_vertex(const struct vertex *vp)
{
	struct bu_ptbl fus_at_vert;
	struct vertexuse *vu;
	struct faceuse *fu;
	int fu_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_kill_cracks_at_vertex( vp=x%x )\n" , vp );

	NMG_CK_VERTEX( vp );

	/* first make a list of all the faceuses at this vertex */
	bu_ptbl_init( &fus_at_vert , 64, " &fus_at_vert ");

	for( BU_LIST_FOR( vu , vertexuse , &vp->vu_hd ) )
	{
		NMG_CK_VERTEXUSE( vu );

		fu = nmg_find_fu_of_vu( vu );
		if( !fu )
			continue;

		NMG_CK_FACEUSE( fu );
		bu_ptbl_ins_unique( &fus_at_vert , (long *)fu );
	}

	/* Now look at these faceuses for cracks ( jaunts from a vertex and back to the same ) */
	for( fu_no=0 ; fu_no<BU_PTBL_END( &fus_at_vert ) ; fu_no++ )
	{
		struct loopuse *lu;
		int bad_face=0;

		fu = (struct faceuse *)BU_PTBL_GET( &fus_at_vert , fu_no );
		NMG_CK_FACEUSE( fu );

		lu = BU_LIST_FIRST( loopuse , &fu->lu_hd );
		while( BU_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			struct loopuse *lu_next;
			struct edgeuse *eu;
			int bad_loop=0;

			NMG_CK_LOOPUSE( lu );

			lu_next = BU_LIST_NEXT( loopuse , &lu->l );

			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			{
				lu = lu_next;
				continue;
			}

			eu = BU_LIST_FIRST( edgeuse , &lu->down_hd );
			while( BU_LIST_NOT_HEAD( eu , &lu->down_hd ) )
			{
				struct edgeuse *eu_prev;
				struct edgeuse *eu_next;

				NMG_CK_EDGEUSE( eu );

				eu_next = BU_LIST_NEXT( edgeuse , &eu->l );
				eu_prev = BU_LIST_PPREV_CIRC( edgeuse , &eu->l );
				NMG_CK_EDGEUSE( eu_prev );

				/* Check for a crack */
				if( EDGESADJ( eu , eu_prev ) )
				{
					/* found a crack, kill it */
					if( nmg_keu( eu ) )
					{
						/* This should never happen */
						bu_log( "ERROR: nmg_kill_cracks_at_vert: bad loopuse x%x\n" , lu );
						bad_loop = 1;
						break;
					}
					if( nmg_keu( eu_prev ) )
					{
						bad_loop = 1;
						break;
					}
				}
				eu = eu_next;
			}
			if( bad_loop )
			{
				if( nmg_klu( lu ) )
				{
					bad_face = 1;
					break;
				}
			}
			lu = lu_next;
		}
		if( bad_face )
		{
			if( nmg_kfu( fu ) )
				bu_log( "ERROR:nmg_kill_cracks_at_vert: bad shell!!!\n" );
		}
	}
	bu_ptbl_free( &fus_at_vert );
}

/**	N M G _ D I S T _ T O _ C R O S S
 *
 * Used by nmg_fix_crossed edges to calculate the point
 * where two edges cross
 *
 *	returns:
 *		distance to intersection if edge intersect
 *		-1.0 if they don't
 */
static fastf_t
nmg_dist_to_cross(const struct intersect_fus *i_fus, const struct intersect_fus *j_fus, fastf_t *new_pt, const struct bn_tol *tol)
{
	plane_t pl;
	struct edgeuse *i_next_eu,*j_next_eu;
	struct vertex *i_end,*j_end;
	struct vertex *i_start,*j_start;
	point_t i_end_pt,j_end_pt;
	vect_t i_dir,j_dir;
	fastf_t dist[2];

	BN_CK_TOL( tol );

	if( i_fus->fu[1] )
		NMG_GET_FU_PLANE( pl , i_fus->fu[1] )

	/* get edgeuses leaving from new vertices */
	if( !i_fus->fu[0] )
		i_next_eu = BU_LIST_PPREV_CIRC( edgeuse , &i_fus->eu->l );
	else
		i_next_eu = BU_LIST_PNEXT_CIRC( edgeuse , &i_fus->eu->l );

	if( !j_fus->fu[0] )
		j_next_eu = BU_LIST_PPREV_CIRC( edgeuse , &j_fus->eu->l );
	else
		j_next_eu = BU_LIST_PNEXT_CIRC( edgeuse , &j_fus->eu->l );

	NMG_CK_EDGEUSE( i_next_eu );
	NMG_CK_EDGEUSE( j_next_eu );

	/* get endpoints for these edges */
	i_end = i_next_eu->eumate_p->vu_p->v_p;
	j_end = j_next_eu->eumate_p->vu_p->v_p;

	NMG_CK_VERTEX( i_end );
	NMG_CK_VERTEX( j_end );

	/* since the other end of these edges may not have been adjusted yet
	 * project the endpoints onto the face plane
	 */
	if( i_fus->fu[1] )
	{
		VJOIN1( i_end_pt , i_end->vg_p->coord , -(DIST_PT_PLANE( i_end->vg_p->coord , pl )) , pl )
		VJOIN1( j_end_pt , j_end->vg_p->coord , -(DIST_PT_PLANE( j_end->vg_p->coord , pl )) , pl )
	}
	else
	{
		VMOVE( i_end_pt , i_end->vg_p->coord )
		VMOVE( j_end_pt , j_end->vg_p->coord )
	}

	/* get start points, guaranteed to be on plane */
	i_start =  i_next_eu->vu_p->v_p;
	j_start =  j_next_eu->vu_p->v_p;

	NMG_CK_VERTEX( i_start );
	NMG_CK_VERTEX( j_start );

	/* calculate direction vectors for use by bn_isect_lseg3_lseg3 */
	VSUB2( i_dir , i_end_pt , i_start->vg_p->coord );
	VSUB2( j_dir , j_end_pt , j_start->vg_p->coord );

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		bu_log( "nmg_dist_to_cross: checking edges x%x and x%x:\n" , i_fus , j_fus );
		bu_log( "\t( %f %f %f ) <-> ( %f %f %f )\n", V3ARGS( i_start->vg_p->coord ), V3ARGS( i_end_pt ) );
		bu_log( "\t( %f %f %f ) <-> ( %f %f %f )\n", V3ARGS( j_start->vg_p->coord ), V3ARGS( j_end_pt ) );
	}

	if( i_fus->free_edge && j_fus->free_edge )
	{
		fastf_t max_dist0;
		fastf_t max_dist1;
		int ret_val;

		if( rt_g.NMG_debug & DEBUG_BASIC )
			bu_log( "\tBoth are free edges\n" );

		max_dist0 = MAGNITUDE( i_dir );
		VSCALE( i_dir , i_dir , (1.0/max_dist0) )
		max_dist1 = MAGNITUDE( j_dir );
		VSCALE( j_dir , j_dir , (1.0/max_dist1) )

		/* check if these two edges intersect or pass near each other */
		if( (ret_val=rt_dist_line3_line3( dist , i_start->vg_p->coord , i_dir ,
			j_start->vg_p->coord , j_dir , tol )) >= 0 )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
			{
				bu_log( "max_dists = %f , %f\n" , max_dist0,max_dist1 );
				bu_log( "dist = %f , %f\n" , dist[0] , dist[1] );
			}

			/* if the closest approach or intersect point is
			 * within the edge endpoints, this is a real intersection
			 */
			if( dist[0] >= 0.0 && dist[0] <= max_dist0 &&
			    dist[1] >= 0.0 && dist[1] <= max_dist1 )
			{
				plane_t pl1,pl2,pl3;

				if( rt_g.NMG_debug & DEBUG_BASIC )
				{
					point_t tmp_pt;

					bu_log( "\t\tintersection!!\n" );
					VJOIN1( tmp_pt , i_start->vg_p->coord , dist[0] , i_dir );
					bu_log( "\t\t\t( %f %f %f )\n" , V3ARGS( tmp_pt ) );
					VJOIN1( tmp_pt , j_start->vg_p->coord , dist[1] , j_dir );
					bu_log( "\t\t\t( %f %f %f )\n" , V3ARGS( tmp_pt ) );
				}

				/* calculate the intersect point */
				NMG_GET_FU_PLANE( pl1 , j_fus->fu[1] );
				NMG_GET_FU_PLANE( pl2 , i_fus->fu[0] );
				VCROSS( pl3 , pl1 , pl2 );
				pl3[3] = VDOT( pl3 , i_fus->vp->vg_p->coord );
				bn_mkpoint_3planes( new_pt , pl1 , pl2 , pl3 );

				return( dist[0] );
			}
			else
				return( (fastf_t)(-1.0) );
		}
		else
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "ret_val = %d\n" , ret_val );

			return( (fastf_t)(-1.0) );
		}
	}
	else
	{
		/* check if these two edges intersect */
		if( bn_isect_lseg3_lseg3( dist , i_start->vg_p->coord , i_dir ,
			j_start->vg_p->coord , j_dir , tol ) == 1 )
		{
			fastf_t len0;

			len0 = MAGNITUDE( i_dir );

			/* calculate intersection point */
			if( dist[0] == 0.0 )
				VMOVE( new_pt , i_start->vg_p->coord )
			else if( dist[0] == 1.0 )
				VMOVE( new_pt , i_end_pt )
			else if( dist[1] == 0.0 )
				VMOVE( new_pt , j_start->vg_p->coord )
			else if( dist[1] == 1.0 )
				VMOVE( new_pt , j_end_pt )
			else
				VJOIN1( new_pt , i_start->vg_p->coord , dist[0] , i_dir )

			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "\tdist=%f, new_pt=( %f %f %f )\n" , dist[0] , V3ARGS( new_pt ) );

			return( dist[0]*len0 );
		}
		else
			return( (fastf_t)(-1.0) );
	}
}

/**	N M G _ F I X _ C R O S S E D _ L O O P S
 *
 * Detect situations where edges have been split, but new vertices are
 * in wrong order. This typically happens as shown:
 *
 *                  new face planes
 *                  |
 *                  |
 *   \       \   /  |    /
 *    \       \ /<--|   /
 *     \       X       /
 *      \     / \     /
 *       \   /___\   /
 *        \         /
 *         \       /<- original face planes
 *          \     /
 *           \___/
 *
 * This can be detected by checking if the edges leaving from the new
 * vertices cross. If so, the middle face is deleted and the
 * two vertices are fused.
 *
 */
static void
nmg_fix_crossed_loops(struct vertex *new_v, struct bu_ptbl *int_faces, const struct bn_tol *tol)
{
	int edge_no;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_fix_crossed_loops( new_v=x%x ( %f %f %f ), %d edges)\n", new_v , V3ARGS( new_v->vg_p->coord ) , BU_PTBL_END( int_faces ) );

	NMG_CK_VERTEX( new_v );
	BU_CK_PTBL( int_faces );
	BN_CK_TOL( tol );

	/* first check for edges that cross both adjacent edges */
	if( BU_PTBL_END( int_faces ) > 2 )
	{
		for( edge_no=0 ; edge_no<BU_PTBL_END( int_faces ) ; edge_no++ )
		{
			int next_edge_no,prev_edge_no;
			struct intersect_fus *edge_fus;
			struct intersect_fus *next_fus,*prev_fus;
			fastf_t dist1,dist2;
			point_t pt1,pt2;

			edge_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , edge_no );

			if( !edge_fus->vp )
				continue;

			/* look at next edge */
			next_edge_no = edge_no + 1;
			if( next_edge_no == BU_PTBL_END( int_faces ) )
				next_edge_no = 0;

			next_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , next_edge_no );

			/* Don't want to fuse two dangling edges */
			if( next_fus->vp && (!edge_fus->free_edge || !next_fus->free_edge) )
				dist1 = nmg_dist_to_cross( edge_fus , next_fus , pt1 , tol );
			else
				dist1 = (-1.0);

			/* look at previous edge */
			prev_edge_no = edge_no - 1;
			if( prev_edge_no < 0 )
				prev_edge_no = BU_PTBL_END( int_faces ) - 1;

			prev_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , prev_edge_no );

			/* Don't want to fuse two dangling edges */
			if( prev_fus->vp && (!edge_fus->free_edge || !prev_fus->free_edge) )
				dist2 = nmg_dist_to_cross( edge_fus , prev_fus , pt2 , tol );
			else
				dist2 = (-1.0);

			/* if no intersections, continue */
			if( dist1 < tol->dist || dist2 < tol->dist )
				continue;

			if( rt_g.NMG_debug & DEBUG_BASIC )
			{
				bu_log( "fus=x%x, prev=x%x, next=x%x, dist1=%f, dist2=%f\n",
					edge_fus,next_fus,prev_fus,dist1,dist2 );
				bu_log( "\t( %f %f %f ), ( %f %f %f )\n" , V3ARGS( pt1 ) , V3ARGS( pt2 ) );
			}

			/* if both intersections are at the same point, merge all three */
			if( bn_pt3_pt3_equal( pt1 , pt2 , tol ) )
			{
				if( rt_g.NMG_debug & DEBUG_BASIC )
					bu_log( "\tMerging all three points to pt1\n" );

				VMOVE( edge_fus->vp->vg_p->coord , pt1 );
				VMOVE( edge_fus->pt , pt1 );
				VMOVE( next_fus->vp->vg_p->coord , pt1 );
				VMOVE( next_fus->pt , pt1 );
				VMOVE( prev_fus->vp->vg_p->coord , pt1 );
				VMOVE( prev_fus->pt , pt1 );
			}
			else if( dist1 > dist2 )
			{
				/* merge edge point with next edge point */
				if( rt_g.NMG_debug & DEBUG_BASIC )
					bu_log( "\tMerging edge and next to pt1, moving prev to pt2\n");
				VMOVE( edge_fus->vp->vg_p->coord , pt1 );
				VMOVE( edge_fus->pt , pt1 );
				VMOVE( next_fus->vp->vg_p->coord , pt1 );
				VMOVE( next_fus->pt , pt1 );

				VMOVE( prev_fus->vp->vg_p->coord , pt2 );
				VMOVE( prev_fus->pt , pt2 );
			}
			else
			{
				/* merge edge point with previous point */
				if( rt_g.NMG_debug & DEBUG_BASIC )
					bu_log( "\tMerging edge and prev to pt2, moving next to pt1\n" );
				VMOVE( edge_fus->vp->vg_p->coord , pt2 );
				VMOVE( edge_fus->pt , pt2 );
				VMOVE( prev_fus->vp->vg_p->coord , pt2 );
				VMOVE( prev_fus->pt , pt2 );

				VMOVE( next_fus->vp->vg_p->coord , pt1 );
				VMOVE( next_fus->pt , pt1 );
			}
		}
	}

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		bu_log( "After fixing edges that intersect two edges:\n" );
		nmg_pr_inter( new_v , int_faces );
	}

	/* now look for edges that cross just a single adjacent edge */
	for( edge_no=0 ; edge_no<BU_PTBL_END( int_faces ) ; edge_no++ )
	{
		int next_edge_no;
		struct intersect_fus *edge_fus;
		struct intersect_fus *next_fus;
		point_t pt;
		fastf_t dist;

		edge_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , edge_no );

		if( !edge_fus->vp )
			continue;

		/* just look at next edge */
		next_edge_no = edge_no + 1;
		if( next_edge_no == BU_PTBL_END( int_faces ) )
			next_edge_no = 0;

		next_fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , next_edge_no );

		if( !next_fus->vp )
			continue;

		/* check for intersection */
		dist = nmg_dist_to_cross( edge_fus , next_fus , pt , tol );

		if( dist > tol->dist )
		{
			/* there is an intersection */
			if( rt_g.NMG_debug & DEBUG_BASIC )
			{
				bu_log( "edge x%x intersect next edge x%x\n" , edge_fus , next_fus );
				bu_log( "\tdist=%f, ( %f %f %f )\n" , dist , V3ARGS( pt ) );
			}
			if( edge_fus->free_edge && next_fus->free_edge )
			{
				/* if both edges are free edges, move new_v to the intersection */
				VMOVE( edge_fus->vp->vg_p->coord , pt );
				VMOVE( edge_fus->pt , pt );
				VMOVE( next_fus->vp->vg_p->coord , pt );
				VMOVE( next_fus->pt , pt );
				VMOVE( new_v->vg_p->coord , pt );
			}
			else
			{
				/* just merge the two points */
				VMOVE( edge_fus->vp->vg_p->coord , pt );
				VMOVE( edge_fus->pt , pt );
				VMOVE( next_fus->vp->vg_p->coord , pt );
				VMOVE( next_fus->pt , pt );
			}
		}
	}
	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		bu_log( "After nmg_fix_crossed_loops:\n" );
		nmg_pr_inter( new_v , int_faces );
	}
}

/*	N M G _ C A L C _ N E W _ V
 *
 * Calculates a new geometry for new_v
 */
static int
nmg_calc_new_v(struct vertex *new_v, const struct bu_ptbl *int_faces, const struct bn_tol *tol)
{
	plane_t *planes;
	int pl_count;
	int i;

	NMG_CK_VERTEX( new_v );
	BU_CK_PTBL( int_faces );
	BN_CK_TOL( tol );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_calc_new_v: (%f %f %f) , %d faces\n" , V3ARGS( new_v->vg_p->coord ) , BU_PTBL_END( int_faces ) );

	/* make space for at least three planes */
	i = BU_PTBL_END( int_faces );
	if( i < 3 )
		i = 3;
	planes = (plane_t *)bu_calloc( i , sizeof( plane_t ) , "nmg_calc_new_v: planes" );

	pl_count = 0;

	for( i=0 ; i<BU_PTBL_END( int_faces ) ; i++ )
	{
		struct intersect_fus *fus;
		plane_t pl;
		int j;
		int unique=1;

		fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , i );

		if( !fus->fu[0] )
			continue;

		NMG_CK_FACEUSE( fus->fu[0] );
		NMG_GET_FU_PLANE( pl , fus->fu[0] );

		for( j=0 ; j<pl_count ; j++ )
		{
			if( bn_coplanar( planes[j] , pl , tol ) > 0 )
			{
				unique = 0;
				break;
			}
		}

		if( !unique )
			continue;

		VMOVE( planes[pl_count] , pl );
		planes[pl_count][H] = pl[H];
		pl_count++;
	}

	if( pl_count > 2 )
	{
		if( bn_isect_planes( new_v->vg_p->coord , (const plane_t *)planes , pl_count ) )
		{
			bu_log( "nmg_cacl_new_v: Cannot solve for new geometry at ( %f %f %f )\n",
				V3ARGS( new_v->vg_p->coord ) );
			bu_free( (char *)planes , "nmg_calc_new_v: planes" );
			return( 1 );
		}
	}
	else if( pl_count == 1 )
	{
		fastf_t vert_move_len;

		/* move the vertex to the plane */
		vert_move_len = DIST_PT_PLANE( new_v->vg_p->coord , planes[0] );
		VJOIN1( new_v->vg_p->coord , new_v->vg_p->coord , -vert_move_len , planes[0] );
	}
	else if( pl_count == 2 )
	{
		VCROSS( planes[2] , planes[0] , planes[1] );
		planes[2][H] = VDOT( new_v->vg_p->coord , planes[2] );
		pl_count = 3;
		if( bn_mkpoint_3planes( new_v->vg_p->coord , planes[0] , planes[1] , planes[2] ) )
		{
			bu_log( "nmg_cacl_new_v: 3 planes do not intersect at a point\n" );
			bu_free( (char *)planes , "nmg_calc_new_v: planes" );
			return( 1 );
		}
	}
	else
	{
		bu_log( "nmg_calc_new_v: No face planes at vertex x%x (%f %f %f)\n",
			new_v , V3ARGS( new_v->vg_p->coord ) );
		bu_free( (char *)planes , "nmg_calc_new_v: planes" );
		return( 1 );
	}

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "\tnew_v = ( %f %f %f )\n" , V3ARGS( new_v->vg_p->coord ) );

	bu_free( (char *)planes , "nmg_calc_new_v: planes" );

	for( i=0 ; i<BU_PTBL_END( int_faces ) ; i++ )
	{
		struct intersect_fus *fus;
		fastf_t dist;

		fus = (struct intersect_fus *)BU_PTBL_GET( int_faces , i );

		(void) rt_dist_pt3_line3( &dist , fus->start , fus->start , fus->dir , new_v->vg_p->coord , tol );
	}

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		bu_log( "After nmg_calc_new_v:\n" );
		nmg_pr_inter( new_v , int_faces );
	}

	return( 0 );
}

/**	N M G _ C O M P L E X _ V E R T E X _ S O L V E
 *
 *	This is intended to handle the cases the "nmg_simple_vertex_solve"
 *	can't do (more than three faces intersecting at a vertex)
 *
 *	This routine may create new edges and/or faces and
 *	Modifies the location of "new_v"
 *
 *	if approximate is non-zero, the new geomatry is
 *	approximated by calculating the point with minimum
 *	distance to all the intersecting faces
 *
 *	returns:
 *		0 - if everything is OK
 *		1 - failure
 */

int
nmg_complex_vertex_solve(struct vertex *new_v, const struct bu_ptbl *faces, const int free_edges, const int approximate, const struct bn_tol *tol)
{
	struct faceuse *fu;
	struct face *fp1;
	struct bu_ptbl int_faces;
	int i;

	/* More than 3 faces intersect at vertex (new_v)
	 * Calculate intersection point along each edge
	 * emanating from new_v */

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_complex_vertex_solve( new_v = x%x , %d faces )\n" , new_v , BU_PTBL_END( faces ) );

	NMG_CK_VERTEX( new_v );
	BU_CK_PTBL( faces );
	BN_CK_TOL( tol );

	if( approximate )
	{
		plane_t *planes;
		int plane_count;

		plane_count = BU_PTBL_END( faces );

		planes = (plane_t *)bu_calloc( plane_count+free_edges , sizeof( plane_t ) , "nmg_complex_vertex_solve: planes" );


		for( i=0 ; i<BU_PTBL_END( faces ) ; i++ )
		{
			fp1 = (struct face *)BU_PTBL_GET( faces , i );
			fu = fp1->fu_p;
			NMG_GET_FU_PLANE( planes[i] , fu );

			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "\t plane #%d: %g %g %g %g\n", i, V4ARGS( planes[i] ) );
		}

		if( rt_g.NMG_debug & DEBUG_BASIC )
		{
			int j;

			for( i=0 ; i<BU_PTBL_END( faces ); i++ )
			{
				fastf_t dot;

				dot = VDOT( planes[i], new_v->vg_p->coord );
				bu_log( "\tVDOT( #%d, new_v ) - dist = %g\n", i, dot-planes[i][3] );

				for( j=0 ; j<BU_PTBL_END( faces ) ; j++ )
				{
					dot = VDOT( planes[i], planes[j] );
					bu_log( "\tVDOT( #%d, #%d ) = %g\n",i,j, dot );
				}
			}
		}

		if( free_edges )
		{
			int free_edge_count=0;
			struct vertexuse *vu;
			struct faceuse *fu_free=(struct faceuse *)NULL;
			struct edgeuse *eu_free=(struct edgeuse *)NULL;

			plane_count = BU_PTBL_END( faces );

			for( BU_LIST_FOR( vu, vertexuse, &new_v->vu_hd ) )
			{
				struct edgeuse *eu;

				if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
					continue;

				eu = vu->up.eu_p;

				if( eu->radial_p == eu->eumate_p )
				{
					vect_t fu_norm;

					/* this is a free edges */
					eu_free = eu;
					fu_free = nmg_find_fu_of_eu( eu_free );
					free_edge_count++;

					NMG_GET_FU_NORMAL( fu_norm, fu_free );

					VCROSS( planes[plane_count], fu_norm, eu_free->g.lseg_p->e_dir );
					VUNITIZE( planes[plane_count] );

					planes[plane_count][3] = VDOT( planes[plane_count], new_v->vg_p->coord );

					if( rt_g.NMG_debug & DEBUG_BASIC )
						bu_log( "\t added plane #%d: %g %g %g %g\n", plane_count, V4ARGS( planes[plane_count] ) );

					plane_count++;

					if( free_edge_count == free_edges )
						break;
				}
			}
		}

		if( bn_isect_planes( new_v->vg_p->coord , (const plane_t *)planes , plane_count ) )
		{
			bu_log( "nmg_complex_vertex_solve: Could not calculate new geometry at ( %f %f %f )\n",
				V3ARGS( new_v->vg_p->coord ) );
			bu_free( (char *) planes , "nmg_complex_vertex_solve: planes" );
			return( 1 );
		}
		bu_free( (char *) planes , "nmg_complex_vertex_solve: planes" );

		if( rt_g.NMG_debug & DEBUG_BASIC )
			bu_log( "nmg_complex_vertex_solve: new coords = ( %f %f %f )\n",
				V3ARGS( new_v->vg_p->coord ) );
		return( 0 );
	}

	bu_ptbl_init( &int_faces , 64, " &int_faces ");

	/* get int_faces table (of intersect_fus structures) partially filled in
	 * with fu's, eu, and edge line definition
	 */
	if( nmg_get_edge_lines( new_v , &int_faces , tol ) )
	{
		bu_ptbl_free( &int_faces );
		return( 1 );
	}

	/* calculate geometry for new_v */
	if( nmg_calc_new_v( new_v , &int_faces , tol ) )
	{
		bu_ptbl_free( &int_faces );
		return( 1 );
	}

	/* fill in "pt" portion of intersect_fus structures with points
	 * that are the intersections of the edge line with the other
	 * edges that meet at new_v. The intersection that is furthest
	 * up the edge away from new_v is selected
	 */
	if (nmg_get_max_edge_inters( new_v , &int_faces , faces , tol ) )
	{
		bu_ptbl_free( &int_faces );
		return( 1 );
	}

	/* split edges at intersection points */
	nmg_split_edges_at_pts( new_v , &int_faces , tol );

	/* fix intersection points that cause loops that cross themselves */
	nmg_fix_crossed_loops( new_v , &int_faces , tol );

	nmg_remove_short_eus_inter( new_v , &int_faces , tol );

	nmg_simplify_inter( new_v , &int_faces , tol );

	/* Build needed faces */
	nmg_make_faces_at_vert( new_v , &int_faces , tol );

	/* Where faces were not built, cracks have formed */
	nmg_kill_cracks_at_vertex( new_v );

	/* free some memory */
	for( i=0 ; i<BU_PTBL_END( &int_faces ) ; i++ )
	{
		struct intersect_fus *i_fus;

		i_fus = (struct intersect_fus *)BU_PTBL_GET( &int_faces , i );
		bu_free( (char *)i_fus , "nmg_complex_vertex_solve: intersect_fus struct\n" );
	}
	bu_ptbl_free( &int_faces );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_co,mplex_vertex_solve: new coords = ( %f %f %f )\n",
			V3ARGS( new_v->vg_p->coord ) );

	return( 0 );
}

/**	N M G _ B A D _ F A C E _ N O R M A L S
 *
 *	Look for faceuses in the shell with normals that do
 *	not agree with the geometry (i.e., in the wrong direction)
 *
 *	return:
 *		1 - at least one faceuse with a bad normal was found
 *		0 - no faceuses with bad normals were found
 */
int
nmg_bad_face_normals(const struct shell *s, const struct bn_tol *tol)
{
	struct faceuse *fu;
	struct loopuse *lu;
	vect_t old_normal;
	plane_t new_plane;

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		fastf_t area = -1;

		NMG_CK_FACEUSE( fu );

		/* only check OT_SAME faseuses */
		if( fu->orientation != OT_SAME )
			continue;

		/* get current normal */
		NMG_GET_FU_NORMAL( old_normal , fu );

		for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );

			if( lu->orientation != OT_SAME && lu->orientation != OT_OPPOSITE )
				continue;

			if( (area = nmg_loop_plane_area( lu , new_plane )) > 0.0 )
			{
				if( lu->orientation != OT_SAME )
					VREVERSE( new_plane , new_plane )
				break;
			}
		}

		if( area > 0.0 )
		{
			if( VDOT( old_normal , new_plane ) < 0.0 )
				return( 1 );
		}
	}
	return( 0 );
}

/**
 *	N M G _ F A C E S _ A R E _ R A D I A L
 *
 *	checks if two faceuses are radial to each other
 *
 *	returns
 *		1 - the two faceuses are radial to each other
 *		0 - otherwise
 *
 */
int
nmg_faces_are_radial(const struct faceuse *fu1, const struct faceuse *fu2)
{
	struct edgeuse *eu,*eu_tmp;
	struct loopuse *lu;

	NMG_CK_FACEUSE( fu1 );
	NMG_CK_FACEUSE( fu2 );

	/* look at every loop in the faceuse #1 */
	for( BU_LIST_FOR( lu , loopuse , &fu1->lu_hd ) )
	{
		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		/* look at every edgeuse in the loop */
		for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			/* now search radially around edge */
			eu_tmp = eu->eumate_p->radial_p;
			while( eu_tmp != eu && eu_tmp != eu->eumate_p )
			{
				struct faceuse *fu_tmp;

				/* find radial faceuse */
				fu_tmp = nmg_find_fu_of_eu( eu_tmp );

				/* if its the same as fu2 or its mate, the faceuses are radial */
				if( fu_tmp == fu2 || fu_tmp == fu2->fumate_p )
					return( 1 );

				/* go to next radial edgeuse */
				eu_tmp = eu_tmp->eumate_p->radial_p;
			}
		}
	}

	return( 0 );
}

/**	N M G _ M O V E _ E D G E _ T H R U _ P T
 *
 *	moves indicated edgeuse (mv_eu) so that it passes thru
 *	the given point (pt). The direction of the edgeuse
 *	is not changed, so new edgeuse is parallel to the original.
 *
 *	plane equations of all radial faces on this edge are changed
 *	and all vertices (except one anchor point) in radial loops are adjusted
 *	Note that the anchor point is chosen arbitrarily.
 *
 *	returns:
 *		1 - failure
 *		0 - success
 */

int
nmg_move_edge_thru_pt(struct edgeuse *mv_eu, const fastf_t *pt, const struct bn_tol *tol)
{
	struct faceuse *fu,*fu1;
	struct edgeuse *eu,*eu1;
	struct edge_g_lseg *eg;
	struct vertex *v1,*v2;
	struct model *m;
	vect_t e_dir;
	struct bu_ptbl tmp_faces[2];
	struct bu_ptbl faces;
	int count;
	long *flags;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_move_edge_thru_pt( mv_eu=x%x , pt=( %f %f %f) )\n" , mv_eu , V3ARGS( pt ) );

	NMG_CK_EDGEUSE( mv_eu );
	BN_CK_TOL( tol );

	m = nmg_find_model( &mv_eu->l.magic );
	NMG_CK_MODEL( m );

	/* get endpoint vertices */
	v1 = mv_eu->vu_p->v_p;
	NMG_CK_VERTEX( v1 );
	v2 = mv_eu->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX( v2 );

	eg = mv_eu->g.lseg_p;

	/* get edge direction */
	if( v1 != v2 )
	{
		if( eg )
		{
			NMG_CK_EDGE_G_LSEG(eg);
			VMOVE( e_dir , eg->e_dir );
			if( mv_eu->orientation == OT_OPPOSITE )
			{
				VREVERSE( e_dir , e_dir );
			}
		}
		else
		{
			nmg_edge_g( mv_eu );
			eg = mv_eu->g.lseg_p;
			VMOVE( e_dir , eg->e_dir );
		}
		VUNITIZE( e_dir );
	}

	eu = mv_eu;
	fu1 = nmg_find_fu_of_eu( eu );

	if( fu1 == NULL )
	{
		vect_t to_pt;
		vect_t move_v;
		fastf_t edir_comp;
		point_t new_loc;

		/* This must be a wire edge, just adjust the endpoints */
		/* keep edge the same length, and move vertices perpendicular to e_dir */

		VSUB2( to_pt , pt , v1->vg_p->coord );
		edir_comp = VDOT( to_pt , e_dir );
		VJOIN1( move_v , to_pt , -edir_comp , e_dir );

		/* move the vertices */
		VADD2( new_loc , v1->vg_p->coord , move_v );
		nmg_vertex_gv( v1 , new_loc );

		if( v2 != v1 )
		{
			VADD2( new_loc , v2->vg_p->coord , move_v );
			nmg_vertex_gv( v2 , new_loc );
		}

		/* adjust edge geometry */
		if( !eg )
			nmg_edge_g( eu );
		else
			VMOVE( eg->e_pt , new_loc )

		if( *eu->up.magic_p == NMG_LOOPUSE_MAGIC )
		{
			struct edgeuse *tmp_eu;

			/* edge is part of a wire loop
			 * need to adjust geometry neighbor edges
			 */

			tmp_eu = BU_LIST_PNEXT_CIRC( edgeuse , &eu->l );
			NMG_CK_EDGEUSE( tmp_eu );
			if( *tmp_eu->g.magic_p == NMG_EDGE_G_LSEG_MAGIC )
			{
				VMOVE( tmp_eu->g.lseg_p->e_pt , tmp_eu->vu_p->v_p->vg_p->coord )
				VSUB2( tmp_eu->g.lseg_p->e_dir, tmp_eu->eumate_p->vu_p->v_p->vg_p->coord, tmp_eu->g.lseg_p->e_pt)
			}
			tmp_eu = BU_LIST_PPREV_CIRC( edgeuse , &eu->l );
			NMG_CK_EDGEUSE( tmp_eu );
			if( *tmp_eu->g.magic_p == NMG_EDGE_G_LSEG_MAGIC )
			{
				VMOVE( tmp_eu->g.lseg_p->e_pt , tmp_eu->vu_p->v_p->vg_p->coord )
				VSUB2( tmp_eu->g.lseg_p->e_dir, tmp_eu->eumate_p->vu_p->v_p->vg_p->coord, tmp_eu->g.lseg_p->e_pt)
			}
		}

		return( 0 );
	}

	/* can only handle edges with up to two radial faces */
	if( mv_eu->radial_p->eumate_p != mv_eu->eumate_p->radial_p && mv_eu->radial_p != mv_eu->eumate_p )
	{
		bu_log( "Cannot handle edges with more than two radial faces\n" );
		return( 1 );
	}

	bu_ptbl_init( &tmp_faces[0] , 64, " &tmp_faces[0] ");
	bu_ptbl_init( &tmp_faces[1] , 64, " &tmp_faces[1] ");

	/* cannot handle complex vertices yet */
	if( nmg_find_isect_faces( v1 , &tmp_faces[0] , &count , tol ) > 3 ||
	    nmg_find_isect_faces( v2 , &tmp_faces[1] , &count , tol ) > 3 )
	{
		bu_log( "nmg_move_edge_thru_pt: cannot handle complex vertices yet\n" );
		bu_ptbl_free( &tmp_faces[0] );
		bu_ptbl_free( &tmp_faces[1] );
		return( 1 );
	}

	/* Move edge geometry to new point */
	if( eg )
	{
		VMOVE( eg->e_pt , pt );
	}

	/* modify plane equation for each face radial to mv_eu */
	fu = fu1;
	do
	{
		struct edgeuse *eu_next;
		plane_t plane;
		int cross_direction;
		vect_t norm;
		int done;

		NMG_CK_EDGEUSE( eu );
		NMG_CK_FACEUSE( fu );

		if( fu->orientation == OT_SAME )
			NMG_GET_FU_NORMAL( norm , fu )
		else
			NMG_GET_FU_NORMAL( norm , fu->fumate_p )

		/* find an anchor point for face to rotate about
		 * go forward in loop until we find a vertex that is
		 * far enough from the line of mv_eu to produce a
		 * non-zero cross product
		 */
		eu_next = eu;
		done = 0;
		while( !done )
		{
			vect_t to_anchor;
			vect_t next_dir;
			vect_t cross;
			struct vertex *anchor_v;
			fastf_t mag;

			/* get next edgeuse in loop */
			eu_next = BU_LIST_PNEXT_CIRC( edgeuse , &eu_next->l );

			/* check if we have circled the entire loop */
			if( eu_next == eu )
			{
				bu_log( "nmg_move_edge_thru_pt: cannot calculate new plane eqn\n" );
				return( 1 );
			}

			/* anchor point is endpoint of this edgeuse */
			anchor_v = eu_next->eumate_p->vu_p->v_p;

			VSUB2( next_dir , anchor_v->vg_p->coord , eu_next->vu_p->v_p->vg_p->coord );
			VCROSS( cross , e_dir , next_dir );
			if( VDOT( cross , norm ) < 0.0 )
				cross_direction = 1;
			else
				cross_direction = 0;

			/* calculate new plane */
			VSUB2( to_anchor , anchor_v->vg_p->coord , pt );
			if( cross_direction )
			{
				VCROSS( plane , to_anchor , e_dir );
			}
			else
			{
				VCROSS( plane , e_dir , to_anchor );
			}

			mag = MAGNITUDE( plane );
			if( mag > SQRT_SMALL_FASTF )
			{
				/* this is an acceptable plane */
				mag = 1.0/mag;
				VSCALE( plane , plane , mag );
				plane[3] = VDOT( plane , pt );

				/* assign this plane to the face */
				if( fu->orientation == OT_SAME )
					nmg_face_g( fu , plane );
				else
					nmg_face_g( fu->fumate_p , plane );
				done = 1;
			}
		}

		/* move on to next radial face */
		eu = eu->eumate_p->radial_p;
		fu = nmg_find_fu_of_eu( eu );
	}
	while( fu != fu1 && fu != fu1->fumate_p );

	/* now recalculate vertex coordinates for all affected vertices,
	 * could be lots of them
	 */

	flags = (long *)bu_calloc( m->maxindex , sizeof( long ) , "nmg_move_edge_thru_pt: flags" );
	bu_ptbl_init( &faces , 64, " &faces ");

	eu1 = mv_eu;
	fu1 = nmg_find_fu_of_eu( eu1 );
	fu = fu1;
	do
	{
		struct loopuse *lu;

		for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			struct edgeuse *eu;

			NMG_CK_LOOPUSE( lu );

			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
			{
				struct vertexuse *vu;
				vu = BU_LIST_FIRST( vertexuse , &lu->down_hd );
				if( NMG_INDEX_TEST_AND_SET( flags , vu->v_p ) )
				{
					bu_ptbl_reset( &faces );

					/* find all unique faces that intersect at this vertex (vu->v_p) */
					if( nmg_find_isect_faces( vu->v_p , &faces , &count , tol ) > 3 )
					{
						bu_log( "mg_move_edge_thru_pt: Cannot handle complex vertices\n" );
						bu_ptbl_free( &faces );
						bu_free( (char *)flags , "mg_move_edge_thru_pt: flags" );
						return( 1 );
					}

					if( nmg_simple_vertex_solve( vu->v_p , &faces, tol ) )
					{
						/* failed */
						bu_log( "nmg_move_edge_thru_pt: Could not solve simple vertex\n" );
						bu_ptbl_free( &faces );
						bu_free( (char *)flags , "mg_move_edge_thru_pt: flags" );
						return( 1 );
					}
				}
				continue;
			}

			for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				struct vertexuse *vu;

				vu = eu->vu_p;
				if( NMG_INDEX_TEST_AND_SET( flags , vu->v_p ) )
				{

					bu_ptbl_reset( &faces );

					/* find all unique faces that intersect at this vertex (vu->v_p) */
					if( nmg_find_isect_faces( vu->v_p , &faces , &count , tol ) > 3 )
					{
						bu_log( "mg_move_edge_thru_pt: Cannot handle complex vertices\n" );
						bu_ptbl_free( &faces );
						bu_free( (char *)flags , "mg_move_edge_thru_pt: flags" );
						return( 1 );
					}

					if( BU_PTBL_END( &faces ) == 1 &&
						(mv_eu->vu_p->v_p == vu->v_p ||
						 mv_eu->eumate_p->vu_p->v_p == vu->v_p) )
					{
						vect_t to_pt;
						vect_t mv_vect;
						vect_t eu_dir;

						/* special case for edge of a dangling face */

						/* just move vertex to new edge geometry */
						VSUB2( eu_dir, eu->eumate_p->vu_p->v_p->vg_p->coord, vu->v_p->vg_p->coord );
						VUNITIZE( eu_dir );
						VSUB2( to_pt, pt , vu->v_p->vg_p->coord );
						VJOIN1( mv_vect, to_pt, -VDOT( e_dir, to_pt ), e_dir );
						VADD2( vu->v_p->vg_p->coord, vu->v_p->vg_p->coord, mv_vect);
					}
					else
					{
						if( nmg_simple_vertex_solve( vu->v_p , &faces, tol ) )
						{
							/* failed */
							bu_log( "nmg_move_edge_thru_pt: Could not solve simple vertex\n" );
							bu_ptbl_free( &faces );
							bu_free( (char *)flags , "mg_move_edge_thru_pt: flags" );
							return( 1 );
						}
					}

					/* adjust edge geometry */
					if( eu->e_p != mv_eu->e_p )
					{
						if( !eu->g.magic_p )
							nmg_edge_g( eu );
						else
						{
							VMOVE( eu->g.lseg_p->e_pt, vu->v_p->vg_p->coord );
							VSUB2( eu->g.lseg_p->e_dir,
								eu->eumate_p->vu_p->v_p->vg_p->coord,
								vu->v_p->vg_p->coord );
						}
					}
				}
			}
		}

		/* move on to next radial face */
		eu1 = eu1->eumate_p->radial_p;
		fu = nmg_find_fu_of_eu( eu1 );
	}
	while( fu != fu1 && fu != fu1->fumate_p );

	bu_free( (char *)flags , "mg_move_edge_thru_pt: flags" );
	bu_ptbl_free( &faces );

	return( 0 );
}

/**	N M G _ V L I S T _ T O _ W I R E _ E D G E S
 *
 *	Convert a vlist to NMG wire edges
 *
 */
void
nmg_vlist_to_wire_edges(struct shell *s, const struct bu_list *vhead)
{
	const struct bn_vlist *vp;
	struct edgeuse *eu;
	struct vertex *v1,*v2;
	point_t pt1,pt2;

	NMG_CK_SHELL( s );
	NMG_CK_LIST( vhead );

	v1 = (struct vertex *)NULL;
	v2 = (struct vertex *)NULL;

	vp = BU_LIST_FIRST( bn_vlist , vhead );
	if( vp->nused < 2 )
		return;

	for( BU_LIST_FOR( vp , bn_vlist , vhead ) )
	{
		register int i;
		register int nused = vp->nused;
		vect_t edge_vec;

		for( i=0 ; i<nused ; i++ )
		{
			switch( vp->cmd[i] )
			{
				case BN_VLIST_LINE_MOVE:
				case BN_VLIST_POLY_MOVE:
					v1 = (struct vertex *)NULL;
					v2 = (struct vertex *)NULL;
					VMOVE( pt2 , vp->pt[i] );
					break;
				case BN_VLIST_LINE_DRAW:
				case BN_VLIST_POLY_DRAW:
					VSUB2( edge_vec , pt2 , vp->pt[i] );
					if( VNEAR_ZERO( edge_vec , SMALL_FASTF ) )
						break;
					VMOVE( pt1 , pt2 );
					v1 = v2;
					VMOVE( pt2 , vp->pt[i] );
					v2 = (struct vertex *)NULL;
					eu = nmg_me( v1 , v2 , s );
					v1 = eu->vu_p->v_p;
					v2 = eu->eumate_p->vu_p->v_p;
					nmg_vertex_gv( v2 , pt2 );
					if( !v1->vg_p )
						nmg_vertex_gv( v1 , pt1 );
					nmg_edge_g( eu );
					break;
				case BN_VLIST_POLY_START:
				case BN_VLIST_POLY_END:
					break;
			}
		}
	}
}

void
nmg_follow_free_edges_to_vertex(const struct vertex *vpa, const struct vertex *vpb, struct bu_ptbl *bad_verts, const struct shell *s, const struct edgeuse *eu, struct bu_ptbl *verts, int *found)
{
	struct vertexuse *vu;

	BU_CK_PTBL( bad_verts );
	NMG_CK_EDGEUSE( eu );
	NMG_CK_VERTEX( vpa );
	NMG_CK_VERTEX( vpb );
	if( s )
		NMG_CK_SHELL( s );

	NMG_CK_VERTEX( eu->eumate_p->vu_p->v_p );

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		bu_log( "nmg_follow_free_edges_to_vertex( vpa=x%x, vpb=x%x s=x%x, eu=x%x, found=%d )\n",
			vpa,vpb,s,eu,*found );
	}

	for( BU_LIST_FOR( vu , vertexuse , &eu->eumate_p->vu_p->v_p->vu_hd ) )
	{
		struct edgeuse *eu1;

		NMG_CK_VERTEXUSE( vu );

		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
			continue;

		if( s && (nmg_find_s_of_vu( vu ) != s ) )
			continue;

		eu1 = vu->up.eu_p;

		NMG_CK_EDGEUSE( eu1 );

		if( rt_g.NMG_debug & DEBUG_BASIC )
			bu_log( "\tchecking eu x%x: x%x ( %f %f %f )\n\t\tto x%x ( %f %f %f )\n", eu1,
				eu1->vu_p->v_p, V3ARGS( eu1->vu_p->v_p->vg_p->coord ),
				eu1->eumate_p->vu_p->v_p, V3ARGS( eu1->eumate_p->vu_p->v_p->vg_p->coord ) );

		/* stick to free edges */
		if( eu1->eumate_p != eu1->radial_p )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "\t\tnot a dangling edge\n" );
			continue;
		}

		/* don`t go back the way we came */
		if( eu1 == eu->eumate_p )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "\t\tback the way we came\n" );
			continue;
		}

		if( eu1->eumate_p->vu_p->v_p == vpb )
		{
			/* found it!!! */
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "\t\tfound goal\n" );
			bu_ptbl_ins( verts , (long *)vu->v_p );
			bu_ptbl_ins( verts , (long *)vpb );
			*found = 1;
		}
		else if( eu1->eumate_p->vu_p->v_p == vpa )
		{
			/* back where we started */
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "\t\tback at start\n" );
			continue;
		}
		else if( bu_ptbl_locate( bad_verts , (long *)eu1->eumate_p->vu_p->v_p ) != (-1))
		{
			/* this is the wrong way */
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "\t\tA bad vertex\n" );
			continue;
		}
		else if( bu_ptbl_locate( verts , (long *)eu1->eumate_p->vu_p->v_p ) != (-1))
		{
			/* This is a loop !!!! */
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "a loop\n" );
			continue;
		}
		else
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "\t\tinserting vertex x%x\n" , vu->v_p );
			bu_ptbl_ins( verts , (long *)vu->v_p );
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "\t\tCalling follow edges\n" );
			nmg_follow_free_edges_to_vertex( vpa , vpb , bad_verts , s , eu1 , verts , found );
			if( *found < 0 )
			{
				if( rt_g.NMG_debug & DEBUG_BASIC )
				{
					bu_log( "\t\treturn is %d\n" , *found );
					bu_log( "\t\t\tremove vertex x%x\n" , vu->v_p );
				}
				bu_ptbl_rm( verts , (long *)vu->v_p );
				*found = 0;
			}
		}
		if( *found )
			return;
	}

	*found = (-1);
}

static struct bu_ptbl *
nmg_find_path(const struct vertex *vpa, const struct vertex *vpb, struct bu_ptbl *bad_verts, const struct shell *s)
{
	int done;
	static struct bu_ptbl verts;
	struct vertexuse *vua;


	BU_CK_PTBL( bad_verts );
	NMG_CK_VERTEX( vpa );
	NMG_CK_VERTEX( vpb );

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		int i;

		bu_log( "nmg_find_path( vpa=x%x ( %f %f %f ), vpb=x%x ( %f %f %f )\n",
			vpa, V3ARGS( vpa->vg_p->coord ), vpb, V3ARGS( vpb->vg_p->coord ) );
		bu_log( "\t%d vertices to avoid\n" , BU_PTBL_END( bad_verts ) );
		for( i=0 ; i<BU_PTBL_END( bad_verts ) ; i++ )
		{
			struct vertex *vpbad;

			vpbad = (struct vertex *)BU_PTBL_GET( bad_verts , i );
			bu_log( "\tx%x ( %f %f %f )\n" , vpbad , V3ARGS( vpbad->vg_p->coord ) );
		}
	}

	bu_ptbl_init( &verts , 64, " &verts ");
	bu_ptbl_ins( &verts , (long *)vpa );

	for( BU_LIST_FOR( vua , vertexuse , &vpa->vu_hd ) )
	{
		struct edgeuse *eua;

		NMG_CK_VERTEXUSE( vua );

		if( *vua->up.magic_p != NMG_EDGEUSE_MAGIC )
			continue;

		if( s && (nmg_find_s_of_vu( vua ) != s) )
			continue;

		eua = vua->up.eu_p;

		NMG_CK_EDGEUSE( eua );

		if( rt_g.NMG_debug & DEBUG_BASIC )
			bu_log( "\tchecking eu x%x: x%x ( %f %f %f )\n\t\tto x%x ( %f %f %f )\n", eua,
				eua->vu_p->v_p, V3ARGS( eua->vu_p->v_p->vg_p->coord ),
				eua->eumate_p->vu_p->v_p, V3ARGS( eua->eumate_p->vu_p->v_p->vg_p->coord ) );

		if( eua->eumate_p != eua->radial_p )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "\t\tback the way we came!\n" );
			continue;
		}

		if( bu_ptbl_locate( bad_verts , (long *)eua->eumate_p->vu_p->v_p ) != (-1) )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "\t\tOne of the bad vertices!!\n" );
			continue;
		}

		if( eua->eumate_p->vu_p->v_p == vpb )
		{
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "\t\tfound goal!!\n" );
			bu_ptbl_ins( &verts , (long *)vpb );
			return( &verts );
		}

		done = 0;
		if( rt_g.NMG_debug & DEBUG_BASIC )
			bu_log( "\tCall follow edges\n" );
		nmg_follow_free_edges_to_vertex( vpa, vpb, bad_verts, s, eua, &verts, &done );

		if( done == 1 )
			break;

		bu_ptbl_reset( &verts );
		bu_ptbl_ins( &verts , (long *)vpa );
	}

	if( done != 1 )
		bu_ptbl_init( &verts , 64, " &verts ");

	return( &verts );
}

void
nmg_glue_face_in_shell(const struct faceuse *fu, struct shell *s, const struct bn_tol *tol)
{
	struct loopuse *lu;

	NMG_CK_FACEUSE( fu );
	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		struct edgeuse *eu;

		NMG_CK_LOOPUSE( lu );
		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			struct edgeuse *eu1;

			NMG_CK_EDGEUSE( eu );

			eu1 = nmg_findeu( eu->vu_p->v_p, eu->eumate_p->vu_p->v_p, s, eu, 1 );
			if( eu1 )
			{
				NMG_CK_EDGEUSE( eu1 );
				nmg_radial_join_eu( eu1, eu, tol );
			}
		}
	}
}

#if 0
static int
nmg_vert_is_normalward( v, vbase, norm )
struct vertex *v;
struct vertex *vbase;
vect_t norm;
{
	vect_t to_v;

	NMG_CK_VERTEX( v );
	NMG_CK_VERTEX( vbase );

	VSUB2( to_v, v->vg_p->coord, vbase->vg_p->coord );
	if( VDOT( to_v, norm ) > 0.0 )
		return( 1 );
	else
		return( 0 );
}


/* Join EU's running from v1 to v2 and from v2 to v3 */
static void
Join_eus( v1, v2, v3, tol )
struct vertex *v1,*v2,*v3;
const struct bn_tol *tol;
{
	struct edgeuse *eu1;
	struct edgeuse *eu2;

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		bu_log( "Join_eus:\n" );
		bu_log( "\tv1 = x%x ( %g %g %g )\n", v1, V3ARGS( v1->vg_p->coord ) );
		bu_log( "\tv2 = x%x ( %g %g %g )\n", v2, V3ARGS( v2->vg_p->coord ) );
		bu_log( "\tv3 = x%x ( %g %g %g )\n", v3, V3ARGS( v3->vg_p->coord ) );
	}

	eu1 = nmg_find_e( v1, v2, (struct shell *)NULL, (struct edge *)NULL );
	if( !eu1 )
	{
		if( rt_g.NMG_debug & DEBUG_BASIC )
			bu_log( "\tNo edge found from v1 to v2\n" );
	}
	else
	{
		NMG_CK_EDGEUSE( eu1 );

		eu2 = nmg_find_e( v1, v2, (struct shell *)NULL, eu1->e_p );
		while( eu2 )
		{
			NMG_CK_EDGEUSE( eu2 );

			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "Joining eus x%x and x%x\n" , eu1, eu2 );
			nmg_radial_join_eu( eu1, eu2, tol );

			eu2 = nmg_find_e( v1, v2, (struct shell *)NULL, eu1->e_p );
		}
	}

	eu1 = nmg_find_e( v2, v3, (struct shell *)NULL, (struct edge *)NULL );
	if( !eu1 )
	{
		if( rt_g.NMG_debug & DEBUG_BASIC )
			bu_log( "\tNo edge found from v2 to v3\n" );
	}
	else
	{
		NMG_CK_EDGEUSE( eu1 );

		eu2 = nmg_find_e( v2, v3, (struct shell *)NULL, eu1->e_p );
		while( eu2 )
		{
			NMG_CK_EDGEUSE( eu2 );
			if( rt_g.NMG_debug & DEBUG_BASIC )
				bu_log( "Joining eus x%x and x%x\n" , eu1, eu2 );
			nmg_radial_join_eu( eu1, eu2, tol );

			eu2 = nmg_find_e( v2, v3, (struct shell *)NULL, eu1->e_p );
		}
	}
}
#endif

static int
nmg_make_connect_faces(struct shell *dst, struct vertex *vpa, struct vertex *vpb, struct bu_ptbl *verts, const struct bn_tol *tol)
{
	int done=0;
	int i,j;
	int verts_in_face=0;
	struct vertex *face_verts[20];
	struct vertex *v;
	int max_vert_no=19;
	int made_face;
	int faces_made=0;
	fastf_t dist_to_a_sq,dist_to_b_sq;
	vect_t to_vpa,to_vpb;

	if( rt_g.NMG_debug & DEBUG_BASIC )
	{
		bu_log( "nmg_make_connect_faces( dst=x%x, vpa=x%x ( %f %f %f ), vpb=x%x ( %f %f %f )\n",
				dst, vpa, V3ARGS( vpa->vg_p->coord ), vpb, V3ARGS( vpb->vg_p->coord ) );
		for( i=0 ; i<BU_PTBL_END( verts ) ; i++ )
		{
			struct vertex *v;

			v = (struct vertex *)BU_PTBL_GET( verts , i );
			bu_log( "\tx%x ( %f %f %f )\n" , v , V3ARGS( v->vg_p->coord ) );
		}
	}

	NMG_CK_SHELL( dst );
	NMG_CK_VERTEX( vpa );
	NMG_CK_VERTEX( vpb );
	BU_CK_PTBL( verts );
	BN_CK_TOL( tol );

	if( BU_PTBL_END( verts ) < 1 )
	{
		bu_log( "nmg_make_connect_faces: no list of vertices from other shell\n" );
		return( 0 );
	}
	if( BU_PTBL_END( verts ) == 1 )
	{
		face_verts[0] = vpb;
		face_verts[1] = vpa;
		face_verts[2] = (struct vertex *)BU_PTBL_GET( verts , 0 );
		i = 0;
	}
	else
	{
		/* set up for first face */
		face_verts[0] = vpa;
		face_verts[1] = (struct vertex *)BU_PTBL_GET( verts , 0 );
		face_verts[2] = (struct vertex *)BU_PTBL_GET( verts , 1 );
		i = 1;
	}
	v = face_verts[2];
	verts_in_face = 3;

	VSUB2( to_vpa , vpa->vg_p->coord , v->vg_p->coord );
	VSUB2( to_vpb , vpb->vg_p->coord , v->vg_p->coord );

	dist_to_a_sq = MAGSQ( to_vpa );
	dist_to_b_sq = MAGSQ( to_vpb );

	while( 1 )
	{
		struct faceuse *new_fu;
		struct loopuse *lu;
		plane_t pl;
		fastf_t area;
		int still_collinear=0;

		made_face = 0;

		/* if the current points are all collinear, add another vertex */
		while( bn_3pts_collinear( face_verts[0]->vg_p->coord,
				face_verts[1]->vg_p->coord,
				face_verts[verts_in_face - 1]->vg_p->coord, tol) )
		{
			if( verts_in_face >= max_vert_no )
			{
				still_collinear = 1;
				break;
			}

			if( i+1 >= BU_PTBL_END( verts ) )
			{
				still_collinear = 1;
				break;
			}
			i++;
			face_verts[verts_in_face] = (struct vertex *)BU_PTBL_GET( verts , i );
			verts_in_face++;
		}

		if( !still_collinear )
		{
			/* make the new face */

			if( rt_g.NMG_debug & DEBUG_BASIC )
			{
				int debug_int;

				bu_log( "make face:\n" );
				for( debug_int=0 ; debug_int<verts_in_face ; debug_int++ )
					bu_log( "\tx%x ( %f %f %f )\n" , face_verts[debug_int],
						V3ARGS( face_verts[debug_int]->vg_p->coord ) );
			}

			new_fu = nmg_cface( dst , face_verts , verts_in_face );
			lu = BU_LIST_FIRST( loopuse , &new_fu->lu_hd );
			area = nmg_loop_plane_area( lu , pl );

			if( area <= 0.0 )
			{
				bu_log( "Bad lu:\n" );
				nmg_pr_lu_briefly( lu, " " );
				nmg_kfu( new_fu );
				rt_bomb( "nmg_make_connect_faces: Failed to calculate plane eqn\n" );
			}
			else
			{
				made_face = 1;
				faces_made++;

				nmg_face_g( new_fu , pl );
				nmg_loop_g( lu->l_p, tol );

				/* glue this face in */
				nmg_glue_face_in_shell( new_fu , dst , tol );
			}
		}
		else
			made_face = 0;

		/* If we are half way to the other end of the edge,
		 * switch from vpa to vpb for the basis of the faces.
		 * Need to make the "middle" face.
		 */
		if( dist_to_b_sq <= dist_to_a_sq && face_verts[0] == vpa )
		{
			face_verts[0] = vpb;
			face_verts[1] = vpa;
			face_verts[2] = v;
			verts_in_face = 3;
		}
		else
		{
			/* Get ready for next face and check if done */
			i++;

			if( i < BU_PTBL_END( verts ) )
			{
				v = (struct vertex *)BU_PTBL_GET( verts , i );
				NMG_CK_VERTEX( v );

				VSUB2( to_vpa , vpa->vg_p->coord , v->vg_p->coord );
				VSUB2( to_vpb , vpb->vg_p->coord , v->vg_p->coord );

				dist_to_a_sq = MAGSQ( to_vpa );
				dist_to_b_sq = MAGSQ( to_vpb );
				face_verts[1] = face_verts[verts_in_face-1];
				face_verts[2] = v;
				verts_in_face = 3;
			}
			else if( face_verts[0] == vpa )
			{
				if( done )
					break;

				/* make last face */
				face_verts[0] = vpb;
				face_verts[1] = vpa;
				face_verts[2] = face_verts[verts_in_face-1];
				verts_in_face = 3;
				done = 1;
			}
			else	/* we are done */
				break;
		}

	}

	if( !made_face )
	{
		int found;

		if( !faces_made )
		{
			/* put all the vertices on the list */

			/* check for vpa */
			found = 0;
			for( i=0 ; i<verts_in_face ; i++ )
			{
				if( face_verts[i] == vpa )
				{
					found = 1;
					break;
				}
			}
			if( !found )
			{
				if( verts_in_face < 19 )
				{
					face_verts[verts_in_face] = vpa;
					verts_in_face++;
				}
			}

			/* check for vpb */
			found = 0;
			for( i=0 ; i<verts_in_face ; i++ )
			{
				if( face_verts[i] == vpb )
				{
					found = 1;
					break;
				}
			}
			if( !found )
			{
				if( verts_in_face < 19 )
				{
					face_verts[verts_in_face] = vpb;
					verts_in_face++;
				}
			}

			/* check verts table */
			for( j=0 ; j<BU_PTBL_END( verts ) ; j++ )
			{
				struct vertex *v_tmp;

				v_tmp = (struct vertex *)BU_PTBL_GET( verts, j );
				found = 0;
				for( i=0 ; i<verts_in_face ; i++ )
				{
					if( face_verts[i] == v_tmp )
					{
						found = 1;
						break;
					}
				}
				if( !found )
				{
					if( verts_in_face < 19 )
					{
						face_verts[verts_in_face] = v_tmp;
						verts_in_face++;
					}
				}
			}
		}
#if 1
		return( 1 );
#else
		if( rt_g.NMG_debug & DEBUG_BASIC )
		{
			bu_log( "nmg_make_connect_faces: Looking for edges to split verts in face = %d\n", verts_in_face );
			for( i=0 ; i<verts_in_face ; i++ )
				bu_log( "\t x%x ( %g %g %g )\n", face_verts[i], V3ARGS( face_verts[i]->vg_p->coord ) );
		}

		for( i=0 ; i<verts_in_face-2 ; i++ )
		{
			for( j=i+1 ; j<verts_in_face-1 ; j++ )
			{
				for( k=j+1 ; k<verts_in_face ; k++ )
				{
					struct edgeuse *eu;

					if( rt_g.NMG_debug & DEBUG_BASIC )
					{
						bu_log( "Checking collinearity of:\n" );
						bu_log( "\t x%x ( %g %g %g )\n", face_verts[i], V3ARGS( face_verts[i]->vg_p->coord ) );
						bu_log( "\t x%x ( %g %g %g )\n", face_verts[j], V3ARGS( face_verts[j]->vg_p->coord ) );
						bu_log( "\t x%x ( %g %g %g )\n", face_verts[k], V3ARGS( face_verts[k]->vg_p->coord ) );
					}

					if( !bn_3pts_collinear( face_verts[i]->vg_p->coord,
								face_verts[j]->vg_p->coord,
								face_verts[k]->vg_p->coord, tol ) )
						continue;
					if( rt_g.NMG_debug & DEBUG_BASIC )
						bu_log( "\t They are collinear\n" );

					if( (eu = nmg_findeu( face_verts[i], face_verts[j], (struct shell *)NULL,
						(struct edgeuse *)NULL, 0 ) ) )
					{
						if( rt_g.NMG_debug & DEBUG_BASIC )
							bu_log( "\tFound eu between x%x and x%x\n", face_verts[i], face_verts[j] );

						if( !bn_dist_pt3_lseg3( &dist, pca,
							face_verts[i]->vg_p->coord,
							face_verts[j]->vg_p->coord,
							face_verts[k]->vg_p->coord, tol ) )
						{
							new_eu = nmg_esplit( face_verts[k], eu, 1 );
							Join_eus( face_verts[i], face_verts[k], face_verts[j], tol );
						}
						else if( rt_g.NMG_debug & DEBUG_BASIC )
							bu_log( "\t\tvertex x%x is outside eu\n" , face_verts[k] );
					}
					if( (eu = nmg_findeu( face_verts[i], face_verts[k], (struct shell *)NULL,
						(struct edgeuse *)NULL, 0 ) ) )
					{
						if( rt_g.NMG_debug & DEBUG_BASIC )
							bu_log( "\tFound eu between x%x and x%x\n", face_verts[i], face_verts[k] );

						if( !bn_dist_pt3_lseg3( &dist, pca,
							face_verts[i]->vg_p->coord,
							face_verts[k]->vg_p->coord,
							face_verts[j]->vg_p->coord, tol ) )
						{
							new_eu = nmg_esplit( face_verts[j], eu, 1 );
							Join_eus( face_verts[i], face_verts[j], face_verts[k], tol );
						}
						else if( rt_g.NMG_debug & DEBUG_BASIC )
							bu_log( "\t\tvertex x%x is outside eu\n" , face_verts[j] );
					}
					if( (eu = nmg_findeu( face_verts[j], face_verts[k], (struct shell *)NULL,
						(struct edgeuse *)NULL, 0 ) ) )
					{
						if( rt_g.NMG_debug & DEBUG_BASIC )
							bu_log( "\tFound eu between x%x and x%x\n", face_verts[j], face_verts[k] );

						if( !bn_dist_pt3_lseg3( &dist, pca,
							face_verts[j]->vg_p->coord,
							face_verts[k]->vg_p->coord,
							face_verts[i]->vg_p->coord, tol ) )
						{
							new_eu = nmg_esplit( face_verts[i], eu, 1 );
							Join_eus( face_verts[j], face_verts[i], face_verts[k], tol );
						}
						else if( rt_g.NMG_debug & DEBUG_BASIC )
							bu_log( "\t\tvertex x%x is outside eu\n" , face_verts[i] );
					}
				}
			}
		}
#endif
	}
	return( 0 );
}

/**	N M G _ O P E N _ S H E L L S _ C O N N E C T
 *
 *	Two open shells are connected along their free edges by building
 *	new faces.  The resluting closed shell is in "dst", and "src" shell
 *	is destroyed.  The "copy_tbl" is a translation table that provides
 *	a one-to-one translation between the vertices in the two shells, i.e.,
 *	NMG_INDEX_GETP(vertex, copy_tbl, v), where v is a pointer to a vertex
 *	in "dst" shell, provides a pointer to the corresponding vertex in "src" shell
 *
 *	returns:
 *		0 - All is well
 *		1 - failure
 */

/* 	structure for use by nmg_open_shells_connect */
struct dangle
{
	struct vertex *va,*vb;		/* vertices of a dangling edge in dst shell */
	struct vertex *v1,*v2;		/* corresponding vertices in src shell */
	struct bu_ptbl bad_verts;	/* list of vertices to avoid when finding path
					 * from v1 to v2 */
	int needs_edge_breaking;
};

int
nmg_open_shells_connect(struct shell *dst, struct shell *src, const long int **copy_tbl, const struct bn_tol *tol)
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct bu_ptbl faces;
	struct bu_ptbl dangles;
	int open_src,open_dst;
	int i,j,k;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_open_shells_connect( dst=x%x , src=x%x , copy_tbl=x%x)\n" , dst , src , copy_tbl );

	NMG_CK_SHELL( dst );
	NMG_CK_SHELL( src );
	BN_CK_TOL( tol );

	open_src = nmg_check_closed_shell( src , tol );
	open_dst = nmg_check_closed_shell( dst , tol );

	if( !open_dst && !open_src )
	{
		/* both shells are closed, just join them */
		nmg_js( dst, src, tol );
		return( 0 );
	}

	if( !open_dst )
	{
		bu_log( "nmg_open_shells_connect: destination shell is closed!\n" );
		return( 1 );
	}

	if( !open_src )
	{
		bu_log( "nmg_open_shells_connect: source shell is closed!\n" );
		return( 1 );
	}

	bu_ptbl_init( &dangles , 64, " &dangles ");

	/* find free edges in "dst" shell */
	for( BU_LIST_FOR( fu , faceuse , &dst->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation != OT_SAME )
			continue;
		for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );
			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
				continue;

			for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				NMG_CK_EDGEUSE( eu );
				if( eu->eumate_p == eu->radial_p )
				{
					struct dangle *dang;
					struct vertexuse *test_vu;
					struct vertex *vpbad1,*vpbada;

					/* found a dangling edge, put it in the list */
					dang = (struct dangle *)bu_malloc( sizeof( struct dangle ) ,
						"nmg_open_shells_connect: dang" );

					dang->needs_edge_breaking = 0;
					dang->va = eu->vu_p->v_p;
					NMG_CK_VERTEX( dang->va );
					dang->v1 = NMG_INDEX_GETP(vertex, copy_tbl, dang->va );
					NMG_CK_VERTEX( dang->v1 );
					dang->vb = eu->eumate_p->vu_p->v_p;
					NMG_CK_VERTEX( dang->vb );
					dang->v2 = NMG_INDEX_GETP(vertex, copy_tbl, dang->vb );
					NMG_CK_VERTEX( dang->v2 );

					bu_ptbl_init( &dang->bad_verts , 64, " &dang->bad_verts ");

					/* look for other dangling edges around this one
					 * to establish direction for nmg_find_path
					 * to look in
					 */

					for( BU_LIST_FOR( test_vu , vertexuse , &dang->va->vu_hd ) )
					{
						struct edgeuse *test_eu;

						if( *test_vu->up.magic_p != NMG_EDGEUSE_MAGIC )
							continue;

						test_eu = test_vu->up.eu_p;
						if( test_eu == eu )
							continue;

						if( test_eu->eumate_p->vu_p->v_p == dang->vb )
							continue;

						if( test_eu->eumate_p == test_eu->radial_p )
						{
							/* another dangling edge, don't want
							 * nmg_find_path to wander off
							 * in this direction
							 */
							vpbada = test_eu->eumate_p->vu_p->v_p;
							vpbad1 = NMG_INDEX_GETP( vertex , copy_tbl , vpbada );
							if( vpbad1 )
								bu_ptbl_ins( &dang->bad_verts , (long *)vpbad1 );
						}
					}

					for( BU_LIST_FOR( test_vu , vertexuse , &dang->vb->vu_hd ) )
					{
						struct edgeuse *test_eu;

						if( *test_vu->up.magic_p != NMG_EDGEUSE_MAGIC )
							continue;

						test_eu = test_vu->up.eu_p;
						if( test_eu == eu->eumate_p )
							continue;


						if( test_eu->eumate_p->vu_p->v_p == dang->va )
							continue;

						if( test_eu->eumate_p == test_eu->radial_p )
						{
							/* another dangling edge, don't want
							 * nmg_find_path to wander off
							 * in this direction
							 */
							vpbada = test_eu->eumate_p->vu_p->v_p;
							vpbad1 = NMG_INDEX_GETP( vertex , copy_tbl , vpbada );
							if( vpbad1 )
								bu_ptbl_ins( &dang->bad_verts , (long *)vpbad1 );
						}
					}
				bu_ptbl_ins( &dangles , (long *)dang );
				}
			}
		}
	}

	/* now build the faces to connect the dangling edges */
	for( i=0 ; i<BU_PTBL_END( &dangles ) ; i++ )
	{
		struct dangle *dang;
		struct bu_ptbl *verts;

		dang = (struct dangle *)BU_PTBL_GET( &dangles , i );

		/* find vertices between vp1 and vp2 */
		verts = nmg_find_path( dang->v1 , dang->v2 , &dang->bad_verts , src );

		/* make faces connecting the two shells */
		if( BU_PTBL_END( verts ) > 1 )
			dang->needs_edge_breaking = nmg_make_connect_faces( dst , dang->va , dang->vb , verts , tol );
		else
		{
			bu_log( "nmg_open_shells_connect: unable to make connecting face\n" );
			bu_log( "\tfor edge from x%x ( %f %f %f )\n\t\tto x%x ( %f %f %f )\n",
				dang->va, V3ARGS( dang->va->vg_p->coord ),
				dang->vb, V3ARGS( dang->vb->vg_p->coord ) );
		}

		bu_ptbl_free( verts );
		bu_ptbl_free( &dang->bad_verts );
	}

	while( BU_PTBL_END( &dangles ) )
	{
		struct vertex *verts[4];
		struct dangle *dang;
		struct edgeuse *eu;
		fastf_t dist;
		point_t pca;
		int done;

		dang = (struct dangle *)BU_PTBL_GET( &dangles , BU_PTBL_END( &dangles )-1 );
		if( dang->needs_edge_breaking )
		{

			dang = (struct dangle *)BU_PTBL_GET( &dangles, BU_PTBL_END( &dangles ) - 1 );
			verts[0] = dang->va;
			verts[1] = dang->vb;
			verts[2] = dang->v1;
			verts[3] = dang->v2;

			for( i=0 ; i<3 ; i++ )
			{
				for( j=i+1 ; j<4 ; j++ )
				{
					eu = nmg_findeu( verts[i], verts[j],
						0, 0, 1 );
					if( !eu )
						continue;

					for( k=0 ; k<4 ; k++ )
					{
						if( k == i || k == j )
							continue;
						if( bn_dist_pt3_lseg3( &dist, pca,
							verts[i]->vg_p->coord,
							verts[j]->vg_p->coord,
							verts[k]->vg_p->coord,  tol ) )
							continue;

						(void)nmg_esplit( verts[k], eu, 1 );
					}
				}
			}
			done = 0;
			while( !done )
			{
				done = 1;
				for( i=0 ; i<3 ; i++ )
				{
					for( j=i+1 ; j<4 ; j++ )
					{
						struct edgeuse *eu2;

						eu = nmg_findeu( verts[i], verts[j], 0, 0, 1 );
						if( !eu )
							continue;

						eu2 = nmg_findeu( verts[i], verts[j], 0, eu, 1 );
						if( !eu2 )
							continue;

						if( eu2->vu_p->v_p == eu->vu_p->v_p )
							eu2 = eu2->eumate_p;
						nmg_je( eu, eu2 );
						done = 0;
					}
				}
			}
		}
		bu_ptbl_rm( &dangles , (long *)dang );
		bu_free( (char *)dang , "nmg_open_shells_connect: dang" );
	}

	bu_ptbl_free( &dangles);
	nmg_js( dst , src , tol );

	/* now glue it all together */
	bu_ptbl_init( &faces , 64, " &faces ");
	for( BU_LIST_FOR( fu , faceuse , &dst->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
			bu_ptbl_ins( &faces , (long *)fu );
	}
	nmg_gluefaces( (struct faceuse **)BU_PTBL_BASEADDR( &faces) , BU_PTBL_END( &faces ), tol );
	bu_ptbl_free( &faces );

	return( 0 );
}

/**	N M G _ I N _ V E R T
 *
 *	Move vertex so it is at the intersection of the newly created faces
 *
 *	This routine is used by "nmg_extrude_shell" to move vertices. Each
 *	plane has already been moved a distance inward and
 *	the surface normals have been reversed.
 *
 *	if approximate is non-zero, then the coordinates of the new
 *	vertex may be calculated as the point with minimum distance
 *	to all the faces that intersect at the vertex for vertices
 *	where more than three faces intersect.
 *
 */
int
nmg_in_vert(struct vertex *new_v, const int approximate, const struct bn_tol *tol)
{
	struct bu_ptbl faces;
	int failed=0;
	int free_edges=0;
	int face_count;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_in_vert( new_v=x%x at ( %f %f %f) )\n" , new_v , V3ARGS( new_v->vg_p->coord ) );

	NMG_CK_VERTEX( new_v );
	BN_CK_TOL( tol );

	bu_ptbl_init( &faces , 64, " &faces ");

	/* find all unique faces that intersect at this vertex (new_v) */
	face_count = nmg_find_isect_faces( new_v , &faces , &free_edges , tol );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "\tnmg_in_vert: found %d faces at new_v, %d free_edges\n",
			face_count, free_edges );

	if( (face_count < 4 && !free_edges) || face_count < 3 )
	{
		if( nmg_simple_vertex_solve( new_v , &faces, tol ) )
		{
			failed = 1;
			bu_log( "Could not solve simple vertex\n" );
		}
	}
	else
	{
		if( nmg_complex_vertex_solve( new_v , &faces , free_edges , approximate , tol ) )
		{
			failed = 1;
			bu_log( "Could not solve complex vertex\n" );
		}
	}

	/* Free memory */
	bu_ptbl_free( &faces );

	return( failed );
}

/**		N M G _ M I R R O R _ M O D E L
 *
 *	mirror model across the y-axis
 *	this does not copy the model, it changes the
 *	model passed to it
 */

void
nmg_mirror_model(struct model *m)
{
	struct bu_ptbl vertices;
	struct nmgregion *r;
	int i;
	long *flags;

	NMG_CK_MODEL( m );

	/* mirror all vertices across the y axis */
	nmg_vertex_tabulate( &vertices , &m->magic );

	for( i=0 ; i<BU_PTBL_END( &vertices ) ; i++ )
	{
		struct vertex *v;

		v = (struct vertex *)BU_PTBL_GET( &vertices , i );
		NMG_CK_VERTEX( v );

		v->vg_p->coord[Y] = (-v->vg_p->coord[Y]);
	}
	(void)bu_ptbl_free( &vertices );

	/* adjust the direction of all the faces */
	flags = (long *)bu_calloc( m->maxindex , sizeof( long ) , "nmg_mirror_model: flags" );
	for( BU_LIST_FOR( r , nmgregion , &m->r_hd ) )
	{
		struct shell *s;

		for( BU_LIST_FOR( s , shell , &r->s_hd ) )
		{
			struct faceuse *fu;

			for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
			{
				int orientation;

				if( NMG_INDEX_TEST_AND_SET( flags , fu ) )
				{
					/* switch orientations of all faceuses */
					orientation = fu->orientation;
					fu->orientation = fu->fumate_p->orientation;
					fu->fumate_p->orientation = orientation;
					NMG_INDEX_SET( flags , fu->fumate_p );

					if( NMG_INDEX_TEST_AND_SET( flags , fu->f_p->g.plane_p ) )
					{
						/* correct normal vector */
						fu->f_p->g.plane_p->N[Y] = (-fu->f_p->g.plane_p->N[Y]);
					}
				}
			}
		}
	}

	bu_free( (char *)flags , "nmg_mirror_model: flags " );
}

int
nmg_kill_cracks(struct shell *s)
{
	struct faceuse *fu;
	struct faceuse *fu_next;
	int empty_shell=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_kill_cracks( s=%x )\n" , s );

	NMG_CK_SHELL( s );
#if 1
	/* Loops may be inadvertently connected with a crack,
	 * this code is to dissconnect them and kill the connecting crack.
	 * Look for cracks that are two EU's from one loop that
	 * share the same edge, but are not consectutive in the loop.
	 * This will require a split_lu to handle.
	 */
	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		struct loopuse *lu;
		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			continue;

		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		{
			struct edgeuse *eu;

			NMG_CK_LOOPUSE( lu );

			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
				continue;
again:
			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				struct edgeuse *eu2;

				NMG_CK_EDGEUSE( eu );

				for( BU_LIST_FOR( eu2, edgeuse, &lu->down_hd ) )
				{
					struct loopuse *new_lu1;
					struct loopuse *new_lu2;

					if( eu == eu2 )
						continue;

					/* check if eu and eu2 share an edge */
					if( eu->e_p != eu2->e_p )
						continue;

					if( eu2 == BU_LIST_PNEXT_CIRC( edgeuse, &eu->l ) )
					{
						/* this is just a regular crack, catch it later */
						continue;
					}
					if( eu2 == BU_LIST_PPREV_CIRC( edgeuse, &eu->l ) )
					{
						/* this is just a regular crack, catch it later */
						continue;
					}

					if( eu->vu_p->v_p == eu2->vu_p->v_p )
					{
						/* This must be part of an accordion, catch it later */
						continue;
					}

					if( eu->eumate_p->vu_p->v_p != eu2->vu_p->v_p )
					{
						/* this is a problem!!! */
						bu_log( "nmg_kill_cracks: found a strange crack at eu1=x%x, eu2=x%x\n", eu, eu2 );
						nmg_pr_lu_briefly( lu, "" );
						rt_bomb( "nmg_kill_cracks: found a strange crack\n" );
					}

					new_lu1 = nmg_split_lu_at_vu( lu, eu->vu_p );
					new_lu2 = nmg_split_lu_at_vu( new_lu1, eu2->vu_p );

					nmg_klu( new_lu2 );

					nmg_lu_reorient( lu );
					nmg_lu_reorient( new_lu1 );

					goto again;
				}
			}
		}
	}
#endif
	fu = BU_LIST_FIRST( faceuse, &s->fu_hd );
	while( BU_LIST_NOT_HEAD( fu, &s->fu_hd ) )
	{
		struct loopuse *lu;
		struct loopuse *lu_next;
		int empty_face=0;

		NMG_CK_FACEUSE( fu );

		fu_next = BU_LIST_PNEXT( faceuse, &fu->l );
		while( BU_LIST_NOT_HEAD( fu_next, &s->fu_hd )
			&& fu_next == fu->fumate_p )
				fu_next = BU_LIST_PNEXT( faceuse, &fu_next->l );

		lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
		while( BU_LIST_NOT_HEAD( lu, &fu->lu_hd ) )
		{
			struct edgeuse *eu;
			struct edgeuse *eu_next;
			int empty_loop=0;

			NMG_CK_LOOPUSE( lu );

			lu_next = BU_LIST_PNEXT( loopuse, &lu->l );

			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			{
				lu = lu_next;
				continue;
			}

crack_top:
			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				NMG_CK_EDGEUSE( eu );

				eu_next = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				NMG_CK_EDGEUSE( eu_next );
				/* check if eu and eu_next form a jaunt */
				if( eu->vu_p->v_p != eu_next->eumate_p->vu_p->v_p )
					continue;

				if( nmg_keu( eu ) )
					empty_loop = 1;
				else if( nmg_keu( eu_next ) )
					empty_loop = 1;

				if( empty_loop )
					break;

				goto crack_top;
			}
			if( empty_loop )
			{
				if( nmg_klu( lu ) )
				{
					empty_face = 1;
					break;
				}
			}
			lu = lu_next;
		}
		if( empty_face )
		{
			if( nmg_kfu( fu ) )
			{
				empty_shell = 1;
				break;
			}
		}
		fu = fu_next;
	}

	if( empty_shell )
	{
		if( rt_g.NMG_debug & DEBUG_BASIC )
			bu_log( "nmg_kill_cracks: ret = %d\n" , empty_shell );
		return( empty_shell );
	}


	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_kill_cracks: ret = %d\n" , empty_shell );

	return( empty_shell );
}

int
nmg_kill_zero_length_edgeuses(struct model *m)
{
	struct nmgregion *r;
	int empty_model=0;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_kill_zero_length_edgeuses( m=%x )\n", m );

	NMG_CK_MODEL( m );

	r = BU_LIST_FIRST( nmgregion, &m->r_hd );
	while( BU_LIST_NOT_HEAD( r, &m->r_hd ) )
	{
		struct nmgregion *next_r;
		struct shell *s;
		int empty_region=0;

		NMG_CK_REGION( r );

		next_r = BU_LIST_PNEXT( nmgregion, &r->l );

		s = BU_LIST_FIRST( shell, &r->s_hd );
		while( BU_LIST_NOT_HEAD( s, &r->s_hd ) )
		{
			struct shell *next_s;
			struct faceuse *fu;
			int empty_shell=0;

			NMG_CK_SHELL( s );

			next_s = BU_LIST_PNEXT( shell, &s->l );

			fu = BU_LIST_FIRST( faceuse, &s->fu_hd );
			while( BU_LIST_NOT_HEAD( fu, &s->fu_hd ) )
			{
				struct loopuse *lu;
				struct faceuse *next_fu;
				int empty_fu=0;

				NMG_CK_FACEUSE( fu );

				next_fu = BU_LIST_PNEXT( faceuse, &fu->l );
				if( next_fu == fu->fumate_p )
					next_fu = BU_LIST_PNEXT( faceuse, &next_fu->l );

				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
				while( BU_LIST_NOT_HEAD( lu, &fu->lu_hd ) )
				{
					struct edgeuse *eu;
					struct loopuse *next_lu;
					int empty_loop=0;

					NMG_CK_LOOPUSE( lu );

					next_lu = BU_LIST_PNEXT( loopuse, &lu->l );

					if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
					{
						lu = next_lu;
						continue;
					}

					eu = BU_LIST_FIRST( edgeuse, &lu->down_hd );
					while( BU_LIST_NOT_HEAD( eu, &lu->down_hd ) )
					{
						struct edgeuse *next_eu;
						struct edgeuse *next_eu_circ;

						NMG_CK_EDGEUSE( eu );

						next_eu = BU_LIST_PNEXT( edgeuse, &eu->l );
						next_eu_circ = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );

						if( eu->vu_p->v_p == next_eu_circ->vu_p->v_p )
						{
							/* found a zero length edgeuse */
							if( nmg_keu( eu ) )
								empty_loop = 1;
						}
						eu = next_eu;
					}
					if( empty_loop )
					{
						if( nmg_klu( lu ) )
							empty_fu = 1;
					}
					lu = next_lu;
				}
				if( empty_fu )
				{
					if( nmg_kfu( fu ) )
						empty_shell = 1;
				}
				fu = next_fu;
			}
			if( empty_shell )
			{
				if( nmg_ks( s ) )
					empty_region = 1;
			}
			s = next_s;
		}
		if( empty_region )
		{
			if( nmg_kr( r ) )
				empty_model = 1;
		}
		r = next_r;
	}

	if( empty_model )
		return( 1 );
	else
		return( 0 );
}

/**	N M G _ M A K E _ F A C E S _ W I T H I N _ T O L
 *
 * Check all vertices on faces of specified shell. Any face containing
 * vertices more than tol->dist off the plane of the face will be
 * triangulated and broken into seperate faces
 */
void
nmg_make_faces_within_tol(struct shell *s, const struct bn_tol *tol)
{
	struct bu_ptbl faceuses;
	struct faceuse *fu;
	int i;

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	bu_ptbl_init( &faceuses, 64, " &faceuses");

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			continue;

		bu_ptbl_ins( &faceuses, (long *)fu );
	}

	for (i = 0; i < BU_PTBL_END( &faceuses ); i++)
	{
		fu = (struct faceuse *)BU_PTBL_GET( &faceuses, i );

		/* check if all the vertices for this face lie on the plane */
		if( nmg_ck_fu_verts( fu, fu->f_p, tol ) )
		{
			plane_t pl;

			/* Need to triangulate this face */
			nmg_triangulate_fu( fu, tol );

			/* split each triangular loop into its own face */
			(void)nmg_split_loops_into_faces( &fu->l.magic, tol );

			if( nmg_calc_face_plane( fu, pl ) )
			{
				bu_log( "nmg_make_faces_within_tol(): nmg_calc_face_plane() failed\n" );
				rt_bomb( "nmg_make_faces_within_tol(): nmg_calc_face_plane() failed" );
			}
			nmg_face_new_g( fu, pl );
		}
	}

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		plane_t pl;

		if( fu->orientation != OT_SAME )
			continue;

		if( bu_ptbl_locate( &faceuses, (long *)fu ) != (-1) )
			continue;

		if( nmg_calc_face_plane( fu, pl ) )
		{
			bu_log( "nmg_make_faces_within_tol(): nmg_calc_face_plane() failed\n" );
			rt_bomb( "nmg_make_faces_within_tol(): nmg_calc_face_plane() failed" );
		}

		nmg_face_new_g( fu, pl );
	}

	bu_ptbl_free( &faceuses);
}

void
nmg_intersect_loops_self(struct shell *s, const struct bn_tol *tol)
{
	struct faceuse *fu;
	struct bu_ptbl edgeuses;

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	bu_ptbl_init( &edgeuses, 64, " &edgeuses");

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		struct loopuse *lu;

		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			continue;

		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		{
			struct edgeuse *eu;
			int i,j;

			NMG_CK_LOOPUSE( lu );

			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
				continue;
top:
			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				bu_ptbl_ins( &edgeuses, (long *)eu );

			for( i=0 ; i<BU_PTBL_END( &edgeuses ); i++ )
			{
				vect_t eu_dir;

				eu = (struct edgeuse *)BU_PTBL_GET( &edgeuses, i );
				VSUB2( eu_dir, eu->eumate_p->vu_p->v_p->vg_p->coord, eu->vu_p->v_p->vg_p->coord );

				for( j=i+1 ; j<BU_PTBL_END( &edgeuses ) ; j++ )
				{
					struct edgeuse *eu2;
					struct edgeuse *new_eu=(struct edgeuse *)NULL;
					struct vertex *v;
					vect_t eu2_dir;
					point_t int_pt;
					fastf_t dist[2];
					int code;

					eu2 = (struct edgeuse *)BU_PTBL_GET( &edgeuses, j );
					VSUB2( eu2_dir, eu2->eumate_p->vu_p->v_p->vg_p->coord, eu2->vu_p->v_p->vg_p->coord );

					code = bn_isect_lseg3_lseg3( dist, eu->vu_p->v_p->vg_p->coord, eu_dir,
						eu2->vu_p->v_p->vg_p->coord, eu2_dir, tol );

					if( code < 0 )	/* no intersection */
						continue;

					if( code == 0 )
					{
						if( dist[0] > 0.0 && dist[0] != 1.0 )
						{
							VJOIN1( int_pt, eu->vu_p->v_p->vg_p->coord, dist[0], eu_dir );
							v = (struct vertex *)NULL;
							new_eu = nmg_ebreaker( v, eu, tol );
							nmg_vertex_gv( new_eu->vu_p->v_p, int_pt );
						}
						if( dist[1] > 0.0 && dist[1] != 1.0 )
						{
							VJOIN1( int_pt, eu->vu_p->v_p->vg_p->coord, dist[1], eu_dir );
							v = (struct vertex *)NULL;
							new_eu = nmg_ebreaker( v, eu, tol );
							nmg_vertex_gv( new_eu->vu_p->v_p, int_pt );
						}
					}
					else
					{
						if( dist[0] != 0.0 && dist[0] != 1.0 )
						{
							VJOIN1( int_pt, eu->vu_p->v_p->vg_p->coord, dist[0], eu_dir );
							v = (struct vertex *)NULL;
							new_eu = nmg_ebreaker( v, eu, tol );
							nmg_vertex_gv( new_eu->vu_p->v_p, int_pt );
						}
						if( dist[1] != 0.0 && dist[1] != 1.0 )
						{
							VJOIN1( int_pt, eu2->vu_p->v_p->vg_p->coord, dist[1], eu2_dir );
							v = (struct vertex *)NULL;
							new_eu = nmg_ebreaker( v, eu2, tol );
							nmg_vertex_gv( new_eu->vu_p->v_p, int_pt );
						}

					}
					if( new_eu )
					{
						bu_ptbl_reset( &edgeuses);
						goto top;
					}
				}
			}

			bu_ptbl_reset( &edgeuses);
		}
	}

	bu_ptbl_free( &edgeuses);
}

/**	R T _ J O I N _ C N U R B S
 *
 * Join a list of cnurb structs into a single cnurb.
 * The curves must have matching endpoints, otherwise
 * nothing is done and (struct cnurb *)NULL is returned.
 *
 * Returns:
 *	A single cnurb structure that joins all the
 *	cnurbs on the list.
 */
struct edge_g_cnurb *
rt_join_cnurbs(struct bu_list *crv_head)
{
	struct edge_g_cnurb *crv,*next_crv;
	struct edge_g_cnurb *new_crv=(struct edge_g_cnurb *)NULL;
	fastf_t knot_delta=0.0;
	fastf_t last_knot=0.0;
	int ncoords;
	int knot_index=(-1);
	int max_order=0;
	int ctl_points=1;
	int ctl_index=(-1);
	int knot_length=0;
	int pt_type=0;
	int curve_count=0;
	int i,j;

	BU_CK_LIST_HEAD( crv_head );

	/* Check that all curves are the same pt_type and
	 * have mutliplicity equal to order at endpoints.
	 */
	for( BU_LIST_FOR( crv, edge_g_cnurb, crv_head ) )
	{
		curve_count++;
		rt_nurb_c_print( crv);
		if( crv->order > max_order )
			max_order = crv->order;

		i = 0;
		while( crv->k.knots[++i] == crv->k.knots[0] );
		if( i != crv->order )
		{
			bu_log( "Curve does not have multiplicity equal to order at start:\n" );
			rt_nurb_c_print( crv);
			return( new_crv );
		}

		i = crv->k.k_size - 1;
		while( crv->k.knots[--i] == crv->k.knots[crv->k.k_size - 1] );
		if( crv->k.k_size - i - 1 != crv->order )
		{
			bu_log( "Curve does not have multiplicity equal to order at end:\n" );
			rt_nurb_c_print( crv);
			return( new_crv );
		}

		if( pt_type == 0 )
			pt_type = crv->pt_type;
		else
		{
			if( crv->pt_type != pt_type )
			{
				bu_log( "rt_join_cnurbs: curves are not the same pt_type (%d vs %d)\n",
					pt_type, crv->pt_type );
				return( new_crv );
			}
		}
	}

	/* If there is only one entry on list, just return it */
	if( curve_count < 2 )
	{
		crv = BU_LIST_FIRST( edge_g_cnurb, crv_head );
		BU_LIST_DEQUEUE( &crv->l );
		return( crv );
	}

	/* Raise each curve to order max_order */
	for( BU_LIST_FOR( crv, edge_g_cnurb, crv_head ) )
	{
		if( crv->order == max_order )
			continue;

		/* This curve must have its order raised to max_order */
		/* XXXX Need a routine to raise order of a curve */
		rt_bomb( "rt_join_cnurbs: Need to raise order of curve\n" );
	}

	/* Check that endponts match */
	crv = BU_LIST_FIRST( edge_g_cnurb, crv_head );
	ncoords = RT_NURB_EXTRACT_COORDS( crv->pt_type );
	next_crv = BU_LIST_NEXT( edge_g_cnurb, &crv->l );
	while( BU_LIST_NOT_HEAD( &next_crv->l, crv_head ) )
	{
		int endpoints_equal;

		endpoints_equal = 1;

		for( i=0 ; i<ncoords ; i++ )
		{
			/* It is tempting to use a tolerance here, but these coordinates may be
			 * x/y/z or x/y or u/v, ...
			 */
			if( crv->ctl_points[(crv->c_size-1)*ncoords+i] == next_crv->ctl_points[i] )
				continue;
			else
			{
				endpoints_equal = 0;
				break;
			}
		}
		if( endpoints_equal )
		{
			/* Nothing needed here, go to next curve */
			crv = next_crv;
			next_crv = BU_LIST_NEXT( edge_g_cnurb, &crv->l );
			continue;
		}

		bu_log( "rt_join_cnurbs: Curve endpoints do not match:\n" );
		rt_nurb_c_print( crv );
		rt_nurb_c_print( next_crv );
		return( new_crv );
	}

	/* Get new knot size and polygon size */
	crv = BU_LIST_FIRST( edge_g_cnurb, crv_head );
	knot_length = crv->order;
	for( BU_LIST_FOR( crv, edge_g_cnurb, crv_head ) )
	{
		ctl_points += (crv->c_size - 1);
		knot_length += (crv->k.k_size - crv->order - 1);

	}
	knot_length++;

	new_crv = rt_nurb_new_cnurb( max_order, knot_length, ctl_points, pt_type );

	crv = BU_LIST_FIRST( edge_g_cnurb, crv_head );

	/* copy first knot values from first curve */
	for( i=0 ; i<crv->order ; i++ )
		new_crv->k.knots[++knot_index] = crv->k.knots[i];

	/* copy first control point from first curve */
	for( j=0 ; j<ncoords ; j++ )
		new_crv->ctl_points[j] = crv->ctl_points[j];

	ctl_index = 0;
	knot_delta = new_crv->k.knots[knot_index];

	/* copy each curve to the new combined curve */
	for( BU_LIST_FOR( crv, edge_g_cnurb, crv_head ) )
	{
		/* copy interior knots */
		for( i=crv->order ; i<crv->k.k_size-crv->order ; i++ )
		{
			new_crv->k.knots[++knot_index] = crv->k.knots[i] + knot_delta;
		}

		/* copy endpoint knots (reduce multiplicity by one) */
		for( i=0 ; i<crv->order-1 ; i++ )
			new_crv->k.knots[++knot_index] = crv->k.knots[crv->k.k_size-1] + knot_delta;

		knot_delta += crv->k.knots[crv->k.k_size-1];
		last_knot = new_crv->k.knots[knot_index];

		/* copy control points (skip duplicate initial point) */
		for( i=1 ; i<crv->c_size ; i++ )
		{
			ctl_index++;
			VMOVEN( &new_crv->ctl_points[ctl_index*ncoords], &crv->ctl_points[i*ncoords], ncoords );
		}
	}
	new_crv->k.knots[++knot_index] = last_knot;

	return( new_crv );
}

/**	R T _ A R C 2 D _ T O _ C N U R B
 *
 * Convert a 2D arc to a NURB curve.
 *	point_type indicates what type of CNURB is requested.
 *	The arc start, end, and center must be at the same Z
 *	coordinate value if point_type is RT_NURB_PT_XYZ. For values of
 *	point_type of RT_NURB_PT_XY or RT_NURB_PT_UV, the Z coordinate
 *	is ignored. (Note that point_type must be one of the point types
 *	defined in nurb.h). The arc is constructed counter-clockwise
 *	(as viewed from the +Z direction).
 */
struct edge_g_cnurb *
rt_arc2d_to_cnurb(fastf_t *i_center, fastf_t *i_start, fastf_t *i_end, int point_type, const struct bn_tol *tol)
{
	struct edge_g_cnurb *crv;
	struct bu_list crv_head;
	point_t center;
	point_t start;
	point_t end;
	double angle;
	double angles[3];
	double radius;
	double tmp_radius;
	vect_t v1, v2;
	vect_t ref1, ref2;
	vect_t norm;
	point_t start1;
	point_t end1;
	int nsegs;
	int pt_type;
	int ncoords = 0;
	int i;

	BN_CK_TOL( tol );

	VMOVE( start, i_start )
	VMOVE( center, i_center )
	VMOVE( end, i_end )
	switch( point_type )
	{
		case RT_NURB_PT_XY:
		case RT_NURB_PT_UV:
			ncoords = 3;
			start[Z] = 0.0;
			center[Z] = 0.0;
			end[Z] = 0.0;
			break;
		case RT_NURB_PT_XYZ:
		case RT_NURB_PT_DATA:
		case RT_NURB_PT_PROJ:
			ncoords = 4;
			break;
	}

	if( point_type == RT_NURB_PT_XYZ )
	{
		/* check for points at same Z-coordinate value */
		if( center[Z] - start[Z] > tol->dist )
		{
			bu_log( "rt_arc2d_to_cnurb: center and start points not at same Z value (%g vs %g)\n",
					center[Z], start[Z] );
			return( (struct edge_g_cnurb *)NULL );
		}

		if( end[Z] - start[Z] > tol->dist )
		{
			bu_log( "rt_arc2d_to_cnurb: end and start points not at same Z value (%g vs %g)\n",
					end[Z], start[Z] );
			return( (struct edge_g_cnurb *)NULL );
		}

		if( end[Z] - center[Z] > tol->dist )
		{
			bu_log( "rt_arc2d_to_cnurb: end and center points not at same Z value (%g vs %g)\n",
					end[Z], center[Z] );
			return( (struct edge_g_cnurb *)NULL );
		}
	}

	/* point type is point_type with weighting factor (rational) */
	pt_type = RT_NURB_MAKE_PT_TYPE( ncoords, point_type, RT_NURB_PT_RATIONAL );

	/* calculate radius twice */
	if( ncoords == 4 )
	{
		VSUB2( v1, start, center );
		radius = MAGNITUDE( v1 );
		VSUB2( v2, end, center );
		tmp_radius = MAGNITUDE( v2 );
	}
	else
	{
		radius = sqrt( (start[X] - center[X])*(start[X] - center[X]) +
			(start[Y] - center[Y])*(start[Y] - center[Y]) );
		tmp_radius = sqrt( (end[X] - center[X])*(end[X] - center[X]) +
			(end[Y] - center[Y])*(end[Y] - center[Y]) );
	}

	/* make sure radii are consistent */
	if( !NEAR_ZERO( radius - tmp_radius, tol->dist ) )
	{
		bu_log( "rt_arc2d_to_cnurb: distances from center to start and center to end are different\n" );
		bu_log( "                        (%g and %g)\n", radius, tmp_radius );
		return( (struct edge_g_cnurb *)NULL );
	}

	/* construct coord system ref1,ref2,norm */
	VSET( norm, 0.0, 0.0, 1.0 );
	VMOVE( ref1, v1 );
	VSCALE( ref1, ref1, 1.0/radius );
	VCROSS( ref2, norm, ref1 );

	/* calculate angle of arc */
	angle = atan2( VDOT( v2, ref2 ), VDOT( v2, ref1 ) );
	if( angle <= 0.0 )
		angle += 2.0*bn_pi;

	if( angle < 150.0*bn_pi/180.0 ) /* angle is reasonable to do in one segment */
	{
		fastf_t dist1, dist2;
		vect_t t1, t2;
		int ret_val;

		/* second control point is intersection of tangent lines */
		VMOVE( t1, ref2 );
		if( VDOT( t1, v2 ) > 0.0 )
			VREVERSE( t1, t1 );
		VCROSS( t2, v2, norm );
		VUNITIZE( t2 );
		if( VDOT( t2, v1 ) > 0.0 )
			VREVERSE( t2, t2 );
		if( (ret_val=bn_isect_line3_line3( &dist1, &dist2, start, t1, end, t2, tol )) < 1 )
		{
			bu_log( "rt_arc2d_to_cnurb: Cannot calculate second control point\n" );
			bu_log( "                   bn_isect_line3_line3 returns %d\n" , ret_val );
			return( (struct edge_g_cnurb *)NULL );
		}

		/* Get memory for this curve (order=3, 6 knot values, 3 control points) */
		crv = rt_nurb_new_cnurb( 3, 6, 3, pt_type );

		/* Set knot values */
		for( i=0 ; i<3 ; i++ )
			crv->k.knots[i] = 0.0;
		for( i=3 ; i<6 ; i++ )
			crv->k.knots[i] = 1.0;

		/* First and third control points are start and end */
		VMOVEN( crv->ctl_points, start, ncoords-1 )
		VMOVEN( &crv->ctl_points[8], end, ncoords-1 )

		/* weights are 1.0 for the endpoints */
		crv->ctl_points[ncoords-1] = 1.0;
		crv->ctl_points[3*ncoords-1] = 1.0;

		/* second control point is as calculated above */
		VJOIN1N( &crv->ctl_points[ncoords], start, dist1, t1, ncoords-1 )

		/* weight for second control point is cosine of half the arc angle */
		crv->ctl_points[2*ncoords-1] = cos( angle/2.0 );

		/* scale middle point by its weight */
		VSCALEN( &crv->ctl_points[ncoords], &crv->ctl_points[ncoords], crv->ctl_points[2*ncoords-1], ncoords-1 )

		return( crv );
	}

	/* Angle is too great for one segment.
	 * Make up to three segments and join them.
	 */

	if( angle < 1.5*bn_pi )
	{
		/* do it in two segments */
		nsegs = 2;
		angles[0] = angle/2.0;
		angles[1] = angle;
	}
	else
	{
		/* use three segments */
		nsegs = 3;
		angles[0] = angle/3.0;
		angles[1] = 2.0*angles[0];
		angles[2] = angle;
	}

	/* initialize bu_list structure to hold list of curves */
	BU_LIST_INIT( &crv_head );

	/* Make each arc segment */
	VMOVE( end1, start );
	for( i=0 ; i<nsegs ; i++ )
	{
		VMOVE( start1, end1 );
		if( i == nsegs-1 )
			VMOVE( end1, end )
		else
			VJOIN2( end1, center, radius*cos( angles[i] ), ref1, radius*sin( angles[i] ), ref2 )

		crv = rt_arc2d_to_cnurb( center, start1, end1, point_type, tol );
		BU_LIST_INSERT( &crv_head, &crv->l );
	}

	/* join the arc segments into one edge_g_cnurb */
	crv = rt_join_cnurbs( &crv_head );

	return( crv );
}

int
nmg_break_edge_at_verts(struct edge *e, struct bu_ptbl *verts, const struct bn_tol *tol)
{
	int break_count=0;
	int j;
	struct vertex *va,*vb;
	struct bu_ptbl edges;

	BN_CK_TOL( tol );
	NMG_CK_EDGE( e );
	BU_CK_PTBL( verts );

	bu_ptbl_init( &edges, 64, " &edges");
	bu_ptbl_ins( &edges, (long *)e );

start:

	while( BU_PTBL_END( &edges ) )
	{
		struct edge *e1;
		struct edgeuse *eu;

		e1 = (struct edge *)BU_PTBL_GET( &edges, BU_PTBL_END( &edges )-1 );
		NMG_CK_EDGE( e1 );
		eu = e1->eu_p;
		NMG_CK_EDGEUSE( eu );

		va = eu->vu_p->v_p;
		NMG_CK_VERTEX( va );
		vb = eu->eumate_p->vu_p->v_p;
		NMG_CK_VERTEX( vb );

		for( j=0 ; j<BU_PTBL_END( verts ) ; j++ )
		{
			struct edgeuse *eu_new;
			struct vertex *v;
			fastf_t dist;
			point_t pca;

			v = (struct vertex *)BU_PTBL_GET( verts, j );
			NMG_CK_VERTEX( v );

			if( v == va || v == vb )
				continue;

			if( bn_dist_pt3_lseg3( &dist, pca, va->vg_p->coord, vb->vg_p->coord, v->vg_p->coord, tol ) )
				continue;

			eu_new = nmg_esplit( v, eu, 1 );
			break_count++;

			bu_ptbl_rm( &edges, (long *)e1 );
			bu_ptbl_ins( &edges, (long *)eu->e_p );
			bu_ptbl_ins( &edges, (long *)eu_new->e_p );

			goto start;
		}
		bu_ptbl_rm( &edges, (long *)e1 );
	}
	bu_ptbl_free( &edges);
	return( break_count );
}

int
nmg_break_edges(long int *magic_p, const struct bn_tol *tol)
{
	struct bu_ptbl edges;
	struct bu_ptbl verts;
	int i;
	int break_count=0;

	BN_CK_TOL( tol );

	nmg_edge_tabulate( &edges, magic_p );
	nmg_vertex_tabulate( &verts, magic_p );

	for( i=0 ; i<BU_PTBL_END( &edges ); i++ )
	{
		struct edge *e;
		int edge_break_count=0;

		e = (struct edge *)BU_PTBL_GET( &edges, i );
		NMG_CK_EDGE( e );
		edge_break_count = nmg_break_edge_at_verts( e, &verts, tol );
		break_count += edge_break_count;
	}
	bu_ptbl_free( &edges);
	bu_ptbl_free( &verts);

	return( break_count );
}


static int
Shell_is_arb(struct shell *s, struct bu_ptbl *tab)
{
	struct faceuse *fu;
	struct face *f;
	int four_verts=0;
	int three_verts=0;
	int face_count=0;
	int loop_count;

	NMG_CK_SHELL( s );

	nmg_vertex_tabulate( tab, &s->l.magic );

	if( BU_PTBL_END( tab ) > 8 || BU_PTBL_END( tab ) < 4 )
		goto not_arb;

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		struct loopuse *lu;
		vect_t fu_norm;

		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			continue;

		f = fu->f_p;
		NMG_CK_FACE( f );

		if( *f->g.magic_p != NMG_FACE_G_PLANE_MAGIC )
			goto not_arb;

		NMG_GET_FU_NORMAL( fu_norm, fu );

		loop_count = 0;
		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		{
			struct edgeuse *eu;

			NMG_CK_LOOPUSE( lu );

			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
				goto not_arb;

			loop_count++;

			/* face must be a single loop */
			if( loop_count > 1 )
				goto not_arb;

			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				struct edgeuse *eu_radial;
				struct faceuse *fu_radial;
				struct face *f_radial;
				vect_t norm_radial;
				vect_t eu_dir;
				vect_t cross;
				fastf_t dot;

				NMG_CK_EDGEUSE( eu );

				eu_radial = nmg_next_radial_eu( eu, s, 0 );

				/* Can't have any dangling faces */
				if( eu_radial == eu || eu_radial == eu->eumate_p )
					goto not_arb;

				fu_radial = nmg_find_fu_of_eu( eu_radial );
				NMG_CK_FACEUSE( fu_radial );

				if( fu_radial->orientation != OT_SAME )
					fu_radial = fu_radial->fumate_p;

				f_radial = fu_radial->f_p;
				NMG_CK_FACE( f_radial );

				/* faces must be planar */
				if( *f_radial->g.magic_p != NMG_FACE_G_PLANE_MAGIC )
					goto not_arb;


				/* Make sure shell is convex by checking that edguses
				 * run in direction fu_norm X norm_radial
				 */
				NMG_GET_FU_NORMAL( norm_radial, fu_radial );

				dot = VDOT( norm_radial, fu_norm );

				if( !NEAR_ZERO( dot - 1.0, 0.00001 ) )
				{

					VCROSS( cross, fu_norm, norm_radial );

					if( eu->orientation == OT_NONE )
					{
						VSUB2( eu_dir, eu->vu_p->v_p->vg_p->coord, eu->eumate_p->vu_p->v_p->vg_p->coord )
						if( eu->orientation != OT_SAME )
							VREVERSE( eu_dir, eu_dir )
					}
					else
						VMOVE( eu_dir, eu->g.lseg_p->e_dir )

					if( eu->orientation == OT_SAME || eu->orientation == OT_NONE )
					{
						if( VDOT( cross, eu_dir ) < 0.0 )
							goto not_arb;
					}
					else
					{
						if( VDOT( cross, eu_dir ) > 0.0 )
							goto not_arb;
					}
				}
			}
		}
	}

	/* count face types */
	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		struct loopuse *lu;
		int vert_count=0;

		if( fu->orientation != OT_SAME )
			continue;

		face_count++;
		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		{
			struct edgeuse *eu;

			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				vert_count++;
		}

		if( vert_count == 3 )
			three_verts++;
		else if( vert_count == 4 )
			four_verts++;
	}

	/* must have only three and four vertices in each face */
	if( face_count != three_verts + four_verts )
		goto not_arb;

	/* which type of arb is this?? */
	switch( BU_PTBL_END( tab ) )
	{
		case 4:		/* each face must have 3 vertices */
			if( three_verts != 4 || four_verts != 0 )
				goto not_arb;
			break;
		case 5:		/* one face with 4 verts, four with 3 verts */
			if( four_verts != 1 || three_verts != 4 )
				goto not_arb;
			break;
		case 6:		/* three faces with 4 verts, two with 3 verts */
			if( three_verts != 2 || four_verts != 3 )
				goto not_arb;
			break;
		case 7:		/* four faces with 4 verts, two with 3 verts */
			if( four_verts != 4 || three_verts != 2 )
				goto not_arb;
			break;
		case 8:		/* each face must have 4 vertices */
			if( four_verts != 6 || three_verts != 0 )
				goto not_arb;
			break;
	}

	return( BU_PTBL_END( tab ) );


not_arb:
	bu_ptbl_free( tab);
	return( 0 );
}

/**		N M G _ T O _ A R B
 *
 *	Converts an NMG to an ARB, if possible.
 *
 *	NMG must have been coplanar face merged and simplified
 *
 *	Returns:
 *		1 - Equivalent ARB was constructed
 *		0 - Cannot construct an equivalent ARB
 *
 *	The newly constructed arb is in "arb_int"
 */
int
nmg_to_arb( const struct model *m, struct rt_arb_internal *arb_int )
{
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct vertex *v;
	struct edgeuse *eu_start;
	struct faceuse *fu1;
	struct bu_ptbl tab;
	int face_verts;
	int i,j;
	int found;
	int ret_val = 0;

	NMG_CK_MODEL( m );

	r = BU_LIST_FIRST( nmgregion, &m->r_hd );

	/* must be a single region */
	if( BU_LIST_NEXT_NOT_HEAD( &r->l, &m->r_hd ) )
		return( 0 );

	s = BU_LIST_FIRST( shell, &r->s_hd );
	NMG_CK_SHELL( s );

	/* must be a single shell */
	if( BU_LIST_NEXT_NOT_HEAD( &s->l, &r->s_hd ) )
		return( 0 );

	switch( Shell_is_arb( s, &tab ) )
	{
		case 0:
			ret_val = 0;
			break;
		case 4:
			v = (struct vertex *)BU_PTBL_GET( &tab, 0 );
			NMG_CK_VERTEX( v );
			VMOVE( arb_int->pt[0], v->vg_p->coord );
			v = (struct vertex *)BU_PTBL_GET( &tab, 1 );
			NMG_CK_VERTEX( v );
			VMOVE( arb_int->pt[1], v->vg_p->coord );
			v = (struct vertex *)BU_PTBL_GET( &tab, 2 );
			NMG_CK_VERTEX( v );
			VMOVE( arb_int->pt[2], v->vg_p->coord );
			VMOVE( arb_int->pt[3], v->vg_p->coord );
			v = (struct vertex *)BU_PTBL_GET( &tab, 3 );
			NMG_CK_VERTEX( v );
			VMOVE( arb_int->pt[4], v->vg_p->coord );
			VMOVE( arb_int->pt[5], v->vg_p->coord );
			VMOVE( arb_int->pt[6], v->vg_p->coord );
			VMOVE( arb_int->pt[7], v->vg_p->coord );

			bu_ptbl_free( &tab);
			ret_val = 1;
			break;
		case 5:
			fu = BU_LIST_FIRST( faceuse, &s->fu_hd );
			face_verts = 0;
			while( face_verts != 4 )
			{
				face_verts = 0;
				fu = BU_LIST_PNEXT_CIRC( faceuse, &fu->l );
				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
					face_verts++;
			}
			NMG_CK_FACEUSE( fu );
			if( fu->orientation != OT_SAME )
				fu = fu->fumate_p;
			NMG_CK_FACEUSE( fu );

			lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
			j = 0;
			eu_start = BU_LIST_FIRST( edgeuse, &lu->down_hd );
			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				VMOVE( arb_int->pt[j], eu->vu_p->v_p->vg_p->coord )
				j++;
			}

			eu = eu_start->radial_p;
			eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
			eu = eu->eumate_p;
			for( i=0 ; i<4 ; i++ )
			{
				VMOVE( arb_int->pt[j], eu->vu_p->v_p->vg_p->coord )
				j++;
			}

			bu_ptbl_free( &tab);
			ret_val = 1;
			break;
		case 6:
			fu = BU_LIST_FIRST( faceuse, &s->fu_hd );
			face_verts = 0;
			while( face_verts != 3 )
			{
				face_verts = 0;
				fu = BU_LIST_PNEXT_CIRC( faceuse, &fu->l );
				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
					face_verts++;
			}
			NMG_CK_FACEUSE( fu );
			if( fu->orientation != OT_SAME )
				fu = fu->fumate_p;
			NMG_CK_FACEUSE( fu );

			lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );

			eu_start = BU_LIST_FIRST( edgeuse, &lu->down_hd );
			eu = eu_start;
			VMOVE( arb_int->pt[1], eu->vu_p->v_p->vg_p->coord );
			eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
			VMOVE( arb_int->pt[0], eu->vu_p->v_p->vg_p->coord );
			eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
			VMOVE( arb_int->pt[4], eu->vu_p->v_p->vg_p->coord );
			VMOVE( arb_int->pt[5], eu->vu_p->v_p->vg_p->coord );

			eu = eu_start->radial_p;
			eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
			eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
			eu = eu->radial_p->eumate_p;
			VMOVE( arb_int->pt[2], eu->vu_p->v_p->vg_p->coord );
			eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
			VMOVE( arb_int->pt[3], eu->vu_p->v_p->vg_p->coord );
			eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
			VMOVE( arb_int->pt[6], eu->vu_p->v_p->vg_p->coord );
			VMOVE( arb_int->pt[7], eu->vu_p->v_p->vg_p->coord );

			bu_ptbl_free( &tab);
			ret_val = 1;
			break;
		case 7:
			found = 0;
			fu = BU_LIST_FIRST( faceuse, &s->fu_hd );
			while( !found )
			{
				int verts4=0,verts3=0;

				lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					struct loopuse *lu1;
					struct edgeuse *eu1;
					int vert_count=0;

					fu1 = nmg_find_fu_of_eu( eu->radial_p );
					lu1 = BU_LIST_FIRST( loopuse, &fu1->lu_hd );
					for( BU_LIST_FOR( eu1, edgeuse, &lu1->down_hd ) )
						vert_count++;

					if( vert_count == 4 )
						verts4++;
					else if( vert_count == 3 )
						verts3++;
				}

				if( verts4 == 2 && verts3 == 2 )
					found = 1;
			}
			if( fu->orientation != OT_SAME )
				fu = fu->fumate_p;
			NMG_CK_FACEUSE( fu );

			lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
			j = 0;
			eu_start = BU_LIST_FIRST( edgeuse, &lu->down_hd );
			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				VMOVE( arb_int->pt[j], eu->vu_p->v_p->vg_p->coord )
				j++;
			}

			eu = eu_start->radial_p;
			eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
			eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
			eu = eu->radial_p->eumate_p;
			fu1 = nmg_find_fu_of_eu( eu );
			if( nmg_faces_are_radial( fu, fu1 ) )
			{
				eu = eu_start->radial_p;
				eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				eu = eu->radial_p->eumate_p;
			}
			for( i=0 ; i<4 ; i++ )
			{
				VMOVE( arb_int->pt[j], eu->vu_p->v_p->vg_p->coord )
				j++;
				eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
			}

			bu_ptbl_free( &tab);
			ret_val = 1;
			break;
		case 8:
			fu = BU_LIST_FIRST( faceuse, &s->fu_hd );
			NMG_CK_FACEUSE( fu );
			if( fu->orientation != OT_SAME )
				fu = fu->fumate_p;
			NMG_CK_FACEUSE( fu );

			lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
			j = 0;
			eu_start = BU_LIST_FIRST( edgeuse, &lu->down_hd );
			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				VMOVE( arb_int->pt[j], eu->vu_p->v_p->vg_p->coord )
				j++;
			}

			eu = eu_start->radial_p;
			eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
			eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
			eu = eu->radial_p->eumate_p;
			for( i=0 ; i<4 ; i++ )
			{
				VMOVE( arb_int->pt[j], eu->vu_p->v_p->vg_p->coord )
				j++;
				eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
			}

			bu_ptbl_free( &tab);
			ret_val = 1;
			break;
		default:
			rt_bomb( "Shell_is_arb screwed up" );
			break;
	}
	if( ret_val )
		arb_int->magic = RT_ARB_INTERNAL_MAGIC;
	return( ret_val );
}

/**		N M G _ T O _ T G C
 *
 *	Converts an NMG to a TGC, if possible.
 *
 *	NMG must have been coplanar face merged and simplified
 *
 *	Returns:
 *		1 - Equivalent TGC was constructed
 *		0 - Cannot construct an equivalent TGC
 *
 *	The newly constructed tgc is in "tgc_int"
 *
 *	Currently only supports RCC, and creates circumscribed RCC
 */
int
nmg_to_tgc(
	const struct model *m,
	struct rt_tgc_internal *tgc_int,
	const struct bn_tol *tol )
{
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct faceuse *fu_base=(struct faceuse *)NULL;
	struct faceuse *fu_top=(struct faceuse *)NULL;
	int three_vert_faces=0;
	int four_vert_faces=0;
	int many_vert_faces=0;
	int base_vert_count=0;
	int top_vert_count=0;
	point_t sum;
	fastf_t vert_count=0.0;
	fastf_t one_over_vert_count;
	point_t base_center;
	fastf_t min_base_r_sq;
	fastf_t max_base_r_sq;
	fastf_t sum_base_r_sq;
	fastf_t ave_base_r_sq;
	fastf_t base_r;
	point_t top_center;
	fastf_t min_top_r_sq;
	fastf_t max_top_r_sq;
	fastf_t sum_top_r_sq;
	fastf_t ave_top_r_sq;
	fastf_t top_r;
	plane_t top_pl;
	plane_t base_pl;
	vect_t plv_1,plv_2;

	NMG_CK_MODEL( m );

	BN_CK_TOL( tol );

	r = BU_LIST_FIRST( nmgregion, &m->r_hd );

	/* must be a single region */
	if( BU_LIST_NEXT_NOT_HEAD( &r->l, &m->r_hd ) )
		return( 0 );

	s = BU_LIST_FIRST( shell, &r->s_hd );
	NMG_CK_SHELL( s );

	/* must be a single shell */
	if( BU_LIST_NEXT_NOT_HEAD( &s->l, &r->s_hd ) )
		return( 0 );

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		int lu_count=0;

		NMG_CK_FACEUSE( fu );
		if( fu->orientation != OT_SAME )
			continue;

		vert_count = 0.0;

		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		{

			NMG_CK_LOOPUSE( lu );

			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
				return( 0 );

			lu_count++;
			if( lu_count > 1 )
				return( 0 );

			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				vert_count++;
		}

		if( vert_count < 3 )
			return( 0 );

		if( vert_count == 4 )
			four_vert_faces++;
		else if( vert_count ==3 )
			three_vert_faces++;
		else
		{
			many_vert_faces++;
			if( many_vert_faces > 2 )
				return( 0 );

			if( many_vert_faces == 1 )
			{
				fu_base = fu;
				base_vert_count = vert_count;
				NMG_GET_FU_PLANE( base_pl, fu_base );
			}
			else if( many_vert_faces == 2 )
			{
				fu_top = fu;
				top_vert_count = vert_count;
				NMG_GET_FU_PLANE( top_pl, fu_top );
			}
		}
	}
	/* if there are any three vertex faces,
	 * there must be an even number of them
	 */
	if( three_vert_faces%2 )
		return( 0 );

	/* base and top must have same number of vertices */
	if( base_vert_count != top_vert_count )
		return( 0 );

	/* Must have correct number of side faces */
	if( (base_vert_count != four_vert_faces) &&
	    (base_vert_count*2 != three_vert_faces ) )
		return( 0 );

	if( !NEAR_ZERO( 1.0 + VDOT( top_pl, base_pl ), tol->perp ) )
		return( 0 );

	/* This looks like a good candidate,
	 * Calculate center of base and top faces
	 */

	vert_count = 0.0;
	VSETALL( sum, 0.0 );
	lu = BU_LIST_FIRST( loopuse, &fu_base->lu_hd );
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct vertex_g *vg;

		NMG_CK_EDGEUSE( eu );

		NMG_CK_VERTEXUSE( eu->vu_p );
		NMG_CK_VERTEX( eu->vu_p->v_p );
		vg = eu->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G( vg );

		VADD2( sum, sum, vg->coord );
		vert_count++;
	}

	one_over_vert_count = 1.0/vert_count;
	VSCALE( base_center, sum, one_over_vert_count );

	/* Calculate Average Radius */
	min_base_r_sq = MAX_FASTF;
	max_base_r_sq = (-min_base_r_sq);
	sum_base_r_sq = 0.0;
	lu = BU_LIST_FIRST( loopuse, &fu_base->lu_hd );
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct vertex_g *vg;
		vect_t rad_vect;
		fastf_t r_sq;

		vg = eu->vu_p->v_p->vg_p;

		VSUB2( rad_vect, vg->coord, base_center );
		r_sq = MAGSQ( rad_vect );
		if( r_sq > max_base_r_sq )
			max_base_r_sq = r_sq;
		if( r_sq < min_base_r_sq )
			min_base_r_sq = r_sq;

		sum_base_r_sq += r_sq;
	}

	ave_base_r_sq = sum_base_r_sq/vert_count;

	base_r = sqrt( max_base_r_sq );

	if( !NEAR_ZERO( (max_base_r_sq - ave_base_r_sq)/ave_base_r_sq, 0.001 ) ||
	    !NEAR_ZERO( (min_base_r_sq - ave_base_r_sq)/ave_base_r_sq, 0.001 ) )
			return( 0 );

	VSETALL( sum, 0.0 );
	lu = BU_LIST_FIRST( loopuse, &fu_top->lu_hd );
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct vertex_g *vg;

		NMG_CK_EDGEUSE( eu );

		NMG_CK_VERTEXUSE( eu->vu_p );
		NMG_CK_VERTEX( eu->vu_p->v_p );
		vg = eu->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G( vg );

		VADD2( sum, sum, vg->coord );
	}

	VSCALE( top_center, sum, one_over_vert_count );

	/* Calculate Average Radius */
	min_top_r_sq = MAX_FASTF;
	max_top_r_sq = (-min_top_r_sq);
	sum_top_r_sq = 0.0;
	lu = BU_LIST_FIRST( loopuse, &fu_top->lu_hd );
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct vertex_g *vg;
		vect_t rad_vect;
		fastf_t r_sq;

		vg = eu->vu_p->v_p->vg_p;

		VSUB2( rad_vect, vg->coord, top_center );
		r_sq = MAGSQ( rad_vect );
		if( r_sq > max_top_r_sq )
			max_top_r_sq = r_sq;
		if( r_sq < min_top_r_sq )
			min_top_r_sq = r_sq;

		sum_top_r_sq += r_sq;
	}

	ave_top_r_sq = sum_top_r_sq/vert_count;
	top_r = sqrt( max_top_r_sq );

	if( !NEAR_ZERO( (max_top_r_sq - ave_top_r_sq)/ave_top_r_sq, 0.001 ) ||
	    !NEAR_ZERO( (min_top_r_sq - ave_top_r_sq)/ave_top_r_sq, 0.001 ) )
			return( 0 );


	VMOVE( tgc_int->v, base_center );
	VSUB2( tgc_int->h, top_center, base_center );

	bn_vec_perp( plv_1, top_pl );
	VCROSS( plv_2, top_pl, plv_1 );
	VUNITIZE( plv_1 );
	VUNITIZE( plv_2 );
	VSCALE( tgc_int->a, plv_1, base_r );
	VSCALE( tgc_int->b, plv_2, base_r );
	VSCALE( tgc_int->c, plv_1, top_r );
	VSCALE( tgc_int->d, plv_2, top_r );

	tgc_int->magic = RT_TGC_INTERNAL_MAGIC;

	return( 1 );
}


/**		N M G _ L U _ I S _ C O N V E X
 *
 *	Checks if lu is convex
 *
 * Returns:
 *	1 - is loop is convex, or lu is a loop of a single vertex
 *	0 - otherwise
 */
int
nmg_lu_is_convex(struct loopuse *lu, const struct bn_tol *tol)
{
	struct edgeuse *eu1,*eu2,*eu3,*eu_start;

	NMG_CK_LOOPUSE( lu );
	BN_CK_TOL( tol );

	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return( 1 );

	if( !lu->l_p->lg_p )
		nmg_loop_g( lu->l_p, tol );

	eu1 = BU_LIST_FIRST( edgeuse, &lu->down_hd );
	eu_start = eu1;
	eu2 = BU_LIST_PNEXT_CIRC( edgeuse, &eu1->l );
	eu3 = BU_LIST_PNEXT_CIRC( edgeuse, &eu2->l );

	if( eu1->vu_p->v_p == eu3->eumate_p->vu_p->v_p )
		return( 1 );		/* triangle */

	while( eu3 != eu_start )
	{
		int class;
		struct vertex_g *vg1,*vg3;
		point_t mid_pt;

		vg1 = eu1->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G( vg1 );

		vg3 = eu3->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G( vg3 );

		VBLEND2( mid_pt, 0.5, vg1->coord, 0.5, vg3->coord );

		class = nmg_class_pt_lu_except( mid_pt, lu, NULL, tol );

		if( (class == NMG_CLASS_AoutB && lu->orientation == OT_SAME) ||
		    (class == NMG_CLASS_AinB  && lu->orientation == OT_OPPOSITE) )
				return( 0 );
		else
		{
			eu1 = BU_LIST_PNEXT_CIRC( edgeuse, &eu1->l );
			eu2 = BU_LIST_PNEXT_CIRC( edgeuse, &eu1->l );
			eu3 = BU_LIST_PNEXT_CIRC( edgeuse, &eu2->l );
		}
	}

	return( 1 );
}

/**
 *			N M G _ T O _ P O L Y
 *
 * XXX This routine is deprecated in favor of BoTs
 */
int
nmg_to_poly(const struct model *m, struct rt_pg_internal *poly_int, const struct bn_tol *tol)
{
	struct nmgregion *r;
	struct shell  *s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct model *dup_m;
	struct nmgregion *dup_r;
	struct shell *dup_s;
	int max_count;
	int count_npts;
	int face_count=0;
#if 0
	struct rt_pg_face_internal *_poly;
#endif

	NMG_CK_MODEL( m );

	BN_CK_TOL( tol );

	for( BU_LIST_FOR( r, nmgregion, &m->r_hd ) )
	{
		for( BU_LIST_FOR( s, shell, &r->s_hd ) )
		{
			if( nmg_check_closed_shell( s, tol ) )
				return( 0 );
		}
	}

	dup_m = nmg_mm();
	dup_r = nmg_mrsv( dup_m );
	dup_s = BU_LIST_FIRST( shell, &dup_r->s_hd );

	for( BU_LIST_FOR( r, nmgregion, &m->r_hd ) )
	{
		for( BU_LIST_FOR( s, shell, &r->s_hd ) )
		{
			for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
			{
				if( fu->orientation != OT_SAME )
					continue;
				(void)nmg_dup_face( fu, dup_s );
			}
		}
	}

	for( BU_LIST_FOR( dup_r, nmgregion, &dup_m->r_hd ) )
	{
		for( BU_LIST_FOR( dup_s, shell, &dup_r->s_hd ) )
		{
			for( BU_LIST_FOR( fu , faceuse , &dup_s->fu_hd ) )
			{
				NMG_CK_FACEUSE( fu );

				/* only do OT_SAME faces */
				if( fu->orientation != OT_SAME )
					continue;

				/* count vertices in loops */
				max_count = 0;
				for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
				{
					NMG_CK_LOOPUSE( lu );
					if( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
						continue;

					if( lu->orientation != OT_SAME )
					{
						/* triangulate holes */
						max_count = 6;
						break;
					}

					count_npts = 0;
					for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
						count_npts++;

					if( count_npts > 5 )
					{
						max_count = count_npts;
						break;
					}
					if( !nmg_lu_is_convex( lu, tol ) )
					{
						/* triangulate non-convex faces */
						max_count = 6;
						break;
					}
				}

				/* if any loop has more than 5 vertices, triangulate the face */
				if( max_count > 5 )
				{
					if( rt_g.NMG_debug & DEBUG_BASIC )
						bu_log( "write_shell_as_polysolid: triangulating fu x%x\n", fu );
					nmg_triangulate_fu( fu, tol );
				}

				for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
				{
					NMG_CK_LOOPUSE( lu );
					if( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
						continue;

					face_count++;
				}
			}
		}
	}
	poly_int->npoly = face_count;
	poly_int->poly = (struct rt_pg_face_internal *)bu_calloc( face_count,
		sizeof( struct rt_pg_face_internal ), "nmg_to_poly: poly" );

	face_count = 0;
	for( BU_LIST_FOR( dup_r, nmgregion, &dup_m->r_hd ) )
	{
		for( BU_LIST_FOR( dup_s, shell, &dup_r->s_hd ) )
		{
			for( BU_LIST_FOR( fu , faceuse , &dup_s->fu_hd ) )
			{
				vect_t norm;

				NMG_CK_FACEUSE( fu );

				/* only do OT_SAME faces */
				if( fu->orientation != OT_SAME )
					continue;

				NMG_GET_FU_NORMAL( norm, fu );

				for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
				{
					int pt_no=0;

					if( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
						continue;

					/* count vertices in loop */
					count_npts = 0;
					for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
						count_npts++;

					poly_int->poly[face_count].npts = count_npts;
					poly_int->poly[face_count].verts = (fastf_t *) bu_calloc( 3*count_npts, sizeof( fastf_t ), "nmg_to_poly: verts" );
					poly_int->poly[face_count].norms = (fastf_t *) bu_calloc( 3*count_npts, sizeof( fastf_t ), "nmg_to_poly: norms" );

					for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
					{
						struct vertex_g *vg;

						vg = eu->vu_p->v_p->vg_p;
						NMG_CK_VERTEX_G( vg );

						VMOVE( &(poly_int->poly[face_count].verts[pt_no*3]), vg->coord );
						VMOVE( &(poly_int->poly[face_count].norms[pt_no*3]), norm );

						pt_no++;
					}
					face_count++;
				}
			}
		}
	}

	poly_int->magic = RT_PG_INTERNAL_MAGIC;
	nmg_km( dup_m );

	return( 1 );
}

int
nmg_simplify_shell_edges(struct shell *s, const struct bn_tol *tol)
{
	struct faceuse *fu;
	int count=0;

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	fu = BU_LIST_FIRST( faceuse, &s->fu_hd );
	while( BU_LIST_NOT_HEAD( &fu->l, &s->fu_hd ) )
	{
		struct faceuse *fu_next;
		struct loopuse *lu;
		int empty_fu=0;

		NMG_CK_FACEUSE( fu );

		fu_next = BU_LIST_PNEXT( faceuse, &fu->l );

		if( fu->orientation != OT_SAME )
		{
			fu = fu_next;
			continue;
		}

		lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );

		while( BU_LIST_NOT_HEAD( &lu->l, &fu->lu_hd ) )
		{
			struct loopuse *lu_next;
			struct edge *ep1,*ep2;
			struct edgeuse *eu;
			struct edgeuse *eu_next;
			vect_t dir_eu;
			vect_t dir_next;

			NMG_CK_LOOPUSE( lu );

			lu_next = BU_LIST_PNEXT( loopuse, &lu->l );

			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			{
				lu = lu_next;
				continue;
			}

			eu = BU_LIST_FIRST( edgeuse, &lu->down_hd );
			while( BU_LIST_NOT_HEAD( &eu->l, &lu->down_hd ) )
			{
				struct vertex *v1,*v2,*v3;
				struct vertex_g *vg1,*vg2,*vg3;
				struct vertexuse *vu;
				struct vertexuse *next_vu;
				struct edgeuse *eu_tmp;
				point_t pca;
				fastf_t dist;
				int skip;
				int empty_lu=0;

				v1 = eu->vu_p->v_p;
				ep1 = eu->e_p;
				eu_next = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				ep2 = eu_next->e_p;
				v2 = eu_next->vu_p->v_p;

				if( v1 == v2 )
				{
					/* this is a crack */
					eu = BU_LIST_PNEXT( edgeuse, &eu->l );
					continue;
				}

				/* make sure there are no uses of v2 outside this shell,
				 * and that all uses are for either ep1 or ep2 */
				skip = 0;
				for( BU_LIST_FOR( vu, vertexuse, &v2->vu_hd ) )
				{
					if( nmg_find_s_of_vu( vu ) != s )
					{
						skip = 1;
						break;
					}
					if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
						continue;

					if( vu->up.eu_p->e_p != ep1 &&
					    vu->up.eu_p->e_p != ep2 )
					{
						skip = 1;
						break;
					}
				}

				if( skip )
				{
					/* can't kill eu_next */
					eu = BU_LIST_PNEXT( edgeuse, &eu->l );
					continue;
				}

				vg2 = v2->vg_p;

				v1 = eu->vu_p->v_p;
				vg1 = v1->vg_p;
				v3 = eu_next->eumate_p->vu_p->v_p;
				vg3 = v3->vg_p;

				VSUB2( dir_eu, vg2->coord, vg1->coord );
				VSUB2( dir_next, vg3->coord, vg2->coord );

				skip = 1;
				if( rt_dist_pt3_line3( &dist, pca, vg1->coord, dir_eu, vg3->coord, tol ) < 2 )
					skip = 0;
				else if( rt_dist_pt3_line3( &dist, pca, vg2->coord, dir_next, vg1->coord, tol ) < 2 )
					skip = 0;

				if( skip )
				{
					/* can't kill eu_next */
					eu = BU_LIST_PNEXT( edgeuse, &eu->l );
					continue;
				}

				count++;
				/* kill all uses of eu_next edge */
				eu_tmp = eu_next->radial_p;
				while( eu_tmp != eu_next->eumate_p )
				{
					nmg_keu( eu_tmp );
					eu_tmp = eu_next->radial_p;
				}
				nmg_keu( eu_next );

				/* move all ep1 EU's using v2 to vertex v3 */
				vu = BU_LIST_FIRST( vertexuse, &v2->vu_hd );
				while( BU_LIST_NOT_HEAD( &vu->l, &v2->vu_hd ) )
				{
					NMG_CK_VERTEXUSE( vu );
					next_vu = BU_LIST_PNEXT( vertexuse, &vu->l );
					if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
					{
						vu = next_vu;
						continue;
					}

					eu_tmp = vu->up.eu_p;
					NMG_CK_EDGEUSE( eu );

					if( eu->e_p != ep1 )
					{
						vu = next_vu;
						continue;
					}

					nmg_movevu( eu_tmp->vu_p, v3 );

					vu = next_vu;
					continue;
				}

				if( BU_LIST_IS_EMPTY( &lu->down_hd ) )
				{
					empty_lu = 1;
					empty_fu = nmg_klu( lu );
				}
				if( empty_lu )
					break;

			}

			if( empty_fu )
				break;
			lu = lu_next;
		}
		if( empty_fu )
		{
			if( fu_next == fu->fumate_p )
				fu_next = BU_LIST_NEXT( faceuse, &fu_next->l );
			(void)nmg_kfu( fu );
		}
		fu = fu_next;
	}
	return( count );
}

#define EDGE_COLLAPSE_DEBUG 0

int
nmg_select_collapse(const fastf_t max_dist1, const fastf_t max_dot1, const int flip1, const fastf_t max_dist2, const fastf_t max_dot2, const int flip2, const fastf_t max_dot, const fastf_t tol_dist)
{
	unsigned int ret;

	ret = 1 | 2;

	if( flip1 )
		ret = ret & ~1;

	if( flip2 )
		ret = ret & ~2;

	if( max_dist1 > tol_dist )
		ret = ret & ~1;

	if( max_dist2 > tol_dist )
		ret = ret & ~2;

	if( max_dot1 > max_dot )
		ret = ret & ~1;

	if( max_dot2 > max_dot )
		ret = ret & ~2;

	if( ret == (1 | 2) )
	{
		if( max_dot1 < max_dot2 )
			ret = 1;
		else
			ret =  2;
	}

#if EDGE_COLLAPSE_DEBUG
bu_log( "\nmax_dot=%g, tol_coll=%g\n", max_dot, tol_dist );
bu_log( "max_dist1, max_dot1, flip1: %g %g %d\n", max_dist1, max_dot1, flip1 );
bu_log( "max_dist2, max_dot2, flip2: %g %g %d\n", max_dist2, max_dot2, flip2 );
bu_log( "choice = %d\n", ret );
#endif

	return( ret );
}

/**	 	N M G _ E D G E _ C O L L A P S E
 *
 * Routine to decimate an NMG model through edge collapse
 * to obtain a model with less faces at a greater tolerance
 *
 *  tol_coll is the tolerance distance to be used for choosing
 *  edges to collapse
 *
 * Model must already be triangulated (this is not checked for)
 *
 * returns number of edges collapsed
 */
int
nmg_edge_collapse(struct model *m, const struct bn_tol *tol, const fastf_t tol_coll, const fastf_t min_angle)
{
	fastf_t max_dot;
	struct bu_ptbl edge_table;
	long edge_no=0;
	long count=0;
	long sub_count=1;
	int choice;
	int i;

	NMG_CK_MODEL( m );
	BN_CK_TOL( tol );

	max_dot = cos( min_angle * bn_pi / 180.0 );

	/* Each triangle must be its own face */
	(void)nmg_split_loops_into_faces( &m->magic, tol );

	nmg_edge_tabulate( &edge_table, &m->magic );

	while( sub_count )
	{
		sub_count = 0;
		for( edge_no=0 ; edge_no < BU_PTBL_END( &edge_table ) ; edge_no++ )
		{
			int done=0;
			struct edge *e;
			struct edgeuse *eu, *eu2, *eu3;
			struct faceuse *fu, *fu2;
			struct vertex *v1, *v2;
			struct vertexuse *vu;
			int real_count1, real_count2;
			vect_t edge_dir;
			fastf_t max_dist1, max_dist2;
			fastf_t max_dot1, max_dot2;
			int flip1, flip2;
			int no_collapse;
			int free_edge;

			max_dist1 = -1.0;
			max_dist2 = -1.0;
			max_dot1 = -1.0;
			max_dot2 = -1.0;

			e = (struct edge *)BU_PTBL_GET( &edge_table, edge_no );
			if( !e )
				continue;
			if( e->magic != NMG_EDGE_MAGIC )
				continue;

			NMG_CK_EDGE( e );
			eu = e->eu_p;
			NMG_CK_EDGEUSE( eu );

			v1 = eu->vu_p->v_p;
			NMG_CK_VERTEX( v1 );
			v2 = eu->eumate_p->vu_p->v_p;
			NMG_CK_VERTEX( v2 );

			/* don't collapse free edges */
			free_edge = 0;
			eu2 = eu;
			do
			{
				eu3 = eu2;
				do {
					if( eu3->radial_p->eumate_p == eu3 )
					{
						free_edge = 1;
						break;
					}
					eu3 = BU_LIST_PNEXT_CIRC( edgeuse, &eu3->l );

				} while ( eu3 != eu2 );
				if( free_edge )
					break;

				eu2 = eu2->radial_p->eumate_p;
			} while( eu2 != eu );

			if( free_edge )
			{
#if EDGE_COLLAPSE_DEBUG
				bu_log( "Not collapsing free edge at (%g %g %g)<->(%g %g %g)\n",
					V3ARGS( v1->vg_p->coord ), V3ARGS( v2->vg_p->coord ) );
#endif
				continue;
			}

			/* consider collapsing this edge */
#if EDGE_COLLAPSE_DEBUG
			bu_log( "Consider collapsing e x%x (%g %g %g)<->(%g %g %g)\n", e, V3ARGS( v1->vg_p->coord ), V3ARGS( v2->vg_p->coord ) );
#endif

			/* don't collapse where more than 2 real edges meet */
			real_count1 = 0;

			for( BU_LIST_FOR( vu, vertexuse, &v1->vu_hd ) )
			{
				struct edgeuse *eu1;

				if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
					continue;

				eu1 = vu->up.eu_p;

				fu = nmg_find_fu_of_eu( eu1 );
				if( fu->orientation != OT_SAME )
					continue;

				if( eu1->e_p->is_real )
					real_count1++;
			}

			if( real_count1 > 2 )
			{
#if EDGE_COLLAPSE_DEBUG
				bu_log( "\t%d real edges at v1\n", real_count1 );
#endif
				continue;
			}

			real_count2 = 0;

			for( BU_LIST_FOR( vu, vertexuse, &v2->vu_hd ) )
			{
				struct edgeuse *eu1;

				if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
					continue;

				eu1 = vu->up.eu_p;

				fu = nmg_find_fu_of_eu( eu1 );
				if( fu->orientation != OT_SAME )
					continue;

				if( eu1->e_p->is_real )
					real_count2++;
			}

			if( real_count2 > 2 )
			{
#if EDGE_COLLAPSE_DEBUG
				bu_log( "\t%d real edges at v1\n", real_count1 );
#endif
				continue;
			}

			/* if real edges are involved, only collapse along the real edges */
			if( (real_count1 || real_count2) && !e->is_real )
			{
#if EDGE_COLLAPSE_DEBUG
				bu_log( "\tthis edge is not real\n" );
#endif
				continue;
			}
#if EDGE_COLLAPSE_DEBUG
			bu_log( "\tpassed real edges test\n" );
#endif

			VSUB2( edge_dir, v2->vg_p->coord, v1->vg_p->coord )
			VUNITIZE( edge_dir )

			fu = nmg_find_fu_of_eu( eu );
			NMG_CK_FACEUSE( fu );
			if( fu->orientation != OT_SAME )
				fu = fu->fumate_p;
			if( fu->orientation != OT_SAME )
			{
				bu_log( "nmg_edge_collapse: fu (x%x) has no OT_SAME use!!!\n", fu );
				continue;
			}
			eu2 = eu->radial_p;
			NMG_CK_EDGEUSE( eu2 );
			fu2 = nmg_find_fu_of_eu( eu2 );
			NMG_CK_FACEUSE( fu2 );
			if( fu2->orientation != OT_SAME )
				fu2 = fu2->fumate_p;
			if( fu2->orientation != OT_SAME )
			{
				bu_log( "nmg_edge_collapse: fu2 (x%x) has no OT_SAME use!!!\n", fu2 );
				continue;
			}

#if EDGE_COLLAPSE_DEBUG
			bu_log( "\tCheck moving v1 to v2\n" );
#endif
			/* check if moving v1 to v2 would flip any loops */
			flip1 = 0;
			for( BU_LIST_FOR( vu, vertexuse, &v1->vu_hd ) )
			{
				struct edgeuse *eu1, *eu2;
				vect_t vec1, vec2, vec3;
				vect_t norma;
				struct vertex_g *vg1, *vg2;
				plane_t normb;
				fastf_t dist;
				fastf_t dot1;

				if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
					continue;

				eu1 = vu->up.eu_p;
				eu2 = BU_LIST_PNEXT_CIRC( edgeuse, &eu1->l );

				if( eu2->eumate_p->vu_p->v_p == v2 )
					continue;

				if( eu2->vu_p->v_p == v2 )
					continue;

				vg1 = eu2->vu_p->v_p->vg_p;
				vg2 = eu2->eumate_p->vu_p->v_p->vg_p;

				VSUB2( vec1, v1->vg_p->coord, vg1->coord )
				VSUB2( vec2, vg2->coord, vg1->coord )
				VCROSS( norma, vec1, vec2 )
#if EDGE_COLLAPSE_DEBUG
				bu_log( "\t\toriginal tri (%g %g %g) (%g %g %g) (%g %g %g)\n",
					V3ARGS( v1->vg_p->coord ),
					V3ARGS( vg1->coord ),
					V3ARGS( vg2->coord ) );
#endif
				VSUB2( vec1, v2->vg_p->coord, vg1->coord )
				VCROSS( normb, vec1, vec2 )
				VSUB2( vec3, v2->vg_p->coord, vg2->coord )
				VUNITIZE( vec1 );
				VUNITIZE( vec2 );
				VUNITIZE( vec3 );

				dot1 = VDOT( vec1, vec2 );
				if( dot1 > max_dot1 )
					max_dot1 = dot1;
				dot1 = VDOT( vec1, vec3 );
				if( dot1 > max_dot1 )
					max_dot1 = dot1;
				dot1 = -VDOT( vec2, vec3 );
				if( dot1 > max_dot1 )
					max_dot1 = dot1;
#if EDGE_COLLAPSE_DEBUG
				bu_log( "\t\tnew tri (%g %g %g) (%g %g %g) (%g %g %g)\n",
					V3ARGS( v2->vg_p->coord ),
					V3ARGS( vg1->coord ),
					V3ARGS( vg2->coord ) );
#endif
				fu = nmg_find_fu_of_eu( eu1 );
				if( fu->orientation == OT_SAME )
				{
					VUNITIZE( normb )
					normb[3] = VDOT( normb, v2->vg_p->coord );

					dist = fabs( DIST_PT_PLANE( v1->vg_p->coord, normb ) );
#if EDGE_COLLAPSE_DEBUG
					bu_log( "\t\t\tdist = %g\n", dist );
#endif
					if( dist > max_dist1 )
						max_dist1 = dist;
				}
#if EDGE_COLLAPSE_DEBUG
				else
					bu_log( "\t\t\tfu->orientation = %s\n", nmg_orientation( fu->orientation ) );
#endif

				if( VDOT( norma, normb ) <= 0.0 )
				{
					/* this would flip a loop */
					flip1 = 1;
#if EDGE_COLLAPSE_DEBUG
					bu_log( "\t\t\tflip1 = 1\n" );
#endif
					break;
				}

			}

			/* check if moving v2 to v1 would flip any loops */
#if EDGE_COLLAPSE_DEBUG
			bu_log( "\tCheck moving v2 to v1\n" );
#endif
			flip2 = 0;
			for( BU_LIST_FOR( vu, vertexuse, &v2->vu_hd ) )
			{
				struct edgeuse *eu1, *eu2;
				vect_t vec1, vec2, vec3;
				vect_t norma;
				plane_t normb;
				struct vertex_g *vg1, *vg2;
				fastf_t dist, dot1;

				if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
					continue;

				eu1 = vu->up.eu_p;
				eu2 = BU_LIST_PNEXT_CIRC( edgeuse, &eu1->l );

				if( eu2->eumate_p->vu_p->v_p == v1 )
					continue;

				if( eu2->vu_p->v_p == v1 )
					continue;

				vg1 = eu2->vu_p->v_p->vg_p;
				vg2 = eu2->eumate_p->vu_p->v_p->vg_p;

				VSUB2( vec1, v2->vg_p->coord, vg1->coord )
				VSUB2( vec2, vg2->coord, vg1->coord )
				VCROSS( norma, vec1, vec2 )
#if EDGE_COLLAPSE_DEBUG
				bu_log( "\t\toriginal tri (%g %g %g) (%g %g %g) (%g %g %g)\n",
					V3ARGS( v2->vg_p->coord ),
					V3ARGS( vg1->coord ),
					V3ARGS( vg2->coord ) );
#endif

				VSUB2( vec1, v1->vg_p->coord, vg1->coord )
				VCROSS( normb, vec1, vec2 )
				VSUB2( vec3, v1->vg_p->coord, vg2->coord );
				VSUB2( vec3, v2->vg_p->coord, vg2->coord )
				VUNITIZE( vec1 );
				VUNITIZE( vec2 );
				VUNITIZE( vec3 );

				dot1 = VDOT( vec1, vec2 );
				if( dot1 > max_dot2 )
					max_dot2 = dot1;
				dot1 = VDOT( vec1, vec3 );
				if( dot1 > max_dot2 )
					max_dot2 = dot1;
				dot1 = -VDOT( vec2, vec3 );
				if( dot1 > max_dot2 )
					max_dot2 = dot1;
#if EDGE_COLLAPSE_DEBUG
				bu_log( "\t\tnew tri (%g %g %g) (%g %g %g) (%g %g %g)\n",
					V3ARGS( v1->vg_p->coord ),
					V3ARGS( vg1->coord ),
					V3ARGS( vg2->coord ) );
#endif

				fu = nmg_find_fu_of_eu( eu1 );
				if( fu->orientation == OT_SAME )
				{
					VUNITIZE( normb )
					normb[3] = VDOT( normb, v1->vg_p->coord );

					dist = fabs( DIST_PT_PLANE( v2->vg_p->coord, normb ));
#if EDGE_COLLAPSE_DEBUG
					bu_log( "\t\t\tdist = %g\n", dist );
#endif
					if( dist > max_dist2 )
						max_dist2 = dist;
				}
#if EDGE_COLLAPSE_DEBUG
				else
					bu_log( "\t\t\tfu->orientation = %s\n", nmg_orientation( fu->orientation ) );
#endif

				if( VDOT( norma, normb ) <= 0.0 )
				{
					/* this would flip a loop */
#if EDGE_COLLAPSE_DEBUG
					bu_log( "\t\t\tflip2 = 1\n" );
#endif
					flip2 = 1;
					break;
				}
			}
#if EDGE_COLLAPSE_DEBUG
			bu_log( "\tmax_dist1=%g, flip1=%d, max_dist2=%g, flip2=%d\n", max_dist1, flip1, max_dist2, flip2 );
#endif

			if( max_dist1 < 0.0 )
				max_dist1 = MAX_FASTF;
			if( max_dist2 < 0.0 )
				max_dist2 = MAX_FASTF;
			if( ((max_dist1 > tol_coll) || (flip1 > 0) ) &&
			    ((max_dist2 > tol_coll) || (flip2 > 0) ) )
				continue;


			/* one last check to insure we don't collapse two faces into
			 * a flat plane, i.e., don't collapse an ARB4 into a single
			 * triangle.
			 */
			no_collapse = 0;
			for( BU_LIST_FOR( vu, vertexuse, &v1->vu_hd ) )
			{
				struct edgeuse *eu1, *eu1a;
				struct edgeuse *eu2, *eu2a;
				struct vertexuse *vu2;

				if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
					continue;

				eu1 = vu->up.eu_p;
				if( eu1->e_p == e )
					continue;

				eu1a = BU_LIST_PNEXT_CIRC( edgeuse, &eu1->l );

				for( BU_LIST_FOR( vu2, vertexuse, &v2->vu_hd ) )
				{
					if( *vu2->up.magic_p != NMG_EDGEUSE_MAGIC )
						continue;

					eu2 = vu2->up.eu_p;
					if( eu2->e_p == e )
						continue;
					if( eu2->e_p == eu1->e_p )
						continue;

					eu2a = BU_LIST_PNEXT_CIRC( edgeuse, &eu2->l );

					if( NMG_ARE_EUS_ADJACENT( eu1a, eu2a ) )
					{
						no_collapse = 1;
						break;
					}
				}

				if( no_collapse )
					break;
			}

			if( no_collapse )
			{
#if EDGE_COLLAPSE_DEBUG
				bu_log( "\tNot collapsing edge (would create 2D object)\n" );
#endif
				continue;
			}

			choice = nmg_select_collapse( max_dist1, max_dot1, flip1,
						  max_dist2, max_dot2, flip2,
						  max_dot, tol_coll );

			if( !choice )
				continue;
#if EDGE_COLLAPSE_DEBUG
			bu_log( "\tCollapsing edge\n" );
#endif
			/* edge will be collapsed */
			bu_ptbl_zero( &edge_table, (long *)e );

			sub_count++;

			/* kill the loops radial to the edge being collapsed */
			eu2 = eu->radial_p;
			done = 0;
			while( !done )
			{
				struct edgeuse *next;
				struct loopuse *lu;
				struct faceuse *fu;

				next = eu2->eumate_p->radial_p;
				if( next == eu2 )
					done = 1;

				if( *eu2->up.magic_p != NMG_LOOPUSE_MAGIC )
					(void)nmg_keu( eu2 );
				else
				{
					lu = eu2->up.lu_p;
					if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )
						(void)nmg_klu( lu );
					else
					{
						fu = lu->up.fu_p;
						if( nmg_klu( lu ) )
							nmg_kfu( fu );
					}
				}
				eu2 = next;
			}

			if( choice == 1 )
			{
				struct edgeuse *eu1,*eu2;
				struct vertexuse *vu2;
#if EDGE_COLLAPSE_DEBUG
				bu_log( "\tMoving v1 to v2 (%g %g %g) -> (%g %g %g)\n", V3ARGS( v1->vg_p->coord ), V3ARGS( v2->vg_p->coord ) );
#endif
				/* vertex1 will be moved to vertex2 */

				/* recalculate edge geometry */
				for( BU_LIST_FOR( vu, vertexuse, &v1->vu_hd ) )
				{
					struct faceuse *fu;
					struct edge_g_lseg *eg;
					struct vertex_g *v1a;

					if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
						continue;

					eu1 = vu->up.eu_p;
					fu = nmg_find_fu_of_eu( eu1 );
					if( !fu )
						continue;

					if( fu->orientation != OT_SAME )
						continue;

					if( !eu1->g.magic_p )
						nmg_edge_g( eu1 );
					else if( *eu1->g.magic_p != NMG_EDGE_G_LSEG_MAGIC )
						continue;

					eg = eu1->g.lseg_p;
					v1a = eu1->eumate_p->vu_p->v_p->vg_p;
					VSUB2( eg->e_dir, v2->vg_p->coord, v1a->coord );
					VMOVE( eg->e_pt, v2->vg_p->coord );
				}

				done = 0;
				while( !done )
				{
					vu = BU_LIST_FIRST( vertexuse, &v1->vu_hd );
					if( BU_LIST_LAST( vertexuse, &v1->vu_hd ) == vu )
						done = 1;
					nmg_movevu( vu, v2 );
				}
				for( BU_LIST_FOR( vu, vertexuse, &v2->vu_hd ) )
				{
					if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
						continue;

					eu1 = vu->up.eu_p;

					vu2 = BU_LIST_NEXT( vertexuse, &vu->l );
					while( BU_LIST_NOT_HEAD( &vu2->l, &v2->vu_hd ) )
					{
						if( *vu2->up.magic_p != NMG_EDGEUSE_MAGIC )
						{
							vu2 = BU_LIST_NEXT( vertexuse, &vu2->l );
							continue;
						}
						eu2 = vu2->up.eu_p;
						if( eu2->eumate_p->vu_p->v_p == eu1->eumate_p->vu_p->v_p &&
							eu2->e_p != eu1->e_p )
						{
							struct edge *e1, *e2;

							e1 = eu1->e_p;
							e2 = eu2->e_p;

							nmg_radial_join_eu_NEW( eu2, eu1, tol );
							if( eu1->e_p != e1 )
								bu_ptbl_zero( &edge_table, (long *)e1 );
							if( eu2->e_p != e2 )
								bu_ptbl_zero( &edge_table, (long *)e2 );
						}
						vu2 = BU_LIST_NEXT( vertexuse, &vu2->l );
					}
				}
			}
			else if( choice == 2 )
			{
				struct edgeuse *eu1,*eu2;
				struct vertexuse *vu2;
#if EDGE_COLLAPSE_DEBUG
				bu_log( "\tMoving v2 to v1 (%g %g %g) -> (%g %g %g)\n", V3ARGS( v2->vg_p->coord ), V3ARGS( v1->vg_p->coord ) );
#endif
				/* vertex2 will be moved to vertex1 */
				/* recalculate edge geometry */
				for( BU_LIST_FOR( vu, vertexuse, &v2->vu_hd ) )
				{
					struct faceuse *fu;
					struct edge_g_lseg *eg;
					struct vertex_g *v1a;

					if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
						continue;

					eu1 = vu->up.eu_p;
					fu = nmg_find_fu_of_eu( eu1 );
					if( !fu )
						continue;

					if( fu->orientation != OT_SAME )
						continue;

					if( !eu1->g.magic_p )
						nmg_edge_g( eu1 );
					else if( *eu1->g.magic_p != NMG_EDGE_G_LSEG_MAGIC )
						continue;

					eg = eu1->g.lseg_p;
					v1a = eu1->eumate_p->vu_p->v_p->vg_p;
					VSUB2( eg->e_dir, v1->vg_p->coord, v1a->coord );
					VMOVE( eg->e_pt, v1->vg_p->coord );
				}

				done = 0;
				while( !done )
				{
					vu = BU_LIST_FIRST( vertexuse, &v2->vu_hd );
					if( BU_LIST_LAST( vertexuse, &v2->vu_hd ) == vu )
						done = 1;
					nmg_movevu( vu, v1 );
				}
				for( BU_LIST_FOR( vu, vertexuse, &v1->vu_hd ) )
				{
					if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
						continue;

					eu1 = vu->up.eu_p;

					vu2 = BU_LIST_NEXT( vertexuse, &vu->l );
					while( BU_LIST_NOT_HEAD( &vu2->l, &v1->vu_hd ) )
					{
						if( *vu2->up.magic_p != NMG_EDGEUSE_MAGIC )
						{
							vu2 = BU_LIST_NEXT( vertexuse, &vu2->l );
							continue;
						}
						eu2 = vu2->up.eu_p;
						if( eu2->eumate_p->vu_p->v_p == eu1->eumate_p->vu_p->v_p &&
							eu2->e_p != eu1->e_p )
						{
							struct edge *e1, *e2;

							e1 = eu1->e_p;
							e2 = eu2->e_p;
							nmg_radial_join_eu_NEW( eu2, eu1, tol );
							if( eu1->e_p != e1 )
								bu_ptbl_zero( &edge_table, (long *)e1 );
							if( eu2->e_p != e2 )
								bu_ptbl_zero( &edge_table, (long *)e2 );
						}
						vu2 = BU_LIST_NEXT( vertexuse, &vu2->l );
					}
				}
			}
			else
				continue;
		}
		count += sub_count;
	}

	if( count )
	{
		/* recalculate face planes */
		/* re-use edge table space */
		bu_ptbl_reset( &edge_table );
		nmg_face_tabulate( &edge_table, &m->magic );

		for( i=0 ; i<BU_PTBL_END( &edge_table ) ; i++ )
		{
			struct face *f;
			struct faceuse *fu;

			f = (struct face *)BU_PTBL_GET( &edge_table, i );
			NMG_CK_FACE( f );
			fu = f->fu_p;
			if( fu->orientation != OT_SAME )
				fu = fu->fumate_p;
			if( fu->orientation != OT_SAME )
				bu_bomb( "nmg_edge_collapse: Face has no OT_SAME use!!!\n" );

			nmg_calc_face_g( fu );
		}
	}
	bu_ptbl_free( &edge_table );
	return( count );
}

/**			N M G _ B O T
 *
 *	Convert an NMG to a BOT solid
 */

struct rt_bot_internal *
nmg_bot(struct shell *s, const struct bn_tol *tol)
{
	struct rt_bot_internal	*bot;
	struct bu_ptbl		nmg_vertices;
	struct bu_ptbl		nmg_faces;
	int			i, face_no;
	struct vertex		*v;

	NMG_CK_SHELL( s );
	BN_CK_TOL(tol);

	/* first convert the NMG to triangles */
	(void)nmg_triangulate_shell(s, tol);

	/* make a list of all the vertices */
	nmg_vertex_tabulate( &nmg_vertices, &s->l.magic );

	/* and a list of all the faces */
	nmg_face_tabulate( &nmg_faces, &s->l.magic );

	/* now build the BOT */
	bot = (struct rt_bot_internal *)bu_calloc(1, sizeof( struct rt_bot_internal ), "BOT from NMG" );

	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->bot_flags = 0;

	bot->num_vertices = BU_PTBL_LEN( &nmg_vertices );
	bot->num_faces = 0;

	/* count the number of triangles */
	for( i=0 ; i<BU_PTBL_LEN( &nmg_faces ); i++ )
	{
		struct face *f;
		struct faceuse *fu;
		struct loopuse *lu;

		f = (struct face *)BU_PTBL_GET( &nmg_faces, i );
		NMG_CK_FACE( f );

		fu = f->fu_p;

		if( fu->orientation != OT_SAME )
		{
			fu = fu->fumate_p;
			if( fu->orientation != OT_SAME )
			{
				bu_log( "nmg_bot(): Face has no OT_SAME use!!!!\n" );
				bu_free( (char *)bot->vertices, "BOT vertices" );
				bu_free( (char *)bot->faces, "BOT faces" );
				bu_free( (char *)bot, "BOT" );
				return( (struct rt_bot_internal *)NULL );
			}
		}

		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		{
			if( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
				continue;
			bot->num_faces++;
		}
	}

	bot->faces = (int *)bu_calloc( bot->num_faces * 3, sizeof( int ), "BOT faces" );
	bot->vertices = (fastf_t *)bu_calloc( bot->num_vertices * 3, sizeof( fastf_t ), "BOT vertices" );

	bot->thickness = (fastf_t *)NULL;
	bot->face_mode = (struct bu_bitv *)NULL;

	/* fill in the vertices */
	for( i=0 ; i<BU_PTBL_LEN( &nmg_vertices ) ; i++ )
	{
		struct vertex_g *vg;

		v = (struct vertex *)BU_PTBL_GET( &nmg_vertices, i );
		NMG_CK_VERTEX( v );

		vg = v->vg_p;
		NMG_CK_VERTEX_G( vg );

		VMOVE( &bot->vertices[i*3], vg->coord );
	}

	/* fill in the faces */
	face_no = 0;
	for( i=0 ; i<BU_PTBL_LEN( &nmg_faces ); i++ )
	{
		struct face *f;
		struct faceuse *fu;
		struct loopuse *lu;

		f = (struct face *)BU_PTBL_GET( &nmg_faces, i );
		NMG_CK_FACE( f );

		fu = f->fu_p;

		if( fu->orientation != OT_SAME )
		{
			fu = fu->fumate_p;
			if( fu->orientation != OT_SAME )
			{
				bu_log( "nmg_bot(): Face has no OT_SAME use!!!!\n" );
				bu_free( (char *)bot->vertices, "BOT vertices" );
				bu_free( (char *)bot->faces, "BOT faces" );
				bu_free( (char *)bot, "BOT" );
				return( (struct rt_bot_internal *)NULL );
			}
		}

		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		{
			struct edgeuse *eu;
			int vertex_no=0;

			if( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
				continue;

			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				struct vertex *v;

				if( vertex_no > 2 )
				{
					bu_log( "nmg_bot(): Face has not been triangulated!!!\n" );
					bu_free( (char *)bot->vertices, "BOT vertices" );
					bu_free( (char *)bot->faces, "BOT faces" );
					bu_free( (char *)bot, "BOT" );
					return( (struct rt_bot_internal *)NULL );
				}

				v = eu->vu_p->v_p;
				NMG_CK_VERTEX( v );


				bot->faces[ face_no*3 + vertex_no ] = bu_ptbl_locate( &nmg_vertices, (long *)v );

				vertex_no++;
			}

			face_no++;
		}
	}

	bu_ptbl_free( &nmg_vertices );
	bu_ptbl_free( &nmg_faces );

	return( bot );
}

/**	N M G _ V L I S T _ T O _ E U
 *
 * create wire edges corresponding to the lines in the vlist. The wire edges are
 * created in the specified shell
 *
 */
void
nmg_vlist_to_eu( struct bu_list *vlist, struct shell *s )
{
    point_t pt1, pt2;
    struct bn_vlist *vp;
    struct edgeuse *eu;
    struct vertex *v=NULL;
    struct vertex *polyStartV=NULL;

    for( BU_LIST_FOR( vp, bn_vlist, vlist ) )  {
	register int	i;
	register int	nused = vp->nused;
	register int	*cmd = vp->cmd;
	register point_t *pt = vp->pt;
	for( i = 0; i < nused; i++,cmd++,pt++ )  {
	    switch( *cmd ) {
	    case BN_VLIST_LINE_MOVE:
	    case BN_VLIST_POLY_MOVE:
		VMOVE( pt2, *pt );
		v = NULL;
		polyStartV = NULL;
		break;
	    case BN_VLIST_LINE_DRAW:
	    case BN_VLIST_POLY_DRAW:
		VMOVE( pt1, pt2 );
		VMOVE( pt2, *pt );
		eu = nmg_me( v, NULL, s );
		if( v == NULL ) {
		    nmg_vertex_gv( eu->vu_p->v_p, pt1 );
		}
		nmg_vertex_gv( eu->eumate_p->vu_p->v_p, pt2 );
		v = eu->eumate_p->vu_p->v_p;
		if( polyStartV == NULL ) polyStartV = eu->vu_p->v_p;
		break;
	    case BN_VLIST_POLY_END:
		if( v != NULL &&  polyStartV != NULL ) {
		    eu = nmg_me( v, polyStartV, s );
		}
		break;
	    case BN_VLIST_POLY_START:
		polyStartV = NULL;
		v = NULL;
		break;
	    default:
		break;
	    }
	}
    }
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
