/*                           N M G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @addtogroup nmg */
/*@{*/
/** @file nmg.h
 *
 *
 *  @author	Lee A. Butler
 *  @author	Michael John Muuss
 *
 *  @par Source
 *	The U. S. Army Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *@brief
 *  Definition of data structures for "Non-Manifold Geometry Modelling."
 *  Developed from "Non-Manifold Geometric Boundary Modeling" by
 *  Kevin Weiler, 5/7/87 (SIGGraph 1989 Course #20 Notes)
 *
 *  Include Sequencing -
 *	#  include <stdio.h>
 *	# include <math.h>
 *	# include "machine.h"	/_* For fastf_t definition on this machine *_/
 *	# include "vmath.h"	/_* For vect_t definition *_/
 *	# include "rtlist.h"	/_* OPTIONAL, auto-included by raytrace.h *_/
 *	# include "rtstring.h"	/_* OPTIONAL, auto-included by raytrace.h *_/
 *	# include "nmg.h"
 *	# include "raytrace.h"
 *	# include "nurb.h"	/_* OPTIONAL, follows raytrace.h when used *_/
 *
 *  @par Libraries Used -
 *	LIBRT LIBRT_LIBES -lm -lc
 *
 *  $Header$
 */
#ifndef NMG_H
#define NMG_H seen

#include "common.h"

/* make sure all the prerequisite include files have been included
 */
#ifndef MACHINE_H
#include "machine.h"
#endif

#ifndef VMATH_H
#include "vmath.h"
#endif

#ifndef SEEN_RTLIST_H
#include "rtlist.h"
#endif

#ifndef SEEN_COMPAT4_H
#include "compat4.h"
#endif

#ifndef SEEN_BU_H
#include "bu.h"
#endif

#ifndef NULL
#define NULL 0
#endif

#define	NMG_EXTERN(type_and_name,args)	RT_EXTERN(type_and_name,args)


#define DEBUG_PL_ANIM	0x00000001	/**< @brief 1 mged: animated evaluation */
#define DEBUG_PL_SLOW	0x00000002	/**< @brief 2 mged: add delays to animation */
#define DEBUG_GRAPHCL	0x00000004	/**< @brief 3 mged: graphic classification */
#define DEBUG_PL_LOOP	0x00000008	/**< @brief 4 loop class (needs GRAPHCL) */
#define DEBUG_PLOTEM	0x00000010	/**< @brief 5 make plots in debugged routines (needs other flags set too) */
#define DEBUG_POLYSECT	0x00000020	/**< @brief 6 nmg_inter: face intersection */
#define DEBUG_VERIFY	0x00000040	/**< @brief 7 nmg_vshell() frequently, verify health */
#define DEBUG_BOOL	0x00000080	/**< @brief 8 nmg_bool:  */
#define DEBUG_CLASSIFY	0x00000100	/**< @brief 9 nmg_class: */
#define DEBUG_BOOLEVAL	0x00000200	/**< @brief 10 nmg_eval: what to retain */
#define DEBUG_BASIC	0x00000400	/**< @brief 013 nmg_mk.c and nmg_mod.c routines */
#define DEBUG_MESH	0x00000800	/**< @brief 12 nmg_mesh: describe edge search */
#define DEBUG_MESH_EU	0x00001000	/**< @brief 13 nmg_mesh: list edges meshed */
#define DEBUG_POLYTO	0x00002000	/**< @brief 14 nmg_misc: polytonmg */
#define DEBUG_LABEL_PTS 0x00004000	/**< @brief 15 label points in plot files */
/***#define DEBUG_INS	0x00008000	/_* 16 bu_ptbl table insert */
#define DEBUG_NMGRT	0x00010000	/**< @brief 17 ray tracing */
#define DEBUG_FINDEU	0x00020000	/**< @brief 18 nmg_mod: nmg_findeu() */
#define DEBUG_CMFACE	0x00040000	/**< @brief 19 nmg_mod: nmg_cmface() */
#define DEBUG_CUTLOOP	0x00080000	/**< @brief 024 nmg_mod: nmg_cut_loop */
#define DEBUG_VU_SORT	0x00100000	/**< @brief 025 nmg_fcut: coincident vu sort */
#define DEBUG_FCUT	0x00200000	/**< @brief 026 nmg_fcut: face cutter */
#define DEBUG_RT_SEGS	0x00400000	/**< @brief 027 nmg_rt_segs: */
#define DEBUG_RT_ISECT	0x00800000	/**< @brief 028 nmg_rt_isect: */
#define DEBUG_TRI	0x01000000	/**< @brief 029 nmg_tri */
#define DEBUG_PT_FU	0x02000000	/**< @brief 029 nmg_pt_fu */
#define DEBUG_MANIF	0x04000000	/**< @brief 029 nmg_manif */
#define NMG_DEBUG_FORMAT \
"\020\033MANIF\032PTFU\031TRIANG\030RT_ISECT\
\027RT_SEGS\026FCUT\025VU_SORT\024CUTLOOP\023CMFACE\022FINDEU\021RT_ISECT\020(FREE)\
\017LABEL_PTS\016POLYTO\015MESH_EU\014MESH\013BASIC\012BOOLEVAL\011CLASSIFY\
\010BOOL\7VERIFY\6POLYSECT\5PLOTEM\4PL_LOOP\3GRAPHCL\2PL_SLOW\1PL_ANIM"

/*
 *  Macros for providing function prototypes, regardless of whether
 *  the compiler understands them or not.
 *  It is vital that the argument list given for "args" be enclosed
 *  in parens.
 *  The setting of USE_PROTOTYPES is done in machine.h
 */
#if USE_PROTOTYPES
#	define	NMG_ARGS(args)			args
#else
#	define	NMG_ARGS(args)			()
#endif

