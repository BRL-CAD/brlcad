/*
 *			N M G _ P L O T . C
 *
 *  This file contains routines that create VLISTs and UNIX-plot files.
 *  Some routines are essential to the MGED interface, some are
 *  more for diagnostic and visualization purposes.
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
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
#include <fcntl.h>
#include <signal.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "plot3.h"
#include "rtstring.h"

#define US_DELAY	10	/* Additional delay between frames */

void		(*nmg_plot_anim_upcall)();	/* For I/F with MGED */
void		(*nmg_vlblock_anim_upcall)();	/* For I/F with MGED */
void		(*nmg_mged_debug_display_hack)();
double nmg_eue_dist = 0.05;

/************************************************************************
 *									*
 *			Generic VLBLOCK routines			*
 *									*
 ************************************************************************/

struct rt_vlblock *
rt_vlblock_init()
{
	struct rt_vlblock *vbp;
	int	i;

	GETSTRUCT( vbp, rt_vlblock );
	vbp->magic = RT_VLBLOCK_MAGIC;
	vbp->max = 32;
	vbp->head = (struct rt_list *)rt_calloc( vbp->max,
		sizeof(struct rt_list), "head[]" );
	vbp->rgb = (long *)rt_calloc( vbp->max,
		sizeof(long), "rgb[]" );

	for( i=0; i < vbp->max; i++ )  {
		vbp->rgb[i] = 0;	/* black, unused */
		RT_LIST_INIT( &(vbp->head[i]) );
	}
	vbp->rgb[0] = 0xFFFF00L;	/* Yellow, default */
	vbp->rgb[1] = 0xFFFFFFL;	/* White */
	vbp->nused = 2;

	return(vbp);
}

void
rt_vlblock_free(vbp)
struct rt_vlblock *vbp;
{
	int	i;

	for( i=0; i < vbp->nused; i++ )  {
		/* Release any remaining vlist storage */
		if( vbp->rgb[i] == 0 )  continue;
		if( RT_LIST_IS_EMPTY( &(vbp->head[i]) ) )  continue;
		RT_FREE_VLIST( &(vbp->head[i]) );
	}

	rt_free( (char *)(vbp->head), "head[]" );
	rt_free( (char *)(vbp->rgb), "rgb[]" );
	rt_free( (char *)vbp, "rt_vlblock" );
}

struct rt_list *
rt_vlblock_find( vbp, r, g, b )
struct rt_vlblock *vbp;
int	r, g, b;
{
	long	new;
	int	n;

	new = ((r&0xFF)<<16)|((g&0xFF)<<8)|(b&0xFF);

	/* Map black plots into default color (yellow) */
	if( new == 0 ) return( &(vbp->head[0]) );

	for( n=0; n < vbp->nused; n++ )  {
		if( vbp->rgb[n] == new )
			return( &(vbp->head[n]) );
	}
	if( vbp->nused < vbp->max )  {
		/* Allocate empty slot */
		n = vbp->nused++;
		vbp->rgb[n] = new;
		return( &(vbp->head[n]) );
	}
	/*  RGB does not match any existing entry, and table is full.
	 *  Eventually, enlarge table.
	 *  For now, just default to yellow.
	 */
	return( &(vbp->head[0]) );
}

/************************************************************************
 *									*
 *		NMG to VLIST routines, for MGED interface		*
 * XXX should take a flags array, to ensure each item done only once!   *
 *									*
 ************************************************************************/

/*
 *			N M G _ V U _ T O _ V L I S T
 *
 *  Plot a single vertexuse
 */
void
nmg_vu_to_vlist( vhead, vu )
struct rt_list		*vhead;
CONST struct vertexuse	*vu;
{
	struct vertex	*v;
	register struct vertex_g *vg;

	NMG_CK_VERTEXUSE(vu);
	v = vu->v_p;
	NMG_CK_VERTEX(v);
	vg = v->vg_p;
	if( vg )  {
		/* Only thing in this shell is a point */
		NMG_CK_VERTEX_G(vg);
		RT_ADD_VLIST( vhead, vg->coord, RT_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vhead, vg->coord, RT_VLIST_LINE_DRAW );
	}
}

/*
 *			N M G _ E U _ T O _ V L I S T
 *
 *  Plot a list of edgeuses.  The last edge is joined back to the first.
 */
void
nmg_eu_to_vlist( vhead, eu_hd )
struct rt_list		*vhead;
CONST struct rt_list	*eu_hd;
{
	struct edgeuse		*eu;
	struct edgeuse		*eumate;
	struct vertexuse	*vu;
	struct vertexuse	*vumate;
	register struct vertex_g *vg;
	register struct vertex_g *vgmate;

	/* Consider all the edges in the wire edge list */
	for( RT_LIST_FOR( eu, edgeuse, eu_hd ) )  {
		/* This wire edge runs from vertex to mate's vertex */
		NMG_CK_EDGEUSE(eu);
		vu = eu->vu_p;
		NMG_CK_VERTEXUSE(vu);
		NMG_CK_VERTEX(vu->v_p);
		vg = vu->v_p->vg_p;

		eumate = eu->eumate_p;
		NMG_CK_EDGEUSE(eumate);
		vumate = eumate->vu_p;
		NMG_CK_VERTEXUSE(vumate);
		NMG_CK_VERTEX(vumate->v_p);
		vgmate = vumate->v_p->vg_p;

		if( !vg || !vgmate ) {
			rt_log("nmg_eu_to_vlist() no vg or mate?\n");
			continue;
		}
		NMG_CK_VERTEX_G(vg);
		NMG_CK_VERTEX_G(vgmate);

		RT_ADD_VLIST( vhead, vg->coord, RT_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vhead, vgmate->coord, RT_VLIST_LINE_DRAW );
	}
}

/*
 *			N M G _ L U _ T O _ V L I S T
 *
 *  Plot a single loopuse into a rt_vlist chain headed by vhead.
 */
void
nmg_lu_to_vlist( vhead, lu, poly_markers, normal )
struct rt_list		*vhead;
CONST struct loopuse	*lu;
int			poly_markers;
CONST vectp_t		normal;
{
	CONST struct edgeuse		*eu;
	CONST struct vertexuse		*vu;
	CONST struct vertex		*v;
	register CONST struct vertex_g	*vg;
	CONST struct vertex_g		*first_vg;
	int		isfirst;
	point_t		centroid;
	int		npoints;

	RT_CKMAG(vhead, RT_LIST_HEAD_MAGIC, "struct rt_list");

	NMG_CK_LOOPUSE(lu);
	if( RT_LIST_FIRST_MAGIC(&lu->down_hd)==NMG_VERTEXUSE_MAGIC )  {
		/* Process a loop of a single vertex */
		vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
		nmg_vu_to_vlist( vhead, vu );
		return;
	}

	/* Consider all the edges in the loop */
	isfirst = 1;
	first_vg = (struct vertex_g *)0;
	npoints = 0;
	VSETALL( centroid, 0 );
	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
		/* Consider this edge */
		NMG_CK_EDGEUSE(eu);
		vu = eu->vu_p;
		NMG_CK_VERTEXUSE(vu);
		v = vu->v_p;
		NMG_CK_VERTEX(v);
		vg = v->vg_p;
		if( !vg ) {
			continue;
		}
		NMG_CK_VERTEX_G(vg);
		VADD2( centroid, centroid, vg->coord );
		npoints++;
		if (isfirst) {
			if( poly_markers) {
				/* Insert a "start polygon, normal" marker */
				RT_ADD_VLIST( vhead, normal, RT_VLIST_POLY_START );
				RT_ADD_VLIST( vhead, vg->coord, RT_VLIST_POLY_MOVE );
			} else {
				/* move */
				RT_ADD_VLIST( vhead, vg->coord, RT_VLIST_LINE_MOVE );
			}
			isfirst = 0;
			first_vg = vg;
		} else {
			if( poly_markers) {
				RT_ADD_VLIST( vhead, vg->coord, RT_VLIST_POLY_DRAW );
			} else {
				/* Draw */
				RT_ADD_VLIST( vhead, vg->coord, RT_VLIST_LINE_DRAW );
			}
		}
	}

	/* Draw back to the first vertex used */
	if( !isfirst && first_vg )  {
		if( poly_markers )  {
			/* Draw, end polygon */
			RT_ADD_VLIST( vhead, first_vg->coord, RT_VLIST_POLY_END );
		} else {
			/* Draw */
			RT_ADD_VLIST( vhead, first_vg->coord, RT_VLIST_LINE_DRAW );
		}
	}
	if( poly_markers > 1 && npoints > 2 )  {
		/* Draw surface normal as a little vector */
		double	f;
		vect_t	tocent;
		f = 1.0 / npoints;
		VSCALE( centroid, centroid, f );
		RT_ADD_VLIST( vhead, centroid, RT_VLIST_LINE_MOVE );
		VSUB2( tocent, first_vg->coord, centroid );
		f = MAGNITUDE( tocent ) * 0.5;
		VSCALE( tocent, normal, f );
		VADD2( centroid, centroid, tocent );
		RT_ADD_VLIST( vhead, centroid, RT_VLIST_LINE_DRAW );
	}
}

