/*
 *			N M G _ F C U T . C
 *
 *  After two faces have been intersected, cut or join loops crossed
 *  by the line of intersection.  (Formerly nmg_comb.c)
 *
 *  The main external routine here is nmg_face_cutjoin().
 *
 *  The line of intersection ("ray") will divide the face into two sets
 *  of loops.
 *  No one loop may cross the ray after this routine is finished.
 *  (Current optimization may remove this property).
 *
 *  Intersection points of significance to the other face but not yet
 *  part of the current face's geometry are denoted by a vu on the ray
 *  list, which points to a loop of a single vertex.  These points
 *  need to be incorporated into the final face.
 *
 *  Authors -
 *	Michael John Muuss
 *	Lee A. Butler
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1992 by the United States Army.
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

/* XXX move to raytrace.h */
RT_EXTERN(double		rt_angle_measure, (vect_t vec, vect_t x_dir,
				vect_t	y_dir));
RT_EXTERN(struct edgeuse	*nmg_eu_with_vu_in_lu, (struct loopuse *lu,
				struct vertexuse *vu));


/* States of the state machine */
#define NMG_STATE_ERROR		0
#define NMG_STATE_OUT		1
#define NMG_STATE_ON_L		2
#define NMG_STATE_ON_R		3
#define NMG_STATE_ON_B		4
#define NMG_STATE_IN		5
static char *nmg_state_names[] = {
	"*ERROR*",
	"out",
	"on_L",
	"on_R",
	"on_both",
	"in",
	"TOOBIG"
};

#define NMG_E_ASSESSMENT_LEFT		0
#define NMG_E_ASSESSMENT_RIGHT		1
#define NMG_E_ASSESSMENT_ON_FORW	2
#define NMG_E_ASSESSMENT_ON_REV		3

#define NMG_V_ASSESSMENT_LONE		16
#define NMG_V_ASSESSMENT_COMBINE(_p,_n)	(((_p)<<2)|(_n))

/* Extract previous and next assessments from combined version */
#define NMG_V_ASSESSMENT_PREV(_a)	(((_a)>>2)&3)
#define NMG_V_ASSESSMENT_NEXT(_a)	((_a)&3)

static char *nmg_v_assessment_names[17] = {
	"Left,Left",
	"Left,Right",
	"Left,On_Forw",
	"Left,On_Rev",
	"Right,Left",
	"Right,Right",
	"Right,On_Forw",
	"Right,On_Rev",
	"On_Forw,Left",
	"On_Forw,Right",
	"On_Forw,On_Forw",
	"On_Forw,On_Rev",
	"On_Rev,Left",
	"On_Rev,Right",
	"On_Rev,On_Forw",
	"On_Rev,On_Rev",
	"LONE_V"
};

static char *nmg_e_assessment_names[4] = {
	"LEFT",
	"RIGHT",
	"ON_FORW",
	"ON_REV"
};

struct nmg_ray_state {
	struct vertexuse	**vu;		/* ptr to vu array */
	int			nvu;		/* len of vu[] */
	point_t			pt;		/* The ray */
	vect_t			dir;
	struct edge_g		*eg_p;		/* Edge geom of the ray */
	struct shell		*sA;
	struct shell		*sB;
	vect_t			left;		/* points left of ray, on face */
	int			state;
	vect_t			ang_x_dir;	/* x axis for angle measure */
	vect_t			ang_y_dir;	/* y axis for angle measure */
};

RT_EXTERN(void			nmg_face_lu_plot, ( struct loopuse *lu, struct nmg_ray_state *rs) );

/*
 *			P T B L _ V S O R T
 *
 *  Sort list of hit points (vertexuse's) in fu1 on plane of fu2,
 *  by increasing distance, vertex ptr, and vu ptr.
 *  Eliminate duplications of vu at same distance.
 *  (Actually, a given vu should show up at exactly 1 distance!)
 *  The line of intersection is pt + t * dir.
 *
 *  For now, a bubble-sort is used, because the list should not have more
 *  than a few hundred entries on it.
 */
static void ptbl_vsort(b, fu1, fu2, pt, dir, mag, dist_tol)
struct nmg_ptbl *b;		/* table of vertexuses on intercept line */
struct faceuse	*fu1;
struct faceuse	*fu2;
point_t		pt;
vect_t		dir;
fastf_t		*mag;
fastf_t		dist_tol;
{
	register struct vertexuse	**vu;
	register int i, j;

	vu = (struct vertexuse **)b->buffer;
	/* check vertexuses and compute distance from start of line */
	for(i = 0 ; i < b->end ; ++i) {
		vect_t		vect;
		NMG_CK_VERTEXUSE(vu[i]);

		VSUB2(vect, vu[i]->v_p->vg_p->coord, pt);
		mag[i] = VDOT( vect, dir );

		/* Find previous vu's at "same" distance, within dist_tol */
		for( j = 0; j < i; j++ )  {
			register fastf_t	tmag;

			tmag = mag[i] - mag[j];
			if( tmag < -dist_tol )  continue;
			if( tmag > dist_tol )  continue;
			/* Nearly equal at same vertex */
			if( mag[i] != mag[j] &&
			    vu[i]->v_p == vu[j]->v_p )  {
	rt_log("ptbl_vsort: forcing vu=x%x & vu=x%x mag equal\n", vu[i], vu[j]);
				mag[j] = mag[i]; /* force equal */
			}
		}
	}

	for(i=0 ; i < b->end - 1 ; ++i) {
		for (j=i+1; j < b->end ; ++j) {

			if( mag[i] < mag[j] )  continue;
			if( mag[i] == mag[j] )  {
				if( vu[i]->v_p < vu[j]->v_p )  continue;
				if( vu[i]->v_p == vu[j]->v_p )  {
					if( vu[i] < vu[j] )  continue;
					if( vu[i] == vu[j] )  {
						int	last = b->end - 1;
						/* vu duplication, eliminate! */
	rt_log("ptbl_vsort: vu duplication eliminated\n");
						if( j >= last )  {
							/* j is last element */
							b->end--;
							break;
						}
						/* rewrite j with last element */
						vu[j] = vu[last];
						mag[j] = mag[last];
						b->end--;
						/* Repeat this index */
						j--;
						continue;
					}
					/* vu[i] > vu[j], fall through */
				}
				/* vu[i]->v_p > vu[j]->v_p, fall through */
			}
			/* mag[i] > mag[j] */

			/* exchange [i] and [j] */
			{
				register struct vertexuse *tvu;
				tvu = vu[i];
				vu[i] = vu[j];
				vu[j] = tvu;
			}

			{
				register fastf_t	tmag;
				tmag = mag[i];
				mag[i] = mag[j];
				mag[j] = tmag;
			}
		}
	}
}

/*
 *			N M G _ V U _ A N G L E _ M E A S U R E
 *
 *  Given a vertexuse from a loop which lies in a plane,
 *  compute the vector 'vec' from the previous vertex to this one.
 *  Using two perpendicular vectors (x_dir and y_dir) which both lie
 *  in the plane of the loop, return the angle (in radians) of 'vec'
 *  from x_dir, going CCW around the perpendicular x_dir CROSS y_dir.
 *
 *  Returns -
 *	vec == x_dir returns 0,
 *	vec == y_dir returns pi/2,
 *	vec == -x_dir returns pi,
 *	vec == -y_dir returns 3*pi/2.
 *	0.0 if unable to compute 'vec'
 */
