/*
 *			N M G _ R T _ S E G S. C
 *
 *	Support routines for raytracing an NMG.
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066  USA
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "./nmg_rt.h"

/*	EDGE-FACE correlation data
 * 	used in edge_hit() for 3manifold case
 */
struct ef_data {
    	fastf_t		fdotr;	/* face vector VDOT with ray */
    	fastf_t		fdotl;	/* face vector VDOT with ray-left */
    	fastf_t		ndotr;	/* face normal VDOT with ray */
    	struct edgeuse *eu;
};

static
struct structparse ef_parsetab[] = {
	{"%f", 1, "fdotr", offsetof(struct ef_data, fdotr), FUNC_NULL},
	{"%f", 1, "fdotl", offsetof(struct ef_data, fdotl), FUNC_NULL},
	{"%f", 1, "ndotr", offsetof(struct ef_data, ndotr), FUNC_NULL},
	{"%x", 1, "eu",   offsetof(struct ef_data, eu),   FUNC_NULL},
	{"", 0, (char *)NULL,	  0,			  FUNC_NULL}
};

static
struct structparse hit_parsetab[] = {
{"%f", 1, "hit_dist", offsetof(struct hit, hit_dist), FUNC_NULL},
{"%f", 3, "hit_point", offsetofarray(struct hit, hit_point), FUNC_NULL},
{"%f", 4, "hit_normal", offsetofarray(struct hit, hit_normal), FUNC_NULL},
{"%f", 3, "hit_vpriv", offsetofarray(struct hit, hit_vpriv), FUNC_NULL},
{"%x", 1, "hit_private", offsetof(struct hit, hit_private), FUNC_NULL},
{"%d", 1, "hit_surfno", offsetof(struct hit, hit_surfno), FUNC_NULL},
{"", 0, (char *)NULL,	  0,			  FUNC_NULL}
};

static
print_seg_list(seghead)
struct seg *seghead;
{
	struct seg *seg_p;

	rt_log("Segment List:\n");
	/* print debugging data before returning */
	for (RT_LIST_FOR(seg_p, seg, &seghead->l) ) {
		rt_log("dist %g  pt(%g,%g,%g)  N(%g,%g,%g)  =>  ",
		seg_p->seg_in.hit_dist,
		seg_p->seg_in.hit_point[0],
		seg_p->seg_in.hit_point[1],
		seg_p->seg_in.hit_point[2],
		seg_p->seg_in.hit_normal[0],
		seg_p->seg_in.hit_normal[1],
		seg_p->seg_in.hit_normal[2]);
		rt_log("dist %g  pt(%g,%g,%g)  N(%g,%g,%g)\n",
		seg_p->seg_out.hit_dist,
		seg_p->seg_out.hit_point[0],
		seg_p->seg_out.hit_point[1],
		seg_p->seg_out.hit_point[2],
		seg_p->seg_out.hit_normal[0],
		seg_p->seg_out.hit_normal[1],
		seg_p->seg_out.hit_normal[2]);
	}
}


/*
 *	We know we've hit a wire.  Make the appropriate 0-length segment.
 *	The caller is responsible for removing the hit point from the hitlist.
 */
static int
wire_hit(e_p, seg_p, rp, a_hit)
struct edge	*e_p;
struct seg	*seg_p;
struct xray	*rp;
struct hitmiss	*a_hit;
{

	vect_t eray;

	/* we've got a wire edge.
	 *
	 * Generate a normal for the edge which is perpendicular to
	 * the edge in the plane formed by the ray and the edge.
	 *
	 */

	bcopy(&a_hit->hit, &seg_p->seg_in, sizeof(struct hit));
	bcopy(&a_hit->hit, &seg_p->seg_out, sizeof(struct hit));

	/* make the normal for the ray
	 *
	 * make ray of edge
	 */
	VSUB2(eray, e_p->eu_p->vu_p->v_p->vg_p->coord,
		e_p->eu_p->eumate_p->vu_p->v_p->vg_p->coord);
	
	/* make N perpendicular to ray and edge */
	VCROSS(seg_p->seg_in.hit_normal, rp->r_dir, eray);

	/* make N point toward ray origin in plane of ray and edge */
	VCROSS(seg_p->seg_in.hit_normal, eray,
		seg_p->seg_in.hit_normal);

	/* reverse normal for out-point */
	VREVERSE(seg_p->seg_out.hit_normal, seg_p->seg_in.hit_normal);

	return(2);

}

#define LEFT_MIN_ENTER	0
#define LEFT_MIN_EXIT	1
#define LEFT_MAX_ENTER	2
#define LEFT_MAX_EXIT	3
#define RIGHT_MIN_ENTER	4
#define RIGHT_MIN_EXIT	5
#define RIGHT_MAX_ENTER	6
#define RIGHT_MAX_EXIT	7


