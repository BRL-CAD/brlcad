/*
 *			N M G _ C O M B . C
 *
 *  After two faces have been intersected, cut or join loops crossed
 *  by the line of intersection.
 *
 *  The line of intersection ("ray") will divide the face into two sets
 *  of loops.  No one loop may cross the ray after this routine is finished.
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
 *	This software is Copyright (C) 1991 by the United States Army.
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

/* States of the state machine */
#define NMG_STATE_ERROR		0
#define NMG_STATE_OUT		1
#define NMG_STATE_ON_L		2
#define NMG_STATE_ON_R		3
#define NMG_STATE_ON_B		4
#define NMG_STATE_IN		5
static char *state_names[] = {
	"*ERROR*",
	"out",
	"on_L",
	"on_R",
	"on_both",
	"in",
	"TOOBIG"
};

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
static void
ptbl_vsort(b, fu1, fu2, pt, dir, mag, dist_tol)
struct nmg_ptbl *b;		/* table of vertexuses on intercept line */
struct faceuse	*fu1;
struct faceuse	*fu2;
point_t		pt;
vect_t		dir;
fastf_t		*mag;
fastf_t		dist_tol;
{
	union {
		struct vertexuse **vu;
		long **magic_p;
	} p;
	register int i, j;

	p.magic_p = b->buffer;
	/* check vertexuses and compute distance from start of line */
	for(i = 0 ; i < b->end ; ++i) {
		vect_t		vect;
		NMG_CK_VERTEXUSE(p.vu[i]);

		VSUB2(vect, p.vu[i]->v_p->vg_p->coord, pt);
		mag[i] = VDOT( vect, dir );
		if( mag[i] < 0 )  {
			if( mag[i] < -dist_tol )  {
				/* Code later depends on positive mag's */
				VPRINT("coord", p.vu[i]->v_p->vg_p->coord);
				VPRINT("pt", pt);
				VPRINT("vect", vect);
				rt_log("magnitude=%e\n", mag[i]);
				rt_bomb("ptbl_vsort: negative dist\n");
			}
			mag[i] = 0;
		}
	}

	for(i=0 ; i < b->end - 1 ; ++i) {
		for (j=i+1; j < b->end ; ++j) {
			register struct vertexuse *tvu;
			register fastf_t	tmag;

			if( mag[i] < mag[j]-dist_tol )  continue;
			tmag = mag[i] - mag[j];
			if( NEAR_ZERO( tmag, dist_tol ) )  {
				mag[j] = mag[i];	/* force equal */
				if( p.vu[i]->v_p < p.vu[j]->v_p )  continue;
				if( p.vu[i]->v_p == p.vu[j]->v_p )  {
					if( p.vu[i] < p.vu[j] )  continue;
					if( p.vu[i] == p.vu[j] )  {
						int	last = b->end - 1;
						/* vu duplication, eliminate! */
						if( j >= last )  {
							/* j is last element */
							b->end--;
							break;
						}
						/* rewrite j with last element */
						p.vu[j] = p.vu[last];
						mag[j] = mag[last];
						b->end--;
						/* Repeat this index */
						j--;
						continue;
					}
					/* p.vu[i] > p.vu[j], fall through */
				}
				/* p.vu[i]->v_p > p.vu[j]->v_p, fall through */
			}
			/* mag[i] > mag[j] */

			/* exchange [i] and [j] */
			tvu = p.vu[i];
			p.vu[i] = p.vu[j];
			p.vu[j] = tvu;

			tmag = mag[i];
			mag[i] = mag[j];
			mag[j] = tmag;
		}
	}
}

struct edgeuse *
nmg_eu_with_vu_in_lu( lu, vu )
struct loopuse		*lu;
struct vertexuse	*vu;
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
}

/*
 *			R T _ A N G L E _ M E A S U R E
 *
 *  Using two perpendicular vectors (x_dir and y_dir) which lie
 *  in the same plane as 'vec', return the angle (in radians) of 'vec'
 *  from x_dir, going CCW around the perpendicular x_dir CROSS y_dir.
 *
 *  Trig note -
 *
 *  theta = atan2(x,y) returns an angle in the range -pi to +pi.
 *  Here, we need an angle in the range of 0 to 2pi.
 *  This could be implemented by adding 2pi to theta when theta is negative,
 *  but this could have nasty numeric ambiguity right in the vicinity
 *  of theta = +pi, which is a very critical angle for the applications using
 *  this routine.
 *  So, an alternative formulation is to compute gamma = atan2(-x,-y),
 *  and then theta = gamma + pi.  Now, any error will occur in the
 *  vicinity of theta = 0, which can be handled much more readily.
 *  If theta is negative, set it to zero.
 *  If theta is greater than two pi, set it to two pi.
 *  These conditions only occur if there are problems in atan2().
 *
 *  Returns -
 *	vec == x_dir returns 0,
 *	vec == y_dir returns pi/2,
 *	vec == -x_dir returns pi,
 *	vec == -y_dir returns 3*pi/2.
 */