/* Boolean operations */
#define NMG_BOOL_SUB 1		/**< @brief subtraction */
#define NMG_BOOL_ADD 2		/**< @brief addition/union */
#define NMG_BOOL_ISECT 4	/**< @brief intsersection */

/* Boolean classifications */
#define NMG_CLASS_Unknown	-1
#define NMG_CLASS_AinB		0
#define NMG_CLASS_AonBshared	1
#define NMG_CLASS_AonBanti	2
#define NMG_CLASS_AoutB		3
#define NMG_CLASS_BinA		4
#define NMG_CLASS_BonAshared	5
#define NMG_CLASS_BonAanti	6
#define NMG_CLASS_BoutA		7

/* orientations available.  All topological elements are orientable. */
#define OT_NONE     0    /**< @brief no orientation (error) */
#define OT_SAME     1    /**< @brief orientation same */
#define OT_OPPOSITE 2    /**< @brief orientation opposite */
#define OT_UNSPEC   3    /**< @brief orientation unspecified */
#define OT_BOOLPLACE 4   /**< @brief object is intermediate data for boolean ops */

/*
 *  Magic Numbers.
 */
#define NMG_MODEL_MAGIC 	0x12121212
#define NMG_REGION_MAGIC	0x23232323
#define NMG_REGION_A_MAGIC	0x696e6720
#define NMG_SHELL_MAGIC 	0x71077345	/**< @brief shell oil */
#define NMG_SHELL_A_MAGIC	0x65207761
#define NMG_FACE_MAGIC		0x45454545
#define NMG_FACE_G_PLANE_MAGIC	0x726b6e65
#define NMG_FACE_G_SNURB_MAGIC	0x736e7262	/**< @brief was RT_SNURB_MAGIC */
#define NMG_FACEUSE_MAGIC	0x56565656
#define NMG_LOOP_MAGIC		0x67676767
#define NMG_LOOP_G_MAGIC	0x6420224c
#define NMG_LOOPUSE_MAGIC	0x78787878
#define NMG_EDGE_MAGIC		0x33333333
#define NMG_EDGE_G_LSEG_MAGIC	0x6c696768
#define NMG_EDGE_G_CNURB_MAGIC	0x636e7262	/**< @brief was RT_CNURB_MAGIC */
#define NMG_EDGEUSE_MAGIC	0x90909090
#define NMG_EDGEUSE2_MAGIC	0x91919191	/**< @brief used in eu->l2.magic */
#define NMG_VERTEX_MAGIC	0x00123123
#define NMG_VERTEX_G_MAGIC	0x72737707
#define NMG_VERTEXUSE_MAGIC	0x12341234
#define NMG_VERTEXUSE_A_PLANE_MAGIC	0x69676874
#define NMG_VERTEXUSE_A_CNURB_MAGIC	0x20416e64
#define NMG_KNOT_VECTOR_MAGIC	0x6b6e6f74	/**< @brief aka RT_KNOT_VECTOR_MAGIC */

/** 
 * macros to check/validate a structure pointer
 */
#define NMG_CKMAG(_ptr, _magic, _str)	BU_CKMAG(_ptr,_magic,_str)
#define NMG_CK2MAG(_ptr, _magic1, _magic2, _str)	\
	if( !(_ptr) || (*((long *)(_ptr)) != (_magic1) && *((long *)(_ptr)) != (_magic2) ) )  { \
		bu_badmagic( (long *)(_ptr), _magic1, _str, __FILE__, __LINE__ ); \
	}

#define NMG_CK_MODEL(_p)	NMG_CKMAG(_p, NMG_MODEL_MAGIC, "model")
#define NMG_CK_REGION(_p)	NMG_CKMAG(_p, NMG_REGION_MAGIC, "region")
#define NMG_CK_REGION_A(_p)	NMG_CKMAG(_p, NMG_REGION_A_MAGIC, "region_a")
#define NMG_CK_SHELL(_p)	NMG_CKMAG(_p, NMG_SHELL_MAGIC, "shell")
#define NMG_CK_SHELL_A(_p)	NMG_CKMAG(_p, NMG_SHELL_A_MAGIC, "shell_a")
#define NMG_CK_FACE(_p)		NMG_CKMAG(_p, NMG_FACE_MAGIC, "face")
#define NMG_CK_FACE_G_PLANE(_p)	NMG_CKMAG(_p, NMG_FACE_G_PLANE_MAGIC, "face_g_plane")
#define NMG_CK_FACE_G_SNURB(_p)	NMG_CKMAG(_p, NMG_FACE_G_SNURB_MAGIC, "face_g_snurb")
#define NMG_CK_FACE_G_EITHER(_p)	NMG_CK2MAG(_p, NMG_FACE_G_PLANE_MAGIC, NMG_FACE_G_SNURB_MAGIC, "face_g_plane|face_g_snurb")
#define NMG_CK_FACEUSE(_p)	NMG_CKMAG(_p, NMG_FACEUSE_MAGIC, "faceuse")
#define NMG_CK_LOOP(_p)		NMG_CKMAG(_p, NMG_LOOP_MAGIC, "loop")
#define NMG_CK_LOOP_G(_p)	NMG_CKMAG(_p, NMG_LOOP_G_MAGIC, "loop_g")
#define NMG_CK_LOOPUSE(_p)	NMG_CKMAG(_p, NMG_LOOPUSE_MAGIC, "loopuse")
#define NMG_CK_EDGE(_p)		NMG_CKMAG(_p, NMG_EDGE_MAGIC, "edge")
#define NMG_CK_EDGE_G_LSEG(_p)	NMG_CKMAG(_p, NMG_EDGE_G_LSEG_MAGIC, "edge_g_lseg")
#define NMG_CK_EDGE_G_CNURB(_p)	NMG_CKMAG(_p, NMG_EDGE_G_CNURB_MAGIC, "edge_g_cnurb")
#define NMG_CK_EDGE_G_EITHER(_p)	NMG_CK2MAG(_p, NMG_EDGE_G_LSEG_MAGIC, NMG_EDGE_G_CNURB_MAGIC, "edge_g_lseg|edge_g_cnurb")
#define NMG_CK_EDGEUSE(_p)	NMG_CKMAG(_p, NMG_EDGEUSE_MAGIC, "edgeuse")
#define NMG_CK_VERTEX(_p)	NMG_CKMAG(_p, NMG_VERTEX_MAGIC, "vertex")
#define NMG_CK_VERTEX_G(_p)	NMG_CKMAG(_p, NMG_VERTEX_G_MAGIC, "vertex_g")
#define NMG_CK_VERTEXUSE(_p)	NMG_CKMAG(_p, NMG_VERTEXUSE_MAGIC, "vertexuse")
#define NMG_CK_VERTEXUSE_A_PLANE(_p)	NMG_CKMAG(_p, NMG_VERTEXUSE_A_PLANE_MAGIC, "vertexuse_a_plane")
#define NMG_CK_VERTEXUSE_A_CNURB(_p)	NMG_CKMAG(_p, NMG_VERTEXUSE_A_CNURB_MAGIC, "vertexuse_a_cnurb")
#define NMG_CK_VERTEXUSE_A_EITHER(_p)	NMG_CK2MAG(_p, NMG_VERTEXUSE_A_PLANE_MAGIC, NMG_VERTEXUSE_A_CNURB_MAGIC, "vertexuse_a_plane|vertexuse_a_cnurb")
#define NMG_CK_LIST(_p)		BU_CKMAG(_p, BU_LIST_HEAD_MAGIC, "bu_list")