static void
sort_eus(prime_uses, e_p, tbl, rp, left_vect)
struct ef_data prime_uses[];
struct edge *e_p;
char tbl[];
struct xray *rp;
vect_t left_vect;
{
	struct ef_data tmp;
	struct face_g *fg_p;
	vect_t face_vect;
	vect_t edge_vect;
	vect_t edge_opp_vect;
	vect_t eu_vect;
	struct edgeuse *teu;

	if (rt_g.NMG_debug & DEBUG_RT_SEGS)
		rt_log("sort_eus\n");

	NMG_CK_EDGE(e_p);
	NMG_CK_EDGE_G(e_p->eg_p);


	if (rt_g.NMG_debug & DEBUG_RT_SEGS)
		VPRINT("ray_vect", rp->r_dir);

	VMOVE(edge_vect, e_p->eg_p->e_dir);
	VREVERSE(edge_opp_vect, e_p->eg_p->e_dir);
	if (rt_g.NMG_debug & DEBUG_RT_SEGS)
		VPRINT("edge_vect", edge_vect);

	/* compute the "left" vector  which is perpendicular to the edge
	 * and the ray
	 */
	VCROSS(left_vect, edge_vect, rp->r_dir);

	if (rt_g.NMG_debug & DEBUG_RT_SEGS)
		VPRINT("left_vect", left_vect);

	VUNITIZE(left_vect);

	/* compute the relevant dot products for each edge/face about
	 * this edge.
	 */
	tmp.eu = e_p->eu_p;
	do {
	    plane_t	plane;
	    /* we only care about the 3-manifold edge uses */
		
	    if (NMG_MANIFOLDS(tbl, tmp.eu) & NMG_3MANIFOLD) {

		NMG_CK_LOOPUSE(tmp.eu->up.lu_p);
		NMG_CK_FACEUSE(tmp.eu->up.lu_p->up.fu_p);
		   NMG_CK_FACE(tmp.eu->up.lu_p->up.fu_p->f_p);
		 NMG_CK_FACE_G(tmp.eu->up.lu_p->up.fu_p->f_p->fg_p);

		fg_p = tmp.eu->up.lu_p->up.fu_p->f_p->fg_p;
		NMG_GET_FU_PLANE( plane, tmp.eu->up.lu_p->up.fu_p );

		if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
			rt_log("\n");
			HPRINT("plane_eqn", plane);
		}
		
		if (tmp.eu->up.lu_p->up.fu_p->orientation == OT_SAME)
			teu = tmp.eu;
		else
			teu = tmp.eu->eumate_p;

		VSUB2(eu_vect, teu->vu_p->v_p->vg_p->coord,
			teu->eumate_p->vu_p->v_p->vg_p->coord);

		if (VDOT(e_p->eg_p->e_dir, eu_vect) < 0) {
			VCROSS(face_vect, edge_opp_vect, plane);
		} else {
			VCROSS(face_vect, edge_vect, plane);
		}
		VUNITIZE(face_vect);

		if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
			VPRINT("face_vect", face_vect);
		}
		/* figure out if this is one of the prime edgeuses */
		tmp.fdotr = VDOT(face_vect, rp->r_dir);
		tmp.fdotl = VDOT(face_vect, left_vect);
		tmp.ndotr = VDOT(plane,     rp->r_dir);

#define SAVE(a) prime_uses[a].fdotl = tmp.fdotl;	\
		prime_uses[a].fdotr = tmp.fdotr;	\
		prime_uses[a].ndotr = tmp.ndotr;	\
		prime_uses[a].eu = tmp.eu

		if (rt_g.NMG_debug & DEBUG_RT_SEGS)
			rt_structprint("tmp", ef_parsetab, (char *)&tmp);

		if (tmp.fdotl >= 0.0) {
			/* face is on "left" of ray */
			if ( tmp.ndotr > 0.0 ) {
				/* ray is "leaving" face */
				if (tmp.fdotr < prime_uses[LEFT_MIN_EXIT].fdotr) {
					SAVE(LEFT_MIN_EXIT);
				}
				if (tmp.fdotr > prime_uses[LEFT_MAX_EXIT].fdotr) {
					SAVE(LEFT_MAX_EXIT);
				}
			} else {
				/* ray is "entering" face */
				if (tmp.fdotr < prime_uses[LEFT_MIN_ENTER].fdotr) {
					SAVE(LEFT_MIN_ENTER);
				}
				if (tmp.fdotr > prime_uses[LEFT_MAX_ENTER].fdotr) {
					SAVE(LEFT_MAX_ENTER);
				}
			}
		} else {
			/* face is on "right" of ray */
			if ( tmp.ndotr > 0.0 ) {
				/* ray is "leaving face */
				if (tmp.fdotr < prime_uses[RIGHT_MIN_EXIT].fdotr) {
					SAVE(RIGHT_MIN_EXIT);
				}
				if (tmp.fdotr > prime_uses[RIGHT_MAX_EXIT].fdotr) {
					SAVE(RIGHT_MAX_EXIT);
				}
			} else {
				/* ray is "entering" face */
				if (tmp.fdotr < prime_uses[RIGHT_MIN_ENTER].fdotr) {
					SAVE(RIGHT_MIN_ENTER);
				}
				if (tmp.fdotr > prime_uses[RIGHT_MAX_ENTER].fdotr) {
					SAVE(RIGHT_MAX_ENTER);
				}
			}
		}
	    }
	    tmp.eu = tmp.eu->radial_p->eumate_p;

	} while (tmp.eu != e_p->eu_p);
}


static
v_neighborhood(v_p, tbl, rp)
struct vertex *v_p;
char		*tbl;
struct xray	*rp;
{

	NMG_CK_VERTEX(v_p);
	

}


/*
 *	if we hit a 3 manifold for seg_in we enter the solid
 *	if we hit a 3 manifold for seg_out we leave the solid
 *	if we hit a 0,1or2 manifold for seg_in, we enter/leave the solid.
 *	if we hit a 0,1or2 manifold for seg_out, we ignore it.
 */