double
rt_angle_measure( vec, x_dir, y_dir )
vect_t	vec;
vect_t	x_dir;
vect_t	y_dir;
{
	fastf_t		xproj, yproj;
	fastf_t		gamma;
	fastf_t		ang;

	xproj = -VDOT( vec, x_dir );
	yproj = -VDOT( vec, y_dir );
	gamma = atan2( yproj, xproj );	/* -pi..+pi */
	ang = rt_pi + gamma;		/* 0..+2pi */
	if( ang < 0 )  {
		return 0;
	} else if( ang > rt_twopi )  {
		return rt_twopi;
	}
	return ang;
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
nmg_vu_angle_measure( vu, x_dir, y_dir )
struct vertexuse	*vu;
vect_t			x_dir;
vect_t			y_dir;
{	
	struct loopuse	*lu;
	struct edgeuse	*this_eu;
	struct edgeuse	*prev_eu;
	vect_t		vec;
	fastf_t		ang;

	NMG_CK_VERTEXUSE( vu );
	if( *vu->up.magic_p == NMG_LOOPUSE_MAGIC )  {
		return 0;		/* Unable to compute 'vec' */
	}
	lu = nmg_lu_of_vu(vu);
	this_eu = nmg_eu_with_vu_in_lu( lu, vu );
	prev_eu = this_eu;
	do {
		prev_eu = RT_LIST_PLAST_CIRC( edgeuse, prev_eu );
		if( prev_eu == this_eu )  {
			return 0;	/* Unable to compute 'vec' */
		}
		/* Skip any edges that stay on this vertex */
	} while( prev_eu->vu_p->v_p == this_eu->vu_p->v_p );
	VSUB2( vec, prev_eu->vu_p->v_p->vg_p->coord, vu->v_p->vg_p->coord );
	ang = rt_angle_measure( vec, x_dir, y_dir );
rt_log("ang=%g, vec=(%g,%g,%g), x=(%g,%g,%g), y=(%g,%g,%g)\n",
 ang*rt_radtodeg, V3ARGS(vec), V3ARGS(x_dir), V3ARGS(y_dir) );
	return ang;
}

#define NMG_E_ASSESSMENT_LEFT		1
#define NMG_E_ASSESSMENT_RIGHT		2
#define NMG_E_ASSESSMENT_ON		3

#define NMG_V_ASSESSMENT_LONE		1
#define NMG_V_ASSESSMENT_COMBINE(_p,_n)	(((_p)<<2)|(_n))

static char *nmg_e_assessment_names[16] = {
	"ASSESS_0",
	"LONE",
	"ASSESS_2",
	"ASSESS_3",
	"Left,0",
	"Left,Left",
	"Left,Right",
	"Left,On",
	"Right,0",
	"Right,Left",
	"Right,Right",
	"Right,On",
	"On,0",
	"On,Left",
	"On,Right",
	"On,On"
};

static char *nmg_v_assessment_names[4] = {
	"*ERROR*",
	"LEFT",
	"RIGHT",
	"ON"
};

struct nmg_ray_state {
	struct vertexuse	**vu;		/* ptr to vu array */
	int			nvu;		/* len of vu[] */
	point_t			pt;		/* The ray */
	vect_t			dir;
	vect_t			left;		/* points left of ray, on face */
	int			state;
	vect_t			ang_x_dir;	/* x axis for angle measure */
	vect_t			ang_y_dir;	/* y axis for angle measure */
};

/*
 *			N M G _ A S S E S S _ E U
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
	do {
		if( forw )  {
			othereu = RT_LIST_PNEXT_CIRC( edgeuse, othereu );
		} else {
			othereu = RT_LIST_PLAST_CIRC( edgeuse, othereu );
		}
		if( othereu == eu )  {
			/* Back to where search started */
rt_log("ON: (no edges)\n");
			ret = NMG_E_ASSESSMENT_ON;
			goto out;
		}
		/* Skip over any edges that stay on this vertex */
		otherv = othereu->vu_p->v_p;
	} while( otherv == v );

	/*  If the other vertex is mentioned anywhere on the ray's vu list,
	 *  then the edge is "on" the ray.
	 *  There is a slight possibility that loop/face orientation might
	 *  play a factor in choosing the correct scan direction.
	 */
