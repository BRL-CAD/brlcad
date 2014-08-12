/*                           N M G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file nmg.h
 *
 * Definition of data structures for "Non-Manifold Geometry
 * Modelling."  Developed from "Non-Manifold Geometric Boundary
 * Modeling" by Kevin Weiler, 5/7/87 (SIGGraph 1989 Course #20 Notes)
 *
 * See also "Topological Structures for Geometric Modeling"
 * by Kevin J. Weiler - RPI Phd thesis from 1986.
 *
 */


#ifndef NMG_H
#define NMG_H

#include "common.h"

__BEGIN_DECLS

/* interface headers */
#include "bu/list.h"
#include "bu/log.h"
#include "bu/magic.h"
#include "bu/ptbl.h"
#include "vmath.h"

#ifndef NMG_EXPORT
#  if defined(NMG_DLL_EXPORTS) && defined(NMG_DLL_IMPORTS)
#    error "Only NMG_DLL_EXPORTS or NMG_DLL_IMPORTS can be defined, not both."
#  elif defined(NMG_DLL_EXPORTS)
#    define NMG_EXPORT __declspec(dllexport)
#  elif defined(NMG_DLL_IMPORTS)
#    define NMG_EXPORT __declspec(dllimport)
#  else
#    define NMG_EXPORT
#  endif
#endif

/**
 * controls the libnmg debug level
 */
NMG_EXPORT extern uint32_t nmg_debug;

#define DEBUG_PL_ANIM   0x00000001	/**< @brief 1 mged: animated evaluation */
#define DEBUG_PL_SLOW   0x00000002	/**< @brief 2 mged: add delays to animation */
#define DEBUG_GRAPHCL   0x00000004	/**< @brief 3 mged: graphic classification */
#define DEBUG_PL_LOOP   0x00000008	/**< @brief 4 loop class (needs GRAPHCL) */
#define DEBUG_PLOTEM    0x00000010	/**< @brief 5 make plots in debugged routines (needs other flags set too) */
#define DEBUG_POLYSECT  0x00000020	/**< @brief 6 nmg_inter: face intersection */
#define DEBUG_VERIFY    0x00000040	/**< @brief 7 nmg_vshell() frequently, verify health */
#define DEBUG_BOOL      0x00000080	/**< @brief 8 nmg_bool:  */
#define DEBUG_CLASSIFY  0x00000100	/**< @brief 9 nmg_class: */
#define DEBUG_BOOLEVAL  0x00000200	/**< @brief 10 nmg_eval: what to retain */
#define DEBUG_BASIC     0x00000400	/**< @brief 11 nmg_mk.c and nmg_mod.c routines */
#define DEBUG_MESH      0x00000800	/**< @brief 12 nmg_mesh: describe edge search */
#define DEBUG_MESH_EU   0x00001000	/**< @brief 13 nmg_mesh: list edges meshed */
#define DEBUG_POLYTO    0x00002000	/**< @brief 14 nmg_misc: polytonmg */
#define DEBUG_LABEL_PTS 0x00004000	/**< @brief 15 label points in plot files */
#define NMG_DEBUG_UNUSED1   0x00008000	/**< @brief 16 UNUSED */
#define DEBUG_NMGRT     0x00010000	/**< @brief 17 ray tracing */
#define DEBUG_FINDEU    0x00020000	/**< @brief 18 nmg_mod: nmg_findeu() */
#define DEBUG_CMFACE    0x00040000	/**< @brief 19 nmg_mod: nmg_cmface() */
#define DEBUG_CUTLOOP   0x00080000	/**< @brief 20 nmg_mod: nmg_cut_loop */
#define DEBUG_VU_SORT   0x00100000	/**< @brief 21 nmg_fcut: coincident vu sort */
#define DEBUG_FCUT      0x00200000	/**< @brief 22 nmg_fcut: face cutter */
#define DEBUG_RT_SEGS   0x00400000	/**< @brief 23 nmg_rt_segs: */
#define DEBUG_RT_ISECT  0x00800000	/**< @brief 24 nmg_rt_isect: */
#define DEBUG_TRI       0x01000000	/**< @brief 25 nmg_tri */
#define DEBUG_PT_FU     0x02000000	/**< @brief 26 nmg_pt_fu */
#define DEBUG_MANIF     0x04000000	/**< @brief 27 nmg_manif */
#define NMG_DEBUG_UNUSED2   0x08000000	/**< @brief 28 UNUSED */
#define NMG_DEBUG_UNUSED3   0x10000000	/**< @brief 29 UNUSED */
#define NMG_DEBUG_UNUSED4   0x20000000	/**< @brief 30 UNUSED */
#define NMG_DEBUG_UNUSED5   0x40000000	/**< @brief 31 UNUSED */
#define NMG_DEBUG_UNUSED6   0x80000000	/**< @brief 32 UNUSED */
#define NMG_DEBUG_FORMAT \
"\020\033MANIF\032PTFU\031TRIANG\030RT_ISECT\
\027RT_SEGS\026FCUT\025VU_SORT\024CUTLOOP\023CMFACE\022FINDEU\021RT_ISECT\020(FREE)\
\017LABEL_PTS\016POLYTO\015MESH_EU\014MESH\013BASIC\012BOOLEVAL\011CLASSIFY\
\010BOOL\7VERIFY\6POLYSECT\5PLOTEM\4PL_LOOP\3GRAPHCL\2PL_SLOW\1PL_ANIM"

/* Boolean operations */
#define NMG_BOOL_SUB   1	/**< @brief subtraction */
#define NMG_BOOL_ADD   2	/**< @brief addition/union */
#define NMG_BOOL_ISECT 4	/**< @brief intersection */

/* Boolean classifications */
#define NMG_CLASS_Unknown   -1
#define NMG_CLASS_AinB       0
#define NMG_CLASS_AonBshared 1
#define NMG_CLASS_AonBanti   2
#define NMG_CLASS_AoutB      3
#define NMG_CLASS_BinA       4
#define NMG_CLASS_BonAshared 5
#define NMG_CLASS_BonAanti   6
#define NMG_CLASS_BoutA      7

/* orientations available.  All topological elements are orientable. */
#define OT_NONE      0 /**< @brief no orientation (error) */
#define OT_SAME      1 /**< @brief orientation same */
#define OT_OPPOSITE  2 /**< @brief orientation opposite */
#define OT_UNSPEC    3 /**< @brief orientation unspecified */
#define OT_BOOLPLACE 4 /**< @brief object is intermediate data for boolean ops */

/**
 * macros to check/validate a structure pointer
 */
#define NMG_CKMAG(_ptr, _magic, _str)	BU_CKMAG(_ptr, _magic, _str)
#define NMG_CK2MAG(_ptr, _magic1, _magic2, _str) \
	if (!(_ptr) || (*((uint32_t *)(_ptr)) != (_magic1) && *((uint32_t *)(_ptr)) != (_magic2))) { \
		bu_badmagic((uint32_t *)(_ptr), _magic1, _str, __FILE__, __LINE__); \
	}

#define NMG_CK_MODEL(_p)              NMG_CKMAG(_p, NMG_MODEL_MAGIC, "model")
#define NMG_CK_REGION(_p)             NMG_CKMAG(_p, NMG_REGION_MAGIC, "region")
#define NMG_CK_REGION_A(_p)           NMG_CKMAG(_p, NMG_REGION_A_MAGIC, "region_a")
#define NMG_CK_SHELL(_p)              NMG_CKMAG(_p, NMG_SHELL_MAGIC, "shell")
#define NMG_CK_SHELL_A(_p)            NMG_CKMAG(_p, NMG_SHELL_A_MAGIC, "shell_a")
#define NMG_CK_FACE(_p)               NMG_CKMAG(_p, NMG_FACE_MAGIC, "face")
#define NMG_CK_FACE_G_PLANE(_p)       NMG_CKMAG(_p, NMG_FACE_G_PLANE_MAGIC, "face_g_plane")
#define NMG_CK_FACE_G_SNURB(_p)       NMG_CKMAG(_p, NMG_FACE_G_SNURB_MAGIC, "face_g_snurb")
#define NMG_CK_FACE_G_EITHER(_p)      NMG_CK2MAG(_p, NMG_FACE_G_PLANE_MAGIC, NMG_FACE_G_SNURB_MAGIC, "face_g_plane|face_g_snurb")
#define NMG_CK_FACEUSE(_p)            NMG_CKMAG(_p, NMG_FACEUSE_MAGIC, "faceuse")
#define NMG_CK_LOOP(_p)               NMG_CKMAG(_p, NMG_LOOP_MAGIC, "loop")
#define NMG_CK_LOOP_G(_p)             NMG_CKMAG(_p, NMG_LOOP_G_MAGIC, "loop_g")
#define NMG_CK_LOOPUSE(_p)            NMG_CKMAG(_p, NMG_LOOPUSE_MAGIC, "loopuse")
#define NMG_CK_EDGE(_p)               NMG_CKMAG(_p, NMG_EDGE_MAGIC, "edge")
#define NMG_CK_EDGE_G_LSEG(_p)        NMG_CKMAG(_p, NMG_EDGE_G_LSEG_MAGIC, "edge_g_lseg")
#define NMG_CK_EDGE_G_CNURB(_p)       NMG_CKMAG(_p, NMG_EDGE_G_CNURB_MAGIC, "edge_g_cnurb")
#define NMG_CK_EDGE_G_EITHER(_p)      NMG_CK2MAG(_p, NMG_EDGE_G_LSEG_MAGIC, NMG_EDGE_G_CNURB_MAGIC, "edge_g_lseg|edge_g_cnurb")
#define NMG_CK_EDGEUSE(_p)            NMG_CKMAG(_p, NMG_EDGEUSE_MAGIC, "edgeuse")
#define NMG_CK_VERTEX(_p)             NMG_CKMAG(_p, NMG_VERTEX_MAGIC, "vertex")
#define NMG_CK_VERTEX_G(_p)           NMG_CKMAG(_p, NMG_VERTEX_G_MAGIC, "vertex_g")
#define NMG_CK_VERTEXUSE(_p)          NMG_CKMAG(_p, NMG_VERTEXUSE_MAGIC, "vertexuse")
#define NMG_CK_VERTEXUSE_A_PLANE(_p)  NMG_CKMAG(_p, NMG_VERTEXUSE_A_PLANE_MAGIC, "vertexuse_a_plane")
#define NMG_CK_VERTEXUSE_A_CNURB(_p)  NMG_CKMAG(_p, NMG_VERTEXUSE_A_CNURB_MAGIC, "vertexuse_a_cnurb")
#define NMG_CK_VERTEXUSE_A_EITHER(_p) NMG_CK2MAG(_p, NMG_VERTEXUSE_A_PLANE_MAGIC, NMG_VERTEXUSE_A_CNURB_MAGIC, "vertexuse_a_plane|vertexuse_a_cnurb")
#define NMG_CK_LIST(_p)               BU_CKMAG(_p, BU_LIST_HEAD_MAGIC, "bu_list")

/* Used only in nmg_mod.c */
#define NMG_TEST_EDGEUSE(_p) \
	if (!(_p)->l.forw || !(_p)->l.back || !(_p)->eumate_p || \
	    !(_p)->radial_p || !(_p)->e_p || !(_p)->vu_p || \
	    !(_p)->up.magic_p) { \
		bu_log("in %s at %d, Bad edgeuse member pointer\n", \
			 __FILE__, __LINE__);  nmg_pr_eu(_p, (char *)NULL); \
		bu_bomb("NULL pointer\n"); \
	} else if ((_p)->vu_p->up.eu_p != (_p) || \
		(_p)->eumate_p->vu_p->up.eu_p != (_p)->eumate_p) {\
		bu_log("in %s at %d, edgeuse lost vertexuse\n", \
			 __FILE__, __LINE__); \
		bu_bomb("bye"); \
	}

NMG_EXPORT extern struct bu_list rtg_vlfree; /**< @brief  head of bn_vlist freelist */

/**
 * Applications that are going to use RT_ADD_VLIST and RT_GET_VLIST
 * are required to execute this macro once, first:
 *
 * BU_LIST_INIT(&RTG.rtg_vlfree);
 *
 * Note that RT_GET_VLIST and RT_FREE_VLIST are non-PARALLEL.
 */
#define RT_GET_VLIST(p) BN_GET_VLIST(&rtg_vlfree, p)

/** Place an entire chain of bn_vlist structs on the freelist */
#define RT_FREE_VLIST(hd) BN_FREE_VLIST(&rtg_vlfree, hd)

#define RT_ADD_VLIST(hd, pnt, draw) BN_ADD_VLIST(&rtg_vlfree, hd, pnt, draw)

/** Set a point size to apply to the vlist elements that follow. */
#define RT_VLIST_SET_POINT_SIZE(hd, size) BN_VLIST_SET_POINT_SIZE(&rtg_vlfree, hd, size)

/** Set a line width to apply to the vlist elements that follow. */
#define RT_VLIST_SET_LINE_WIDTH(hd, width) BN_VLIST_SET_LINE_WIDTH(&rtg_vlfree, hd, width)

/**
 * @brief
 * Definition of a knot vector.
 *
 * Not found independently, but used in the cnurb and snurb
 * structures.  (Exactly the same as the definition in nurb.h)
 */
struct knot_vector {
    uint32_t magic;
    int k_size;		/**< @brief knot vector size */
    fastf_t * knots;	/**< @brief pointer to knot vector */
};

/*
 * NOTE: We rely on the fact that the first 32 bits in a struct is the
 * magic number (which is used to identify the struct type).  This may
 * be either a magic value, or an rt_list structure, which starts with
 * a magic number.
 *
 * To these ends, there is a standard ordering for fields in
 * "object-use" structures.  That ordering is:
 *
 *   1) magic number, or rt_list structure
 *   2) pointer to parent
 *   5) pointer to mate
 *   6) pointer to geometry
 *   7) pointer to attributes
 *   8) pointer to child(ren)
 */