double
nmg_vu_angle_measure( vu, x_dir, y_dir, assessment )
struct vertexuse	*vu;
vect_t			x_dir;
vect_t			y_dir;
int			assessment;
{	
	struct loopuse	*lu;
	struct edgeuse	*this_eu;
	struct edgeuse	*prev_eu;
	vect_t		vec;
	fastf_t		ang;
	int		entry_ass;

	NMG_CK_VERTEXUSE( vu );
	if( *vu->up.magic_p == NMG_LOOPUSE_MAGIC )  {
		return 0;		/* Unable to compute 'vec' */
	}

	/*
	 *  For consistency, if entry edge is ON the ray,
	 *  force the angles to be exact, don't compute them.
	 */
	entry_ass = NMG_V_ASSESSMENT_PREV( assessment );
	if( entry_ass == NMG_E_ASSESSMENT_ON_FORW )  {
		if(rt_g.NMG_debug&DEBUG_COMBINE)
			rt_log("nmg_vu_angle_measure:  NMG_E_ASSESSMENT_ON_FORW, ang=0\n");
		return 0;		/* zero angle */
	}
	if( entry_ass == NMG_E_ASSESSMENT_ON_REV )  {
		if(rt_g.NMG_debug&DEBUG_COMBINE)
			rt_log("nmg_vu_angle_measure:  NMG_E_ASSESSMENT_ON_FORW, ang=180\n");
		return rt_pi;		/* 180 degrees */
	}

	/*
	 *  Compute the angle
	 */
	lu = nmg_lu_of_vu(vu);
	this_eu = nmg_eu_with_vu_in_lu( lu, vu );
	prev_eu = this_eu;
	do {
		prev_eu = RT_LIST_PLAST_CIRC( edgeuse, prev_eu );
		if( prev_eu == this_eu )  {
			if(rt_g.NMG_debug&DEBUG_COMBINE)
				rt_log("nmg_vu_angle_measure: prev eu is this eu, ang=0\n");
			return 0;	/* Unable to compute 'vec' */
		}
		/* Skip any edges that stay on this vertex */
	} while( prev_eu->vu_p->v_p == this_eu->vu_p->v_p );

	/* Get vector which represents the inbound edge */
	VSUB2( vec, prev_eu->vu_p->v_p->vg_p->coord, vu->v_p->vg_p->coord );

	ang = rt_angle_measure( vec, x_dir, y_dir );
	if(rt_g.NMG_debug&DEBUG_COMBINE)
		rt_log("nmg_vu_angle_measure:  measured angle=%e\n", ang*rt_radtodeg);

	/*
	 *  Since the entry edge is not on the ray, ensure the
	 *  angles are not exactly 0 or pi.
	 */
#define RADIAN_TWEEK	1.0e-14	/* low bits of double prec., re: 6.28... */
	if( ang == 0 )  {
		if( entry_ass == NMG_E_ASSESSMENT_RIGHT )  {
			ang = RADIAN_TWEEK;
		} else {
			/* Assuming NMG_E_ASSESSMENT_LEFT */
			ang = rt_twopi - RADIAN_TWEEK;
		}
	} else if( ang == rt_pi )  {
		if( entry_ass == NMG_E_ASSESSMENT_RIGHT )  {
			ang = rt_pi - RADIAN_TWEEK;
		} else {
			ang = rt_pi + RADIAN_TWEEK;
		}
	}

	/*
	 *  Also, ensure computed angle and topological assessment agree
	 *  about which side of the ray this edge is on.
	 */
	if( ang > rt_pi )  {
		if( entry_ass != NMG_E_ASSESSMENT_LEFT )  {
			rt_log("*** ERROR topology/geometry conflict, ang=%e, ass=%s\n",
				ang*rt_radtodeg,
				nmg_e_assessment_names[entry_ass] );
		}
	} else if( ang < rt_pi )  {
		if( entry_ass != NMG_E_ASSESSMENT_RIGHT )  {
			rt_log("*** ERROR topology/geometry conflict, ang=%e, ass=%s\n",
				ang*rt_radtodeg,
				nmg_e_assessment_names[entry_ass] );
		}
	}
	if(rt_g.NMG_debug&DEBUG_COMBINE)
		rt_log("ang=%g (%e), vec=(%g,%g,%g)\n", ang*rt_radtodeg, ang*rt_radtodeg, V3ARGS(vec) );
	return ang;
}

/*
 *			N M G _ A S S E S S _ E U
 *
 *  The current vertex (eu->vu_p) is on the line of intersection.
 *  Assess the indicated edge, to see if it lies on the line of
 *  intersection, or departs towards the left or right.
 *
 *  There is no need to look more than one edge forward or backward.
 *  Even if there are edges which loop around to the same vertex
 *  (with a different vertexuse), that (0-length) edge is ON the ray.
 */
int
nmg_assess_eu( eu, forw, rs, pos )
struct edgeuse		*eu;
int			forw;
struct nmg_ray_state	*rs;
int			pos;
{
	struct vertex		*v;
	struct vertex		*otherv = (struct vertex *)0;
	struct edgeuse		*othereu;
	vect_t			heading;
	int			ret;
	register int		i;

	v = eu->vu_p->v_p;
	NMG_CK_VERTEX(v);
	othereu = eu;
	if( forw )  {
		othereu = RT_LIST_PNEXT_CIRC( edgeuse, othereu );
	} else {
		othereu = RT_LIST_PLAST_CIRC( edgeuse, othereu );
	}
	if( othereu == eu )  {
		/* Back to where search started */
		rt_bomb("nmg_assess_eu() no edges leave the vertex!\n");
	}
	otherv = othereu->vu_p->v_p;
	if( otherv == v )  {
		/* Edge stays on this vertex -- can't tell if forw or rev! */
		rt_bomb("nmg_assess_eu() edge runs from&to same vertex!\n");
	}

	/*  If the other vertex is mentioned anywhere on the ray's vu list,
	 *  then the edge is "on" the ray.
	 *  Match against vertex (rather than vertexuse) because cut/join
	 *  operations may have changed the particular vertexuse pointer.
	 */
	for( i=rs->nvu-1; i >= 0; i-- )  {
		if( rs->vu[i]->v_p != otherv )  continue;
		/* Edge is on the ray.  Which way does it go? */
/* XXX How to detect leaving the current vertex groups? */
		if(rt_g.NMG_debug&DEBUG_COMBINE)
			rt_log("ON: vu[%d]=x%x otherv=x%x, i=%d\n",
				pos, rs->vu[pos], otherv, i );

		/* Compute edge vector, for purposes of orienting answer */
		if( forw )  {
			/* Edge goes from v to otherv */
			VSUB2( heading, otherv->vg_p->coord, v->vg_p->coord );
		} else {
			/* Edge goes from otherv to v */
			VSUB2( heading, v->vg_p->coord, otherv->vg_p->coord );
		}
		if( MAGSQ(heading) < SMALL_FASTF )  rt_bomb("null heading\n");
		if( VDOT( heading, rs->dir ) < 0 )  {
			ret = NMG_E_ASSESSMENT_ON_REV;
		} else {
			ret = NMG_E_ASSESSMENT_ON_FORW;
		}
		goto out;
	}

	/*
	 *  Since other vertex does not lie anywhere on line of intersection,
	 *  the edge must lie to one side or the other of the ray.
	 *  Check vector from v to otherv against "left" vector.
	 */
	VSUB2( heading, otherv->vg_p->coord, v->vg_p->coord );
	if( MAGSQ(heading) < SMALL_FASTF )  rt_bomb("null heading 2\n");
	if( VDOT( heading, rs->left ) < 0 )  {
		ret = NMG_E_ASSESSMENT_RIGHT;
	} else {
		ret = NMG_E_ASSESSMENT_LEFT;
	}
out:
	if(rt_g.NMG_debug&DEBUG_COMBINE)  {
		rt_log("nmg_assess_eu(x%x, fw=%d, pos=%d) v=x%x otherv=x%x: %s\n",
			eu, forw, pos, v, otherv,
			nmg_e_assessment_names[ret] );
		rt_log(" v(%g,%g,%g) other(%g,%g,%g)\n",
			V3ARGS(v->vg_p->coord), V3ARGS(otherv->vg_p->coord) );
	}
	return ret;
}

