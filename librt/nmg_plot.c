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

/************************************************************************
 *									*
 *		NMG to VLIST routines, for MGED interface		*
 *									*
 ************************************************************************/

/*
 *			N M G _ V U _ T O _ V L I S T
 *
 *  Plot a single vertexuse
 */
nmg_vu_to_vlist( vhead, vu )
struct vlhead		*vhead;
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
		ADD_VL( vhead, vg->coord, VL_CMD_LINE_MOVE );
		ADD_VL( vhead, vg->coord, VL_CMD_LINE_DRAW );
	}
}

/*
 *			N M G _ E U _ T O _ V L I S T
 *
 *  Plot a list of edgeuses.  The last edge is joined back to the first.
 */
nmg_eu_to_vlist( vhead, eu_hd )
struct vlhead	*vhead;
struct nmg_list	*eu_hd;
{
	struct edgeuse		*eu;
	struct edgeuse		*eumate;
	struct vertexuse	*vu;
	struct vertexuse	*vumate;
	register struct vertex_g *vg;
	register struct vertex_g *vgmate;

	/* Consider all the edges in the wire edge list */
	for( NMG_LIST( eu, edgeuse, eu_hd ) )  {
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

		ADD_VL( vhead, vg->coord, VL_CMD_LINE_MOVE );
		ADD_VL( vhead, vgmate->coord, VL_CMD_LINE_DRAW );
	}
}

/*
 *			N M G _ L U _ T O _ V L I S T
 *
 *  Plot a list of loopuses.
 */