/**
 * When a shell encloses volume, it's done entirely by the list of
 * faceuses.
 *
 * The wire loopuses (each of which heads a list of edges) define a
 * set of connected line segments which form a closed path, but do not
 * enclose either volume or surface area.
 *
 * The wire edgeuses are disconnected line segments.  There is a
 * special interpretation to the eu_hd list of wire edgeuses.  Unlike
 * edgeuses seen in loops, the eu_hd list contains eu1, eu1mate, eu2,
 * eu2mate, ..., where each edgeuse and its mate comprise a
 * *non-connected* "wire" edge which starts at eu1->vu_p->v_p and ends
 * at eu1mate->vu_p->v_p.  There is no relationship between the pairs
 * of edgeuses at all, other than that they all live on the same
 * linked list.
 */
struct shell {
    uint32_t magic;
    struct shell_a *sa_p;	/**< @brief attribs */
    struct bu_list fu_hd;	/**< @brief list of face uses in shell */
    struct bu_list lu_hd;	/**< @brief wire loopuses (edge groups) */
    struct bu_list eu_hd;	/**< @brief wire list (shell has wires) */
    struct vertexuse *vu_p;	/**< @brief internal ptr to single vertexuse */
    char *manifolds;	/**< @brief structure 1-3manifold table */
    long index;			/**< @brief struct # in this model */
    long maxindex;		/**< @brief # of structs so far */
};

struct shell_a {
    uint32_t magic;
    point_t min_pt;		/**< @brief minimums of bounding box */
    point_t max_pt;		/**< @brief maximums of bounding box */
    long index;			/**< @brief struct # in this model */
};

/**
 * Note: there will always be exactly two faceuse's using a face.  To
 * find them, go up fu_p for one, then across fumate_p to other.
 */
struct face {
    struct bu_list l;		/**< @brief faces in face_g's f_hd list */
    struct faceuse *fu_p;	/**< @brief Ptr up to one use of this face */
    union {
	uint32_t *magic_p;
	struct face_g_plane *plane_p;
	struct face_g_snurb *snurb_p;
    } g;			/**< @brief geometry */
    int flip;			/**< @brief !0 ==> flip normal of fg */
    /* These might be better stored in a face_a (not faceuse_a!) */
    /* These are not stored on disk */
    point_t min_pt;		/**< @brief minimums of bounding box */
    point_t max_pt;		/**< @brief maximums of bounding box */
    long index;			/**< @brief struct # in this model */
};

struct face_g_plane {
    uint32_t magic;
    struct bu_list f_hd;	/**< @brief list of faces sharing this surface */
    plane_t N;			/**< @brief Plane equation (incl normal) */
    long index;			/**< @brief struct # in this model */
};

struct face_g_snurb {
    /* NOTICE: l.forw & l.back *not* stored in database.  They are for
     * bspline primitive internal use only.
     */
    struct bu_list l;
    struct bu_list f_hd;	/**< @brief list of faces sharing this surface */
    int order[2];		/**< @brief surface order [0] = u, [1] = v */
    struct knot_vector u;	/**< @brief surface knot vectors */
    struct knot_vector v;	/**< @brief surface knot vectors */
    /* surface control points */
    int s_size[2];		/**< @brief mesh size, u, v */
    int pt_type;		/**< @brief surface point type */
    fastf_t *ctl_points;	/**< @brief array [size[0]*size[1]] */
    /* START OF ITEMS VALID IN-MEMORY ONLY -- NOT STORED ON DISK */
    int dir;			/**< @brief direction of last refinement */
    point_t min_pt;		/**< @brief min corner of bounding box */
    point_t max_pt;		/**< @brief max corner of bounding box */
    /* END OF ITEMS VALID IN-MEMORY ONLY -- NOT STORED ON DISK */
    long index;			/**< @brief struct # in this model */
};

struct faceuse {
    struct bu_list l;		/**< @brief fu's, in shell's fu_hd list */
    struct shell *s_p;		/**< @brief owning shell */
    struct faceuse *fumate_p;	/**< @brief opposite side of face */
    int orientation;		/**< @brief rel to face geom defn */
    int outside;		/**< @brief RESERVED for future:  See Lee Butler */
    struct face *f_p;		/**< @brief face definition and attributes */
    struct bu_list lu_hd;	/**< @brief list of loops in face-use */
    long index;			/**< @brief struct # in this model */
};

/** Returns a 3-tuple (vect_t), given faceuse and state of flip flags */
#define NMG_GET_FU_NORMAL(_N, _fu) { \
	register const struct faceuse *_fu1 = (_fu); \
	register const struct face_g_plane *_fg; \
	NMG_CK_FACEUSE(_fu1); \
	NMG_CK_FACE(_fu1->f_p); \
	_fg = _fu1->f_p->g.plane_p; \
	NMG_CK_FACE_G_PLANE(_fg); \
	if ((_fu1->orientation != OT_SAME) != (_fu1->f_p->flip != 0)) { \
		VREVERSE(_N, _fg->N); \
	} else { \
		VMOVE(_N, _fg->N); \
	} }

/**
 * Returns a 4-tuple (plane_t), given faceuse and state of flip flags.
 */
#define NMG_GET_FU_PLANE(_N, _fu) { \
	register const struct faceuse *_fu1 = (_fu); \
	register const struct face_g_plane *_fg; \
	NMG_CK_FACEUSE(_fu1); \
	NMG_CK_FACE(_fu1->f_p); \
	_fg = _fu1->f_p->g.plane_p; \
	NMG_CK_FACE_G_PLANE(_fg); \
	if ((_fu1->orientation != OT_SAME) != (_fu1->f_p->flip != 0)) { \
		HREVERSE(_N, _fg->N); \
	} else { \
		HMOVE(_N, _fg->N); \
	} }

/**
 * To find all the uses of this loop, use lu_p for one loopuse, then
 * go down and find an edge, then wander around either eumate_p or
 * radial_p from there.
 *
 * Normally, down_hd heads a doubly linked list of edgeuses.  But,
 * before using it, check BU_LIST_FIRST_MAGIC(&lu->down_hd) for the
 * magic number type.  If this is a self-loop on a single vertexuse,
 * then get the vertex pointer with vu = BU_LIST_FIRST(vertexuse,
 * &lu->down_hd)
 *
 * This is an especially dangerous storage efficiency measure
 * ("hack"), because the list that the vertexuse structure belongs to
 * is headed, not by a superior element type, but by the vertex
 * structure.  When a loopuse needs to point down to a vertexuse, rip
 * off the forw pointer.  Take careful note that this is just a
 * pointer, **not** the head of a linked list (single, double, or
 * otherwise)!  Exercise great care!
 *
 * The edges of an exterior (OT_SAME) loop occur in counter-clockwise
 * order, as viewed from the normalward side (outside).
 */
#define RT_LIST_SET_DOWN_TO_VERT(_hp, _vu) { \
	(_hp)->forw = &((_vu)->l); (_hp)->back = (struct bu_list *)NULL; }

struct loop {
    uint32_t magic;
    struct loopuse *lu_p;	/**< @brief Ptr to one use of this loop */
    struct loop_g *lg_p;	/**< @brief Geometry */
    long index;			/**< @brief struct # in this model */
};

struct loop_g {
    uint32_t magic;
    point_t min_pt;		/**< @brief minimums of bounding box */
    point_t max_pt;		/**< @brief maximums of bounding box */
    long index;			/**< @brief struct # in this model */
};

struct loopuse {
    struct bu_list l;		/**< @brief lu's, in fu's lu_hd, or shell's lu_hd */
    union {
	struct faceuse *fu_p;	/**< @brief owning face-use */
	struct shell *s_p;
	uint32_t *magic_p;
    } up;
    struct loopuse *lumate_p;	/**< @brief loopuse on other side of face */
    int orientation;		/**< @brief OT_SAME=outside loop */
    struct loop *l_p;		/**< @brief loop definition and attributes */
    struct bu_list down_hd;	/**< @brief eu list or vu pointer */
    long index;			/**< @brief struct # in this model */
};

/**
 * To find all edgeuses of an edge, use eu_p to get an arbitrary
 * edgeuse, then wander around either eumate_p or radial_p from there.
 *
 * Only the first vertex of an edge is kept in an edgeuse (eu->vu_p).
 * The other vertex can be found by either eu->eumate_p->vu_p or by
 * BU_LIST_PNEXT_CIRC(edgeuse, eu)->vu_p.  Note that the first form
 * gives a vertexuse in the faceuse of *opposite* orientation, while
 * the second form gives a vertexuse in the faceuse of the correct
 * orientation.  If going on to the vertex (vu_p->v_p), both forms are
 * identical.
 *
 * An edge_g_lseg structure represents a line in 3-space.  All edges
 * on that line should share the same edge_g.
 *
 * An edge occupies the range eu->param to eu->eumate_p->param in its
 * geometry's parameter space.  (cnurbs only)
 */
struct edge {
    uint32_t magic;
    struct edgeuse *eu_p;	/**< @brief Ptr to one use of this edge */
    long is_real;		/**< @brief artifact or modeled edge (from tessellator) */
    long index;			/**< @brief struct # in this model */
};

/**
 * IMPORTANT: First two items in edge_g_lseg and edge_g_cnurb must be
 * identical structure, so pointers are puns for both.  eu_hd2 list
 * must be in same place for both.
 */
struct edge_g_lseg {
    struct bu_list l;		/**< @brief NOTICE:  l.forw & l.back *not* stored in database.  For alignment only. */
    struct bu_list eu_hd2;	/**< @brief heads l2 list of edgeuses on this line */
    point_t e_pt;		/**< @brief parametric equation of the line */
    vect_t e_dir;
    long index;			/**< @brief struct # in this model */
};

/**
 * The ctl_points on this curve are (u, v) values on the face's
 * surface.  As a storage and performance efficiency measure, if order
 * <= 0, then the cnurb is a straight line segment in parameter space,
 * and the k.knots and ctl_points pointers will be NULL.  In this
 * case, the vertexuse_a_cnurb's at both ends of the edgeuse define
 * the path through parameter space.
 */
struct edge_g_cnurb {
    struct bu_list l;		/**< @brief NOTICE: l.forw & l.back are NOT stored in database.  For bspline primitive internal use only. */
    struct bu_list eu_hd2;	/**< @brief heads l2 list of edgeuses on this curve */
    int order;			/**< @brief Curve Order */
    struct knot_vector k;	/**< @brief curve knot vector */
    /* curve control polygon */
    int c_size;			/**< @brief number of ctl points */
    int pt_type;		/**< @brief curve point type */
    fastf_t *ctl_points;	/**< @brief array [c_size] */
    long index;			/**< @brief struct # in this model */
};

struct edgeuse {
    struct bu_list l;		/**< @brief cw/ccw edges in loop or wire edges in shell */
    struct bu_list l2;		/**< @brief member of edge_g's eu_hd2 list */
    union {
	struct loopuse *lu_p;
	struct shell *s_p;
	uint32_t *magic_p;	/**< @brief for those times when we're not sure */
    } up;
    struct edgeuse *eumate_p;	/**< @brief eu on other face or other end of wire*/
    struct edgeuse *radial_p;	/**< @brief eu on radially adj. fu (null if wire)*/
    struct edge *e_p;		/**< @brief edge definition and attributes */
    int orientation;		/**< @brief compared to geom (null if wire) */
    struct vertexuse *vu_p;	/**< @brief first vu of eu in this orient */
    union {
	uint32_t *magic_p;
	struct edge_g_lseg *lseg_p;
	struct edge_g_cnurb *cnurb_p;
    } g;			/**< @brief geometry */
    /* (u, v, w) param[] of vu is found in vu_p->vua_p->param */
    long index;			/**< @brief struct # in this model */
};

/**
 * The vertex and vertexuse structures are connected in a way
 * different from the superior kinds of topology elements.  The vertex
 * structure heads a linked list that all vertexuse's that use the
 * vertex are linked onto.
 */
struct vertex {
    uint32_t magic;
    struct bu_list vu_hd;	/**< @brief heads list of vu's of this vertex */
    struct vertex_g *vg_p;	/**< @brief geometry */
    long index;			/**< @brief struct # in this model */
};

struct vertex_g {
    uint32_t magic;
    point_t coord;		/**< @brief coordinates of vertex in space */
    long index;			/**< @brief struct # in this model */
};

struct vertexuse {
    struct bu_list l;		/**< @brief list of all vu's on a vertex */
    union {
	struct shell *s_p;	/**< @brief no fu's or eu's on shell */
	struct loopuse *lu_p;	/**< @brief loopuse contains single vertex */
	struct edgeuse *eu_p;	/**< @brief eu causing this vu */
	uint32_t *magic_p; /**< @brief for those times when we're not sure */
    } up;
    struct vertex *v_p;		/**< @brief vertex definition and attributes */
    union {
	uint32_t *magic_p;
	struct vertexuse_a_plane *plane_p;
	struct vertexuse_a_cnurb *cnurb_p;
    } a;			/**< @brief Attributes */
    long index;			/**< @brief struct # in this model */
};

struct vertexuse_a_plane {
    uint32_t magic;
    vect_t N;			/**< @brief (opt) surface Normal at vertexuse */
    long index;			/**< @brief struct # in this model */
};

struct vertexuse_a_cnurb {
    uint32_t magic;
    fastf_t param[3];		/**< @brief (u, v, w) of vu on eu's cnurb */
    long index;			/**< @brief struct # in this model */
};

/**
 * storage allocation support
 * OBSOLETE
 */
#define NMG_GETSTRUCT(p, str) BU_GET(p, struct str)

/**
 * storage de-allocation support
 * OBSOLETE
 */