/* Used only in nmg_mod.c */
#define NMG_TEST_EDGEUSE(_p) \
	if (!(_p)->l.forw || !(_p)->l.back || !(_p)->eumate_p || \
	    !(_p)->radial_p || !(_p)->e_p || !(_p)->vu_p || \
	    !(_p)->up.magic_p ) { \
		bu_log("in %s at %d Bad edgeuse member pointer\n",\
			 __FILE__, __LINE__);  nmg_pr_eu(_p, (char *)NULL); \
			rt_bomb("Null pointer\n"); \
	} else if ((_p)->vu_p->up.eu_p != (_p) || \
	(_p)->eumate_p->vu_p->up.eu_p != (_p)->eumate_p) {\
	    	bu_log("in %s at %d edgeuse lost vertexuse\n",\
	    		 __FILE__, __LINE__); rt_bomb("bye");}

/**
 *			K N O T _ V E C T O R
 * @brief
 *  Definition of a knot vector.
 *
 *  Not found independently, but used in the cnurb and snurb structures.
 *  (Exactly the same as the definition in nurb.h)
 */
struct knot_vector {
	int		magic;
	int		k_size;		/**< @brief knot vector size */
	fastf_t		* knots;	/**< @brief pointer to knot vector  */
};
#define RT_KNOT_VECTOR_MAGIC	NMG_KNOT_VECTOR_MAGIC	/**< @brief nurb.h compat */

/*
 *	N O T I C E !
 *
 *	We rely on the fact that the first long in a struct is the magic
 *	number (which is used to identify the struct type).
 *	This may be either a long, or an rt_list structure, which
 *	starts with a magic number.
 *
 *	To these ends, there is a standard ordering for fields in "object-use"
 *	structures.  That ordering is:
 *		1) magic number, or rt_list structure
 *		2) pointer to parent
 *		5) pointer to mate
 *		6) pointer to geometry
 *		7) pointer to attributes
 *		8) pointer to child(ren)
 */


/**
 *			M O D E L
 */
struct model {
	long			magic;
	struct bu_list		r_hd;	/**< @brief list of regions */
	long			index;	/**< @brief struct # in this model */
	long			maxindex; /**< @brief # of structs so far */
};

/**
 *			R E G I O N
 */
struct nmgregion {
	struct bu_list		l;	/**< @brief regions, in model's r_hd list */
	struct model   		*m_p;	/**< @brief owning model */
	struct nmgregion_a	*ra_p;	/**< @brief attributes */
	struct bu_list		s_hd;	/**< @brief list of shells in region */
	long			index;	/**< @brief struct # in this model */
};

struct nmgregion_a {
	long			magic;
	point_t			min_pt;	/**< @brief minimums of bounding box */
	point_t			max_pt;	/**< @brief maximums of bounding box */
	long			index;	/**< @brief struct # in this model */
};

/**
 *			S H E L L
 *
 *  When a shell encloses volume, it's done entirely by the list of faceuses.
 *
 *  The wire loopuses (each of which heads a list of edges) define a
 *  set of connected line segments which form a closed path, but do not
 *  enclose either volume or surface area.
 *
 *  The wire edgeuses are disconnected line segments.
 *  There is a special interpetation to the eu_hd list of wire edgeuses.
 *  Unlike edgeuses seen in loops, the eu_hd list contains eu1, eu1mate,
 *  eu2, eu2mate, ..., where each edgeuse and it's mate comprise a
 *  *non-connected* "wire" edge which starts at eu1->vu_p->v_p and ends
 *  at eu1mate->vu_p->v_p.  There is no relationship between the pairs
 *  of edgeuses at all, other than that they all live on the same linked
 *  list.
 */