/**	for( i=rs->nvu-1; i >= 0; i-- )  { **/
	for( i=0; i < rs->nvu; i++ )  {
		if( rs->vu[i]->v_p == otherv )  {
rt_log("ON: vu[%d]=x%x otherv=x%x, i=%d\n", pos, rs->vu[pos], otherv, i );
			ret = NMG_E_ASSESSMENT_ON;
			goto out;
		}
	}

	/*
	 *  The edge must lie to one side or the other of the ray.
	 */
#if 0
VPRINT("assess_eu from", v->vg_p->coord);
VPRINT("          to  ", otherv->vg_p->coord);
#endif
	VSUB2( heading, otherv->vg_p->coord, v->vg_p->coord );
	if( VDOT( heading, rs->left ) < 0 )  {
		ret = NMG_E_ASSESSMENT_RIGHT;
	} else {
		ret = NMG_E_ASSESSMENT_LEFT;
	}
out:
	rt_log("nmg_assess_eu(x%x, fw=%d, pos=%d) v=x%x otherv=x%x: %s\n",
		eu, forw, pos, v, otherv,
		nmg_v_assessment_names[ret] );
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
	next_ass = nmg_assess_eu( this_eu, 1, rs, pos );
	prev_ass = nmg_assess_eu( this_eu, 0, rs, pos );
	ass = NMG_V_ASSESSMENT_COMBINE( prev_ass, next_ass );
	return ass;
}

struct nmg_vu_stuff {
	struct vertexuse	*vu;
	int			loop_index;
	struct nmg_loop_stuff	*lsp;
	fastf_t			vu_angle;
};
struct nmg_loop_stuff {
	struct loopuse	*lu;
	fastf_t		max_angle;
};

/*
 *			N M G _ F A C E _ V U _ C O M P A R E
 *
 *  Support routine for nmg_face_vu_sort(), via qsort().
 *
 *  If vu's are from same loop, sort is from larger to smaller vu_angle.
 *  If vu's are from different loops,
 *  sort is from small to large loop max_angle.
 */
static int
nmg_face_vu_compare( a, b )
genptr_t	a;
genptr_t	b;
{
	register struct nmg_vu_stuff	*va = (struct nmg_vu_stuff *)a;
	register struct nmg_vu_stuff	*vb = (struct nmg_vu_stuff *)b;

	if( va->loop_index == vb->loop_index )  {
		/* Sort from larger to smaller vu_angle */
		if( va->vu_angle > vb->vu_angle )  return -1;
		return 1;
	}
	/* Sort from small to large max_angle */
	if( va->lsp->max_angle > vb->lsp->max_angle )
		return 1;
	return -1;
}

/*
 *			N M G _ F A C E _ V U _ S O R T
 */
int
nmg_face_vu_sort( rs, start, end )
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

rt_log("nmg_face_vu_sort(, %d, %d)\n", start, end);
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
		rt_log("vu[%d]=x%x v=x%x assessment=%s\n",
			i, rs->vu[i], rs->vu[i]->v_p, nmg_e_assessment_names[ass] );
		/*  Ignore lone vertices, unless that is all that there is,
		 *  in which case, let just one through.  (return 'start+1');
		 */
		if( *(rs->vu[i]->up.magic_p) == NMG_LOOPUSE_MAGIC )  {
			if( i <= start && nvu == 0 )  return start+1;
			/* Drop this loop of a single vertex in sanitize() */
			lu->orientation =
			  lu->lumate_p->orientation = OT_BOOLPLACE;
			continue;
		}
		vs[nvu].vu = rs->vu[i];
		/* x_dir is -dir, y_dir is -left */
		vs[nvu].vu_angle = nmg_vu_angle_measure( rs->vu[i],
			rs->ang_x_dir, rs->ang_y_dir ) * rt_radtodeg;
		/* Search for loopuse table entry */
		for( l = 0; l < nloop; l++ )  {
			if( ls[l].lu == lu )  goto got_loop;
		}
		/* didn't find loopuse in table, add to table */
		l = nloop++;
		ls[l].lu = lu;
		ls[l].max_angle = 0;