#define NMG_FREESTRUCT(ptr, str) BU_PUT(ptr, struct str)


/*
 * Macros to create and destroy storage for the NMG data structures.
 * Since nmg_mk.c and g_nmg.c are the only source file which should
 * perform these most fundamental operations, the macros do not belong
 * in nmg.h In particular, application code should NEVER do these
 * things.  Any need to do so should be handled by extending nmg_mk.c
 */
#define NMG_INCR_INDEX(_p, _s)	\
	NMG_CK_SHELL(_s); (_p)->index = ((_s)->maxindex)++


#define GET_SHELL(p, s)             {NMG_GETSTRUCT(p, shell); NMG_INCR_INDEX(p, s);}
#define GET_SHELL_A(p, s)           {NMG_GETSTRUCT(p, shell_a); NMG_INCR_INDEX(p, s);}
#define GET_FACE(p, s)              {NMG_GETSTRUCT(p, face); NMG_INCR_INDEX(p, s);}
#define GET_FACE_G_PLANE(p, s)      {NMG_GETSTRUCT(p, face_g_plane); NMG_INCR_INDEX(p, s);}
#define GET_FACE_G_SNURB(p, s)      {NMG_GETSTRUCT(p, face_g_snurb); NMG_INCR_INDEX(p, s);}
#define GET_FACEUSE(p, s)           {NMG_GETSTRUCT(p, faceuse); NMG_INCR_INDEX(p, s);}
#define GET_LOOP(p, s)              {NMG_GETSTRUCT(p, loop); NMG_INCR_INDEX(p, s);}
#define GET_LOOP_G(p, s)            {NMG_GETSTRUCT(p, loop_g); NMG_INCR_INDEX(p, s);}
#define GET_LOOPUSE(p, s)           {NMG_GETSTRUCT(p, loopuse); NMG_INCR_INDEX(p, s);}
#define GET_LOOPUSE_A(p, s)         {NMG_GETSTRUCT(p, loopuse_a); NMG_INCR_INDEX(p, s);}
#define GET_EDGE(p, s)              {NMG_GETSTRUCT(p, edge); NMG_INCR_INDEX(p, s);}
#define GET_EDGE_G_LSEG(p, s)       {NMG_GETSTRUCT(p, edge_g_lseg); NMG_INCR_INDEX(p, s);}
#define GET_EDGE_G_CNURB(p, s)      {NMG_GETSTRUCT(p, edge_g_cnurb); NMG_INCR_INDEX(p, s);}
#define GET_EDGEUSE(p, s)           {NMG_GETSTRUCT(p, edgeuse); NMG_INCR_INDEX(p, s);}
#define GET_VERTEX(p, s)            {NMG_GETSTRUCT(p, vertex); NMG_INCR_INDEX(p, s);}
#define GET_VERTEX_G(p, s)          {NMG_GETSTRUCT(p, vertex_g); NMG_INCR_INDEX(p, s);}
#define GET_VERTEXUSE(p, s)         {NMG_GETSTRUCT(p, vertexuse); NMG_INCR_INDEX(p, s);}
#define GET_VERTEXUSE_A_PLANE(p, s) {NMG_GETSTRUCT(p, vertexuse_a_plane); NMG_INCR_INDEX(p, s);}
#define GET_VERTEXUSE_A_CNURB(p, s) {NMG_GETSTRUCT(p, vertexuse_a_cnurb); NMG_INCR_INDEX(p, s);}


#define FREE_MODEL(p)             NMG_FREESTRUCT(p, model)
#define FREE_REGION(p)            NMG_FREESTRUCT(p, nmgregion)
#define FREE_REGION_A(p)          NMG_FREESTRUCT(p, nmgregion_a)
#define FREE_SHELL(p)             NMG_FREESTRUCT(p, shell)
#define FREE_SHELL_A(p)           NMG_FREESTRUCT(p, shell_a)
#define FREE_FACE(p)              NMG_FREESTRUCT(p, face)
#define FREE_FACE_G_PLANE(p)      NMG_FREESTRUCT(p, face_g_plane)
#define FREE_FACE_G_SNURB(p)      NMG_FREESTRUCT(p, face_g_snurb)
#define FREE_FACEUSE(p)           NMG_FREESTRUCT(p, faceuse)
#define FREE_LOOP(p)              NMG_FREESTRUCT(p, loop)
#define FREE_LOOP_G(p)            NMG_FREESTRUCT(p, loop_g)
#define FREE_LOOPUSE(p)           NMG_FREESTRUCT(p, loopuse)
#define FREE_LOOPUSE_A(p)         NMG_FREESTRUCT(p, loopuse_a)
#define FREE_EDGE(p)              NMG_FREESTRUCT(p, edge)
#define FREE_EDGE_G_LSEG(p)       NMG_FREESTRUCT(p, edge_g_lseg)
#define FREE_EDGE_G_CNURB(p)      NMG_FREESTRUCT(p, edge_g_cnurb)
#define FREE_EDGEUSE(p)           NMG_FREESTRUCT(p, edgeuse)
#define FREE_VERTEX(p)            NMG_FREESTRUCT(p, vertex)
#define FREE_VERTEX_G(p)          NMG_FREESTRUCT(p, vertex_g)
#define FREE_VERTEXUSE(p)         NMG_FREESTRUCT(p, vertexuse)
#define FREE_VERTEXUSE_A_PLANE(p) NMG_FREESTRUCT(p, vertexuse_a_plane)
#define FREE_VERTEXUSE_A_CNURB(p) NMG_FREESTRUCT(p, vertexuse_a_cnurb)


/**
 * Do two edgeuses share the same two vertices? If yes, eu's should be
 * joined.
 */
#define NMG_ARE_EUS_ADJACENT(_eu1, _eu2) (\
	((_eu1)->vu_p->v_p == (_eu2)->vu_p->v_p && \
	  (_eu1)->eumate_p->vu_p->v_p == (_eu2)->eumate_p->vu_p->v_p)  || \
	((_eu1)->vu_p->v_p == (_eu2)->eumate_p->vu_p->v_p && \
	  (_eu1)->eumate_p->vu_p->v_p == (_eu2)->vu_p->v_p))

/** Compat: Used in nmg_misc.c and nmg_mod.c */
#define EDGESADJ(_e1, _e2) NMG_ARE_EUS_ADJACENT(_e1, _e2)

/** Print a plane equation. */
#define PLPRINT(_s, _pl) bu_log("%s %gx + %gy + %gz = %g\n", (_s), \
	(_pl)[0], (_pl)[1], (_pl)[2], (_pl)[3])


/** values for the "allhits" argument to mg_class_pt_fu_except() */
#define NMG_FPI_FIRST   0	/**< @brief return after finding first
				 * touch
				 */
#define NMG_FPI_PERGEOM 1	/**< @brief find all touches, call
				 * user funcs once for each geometry
				 * element touched.
				 */
#define NMG_FPI_PERUSE  2	/**< @brief find all touches, call
				 * user funcs once for each use of
				 * geom elements touched.
				 */


struct nmg_boolstruct {
    struct bu_ptbl ilist;	/**< @brief vertexuses on intersection line */
    fastf_t tol;
    point_t pt;			/**< @brief line of intersection */
    vect_t dir;
    int coplanar;
    char *vertlist;
    int vlsize;
    struct model *model;
};

#define PREEXIST 1
#define NEWEXIST 2


#define VU_PREEXISTS(_bs, _vu) { chkidxlist((_bs), (_vu)); \
	(_bs)->vertlist[(_vu)->index] = PREEXIST; }

#define VU_NEW(_bs, _vu) { chkidxlist((_bs), (_vu)); \
	(_bs)->vertlist[(_vu)->index] = NEWEXIST; }


struct nmg_struct_counts {
    /* Actual structure counts (Xuse, then X) */
    long model;
    long region;
    long region_a;
    long shell;
    long shell_a;
    long faceuse;
    long face;
    long face_g_plane;
    long face_g_snurb;
    long loopuse;
    long loop;
    long loop_g;
    long edgeuse;
    long edge;
    long edge_g_lseg;
    long edge_g_cnurb;
    long vertexuse;
    long vertexuse_a_plane;
    long vertexuse_a_cnurb;
    long vertex;
    long vertex_g;
    /* Abstractions */
    long max_structs;
    long face_loops;
    long face_edges;
    long face_lone_verts;
    long wire_loops;
    long wire_loop_edges;
    long wire_edges;
    long wire_lone_verts;
    long shells_of_lone_vert;
};

/*
 * For use with tables subscripted by NMG structure "index" values,
 * traditional test and set macros.
 *
 * A value of zero indicates unset, a value of one indicates set.
 * test-and-set returns TRUE if value was unset; in the process, value
 * has become set.  This is often used to detect the first time an
 * item is used, so an alternative name is given, for clarity.
 *
 * Note that the somewhat simpler auto-increment form:
 *	((tab)[(p)->index]++ == 0)
 * is not used, to avoid the possibility of integer overflow from
 * repeated test-and-set operations on one item.
 */
#define NMG_INDEX_VALUE(_tab, _index)		((_tab)[_index])
#define NMG_INDEX_TEST(_tab, _p)		((_tab)[(_p)->index])
#define NMG_INDEX_SET(_tab, _p) {(_tab)[(_p)->index] = 1;}
#define NMG_INDEX_CLEAR(_tab, _p) {(_tab)[(_p)->index] = 0;}
#define NMG_INDEX_TEST_AND_SET(_tab, _p)	((_tab)[(_p)->index] == 0 ? ((_tab)[(_p)->index] = 1) : 0)
#define NMG_INDEX_IS_SET(_tab, _p)		NMG_INDEX_TEST(_tab, _p)
#define NMG_INDEX_FIRST_TIME(_tab, _p)		NMG_INDEX_TEST_AND_SET(_tab, _p)
#define NMG_INDEX_ASSIGN(_tab, _p, _val) {(_tab)[(_p)->index] = _val;}
#define NMG_INDEX_GET(_tab, _p)			((_tab)[(_p)->index])
#define NMG_INDEX_GETP(_ty, _tab, _p)		((struct _ty *)((_tab)[(_p)->index]))
#define NMG_INDEX_OR(_tab, _p, _val) {(_tab)[(_p)->index] |= _val;}
#define NMG_INDEX_AND(_tab, _p, _val) {(_tab)[(_p)->index] &= _val;}
#define NMG_INDEX_RETURN_IF_SET_ELSE_SET(_tab, _index) { \
	if ((_tab)[_index]) return; \
	else (_tab)[_index] = 1; \
}

/* flags for manifold-ness */
#define NMG_0MANIFOLD  1
#define NMG_1MANIFOLD  2
#define NMG_2MANIFOLD  4
#define NMG_DANGLING   8 /* NMG_2MANIFOLD + 4th bit for special cond (UNUSED) */
#define NMG_3MANIFOLD 16

#define NMG_SET_MANIFOLD(_t, _p, _v) NMG_INDEX_OR(_t, _p, _v)
#define NMG_MANIFOLDS(_t, _p)        NMG_INDEX_VALUE(_t, (_p)->index)
#define NMG_CP_MANIFOLD(_t, _p, _q)  (_t)[(_p)->index] = (_t)[(_q)->index]

/*
 * Bit-parameters for nmg_lu_to_vlist() poly_markers code.
 */
#define NMG_VLIST_STYLE_VECTOR            0
#define NMG_VLIST_STYLE_POLYGON           1
#define NMG_VLIST_STYLE_VISUALIZE_NORMALS 2
#define NMG_VLIST_STYLE_USE_VU_NORMALS    4
#define NMG_VLIST_STYLE_NO_SURFACES       8

/**
 * Function table, for use with nmg_visit().
 *
 * Intended to have same generally the organization as
 * nmg_struct_counts.  The handler's args are long* to allow generic
 * handlers to be written, in which case the magic number at long*
 * specifies the object type.
 *
 * The "vis_" prefix means the handler is visited only once.  The
 * "bef_" and "aft_" prefixes are called (respectively) before and
 * after recursing into subsidiary structures.  The 3rd arg is 0 for a
 * "bef_" call, and 1 for an "aft_" call, to allow generic handlers to
 * be written, if desired.
 */
struct nmg_visit_handlers {
    void (*bef_model)(uint32_t *, void *, int);
    void (*aft_model)(uint32_t *, void *, int);

    void (*bef_region)(uint32_t *, void *, int);
    void (*aft_region)(uint32_t *, void *, int);

    void (*vis_region_a)(uint32_t *, void *, int);

    void (*bef_shell)(uint32_t *, void *, int);
    void (*aft_shell)(uint32_t *, void *, int);

    void (*vis_shell_a)(uint32_t *, void *, int);

    void (*bef_faceuse)(uint32_t *, void *, int);
    void (*aft_faceuse)(uint32_t *, void *, int);

    void (*vis_face)(uint32_t *, void *, int);
    void (*vis_face_g)(uint32_t *, void *, int);

    void (*bef_loopuse)(uint32_t *, void *, int);
    void (*aft_loopuse)(uint32_t *, void *, int);

    void (*vis_loop)(uint32_t *, void *, int);
    void (*vis_loop_g)(uint32_t *, void *, int);

    void (*bef_edgeuse)(uint32_t *, void *, int);
    void (*aft_edgeuse)(uint32_t *, void *, int);

    void (*vis_edge)(uint32_t *, void *, int);
    void (*vis_edge_g)(uint32_t *, void *, int);

    void (*bef_vertexuse)(uint32_t *, void *, int);
    void (*aft_vertexuse)(uint32_t *, void *, int);

    void (*vis_vertexuse_a)(uint32_t *, void *, int);
    void (*vis_vertex)(uint32_t *, void *, int);
    void (*vis_vertex_g)(uint32_t *, void *, int);
};