struct shell {
	struct bu_list		l;	/**< @brief shells, in region's s_hd list */
	struct nmgregion	*r_p;	/**< @brief owning region */
	struct shell_a		*sa_p;	/**< @brief attribs */

	struct bu_list		fu_hd;	/**< @brief list of face uses in shell */
	struct bu_list		lu_hd;	/**< @brief wire loopuses (edge groups) */
	struct bu_list		eu_hd;	/**< @brief wire list (shell has wires) */
	struct vertexuse	*vu_p;	/**< @brief internal ptr to single vertexuse */
	long			index;	/**< @brief struct # in this model */
};

struct shell_a {
	long			magic;
	point_t			min_pt;	/**< @brief minimums of bounding box */
	point_t			max_pt;	/**< @brief maximums of bounding box */
	long			index;	/**< @brief struct # in this model */
};

/**
 *			F A C E
 *
 *  Note: there will always be exactly two faceuse's using a face.
 *  To find them, go up fu_p for one, then across fumate_p to other.
 */
struct face {
	struct bu_list		l;	/**< @brief faces in face_g's f_hd list */
	struct faceuse		*fu_p;	/**< @brief Ptr up to one use of this face */
	union {
		long		    *magic_p;
		struct face_g_plane *plane_p;
		struct face_g_snurb *snurb_p;
	} g;				/**< @brief geometry */
	int			flip;	/**< @brief !0 ==> flip normal of fg */
	/* These might be better stored in a face_a (not faceuse_a!) */
	/* These are not stored on disk */
	point_t			min_pt;	/**< @brief minimums of bounding box */
	point_t			max_pt;	/**< @brief maximums of bounding box */
	long			index;	/**< @brief struct # in this model */
};

struct face_g_plane {
	long			magic;
	struct bu_list		f_hd;	/**< @brief list of faces sharing this surface */
	plane_t			N;	/**< @brief Plane equation (incl normal) */
	long			index;	/**< @brief struct # in this model */
};

struct face_g_snurb {
	/* NOTICE:  l.forw & l.back *not* stored in database.  For LIBNURB internal use only. */
	struct bu_list		l;
	struct bu_list		f_hd;	/**< @brief list of faces sharing this surface */
	int			order[2]; /**< @brief surface order [0] = u, [1] = v */
	struct knot_vector	u;	/**< @brief surface knot vectors */
	struct knot_vector	v;	/**< @brief surface knot vectors */
	/* surface control points */
	int			s_size[2]; /**< @brief mesh size, u,v */
	int			pt_type; /**< @brief surface point type */
	fastf_t			*ctl_points; /**< @brief array [size[0]*size[1]] */
	/* START OF ITEMS VALID IN-MEMORY ONLY -- NOT STORED ON DISK */
	int			dir;	/**< @brief direction of last refinement */
	point_t			min_pt;	/**< @brief min corner of bounding box */
	point_t			max_pt;	/**< @brief max corner of bounding box */
	/*   END OF ITEMS VALID IN-MEMORY ONLY -- NOT STORED ON DISK */
	long			index;	/**< @brief struct # in this model */
};

struct faceuse {
	struct bu_list		l;	/**< @brief fu's, in shell's fu_hd list */
	struct shell		*s_p;	/**< @brief owning shell */
	struct faceuse		*fumate_p;    /**< @brief opposite side of face */
	int			orientation;  /**< @brief rel to face geom defn */
	int			outside; /**< @brief RESERVED for future:  See Lee Butler */
	struct face		*f_p;	/**< @brief face definition and attributes */
	struct bu_list		lu_hd;	/**< @brief list of loops in face-use */
	long			index;	/**< @brief struct # in this model */
};

/** Returns a 3-tuple (vect_t), given faceuse and state of flip flags */
#define NMG_GET_FU_NORMAL(_N, _fu)	{ \
	register const struct faceuse	*_fu1 = (_fu); \
	register const struct face_g_plane	*_fg; \
	NMG_CK_FACEUSE(_fu1); \
	NMG_CK_FACE(_fu1->f_p); \
	_fg = _fu1->f_p->g.plane_p; \
	NMG_CK_FACE_G_PLANE(_fg); \
	if( (_fu1->orientation != OT_SAME) != (_fu1->f_p->flip != 0) )  { \
		VREVERSE( _N, _fg->N); \
	} else { \
		VMOVE( _N, _fg->N ); \
	} }

/** Returns a 4-tuple (plane_t), given faceuse and state of flip flags */
#define NMG_GET_FU_PLANE(_N, _fu)	{ \
	register const struct faceuse	*_fu1 = (_fu); \
	register const struct face_g_plane	*_fg; \
	NMG_CK_FACEUSE(_fu1); \
	NMG_CK_FACE(_fu1->f_p); \
	_fg = _fu1->f_p->g.plane_p; \
	NMG_CK_FACE_G_PLANE(_fg); \
	if( (_fu1->orientation != OT_SAME) != (_fu1->f_p->flip != 0) )  { \
		HREVERSE( _N, _fg->N); \
	} else { \
		HMOVE( _N, _fg->N ); \
	} }

/**
 *			L O O P
 *
 *  To find all the uses of this loop, use lu_p for one loopuse,
 *  then go down and find an edge,
 *  then wander around either eumate_p or radial_p from there.
 *
 *  Normally, down_hd heads a doubly linked list of edgeuses.
 *  But, before using it, check BU_LIST_FIRST_MAGIC(&lu->down_hd)
 *  for the magic number type.
 *  If this is a self-loop on a single vertexuse, then get the vertex pointer
 *  with vu = BU_LIST_FIRST(vertexuse, &lu->down_hd)
 *
 *  This is an especially dangerous storage efficiency measure ("hack"),
 *  because the list that the vertexuse structure belongs to is headed,
 *  not by a superior element type, but by the vertex structure.
 *  When a loopuse needs to point down to a vertexuse, rip off the
 *  forw pointer.  Take careful note that this is just a pointer,
 *  **not** the head of a linked list (single, double, or otherwise)!
 *  Exercise great care!
 *
 *  The edges of an exterior (OT_SAME) loop occur in counter-clockwise
 *  order, as viewed from the normalward side (outside).
 */
