/*
 *			N M G _ I N T E R . C
 *
 *	Routines to intersect two NMG regions.
 *	There are no "user interface" routines in here.
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
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
#include <math.h>
#include "externs.h"
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

/* XXX move to vmath.h, incorporate in librt/shoot.c */
/* Given a direction vector, compute the inverses of each element. */
/* When division by zero would have occured, mark inverse as INFINITY. */
#define VINVDIR( _inv, _dir )	{ \
	if( (_dir)[X] < -SQRT_SMALL_FASTF || (_dir)[X] > SQRT_SMALL_FASTF )  { \
		(_inv)[X]=1.0/(_dir)[X]; \
	} else { \
		(_dir)[X] = 0.0; \
		(_inv)[X] = INFINITY; \
	} \
	if( (_dir)[Y] < -SQRT_SMALL_FASTF || (_dir)[Y] > SQRT_SMALL_FASTF )  { \
		(_inv)[Y]=1.0/(_dir)[Y]; \
	} else { \
		(_dir)[Y] = 0.0; \
		(_inv)[Y] = INFINITY; \
	} \
	if( (_dir)[Z] < -SQRT_SMALL_FASTF || (_dir)[Z] > SQRT_SMALL_FASTF )  { \
		(_inv)[Z]=1.0/(_dir)[Z]; \
	} else { \
		(_dir)[Z] = 0.0; \
		(_inv)[Z] = INFINITY; \
	} \
    }

/* Was nmg_boolstruct, but that name has appeared in nmg.h */
struct nmg_inter_struct {
	long		magic;
	struct nmg_ptbl	*l1;		/* vertexuses on the line of */
	struct nmg_ptbl *l2;		/* intersection between planes */
	struct rt_tol	tol;
	point_t		pt;		/* 3D line of intersection */
	vect_t		dir;
	int		coplanar;
	fastf_t		*vert2d;	/* Array of 2d vertex projections [index] */
	int		maxindex;	/* size of vert2d[] */
	mat_t		proj;		/* Matrix to project onto XY plane */
	struct face	*face;		/* fu of 2d projection plane */
};
#define NMG_INTER_STRUCT_MAGIC	0x99912120
#define NMG_CK_INTER_STRUCT(_p)	NMG_CKMAG(_p, NMG_INTER_STRUCT_MAGIC, "nmg_inter_struct")

static CONST struct nmg_visit_handlers nmg_null_handlers;	/* null filled */

struct ee_2d_state {
	struct nmg_inter_struct	*is;
	struct edgeuse	*eu;
	point_t	start;
	point_t	end;
	vect_t	dir;
};

RT_EXTERN(void		nmg_isect2d_prep, (struct nmg_inter_struct *is, struct face *f1));
RT_EXTERN(CONST struct vertexuse *nmg_touchingloops, (CONST struct loopuse *lu));

static void	nmg_isect_edge2p_face2p RT_ARGS((struct nmg_inter_struct *is,
			struct edgeuse *eu, struct faceuse *fu,
			struct faceuse *eu_fu));



/*
 *			N M G _ G E T _ 2 D _ V E R T E X
 *
 *  A "lazy evaluator" to obtain the 2D projection of a vertex.
 *  The lazy approach is not a luxury, since new (3D) vertices are created
 *  as the edge/edge intersection proceeds, and their 2D coordinates may
 *  be needed later on in the calculation.
 *  The alternative would be to store the 2D projection each time a
 *  new vertex is created, but that is likely to be a lot of bothersome
 *  code, where one omission would be deadly.
 *
 *  The return is a 3-tuple, with the Z coordinate set to 0.0 for safety.
 *  This is especially useful when the projected value is printed using
 *  one of the 3D print routines.
 */
static void
nmg_get_2d_vertex( v2d, v, is, fu )
point_t		v2d;		/* a 3-tuple */
struct vertex	*v;
struct nmg_inter_struct	*is;
CONST struct faceuse	*fu;	/* for plane equation */
{
	register fastf_t	*pt2d;
	point_t			pt;
	struct vertex_g		*vg;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_VERTEX(v);

	/* If 2D preparations have not been made yet, do it now */
	if( !is->vert2d )  {
		nmg_isect2d_prep( is, fu->f_p );
	}

	if( fu->f_p != is->face )  {
		rt_log("nmg_get_2d_vertex(,fu=%x) f=x%x, is->face=%x\n",
			fu, fu->f_p, is->face);
		rt_bomb("nmg_get_2d_vertex:  face mis-match\n");
	}

	if( !v->vg_p )  rt_bomb("nmg_get_2d_vertex:  vertex with no geometry!\n");
	vg = v->vg_p;
	NMG_CK_VERTEX_G(vg);
	if( v->index >= is->maxindex )  rt_bomb("nmg_get_2d_vertex:  array overrun\n");
	pt2d = &is->vert2d[v->index*3];
	if( pt2d[2] == 0 )  {
		/* Flag set.  Conversion is done.  Been here before */
		v2d[0] = pt2d[0];
		v2d[1] = pt2d[1];
		v2d[2] = 0;
		return;
	}

	MAT4X3PNT( pt, is->proj, vg->coord );
	v2d[0] = pt2d[0] = pt[0];
	v2d[1] = pt2d[1] = pt[1];
	v2d[2] = pt2d[2] = 0;		/* flag */

	if( !NEAR_ZERO( pt[2], is->tol.dist ) )  {
		rt_log("nmg_get_2d_vertex ERROR #%d (%g %g %g) becomes (%g,%g) %g != zero!\n",
			v->index, V3ARGS(vg->coord), V3ARGS(pt) );
		if( !NEAR_ZERO( pt[2], 10*is->tol.dist ) )  {
			rt_log("nmg_get_2d_vertex(,fu=%x) f=x%x, is->face=%x\n",
				fu, fu->f_p, is->face);
			PLPRINT("is->face N", is->face->fg_p->N);
			PLPRINT("fu->f_p N", fu->f_p->fg_p->N);
			rt_bomb("3D->2D point projection error\n");
		}
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("2d #%d (%g %g %g) becomes (%g,%g) %g\n",
			v->index, V3ARGS(vg->coord), V3ARGS(pt) );
	}
}

/*
 *			N M G _ I S E C T 2 D _ P R E P
 *
 *  To intersect two co-planar faces, project all vertices from those
 *  faces into 2D.
 *  At the moment, use a memory intensive strategy which allocates a
 *  (3d) point_t for each "index" item, and subscripts the resulting
 *  array by the vertices index number.
 *  Since additional vertices can be created as the intersection process
 *  operates, 2*maxindex items are originall allocated, as a (generous)
 *  upper bound on the amount of intersecting that might happen.
 *
 *  In the array, the third double of each projected vertex is set to -1 when
 *  that slot has not been filled yet, and 0 when it has been.
 */
void
nmg_isect2d_prep( is, f1 )
struct nmg_inter_struct	*is;
struct face		*f1;
{
	struct model	*m;
	struct face_g	*fg;
	vect_t		to;
	point_t		centroid;
	point_t		centroid_proj;
	register int	i;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_FACE(f1);
	fg = f1->fg_p;
	NMG_CK_FACE_G(fg);
	is->face = f1;
rt_log("nmg_isect2d_prep(f=x%x)\n", f1);
PLPRINT("N", fg->N);

	m = nmg_find_model( &f1->magic );

	is->maxindex = ( 2 * m->maxindex );
	is->vert2d = (fastf_t *)rt_malloc( 3 * is->maxindex * sizeof(fastf_t), "vert2d[]");

	/*
	 *  Rotate so that f1's N vector points up +Z.
	 *  This places all 2D calcuations in the XY plane.
	 *  Translate so that f1's centroid becomes the 2D origin.
	 *  Reasoning:  no vertex should be favored by putting it at
	 *  the origin.  The "desirable" floating point space in the
	 *  vicinity of the origin should be used to best advantage,
	 *  by centering calculations around it.
	 */
	VSET( to, 0, 0, 1 );
	mat_fromto( is->proj, fg->N, to );
	VADD2SCALE( centroid, fg->max_pt, fg->min_pt, 0.5 );
	MAT4X3PNT( centroid_proj, is->proj, centroid );
	centroid_proj[Z] = fg->N[3];	/* pull dist from origin off newZ */
	MAT_DELTAS_VEC_NEG( is->proj, centroid_proj );

	/* Clear out the 2D vertex array, setting flag in [2] to -1 */
	for( i = (3*is->maxindex)-1-2; i >= 0; i -= 3 )  {
		VSET( &is->vert2d[i], 0, 0, -1 );
	}
}

/* XXX Move to nmg_pr.c or nmg_misc.c */
/*
 *			N M G _ P R _ P T B L _ V E R T _ L I S T
 *
 *  Print a ptbl array as a vertex list.
 */
void
nmg_pr_ptbl_vert_list( str, tbl )
char		*str;
struct nmg_ptbl	*tbl;
{
	int			i;
	struct vertexuse	**vup;
	struct vertexuse	*vu;
	struct vertex		*v;
	struct vertex_g		*vg;

    	rt_log("nmg_pr_ptbl_vert_list(%s):\n", str);

	vup = (struct vertexuse **)tbl->buffer;
	for (i=0 ; i < tbl->end ; ++i) {
		vu = vup[i];
		NMG_CK_VERTEXUSE(vu);
		v = vu->v_p;
		NMG_CK_VERTEX(v);
		vg = v->vg_p;
		NMG_CK_VERTEX_G(vg);
		rt_log("%d\t%g, %g, %g\t", i, V3ARGS(vg->coord) );
		if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC) {
			rt_log("EDGEUSE");
		} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
			rt_log("LOOPUSE");
			if ((struct vertexuse *)vu->up.lu_p->down_hd.forw != vu) {
				rt_log("ERROR vertexuse's parent disowns us!\n");
				if (((struct vertexuse *)(vu->up.lu_p->lumate_p->down_hd.forw))->l.magic == NMG_VERTEXUSE_MAGIC)
					rt_bomb("lumate has vertexuse\n");
				else
					rt_bomb("lumate has garbage\n");
			}
		} else {
			rt_log("UNKNOWN");
		}
		rt_log("\tv=x%x, vu=x%x\n", v , vu);
	}
}