got_loop:
		vs[nvu].loop_index = l;
		vs[nvu].lsp = &ls[l];
		if( vs[nvu].vu_angle > ls[l].max_angle )
			ls[l].max_angle = vs[nvu].vu_angle;
		nvu++;
	}
	rt_log("Loop table (before sort):\n");
	for( l=0; l < nloop; l++ )  {
		rt_log("  index=%d, lu=x%x, max_angle=%g\n",
			l, ls[l].lu, ls[l].max_angle );
	}
	rt_log("Vertexuse table:\n");
	for( i=0; i < nvu; i++ )  {
		rt_log("  vu=x%x, loop_index=%d, vu_angle=%g\n",
			vs[i].vu, vs[i].loop_index, vs[i].vu_angle );
	}

	/* Sort the vertexuse table into appropriate order */
	qsort( (genptr_t)vs, nvu, sizeof(*vs), nmg_face_vu_compare );

	rt_log("Vertexuse table (after sort):\n");
	for( i=0; i < nvu; i++ )  {
		rt_log("  vu=x%x, loop_index=%d, vu_angle=%g\n",
			vs[i].vu, vs[i].loop_index, vs[i].vu_angle );
	}

	/* Copy new vu's back to main array */
	for( i=0; i < nvu; i++ )  {
		rs->vu[start+i] = vs[i].vu;
	}
	return start+nvu;
}

/*
 *			N M G _ F A C E _ C O M B I N E
 *
 *	collapse loops,vertices within face fu1 (relative to fu2)
 *
 */
void
nmg_face_combine(b, fu1, fu2, pt, dir)
struct nmg_ptbl	*b;		/* table of vertexuses in fu1 on intercept line */
struct faceuse	*fu1;		/* face being worked */
struct faceuse	*fu2;		/* for plane equation */
point_t		pt;
vect_t		dir;
{
	fastf_t		*mag;
	struct vertexuse	**vu;
	register int	i;
	register int	j;
	int		k;
	int		m;
	fastf_t		dist_tol = 0.05;	/* XXX */
	struct nmg_ray_state	rs;

rt_log("\nnmg_face_combine(fu1=x%x, fu2=x%x)\n", fu1, fu2);
nmg_pr_fu_briefly(fu1,(char *)0);
	mag = (fastf_t *)rt_calloc(b->end, sizeof(fastf_t),
		"vector magnitudes along ray, for sort");

	/*
	 *  Sort hit points by increasing distance, vertex ptr, vu ptr,
	 *  and eliminate any duplicate vu's.
	 */
	ptbl_vsort(b, fu1, fu2, pt, dir, mag, dist_tol);

	/*
	 *  Set up nmg_ray_state structure.
	 *  "left" is a vector that lies in the plane of the face
	 *  which contains the loops being operated on.
	 *  It points in the direction "left" of the ray.
	 */
	vu = (struct vertexuse **)b->buffer;
	rs.vu = vu;
	rs.nvu = b->end;
	VMOVE( rs.pt, pt );
	VMOVE( rs.dir, dir );
rt_log("fu->orientation=%d\n", fu1->orientation);
VPRINT("fg_p->N", fu1->f_p->fg_p->N);
VPRINT(" pt", pt);
VPRINT("dir", dir);
	VCROSS( rs.left, fu1->f_p->fg_p->N, dir );
VPRINT("left", rs.left);
	rs.state = NMG_STATE_OUT;

	/* For measuring angle CCW around plane from -dir */
	VREVERSE( rs.ang_x_dir, dir );
	VREVERSE( rs.ang_y_dir, rs.left );

	nmg_face_plot( fu1 );

	/* Print list of intersections */
	rt_log("Ray vu intersection list:\n");
	for( i=0; i < b->end; i++ )  {
		rt_log(" %d ", i );
		nmg_pr_vu_briefly( vu[i], (char *)0 );
	}

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
rt_log("single vertexuse at index %d\n", i);
(void)nmg_vu_angle_measure( vu[i], rs.ang_x_dir, rs.ang_y_dir );
			nmg_face_state_transition( vu[i], &rs, i );
			nmg_face_plot( fu1 );
			j = i+1;
		} else {
			/* Find range of vertexuses at this distance */
			struct vertex	*v;

			for( j = i+1; j < b->end; j++ )  {
				if( mag[j] != mag[i] )  break;
			}
			/* vu Interval runs from [i] to [j-1] inclusive */
rt_log("interval from %d to %d\n", i, j );
			/* Ensure that all vu's point to same vertex */
			v = vu[i]->v_p;
			for( k = i+1; k < j; k++ )  {
				if( vu[k]->v_p != v )  rt_bomb("nmg_face_combine: vu block with differing vertices\n");
			}
			/* All vu's point to the same vertex, sort them */
			m = nmg_face_vu_sort( &rs, i, j );
			/* Process vu list, up to cutoff index 'm' */
			for( k = i; k < m; k++ )  {
				nmg_face_state_transition( vu[k], &rs, k );
				nmg_face_plot( fu1 );
			}
			vu[j-1] = vu[m-1]; /* for next iteration's lookback */
		}
	}

	if( rs.state != NMG_STATE_OUT )  {
		rt_log("ERROR nmg_face_combine() ended in state %s?\n",
			state_names[rs.state] );
	}

	rt_free((char *)mag, "vector magnitudes");
}