/*
 *			N M G _ S _ T O _ V L I S T
 *
 *  Plot the entire contents of a shell.
 *
 *  poly_markers =
 *	0 for vectors
 *	1 for polygons
 *	2 for polygons and surface normals drawn with vectors
 */
void
nmg_s_to_vlist( vhead, s, poly_markers )
struct rt_list		*vhead;
CONST struct shell	*s;
int			poly_markers;
{
	struct faceuse	*fu;
	struct face_g	*fg;
	register struct loopuse	*lu;
	vect_t		normal;

	NMG_CK_SHELL(s);

	/* faces */
	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		vect_t		n;

		/* Consider this face */
		NMG_CK_FACEUSE(fu);
		NMG_CK_FACE(fu->f_p);
		fg = fu->f_p->fg_p;
		NMG_CK_FACE_G(fg);
		if (fu->orientation != OT_SAME)  continue;
		NMG_GET_FU_NORMAL( n, fu );
		for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		   	nmg_lu_to_vlist( vhead, lu, poly_markers, n );
		}
	}

	/* wire loops.  poly_markers=0 so wires are always drawn as vectors */
	VSETALL(normal, 0);
	for( RT_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
		nmg_lu_to_vlist( vhead, lu, 0, normal );
	}

	/* wire edges */
	nmg_eu_to_vlist( vhead, &s->eu_hd );

	/* single vertices */
	if (s->vu_p)  {
		nmg_vu_to_vlist( vhead, s->vu_p );
	}
}

/*
 *			N M G _ R _ T O _ V L I S T
 */
void
nmg_r_to_vlist( vhead, r, poly_markers )
struct rt_list		*vhead;
CONST struct nmgregion	*r;
int			poly_markers;
{
	register struct shell	*s;

	NMG_CK_REGION( r );
	for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
		nmg_s_to_vlist( vhead, s, poly_markers );
	}
}

/*
 *			N M G _ M _ T O _ V L I S T
 *
 */
void
nmg_m_to_vlist( vhead, m, poly_markers )
struct rt_list	*vhead;
struct model	*m;
int		poly_markers;
{
	register struct nmgregion	*r;

	NMG_CK_MODEL( m );
	for( RT_LIST_FOR( r, nmgregion, &m->r_hd ) )  {
		NMG_CK_REGION( r );
		nmg_r_to_vlist( vhead, r, poly_markers );
	}
}
/************************************************************************
 *									*
 *		NMG to UNIX-Plot routines, for visualization		*
 *									*
 ************************************************************************/

#define LEE_DIVIDE_TOL	(1.0e-5)	/* sloppy tolerance */

/*
 *			N M G _ O F F S E T _ E U _ C O O R D
 *
 *  Given an edgeuse structure, return the coordinates of
 *  the "base" and "tip" of this edge, suitable for visualization.
 *  These points will be offset inwards along the edge
 *  slightly, to avoid obscuring the vertex, and will be offset off the
 *  face (in the direction of the face normal) slightly, to avoid
 *  obscuring the edge itself.
 *
 *		    final_pt_p		    next_pt_p
 *			* <-------------------- *
 *				final_vec	^
 *						|
 *						|
 *						| edge_vec
 *						|
 *						|
 *				prev_vec	|
 *			* <-------------------- *
 *		    prev_pt_p		     cur_pt_p
 */
static void nmg_offset_eu_coord( base, tip, eu, face_normal )
point_t		base;
point_t		tip;
struct edgeuse	*eu;
vect_t		face_normal;
{
	fastf_t		dist1;
	struct edgeuse	*prev_eu;
	struct edgeuse	*next_eu;
	struct edgeuse	*final_eu;
	vect_t		edge_vec;	/* from cur_pt to next_pt */
	vect_t		prev_vec;	/* from cur_pt to prev_pt */
	vect_t		final_vec;	/* from next_pt to final_pt */
	pointp_t	prev_pt_p;
	pointp_t	cur_pt_p;
	pointp_t	next_pt_p;
	pointp_t	final_pt_p;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEX(eu->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);

	NMG_CK_EDGEUSE(eu->eumate_p);
	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);

	cur_pt_p = eu->vu_p->v_p->vg_p->coord;
	next_pt_p = eu->eumate_p->vu_p->v_p->vg_p->coord;
	VSUB2(edge_vec, next_pt_p, cur_pt_p); 
	VUNITIZE(edge_vec);

	/* find previous vertex in the loop, not colinear with the edge */
	prev_eu = RT_LIST_PLAST_CIRC( edgeuse, eu );
	prev_pt_p = prev_eu->vu_p->v_p->vg_p->coord;
	dist1 = rt_dist_line_point(cur_pt_p, edge_vec, prev_pt_p);
	while (NEAR_ZERO(dist1, LEE_DIVIDE_TOL) && prev_eu != eu) {
		prev_eu = RT_LIST_PLAST_CIRC( edgeuse, prev_eu );
		prev_pt_p = prev_eu->vu_p->v_p->vg_p->coord;
		dist1 = rt_dist_line_point(cur_pt_p, edge_vec, prev_pt_p);
	}

	/* find vertex after "next" in the loop, not colinear with the edge */
	next_eu = RT_LIST_PNEXT_CIRC( edgeuse, eu );
	final_eu = RT_LIST_PNEXT_CIRC( edgeuse, next_eu );
	final_pt_p = final_eu->vu_p->v_p->vg_p->coord;
	dist1 = rt_dist_line_point(cur_pt_p, edge_vec, final_pt_p);
	while (NEAR_ZERO(dist1, LEE_DIVIDE_TOL) && final_eu != eu) {
		final_eu = RT_LIST_PNEXT_CIRC( edgeuse, final_eu );
		final_pt_p = final_eu->vu_p->v_p->vg_p->coord;
		dist1 = rt_dist_line_point(cur_pt_p, edge_vec, final_pt_p);
	}

	VSUB2(prev_vec, prev_pt_p, cur_pt_p);
	VADD2( prev_vec, prev_vec, edge_vec );
	VUNITIZE(prev_vec);

	VSUB3(final_vec, final_pt_p, next_pt_p, edge_vec);
	VUNITIZE(final_vec);

	/* XXX offset vector lengths should be scaled by 5% of face size */

/*	dist1=MAGNITUDE(edge_vec); */
	dist1=0.125;

	VJOIN2(base, cur_pt_p, dist1,prev_vec, nmg_eue_dist,face_normal);
	VJOIN2(tip, next_pt_p, dist1,final_vec, nmg_eue_dist,face_normal);
}

/*	N M G _ O F F S E T _ E U _ V E R T
 *
 *	Given an edgeuse, find an offset for its vertexuse which will place
 *	it "above" and "inside" the area of the face.
 *
 *  The point will be offset inwards along the edge
 *  slightly, to avoid obscuring the vertex, and will be offset off the
 *  face (in the direction of the face normal) slightly, to avoid
 *  obscuring the edge itself.
 *	
 */