/*
 *			N M G _ I S E C T _ E D G E 2 P _ V E R T 2 P
 *
 *  Intersect an edge and a vertex known to lie in the same plane.
 *
 *  This is a separate routine because it's used more than once.
 *
 *  Returns -
 *	0	if there was no intersection
 *	1	if the vertex was an endpoint of the edge.
 *	2	if edge was split at vertex.
 */
static int
nmg_isect_edge2p_vert2p( is, eu, vu )
struct nmg_inter_struct *is;
struct edgeuse		*eu;
struct vertexuse	*vu;
{
	fastf_t		dist;
	int		status;
	point_t		endpt;
	int		ret;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(vu);

	/* Topology check */
	if( vu->v_p == eu->vu_p->v_p || vu->v_p == eu->eumate_p->vu_p->v_p )  {
		ret = 1;
		goto out;
	}

	/* XXX a 2d formulation would be better! */
#if 0
	/* This needs to be done every pass:  it changes as edges are cut */
	nmg_get_2d_vertex( is->pt, eu->vu_p->v_p, is, fu );
	nmg_get_2d_vertex( endpt, eu->eumate_p->vu_p->v_p, is, fu );
	VSUB2( is->dir, endpt, is->pt );
	/* rt_isect_pt2_lseg2() */
#endif

	status = rt_isect_pt_lseg( &dist,
		eu->vu_p->v_p->vg_p->coord,
		eu->eumate_p->vu_p->v_p->vg_p->coord,
		vu->v_p->vg_p->coord, &is->tol );
	switch( status )  {
	default:
		ret = 0;
		break;
	case 1:
		/* pt is at start of edge */
		nmg_jv( vu->v_p, eu->vu_p->v_p );
		ret = 1;
		break;
	case 2:
		/* pt is at end of edge */
		nmg_jv( vu->v_p, eu->eumate_p->vu_p->v_p );
		ret = 1;
		break;
	case 3:
		/* pt is in interior of edge, break edge */
		(void)nmg_ebreak( vu->v_p, eu );
		ret = 2;
		break;
	}
out:
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_edge2p_vert2p(, eu=x%x, vu=x%x) ret=%d\n", eu, vu, ret);
	return ret;
}

/*
 *			N M G _ I S E C T _ V E R T 2 P _ F A C E 2 P
 *
 *  Handle the complete intersection of a vertex which lies on the
 *  plane of a face.
 *  If already part of the topology of the face, do nothing more.
 *  It it intersects one of the edges of the face, break the edge there.
 *  Otherwise, add a self-loop into the face as a marker.
 *
 *  Since this is an intersection of a vertex, no loop cut/join action
 *  is needed, since the only possible actions are fusing vertices
 *  and breaking edges into two edges about this vertex.
 *  This also means that no nmg_tbl( , TBL_INS_UNIQUE ) ops need be done.
 */
static void
nmg_isect_vert2p_face2p(is, vu, fu)
struct nmg_inter_struct *is;
struct vertexuse	*vu;
struct faceuse		*fu;
{
	struct loopuse	 *lu;
	pointp_t	pt;
	fastf_t		dist;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_vert2p_face2p(, vu=x%x, fu=x%x)\n", vu, fu);
	NMG_CK_INTER_STRUCT(is);
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_FACEUSE(fu);

	/* check the topology first */	
	if( nmg_find_v_in_face(vu->v_p, fu) )  return;

	/* topology didn't say anything, check with the geometry. */
	pt = vu->v_p->vg_p->coord;

	/* For every edge and vert in face, check geometric intersection */
	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		struct edgeuse	*eu;
		struct vertexuse *vu2;

		NMG_CK_LOOPUSE(lu);
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )  {
			vu2 = RT_LIST_FIRST( vertexuse, &lu->down_hd );
			if( vu->v_p == vu2->v_p )  return;
			/* Perhaps a 2d routine here? */
			if( rt_pt3_pt3_equal( pt, vu2->v_p->vg_p->coord, &is->tol ) )  {
				/* Fuse the two verts together */
				nmg_jv( vu->v_p, vu2->v_p );
				return;
			}
			continue;
		}
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			/* If there is an intersection, we are done */
			if( nmg_isect_edge2p_vert2p( is, eu, vu ) )  return;
		}
	}

	/* The vertex lies in the face, but touches nothing.  Place marker */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    	VPRINT("Making vertexloop", pt);

	lu = nmg_mlv(&fu->l.magic, vu->v_p, OT_UNSPEC);
	nmg_loop_g( lu->l_p );
}

/*
 *			N M G _ I S E C T _ 3 V E R T E X _ 3 F A C E
 *
 *	intersect a vertex with a face (primarily for intersecting
 *	loops of a single vertex with a face).
 */
static void
nmg_isect_3vertex_3face(bs, vu, fu)
struct nmg_inter_struct *bs;
struct vertexuse *vu;
struct faceuse *fu;
{
	struct loopuse *plu;
	struct vertexuse *vup;
	pointp_t pt;
	fastf_t dist;

	NMG_CK_INTER_STRUCT(bs);
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_VERTEX(vu->v_p);
	NMG_CK_FACEUSE(fu);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_3vertex_3face(, vu=x%x, fu=x%x) v=x%x\n", vu, fu, vu->v_p);

	/* check the topology first */	
	if (vup=nmg_find_v_in_face(vu->v_p, fu)) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT) rt_log("\tvu lies in face (topology 1)\n");
		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &vu->l.magic);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vup->l.magic);
		return;
	}


	/* since the topology didn't tell us anything, we need to check with
	 * the geometry
	 */
	pt = vu->v_p->vg_p->coord;
	dist = DIST_PT_PLANE(pt, fu->f_p->fg_p->N);

	if ( !NEAR_ZERO(dist, bs->tol.dist) )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT) rt_log("\tvu not on face (geometry)\n");
		return;
	}

	/*
	 *  The point lies on the plane of the face, by geometry.
	 *  This is now a 2-D problem.
	 *
	 *  Intersect this point with all the edges of the face.
	 *  Note that no nmg_tbl() calls will be done during vert2p_face2p,
	 *  which is exactly what is needed here.
	 */
	nmg_isect_vert2p_face2p( bs, vu, fu );

	/* Re-check the topology */
	if (vup=nmg_find_v_in_face(vu->v_p, fu)) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT) rt_log("\tvu lies in face (topology 2)\n");
		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &vu->l.magic);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vup->l.magic);
		return;
	}

	/* Make lone vertex-loop in fu to indicate sharing */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    	VPRINT("nmg_isect_3vertex_3face() making vertexloop", vu->v_p->vg_p->coord);

	plu = nmg_mlv(&fu->l.magic, vu->v_p, OT_UNSPEC);
	nmg_loop_g(plu->l_p);
	vup = RT_LIST_FIRST(vertexuse, &plu->down_hd);
	NMG_CK_VERTEXUSE(vup);
	(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &vu->l.magic);
    	(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vup->l.magic);
}

/*
 *			N M G _ B R E A K _ 3 E D G E _ A T _ P L A N E
 *
 *	Having decided that an edge(use) crosses a plane of intersection,
 *	stick a vertex at the point of intersection along the edge.
 */
static void
nmg_break_3edge_at_plane(hit_pt, fu, bs, eu, v1, v1mate)
point_t	hit_pt;
struct faceuse *fu;
struct nmg_inter_struct *bs;
struct edgeuse *eu;
struct vertex	*v1;
struct vertex	*v1mate;
{
	struct vertexuse *vu_other;
	struct loopuse	*plu;
	struct edgeuse *euforw;

	NMG_CK_INTER_STRUCT(bs);

	/* Intersection is between first and second vertex points.
	 * Insert new vertex at intersection point.
	 */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("Splitting %g, %g, %g <-> %g, %g, %g\n",
			V3ARGS(v1->vg_p->coord), V3ARGS(v1mate->vg_p->coord) );
		VPRINT("\tPoint of intersection", hit_pt);
	}

	/* if we can't find the appropriate vertex in the
	 * other face, we'll build a new vertex.  Otherwise
	 * we re-use an old one.
	 */
	vu_other = nmg_find_pt_in_face(hit_pt, fu, &(bs->tol));
	if (vu_other) {
		/* the other face has a convenient vertex for us */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("re-using vertex v=x%x from other face\n", vu_other->v_p);

		euforw = nmg_ebreak(vu_other->v_p, eu);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
	} else {
		/* The other face has no vertex in this vicinity */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("Making new vertex\n");

		euforw = nmg_ebreak((struct vertex *)NULL, eu);
		nmg_vertex_gv(euforw->vu_p->v_p, hit_pt);

		NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
		NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);
		NMG_CK_VERTEX_G(euforw->vu_p->v_p->vg_p);
		NMG_CK_VERTEX_G(euforw->eumate_p->vu_p->v_p->vg_p);

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			register pointp_t p1 = eu->vu_p->v_p->vg_p->coord;
			register pointp_t p2 = eu->eumate_p->vu_p->v_p->vg_p->coord;

			rt_log("Just split %g, %g, %g -> %g, %g, %g\n",
				V3ARGS(p1), V3ARGS(p2) );
			p1 = euforw->vu_p->v_p->vg_p->coord;
			p2 = euforw->eumate_p->vu_p->v_p->vg_p->coord;
			rt_log("\t\t\t%g, %g, %g -> %g, %g, %g\n",
				V3ARGS(p1), V3ARGS(p2) );
		}

		/* stick this vertex in the other shell
		 * and make sure it is in the other shell's
		 * list of vertices on the intersect line
		 */
		plu = nmg_mlv(&fu->l.magic, euforw->vu_p->v_p, OT_UNSPEC);
		vu_other = RT_LIST_FIRST( vertexuse, &plu->down_hd );
		NMG_CK_VERTEXUSE(vu_other);
		nmg_loop_g(plu->l_p);

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		    	VPRINT("Making vertexloop",
				vu_other->v_p->vg_p->coord);
		}
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		register pointp_t	p1, p2;
		p1 = eu->vu_p->v_p->vg_p->coord;
		p2 = eu->eumate_p->vu_p->v_p->vg_p->coord;
		rt_log("\tNow %g, %g, %g <-> %g, %g, %g\n",
			V3ARGS(p1), V3ARGS(p2) );
		p1 = euforw->vu_p->v_p->vg_p->coord;
		p2 = euforw->eumate_p->vu_p->v_p->vg_p->coord;
		rt_log("\tand %g, %g, %g <-> %g, %g, %g\n\n",
			V3ARGS(p1), V3ARGS(p2) );
	}
	(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &euforw->vu_p->l.magic);
}