/*
 *  State machine transition tables
 *  Indexed by MNG_V_ASSESSMENT values.
 */

#define NMG_ACTION_ERROR	0
#define NMG_ACTION_NONE		1
#define NMG_ACTION_VFY_EXT	2
#define NMG_ACTION_LONE_V_ESPLIT	3
#define NMG_ACTION_LONE_V_JAUNT	4
#define NMG_ACTION_CUTJOIN	5
static char *action_names[] = {
	"*ERROR*",
	"-none-",
	"VFY_EXT",
	"ESPLIT",
	"JAUNT",
	"CUTJOIN",
	"*TOOBIG*"
};

struct state_transitions {
	int	new_state;
	int	action;
};

static CONST struct state_transitions nmg_state_is_out[16] = {
	{ /* 0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LONE */		NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ /* 2 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* 3 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,LEFT */	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ /* LEFT,RIGHT */	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ /* LEFT,ON */		NMG_STATE_ON_L,		NMG_ACTION_VFY_EXT },
	{ /* RIGHT,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,LEFT */	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ /* RIGHT,RIGHT */	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ /* RIGHT,ON */	NMG_STATE_ON_R,		NMG_ACTION_VFY_EXT },
	{ /* ON,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON,LEFT */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON,RIGHT */	NMG_STATE_ON_R,		NMG_ACTION_VFY_EXT },
	{ /* ON,ON */		NMG_STATE_ERROR,	NMG_ACTION_ERROR }
};

static CONST struct state_transitions nmg_state_is_on_L[16] = {
	{ /* 0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LONE */		NMG_STATE_ON_L,		NMG_ACTION_LONE_V_ESPLIT },
	{ /* 2 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* 3 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,ON */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,RIGHT */	NMG_STATE_ON_L,		NMG_ACTION_NONE },
	{ /* RIGHT,ON */	NMG_STATE_ON_B,		NMG_ACTION_VFY_EXT },
	{ /* ON,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON,LEFT */		NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ /* ON,RIGHT */	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ /* ON,ON */		NMG_STATE_ON_L,		NMG_ACTION_NONE }
};

static CONST struct state_transitions nmg_state_is_on_R[16] = {
	{ /* 0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LONE */		NMG_STATE_ON_R,		NMG_ACTION_LONE_V_ESPLIT },
	{ /* 2 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* 3 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,LEFT */	NMG_STATE_ON_R,		NMG_ACTION_NONE },
	{ /* LEFT,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,ON */		NMG_STATE_ON_B,		NMG_ACTION_VFY_EXT },
	{ /* RIGHT,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,ON */	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ /* ON,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON,LEFT */		NMG_STATE_IN,		NMG_ACTION_NONE },
	{ /* ON,RIGHT */	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ /* ON,ON */		NMG_STATE_ON_R,		NMG_ACTION_NONE }
};
static CONST struct state_transitions nmg_state_is_on_B[16] = {
	{ /* 0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LONE */		NMG_STATE_ON_B,		NMG_ACTION_LONE_V_ESPLIT },
	{ /* 2 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* 3 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,ON */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,LEFT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,ON */	NMG_STATE_ON_L,		NMG_ACTION_NONE },
	{ /* ON,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON,LEFT */		NMG_STATE_ON_R,		NMG_ACTION_NONE },
	{ /* ON,RIGHT */	NMG_STATE_ON_L,		NMG_ACTION_NONE },
	{ /* ON,ON */		NMG_STATE_ON_B,		NMG_ACTION_NONE }
};