#define RT_LIST_SET_DOWN_TO_VERT(_hp,_vu)	{ \
	(_hp)->forw = &((_vu)->l); (_hp)->back = (struct bu_list *)NULL; }

struct loop {
	long			magic;
	struct loopuse		*lu_p;	/**< @brief Ptr to one use of this loop */
	struct loop_g		*lg_p;  /**< @brief Geometry */
	long			index;	/**< @brief struct # in this model */
};

struct loop_g {
	long			magic;
	point_t			min_pt;	/**< @brief minimums of bounding box */
	point_t			max_pt;	/**< @brief maximums of bounding box */
	long			index;	/**< @brief struct # in this model */
};

struct loopuse {
	struct bu_list		l;	/**< @brief lu's, in fu's lu_hd, or shell's lu_hd */
	union {
		struct faceuse  *fu_p;	/**< @brief owning face-use */
		struct shell	*s_p;
		long		*magic_p;
	} up;
	struct loopuse		*lumate_p; /**< @brief loopuse on other side of face */
	int			orientation;  /**< @brief OT_SAME=outside loop */
	struct loop		*l_p;	/**< @brief loop definition and attributes */
	struct bu_list		down_hd; /**< @brief eu list or vu pointer */
	long			index;	/**< @brief struct # in this model */
};

/**
 *			E D G E
 *
 *  To find all edgeuses of an edge, use eu_p to get an arbitrary edgeuse,
 *  then wander around either eumate_p or radial_p from there.
 *
 *  Only the first vertex of an edge is kept in an edgeuse (eu->vu_p).
 *  The other vertex can be found by either eu->eumate_p->vu_p or
 *  by BU_LIST_PNEXT_CIRC(edgeuse,eu)->vu_p.  Note that the first
 *  form gives a vertexuse in the faceuse of *opposite* orientation,
 *  while the second form gives a vertexuse in the faceuse of the correct
 *  orientation.  If going on to the vertex (vu_p->v_p), both forms
 *  are identical.
 *
 *  An edge_g_lseg structure represents a line in 3-space.  All edges on that
 *  line should share the same edge_g.
 *
 *  An edge occupies the range eu->param to eu->eumate_p->param in it's
 *  geometry's parameter space.  (cnurbs only)
 */
struct edge {
	long			magic;
	struct edgeuse		*eu_p;	/**< @brief Ptr to one use of this edge */
	long			is_real;/**< @brief artifact or modeled edge (from tessellator) */
	long			index;	/**< @brief struct # in this model */
};

/**
 *  IMPORTANT:  First two items in edge_g_lseg and edge_g_cnurb must be
 *  identical structure, so pointers are puns for both.
 *  eu_hd2 list must be in same place for both.
 */
struct edge_g_lseg {
	struct bu_list		l;	/**< @brief NOTICE:  l.forw & l.back *not* stored in database.  For alignment only. */
	struct bu_list		eu_hd2;	/**< @brief heads l2 list of edgeuses on this line */
	point_t			e_pt;	/**< @brief parametric equation of the line */
	vect_t			e_dir;
	long			index;	/**< @brief struct # in this model */
};

/*
 *  The ctl_points on this curve are (u,v) values on the face's surface.
 *  As a storage and performance efficiency measure, if order <= 0,
 *  then the cnurb is a straight line segment in parameter space,
 *  and the k.knots and ctl_points pointers will be NULL.
 *  In this case, the vertexuse_a_cnurb's at both ends of the edgeuse define
 *  the path through parameter space.
 */
struct edge_g_cnurb {
	struct bu_list		l;	/**< @brief NOTICE:  l.forw & l.back *not* stored in database.  For LIBNURB internal use only. */
	struct bu_list		eu_hd2;	/**< @brief heads l2 list of edgeuses on this curve */
	int			order;	/**< @brief Curve Order */
	struct knot_vector	k;	/**< @brief curve knot vector */
	/* curve control polygon */
	int			c_size;	/**< @brief number of ctl points */
	int			pt_type;/**< @brief curve point type */
	fastf_t			*ctl_points; /**< @brief array [c_size] */
	long			index;	/**< @brief struct # in this model */
};

struct edgeuse {
	struct bu_list		l;	/**< @brief cw/ccw edges in loop or wire edges in shell */
	struct bu_list		l2;	/**< @brief member of edge_g's eu_hd2 list */
	union {
		struct loopuse	*lu_p;
		struct shell	*s_p;
		long	        *magic_p; /**< @brief for those times when we're not sure */
	} up;
	struct edgeuse		*eumate_p;  /**< @brief eu on other face or other end of wire*/
	struct edgeuse		*radial_p;  /**< @brief eu on radially adj. fu (null if wire)*/
	struct edge		*e_p;	    /**< @brief edge definition and attributes */
	int	  		orientation;/**< @brief compared to geom (null if wire) */
	struct vertexuse	*vu_p;	    /**< @brief first vu of eu in this orient */
	union {
		long		    *magic_p;
		struct edge_g_lseg  *lseg_p;
		struct edge_g_cnurb *cnurb_p;
	} g;				/**< @brief geometry */
	/* (u,v,w) param[] of vu is found in vu_p->vua_p->param */
	long			index;	/**< @brief struct # in this model */
};

