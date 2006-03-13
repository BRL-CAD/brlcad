/*                      R A Y T R A C E . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file raytrace.h
 *
 *  All the data structures and manifest constants
 *  necessary for interacting with the BRL-CAD LIBRT ray-tracing library.
 *
 *  Note that this header file defines many internal data structures,
 *  as well as the library's external (interface) data structures.  These are
 *  provided for the convenience of applications builders.  However,
 *  the internal data structures are subject to change in each release.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005 USA
 *
 *  Include Sequencing -
 *	#include "common.h"	/_* Must come before system <> includes *_/
 *	#ifdef HAVE_STRING_H	/_* OPTIONAL, for strcmp() etc. *_/
 *	#  include <string.h>
 *	#else
 *	#  include <strings.h>
 *	#endif
 *	#include <stdio.h>
 *	#include <math.h>
 *	#include "machine.h"	/_* For fastf_t definition on this machine *_/
 *	#include "bu.h"
 *	#include "vmath.h"	/_* For vect_t definition *_/
 *	#include "bn.h"
 *	#include "db.h"		/_* OPTIONAL, precedes raytrace.h when used *_/
 *	#include "nmg.h"	/_* OPTIONAL, precedes raytrace.h when used *_/
 *	#include "raytrace.h"
 *	#include "nurb.h"	/_* OPTIONAL, follows raytrace.h when used *_/
 *
 *  Libraries Used -
 *	LIBRT LIBRT_LIBES -lm -lc
 *
 *  $Header$
 */

#ifndef RAYTRACE_H
#define RAYTRACE_H seen

/*
 *  Auto-include the BRL-CAD Utilities library, and compatability macros
 */
#include "bu.h"
#include "compat4.h"
#include "bn.h"
#include "db5.h"
#include "tcl.h"


__BEGIN_DECLS


#include "rt.h"
#include "nmg.h"


/*
 *			V I E W _ O B J
 *
 * A view object maintains state for controlling a view.
 */
struct view_obj {
	struct bu_list	l;
	struct bu_vls		vo_name;		/* view object name/cmd */
	fastf_t			vo_scale;
	fastf_t			vo_size;		/* 2.0 * scale */
	fastf_t			vo_invSize;		/* 1.0 / size */
	fastf_t			vo_perspective;		/* perspective angle */
	fastf_t			vo_local2base;		/* scale local units to base units (i.e. mm) */
	fastf_t			vo_base2local;		/* scale base units (i.e. mm) to local units */
	vect_t			vo_aet;
	vect_t			vo_eye_pos;		/* eye position */
	vect_t			vo_keypoint;
	char			vo_coord;		/* coordinate system */
	char			vo_rotate_about;	/* indicates what point rotations are about */
	mat_t			vo_rotation;
	mat_t			vo_center;
	mat_t			vo_model2view;
	mat_t			vo_pmodel2view;
	mat_t			vo_view2model;
	mat_t			vo_pmat;		/* perspective matrix */
	struct bu_observer	vo_observers;
	void 			(*vo_callback)();	/* called in vo_update with vo_clientData and vop */
	genptr_t		vo_clientData;		/* passed to vo_callback */
	int			vo_zclip;
};
RT_EXPORT extern struct view_obj HeadViewObj;		/* head of view object list */
#define RT_VIEW_OBJ_NULL		((struct view_obj *)NULL)
#define RT_MINVIEWSIZE 0.0001
#define RT_MINVIEWSCALE 0.00005

/*
 *			A N I M A T E
 *
 *  Each one of these structures specifies an arc in the tree that
 *  is to be operated on for animation purposes.  More than one
 *  animation operation may be applied at any given arc.  The directory
 *  structure points to a linked list of animate structures
 *  (built by rt_anim_add()), and the operations are processed in the
 *  order given.
 */
struct anim_mat {
	int		anm_op;			/* ANM_RSTACK, ANM_RARC... */
	mat_t		anm_mat;		/* Matrix */
};
#define ANM_RSTACK	1			/* Replace stacked matrix */
#define ANM_RARC	2			/* Replace arc matrix */
#define ANM_LMUL	3			/* Left (root side) mul */
#define ANM_RMUL	4			/* Right (leaf side) mul */
#define ANM_RBOTH	5			/* Replace stack, arc=Idn */

struct rt_anim_property {
	long		magic;
	int		anp_op;			/* RT_ANP_REPLACE, etc */
	struct bu_vls	anp_shader;		/* Update string */
};
#define RT_ANP_REPLACE	1			/* Replace shader string */
#define RT_ANP_APPEND	2			/* Append to shader string */
#define RT_ANP_MAGIC	0x41507270		/* 'APrp' */
#define RT_CK_ANP(_p)	BU_CKMAG((_p), RT_ANP_MAGIC, "rt_anim_property")

struct rt_anim_color {
	int		anc_rgb[3];		/* New color */
};

struct animate {
	long		magic;			/* magic number */
	struct animate	*an_forw;		/* forward link */
	struct db_full_path an_path;		/* (sub)-path pattern */
	int		an_type;		/* AN_MATRIX, AN_COLOR... */
	union animate_specific {
		struct anim_mat		anu_m;
		struct rt_anim_property	anu_p;
		struct rt_anim_color	anu_c;
		float			anu_t;
	}		an_u;
};
#define RT_AN_MATRIX	1			/* Matrix animation */
#define RT_AN_MATERIAL	2			/* Material property anim */
#define RT_AN_COLOR	3			/* Material color anim */
#define RT_AN_SOLID	4			/* Solid parameter anim */
#define RT_AN_TEMPERATURE 5			/* Region temperature */

#define ANIM_NULL	((struct animate *)0)
#define ANIMATE_MAGIC	0x414e4963		/* 1095649635 */
#define RT_CK_ANIMATE(_p)	BU_CKMAG((_p), ANIMATE_MAGIC, "animate")


/*
 *  Replacements for definitions from vmath.h
 */
#undef V2PRINT
#undef VPRINT
#undef HPRINT
#define V2PRINT(a,b)	bu_log("%s (%g, %g)\n", a, (b)[0], (b)[1] );
#define VPRINT(a,b)	bu_log("%s (%g, %g, %g)\n", a, (b)[0], (b)[1], (b)[2])
#define HPRINT(a,b)	bu_log("%s (%g, %g, %g, %g)\n", a, (b)[0], (b)[1], (b)[2], (b)[3])


/*
 *			R T _ P O I N T _ L A B E L S
 *
 *  Used by MGED for labeling vertices of a solid.
 */
struct rt_point_labels {
	char	str[8];
	point_t	pt;
};

/*
 *			R T _ P T _ N O D E
 *
 *  Used by g_rpc.c and others to contain forward-linked lists of points.
 */
struct rt_pt_node {
	point_t			p;	/* a point */
	struct rt_pt_node	*next;	/* ptr to next pt */
};


/*
 *			L I N E _ S E G,  C A R C _ S E G,  N U R B _ S E G
 *	used by the sketch and solid of extrusion
 */

struct line_seg		/* line segment */
{
	long			magic;
	int			start, end;	/* indices into sketch's array of vertices */
};
#define CURVE_LSEG_MAGIC     0x6c736567		/* lseg */

struct carc_seg		/* circular arc segment */
{
	long			magic;
	int			start, end;	/* indices */
	fastf_t			radius;		/* radius < 0.0 -> full circle with start point on
						 * circle and "end" at center */
	int			center_is_left;	/* flag indicating where center of curvature is.
						 * If non-zero, then center is to left of vector
						 * from start to end */
	int			orientation;	/* 0 -> ccw, !0 -> cw */
	int			center;		/* index of vertex at center of arc (only used by rt_extrude_prep and rt_extrude_shot) */
};
#define CURVE_CARC_MAGIC     0x63617263		/* carc */

struct nurb_seg		/* NURB curve segment */
{
	long			magic;
	int			order;		/* order of NURB curve (degree - 1) */
	int			pt_type;	/* type of NURB curve */
	struct knot_vector	k;		/* knot vector for NURB curve */
	int			c_size;		/* number of control points */
	int			*ctl_points;	/* array of indicies for control points */
	fastf_t			*weights;	/* array of weights for control points (NULL if non_rational) */
};
#define CURVE_NURB_MAGIC     0x6e757262		/* nurb */

struct bezier_seg	/* Bezier curve segment */
{
	long			magic;
	int			degree;		/* degree of curve (number of control points - 1) */
	int			*ctl_points;	/* array of indices for control points */
};
#define CURVE_BEZIER_MAGIC	0x62657a69	/* bezi */


/*********************************************************************************
 *	The following section is an exact copy of what was previously "nmg_rt.h" *
 *      (with minor changes to GET_HITMISS and NMG_FREE_HITLIST                  *
 *	moved here to use rt_g.rtg_nmgfree freelist for hitmiss structs.         *
 *********************************************************************************
 *			N M G _ R T . H
 *
 *  Author -
 *	Lee A. Butler
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *  $Header$
 */

/* defining the following flag will improve NMG raytrace speed by eliminating some checking
 * Use with CAUTION!!! */
#define FAST_NMG	1

#define NMG_HIT_LIST	0
#define NMG_MISS_LIST	1
#define NMG_RT_HIT_MAGIC 0x48697400	/* "Hit" */
#define NMG_RT_HIT_SUB_MAGIC 0x48696d00	/* "Him" */
#define NMG_RT_MISS_MAGIC 0x4d697300	/* "Mis" */


/* These values are for the hitmiss "in_out" variable and indicate the
 * nature of the hit when known
 */
#define HMG_INBOUND_STATE(_hm) (((_hm)->in_out & 0x0f0) >> 4)
#define HMG_OUTBOUND_STATE(_hm) ((_hm)->in_out & 0x0f)


#define NMG_RAY_STATE_INSIDE	1
#define NMG_RAY_STATE_ON	2
#define NMG_RAY_STATE_OUTSIDE	4
#define NMG_RAY_STATE_ANY	8

#define HMG_HIT_IN_IN	0x11	/* hit internal structure */
#define HMG_HIT_IN_OUT	0x14	/* breaking out */
#define HMG_HIT_OUT_IN	0x41	/* breaking in */
#define HMG_HIT_OUT_OUT 0x44	/* edge/vertex graze */
#define HMG_HIT_IN_ON	0x12
#define HMG_HIT_ON_IN	0x21
#define HMG_HIT_ON_ON	0x22
#define HMG_HIT_OUT_ON	0x42
#define HMG_HIT_ON_OUT	0x24
#define HMG_HIT_ANY_ANY	0x88	/* hit on non-3-mainifold */

#define	NMG_VERT_ENTER 1
#define NMG_VERT_ENTER_LEAVE 0
#define NMG_VERT_LEAVE -1
#define NMG_VERT_UNKNOWN -2

#define NMG_HITMISS_SEG_IN 0x696e00	/* "in" */
#define NMG_HITMISS_SEG_OUT 0x6f757400	/* "out" */

struct hitmiss {
	struct bu_list	l;
	struct hit	hit;
	fastf_t		dist_in_plane;	/* distance from plane intersect */
	int		in_out;		/* status of ray as it transitions
					 * this hit point.
					 */
	long		*inbound_use;
	vect_t		inbound_norm;
	long		*outbound_use;
	vect_t		outbound_norm;
	int		start_stop;	/* is this a seg_in or seg_out */
	struct hitmiss	*other;		/* for keeping track of the other
					 * end of the segment when we know
					 * it
					 */
};

#define NMG_CK_HITMISS(hm) {\
	switch (hm->l.magic) { \
	case NMG_RT_HIT_MAGIC: \
	case NMG_RT_HIT_SUB_MAGIC: \
	case NMG_RT_MISS_MAGIC: \
		break; \
	case NMG_MISS_LIST: \
		bu_log("%s[%d]: struct hitmiss has  NMG_MISS_LIST magic #\n",\
			__FILE__, __LINE__); \
		rt_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	case NMG_HIT_LIST: \
		bu_log("%s[%d]: struct hitmiss has  NMG_MISS_LIST magic #\n",\
			__FILE__, __LINE__); \
		rt_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	default: \
		bu_log("%s[%d]: bad struct hitmiss magic: %d:(0x%08x)\n", \
			__FILE__, __LINE__, hm->l.magic, hm->l.magic); \
		rt_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	}\
	if (!hm->hit.hit_private) { \
		bu_log("%s[%d]: NULL hit_private in hitmiss struct\n", \
			__FILE__, __LINE__); \
		rt_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	} \
}

#define NMG_CK_HITMISS_LISTS(a_hit, rd) { \
    for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)){NMG_CK_HITMISS(a_hit);} \
    for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_miss)){NMG_CK_HITMISS(a_hit);} }


/*	Ray Data structure
 *
 * A)	the hitmiss table has one element for each nmg structure in the
 *	nmgmodel.  The table keeps track of which elements have been
 *	processed before and which haven't.  Elements in this table
 *	will either be:
 *		(NULL)		item not previously processed
 *		hitmiss ptr	item previously processed
 *
 *	the 0th item in the array is a pointer to the head of the "hit"
 *	list.  The 1th item in the array is a pointer to the head of the
 *	"miss" list.
 *
 * B)	If plane_pt is non-null then we are currently processing a face
 *	intersection.  The plane_dist and ray_dist_to_plane are valid.
 *	The ray/edge intersector should check the distance from the plane
 *	intercept to the edge and update "plane_closest" if the current
 *	edge is closer to the intercept than the previous closest object.
 */
#define NMG_PCA_EDGE	1
#define NMG_PCA_EDGE_VERTEX 2
#define NMG_PCA_VERTEX 3
#define NMG_RAY_DATA_MAGIC 0x1651771
#define NMG_CK_RD(_rd) NMG_CKMAG(_rd, NMG_RAY_DATA_MAGIC, "ray data");
struct ray_data {
	long magic;
	struct model		*rd_m;
	char			*manifolds; /*  structure 1-3manifold table */
	vect_t			rd_invdir;
	struct xray		*rp;
	struct application	*ap;
	struct seg		*seghead;
	struct soltab 		*stp;
	const struct bn_tol	*tol;
	struct hitmiss	**hitmiss;	/* 1 struct hitmiss ptr per elem. */
	struct bu_list	rd_hit;		/* list of hit elements */
	struct bu_list	rd_miss;	/* list of missed/sub-hit elements */

/* The following are to support isect_ray_face() */

	/* plane_pt is the intercept point of the ray with the plane of the
	 * face.
	 */
	point_t	plane_pt;	/* ray/plane(face) intercept point */

	/* ray_dist_to_plane is the parametric distance along the ray from
	 * the ray origin (rd->rp->r_pt) to the ray/plane intercept point
	 */
	fastf_t		ray_dist_to_plane; /* ray parametric dist to plane */

	/* the "face_subhit" element is a boolean used by isect_ray_face
	 * and [e|v]u_touch_func to record the fact that the ray/(plane/face)
	 * intercept point was within tolerance of an edge/vertex of the face.
	 * In such instances, isect_ray_face does NOT need to generate a hit
	 * point for the face, as the hit point for the edge/vertex will
	 * suffice.
	 */
	int		face_subhit;

	/* the "classifying_ray" flag indicates that this ray is being used to
	 * classify a point, so that the "eu_touch" and "vu_touch" functions
	 * should not be called.
	 */
	int		classifying_ray;
};


#ifdef FAST_NMG
#define GET_HITMISS(_p, _ap) { \
	(_p) = BU_LIST_FIRST( hitmiss, &((_ap)->a_resource->re_nmgfree) ); \
	if( BU_LIST_IS_HEAD( (_p), &((_ap)->a_resource->re_nmgfree ) ) ) \
		(_p) = (struct hitmiss *)bu_calloc(1, sizeof( struct hitmiss ), "hitmiss" ); \
	else \
		BU_LIST_DEQUEUE( &((_p)->l) ); \
	}

#define NMG_FREE_HITLIST(_p, _ap) { \
	BU_CK_LIST_HEAD( (_p) ); \
	BU_LIST_APPEND_LIST( &((_ap)->a_resource->re_nmgfree), (_p) ); \
	}
#else
#define GET_HITMISS(_p) { \
	char str[64]; \
	(void)sprintf(str, "GET_HITMISS %s %d", __FILE__, __LINE__); \
	(_p) = (struct hitmiss *)bu_calloc(1, sizeof(struct hitmiss), str); \
	}

#define FREE_HITMISS(_p) { \
	char str[64]; \
	(void)sprintf(str, "FREE_HITMISS %s %d", __FILE__, __LINE__); \
	(void)bu_free( (char *)_p,  str); \
	}

#define NMG_FREE_HITLIST(_p) { \
	struct hitmiss *_hit; \
	while ( BU_LIST_WHILE(_hit, hitmiss, _p)) { \
		NMG_CK_HITMISS(_hit); \
		BU_LIST_DEQUEUE( &_hit->l ); \
		FREE_HITMISS( _hit ); \
	} }

#endif

#define HIT 1	/* a hit on a face */
#define MISS 0	/* a miss on the face */


#ifdef NO_BOMBING_MACROS
#  define nmg_rt_bomb(rd, str)
#else
#  define nmg_rt_bomb(rd, str) { \
	bu_log("%s", str); \
	if (rt_g.NMG_debug & DEBUG_NMGRT) rt_bomb("End of diagnostics"); \
	BU_LIST_INIT(&rd->rd_hit); \
	BU_LIST_INIT(&rd->rd_miss); \
	rt_g.NMG_debug |= DEBUG_NMGRT; \
	nmg_isect_ray_model(rd); \
	(void) nmg_ray_segs(rd); \
	rt_bomb("Should have bombed before this\n"); }
#endif


struct nmg_radial {
	struct bu_list	l;
	struct edgeuse	*eu;
	struct faceuse	*fu;		/* Derrived from eu */
	struct shell	*s;		/* Derrived from eu */
	int		existing_flag;	/* !0 if this eu exists on dest edge */
	int		is_crack;	/* This eu is part of a crack. */
	int		is_outie;	/* This crack is an "outie" */
	int		needs_flip;	/* Insert eumate, not eu */
	fastf_t		ang;		/* angle, in radians.  0 to 2pi */
};
#define NMG_RADIAL_MAGIC	0x52614421	/* RaD! */
#define NMG_CK_RADIAL(_p)	NMG_CKMAG(_p, NMG_RADIAL_MAGIC, "nmg_radial")

struct nmg_inter_struct {
	long		magic;
	struct bu_ptbl	*l1;		/* vertexuses on the line of */
	struct bu_ptbl *l2;		/* intersection between planes */
	fastf_t		*mag1;		/* Distances along intersection line */
	fastf_t		*mag2;		/* for each vertexuse in l1 and l2. */
	int		mag_len;	/* Array size of mag1 and mag2 */
	struct shell	*s1;
	struct shell	*s2;
	struct faceuse	*fu1;		/* null if l1 comes from a wire */
	struct faceuse	*fu2;		/* null if l2 comes from a wire */
	struct bn_tol	tol;
	int		coplanar;	/* a flag */
	struct edge_g_lseg	*on_eg;		/* edge_g for line of intersection */
	point_t		pt;		/* 3D line of intersection */
	vect_t		dir;
	point_t		pt2d;		/* 2D projection of isect line */
	vect_t		dir2d;
	fastf_t		*vert2d;	/* Array of 2d vertex projections [index] */
	int		maxindex;	/* size of vert2d[] */
	mat_t		proj;		/* Matrix to project onto XY plane */
	const long	*twod;		/* ptr to face/edge of 2d projection */
};
#define NMG_INTER_STRUCT_MAGIC	0x99912120
#define NMG_CK_INTER_STRUCT(_p)	NMG_CKMAG(_p, NMG_INTER_STRUCT_MAGIC, "nmg_inter_struct")