static CONST struct state_transitions nmg_state_is_in[16] = {
	{ /* 0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LONE */		NMG_STATE_IN,		NMG_ACTION_LONE_V_JAUNT },
	{ /* 2 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* 3 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* LEFT,LEFT */	NMG_STATE_IN,		NMG_ACTION_CUTJOIN },
	{ /* LEFT,RIGHT */	NMG_STATE_OUT,		NMG_ACTION_CUTJOIN },
	{ /* LEFT,ON */		NMG_STATE_ON_R,		NMG_ACTION_CUTJOIN },
	{ /* RIGHT,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* RIGHT,LEFT */	NMG_STATE_OUT,		NMG_ACTION_CUTJOIN },
	{ /* RIGHT,RIGHT */	NMG_STATE_IN,		NMG_ACTION_CUTJOIN },
	{ /* RIGHT,ON */	NMG_STATE_ON_L,		NMG_ACTION_CUTJOIN },
	{ /* ON,0 */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON,LEFT */		NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON,RIGHT */	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ /* ON,ON */		NMG_STATE_ERROR,	NMG_ACTION_ERROR }
};

int
nmg_face_state_transition( vu, rs, pos )
struct vertexuse	*vu;
struct nmg_ray_state	*rs;
{
	int			assessment;
	int			old;
	struct state_transitions	*stp;
	struct vertexuse	*prev_vu;
	struct edgeuse		*eu;
	struct loopuse		*lu;
	struct loopuse		*prev_lu;
	struct edgeuse	*first_new_eu;
	struct edgeuse	*second_new_eu;

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
rt_log("nmg_face_state_transition(vu x%x)\n\told=%s, assessed=%s, new=%s, action=%s\n",
vu, state_names[old], nmg_e_assessment_names[assessment],
state_names[stp->new_state], action_names[stp->action] );

	switch( stp->action )  {
	default:
	case NMG_ACTION_ERROR:
		/* First, print this faceuse */
		lu = nmg_lu_of_vu( vu );
		/* Drop a plot file */
		rt_g.NMG_debug |= DEBUG_COMBINE|DEBUG_PLOTEM;
		nmg_pl_comb_fu( 0, 1, lu->up.fu_p );
		/* Print the faceuse for later analysis */
		nmg_pr_fu(lu->up.fu_p, (char *)0);
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
			rt_log("nmg_face_state_transition: VFY_EXT got orientation=%d\n",
				lu->orientation );
			break;
		}
		break;
	case NMG_ACTION_LONE_V_ESPLIT:
		/*
		 *  Split edge to include vertex from this lone vert loop.
		 *  This only happens in an "ON" state, so split edge that
		 *  starts with previously seen vertex.
		 */
		lu = nmg_lu_of_vu( vu );
		NMG_CK_LOOPUSE(lu);
		prev_vu = rs->vu[pos-1];
		NMG_CK_VERTEXUSE(prev_vu);
		eu = prev_vu->up.eu_p;
		NMG_CK_EDGEUSE(eu);
		(void)nmg_esplit( vu->v_p, eu->e_p );
		/* Update vu table with new value */
		rs->vu[pos] = RT_LIST_PNEXT_CIRC(edgeuse, eu)->vu_p;
		/* Kill lone vertex loop */
		nmg_klu(lu);
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

		/*  Kill lone vertex loop and that vertex use.
		 *  Vertex is still safe, being also used by new edge.
		 */
		nmg_klu(lu);

		/* Because vu changed, update vu table, for next action */
		rs->vu[pos] = second_new_eu->vu_p;
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
rt_log("nmg_cut_loop\n");
			nmg_cut_loop( prev_vu, vu );
		} else {
			/* Different loops, join into one. */
			nmg_join_2loops( prev_vu, vu );
			/* update vu[pos], as it will have changed. */
			rs->vu[pos] = RT_LIST_PNEXT_CIRC(edgeuse,
				prev_vu->up.eu_p)->vu_p;
		}
		break;
	}

	rs->state = stp->new_state;
}

