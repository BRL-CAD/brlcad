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
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nmg.h"
#include "plot3.h"

#define US_DELAY	10	/* Additional delay between frames */

#define NMG_TAB_RETURN_IF_SET_ELSE_SET(_tab,_index)	\
	{ if( (_tab)[_index] )  return; \
	  else (_tab)[_index] = 1; }

void		(*nmg_plot_anim_upcall)();	/* For I/F with MGED */
void		(*nmg_vlblock_anim_upcall)();	/* For I/F with MGED */

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
nmg_vu_to_vlist( vhead, vu )
struct rt_list		*vhead;
struct vertexuse	*vu;
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
nmg_eu_to_vlist( vhead, eu_hd )
struct rt_list	*vhead;
struct rt_list	*eu_hd;
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
 *  Plot a list of loopuses.
 */
nmg_lu_to_vlist( vhead, lu_hd, poly_markers, normal )
struct rt_list	*vhead;
struct rt_list	*lu_hd;
int		poly_markers;
vectp_t		normal;
{
	struct loopuse	*lu;
	struct edgeuse	*eu;
	struct vertexuse *vu;
	struct vertex	*v;
	register struct vertex_g *vg;

	for( RT_LIST_FOR( lu, loopuse, lu_hd ) )  {
		int		isfirst;
		struct vertex_g	*first_vg;
		point_t		centroid;
		int		npoints;

		/* Consider this loop */
		NMG_CK_LOOPUSE(lu);
		if( RT_LIST_FIRST_MAGIC(&lu->down_hd)==NMG_VERTEXUSE_MAGIC )  {
			/* loop of a single vertex */
			vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
			nmg_vu_to_vlist( vhead, vu );
			continue;
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
struct rt_list	*vhead;
struct shell	*s;
int		poly_markers;
{
	struct faceuse	*fu;
	struct face_g	*fg;
	vect_t		normal;

	NMG_CK_SHELL(s);

	/* faces */
	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		/* Consider this face */
		NMG_CK_FACEUSE(fu);
		NMG_CK_FACE(fu->f_p);
		fg = fu->f_p->fg_p;
		NMG_CK_FACE_G(fg);
		if (fu->orientation != OT_SAME)  continue;
	   	nmg_lu_to_vlist( vhead, &fu->lu_hd, poly_markers, fg->N );
	}

	/* wire loops.  poly_markers=0 so wires are always drawn as vectors */
	VSETALL(normal, 0);
	nmg_lu_to_vlist( vhead, &s->lu_hd, 0, normal );

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
struct rt_list	*vhead;
struct nmgregion	*r;
int		poly_markers;
{
	register struct shell	*s;

	NMG_CK_REGION( r );
	for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
		nmg_s_to_vlist( vhead, s, poly_markers );
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
	
	VJOIN2(base, cur_pt_p, 0.125,prev_vec, 0.05,face_normal);
	VJOIN2(tip, next_pt_p, 0.125,final_vec, 0.05,face_normal);
}


/*			N M G _ E U _ C O O R D S
 *
 *  Get the two (offset and shrunken) endpoints that represent
 *  an edgeuse.
 *  Return the base point, and a point 60% along the way towards the
 *  other end.
 */
static void nmg_eu_coords(eu, base, tip60)
struct edgeuse *eu;
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
		if (fu->orientation == OT_OPPOSITE) {
			VREVERSE( face_normal, fu->f_p->fg_p->N );
		} else {
		    	VMOVE( face_normal, fu->f_p->fg_p->N );
		}

		nmg_offset_eu_coord(base, tip, eu, face_normal);
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
struct edgeuse *eu;
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
struct edgeuse	*eu;
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
struct edgeuse	*eu;
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
void nmg_pl_v(fp, v, b)
FILE		*fp;
struct vertex	*v;
long		*b;
{
	pointp_t p;
	static char label[128];

	NMG_TAB_RETURN_IF_SET_ELSE_SET(b, v->index);

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
FILE		*fp;
struct edge	*e;
long		*b;
int		red, green, blue;
{
	pointp_t	p0, p1;
	point_t		end0, end1;
	vect_t		v;

	NMG_TAB_RETURN_IF_SET_ELSE_SET(b, e->index);
	
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
FILE		*fp;
struct edgeuse	*eu;
long		*b;
int		red, green, blue;
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

	NMG_TAB_RETURN_IF_SET_ELSE_SET(b, eu->index);

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
void nmg_pl_lu(fp, lu, b, red, green, blue)
FILE		*fp;
struct loopuse	*lu;
long		*b;
int		red, green, blue;
{
	struct edgeuse	*eu;
	long		magic1;

	NMG_CK_LOOPUSE(lu);
	NMG_TAB_RETURN_IF_SET_ELSE_SET(b, lu->index);

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
void nmg_pl_fu(fp, fu, b, red, green, blue)
FILE		*fp;
struct faceuse	*fu;
long		*b;
int		red, green, blue;
{
	struct loopuse *lu;

	NMG_CK_FACEUSE(fu);

	NMG_TAB_RETURN_IF_SET_ELSE_SET(b, fu->index);

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
void nmg_pl_s(fp, s)
FILE		*fp;
struct shell	*s;
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
void nmg_pl_r(fp, r)
FILE		*fp;
struct nmgregion *r;
{
	struct shell *s;

	for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
		nmg_pl_s(fp, s);
	}
}

/*
 *			N M G _ P L _ M
 */
void nmg_pl_m(fp, m)
FILE		*fp;
struct model	*m;
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
void nmg_vlblock_v(vbp, v, tab)
struct rt_vlblock	*vbp;
struct vertex		*v;
long			*tab;
{
	pointp_t p;
	static char label[128];
	struct rt_list	*vh;

	NMG_CK_VERTEX(v);
	NMG_TAB_RETURN_IF_SET_ELSE_SET( tab, v->index );

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
void nmg_vlblock_e(vbp, e, tab, red, green, blue, fancy)
struct rt_vlblock	*vbp;
struct edge	*e;
long		*tab;
int		red, green, blue;
int		fancy;
{
	pointp_t p0, p1;
	point_t end0, end1;
	vect_t v;
	struct rt_list	*vh;

	NMG_CK_EDGE(e);
	NMG_TAB_RETURN_IF_SET_ELSE_SET( tab, e->index );
	
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

	vh = rt_vlblock_find( vbp, red, green, blue );
	RT_ADD_VLIST( vh, end0, RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vh, end1, RT_VLIST_LINE_DRAW );

	nmg_vlblock_v(vbp, e->eu_p->vu_p->v_p, tab);
	nmg_vlblock_v(vbp, e->eu_p->eumate_p->vu_p->v_p, tab);
}

/*
 *			M N G _ V L B L O C K _ E U
 */
void nmg_vlblock_eu(vbp, eu, tab, red, green, blue, fancy)
struct rt_vlblock	*vbp;
struct edgeuse		*eu;
long			*tab;
int			red, green, blue;
int			fancy;
{
	point_t base, tip;
	point_t	radial_tip;
	point_t	next_base;
	point_t	last_tip;
	struct rt_list	*vh;

	NMG_CK_EDGEUSE(eu);
	NMG_TAB_RETURN_IF_SET_ELSE_SET( tab, eu->index );

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
void nmg_vlblock_lu(vbp, lu, tab, red, green, blue, fancy)
struct rt_vlblock	*vbp;
struct loopuse	*lu;
long		*tab;
int		red, green, blue;
int		fancy;
{
	struct edgeuse	*eu;
	long		magic1;
	struct vertexuse *vu;

	NMG_CK_LOOPUSE(lu);
	NMG_TAB_RETURN_IF_SET_ELSE_SET( tab, lu->index );

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
void nmg_vlblock_fu(vbp, fu, tab, fancy)
struct rt_vlblock	*vbp;
struct faceuse	*fu;
long		*tab;
int		fancy;
{
	struct loopuse *lu;

	NMG_CK_FACEUSE(fu);
	NMG_TAB_RETURN_IF_SET_ELSE_SET( tab, fu->index );

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
void nmg_vlblock_s(vbp, s, fancy)
struct rt_vlblock	*vbp;
struct shell	*s;
int		fancy;
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
void nmg_vlblock_r(vbp, r, fancy)
struct rt_vlblock	*vbp;
struct nmgregion *r;
int		fancy;
{
	struct shell *s;

	for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
		nmg_vlblock_s(vbp, s, fancy);
	}
}

/*
 *			N M G _ V L B L O C K _ M
 */
void nmg_vlblock_m(vbp, m, fancy)
struct rt_vlblock	*vbp;
struct model	*m;
int		fancy;
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
void nmg_pl_around_edge(fd, b, eu)
FILE		*fd;
long		*b;
struct edgeuse	*eu;
{
	struct edgeuse *eur = eu;
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
void nmg_pl_edges_in_2_shells(fd, b, eu)
FILE		*fd;
long		*b;
struct edgeuse	*eu;
{
	struct edgeuse	*eur;
	struct shell	*s;

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

void nmg_pl_isect(filename, s)
char		*filename;
struct shell	*s;
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
struct faceuse	*fu1;
{
	FILE	*fp;
	char	name[64];
	int	do_plot = 0;
	int	do_anim = 0;
	struct model	*m;
	long	*tab;

	if(rt_g.NMG_debug & DEBUG_PLOTEM &&
	   rt_g.NMG_debug & DEBUG_COMBINE ) do_plot = 1;
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

		nmg_vlblock_fu(vbp, fu1, tab, 1);

		if( nmg_vlblock_anim_upcall )  {
			(*nmg_vlblock_anim_upcall)( vbp, US_DELAY );
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
char		*str;
int		num;
struct faceuse	*fu1;
struct faceuse	*fu2;
int		show_mates;
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

		nmg_vlblock_fu( vbp, fu1, tab, 1);
		if( show_mates )
			nmg_vlblock_fu( vbp, fu1->fumate_p, tab, 1);

		nmg_vlblock_fu( vbp, fu2, tab, 1);
		if( show_mates )
			nmg_vlblock_fu( vbp, fu2->fumate_p, tab, 1);

		/* Cause animation of boolean operation as it proceeds! */
		if( nmg_vlblock_anim_upcall )  {
			(*nmg_vlblock_anim_upcall)( vbp, US_DELAY );
		}
		rt_vlblock_free(vbp);
	}
	rt_free( (char *)tab, "nmg_pl_2fu tab[]" );
}