/*
 *			N M G _ A S S E S S _ V U
 */
int
nmg_assess_vu( rs, pos )
struct nmg_ray_state	*rs;
int			pos;
{
	struct vertexuse	*vu;
	struct loopuse	*lu;
	struct edgeuse	*this_eu;
	int		next_ass;
	int		prev_ass;
	int		ass;

	vu = rs->vu[pos];
	NMG_CK_VERTEXUSE( vu );
	if( *vu->up.magic_p == NMG_LOOPUSE_MAGIC )  {
		return NMG_V_ASSESSMENT_LONE;
	}
	if( (lu = nmg_lu_of_vu(vu)) == (struct loopuse *)0 )
		rt_bomb("nmg_assess_vu: no lu\n");
	this_eu = nmg_eu_with_vu_in_lu( lu, vu );
	prev_ass = nmg_assess_eu( this_eu, 0, rs, pos );
	next_ass = nmg_assess_eu( this_eu, 1, rs, pos );
	ass = NMG_V_ASSESSMENT_COMBINE( prev_ass, next_ass );
	if(rt_g.NMG_debug&DEBUG_COMBINE)  {
		rt_log("nmg_assess_vu() vu[%d]=x%x, v=x%x: %s\n",
			pos, vu, vu->v_p, nmg_v_assessment_names[ass] );
	}
	return ass;
}

struct nmg_vu_stuff {
	struct vertexuse	*vu;
	int			loop_index;
	struct nmg_loop_stuff	*lsp;
	fastf_t			vu_angle;
	fastf_t			min_vu_dot;
	int			seq;		/* seq # after lsp->min_vu */
};
struct nmg_loop_stuff {
	struct loopuse		*lu;
	fastf_t			min_dot;
	struct vertexuse	*min_vu;
	int			n_vu_in_loop;	/* # ray vu's in this loop */
};

/*
 *			N M G _ F A C E _ V U _ C O M P A R E
 *
 *  Support routine for nmg_face_coincident_vu_sort(), via qsort().
 *
 */
static int
nmg_face_vu_compare( a, b )
CONST genptr_t	a;
CONST genptr_t	b;
{
	register CONST struct nmg_vu_stuff *va = (CONST struct nmg_vu_stuff *)a;
	register CONST struct nmg_vu_stuff *vb = (CONST struct nmg_vu_stuff *)b;
	register double	diff;

	if( va->loop_index == vb->loop_index )  {
		/* Within a loop, sort by vu sequence number */
		if( va->seq < vb->seq )  return -1;
		if( va->seq == vb->seq )  return 0;
		return 1;
	}
#if 0
	/* Between two loops each with a single vertex, use min angle */
	/* This works, but is the "old way".  Should use dot products. */
	if( va->lsp->n_vu_in_loop <= 1 && vb->lsp->n_vu_in_loop <= 1 )  {
		diff = va->vu_angle - vb->vu_angle;
		if( diff < 0 )  return -1;
		if( diff == 0 )  {
			/* Gak, this really means trouble! */
			rt_log("nmg_face_vu_compare(): two loops (single vertex) have same vu_angle%g?\n",
				va->vu_angle);
			return 0;
		}
		return 1;
	}
#endif

	/* Between loops, sort by minimum dot product of the loops */
	diff = va->lsp->min_dot - vb->lsp->min_dot;
	if( NEAR_ZERO( diff, RT_DOT_TOL) )  {
		/*
		 *  The dot product is the same, so loop edges are parallel.
		 *  Take minimum CCW angle first.
		 */
		diff = va->vu_angle - vb->vu_angle;
		if( diff < 0 )  return -1;
		if( diff == 0 )  {
			/* Gak, this really means trouble! */
			rt_log("nmg_face_vu_compare(): two loops have same min_dot %g, vu_angle%g?\n",
				va->lsp->min_dot, va->vu_angle);
			return 0;
		}
		return 1;
	}
	if( diff < 0 )  return -1;
	return 1;
}

/*
 *			N M G _ F A C E _ V U _ D O T
 *
 *  For the purpose of computing the dot product of the edges around
 *  this vertexuse and the ray direction vector, the edge vectors should
 *  both be pointing inwards to the vertex,
 *  rather than in the edge direction, so that it is possible to sort
 *  the vertexuse's into sequence by "angle" along the ray direction,
 *  starting with the vertexuse that the ray first encounters.
 */
static void
nmg_face_vu_dot( vsp, lu, rs, ass )
struct nmg_vu_stuff		*vsp;
struct loopuse			*lu;
CONST struct nmg_ray_state	*rs;
int				ass;
{
	struct edgeuse	*this_eu;
	struct edgeuse	*othereu;
	vect_t		vec;
	fastf_t		dot;
	struct vertexuse	*vu;
	int		this;

	vu = vsp->vu;
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_LOOPUSE(lu);
	this_eu = nmg_eu_with_vu_in_lu( lu, vu );

	/* First, consider the edge inbound into this vertex */
	this = NMG_V_ASSESSMENT_PREV(ass);
	if( this == NMG_E_ASSESSMENT_ON_REV )  {
		vsp->min_vu_dot = -1;		/* straight back */
	} else if( this == NMG_E_ASSESSMENT_ON_FORW )  {
		vsp->min_vu_dot = 1;		/* straight forw */
	} else {
		othereu = RT_LIST_PLAST_CIRC( edgeuse, this_eu );
		if( vu->v_p != othereu->vu_p->v_p )  {
			/* Vector from othereu to this_eu */
			VSUB2( vec, vu->v_p->vg_p->coord,
				othereu->vu_p->v_p->vg_p->coord );
			VUNITIZE(vec);
			vsp->min_vu_dot = VDOT( vec, rs->dir );
		} else {
			vsp->min_vu_dot = 99;		/* larger than +1 */
		}
	}

	/* Second, consider the edge outbound from this vertex (forw) */
	this = NMG_V_ASSESSMENT_NEXT(ass);
	if( this == NMG_E_ASSESSMENT_ON_REV )  {
		dot = -1;		/* straight back */
		if( dot < vsp->min_vu_dot )  vsp->min_vu_dot = dot;
	} else if( this == NMG_E_ASSESSMENT_ON_FORW )  {
		dot = 1;		/* straight forw */
		if( dot < vsp->min_vu_dot )  vsp->min_vu_dot = dot;
	} else {
		othereu = RT_LIST_PNEXT_CIRC( edgeuse, this_eu );
		if( vu->v_p != othereu->vu_p->v_p )  {
			/* Vector from othereu to this_eu */
			VSUB2( vec, vu->v_p->vg_p->coord,
				othereu->vu_p->v_p->vg_p->coord );
			VUNITIZE(vec);
			dot = VDOT( vec, rs->dir );
			if( dot < vsp->min_vu_dot )  {
				vsp->min_vu_dot = dot;
			}
		}
	}
}