/*
 *			N M G _ J O I N _ 2 L O O P S
 *
 *  Intended to join an interior and exterior loop together,
 *  by building a bridge between the two indicated vertices.
 */
int
nmg_join_2loops( vu1, vu2 )
struct vertexuse	*vu1;
struct vertexuse	*vu2;
{
	struct edgeuse	*eu1, *eu2;
	struct edgeuse	*new_eu;
	struct edgeuse	*first_new_eu;
	struct edgeuse	*second_new_eu;
	struct edgeuse	*final_eu2;
	struct loopuse	*lu1, *lu2;

	NMG_CK_VERTEXUSE(vu1);
	NMG_CK_VERTEXUSE(vu2);
	eu1 = vu1->up.eu_p;
	eu2 = vu2->up.eu_p;
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	lu1 = eu1->up.lu_p;
	lu2 = eu2->up.lu_p;
	NMG_CK_LOOPUSE(lu1);
	NMG_CK_LOOPUSE(lu2);

rt_log("nmg_join_2loops\n");
	if( lu1 == lu2 || lu1->l_p == lu2->l_p )
		rt_bomb("nmg_join_2loops: can't join loop to itself\n");

	if( lu1->orientation == lu2->orientation )
		rt_bomb("nmg_join_2loops: can't join loops of same orientation\n");

	if( lu1->up.fu_p != lu2->up.fu_p )
		rt_bomb("nmg_join_2loops: can't join loops in different faces\n");

	/*
	 *  Start by taking a jaunt from vu1 to vu2 and back.
	 */
	/* insert 0 length edge */
	first_new_eu = nmg_eins(eu1);
	/* split the new edge, and connect it to vertex 2 */
	second_new_eu = nmg_eusplit( vu2->v_p, first_new_eu );
	first_new_eu = RT_LIST_PLAST_CIRC(edgeuse, second_new_eu);
	/* Make the two new edgeuses share just one edge */
	nmg_moveeu( second_new_eu, first_new_eu );

	/*
	 *  Gobble edges off of loop2, and insert them into loop1,
	 *  immediately after first_new_eu.
	 *  The final edge from loop 2 will then be followed by
	 *  second_new_eu.
	 */
	final_eu2 = RT_LIST_PLAST_CIRC(edgeuse, eu2 );
	while( RT_LIST_NON_EMPTY( &lu2->down_hd ) )  {
		eu2 = RT_LIST_PNEXT_CIRC(edgeuse, final_eu2);

		RT_LIST_DEQUEUE(&eu2->l);
		RT_LIST_INSERT(&second_new_eu->l, &eu2->l);
		eu2->up.lu_p = lu1;

		RT_LIST_DEQUEUE(&eu2->eumate_p->l);
		RT_LIST_APPEND(&second_new_eu->eumate_p->l, &eu2->eumate_p->l);
		eu2->eumate_p->up.lu_p = lu1;
	}

	/* Kill entire loop associated with lu2 */
rt_log("Here is what will be killed\n");
nmg_pr_lu(lu2, (char *)0);
	nmg_klu(lu2);
}

nmg_face_plot( fu )
struct faceuse	*fu;
{
	extern void (*nmg_vlblock_anim_upcall)();
	struct model		*m;
	struct rt_vlblock	*vbp;
	struct face_g	*fg;
	long		*tab;
	int		fancy;
	int		delay;

	if( ! (rt_g.NMG_debug & DEBUG_PL_ANIM) )  return;

	NMG_CK_FACEUSE(fu);

	m = nmg_find_model( (long *)fu );
	NMG_CK_MODEL(m);

	/* get space for list of items processed */
	tab = (long *)rt_calloc( m->maxindex+1, sizeof(long),
		"nmg_face_plot tab[]");

	vbp = rt_vlblock_init();

	fancy = 1;
	nmg_vlblock_fu(vbp, fu, tab, fancy );

	/* Cause animation of boolean operation as it proceeds! */
	if( nmg_vlblock_anim_upcall )  {
		delay = 1;

		/* if requested, delay 3/4 second */
		(*nmg_vlblock_anim_upcall)( vbp,
			delay ? 750000 : 0 );
	} else {
		rt_log("null nmg_vlblock_anim_upcall, no animation\n");
	}
	rt_vlblock_free(vbp);
	rt_free( (char *)tab, "nmg_face_plot tab[]" );

}
                                                                                                                                      