/*
 *			R T _ F U N C T A B
 *
 *  Object-oriented interface to BRL-CAD geometry.
 *
 *  These are the methods for a notional object class "brlcad_solid".
 *  The data for each instance is found separately in struct soltab.
 *  This table is indexed by ID_xxx value of particular solid
 *  found in st_id, or directly pointed at by st_meth.
 *
 *  This needs to be at the end of the raytrace.h header file,
 *  so that all the structure names are known.
 *  The "union record" and "struct nmgregion" pointers are problematic,
 *  so generic pointers are used when those header files have not yet
 *  been seen.
 *
 *  XXX On SGI, can not use identifiers in prototypes inside structure!
 */
struct rt_functab {
	long	magic;
	char	ft_name[16];
	char	ft_label[8];
	int	ft_use_rpp;
	int	(*ft_prep) BU_ARGS((struct soltab * /*stp*/,
			struct rt_db_internal * /*ip*/,
			struct rt_i * /*rtip*/ ));
	int 	(*ft_shot) BU_ARGS((struct soltab * /*stp*/,
			struct xray * /*rp*/,
			struct application * /*ap*/,	/* has resource */
			struct seg * /*seghead*/ ));
	void	(*ft_print) BU_ARGS((const struct soltab * /*stp*/));
	void	(*ft_norm) BU_ARGS((struct hit * /*hitp*/,
			struct soltab * /*stp*/,
			struct xray * /*rp*/));
	int 	(*ft_piece_shot) BU_ARGS((
			struct rt_piecestate * /*psp*/,
			struct rt_piecelist * /*plp*/,
			double /* dist_correction to apply to hit distances */,
			struct xray * /* ray transformed to be near cut cell */,
			struct application * /*ap*/,	/* has resource */
			struct seg * /*seghead*/));	/* used only for PLATE mode hits */
	void 	(*ft_piece_hitsegs) BU_ARGS((
			struct rt_piecestate * /*psp*/,
			struct seg * /*seghead*/,
			struct application * /*ap*/));	/* has resource */
	void	(*ft_uv) BU_ARGS((struct application * /*ap*/,	/* has resource */
			struct soltab * /*stp*/,
			struct hit * /*hitp*/,
			struct uvcoord * /*uvp*/));
	void	(*ft_curve) BU_ARGS((struct curvature * /*cvp*/,
			struct hit * /*hitp*/,
			struct soltab * /*stp*/));
	int	(*ft_classify) BU_ARGS((
			const struct soltab * /*stp*/,
			const vect_t /*min*/,
			const vect_t /*max*/,
			const struct bn_tol * /*tol*/));
	void	(*ft_free) BU_ARGS((struct soltab * /*stp*/));
	int	(*ft_plot) BU_ARGS((
			struct bu_list * /*vhead*/,
			struct rt_db_internal * /*ip*/,
			const struct rt_tess_tol * /*ttol*/,
			const struct bn_tol * /*tol*/));
	void	(*ft_vshot) BU_ARGS((struct soltab * /*stp*/[],
			struct xray *[] /*rp*/,
			struct seg [] /*segp*/, int /*n*/,
			struct application * /*ap*/ ));
	int	(*ft_tessellate) BU_ARGS((
			struct nmgregion ** /*r*/,
			struct model * /*m*/,
			struct rt_db_internal * /*ip*/,
			const struct rt_tess_tol * /*ttol*/,
			const struct bn_tol * /*tol*/));
	int	(*ft_tnurb) BU_ARGS((
			struct nmgregion ** /*r*/,
			struct model * /*m*/,
			struct rt_db_internal * /*ip*/,
			const struct bn_tol * /*tol*/));
	int	(*ft_import5) BU_ARGS((struct rt_db_internal * /*ip*/,
			const struct bu_external * /*ep*/,
			const mat_t /*mat*/,
			const struct db_i * /*dbip*/,
			struct resource * /*resp*/,
			const int minor_type));
	int	(*ft_export5) BU_ARGS((struct bu_external * /*ep*/,
			const struct rt_db_internal * /*ip*/,
			double /*local2mm*/,
			const struct db_i * /*dbip*/,
			struct resource * /*resp*/,
			const int minor_type));
	int	(*ft_import) BU_ARGS((struct rt_db_internal * /*ip*/,
			const struct bu_external * /*ep*/,
			const mat_t /*mat*/,
			const struct db_i * /*dbip*/,
			struct resource * /*resp*/));
	int	(*ft_export) BU_ARGS((struct bu_external * /*ep*/,
			const struct rt_db_internal * /*ip*/,
			double /*local2mm*/,
			const struct db_i * /*dbip*/,
			struct resource * /*resp*/));
	void	(*ft_ifree) BU_ARGS((struct rt_db_internal * /*ip*/,
			struct resource * /*resp*/));
	int	(*ft_describe) BU_ARGS((struct bu_vls * /*str*/,
			const struct rt_db_internal * /*ip*/,
			int /*verbose*/,
			double /*mm2local*/,
			struct resource * /*resp*/,
			struct db_i *));
	int	(*ft_xform) BU_ARGS((struct rt_db_internal * /*op*/,
			const mat_t /*mat*/, struct rt_db_internal * /*ip*/,
			int /*free*/, struct db_i * /*dbip*/,
			struct resource * /*resp*/));
	const struct bu_structparse *ft_parsetab;	/* rt_xxx_parse */
	size_t	ft_internal_size;	/* sizeof(struct rt_xxx_internal) */
	unsigned long	ft_internal_magic;	/* RT_XXX_INTERNAL_MAGIC */
#if defined(TCL_OK)
	int	(*ft_tclget) BU_ARGS((Tcl_Interp *,
			const struct rt_db_internal *, const char *item));
	int	(*ft_tcladjust) BU_ARGS((Tcl_Interp *,
			struct rt_db_internal *,
			int /*argc*/, char ** /*argv*/,
			struct resource * /*resp*/));
	int	(*ft_tclform) BU_ARGS((const struct rt_functab *,
			Tcl_Interp *));
#else
	int	(*ft_tclget) BU_ARGS((genptr_t /*interp*/,
			const struct rt_db_internal *, const char *item));
	int	(*ft_tcladjust) BU_ARGS((genptr_t /*interp*/,
			struct rt_db_internal *,
			int /*argc*/, char ** /*argv*/,
			struct resource * /*resp*/));
	int	(*ft_tclform) BU_ARGS((const struct rt_functab *,
			genptr_t /*interp*/));
#endif
	void	(*ft_make) BU_ARGS((const struct rt_functab *,
			struct rt_db_internal *, double /*diameter*/));
};
RT_EXPORT extern const struct rt_functab rt_functab[];
RT_EXPORT extern const int rt_nfunctab;
#define RT_FUNCTAB_MAGIC		0x46754e63	/* FuNc */
#define RT_CK_FUNCTAB(_p)	BU_CKMAG(_p, RT_FUNCTAB_MAGIC, "functab" );

#define RT_CLASSIFY_UNIMPLEMENTED	BN_CLASSIFY_UNIMPLEMENTED
#define RT_CLASSIFY_INSIDE		BN_CLASSIFY_INSIDE
#define RT_CLASSIFY_OVERLAPPING		BN_CLASSIFY_OVERLAPPING
#define RT_CLASSIFY_OUTSIDE		BN_CLASSIFY_OUTSIDE





/* The database library */

/* wdb.c */
RT_EXPORT BU_EXTERN(struct rt_wdb *wdb_fopen,
		    (const char *filename));
RT_EXPORT BU_EXTERN(struct rt_wdb *wdb_fopen_v,
		    (const char *filename,
		     int version));
RT_EXPORT BU_EXTERN(struct rt_wdb *wdb_dbopen,
		    (struct db_i *dbip,
		     int mode));
RT_EXPORT BU_EXTERN(int wdb_import,
		    (struct rt_wdb *wdbp,
		     struct rt_db_internal *internp,
		     const char *name,
		     const mat_t mat));
RT_EXPORT BU_EXTERN(int wdb_export_external,
		    (struct rt_wdb *wdbp,
		     struct bu_external *ep,
		     const char *name,
		     int flags,
		     unsigned char minor_type));
RT_EXPORT BU_EXTERN(int wdb_put_internal,
		    (struct rt_wdb *wdbp,
		     const char *name,
		     struct rt_db_internal *ip,
		     double local2mm));
RT_EXPORT BU_EXTERN(int wdb_export,
		    (struct rt_wdb *wdbp,
		     const char *name,
		     genptr_t gp,
		     int id,
		     double local2mm));
RT_EXPORT BU_EXTERN(void wdb_close,
		    (struct rt_wdb *wdbp));

/* db_anim.c */
RT_EXPORT BU_EXTERN(struct animate *db_parse_1anim,
		    (struct db_i     *dbip,
		     int             argc,
		     const char      **argv));

RT_EXPORT BU_EXTERN(int db_parse_anim,
		    (struct db_i     *dbip,
		     int             argc,
		     const char      **argv));



RT_EXPORT BU_EXTERN(int db_add_anim,
		    (struct db_i *dbip,
		     struct animate *anp,
		     int root));
RT_EXPORT BU_EXTERN(int db_do_anim,
		    (struct animate *anp,
		     mat_t stack,
		     mat_t arc,
		     struct mater_info *materp));
RT_EXPORT BU_EXTERN(void db_free_anim,
		    (struct db_i *dbip));
RT_EXPORT BU_EXTERN(void db_write_anim,
		    (FILE *fop,
		     struct animate *anp));
RT_EXPORT BU_EXTERN(struct animate *db_parse_1anim,
		    (struct db_i *dbip,
		     int argc,
		     const char **argv));
RT_EXPORT BU_EXTERN(void db_free_1anim,
		    (struct animate *anp));

/* db_path.c */
RT_EXPORT BU_EXTERN(void db_full_path_init,
		    (struct db_full_path *pathp));
RT_EXPORT BU_EXTERN(void db_add_node_to_full_path,
		    (struct db_full_path *pp,
		     struct directory *dp));
RT_EXPORT BU_EXTERN(void db_dup_full_path,
		    (struct db_full_path *newp,
		     const struct db_full_path *oldp));
RT_EXPORT BU_EXTERN(void db_extend_full_path,
		    (struct db_full_path *pathp,
		     int incr));
RT_EXPORT BU_EXTERN(void db_append_full_path,
		    (struct db_full_path *dest,
		     const struct db_full_path *src));
RT_EXPORT BU_EXTERN(void db_dup_path_tail,
		    (struct db_full_path	*newp,
		     const struct db_full_path	*oldp,
		     int			start));
RT_EXPORT BU_EXTERN(char *db_path_to_string,
		    (const struct db_full_path *pp));
RT_EXPORT BU_EXTERN(void db_path_to_vls,
		    (struct bu_vls *str,
		     const struct db_full_path *pp));
RT_EXPORT BU_EXTERN(void db_pr_full_path,
		    (const char *msg,
		     const struct db_full_path *pathp));
RT_EXPORT BU_EXTERN(int db_string_to_path,
		    (struct db_full_path *pp,
		     const struct db_i *dbip,
		     const char *str));
RT_EXPORT BU_EXTERN(int db_argv_to_path,
		    (register struct db_full_path	*pp,
		     struct db_i			*dbip,
		     int				argc,
		     const char			*const*argv));
RT_EXPORT BU_EXTERN(void db_free_full_path,
		    (struct db_full_path *pp));
RT_EXPORT BU_EXTERN(int db_identical_full_paths,
		    ( const struct db_full_path *a,
		      const struct db_full_path *b));
RT_EXPORT BU_EXTERN(int db_full_path_subset,
		    (const struct db_full_path *a,
		     const struct db_full_path *b));
RT_EXPORT BU_EXTERN(int db_full_path_search,
		    (const struct db_full_path *a,
		     const struct directory *dp));


/* db_open.c */
RT_EXPORT BU_EXTERN(void db_sync,
		    (struct db_i	*dbip));

/* open an existing model database */
RT_EXPORT BU_EXTERN(struct db_i *db_open,
		    (const char *name,
		     const char *mode));
/* create a new model database */
RT_EXPORT BU_EXTERN(struct db_i *db_create,
		    (const char *name,
		     int version));
/* close a model database */
RT_EXPORT BU_EXTERN(void db_close_client,
		    (struct db_i *dbip,
		     long *client));
RT_EXPORT BU_EXTERN(void db_close,
		    (struct db_i *dbip));
/* dump a full copy of a database */
RT_EXPORT BU_EXTERN(int db_dump,
		    (struct rt_wdb *wdbp,
		     struct db_i *dbip));
RT_EXPORT BU_EXTERN(struct db_i *db_clone_dbi,
		    (struct db_i *dbip,
		     long *client));
/* db5_alloc.c */

RT_EXPORT BU_EXTERN(int db5_write_free,
		    (struct db_i *dbip,
		     struct directory *dp,
		     long length));
RT_EXPORT BU_EXTERN(int db5_realloc,
		    (struct db_i *dbip,
		     struct directory *dp,
		     struct bu_external *ep));

/* db5_io.c */
RT_EXPORT BU_EXTERN(void db5_export_object3,
		    (struct bu_external *out,
		     int			dli,
		     const char			*name,
		     const unsigned char	hidden,
		     const struct bu_external	*attrib,
		     const struct bu_external	*body,
		     int			major,
		     int			minor,
		     int			a_zzz,
		     int			b_zzz));
RT_EXPORT BU_EXTERN(int rt_db_cvt_to_external5,
		    (struct bu_external *ext,
		     const char *name,
		     const struct rt_db_internal *ip,
		     double conv2mm,
		     struct db_i *dbip,
		     struct resource *resp,
		     const int major));

RT_EXPORT BU_EXTERN(int db_wrap_v5_external,
		    (struct bu_external *ep,
		     const char *name));

RT_EXPORT BU_EXTERN(int rt_db_get_internal5,
		    (struct rt_db_internal	*ip,
		     const struct directory	*dp,
		     const struct db_i	*dbip,
		     const mat_t		mat,
		     struct resource		*resp));
RT_EXPORT BU_EXTERN(int rt_db_put_internal5,
		    (struct directory	*dp,
		     struct db_i		*dbip,
		     struct rt_db_internal	*ip,
		     struct resource		*resp,
		     const int		major));

RT_EXPORT BU_EXTERN(void db5_make_free_object_hdr,
		    (struct bu_external *ep,
		     long length));
RT_EXPORT BU_EXTERN(void db5_make_free_object,
		    (struct bu_external *ep,
		     long length));
RT_EXPORT BU_EXTERN(int db5_decode_signed,
		    (long			*lenp,
		     const unsigned char	*cp,
		     int			format));

RT_EXPORT BU_EXTERN(int db5_decode_length,
		    (long			*lenp,
		     const unsigned char	*cp,
		     int			format));

RT_EXPORT BU_EXTERN(int db5_select_length_encoding,
		    (long len));

RT_EXPORT BU_EXTERN(void db5_import_color_table,
		    (char *cp));

RT_EXPORT BU_EXTERN(int db5_import_attributes,
		    (struct bu_attribute_value_set *avs,
		     const struct bu_external *ap));

RT_EXPORT BU_EXTERN(void db5_export_attributes,
		    (struct bu_external *ap,
		     const struct bu_attribute_value_set *avs));

RT_EXPORT BU_EXTERN(int db5_get_raw_internal_fp,
		    (struct db5_raw_internal	*rip,
		     FILE			*fp));

RT_EXPORT BU_EXTERN(int db5_header_is_valid,
		    (const unsigned char *hp));
#define rt_fwrite_internal5	+++__deprecated_rt_fwrite_internal5__+++
RT_EXPORT BU_EXTERN(int db5_fwrite_ident,
		    (FILE *,
		     const char *,
		     double));

RT_EXPORT BU_EXTERN(int db5_put_color_table,
		    (struct db_i *dbip));
RT_EXPORT BU_EXTERN(int db5_update_ident,
		    (struct db_i *dbip,
		     const char *title,
		     double local2mm));
RT_EXPORT BU_EXTERN(int db_put_external5,
		    (struct bu_external *ep,
		     struct directory *dp,
		     struct db_i *dbip));
RT_EXPORT BU_EXTERN(int db5_update_attributes,
		    (struct directory *dp,
		     struct bu_attribute_value_set *avsp,
		     struct db_i *dbip));
RT_EXPORT BU_EXTERN(int db5_update_attribute,
		    (const char *obj_name,
		     const char *aname,
		     const char *value,
		     struct db_i *dbip));
RT_EXPORT BU_EXTERN(int db5_replace_attributes,
		    (struct directory *dp,
		     struct bu_attribute_value_set *avsp,
		     struct db_i *dbip));
RT_EXPORT BU_EXTERN(int db5_get_attributes,
		    (const struct db_i *dbip,
		     struct bu_attribute_value_set *avs,
		     const struct directory *dp));

/* db_comb.c */
RT_EXPORT BU_EXTERN(int db_tree_nleaves,
		    (const union tree *tp));
RT_EXPORT BU_EXTERN(struct rt_tree_array *db_flatten_tree,
		    (struct rt_tree_array	*rt_tree_array,
		     union tree			*tp,
		     int			op,
		     int			avail,
		     struct resource		*resp));
RT_EXPORT BU_EXTERN(int rt_comb_import4,
		    (struct rt_db_internal	*ip,
		     const struct bu_external	*ep,
		     const mat_t		matrix,		/* NULL if identity */
		     const struct db_i		*dbip,
		     struct resource		*resp));
RT_EXPORT BU_EXTERN(int rt_comb_export4,
		    (struct bu_external			*ep,
		     const struct rt_db_internal	*ip,
		     double				local2mm,
		     const struct db_i			*dbip,
		     struct resource			*resp));
RT_EXPORT BU_EXTERN(void db_tree_flatten_describe,
		    (struct bu_vls	*vls,
		     const union tree	*tp,
		     int		indented,
		     int		lvl,
		     double		mm2local,
		     struct resource	*resp));
RT_EXPORT BU_EXTERN(void db_tree_describe,
		    (struct bu_vls	*vls,
		     const union tree	*tp,
		     int		indented,
		     int		lvl,
		     double		mm2local));
RT_EXPORT BU_EXTERN(void db_comb_describe,
		    (struct bu_vls	*str,
		     const struct rt_comb_internal	*comb,
		     int		verbose,
		     double		mm2local,
		     struct resource	*resp));