void
nmg_offset_eu_vert(base, eu, face_normal, tip)
point_t			base;
CONST struct edgeuse	*eu;
CONST vect_t		face_normal;
int			tip;
{
	struct edgeuse	*prev_eu;
	CONST struct edgeuse	*this_eu;
	vect_t		prev_vec;	/* from cur_pt to prev_pt */
	vect_t		eu_vec;		/* from cur_pt to next_pt */
	vect_t		prev_left;
	vect_t		eu_left;
	vect_t		delta_vec;	/* offset vector from vertex */
	struct vertex_g	*this_vg, *mate_vg, *prev_vg;

	bzero(delta_vec, sizeof(vect_t)),
	prev_eu = RT_LIST_PLAST_CIRC( edgeuse, eu ); 
	this_eu = eu;

	NMG_CK_EDGEUSE(this_eu);
	NMG_CK_VERTEXUSE(this_eu->vu_p);
	NMG_CK_VERTEX(this_eu->vu_p->v_p);
    	this_vg = this_eu->vu_p->v_p->vg_p;
	NMG_CK_VERTEX_G(this_vg);

	NMG_CK_EDGEUSE(this_eu->eumate_p);
	NMG_CK_VERTEXUSE(this_eu->eumate_p->vu_p);
	NMG_CK_VERTEX(this_eu->eumate_p->vu_p->v_p);
    	mate_vg = this_eu->eumate_p->vu_p->v_p->vg_p;
	NMG_CK_VERTEX_G(mate_vg);

	NMG_CK_EDGEUSE(prev_eu);
	NMG_CK_VERTEXUSE(prev_eu->vu_p);
	NMG_CK_VERTEX(prev_eu->vu_p->v_p);
    	prev_vg = prev_eu->vu_p->v_p->vg_p;
	NMG_CK_VERTEX_G(prev_vg);

	/* get "left" vector for edgeuse */
	VSUB2(eu_vec, mate_vg->coord, this_vg->coord); 
	VUNITIZE(eu_vec);
	VCROSS(eu_left, face_normal, eu_vec);


	/* get "left" vector for previous edgeuse */
	VSUB2(prev_vec, this_vg->coord, prev_vg->coord);
	VUNITIZE(prev_vec);
	VCROSS(prev_left, face_normal, prev_vec);

	/* get "delta" vector to apply to vertex */
	VADD2(delta_vec, prev_left, eu_left);

	if (MAGSQ(delta_vec) > VDIVIDE_TOL) {
		VUNITIZE(delta_vec);
		VJOIN2(base, this_vg->coord,
			(nmg_eue_dist*1.3),delta_vec,
			(nmg_eue_dist*0.8),face_normal);
		
	} else if (tip) {
		VJOIN2(base, this_vg->coord,
			(nmg_eue_dist*1.3),prev_left,
			(nmg_eue_dist*0.8),face_normal);
	} else {
		VJOIN2(base, this_vg->coord,
			(nmg_eue_dist*1.3),eu_left,
			(nmg_eue_dist*0.8),face_normal);
	}
}



/*			N M G _ E U _ C O O R D S
 *
 *  Get the two (offset and shrunken) endpoints that represent
 *  an edgeuse.
 *  Return the base point, and a point 60% along the way towards the
 *  other end.
 */
static void nmg_eu_coords(eu, base, tip60)
CONST struct edgeuse *eu;
point_t base, tip60;
{
	point_t	tip;

	NMG_CK_EDGEUSE(eu);

	if (*eu->up.magic_p == NMG_SHELL_MAGIC ||
	    (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
	    *eu->up.lu_p->up.magic_p == NMG_SHELL_MAGIC) ) {
	    	/* Wire edge, or edge in wire loop */
	    	VMOVE( base, eu->vu_p->v_p->vg_p->coord );
		NMG_CK_EDGEUSE(eu->eumate_p);
		VMOVE( tip, eu->eumate_p->vu_p->v_p->vg_p->coord );
	}
	else if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
	    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC) {
	    	/* Loop in face */
	    	struct faceuse	*fu;
	    	vect_t		face_normal;

	    	fu = eu->up.lu_p->up.fu_p;
	    	NMG_GET_FU_NORMAL( face_normal, fu );
#if 0
		nmg_offset_eu_coord(base, tip, eu, face_normal);
#else
	    	nmg_offset_eu_vert(base, eu, face_normal, 0);
		nmg_offset_eu_vert(tip, RT_LIST_PNEXT_CIRC(edgeuse, eu),
			face_normal, 1);
#endif
	} else
		rt_bomb("nmg_eu_coords: bad edgeuse up. What's going on?\n");

	VBLEND2( tip60, 0.4, base, 0.6, tip );
}

/*
 *			N M G _ E U _ R A D I A L
 *
 *  Find location for 80% tip on edgeuse's radial edgeuse.
 */
static void nmg_eu_radial(eu, tip)
CONST struct edgeuse *eu;
point_t tip;
{
	point_t	b2, t2;

	NMG_CK_EDGEUSE(eu->radial_p);
	NMG_CK_VERTEXUSE(eu->radial_p->vu_p);
	NMG_CK_VERTEX(eu->radial_p->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->radial_p->vu_p->v_p->vg_p);

	nmg_eu_coords(eu->radial_p, b2, t2);

	/* find point 80% along other eu where radial pointer should touch */
	VCOMB2( tip, 0.8, t2, 0.2, b2 );
}

/*
 *			N M G _ E U _ L A S T
 *
 *  Find the tip of the last (previous) edgeuse from 'eu'.
 */
static void nmg_eu_last( eu, tip_out )
CONST struct edgeuse	*eu;
point_t		tip_out;
{
	point_t		radial_base;
	point_t		radial_tip;
	point_t		last_base;
	point_t		last_tip;
	point_t		p;
	struct edgeuse	*eulast;

	NMG_CK_EDGEUSE(eu);
	eulast = RT_LIST_PLAST_CIRC( edgeuse, eu );
	NMG_CK_EDGEUSE(eulast);
	NMG_CK_VERTEXUSE(eulast->vu_p);
	NMG_CK_VERTEX(eulast->vu_p->v_p);
	NMG_CK_VERTEX_G(eulast->vu_p->v_p->vg_p);

	nmg_eu_coords(eulast->radial_p, radial_base, radial_tip);

	/* find pt 80% along LAST eu's radial eu where radial ptr touches */
	VCOMB2( p, 0.8, radial_tip, 0.2, radial_base );

	/* get coordinates of last edgeuse */
	nmg_eu_coords(eulast, last_base, last_tip);

	/* Find pt 80% along other eu where last pointer should touch */
	VCOMB2( tip_out, 0.8, last_tip, 0.2, p );
}

/*
 *			N M G _ E U _ N E X T
 *
 *  Return the base of the next edgeuse
 */
static void nmg_eu_next_base( eu, next_base)
CONST struct edgeuse	*eu;
point_t		next_base;
{
	point_t	t2;
	register struct edgeuse	*nexteu;

	NMG_CK_EDGEUSE(eu);
	nexteu = RT_LIST_PNEXT_CIRC( edgeuse, eu );
	NMG_CK_EDGEUSE(nexteu);
	NMG_CK_VERTEXUSE(nexteu->vu_p);
	NMG_CK_VERTEX(nexteu->vu_p->v_p);
	NMG_CK_VERTEX_G(nexteu->vu_p->v_p->vg_p);

	nmg_eu_coords(nexteu, next_base, t2);
}

/*
 *			N M G _ P L _ V
 */
void
nmg_pl_v(fp, v, b)
FILE			*fp;
CONST struct vertex	*v;
long			*b;
{
	pointp_t p;
	static char label[128];

	NMG_INDEX_RETURN_IF_SET_ELSE_SET(b, v->index);

	NMG_CK_VERTEX(v);
	NMG_CK_VERTEX_G(v->vg_p);
	p = v->vg_p->coord;

	pl_color(fp, 255, 255, 255);
	if (rt_g.NMG_debug & DEBUG_LABEL_PTS) {
		(void)sprintf(label, "%g %g %g", p[0], p[1], p[2]);
		pdv_3move( fp, p );
		pl_label(fp, label);
	}
	pdv_3point(fp, p);
}

/*
 *			N M G _ P L _ E
 */
void
nmg_pl_e(fp, e, b, red, green, blue)
FILE			*fp;
CONST struct edge	*e;
long			*b;
int			red, green, blue;
{
	pointp_t	p0, p1;
	point_t		end0, end1;
	vect_t		v;

	NMG_INDEX_RETURN_IF_SET_ELSE_SET(b, e->index);
	
	NMG_CK_EDGEUSE(e->eu_p);
	NMG_CK_VERTEXUSE(e->eu_p->vu_p);
	NMG_CK_VERTEX(e->eu_p->vu_p->v_p);
	NMG_CK_VERTEX_G(e->eu_p->vu_p->v_p->vg_p);
	p0 = e->eu_p->vu_p->v_p->vg_p->coord;

	NMG_CK_VERTEXUSE(e->eu_p->eumate_p->vu_p);
	NMG_CK_VERTEX(e->eu_p->eumate_p->vu_p->v_p);
	NMG_CK_VERTEX_G(e->eu_p->eumate_p->vu_p->v_p->vg_p);
	p1 = e->eu_p->eumate_p->vu_p->v_p->vg_p->coord;

	/* leave a little room between the edge endpoints and the vertex
	 * compute endpoints by forming a vector between verets, scale vector
	 * and modify points
	 */
	VSUB2SCALE(v, p1, p0, 0.95);
	VADD2(end0, p0, v);
	VSUB2(end1, p1, v);

	pl_color(fp, red, green, blue);
	pdv_3line( fp, end0, end1 );

	nmg_pl_v(fp, e->eu_p->vu_p->v_p, b);
	nmg_pl_v(fp, e->eu_p->eumate_p->vu_p->v_p, b);
}

/*
 *			M N G _ P L _ E U
 */