/* XXX move to nmg_ck.c */
/*
 *			N M G _ C K _ F A C E _ W O R T H L E S S _ E D G E S
 *
 *  For the moment, a quick hack to see if breaking an edge at a given
 *  vertex results in a null edge being created.
 */
void
nmg_ck_face_worthless_edges( fu )
CONST struct faceuse	*fu;
{
	CONST struct loopuse	*lu;

	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		struct edgeuse	*eu;
		struct vertexuse *vu2;

		NMG_CK_LOOPUSE(lu);
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
			continue;
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			struct edgeuse		*neu;
			neu = RT_LIST_PNEXT_CIRC( edgeuse, eu );
			if( eu->vu_p == neu->vu_p )
				rt_bomb("edge runs between two copies of vu??\n");
			if( eu->vu_p->v_p == neu->vu_p->v_p )  {
#if 0
				nmg_pr_eu( eu, NULL );
				nmg_pr_eu( neu, NULL );
#endif
				rt_log("eu=x%x, neu=x%x, v=x%x\n", eu, neu, eu->vu_p->v_p);
				rt_log("eu=x%x, neu=x%x, v=x%x\n", eu->eumate_p, neu->eumate_p, eu->eumate_p->vu_p->v_p);
				rt_bomb("nmg_ck_face_worthless_edges() edge runs from&to same vertex\n");
			}
		}
	}
}

/*
 *			N M G _ I S E C T _ T W O _ C O L I N E A R _ E D G E 2 P
 *
 *  Two colinear line segments (eu1 and eu2, or just "1" and "2" in the
 *  diagram) can overlap each other in one of 8 configurations,
 *  labeled A through H:
 *
 *	A	B	C	D	E	F	G	H
 *
 *  vu1b,vu2b
 *	*	*	  *	  *	*	  *	*	  *
 *	1	1	  2	  2	1	  2	1	  2
 *	1=*	1	  2	*=2	1=*	*=2	*	  *
 *	1 2	*=*	*=*	1 2	1 2	1 2
 *	1 2	  2	1	1 2	1 2	1 2	  *	*
 *	1=*	  2	1	*=2	*=2	1=*	  2	1
 *	1	  *	*	  2	  2	1	  *	*
 *	*			  *	  *	*
 *   vu1a,vu2a
 *
 *  dist[0] has the distance (0..1) along eu1 from vu1a to vu2a.
 *  dist[1] has the distance (0..1) along eu1 from vua1 to vu2b.
 *
 *  As a consequence of this, conditions D, E, F can not be
 *  completely processed by just one call.
 *  If the caller computes a second array where the sense of eu1 and eu2
 *  are exchanged, and calls again, then the full intersection 
 *  be achieved.
 */
void
nmg_isect_two_colinear_edge2p( dist, l1, l2, vu1a, vu1b, vu2a, vu2b, eu1, eu2, fu1, fu2, str )
CONST fastf_t		dist[2];
struct nmg_ptbl		*l1;
struct nmg_ptbl		*l2;
struct vertexuse	*vu1a;
struct vertexuse	*vu1b;
struct vertexuse	*vu2a;
struct vertexuse	*vu2b;
struct edgeuse		*eu1;
struct edgeuse		*eu2;
struct faceuse		*fu1;		/* fu of eu1, for plane equation */
struct faceuse		*fu2;		/* fu of eu2, for error checks */
CONST char		*str;
{
	NMG_CK_VERTEXUSE(vu1a);
	NMG_CK_VERTEXUSE(vu1b);
	NMG_CK_VERTEXUSE(vu2a);
	NMG_CK_VERTEXUSE(vu2b);
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_two_colinear_edge2p(x%x, x%x) %s\n", eu1, eu2, str);
	nmg_ck_face_worthless_edges( fu1 );
	nmg_ck_face_worthless_edges( fu2 );

	/* First intersection point:  break eu1 */
	if( dist[0] == 0 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu1a intersects vu2a\n");
		(void)nmg_tbl(l1, TBL_INS_UNIQUE, &vu1a->l.magic);
		(void)nmg_tbl(l2, TBL_INS_UNIQUE, &vu2a->l.magic);
		nmg_jv(vu1a->v_p, vu2a->v_p);
	} else if( dist[0] == 1 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu1a intersects vu2b\n");
		(void)nmg_tbl(l1, TBL_INS_UNIQUE, &vu1a->l.magic);
		(void)nmg_tbl(l2, TBL_INS_UNIQUE, &vu2b->l.magic);
		nmg_jv(vu1a->v_p, vu2b->v_p);
	} else if( dist[0] > 0 && dist[0] < 1 )  {
		/* Break eu1 into two pieces */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu2a=x%x breaks eu1=x%x\n", vu2a, eu1 );
		(void)nmg_ebreak( vu2a->v_p, eu1 );
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
	}

	/* Second intersection point: break eu1 again */
	if( dist[1] == 0 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu1a intersects vu2b\n");
		(void)nmg_tbl(l1, TBL_INS_UNIQUE, &vu1a->l.magic);
		(void)nmg_tbl(l2, TBL_INS_UNIQUE, &vu2b->l.magic);
		nmg_jv(vu1a->v_p, vu2b->v_p);
	} else if( dist[1] == 1 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu1b intersects vu2b\n");
		(void)nmg_tbl(l1, TBL_INS_UNIQUE, &vu1b->l.magic);
		(void)nmg_tbl(l2, TBL_INS_UNIQUE, &vu2b->l.magic);
		nmg_jv(vu1b->v_p, vu2b->v_p);
	} else if( dist[1] > 0 && dist[1] < 1 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu2b=x%x breaks eu1=x%x\n", vu2b, eu1 );
		(void)nmg_ebreak( vu2b->v_p, eu1 );
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
	}
}

/*
 *			N M G _ I S E C T _ E D G E 2 P _ E D G E 2 P
 *
 *  Actual 2d edge/edge intersector
 *
 *  Returns -
 *	0	no intersection
 *	1	intersection was at a shared vertex
 *	2	one or both edges was split at (geometric) intersection.
 */