/*
 *			N M G _ F A C E _ C O I N C I D E N T _ V U _ S O R T
 *
 *  Given co-incident vertexuses (same distance along the ray),
 *  sort them into the "proper" order for driving the state machine.
 */
int
nmg_face_coincident_vu_sort( rs, start, end )
struct nmg_ray_state	*rs;
int			start;		/* first index */
int			end;		/* last index + 1 */
{
	int		num;
	struct nmg_vu_stuff	*vs;
	struct nmg_loop_stuff *ls;
	int		nloop;
	int		nvu;
	int		i;
	struct loopuse	*lu;
	int		ass;
	int		l;

	if(rt_g.NMG_debug&DEBUG_COMBINE)
		rt_log("nmg_face_coincident_vu_sort(, %d, %d)\n", start, end);
	num = end - start;
	vs = (struct nmg_vu_stuff *)rt_malloc( sizeof(struct nmg_vu_stuff)*num,
		"nmg_vu_stuff" );
	ls = (struct nmg_loop_stuff *)rt_malloc( sizeof(struct nmg_loop_stuff)*num,
		"nmg_loop_stuff" );

	/* Assess each vu, create list of loopuses, find max angles */
	nloop = 0;
	nvu = 0;
	for( i = end-1; i >= start; i-- )  {
		lu = nmg_lu_of_vu( rs->vu[i] );
		ass = nmg_assess_vu( rs, i );
		if(rt_g.NMG_debug&DEBUG_COMBINE)
		   rt_log("vu[%d]=x%x v=x%x assessment=%s\n",
			i, rs->vu[i], rs->vu[i]->v_p, nmg_v_assessment_names[ass] );
		/*  Ignore lone vertices, unless that is all that there is,
		 *  in which case, let just one through.  (return 'start+1');
		 */
		if( *(rs->vu[i]->up.magic_p) == NMG_LOOPUSE_MAGIC )  {
			if( i <= start && nvu == 0 )  {
				rt_free( (char *)vs, "nmg_vu_stuff");
				rt_free( (char *)ls, "nmg_loop_stuff");
				return start+1;
			}
			/* Drop this loop of a single vertex in sanitize() */
			lu->orientation =
			  lu->lumate_p->orientation = OT_BOOLPLACE;
			continue;
		}
		vs[nvu].vu = rs->vu[i];
		vs[nvu].seq = -1;		/* Not assigned yet */

		/* x_dir is -dir, y_dir is -left */
		vs[nvu].vu_angle = nmg_vu_angle_measure( rs->vu[i],
			rs->ang_x_dir, rs->ang_y_dir, ass ) * rt_radtodeg;

		/* Check entering and departing edgeuse angle w.r.t. ray */
		/* This is already done once in nmg_assess_vu();  reuse? */
		/* Computes vs[nvu].min_vu_dot */
		nmg_face_vu_dot( &vs[nvu], lu, rs, ass );

		/* Search for loopuse table entry */
		for( l = 0; l < nloop; l++ )  {
			if( ls[l].lu == lu )  goto got_loop;
		}
		/* didn't find loopuse in table, add to table */
		l = nloop++;
		ls[l].lu = lu;
		ls[l].n_vu_in_loop = 0;
		ls[l].min_dot = 99;		/* > +1 */
got_loop:
		ls[l].n_vu_in_loop++;
		vs[nvu].loop_index = l;
		vs[nvu].lsp = &ls[l];
		if( vs[nvu].min_vu_dot < ls[l].min_dot )  {
			ls[l].min_dot = vs[nvu].min_vu_dot;
			ls[l].min_vu = vs[nvu].vu;
		}
		nvu++;
	}

	/*
	 *  For each loop which has more than one vertexuse present on the
	 *  ray, start at the vu which has the smallest angle off the ray,
	 *  and walk the edges of the loop, marking off the vu sequence for
	 *  those vu's on the ray (those vu's found in vs[].vu).
	 */
	for( l=0; l < nloop; l++ )  {
		register struct edgeuse	*eu;
		struct edgeuse	*first_eu;
		int		seq = 0;

		if( ls[l].n_vu_in_loop <= 1 )  continue;

		first_eu = nmg_eu_with_vu_in_lu( ls[l].lu, ls[l].min_vu );
		for( eu = RT_LIST_PNEXT_CIRC(edgeuse,first_eu);
		     eu != first_eu;
		     eu = RT_LIST_PNEXT_CIRC(edgeuse,eu)
		)  {
			register struct vertexuse *vu = eu->vu_p;
			NMG_CK_VERTEXUSE(vu);
			for( i=0; i < nvu; i++ )  {
				if( vs[i].vu != vu )  continue;
				vs[i].seq = seq++;
				break;
			}
		}
	}

#if 0
/**
	if(rt_g.NMG_debug&DEBUG_COMBINE)
**/
	{
		rt_log("Loop table (before sort):\n");
		for( l=0; l < nloop; l++ )  {
			rt_log("  index=%d, lu=x%x, min_dot=%g, #vu=%d\n",
				l, ls[l].lu, ls[l].min_dot,
				ls[l].n_vu_in_loop );
		}
		rt_log("Vertexuse table:\n");
		for( i=0; i < nvu; i++ )  {
			rt_log("  vu=x%x, loop_index=%d, vu_angle=%g, min_vu_dot=%g, seq=%d\n",
				vs[i].vu, vs[i].loop_index,
				vs[i].vu_angle, vs[i].min_vu_dot, vs[i].seq );
		}
	}
#endif

	/* Sort the vertexuse table into appropriate order */
#if defined(__convexc__)
	qsort( (genptr_t)vs, nvu, sizeof(*vs),
		(int (*)())nmg_face_vu_compare);
#else
	qsort( (genptr_t)vs, nvu, sizeof(*vs), nmg_face_vu_compare );
#endif
	if(rt_g.NMG_debug&DEBUG_COMBINE)
	{
		rt_log("Vertexuse table (after sort):\n");
		for( i=0; i < nvu; i++ )  {
			rt_log("  vu=x%x, loop_index=%d, vu_angle=%g, min_vu_dot=%g, seq=%d\n",
				vs[i].vu, vs[i].loop_index,
				vs[i].vu_angle, vs[i].min_vu_dot, vs[i].seq );
		}
	}

	/* Copy new vu's back to main array */
	for( i=0; i < nvu; i++ )  {
		rs->vu[start+i] = vs[i].vu;
		if(rt_g.NMG_debug&DEBUG_COMBINE)
			rt_log(" vu[%d]=x%x, v=x%x\n",
				start+i, rs->vu[start+i], rs->vu[start+i]->v_p );
	}

	rt_free( (char *)vs, "nmg_vu_stuff");
	rt_free( (char *)ls, "nmg_loop_stuff");
	return start+nvu;
}

/*
 *			N M G _ F A C E _ C O M B I N E
 *
 *	collapse loops,vertices within face fu1 (relative to fu2)
 *
 */