static int
vertex_hit(v_p, seg_p, rp, tbl, a_hit, filled)
struct vertex	*v_p;
struct seg	*seg_p;
struct xray	*rp;
char		*tbl;
struct hitmiss	*a_hit;
int		filled;
{

	char manifolds = NMG_MANIFOLDS(tbl, v_p);
	struct faceuse *fu_p;
	struct vertexuse *vu_p;
	int entries = 0;
	int exits = 0;
	vect_t	saved_normal;
	fastf_t BestRdotN = 5.0;
	fastf_t RdotN;
	struct face_g *fg;
	vect_t	normal;

	if (manifolds & NMG_3MANIFOLD) {

		for (RT_LIST_FOR(vu_p, vertexuse, &v_p->vu_hd)) {
			if (NMG_MANIFOLDS(tbl, vu_p) & NMG_3MANIFOLD ||
			    NMG_MANIFOLDS(tbl, vu_p) & NMG_2MANIFOLD) {

				switch (*vu_p->up.magic_p) {
				case NMG_EDGEUSE_MAGIC:
					NMG_CK_LOOPUSE(vu_p->up.eu_p->up.lu_p);
					NMG_CK_FACEUSE(vu_p->up.eu_p->up.lu_p->up.fu_p);
					   NMG_CK_FACE(vu_p->up.eu_p->up.lu_p->up.fu_p->f_p);
					 NMG_CK_FACE_G(vu_p->up.eu_p->up.lu_p->up.fu_p->f_p->fg_p);
					fg = vu_p->up.eu_p->up.lu_p->up.fu_p->f_p->fg_p;
					NMG_GET_FU_NORMAL( normal, vu_p->up.eu_p->up.lu_p->up.fu_p );

					break;
				case NMG_LOOPUSE_MAGIC:
					NMG_CK_FACEUSE(vu_p->up.eu_p->up.lu_p->up.fu_p);
					NMG_CK_FACE(vu_p->up.eu_p->up.lu_p->up.fu_p->f_p);
					NMG_CK_FACE_G(vu_p->up.eu_p->up.lu_p->up.fu_p->f_p->fg_p);
					fg = vu_p->up.eu_p->up.lu_p->up.fu_p->f_p->fg_p;
					NMG_GET_FU_NORMAL( normal, vu_p->up.eu_p->up.lu_p->up.fu_p );
					break;
				}

				if ((RdotN = VDOT(rp->r_dir, normal)) < 0) {
					entries++;
					if (-1.0 * RdotN < BestRdotN) {
						BestRdotN = fabs(RdotN);
						VMOVE(saved_normal, normal);
					}
				} else {
					exits++;
					if (RdotN < BestRdotN) {
						BestRdotN = fabs(RdotN);
						VMOVE(saved_normal, normal);
					}
				}
			}
		}

		if (entries && !exits && filled == 0) {
			/* single in-bound hit */
			bcopy(&a_hit->hit, &seg_p->seg_in,sizeof(struct hit));
			VMOVE(seg_p->seg_in.hit_normal, saved_normal);
			filled++;
		} else if (exits && !entries && filled) {
			/* single out-bound hit */
			bcopy(&a_hit->hit, &seg_p->seg_in,sizeof(struct hit));
			VMOVE(seg_p->seg_in.hit_normal, saved_normal);
			filled++;
		}else {

			/* there are both faces which the ray "enters" and
			 * faces which it "exits" at this vertex  We have
			 * to determine what is really happening here.
			 */
			v_neighborhood(v_p, tbl, rp);
		}

		RT_LIST_DEQUEUE(&a_hit->l);
		rt_free((char *)a_hit, "freeing hitpoint");

	} else if (manifolds & NMG_2MANIFOLD) {
	    /* we've hit the corner of a dangling face */
	    if (filled == 0) {
		register int found=0;

		/* find a surface normal */
		for (RT_LIST_FOR(vu_p, vertexuse, &v_p->vu_hd)) {

			if (NMG_MANIFOLDS(tbl, vu_p) & NMG_2MANIFOLD &&
			    !(NMG_MANIFOLDS(tbl, vu_p) & NMG_3MANIFOLD) ) {
				found = 1; break;
			}
		}
		
		bcopy(&a_hit->hit, &seg_p->seg_in, sizeof(struct hit));
		bcopy(&a_hit->hit, &seg_p->seg_out, sizeof(struct hit));

		if (found) {
			/* we've found a 2manifold vertexuse.
			 * get a pointer to the faceuse and get the
			 * normal from the face
			 */

			if (*vu_p->up.magic_p == NMG_EDGEUSE_MAGIC)
				fu_p = vu_p->up.eu_p->up.lu_p->up.fu_p;
			else if (*vu_p->up.magic_p == NMG_LOOPUSE_MAGIC)
				fu_p = vu_p->up.lu_p->up.fu_p;
			else  {
				fu_p = (struct faceuse *)NULL;
				rt_bomb("vertex_hit: bad vu->up\n");
				/* NOTREACHED */
			}
			NMG_GET_FU_NORMAL( normal, fu_p );

			if (VDOT(normal, rp->r_dir) > 0.0) {
				VREVERSE(seg_p->seg_in.hit_normal, normal);
				VMOVE(seg_p->seg_out.hit_normal, normal);
			} else {
				VMOVE(seg_p->seg_in.hit_normal, normal);
				VREVERSE(seg_p->seg_out.hit_normal, normal);
			}

		} else {
			/* we didn't find a face.  How did this happen? */
			rt_log("2manifold lone vertex?\n");
			VREVERSE(seg_p->seg_in.hit_normal, rp->r_dir);
			VMOVE(seg_p->seg_out.hit_normal, rp->r_dir);

			filled = 2;
		}
	    }
	    RT_LIST_DEQUEUE(&a_hit->l);
	    rt_free((char *)a_hit, "freeing hitpoint");

	} else if (manifolds & NMG_1MANIFOLD) {
	    /* we've hit the end of a wire.
	     * this is the same as for hitting a wire.
	     */
	    if (filled == 0) {
		/* go looking for a wire edge */
		for (RT_LIST_FOR(vu_p, vertexuse, &v_p->vu_hd)) {
		    /* if we find an edge which is only a 1 manifold
		     * we'll call edge_hit with it
		     */
		    if (*vu_p->up.magic_p == NMG_EDGEUSE_MAGIC &&
			! (NMG_MANIFOLDS(tbl, vu_p->up.eu_p->e_p) &
			(NMG_3MANIFOLD|NMG_2MANIFOLD)) &&
			(NMG_MANIFOLDS(tbl, vu_p->up.eu_p->e_p) &
			NMG_1MANIFOLD) ) {
			    filled = wire_hit(vu_p->up.eu_p->e_p,
						seg_p, rp, a_hit);
						
			    break;
		    }
		}
		if (filled == 0) {
			/* we didn't find an edge.  How did this happen? */
			rt_log("1manifold lone vertex?\n");
			bcopy(&a_hit->hit, &seg_p->seg_in, sizeof(struct hit));
			bcopy(&a_hit->hit, &seg_p->seg_out, sizeof(struct hit));
			VREVERSE(seg_p->seg_in.hit_normal, rp->r_dir);
			VMOVE(seg_p->seg_out.hit_normal, rp->r_dir);

			filled = 2;
		}
	    }
	    RT_LIST_DEQUEUE(&a_hit->l);
	    rt_free((char *)a_hit, "freeing hitpoint");

	} else if (manifolds & NMG_0MANIFOLD ) {
	    if (filled == 0) {
		/* we've hit a lone vertex */
		bcopy(&a_hit->hit, &seg_p->seg_in, sizeof(struct hit));
		bcopy(&a_hit->hit, &seg_p->seg_out, sizeof(struct hit));
		VREVERSE(seg_p->seg_in.hit_normal, rp->r_dir);
		VMOVE(seg_p->seg_out.hit_normal, rp->r_dir);

		filled = 2;
	    }
	    RT_LIST_DEQUEUE(&a_hit->l);
	    rt_free((char *)a_hit, "freeing hitpoint");
	}

	return(filled);
}



/*
 *	Fill in a seg struct to reflect a grazing hit of a solid on an
 *	edge.
 *
 *	^  /___
 *	| /____
 *	|/_____
 *	o______
 *	|\_____
 *	| \____
 *	|  \___
 */
static void
edge_ray_graze(a_hit, seg_p, i_u, in, out, s)
struct seg	*seg_p;
struct hitmiss	*a_hit;
struct ef_data i_u[8];	/* "important" uses of edge */
int in, out;
char *s;
{
	if (rt_g.NMG_debug & DEBUG_RT_SEGS)
		rt_log("edge_ray_graze(%s)\n", s);

	bcopy(&a_hit->hit, &seg_p->seg_in, sizeof(struct hit));
	bcopy(&a_hit->hit, &seg_p->seg_out, sizeof(struct hit));
				
	NMG_GET_FU_NORMAL( seg_p->seg_in.hit_normal,
		i_u[in].eu->up.lu_p->up.fu_p );
				
	NMG_GET_FU_NORMAL( seg_p->seg_out.hit_normal,
		i_u[out].eu->up.lu_p->up.fu_p );

	RT_LIST_DEQUEUE(&a_hit->l);
	rt_free((char *)a_hit, "freeing hitpoint");

}
/*
 *	Fill in a seg struct to reflect entering a solid at a point
 *	on the edge.
 */
static void
edge_enter_solid(a_hit, seg_p, i_u)
struct seg	*seg_p;
struct hitmiss	*a_hit;
struct ef_data *i_u;	/* "important" uses of edge */
{