RT_EXPORT BU_EXTERN(void rt_comb_ifree,
		    (struct rt_db_internal *ip,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(int rt_comb_describe,
		    (struct bu_vls	*str,
		     const struct rt_db_internal *ip,
		     int		verbose,
		     double		mm2local,
		     struct resource *resp,
		     struct db_i *db_i));
RT_EXPORT BU_EXTERN(void db_wrap_v4_external,
		    (struct bu_external *op,
		     const char *name));
RT_EXPORT BU_EXTERN(int db_ck_left_heavy_tree,
		    (const union tree	*tp,
		     int		no_unions));
RT_EXPORT BU_EXTERN(int db_ck_v4gift_tree,
		    (const union tree *tp));
RT_EXPORT BU_EXTERN(union tree *db_mkbool_tree,
		    (struct rt_tree_array *rt_tree_array,
		     int		howfar,
		     struct resource	*resp));
RT_EXPORT BU_EXTERN(union tree *db_mkgift_tree,
		    (struct rt_tree_array	*trees,
		     int			subtreecount,
		     struct resource		*resp));

/* g_tgc.c */
RT_EXPORT BU_EXTERN(void rt_pt_sort,
		    (register fastf_t t[],
		     int npts));

/* g_ell.c */
RT_EXPORT BU_EXTERN(void rt_ell_16pts,
		    (register fastf_t *ov,
		     register fastf_t *V,
		     fastf_t *A,
		     fastf_t *B));


/* roots.c */
RT_EXPORT BU_EXTERN(int rt_poly_roots,
		    (bn_poly_t *eqn,
		     bn_complex_t roots[],
		     const char *name));


/* db_io.c */
RT_EXPORT BU_EXTERN(int db_write,
		    (struct db_i	*dbip,
		     const genptr_t	addr,
		     long		count,
		     long		offset));
RT_EXPORT BU_EXTERN(int db_fwrite_external,
		    (FILE			*fp,
		     const char		*name,
		     struct bu_external	*ep));

/* It is normal to test for __STDC__ when using *_DEFINED tests but in
 * in this case "union record" is used for db_getmrec's return type.  This
 * requires that the "union_record *db_getmrec" be used whenever
 * RECORD_DEFINED is defined.
 */
#if defined(RECORD_DEFINED)
/* malloc & read records */
RT_EXPORT BU_EXTERN(union record *db_getmrec,
		    (const struct db_i *,
		     const struct directory *dp ));
/* get several records from db */
RT_EXPORT BU_EXTERN(int db_get,
		    (const struct db_i *,
		     const struct directory *dp,
		     union record *where,
		     int offset,
		     int len));
/* put several records into db */
RT_EXPORT BU_EXTERN(int db_put,
		    (struct db_i *,
		     const struct directory *dp,
		     union record *where,
		     int offset, int len));
#else /* RECORD_DEFINED */
/* malloc & read records */
RT_EXPORT BU_EXTERN(genptr_t db_getmrec,
		    (const struct db_i *,
		     const struct directory *dp));
/* get several records from db */
RT_EXPORT BU_EXTERN(int db_get,
		    (const struct db_i *,
		     const struct directory *dp,
		     genptr_t where,
		     int offset,
		     int len));
/* put several records into db */
RT_EXPORT BU_EXTERN(int db_put,
		    (const struct db_i *,
		     const struct directory *dp,
		     genptr_t where,
		     int offset,
		     int len));
#endif /* RECORD_DEFINED */
RT_EXPORT BU_EXTERN(int db_get_external,
		    (struct bu_external *ep,
		     const struct directory *dp,
		     const struct db_i *dbip));
RT_EXPORT BU_EXTERN(int db_put_external,
		    (struct bu_external *ep,
		     struct directory *dp,
		     struct db_i *dbip));
RT_EXPORT BU_EXTERN(void db_free_external,
		    (struct bu_external *ep));

/* db_scan.c */
/* read db (to build directory) */
RT_EXPORT BU_EXTERN(int db_scan,
		    (struct db_i *,
		     int (*handler)BU_ARGS((struct db_i *,
					    const char *name,
					    long addr,
					    int nrec,
					    int flags,
					    genptr_t client_data)),
		     int do_old_matter,
		     genptr_t client_data));
					/* update db unit conversions */
#define db_ident(a,b,c)		+++error+++
RT_EXPORT BU_EXTERN(int db_update_ident,
		    (struct db_i *dbip,
		     const char *title,
		     double local2mm));
RT_EXPORT BU_EXTERN(int db_fwrite_ident,
		    (FILE *fp,
		     const char *title,
		     double local2mm));
RT_EXPORT BU_EXTERN(void db_conversions,
		    (struct db_i *,
		     int units));
RT_EXPORT BU_EXTERN(int db_v4_get_units_code,
		    (const char *str));

/* db5_scan.c */
RT_EXPORT BU_EXTERN(int db_dirbuild,
		    (struct db_i *dbip));
RT_EXPORT BU_EXTERN(struct directory *db5_diradd,
		    (struct db_i *dbip,
		     const struct db5_raw_internal *rip,
		     long laddr,
		     genptr_t client_data));
RT_EXPORT BU_EXTERN(int db_get_version,
		    (struct db_i *dbip));
RT_EXPORT BU_EXTERN(int db5_scan,
		    (struct db_i *dbip,
		     void (*handler)(struct db_i *,
				     const struct db5_raw_internal *,
				     long addr,
				     genptr_t client_data),
		     genptr_t client_data));

/* db5_comb.c */
RT_EXPORT BU_EXTERN(int rt_comb_import5,
		    (struct rt_db_internal   *ip,
		     const struct bu_external *ep,
		     const mat_t             mat,
		     const struct db_i       *dbip,
		     struct resource         *resp,
		     const int		minor_type));

/* g_extrude.c */
RT_EXPORT BU_EXTERN(int rt_extrude_import5,
		    (struct rt_db_internal	*ip,
		     const struct bu_external	*ep,
		     register const mat_t	mat,
		     const struct db_i		*dbip,
		     struct resource		*resp,
		     const int			minor_type));


/* db_inmem.c */
RT_EXPORT BU_EXTERN(struct db_i * db_open_inmem, (void));

RT_EXPORT BU_EXTERN(struct db_i * db_create_inmem, (void));

RT_EXPORT BU_EXTERN(void db_inmem,
		    (struct directory	*dp,
		     struct bu_external	*ext,
		     int		flags,
		     struct db_i	*dbip));

/* db_lookup.c */
RT_EXPORT BU_EXTERN(int db_get_directory_size,
		    (const struct db_i	*dbip));
RT_EXPORT BU_EXTERN(void db_ck_directory,
		    (const struct db_i *dbip));

RT_EXPORT BU_EXTERN(int db_is_directory_non_empty,
		    (const struct db_i	*dbip));

RT_EXPORT BU_EXTERN(int db_dirhash,
		    (const char *str));
RT_EXPORT BU_EXTERN(int db_dircheck,
		    (struct db_i *dbip,
		     struct bu_vls *ret_name,
		     int noisy,
		     struct directory ***headp));
/* convert name to directory ptr */
RT_EXPORT BU_EXTERN(struct directory *db_lookup,
		    (const struct db_i *,
		     const char *name,
		     int noisy));
/* lookup directory entries based on attributes */
RT_EXPORT BU_EXTERN(struct bu_ptbl *db_lookup_by_attr,
		    (struct db_i *dbip,
		     int dir_flags,
		     struct bu_attribute_value_set *avs,
		     int op));
/* add entry to directory */
RT_EXPORT BU_EXTERN(struct directory *db_diradd,
		    (struct db_i *,
		     const char *name,
		     long laddr,
		     int len,
		     int flags,
		     genptr_t ptr));
RT_EXPORT BU_EXTERN(struct directory *db_diradd5,
		    (struct db_i *dbip,
		     const char *name,
		     long				laddr,
		     unsigned char			major_type,
		     unsigned char 			minor_type,
		     unsigned char			name_hidden,
		     long				object_length,
		     struct bu_attribute_value_set	*avs));

					/* delete entry from directory */
RT_EXPORT BU_EXTERN(int db_dirdelete,
		    (struct db_i *,
		     struct directory *dp));
RT_EXPORT BU_EXTERN(int db_fwrite_ident,
		    (FILE *,
		     const char *,
		     double));
RT_EXPORT BU_EXTERN(void db_pr_dir,
		    (const struct db_i *dbip));
RT_EXPORT BU_EXTERN(int db_rename,
		    (struct db_i *,
		     struct directory *,
		     const char *newname));


/* db_match.c */
RT_EXPORT BU_EXTERN(void db_update_nref,
		    (struct db_i *dbip,
		     struct resource *resp));

RT_EXPORT BU_EXTERN(int db_regexp_match,
		    (const char *pattern,
		     const char *string));
RT_EXPORT BU_EXTERN(int db_regexp_match_all,
		    (struct bu_vls *dest,
		     struct db_i *dbip,
		     const char *pattern));

/* db_flags.c */
RT_EXPORT BU_EXTERN(int db_flags_internal,
		    (const struct rt_db_internal *intern));

RT_EXPORT BU_EXTERN(int db_flags_raw_internal,
		    (const struct db5_raw_internal *intern));

/* db_alloc.c */

/* allocate "count" granules */
RT_EXPORT BU_EXTERN(int db_alloc,
		    (struct db_i *,
		     struct directory *dp,
		     int count));
/* delete "recnum" from entry */
RT_EXPORT BU_EXTERN(int db_delrec,
		    (struct db_i *,
		     struct directory *dp,
		     int recnum));
/* delete all granules assigned dp */
RT_EXPORT BU_EXTERN(int db_delete,
		    (struct db_i *,
		     struct directory *dp));
/* write FREE records from 'start' */
RT_EXPORT BU_EXTERN(int db_zapper,
		    (struct db_i *,
		     struct directory *dp,
		     int start));

/* db_tree.c */
RT_EXPORT BU_EXTERN(void db_dup_db_tree_state,
		    (struct db_tree_state *otsp,
		     const struct db_tree_state *itsp));
RT_EXPORT BU_EXTERN(void db_free_db_tree_state,
		    (struct db_tree_state *tsp));
RT_EXPORT BU_EXTERN(void db_init_db_tree_state,
		    (struct db_tree_state *tsp,
		     struct db_i *dbip,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(struct combined_tree_state *db_new_combined_tree_state,
		    (const struct db_tree_state *tsp,
		     const struct db_full_path *pathp));
RT_EXPORT BU_EXTERN(struct combined_tree_state *db_dup_combined_tree_state,
		    (const struct combined_tree_state *old));
RT_EXPORT BU_EXTERN(void db_free_combined_tree_state,
		    (struct combined_tree_state *ctsp));
RT_EXPORT BU_EXTERN(void db_pr_tree_state,
		    (const struct db_tree_state *tsp));
RT_EXPORT BU_EXTERN(void db_pr_combined_tree_state,
		    (const struct combined_tree_state *ctsp));
RT_EXPORT BU_EXTERN(int db_apply_state_from_comb,
		    (struct db_tree_state *tsp,
		     const struct db_full_path *pathp,
		     const struct rt_comb_internal *comb));
RT_EXPORT BU_EXTERN(int db_apply_state_from_memb,
		    (struct db_tree_state *tsp,
		     struct db_full_path *pathp,
		     const union tree *tp));
RT_EXPORT BU_EXTERN(int db_apply_state_from_one_member,
		    (struct db_tree_state *tsp,
		     struct db_full_path *pathp,
		     const char *cp,
		     int sofar,
		     const union tree *tp));
RT_EXPORT BU_EXTERN(union tree *db_find_named_leaf,
		    (union tree *tp, const char *cp));
RT_EXPORT BU_EXTERN(union tree *db_find_named_leafs_parent,
		    (int *side,
		     union tree *tp,
		     const char *cp));
RT_EXPORT BU_EXTERN(void db_tree_del_lhs,
		    (union tree *tp,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(void db_tree_del_rhs,
		    (union tree *tp,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(int db_tree_del_dbleaf,
		    (union tree **tp,
		     const char *cp,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(void db_tree_mul_dbleaf,
		    (union tree *tp,
		     const mat_t mat));
RT_EXPORT BU_EXTERN(void db_tree_funcleaf,
		    (struct db_i		*dbip,
		     struct rt_comb_internal	*comb,
		     union tree		*comb_tree,
		     void		(*leaf_func)(),
		     genptr_t		user_ptr1,
		     genptr_t		user_ptr2,
		     genptr_t		user_ptr3));
RT_EXPORT BU_EXTERN(int db_follow_path,
		    (struct db_tree_state	*tsp,
		     struct db_full_path	*total_path,
		     const struct db_full_path	*new_path,
		     int			noisy,
		     int			depth));
RT_EXPORT BU_EXTERN(int db_follow_path_for_state,
		    (struct db_tree_state *tsp,
		     struct db_full_path *pathp,
		     const char *orig_str, int noisy));
RT_EXPORT BU_EXTERN(union tree *db_recurse,
		    (struct db_tree_state	*tsp,
		     struct db_full_path *pathp,
		     struct combined_tree_state **region_start_statepp,
		     genptr_t client_data));
RT_EXPORT BU_EXTERN(union tree *db_dup_subtree,
		    (const union tree *tp,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(void db_ck_tree,
		    (const union tree *tp));
RT_EXPORT BU_EXTERN(void db_free_tree,
		    (union tree *tp,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(void db_left_hvy_node,
		    (union tree *tp));
RT_EXPORT BU_EXTERN(void db_non_union_push,
		    (union tree *tp,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(int db_count_tree_nodes,
		    (const union tree *tp,
		     int count));
RT_EXPORT BU_EXTERN(int db_is_tree_all_unions,
		    (const union tree *tp));
RT_EXPORT BU_EXTERN(int db_count_subtree_regions,
		    (const union tree *tp));
RT_EXPORT BU_EXTERN(int db_tally_subtree_regions,
		    (union tree	*tp,
		     union tree	**reg_trees,
		     int		cur,
		     int		lim,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(int db_walk_tree,
		    (struct db_i *dbip,
		     int argc,
		     const char **argv,
		     int ncpu,
		     const struct db_tree_state *init_state,
		     int (*reg_start_func) (struct db_tree_state * /*tsp*/,
					    struct db_full_path * /*pathp*/,
					    const struct rt_comb_internal * /* combp */,
					    genptr_t client_data),
		     union tree *(*reg_end_func) (struct db_tree_state * /*tsp*/,
						  struct db_full_path * /*pathp*/,
						  union tree * /*curtree*/,
						  genptr_t client_data),
		     union tree *(*leaf_func) (struct db_tree_state * /*tsp*/,
					       struct db_full_path * /*pathp*/,
					       struct rt_db_internal * /*ip*/,
					       genptr_t client_data ),
		     genptr_t client_data ));
RT_EXPORT BU_EXTERN(int db_path_to_mat,
		    (struct db_i		*dbip,
		     struct db_full_path	*pathp,
		     mat_t			mat,		/* result */
		     int			depth,		/* number of arcs */
		     struct resource		*resp));
RT_EXPORT BU_EXTERN(void db_apply_anims,
		    (struct db_full_path *pathp,
		     struct directory *dp,
		     mat_t stck,
		     mat_t arc,
		     struct mater_info *materp));
/* XXX db_shader_mat, should be called rt_shader_mat */
RT_EXPORT BU_EXTERN(int db_region_mat,
		    (mat_t		m,		/* result */
		     struct db_i	*dbip,
		     const char	*name,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(int db_shader_mat,
		    (mat_t			model_to_shader,	/* result */
		     const struct rt_i	*rtip,
		     const struct region	*rp,
		     point_t			p_min,	/* input/output: shader/region min point */
		     point_t			p_max,	/* input/output: shader/region max point */
		     struct resource		*resp));

/* dir.c */
RT_EXPORT BU_EXTERN(struct rt_i *rt_dirbuild,
		    (const char *filename,
		     char *buf, int len));
RT_EXPORT BU_EXTERN(int rt_db_get_internal,
		    (struct rt_db_internal	*ip,
		     const struct directory	*dp,
		     const struct db_i	*dbip,
		     const mat_t		mat,
		     struct resource		*resp));
RT_EXPORT BU_EXTERN(int rt_db_put_internal,
		    (struct directory	*dp,
		     struct db_i		*dbip,
		     struct rt_db_internal	*ip,
		     struct resource		*resp));
RT_EXPORT BU_EXTERN(int rt_fwrite_internal,
		    (FILE *fp,
		     const char *name,
		     const struct rt_db_internal *ip,
		     double conv2mm));
RT_EXPORT BU_EXTERN(void rt_db_free_internal,
		    (struct rt_db_internal *ip,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(int rt_db_lookup_internal,
		    (struct db_i *dbip,
		     const char *obj_name,
		     struct directory **dpp,
		     struct rt_db_internal *ip,
		     int noisy,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(void rt_optim_tree,
		    (register union tree *tp,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(void db_get_directory,
		    (register struct resource *resp));

/* db_walk.c */
RT_EXPORT BU_EXTERN(void db_functree,
		    (struct db_i *dbip,
		     struct directory *dp,
		     void (*comb_func)(struct db_i *,
				       struct directory *,
				       genptr_t),
		     void (*leaf_func)(struct db_i *,
				       struct directory *,
				       genptr_t),
		     struct resource *resp,
		     genptr_t client_data));

/* g_arb.c */
RT_EXPORT BU_EXTERN(int rt_arb_get_cgtype,
		    ());		/* needs rt_arb_internal for arg list */
RT_EXPORT BU_EXTERN(int rt_arb_std_type,
		    (const struct rt_db_internal *ip,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void rt_arb_centroid,
		    ());		/* needs rt_arb_internal for arg list */
RT_EXPORT BU_EXTERN(int rt_arb_calc_points,
		    ());		/* needs wdb.h for arg list */
RT_EXPORT BU_EXTERN(int rt_arb_3face_intersect,
		    (point_t			point,
		     const plane_t		planes[6],
		     int			type,		/* 4..8 */
		     int			loc));
#ifdef SEEN_RTGEOM_H
RT_EXPORT BU_EXTERN(int rt_arb_calc_planes,
		    (Tcl_Interp			*interp,
		     struct rt_arb_internal	*arb,
		     int			type,
		     plane_t			planes[6],
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int rt_arb_move_edge,
		    (Tcl_Interp		*interp,
		     struct rt_arb_internal	*arb,
		     vect_t			thru,
		     int			bp1,
		     int			bp2,
		     int			end1,
		     int			end2,
		     const vect_t		dir,
		     plane_t			planes[6],
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int rt_arb_edit,
		    (Tcl_Interp			*interp,
		     struct rt_arb_internal	*arb,
		     int			arb_type,
		     int			edit_type,
		     vect_t			pos_model,
		     plane_t			planes[6],
		     const struct bn_tol	*tol));
#endif
RT_EXPORT extern const int rt_arb_faces[5][24];
RT_EXPORT extern short earb8[12][18];
RT_EXPORT extern short earb7[12][18];
RT_EXPORT extern short earb6[10][18];
RT_EXPORT extern short earb5[9][18];
RT_EXPORT extern short earb4[5][18];

/* g_epa.c */
RT_EXPORT BU_EXTERN(void rt_ell,
		    (fastf_t *ov,
		     const fastf_t *V,
		     const fastf_t *A,
		     const fastf_t *B,
		     int sides));

/* g_pipe.c */
RT_EXPORT BU_EXTERN(void rt_vls_pipept,
		    (struct bu_vls *vp,
		     int seg_no,
		     const struct rt_db_internal *ip,
		     double mm2local));
RT_EXPORT BU_EXTERN(void rt_pipept_print,
		    ());		/* needs wdb_pipept for arg */
RT_EXPORT BU_EXTERN(int rt_pipe_ck,
		    (const struct bu_list *headp));

/* g_rpc.c */
RT_EXPORT BU_EXTERN(int rt_mk_parabola,
		    (struct rt_pt_node *pts,
		     fastf_t r,
		     fastf_t b,
		     fastf_t dtol,
		     fastf_t ntol));
RT_EXPORT BU_EXTERN(struct rt_pt_node *rt_ptalloc,
		    ());

/* memalloc.c -- non PARALLEL routines */
RT_EXPORT BU_EXTERN(unsigned long rt_memalloc,
		    (struct mem_map **pp,
		     unsigned size));
RT_EXPORT BU_EXTERN(struct mem_map * rt_memalloc_nosplit,
		    (struct mem_map **pp,
		     unsigned size));
RT_EXPORT BU_EXTERN(unsigned long rt_memget,
		    (struct mem_map **pp,
		     unsigned int size,
		     unsigned int place));
RT_EXPORT BU_EXTERN(void rt_memfree,
		    (struct mem_map **pp,
		     unsigned size,
		     unsigned long addr));
RT_EXPORT BU_EXTERN(void rt_mempurge,
		    (struct mem_map **pp));
RT_EXPORT BU_EXTERN(void rt_memprint,
		    (struct mem_map **pp));
RT_EXPORT BU_EXTERN(void rt_memclose,
		    ());

RT_EXPORT BU_EXTERN(struct bn_vlblock *rt_vlblock_init,
		    ());
RT_EXPORT BU_EXTERN(void rt_vlblock_free,
		    (struct bn_vlblock *vbp));
RT_EXPORT BU_EXTERN(struct bu_list *rt_vlblock_find,
		    (struct bn_vlblock *vbp,
		     int r,
		     int g,
		     int b));

/* g_ars.c */
RT_EXPORT BU_EXTERN(void rt_hitsort,
		    (struct hit h[],
		     int nh));

/* g_pg.c */
RT_EXPORT BU_EXTERN(int rt_pg_to_bot,
		    (struct rt_db_internal *ip,
		     const struct bn_tol *tol,
		     struct resource *resp0));
RT_EXPORT BU_EXTERN(int rt_pg_plot,
		    (struct bu_list		*vhead,
		     struct rt_db_internal	*ip,
		     const struct rt_tess_tol *ttol,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int rt_pg_plot_poly,
		    (struct bu_list		*vhead,
		     struct rt_db_internal	*ip,
		     const struct rt_tess_tol	*ttol,
		     const struct bn_tol	*tol));

/* g_hf.c */
RT_EXPORT BU_EXTERN(int rt_hf_to_dsp,
		    (struct rt_db_internal *db_intern,
		     struct resource *resp));

/* g_dsp.c */
RT_EXPORT BU_EXTERN(int dsp_pos,
		    (point_t out,
		     struct soltab *stp,
		     point_t p));

/* pr.c */
RT_EXPORT BU_EXTERN(void rt_pr_soltab,
		    (const struct soltab *stp));
RT_EXPORT BU_EXTERN(void rt_pr_region,
		    (const struct region *rp));
RT_EXPORT BU_EXTERN(void rt_pr_partitions,
		    (const struct rt_i *rtip,
		     const struct partition	*phead,
		     const char *title));
RT_EXPORT BU_EXTERN(void rt_pr_pt_vls,
		    (struct bu_vls *v,
		     const struct rt_i *rtip,
		     const struct partition *pp));
RT_EXPORT BU_EXTERN(void rt_pr_pt,
		    (const struct rt_i *rtip,
		     const struct partition *pp));
RT_EXPORT BU_EXTERN(void rt_pr_seg_vls,
		    (struct bu_vls *,
		     const struct seg *));
RT_EXPORT BU_EXTERN(void rt_pr_seg,
		    (const struct seg *segp));
RT_EXPORT BU_EXTERN(void rt_pr_hit,
		    (const char *str,
		     const struct hit	*hitp));
RT_EXPORT BU_EXTERN(void rt_pr_hit_vls,
		    (struct bu_vls *v,
		     const char *str,
		     const struct hit *hitp));
RT_EXPORT BU_EXTERN(void rt_pr_hitarray_vls,
		    (struct bu_vls *v,
		     const char *str,
		     const struct hit *hitp,
		     int count));
RT_EXPORT BU_EXTERN(void rt_pr_tree,
		    (const union tree *tp,
		     int lvl));
RT_EXPORT BU_EXTERN(void rt_pr_tree_vls,
		    (struct bu_vls *vls,
		     const union tree *tp));
RT_EXPORT BU_EXTERN(char *rt_pr_tree_str,
		    (const union tree *tree));
RT_EXPORT BU_EXTERN(void rt_pr_tree_val,
		    (const union tree *tp,
		     const struct partition *partp,
		     int pr_name,
		     int lvl));
RT_EXPORT BU_EXTERN(void rt_pr_fallback_angle,
		    (struct bu_vls *str,
		     const char *prefix,
		     const double angles[5]));
RT_EXPORT BU_EXTERN(void rt_find_fallback_angle,
		    (double angles[5],
		     const vect_t vec));
RT_EXPORT BU_EXTERN(void rt_pr_tol,
		    (const struct bn_tol *tol));

/* regionfix.c */
RT_EXPORT BU_EXTERN(void rt_regionfix,
		    (struct rt_i *rtip));

/* table.c */
RT_EXPORT BU_EXTERN(int rt_id_solid,
		    (struct bu_external *ep));
RT_EXPORT BU_EXTERN(const struct rt_functab *rt_get_functab_by_label,
		    (const char *label));
RT_EXPORT BU_EXTERN(int rt_generic_xform,
		    (struct rt_db_internal	*op,
		     const mat_t		mat,
		     struct rt_db_internal	*ip,
		     int			avail,
		     struct db_i		*dbip,
		     struct resource		*resp));

RT_EXPORT BU_EXTERN(void rt_nul_make,
		    (const struct rt_functab *ftp,
		     struct rt_db_internal *intern,
		     double diameter));

/* prep.c */
RT_EXPORT BU_EXTERN(void rt_plot_all_bboxes,
		    (FILE *fp,
		     struct rt_i *rtip));
RT_EXPORT BU_EXTERN(void rt_plot_all_solids,
		    (FILE		*fp,
		     struct rt_i	*rtip,
		     struct resource	*resp));
RT_EXPORT BU_EXTERN(void rt_init_resource,
		    (struct resource *resp,
		     int		cpu_num,
		     struct rt_i	*rtip));
RT_EXPORT BU_EXTERN(void rt_clean_resource,
		    (struct rt_i *rtip,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(int rt_unprep,
		    (struct rt_i *rtip,
		     struct rt_reprep_obj_list *objs,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(int rt_reprep,
		    (struct rt_i *rtip,
		     struct rt_reprep_obj_list *objs,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(int re_prep_solids,
		    (struct rt_i *rtip,
		     int num_solids,
		     char **solid_names,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(int rt_find_paths,
		    (struct db_i *dbip,
		     struct directory *start,
		     struct directory *end,
		     struct bu_ptbl *paths,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(struct bu_bitv *get_solidbitv,
		    (long nbits,
		     struct resource *resp));

/* shoot.c */
RT_EXPORT BU_EXTERN(void rt_add_res_stats,
		    (struct rt_i *rtip,
		     struct resource *resp));
		    /* Tally stats into struct rt_i */
RT_EXPORT BU_EXTERN(void rt_zero_res_stats,
		    (struct resource *resp));
RT_EXPORT BU_EXTERN(void rt_res_pieces_clean,
		    (struct resource *resp,
		     struct rt_i *rtip));
RT_EXPORT BU_EXTERN(void rt_res_pieces_init,
		    (struct resource *resp,
		     struct rt_i *rtip));
RT_EXPORT BU_EXTERN(void rt_vstub,
		    (struct soltab *stp[],
		     struct xray *rp[],
		     struct  seg segp[],
		     int n,
		     struct application	*ap));


/* tree.c */
RT_EXPORT BU_EXTERN(int rt_bound_tree,
		    (const union tree	*tp,
		     vect_t		tree_min,
		     vect_t		tree_max));
RT_EXPORT BU_EXTERN(int rt_tree_elim_nops,
		    (union tree *,
		     struct resource *resp));


/* vlist.c */
/* XXX Has some stuff mixed in here that should go in LIBBN */
RT_EXPORT BU_EXTERN(struct bn_vlblock *bn_vlblock_init,
		    (struct bu_list	*free_vlist_hd,		/* where to get/put free vlists */
		     int		max_ent));
RT_EXPORT BU_EXTERN(struct bn_vlblock *	rt_vlblock_init,
		    ());
RT_EXPORT BU_EXTERN(void rt_vlblock_free,
		    (struct bn_vlblock *vbp));
RT_EXPORT BU_EXTERN(struct bu_list *rt_vlblock_find,
		    (struct bn_vlblock *vbp,
		     int r,
		     int g,
		     int b));
RT_EXPORT BU_EXTERN(int rt_ck_vlist,
		    (const struct bu_list *vhead));
RT_EXPORT BU_EXTERN(void rt_vlist_copy,
		    (struct bu_list *dest,
		     const struct bu_list *src));
RT_EXPORT BU_EXTERN(void bn_vlist_cleanup,
		    (struct bu_list *hd));
RT_EXPORT BU_EXTERN(void rt_vlist_cleanup,
		    ());
RT_EXPORT BU_EXTERN(void bn_vlist_rpp,
		    (struct bu_list *hd,
		     const point_t minn,
		     const point_t maxx));
RT_EXPORT BU_EXTERN(void rt_vlist_export,
		    (struct bu_vls *vls,
		     struct bu_list *hp,
		     const char *name));
RT_EXPORT BU_EXTERN(void rt_vlist_import,
		    (struct bu_list *hp,
		     struct bu_vls *namevls,
		     const unsigned char *buf));
RT_EXPORT BU_EXTERN(void rt_plot_vlblock,
		    (FILE *fp,
		     const struct bn_vlblock *vbp));
RT_EXPORT BU_EXTERN(void rt_vlist_to_uplot,
		    (FILE *fp,
		     const struct bu_list *vhead));
RT_EXPORT BU_EXTERN(int rt_process_uplot_value,
		    (struct bu_list **vhead,
		     struct bn_vlblock *vbp,
		     FILE *fp,
		     int c,
		     double char_size,
		     int mode));
RT_EXPORT BU_EXTERN(int rt_uplot_to_vlist,
		    (struct bn_vlblock *vbp,
		     FILE *fp,
		     double char_size,
		     int mode));
RT_EXPORT BU_EXTERN(void rt_label_vlist_verts,
		    (struct bn_vlblock *vbp,
		     struct bu_list *src,
		     mat_t mat,
		     double sz,
		     double mm2local));

#ifdef SEEN_RTGEOM_H
/* g_sketch.c */
RT_EXPORT BU_EXTERN(void rt_sketch_ifree,
		    (struct rt_db_internal	*ip));
RT_EXPORT BU_EXTERN(int curve_to_vlist,
		    (struct bu_list		*vhead,
		     const struct rt_tess_tol	*ttol,
		     point_t			V,
		     vect_t			u_vec,
		     vect_t			v_vec,
		     struct rt_sketch_internal *sketch_ip,
		     struct curve		*crv));

RT_EXPORT BU_EXTERN(int rt_check_curve,
		    (struct curve *crv,
		     struct rt_sketch_internal *skt,
		     int noisey));

RT_EXPORT BU_EXTERN(void rt_curve_reverse_segment,
		    (long *lng));
RT_EXPORT BU_EXTERN(void rt_curve_order_segments,
		    (struct curve *crv));

RT_EXPORT BU_EXTERN(void rt_copy_curve,
		    (struct curve *crv_out,
		     const struct curve *crv_in));

RT_EXPORT BU_EXTERN(int rt_check_curve,
		    (struct curve *crv,
		     struct rt_sketch_internal *skt,
		     int noisey));
RT_EXPORT BU_EXTERN(void rt_curve_free,
		    (struct curve *crv));
RT_EXPORT BU_EXTERN(void rt_copy_curve,
		    (struct curve *crv_out,
		     const struct curve *crv_in));
RT_EXPORT BU_EXTERN(struct rt_sketch_internal *rt_copy_sketch,
		    (const struct rt_sketch_internal *sketch_ip));
RT_EXPORT BU_EXTERN(int curve_to_tcl_list,
		    (struct bu_vls *vls,
		     struct curve *crv));
#endif

/* htbl.c */
RT_EXPORT BU_EXTERN(void rt_htbl_init,
		    (struct rt_htbl *b,
		     int len,
		     const char *str));
RT_EXPORT BU_EXTERN(void rt_htbl_reset,
		    (struct rt_htbl *b));
RT_EXPORT BU_EXTERN(void rt_htbl_free,
		    (struct rt_htbl *b));
RT_EXPORT BU_EXTERN(struct hit *rt_htbl_get,
		    (struct rt_htbl *b));

/************************************************************************
 *									*
 *			N M G Support Function Declarations		*
 *									*
 ************************************************************************/
#if defined(NMG_H)

/* From file nmg_mk.c */
/*	MAKE routines */
RT_EXPORT BU_EXTERN(struct model *nmg_mm,
		    ());
RT_EXPORT BU_EXTERN(struct model *nmg_mmr,
		    ());
RT_EXPORT BU_EXTERN(struct nmgregion *nmg_mrsv,
		    (struct model *m));
RT_EXPORT BU_EXTERN(struct shell *nmg_msv,
		    (struct nmgregion *r_p));
RT_EXPORT BU_EXTERN(struct faceuse *nmg_mf,
		    (struct loopuse *lu1));
RT_EXPORT BU_EXTERN(struct loopuse *nmg_mlv,
		    (long *magic,
		     struct vertex *v,
		     int orientation));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_me,
		    (struct vertex *v1,
		     struct vertex *v2,
		     struct shell *s));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_meonvu,
		    (struct vertexuse *vu));
RT_EXPORT BU_EXTERN(struct loopuse *nmg_ml,
		    (struct shell *s));
/*	KILL routines */
RT_EXPORT BU_EXTERN(int nmg_keg,
		    (struct edgeuse *eu));
RT_EXPORT BU_EXTERN(int nmg_kvu,
		    (struct vertexuse *vu));
RT_EXPORT BU_EXTERN(int nmg_kfu,
		    (struct faceuse *fu1));
RT_EXPORT BU_EXTERN(int nmg_klu,
		    (struct loopuse *lu1));
RT_EXPORT BU_EXTERN(int nmg_keu,
		    (struct edgeuse *eu));
RT_EXPORT BU_EXTERN(int nmg_ks,
		    (struct shell *s));
RT_EXPORT BU_EXTERN(int nmg_kr,
		    (struct nmgregion *r));
RT_EXPORT BU_EXTERN(void nmg_km,
		    (struct model *m));
/*	Geometry and Attribute routines */
RT_EXPORT BU_EXTERN(void nmg_vertex_gv,
		    (struct vertex *v,
		     const point_t pt));
RT_EXPORT BU_EXTERN(void nmg_vertex_g,
		    (struct vertex *v,
		     fastf_t x,
		     fastf_t y,
		     fastf_t z));
RT_EXPORT BU_EXTERN(void nmg_vertexuse_nv,
		    (struct vertexuse *vu,
		     const vect_t norm));
RT_EXPORT BU_EXTERN(void nmg_vertexuse_a_cnurb,
		    (struct vertexuse *vu,
		     const fastf_t *uvw));
RT_EXPORT BU_EXTERN(void nmg_edge_g,
		    (struct edgeuse *eu));
RT_EXPORT BU_EXTERN(void nmg_edge_g_cnurb,
		    (struct edgeuse *eu,
		     int order,
		     int n_knots,
		     fastf_t *kv,
		     int n_pts,
		     int pt_type,
		     fastf_t *points));
RT_EXPORT BU_EXTERN(void nmg_edge_g_cnurb_plinear,
		    (struct edgeuse *eu));
RT_EXPORT BU_EXTERN(int nmg_use_edge_g,
		    (struct edgeuse *eu,
		     long *eg));
RT_EXPORT BU_EXTERN(void nmg_loop_g,
		    (struct loop *l,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_face_g,
		    (struct faceuse *fu,
		     const plane_t p));
RT_EXPORT BU_EXTERN(void nmg_face_new_g,
		    (struct faceuse *fu,
		     const plane_t pl));
RT_EXPORT BU_EXTERN(void nmg_face_g_snurb,
		    (struct faceuse *fu,
		     int u_order,
		     int v_order,
		     int n_u_knots,
		     int n_v_knots,
		     fastf_t *ukv,
		     fastf_t *vkv,
		     int n_rows,
		     int n_cols,
		     int pt_type,
		     fastf_t *mesh));
RT_EXPORT BU_EXTERN(void nmg_face_bb,
		    (struct face *f,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_shell_a,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_region_a,
		    (struct nmgregion *r,
		     const struct bn_tol *tol));
/*	DEMOTE routines */
RT_EXPORT BU_EXTERN(int nmg_demote_lu,
		    (struct loopuse *lu));
RT_EXPORT BU_EXTERN(int nmg_demote_eu,
		    (struct edgeuse *eu));
/*	MODIFY routines */
RT_EXPORT BU_EXTERN(void nmg_movevu,
		    (struct vertexuse *vu,
		     struct vertex *v));
#define nmg_moveeu(a,b)		nmg_je(a,b)
RT_EXPORT BU_EXTERN(void nmg_je,
		    (struct edgeuse *eudst,
		     struct edgeuse *eusrc));
RT_EXPORT BU_EXTERN(void nmg_unglueedge,
		    (struct edgeuse *eu));
RT_EXPORT BU_EXTERN(void nmg_jv,
		    (struct vertex *v1,
		     struct vertex *v2));
RT_EXPORT BU_EXTERN(void nmg_jfg,
		    (struct face *f1,
		     struct face *f2));
RT_EXPORT BU_EXTERN(void nmg_jeg,
		    (struct edge_g_lseg *dest_eg,
		     struct edge_g_lseg *src_eg));

/* From nmg_mod.c */
/*	REGION Routines */
RT_EXPORT BU_EXTERN(void nmg_merge_regions,
		    (struct nmgregion *r1,
		     struct nmgregion *r2,
		     const struct bn_tol *tol));

/*	SHELL Routines */
RT_EXPORT BU_EXTERN(void nmg_shell_coplanar_face_merge,
		    (struct shell *s,
		     const struct bn_tol *tol,
		     const int simplify));
RT_EXPORT BU_EXTERN(int nmg_simplify_shell,
		    (struct shell *s));
RT_EXPORT BU_EXTERN(void nmg_rm_redundancies,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_sanitize_s_lv,
		    (struct shell *s,
		     int orient));
RT_EXPORT BU_EXTERN(void nmg_s_split_touchingloops,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_s_join_touchingloops,
		    (struct shell		*s,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_js,
		    (struct shell	*s1,
		     struct shell	*s2,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_invert_shell,
		    (struct shell		*s,
		     const struct bn_tol	*tol));

/*	FACE Routines */
RT_EXPORT BU_EXTERN(struct faceuse *nmg_cmface,
		    (struct shell *s,
		     struct vertex **vt[],
		     int n));
RT_EXPORT BU_EXTERN(struct faceuse *nmg_cface,
		    (struct shell *s,
		     struct vertex **vt,
		     int n));
RT_EXPORT BU_EXTERN(struct faceuse *nmg_add_loop_to_face,
		    (struct shell *s,
		     struct faceuse *fu,
		     struct vertex **verts,
		     int n,
		     int dir));
RT_EXPORT BU_EXTERN(int nmg_fu_planeeqn,
		    (struct faceuse *fu,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_gluefaces,
		    (struct faceuse *fulist[],
		     int n,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_simplify_face,
		    (struct faceuse *fu));
RT_EXPORT BU_EXTERN(void nmg_reverse_face,
		    (struct faceuse *fu));
RT_EXPORT BU_EXTERN(void nmg_mv_fu_between_shells,
		    (struct shell *dest,
		     struct shell *src,
		     struct faceuse *fu));
RT_EXPORT BU_EXTERN(void nmg_jf,
		    (struct faceuse *dest_fu,
		     struct faceuse *src_fu));
RT_EXPORT BU_EXTERN(struct faceuse *nmg_dup_face,
		    (struct faceuse *fu,
		     struct shell *s));
/*	LOOP Routines */
RT_EXPORT BU_EXTERN(void nmg_jl,
		    (struct loopuse *lu,
		     struct edgeuse *eu));
RT_EXPORT BU_EXTERN(struct vertexuse *nmg_join_2loops,
		    (struct vertexuse *vu1,
		     struct vertexuse *vu2));
RT_EXPORT BU_EXTERN(struct vertexuse *nmg_join_singvu_loop,
		    (struct vertexuse *vu1,
		     struct vertexuse *vu2));
RT_EXPORT BU_EXTERN(struct vertexuse *nmg_join_2singvu_loops,
		    (struct vertexuse *vu1,
		     struct vertexuse *vu2));
RT_EXPORT BU_EXTERN(struct loopuse *nmg_cut_loop,
		    (struct vertexuse *vu1,
		     struct vertexuse *vu2));
RT_EXPORT BU_EXTERN(struct loopuse *nmg_split_lu_at_vu,
		    (struct loopuse *lu,
		     struct vertexuse *vu));
RT_EXPORT BU_EXTERN(struct vertexuse *nmg_find_repeated_v_in_lu,
		    (struct vertexuse *vu));
RT_EXPORT BU_EXTERN(void nmg_split_touchingloops,
		    (struct loopuse *lu,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_join_touchingloops,
		    (struct loopuse *lu));
RT_EXPORT BU_EXTERN(int nmg_get_touching_jaunts,
		    (const struct loopuse *lu,
		     struct bu_ptbl *tbl,
		     int *need_init));
RT_EXPORT BU_EXTERN(void nmg_kill_accordions,
		    (struct loopuse *lu));
RT_EXPORT BU_EXTERN(int nmg_loop_split_at_touching_jaunt,
		    (struct loopuse		*lu,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_simplify_loop,
		    (struct loopuse *lu));
RT_EXPORT BU_EXTERN(int nmg_kill_snakes,
		    (struct loopuse *lu));
RT_EXPORT BU_EXTERN(void nmg_mv_lu_between_shells,
		    (struct shell *dest,
		     struct shell *src,
		     struct loopuse *lu));
RT_EXPORT BU_EXTERN(void nmg_moveltof,
		    (struct faceuse *fu,
		     struct shell *s));
RT_EXPORT BU_EXTERN(struct loopuse *nmg_dup_loop,
		    (struct loopuse *lu,
		     long *parent,
		     long **trans_tbl));
RT_EXPORT BU_EXTERN(void nmg_set_lu_orientation,
		    (struct loopuse *lu,
		     int is_opposite));
RT_EXPORT BU_EXTERN(void nmg_lu_reorient,
		    (struct loopuse *lu));
/*	EDGE Routines */
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_eusplit,
		    (struct vertex *v,
		     struct edgeuse *oldeu,
		     int share_geom));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_esplit,
		    (struct vertex *v,
		     struct edgeuse *eu,
		     int share_geom));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_ebreak,
		    (struct vertex *v,
		     struct edgeuse *eu));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_ebreaker,
		    (struct vertex *v,
		     struct edgeuse *eu,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct vertex *nmg_e2break,
		    (struct edgeuse *eu1,
		     struct edgeuse *eu2));
RT_EXPORT BU_EXTERN(int nmg_unbreak_edge,
		    (struct edgeuse *eu1_first));
RT_EXPORT BU_EXTERN(int nmg_unbreak_shell_edge_unsafe,
		    (struct edgeuse *eu1_first));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_eins,
		    (struct edgeuse *eu));
RT_EXPORT BU_EXTERN(void nmg_mv_eu_between_shells,
		    (struct shell *dest,
		     struct shell *src,
		     struct edgeuse *eu));
/*	VERTEX Routines */
RT_EXPORT BU_EXTERN(void nmg_mv_vu_between_shells,
		    (struct shell *dest,
		     struct shell *src,
		     struct vertexuse *vu));

/* From nmg_info.c */
	/* Model routines */
RT_EXPORT BU_EXTERN(struct model *nmg_find_model,
		    (const long *magic_p));
		    RT_EXPORT BU_EXTERN(void nmg_model_bb,
		    (point_t min_pt,
		     point_t max_pt,
		     const struct model *m));


		    /* Shell routines */
RT_EXPORT BU_EXTERN(int nmg_shell_is_empty,
		    (const struct shell *s));
RT_EXPORT BU_EXTERN(struct shell *nmg_find_s_of_lu,
		    (const struct loopuse *lu));
RT_EXPORT BU_EXTERN(struct shell *nmg_find_s_of_eu,
		    (const struct edgeuse *eu));
RT_EXPORT BU_EXTERN(struct shell *nmg_find_s_of_vu,
		    (const struct vertexuse *vu));

	/* Face routines */
RT_EXPORT BU_EXTERN(struct faceuse *nmg_find_fu_of_eu,
		    (const struct edgeuse *eu));
RT_EXPORT BU_EXTERN(struct faceuse *nmg_find_fu_of_lu,
		    (const struct loopuse *lu));
RT_EXPORT BU_EXTERN(struct faceuse *nmg_find_fu_of_vu,
		    (const struct vertexuse *vu));
RT_EXPORT BU_EXTERN(struct faceuse *nmg_find_fu_with_fg_in_s,
		    (const struct shell *s1,
		     const struct faceuse *fu2));
RT_EXPORT BU_EXTERN(double nmg_measure_fu_angle,
		    (const struct edgeuse *eu,
		     const vect_t xvec,
		     const vect_t yvec,
		     const vect_t zvec));

	/* Loop routines */
RT_EXPORT BU_EXTERN(struct loopuse*nmg_find_lu_of_vu,
		    (const struct vertexuse *vu));
RT_EXPORT BU_EXTERN(int nmg_loop_is_a_crack,
		    (const struct loopuse *lu));
RT_EXPORT BU_EXTERN(int	nmg_loop_is_ccw,
		    (const struct loopuse *lu,
		     const plane_t norm,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(const struct vertexuse *nmg_loop_touches_self,
		    (const struct loopuse *lu));
RT_EXPORT BU_EXTERN(int nmg_2lu_identical,
		    (const struct edgeuse *eu1,
		     const struct edgeuse *eu2));

	/* Edge routines */
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_find_matching_eu_in_s,
		    (const struct edgeuse	*eu1,
		     const struct shell	*s2));
RT_EXPORT BU_EXTERN(struct edgeuse	*nmg_findeu,
		    (const struct vertex *v1,
		     const struct vertex *v2,
		     const struct shell *s,
		     const struct edgeuse *eup,
		     int dangling_only));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_find_eu_in_face,
		    (const struct vertex *v1,
		     const struct vertex *v2,
		     const struct faceuse *fu,
		     const struct edgeuse *eup,
		     int dangling_only));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_find_e,
		    (const struct vertex *v1,
		     const struct vertex *v2,
		     const struct shell *s,
		     const struct edge *ep));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_find_eu_of_vu,
		    (const struct vertexuse *vu));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_find_eu_with_vu_in_lu,
		    (const struct loopuse *lu,
		     const struct vertexuse *vu));
RT_EXPORT BU_EXTERN(const struct edgeuse *nmg_faceradial,
		    (const struct edgeuse *eu));
RT_EXPORT BU_EXTERN(const struct edgeuse *nmg_radial_face_edge_in_shell,
		    (const struct edgeuse *eu));
RT_EXPORT BU_EXTERN(const struct edgeuse *nmg_find_edge_between_2fu,
		    (const struct faceuse *fu1,
		     const struct faceuse *fu2,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct edge *nmg_find_e_nearest_pt2,
		    (long *magic_p,
		     const point_t pt2,
		     const mat_t mat,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_find_matching_eu_in_s,
		    (const struct edgeuse *eu1,
		     const struct shell *s2));
RT_EXPORT BU_EXTERN(void nmg_eu_2vecs_perp,
		    (vect_t xvec,
		     vect_t yvec,
		     vect_t zvec,
		     const struct edgeuse *eu,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_find_eu_leftvec,
		    (vect_t left,
		     const struct edgeuse *eu));
RT_EXPORT BU_EXTERN(int nmg_find_eu_left_non_unit,
		    (vect_t left,
		     const struct edgeuse	*eu));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_find_ot_same_eu_of_e,
		    (const struct edge *e));

	/* Vertex routines */
RT_EXPORT BU_EXTERN(struct vertexuse *nmg_find_v_in_face,
		    (const struct vertex *,
		     const struct faceuse *));
RT_EXPORT BU_EXTERN(struct vertexuse *nmg_find_v_in_shell,
		    (const struct vertex *v,
		     const struct shell *s,
		     int edges_only));
RT_EXPORT BU_EXTERN(struct vertexuse *nmg_find_pt_in_lu,
		    (const struct loopuse *lu,
		     const point_t pt,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct vertexuse *nmg_find_pt_in_face,
		    (const struct faceuse *fu,
		     const point_t pt,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct vertex *nmg_find_pt_in_shell,
		    (const struct shell *s,
		     const point_t pt,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct vertex *nmg_find_pt_in_model,
		    (const struct model *m,
		     const point_t pt,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_is_vertex_in_edgelist,
		    (const struct vertex *v,
		     const struct bu_list *hd));
RT_EXPORT BU_EXTERN(int nmg_is_vertex_in_looplist,
		    (const struct vertex *v,
		     const struct bu_list *hd,
		     int singletons));
RT_EXPORT BU_EXTERN(struct vertexuse *nmg_is_vertex_in_face,
		    (const struct vertex *v,
		     const struct face *f));
RT_EXPORT BU_EXTERN(int nmg_is_vertex_a_selfloop_in_shell,
		    (const struct vertex *v,
		     const struct shell *s));
RT_EXPORT BU_EXTERN(int nmg_is_vertex_in_facelist,
		    (const struct vertex *v,
		     const struct bu_list *hd));
RT_EXPORT BU_EXTERN(int nmg_is_edge_in_edgelist,
		    (const struct edge *e,
		     const struct bu_list *hd));
RT_EXPORT BU_EXTERN(int nmg_is_edge_in_looplist,
		    (const struct edge *e,
		     const struct bu_list *hd));
RT_EXPORT BU_EXTERN(int nmg_is_edge_in_facelist,
		    (const struct edge *e,
		     const struct bu_list *hd));
RT_EXPORT BU_EXTERN(int nmg_is_loop_in_facelist,
		    (const struct loop *l,
		     const struct bu_list *fu_hd));

/* Tabulation routines */
RT_EXPORT BU_EXTERN(void nmg_vertex_tabulate,
		    (struct bu_ptbl *tab,
		     const long *magic_p));
RT_EXPORT BU_EXTERN(void nmg_vertexuse_normal_tabulate,
		    (struct bu_ptbl *tab,
		     const long *magic_p));
RT_EXPORT BU_EXTERN(void nmg_edgeuse_tabulate,
		    (struct bu_ptbl *tab,
		     const long *magic_p));
RT_EXPORT BU_EXTERN(void nmg_edge_tabulate,
		    (struct bu_ptbl *tab,
		     const long *magic_p));
RT_EXPORT BU_EXTERN(void nmg_edge_g_tabulate,
		    (struct bu_ptbl *tab,
		     const long	*magic_p));
RT_EXPORT BU_EXTERN(void nmg_face_tabulate,
		    (struct bu_ptbl *tab,
		     const long *magic_p));
RT_EXPORT BU_EXTERN(void nmg_edgeuse_with_eg_tabulate,
		    (struct bu_ptbl *tab,
		     const struct edge_g_lseg *eg));
RT_EXPORT BU_EXTERN(void nmg_edgeuse_on_line_tabulate,
		    (struct bu_ptbl		*tab,
		     const long			*magic_p,
		     const point_t		pt,
		     const vect_t		dir,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_e_and_v_tabulate,
		    (struct bu_ptbl		*eutab,
		     struct bu_ptbl		*vtab,
		     const long		*magic_p));
RT_EXPORT BU_EXTERN(int nmg_2edgeuse_g_coincident,
		    (const struct edgeuse	*eu1,
		     const struct edgeuse	*eu2,
		     const struct bn_tol	*tol));

/* From nmg_extrude.c */
RT_EXPORT BU_EXTERN(void nmg_translate_face,
		    (struct faceuse *fu,
		     const vect_t Vec,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_extrude_face,
		    (struct faceuse *fu,
		     const vect_t Vec,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct vertexuse *nmg_find_vertex_in_lu,
		    (const struct vertex *v,
		     const struct loopuse *lu));
RT_EXPORT BU_EXTERN(void nmg_fix_overlapping_loops,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_break_crossed_loops,
		    (struct shell *is,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct shell *nmg_extrude_cleanup,
		    (struct shell *is,
		     const int is_void,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_hollow_shell,
		    (struct shell *s,
		     const fastf_t thick,
		     const int approximate,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct shell *nmg_extrude_shell,
		    (struct shell *s,
		     const fastf_t dist,
		     const int normal_ward,
		     const int approximate,
		     const struct bn_tol *tol));

/* From nmg_pr.c */
RT_EXPORT BU_EXTERN(char *nmg_orientation,
		    (int orientation));
RT_EXPORT BU_EXTERN(void nmg_pr_orient,
		    (int orientation,
		     const char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_m,
		    (const struct model *m));
RT_EXPORT BU_EXTERN(void nmg_pr_r,
		    (const struct nmgregion *r,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_sa,
		    (const struct shell_a *sa,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_lg,
		    (const struct loop_g *lg,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_fg,
		    (const long *magic,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_s,
		    (const struct shell *s,
		     char *h));
RT_EXPORT BU_EXTERN(void  nmg_pr_s_briefly,
		    (const struct shell *s,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_f,
		    (const struct face *f,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_fu,
		    (const struct faceuse *fu,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_fu_briefly,
		    (const struct faceuse *fu,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_l,
		    (const struct loop *l,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_lu,
		    (const struct loopuse *lu,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_lu_briefly,
		    (const struct loopuse *lu,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_eg,
		    (const long *eg,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_e,
		    (const struct edge *e,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_eu,
		    (const struct edgeuse *eu,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_eu_briefly,
		    (const struct edgeuse *eu,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_eu_endpoints,
		    (const struct edgeuse *eu,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_vg,
		    (const struct vertex_g *vg,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_v,
		    (const struct vertex *v,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_vu,
		    (const struct vertexuse *vu,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_vu_briefly,
		    (const struct vertexuse *vu,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_pr_vua,
		    (const long *magic_p,
		     char *h));
RT_EXPORT BU_EXTERN(void nmg_euprint,
		    (const char *str,
		     const struct edgeuse *eu));
RT_EXPORT BU_EXTERN(void nmg_pr_ptbl,
		    (const char *title,
		     const struct bu_ptbl *tbl,
		     int verbose));
RT_EXPORT BU_EXTERN(void nmg_pr_ptbl_vert_list,
		    (const char *str,
		     const struct bu_ptbl *tbl,
		     const fastf_t *mag));
RT_EXPORT BU_EXTERN(void nmg_pr_one_eu_vecs,
		    (const struct edgeuse *eu,
		     const vect_t xvec,
		     const vect_t yvec,
		     const vect_t zvec,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_pr_fu_around_eu_vecs,
		    (const struct edgeuse *eu,
		     const vect_t xvec,
		     const vect_t yvec,
		     const vect_t zvec,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_pr_fu_around_eu,
		    (const struct edgeuse *eu,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_pl_lu_around_eu,
		    (const struct edgeuse *eu));
RT_EXPORT BU_EXTERN(void nmg_pr_fus_in_fg,
		    (const long *fg_magic));

/* From nmg_misc.c */
RT_EXPORT BU_EXTERN(struct rt_bot_internal *nmg_bot,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int rt_dist_pt3_line3,
		    (fastf_t		*dist,
		     point_t		pca,
		     const point_t	a,
		     const point_t	p,
		     const vect_t	dir,
		     const struct bn_tol *tol));

RT_EXPORT BU_EXTERN(int rt_dist_line3_line3,
		    (fastf_t dist[2],
		     const point_t p1,
		     const point_t p2,
		     const vect_t d1,
		     const vect_t d2,
		     const struct bn_tol *tol));

RT_EXPORT BU_EXTERN(int nmg_snurb_calc_lu_uv_orient,
		    (const struct loopuse *lu));
RT_EXPORT BU_EXTERN(void nmg_snurb_fu_eval,
		    (const struct faceuse *fu,
		     const fastf_t u,
		     const fastf_t v,
		     point_t pt_on_srf));
RT_EXPORT BU_EXTERN(void nmg_snurb_fu_get_norm,
		    (const struct faceuse *fu,
		     const fastf_t u,
		     const fastf_t v,
		     vect_t norm));
RT_EXPORT BU_EXTERN(void nmg_snurb_fu_get_norm_at_vu,
		    (const struct faceuse *fu,
		     const struct vertexuse *vu,
		     vect_t norm));
RT_EXPORT BU_EXTERN(void nmg_find_zero_length_edges,
		    (const struct model *m));
RT_EXPORT BU_EXTERN(struct face *nmg_find_top_face_in_dir,
		    (const struct shell *s,
		     int dir, long *flags));
RT_EXPORT BU_EXTERN(struct face *nmg_find_top_face,
		    (const struct shell *s,
		     int *dir,
		     long *flags));
RT_EXPORT BU_EXTERN(int nmg_find_outer_and_void_shells,
		    (struct nmgregion *r,
		     struct bu_ptbl ***shells,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_mark_edges_real,
		    (const long *magic_p));
RT_EXPORT BU_EXTERN(void nmg_tabulate_face_g_verts,
		    (struct bu_ptbl *tab,
		     const struct face_g_plane *fg));
RT_EXPORT BU_EXTERN(void nmg_isect_shell_self,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_next_radial_eu,
		    (const struct edgeuse *eu,
		     const struct shell *s,
		     const int wires));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_prev_radial_eu,
		    (const struct edgeuse *eu,
		     const struct shell *s,
		     const int wires));
RT_EXPORT BU_EXTERN(int nmg_radial_face_count,
		    (const struct edgeuse *eu,
		     const struct shell *s));
RT_EXPORT BU_EXTERN(int nmg_check_closed_shell,
		    (const struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_move_lu_between_fus,
		    (struct faceuse *dest,
		     struct faceuse *src,
		     struct loopuse *lu));
RT_EXPORT BU_EXTERN(void nmg_loop_plane_newell,
		    (const struct loopuse *lu,
		     plane_t pl));
RT_EXPORT BU_EXTERN(fastf_t nmg_loop_plane_area,
		    (const struct loopuse *lu,
		     plane_t pl));
RT_EXPORT BU_EXTERN(int nmg_calc_face_plane,
		    (struct faceuse *fu_in,
		     plane_t pl));
RT_EXPORT BU_EXTERN(int nmg_calc_face_g,
		    (struct faceuse *fu));
RT_EXPORT BU_EXTERN(fastf_t nmg_faceuse_area,
		    (const struct faceuse *fu));
RT_EXPORT BU_EXTERN(fastf_t nmg_shell_area,
		    (const struct shell *s));
RT_EXPORT BU_EXTERN(fastf_t nmg_region_area,
		    (const struct nmgregion *r));
RT_EXPORT BU_EXTERN(fastf_t nmg_model_area,
		    (const struct model *m));
/* Some stray rt_ plane functions here */
RT_EXPORT BU_EXTERN(void nmg_purge_unwanted_intersection_points,
		    (struct bu_ptbl *vert_list,
		     fastf_t *mag,
		     const struct faceuse *fu,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_in_or_ref,
		    (struct vertexuse *vu,
		     struct bu_ptbl *b));
RT_EXPORT BU_EXTERN(void nmg_rebound,
		    (struct model *m,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_count_shell_kids,
		    (const struct model *m,
		     unsigned long *total_wires,
		     unsigned long *total_faces,
		     unsigned long *total_points));
RT_EXPORT BU_EXTERN(void nmg_close_shell,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct shell *nmg_dup_shell ,
		    (struct shell *s,
		     long ***copy_tbl,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_pop_eu,
		    (struct bu_ptbl *stack));
RT_EXPORT BU_EXTERN(void nmg_reverse_radials,
		    (struct faceuse *fu,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_reverse_face_and_radials,
		    (struct faceuse *fu,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_shell_is_void,
		    (const struct shell *s));
RT_EXPORT BU_EXTERN(void nmg_propagate_normals,
		    (struct faceuse *fu_in,
		     long *flags,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_connect_same_fu_orients,
		    (struct shell *s));
RT_EXPORT BU_EXTERN(void nmg_fix_decomposed_shell_normals,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct model *nmg_mk_model_from_region,
		    (struct nmgregion *r,
		     int reindex));
RT_EXPORT BU_EXTERN(void nmg_fix_normals,
		    (struct shell *s_orig,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_break_long_edges,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct faceuse *nmg_mk_new_face_from_loop,
		    (struct loopuse *lu));
RT_EXPORT BU_EXTERN(int nmg_split_loops_into_faces,
		    (long *magic_p,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_decompose_shell,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_stash_model_to_file,
		    (const char *filename,
		     const struct model *m,
		     const char *title));
RT_EXPORT BU_EXTERN(int nmg_unbreak_region_edges,
		    (long *magic_p));
/* rt_dist_pt3_line3 */
RT_EXPORT BU_EXTERN(int nmg_mv_shell_to_region,
		    (struct shell *s,
		     struct nmgregion *r));
RT_EXPORT BU_EXTERN(int nmg_find_isect_faces,
		    (const struct vertex *new_v,
		     struct bu_ptbl *faces,
		     int *free_edges,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_simple_vertex_solve,
		    (struct vertex *new_v,
		     const struct bu_ptbl *faces,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_ck_vert_on_fus,
		    (const struct vertex *v,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_make_faces_at_vert,
		    (struct vertex *new_v,
		     struct bu_ptbl *int_faces,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_kill_cracks_at_vertex,
		    (const struct vertex *vp));
RT_EXPORT BU_EXTERN(int nmg_complex_vertex_solve,
		    (struct vertex *new_v,
		     const struct bu_ptbl *faces,
		     const int free_edges,
		     const int approximate,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_bad_face_normals,
		    (const struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_faces_are_radial,
		    (const struct faceuse *fu1,
		     const struct faceuse *fu2));
RT_EXPORT BU_EXTERN(int nmg_move_edge_thru_pt,
		    (struct edgeuse *mv_eu,
		     const point_t pt,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_vlist_to_wire_edges,
		    (struct shell *s,
		     const struct bu_list *vhead));
RT_EXPORT BU_EXTERN(void nmg_follow_free_edges_to_vertex,
		    (const struct vertex *vpa,
		     const struct vertex *vpb,
		     struct bu_ptbl *bad_verts,
		     const struct shell *s,
		     const struct edgeuse *eu,
		     struct bu_ptbl *verts,
		     int *found));
RT_EXPORT BU_EXTERN(void nmg_glue_face_in_shell,
		    (const struct faceuse *fu,
		     struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_open_shells_connect,
		    (struct shell *dst,
		     struct shell *src,
		     const long **copy_tbl,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_in_vert,
		    (struct vertex *new_v,
		     const int approximate,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_mirror_model,
		    (struct model *m));
RT_EXPORT BU_EXTERN(int nmg_kill_cracks,
		    (struct shell *s));
RT_EXPORT BU_EXTERN(int nmg_kill_zero_length_edgeuses,
		    (struct model *m));
RT_EXPORT BU_EXTERN(void nmg_make_faces_within_tol,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_intersect_loops_self,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct edge_g_cnurb *rt_join_cnurbs,
		    (struct bu_list *crv_head));
RT_EXPORT BU_EXTERN(struct edge_g_cnurb *rt_arc2d_to_cnurb,
		    (point_t i_center,
		     point_t i_start,
		     point_t i_end,
		     int point_type,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_break_edge_at_verts,
		    (struct edge *e,
		     struct bu_ptbl *verts,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_isect_shell_self,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(fastf_t nmg_loop_plane_area,
		    (const struct loopuse *lu,
		     plane_t pl));
RT_EXPORT BU_EXTERN(int nmg_break_edges,
		    (long *magic_p,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_lu_is_convex,
		    (struct loopuse *lu,
		     const struct bn_tol *tol));
#ifdef SEEN_RTGEOM_H
RT_EXPORT BU_EXTERN(int nmg_to_arb,
		    (const struct model *m,
		     struct rt_arb_internal *arb_int));
RT_EXPORT BU_EXTERN(int nmg_to_tgc,
		    (const struct model *m,
		     struct rt_tgc_internal *tgc_int,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_to_poly,
		    (const struct model *m,
		     struct rt_pg_internal *poly_int,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct rt_bot_internal *nmg_bot,
		    (struct shell *s,
		     const struct bn_tol *tol));
#endif

RT_EXPORT BU_EXTERN(int nmg_simplify_shell_edges,
		    (struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_edge_collapse,
		    (struct model *m,
		     const struct bn_tol *tol,
		     const fastf_t tol_coll,
		     const fastf_t min_angle));

/* g_bot.c */
RT_EXPORT BU_EXTERN(int rt_bot_edge_in_list,
		    (const int v1,
		     const int v2,
		     const int edge_list[],
		     const int edge_count0));
RT_EXPORT BU_EXTERN(int rt_bot_plot,
		    (struct bu_list		*vhead,
		     struct rt_db_internal	*ip,
		     const struct rt_tess_tol *ttol,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int rt_bot_plot_poly,
		    (struct bu_list		*vhead,
		     struct rt_db_internal	*ip,
		     const struct rt_tess_tol *ttol,
		     const struct bn_tol	*tol));
#ifdef SEEN_RTGEOM_H
RT_EXPORT BU_EXTERN(int rt_bot_find_v_nearest_pt2,
		    (const struct rt_bot_internal *bot,
		     const point_t	pt2,
		     const mat_t	mat));
RT_EXPORT BU_EXTERN(int rt_bot_find_e_nearest_pt2,
		    (int *vert1,
		     int *vert2,
		     const struct rt_bot_internal *bot,
		     const point_t	pt2,
		     const mat_t	mat));
RT_EXPORT BU_EXTERN(int rt_bot_vertex_fuse,
		    (struct rt_bot_internal *bot));
RT_EXPORT BU_EXTERN(int rt_bot_face_fuse,
		    (struct rt_bot_internal *bot));
RT_EXPORT BU_EXTERN(int rt_bot_condense,
		    (struct rt_bot_internal *bot));
RT_EXPORT BU_EXTERN(int rt_smooth_bot,
		    (struct rt_bot_internal *bot,
		     char *bot_name,
		     struct db_i *dbip,
		     fastf_t normal_tolerance_angle));

#endif
RT_EXPORT BU_EXTERN(int rt_bot_same_orientation,
		    (const int *a,
		     const int *b));

/* From nmg_tri.c */
RT_EXPORT BU_EXTERN(void nmg_triangulate_shell,
		    (struct shell *s,
		     const struct bn_tol  *tol));


RT_EXPORT BU_EXTERN(void nmg_triangulate_model,
		    (struct model *m,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_triangulate_fu,
		    (struct faceuse *fu,
		     const struct bn_tol *tol));

/* nmg_manif.c */
RT_EXPORT BU_EXTERN(int nmg_dangling_face,
		    (const struct faceuse *fu,
		     const char *manifolds));
/* static paint_face */
/* static set_edge_sub_manifold */
/* static set_loop_sub_manifold */
/* static set_face_sub_manifold */
RT_EXPORT BU_EXTERN(char *nmg_shell_manifolds,
		    (struct shell *sp,
		     char *tbl));
RT_EXPORT BU_EXTERN(char *nmg_manifolds,
		    (struct model *m));

/* g_nmg.c */
RT_EXPORT BU_EXTERN(int nmg_ray_segs,
		    (struct ray_data	*rd));

/* g_torus.c */
RT_EXPORT BU_EXTERN(int rt_num_circular_segments,
		    (double maxerr,
		     double radius));

/* tcl.c */
RT_EXPORT BU_EXTERN(int rt_tcl_parse_ray,
		    (Tcl_Interp *interp,
		     struct xray *rp,
		     const char *const*argv));
RT_EXPORT BU_EXTERN(void rt_tcl_pr_cutter,
		    (Tcl_Interp *interp,
		     const union cutter *cutp));
RT_EXPORT BU_EXTERN(int rt_tcl_cutter,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char *const*argv));
RT_EXPORT BU_EXTERN(void rt_tcl_pr_hit,
		    (Tcl_Interp *interp,
		     struct hit *hitp,
		     const struct seg *segp,
		     const struct xray *rayp,
		     int flipflag));
RT_EXPORT BU_EXTERN(void rt_tcl_setup,
		    (Tcl_Interp *interp));
#ifdef BRLCAD_DEBUG
RT_EXPORT BU_EXTERN(int Sysv_d_Init,
		    (Tcl_Interp *interp));
RT_EXPORT BU_EXTERN(int Rt_d_Init,
		    (Tcl_Interp *interp));
#else
RT_EXPORT BU_EXTERN(int Sysv_Init,
		    (Tcl_Interp *interp));
RT_EXPORT BU_EXTERN(int Rt_Init,
		    (Tcl_Interp *interp));
#endif
RT_EXPORT BU_EXTERN(void db_full_path_appendresult,
		    (Tcl_Interp *interp,
		     const struct db_full_path *pp));
RT_EXPORT BU_EXTERN(int tcl_obj_to_int_array,
		    (Tcl_Interp *interp,
		     Tcl_Obj *list,
		     int **array,
		     int *array_len));


RT_EXPORT BU_EXTERN(int tcl_obj_to_fastf_array,
		    (Tcl_Interp *interp,
		     Tcl_Obj *list,
		     fastf_t **array,
		     int *array_len));

RT_EXPORT BU_EXTERN(int tcl_list_to_int_array,
		    (Tcl_Interp *interp,
		     char *char_list,
		     int **array,
		     int *array_len));

RT_EXPORT BU_EXTERN(int tcl_list_to_fastf_array,
		    (Tcl_Interp *interp,
		     char *char_list,
		     fastf_t **array,
		     int *array_len));


/* g_rhc.c */
RT_EXPORT BU_EXTERN(int rt_mk_hyperbola,
		    (struct rt_pt_node *pts,
		     fastf_t r,
		     fastf_t b,
		     fastf_t c,
		     fastf_t dtol,
		     fastf_t ntol));



/* nmg_class.c */
RT_EXPORT BU_EXTERN(int nmg_classify_pt_loop,
		    (const point_t pt,
		     const struct loopuse *lu,
		     const struct bn_tol *tol));

RT_EXPORT BU_EXTERN(int nmg_classify_s_vs_s,
		    (struct shell *s,
		     struct shell *s2,
		     const struct bn_tol *tol));

RT_EXPORT BU_EXTERN(int nmg_classify_lu_lu,
		    (const struct loopuse *lu1,
		     const struct loopuse *lu2,
		     const struct bn_tol *tol));

RT_EXPORT BU_EXTERN(int nmg_class_pt_f,
		    (const point_t pt,
		     const struct faceuse *fu,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_class_pt_s,
		    (const point_t pt,
		     const struct shell *s,
		     const int in_or_out_only,
		     const struct bn_tol *tol));

/* From nmg_pt_fu.c */
RT_EXPORT BU_EXTERN(int nmg_eu_is_part_of_crack,
		    (const struct edgeuse *eu));

RT_EXPORT BU_EXTERN(int nmg_class_pt_lu_except,
		    (point_t		pt,
		     const struct loopuse	*lu,
		     const struct edge		*e_p,
		     const struct bn_tol	*tol));

RT_EXPORT BU_EXTERN(int nmg_class_pt_fu_except,
		    (const point_t pt,
		     const struct faceuse *fu,
		     const struct loopuse *ignore_lu,
		     void (*eu_func)(), void (*vu_func)(),
		     const char *priv,
		     const int call_on_hits,
		     const int in_or_out_only,
		     const struct bn_tol *tol));

/* From nmg_plot.c */
RT_EXPORT BU_EXTERN(void nmg_pl_shell,
		    (FILE *fp,
		     const struct shell *s,
		     int fancy));

RT_EXPORT BU_EXTERN(void nmg_vu_to_vlist,
		    (struct bu_list *vhead,
		     const struct vertexuse	*vu));
RT_EXPORT BU_EXTERN(void nmg_eu_to_vlist,
		    (struct bu_list *vhead,
		     const struct bu_list	*eu_hd));
RT_EXPORT BU_EXTERN(void nmg_lu_to_vlist,
		    (struct bu_list *vhead,
		     const struct loopuse	*lu,
		     int			poly_markers,
		     const vectp_t		normal));
RT_EXPORT BU_EXTERN(void nmg_snurb_fu_to_vlist,
		    (struct bu_list		*vhead,
		     const struct faceuse	*fu,
		     int			poly_markers));
RT_EXPORT BU_EXTERN(void nmg_s_to_vlist,
		    (struct bu_list		*vhead,
		     const struct shell	*s,
		     int			poly_markers));
RT_EXPORT BU_EXTERN(void nmg_r_to_vlist,
		    (struct bu_list		*vhead,
		     const struct nmgregion	*r,
		     int			poly_markers));
RT_EXPORT BU_EXTERN(void nmg_m_to_vlist,
		    (struct bu_list	*vhead,
		     struct model	*m,
		     int		poly_markers));
RT_EXPORT BU_EXTERN(void nmg_offset_eu_vert,
		    (point_t			base,
		     const struct edgeuse	*eu,
		     const vect_t		face_normal,
		     int			tip));
	/* plot */
RT_EXPORT BU_EXTERN(void nmg_pl_v,
		    (FILE	*fp,
		     const struct vertex *v,
		     long *b));
RT_EXPORT BU_EXTERN(void nmg_pl_e,
		    (FILE *fp,
		     const struct edge *e,
		     long *b,
		     int red,
		     int green,
		     int blue));
RT_EXPORT BU_EXTERN(void nmg_pl_eu,
		    (FILE *fp,
		     const struct edgeuse *eu,
		     long *b,
		     int red,
		     int green,
		     int blue));
RT_EXPORT BU_EXTERN(void nmg_pl_lu,
		    (FILE *fp,
		     const struct loopuse *fu,
		     long *b,
		     int red,
		     int green,
		     int blue));
RT_EXPORT BU_EXTERN(void nmg_pl_fu,
		    (FILE *fp,
		     const struct faceuse *fu,
		     long *b,
		     int red,
		     int green,
		     int blue));
RT_EXPORT BU_EXTERN(void nmg_pl_s,
		    (FILE *fp,
		     const struct shell *s));
RT_EXPORT BU_EXTERN(void nmg_pl_r,
		    (FILE *fp,
		     const struct nmgregion *r));
RT_EXPORT BU_EXTERN(void nmg_pl_m,
		    (FILE *fp,
		     const struct model *m));
RT_EXPORT BU_EXTERN(void nmg_vlblock_v,
		    (struct bn_vlblock *vbp,
		     const struct vertex *v,
		     long *tab));
RT_EXPORT BU_EXTERN(void nmg_vlblock_e,
		    (struct bn_vlblock *vbp,
		     const struct edge *e,
		     long *tab,
		     int red,
		     int green,
		     int blue,
		     int fancy));
RT_EXPORT BU_EXTERN(void nmg_vlblock_eu,
		    (struct bn_vlblock *vbp,
		     const struct edgeuse *eu,
		     long *tab,
		     int red,
		     int green,
		     int blue,
		     int fancy,
		     int loopnum));
RT_EXPORT BU_EXTERN(void nmg_vlblock_euleft,
		    (struct bu_list			*vh,
		     const struct edgeuse		*eu,
		     const point_t			center,
		     const mat_t			mat,
		     const vect_t			xvec,
		     const vect_t			yvec,
		     double				len,
		     const struct bn_tol		*tol));
RT_EXPORT BU_EXTERN(void nmg_vlblock_around_eu,
		    (struct bn_vlblock		*vbp,
		     const struct edgeuse	*arg_eu,
		     long			*tab,
		     int			fancy,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_vlblock_lu,
		    (struct bn_vlblock *vbp,
		     const struct loopuse *lu,
		     long *tab,
		     int red,
		     int green,
		     int blue,
		     int fancy,
		     int loopnum));
RT_EXPORT BU_EXTERN(void nmg_vlblock_fu,
		    (struct bn_vlblock *vbp,
		     const struct faceuse *fu,
		     long *tab, int fancy));
RT_EXPORT BU_EXTERN(void nmg_vlblock_s,
		    (struct bn_vlblock *vbp,
		     const struct shell *s,
		     int fancy));
RT_EXPORT BU_EXTERN(void nmg_vlblock_r,
		    (struct bn_vlblock *vbp,
		     const struct nmgregion *r,
		     int fancy));
RT_EXPORT BU_EXTERN(void nmg_vlblock_m,
		    (struct bn_vlblock *vbp,
		     const struct model *m,
		     int fancy));
/* visualization helper routines */
RT_EXPORT BU_EXTERN(void nmg_pl_edges_in_2_shells,
		    (struct bn_vlblock	*vbp,
		     long			*b,
		     const struct edgeuse	*eu,
		     int			fancy,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_pl_isect,
		    (const char		*filename,
		     const struct shell	*s,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_pl_comb_fu,
		    (int num1,
		     int num2,
		     const struct faceuse *fu1));
RT_EXPORT BU_EXTERN(void nmg_pl_2fu,
		    (const char *str,
		     int num,
		     const struct faceuse *fu1,
		     const struct faceuse *fu2,
		     int show_mates));
	/* graphical display of classifier results */
RT_EXPORT BU_EXTERN(void nmg_show_broken_classifier_stuff,
		    (long	*p,
		     long	*classlist[4],
		     int	all_new,
		     int	fancy,
		     const char	*a_string));
RT_EXPORT BU_EXTERN(void nmg_face_plot,
		    (const struct faceuse *fu));
RT_EXPORT BU_EXTERN(void nmg_2face_plot,
		    (const struct faceuse *fu1,
		     const struct faceuse *fu2));
RT_EXPORT BU_EXTERN(void nmg_face_lu_plot,
		    (const struct loopuse *lu,
		     const struct vertexuse *vu1,
		     const struct vertexuse *vu2));
RT_EXPORT BU_EXTERN(void nmg_plot_lu_ray,
		    (const struct loopuse		*lu,
		     const struct vertexuse		*vu1,
		     const struct vertexuse		*vu2,
		     const vect_t			left));
RT_EXPORT BU_EXTERN(void nmg_plot_ray_face,
		    (const char *fname,
		     point_t pt,
		     const vect_t dir,
		     const struct faceuse *fu));
RT_EXPORT BU_EXTERN(void nmg_plot_lu_around_eu,
		    (const char		*prefix,
		     const struct edgeuse	*eu,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int nmg_snurb_to_vlist,
		    (struct bu_list		*vhead,
		     const struct face_g_snurb	*fg,
		     int			n_interior));
RT_EXPORT BU_EXTERN(void nmg_cnurb_to_vlist,
		    (struct bu_list *vhead,
		     const struct edgeuse *eu,
		     int n_interior,
		     int cmd));
RT_EXPORT extern void (*nmg_plot_anim_upcall)();
RT_EXPORT extern void (*nmg_vlblock_anim_upcall)();
RT_EXPORT extern void (*nmg_mged_debug_display_hack)();
RT_EXPORT extern double nmg_eue_dist;

/* from nmg_mesh.c */
RT_EXPORT BU_EXTERN(int nmg_mesh_two_faces,
		    (struct faceuse *fu1,
		     struct faceuse *fu2,
		     const struct bn_tol	*tol));

RT_EXPORT BU_EXTERN(void nmg_radial_join_eu,
		    (struct edgeuse *eu1,
		     struct edgeuse *eu2,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_mesh_faces,
		    (struct faceuse *fu1,
		     struct faceuse *fu2,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_mesh_face_shell,
		    (struct faceuse *fu1,
		     struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_mesh_shell_shell,
		    (struct shell *s1,
		     struct shell *s2,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(double nmg_measure_fu_angle,
		    (const struct edgeuse *eu,
		     const vect_t xvec,
		     const vect_t yvec,
		     const vect_t zvec));

/* from nmg_bool.c */
RT_EXPORT BU_EXTERN(struct nmgregion *nmg_do_bool,
		    (struct nmgregion *s1,
		     struct nmgregion *s2,
		     const int oper, const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_shell_coplanar_face_merge,
		    (struct shell *s,
		     const struct bn_tol *tol,
		     const int simplify));
RT_EXPORT BU_EXTERN(int nmg_two_region_vertex_fuse,
		    (struct nmgregion *r1,
		     struct nmgregion *r2,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(union tree *nmg_booltree_leaf_tess,
		    (struct db_tree_state *tsp,
		     struct db_full_path *pathp,
		     struct rt_db_internal *ip,
		     genptr_t client_data));
RT_EXPORT BU_EXTERN(union tree *nmg_booltree_leaf_tnurb,
		    (struct db_tree_state *tsp,
		     struct db_full_path *pathp,
		     struct rt_db_internal *ip,
		     genptr_t client_data));
RT_EXPORT BU_EXTERN(union tree *nmg_booltree_evaluate,
		    (union tree *tp,
		     const struct bn_tol *tol,
		     struct resource *resp));
RT_EXPORT BU_EXTERN(int nmg_boolean,
		    (union tree *tp,
		     struct model *m,
		     const struct bn_tol *tol,
		     struct resource *resp));

/* from nmg_class.c */
RT_EXPORT BU_EXTERN(void nmg_class_shells,
		    (struct shell *sA,
		     struct shell *sB,
		     long *classlist[4],
		     const struct bn_tol *tol));

/* from nmg_fcut.c */
/* static void ptbl_vsort */
RT_EXPORT BU_EXTERN(int nmg_ck_vu_ptbl,
		    (struct bu_ptbl	*p,
		     struct faceuse	*fu));
RT_EXPORT BU_EXTERN(double nmg_vu_angle_measure,
		    (struct vertexuse	*vu,
		     vect_t x_dir,
		     vect_t y_dir,
		     int assessment,
		     int in));
#if 0
RT_EXPORT BU_EXTERN(int nmg_is_v_on_rs_list,
		    (const struct nmg_ray_state *rs,
		     const struct vertex		*v));
RT_EXPORT BU_EXTERN(int nmg_assess_eu,
		    (struct edgeuse *eu,
		     int			forw,
		     struct nmg_ray_state	*rs,
		     int			pos));
RT_EXPORT BU_EXTERN(int nmg_assess_vu,
		    (struct nmg_ray_state	*rs,
		     int			pos));
RT_EXPORT BU_EXTERN(void nmg_pr_vu_stuff,
		    (const struct nmg_vu_stuff *vs));
#endif
RT_EXPORT BU_EXTERN(int nmg_wedge_class,
		    (int	ass,	/* assessment of two edges forming wedge */
		     double	a,
		     double	b));
#if 0
RT_EXPORT BU_EXTERN(int nmg_face_coincident_vu_sort,
		    (struct nmg_ray_state	*rs,
		     int			start,
		     int			end));
#endif
RT_EXPORT BU_EXTERN(void nmg_sanitize_fu,
		    (struct faceuse	*fu));
#if 0
RT_EXPORT BU_EXTERN(void nmg_face_rs_init,
		    (struct nmg_ray_state	*rs,
		     struct bu_ptbl	*b,
		     struct faceuse	*fu1,
		     struct faceuse	*fu2,
		     point_t		pt,
		     vect_t		dir,
		     struct edge_g_lseg		*eg,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_edge_geom_isect_line,
		    (struct edgeuse		*eu,
		     struct nmg_ray_state	*rs,
		     const char		*reason));
#endif
RT_EXPORT BU_EXTERN(void nmg_unlist_v,
		    (struct bu_ptbl	*b,
		     fastf_t *mag,
		     struct vertex	*v));
#if 0
RT_EXPORT BU_EXTERN(int mg_onon_fix,
		    (struct nmg_ray_state	*rs,
		     struct bu_ptbl		*b,
		     struct bu_ptbl		*ob,
		     fastf_t			*mag,
		     fastf_t			*omag));
#endif
RT_EXPORT BU_EXTERN(struct edge_g_lseg *nmg_face_cutjoin,
		    (struct bu_ptbl *b1,
		     struct bu_ptbl *b2,
		     fastf_t *mag1,
		     fastf_t *mag2,
		     struct faceuse *fu1,
		     struct faceuse *fu2,
		     point_t pt,
		     vect_t dir,
		     struct edge_g_lseg *eg,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_fcut_face_2d,
		    (struct bu_ptbl *vu_list,
		     fastf_t *mag,
		     struct faceuse *fu1,
		     struct faceuse *fu2,
		     struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_insert_vu_if_on_edge,
		    (struct vertexuse *vu1,
		     struct vertexuse *vu2,
		     struct edgeuse *new_eu,
		     struct bn_tol *tol));
/* nmg_face_state_transition */

#define nmg_mev(_v, _u)	nmg_me((_v), (struct vertex *)NULL, (_u))

/* From nmg_eval.c */
RT_EXPORT BU_EXTERN(void nmg_ck_lu_orientation,
		    (struct loopuse *lu,
		     const struct bn_tol *tolp));
RT_EXPORT BU_EXTERN(const char *nmg_class_name,
		    (int class_no));
RT_EXPORT BU_EXTERN(void nmg_evaluate_boolean,
		    (struct shell	*sA,
		     struct shell	*sB,
		     int		op,
		     long		*classlist[8],
		     const struct bn_tol	*tol));
#if 0
/* These can't be included because struct nmg_bool_state is in nmg_eval.c */
/* nmg_eval_shell */
/* nmg_eval_action */
/* nmg_eval_plot */
#endif


/* From nmg_rt_isect.c */
RT_EXPORT BU_EXTERN(void nmg_rt_print_hitlist,
		    (struct hitmiss *hl));

RT_EXPORT BU_EXTERN(void nmg_rt_print_hitmiss,
		    (struct hitmiss *a_hit));

RT_EXPORT BU_EXTERN(int nmg_class_ray_vs_shell,
		    (struct xray *rp,
		     const struct shell *s,
		     const int in_or_out_only,
		     const struct bn_tol *tol));

RT_EXPORT BU_EXTERN(void nmg_isect_ray_model,
		    (struct ray_data *rd));
RT_EXPORT BU_EXTERN(int nmg_uv_in_lu,
		    (const fastf_t u,
		     const fastf_t v,
		     const struct loopuse *lu));

/* From nmg_rt_segs.c */
#if 0
/* Don't have "nmg_specific" */
RT_EXPORT BU_EXTERN(int nmg_ray_isect_segs,
		    (struct soltab *stp,
		     struct xray *rp,
		     struct application *ap,
		     struct seg *seghead,
		     struct nmg_specific *nmg_spec));
#endif

/* From nmg_ck.c */
RT_EXPORT BU_EXTERN(void nmg_vvg,
		    (const struct vertex_g *vg));
RT_EXPORT BU_EXTERN(void nmg_vvertex,
		    (const struct vertex *v,
				const struct vertexuse *vup));
RT_EXPORT BU_EXTERN(void nmg_vvua,
		    (const long *vua));
RT_EXPORT BU_EXTERN(void nmg_vvu,
		    (const struct vertexuse *vu,
				const long *up_magic_p));
RT_EXPORT BU_EXTERN(void nmg_veg,
		    (const long *eg));
RT_EXPORT BU_EXTERN(void nmg_vedge,
		    (const struct edge *e,
		     const struct edgeuse *eup));
RT_EXPORT BU_EXTERN(void nmg_veu,
		    (const struct bu_list	*hp,
		     const long *up_magic_p));
RT_EXPORT BU_EXTERN(void nmg_vlg,
		    (const struct loop_g *lg));
RT_EXPORT BU_EXTERN(void nmg_vloop,
		    (const struct loop *l,
		     const struct loopuse *lup));
RT_EXPORT BU_EXTERN(void nmg_vlu,
		    (const struct bu_list	*hp,
		     const long *up));
RT_EXPORT BU_EXTERN(void nmg_vfg,
		    (const struct face_g_plane *fg));
RT_EXPORT BU_EXTERN(void nmg_vface,
		    (const struct face *f,
		     const struct faceuse *fup));
RT_EXPORT BU_EXTERN(void nmg_vfu,
		    (const struct bu_list	*hp,
		     const struct shell *s));
RT_EXPORT BU_EXTERN(void nmg_vshell,
		    (const struct bu_list *hp,
		     const struct nmgregion *r));
RT_EXPORT BU_EXTERN(void nmg_vregion,
		    (const struct bu_list *hp,
		     const struct model *m));
RT_EXPORT BU_EXTERN(void nmg_vmodel,
		    (const struct model *m));

/* checking routines */
RT_EXPORT BU_EXTERN(void nmg_ck_e,
		    (const struct edgeuse *eu,
		     const struct edge *e,
		     const char *str));
RT_EXPORT BU_EXTERN(void nmg_ck_vu,
		    (const long *parent,
		     const struct vertexuse *vu,
		     const char *str));
RT_EXPORT BU_EXTERN(void nmg_ck_eu,
		    (const long *parent,
		     const struct edgeuse *eu,
		     const char *str));
RT_EXPORT BU_EXTERN(void nmg_ck_lg,
		    (const struct loop *l,
		     const struct loop_g *lg,
		     const char *str));
RT_EXPORT BU_EXTERN(void nmg_ck_l,
		    (const struct loopuse *lu,
		     const struct loop *l,
		     const char *str));
RT_EXPORT BU_EXTERN(void nmg_ck_lu,
		    (const long *parent,
		     const struct loopuse *lu,
		     const char *str));
RT_EXPORT BU_EXTERN(void nmg_ck_fg,
		    (const struct face *f,
		     const struct face_g_plane *fg,
		     const char *str));
RT_EXPORT BU_EXTERN(void nmg_ck_f,
		    (const struct faceuse *fu,
		     const struct face *f,
		     const char *str));
RT_EXPORT BU_EXTERN(void nmg_ck_fu,
		    (const struct shell *s,
		     const struct faceuse *fu,
		     const char *str));
RT_EXPORT BU_EXTERN(int nmg_ck_eg_verts,
		    (const struct edge_g_lseg *eg,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_ck_geometry,
		    (const struct model *m,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_ck_face_worthless_edges,
		    (const struct faceuse *fu));
RT_EXPORT BU_EXTERN(void nmg_ck_lueu,
		    (const struct loopuse *lu, const char *s));
RT_EXPORT BU_EXTERN(int nmg_check_radial,
		    (const struct edgeuse *eu, const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_eu_2s_orient_bad,
		    (const struct edgeuse	*eu,
		     const struct shell	*s1,
		     const struct shell	*s2,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int nmg_ck_closed_surf,
		    (const struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_ck_closed_region,
		    (const struct nmgregion *r,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_ck_v_in_2fus,
		    (const struct vertex *vp,
		     const struct faceuse *fu1,
		     const struct faceuse *fu2,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_ck_vs_in_region,
		    (const struct nmgregion *r,
		     const struct bn_tol *tol));


/* From nmg_inter.c */
RT_EXPORT BU_EXTERN(struct vertexuse *nmg_make_dualvu,
		    (struct vertex *v,
		     struct faceuse *fu,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(struct vertexuse *nmg_enlist_vu,
		    (struct nmg_inter_struct	*is,
		     const struct vertexuse *vu,
		     struct vertexuse *dualvu,
		     fastf_t dist));
RT_EXPORT BU_EXTERN(void nmg_isect2d_prep,
		    (struct nmg_inter_struct *is,
		     const long *assoc_use));
RT_EXPORT BU_EXTERN(void nmg_isect2d_cleanup,
		    (struct nmg_inter_struct *is));
RT_EXPORT BU_EXTERN(void nmg_isect2d_final_cleanup,
		    ());
RT_EXPORT BU_EXTERN(void nmg_isect_vert2p_face2p,
		    (struct nmg_inter_struct *is,
		     struct vertexuse *vu1,
		     struct faceuse *fu2));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_break_eu_on_v,
		    (struct edgeuse *eu1,
		     struct vertex *v2,
		     struct faceuse *fu,
		     struct nmg_inter_struct *is));
RT_EXPORT BU_EXTERN(void nmg_break_eg_on_v,
		    (const struct edge_g_lseg	*eg,
		     struct vertex		*v,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int nmg_isect_2colinear_edge2p,
		    (struct edgeuse	*eu1,
		     struct edgeuse	*eu2,
		     struct faceuse		*fu,
		     struct nmg_inter_struct	*is,
		     struct bu_ptbl		*l1,
		     struct bu_ptbl		*l2));
RT_EXPORT BU_EXTERN(int nmg_isect_edge2p_edge2p,
		    (struct nmg_inter_struct	*is,
		     struct edgeuse		*eu1,
		     struct edgeuse		*eu2,
		     struct faceuse		*fu1,
		     struct faceuse		*fu2));
RT_EXPORT BU_EXTERN(int nmg_isect_construct_nice_ray,
		    (struct nmg_inter_struct	*is,
		     struct faceuse		*fu2));
RT_EXPORT BU_EXTERN(void nmg_enlist_one_vu,
		    (struct nmg_inter_struct	*is,
		     const struct vertexuse	*vu,
		     fastf_t			dist));
RT_EXPORT BU_EXTERN(int	nmg_isect_line2_edge2p,
		    (struct nmg_inter_struct	*is,
		     struct bu_ptbl		*list,
		     struct edgeuse		*eu1,
		     struct faceuse		*fu1,
		     struct faceuse		*fu2));
RT_EXPORT BU_EXTERN(void nmg_isect_line2_vertex2,
		    (struct nmg_inter_struct	*is,
		     struct vertexuse	*vu1,
		     struct faceuse		*fu1));
RT_EXPORT BU_EXTERN(int nmg_isect_two_ptbls,
		    (struct nmg_inter_struct		*is,
		     const struct bu_ptbl		*t1,
		     const struct bu_ptbl		*t2));
RT_EXPORT BU_EXTERN(struct edge_g_lseg	*nmg_find_eg_on_line,
		    (const long		*magic_p,
		     const point_t		pt,
		     const vect_t		dir,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int nmg_k0eu,
		    (struct vertex	*v));
RT_EXPORT BU_EXTERN(struct vertex *nmg_repair_v_near_v,
		    (struct vertex		*hit_v,
		     struct vertex		*v,
		     const struct edge_g_lseg	*eg1,
		     const struct edge_g_lseg	*eg2,
		     int			bomb,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(struct vertex *nmg_search_v_eg,
		    (const struct edgeuse		*eu,
		     int				second,
		     const struct edge_g_lseg	*eg1,
		     const struct edge_g_lseg	*eg2,
		     struct vertex		*hit_v,
		     const struct bn_tol		*tol));
RT_EXPORT BU_EXTERN(struct vertex *nmg_common_v_2eg,
		    (struct edge_g_lseg	*eg1,
		     struct edge_g_lseg	*eg2,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int nmg_is_vertex_on_inter,
		    (struct vertex *v,
		     struct faceuse *fu1,
		     struct faceuse *fu2,
		     struct nmg_inter_struct *is));
RT_EXPORT BU_EXTERN(void nmg_isect_eu_verts,
		    (struct edgeuse *eu,
		     struct vertex_g *vg1,
		     struct vertex_g *vg2,
		     struct bu_ptbl *verts,
		     struct bu_ptbl *inters,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_isect_eu_eu,
		    (struct edgeuse *eu1,
		     struct vertex_g *vg1a,
		     struct vertex_g *vg1b,
		     vect_t dir1,
		     struct edgeuse *eu2,
		     struct bu_ptbl *verts,
		     struct bu_ptbl *inters,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_isect_eu_fu,
		    (struct nmg_inter_struct *is,
		     struct bu_ptbl		*verts,
		     struct edgeuse		*eu,
		     struct faceuse          *fu));
RT_EXPORT BU_EXTERN(void nmg_isect_fu_jra,
		    (struct nmg_inter_struct	*is,
		     struct faceuse		*fu1,
		     struct faceuse		*fu2,
		     struct bu_ptbl		*eu1_list,
		     struct bu_ptbl		*eu2_list));
RT_EXPORT BU_EXTERN(void nmg_isect_line2_face2pNEW,
		    (struct nmg_inter_struct *is,
		     struct faceuse *fu1, struct faceuse *fu2,
		     struct bu_ptbl *eu1_list,
		     struct bu_ptbl *eu2_list));
RT_EXPORT BU_EXTERN(int	nmg_is_eu_on_line3,
		    (const struct edgeuse	*eu,
		     const point_t		pt,
		     const vect_t		dir,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(struct edge_g_lseg	*nmg_find_eg_between_2fg,
		    (const struct faceuse	*ofu1,
		     const struct faceuse	*fu2,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(struct edgeuse *nmg_does_fu_use_eg,
		    (const struct faceuse	*fu1,
		     const long		*eg));
RT_EXPORT BU_EXTERN(int rt_line_on_plane,
		    (const point_t	pt,
		     const vect_t	dir,
		     const plane_t	plane,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_cut_lu_into_coplanar_and_non,
		    (struct loopuse *lu,
		     plane_t pl,
		     struct nmg_inter_struct *is));
RT_EXPORT BU_EXTERN(void nmg_check_radial_angles,
		    (char *str,
		     struct shell *s,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_faces_can_be_intersected,
		    (struct nmg_inter_struct *bs,
		     const struct faceuse *fu1,
		     const struct faceuse *fu2,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_isect_two_generic_faces,
		    (struct faceuse		*fu1,
		     struct faceuse		*fu2,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_crackshells,
		    (struct shell *s1,
		     struct shell *s2,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_fu_touchingloops,
		    (const struct faceuse *fu));


/* From nmg_index.c */
RT_EXPORT BU_EXTERN(int nmg_index_of_struct,
		    (const long *p));
RT_EXPORT BU_EXTERN(void nmg_m_set_high_bit,
		    (struct model *m));
RT_EXPORT BU_EXTERN(void nmg_m_reindex,
		    (struct model *m, long newindex));
RT_EXPORT BU_EXTERN(void nmg_vls_struct_counts,
		    (struct bu_vls *str,
		     const struct nmg_struct_counts *ctr));
RT_EXPORT BU_EXTERN(void nmg_pr_struct_counts,
		    (const struct nmg_struct_counts *ctr,
		     const char *str));
RT_EXPORT BU_EXTERN(long **nmg_m_struct_count,
		    (struct nmg_struct_counts *ctr,
		     const struct model *m));
RT_EXPORT BU_EXTERN(void nmg_struct_counts,
		    (const struct model	*m,
		     const char		*str));
RT_EXPORT BU_EXTERN(void nmg_merge_models,
		    (struct model *m1,
		     struct model *m2));
RT_EXPORT BU_EXTERN(long nmg_find_max_index,
		    (const struct model *m));

/* From nmg_rt.c */

/* From rt_dspline.c */
RT_EXPORT BU_EXTERN(void rt_dspline_matrix,
		    (mat_t m,const char *type,
		     const double	tension,
		     const double	bias));
RT_EXPORT BU_EXTERN(double rt_dspline4,
		    (mat_t m,
		     double a,
		     double b,
		     double c,
		     double d,
		     double alpha));
RT_EXPORT BU_EXTERN(void rt_dspline4v,
		    (double *pt,
		     const mat_t m,
		     const double *a,
		     const double *b,
		     const double *c,
		     const double *d,
		     const int depth,
		     const double alpha));
RT_EXPORT BU_EXTERN(void rt_dspline_n,
		    (double *r,
		     const mat_t m,
		     const double *knots,
		     const int n,
		     const int depth,
		     const double alpha));

/* From nmg_fuse.c */
RT_EXPORT BU_EXTERN(int nmg_is_common_bigloop,
		    (const struct face *f1,
		     const struct face *f2));
RT_EXPORT BU_EXTERN(void nmg_region_v_unique,
		    (struct nmgregion *r1,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_ptbl_vfuse,
		    (struct bu_ptbl *t,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int	nmg_region_both_vfuse,
		    (struct bu_ptbl *t1,
		     struct bu_ptbl *t2,
		     const struct bn_tol	*tol));
/* nmg_two_region_vertex_fuse replaced with nmg_model_vertex_fuse */
RT_EXPORT BU_EXTERN(int nmg_model_vertex_fuse,
		    (struct model *m,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_cnurb_is_linear,
		    (const struct edge_g_cnurb *cnrb));
RT_EXPORT BU_EXTERN(int nmg_snurb_is_planar,
		    (const struct face_g_snurb *srf,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_eval_linear_trim_curve,
		    (const struct face_g_snurb *snrb,
		     const fastf_t uvw[3],
		     point_t xyz));
RT_EXPORT BU_EXTERN(void nmg_eval_trim_curve,
		    (const struct edge_g_cnurb *cnrb,
		     const struct face_g_snurb *snrb,
		     const fastf_t t, point_t xyz));
/* nmg_split_trim */
RT_EXPORT BU_EXTERN(void nmg_eval_trim_to_tol,
		    (const struct edge_g_cnurb *cnrb,
		     const struct face_g_snurb *snrb,
		     const fastf_t t0,
		     const fastf_t t1,
		     struct bu_list *head,
		     const struct bn_tol *tol));
/* nmg_split_linear_trim */
RT_EXPORT BU_EXTERN(void nmg_eval_linear_trim_to_tol,
		    (const struct edge_g_cnurb *cnrb,
		     const struct face_g_snurb *snrb,
		     const fastf_t uvw1[3],
		     const fastf_t uvw2[3],
		     struct bu_list *head,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_cnurb_lseg_coincident,
		    (const struct edgeuse *eu1,
		     const struct edge_g_cnurb *cnrb,
		     const struct face_g_snurb *snrb,
		     const point_t pt1,
		     const point_t pt2,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int	nmg_cnurb_is_on_crv,
		    (const struct edgeuse *eu,
		     const struct edge_g_cnurb *cnrb,
		     const struct face_g_snurb *snrb,
		     const struct bu_list *head,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_model_edge_fuse,
		    (struct model *m,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_model_edge_g_fuse,
		    (struct model		*m,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int nmg_ck_fu_verts,
		    (struct faceuse *fu1,
		     struct face *f2,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_ck_fg_verts,
		    (struct faceuse *fu1,
		     struct face *f2,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int	nmg_two_face_fuse,
		    (struct face	*f1,
		     struct face *f2,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_model_face_fuse,
		    (struct model *m,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_break_all_es_on_v,
		    (long *magic_p,
		     struct vertex *v,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_model_break_e_on_v,
		    (struct model *m,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(int nmg_model_fuse,
		    (struct model *m,
		     const struct bn_tol *tol));

	/* radial routines */
RT_EXPORT BU_EXTERN(void nmg_radial_sorted_list_insert,
		    (struct bu_list *hd,
		     struct nmg_radial *rad));
RT_EXPORT BU_EXTERN(void nmg_radial_verify_pointers,
		    (const struct bu_list *hd,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_radial_verify_monotone,
		    (const struct bu_list	*hd,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_insure_radial_list_is_increasing,
		    (struct bu_list	*hd,
		     fastf_t amin, fastf_t amax));
RT_EXPORT BU_EXTERN(void nmg_radial_build_list,
		    (struct bu_list		*hd,
		     struct bu_ptbl		*shell_tbl,
		     int			existing,
		     struct edgeuse		*eu,
		     const vect_t		xvec,
		     const vect_t		yvec,
		     const vect_t		zvec,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_radial_merge_lists,
		    (struct bu_list		*dest,
		     struct bu_list		*src,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int	 nmg_is_crack_outie,
		    (const struct edgeuse	*eu,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(struct nmg_radial	*nmg_find_radial_eu,
		    (const struct bu_list *hd,
		     const struct edgeuse *eu));
RT_EXPORT BU_EXTERN(const struct edgeuse *nmg_find_next_use_of_2e_in_lu,
		    (const struct edgeuse	*eu,
		     const struct edge	*e1,
		     const struct edge	*e2));
RT_EXPORT BU_EXTERN(void nmg_radial_mark_cracks,
		    (struct bu_list	*hd,
		     const struct edge	*e1,
		     const struct edge	*e2,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(struct nmg_radial *nmg_radial_find_an_original,
		    (const struct bu_list	*hd,
		     const struct shell	*s,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int nmg_radial_mark_flips,
		    (struct bu_list		*hd,
		     const struct shell	*s,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int nmg_radial_check_parity,
		    (const struct bu_list	*hd,
		     const struct bu_ptbl	*shells,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_radial_implement_decisions,
		    (struct bu_list		*hd,
		     const struct bn_tol	*tol,
		     struct edgeuse		*eu1,
		     vect_t xvec,
		     vect_t yvec,
		     vect_t zvec));
RT_EXPORT BU_EXTERN(void nmg_pr_radial,
		    (const char *title,
		     const struct nmg_radial	*rad));
RT_EXPORT BU_EXTERN(void nmg_pr_radial_list,
		    (const struct bu_list *hd,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_do_radial_flips,
		    (struct bu_list *hd));
RT_EXPORT BU_EXTERN(void nmg_do_radial_join,
		    (struct bu_list *hd,
		     struct edgeuse *eu1ref,
		     vect_t xvec, vect_t yvec, vect_t zvec,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_radial_join_eu_NEW,
		    (struct edgeuse *eu1,
		     struct edgeuse *eu2,
		     const struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void nmg_radial_exchange_marked,
		    (struct bu_list		*hd,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_s_radial_harmonize,
		    (struct shell		*s,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(int nmg_eu_radial_check,
		    (const struct edgeuse	*eu,
		     const struct shell	*s,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_s_radial_check,
		    (struct shell		*s,
		     const struct bn_tol	*tol));
RT_EXPORT BU_EXTERN(void nmg_r_radial_check,
		    (const struct nmgregion	*r,
		     const struct bn_tol	*tol));


RT_EXPORT BU_EXTERN(struct edge_g_lseg	*nmg_pick_best_edge_g,
		    (struct edgeuse *eu1,
		     struct edgeuse *eu2,
		     const struct bn_tol *tol));

/* nmg_visit.c */
RT_EXPORT BU_EXTERN(void nmg_visit_vertex,
		    (struct vertex			*v,
		     const struct nmg_visit_handlers	*htab,
		     genptr_t			state));
RT_EXPORT BU_EXTERN(void nmg_visit_vertexuse,
		    (struct vertexuse		*vu,
		     const struct nmg_visit_handlers	*htab,
		     genptr_t			state));
RT_EXPORT BU_EXTERN(void nmg_visit_edge,
		    (struct edge			*e,
		     const struct nmg_visit_handlers	*htab,
		     genptr_t			state));
RT_EXPORT BU_EXTERN(void nmg_visit_edgeuse,
		    (struct edgeuse			*eu,
		     const struct nmg_visit_handlers	*htab,
		     genptr_t			state));
RT_EXPORT BU_EXTERN(void nmg_visit_loop,
		    (struct loop			*l,
		     const struct nmg_visit_handlers	*htab,
		     genptr_t			state));
RT_EXPORT BU_EXTERN(void nmg_visit_loopuse,
		    (struct loopuse			*lu,
		     const struct nmg_visit_handlers	*htab,
		     genptr_t			state));
RT_EXPORT BU_EXTERN(void nmg_visit_face,
		    (struct face			*f,
		     const struct nmg_visit_handlers	*htab,
		     genptr_t			state));
RT_EXPORT BU_EXTERN(void nmg_visit_faceuse,
		    (struct faceuse			*fu,
		     const struct nmg_visit_handlers	*htab,
		     genptr_t			state));
RT_EXPORT BU_EXTERN(void nmg_visit_shell,
		    (struct shell			*s,
		     const struct nmg_visit_handlers	*htab,
		     genptr_t			state));
RT_EXPORT BU_EXTERN(void nmg_visit_region,
		    (struct nmgregion		*r,
		     const struct nmg_visit_handlers	*htab,
		     genptr_t			state));
RT_EXPORT BU_EXTERN(void nmg_visit_model,
		    (struct model			*model,
		     const struct nmg_visit_handlers	*htab,
		     genptr_t			state));
RT_EXPORT BU_EXTERN(void nmg_visit,
		    (const long			*magicp,
		     const struct nmg_visit_handlers	*htab,
		     genptr_t			state));

/* db5_types.c */
RT_EXPORT BU_EXTERN(int db5_type_tag_from_major,
		    (char	**tag,
		     const int	major));

RT_EXPORT BU_EXTERN(int db5_type_descrip_from_major,
		    (char	**descrip,
		     const int	major));

RT_EXPORT BU_EXTERN(int db5_type_tag_from_codes,
		    (char	**tag,
		     const int	major,
		     const int	minor));

RT_EXPORT BU_EXTERN(int db5_type_descrip_from_codes,
		    (char	**descrip,
		     const int	major,
		     const int	minor));

RT_EXPORT BU_EXTERN(int db5_type_codes_from_tag,
		    (int	*major,
		     int	*minor,
		     const char	*tag));

RT_EXPORT BU_EXTERN(int db5_type_codes_from_descrip,
		    (int	*major,
		     int	*minor,
		     const char	*descrip));

RT_EXPORT BU_EXTERN(size_t db5_type_sizeof_h_binu,
		    (const int minor));

RT_EXPORT BU_EXTERN(size_t db5_type_sizeof_n_binu,
		    (const int minor));

#endif

/* defined in wdb_obj.c */
RT_EXPORT BU_EXTERN(int wdb_create_cmd,
		    (Tcl_Interp	*interp,
		     struct rt_wdb *wdbp,
		     const char	*oname));
RT_EXPORT BU_EXTERN(void wdb_deleteProc,
		    (ClientData clientData));
RT_EXPORT BU_EXTERN(int	wdb_get_tcl,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc, char **argv));
RT_EXPORT BU_EXTERN(int dgo_cmd,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_init_obj,
		    (Tcl_Interp *interp,
		     struct rt_wdb *wdbp,
		     const char *oname));
RT_EXPORT BU_EXTERN(struct db_i	*wdb_prep_dbip,
		    (Tcl_Interp *interp,
		     const char *filename));
RT_EXPORT BU_EXTERN(int	wdb_bot_face_sort_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc, char **argv));
RT_EXPORT BU_EXTERN(int	wdb_bot_decimate_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_close_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_reopen_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_match_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_get_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_put_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_adjust_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_form_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_tops_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_rt_gettrees_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_dump_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_dbip_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_ls_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_list_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_lt_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_pathlist_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_pathsum_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_expand_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_kill_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_killall_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_killtree_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_copy_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_move_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_move_all_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_concat_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_dup_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_group_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_remove_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_region_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_comb_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_find_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_which_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_title_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_tree_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_color_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_prcolor_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_tol_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_push_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_whatid_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_keep_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_cat_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_instance_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_observer_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_make_bb_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_make_name_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_units_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_hide_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_unhide_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_attr_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_summary_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_comb_std_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_nmg_collapse_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_nmg_simplify_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_shells_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_xpush_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_showmats_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_copyeval_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_version_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_binary_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_track_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	wdb_smooth_bot_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
#ifdef IMPORT_FASTGEN4_SECTION
RT_EXPORT BU_EXTERN(int	wdb_importFg4Section_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
#endif

/* defined in dg_obj.c */
RT_EXPORT BU_EXTERN(int dgo_set_outputHandler_cmd,
		    (struct dg_obj	*dgop,
		     Tcl_Interp		*interp,
		     int		argc,
		     char 		**argv));
RT_EXPORT BU_EXTERN(int dgo_set_transparency_cmd,
		    (struct dg_obj	*dgop,
		     Tcl_Interp		*interp,
		     int		argc,
		     char 		**argv));
RT_EXPORT BU_EXTERN(int	dgo_observer_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(void dgo_deleteProc,
		    (ClientData clientData));
RT_EXPORT BU_EXTERN(void dgo_autoview,
		    (struct dg_obj *dgop,
		     struct view_obj *vop,
		     Tcl_Interp *interp));
RT_EXPORT BU_EXTERN(int	dgo_autoview_cmd,
		    (struct dg_obj *dgop,
		     struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char **argv));
RT_EXPORT BU_EXTERN(int	dgo_blast_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	dgo_draw_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc, char **argv,
		     int kind));
RT_EXPORT BU_EXTERN(int	dgo_E_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	dgo_erase_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	dgo_erase_all_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	dgo_get_autoview_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	dgo_how_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	dgo_illum_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	dgo_label_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(struct dg_obj *dgo_open_cmd,
		    (char *oname,
		     struct rt_wdb *wdbp));
RT_EXPORT BU_EXTERN(int	dgo_overlay_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	dgo_report_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	dgo_rt_cmd,
		    (struct dg_obj *dgop,
		     struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	dgo_rtabort_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	dgo_rtcheck_cmd,
		    (struct dg_obj *dgop,
		     struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char **argv));
RT_EXPORT BU_EXTERN(int	dgo_vdraw_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	dgo_who_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(void dgo_zap_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp));
RT_EXPORT BU_EXTERN(int	dgo_shaded_mode_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));

RT_EXPORT BU_EXTERN(void dgo_color_soltab,
		    ());
RT_EXPORT BU_EXTERN(void dgo_drawH_part2,
		    ());
RT_EXPORT BU_EXTERN(void dgo_eraseobjall_callback,
		    (struct db_i	*dbip,
		     Tcl_Interp		*interp,
		     struct directory	*dp,
		     int		notify));
RT_EXPORT BU_EXTERN(void dgo_eraseobjpath,
		    ());
RT_EXPORT BU_EXTERN(void dgo_impending_wdb_close,
		    ());
RT_EXPORT BU_EXTERN(int dgo_invent_solid,
		    ());
RT_EXPORT BU_EXTERN(void dgo_notify,
		    (struct dg_obj	*dgop,
		     Tcl_Interp		*interp));
RT_EXPORT BU_EXTERN(void dgo_notifyWdb,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp    *interp));
RT_EXPORT BU_EXTERN(void dgo_zapall,
		    ());

/* defined in nirt.c */
RT_EXPORT BU_EXTERN(int	dgo_nirt_cmd,
		    (struct dg_obj *dgop,
		     struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	dgo_vnirt_cmd,
		    (struct dg_obj *dgop,
		     struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));


/* defined in bigE.c */
RT_EXPORT BU_EXTERN(int	dg_E_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));

/* defined in view_obj.c */
RT_EXPORT BU_EXTERN(struct view_obj *vo_open_cmd,
		    (const char *oname));
RT_EXPORT BU_EXTERN(void vo_center,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     point_t center));
RT_EXPORT BU_EXTERN(int	vo_center_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(void vo_size,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     fastf_t size));
RT_EXPORT BU_EXTERN(int	vo_size_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_invSize_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(void vo_mat_aet,
		    (struct view_obj *vop));
RT_EXPORT BU_EXTERN(int	vo_zoom,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     fastf_t sf));
RT_EXPORT BU_EXTERN(int	vo_zoom_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char **argv));
RT_EXPORT BU_EXTERN(int	vo_orientation_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_lookat_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(void vo_setview,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     vect_t rvec));
RT_EXPORT BU_EXTERN(int	vo_setview_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_eye_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_eye_pos_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_pmat_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_perspective_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(void vo_update,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int oflag));
RT_EXPORT BU_EXTERN(int	vo_aet_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_rmat_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_model2view_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_pmodel2view_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_view2model_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_pov_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_units_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_base2local_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_local2base_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_rot,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     char coord,
		     char origin,
		     mat_t rmat,
		     int (*func)()));
RT_EXPORT BU_EXTERN(int	vo_rot_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char **argv,
		     int (*func)()));
RT_EXPORT BU_EXTERN(int	vo_arot_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv,
		     int (*func)()));
RT_EXPORT BU_EXTERN(int	vo_mrot_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv,
		     int (*func)()));
RT_EXPORT BU_EXTERN(int	vo_tra,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     char coord,
		     vect_t tvec,
		     int (*func)()));
RT_EXPORT BU_EXTERN(int	vo_tra_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char **argv,
		     int (*func)()));
RT_EXPORT BU_EXTERN(int	vo_slew,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     vect_t svec));
RT_EXPORT BU_EXTERN(int	vo_slew_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char **argv));
RT_EXPORT BU_EXTERN(int	vo_observer_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_coord_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_rotate_about_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_keypoint_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_vrot_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
RT_EXPORT BU_EXTERN(int	vo_sca,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     fastf_t sf,
		     int (*func)()));
RT_EXPORT BU_EXTERN(int	vo_sca_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv,
		     int (*func)()));

/* defined in binary_obj.c */
RT_EXPORT BU_EXTERN(int rt_mk_binunif,
		    (struct rt_wdb *wdbp,
		     const char *obj_name,
		     const char *file_name,
		     unsigned int minor_type,
		     long max_count));

#ifdef _WIN32
/* defined in g_dsp.c */
RT_EXPORT BU_EXTERN(void rt_dsp_ifree,
		    (struct rt_db_internal *ip));

/* defined in g_ebm.c */
RT_EXPORT BU_EXTERN(void rt_ebm_ifree,
		    (struct rt_db_internal *ip));

/* defined in g_vol.c */
RT_EXPORT BU_EXTERN(void rt_vol_ifree,
		    (struct rt_db_internal *ip));
#endif

/* defined in db5_bin.c */
RT_EXPORT BU_EXTERN(void rt_binunif_free,
		    (struct rt_binunif_internal *bip));
RT_EXPORT BU_EXTERN(void rt_binunif_dump,
		    (struct rt_binunif_internal *bip));

/* defined in g_cline.c */
RT_EXPORT extern fastf_t rt_cline_radius;

/* defined in g_bot.c */
RT_EXPORT extern int rt_bot_minpieces;
RT_EXPORT extern int rt_bot_tri_per_piece;


/*
 *  Constants provided and used by the RT library.
 */
RT_EXPORT extern const struct db_tree_state rt_initial_tree_state;
RT_EXPORT extern const char *rt_vlist_cmd_descriptions[];

/* vers.c (created by librt/Cakefile) */
RT_EXPORT extern const char rt_version[];

__END_DECLS

#endif /* RAYTRACE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