HIDDEN void
nmg_face_combineX(b, fu1, fu2, pt, dir, mag)
struct nmg_ptbl	*b;		/* table of vertexuses in fu1 on intercept line */
struct faceuse	*fu1;		/* face being worked */
struct faceuse	*fu2;		/* for plane equation */
point_t		pt;
vect_t		dir;
fastf_t		*mag;
{
	struct vertexuse	**vu;
	register int	i;
	register int	j;
	int		k;
	int		m;
	struct nmg_ray_state	rs;

	if(rt_g.NMG_debug&DEBUG_COMBINE)  {
		rt_log("\nnmg_face_combine(fu1=x%x, fu2=x%x)\n", fu1, fu2);
		nmg_pr_fu_briefly(fu1,(char *)0);
	}

	/*
	 *  Set up nmg_ray_state structure.
	 *  "left" is a vector that lies in the plane of the face
	 *  which contains the loops being operated on.
	 *  It points in the direction "left" of the ray.
	 */
	vu = (struct vertexuse **)b->buffer;
	rs.vu = vu;
	rs.nvu = b->end;
	rs.eg_p = (struct edge_g *)NULL;
	rs.sA = fu1->s_p;
	rs.sB = fu2->s_p;
	VMOVE( rs.pt, pt );
	VMOVE( rs.dir, dir );
	VCROSS( rs.left, fu1->f_p->fg_p->N, dir );
	switch( fu1->orientation )  {
	case OT_SAME:
		break;
	case OT_OPPOSITE:
		VREVERSE(rs.left, rs.left);
		break;
	default:
		rt_bomb("nmg_face_combine: bad orientation\n");
	}
	if(rt_g.NMG_debug&DEBUG_COMBINE)  {
		rt_log("\tfu->orientation=%s\n", nmg_orientation(fu1->orientation) );
		HPRINT("\tfg N", fu1->f_p->fg_p->N);
		VPRINT("\t  pt", pt);
		VPRINT("\t dir", dir);
		VPRINT("\tleft", rs.left);
	}
	rs.state = NMG_STATE_OUT;

	/* For measuring angle CCW around plane from -dir */
	VREVERSE( rs.ang_x_dir, dir );
	VREVERSE( rs.ang_y_dir, rs.left );

	nmg_face_plot( fu1 );

	/*
	 *  Find the extent of the vertexuses at this distance.
	 *  ptbl_vsort() will have forced all the distances to be
	 *  exactly equal if they are within tolerance of each other.
	 *
	 *  Two cases:  lone vertexuse, and range of vertexuses.
	 */
	for( i = 0; i < b->end; i = j )  {
		if( i == b->end-1 || mag[i+1] != mag[i] )  {
			/* Single vertexuse at this dist */
			if(rt_g.NMG_debug&DEBUG_COMBINE)
				rt_log("single vertexuse at index %d\n", i);
			nmg_face_state_transition( vu[i], &rs, i, 0 );
			nmg_face_plot( fu1 );
			j = i+1;
		} else {
			/* Find range of vertexuses at this distance */
			struct vertex	*v;

			for( j = i+1; j < b->end; j++ )  {
				if( mag[j] != mag[i] )  break;
			}
			/* vu Interval runs from [i] to [j-1] inclusive */
			if(rt_g.NMG_debug&DEBUG_COMBINE)
				rt_log("vu's on list interval [%d] to [%d] equal\n", i, j-1 );
			/* Ensure that all vu's point to same vertex */
			v = vu[i]->v_p;
			for( k = i+1; k < j; k++ )  {
				if( vu[k]->v_p != v )  rt_bomb("nmg_face_combine: vu block with differing vertices\n");
			}
			/* All vu's point to the same vertex, sort them */
			m = nmg_face_coincident_vu_sort( &rs, i, j );
			/* Process vu list, up to cutoff index 'm' */
			for( k = i; k < m; k++ )  {
				nmg_face_state_transition( vu[k], &rs, k, 1 );
				nmg_face_plot( fu1 );
			}
			vu[j-1] = vu[m-1]; /* for next iteration's lookback */
			if(rt_g.NMG_debug&DEBUG_COMBINE)
				rt_log("vu[%d] set to x%x\n", j-1, vu[j-1] );
		}
	}

	if( rs.state != NMG_STATE_OUT )  {
		rt_log("ERROR nmg_face_combine() ended in state '%s'?\n",
			nmg_state_names[rs.state] );

		/* Drop a plot file */
		rt_g.NMG_debug |= DEBUG_COMBINE|DEBUG_PLOTEM;
		nmg_pl_comb_fu( 0, 1, fu1 );
		nmg_pl_comb_fu( 0, 2, fu2 );

/*		rt_bomb("nmg_face_combine() bad ending state\n"); */
	}
}

/*
 *			N M G _ F A C E _ C U T J O I N
 *
 *  The main face cut handler.
 *  Called from nmg_inter.c by nmg_isect_2faces().
 *
 *  A wrapper for nmg_face_combine, for now.
 *
 *  The two vertexuse lists may be of different lengths, because
 *  one may have multiple uses of a vertex, while the other has only
 *  a single use of that same vertex.
 */
void
nmg_face_cutjoin(b1, b2, fu1, fu2, pt, dir, tol)
struct nmg_ptbl	*b1;		/* table of vertexuses in fu1 on intercept line */
struct nmg_ptbl	*b2;		/* table of vertexuses in fu2 on intercept line */
struct faceuse	*fu1;		/* face being worked */
struct faceuse	*fu2;		/* for plane equation */
point_t		pt;
vect_t		dir;
CONST struct rt_tol	*tol;
{
	fastf_t		*mag1;
	fastf_t		*mag2;
	fastf_t		dist_tol = 0.005;	/* XXX */
	struct vertexuse **vu1, **vu2;
	int		i;

	if(rt_g.NMG_debug&DEBUG_COMBINE)  {
		rt_log("\nnmg_face_cutjoin(fu1=x%x, fu2=x%x)\n", fu1, fu2);
	}

	mag1 = (fastf_t *)rt_calloc(b1->end, sizeof(fastf_t),
		"vector magnitudes along ray, for sort");
	mag2 = (fastf_t *)rt_calloc(b2->end, sizeof(fastf_t),
		"vector magnitudes along ray, for sort");

	/*
	 *  Sort hit points by increasing distance, vertex ptr, vu ptr,
	 *  and eliminate any duplicate vu's.
	 */
	ptbl_vsort(b1, fu1, fu2, pt, dir, mag1, dist_tol);
	ptbl_vsort(b2, fu2, fu1, pt, dir, mag2, dist_tol);

	vu1 = (struct vertexuse **)b1->buffer;
	vu2 = (struct vertexuse **)b2->buffer;

	/* Print list of intersections */
	if(rt_g.NMG_debug&DEBUG_COMBINE)  {
		rt_log("Ray vu intersection list:\n");
		for( i=0; i < b1->end; i++ )  {
			rt_log(" %d %e ", i, mag1[i] );
			nmg_pr_vu_briefly( vu1[i], (char *)0 );
		}
		for( i=0; i < b2->end; i++ )  {
			rt_log(" %d %e ", i, mag2[i] );
			nmg_pr_vu_briefly( vu2[i], (char *)0 );
		}
	}

#if 0
	/* Check to see if lists are different */
	{
		i = b1->end-1;
		if( b2->end-1 < i )  i = b2->end-1;
		for( ; i >= 0; i-- )  {
			NMG_CK_VERTEXUSE(vu1[i]);
			NMG_CK_VERTEXUSE(vu2[i]);
			if( vu1[i]->v_p == vu2[i]->v_p ) continue;
			rt_log("Index %d mis-match, x%x != x%x\n",
				i, vu1[i]->v_p, vu2[i]->v_p );
		}
	}
#endif

	nmg_face_combineX( b1, fu1, fu2, pt, dir, mag1 );
	nmg_face_combineX( b2, fu2, fu1, pt, dir, mag2 );

	rt_free((char *)mag1, "vector magnitudes");
	rt_free((char *)mag2, "vector magnitudes");
}