void
nmg_pl_eu(fp, eu, b, red, green, blue)
FILE			*fp;
CONST struct edgeuse	*eu;
long			*b;
int			red, green, blue;
{
	point_t base, tip;
	point_t	radial_tip;
	point_t	next_base;
	point_t	last_tip;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGE(eu->e_p);
	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEX(eu->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);

	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);

	NMG_INDEX_RETURN_IF_SET_ELSE_SET(b, eu->index);

	nmg_pl_e(fp, eu->e_p, b, red, green, blue);

	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
	    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC) {

	    	nmg_eu_coords(eu, base, tip);
	    	if (eu->up.lu_p->up.fu_p->orientation == OT_SAME)
	    		red += 50;
		else if (eu->up.lu_p->up.fu_p->orientation == OT_OPPOSITE)
			red -= 50;
	    	else
	    		red = green = blue = 255;

		pl_color(fp, red, green, blue);
	    	pdv_3line( fp, base, tip );

	    	nmg_eu_radial( eu, radial_tip );
		pl_color(fp, red, green-20, blue);
	    	pdv_3line( fp, tip, radial_tip );

		pl_color(fp, 0, 100, 0);
	    	nmg_eu_next_base( eu, next_base );
	    	pdv_3line( fp, tip, next_base );

/*** presently unused ***
	    	nmg_eu_last( eu, last_tip );
		pl_color(fp, 0, 200, 0);
	    	pdv_3line( fp, base, last_tip );
****/
	    }
}

/*
 *			N M G _ P L _ L U
 */
void
nmg_pl_lu(fp, lu, b, red, green, blue)
FILE			*fp;
CONST struct loopuse	*lu;
long			*b;
int			red, green, blue;
{
	struct edgeuse	*eu;
	long		magic1;

	NMG_CK_LOOPUSE(lu);
	NMG_INDEX_RETURN_IF_SET_ELSE_SET(b, lu->index);

	magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC &&
	    lu->orientation != OT_BOOLPLACE) {
	    	nmg_pl_v(fp, RT_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p, b);
	} else if (magic1 == NMG_EDGEUSE_MAGIC) {
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			nmg_pl_eu(fp, eu, b, red, green, blue);
		}
	}
}

/*
 *			M N G _ P L _ F U
 */
void
nmg_pl_fu(fp, fu, b, red, green, blue)
FILE			*fp;
CONST struct faceuse	*fu;
long			*b;
int			red, green, blue;
{
	struct loopuse *lu;

	NMG_CK_FACEUSE(fu);

	NMG_INDEX_RETURN_IF_SET_ELSE_SET(b, fu->index);

	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		nmg_pl_lu(fp, lu, b, red, green, blue);
	}
}

/*
 *			N M G _ P L _ S
 *
 *  Note that "b" should probably be defined a level higher,
 *  to reduce malloc/free calls when plotting multiple shells.
 */
void
nmg_pl_s(fp, s)
FILE			*fp;
CONST struct shell	*s;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	long		*b;

	NMG_CK_SHELL(s);
	if( s->sa_p )  {
		NMG_CK_SHELL_A( s->sa_p );
		pdv_3space( fp, s->sa_p->min_pt, s->sa_p->max_pt );
	}

	/* get space for flag array, to ensure each item is done once */
	b = (long *)rt_calloc( s->r_p->m_p->maxindex, sizeof(long),
		"nmg_pl_s flag[]" );

	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		nmg_pl_fu(fp, fu, b, 80, 100, 170);
	}

	for( RT_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		nmg_pl_lu(fp, lu, b, 255, 0, 0);
	}

	for( RT_LIST_FOR( eu, edgeuse, &s->eu_hd ) )  {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_EDGE(eu->e_p);

		nmg_pl_eu(fp, eu, b, 200, 200, 0 );
	}
	if (s->vu_p) {
		nmg_pl_v(fp, s->vu_p->v_p, b );
	}

	if( RT_LIST_IS_EMPTY( &s->fu_hd ) &&
	    RT_LIST_IS_EMPTY( &s->lu_hd ) &&
	    RT_LIST_IS_EMPTY( &s->eu_hd ) && !s->vu_p) {
	    	rt_log("WARNING nmg_pl_s(): shell has no children\n");
	}

	rt_free( (char *)b, "nmg_pl_s flag[]" );
}

/*
 *			N M G _ P L _ R
 */
void
nmg_pl_r(fp, r)
FILE			*fp;
CONST struct nmgregion	*r;
{
	struct shell *s;

	for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
		nmg_pl_s(fp, s);
	}
}

/*
 *			N M G _ P L _ M
 */
void
nmg_pl_m(fp, m)
FILE			*fp;
CONST struct model	*m;
{
	struct nmgregion *r;

	for( RT_LIST_FOR( r, nmgregion, &m->r_hd ) )  {
		nmg_pl_r(fp, r);
	}
}

/************************************************************************
 *									*
 *		Visualization as VLIST routines				*
 *									*
 ************************************************************************/

/*
 *			N M G _ V L B L O C K _ V
 */
void
nmg_vlblock_v(vbp, v, tab)
struct rt_vlblock		*vbp;
CONST struct vertex		*v;
long				*tab;
{
	pointp_t p;
	static char label[128];
	struct rt_list	*vh;

	NMG_CK_VERTEX(v);
	NMG_INDEX_RETURN_IF_SET_ELSE_SET( tab, v->index );

	NMG_CK_VERTEX_G(v->vg_p);
	p = v->vg_p->coord;

	vh = rt_vlblock_find( vbp, 255, 255, 255 );
#if 0
	if (rt_g.NMG_debug & DEBUG_LABEL_PTS) {
		(void)sprintf(label, "%g %g %g", p[0], p[1], p[2]);
		pdv_3move( vbp, p );
		pl_label(vbp, label);
	}
#endif
	RT_ADD_VLIST( vh, p, RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vh, p, RT_VLIST_LINE_DRAW );
}

/*
 *			N M G _ V L B L O C K _ E
 */
void
nmg_vlblock_e(vbp, e, tab, red, green, blue, fancy)
struct rt_vlblock	*vbp;
CONST struct edge	*e;
long			*tab;
int			red, green, blue;
int			fancy;
{
	pointp_t p0, p1;
	point_t end0, end1;
	vect_t v;
	struct rt_list	*vh;

	NMG_CK_EDGE(e);
	NMG_INDEX_RETURN_IF_SET_ELSE_SET( tab, e->index );
	
	NMG_CK_EDGEUSE(e->eu_p);
	NMG_CK_VERTEXUSE(e->eu_p->vu_p);
	NMG_CK_VERTEX(e->eu_p->vu_p->v_p);
	NMG_CK_VERTEX_G(e->eu_p->vu_p->v_p->vg_p);
	p0 = e->eu_p->vu_p->v_p->vg_p->coord;

	NMG_CK_VERTEXUSE(e->eu_p->eumate_p->vu_p);
	NMG_CK_VERTEX(e->eu_p->eumate_p->vu_p->v_p);
	NMG_CK_VERTEX_G(e->eu_p->eumate_p->vu_p->v_p->vg_p);
	p1 = e->eu_p->eumate_p->vu_p->v_p->vg_p->coord;

	/* leave a little room between the edge endpoints and the vertex
	 * compute endpoints by forming a vector between verets, scale vector
	 * and modify points
	 */
	VSUB2SCALE(v, p1, p0, 0.90);
	VADD2(end0, p0, v);
	VSUB2(end1, p1, v);

	vh = rt_vlblock_find( vbp, red, green, blue );
	RT_ADD_VLIST( vh, end0, RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vh, end1, RT_VLIST_LINE_DRAW );

	nmg_vlblock_v(vbp, e->eu_p->vu_p->v_p, tab);
	nmg_vlblock_v(vbp, e->eu_p->eumate_p->vu_p->v_p, tab);
}

/*
 *			M N G _ V L B L O C K _ E U
 */