/**
 *			V E R T E X
 *
 *  The vertex and vertexuse structures are connected in a way different
 *  from the superior kinds of topology elements.
 *  The vertex structure heads a linked list that all vertexuse's
 *  that use the vertex are linked onto.
 */
struct vertex {
	long			magic;
	struct bu_list		vu_hd;	/**< @brief heads list of vu's of this vertex */
	struct vertex_g		*vg_p;	/**< @brief geometry */
	long			index;	/**< @brief struct # in this model */
};

struct vertex_g {
	long			magic;
	point_t			coord;	/**< @brief coordinates of vertex in space */
	long			index;	/**< @brief struct # in this model */
};

struct vertexuse {
	struct bu_list		l;	/**< @brief list of all vu's on a vertex */
	union {
		struct shell	*s_p;	/**< @brief no fu's or eu's on shell */
		struct loopuse	*lu_p;	/**< @brief loopuse contains single vertex */
		struct edgeuse	*eu_p;	/**< @brief eu causing this vu */
		long		*magic_p; /**< @brief for those times when we're not sure */
	} up;
	struct vertex		*v_p;	/**< @brief vertex definition and attributes */
	union {
		long				*magic_p;
		struct vertexuse_a_plane	*plane_p;
		struct vertexuse_a_cnurb	*cnurb_p;
	} a;				/**< @brief Attributes */
	long			index;	/**< @brief struct # in this model */
};

struct vertexuse_a_plane {
	long			magic;
	vect_t			N;	/**< @brief (opt) surface Normal at vertexuse */
	long			index;	/**< @brief struct # in this model */
};

struct vertexuse_a_cnurb {
	long			magic;
	fastf_t			param[3]; /**< @brief (u,v,w) of vu on eu's cnurb */
	long			index;	/**< @brief struct # in this model */
};

/**
 * storage allocation and de-allocation support
 *  Primarily used by nmg_mk.c
 */

#define NMG_GETSTRUCT(p,str)	BU_GETSTRUCT(p,str)


#if __STDC__ && !defined(alliant) && !defined(apollo)
# define NMG_FREESTRUCT(ptr, str) \
	{ bzero((char *)(ptr), sizeof(struct str)); \
	  bu_free((char *)(ptr), "freestruct " #str); }
#else
# define NMG_FREESTRUCT(ptr, str) \
	{ bzero((char *)(ptr), sizeof(struct str)); \
	  bu_free((char *)(ptr), "freestruct str"); }
#endif


/*
 *  Macros to create and destroy storage for the NMG data structures.
 *  Since nmg_mk.c and g_nmg.c are the only source file which should perform
 *  these most fundamental operations, the macros do not belong in nmg.h
 *  In particular, application code should NEVER do these things.
 *  Any need to do so should be handled by extending nmg_mk.c
 */
#define NMG_INCR_INDEX(_p,_m)	\
	NMG_CK_MODEL(_m); (_p)->index = ((_m)->maxindex)++

#define GET_REGION(p,m)	    {NMG_GETSTRUCT(p, nmgregion); NMG_INCR_INDEX(p,m);}
#define GET_REGION_A(p,m)   {NMG_GETSTRUCT(p, nmgregion_a); NMG_INCR_INDEX(p,m);}
#define GET_SHELL(p,m)	    {NMG_GETSTRUCT(p, shell); NMG_INCR_INDEX(p,m);}
#define GET_SHELL_A(p,m)    {NMG_GETSTRUCT(p, shell_a); NMG_INCR_INDEX(p,m);}
#define GET_FACE(p,m)	    {NMG_GETSTRUCT(p, face); NMG_INCR_INDEX(p,m);}
#define GET_FACE_G_PLANE(p,m) {NMG_GETSTRUCT(p, face_g_plane); NMG_INCR_INDEX(p,m);}
#define GET_FACE_G_SNURB(p,m) {NMG_GETSTRUCT(p, face_g_snurb); NMG_INCR_INDEX(p,m);}
#define GET_FACEUSE(p,m)    {NMG_GETSTRUCT(p, faceuse); NMG_INCR_INDEX(p,m);}
#define GET_LOOP(p,m)	    {NMG_GETSTRUCT(p, loop); NMG_INCR_INDEX(p,m);}
#define GET_LOOP_G(p,m)	    {NMG_GETSTRUCT(p, loop_g); NMG_INCR_INDEX(p,m);}
#define GET_LOOPUSE(p,m)    {NMG_GETSTRUCT(p, loopuse); NMG_INCR_INDEX(p,m);}
#define GET_EDGE(p,m)	    {NMG_GETSTRUCT(p, edge); NMG_INCR_INDEX(p,m);}
#define GET_EDGE_G_LSEG(p,m)  {NMG_GETSTRUCT(p, edge_g_lseg); NMG_INCR_INDEX(p,m);}
#define GET_EDGE_G_CNURB(p,m) {NMG_GETSTRUCT(p, edge_g_cnurb); NMG_INCR_INDEX(p,m);}
#define GET_EDGEUSE(p,m)    {NMG_GETSTRUCT(p, edgeuse); NMG_INCR_INDEX(p,m);}
#define GET_VERTEX(p,m)	    {NMG_GETSTRUCT(p, vertex); NMG_INCR_INDEX(p,m);}
#define GET_VERTEX_G(p,m)   {NMG_GETSTRUCT(p, vertex_g); NMG_INCR_INDEX(p,m);}
#define GET_VERTEXUSE(p,m)  {NMG_GETSTRUCT(p, vertexuse); NMG_INCR_INDEX(p,m);}
#define GET_VERTEXUSE_A_PLANE(p,m) {NMG_GETSTRUCT(p, vertexuse_a_plane); NMG_INCR_INDEX(p,m);}
#define GET_VERTEXUSE_A_CNURB(p,m) {NMG_GETSTRUCT(p, vertexuse_a_cnurb); NMG_INCR_INDEX(p,m);}

#define FREE_MODEL(p)	    NMG_FREESTRUCT(p, model)
#define FREE_REGION(p)	    NMG_FREESTRUCT(p, nmgregion)
#define FREE_REGION_A(p)    NMG_FREESTRUCT(p, nmgregion_a)
#define FREE_SHELL(p)	    NMG_FREESTRUCT(p, shell)
#define FREE_SHELL_A(p)	    NMG_FREESTRUCT(p, shell_a)
#define FREE_FACE(p)	    NMG_FREESTRUCT(p, face)
#define FREE_FACE_G_PLANE(p) NMG_FREESTRUCT(p, face_g_plane)
#define FREE_FACE_G_SNURB(p) NMG_FREESTRUCT(p, face_g_snurb)
#define FREE_FACEUSE(p)	    NMG_FREESTRUCT(p, faceuse)
#define FREE_LOOP(p)	    NMG_FREESTRUCT(p, loop)
#define FREE_LOOP_G(p)	    NMG_FREESTRUCT(p, loop_g)
#define FREE_LOOPUSE(p)	    NMG_FREESTRUCT(p, loopuse)
#define FREE_LOOPUSE_A(p)   NMG_FREESTRUCT(p, loopuse_a)
#define FREE_EDGE(p)	    NMG_FREESTRUCT(p, edge)
#define FREE_EDGE_G_LSEG(p)  NMG_FREESTRUCT(p, edge_g_lseg)
#define FREE_EDGE_G_CNURB(p) NMG_FREESTRUCT(p, edge_g_cnurb)
#define FREE_EDGEUSE(p)	    NMG_FREESTRUCT(p, edgeuse)
#define FREE_VERTEX(p)	    NMG_FREESTRUCT(p, vertex)
#define FREE_VERTEX_G(p)    NMG_FREESTRUCT(p, vertex_g)
#define FREE_VERTEXUSE(p)   NMG_FREESTRUCT(p, vertexuse)
#define FREE_VERTEXUSE_A_PLANE(p) NMG_FREESTRUCT(p, vertexuse_a_plane)
#define FREE_VERTEXUSE_A_CNURB(p) NMG_FREESTRUCT(p, vertexuse_a_cnurb)

/** Do two edgeuses share the same two vertices? If yes, eu's should be joined. */
#define NMG_ARE_EUS_ADJACENT(_eu1,_eu2)	(  \
	( (_eu1)->vu_p->v_p == (_eu2)->vu_p->v_p &&   \
	  (_eu1)->eumate_p->vu_p->v_p == (_eu2)->eumate_p->vu_p->v_p )  ||  \
	( (_eu1)->vu_p->v_p == (_eu2)->eumate_p->vu_p->v_p &&  \
	  (_eu1)->eumate_p->vu_p->v_p == (_eu2)->vu_p->v_p ) )

/** Compat: Used in nmg_misc.c and nmg_mod.c */
#define EDGESADJ(_e1, _e2) NMG_ARE_EUS_ADJACENT(_e1,_e2)

/** Print a plane equation. */
#define PLPRINT(_s, _pl) bu_log("%s %gx + %gy + %gz = %g\n", (_s), \
	(_pl)[0], (_pl)[1], (_pl)[2], (_pl)[3])