/*
 *			N M G _ E D G E _ G E O M _ I S E C T _ L I N E
 *
 *  Force the edge geometry structure for a given edge to be that of
 *  the intersection line between the two faces.
 *
 *  XXX What about orientation?  Which way should direction vector point?
 *  XXX What about edgeuse orientation flags?
 */
void
nmg_edge_geom_isect_line( e, rs )
struct edge		*e;
struct nmg_ray_state	*rs;
{
	register struct edge_g	*eg;

	NMG_CK_EDGE(e);
	if( !e->eg_p )  {
		/* No edge geometry so far */
		if( !rs->eg_p )  {
			nmg_edge_g( e );
			eg = e->eg_p;
			NMG_CK_EDGE_G(eg);
			VMOVE( eg->e_pt, rs->pt );
			VMOVE( eg->e_dir, rs->dir );
			rs->eg_p = eg;
		} else {
			nmg_use_edge_g( e, rs->eg_p );
		}
		return;
	}
	/* Edge has edge geometry */
	if( e->eg_p == rs->eg_p )  return;
	if( !rs->eg_p )  {
		/* Smash edge geom with isect line geom, and remember it */
		eg = e->eg_p;
		NMG_CK_EDGE_G(eg);
		VMOVE( eg->e_pt, rs->pt );
		VMOVE( eg->e_dir, rs->dir );
		rs->eg_p = eg;
		return;
	}
	/*
	 * Edge has an edge geometry struct, different from that of isect line.
	 * Force all uses of this edge geom to take on isect line's geometry.
	 * Everywhere e->eg_p is seen, replace with rs->eg_p.
	 */
	nmg_move_eg( e->eg_p, rs->eg_p, rs->sA );
	nmg_move_eg( e->eg_p, rs->eg_p, rs->sB );
}

/*
 *  State machine transition tables
 *  Indexed by MNG_V_ASSESSMENT values.
 */

#define NMG_ACTION_ERROR		0
#define NMG_ACTION_NONE			1
#define NMG_ACTION_VFY_EXT		2
#define NMG_ACTION_VFY_MULTI		3
#define NMG_ACTION_LONE_V_ESPLIT	4
#define NMG_ACTION_LONE_V_JAUNT		5
#define NMG_ACTION_CUTJOIN		6
static char *action_names[] = {
	"*ERROR*",
	"-none-",
	"VFY_EXT",
	"VFY_MULTI",
	"ESPLIT",
	"JAUNT",
	"CUTJOIN",
	"*TOOBIG*"
};

struct state_transitions {
	int	new_state;
	int	action;
};

static CONST struct state_transitions nmg_state_is_out[17] = {
	{ /* LEFT,LEFT */	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ /* LEFT,RIGHT */	NMG_STATE_IN,		NMG_ACTION_VFY_EXT },
	{ /* LEFT,ON_FORW */	NMG_STATE_ON_L,		NMG_ACTION_VFY_EXT },
	{ /* LEFT,ON_REV */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,RIGHT */	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ /* RIGHT,ON_FORW */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,ON_REV */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_FORW,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_FORW,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_FORW,ON_FORW */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_FORW,ON_REV */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_REV,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_REV,RIGHT */	NMG_STATE_ON_R,		NMG_ACTION_VFY_EXT },
	{ /* ON_REV,ON_FORW */	NMG_STATE_IN,		NMG_ACTION_VFY_EXT },
	{ /* ON_REV,ON_REV */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LONE */		NMG_STATE_OUT,		NMG_ACTION_NONE }
};

static CONST struct state_transitions nmg_state_is_on_L[17] = {
	{ /* LEFT,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,ON_FORW */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,ON_REV */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,RIGHT */	NMG_STATE_ON_L,		NMG_ACTION_NONE },
	{ /* RIGHT,ON_FORW */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,ON_REV */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_FORW,LEFT */	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ /* ON_FORW,RIGHT */	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ /* ON_FORW,ON_FORW */	NMG_STATE_ON_L,		NMG_ACTION_NONE },
	{ /* ON_FORW,ON_REV */	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ /* ON_REV,LEFT */	NMG_STATE_IN,		NMG_ACTION_VFY_MULTI },
	{ /* ON_REV,RIGHT */	NMG_STATE_ON_B,		NMG_ACTION_VFY_EXT },
	{ /* ON_REV,ON_FORW */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_REV,ON_REV */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LONE */		NMG_STATE_ON_L,		NMG_ACTION_LONE_V_ESPLIT }
};

static CONST struct state_transitions nmg_state_is_on_R[17] = {
	{ /* LEFT,LEFT */	NMG_STATE_ON_R,		NMG_ACTION_NONE },
	{ /* LEFT,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,ON_FORW */	NMG_STATE_ON_B,		NMG_ACTION_NONE },
	{ /* LEFT,ON_REV */	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ /* RIGHT,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,ON_FORW */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,ON_REV */	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ /* ON_FORW,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_FORW,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_FORW,ON_FORW */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_FORW,ON_REV */	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ /* ON_REV,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_REV,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_REV,ON_FORW */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_REV,ON_REV */	NMG_STATE_ON_R,		NMG_ACTION_NONE },
	{ /* LONE */		NMG_STATE_ON_R,		NMG_ACTION_LONE_V_ESPLIT }
};
static CONST struct state_transitions nmg_state_is_on_B[17] = {
	{ /* LEFT,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,ON_FORW */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,ON_REV */	NMG_STATE_IN,		NMG_ACTION_VFY_MULTI },
	{ /* RIGHT,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,ON_FORW */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,ON_REV */	NMG_STATE_ON_L,		NMG_ACTION_NONE },
	{ /* ON_FORW,LEFT */	NMG_STATE_ON_R,		NMG_ACTION_NONE },
	{ /* ON_FORW,RIGHT */	NMG_STATE_IN,		NMG_ACTION_VFY_MULTI },
	{ /* ON_FORW,ON_FORW */	NMG_STATE_ON_B,		NMG_ACTION_VFY_MULTI },
	{ /* ON_FORW,ON_REV */	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ /* ON_REV,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_REV,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_REV,ON_FORW */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_REV,ON_REV */	NMG_STATE_ON_B,		NMG_ACTION_VFY_MULTI },
	{ /* LONE */		NMG_STATE_ON_B,		NMG_ACTION_LONE_V_ESPLIT }
};