void
nmg_vlblock_eu(vbp, eu, tab, red, green, blue, fancy)
struct rt_vlblock		*vbp;
CONST struct edgeuse		*eu;
long				*tab;
int				red, green, blue;
int				fancy;
{
	point_t base, tip;
	point_t	radial_tip;
	point_t	next_base;
	point_t	last_tip;
	struct rt_list	*vh;

	NMG_CK_EDGEUSE(eu);
	NMG_INDEX_RETURN_IF_SET_ELSE_SET( tab, eu->index );

	NMG_CK_EDGE(eu->e_p);
	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEX(eu->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);

	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);

	nmg_vlblock_e(vbp, eu->e_p, tab, red, green, blue, fancy);

	if( !fancy )  return;

	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
	    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC) {

	    	/* if "fancy" doesn't specify plotting edgeuses of this
	    	 * particular face orientation, return
	    	 */
	    	if ( (eu->up.lu_p->up.fu_p->orientation == OT_SAME &&
	    	     (fancy & 1) == 0) ||
		     (eu->up.lu_p->up.fu_p->orientation == OT_OPPOSITE &&
		     (fancy & 2) == 0) )
	    		return;

	    	nmg_eu_coords(eu, base, tip);
	    	if (eu->up.lu_p->up.fu_p->orientation == OT_SAME)
	    		red += 50;
		else if (eu->up.lu_p->up.fu_p->orientation == OT_OPPOSITE)
			red -= 50;
	    	else
	    		red = green = blue = 255;

		vh = rt_vlblock_find( vbp, red, green, blue );
		RT_ADD_VLIST( vh, base, RT_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vh, tip, RT_VLIST_LINE_DRAW );

	    	nmg_eu_radial( eu, radial_tip );
		vh = rt_vlblock_find( vbp, red, green-20, blue );
		RT_ADD_VLIST( vh, tip, RT_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vh, radial_tip, RT_VLIST_LINE_DRAW );

	    	nmg_eu_next_base( eu, next_base );
		vh = rt_vlblock_find( vbp, 0, 100, 0 );
		RT_ADD_VLIST( vh, tip, RT_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vh, next_base, RT_VLIST_LINE_DRAW );
	}
}

/*
 *			N M G _ V L B L O C K _ L U
 */
void
nmg_vlblock_lu(vbp, lu, tab, red, green, blue, fancy)
struct rt_vlblock	*vbp;
CONST struct loopuse	*lu;
long			*tab;
int			red, green, blue;
int			fancy;
{
	struct edgeuse	*eu;
	long		magic1;
	struct vertexuse *vu;

	NMG_CK_LOOPUSE(lu);
	NMG_INDEX_RETURN_IF_SET_ELSE_SET( tab, lu->index );

	magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC &&
	    lu->orientation != OT_BOOLPLACE) {
	    	vu = RT_LIST_PNEXT(vertexuse, &lu->down_hd);
	    	NMG_CK_VERTEXUSE(vu);
	    	nmg_vlblock_v(vbp, vu->v_p, tab);
	} else if (magic1 == NMG_EDGEUSE_MAGIC) {
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			nmg_vlblock_eu(vbp, eu, tab, red, green, blue, fancy);
		}
	}
}

/*
 *			M N G _ V L B L O C K _ F U
 */
void
nmg_vlblock_fu(vbp, fu, tab, fancy)
struct rt_vlblock	*vbp;
CONST struct faceuse	*fu;
long			*tab;
int			fancy;
{
	struct loopuse *lu;

	NMG_CK_FACEUSE(fu);
	NMG_INDEX_RETURN_IF_SET_ELSE_SET( tab, fu->index );

	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		/* Draw in pale blue / purple */
		if( fancy )  {
			nmg_vlblock_lu(vbp, lu, tab, 80, 100, 170, fancy );
		} else {
			/* Non-fancy */
			nmg_vlblock_lu(vbp, lu, tab, 80, 100, 170, 0 );
		}
	}
}

/*
 *			N M G _ V L B L O C K _ S
 */
void
nmg_vlblock_s(vbp, s, fancy)
struct rt_vlblock	*vbp;
CONST struct shell	*s;
int			fancy;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct model	*m;
	long		*tab;

	NMG_CK_SHELL(s);
	m = s->r_p->m_p;
	NMG_CK_MODEL(m);

	/* get space for list of items processed */
	tab = (long *)rt_calloc( m->maxindex+1, sizeof(long),
		"nmg_vlblock_s tab[]");

	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		nmg_vlblock_fu(vbp, fu, tab, fancy );
	}

	for( RT_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		if( fancy ) {
			nmg_vlblock_lu(vbp, lu, tab, 255, 0, 0, fancy);
		} else {
			/* non-fancy, wire loops in red */
			nmg_vlblock_lu(vbp, lu, tab, 200, 0, 0, 0);
		}
	}

	for( RT_LIST_FOR( eu, edgeuse, &s->eu_hd ) )  {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_EDGE(eu->e_p);

		if( fancy )  {
			nmg_vlblock_eu(vbp, eu, tab, 200, 200, 0, fancy );
		} else {
			/* non-fancy, wire edges in yellow */
			nmg_vlblock_eu(vbp, eu, tab, 200, 200, 0, 0 ); 
		}
	}
	if (s->vu_p) {
		nmg_vlblock_v(vbp, s->vu_p->v_p, tab );
	}

	rt_free( (char *)tab, "nmg_vlblock_s tab[]" );
}

/*
 *			N M G _ V L B L O C K _ R
 */
void
nmg_vlblock_r(vbp, r, fancy)
struct rt_vlblock	*vbp;
CONST struct nmgregion	*r;
int			fancy;
{
	struct shell *s;

	for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
		nmg_vlblock_s(vbp, s, fancy);
	}
}

/*
 *			N M G _ V L B L O C K _ M
 */
void
nmg_vlblock_m(vbp, m, fancy)
struct rt_vlblock	*vbp;
CONST struct model	*m;
int			fancy;
{
	struct nmgregion *r;

	for( RT_LIST_FOR( r, nmgregion, &m->r_hd ) )  {
		nmg_vlblock_r(vbp, r, fancy);
	}
}

/************************************************************************
 *									*
 *		Visualization helper routines				*
 *									*
 ************************************************************************/

/*
 *  Plot all edgeuses around an edge
 */
void
nmg_pl_around_edge(fd, b, eu)
FILE			*fd;
long			*b;
CONST struct edgeuse	*eu;
{
	CONST struct edgeuse *eur = eu;
	do {
		NMG_CK_EDGEUSE(eur);
		nmg_pl_eu(fd, eur, b, 180, 180, 180);
		eur = eur->radial_p->eumate_p;
	} while (eur != eu);
}

/*
 *  If another use of this edge is in another shell, plot all the
 *  uses around this edge.
 */
void
nmg_pl_edges_in_2_shells(fd, b, eu)
FILE			*fd;
long			*b;
CONST struct edgeuse	*eu;
{
	CONST struct edgeuse	*eur;
	CONST struct shell	*s;

	eur = eu;
	NMG_CK_EDGEUSE(eu);
	NMG_CK_LOOPUSE(eu->up.lu_p);
	NMG_CK_FACEUSE(eu->up.lu_p->up.fu_p);
	s = eu->up.lu_p->up.fu_p->s_p;
	NMG_CK_SHELL(s);

	do {
		NMG_CK_EDGEUSE(eur);

		if (*eur->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    *eur->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
		    eur->up.lu_p->up.fu_p->s_p != s) {
			nmg_pl_around_edge(fd, b, eu);
		    	break;
		    }

		eur = eur->radial_p->eumate_p;
	} while (eur != eu);
}

void
nmg_pl_isect(filename, s)
CONST char		*filename;
CONST struct shell	*s;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	long		*b;
	FILE	*fp;
	long	magic1;

	NMG_CK_SHELL(s);

	if ((fp=fopen(filename, "w")) == (FILE *)NULL) {
		(void)perror(filename);
		exit(-1);
	}

	b = (long *)rt_calloc( s->r_p->m_p->maxindex+1, sizeof(long),
		"nmg_pl_isect flags[]" );

	rt_log("Plotting to \"%s\"\n", filename);
	if( s->sa_p )  {
		NMG_CK_SHELL_A( s->sa_p );
		pdv_3space( fp, s->sa_p->min_pt, s->sa_p->max_pt );
	}

	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
			NMG_CK_LOOPUSE(lu);
			magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
			if (magic1 == NMG_EDGEUSE_MAGIC) {
				for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
					NMG_CK_EDGEUSE(eu);
					nmg_pl_edges_in_2_shells(fp, b, eu);
				}
			} else if (magic1 == NMG_VERTEXUSE_MAGIC) {
				;
			} else {
				rt_bomb("nmg_pl_isect() bad loopuse down\n");
			}
		}
	}
	rt_free( (char *)b, "nmg_pl_isect flags[]" );

	(void)fclose(fp);
}

/*
 *			N M G _ P L _ C O M B _ F U
 *
 *  Called from nmg_bool.c/nmg_face_combine()
 */