/** values for the "allhits" argument to mg_class_pt_fu_except() */
#define NMG_FPI_FIRST	0	/**< @brief return after finding first touch */
#define NMG_FPI_PERGEOM	1	/**< @brief find all touches,
				 *  call user funcs once for each
				 * geometry element touched
				 */
#define NMG_FPI_PERUSE	2	/**< @brief find all touches,
				 *  call user funcs once for each
				 * use of geom elements touched
				 */


struct nmg_boolstruct {
	struct bu_ptbl	ilist;		/**< @brief vertexuses on intersection line */
	fastf_t		tol;
	point_t		pt;		/**< @brief line of intersection */
	vect_t		dir;
	int		coplanar;
	char		*vertlist;
	int		vlsize;
	struct model	*model;
};

#define PREEXIST 1
#define NEWEXIST 2


#define VU_PREEXISTS(_bs, _vu) { chkidxlist((_bs), (_vu)); \
	(_bs)->vertlist[(_vu)->index] = PREEXIST; }

#define VU_NEW(_bs, _vu) { chkidxlist((_bs), (_vu)); \
	(_bs)->vertlist[(_vu)->index] = NEWEXIST; }


struct nmg_struct_counts {
	/* Actual structure counts (Xuse, then X) */
	long	model;
	long	region;
	long	region_a;
	long	shell;
	long	shell_a;
	long	faceuse;
	long	face;
	long	face_g_plane;
	long	face_g_snurb;
	long	loopuse;
	long	loop;
	long	loop_g;
	long	edgeuse;
	long	edge;
	long	edge_g_lseg;
	long	edge_g_cnurb;
	long	vertexuse;
	long	vertexuse_a_plane;
	long	vertexuse_a_cnurb;
	long	vertex;
	long	vertex_g;
	/* Abstractions */
	long	max_structs;
	long	face_loops;
	long	face_edges;
	long	face_lone_verts;
	long	wire_loops;
	long	wire_loop_edges;
	long	wire_edges;
	long	wire_lone_verts;
	long	shells_of_lone_vert;
};

/*
 *  For use with tables subscripted by NMG structure "index" values,
 *  traditional test and set macros.
 *  A value of zero indicates unset, a value of one indicates set.
 *  test-and-set returns TRUE if value was unset;  in the process,
 *  value has become set.  This is often used to detect the first
 *  time an item is used, so an alternative name is given, for clarity.
 *  Note that the somewhat simpler auto-increment form
 *	( (tab)[(p)->index]++ == 0 )
 *  is not used, to avoid the possibility of integer overflow from
 *  repeated test-and-set operations on one item.
 */