	if (rt_g.NMG_debug & DEBUG_RT_SEGS)
		rt_log("edge_enter_solid()\n");
	/* we enter solid, normal from face
	 * with most anti-rayward normal
	 *
	 *	____ ______
	 *      ____^______
	 *      ____|______
    	 *	____o______
	 *	___/|\_____
	 *	__/ | \____
	 *	_/  |  \___
	 */

		if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
			rt_log("edge_enter_solid w/ ray_hit_distance %g (%g %g %g)",
				a_hit->hit.hit_dist,
				a_hit->hit.hit_point[0],
				a_hit->hit.hit_point[1],
				a_hit->hit.hit_point[2]);

			switch ( *(int*)a_hit->hit.hit_private) {
			case NMG_FACE_MAGIC: rt_log("\tface\n"); break;
			case NMG_EDGE_MAGIC: rt_log("\tedge\n"); break;
			case NMG_VERTEX_MAGIC: rt_log("\tvertex\n"); break;
			default : rt_log(" hit unknown magic (%d)\n", 
				*(int*)a_hit->hit.hit_private); break;
			}
		}

	bcopy(&a_hit->hit, &seg_p->seg_in, sizeof(struct hit));

	if (i_u[LEFT_MIN_ENTER].ndotr < i_u[RIGHT_MIN_ENTER].ndotr) {
		NMG_GET_FU_NORMAL( seg_p->seg_in.hit_normal,
			i_u[LEFT_MIN_ENTER].eu->up.lu_p->up.fu_p );
	} else {
		NMG_GET_FU_NORMAL( seg_p->seg_in.hit_normal,
			i_u[RIGHT_MIN_ENTER].eu->up.lu_p->up.fu_p );
	}

	RT_LIST_DEQUEUE(&a_hit->l);
	rt_free((char *)a_hit, "freeing hitpoint");

}



/*
 *	Fill in seg struct to reflect hitting an edge & exiting solid
 *	at same point on edge
 */
static void
edge_zero_depth_hit(a_hit, seg_p, i_u)
struct seg	*seg_p;
struct hitmiss	*a_hit;
struct ef_data *i_u;	/* "important" uses of edge */
{

	if (rt_g.NMG_debug & DEBUG_RT_SEGS)
		rt_log("edge_zero_depth_hit()\n");
	/* zero depth hit
    	 *
	 *	_\  ^  /___
	 *	__\ | /____
	 *	___\|/_____
    	 *	____o______
	 *	___/|\_____
	 *	__/ | \____
	 *	_/  |  \___
	 */
	bcopy(&a_hit->hit, &seg_p->seg_in, sizeof(struct hit));
	bcopy(&a_hit->hit, &seg_p->seg_out, sizeof(struct hit));

    	/* choose an entry hit normal */
	if (i_u[LEFT_MIN_ENTER].ndotr < i_u[RIGHT_MIN_ENTER].ndotr) {
		NMG_GET_FU_NORMAL( seg_p->seg_in.hit_normal,
			i_u[LEFT_MIN_ENTER].eu->up.lu_p->up.fu_p );
	} else {
		NMG_GET_FU_NORMAL( seg_p->seg_in.hit_normal,
			i_u[RIGHT_MIN_ENTER].eu->up.lu_p->up.fu_p );
	}

    	/* choose an exit hit normal */
	if (i_u[LEFT_MIN_EXIT].ndotr > i_u[RIGHT_MIN_EXIT].ndotr) {
		NMG_GET_FU_NORMAL( seg_p->seg_out.hit_normal,
			i_u[LEFT_MIN_EXIT].eu->up.lu_p->up.fu_p );
	} else {
		NMG_GET_FU_NORMAL( seg_p->seg_out.hit_normal,
			i_u[RIGHT_MIN_EXIT].eu->up.lu_p->up.fu_p );
	}
	RT_LIST_DEQUEUE(&a_hit->l);
	rt_free((char *)a_hit, "freeing hitpoint");

}

/*
 *	Fill in seg structure to reflect that the ray leaves the solid at
 *	this hit location.
 */
static void
edge_leave_solid(a_hit, seg_p, i_u)
struct seg	*seg_p;
struct hitmiss	*a_hit;
struct ef_data *i_u;	/* "important" uses of edge */
{

	if (rt_g.NMG_debug & DEBUG_RT_SEGS)
		rt_log("edge_leave_solid()\n");
	/* leave solid
	*
	* 	   ^
	*	   |
	*	   o
	*	  /|\
	*	 /_|_\
	*	/__|__\
	*          |
	*/

	bcopy(&a_hit->hit, &seg_p->seg_out, sizeof(struct hit));

	/* pick an exit normal */
	if (i_u[LEFT_MIN_EXIT].ndotr > i_u[RIGHT_MIN_EXIT].ndotr) {
		NMG_GET_FU_NORMAL( seg_p->seg_out.hit_normal,
			i_u[LEFT_MIN_EXIT].eu->up.lu_p->up.fu_p);

	} else {
		NMG_GET_FU_NORMAL( seg_p->seg_out.hit_normal,
			i_u[RIGHT_MIN_EXIT].eu->up.lu_p->up.fu_p);
	}

	RT_LIST_DEQUEUE(&a_hit->l);
	rt_free((char *)a_hit, "freeing hitpoint");

}

static void
edge_confusion(a_hit, i_u, file, line)
struct hitmiss	*a_hit;
struct ef_data i_u[8];	/* "important" uses of edge */
char *file;
int line;
{

	if (file)
		rt_log("edge_hit() in %s at %d is confused\n", file, line);

	rt_structprint("LEFT_MIN_ENTER", ef_parsetab,
		(char *)&i_u[LEFT_MIN_ENTER]);
	rt_structprint("LEFT_MIN_EXIT", ef_parsetab,
		(char *)&i_u[LEFT_MIN_EXIT]);
	rt_structprint("LEFT_MAX_ENTER", ef_parsetab,
		(char *)&i_u[LEFT_MAX_ENTER]);
	rt_structprint("LEFT_MAX_EXIT", ef_parsetab,
		(char *)&i_u[LEFT_MAX_EXIT]);
	rt_structprint("RIGHT_MIN_ENTER", ef_parsetab,
		(char *)&i_u[RIGHT_MIN_ENTER]);
	rt_structprint("RIGHT_MIN_EXIT", ef_parsetab,
		(char *)&i_u[RIGHT_MIN_EXIT]);
	rt_structprint("RIGHT_MAX_ENTER", ef_parsetab,
		(char *)&i_u[RIGHT_MAX_ENTER]);
	rt_structprint("RIGHT_MAX_EXIT", ef_parsetab,
		(char *)&i_u[RIGHT_MAX_EXIT]);

}

/*
 *	if we hit a 3 manifold for seg_in we enter the solid
 *	if we hit a 3 manifold for seg_out we leave the solid
 *	if we hit a 1or2 manifold for seg_in, we enter/leave the solid.
 *	if we hit a 1or2 manifold for seg_out, we ignore it.
 */