void
nmg_pl_comb_fu( num1, num2, fu1 )
int	num1;
int	num2;
CONST struct faceuse	*fu1;
{
	FILE	*fp;
	char	name[64];
	int	do_plot = 0;
	int	do_anim = 0;
	struct model	*m;
	long	*tab;

	if(rt_g.NMG_debug & DEBUG_PLOTEM &&
	   rt_g.NMG_debug & DEBUG_FCUT ) do_plot = 1;
	if( rt_g.NMG_debug & DEBUG_PL_ANIM )  do_anim = 1;

	if( !do_plot && !do_anim )  return;

	m = nmg_find_model( &fu1->l.magic );
	NMG_CK_MODEL(m);
	/* get space for list of items processed */
	tab = (long *)rt_calloc( m->maxindex+1, sizeof(long),
		"nmg_pl_comb_fu tab[]");

	if( do_plot )  {
	    	(void)sprintf(name, "comb%d.%d.pl", num1, num2);
		if ((fp=fopen(name, "w")) == (FILE *)NULL) {
			(void)perror(name);
			return;
		}
		rt_log("Plotting %s\n", name);
		nmg_pl_fu(fp, fu1, tab, 200, 200, 200);
		(void)fclose(fp);
	}

	if( do_anim )  {
		struct rt_vlblock	*vbp;

		vbp = rt_vlblock_init();

		nmg_vlblock_fu(vbp, fu1, tab, 3);

		if( nmg_vlblock_anim_upcall )  {
			(*nmg_vlblock_anim_upcall)( vbp,
				(rt_g.NMG_debug&DEBUG_PL_SLOW) ? US_DELAY : 0,
				0 );
		} else {
			rt_log("null nmg_vlblock_anim_upcall, no animation\n");
		}
		rt_vlblock_free(vbp);
	}
	rt_free( (char *)tab, "nmg_pl_comb_fu tab[]" );
}

/*
 *			N M G _ P L _ 2 F U
 *
 *  Note that 'str' is expected to contain a %d to place the frame number.
 *
 *  Called from nmg_isect_2faces and other places.
 */
void
nmg_pl_2fu( str, num, fu1, fu2, show_mates )
CONST char		*str;
int			num;
CONST struct faceuse	*fu1;
CONST struct faceuse	*fu2;
int			show_mates;
{
	FILE		*fp;
	char		name[32];
	struct model	*m;
	long		*tab;

	m = nmg_find_model( &fu1->l.magic );
	NMG_CK_MODEL(m);
	/* get space for list of items processed */
	tab = (long *)rt_calloc( m->maxindex+1, sizeof(long),
		"nmg_pl_comb_fu tab[]");

	if( rt_g.NMG_debug & DEBUG_PLOTEM )  {
		(void)sprintf(name, str, num);
		rt_log("plotting to %s\n", name);
		if ((fp=fopen(name, "w")) == (FILE *)NULL)  {
			perror(name);
			return;
		}

		nmg_pl_fu(fp, fu1, tab, 100, 100, 180);
		if( show_mates )
			nmg_pl_fu(fp, fu1->fumate_p, tab, 100, 100, 180);

		nmg_pl_fu(fp, fu2, tab, 100, 100, 180);
		if( show_mates )
			nmg_pl_fu(fp, fu2->fumate_p, tab, 100, 100, 180);

		(void)fclose(fp);
	}

	if( rt_g.NMG_debug & DEBUG_PL_ANIM )  {
		struct rt_vlblock	*vbp;

		vbp = rt_vlblock_init();

		nmg_vlblock_fu( vbp, fu1, tab, 3);
		if( show_mates )
			nmg_vlblock_fu( vbp, fu1->fumate_p, tab, 3);

		nmg_vlblock_fu( vbp, fu2, tab, 3);
		if( show_mates )
			nmg_vlblock_fu( vbp, fu2->fumate_p, tab, 3);

		/* Cause animation of boolean operation as it proceeds! */
		if( nmg_vlblock_anim_upcall )  {
			(*nmg_vlblock_anim_upcall)( vbp,
				(rt_g.NMG_debug&DEBUG_PL_SLOW) ? US_DELAY : 0,
				0 );
		}
		rt_vlblock_free(vbp);
	}
	rt_free( (char *)tab, "nmg_pl_2fu tab[]" );
}


int		nmg_class_nothing_broken=1;
static long	**global_classlist;
static struct rt_vlblock *vbp_old;
static long	*broken_tab;
static int 	broken_color;
static unsigned char broken_colors[][3] = {
	{ 100, 100, 255 },	/* NMG_CLASS_AinB (bright blue) */
	{ 255,  50,  50 },	/* NMG_CLASS_AonBshared (red) */
	{ 255,  50, 255 }, 	/* NMG_CLASS_AonBanti (magenta) */
	{  50, 255,  50 },	/* NMG_CLASS_AoutB (bright green) */
	{ 255, 255, 255 },	/* UNKNOWN (white) */
	{ 255, 255, 125 }	/* no classification list (cyan) */
};
#define PICK_BROKEN_COLOR(p) { \
	if (global_classlist == (long **)NULL) { \
		broken_color = 5; \
	} else if( NMG_INDEX_TEST(global_classlist[NMG_CLASS_AinB], (p)) ) \
		broken_color = NMG_CLASS_AinB; \
	else if( NMG_INDEX_TEST(global_classlist[NMG_CLASS_AonBshared], (p)) ) \
		broken_color = NMG_CLASS_AonBshared; \
	else if( NMG_INDEX_TEST(global_classlist[NMG_CLASS_AonBanti], (p)) ) \
		broken_color = NMG_CLASS_AonBanti; \
	else if ( NMG_INDEX_TEST(global_classlist[NMG_CLASS_AoutB], (p)) ) \
		broken_color = NMG_CLASS_AoutB; \
	else \
		broken_color = 4;}

/*
 *			S H O W _ B R O K E N _ V U
 */
static void
show_broken_vu(vbp, vu, fancy)
struct rt_vlblock *vbp;
int fancy;
CONST struct vertexuse *vu;
{
	pointp_t p;
	static char label[128];
	struct rt_list	*vh;
	struct vertex *v;
	point_t pt;

	NMG_CK_VERTEXUSE(vu);
	v = vu->v_p;
	NMG_CK_VERTEX(v);
	NMG_CK_VERTEX_G(v->vg_p);

	NMG_INDEX_RETURN_IF_SET_ELSE_SET( broken_tab, v->index );

	NMG_CK_VERTEX_G(v->vg_p);
	p = v->vg_p->coord;

	PICK_BROKEN_COLOR(vu->v_p);
	if (broken_color == 4) {
/*		fprintf(stderr, "vertex broken_color %d...", broken_color); */
		PICK_BROKEN_COLOR(vu);
/*		fprintf(stderr, "vertexuse broken_color %d\n", broken_color); */
	}
	vh = rt_vlblock_find( vbp, 
		broken_colors[broken_color][0], broken_colors[broken_color][1], broken_colors[broken_color][2]);

	RT_ADD_VLIST( vh, p, RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vh, p, RT_VLIST_LINE_DRAW );


	VMOVE(pt, p);
	pt[0] += 0.05;
	RT_ADD_VLIST( vh, pt, RT_VLIST_LINE_MOVE );
	VMOVE(pt, p);
	pt[0] -= 0.05;
	RT_ADD_VLIST( vh, pt, RT_VLIST_LINE_DRAW );

	VMOVE(pt, p);
	pt[1] += 0.05;
	RT_ADD_VLIST( vh, pt, RT_VLIST_LINE_MOVE );
	VMOVE(pt, p);
	pt[1] -= 0.05;
	RT_ADD_VLIST( vh, pt, RT_VLIST_LINE_DRAW );

	VMOVE(pt, p);
	pt[2] += 0.05;
	RT_ADD_VLIST( vh, pt, RT_VLIST_LINE_MOVE );
	VMOVE(pt, p);
	pt[2] -= 0.05;
	RT_ADD_VLIST( vh, pt, RT_VLIST_LINE_DRAW );

	RT_ADD_VLIST( vh, p, RT_VLIST_LINE_MOVE );
}

static void
show_broken_e(vbp, eu, fancy)
struct rt_vlblock *vbp;
int fancy;
CONST struct edgeuse *eu;
{
	pointp_t p0, p1;
	point_t end0, end1;
	vect_t v;
	struct rt_list	*vh;

	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEX(eu->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);


	p0 = eu->vu_p->v_p->vg_p->coord;
	p1 = eu->eumate_p->vu_p->v_p->vg_p->coord;

	/* leave a little room between the edge endpoints and the vertex
	 * compute endpoints by forming a vector between verets, scale vector
	 * and modify points
	 */
	VSUB2SCALE(v, p1, p0, 0.90);
	VADD2(end0, p0, v);
	VSUB2(end1, p1, v);


	PICK_BROKEN_COLOR(eu->e_p);
	if (broken_color == 4) {
/*		fprintf(stderr, "edge broken_color %d... ", broken_color); */
		PICK_BROKEN_COLOR(eu);
/*		fprintf(stderr, "edgeuse broken_color %d\n", broken_color); */
	}

	vh = rt_vlblock_find( vbp, 
		broken_colors[broken_color][0], broken_colors[broken_color][1], broken_colors[broken_color][2]);

	RT_ADD_VLIST( vh, end0, RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vh, end1, RT_VLIST_LINE_DRAW );

	show_broken_vu(vbp, eu->vu_p, fancy);
	show_broken_vu(vbp, eu->eumate_p->vu_p, fancy);

}