nmg_lu_to_vlist( vhead, lu_hd, poly_markers, normal )
struct vlhead	*vhead;
struct nmg_list	*lu_hd;
int		poly_markers;
vectp_t		normal;
{
	struct loopuse	*lu;
	struct edgeuse	*eu;
	struct vertexuse *vu;
	struct vertex	*v;
	register struct vertex_g *vg;

	for( NMG_LIST( lu, loopuse, lu_hd ) )  {
		int		isfirst;
		struct vertex_g	*first_vg;
		point_t		centroid;
		int		npoints;

		/* Consider this loop */
		NMG_CK_LOOPUSE(lu);
		if( NMG_LIST_FIRST_MAGIC(&lu->down_hd)==NMG_VERTEXUSE_MAGIC )  {
			/* loop of a single vertex */
			vu = NMG_LIST_FIRST(vertexuse, &lu->down_hd);
			nmg_vu_to_vlist( vhead, vu );
			continue;
		}
		/* Consider all the edges in the loop */
		isfirst = 1;
		first_vg = (struct vertex_g *)0;
		npoints = 0;
		VSETALL( centroid, 0 );
		for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
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
					ADD_VL( vhead, normal, VL_CMD_POLY_START );
					ADD_VL( vhead, vg->coord, VL_CMD_POLY_MOVE );
				} else {
					/* move */
					ADD_VL( vhead, vg->coord, VL_CMD_LINE_MOVE );
				}
				isfirst = 0;
				first_vg = vg;
			} else {
				if( poly_markers) {
					ADD_VL( vhead, vg->coord, VL_CMD_POLY_DRAW );
				} else {
					/* Draw */
					ADD_VL( vhead, vg->coord, VL_CMD_LINE_DRAW );
				}
			}
		}

		/* Draw back to the first vertex used */
		if( !isfirst && first_vg )  {
			if( poly_markers )  {
				/* Draw, end polygon */
				ADD_VL( vhead, first_vg->coord, VL_CMD_POLY_END );
			} else {
				/* Draw */
				ADD_VL( vhead, first_vg->coord, VL_CMD_LINE_DRAW );
			}
		}
		if( poly_markers > 1 && npoints > 2 )  {
			/* Draw surface normal as a little vector */
			double	f;
			vect_t	tocent;
			f = 1.0 / npoints;
			VSCALE( centroid, centroid, f );
			ADD_VL( vhead, centroid, VL_CMD_LINE_MOVE );
			VSUB2( tocent, first_vg->coord, centroid );
			f = MAGNITUDE( tocent ) * 0.5;
			VSCALE( tocent, normal, f );
			VADD2( centroid, centroid, tocent );
			ADD_VL( vhead, centroid, VL_CMD_LINE_DRAW );
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
struct vlhead	*vhead;
struct shell	*s;
int		poly_markers;
{
	struct faceuse	*fu;
	struct face_g	*fg;
	vect_t		normal;

	NMG_CK_SHELL(s);

	/* faces */
	for( NMG_LIST( fu, faceuse, &s->fu_hd ) )  {
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
struct vlhead	*vhead;
struct nmgregion	*r;
int		poly_markers;
{
	register struct shell	*s;

	NMG_CK_REGION( r );
	for( NMG_LIST( s, shell, &r->s_hd ) )  {
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
 *			N M G _ E U _ C O O R D
 *
 *  Given an edgeuse structure, return the coordinates of the "base point"
 *  of this edge.  This base point will be offset inwards along the edge
 *  slightly, to avoid obscuring the vertex, and will be offset off the
 *  face (in the direction of the face normal) slightly, to avoid
 *  obscuring the edge itself.
 */
static void nmg_eu_coord(eu, base)
struct edgeuse *eu;
point_t base;
{
	fastf_t dist1;
	struct edgeuse *peu;
	struct loopuse *lu;
	vect_t v_eu,		/* vector of edgeuse */
		v_other,	/* vector of last edgeuse */
		N;		/* normal vector for this edgeuse's face */
	pointp_t pt_other, pt_eu;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEX(eu->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);

	NMG_CK_EDGEUSE(eu->eumate_p);
	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);

	pt_eu = eu->vu_p->v_p->vg_p->coord;
	pt_other = eu->eumate_p->vu_p->v_p->vg_p->coord;

	if (*eu->up.magic_p == NMG_SHELL_MAGIC || 
	    (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
	     *eu->up.lu_p->up.magic_p != NMG_FACEUSE_MAGIC) ) {
	     	/* Wire edge, or edge in wire loop */
		VMOVE(base, pt_eu);
		return;
	}
	if (*eu->up.magic_p != NMG_LOOPUSE_MAGIC ||
	    *eu->up.lu_p->up.magic_p != NMG_FACEUSE_MAGIC) {
		rt_log("in %s at %d edgeuse has bad parent\n", __FILE__, __LINE__);
		rt_bomb("nmg_eu_coord\n");
	}

	lu = eu->up.lu_p;
	NMG_CK_FACE(lu->up.fu_p->f_p);
	NMG_CK_FACE_G(lu->up.fu_p->f_p->fg_p);

	VMOVE(N, lu->up.fu_p->f_p->fg_p->N);
	if (lu->up.fu_p->orientation == OT_OPPOSITE) {
		VREVERSE(N,N);
	}

	/* v_eu is the vector of the edgeuse
	 * mag is the magnitude of the edge vector
	 */
	VSUB2(v_eu, pt_other, pt_eu); 
	VUNITIZE(v_eu);

	/* find a point not on the edge */
	peu = NMG_LIST_PLAST_CIRC( edgeuse, eu );
	pt_other = peu->vu_p->v_p->vg_p->coord;
	dist1 = rt_dist_line_point(pt_eu, v_eu, pt_other);
	while (NEAR_ZERO(dist1, LEE_DIVIDE_TOL) && peu != eu) {
		peu = NMG_LIST_PLAST_CIRC( edgeuse, peu );
		pt_other = peu->vu_p->v_p->vg_p->coord;
		dist1 = rt_dist_line_point(pt_eu, v_eu, pt_other);
	}

	/* make a vector from the "last" edgeuse (reversed) */
	VSUB2(v_other, pt_other, pt_eu); VUNITIZE(v_other);

	/* combine the two vectors to get a vector
	 * pointing to the location where the edgeuse
	 * should start
	 */
	VADD2(v_other, v_other, v_eu); VUNITIZE(v_other);

	/* XXX vector lengths should be scaled by 5% of face size */
	
	/* compute the start of the edgeuse */
	VJOIN2(base, pt_eu, 0.125,v_other, 0.05,N);
}


/*			N M G _ E U _ C O O R D S
 *
 *  Get the two (offset and shrunken) endpoints that represent
 *  an edgeuse.
 *  Return the base point, and a point 60% along the way towards the
 *  other end.
 */
static nmg_eu_coords(eu, base, tip60)
struct edgeuse *eu;
point_t base, tip60;
{
	point_t	tip;

	NMG_CK_EDGEUSE(eu);

	nmg_eu_coord(eu, base);
	if (*eu->up.magic_p == NMG_SHELL_MAGIC ||
	    (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
	    *eu->up.lu_p->up.magic_p == NMG_SHELL_MAGIC) ) {
	    	/* Wire edge, or edge in wire loop */
		NMG_CK_EDGEUSE(eu->eumate_p);
		nmg_eu_coord( eu->eumate_p, tip );
	}
	else if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
	    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC) {
	    	/* Loop in face */
	    	register struct edgeuse *eutmp;
		eutmp = NMG_LIST_PNEXT_CIRC(edgeuse, eu);
		NMG_CK_EDGEUSE(eutmp);
		nmg_eu_coord( eutmp, tip );
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
	eulast = NMG_LIST_PLAST_CIRC( edgeuse, eu );
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
	nexteu = NMG_LIST_PNEXT_CIRC( edgeuse, eu );
	NMG_CK_EDGEUSE(nexteu);
	NMG_CK_VERTEXUSE(nexteu->vu_p);
	NMG_CK_VERTEX(nexteu->vu_p->v_p);
	NMG_CK_VERTEX_G(nexteu->vu_p->v_p->vg_p);

	nmg_eu_coords(nexteu, next_base, t2);
}

/*
 *			N M G _ P L _ V
 */
static void nmg_pl_v(fp, v, b)
FILE *fp;
struct vertex *v;
struct nmg_ptbl *b;
{
	pointp_t p;
	static char label[128];

	if (nmg_tbl(b, TBL_LOC, &v->magic) >= 0) return;

	(void)nmg_tbl(b, TBL_INS, &v->magic);

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
static nmg_pl_e(fp, e, b, R, G, B)
FILE *fp;
struct edge *e;
struct nmg_ptbl *b;
unsigned char R, G, B;
{
	pointp_t p0, p1;
	point_t end0, end1;
	vect_t v;

	if (nmg_tbl(b, TBL_LOC, &e->magic) >= 0) return;

	(void)nmg_tbl(b, TBL_INS, &e->magic);
	
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
	VREVERSE(v, v);

	VADD2(end1, p1, v);

	pl_color(fp, R, G, B);
	pdv_3line( fp, end0, end1 );

	nmg_pl_v(fp, e->eu_p->vu_p->v_p, b);
	nmg_pl_v(fp, e->eu_p->eumate_p->vu_p->v_p, b);
}

/*
 *			M N G _ P L _ E U
 */
void nmg_pl_eu(fp, eu, b, R, G, B)
FILE *fp;
struct edgeuse *eu;
struct nmg_ptbl *b;
unsigned char R, G, B;
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

	if (nmg_tbl(b, TBL_LOC, &eu->l.magic) >= 0) return;
	(void)nmg_tbl(b, TBL_INS, &eu->l.magic);

	nmg_pl_e(fp, eu->e_p, b, R, G, B);

	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
	    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC) {

	    	nmg_eu_coords(eu, base, tip);
	    	if (eu->up.lu_p->up.fu_p->orientation == OT_SAME)
	    		R += 50;
		else if (eu->up.lu_p->up.fu_p->orientation == OT_OPPOSITE)
			R -= 50;
	    	else
	    		R = G = B = (unsigned char)255;

		pl_color(fp, R, G, B);
	    	pdv_3line( fp, base, tip );

	    	nmg_eu_radial( eu, radial_tip );
		pl_color(fp, R, G-20, B);
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
void nmg_pl_lu(fp, lu, b, R, G, B)
FILE *fp;
struct loopuse *lu;
struct nmg_ptbl *b;
unsigned char R, G, B;
{
	struct edgeuse	*eu;
	long		magic1;

	NMG_CK_LOOPUSE(lu);
	if (nmg_tbl(b, TBL_LOC, &lu->l.magic) >= 0) return;

	(void)nmg_tbl(b, TBL_INS, &lu->l.magic);

	magic1 = NMG_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC &&
	    lu->orientation != OT_BOOLPLACE) {
	    	nmg_pl_v(fp, NMG_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p, b);
	} else if (magic1 == NMG_EDGEUSE_MAGIC) {
		for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
			nmg_pl_eu(fp, eu, b, R, G, B);
		}
	}
}

/*
 *			M N G _ P L _ F U
 */
void nmg_pl_fu(fp, fu, b)
FILE *fp;
struct faceuse *fu;
struct nmg_ptbl *b;
{
	struct loopuse *lu;

	NMG_CK_FACEUSE(fu);
	if (nmg_tbl(b, TBL_LOC, &fu->l.magic) >= 0) return;
	(void)nmg_tbl(b, TBL_INS, &fu->l.magic);

	for( NMG_LIST( lu, loopuse, &fu->lu_hd ) )  {
		nmg_pl_lu(fp, lu, b, 80, 100, 170 );
	}
}

/*
 *			N M G _ P L _ S
 */
void nmg_pl_s(fp, s)
FILE *fp;
struct shell *s;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct nmg_ptbl b;


	NMG_CK_SHELL(s);

	/* get space for list of items processed */
	(void)nmg_tbl(&b, TBL_INIT, (long *)0);	

	for( NMG_LIST( fu, faceuse, &s->fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		nmg_pl_fu(fp, fu, &b );
	}

	for( NMG_LIST( lu, loopuse, &s->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		nmg_pl_lu(fp, lu, &b, 255, 0, 0);
	}

	for( NMG_LIST( eu, edgeuse, &s->eu_hd ) )  {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_EDGE(eu->e_p);

		nmg_pl_eu(fp, eu, &b, (unsigned char)200, 
			(unsigned char)200, 
			(unsigned char)0);
	}
	if (s->vu_p) {
		nmg_pl_v(fp, s->vu_p->v_p, &b );
	}

	if( NMG_LIST_IS_EMPTY( &s->fu_hd ) &&
	    NMG_LIST_IS_EMPTY( &s->lu_hd ) &&
	    NMG_LIST_IS_EMPTY( &s->eu_hd ) && !s->vu_p) {
	    	rt_log("WARNING nmg_pl_s() shell has no children\n");
	}

	(void)nmg_tbl(&b, TBL_FREE, (long *)NULL);
}

/*
 *			N M G _ P L _ R
 */
void nmg_pl_r(fp, r)
FILE *fp;
struct nmgregion *r;
{
	struct shell *s;

	for( NMG_LIST( s, shell, &r->s_hd ) )  {
		nmg_pl_s(fp, s);
	}
}

/*
 *			N M G _ P L _ M
 */
void nmg_pl_m(fp, m)
FILE *fp;
struct model *m;
{
	struct nmgregion *r;

	for( NMG_LIST( r, nmgregion, &m->r_hd ) )  {
		nmg_pl_r(fp, r);
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
static void nmg_pl_around_edge(fd, b, eu)
FILE *fd;
struct nmg_ptbl *b;
struct edgeuse *eu;
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
FILE *fd;
struct nmg_ptbl *b;
struct edgeuse *eu;
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
char *filename;
struct shell *s;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct nmg_ptbl b;
	FILE	*fp;
	long	magic1;

	NMG_CK_SHELL(s);

	if ((fp=fopen(filename, "w")) == (FILE *)NULL) {
		(void)perror(filename);
		exit(-1);
	}

	(void)nmg_tbl(&b, TBL_INIT, (long *)NULL);

	rt_log("Plotting to \"%s\"\n", filename);
	for( NMG_LIST( fu, faceuse, &s->fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		for( NMG_LIST( lu, loopuse, &fu->lu_hd ) )  {
			NMG_CK_LOOPUSE(lu);
			magic1 = NMG_LIST_FIRST_MAGIC( &lu->down_hd );
			if (magic1 == NMG_EDGEUSE_MAGIC) {
				for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
					NMG_CK_EDGEUSE(eu);
					nmg_pl_edges_in_2_shells(fp, &b, eu);
				}
			} else if (magic1 == NMG_VERTEXUSE_MAGIC) {
				;
			} else {
				rt_bomb("nmg_pl_isect() bad loopuse down\n");
			}
		}
	}
	(void)nmg_tbl(&b, TBL_FREE, (long *)NULL);

	(void)fclose(fp);
}