int
nmg_isect_edge2p_edge2p( is, eu1, eu2, fu1, fu2 )
struct nmg_inter_struct	*is;
struct edgeuse		*eu1;
struct edgeuse		*eu2;
struct faceuse		*fu1;		/* fu of eu1, for plane equation */
struct faceuse		*fu2;		/* fu of eu2, for error checks */
{
	point_t		eu1_start;
	point_t		eu1_end;
	vect_t		eu1_dir;
	point_t		eu2_start;
	point_t		eu2_end;
	vect_t		eu2_dir;
	vect_t		dir3d;
	fastf_t	dist[2];
	int	status;
	point_t		hit_pt;
	struct vertexuse	*vu;
	struct vertexuse	*vu1a, *vu1b;
	struct vertexuse	*vu2a, *vu2b;
	int		ret = 0;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	/*
	 * Important note:  don't use eu1->eumate_p->vu_p here,
	 * because that vu is in the opposite orientation faceuse.
	 * Putting those vu's on the intersection line makes for big trouble.
	 */
	vu1a = eu1->vu_p;
	vu1b = RT_LIST_PNEXT_CIRC( edgeuse, eu1 )->vu_p;
	vu2a = eu2->vu_p;
	vu2b = RT_LIST_PNEXT_CIRC( edgeuse, eu2 )->vu_p;
	NMG_CK_VERTEXUSE(vu1a);
	NMG_CK_VERTEXUSE(vu1b);
	NMG_CK_VERTEXUSE(vu2a);
	NMG_CK_VERTEXUSE(vu2b);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_edge2p_edge2p(eu1=x%x, eu2=x%x, fu1=x%x, fu2=x%x)\n\tvu1a=%x vu1b=%x, vu2a=%x vu2b=%x\n\tv1a=%x v1b=%x,   v2a=%x v2b=%x\n",
			eu1, eu2, fu1, fu2,
			vu1a, vu1b, vu2a, vu2b,
			vu1a->v_p, vu1b->v_p, vu2a->v_p, vu2b->v_p );

	/*
	 *  Topology check.
	 *  If both endpoints of both edges match, this is a trivial accept.
	 */
	if( (vu1a->v_p == vu2a->v_p && vu1b->v_p == vu2b->v_p) ||
	    (vu1a->v_p == vu2b->v_p && vu1b->v_p == vu2a->v_p) )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("edge2p_edge2p: shared edge topology, both ends\n");
	    	ret = 1;
		goto topo;
	}

	/*
	 *  The 3D line in is->pt and is->dir is prepared by the caller.
	 *  is->pt is *not* one of the endpoints of this edge.
	 *
	 *  IMPORTANT NOTE:  The edge-ray used for the edge intersection
	 *  calculations is colinear with the "intersection line",
	 *  but the edge-ray starts at vu1a and points to vu1b,
	 *  while the intersection line has to satisfy different constraints.
	 *  Don't confuse the two!
	 */
	nmg_get_2d_vertex( eu1_start, vu1a->v_p, is, fu2 );	/* 2D line */
	nmg_get_2d_vertex( eu1_end, vu1b->v_p, is, fu2 );
	VSUB2( eu1_dir, eu1_end, eu1_start );

	nmg_get_2d_vertex( eu2_start, vu2a->v_p, is, fu2 );
	nmg_get_2d_vertex( eu2_end, vu2b->v_p, is, fu2 );
	VSUB2( eu2_dir, eu2_end, eu2_start );

	dist[0] = dist[1] = 0;	/* for clean prints, below */

	/* The "proper" thing to do is intersect two line segments.
	 * However, this means that none of the intersections of edge "line"
	 * with the exterior of the loop are computed, and that
	 * violates the strategy assumptions of the face-cutter.
	 */
	/* To pick up ALL intersection points, the source edge is a line */
	status = rt_isect_line2_lseg2( dist, eu1_start, eu1_dir,
			eu2_start, eu2_dir, &is->tol );

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("\trt_isect_line2_lseg2()=%d, dist: %g, %g\n",
			status, dist[0], dist[1] );
	}

	/*
	 *  If one endpoint matches, and edges are not colinear,
	 *  then accept the one shared vertex as the intersection point.
	 */
	if( status != 0 && (
	    vu1a->v_p == vu2a->v_p || vu1a->v_p == vu2b->v_p ||
	    vu2a->v_p == vu1b->v_p || vu2b->v_p == vu1b->v_p )
	)  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("edge2p_edge2p: non-colinear edges share one vertex (topology)\n");
		ret = 1;
		goto topo;
	}

	if (status < 0)  return 0;	/* No geometric intersection */

	if( status == 0 )  {
		fastf_t	eu2dist[2];
		fastf_t	ptol;

		/*
		 *  Special case of colinear overlapping edges.
		 *  There may be 2 intersect points.
		 *  dist[1] has special meaning for this return status:
		 *  it's distance w.r.t. eu1's 1st point (vu1a), not eu2's 1st.
		 *  Find break points on eu1 caused by vu2[ab].
		 */
		nmg_isect_two_colinear_edge2p( dist, is->l1, is->l2,
			vu1a, vu1b, vu2a, vu2b,
			eu1, eu2, fu1, fu2, "eu1/eu2" );

		/*
		 *  If the segments only partially overlap, need to intersect
		 *  the other way as well.
		 *  Find break points on eu2 caused by vu1[ab].
		 */
		if( fabs(eu2_dir[X]) >= fabs(eu2_dir[Y]) )  {
			eu2dist[0] = (eu1_start[X] - eu2_start[X])/eu2_dir[X];
			eu2dist[1] = (eu1_start[X] + eu1_dir[X] - eu2_start[X])/eu2_dir[X];
		} else {
			eu2dist[0] = (eu1_start[Y] - eu2_start[Y])/eu2_dir[Y];
			eu2dist[1] = (eu1_start[Y] + eu1_dir[Y] - eu2_start[Y])/eu2_dir[Y];
		}
		/* Tolerance processing */
		ptol = is->tol.dist / sqrt( eu2_dir[X]*eu2_dir[X] + eu2_dir[Y]*eu2_dir[Y] );
		if( eu2dist[0] > -ptol && eu2dist[0] < ptol )  eu2dist[0] = 0;
		else if( eu2dist[0] > 1-ptol && eu2dist[0] < 1+ptol ) eu2dist[0] = 1;

		if( eu2dist[1] > -ptol && eu2dist[1] < ptol )  eu2dist[1] = 0;
		else if( eu2dist[1] > 1-ptol && eu2dist[1] < 1+ptol ) eu2dist[1] = 1;

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			rt_log("\trt_isect_line2_lseg2()=%d, eu2dist: %g, %g\n",
				status, eu2dist[0], eu2dist[1] );
rt_log("ptol = %g, eu2dist=%g, %g\n", ptol, eu2dist[0], eu2dist[1]);
		}

		nmg_isect_two_colinear_edge2p( eu2dist, is->l2, is->l1,
			vu2a, vu2b, vu1a, vu1b,
			eu2, eu1, fu2, fu1, "eu2/eu1" );

		return 2;	/* XXX unsure of what happened, be conservative */
	}

	/* There is only one intersect point.  Break one or both edges. */


	/* The ray defined by the edgeuse intersects the eu2.
	 * Tolerances have already been factored in.
	 * The edge exists over values of 0 <= dist <= 1.
	 */
	VSUB2( dir3d, vu1b->v_p->vg_p->coord, vu1a->v_p->vg_p->coord );
	VJOIN1( hit_pt, vu1a->v_p->vg_p->coord, dist[0], dir3d );

	if ( dist[0] == 0 )  {
		/* First point of eu1 is on eu2, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu=x%x vu1a is intersect point\n", vu1a);
		if( dist[1] < 0 || dist[1] > 1 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\teu1 line intersects eu2 outside vu2a...vu2b range, ignore.\n");
			return 0;
		}
		(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1a->l.magic);

		/* Edges not colinear. Either join up with a matching vertex,
		 * or break eu2 on our vert.
		 */
		if( dist[1] == 0 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tvu2a matches vu1a\n");
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2a->l.magic);
			nmg_jv(vu1a->v_p, vu2a->v_p);
			return 1;
		}
		if( dist[1] == 1 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tsecond point of eu2 matches vu1a\n");
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
			nmg_jv(vu1a->v_p, vu2b->v_p);
			return 1;
		}
		/* Break eu2 on our first vertex */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tbreaking eu2 on vu1a\n");
		(void)nmg_ebreak( vu1a->v_p, eu2 );
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
		return 2;
	}

	if ( dist[0] == 1 )  {
		/* Second point of eu1 is on eu2, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu=x%x vu1b is intersect point\n", vu1b);
		if( dist[1] < 0 || dist[1] > 1 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\teu1 line intersects eu2 outside vu2a...vu2b range, ignore.\n");
			return 0;
		}
		(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1b->l.magic);

		/* Edges not colinear. Either join up with a matching vertex,
		 * or break eu2 on our vert.
		 */
		if( dist[1] == 0 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tvu2a matches vu1b\n");
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2a->l.magic);
			nmg_jv(vu1b->v_p, vu2a->v_p);
			return 1;
		}
		if( dist[1] == 1 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tsecond point of eu2 matches vu1b\n");
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
			nmg_jv(vu1b->v_p, vu2b->v_p);
			return 1;
		}
		/* Break eu2 on our second vertex */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tbreaking eu2 on vu1b\n");
		(void)nmg_ebreak( vu1b->v_p, eu2 );
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
		return 2;
	}

	/* eu2 intersect point is on eu1 line, but not between vertices */
	if( dist[0] < 0 || dist[0] > 1 )  {
		struct loopuse *plu;
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tIntersect point on eu2 is outside vu1a...vu1b.  Break eu2 anyway.\n");

		if( dist[1] == 0 )  {
			if( vu = nmg_find_v_in_face( vu2a->v_p, fu1 ) )  {
				if (rt_g.NMG_debug & DEBUG_POLYSECT)
					rt_log("\t\tIntersect point is vu2a\n");
			} else {
				if (rt_g.NMG_debug & DEBUG_POLYSECT)
					rt_log("\t\tIntersect point is vu2a, make self-loop in fu1\n");
				plu = nmg_mlv(&fu1->l.magic, vu2a->v_p, OT_UNSPEC);
				nmg_loop_g(plu->l_p);
				vu = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			}
			NMG_CK_VERTEXUSE(vu);
			(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu->l.magic);
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2a->l.magic);
			return 1;
		} else if( dist[1] == 1 )  {
			if( vu = nmg_find_v_in_face( vu2b->v_p, fu1 ) )  {
				if (rt_g.NMG_debug & DEBUG_POLYSECT)
					rt_log("\t\tIntersect point is vu2b\n");
			} else {
				if (rt_g.NMG_debug & DEBUG_POLYSECT)
					rt_log("\t\tIntersect point is vu2b, make self-loop in fu1\n");
				plu = nmg_mlv(&fu1->l.magic, vu2b->v_p, OT_UNSPEC);
				nmg_loop_g(plu->l_p);
				vu = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			}
			NMG_CK_VERTEXUSE(vu);
			(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu->l.magic);
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
			return 1;
		} else if( dist[1] > 0 && dist[1] < 1 )  {
			/* Break eu2 somewhere in the middle */
			struct vertexuse	*new_vu2;
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    	VPRINT("\t\tBreaking eu2 at intersect point", hit_pt);
			new_vu2 = nmg_ebreak( NULL, eu2 )->vu_p;
			nmg_vertex_gv( new_vu2->v_p, hit_pt );	/* 3d geom */

			plu = nmg_mlv(&fu1->l.magic, new_vu2->v_p, OT_UNSPEC);
			nmg_loop_g(plu->l_p);
			vu = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			NMG_CK_VERTEXUSE(vu);

			(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu->l.magic);
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &new_vu2->l.magic);
			nmg_ck_face_worthless_edges( fu1 );
			nmg_ck_face_worthless_edges( fu2 );
			return 2;
		}

		/* Ray misses eu2, nothing to do */
		return 0;
	}

	/* Intersection is in the middle of the reference edge */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("\tintersect is in middle of eu1, breaking it\n");

	/* Edges not colinear. Either join up with a matching vertex,
	 * or break eu2 on our vert.
	 */
	if( dist[1] == 0 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\t\tintersect point is vu2a\n");
		nmg_ebreak( vu2a->v_p, eu1 );
		(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1b->l.magic);
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2a->l.magic);
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
	} else if( dist[1] == 1 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\t\tintersect point is vu2b\n");
		nmg_ebreak( vu2b->v_p, eu1 );
		(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1b->l.magic);
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
	} else {
		/* Intersection is in the middle of both, split edge */
		struct vertex	*new_v;
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
		    	VPRINT("\t\tBreaking both edges at intersect point", hit_pt);
		new_v = nmg_e2break( eu1, eu2 );
		nmg_vertex_gv( new_v, hit_pt );	/* 3d geometry */

		/* new_v is at far end of eu1 */
		if( eu1->eumate_p->vu_p->v_p != new_v ) rt_bomb("new_v 1\n");
		/* Can't use eumate_p here, it's in wrong orientation face */
		(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &RT_LIST_PNEXT_CIRC(edgeuse,eu1)->vu_p->l.magic);
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &RT_LIST_PNEXT_CIRC(edgeuse,eu2)->vu_p->l.magic);
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
	}
	ret = 2;	/* an edge was broken */

	/*
	 *  Now that processing is done and edges may have been shortened,
	 *  enrole any shared topology (representing edge intersections)
	 *  in the list of vu's encountered along this edge.
	 *  Note that either (or both) vu's may be shared in the case
	 *  of co-linear edges.
	 *  Can't use eumate_p here, it's in wrong orientation faceuse.
	 */