static void
show_broken_eu(vbp, eu, fancy)
struct rt_vlblock *vbp;
int fancy;
CONST struct edgeuse *eu;
{
	vect_t v;
	struct rt_list	*vh;
    	int red, green, blue;
	point_t base, tip;
	point_t	radial_tip;
	point_t	next_base;
	point_t	last_tip;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGE(eu->e_p);

	if ( ! NMG_INDEX_TEST_AND_SET(broken_tab, eu->e_p) )
		show_broken_e(vbp, eu, fancy);

	if (!fancy) return;

	/* paint the edgeuse lines */
	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
	    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC) {

	    	red = broken_colors[broken_color][0];
	    	green = broken_colors[broken_color][1];
	    	blue = broken_colors[broken_color][2];

	    	nmg_eu_coords(eu, base, tip);
	    	if (eu->up.lu_p->up.fu_p->orientation == OT_SAME)
	    		red += 50;
		else if (eu->up.lu_p->up.fu_p->orientation == OT_OPPOSITE)
			red -= 50;
	    	else
	    		red = green = blue = 255;

		vh = rt_vlblock_find( vbp, red, green, blue );
		RT_ADD_VLIST( vh, base, RT_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vh, tip, RT_VLIST_LINE_DRAW );

	    	nmg_eu_radial( eu, radial_tip );
		vh = rt_vlblock_find( vbp, red, green-20, blue );
		RT_ADD_VLIST( vh, tip, RT_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vh, radial_tip, RT_VLIST_LINE_DRAW );

	    	nmg_eu_next_base( eu, next_base );
		vh = rt_vlblock_find( vbp, 0, 100, 0 );
#if 0
		RT_ADD_VLIST( vh, tip, RT_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vh, next_base, RT_VLIST_LINE_DRAW );
#else
	    	{
	    		register struct rt_vlist *_vp = RT_LIST_LAST( rt_vlist, (vh) );
	    		if( RT_LIST_IS_HEAD( _vp, (vh) ) || _vp->nused >= RT_VLIST_CHUNK ) {
#if 0
	    			RT_GET_VLIST(_vp);
#else
	    			_vp = RT_LIST_FIRST( rt_vlist, &rt_g.rtg_vlfree );
	    			if( RT_LIST_IS_HEAD( _vp, &rt_g.rtg_vlfree ) ) {
					_vp = (struct rt_vlist *)rt_malloc(sizeof(struct rt_vlist), "rt_vlist");
					_vp->l.magic = RT_VLIST_MAGIC;
				} else {
					RT_LIST_DEQUEUE( &(_vp->l) );
				}
				_vp->nused = 0;
#endif
	    			RT_LIST_INSERT( (vh), &(_vp->l) );
	    		}
	    		VMOVE( _vp->pt[_vp->nused], (tip) );
	    		_vp->cmd[_vp->nused++] = (RT_VLIST_LINE_MOVE);
	    	}
	    	{
	    		register struct rt_vlist *_vp = RT_LIST_LAST( rt_vlist, (vh) );
	    		if( RT_LIST_IS_HEAD( _vp, (vh) ) || _vp->nused >= RT_VLIST_CHUNK ) {
	    			RT_GET_VLIST(_vp);
	    			RT_LIST_INSERT( (vh), &(_vp->l) );
	    		}
	    		VMOVE( _vp->pt[_vp->nused], (next_base) );
	    		_vp->cmd[_vp->nused++] = (RT_VLIST_LINE_DRAW);
	    	}
#endif
	}

}

static void
show_broken_lu(vbp, lu, fancy)
struct rt_vlblock *vbp;
int fancy;
CONST struct loopuse *lu;
{
	register struct edgeuse *eu;
	struct rt_list	*vh;
	vect_t		n;

	NMG_CK_LOOPUSE(lu);

	if( RT_LIST_FIRST_MAGIC(&lu->down_hd)==NMG_VERTEXUSE_MAGIC )  {
		register struct vertexuse *vu;
		vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
		show_broken_vu(vbp, vu, fancy);
		return;
	}

	if (rt_g.NMG_debug & DEBUG_GRAPHCL)  {
		for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd))
			show_broken_eu(vbp, eu, fancy);
	}

	/* Draw colored polygons for the actual face loops */
	/* Faces are not classified, only loops */
	/* This can obscure the edge/vertex info */
	PICK_BROKEN_COLOR(lu->l_p);
	vh = rt_vlblock_find( vbp, 
		broken_colors[broken_color][0], broken_colors[broken_color][1], broken_colors[broken_color][2]);

	if( *lu->up.magic_p == NMG_FACEUSE_MAGIC )  {
		NMG_GET_FU_NORMAL( n, lu->up.fu_p );
	} else {
		/* For wire loops, use a constant normal */
		VSET( n, 0, 0, 1 );
	}

	if ((rt_g.NMG_debug & (DEBUG_GRAPHCL|DEBUG_PL_LOOP)) == (DEBUG_PL_LOOP) ) {
		/* If only DEBUG_PL_LOOP set, just draw lu as wires */
		nmg_lu_to_vlist( vh, lu, 0, n );
	} else if ((rt_g.NMG_debug & (DEBUG_GRAPHCL|DEBUG_PL_LOOP)) == (DEBUG_GRAPHCL|DEBUG_PL_LOOP) ) {
		/* Draw as polygons if both set */
		nmg_lu_to_vlist( vh, lu, 1, n );
	} else {
		/* If only DEBUG_GRAPHCL set, don't draw lu's at all */
	}
}



static void
show_broken_fu(vbp, fu, fancy)
struct rt_vlblock *vbp;
int fancy;
CONST struct faceuse *fu;
{
	register struct loopuse *lu;

	NMG_CK_FACEUSE(fu);
	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		show_broken_lu(vbp, lu, fancy);
	}
}

static void
show_broken_s(vbp, s, fancy)
struct rt_vlblock *vbp;
CONST struct shell *s;
int fancy;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct vertexuse *vu;

	NMG_CK_SHELL(s);
	for ( RT_LIST_FOR(fu, faceuse, &s->fu_hd ))
		show_broken_fu(vbp, fu, fancy);
	for ( RT_LIST_FOR(lu, loopuse, &s->lu_hd ))
		show_broken_lu(vbp, lu, fancy);
	for ( RT_LIST_FOR(eu, edgeuse, &s->eu_hd ))
		show_broken_eu(vbp, eu, fancy);
	if ( s->vu_p )
		show_broken_vu(vbp, s->vu_p, fancy);
}
static void
show_broken_r(vbp, r, fancy)
struct rt_vlblock *vbp;
CONST struct nmgregion *r;
int fancy;
{
	register struct shell *s;

	NMG_CK_REGION(r);
	for ( RT_LIST_FOR(s, shell, & r->s_hd))
		show_broken_s(vbp, s, fancy);
}

static void
show_broken_m(vbp, m, fancy)
struct rt_vlblock *vbp;
CONST struct model *m;
int fancy;
{
	register struct nmgregion *r;

	NMG_CK_MODEL(m);
	for (RT_LIST_FOR(r, nmgregion, &m->r_hd))
		show_broken_r(vbp, r, fancy);
}

static struct rt_vlblock *vbp = (struct rt_vlblock *)NULL;
static int stepalong = 0;

void
sigstepalong(i)
int i;
{
	stepalong=1;
}