static int
edge_hit(e_p, seg_p, rp, tbl, a_hit, filled)
struct edge	*e_p;
struct seg	*seg_p;
struct xray	*rp;
char		*tbl;
struct hitmiss	*a_hit;
int		filled;
{

	struct edgeuse *eu_p;
	char manifolds = NMG_MANIFOLDS(tbl, e_p);
	
	if (rt_g.NMG_debug & DEBUG_RT_SEGS)
		rt_log("edge_hit()\n");

	if (manifolds & NMG_3MANIFOLD) {
	
	    /* if the ray enters or leaves the solid via an edge, it is at
	     * the juncture of exactly two faces.  To determine which two
	     * faces, it is necessary to sort the faces about this edge
	     * radially about the edge.
	     *
	     * Taking the cross product of an edgeuse vector and the face
	     * normal, we get a pointer "into" and "along the surface" of
	     * the face.  We enter the solid between the two faces for which
	     * these "face vectors" are most anti-ray-ward and whose surface
	     * normals have an anti-ray-ward component.  There must
	     * be such a face on each "side" of the edge to enter or leave
	     * the solid at this edge.
	     *
	     * Likewise, we leave the solid between the two faces for which
	     * the "face vectors" are most ray-ward and whose surface normals
	     * have a ray-ward component.
	     *
	     * To differentiate sidedness, we cross the ray with an edge
	     * vector to get a vector perpendicular to the edge and the ray.
	     * this vector is said to point "left" of the ray wrt the edge.
	     *
	     *		 	^  _
	     *			|  /|
	     *			| /
	     *			|/
	     *	 "left"	<-------/
	     *		       /|
	     *		      / |
	     *		     /  |
	     *		   ray  edge
	     *
	     *
	     * If we are found to be entering the solid at an edge, we leave
	     * it again at the edge if there is a pair of "exit" faces
	     * (faces on each side of the ray/edge with ray-ward pointing
	     * "face vectors")
	     * "behind" the entry faces.  Likewise, a ray leaving a solid at
	     * an edge may re-enter the solid at the edge if there are a
	     * pair of "entry" faces "behind" the exit faces.
	     */
	    vect_t left_vect;
	    struct ef_data i_u[8];	/* "important" uses of edge */
	    register int i;

	    /*
	     * First we "sort" the faces to find the "important" ones.
	     */

	    bzero(i_u, sizeof(i_u));

	    i_u[LEFT_MIN_ENTER].fdotr = 2.0;
	    i_u[LEFT_MAX_ENTER].fdotr = -2.0;
	    i_u[LEFT_MIN_EXIT].fdotr = 2.0;
	    i_u[LEFT_MAX_EXIT].fdotr = -2.0;

	    i_u[RIGHT_MIN_ENTER].fdotr = 2.0;
	    i_u[RIGHT_MAX_ENTER].fdotr = -2.0;
	    i_u[RIGHT_MIN_EXIT].fdotr = 2.0;
	    i_u[RIGHT_MAX_EXIT].fdotr = -2.0;

	    sort_eus(i_u, e_p, tbl, rp, left_vect);

	    if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
		rt_log("\nedge_hit 3MANIFOLD\n");
		edge_confusion(a_hit, i_u, (char *)NULL, 0);
	    }

	    /* now find the two faces we're looking for */
	    if (filled) {
		/* we're looking for an exit (then entry) point */

		if (i_u[LEFT_MIN_EXIT].eu) {
			if (i_u[RIGHT_MIN_EXIT].eu) {
				if (i_u[LEFT_MAX_ENTER].eu !=
				    i_u[LEFT_MIN_ENTER].eu &&
				    i_u[RIGHT_MAX_ENTER].eu !=
				    i_u[RIGHT_MIN_ENTER].eu ){

					/* leave & re-enter solid */

					if (rt_g.NMG_debug & DEBUG_RT_SEGS)
						rt_log("edge_hit leave&re-enter\n");

					RT_LIST_DEQUEUE(&a_hit->l);
					rt_free((char *)a_hit,
						"freeing hitpoint");

				} else {
					/* leave solid */
					edge_leave_solid(a_hit, seg_p, i_u);
					filled++;
				}
			} else if (i_u[LEFT_MAX_ENTER].eu){
				/* graze exterior of solid on left */

				RT_LIST_DEQUEUE(&a_hit->l);
				rt_free((char *)a_hit, "freeing hitpoint");

				if (rt_g.NMG_debug & DEBUG_RT_SEGS)
					rt_log("edge_hit graze exterior on left\n");
			} else {
				/* What's going on? */

				edge_confusion(a_hit, i_u, __FILE__, __LINE__);
				rt_bomb("goodbye\n");

				RT_LIST_DEQUEUE(&a_hit->l);
				rt_free((char *)a_hit, "freeing hitpoint");
			}
		} else if (i_u[RIGHT_MIN_EXIT].eu &&
		    i_u[RIGHT_MAX_ENTER].eu != i_u[RIGHT_MIN_EXIT].eu) {

			/* grazing exterior of solid on right */
			if (rt_g.NMG_debug & DEBUG_RT_SEGS)
				rt_log("edge_hit graze exterior on right\n");
			RT_LIST_DEQUEUE(&a_hit->l);
			rt_free((char *)a_hit, "freeing hitpoint");
		} else {
			/* What's going on? */
    			edge_confusion(a_hit, i_u, __FILE__, __LINE__);
			rt_bomb("goodbye\n");

			RT_LIST_DEQUEUE(&a_hit->l);
			rt_free((char *)a_hit, "freeing hitpoint");
		}

	    } else if (i_u[LEFT_MIN_ENTER].eu) /* filled == 0 */ {
		/* we're looking for an entry (then exit) point on the 
		 * solid at this point.
		 */

		if (i_u[RIGHT_MIN_ENTER].eu) {
			if (i_u[LEFT_MAX_ENTER].eu !=
			    i_u[LEFT_MIN_ENTER].eu &&
			    i_u[RIGHT_MAX_ENTER].eu != 
			    i_u[RIGHT_MIN_ENTER].eu ) {
				edge_zero_depth_hit(a_hit, seg_p, i_u);
    				filled = 2;
			} else {
				edge_enter_solid(a_hit, seg_p, i_u);
    				filled = 1;
				if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
					rt_log("added %d to segment: ", filled);
					rt_pr_seg(seg_p);
				}
			}
		} else if (i_u[LEFT_MAX_EXIT].eu &&
			i_u[LEFT_MAX_EXIT].eu !=
			i_u[LEFT_MAX_ENTER].eu) {

			/* grazing hit on solid to left
			 *
			 *	__\ ^
			 *	___\|
			 *	____o
			 *	___/|
			 *	__/ |
			 */
			edge_ray_graze(a_hit, seg_p, i_u,
					LEFT_MIN_ENTER, LEFT_MAX_EXIT, "left");
			filled = 2;
		} else {
			/* How did I get here? */
    			edge_confusion(a_hit, i_u, __FILE__, __LINE__);
			rt_bomb("goodbye\n");

			RT_LIST_DEQUEUE(&a_hit->l);
			rt_free((char *)a_hit, "freeing hitpoint");
		}
	    } else if (i_u[RIGHT_MIN_ENTER].eu &&
			i_u[RIGHT_MAX_EXIT].eu !=
			i_u[RIGHT_MIN_ENTER].eu) {
    			/* grazing hit on solid to right
		   	 *
			 *	^ /____
			 *	|/_____
			 *	o______
			 *	|\_____
			 *	| \____
    			 */
			edge_ray_graze(a_hit, seg_p, i_u,
				RIGHT_MIN_ENTER, RIGHT_MAX_EXIT, "right");
			filled = 2;
	    } else {
    			/* How did this happen? */
    			edge_confusion(a_hit, i_u, __FILE__, __LINE__);
			rt_bomb("goodbye\n");

			RT_LIST_DEQUEUE(&a_hit->l);
			rt_free((char *)a_hit, "freeing hitpoint");
	    }

	} else if (manifolds & NMG_2MANIFOLD) {
	    if (filled == 0) {
		/* hit a 2-manifold in space ( dangling face ) */

		bcopy(&a_hit->hit, &seg_p->seg_in, sizeof(struct hit));
		bcopy(&a_hit->hit, &seg_p->seg_out, sizeof(struct hit));

		eu_p = e_p->eu_p;

		/* go looking for a use of this edge in a 2-Manifold */

		while ( ! (NMG_MANIFOLDS(tbl, eu_p) & NMG_2MANIFOLD) &&
		    eu_p->radial_p->eumate_p != e_p->eu_p)
			eu_p = eu_p->radial_p->eumate_p;

		if (NMG_MANIFOLDS(tbl, eu_p) & NMG_2MANIFOLD ) {
			vect_t	normal;
			NMG_GET_FU_NORMAL( normal, eu_p->up.lu_p->up.fu_p );

			if (VDOT(normal, rp->r_dir) > 0.0) {
				VMOVE(seg_p->seg_out.hit_normal, normal);
				VREVERSE(seg_p->seg_in.hit_normal, normal);
			} else {
				VMOVE(seg_p->seg_in.hit_normal, normal);
				VREVERSE(seg_p->seg_out.hit_normal, normal);
			}
		}
		
	    }
	    RT_LIST_DEQUEUE(&a_hit->l);
	    rt_free((char *)a_hit, "freeing hitpoint");

	} else if (manifolds & NMG_1MANIFOLD) {
	    if (filled == 0) {
		filled = wire_hit(e_p, seg_p, rp, a_hit);
	    }
	    RT_LIST_DEQUEUE(&a_hit->l);
	    rt_free((char *)a_hit, "freeing hitpoint");
	}

	return(filled);
}