/* MAKE routines. (nmg_mk.c) */
NMG_EXPORT extern struct shell *nmg_ms();
NMG_EXPORT extern struct shell *nmg_msv();
NMG_EXPORT extern struct faceuse *nmg_mf(struct loopuse *lu1);
NMG_EXPORT extern struct loopuse *nmg_mlv(uint32_t *magic,
					 struct vertex *v,
					 int orientation);
NMG_EXPORT extern struct edgeuse *nmg_me(struct vertex *v1,
					struct vertex *v2,
					struct shell *s);
NMG_EXPORT extern struct edgeuse *nmg_meonvu(struct vertexuse *vu);
NMG_EXPORT extern struct loopuse *nmg_ml(struct shell *s);

/* KILL routines. (nmg_mk.c) */
NMG_EXPORT extern int nmg_keg(struct edgeuse *eu);
NMG_EXPORT extern int nmg_kvu(struct vertexuse *vu);
NMG_EXPORT extern int nmg_kfu(struct faceuse *fu1);
NMG_EXPORT extern int nmg_klu(struct loopuse *lu1);
NMG_EXPORT extern int nmg_keu(struct edgeuse *eu);
NMG_EXPORT extern int nmg_keu_zl(struct shell *s,
				const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ks(struct shell *s);

/* Geometry and Attribute routines. (nmg_mk.c) */
NMG_EXPORT extern void nmg_vertex_gv(struct vertex *v,
				    const point_t pt);
NMG_EXPORT extern void nmg_vertex_g(struct vertex *v,
				   fastf_t x,
				   fastf_t y,
				   fastf_t z);
NMG_EXPORT extern void nmg_vertexuse_nv(struct vertexuse *vu,
				       const vect_t norm);
NMG_EXPORT extern void nmg_vertexuse_a_cnurb(struct vertexuse *vu,
					    const fastf_t *uvw);
NMG_EXPORT extern void nmg_edge_g(struct edgeuse *eu);
NMG_EXPORT extern void nmg_edge_g_cnurb(struct edgeuse *eu,
				       int order,
				       int n_knots,
				       fastf_t *kv,
				       int n_pts,
				       int pt_type,
				       fastf_t *points);
NMG_EXPORT extern void nmg_edge_g_cnurb_plinear(struct edgeuse *eu);
NMG_EXPORT extern int nmg_use_edge_g(struct edgeuse *eu,
				    uint32_t *eg);
NMG_EXPORT extern void nmg_loop_g(struct loop *l,
				 const struct bn_tol *tol);
NMG_EXPORT extern void nmg_face_g(struct faceuse *fu,
				 const plane_t p);
NMG_EXPORT extern void nmg_face_new_g(struct faceuse *fu,
				     const plane_t pl);
NMG_EXPORT extern void nmg_face_g_snurb(struct faceuse *fu,
				       int u_order,
				       int v_order,
				       int n_u_knots,
				       int n_v_knots,
				       fastf_t *ukv,
				       fastf_t *vkv,
				       int n_rows,
				       int n_cols,
				       int pt_type,
				       fastf_t *mesh);
NMG_EXPORT extern void nmg_face_bb(struct face *f,
				  const struct bn_tol *tol);
NMG_EXPORT extern void nmg_shell_a(struct shell *s, 
				  const struct bn_tol *tol);

/* DEMOTE routines (nmg_mk.c) */
NMG_EXPORT extern int nmg_demote_lu(struct loopuse *lu);
NMG_EXPORT extern int nmg_demote_eu(struct edgeuse *eu);

/* MODIFY routines (nmg_mk.c) */
NMG_EXPORT extern void nmg_movevu(struct vertexuse *vu,
				 struct vertex *v);
NMG_EXPORT extern void nmg_je(struct edgeuse *eudst,
			     struct edgeuse *eusrc);
NMG_EXPORT extern void nmg_unglueedge(struct edgeuse *eu);
NMG_EXPORT extern void nmg_jv(struct vertex *v1,
			     struct vertex *v2);
NMG_EXPORT extern void nmg_jfg(struct face *f1,
			      struct face *f2);
NMG_EXPORT extern void nmg_jeg(struct edge_g_lseg *dest_eg,
			      struct edge_g_lseg *src_eg);

/* SHELL Routines (nmg_mod.c) */
NMG_EXPORT extern void nmg_shell_coplanar_face_merge(struct shell *s,
						    const struct bn_tol *tol,
						    const int simplify);
NMG_EXPORT extern int nmg_simplify_shell(struct shell *s);
NMG_EXPORT extern void nmg_rm_redundancies(struct shell *s,
					  const struct bn_tol *tol);
NMG_EXPORT extern void nmg_sanitize_s_lv(struct shell *s,
					int orient);
NMG_EXPORT extern void nmg_s_split_touchingloops(struct shell *s,
						const struct bn_tol *tol);
NMG_EXPORT extern void nmg_s_join_touchingloops(struct shell		*s,
					       const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_js(struct shell	*s1,
			     struct shell	*s2,
			     const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_invert_shell(struct shell		*s);

/* FACE Routines (nmg_mod.c) */
NMG_EXPORT extern struct faceuse *nmg_cmface(struct shell *s,
					    struct vertex **vt[],
					    int n);
NMG_EXPORT extern struct faceuse *nmg_cface(struct shell *s,
					   struct vertex **vt,
					   int n);
NMG_EXPORT extern struct faceuse *nmg_add_loop_to_face(struct shell *s,
						      struct faceuse *fu,
						      struct vertex **verts,
						      int n,
						      int dir);
NMG_EXPORT extern int nmg_fu_planeeqn(struct faceuse *fu,
				     const struct bn_tol *tol);
NMG_EXPORT extern void nmg_gluefaces(struct faceuse *fulist[],
				    int n,
				    const struct bn_tol *tol);
NMG_EXPORT extern int nmg_simplify_face(struct faceuse *fu);
NMG_EXPORT extern void nmg_reverse_face(struct faceuse *fu);
NMG_EXPORT extern void nmg_mv_fu_between_shells(struct shell *dest,
					       struct shell *src,
					       struct faceuse *fu);
NMG_EXPORT extern void nmg_jf(struct faceuse *dest_fu,
			     struct faceuse *src_fu);
NMG_EXPORT extern struct faceuse *nmg_dup_face(struct faceuse *fu,
					      struct shell *s);

/* LOOP Routines (nmg_mod.c) */
NMG_EXPORT extern void nmg_jl(struct loopuse *lu,
			     struct edgeuse *eu);
NMG_EXPORT extern struct vertexuse *nmg_join_2loops(struct vertexuse *vu1,
						   struct vertexuse *vu2);
NMG_EXPORT extern struct vertexuse *nmg_join_singvu_loop(struct vertexuse *vu1,
							struct vertexuse *vu2);
NMG_EXPORT extern struct vertexuse *nmg_join_2singvu_loops(struct vertexuse *vu1,
							  struct vertexuse *vu2);
NMG_EXPORT extern struct loopuse *nmg_cut_loop(struct vertexuse *vu1,
					      struct vertexuse *vu2);
NMG_EXPORT extern struct loopuse *nmg_split_lu_at_vu(struct loopuse *lu,
						    struct vertexuse *vu);
NMG_EXPORT extern struct vertexuse *nmg_find_repeated_v_in_lu(struct vertexuse *vu);
NMG_EXPORT extern void nmg_split_touchingloops(struct loopuse *lu,
					      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_join_touchingloops(struct loopuse *lu);
NMG_EXPORT extern int nmg_get_touching_jaunts(const struct loopuse *lu,
					     struct bu_ptbl *tbl,
					     int *need_init);
NMG_EXPORT extern void nmg_kill_accordions(struct loopuse *lu);
NMG_EXPORT extern int nmg_loop_split_at_touching_jaunt(struct loopuse		*lu,
						      const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_simplify_loop(struct loopuse *lu);
NMG_EXPORT extern int nmg_kill_snakes(struct loopuse *lu);
NMG_EXPORT extern void nmg_mv_lu_between_shells(struct shell *dest,
					       struct shell *src,
					       struct loopuse *lu);
NMG_EXPORT extern void nmg_moveltof(struct faceuse *fu,
				   struct shell *s);
NMG_EXPORT extern struct loopuse *nmg_dup_loop(struct loopuse *lu,
					      uint32_t *parent,
					      long **trans_tbl);
NMG_EXPORT extern void nmg_set_lu_orientation(struct loopuse *lu,
					     int is_opposite);
NMG_EXPORT extern void nmg_lu_reorient(struct loopuse *lu);

/* EDGE Routines (nmg_mod.c) */
NMG_EXPORT extern struct edgeuse *nmg_eusplit(struct vertex *v,
					     struct edgeuse *oldeu,
					     int share_geom);
NMG_EXPORT extern struct edgeuse *nmg_esplit(struct vertex *v,
					    struct edgeuse *eu,
					    int share_geom);
NMG_EXPORT extern struct edgeuse *nmg_ebreak(struct vertex *v,
					    struct edgeuse *eu);
NMG_EXPORT extern struct edgeuse *nmg_ebreaker(struct vertex *v,
					      struct edgeuse *eu,
					      const struct bn_tol *tol);
NMG_EXPORT extern struct vertex *nmg_e2break(struct edgeuse *eu1,
					    struct edgeuse *eu2);
NMG_EXPORT extern int nmg_unbreak_edge(struct edgeuse *eu1_first);
NMG_EXPORT extern int nmg_unbreak_shell_edge_unsafe(struct edgeuse *eu1_first);
NMG_EXPORT extern struct edgeuse *nmg_eins(struct edgeuse *eu);
NMG_EXPORT extern void nmg_mv_eu_between_shells(struct shell *dest,
					       struct shell *src,
					       struct edgeuse *eu);

/* VERTEX Routines (nmg_mod.c) */
NMG_EXPORT extern void nmg_mv_vu_between_shells(struct shell *dest,
					       struct shell *src,
					       struct vertexuse *vu);

/* Model routines (nmg_info.c) */
NMG_EXPORT extern struct shell *nmg_find_shell(const uint32_t *magic_p);
NMG_EXPORT extern void nmg_shell_bb(point_t min_pt,
				   point_t max_pt,
				   const struct shell *s);

/* Shell routines (nmg_info.c) */
NMG_EXPORT extern int nmg_shell_is_empty(const struct shell *s);
NMG_EXPORT extern struct shell *nmg_find_s_of_lu(const struct loopuse *lu);
NMG_EXPORT extern struct shell *nmg_find_s_of_eu(const struct edgeuse *eu);
NMG_EXPORT extern struct shell *nmg_find_s_of_vu(const struct vertexuse *vu);

/* Face routines (nmg_info.c) */
NMG_EXPORT extern struct faceuse *nmg_find_fu_of_eu(const struct edgeuse *eu);
NMG_EXPORT extern struct faceuse *nmg_find_fu_of_lu(const struct loopuse *lu);
NMG_EXPORT extern struct faceuse *nmg_find_fu_of_vu(const struct vertexuse *vu);
NMG_EXPORT extern struct faceuse *nmg_find_fu_with_fg_in_s(const struct shell *s1,
							  const struct faceuse *fu2);
NMG_EXPORT extern double nmg_measure_fu_angle(const struct edgeuse *eu,
					     const vect_t xvec,
					     const vect_t yvec,
					     const vect_t zvec);

/* Loop routines (nmg_info.c) */
NMG_EXPORT extern struct loopuse*nmg_find_lu_of_vu(const struct vertexuse *vu);
NMG_EXPORT extern int nmg_loop_is_a_crack(const struct loopuse *lu);
NMG_EXPORT extern int	nmg_loop_is_ccw(const struct loopuse *lu,
					const plane_t norm,
					const struct bn_tol *tol);
NMG_EXPORT extern const struct vertexuse *nmg_loop_touches_self(const struct loopuse *lu);
NMG_EXPORT extern int nmg_2lu_identical(const struct edgeuse *eu1,
				       const struct edgeuse *eu2);

/* Edge routines (nmg_info.c) */
NMG_EXPORT extern struct edgeuse *nmg_find_matching_eu_in_s(const struct edgeuse	*eu1,
							   const struct shell	*s2);
NMG_EXPORT extern struct edgeuse	*nmg_findeu(const struct vertex *v1,
					    const struct vertex *v2,
					    const struct shell *s,
					    const struct edgeuse *eup,
					    int dangling_only);
NMG_EXPORT extern struct edgeuse *nmg_find_eu_in_face(const struct vertex *v1,
						     const struct vertex *v2,
						     const struct faceuse *fu,
						     const struct edgeuse *eup,
						     int dangling_only);
NMG_EXPORT extern struct edgeuse *nmg_find_e(const struct vertex *v1,
					    const struct vertex *v2,
					    const struct shell *s,
					    const struct edge *ep);
NMG_EXPORT extern struct edgeuse *nmg_find_eu_of_vu(const struct vertexuse *vu);
NMG_EXPORT extern struct edgeuse *nmg_find_eu_with_vu_in_lu(const struct loopuse *lu,
							   const struct vertexuse *vu);
NMG_EXPORT extern const struct edgeuse *nmg_faceradial(const struct edgeuse *eu);
NMG_EXPORT extern const struct edgeuse *nmg_radial_face_edge_in_shell(const struct edgeuse *eu);
NMG_EXPORT extern const struct edgeuse *nmg_find_edge_between_2fu(const struct faceuse *fu1,
								 const struct faceuse *fu2,
								 const struct bn_tol *tol);
NMG_EXPORT extern struct edge *nmg_find_e_nearest_pt2(uint32_t *magic_p,
						     const point_t pt2,
						     const mat_t mat,
						     const struct bn_tol *tol);
NMG_EXPORT extern struct edgeuse *nmg_find_matching_eu_in_s(const struct edgeuse *eu1,
							   const struct shell *s2);
NMG_EXPORT extern void nmg_eu_2vecs_perp(vect_t xvec,
					vect_t yvec,
					vect_t zvec,
					const struct edgeuse *eu,
					const struct bn_tol *tol);
NMG_EXPORT extern int nmg_find_eu_leftvec(vect_t left,
					 const struct edgeuse *eu);
NMG_EXPORT extern int nmg_find_eu_left_non_unit(vect_t left,
					       const struct edgeuse	*eu);
NMG_EXPORT extern struct edgeuse *nmg_find_ot_same_eu_of_e(const struct edge *e);

/* Vertex routines (nmg_info.c) */
NMG_EXPORT extern struct vertexuse *nmg_find_v_in_face(const struct vertex *,
						      const struct faceuse *);
NMG_EXPORT extern struct vertexuse *nmg_find_v_in_shell(const struct vertex *v,
						       const struct shell *s,
						       int edges_only);
NMG_EXPORT extern struct vertexuse *nmg_find_pt_in_lu(const struct loopuse *lu,
						     const point_t pt,
						     const struct bn_tol *tol);
NMG_EXPORT extern struct vertexuse *nmg_find_pt_in_face(const struct faceuse *fu,
						       const point_t pt,
						       const struct bn_tol *tol);
NMG_EXPORT extern struct vertex *nmg_find_pt_in_shell(const struct shell *s,
						     const point_t pt,
						     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_is_vertex_in_edgelist(const struct vertex *v,
					       const struct bu_list *hd);
NMG_EXPORT extern int nmg_is_vertex_in_looplist(const struct vertex *v,
					       const struct bu_list *hd,
					       int singletons);
NMG_EXPORT extern struct vertexuse *nmg_is_vertex_in_face(const struct vertex *v,
							 const struct face *f);
NMG_EXPORT extern int nmg_is_vertex_a_selfloop_in_shell(const struct vertex *v,
						       const struct shell *s);
NMG_EXPORT extern int nmg_is_vertex_in_facelist(const struct vertex *v,
					       const struct bu_list *hd);
NMG_EXPORT extern int nmg_is_edge_in_edgelist(const struct edge *e,
					     const struct bu_list *hd);
NMG_EXPORT extern int nmg_is_edge_in_looplist(const struct edge *e,
					     const struct bu_list *hd);
NMG_EXPORT extern int nmg_is_edge_in_facelist(const struct edge *e,
					     const struct bu_list *hd);
NMG_EXPORT extern int nmg_is_loop_in_facelist(const struct loop *l,
					     const struct bu_list *fu_hd);

/* from nmg_class.c */
NMG_EXPORT extern void nmg_class_shells(struct shell *sA,
				       struct shell *sB,
				       char **classlist,
				       const struct bn_tol *tol);

/* from nmg_fcut.c */
/* static void ptbl_vsort */
NMG_EXPORT extern int nmg_ck_vu_ptbl(struct bu_ptbl	*p,
				    struct faceuse	*fu);
NMG_EXPORT extern double nmg_vu_angle_measure(struct vertexuse	*vu,
					     vect_t x_dir,
					     vect_t y_dir,
					     int assessment,
					     int in);
NMG_EXPORT extern int nmg_wedge_class(int	ass,	/* assessment of two edges forming wedge */
				     double	a,
				     double	b);
NMG_EXPORT extern void nmg_sanitize_fu(struct faceuse	*fu);
NMG_EXPORT extern void nmg_unlist_v(struct bu_ptbl	*b,
				   fastf_t *mag,
				   struct vertex	*v);
NMG_EXPORT extern struct edge_g_lseg *nmg_face_cutjoin(struct bu_ptbl *b1,
						      struct bu_ptbl *b2,
						      fastf_t *mag1,
						      fastf_t *mag2,
						      struct faceuse *fu1,
						      struct faceuse *fu2,
						      point_t pt,
						      vect_t dir,
						      struct edge_g_lseg *eg,
						      const struct bn_tol *tol);
NMG_EXPORT extern void nmg_fcut_face_2d(struct bu_ptbl *vu_list,
				       fastf_t *mag,
				       struct faceuse *fu1,
				       struct faceuse *fu2,
				       struct bn_tol *tol);
NMG_EXPORT extern int nmg_insert_vu_if_on_edge(struct vertexuse *vu1,
					      struct vertexuse *vu2,
					      struct edgeuse *new_eu,
					      struct bn_tol *tol);
/* nmg_face_state_transition */

#define nmg_mev(_v, _u) nmg_me((_v), (struct vertex *)NULL, (_u))

/* From nmg_eval.c */
NMG_EXPORT extern void nmg_ck_lu_orientation(struct loopuse *lu,
					    const struct bn_tol *tolp);
NMG_EXPORT extern const char *nmg_class_name(int class_no);
NMG_EXPORT extern void nmg_evaluate_boolean(struct shell	*sA,
					   struct shell	*sB,
					   int		op,
					   char		**classlist,
					   const struct bn_tol	*tol);

/* The following functions cannot be publicly declared because struct
 * nmg_bool_state is private to nmg_eval.c
 */
/* nmg_eval_shell */
/* nmg_eval_action */
/* nmg_eval_plot */


/* From nmg_rt_isect.c */
NMG_EXPORT extern void nmg_rt_print_hitlist(struct bu_list *hd);

NMG_EXPORT extern void nmg_rt_print_hitmiss(struct hitmiss *a_hit);

NMG_EXPORT extern int nmg_class_ray_vs_shell(struct xray *rp,
					    const struct shell *s,
					    const int in_or_out_only,
					    const struct bn_tol *tol);

NMG_EXPORT extern void nmg_isect_ray_shell(struct ray_data *rd);

/* From nmg_ck.c */
NMG_EXPORT extern void nmg_vvg(const struct vertex_g *vg);
NMG_EXPORT extern void nmg_vvertex(const struct vertex *v,
				  const struct vertexuse *vup);
NMG_EXPORT extern void nmg_vvua(const uint32_t *vua);
NMG_EXPORT extern void nmg_vvu(const struct vertexuse *vu,
			      const uint32_t *up_magic_p);
NMG_EXPORT extern void nmg_veg(const uint32_t *eg);
NMG_EXPORT extern void nmg_vedge(const struct edge *e,
				const struct edgeuse *eup);
NMG_EXPORT extern void nmg_veu(const struct bu_list	*hp,
			      const uint32_t *up_magic_p);
NMG_EXPORT extern void nmg_vlg(const struct loop_g *lg);
NMG_EXPORT extern void nmg_vloop(const struct loop *l,
				const struct loopuse *lup);
NMG_EXPORT extern void nmg_vlu(const struct bu_list	*hp,
			      const uint32_t *up);
NMG_EXPORT extern void nmg_vfg(const struct face_g_plane *fg);
NMG_EXPORT extern void nmg_vface(const struct face *f,
				const struct faceuse *fup);
NMG_EXPORT extern void nmg_vfu(const struct bu_list	*hp,
			      const struct shell *s);
NMG_EXPORT extern void nmg_vsshell(const struct shell *s);
NMG_EXPORT extern void nmg_vshell(const struct bu_list *hp);

/* checking routines */
NMG_EXPORT extern void nmg_ck_e(const struct edgeuse *eu,
			       const struct edge *e,
			       const char *str);
NMG_EXPORT extern void nmg_ck_vu(const uint32_t *parent,
				const struct vertexuse *vu,
				const char *str);
NMG_EXPORT extern void nmg_ck_eu(const uint32_t *parent,
				const struct edgeuse *eu,
				const char *str);
NMG_EXPORT extern void nmg_ck_lg(const struct loop *l,
				const struct loop_g *lg,
				const char *str);
NMG_EXPORT extern void nmg_ck_l(const struct loopuse *lu,
			       const struct loop *l,
			       const char *str);
NMG_EXPORT extern void nmg_ck_lu(const uint32_t *parent,
				const struct loopuse *lu,
				const char *str);
NMG_EXPORT extern void nmg_ck_fg(const struct face *f,
				const struct face_g_plane *fg,
				const char *str);
NMG_EXPORT extern void nmg_ck_f(const struct faceuse *fu,
			       const struct face *f,
			       const char *str);
NMG_EXPORT extern void nmg_ck_fu(const struct shell *s,
				const struct faceuse *fu,
				const char *str);
NMG_EXPORT extern int nmg_ck_eg_verts(const struct edge_g_lseg *eg,
				     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ck_geometry(const struct shell *s,
				     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ck_face_worthless_edges(const struct faceuse *fu);
NMG_EXPORT extern void nmg_ck_lueu(const struct loopuse *lu, const char *s);
NMG_EXPORT extern int nmg_check_radial(const struct edgeuse *eu, const struct bn_tol *tol);
NMG_EXPORT extern int nmg_eu_2s_orient_bad(const struct edgeuse	*eu,
					  const struct shell	*s1,
					  const struct shell	*s2,
					  const struct bn_tol	*tol);
NMG_EXPORT extern int nmg_ck_closed_surf(const struct shell *s,
					const struct bn_tol *tol);
NMG_EXPORT extern void nmg_ck_v_in_2fus(const struct vertex *vp,
				       const struct faceuse *fu1,
				       const struct faceuse *fu2,
				       const struct bn_tol *tol);
NMG_EXPORT extern void nmg_ck_vs_in_shell(const struct shell *s,
					  const struct bn_tol *tol);

/* From nmg_inter.c */
NMG_EXPORT extern struct vertexuse *nmg_make_dualvu(struct vertex *v,
						   struct faceuse *fu,
						   const struct bn_tol *tol);
NMG_EXPORT extern struct vertexuse *nmg_enlist_vu(struct nmg_inter_struct	*is,
						 const struct vertexuse *vu,
						 struct vertexuse *dualvu,
						 fastf_t dist);
NMG_EXPORT extern void nmg_isect2d_prep(struct nmg_inter_struct *is,
				       const uint32_t *assoc_use);
NMG_EXPORT extern void nmg_isect2d_cleanup(struct nmg_inter_struct *is);
NMG_EXPORT extern void nmg_isect2d_final_cleanup(void);
NMG_EXPORT extern int nmg_isect_2faceuse(point_t pt,
					vect_t dir,
					struct faceuse *fu1,
					struct faceuse *fu2,
					const struct bn_tol *tol);
NMG_EXPORT extern void nmg_isect_vert2p_face2p(struct nmg_inter_struct *is,
					      struct vertexuse *vu1,
					      struct faceuse *fu2);
NMG_EXPORT extern struct edgeuse *nmg_break_eu_on_v(struct edgeuse *eu1,
						   struct vertex *v2,
						   struct faceuse *fu,
						   struct nmg_inter_struct *is);
NMG_EXPORT extern void nmg_break_eg_on_v(const struct edge_g_lseg	*eg,
					struct vertex		*v,
					const struct bn_tol	*tol);
NMG_EXPORT extern int nmg_isect_2colinear_edge2p(struct edgeuse	*eu1,
						struct edgeuse	*eu2,
						struct faceuse		*fu,
						struct nmg_inter_struct	*is,
						struct bu_ptbl		*l1,
						struct bu_ptbl		*l2);
NMG_EXPORT extern int nmg_isect_edge2p_edge2p(struct nmg_inter_struct	*is,
					     struct edgeuse		*eu1,
					     struct edgeuse		*eu2,
					     struct faceuse		*fu1,
					     struct faceuse		*fu2);
NMG_EXPORT extern int nmg_isect_construct_nice_ray(struct nmg_inter_struct	*is,
						  struct faceuse		*fu2);
NMG_EXPORT extern void nmg_enlist_one_vu(struct nmg_inter_struct	*is,
					const struct vertexuse	*vu,
					fastf_t			dist);
NMG_EXPORT extern int	nmg_isect_line2_edge2p(struct nmg_inter_struct	*is,
					       struct bu_ptbl		*list,
					       struct edgeuse		*eu1,
					       struct faceuse		*fu1,
					       struct faceuse		*fu2);
NMG_EXPORT extern void nmg_isect_line2_vertex2(struct nmg_inter_struct	*is,
					      struct vertexuse	*vu1,
					      struct faceuse		*fu1);
NMG_EXPORT extern int nmg_isect_two_ptbls(struct nmg_inter_struct	*is,
					 const struct bu_ptbl	*t1,
					 const struct bu_ptbl	*t2);
NMG_EXPORT extern struct edge_g_lseg	*nmg_find_eg_on_line(const uint32_t *magic_p,
							     const point_t		pt,
							     const vect_t		dir,
							     const struct bn_tol	*tol);
NMG_EXPORT extern int nmg_k0eu(struct vertex	*v);
NMG_EXPORT extern struct vertex *nmg_repair_v_near_v(struct vertex		*hit_v,
						    struct vertex		*v,
						    const struct edge_g_lseg	*eg1,
						    const struct edge_g_lseg	*eg2,
						    int			bomb,
						    const struct bn_tol	*tol);
NMG_EXPORT extern struct vertex *nmg_search_v_eg(const struct edgeuse	*eu,
						int			second,
						const struct edge_g_lseg	*eg1,
						const struct edge_g_lseg	*eg2,
						struct vertex		*hit_v,
						const struct bn_tol	*tol);
NMG_EXPORT extern struct vertex *nmg_common_v_2eg(struct edge_g_lseg	*eg1,
						 struct edge_g_lseg	*eg2,
						 const struct bn_tol	*tol);
NMG_EXPORT extern int nmg_is_vertex_on_inter(struct vertex *v,
					    struct faceuse *fu1,
					    struct faceuse *fu2,
					    struct nmg_inter_struct *is);
NMG_EXPORT extern void nmg_isect_eu_verts(struct edgeuse *eu,
					 struct vertex_g *vg1,
					 struct vertex_g *vg2,
					 struct bu_ptbl *verts,
					 struct bu_ptbl *inters,
					 const struct bn_tol *tol);
NMG_EXPORT extern void nmg_isect_eu_eu(struct edgeuse *eu1,
				      struct vertex_g *vg1a,
				      struct vertex_g *vg1b,
				      vect_t dir1,
				      struct edgeuse *eu2,
				      struct bu_ptbl *verts,
				      struct bu_ptbl *inters,
				      const struct bn_tol *tol);
NMG_EXPORT extern void nmg_isect_eu_fu(struct nmg_inter_struct *is,
				      struct bu_ptbl		*verts,
				      struct edgeuse		*eu,
				      struct faceuse          *fu);
NMG_EXPORT extern void nmg_isect_fu_jra(struct nmg_inter_struct	*is,
				       struct faceuse		*fu1,
				       struct faceuse		*fu2,
				       struct bu_ptbl		*eu1_list,
				       struct bu_ptbl		*eu2_list);
NMG_EXPORT extern void nmg_isect_line2_face2pNEW(struct nmg_inter_struct *is,
						struct faceuse *fu1, struct faceuse *fu2,
						struct bu_ptbl *eu1_list,
						struct bu_ptbl *eu2_list);
NMG_EXPORT extern int	nmg_is_eu_on_line3(const struct edgeuse	*eu,
					   const point_t		pt,
					   const vect_t		dir,
					   const struct bn_tol	*tol);
NMG_EXPORT extern struct edge_g_lseg *nmg_find_eg_between_2fg(const struct faceuse	*ofu1,
							     const struct faceuse	*fu2,
							     const struct bn_tol	*tol);
NMG_EXPORT extern struct edgeuse *nmg_does_fu_use_eg(const struct faceuse	*fu1,
						    const uint32_t	*eg);
NMG_EXPORT extern int rt_line_on_plane(const point_t	pt,
				      const vect_t	dir,
				      const plane_t	plane,
				      const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_cut_lu_into_coplanar_and_non(struct loopuse *lu,
						       plane_t pl,
						       struct nmg_inter_struct *is);
NMG_EXPORT extern void nmg_check_radial_angles(char *str,
					      struct shell *s,
					      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_faces_can_be_intersected(struct nmg_inter_struct *bs,
						  const struct faceuse *fu1,
						  const struct faceuse *fu2,
						  const struct bn_tol *tol);
NMG_EXPORT extern void nmg_isect_two_generic_faces(struct faceuse		*fu1,
						  struct faceuse		*fu2,
						  const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_crackshells(struct shell *s1,
				      struct shell *s2,
				      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_fu_touchingloops(const struct faceuse *fu);


/* From nmg_index.c */
NMG_EXPORT extern int nmg_index_of_struct(const uint32_t *p);
NMG_EXPORT extern void nmg_s_set_high_bit(struct shell *s);
NMG_EXPORT extern void nmg_s_reindex(struct shell *s, long newindex);
NMG_EXPORT extern void nmg_vls_struct_counts(struct bu_vls *str,
					    const struct nmg_struct_counts *ctr);
NMG_EXPORT extern void nmg_pr_struct_counts(const struct nmg_struct_counts *ctr,
					   const char *str);
NMG_EXPORT extern uint32_t **nmg_s_struct_count(struct nmg_struct_counts *ctr,
						    const struct shell *s);
NMG_EXPORT extern void nmg_pr_s_struct_counts(const struct shell	*s,
					     const char		*str);
NMG_EXPORT extern long nmg_find_max_index(const struct shell *s);

/* From nmg_fuse.c */
NMG_EXPORT extern int nmg_is_common_bigloop(const struct face *f1,
					   const struct face *f2);
NMG_EXPORT extern void nmg_shell_v_unique(struct shell *s1,
					  const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ptbl_vfuse(struct bu_ptbl *t,
				    const struct bn_tol *tol);
NMG_EXPORT extern int	nmg_region_both_vfuse(struct bu_ptbl *t1,
					      struct bu_ptbl *t2,
					      const struct bn_tol *tol);
/* nmg_two_region_vertex_fuse replaced with nmg_vertex_fuse */
NMG_EXPORT extern int nmg_vertex_fuse(const uint32_t *magic_p,
				     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_cnurb_is_linear(const struct edge_g_cnurb *cnrb);
NMG_EXPORT extern int nmg_snurb_is_planar(const struct face_g_snurb *srf,
					 const struct bn_tol *tol);
NMG_EXPORT extern void nmg_eval_linear_trim_curve(const struct face_g_snurb *snrb,
						 const fastf_t uvw[3],
						 point_t xyz);
NMG_EXPORT extern void nmg_eval_trim_curve(const struct edge_g_cnurb *cnrb,
					  const struct face_g_snurb *snrb,
					  const fastf_t t, point_t xyz);
/* nmg_split_trim */
NMG_EXPORT extern void nmg_eval_trim_to_tol(const struct edge_g_cnurb *cnrb,
					   const struct face_g_snurb *snrb,
					   const fastf_t t0,
					   const fastf_t t1,
					   struct bu_list *head,
					   const struct bn_tol *tol);
/* nmg_split_linear_trim */
NMG_EXPORT extern void nmg_eval_linear_trim_to_tol(const struct edge_g_cnurb *cnrb,
						  const struct face_g_snurb *snrb,
						  const fastf_t uvw1[3],
						  const fastf_t uvw2[3],
						  struct bu_list *head,
						  const struct bn_tol *tol);
NMG_EXPORT extern int nmg_cnurb_lseg_coincident(const struct edgeuse *eu1,
					       const struct edge_g_cnurb *cnrb,
					       const struct face_g_snurb *snrb,
					       const point_t pt1,
					       const point_t pt2,
					       const struct bn_tol *tol);
NMG_EXPORT extern int	nmg_cnurb_is_on_crv(const struct edgeuse *eu,
					    const struct edge_g_cnurb *cnrb,
					    const struct face_g_snurb *snrb,
					    const struct bu_list *head,
					    const struct bn_tol *tol);
NMG_EXPORT extern int nmg_edge_fuse(const uint32_t *magic_p,
				   const struct bn_tol *tol);
NMG_EXPORT extern int nmg_edge_g_fuse(const uint32_t *magic_p,
					   const struct bn_tol	*tol);
NMG_EXPORT extern int nmg_ck_fu_verts(struct faceuse *fu1,
				     struct face *f2,
				     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ck_fg_verts(struct faceuse *fu1,
				     struct face *f2,
				     const struct bn_tol *tol);
NMG_EXPORT extern int	nmg_two_face_fuse(struct face	*f1,
					  struct face *f2,
					  const struct bn_tol *tol);
NMG_EXPORT extern int nmg_shell_face_fuse(struct shell *s,
					 const struct bn_tol *tol);
NMG_EXPORT extern int nmg_break_all_es_on_v(uint32_t *magic_p,
					   struct vertex *v,
					   const struct bn_tol *tol);
NMG_EXPORT extern int nmg_break_e_on_v(const uint32_t *magic_p,
				      const struct bn_tol *tol);
/* DEPRECATED: use nmg_break_e_on_v */
NMG_EXPORT extern int nmg_shell_break_e_on_v(const uint32_t *magic_p,
					    const struct bn_tol *tol);
NMG_EXPORT extern int nmg_shell_fuse(struct shell *s,
				    const struct bn_tol *tol);

/* radial routines */
NMG_EXPORT extern void nmg_radial_sorted_list_insert(struct bu_list *hd,
						    struct nmg_radial *rad);
NMG_EXPORT extern void nmg_radial_verify_pointers(const struct bu_list *hd,
						 const struct bn_tol *tol);
NMG_EXPORT extern void nmg_radial_verify_monotone(const struct bu_list	*hd,
						 const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_insure_radial_list_is_increasing(struct bu_list	*hd,
							   fastf_t amin, fastf_t amax);
NMG_EXPORT extern void nmg_radial_build_list(struct bu_list		*hd,
					    struct bu_ptbl		*shell_tbl,
					    int			existing,
					    struct edgeuse		*eu,
					    const vect_t		xvec,
					    const vect_t		yvec,
					    const vect_t		zvec,
					    const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_radial_merge_lists(struct bu_list		*dest,
					     struct bu_list		*src,
					     const struct bn_tol	*tol);
NMG_EXPORT extern int	 nmg_is_crack_outie(const struct edgeuse	*eu,
					    const struct bn_tol	*tol);
NMG_EXPORT extern struct nmg_radial	*nmg_find_radial_eu(const struct bu_list *hd,
							    const struct edgeuse *eu);
NMG_EXPORT extern const struct edgeuse *nmg_find_next_use_of_2e_in_lu(const struct edgeuse	*eu,
								     const struct edge	*e1,
								     const struct edge	*e2);
NMG_EXPORT extern void nmg_radial_mark_cracks(struct bu_list	*hd,
					     const struct edge	*e1,
					     const struct edge	*e2,
					     const struct bn_tol	*tol);
NMG_EXPORT extern struct nmg_radial *nmg_radial_find_an_original(const struct bu_list	*hd,
								const struct shell	*s,
								const struct bn_tol	*tol);
NMG_EXPORT extern int nmg_radial_mark_flips(struct bu_list		*hd,
					   const struct shell	*s,
					   const struct bn_tol	*tol);
NMG_EXPORT extern int nmg_radial_check_parity(const struct bu_list	*hd,
					     const struct bu_ptbl	*shells,
					     const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_radial_implement_decisions(struct bu_list		*hd,
						     const struct bn_tol	*tol,
						     struct edgeuse		*eu1,
						     vect_t xvec,
						     vect_t yvec,
						     vect_t zvec);
NMG_EXPORT extern void nmg_pr_radial(const char *title,
				    const struct nmg_radial	*rad);
NMG_EXPORT extern void nmg_pr_radial_list(const struct bu_list *hd,
					 const struct bn_tol *tol);
NMG_EXPORT extern void nmg_do_radial_flips(struct bu_list *hd);
NMG_EXPORT extern void nmg_do_radial_join(struct bu_list *hd,
					 struct edgeuse *eu1ref,
					 vect_t xvec, vect_t yvec, vect_t zvec,
					 const struct bn_tol *tol);
NMG_EXPORT extern void nmg_radial_join_eu_NEW(struct edgeuse *eu1,
					     struct edgeuse *eu2,
					     const struct bn_tol *tol);
NMG_EXPORT extern void nmg_radial_exchange_marked(struct bu_list		*hd,
						 const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_s_radial_harmonize(struct shell		*s,
					     const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_s_radial_check(struct shell		*s,
					 const struct bn_tol	*tol);


NMG_EXPORT extern struct edge_g_lseg	*nmg_pick_best_edge_g(struct edgeuse *eu1,
							      struct edgeuse *eu2,
							      const struct bn_tol *tol);

/* nmg_visit.c */
NMG_EXPORT extern void nmg_visit_vertex(struct vertex			*v,
				       const struct nmg_visit_handlers	*htab,
				       void *			state);
NMG_EXPORT extern void nmg_visit_vertexuse(struct vertexuse		*vu,
					  const struct nmg_visit_handlers	*htab,
					  void *			state);
NMG_EXPORT extern void nmg_visit_edge(struct edge			*e,
				     const struct nmg_visit_handlers	*htab,
				     void *			state);
NMG_EXPORT extern void nmg_visit_edgeuse(struct edgeuse			*eu,
					const struct nmg_visit_handlers	*htab,
					void *			state);
NMG_EXPORT extern void nmg_visit_loop(struct loop			*l,
				     const struct nmg_visit_handlers	*htab,
				     void *			state);
NMG_EXPORT extern void nmg_visit_loopuse(struct loopuse			*lu,
					const struct nmg_visit_handlers	*htab,
					void *			state);
NMG_EXPORT extern void nmg_visit_face(struct face			*f,
				     const struct nmg_visit_handlers	*htab,
				     void *			state);
NMG_EXPORT extern void nmg_visit_faceuse(struct faceuse			*fu,
					const struct nmg_visit_handlers	*htab,
					void *			state);
NMG_EXPORT extern void nmg_visit_shell(struct shell			*s,
				      const struct nmg_visit_handlers	*htab,
				      void *			state);
NMG_EXPORT extern void nmg_visit(const uint32_t		*magicp,
				const struct nmg_visit_handlers	*htab,
				void *			state);

/* Tabulation routines (nmg_info.c) */
NMG_EXPORT extern void nmg_vertex_tabulate(struct bu_ptbl *tab,
					  const uint32_t *magic_p);
NMG_EXPORT extern void nmg_vertexuse_normal_tabulate(struct bu_ptbl *tab,
						    const uint32_t *magic_p);
NMG_EXPORT extern void nmg_edgeuse_tabulate(struct bu_ptbl *tab,
					   const uint32_t *magic_p);
NMG_EXPORT extern void nmg_edge_tabulate(struct bu_ptbl *tab,
					const uint32_t *magic_p);
NMG_EXPORT extern void nmg_edge_g_tabulate(struct bu_ptbl *tab,
					  const uint32_t *magic_p);
NMG_EXPORT extern void nmg_face_tabulate(struct bu_ptbl *tab,
					const uint32_t *magic_p);
NMG_EXPORT extern void nmg_edgeuse_with_eg_tabulate(struct bu_ptbl *tab,
						   const struct edge_g_lseg *eg);
NMG_EXPORT extern void nmg_edgeuse_on_line_tabulate(struct bu_ptbl *tab,
						   const uint32_t *magic_p,
						   const point_t pt,
						   const vect_t dir,
						   const struct bn_tol *tol);
NMG_EXPORT extern void nmg_e_and_v_tabulate(struct bu_ptbl *eutab,
					   struct bu_ptbl *vtab,
					   const uint32_t *magic_p);
NMG_EXPORT extern int nmg_2edgeuse_g_coincident(const struct edgeuse	*eu1,
					       const struct edgeuse	*eu2,
					       const struct bn_tol	*tol);

/* Extrude routines (nmg_extrude.c) */
NMG_EXPORT extern void nmg_translate_face(struct faceuse *fu, const vect_t Vec);
NMG_EXPORT extern int nmg_extrude_face(struct faceuse *fu, const vect_t Vec, const struct bn_tol *tol);
NMG_EXPORT extern struct vertexuse *nmg_find_vertex_in_lu(const struct vertex *v, const struct loopuse *lu);
NMG_EXPORT extern void nmg_fix_overlapping_loops(struct shell *s, const struct bn_tol *tol);
NMG_EXPORT extern void nmg_break_crossed_loops(struct shell *is, const struct bn_tol *tol);
NMG_EXPORT extern struct shell *nmg_extrude_cleanup(struct shell *is, const int is_void, const struct bn_tol *tol);
NMG_EXPORT extern void nmg_hollow_shell(struct shell *s, const fastf_t thick, const int approximate, const struct bn_tol *tol);
NMG_EXPORT extern struct shell *nmg_extrude_shell(struct shell *s, const fastf_t dist, const int normal_ward, const int approximate, const struct bn_tol *tol);

/* Print routines (nmg_pr.c) */
NMG_EXPORT extern char *nmg_orientation(int orientation);
NMG_EXPORT extern void nmg_pr_orient(int orientation,
				    const char *h);
NMG_EXPORT extern void nmg_pr_sa(const struct shell_a *sa,
				char *h);
NMG_EXPORT extern void nmg_pr_lg(const struct loop_g *lg,
				char *h);
NMG_EXPORT extern void nmg_pr_fg(const uint32_t *magic,
				char *h);
NMG_EXPORT extern void nmg_pr_s(const struct shell *s);
NMG_EXPORT extern void nmg_pr_s_briefly(const struct shell *s,
					char *h);
NMG_EXPORT extern void nmg_pr_f(const struct face *f,
			       char *h);
NMG_EXPORT extern void nmg_pr_fu(const struct faceuse *fu,
				char *h);
NMG_EXPORT extern void nmg_pr_fu_briefly(const struct faceuse *fu,
					char *h);
NMG_EXPORT extern void nmg_pr_l(const struct loop *l,
			       char *h);
NMG_EXPORT extern void nmg_pr_lu(const struct loopuse *lu,
				char *h);
NMG_EXPORT extern void nmg_pr_lu_briefly(const struct loopuse *lu,
					char *h);
NMG_EXPORT extern void nmg_pr_eg(const uint32_t *eg,
				char *h);
NMG_EXPORT extern void nmg_pr_e(const struct edge *e,
			       char *h);
NMG_EXPORT extern void nmg_pr_eu(const struct edgeuse *eu,
				char *h);
NMG_EXPORT extern void nmg_pr_eu_briefly(const struct edgeuse *eu,
					char *h);
NMG_EXPORT extern void nmg_pr_eu_endpoints(const struct edgeuse *eu,
					  char *h);
NMG_EXPORT extern void nmg_pr_vg(const struct vertex_g *vg,
				char *h);
NMG_EXPORT extern void nmg_pr_v(const struct vertex *v,
			       char *h);
NMG_EXPORT extern void nmg_pr_vu(const struct vertexuse *vu,
				char *h);
NMG_EXPORT extern void nmg_pr_vu_briefly(const struct vertexuse *vu,
					char *h);
NMG_EXPORT extern void nmg_pr_vua(const uint32_t *magic_p,
				 char *h);
NMG_EXPORT extern void nmg_euprint(const char *str,
				  const struct edgeuse *eu);
NMG_EXPORT extern void nmg_pr_ptbl(const char *title,
				  const struct bu_ptbl *tbl,
				  int verbose);
NMG_EXPORT extern void nmg_pr_ptbl_vert_list(const char *str,
					    const struct bu_ptbl *tbl,
					    const fastf_t *mag);
NMG_EXPORT extern void nmg_pr_one_eu_vecs(const struct edgeuse *eu,
					 const vect_t xvec,
					 const vect_t yvec,
					 const vect_t zvec,
					 const struct bn_tol *tol);
NMG_EXPORT extern void nmg_pr_fu_around_eu_vecs(const struct edgeuse *eu,
					       const vect_t xvec,
					       const vect_t yvec,
					       const vect_t zvec,
					       const struct bn_tol *tol);
NMG_EXPORT extern void nmg_pr_fu_around_eu(const struct edgeuse *eu,
					  const struct bn_tol *tol);
NMG_EXPORT extern void nmg_pl_lu_around_eu(const struct edgeuse *eu);
NMG_EXPORT extern void nmg_pr_fus_in_fg(const uint32_t *fg_magic);

/* Miscellaneous routines (nmg_misc.c) */
NMG_EXPORT extern int nmg_snurb_calc_lu_uv_orient(const struct loopuse *lu);
NMG_EXPORT extern void nmg_snurb_fu_eval(const struct faceuse *fu,
					const fastf_t u,
					const fastf_t v,
					point_t pt_on_srf);
NMG_EXPORT extern void nmg_snurb_fu_get_norm(const struct faceuse *fu,
					    const fastf_t u,
					    const fastf_t v,
					    vect_t norm);
NMG_EXPORT extern void nmg_snurb_fu_get_norm_at_vu(const struct faceuse *fu,
						  const struct vertexuse *vu,
						  vect_t norm);
NMG_EXPORT extern void nmg_find_zero_length_edges(const struct shell *s);
NMG_EXPORT extern struct face *nmg_find_top_face_in_dir(const struct shell *s,
						       int dir, long *flags);
NMG_EXPORT extern struct face *nmg_find_top_face(const struct shell *s,
						int *dir,
						long *flags);
NMG_EXPORT extern int nmg_mark_edges_real(const uint32_t *magic_p);
NMG_EXPORT extern void nmg_tabulate_face_g_verts(struct bu_ptbl *tab,
						const struct face_g_plane *fg);
NMG_EXPORT extern void nmg_isect_shell_self(struct shell *s,
					   const struct bn_tol *tol);
NMG_EXPORT extern struct edgeuse *nmg_next_radial_eu(const struct edgeuse *eu,
						    const struct shell *s,
						    const int wires);
NMG_EXPORT extern struct edgeuse *nmg_prev_radial_eu(const struct edgeuse *eu,
						    const struct shell *s,
						    const int wires);
NMG_EXPORT extern int nmg_radial_face_count(const struct edgeuse *eu,
					   const struct shell *s);
NMG_EXPORT extern int nmg_check_closed_shell(const struct shell *s,
					    const struct bn_tol *tol);
NMG_EXPORT extern int nmg_move_lu_between_fus(struct faceuse *dest,
					     struct faceuse *src,
					     struct loopuse *lu);
NMG_EXPORT extern void nmg_loop_plane_newell(const struct loopuse *lu,
					    plane_t pl);
NMG_EXPORT extern fastf_t nmg_loop_plane_area(const struct loopuse *lu,
					     plane_t pl);
NMG_EXPORT extern fastf_t nmg_loop_plane_area2(const struct loopuse *lu,
					      plane_t pl,
					      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_calc_face_plane(struct faceuse *fu_in,
					 plane_t pl);
NMG_EXPORT extern int nmg_calc_face_g(struct faceuse *fu);
NMG_EXPORT extern fastf_t nmg_faceuse_area(const struct faceuse *fu);
NMG_EXPORT extern fastf_t nmg_shell_area(const struct shell *s);
NMG_EXPORT extern void nmg_purge_unwanted_intersection_points(struct bu_ptbl *vert_list,
							     fastf_t *mag,
							     const struct faceuse *fu,
							     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_in_or_ref(struct vertexuse *vu,
				   struct bu_ptbl *b);
NMG_EXPORT extern void nmg_rebound(struct shell *s,
				  const struct bn_tol *tol);
NMG_EXPORT extern void nmg_count_shell_kids(const struct shell *s,
					   size_t *total_wires,
					   size_t *total_faces,
					   size_t *total_points);
NMG_EXPORT extern void nmg_close_shell(struct shell *s,
				      const struct bn_tol *tol);
NMG_EXPORT extern struct shell *nmg_dup_shell(struct shell *s,
					     long ***copy_tbl,
					     const struct bn_tol *tol);
NMG_EXPORT extern struct edgeuse *nmg_pop_eu(struct bu_ptbl *stack);
NMG_EXPORT extern void nmg_reverse_radials(struct faceuse *fu,
					  const struct bn_tol *tol);
NMG_EXPORT extern void nmg_reverse_face_and_radials(struct faceuse *fu,
						   const struct bn_tol *tol);
NMG_EXPORT extern int nmg_shell_is_void(const struct shell *s);
NMG_EXPORT extern void nmg_propagate_normals(struct faceuse *fu_in,
					    long *flags,
					    const struct bn_tol *tol);
NMG_EXPORT extern void nmg_connect_same_fu_orients(struct shell *s);
NMG_EXPORT extern void nmg_fix_decomposed_shell_normals(struct shell *s,
						       const struct bn_tol *tol);
NMG_EXPORT extern void nmg_fix_normals(struct shell *s_orig,
				      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_break_long_edges(struct shell *s,
					  const struct bn_tol *tol);
NMG_EXPORT extern struct faceuse *nmg_mk_new_face_from_loop(struct loopuse *lu);
NMG_EXPORT extern int nmg_split_loops_into_faces(uint32_t *magic_p,
						const struct bn_tol *tol);
NMG_EXPORT extern int nmg_decompose_shell(struct shell *s,
					 const struct bn_tol *tol);

NMG_EXPORT extern int nmg_unbreak_shell_edges(uint32_t *magic_p);
NMG_EXPORT extern void nmg_vlist_to_eu(struct bu_list *vlist,
				      struct shell *s);
NMG_EXPORT extern int nmg_find_isect_faces(const struct vertex *new_v,
					  struct bu_ptbl *faces,
					  int *free_edges,
					  const struct bn_tol *tol);
NMG_EXPORT extern int nmg_simple_vertex_solve(struct vertex *new_v,
					     const struct bu_ptbl *faces,
					     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ck_vert_on_fus(const struct vertex *v,
					const struct bn_tol *tol);
NMG_EXPORT extern void nmg_make_faces_at_vert(struct vertex *new_v,
					     struct bu_ptbl *int_faces,
					     const struct bn_tol *tol);
NMG_EXPORT extern void nmg_kill_cracks_at_vertex(const struct vertex *vp);
NMG_EXPORT extern int nmg_complex_vertex_solve(struct vertex *new_v,
					      const struct bu_ptbl *faces,
					      const int free_edges,
					      const int approximate,
					      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_bad_face_normals(const struct shell *s,
					  const struct bn_tol *tol);
NMG_EXPORT extern int nmg_faces_are_radial(const struct faceuse *fu1,
					  const struct faceuse *fu2);
NMG_EXPORT extern int nmg_move_edge_thru_pt(struct edgeuse *mv_eu,
					   const point_t pt,
					   const struct bn_tol *tol);
NMG_EXPORT extern void nmg_vlist_to_wire_edges(struct shell *s,
					      const struct bu_list *vhead);
NMG_EXPORT extern void nmg_follow_free_edges_to_vertex(const struct vertex *vpa,
						      const struct vertex *vpb,
						      struct bu_ptbl *bad_verts,
						      const struct shell *s,
						      const struct edgeuse *eu,
						      struct bu_ptbl *verts,
						      int *found);
NMG_EXPORT extern void nmg_glue_face_in_shell(const struct faceuse *fu,
					     struct shell *s,
					     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_open_shells_connect(struct shell *dst,
					     struct shell *src,
					     const long **copy_tbl,
					     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_in_vert(struct vertex *new_v,
				 const int approximate,
				 const struct bn_tol *tol);
NMG_EXPORT extern void nmg_mirror_shell(struct shell *s);
NMG_EXPORT extern int nmg_kill_cracks(struct shell *s);
NMG_EXPORT extern int nmg_kill_zero_length_edgeuses(struct shell *s);
NMG_EXPORT extern void nmg_make_faces_within_tol(struct shell *s,
						const struct bn_tol *tol);
NMG_EXPORT extern void nmg_intersect_loops_self(struct shell *s,
					       const struct bn_tol *tol);
NMG_EXPORT extern struct edge_g_cnurb *rt_join_cnurbs(struct bu_list *crv_head);
NMG_EXPORT extern struct edge_g_cnurb *rt_arc2d_to_cnurb(point_t i_center,
							point_t i_start,
							point_t i_end,
							int point_type,
							const struct bn_tol *tol);
NMG_EXPORT extern int nmg_break_edge_at_verts(struct edge *e,
					     struct bu_ptbl *verts,
					     const struct bn_tol *tol);
NMG_EXPORT extern fastf_t nmg_loop_plane_area(const struct loopuse *lu,
					     plane_t pl);
NMG_EXPORT extern int nmg_break_edges(uint32_t *magic_p,
				     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_lu_is_convex(struct loopuse *lu,
				      const struct bn_tol *tol);

NMG_EXPORT extern int nmg_simplify_shell_edges(struct shell *s,
					      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_edge_collapse(struct shell *s,
				       const struct bn_tol *tol,
				       const fastf_t tol_coll,
				       const fastf_t min_angle);

/* Triangulation routines (nmg_tri.c) */
NMG_EXPORT extern void nmg_triangulate_shell(struct shell *s,
					    const struct bn_tol  *tol);
NMG_EXPORT extern int nmg_triangulate_fu(struct faceuse *fu,
					const struct bn_tol *tol);
NMG_EXPORT extern void nmg_dump_shell(struct shell *s);

/* Copy routines (nmg_copy.c) */
NMG_EXPORT extern struct shell *nmg_clone_shell(const struct shell *original);

/* Manifold dimension access routines (nmg_manif.c) */
NMG_EXPORT extern int nmg_dangling_face(const struct faceuse *fu,
				       const char *manifolds);
NMG_EXPORT extern char *nmg_shell_manifolds(struct shell *sp,
					   char *tbl);
NMG_EXPORT extern char *nmg_manifolds(struct shell *s);

/* nmg.c */
NMG_EXPORT extern int nmg_ray_segs(struct ray_data *rd);

/* nmg_class.c */
NMG_EXPORT extern int nmg_classify_pt_loop(const point_t pt,
					  const struct loopuse *lu,
					  const struct bn_tol *tol);

NMG_EXPORT extern int nmg_classify_s_vs_s(struct shell *s,
					 struct shell *s2,
					 const struct bn_tol *tol);

NMG_EXPORT extern int nmg_classify_lu_lu(const struct loopuse *lu1,
					const struct loopuse *lu2,
					const struct bn_tol *tol);

NMG_EXPORT extern int nmg_class_pt_f(const point_t pt,
				    const struct faceuse *fu,
				    const struct bn_tol *tol);
NMG_EXPORT extern int nmg_class_pt_s(const point_t pt,
				    const struct shell *s,
				    const int in_or_out_only,
				    const struct bn_tol *tol);

/* From nmg_pt_fu.c */
NMG_EXPORT extern int nmg_eu_is_part_of_crack(const struct edgeuse *eu);

NMG_EXPORT extern int nmg_class_pt_lu_except(point_t		pt,
					    const struct loopuse	*lu,
					    const struct edge		*e_p,
					    const struct bn_tol	*tol);

NMG_EXPORT extern int nmg_class_pt_fu_except(const point_t pt,
					    const struct faceuse *fu,
					    const struct loopuse *ignore_lu,
					    void (*eu_func)(struct edgeuse *, point_t, const char *),
					    void (*vu_func)(struct vertexuse *, point_t, const char *),
					    const char *priv,
					    const int call_on_hits,
					    const int in_or_out_only,
					    const struct bn_tol *tol);

/* From nmg_plot.c */
NMG_EXPORT extern void nmg_pl_shell(FILE *fp,
				   const struct shell *s,
				   int fancy);

NMG_EXPORT extern void nmg_vu_to_vlist(struct bu_list *vhead,
				      const struct vertexuse	*vu);
NMG_EXPORT extern void nmg_eu_to_vlist(struct bu_list *vhead,
				      const struct bu_list	*eu_hd);
NMG_EXPORT extern void nmg_lu_to_vlist(struct bu_list *vhead,
				      const struct loopuse	*lu,
				      int			poly_markers,
				      const vectp_t		norm);
NMG_EXPORT extern void nmg_snurb_fu_to_vlist(struct bu_list		*vhead,
					    const struct faceuse	*fu,
					    int			poly_markers);
NMG_EXPORT extern void nmg_s_to_vlist(struct bu_list		*vhead,
				     const struct shell	*s,
				     int			poly_markers);
NMG_EXPORT extern void nmg_offset_eu_vert(point_t			base,
					 const struct edgeuse	*eu,
					 const vect_t		face_normal,
					 int			tip);
/* plot */
NMG_EXPORT extern void nmg_pl_v(FILE	*fp,
			       const struct vertex *v,
			       long *b);
NMG_EXPORT extern void nmg_pl_e(FILE *fp,
			       const struct edge *e,
			       long *b,
			       int red,
			       int green,
			       int blue);
NMG_EXPORT extern void nmg_pl_eu(FILE *fp,
				const struct edgeuse *eu,
				long *b,
				int red,
				int green,
				int blue);
NMG_EXPORT extern void nmg_pl_lu(FILE *fp,
				const struct loopuse *fu,
				long *b,
				int red,
				int green,
				int blue);
NMG_EXPORT extern void nmg_pl_fu(FILE *fp,
				const struct faceuse *fu,
				long *b,
				int red,
				int green,
				int blue);
NMG_EXPORT extern void nmg_pl_s(FILE *fp,
			       const struct shell *s);
NMG_EXPORT extern void nmg_vlblock_v(struct bn_vlblock *vbp,
				    const struct vertex *v,
				    long *tab);
NMG_EXPORT extern void nmg_vlblock_e(struct bn_vlblock *vbp,
				    const struct edge *e,
				    long *tab,
				    int red,
				    int green,
				    int blue);
NMG_EXPORT extern void nmg_vlblock_eu(struct bn_vlblock *vbp,
				     const struct edgeuse *eu,
				     long *tab,
				     int red,
				     int green,
				     int blue,
				     int fancy);
NMG_EXPORT extern void nmg_vlblock_euleft(struct bu_list			*vh,
					 const struct edgeuse		*eu,
					 const point_t			center,
					 const mat_t			mat,
					 const vect_t			xvec,
					 const vect_t			yvec,
					 double				len,
					 const struct bn_tol		*tol);
NMG_EXPORT extern void nmg_vlblock_around_eu(struct bn_vlblock		*vbp,
					    const struct edgeuse	*arg_eu,
					    long			*tab,
					    int			fancy,
					    const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_vlblock_lu(struct bn_vlblock *vbp,
				     const struct loopuse *lu,
				     long *tab,
				     int red,
				     int green,
				     int blue,
				     int fancy);
NMG_EXPORT extern void nmg_vlblock_fu(struct bn_vlblock *vbp,
				     const struct faceuse *fu,
				     long *tab, int fancy);
NMG_EXPORT extern void nmg_vlblock_s(struct bn_vlblock *vbp,
				    const struct shell *s,
				    int fancy);
/* visualization helper routines */
NMG_EXPORT extern void nmg_pl_edges_in_2_shells(struct bn_vlblock	*vbp,
					       long			*b,
					       const struct edgeuse	*eu,
					       int			fancy,
					       const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_pl_isect(const char		*filename,
				   const struct shell	*s,
				   const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_pl_comb_fu(int num1,
				     int num2,
				     const struct faceuse *fu1);
NMG_EXPORT extern void nmg_pl_2fu(const char *str,
				 const struct faceuse *fu1,
				 const struct faceuse *fu2,
				 int show_mates);
/* graphical display of classifier results */
NMG_EXPORT extern void nmg_show_broken_classifier_stuff(uint32_t	*p,
						       char	**classlist,
						       int	all_new,
						       int	fancy,
						       const char	*a_string);
NMG_EXPORT extern void nmg_face_plot(const struct faceuse *fu);
NMG_EXPORT extern void nmg_2face_plot(const struct faceuse *fu1,
				     const struct faceuse *fu2);
NMG_EXPORT extern void nmg_face_lu_plot(const struct loopuse *lu,
				       const struct vertexuse *vu1,
				       const struct vertexuse *vu2);
NMG_EXPORT extern void nmg_plot_lu_ray(const struct loopuse		*lu,
				      const struct vertexuse		*vu1,
				      const struct vertexuse		*vu2,
				      const vect_t			left);
NMG_EXPORT extern void nmg_plot_ray_face(const char *fname,
					point_t pt,
					const vect_t dir,
					const struct faceuse *fu);
NMG_EXPORT extern void nmg_plot_lu_around_eu(const char		*prefix,
					    const struct edgeuse	*eu,
					    const struct bn_tol	*tol);
NMG_EXPORT extern int nmg_snurb_to_vlist(struct bu_list		*vhead,
					const struct face_g_snurb	*fg,
					int			n_interior);
NMG_EXPORT extern void nmg_cnurb_to_vlist(struct bu_list *vhead,
					 const struct edgeuse *eu,
					 int n_interior,
					 int cmd);

/**
 * global nmg animation plot callback
 */
NMG_EXPORT extern void (*nmg_plot_anim_upcall)(void);

/**
 * global nmg animation vblock callback
 */
NMG_EXPORT extern void (*nmg_vlblock_anim_upcall)(void);

/**
 * global nmg mged display debug callback
 */
NMG_EXPORT extern void (*nmg_mged_debug_display_hack)(void);

/**
 * edge use distance tolerance
 */
NMG_EXPORT extern double nmg_eue_dist;

/* from nmg_mesh.c */
NMG_EXPORT extern int nmg_mesh_two_faces(struct faceuse *fu1,
					struct faceuse *fu2,
					const struct bn_tol	*tol);

NMG_EXPORT extern void nmg_radial_join_eu(struct edgeuse *eu1,
					 struct edgeuse *eu2,
					 const struct bn_tol *tol);
NMG_EXPORT extern void nmg_mesh_faces(struct faceuse *fu1,
				     struct faceuse *fu2,
				     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_mesh_face_shell(struct faceuse *fu1,
					 struct shell *s,
					 const struct bn_tol *tol);
NMG_EXPORT extern int nmg_mesh_shell_shell(struct shell *s1,
					  struct shell *s2,
					  const struct bn_tol *tol);

/**
 * radius of a FASTGEN cline element.
 *
 * shared with rt/do.c
 */
NMG_EXPORT extern fastf_t rt_cline_radius;

/* defined in bot.c */
/* TODO - these global variables need to be rolled in to the rt_i structure */
NMG_EXPORT extern size_t rt_bot_minpieces;
NMG_EXPORT extern size_t rt_bot_tri_per_piece;
NMG_EXPORT extern size_t rt_bot_mintie;

NMG_EXPORT extern int nmg_class_nothing_broken;

NMG_EXPORT extern void rt_prep_timer(void);
/* Read global timer, return time + str */
/**
 * Reports on the passage of time, since rt_prep_timer() was called.
 * Explicit return is number of CPU seconds.  String return is
 * descriptive.  If "elapsed" pointer is non-null, number of elapsed
 * seconds are returned.  Times returned will never be zero.
 */
NMG_EXPORT extern double rt_get_timer(struct bu_vls *vp,
				     double *elapsed);
/* Return CPU time, text, & wall clock time off the global timer */
/**
 * Compatibility routine
 */
NMG_EXPORT extern double rt_read_timer(char *str, int len);

/* vlist.c */
/** @addtogroup librt */
/** @{ */
/** @file librt/vlist.c
 *
 * Routines for the import and export of vlist chains as:
 * Network independent binary,
 * BRL-extended UNIX-Plot files.
 *
 */

/* FIXME: Has some stuff mixed in here that should go in LIBBN */
/************************************************************************
 *									*
 *			Generic VLBLOCK routines			*
 *									*
 ************************************************************************/

NMG_EXPORT extern struct bn_vlblock * bn_vlblock_init(struct bu_list *free_vlist_hd, /**< where to get/put free vlists */
						int max_ent /**< maximum number of entities to get/put */);

NMG_EXPORT extern struct bn_vlblock *rt_vlblock_init(void);

NMG_EXPORT extern void rt_vlblock_free(struct bn_vlblock *vbp);

NMG_EXPORT extern struct bu_list *rt_vlblock_find(struct bn_vlblock *vbp,
						 int r,
						 int g,
						 int b);

/************************************************************************
 *									*
 *			Generic BN_VLIST routines			*
 *									*
 ************************************************************************/

/**
 * Validate an bn_vlist chain for having reasonable values inside.
 * Calls bu_bomb() if not.
 *
 * Returns -
 * npts Number of point/command sets in use.
 */
NMG_EXPORT extern int rt_ck_vlist(const struct bu_list *vhead);


/**
 * Duplicate the contents of a vlist.  Note that the copy may be more
 * densely packed than the source.
 */
NMG_EXPORT extern void rt_vlist_copy(struct bu_list *dest,
				    const struct bu_list *src);


/**
 * The macro RT_FREE_VLIST() simply appends to the list
 * &RTG.rtg_vlfree.  Now, give those structures back to bu_free().
 */
NMG_EXPORT extern void bn_vlist_cleanup(struct bu_list *hd);


/**
 * XXX This needs to remain a LIBRT function.
 */
NMG_EXPORT extern void rt_vlist_cleanup(void);


/**
 * Given an RPP, draw the outline of it into the vlist.
 */
NMG_EXPORT extern void bn_vlist_rpp(struct bu_list *hd,
				   const point_t minn,
				   const point_t maxx);


/************************************************************************
 *									*
 *			Binary VLIST import/export routines		*
 *									*
 ************************************************************************/

/**
 * Convert a vlist chain into a blob of network-independent binary,
 * for transmission to another machine.
 *
 * The result is stored in a vls string, so that both the address and
 * length are available conveniently.
 */
NMG_EXPORT extern void rt_vlist_export(struct bu_vls *vls,
				      struct bu_list *hp,
				      const char *name);


/**
 * Convert a blob of network-independent binary prepared by
 * vls_vlist() and received from another machine, into a vlist chain.
 */
NMG_EXPORT extern void rt_vlist_import(struct bu_list *hp,
				      struct bu_vls *namevls,
				      const unsigned char *buf);


/************************************************************************
 *									*
 *			UNIX-Plot VLIST import/export routines		*
 *									*
 ************************************************************************/

/**
 * Output a bn_vlblock object in extended UNIX-plot format, including
 * color.
 */
NMG_EXPORT extern void rt_plot_vlblock(FILE *fp,
				      const struct bn_vlblock *vbp);


/**
 * Output a vlist as an extended 3-D floating point UNIX-Plot file.
 * You provide the file.  Uses libplot3 routines to create the
 * UNIX-Plot output.
 */
NMG_EXPORT extern void rt_vlist_to_uplot(FILE *fp,
					const struct bu_list *vhead);
NMG_EXPORT extern int rt_process_uplot_value(struct bu_list **vhead,
					    struct bn_vlblock *vbp,
					    FILE *fp,
					    int c,
					    double char_size,
					    int mode);


/**
 * Read a BRL-style 3-D UNIX-plot file into a vector list.  For now,
 * discard color information, only extract vectors.  This might be
 * more naturally located in mged/plot.c
 */
NMG_EXPORT extern int rt_uplot_to_vlist(struct bn_vlblock *vbp,
				       FILE *fp,
				       double char_size,
				       int mode);

__END_DECLS

#endif /* NMG_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