static CONST struct state_transitions nmg_state_is_in[17] = {
	{ /* LEFT,LEFT */	NMG_STATE_IN,		NMG_ACTION_CUTJOIN },
	{ /* LEFT,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,ON_FORW */	NMG_STATE_ON_R,		NMG_ACTION_CUTJOIN },
	{ /* LEFT,ON_REV */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,LEFT */	NMG_STATE_OUT,		NMG_ACTION_CUTJOIN },
	{ /* RIGHT,RIGHT */	NMG_STATE_IN,		NMG_ACTION_CUTJOIN },
	{ /* RIGHT,ON_FORW */	NMG_STATE_ON_L,		NMG_ACTION_CUTJOIN },
	{ /* RIGHT,ON_REV */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_FORW,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_FORW,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_FORW,ON_FORW */	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ /* ON_FORW,ON_REV */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_REV,LEFT */	NMG_STATE_ON_R,		NMG_ACTION_CUTJOIN },
	{ /* ON_REV,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON_REV,ON_FORW */	NMG_STATE_ON_B,		NMG_ACTION_CUTJOIN },
	{ /* ON_REV,ON_REV */	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ /* LONE */		NMG_STATE_IN,		NMG_ACTION_LONE_V_JAUNT }
};

/*
 *			N M G _ F A C E _ S T A T E _ T R A N S I T I O N
 */