/*
 *	if we hit a 3 manifold face for seg_in, we're entering a solid
 *	if we hit a 3 manifold face for seg_out, we're leaving a solid
 *	if we hit a 2 manifold face for seg_in, we're entering/leaving a solid
 *	if we hit a 2 manifold face for seg_out, it's ignored
 */
static int
face_hit(f_p, seg_p, rp, tbl, a_hit, filled)
struct face	*f_p;
struct seg	*seg_p;
struct xray	*rp;
char		*tbl;
struct hitmiss	*a_hit;
int		filled;
{

	char manifolds = NMG_MANIFOLDS(tbl, f_p);
	vect_t	normal;

	RT_CK_SEG(seg_p);

	if( f_p->flip ) {
		VREVERSE( normal, f_p->fg_p->N );
	} else {
		VMOVE( normal, f_p->fg_p->N );
	}

	if (manifolds & NMG_3MANIFOLD) {
		if (filled == 0) {
			/* entering solid */
			bcopy(&a_hit->hit, &seg_p->seg_in, sizeof(struct hit));
			VMOVE(seg_p->seg_in.hit_normal, normal);
			filled = 1;
		} else {
			/* leaving solid */
			bcopy(&a_hit->hit, &seg_p->seg_out, sizeof(struct hit));
			VMOVE(seg_p->seg_out.hit_normal, normal);
			filled = 2;
		}

		if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
			rt_log("added %d to segment: ", filled);
			rt_pr_seg(seg_p);
 		}

	} else if (filled == 0) {
		/* just hit an exterior dangling face */

		bcopy(&a_hit->hit, &seg_p->seg_in, sizeof(struct hit));
		bcopy(&a_hit->hit, &seg_p->seg_out, sizeof(struct hit));
		if (VDOT(normal, rp->r_dir) <= 0.0) {
			/* face normal points back along ray */
			VMOVE(seg_p->seg_in.hit_normal, normal);
			VMOVE(seg_p->seg_out.hit_normal, normal);
		} else {
			VREVERSE(seg_p->seg_in.hit_normal, normal);
			VREVERSE(seg_p->seg_out.hit_normal, normal);
		}

		filled = 2;
		if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
			rt_log("added %d to segment: ", filled);
			rt_pr_seg(seg_p);
		}
	}

	RT_LIST_DEQUEUE(&a_hit->l);
	rt_free((char *)a_hit, "freeing hitpoint");

	return(filled);
}

/*
 *	Faced with an apparent conflict in the ray state, see if we can
 *	reconcile the conflict.
 *
 *	If we can reconcile the conflict
 *		return a non-zero,
 *	otherwise
 *		return zero.
 */
static int
reconcile(prev_hit, curr_hit)
struct hitmiss *curr_hit;
struct hitmiss *prev_hit;
{
	struct faceuse *fu;

	NMG_CK_HITMISS(curr_hit);

	/* if we have a conflict on the very first hit, there is nothing 
	 * to do about it;
	 */
	if (!prev_hit)
		return 0;