#define NMG_INDEX_VALUE(_tab,_index)	((_tab)[_index])
#define NMG_INDEX_TEST(_tab,_p)		( (_tab)[(_p)->index] )
#define NMG_INDEX_SET(_tab,_p)		{(_tab)[(_p)->index] = 1;}
#define NMG_INDEX_CLEAR(_tab,_p)	{(_tab)[(_p)->index] = 0;}
#define NMG_INDEX_TEST_AND_SET(_tab,_p)	\
	( (_tab)[(_p)->index] == 0 ? ((_tab)[(_p)->index] = 1) : 0 )
#define NMG_INDEX_IS_SET(_tab,_p)	NMG_INDEX_TEST(_tab,_p)
#define NMG_INDEX_FIRST_TIME(_tab,_p)	NMG_INDEX_TEST_AND_SET(_tab,_p)
#define NMG_INDEX_ASSIGN(_tab,_p,_val)	{(_tab)[(_p)->index] = _val;}
#define NMG_INDEX_GET(_tab,_p)		((_tab)[(_p)->index])
#define NMG_INDEX_GETP(_ty,_tab,_p)	((struct _ty *)((_tab)[(_p)->index]))
#define NMG_INDEX_OR(_tab,_p,_val)	{(_tab)[(_p)->index] |= _val;}
#define NMG_INDEX_AND(_tab,_p,_val)	{(_tab)[(_p)->index] &= _val;}
#define NMG_INDEX_RETURN_IF_SET_ELSE_SET(_tab,_index)	\
	{ if( (_tab)[_index] )  return; \
	  else (_tab)[_index] = 1; }

/* flags for manifold-ness */
#define NMG_3MANIFOLD	16
#define NMG_2MANIFOLD	4
#define NMG_1MANIFOLD	2
#define NMG_0MANIFOLD	1
#if 0
# define NMG_DANGLING	8 /* NMG_2MANIFOLD + 4th bit for special cond */
#endif

#define NMG_SET_MANIFOLD(_t,_p,_v) NMG_INDEX_OR(_t, _p, _v)
#define NMG_MANIFOLDS(_t, _p)	   NMG_INDEX_VALUE(_t, (_p)->index)
#define NMG_CP_MANIFOLD(_t, _p, _q) (_t)[(_p)->index] = (_t)[(_q)->index]

/*
 *  Bit-parameters for nmg_lu_to_vlist() poly_markers code.
 */
#define NMG_VLIST_STYLE_VECTOR			0
#define NMG_VLIST_STYLE_POLYGON			1
#define NMG_VLIST_STYLE_VISUALIZE_NORMALS	2
#define NMG_VLIST_STYLE_USE_VU_NORMALS		4
#define NMG_VLIST_STYLE_NO_SURFACES		8

/**
 *  Function table, for use with nmg_visit()
 *  Indended to have same generally the organization as nmg_struct_counts.
 *  The handler's args are long* to allow generic handlers to be written,
 *  in which case the magic number at long* specifies the object type.
 *
 *  The "vis_" prefix means the handler is visited only once.
 *  The "bef_" and "aft_" prefixes are called (respectively) before
 *  and after recursing into subsidiary structures.
 *  The 3rd arg is 0 for a "bef_" call, and 1 for an "aft_" call,
 *  to allow generic handlers to be written, if desired.
 */
struct nmg_visit_handlers {
	void	(*bef_model) NMG_ARGS((long *, genptr_t, int));
	void	(*aft_model) NMG_ARGS((long *, genptr_t, int));

	void	(*bef_region) NMG_ARGS((long *, genptr_t, int));
	void	(*aft_region) NMG_ARGS((long *, genptr_t, int));

	void	(*vis_region_a) NMG_ARGS((long *, genptr_t, int));

	void	(*bef_shell) NMG_ARGS((long *, genptr_t, int));
	void	(*aft_shell) NMG_ARGS((long *, genptr_t, int));

	void	(*vis_shell_a) NMG_ARGS((long *, genptr_t, int));

	void	(*bef_faceuse) NMG_ARGS((long *, genptr_t, int));
	void	(*aft_faceuse) NMG_ARGS((long *, genptr_t, int));

	void	(*vis_face) NMG_ARGS((long *, genptr_t, int));
	void	(*vis_face_g) NMG_ARGS((long *, genptr_t, int));

	void	(*bef_loopuse) NMG_ARGS((long *, genptr_t, int));
	void	(*aft_loopuse) NMG_ARGS((long *, genptr_t, int));

	void	(*vis_loop) NMG_ARGS((long *, genptr_t, int));
	void	(*vis_loop_g) NMG_ARGS((long *, genptr_t, int));

	void	(*bef_edgeuse) NMG_ARGS((long *, genptr_t, int));
	void	(*aft_edgeuse) NMG_ARGS((long *, genptr_t, int));

	void	(*vis_edge) NMG_ARGS((long *, genptr_t, int));
	void	(*vis_edge_g) NMG_ARGS((long *, genptr_t, int));

	void	(*bef_vertexuse) NMG_ARGS((long *, genptr_t, int));
	void	(*aft_vertexuse) NMG_ARGS((long *, genptr_t, int));

	void	(*vis_vertexuse_a) NMG_ARGS((long *, genptr_t, int));
	void	(*vis_vertex) NMG_ARGS((long *, genptr_t, int));
	void	(*vis_vertex_g) NMG_ARGS((long *, genptr_t, int));
};

#endif
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