topo:
	vu1a = eu1->vu_p;
	vu1b = RT_LIST_PNEXT_CIRC(edgeuse,eu1)->vu_p;
	vu2a = eu2->vu_p;
	vu2b = RT_LIST_PNEXT_CIRC(edgeuse,eu2)->vu_p;
	/*
	 *  vu1a and vu1b and their duals MUST be listed on
	 *  the intersection line.
	 *  Given that the edges are coplanar, if each vu does not
	 *  have a (dual) reference in the other face, then we need to
	 *  create a self-loop vu over there.
	 */
	(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1a->l.magic);
	(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1b->l.magic);

	/* Find dual of vu1a in fu2 */
	if( vu1a->v_p == vu2a->v_p )  {
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2a->l.magic);
	} else if( vu1a->v_p == vu2b->v_p )  {
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
	} else if( vu=nmg_find_v_in_face(vu1a->v_p, fu2) )  {
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu->l.magic);
	} else {
		struct loopuse *plu;
		plu = nmg_mlv(&fu2->l.magic, vu1a->v_p, OT_UNSPEC);
		nmg_loop_g(plu->l_p);
		vu = RT_LIST_FIRST( vertexuse, &plu->down_hd );
		NMG_CK_VERTEXUSE(vu);
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu->l.magic);
	}

	/* Find dual of vu1b in fu2 */
	if( vu1b->v_p == vu2a->v_p )  {
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2a->l.magic);
	} else if( vu1b->v_p == vu2b->v_p )  {
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
	} else if( vu=nmg_find_v_in_face(vu1b->v_p, fu2) )  {
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu->l.magic);
	} else {
		struct loopuse *plu;
		plu = nmg_mlv(&fu2->l.magic, vu1b->v_p, OT_UNSPEC);
		nmg_loop_g(plu->l_p);
		vu = RT_LIST_FIRST( vertexuse, &plu->down_hd );
		NMG_CK_VERTEXUSE(vu);
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu->l.magic);
	}

	return ret;
}

/*
 *			N M G _ I S E C T _ 3 E D G E _ 3 F A C E
 *
 *	Intersect an edge with a (hopefully non-colinear/coplanar) face
 *
 * This code currently assumes that an edge can only intersect a face at one
 * point.  This is probably a bad assumption for the future
 *
 *  The line of intersection between the two faces is found in
 *  nmg_isect_two_face2p().
 */
static void
nmg_isect_3edge_3face(bs, eu, fu)
struct nmg_inter_struct *bs;
struct edgeuse *eu;
struct faceuse *fu;
{
	struct vertexuse *vu_other;
	struct vertex	*v1;
	struct vertex	*v1mate;
	point_t		hit_pt;
	vect_t		edge_vect;
	fastf_t		edge_len;	/* MAGNITUDE(edge_vect) */
	fastf_t		dist;		/* parametric dist to hit point */
	fastf_t		dist_to_plane;	/* distance to hit point, in mm */
	int		status;
	struct loopuse	*plu;
	fastf_t		dist2;
	vect_t		start_pt;
	struct edgeuse	*euforw;
	struct edgeuse	*eunext;
	struct edgeuse	*eulast;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_3edge_3face(, eu=x%x, fu=x%x) START\n", eu, fu);

	NMG_CK_INTER_STRUCT(bs);
	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(eu->vu_p);
	v1 = eu->vu_p->v_p;
	NMG_CK_VERTEX(v1);
	NMG_CK_VERTEX_G(v1->vg_p);

	NMG_CK_EDGEUSE(eu->eumate_p);
	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	v1mate = eu->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX(v1mate);
	NMG_CK_VERTEX_G(v1mate->vg_p);

	if( fu->orientation != OT_SAME )  rt_bomb("nmg_isect_3edge_3face() fu not OT_SAME\n");

	/*
	 *  Form a ray that starts at one vertex of the edgeuse
	 *  and points to the other vertex.
	 */
	VSUB2(edge_vect, v1mate->vg_p->coord, v1->vg_p->coord);
	edge_len = MAGNITUDE(edge_vect);

	VMOVE( start_pt, v1->vg_p->coord );

	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("Testing (%g, %g, %g) -> (%g, %g, %g) dir=(%g, %g, %g)\n",
			V3ARGS(start_pt),
			V3ARGS(v1mate->vg_p->coord),
			V3ARGS(edge_vect) );
	}

	status = rt_isect_line3_plane(&dist, start_pt, edge_vect,
		fu->f_p->fg_p->N, &bs->tol);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    if (status >= 0)
		rt_log("\tHit. Status of rt_isect_line3_plane: %d dist: %g\n",
				status, dist);
	    else
		rt_log("\tMiss. Boring status of rt_isect_line3_plane: %d\n",
				status);
	}
	if( status == 0 )  {
		struct nmg_inter_struct	is;
		struct nmg_ptbl		t1, t2;
		point_t	foo;

		/*
		 *  Edge (ray) lies in the plane of the other face,
		 *  by geometry.  Drop into 2D code to handle all
		 *  possible intersections (there may be many),
		 *  and any cut/joins, then resume with the previous work.
		 */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("nmg_isect_3edge_3face: edge lies ON face, using 2D code\n");

		is = *bs;	/* make private copy */
		is.vert2d = 0;	/* Don't use previously initialized stuff */

		nmg_isect_edge2p_face2p( &is, eu, fu, nmg_find_fu_of_eu(eu) );

		if( is.vert2d )  rt_free( (char *)is.vert2d, "vert2d");

		/* See if start vertex is now shared */
		if (vu_other=nmg_find_v_in_face(eu->vu_p->v_p, fu)) {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tEdge start vertex lies on other face (2d topology).\n");
			(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eu->vu_p->l.magic);
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		}
		goto out;
	}

	/*
	 *  We now know that the the edge does not lie +in+ the other face,
	 *  so it will intersect the face in at most one point.
	 *  Before looking at the results of the geometric calculation,
	 *  check the topology.  If the topology says that starting vertex
	 *  of this edgeuse is on the other face, that is the hit point.
	 *  Enter the two vertexuses of that starting vertex in the list,
	 *  and return.
	 */
	if (vu_other=nmg_find_v_in_face(v1, fu)) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			rt_log("\tEdge start vertex lies on other face (topology).\n\tAdding vu1=x%x (v=x%x), vu_other=x%x (v=x%x)\n",
				eu->vu_p, eu->vu_p->v_p,
				vu_other, vu_other->v_p);
		}
		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eu->vu_p->l.magic);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		goto out;
	}

	if (status < 0)  {
		/*  Ray does not strike plane.
		 *  See if start point lies on plane.
		 */
		dist = VDOT( start_pt, fu->f_p->fg_p->N ) - fu->f_p->fg_p->N[3];
		if( !NEAR_ZERO( dist, bs->tol.dist ) )
			goto out;		/* No geometric intersection */

		/* Start point lies on plane of other face */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tEdge start vertex lies on other face (geometry)\n");
		dist = VSUB2DOT( v1->vg_p->coord, start_pt, edge_vect )
				/ edge_len;
	}

	/* The ray defined by the edgeuse intersects the plane 
	 * of the other face.  Check to see if the distance to
         * intersection is between limits of the endpoints of
	 * this edge(use).
	 * The edge exists over values of 0 <= dist <= 1, ie,
	 * over values of 0 <= dist_to_plane <= edge_len.
	 * The tolerance, an absolute distance, can only be compared
	 * to other absolute distances like dist_to_plane & edge_len.
	 * The vertices are "fattened" by +/- bs->tol units.
	 */
	VJOIN1( hit_pt, start_pt, dist, edge_vect );
	dist_to_plane = edge_len * dist;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("\tedge_len=%g, dist=%g, dist_to_plane=%g\n",
			edge_len, dist, dist_to_plane);

	if ( dist_to_plane < -bs->tol.dist )  {
		/* Hit is behind first point */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tplane behind first point\n");
		goto out;
	}


	/*
	 * If the vertex on the other end of this edgeuse is on the face,
	 * then make a linkage to an existing face vertex (if found),
	 * and give up on this edge, knowing that we'll pick up the
	 * intersection of the next edgeuse with the face later.
	 */
	if ( dist_to_plane < bs->tol.dist )  {
		/* First point is on plane of face, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tedge starts at plane intersect\n");
		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eu->vu_p->l.magic);

		vu_other = nmg_find_pt_in_face(v1->vg_p->coord, fu, &(bs->tol));
		if (vu_other) {
			register pointp_t	p3;
			/* Face has a very similar vertex.  Add to list */
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
			/* Make new coordinates be the midpoint */
			p3 = vu_other->v_p->vg_p->coord;
			VADD2SCALE(v1->vg_p->coord, v1->vg_p->coord, p3, 0.5);
			/* Combine the two vertices */
			nmg_jv(v1, vu_other->v_p);
		} else {
			/* Insert copy of this vertex into face */
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    	VPRINT("Making vertexloop",
			    		v1->vg_p->coord);

			plu = nmg_mlv(&fu->l.magic, v1, OT_UNSPEC);
			nmg_loop_g(plu->l_p);
			vu_other = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			NMG_CK_VERTEXUSE(vu_other);
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		}
		goto out;
	}

	if ( dist_to_plane < edge_len - bs->tol.dist) {
		/* Intersection is between first and second vertex points.
		 * Insert new vertex at intersection point.
		 */
		nmg_break_3edge_at_plane(hit_pt, fu, bs, eu, v1, v1mate);
		goto out;
	}

	if ( dist_to_plane < edge_len + bs->tol.dist) {
		/* Second point is on plane of face, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tedge ends at plane intersect\n");

		eunext = RT_LIST_PNEXT_CIRC(edgeuse,eu);
		NMG_CK_EDGEUSE(eunext);
		if( eunext->vu_p->v_p != v1mate )
			rt_bomb("nmg_isect_3edge_3face: discontinuous eu loop\n");

		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eunext->vu_p->l.magic);

		vu_other = nmg_find_pt_in_face(v1mate->vg_p->coord, fu, &(bs->tol));
		if (vu_other) {
			register pointp_t	p3;
			/* Face has a very similar vertex.  Add to list */
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
			/* Make new coordinates be the midpoint */
			p3 = vu_other->v_p->vg_p->coord;
			VADD2SCALE(v1mate->vg_p->coord, v1mate->vg_p->coord, p3, 0.5);
			/* Combine the two vertices */
			nmg_jv(v1mate, vu_other->v_p);
		} else {
			/* Insert copy of this vertex into face */
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    	VPRINT("Making vertexloop",
			    		v1mate->vg_p->coord);

			plu = nmg_mlv(&fu->l.magic, v1mate, OT_UNSPEC);
			nmg_loop_g(plu->l_p);
			vu_other = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			NMG_CK_VERTEXUSE(vu_other);
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		}
		goto out;
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("\tdist to plane X: X > MAGNITUDE(edge)\n");