	/* If we have a conflict between a hit on a face, and a hit on an
	 * edge or vertex of that face, then we can resolve the issue.
	 */
	if ( *prev_hit->outbound_use == NMG_EDGEUSE_MAGIC &&
	    *curr_hit->inbound_use == NMG_FACEUSE_MAGIC) {
		fu=nmg_find_fu_of_eu((struct edgeuse *)prev_hit->outbound_use);
		if ((long *)fu == curr_hit->inbound_use ||
		    (long *)fu->fumate_p == curr_hit->inbound_use)
			return 1;
	} else if (*curr_hit->inbound_use == NMG_EDGEUSE_MAGIC &&
	    *prev_hit->outbound_use == NMG_FACEUSE_MAGIC) {
		fu=nmg_find_fu_of_eu((struct edgeuse *)curr_hit->inbound_use);
		if ((long *)fu == prev_hit->outbound_use ||
		    (long *)fu->fumate_p == prev_hit->outbound_use)
			return 1;
	} else if ( *prev_hit->outbound_use == NMG_VERTEXUSE_MAGIC &&
	    *curr_hit->inbound_use == NMG_FACEUSE_MAGIC) {
		fu=nmg_find_fu_of_vu((struct vertexuse *)prev_hit->outbound_use);
		if ((long *)fu == curr_hit->inbound_use ||
		    (long *)fu->fumate_p == curr_hit->inbound_use)
			return 1;
	} else if (*curr_hit->outbound_use == NMG_VERTEXUSE_MAGIC &&
	    *prev_hit->inbound_use == NMG_FACEUSE_MAGIC) {
		fu=nmg_find_fu_of_vu((struct vertexuse *)curr_hit->inbound_use);
		if ((long *)fu == prev_hit->outbound_use ||
		    (long *)fu->fumate_p == prev_hit->outbound_use)
			return 1;
	}
	return 0;
}


#define MAKE_SEG(in_hit, out_hit) \
	RT_GET_SEG(seg_p, ap->a_resource); \
	RT_CK_SEG(seg_p); \
	seg_p->seg_stp = stp; \
	bcopy(&in_hit->hit, &seg_p->seg_in, sizeof(struct hit)); \
	bcopy(&out_hit->hit, &seg_p->seg_out, sizeof(struct hit)); \
	seg_count++; \
	RT_LIST_INSERT(&(seghead->l), &(seg_p->l) )



static int
nmg_bsegs(rd, ap, seghead, stp)
struct ray_data		*rd;
struct application	*ap;
struct seg		*seghead;	/* intersection w/ ray */
struct soltab		*stp;
{
	int ray_state = NMG_RAY_STATE_OUTSIDE;
	struct hitmiss *in_hit = (struct hitmiss *)NULL;
	struct hitmiss *a_hit;
	int seg_count = 0;
	struct seg *seg_p;

	NMG_CK_HITMISS_LISTS(a_hit, rd);


	while (RT_LIST_NON_EMPTY(&rd->rd_hit) ) {

		a_hit = RT_LIST_FIRST(hitmiss, &rd->rd_hit);
		NMG_CK_HITMISS(a_hit);
		RT_LIST_DEQUEUE( &a_hit->l );

		if (ray_state == NMG_RAY_STATE_OUTSIDE) {
			switch (a_hit->in_out) {
			case HMG_HIT_OUT_IN:
			case HMG_HIT_OUT_ON:
				in_hit = a_hit;
				break;
			case HMG_HIT_ANY_ANY:
				/* Hit a Non-3-Manifold */
			case HMG_HIT_OUT_OUT:
				if (in_hit)
					rt_bomb("What should I have done?\n");

				/* build the seg */
				MAKE_SEG(a_hit, a_hit);

				ray_state = NMG_RAY_STATE_INSIDE;
				break;
			case HMG_HIT_IN_IN:
			case HMG_HIT_IN_ON:
			case HMG_HIT_ON_IN:
			case HMG_HIT_ON_ON:
				if (!in_hit)
					rt_bomb("How did I get in?\n");
				/* if we're already inside, just eat this */
				break;
			case HMG_HIT_IN_OUT:
			case HMG_HIT_ON_OUT:
				if (!in_hit)
					rt_bomb("How can I be leaving when I haven't entered?\n");
				/* build the seg */
				MAKE_SEG(in_hit, a_hit);

				in_hit = (struct hitmiss *)NULL;
				ray_state = NMG_RAY_STATE_INSIDE;
				break;
			default:
				rt_bomb("Bogus ray state while leaving\n");
				break;
			}
		} else { /* ray_state == NMG_RAY_STATE_INSIDE */
			switch (a_hit->in_out) {
			case HMG_HIT_IN_OUT:
			case HMG_HIT_ON_OUT:
				/* "eat" the hit point */
				break;
			case HMG_HIT_IN_IN:
			case HMG_HIT_IN_ON:
			case HMG_HIT_ON_IN:
			case HMG_HIT_ON_ON:
				rt_bomb("I've already left the NMG and this guy wants to stay inside\n");
				break;
			case HMG_HIT_OUT_IN:
			case HMG_HIT_OUT_ON:
				ray_state = NMG_RAY_STATE_OUTSIDE;
				in_hit = a_hit;
				break;

			case HMG_HIT_ANY_ANY:
			case HMG_HIT_OUT_OUT:
				ray_state = NMG_RAY_STATE_OUTSIDE;
				/* build the seg */
				MAKE_SEG(a_hit, a_hit);
				break;
			default:
				rt_bomb("Bogus ray state while leaving\n");
				break;
			}
		}
	}


	while ( RT_LIST_NON_EMPTY(&rd->rd_miss) ) {
		a_hit = RT_LIST_FIRST(hitmiss, &rd->rd_miss);
		NMG_CK_HITMISS(a_hit);
		RT_LIST_DEQUEUE( &a_hit->l );
		if (rt_g.NMG_debug & DEBUG_RT_SEGS)
			rt_log("freeing miss point\n");
		rt_free((char *)a_hit, "freeing miss point");
	}

	return seg_count;
}

static int
nmg_build_segs(rd, ap, seghead, stp)
struct ray_data		*rd;
struct application	*ap;
struct seg		*seghead;	/* intersection w/ ray */
struct soltab		*stp;
{

	struct seg *seg_p;
	int seg_count=0;
	struct hitmiss *curr_hit;
	struct hitmiss *prev_hit;
	char *tbl;
	int hits_filled;
	static struct hitmiss dummy;
		


	tbl = rd->rd_m->manifolds;

	/* build up the list of segments based upon the hit points.
	 */

	curr_hit = (struct hitmiss *)NULL;
	hits_filled = 0;