int
nmg_face_state_transition( vu, rs, pos, multi )
struct vertexuse	*vu;
struct nmg_ray_state	*rs;
int			pos;
int			multi;
{
	int			assessment;
	int			old;
	CONST struct state_transitions	*stp;
	struct vertexuse	*prev_vu;
	struct edgeuse		*eu;
	struct loopuse		*lu;
	struct loopuse		*prev_lu;
	struct edgeuse	*first_new_eu;
	struct edgeuse	*second_new_eu;
	int			e_assessment;

	NMG_CK_VERTEXUSE(vu);
	assessment = nmg_assess_vu( rs, pos );
	old = rs->state;
	switch( old )  {
	default:
	case NMG_STATE_ERROR:
		rt_bomb("nmg_face_state_transition: was in ERROR state\n");
	case NMG_STATE_OUT:
		stp = &nmg_state_is_out[assessment];
		break;
	case NMG_STATE_ON_L:
		stp = &nmg_state_is_on_L[assessment];
		break;
	case NMG_STATE_ON_R:
		stp = &nmg_state_is_on_R[assessment];
		break;
	case NMG_STATE_ON_B:
		stp = &nmg_state_is_on_B[assessment];
		break;
	case NMG_STATE_IN:
		stp = &nmg_state_is_in[assessment];
		break;
	}
	if(rt_g.NMG_debug&DEBUG_COMBINE)  {
		rt_log("nmg_face_state_transition(vu x%x, pos=%d)\n\told=%s, assessed=%s, new=%s, action=%s\n",
			vu, pos,
			nmg_state_names[old], nmg_v_assessment_names[assessment],
			nmg_state_names[stp->new_state], action_names[stp->action] );
		rt_log("This loopuse, before action:\n");
		nmg_face_lu_plot(nmg_lu_of_vu(vu), rs);
	}

	/*
	 *  Force edge geometry that lies on the intersection line
	 *  to use the edge_g structure of the intersection line (ray).
	 */
	if( NMG_V_ASSESSMENT_PREV(assessment) == NMG_E_ASSESSMENT_ON_REV )  {
		eu = nmg_eu_with_vu_in_lu( nmg_lu_of_vu(vu), vu );
		eu = RT_LIST_PLAST_CIRC( edgeuse, eu );
		NMG_CK_EDGEUSE(eu);
		if( rs->eg_p && eu->e_p->eg_p != rs->eg_p )  {
rt_log("force prev eu to ray\n");
			nmg_edge_geom_isect_line( eu->e_p, rs );
		}
	}
	if( NMG_V_ASSESSMENT_NEXT(assessment) == NMG_E_ASSESSMENT_ON_FORW )  {
		eu = nmg_eu_with_vu_in_lu( nmg_lu_of_vu(vu), vu );
		NMG_CK_EDGEUSE(eu);
		if( rs->eg_p && eu->e_p->eg_p != rs->eg_p )  {
rt_log("force next eu to ray\n");
			nmg_edge_geom_isect_line( eu->e_p, rs );
		}
	}

	switch( stp->action )  {
	default:
	case NMG_ACTION_ERROR:
	bomb:
		rt_log("nmg_face_state_transition(vu x%x, pos=%d)\n\told=%s, assessed=%s, new=%s, action=%s\n",
			vu, pos,
			nmg_state_names[old], nmg_v_assessment_names[assessment],
			nmg_state_names[stp->new_state], action_names[stp->action] );
#if 0	/* XXX turn this on only for debugging */
		/* First, print this faceuse */
		lu = nmg_lu_of_vu( vu );
		/* Drop a plot file */
		rt_g.NMG_debug |= DEBUG_COMBINE|DEBUG_PLOTEM;
		nmg_pl_comb_fu( 0, 1, lu->up.fu_p );
		/* Print the faceuse for later analysis */
		rt_log("Loop with the offending vertex\n");
		nmg_pr_lu_briefly(lu, (char *)0);
		rt_log("The whole face\n");
		nmg_pr_fu(lu->up.fu_p, (char *)0);
		nmg_face_lu_plot(lu, rs);
		{
			FILE	*fp = fopen("error.pl", "w");
			nmg_pl_m(fp, nmg_find_model((long *)lu));
			fclose(fp);
			rt_log("wrote error.pl\n");
		}
		/* Store this face in a .g file for examination! */
		{
			FILE	*fp = fopen("error.g", "w");
			struct rt_external	ext;
			struct rt_db_internal	intern;
			static union record		rec;

			RT_INIT_DB_INTERNAL(&intern);
			intern.idb_type = ID_NMG;
			intern.idb_ptr = (genptr_t)nmg_find_model((long*)lu);
			RT_INIT_EXTERNAL( &ext );

			/* Scale change on export is 1.0 -- no change */
			if( rt_functab[ID_NMG].ft_export( &ext, &intern, 1.0 ) < 0 )  {
				rt_log("solid export failure\n");
				if( intern.idb_ptr )  rt_functab[ID_NMG].ft_ifree( &intern );
				db_free_external( &ext );
				rt_bomb("zappo");
			}
			rt_functab[ID_NMG].ft_ifree( &intern );
			NAMEMOVE( "error", ((union record *)ext.ext_buf)->s.s_name );

			rec.u_id = ID_IDENT;
			strcpy( rec.i.i_version, ID_VERSION );
			strcpy( rec.i.i_title, "nmg_fcut.c error dump" );
			fwrite( (char *)&rec, sizeof(rec), 1, fp );
			fwrite( ext.ext_buf, ext.ext_nbytes, 1, fp );
			fclose(fp);
			rt_log("wrote error.g\n");
		}
#endif
		/* Explode */
		rt_bomb("nmg_face_state_transition: got action=ERROR\n");
	case NMG_ACTION_NONE:
		if( *(vu->up.magic_p) == NMG_LOOPUSE_MAGIC )  {
			lu = vu->up.lu_p;
			/* Drop this loop of a single vertex in sanitize() */
			if( lu->orientation == OT_UNSPEC );
				lu->orientation =
				  lu->lumate_p->orientation = OT_BOOLPLACE;
		}
		break;
	case NMG_ACTION_VFY_EXT:
		/* Verify loop containing this vertex has external orientation */
		lu = nmg_lu_of_vu( vu );
		switch( lu->orientation )  {
		case OT_SAME:
			break;
		default:
			rt_log("nmg_face_state_transition: VFY_EXT got orientation=%s\n",
				nmg_orientation(lu->orientation) );
			break;
		}
		break;
	case NMG_ACTION_VFY_MULTI:
		/*  Ensure that there are multiple vertexuse's at this
		 *  vertex along the ray.
		 *  If not, the table entry is illegal.
		 */
		if( multi )  break;
		rt_log("nmg_face_state_transition: VFY_MULTI had only 1 vertex\n");
		goto bomb;
	case NMG_ACTION_LONE_V_ESPLIT:
		/*
		 *  Split edge to include vertex from this lone vert loop.
		 *  This only happens in an "ON" state, so split the edge that
		 *  starts (or ends) with the previously seen vertex.
		 *  Note that the forward going edge may point the wrong way,
		 *  i.e., not lie on the ray at all.
		 */
		lu = nmg_lu_of_vu( vu );
		NMG_CK_LOOPUSE(lu);
		prev_vu = rs->vu[pos-1];
		NMG_CK_VERTEXUSE(prev_vu);
		eu = prev_vu->up.eu_p;
		NMG_CK_EDGEUSE(eu);
		e_assessment = nmg_assess_eu( eu, 1, rs, pos-1 );	/* forw */
		if( e_assessment == NMG_E_ASSESSMENT_ON_FORW )  {
			/* "eu" (forw) is the right edge to split */
		} else {
			e_assessment = nmg_assess_eu( eu, 0, rs, pos-1 ); /*rev*/
			if( e_assessment == NMG_E_ASSESSMENT_ON_REV )  {
				/* (reverse) "eu" is the right one */
				eu = RT_LIST_PLAST_CIRC( edgeuse, eu );
			} else {
				/* What went wrong? */
				rt_log("LONE_V_ESPLIT:  rev e_assessment = %s\n", nmg_e_assessment_names[e_assessment]);
				rt_bomb("nmg_face_state_transition: LONE_V_ESPLIT could not find ON edge to split\n");
			}
		}
		(void)nmg_ebreak( vu->v_p, eu->e_p );
		/* Update vu table with new value */
		rs->vu[pos] = RT_LIST_PNEXT_CIRC(edgeuse, eu)->vu_p;
		/* Kill lone vertex loop */
		nmg_klu(lu);
		if(rt_g.NMG_debug&DEBUG_COMBINE)  {
			rt_log("After LONE_V_ESPLIT, the final loop:\n");
			nmg_pr_lu(nmg_lu_of_vu(rs->vu[pos]), "   ");
			nmg_face_lu_plot(nmg_lu_of_vu(rs->vu[pos]), rs);
		}
		break;
	case NMG_ACTION_LONE_V_JAUNT:
		/*
		 * Take current loop on a jaunt from current edge up to the
		 * vertex from this lone vertex loop,
		 * and back again.
		 * This only happens in "IN" state.
		 */
		lu = nmg_lu_of_vu( vu );
		NMG_CK_LOOPUSE(lu);
		prev_vu = rs->vu[pos-1];
		NMG_CK_VERTEXUSE(prev_vu);
		eu = prev_vu->up.eu_p;
		NMG_CK_EDGEUSE(eu);

		/* insert 0 length edge */
		first_new_eu = nmg_eins(eu);
		/* split the new edge, and connect it to vertex of "vu" */
		second_new_eu = nmg_eusplit( vu->v_p, first_new_eu );
		first_new_eu = RT_LIST_PLAST_CIRC(edgeuse, second_new_eu);
		/* Make the two new edgeuses share just one edge */
		nmg_moveeu( second_new_eu, first_new_eu );

		/*  We know edge geom is null, make it be the isect line */
		nmg_edge_geom_isect_line( first_new_eu->e_p, rs );

		/*  Kill lone vertex loop and that vertex use.
		 *  Vertex is still safe, being also used by new edge.
		 */
		nmg_klu(lu);

		/* Because vu changed, update vu table, for next action */
		rs->vu[pos] = second_new_eu->vu_p;
		if(rt_g.NMG_debug&DEBUG_COMBINE)  {
			rt_log("After LONE_V_JAUNT, the final loop:\n");
			nmg_face_lu_plot(nmg_lu_of_vu(rs->vu[pos]), rs);
		}
		break;
	case NMG_ACTION_CUTJOIN:
		/*
		 *  Cut loop into two, or join two into one.
		 *  The operation happens between the previous vu
		 *  and the current one.
		 *  If the two vu's are part of the same loop,
		 *  then cut the loop into two, otherwise
		 *  join the two loops into one.
		 */
		lu = nmg_lu_of_vu( vu );
		NMG_CK_LOOPUSE(lu);
		prev_vu = rs->vu[pos-1];
		NMG_CK_VERTEXUSE(prev_vu);
		prev_lu = nmg_lu_of_vu( prev_vu );
		NMG_CK_LOOPUSE(prev_lu);

		if( lu->l_p == prev_lu->l_p )  {
			/* Same loop, cut into two */
			if(rt_g.NMG_debug&DEBUG_COMBINE)
				rt_log("nmg_cut_loop(prev_vu=x%x, vu=x%x)\n", prev_vu, vu);
			nmg_cut_loop( prev_vu, vu );
			if(rt_g.NMG_debug&DEBUG_COMBINE)  {
				rt_log("After CUT, the final loop:\n");
				nmg_pr_lu_briefly(nmg_lu_of_vu(rs->vu[pos]), (char *)0);
				nmg_face_lu_plot(nmg_lu_of_vu(rs->vu[pos]), rs);
			}
			break;
		}
		/*
		 *  prev_vu and vu are in different loops,
		 *  join the two loops into one loop.
		 *  No edgeuses are deleted at this stage,
		 *  so some "snakes" may appear in the process.
		 */
		if(rt_g.NMG_debug&DEBUG_COMBINE)
			rt_log("nmg_join_2loops(prev_vu=x%x, vu=x%x)\n",
			prev_vu, vu);

		nmg_vmodel(nmg_find_model(&vu->l.magic));
		nmg_join_2loops( prev_vu, vu );

		/* update vu[pos], as it will have changed. */
		/* Must be all on one line for SGI 3d compiler */
		rs->vu[pos] = RT_LIST_PNEXT_CIRC(edgeuse,prev_vu->up.eu_p)->vu_p;

		if(rt_g.NMG_debug&DEBUG_COMBINE)  {
			rt_log("After JOIN, the final loop:\n");
			nmg_face_lu_plot(nmg_lu_of_vu(rs->vu[pos]), rs);
		}
		break;
	}

	rs->state = stp->new_state;
}

void
nmg_face_plot( fu )
struct faceuse	*fu;
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

void
nmg_face_lu_plot( lu, rs )
struct loopuse		*lu;
struct nmg_ray_state	*rs;
{
	FILE	*fp;
	struct model	*m;
	long		*b;
	char		buf[128];
	static int	num = 0;

	if(!(rt_g.NMG_debug&DEBUG_PLOTEM)) return;

	NMG_CK_LOOPUSE(lu);
	m = nmg_find_model((long *)lu);
	sprintf(buf, "loop%d.pl", num++ );

	fp = fopen(buf, "w");
	b = (long *)rt_calloc( m->maxindex, sizeof(long), "nmg_face_lu_plot flag[]" );
	nmg_pl_lu(fp, lu, b, 255, 0, 0);
	/* A yellow line for the ray */
	pl_color(fp, 255, 255, 0);
	pdv_3line(fp, rs->vu[0]->v_p->vg_p->coord,
		rs->vu[rs->nvu-1]->v_p->vg_p->coord );
	fclose(fp);
	rt_log("wrote %s\n", buf);
	rt_free( (char *)b, "nmg_face_lu_plot flag[]" );
}
                                                                                                                                      