out:
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_3edge_3face(, eu=x%x, fu=x%x) END\n", eu, fu);
}

/*
 *			N M G _ I S E C T _ L O O P 3 P _ F A C E 3 P
 *
 *	Intersect a single loop with another face
 */
static void
nmg_isect_loop3p_face3p(bs, lu, fu)
struct nmg_inter_struct *bs;
struct loopuse *lu;
struct faceuse *fu;
{
	struct edgeuse	*eu;
	long		magic1;

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("nmg_isect_loop3p_face3p(, lu=x%x, fu=x%x)\n", lu, fu);
		HPRINT("  fg N", fu->f_p->fg_p->N);
	}

	NMG_CK_INTER_STRUCT(bs);
	NMG_CK_LOOPUSE(lu);
	NMG_CK_LOOP(lu->l_p);
	NMG_CK_LOOP_G(lu->l_p->lg_p);

	NMG_CK_FACEUSE(fu);

	magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		struct vertexuse	*vu = RT_LIST_FIRST(vertexuse,&lu->down_hd);
		/* this is most likely a loop inserted when we split
		 * up fu2 wrt fu1 (we're now splitting fu1 wrt fu2)
		 */
		nmg_isect_3vertex_3face(bs, vu, fu);
		return;
	} else if (magic1 != NMG_EDGEUSE_MAGIC) {
		rt_bomb("nmg_isect_loop3p_face3p() Unknown type of NMG loopuse\n");
	}

	/*  Process loop consisting of a list of edgeuses.
	 *
	 * By going backwards around the list we avoid
	 * re-processing an edgeuse that was just created
	 * by nmg_isect_3edge_3face.  This is because the edgeuses
	 * point in the "next" direction, and when one of
	 * them is split, it inserts a new edge AHEAD or
	 * "nextward" of the current edgeuse.
	 */ 
	for( eu = RT_LIST_LAST(edgeuse, &lu->down_hd );
	     RT_LIST_NOT_HEAD(eu,&lu->down_hd);
	     eu = RT_LIST_PLAST(edgeuse,eu) )  {
		NMG_CK_EDGEUSE(eu);

		if (eu->up.magic_p != &lu->l.magic) {
			rt_bomb("nmg_isect_loop3p_face3p: edge does not share loop\n");
		}

		nmg_isect_3edge_3face(bs, eu, fu);

		nmg_ck_lueu(lu, "nmg_isect_loop3p_face3p");
	}
}

/*
 *			N M G _ I S E C T _ F A C E 3 P _ F A C E 3 P
 *
 *	Intersect entirely of planar face 1 with the entirety of planar face 2
 *	given that the two faces are not coplanar.
 */
static void
nmg_isect_face3p_face3p(bs, fu1, fu2)
struct nmg_inter_struct *bs;
struct faceuse	*fu1;
struct faceuse	*fu2;
{
	struct loopuse	*lu, *fu2lu;
	struct loop_g	*lg;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_face3p_face3p(, fu1=x%x, fu2=x%x) START ++++++++++\n", fu1, fu2);

	NMG_CK_INTER_STRUCT(bs);
	NMG_CK_FACE_G(fu2->f_p->fg_p);
	NMG_CK_FACE_G(fu1->f_p->fg_p);

	if( fu1->orientation != OT_SAME )  rt_bomb("nmg_isect_face3p_face3p() fu1 not OT_SAME\n");
	if( fu2->orientation != OT_SAME )  rt_bomb("nmg_isect_face3p_face3p() fu2 not OT_SAME\n");

	/* process each face loop in face 1 */
	for( RT_LIST_FOR( lu, loopuse, &fu1->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		if (lu->up.fu_p != fu1) {
			rt_bomb("nmg_isect_face3p_face3p() Child loop doesn't share parent!\n");
		}
		NMG_CK_LOOP(lu->l_p);
		lg = lu->l_p->lg_p;
		NMG_CK_LOOP_G(lg);

		/* If the bounding box of a loop doesn't intersect the
		 * bounding box of a loop in the other face, it doesn't need
		 * to get cut.
		 */
		for (RT_LIST_FOR(fu2lu, loopuse, &fu2->lu_hd )){
			struct loop_g *fu2lg;

			NMG_CK_LOOPUSE(fu2lu);
			NMG_CK_LOOP(fu2lu->l_p);
			fu2lg = fu2lu->l_p->lg_p;
			NMG_CK_LOOP_G(fu2lg);

			if (! V3RPP_OVERLAP_TOL( fu2lg->min_pt, fu2lg->max_pt,
			    lg->min_pt, lg->max_pt, &bs->tol)) continue;

			nmg_isect_loop3p_face3p(bs, lu, fu2);
		}
	}
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_face3p_face3p(, fu1=x%x, fu2=x%x) RETURN ++++++++++\n\n", fu1, fu2);
}

/*
 *			N M G _ I S E C T _ E D G E 2 P _ F A C E 2 P
 *
 *  Given one (2D) edge (eu1) lying in the plane of another face (fu2),
 *  intersect with all the other edges of that face.
 *  This edge represents a line of intersection, and so a
 *  cutjoin/mesh pass will be needed for each one.
 *
 *  Note that this routine completely conducts the
 *  intersection operation, so that edges may come and go, loops
 *  may join or split, each time it is called.
 *  This imposes special requirements on handling the march through
 *  the linked lists in this routine.
 *
 *  This also means that much of argument "is" is changed each call.
 */
static void
nmg_isect_edge2p_face2p( is, eu1, fu2, fu1 )
struct nmg_inter_struct	*is;
struct edgeuse		*eu1;		/* edge to be intersected w/fu2 */
struct faceuse		*fu2;		/* face to be intersected w/eu1 */
struct faceuse		*fu1;		/* fu that eu1 is from */
{
	struct nmg_ptbl vert_list1, vert_list2;
	struct loopuse		*lu;
	struct vertexuse	*vu1;
	struct vertexuse	*vu2;
	struct edgeuse		*fu2_eu;	/* use of edge in fu2 */
	struct xray		line;
	vect_t			invdir;
	int			another_pass;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_FACEUSE(fu2);
	NMG_CK_FACEUSE(fu1);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_edge2p_face2p(eu1=x%x, fu2=x%x, fu1=x%x)\n", eu1, fu2, fu1);

	if( fu2->orientation != OT_SAME )  rt_bomb("nmg_isect_edge2p_face2p() fu2 not OT_SAME\n");
	if( fu1->orientation != OT_SAME )  rt_bomb("nmg_isect_edge2p_face2p() fu1 not OT_SAME\n");

	/*  See if an edge exists in other face that connects these 2 verts */
	/* XXX This searches whole other shell.  Should be shared w/fu2, but... */
	fu2_eu = nmg_findeu( eu1->vu_p->v_p, eu1->eumate_p->vu_p->v_p,
	    fu2->s_p, (CONST struct edgeuse *)NULL, 0 );
	if( fu2_eu != (struct edgeuse *)NULL )  {
		/* There is an edge in other face that joins these 2 verts. */
		NMG_CK_EDGEUSE(fu2_eu);
		if( fu2_eu->e_p != eu1->e_p )  {
			/* Not the same edge, fuse! */
			rt_log("nmg_isect_edge2p_face2p() fusing unshared shared edge\n");
			nmg_radial_join_eu( eu1, fu2_eu, &is->tol );
			return;		/* Topology is completely shared */
		}
	}