	while (RT_LIST_NON_EMPTY(&rd->rd_hit) ) {

		prev_hit = curr_hit;
		curr_hit = RT_LIST_FIRST(hitmiss, &rd->rd_hit);
		NMG_CK_HITMISS(curr_hit);
		RT_LIST_DEQUEUE( &curr_hit->l );

/* 					Current Ray State
 * Next Element			Inside		On		Outside
 * ----------------------------------------------------------------------
 * HMG_HIT_IN_IN		OK		?OK		BAD
 * HMG_HIT_IN_OUT		OK		?OK		BAD
 * HMG_HIT_OUT_IN		BAD		BAD		OK
 * HMG_HIT_OUT_OUT		BAD		BAD		OK
 * HMG_HIT_IN_ON		OK		?OK		BAD
 * HMG_HIT_ON_IN		?OK		OK		BAD
 * HMG_HIT_OUT_ON		BAD		BAD		OK
 * HMG_HIT_ON_OUT		?OK		OK		BAD
 * HMG_HIT_ANY_ANY		OK->Inside	OK->On		OK->Outside
 */

		if (!prev_hit || (prev_hit->in_out & 0x0f) == NMG_RAY_STATE_OUTSIDE) {

			switch (curr_hit->in_out) {
			case HMG_HIT_IN_IN   :
			case HMG_HIT_IN_OUT  :
			case HMG_HIT_IN_ON   :
			case HMG_HIT_ON_IN   :
			case HMG_HIT_ON_OUT  :
				rt_log("%s %d: ray state violation\n\tNMG_RAY_STATE_OUTSIDE followed by hit:\n\t",
					__FILE__, __LINE__);
				nmg_rt_print_hitmiss(curr_hit);
				if (reconcile(prev_hit, curr_hit))
					rt_log("---- reconciled ----\n");
				else
					rt_bomb("bombing");
				break;

			case HMG_HIT_OUT_ON  :
				/* fallthrough */
			case HMG_HIT_OUT_IN  :
#if 0
				if (hits_filled != 0)
					rt_bomb("Why hits_filled != 0?\n");
				RT_GET_SEG(seg_p, ap->a_resource);
				RT_CK_SEG(seg_p);
				seg_p->seg_stp = stp;
				bcopy(&curr_hit->hit, seg_p->seg_in,
					sizeof(struct hit));
				hits_filled = 1;
#endif
				break;
			case HMG_HIT_OUT_OUT :
#if 0
				if (hits_filled != 0)
					rt_bomb("Why hits_filled != 0?\n");
				RT_GET_SEG(seg_p, ap->a_resource);
				RT_CK_SEG(seg_p);
				seg_p->seg_stp = stp;
				bcopy(&curr_hit->hit, seg_p->seg_in,
					sizeof(struct hit));
				bcopy(&curr_hit->hit, seg_p->seg_out,
					sizeof(struct hit));
#endif
				break;
			case HMG_HIT_ANY_ANY  :
				/* hits_filled better be 0 */
				/* copy 2 hits */
				/* hits_filled = 0 */
				break;
			default:
				rt_log("%s %d: Bogus in_out value 0x%0x\n",
					__FILE__, __LINE__, curr_hit->in_out);
				break;
			}
		} else if ((prev_hit->in_out & 0x0f) == NMG_RAY_STATE_INSIDE ||
		    (prev_hit->in_out & 0x0f) == NMG_RAY_STATE_ON) {
			switch (curr_hit->in_out) {
			case HMG_HIT_ANY_ANY  :
				/* fallthrough */
			case HMG_HIT_IN_IN   :
				/* copy 2 hits */
				break;
			case HMG_HIT_IN_OUT  :
				/* hits_filled should be 1 */
				/* copy 1 hit */
				break;
			case HMG_HIT_IN_ON   :
				/* copy 2 hits */
				break;
			case HMG_HIT_ON_IN   :
				/* copy 2 hits */
				break;
			case HMG_HIT_ON_OUT  :
				/* hits_filled should be 1 */
				/* copy 1 hit */
				break;

				break;

			case HMG_HIT_OUT_IN  :
			case HMG_HIT_OUT_OUT :
			case HMG_HIT_OUT_ON  :
				switch (prev_hit->in_out & 0x0f) {
				case NMG_RAY_STATE_ON:
					rt_log("%s %d: ray state violation\n\tNMG_RAY_STATE_INSIDE followed by hit:\n\t",
						__FILE__, __LINE__);
					break;
				case NMG_RAY_STATE_INSIDE:
					rt_log("%s %d: ray state violation\n\tNMG_RAY_STATE_INSIDE followed by hit:\n\t",
						__FILE__, __LINE__);
					break;
				}
				nmg_rt_print_hitmiss(curr_hit);
				if (reconcile(prev_hit, curr_hit))
					rt_log("---- reconciled ----\n");
				else
					rt_bomb("bombing");
				break;
			default:
				rt_log("%s %d: Bogus in_out value 0x%0x\n",
					__FILE__, __LINE__, curr_hit->in_out);
				break;
			}
		}

#if 0



state = out;
inpoint = null;

while (get hit) {

    if (state = out)
	switch (in_out)
	case OUT_ON: 
	case OUT_IN: inpoint = hit
		break;
	case IN_IN:
	case ON_IN:
	case IN_ON:
	case ON_ON:	
		if (inpoint)
			/* eat it */
		else
			error
		break;
	case IN_OUT:
	case ON_OUT:
		if (inpoint) {
			build seg
			state = in
		} else error
		break;
	case OUT_OUT
		if (!inpoint)
			build seg on point
		else
			build seg?
    else /* state == in */
	switch (in_out)
	case HMG_HIT_IN_OUT:	/* eat it */
	case HMG_HIT_ON_OUT:	/* eat it */

	case HMG_HIT_OUT_IN:	/* state = out, inpoint = hit */
	case HMG_HIT_OUT_ON:	/* state = out, inpoint = hit */
	case HMG_HIT_OUT_OUT:	/* state = out, build seg on point */
}


		switch (hits_filled) {
		case 1:
		case 2:
			if (!seg_p) {
			RT_GET_SEG(seg_p, ap->a_resource);
			RT_CK_SEG(seg_p);
			seg_p->seg_stp = stp;
			bcopy(&curr_hit->hit, seg_p->seg_in, sizeof(struct hit));

			/* fallthrough */
		case 0:
		}



		

	    ++seg_count;
	    if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
		rt_log("adding segment %d to seg list\n", seg_count);
		rt_pr_seg(seg_p);
	    }

	    RT_LIST_INSERT(&(seghead->l), &(seg_p->l) );
#endif
	}
	return(seg_count);

}


/*	N M G _ R A Y _ S E G S
 *
 *	Obtain the list of ray segments which intersect with the nmg.
 *	This routine does all of the "work" for rt_nmg_shot()
 */
int
nmg_ray_segs(rd)
struct ray_data	*rd;
{
	struct hitmiss *a_hit;
	int seg_count=0;

	NMG_CK_HITMISS_LISTS(a_hit, rd);

	if (RT_LIST_IS_EMPTY(&rd->rd_hit)) {
		if (rt_g.NMG_debug & DEBUG_RT_SEGS)
			rt_log("ray missed NMG\n");
		return(0);			/* MISS */
	} else if (rt_g.NMG_debug & DEBUG_RT_SEGS) {

		rt_log("\nnmg_ray_segs(rd)\nsorted nmg/ray hit list\n");

		for (RT_LIST_FOR(a_hit, hitmiss, &rd->rd_hit))
			nmg_rt_print_hitmiss(a_hit);
	}

	seg_count = nmg_bsegs(rd, rd->ap, rd->seghead, rd->stp);

	if (!(rt_g.NMG_debug & DEBUG_RT_SEGS))
		return(seg_count);

	/* print debugging data before returning */
	print_seg_list(rd->seghead);

	return(seg_count);
}