void
show_broken_stuff(p, classlist, all_new, fancy, a_string)
long	*classlist[4];
long	*p;
int	all_new;
CONST char	*a_string;
{
	struct model *m;

/*	printf("showing broken stuff\n"); */

	global_classlist = classlist;

	nmg_class_nothing_broken = 0;

	if (!vbp)
		vbp = rt_vlblock_init();
	else if (all_new) {
		rt_vlblock_free(vbp);
		vbp = (struct rt_vlblock *)NULL;
		vbp = rt_vlblock_init();
	}

	m = nmg_find_model(p);
	/* get space for list of items processed */
	if (!broken_tab) {
		broken_tab = (long *)rt_calloc( m->maxindex+1, sizeof(long),
			"nmg_vlblock_s tab[]");
	} else if (all_new) {
		bzero(broken_tab,  (m->maxindex+1) * sizeof(long));
	}


	switch (*p) {
	case NMG_MODEL_MAGIC:
		show_broken_m( vbp, (struct model *)p, fancy);
		break;
	case NMG_REGION_MAGIC:
		show_broken_r( vbp, (struct nmgregion *)p, fancy);
		break;
	case NMG_SHELL_MAGIC:
		show_broken_s( vbp, (struct shell *)p, fancy);
		break;
	case NMG_FACE_MAGIC:
		show_broken_fu( vbp, ((struct face *)p)->fu_p, fancy);
		break;
	case NMG_FACEUSE_MAGIC:
		show_broken_fu( vbp, (struct faceuse *)p, fancy);
#if 0
		{
			struct rt_vlblock *vbp2 = vbp;
			register struct loopuse *lu;
			struct faceuse *fu = (struct faceuse *)p;
			int i;
			void            (*cur_sigint)();

			cur_sigint = signal(SIGINT, sigstepalong);
			for (stepalong=0;!stepalong;) {
				for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
					show_broken_stuff(lu, classlist, 1, fancy);
					for (i=0 ; ++i ; );
				}
			}
			signal(SIGINT, cur_sigint);

			show_broken_fu( vbp, (struct faceuse *)p, fancy);
		}
#endif
		break;
	case NMG_LOOPUSE_MAGIC:
		show_broken_lu( vbp, (struct loopuse *)p, fancy);
		break;
	case NMG_EDGE_MAGIC:
		 show_broken_eu( vbp, ((struct edge *)p)->eu_p, fancy);
		 break;
	case NMG_EDGEUSE_MAGIC:
		show_broken_eu( vbp, (struct edgeuse *)p, fancy);
		break;
	case NMG_VERTEXUSE_MAGIC:
		show_broken_vu( vbp, (struct vertexuse *)p, fancy);
		break;
	default: fprintf(stderr, "Unknown magic number %ld %0x %ld %0x\n", *p, *p, p, p);
				break;
	}

	/* Cause animation of boolean operation as it proceeds! */
	/* The "copy" flag on nmg_vlblock_anim_upcall() means that
	 * the vlist will remain, undisturbed, for further use. */
	if( nmg_vlblock_anim_upcall )  {
		struct rt_vlblock *vbp2 = vbp;
		register struct loopuse *lu;
		struct faceuse *fu = (struct faceuse *)p;
		int i;
		void            (*cur_sigint)();

		if (!a_string) {
			(*nmg_vlblock_anim_upcall)( vbp,
				(rt_g.NMG_debug&DEBUG_PL_SLOW) ? US_DELAY : 0,
				1 );
		} else {

			fprintf(stderr, "NMG Intermediate display Ctrl-C to continue (%s)\n", a_string);
			cur_sigint = signal(SIGINT, sigstepalong);
			(*nmg_vlblock_anim_upcall)( vbp,
				(rt_g.NMG_debug&DEBUG_PL_SLOW) ? US_DELAY : 0,
				1 );
			for (stepalong = 0; !stepalong ; ) {
				(*nmg_mged_debug_display_hack)();
			}
			signal(SIGINT, cur_sigint);
			fprintf(stderr, "Continuing\n");
		}
	} else {
		rt_vlblock_free(vbp);
		vbp = (struct rt_vlblock *)NULL;
		rt_free((char *)broken_tab, "broken_tab");
		broken_tab = (long *)NULL;
	}
}

/*
 *			N M G _ F A C E _ P L O T
 */
void
nmg_face_plot( fu )
CONST struct faceuse	*fu;
{
	extern void (*nmg_vlblock_anim_upcall)();
	struct model		*m;
	struct rt_vlblock	*vbp;
	struct face_g	*fg;
	long		*tab;
	int		fancy;

	if( ! (rt_g.NMG_debug & DEBUG_PL_ANIM) )  return;

	NMG_CK_FACEUSE(fu);

	m = nmg_find_model( (long *)fu );
	NMG_CK_MODEL(m);

	/* get space for list of items processed */
	tab = (long *)rt_calloc( m->maxindex+1, sizeof(long),
		"nmg_face_plot tab[]");

	vbp = rt_vlblock_init();

	fancy = 3;	/* show both types of edgeuses */
	nmg_vlblock_fu(vbp, fu, tab, fancy );

	/* Cause animation of boolean operation as it proceeds! */
	if( nmg_vlblock_anim_upcall )  {
		/* if requested, delay 3/4 second */
		(*nmg_vlblock_anim_upcall)( vbp,
			(rt_g.NMG_debug&DEBUG_PL_SLOW) ? 750000 : 0,
			0 );
	} else {
		rt_log("null nmg_vlblock_anim_upcall, no animation\n");
	}
	rt_vlblock_free(vbp);
	rt_free( (char *)tab, "nmg_face_plot tab[]" );

}

/*
 *			N M G _ 2 F A C E _ P L O T
 *
 *  Just like nmg_face_plot, except it draws two faces each iteration.
 */
void
nmg_2face_plot( fu1, fu2 )
CONST struct faceuse	*fu1, *fu2;
{
	extern void (*nmg_vlblock_anim_upcall)();
	struct model		*m;
	struct rt_vlblock	*vbp;
	struct face_g	*fg;
	long		*tab;
	int		fancy;

	if( ! (rt_g.NMG_debug & DEBUG_PL_ANIM) )  return;

	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

	m = nmg_find_model( (long *)fu1 );
	NMG_CK_MODEL(m);

	/* get space for list of items processed */
	tab = (long *)rt_calloc( m->maxindex+1, sizeof(long),
		"nmg_2face_plot tab[]");

	vbp = rt_vlblock_init();

	fancy = 3;	/* show both types of edgeuses */
	nmg_vlblock_fu(vbp, fu1, tab, fancy );
	nmg_vlblock_fu(vbp, fu2, tab, fancy );

	/* Cause animation of boolean operation as it proceeds! */
	if( nmg_vlblock_anim_upcall )  {
		/* if requested, delay 3/4 second */
		(*nmg_vlblock_anim_upcall)( vbp,
			(rt_g.NMG_debug&DEBUG_PL_SLOW) ? 750000 : 0,
			0 );
	} else {
		rt_log("null nmg_vlblock_anim_upcall, no animation\n");
	}
	rt_vlblock_free(vbp);
	rt_free( (char *)tab, "nmg_2face_plot tab[]" );

}

/*
 *			N M G _ F A C E _ L U _ P L O T
 *
 *  Plot the loop, and a ray from vu1 to vu2.
 */
void
nmg_face_lu_plot( lu, vu1, vu2 )
CONST struct loopuse		*lu;
CONST struct vertexuse		*vu1, *vu2;
{
	FILE	*fp;
	struct model	*m;
	long		*b;
	char		buf[128];
	static int	num = 0;
	vect_t		dir;
	point_t		p1, p2;

	if(!(rt_g.NMG_debug&DEBUG_PLOTEM)) return;

	NMG_CK_LOOPUSE(lu);
	NMG_CK_VERTEXUSE(vu1);
	NMG_CK_VERTEXUSE(vu2);

	m = nmg_find_model((long *)lu);
	sprintf(buf, "loop%d.pl", num++ );

	fp = fopen(buf, "w");
	b = (long *)rt_calloc( m->maxindex, sizeof(long), "nmg_face_lu_plot flag[]" );
	nmg_pl_lu(fp, lu, b, 255, 0, 0);

	/* A yellow line for the ray.  Overshoot edge by +/-10%, for visibility. */
	pl_color(fp, 255, 255, 0);
	VSUB2( dir, vu2->v_p->vg_p->coord, vu1->v_p->vg_p->coord );
	VJOIN1( p1, vu1->v_p->vg_p->coord, -0.1, dir );
	VJOIN1( p2, vu1->v_p->vg_p->coord,  1.1, dir );
	pdv_3line(fp, p1, p2 );

	fclose(fp);
	rt_log("wrote %s\n", buf);
	rt_free( (char *)b, "nmg_face_lu_plot flag[]" );
}

/*
 *			N M G _ P L O T _ R A Y _ F A C E
 */
void
nmg_plot_ray_face(fname, pt, dir, fu)
CONST char *fname;
point_t pt;
CONST vect_t dir;
CONST struct faceuse *fu;
{
	FILE *fd;
	long *b;
	point_t pp;
	static int i=0;
	char name[1024];

	if ( ! (rt_g.NMG_debug & DEBUG_NMGRT) )
		return;

	sprintf(name, "%s%0d.pl", fname, i++);
	if ((fd = fopen(name, "w")) == (FILE *)NULL) {
		rt_log("plot_ray_face cannot open %s", name);
		rt_bomb("aborting");
	}

	b = (long *)rt_calloc( fu->s_p->r_p->m_p->maxindex, sizeof(long), "bit vec");

	nmg_pl_fu(fd, fu, b, 200, 200, 200);

	rt_free((char *)b, "bit vec");

	VSCALE(pp, dir, 1000.0);
	VADD2(pp, pt, pp);
	pdv_3line( fd, pt, pp );
}