	(void)nmg_tbl(&vert_list1, TBL_INIT,(long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_INIT,(long *)NULL);

    	is->l1 = &vert_list1;
    	is->l2 = &vert_list2;

    	if (rt_g.NMG_debug & (DEBUG_POLYSECT|DEBUG_FCUT|DEBUG_MESH)
    	    && rt_g.NMG_debug & DEBUG_PLOTEM) {
    		static int fno=1;
    	    	nmg_pl_2fu( "Isect_2d_faces%d.pl", fno++, fu2, fu1, 0 );
    	}

	/*
	 *  Construct the ray which contains the line of intersection,
	 *  i.e. the line that contains the edge "eu1".
	 *
	 *  See the comment in nmg_isect_two_generic_faces() for details
	 *  on the constraints on this ray, and the algorithm.
	 */
	vu1 = eu1->vu_p;
	vu2 = RT_LIST_PNEXT_CIRC( edgeuse, eu1 )->vu_p;
	if( vu1->v_p == vu2->v_p )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("nmg_isect_edge2p_face2p(eu1=x%x) skipping 0-len edge (topology)\n", eu1);
		goto out;
	}
	VMOVE( line.r_pt, vu1->v_p->vg_p->coord );		/* 3D line */
	VSUB2( line.r_dir, vu2->v_p->vg_p->coord, line.r_pt );
	VUNITIZE( line.r_dir );
	VINVDIR( invdir, line.r_dir );
VPRINT("fu2 min", fu2->f_p->fg_p->min_pt);
VPRINT("fu2 max", fu2->f_p->fg_p->max_pt);
	if( !rt_in_rpp( &line, invdir, fu2->f_p->fg_p->min_pt, fu2->f_p->fg_p->max_pt ) )  {
		rt_bomb("nmg_isect_edge2p_face2p() edge ray missed face bounding RPP\n");
	}
rt_log("r_min=%g, r_max=%g\n", line.r_min, line.r_max);
	/* Start point will line on min side of face RPP */
	VJOIN1( is->pt, line.r_pt, line.r_min, line.r_dir );
	if( line.r_min > line.r_max )  {
		/* Direction is heading the wrong way, flip it */
		VREVERSE( is->dir, line.r_dir );
rt_log("flipping dir\n");
	} else {
		VMOVE( is->dir, line.r_dir );
	}
VPRINT("r_pt ", line.r_pt);
VPRINT("r_dir", line.r_dir);
VPRINT("->pt ", is->pt);
VPRINT("->dir", is->dir);

nmg_fu_touchingloops(fu2);
nmg_fu_touchingloops(fu1);
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
	/* Run through the list until no more edges are split */
	another_pass = 0;
	do  {
		for( RT_LIST_FOR( lu, loopuse, &fu2->lu_hd ) )  {
			struct edgeuse	*eu2;

			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )  {
				struct vertexuse	*vu;
				vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
				if( nmg_isect_edge2p_vert2p( is, eu1, vu ) > 1 )
					another_pass++;	/* edge was split */
				continue;
			}
			for( RT_LIST_FOR( eu2, edgeuse, &lu->down_hd ) )  {
				/* isect eu1 with eu2 */
				if( nmg_isect_edge2p_edge2p( is, eu1, eu2, fu1, fu2 ) > 1 )
					another_pass++;	/* edge was split */
nmg_fu_touchingloops(fu2);
nmg_fu_touchingloops(fu1);
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
			}
		}
	} while( another_pass );

    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_edge2p_face2p(eu1=x%x, fu2=x%x) vert_lists A:\n", eu1, fu2 );
    		nmg_pr_ptbl_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_ptbl_vert_list( "vert_list2", &vert_list2 );
    	}

	nmg_purge_unwanted_intersection_points(&vert_list1, fu2, &is->tol);
	nmg_purge_unwanted_intersection_points(&vert_list2, fu1, &is->tol);

    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_edge2p_face2p(eu1=x%x, fu2=x%x) vert_lists B:\n", eu1, fu2 );
    		nmg_pr_ptbl_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_ptbl_vert_list( "vert_list2", &vert_list2 );
    	}

    	if (vert_list1.end == 0 && vert_list2.end == 0) {
    		/* there were no intersections */
    		goto out;
    	}

nmg_fu_touchingloops(fu2);
nmg_fu_touchingloops(fu1);
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
	nmg_face_cutjoin(&vert_list1, &vert_list2, fu1, fu2, is->pt, is->dir, &is->tol);
nmg_fu_touchingloops(fu2);		/* XXX r410 dies here */
nmg_fu_touchingloops(fu1);
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
	nmg_mesh_faces(fu2, fu1, &is->tol);

out:
	(void)nmg_tbl(&vert_list1, TBL_FREE, (long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_FREE, (long *)NULL);
nmg_fu_touchingloops(fu2);
nmg_fu_touchingloops(fu1);
}

/*
 *			N M G _ I S E C T _ T W O _ F A C E 2 P
 *
 *  Manage the mutual intersection of two 3-D coplanar planar faces.
 */
static void
nmg_isect_two_face2p( is, fu1, fu2 )
struct nmg_inter_struct	*is;
struct faceuse		*fu1, *fu2;
{
	struct loopuse	*lu;
	struct edgeuse	*eu;
	struct vertexuse	*vu;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

#if 0
	/* r71, r23 are useful demonstrations */
	/* Turn these flags on only for early code debugging */
	rt_g.NMG_debug |= DEBUG_POLYSECT;
	rt_g.NMG_debug |= DEBUG_FCUT;
	rt_g.NMG_debug |= DEBUG_VU_SORT;
	rt_g.NMG_debug |= DEBUG_PLOTEM;
	rt_g.NMG_debug |= DEBUG_INS;
	rt_g.NMG_debug |= DEBUG_FCUT;
#endif

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_two_face2p\n");

	/* For every edge in f1, intersect with f2, incl. cutjoin */
f1_again:
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );
	for( RT_LIST_FOR( lu, loopuse, &fu1->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )  {
			vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
			nmg_isect_vert2p_face2p( is, vu, fu2 );
			continue;
		}
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			NMG_CK_EDGEUSE(eu);
			nmg_isect_edge2p_face2p( is, eu, fu2, fu1 );
			/* "eu" may have moved to another loop,
			 * need to abort for() to avoid getting
			 * RT_LIST_NEXT on wrong linked list!
			 */
			NMG_CK_EDGEUSE(eu);
			if(eu->up.lu_p != lu)  goto f1_again;
		}
	}

	/* For every edge in f2, intersect with f1, incl. cutjoin */
f2_again:
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );
	for( RT_LIST_FOR( lu, loopuse, &fu2->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )  {
			vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
			nmg_isect_vert2p_face2p( is, vu, fu1 );
			continue;
		}
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			NMG_CK_EDGEUSE(eu);
			nmg_isect_edge2p_face2p( is, eu, fu1, fu2 );
			/* "eu" may have moved to another loop */
			NMG_CK_EDGEUSE(eu);
			if(eu->up.lu_p != lu )  goto f2_again;
		}
	}
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );
}

/*
 *			N M G _ I S E C T _ T W O _ F A C E 3 P
 *
 *  Handle the complete mutual intersection of
 *  two 3-D non-coplanar planar faces,
 *  including cutjoin and meshing.
 */
static void
nmg_isect_two_face3p( is, fu1, fu2 )
struct nmg_inter_struct	*is;
struct faceuse		*fu1, *fu2;
{
	struct nmg_ptbl vert_list1, vert_list2;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("nmg_isect_two_face3p( fu1=x%x, fu2=x%x )\n", fu1, fu2);
		VPRINT("isect ray is->pt ", is->pt);
		VPRINT("isect ray is->dir", is->dir);
	}

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
		nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );
	}
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );

	(void)nmg_tbl(&vert_list1, TBL_INIT,(long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_INIT,(long *)NULL);

    	is->l1 = &vert_list1;
    	is->l2 = &vert_list2;

    	if (rt_g.NMG_debug & (DEBUG_POLYSECT|DEBUG_FCUT|DEBUG_MESH)
    	    && rt_g.NMG_debug & DEBUG_PLOTEM) {
    		static int fno=1;
    	    	nmg_pl_2fu( "Isect_faces%d.pl", fno++, fu1, fu2, 0 );
    	}

	nmg_isect_face3p_face3p(is, fu1, fu2);
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
		nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );
	}

    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_two_generic_faces(fu1=x%x, fu2=x%x) vert_lists A:\n", fu1, fu2);
    		nmg_pr_ptbl_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_ptbl_vert_list( "vert_list2", &vert_list2 );
    	}

    	is->l2 = &vert_list1;
    	is->l1 = &vert_list2;
	nmg_isect_face3p_face3p(is, fu2, fu1);
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
		nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );
	}

	nmg_purge_unwanted_intersection_points(&vert_list1, fu2, &is->tol);
	nmg_purge_unwanted_intersection_points(&vert_list2, fu1, &is->tol);

    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_two_generic_faces(fu1=x%x, fu2=x%x) vert_lists B:\n", fu1, fu2);
    		nmg_pr_ptbl_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_ptbl_vert_list( "vert_list2", &vert_list2 );
    	}

    	if (vert_list1.end == 0 && vert_list2.end == 0) {
    		/* there were no intersections */
    		goto out;
    	}

	nmg_face_cutjoin(&vert_list1, &vert_list2, fu1, fu2, is->pt, is->dir, &is->tol);
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
nmg_region_v_unique( fu1->s_p->r_p, &is->tol );
nmg_region_v_unique( fu2->s_p->r_p, &is->tol );

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
		nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );
	}

	nmg_mesh_faces(fu1, fu2, &is->tol);
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);

#if 0
	show_broken_stuff((long *)fu1, (long **)NULL, 1, 0);
	show_broken_stuff((long *)fu2, (long **)NULL, 1, 0);
#endif

out:
	(void)nmg_tbl(&vert_list1, TBL_FREE, (long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_FREE, (long *)NULL);

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
		nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );
	}
}

/*
 *			N M G _ I S E C T _ T W O _ G E N E R I C _ F A C E S
 *
 *	Intersect a pair of faces
 */
static void
nmg_isect_two_generic_faces(fu1, fu2, tol)
struct faceuse		*fu1, *fu2;
CONST struct rt_tol	*tol;
{
	struct nmg_inter_struct	bs;
	int		i;
	fastf_t		*pl1, *pl2;
	struct face	*f1;
	struct face	*f2;
	point_t		min_pt;
	int		status;

	RT_CK_TOL(tol);
	bs.magic = NMG_INTER_STRUCT_MAGIC;
	bs.vert2d = (fastf_t *)NULL;
	bs.tol = *tol;		/* struct copy */

	NMG_CK_FACEUSE(fu1);
	f1 = fu1->f_p;
	NMG_CK_FACE(f1);
	NMG_CK_FACE_G(f1->fg_p);

	NMG_CK_FACEUSE(fu2);
	f2 = fu2->f_p;
	NMG_CK_FACE(f2);
	NMG_CK_FACE_G(f2->fg_p);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("\nnmg_isect_two_generic_faces(fu1=x%x, fu2=x%x)\n", fu1, fu2);
		pl1 = f1->fg_p->N;
		pl2 = f2->fg_p->N;

		rt_log("Planes\t%gx + %gy + %gz = %g\n\t%gx + %gy + %gz = %g\n",
			pl1[0], pl1[1], pl1[2], pl1[3],
			pl2[0], pl2[1], pl2[2], pl2[3]);
		rt_log( "Cosine of angle between planes = %g\n" , VDOT( pl1 , pl2 ) );
		rt_log( "fu1:\n" );
		nmg_pr_fu_briefly( fu1 , "\t" );
		rt_log( "fu2:\n" );
		nmg_pr_fu_briefly( fu2 , "\t" );
	}
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);

	if ( !V3RPP_OVERLAP_TOL(f2->fg_p->min_pt, f2->fg_p->max_pt,
	    f1->fg_p->min_pt, f1->fg_p->max_pt, &bs.tol) )  return;

	/*
	 *  The extents of face1 overlap the extents of face2.
	 *  Construct a ray which contains the line of intersection.
	 *  There are two choices for direction, and an infinite number
	 *  of candidate points.
	 *
	 *  The correct choice of this ray is very important, so that:
	 *	1)  All intersections are at positive distances on the ray,
	 *	2)  dir cross N will point "left".
	 *
	 *  These two conditions can be satisfied by intersecting the
	 *  line with the face's bounding RPP.  This will give two
	 *  points A and B, where A is closer to the min point of the RPP
	 *  and B is closer to the max point of the RPP.
	 *  Let bs.pt be A, and let bs.dir point from A towards B.
	 *  This choice will satisfy both constraints, above.
	 *
	 *  NOTE:  These conditions must be enforced in the 2D code, also.
	 */
	VMOVE(min_pt, f1->fg_p->min_pt);
	VMIN(min_pt, f2->fg_p->min_pt);
	status = rt_isect_2planes( bs.pt, bs.dir, f1->fg_p->N, f2->fg_p->N,
		min_pt, tol );

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log( "\tnmg_isect_two_generic_faces: intersect ray start (%f , %f , %f )\n\t\tin direction (%f , %f , %f )\n",
			bs.pt[X],
			bs.pt[Y],
			bs.pt[Z],
			bs.dir[X],
			bs.dir[Y],
			bs.dir[Z] );
	}

	switch( status )  {
	case 0:
		/* All is well */
		bs.coplanar = 0;
		nmg_isect_two_face3p( &bs, fu1, fu2 );
		break;
	case -1:
		/* co-planar faces */
rt_log("co-planar faces.\n");
		bs.coplanar = 1;
		nmg_isect_two_face2p( &bs, fu1, fu2 );
		break;
	case -2:
		/* parallel and distinct, no intersection */
		break;
	default:
		/* internal error */
		rt_log("ERROR nmg_isect_two_generic_faces() unable to find plane intersection\n");
		break;
	}

	if(bs.vert2d)  rt_free( (char *)bs.vert2d, "vert2d" );
nmg_region_v_unique( fu1->s_p->r_p, &bs.tol );
nmg_region_v_unique( fu2->s_p->r_p, &bs.tol );
nmg_fu_touchingloops(fu1);
nmg_fu_touchingloops(fu2);
}

/*
 *			N M G _ C R A C K S H E L L S
 *
 *	Split the components of two shells wherever they may intersect,
 *	in preparation for performing boolean operations on the shells.
 */
void
nmg_crackshells(s1, s2, tol)
struct shell		*s1;
struct shell		*s2;
CONST struct rt_tol	*tol;
{
	struct face	*f1;
	struct faceuse	*fu1, *fu2;
	struct shell_a	*sa1, *sa2;
	long		*flags;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_crackshells(s1=x%x, s2=x%x)\n", s1, s2);

	RT_CK_TOL(tol);
	NMG_CK_SHELL(s1);
	sa1 = s1->sa_p;
	NMG_CK_SHELL_A(sa1);

	NMG_CK_SHELL(s2);
	sa2 = s2->sa_p;
	NMG_CK_SHELL_A(sa2);

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vshell( &s1->r_p->s_hd, s1->r_p );
		nmg_vshell( &s2->r_p->s_hd, s2->r_p );
	}

	/* XXX this isn't true for non-3-manifold geometry! */
	if( RT_LIST_IS_EMPTY( &s1->fu_hd ) ||
	    RT_LIST_IS_EMPTY( &s2->fu_hd ) )  {
		rt_log("ERROR:shells must contain faces for boolean operations.");
		return;
	}

	/* See if shells overlap */
	if ( ! V3RPP_OVERLAP_TOL(sa1->min_pt, sa1->max_pt,
	    sa2->min_pt, sa2->max_pt, tol) )
		return;

	flags = (long *)rt_calloc( s1->r_p->m_p->maxindex, sizeof(long),
		"nmg_crackshells flags[]" );

	/*
	 *  Check each of the faces in shell 1 to see
	 *  if they overlap the extent of shell 2
	 */
	for( RT_LIST_FOR( fu1, faceuse, &s1->fu_hd ) )  {
		NMG_CK_FACEUSE(fu1);
		f1 = fu1->f_p;
		NMG_CK_FACE(f1);

		if( NMG_INDEX_IS_SET(flags, f1) )  continue;
		NMG_CK_FACE_G(f1->fg_p);

		/* See if face f1 overlaps shell2 */
		if( ! V3RPP_OVERLAP_TOL(sa2->min_pt, sa2->max_pt,
		    f1->fg_p->min_pt, f1->fg_p->max_pt, tol) )
			continue;

		/*
		 *  Now, check the face f1 from shell 1
		 *  against each of the faces of shell 2
		 */
	    	for( RT_LIST_FOR( fu2, faceuse, &s2->fu_hd ) )  {
	    		NMG_CK_FACEUSE(fu2);
	    		NMG_CK_FACE(fu2->f_p);

			nmg_isect_two_generic_faces(fu1, fu2, tol);

			/* try not to process redundant faceuses (mates) */
			if( RT_LIST_NOT_HEAD( fu2, &s2->fu_hd ) )  {
				register struct faceuse	*nextfu;
				nextfu = RT_LIST_PNEXT(faceuse, fu2 );
				if( nextfu->f_p == fu2->f_p )
					fu2 = nextfu;
			}
	    	}

		/* Check f1 from s1 against wire loops and edges of s2 */

		/* Check f1 from s1 against lone vert of s2 */

	    	NMG_INDEX_SET(flags, f1);

		if( rt_g.NMG_debug & DEBUG_VERIFY )  {
			nmg_vshell( &s1->r_p->s_hd, s1->r_p );
			nmg_vshell( &s2->r_p->s_hd, s2->r_p );
		}
	}

	/*
	 *  Check each wire loop and wire edge of shell 1 against shell 2.
	 */
#if 0
	for( RT_LIST_FOR( lu1, loopuse, &s1->lu_hd ) )  {
	}
	for( RT_LIST_FOR( eu1, edgeuse, &s1->eu_hd ) )  {
	}
#endif

	/* Check each lone vert of s1 against shell 2 */

	rt_free( (char *)flags, "nmg_crackshells flags[]" );

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vshell( &s1->r_p->s_hd, s1->r_p );
		nmg_vshell( &s2->r_p->s_hd, s2->r_p );
	}
}


/*
 *			N M G _ F U _ T O U C H I N G L O O P S
 */
int
nmg_fu_touchingloops(fu)
CONST struct faceuse	*fu;
{
	CONST struct loopuse	*lu;
	CONST struct vertexuse	*vu;

	NMG_CK_FACEUSE(fu);
	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		if( vu = nmg_touchingloops( lu ) )  {
#if 0
			/* Right now, this routine is used for debugging ONLY,
			 * so if this condition exists, die.
			 * However, note that this condition happens a lot
			 * for valid reasons, too.
			 */
			rt_log("nmg_fu_touchingloops(lu=x%x, vu=x%x, v=x%x)\n",
				lu, vu, vu->v_p );
			nmg_pr_lu_briefly(lu,0);
			rt_bomb("nmg_touchingloops()\n");
#endif
			return 1;
		}
	}
	return 0;
}

/*
 *			N M G _ T O U C H I N G L O O P S
 *
 *  Search through all the vertices in a loop.
 *  If there are two distinct uses of one vertex in the loop,
 *  return true.
 *  This is useful for detecting "accordian pleats"
 *  unexpectedly showing up in a loop.
 *  Intended for specific debugging tasks, rather than as a
 *  routine used generally.
 *  Derrived from nmg_split_touchingloops().
 *
 *  Returns -
 *	vu	Yes, the loop touches itself at least once, at this vu.
 *	0	No, the loop does not touch itself.
 */
CONST struct vertexuse *
nmg_touchingloops( lu )
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
