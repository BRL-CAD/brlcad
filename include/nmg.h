/*                           N M G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @addtogroup libnmg
 *
 * Definition of data structures for "Non-Manifold Geometry
 * Modelling."  Developed from "Non-Manifold Geometric Boundary
 * Modeling" by Kevin Weiler, 5/7/87 (SIGGraph 1989 Course #20 Notes)
 *
 * See also "Topological Structures for Geometric Modeling"
 * by Kevin J. Weiler - RPI Phd thesis from 1986.
 *
 */
/** @{ */
/** @file include/nmg.h */

/*
 * TODO: since this original work, there have been a number of subsequent
 * papers published on ways of representing non-manifold geometry.  In
 * particular, the "partial entity" structure is purported to have essentially
 * the same representation power as Weiler's radial edge approach but with
 * more compact storage:
 *
 *  Sang Hun Lee and Kunwoo Lee, Partial Entity Structure: A Compact Boundary
 *  Representation for Non-Manifold Geometric Modeling, J. Comput. Inf. Sci.
 *  Eng 1(4), 356-365 (Nov 01, 2001)
 *
 *  Sang Hun Lee and Kunwoo Lee, Partial entity structure: a compact
 *  non-manifold boundary representation based on partial topological entities,
 *  SMA '01 Proceedings of the sixth ACM symposium on Solid modeling and
 *  applications Pages 159-170
 *
 * Also, from a design perspective, it would be nice if we could set this up so
 * that the more general non-manifold data structures include less general
 * containers and just extend them with the necessary extra information...
 * Don't know if that's possible, but it would be really nice from a data
 * conversion standpoint...
 *
 * TODO:  This paper may be worth a look from an API design perspective:
 * Topological Operators for Non-Manifold Modeling (1995) 
 * http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.50.1961
 *
 * also potentially useful:
 * https://www.cs.purdue.edu/homes/cmh/distribution/books/geo.html
 * https://cs.nyu.edu/faculty/yap/book/egc/
 *
 */

#ifndef NMG_H
#define NMG_H

#include "common.h"

/* system headers */
#include <stdio.h> /* for FILE */

/* interface headers */
#include "vmath.h"
#include "bu/list.h"
#include "bu/log.h"
#include "bu/magic.h"
#include "bu/ptbl.h"
#include "bg/plane.h"
#include "bn/tol.h"
#include "bv/vlist.h"
#include "vmath.h"


#ifndef NMG_EXPORT
#  if defined(NMG_DLL_EXPORTS) && defined(NMG_DLL_IMPORTS)
#    error "Only NMG_DLL_EXPORTS or NMG_DLL_IMPORTS can be defined, not both."
#  elif defined(NMG_DLL_EXPORTS)
#    define NMG_EXPORT COMPILER_DLLEXPORT
#  elif defined(NMG_DLL_IMPORTS)
#    define NMG_EXPORT COMPILER_DLLIMPORT
#  else
#    define NMG_EXPORT
#  endif
#endif


__BEGIN_DECLS

#include "nmg/debug.h"

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
#define NMG_TEST_EDGEUSE(_p) do { \
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
    } } while (0)


/**
 * @brief  debug bits for NMG's see nmg.h
 */
NMG_EXPORT extern uint32_t nmg_debug;

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


struct model {
    uint32_t magic;
    struct bu_list r_hd;	/**< @brief list of regions */
    char *manifolds;            /**< @brief structure 1-3manifold table */
    long index;			/**< @brief struct # in this model */
    long maxindex;		/**< @brief # of structs so far */
};

struct nmgregion {
    struct bu_list l;		/**< @brief regions, in model's r_hd list */
    struct model *m_p;		/**< @brief owning model */
    struct nmgregion_a *ra_p;	/**< @brief attributes */
    struct bu_list s_hd;	/**< @brief list of shells in region */
    long index;			/**< @brief struct # in this model */
};

struct nmgregion_a {
    uint32_t magic;
    point_t min_pt;		/**< @brief minimums of bounding box */
    point_t max_pt;		/**< @brief maximums of bounding box */
    long index;			/**< @brief struct # in this model */
};

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
    struct bu_list l;		/**< @brief shells, in region's s_hd list */
    struct nmgregion *r_p;	/**< @brief owning region */
    struct shell_a *sa_p;	/**< @brief attribs */

    struct bu_list fu_hd;	/**< @brief list of face uses in shell */
    struct bu_list lu_hd;	/**< @brief wire loopuses (edge groups) */
    struct bu_list eu_hd;	/**< @brief wire list (shell has wires) */
    struct vertexuse *vu_p;	/**< @brief internal ptr to single vertexuse */
    long index;			/**< @brief struct # in this model */
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
 * storage allocation/deallocation support
 */

NMG_EXPORT extern int nmg_memtrack;
NMG_EXPORT void *nmg_malloc(size_t s, const char *msg);
NMG_EXPORT void *nmg_calloc(int cnt, size_t s, const char *msg);
NMG_EXPORT void *nmg_realloc(void *ptr, size_t s, const char *msg);
NMG_EXPORT void nmg_free(void *, const char *str);
NMG_EXPORT void nmg_destroy();
#define NMG_GETSTRUCT(p, str) p = (struct str *)nmg_calloc(1, sizeof(struct str), "NMG_GETSTRUCT")
#define NMG_FREESTRUCT(p, str) nmg_free(p, "NMG_FREESTRUCT")
#define NMG_ALLOC(_ptr, _type) _ptr = (_type *)nmg_calloc(1, sizeof(_type), #_type " (NMG_ALLOC) " CPP_FILELINE)


/*
 * Macros to create and destroy storage for the NMG data structures.
 * Since nmg_mk.c and g_nmg.c are the only source file which should
 * perform these most fundamental operations, the macros do not belong
 * in nmg.h In particular, application code should NEVER do these
 * things.  Any need to do so should be handled by extending nmg_mk.c
 */
#define NMG_INCR_INDEX(_p, _m)	\
    NMG_CK_MODEL(_m); (_p)->index = ((_m)->maxindex)++


#define GET_MODEL(p, m)             {NMG_GETSTRUCT(p, model); NMG_INCR_INDEX(p, m);}
#define GET_REGION(p, m)            {NMG_GETSTRUCT(p, nmgregion); NMG_INCR_INDEX(p, m);}
#define GET_REGION_A(p, m)          {NMG_GETSTRUCT(p, nmgregion_a); NMG_INCR_INDEX(p, m);}
#define GET_SHELL(p, m)             {NMG_GETSTRUCT(p, shell); NMG_INCR_INDEX(p, m);}
#define GET_SHELL_A(p, m)           {NMG_GETSTRUCT(p, shell_a); NMG_INCR_INDEX(p, m);}
#define GET_FACE(p, m)              {NMG_GETSTRUCT(p, face); NMG_INCR_INDEX(p, m);}
#define GET_FACE_G_PLANE(p, m)      {NMG_GETSTRUCT(p, face_g_plane); NMG_INCR_INDEX(p, m);}
#define GET_FACE_G_SNURB(p, m)      {NMG_GETSTRUCT(p, face_g_snurb); NMG_INCR_INDEX(p, m);}
#define GET_FACEUSE(p, m)           {NMG_GETSTRUCT(p, faceuse); NMG_INCR_INDEX(p, m);}
#define GET_LOOP(p, m)              {NMG_GETSTRUCT(p, loop); NMG_INCR_INDEX(p, m);}
#define GET_LOOP_G(p, m)            {NMG_GETSTRUCT(p, loop_g); NMG_INCR_INDEX(p, m);}
#define GET_LOOPUSE(p, m)           {NMG_GETSTRUCT(p, loopuse); NMG_INCR_INDEX(p, m);}
#define GET_LOOPUSE_A(p, m)         {NMG_GETSTRUCT(p, loopuse_a); NMG_INCR_INDEX(p, m);}
#define GET_EDGE(p, m)              {NMG_GETSTRUCT(p, edge); NMG_INCR_INDEX(p, m);}
#define GET_EDGE_G_LSEG(p, m)       {NMG_GETSTRUCT(p, edge_g_lseg); NMG_INCR_INDEX(p, m);}
#define GET_EDGE_G_CNURB(p, m)      {NMG_GETSTRUCT(p, edge_g_cnurb); NMG_INCR_INDEX(p, m);}
#define GET_EDGEUSE(p, m)           {NMG_GETSTRUCT(p, edgeuse); NMG_INCR_INDEX(p, m);}
#define GET_VERTEX(p, m)            {NMG_GETSTRUCT(p, vertex); NMG_INCR_INDEX(p, m);}
#define GET_VERTEX_G(p, m)          {NMG_GETSTRUCT(p, vertex_g); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE(p, m)         {NMG_GETSTRUCT(p, vertexuse); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE_A_PLANE(p, m) {NMG_GETSTRUCT(p, vertexuse_a_plane); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE_A_CNURB(p, m) {NMG_GETSTRUCT(p, vertexuse_a_cnurb); NMG_INCR_INDEX(p, m);}


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
    void (*aft_faceuse)(uint32_t *, void *, int, struct bu_list *);

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

struct nmg_radial {
    struct bu_list      l;
    struct edgeuse      *eu;
    struct faceuse      *fu;            /**< @brief  Derived from eu */
    struct shell        *s;             /**< @brief  Derived from eu */
    int                 existing_flag;  /**< @brief  !0 if this eu exists on dest edge */
    int                 is_crack;       /**< @brief  This eu is part of a crack. */
    int                 is_outie;       /**< @brief  This crack is an "outie" */
    int                 needs_flip;     /**< @brief  Insert eumate, not eu */
    fastf_t             ang;            /**< @brief  angle, in radians.  0 to 2pi */
};
#define NMG_CK_RADIAL(_p) NMG_CKMAG(_p, NMG_RADIAL_MAGIC, "nmg_radial")

struct nmg_inter_struct {
    uint32_t            magic;
    struct bu_ptbl      *l1;            /**< @brief  vertexuses on the line of */
    struct bu_ptbl      *l2;            /**< @brief  intersection between planes */
    fastf_t             *mag1;          /**< @brief  Distances along intersection line */
    fastf_t             *mag2;          /**< @brief  for each vertexuse in l1 and l2. */
    size_t              mag_len;        /**< @brief  Array size of mag1 and mag2 */
    struct shell        *s1;
    struct shell        *s2;
    struct faceuse      *fu1;           /**< @brief  null if l1 comes from a wire */
    struct faceuse      *fu2;           /**< @brief  null if l2 comes from a wire */
    struct bn_tol       tol;
    int                 coplanar;       /**< @brief  a flag */
    struct edge_g_lseg  *on_eg;         /**< @brief  edge_g for line of intersection */
    point_t             pt;             /**< @brief  3D line of intersection */
    vect_t              dir;
    point_t             pt2d;           /**< @brief  2D projection of isect line */
    vect_t              dir2d;
    fastf_t             *vert2d;        /**< @brief  Array of 2d vertex projections [index] */
    size_t              maxindex;       /**< @brief  size of vert2d[] */
    mat_t               proj;           /**< @brief  Matrix to project onto XY plane */
    const uint32_t      *twod;          /**< @brief  ptr to face/edge of 2d projection */
};
#define NMG_CK_INTER_STRUCT(_p) NMG_CKMAG(_p, NMG_INTER_STRUCT_MAGIC, "nmg_inter_struct")



/*NURBS*/

#define RT_NURB_SPLIT_ROW 0
#define RT_NURB_SPLIT_COL 1
#define RT_NURB_SPLIT_FLAT 2

/* Definition of NURB point types and coordinates
 * Bit:	  8765 4321 0
 *       |nnnn|tttt|h
 *			h     - 1 if Homogeneous
 *			tttt  - point type
 *				1 = XY
 *				2 = XYZ
 *				3 = UV
 *				4 = Random data
 *				5 = Projected surface
 *			nnnnn - number of coordinates per point
 *				includes the rational coordinate
 */

/* point types */
#define RT_NURB_PT_XY 	1			/**< @brief x, y coordinates */
#define RT_NURB_PT_XYZ	2			/**< @brief x, y, z coordinates */
#define RT_NURB_PT_UV	3			/**< @brief trim u, v parameter space */
#define RT_NURB_PT_DATA 4			/**< @brief random data */
#define RT_NURB_PT_PROJ	5			/**< @brief Projected Surface */

#define RT_NURB_PT_RATIONAL	1
#define RT_NURB_PT_NONRAT 	0

#define RT_NURB_MAKE_PT_TYPE(n, t, h)	((n<<5) | (t<<1) | h)
#define RT_NURB_EXTRACT_COORDS(pt)	(pt>>5)
#define RT_NURB_EXTRACT_PT_TYPE(pt)		((pt>>1) & 0x0f)
#define RT_NURB_IS_PT_RATIONAL(pt)		(pt & 0x1)
#define RT_NURB_STRIDE(pt)		(RT_NURB_EXTRACT_COORDS(pt) * sizeof( fastf_t))

#define DEBUG_NMG_SPLINE	0x00000100	/**< @brief 9 Splines */

/* macros to check/validate a structure pointer
 */
#define NMG_CK_KNOT(_p)		BU_CKMAG(_p, RT_KNOT_VECTOR_MAGIC, "knot_vector")
#define NMG_CK_CNURB(_p)	BU_CKMAG(_p, NMG_EDGE_G_CNURB_MAGIC, "cnurb")
#define NMG_CK_SNURB(_p)	BU_CKMAG(_p, NMG_FACE_G_SNURB_MAGIC, "snurb")

/* DEPRECATED */
#define GET_CNURB(p/*, m*/) { \
	BU_ALLOC((p), struct edge_g_cnurb); \
	/* NMG_INCR_INDEX(p, m); */ \
	BU_LIST_INIT( &(p)->l ); (p)->l.magic = NMG_EDGE_G_CNURB_MAGIC; \
    }

/* DEPRECATED */
#define GET_SNURB(p/*, m*/) { \
	BU_ALLOC((p), struct face_g_snurb); \
	/* NMG_INCR_INDEX(p, m); */ \
	BU_LIST_INIT( &(p)->l ); (p)->l.magic = NMG_FACE_G_SNURB_MAGIC; \
    }


/* ----- Internal structures ----- */

struct nmg_nurb_poly {
    struct nmg_nurb_poly * next;
    point_t		ply[3];		/**< @brief Vertices */
    fastf_t		uv[3][2];	/**< @brief U, V parametric values */
};

struct nmg_nurb_uv_hit {
    struct nmg_nurb_uv_hit * next;
    int		sub;
    fastf_t		u;
    fastf_t		v;
};


struct oslo_mat {
    struct oslo_mat * next;
    int		offset;
    int		osize;
    fastf_t		* o_vec;
};

/* --- new way */

struct bezier_2d_list {
    struct bu_list	l;
    point2d_t	*ctl;
};

/**
 *  * Evaluate a Bezier curve at a particular parameter value. Fill in
 *   * control points for resulting sub-curves if "Left" and "Right" are
 *    * non-null.
 *     */
NMG_EXPORT extern void bezier(point2d_t *V, int degree, double t, point2d_t *Left, point2d_t *Right, point2d_t eval_pt, point2d_t normal );

/**
 *  * Given an equation in Bernstein-Bezier form, find all of the roots
 *   * in the interval [0, 1].  Return the number of roots found.
 *    */
NMG_EXPORT extern int bezier_roots(point2d_t *w, int degree, point2d_t **intercept, point2d_t **normal, point2d_t ray_start, point2d_t ray_dir, point2d_t ray_perp, int depth, fastf_t epsilon);

/**
 *  * subdivide a 2D bezier curve at t=0.5
 *   */
NMG_EXPORT extern struct bezier_2d_list *bezier_subdivide(struct bezier_2d_list *bezier_hd, int degree, fastf_t epsilon, int depth);


/* TODO - these structs and ray_in_rpp are versions of librt functionality,
 * and we need to think about how/where to merge them into a common function
 * and struct that are available to both libraries without introducing a
 * coupling dependency. */

NMG_EXPORT extern struct bu_list re_nmgfree;     /**< @brief  head of NMG hitmiss freelist */

#define NMG_HIT_LIST    0
#define NMG_MISS_LIST   1

/* These values are for the hitmiss "in_out" variable and indicate the
 * nature of the hit when known
 */
#define HMG_INBOUND_STATE(_hm) (((_hm)->in_out & 0x0f0) >> 4)
#define HMG_OUTBOUND_STATE(_hm) ((_hm)->in_out & 0x0f)


#define NMG_RAY_STATE_INSIDE    1
#define NMG_RAY_STATE_ON        2
#define NMG_RAY_STATE_OUTSIDE   4
#define NMG_RAY_STATE_ANY       8

#define HMG_HIT_IN_IN   0x11    /**< @brief  hit internal structure */
#define HMG_HIT_IN_OUT  0x14    /**< @brief  breaking out */
#define HMG_HIT_OUT_IN  0x41    /**< @brief  breaking in */
#define HMG_HIT_OUT_OUT 0x44    /**< @brief  edge/vertex graze */
#define HMG_HIT_IN_ON   0x12
#define HMG_HIT_ON_IN   0x21
#define HMG_HIT_ON_ON   0x22
#define HMG_HIT_OUT_ON  0x42
#define HMG_HIT_ON_OUT  0x24
#define HMG_HIT_ANY_ANY 0x88    /**< @brief  hit on non-3-manifold */

#define NMG_VERT_ENTER 1
#define NMG_VERT_ENTER_LEAVE 0
#define NMG_VERT_LEAVE -1
#define NMG_VERT_UNKNOWN -2

#define NMG_HITMISS_SEG_IN 0x696e00     /**< @brief  "in" */
#define NMG_HITMISS_SEG_OUT 0x6f757400  /**< @brief  "out" */

#define NMG_CK_RD(_rd) NMG_CKMAG(_rd, NMG_RAY_DATA_MAGIC, "ray data");

#ifdef NO_BOMBING_MACROS
#  define NMG_CK_HITMISS(hm) (void)(hm)
#else
#  define NMG_CK_HITMISS(hm) \
    {\
	switch (hm->l.magic) { \
	    case NMG_RT_HIT_MAGIC: \
	    case NMG_RT_HIT_SUB_MAGIC: \
	    case NMG_RT_MISS_MAGIC: \
		break; \
	    case NMG_MISS_LIST: \
		bu_log(CPP_FILELINE ": struct hitmiss has NMG_MISS_LIST magic #\n"); \
		bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
		break; \
	    case NMG_HIT_LIST: \
		bu_log(CPP_FILELINE ": struct hitmiss has NMG_MISS_LIST magic #\n"); \
		bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
		break; \
	    default: \
		bu_log(CPP_FILELINE ": bad struct hitmiss magic: %u:(0x%08x)\n", \
		       hm->l.magic, hm->l.magic); \
		bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	}\
	if (!hm->hit.hit_private) { \
	    bu_log(CPP_FILELINE ": NULL hit_private in hitmiss struct\n"); \
	    bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	} \
    }
#endif

#ifdef NO_BOMBING_MACROS
#  define NMG_CK_HITMISS_LISTS(rd) (void)(rd)
#else
#  define NMG_CK_HITMISS_LISTS(rd) \
    { \
	struct nmg_hitmiss *_a_hit; \
	for (BU_LIST_FOR(_a_hit, nmg_hitmiss, &rd->rd_hit)) {NMG_CK_HITMISS(_a_hit);} \
	for (BU_LIST_FOR(_a_hit, nmg_hitmiss, &rd->rd_miss)) {NMG_CK_HITMISS(_a_hit);} \
    }
#endif


#define NMG_GET_HITMISS(_p) { \
	(_p) = BU_LIST_FIRST(nmg_hitmiss, &(re_nmgfree)); \
	if (BU_LIST_IS_HEAD((_p), &(re_nmgfree))) \
	    BU_ALLOC((_p), struct nmg_hitmiss); \
	else \
	    BU_LIST_DEQUEUE(&((_p)->l)); \
    }


#define NMG_FREE_HITLIST(_p) { \
	BU_CK_LIST_HEAD((_p)); \
	BU_LIST_APPEND_LIST(&(re_nmgfree), (_p)); \
    }

#ifdef NO_BOMBING_MACROS
#  define nmg_bu_bomb(rd, vlfree, str) (void)(rd)
#else
#  define nmg_bu_bomb(rd, vlfree, str) { \
	bu_log("%s", str); \
	if (nmg_debug & NMG_DEBUG_NMGRT) bu_bomb("End of diagnostics"); \
	BU_LIST_INIT(&rd->rd_hit); \
	BU_LIST_INIT(&rd->rd_miss); \
	nmg_debug |= NMG_DEBUG_NMGRT; \
	nmg_isect_ray_model(rd,vlfree); \
	bu_bomb("Should have bombed before this\n"); \
    }
#endif


#define HIT 1   /**< @brief  a hit on a face */
#define MISS 0  /**< @brief  a miss on the face */


struct nmg_ray {
    uint32_t		magic;
    point_t             r_pt;           /**< @brief Point at which ray starts */
    vect_t              r_dir;          /**< @brief Direction of ray (UNIT Length) */
    fastf_t             r_min;          /**< @brief entry dist to bounding sphere */
    fastf_t             r_max;          /**< @brief exit dist from bounding sphere */
};

struct nmg_hit {
    uint32_t		hit_magic;
    fastf_t             hit_dist;       /**< @brief dist from r_pt to hit_point */
    point_t             hit_point;      /**< @brief DEPRECATED: Intersection point, use VJOIN1 hit_dist */
    vect_t              hit_normal;     /**< @brief DEPRECATED: Surface Normal at hit_point, use RT_HIT_NORMAL */
    vect_t              hit_vpriv;      /**< @brief PRIVATE vector for xxx_*() */
    void *              hit_private;    /**< @brief PRIVATE handle for xxx_shot() */ 
    int                 hit_surfno;     /**< @brief solid-specific surface indicator */
    struct nmg_ray *    hit_rayp;       /**< @brief pointer to defining ray */
};

struct nmg_seg {
    struct bu_list      l;
    struct nmg_hit      seg_in;         /**< @brief IN information */
    struct nmg_hit      seg_out;        /**< @brief OUT information */
    void *		seg_stp;	/**< @brief pointer back to soltab */
};

struct nmg_hitmiss {
    struct bu_list      l;
    struct nmg_hit      hit;
    fastf_t             dist_in_plane;  /**< @brief  distance from plane intersect */
    int                 in_out;         /**< @brief  status of ray as it transitions
					 * this hit point.
					 */
    long                *inbound_use;
    vect_t              inbound_norm;
    long                *outbound_use;
    vect_t              outbound_norm;
    int                 start_stop;     /**< @brief  is this a seg_in or seg_out */
    struct nmg_hitmiss  *other;         /**< @brief  for keeping track of the other
					 * end of the segment when we know
					 * it
					 */
};

/**
 * Ray Data structure
 *
 * A) the hitmiss table has one element for each nmg structure in the
 * nmgmodel.  The table keeps track of which elements have been
 * processed before and which haven't.  Elements in this table will
 * either be: (NULL) item not previously processed hitmiss ptr item
 * previously processed
 *
 * the 0th item in the array is a pointer to the head of the "hit"
 * list.  The 1th item in the array is a pointer to the head of the
 * "miss" list.
 *
 * B) If plane_pt is non-null then we are currently processing a face
 * intersection.  The plane_dist and ray_dist_to_plane are valid.  The
 * ray/edge intersector should check the distance from the plane
 * intercept to the edge and update "plane_closest" if the current
 * edge is closer to the intercept than the previous closest object.
 */
struct nmg_ray_data {
    uint32_t            magic;
    struct model        *rd_m;
    char                *manifolds; /**< @brief   structure 1-3manifold table */
    vect_t              rd_invdir;
    struct nmg_ray      *rp;
    void *		*ap;
    struct nmg_seg      *seghead;
    void *		*stp;
    const struct bn_tol *tol;
    struct nmg_hitmiss  **hitmiss;      /**< @brief  1 struct hitmiss ptr per elem. */
    struct bu_list      rd_hit;         /**< @brief  list of hit elements */
    struct bu_list      rd_miss;        /**< @brief  list of missed/sub-hit elements */

/* The following are to support isect_ray_face() */

    /**
     * plane_pt is the intercept point of the ray with the plane of
     * the face.
     */
    point_t             plane_pt;       /**< @brief  ray/plane(face) intercept point */

    /**
     * ray_dist_to_plane is the parametric distance along the ray from
     * the ray origin (rd->rp->r_pt) to the ray/plane intercept point
     */
    fastf_t             ray_dist_to_plane; /**< @brief  ray parametric dist to plane */

    /**
     * the "face_subhit" element is a boolean used by isect_ray_face
     * and [e|v]u_touch_func to record the fact that the
     * ray/(plane/face) intercept point was within tolerance of an
     * edge/vertex of the face.  In such instances, isect_ray_face
     * does NOT need to generate a hit point for the face, as the hit
     * point for the edge/vertex will suffice.
     */
    int                 face_subhit;

    /**
     * the "classifying_ray" flag indicates that this ray is being
     * used to classify a point, so that the "eu_touch" and "vu_touch"
     * functions should not be called.
     */
    int                 classifying_ray;
};

int
ray_in_rpp(struct nmg_ray *rp,
	   const fastf_t *invdir,       /* inverses of rp->r_dir[] */
	   const fastf_t *min,
	   const fastf_t *max);

/**
 * global nmg animation vblock callback
 */
NMG_EXPORT extern void (*nmg_vlblock_anim_upcall)(void);

/**
 * global nmg mged display debug callback (ew)
 */
NMG_EXPORT extern void (*nmg_mged_debug_display_hack)(void);




/* From file nmg_mk.c */
/*      MAKE routines */
NMG_EXPORT extern struct model *nmg_mm(void);
NMG_EXPORT extern struct model *nmg_mmr(void);
NMG_EXPORT extern struct nmgregion *nmg_mrsv(struct model *m);
NMG_EXPORT extern struct shell *nmg_msv(struct nmgregion *r_p);
NMG_EXPORT extern struct faceuse *nmg_mf(struct loopuse *lu1);
NMG_EXPORT extern struct loopuse *nmg_mlv(uint32_t *magic,
					  struct vertex *v,
					  int orientation);
NMG_EXPORT extern struct edgeuse *nmg_me(struct vertex *v1,
					 struct vertex *v2,
					 struct shell *s);
NMG_EXPORT extern struct edgeuse *nmg_meonvu(struct vertexuse *vu);
NMG_EXPORT extern struct loopuse *nmg_ml(struct shell *s);
/*      KILL routines */
NMG_EXPORT extern int nmg_keg(struct edgeuse *eu);
NMG_EXPORT extern int nmg_kvu(struct vertexuse *vu);
NMG_EXPORT extern int nmg_kfu(struct faceuse *fu1);
NMG_EXPORT extern int nmg_klu(struct loopuse *lu1);
NMG_EXPORT extern int nmg_keu(struct edgeuse *eu);
NMG_EXPORT extern int nmg_keu_zl(struct shell *s,
				 const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ks(struct shell *s);
NMG_EXPORT extern int nmg_kr(struct nmgregion *r);
NMG_EXPORT extern void nmg_km(struct model *m);
/*      Geometry and Attribute routines */
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
NMG_EXPORT extern void nmg_region_a(struct nmgregion *r,
				    const struct bn_tol *tol);
/*      DEMOTE routines */
NMG_EXPORT extern int nmg_demote_lu(struct loopuse *lu);
NMG_EXPORT extern int nmg_demote_eu(struct edgeuse *eu);
/*      MODIFY routines */
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

/* From nmg_mod.c */
/*      REGION Routines */
NMG_EXPORT extern void nmg_merge_regions(struct nmgregion *r1,
					 struct nmgregion *r2,
					 const struct bn_tol *tol);

/*      SHELL Routines */
NMG_EXPORT extern void nmg_shell_coplanar_face_merge(struct shell *s,
						     const struct bn_tol *tol,
						     const int simplify,
						     struct bu_list *vlfree);
NMG_EXPORT extern int nmg_simplify_shell(struct shell *s, struct bu_list *vlfree);
NMG_EXPORT extern void nmg_rm_redundancies(struct shell *s,
					   struct bu_list *vlfree,
					   const struct bn_tol *tol);
NMG_EXPORT extern void nmg_sanitize_s_lv(struct shell *s,
					 int orient);
NMG_EXPORT extern void nmg_s_split_touchingloops(struct shell *s,
						 const struct bn_tol *tol);
NMG_EXPORT extern void nmg_s_join_touchingloops(struct shell             *s,
						const struct bn_tol      *tol);
NMG_EXPORT extern void nmg_js(struct shell       *s1,
			      struct shell       *s2,
			      struct bu_list *vlfree,
			      const struct bn_tol        *tol);
NMG_EXPORT extern void nmg_invert_shell(struct shell             *s);

/*      FACE Routines */
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
				     struct bu_list *vlfree,
				     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_simplify_face(struct faceuse *fu, struct bu_list *vlfree);
NMG_EXPORT extern void nmg_reverse_face(struct faceuse *fu);
NMG_EXPORT extern void nmg_mv_fu_between_shells(struct shell *dest,
						struct shell *src,
						struct faceuse *fu);
NMG_EXPORT extern void nmg_jf(struct faceuse *dest_fu,
			      struct faceuse *src_fu);
NMG_EXPORT extern struct faceuse *nmg_dup_face(struct faceuse *fu,
					       struct shell *s);
/*      LOOP Routines */
NMG_EXPORT extern void nmg_jl(struct loopuse *lu,
			      struct edgeuse *eu);
NMG_EXPORT extern struct vertexuse *nmg_join_2loops(struct vertexuse *vu1,
						    struct vertexuse *vu2);
NMG_EXPORT extern struct vertexuse *nmg_join_singvu_loop(struct vertexuse *vu1,
							 struct vertexuse *vu2);
NMG_EXPORT extern struct vertexuse *nmg_join_2singvu_loops(struct vertexuse *vu1,
							   struct vertexuse *vu2);
NMG_EXPORT extern struct loopuse *nmg_cut_loop(struct vertexuse *vu1,
					       struct vertexuse *vu2,
					       struct bu_list *vlfree);
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
NMG_EXPORT extern int nmg_loop_split_at_touching_jaunt(struct loopuse            *lu,
						       const struct bn_tol       *tol);
NMG_EXPORT extern void nmg_simplify_loop(struct loopuse *lu, struct bu_list *vlfree);
NMG_EXPORT extern int nmg_kill_snakes(struct loopuse *lu, struct bu_list *vlfree);
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
/*      EDGE Routines */
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
/*      VERTEX Routines */
NMG_EXPORT extern void nmg_mv_vu_between_shells(struct shell *dest,
						struct shell *src,
						struct vertexuse *vu);

/* From nmg_info.c */
/* Model routines */
NMG_EXPORT extern struct model *nmg_find_model(const uint32_t *magic_p);
NMG_EXPORT extern struct shell *nmg_find_shell(const uint32_t *magic_p);
NMG_EXPORT extern void nmg_model_bb(point_t min_pt,
				    point_t max_pt,
				    const struct model *m);


/* Shell routines */
NMG_EXPORT extern int nmg_shell_is_empty(const struct shell *s);
NMG_EXPORT extern struct shell *nmg_find_s_of_lu(const struct loopuse *lu);
NMG_EXPORT extern struct shell *nmg_find_s_of_eu(const struct edgeuse *eu);
NMG_EXPORT extern struct shell *nmg_find_s_of_vu(const struct vertexuse *vu);

/* Face routines */
NMG_EXPORT extern struct faceuse *nmg_find_fu_of_eu(const struct edgeuse *eu);
NMG_EXPORT extern struct faceuse *nmg_find_fu_of_lu(const struct loopuse *lu);
NMG_EXPORT extern struct faceuse *nmg_find_fu_of_vu(const struct vertexuse *vu);
NMG_EXPORT extern struct faceuse *nmg_find_fu_with_fg_in_s(const struct shell *s1,
							   const struct faceuse *fu2);
NMG_EXPORT extern double nmg_measure_fu_angle(const struct edgeuse *eu,
					      const vect_t xvec,
					      const vect_t yvec,
					      const vect_t zvec);

/* Loop routines */
NMG_EXPORT extern struct loopuse*nmg_find_lu_of_vu(const struct vertexuse *vu);
NMG_EXPORT extern int nmg_loop_is_a_crack(const struct loopuse *lu);
NMG_EXPORT extern int    nmg_loop_is_ccw(const struct loopuse *lu,
					 const vect_t norm,
					 const struct bn_tol *tol);
NMG_EXPORT extern const struct vertexuse *nmg_loop_touches_self(const struct loopuse *lu);
NMG_EXPORT extern int nmg_2lu_identical(const struct edgeuse *eu1,
					const struct edgeuse *eu2);

/* Edge routines */
NMG_EXPORT extern struct edgeuse *nmg_find_matching_eu_in_s(const struct edgeuse *eu1,
							    const struct shell   *s2);
NMG_EXPORT extern struct edgeuse *nmg_findeu(const struct vertex *v1,
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
								  struct bu_list *vlfree,
								  const struct bn_tol *tol);
NMG_EXPORT extern struct edge *nmg_find_e_nearest_pt2(uint32_t *magic_p,
						      const point_t pt2,
						      const mat_t mat,
						      struct bu_list *vlfree,
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
						const struct edgeuse     *eu);
NMG_EXPORT extern struct edgeuse *nmg_find_ot_same_eu_of_e(const struct edge *e);

/* Vertex routines */
NMG_EXPORT extern struct vertexuse *nmg_find_v_in_face(const struct vertex *,
						       const struct faceuse *);
NMG_EXPORT extern struct vertexuse *nmg_find_v_in_shell(const struct vertex *v,
							const struct shell *s,
							int edges_only);
NMG_EXPORT extern struct vertexuse *nmg_find_pnt_in_lu(const struct loopuse *lu,
						      const point_t pt,
						      const struct bn_tol *tol);
NMG_EXPORT extern struct vertexuse *nmg_find_pnt_in_face(const struct faceuse *fu,
							const point_t pt,
							const struct bn_tol *tol);
NMG_EXPORT extern struct vertex *nmg_find_pnt_in_shell(const struct shell *s,
						      const point_t pt,
						      const struct bn_tol *tol);
NMG_EXPORT extern struct vertex *nmg_find_pnt_in_model(const struct model *m,
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

/* Tabulation routines */
NMG_EXPORT extern void nmg_vertex_tabulate(struct bu_ptbl *tab,
					   const uint32_t *magic_p,
					   struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vertexuse_normal_tabulate(struct bu_ptbl *tab,
						     const uint32_t *magic_p,
						     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_edgeuse_tabulate(struct bu_ptbl *tab,
					    const uint32_t *magic_p,
					    struct bu_list *vlfree);
NMG_EXPORT extern void nmg_edge_tabulate(struct bu_ptbl *tab,
					 const uint32_t *magic_p,
					 struct bu_list *vlfree);
NMG_EXPORT extern void nmg_edge_g_tabulate(struct bu_ptbl *tab,
					   const uint32_t *magic_p,
					   struct bu_list *vlfree);
NMG_EXPORT extern void nmg_face_tabulate(struct bu_ptbl *tab,
					 const uint32_t *magic_p,
					 struct bu_list *vlfree);
NMG_EXPORT extern void nmg_edgeuse_with_eg_tabulate(struct bu_ptbl *tab,
						    const struct edge_g_lseg *eg);
NMG_EXPORT extern void nmg_edgeuse_on_line_tabulate(struct bu_ptbl *tab,
						    const uint32_t *magic_p,
						    const point_t pt,
						    const vect_t dir,
						    struct bu_list *vlfree,
						    const struct bn_tol *tol);
NMG_EXPORT extern void nmg_e_and_v_tabulate(struct bu_ptbl *eutab,
					    struct bu_ptbl *vtab,
					    const uint32_t *magic_p,
					    struct bu_list *vlfree);
NMG_EXPORT extern int nmg_2edgeuse_g_coincident(const struct edgeuse     *eu1,
						const struct edgeuse     *eu2,
						const struct bn_tol      *tol);

/* From nmg_extrude.c */
NMG_EXPORT extern void nmg_translate_face(struct faceuse *fu, const vect_t Vec, struct bu_list *vlfree);
NMG_EXPORT extern int nmg_extrude_face(struct faceuse *fu, const vect_t Vec, struct bu_list *vlfree, const struct bn_tol *tol);
NMG_EXPORT extern struct vertexuse *nmg_find_vertex_in_lu(const struct vertex *v, const struct loopuse *lu);
NMG_EXPORT extern void nmg_fix_overlapping_loops(struct shell *s, struct bu_list *vlfree, const struct bn_tol *tol);
NMG_EXPORT extern void nmg_break_crossed_loops(struct shell *is, const struct bn_tol *tol);
NMG_EXPORT extern struct shell *nmg_extrude_cleanup(struct shell *is, const int is_void, struct bu_list *vlfree, const struct bn_tol *tol);
NMG_EXPORT extern void nmg_hollow_shell(struct shell *s, const fastf_t thick, const int approximate, struct bu_list *vlfree, const struct bn_tol *tol);
NMG_EXPORT extern struct shell *nmg_extrude_shell(struct shell *s, const fastf_t dist, const int normal_ward, const int approximate, struct bu_list *vlfree, const struct bn_tol *tol);

/* From nmg_pr.c */
NMG_EXPORT extern char *nmg_orientation(int orientation);
NMG_EXPORT extern void nmg_pr_orient(int orientation,
				     const char *h);
NMG_EXPORT extern void nmg_pr_m(const struct model *m);
NMG_EXPORT extern void nmg_pr_r(const struct nmgregion *r,
				char *h);
NMG_EXPORT extern void nmg_pr_sa(const struct shell_a *sa,
				 char *h);
NMG_EXPORT extern void nmg_pr_lg(const struct loop_g *lg,
				 char *h);
NMG_EXPORT extern void nmg_pr_fg(const uint32_t *magic,
				 char *h);
NMG_EXPORT extern void nmg_pr_s(const struct shell *s,
				char *h);
NMG_EXPORT extern void  nmg_pr_s_briefly(const struct shell *s,
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
NMG_EXPORT extern void nmg_pl_lu_around_eu(const struct edgeuse *eu, struct bu_list *vlfree);
NMG_EXPORT extern void nmg_pr_fus_in_fg(const uint32_t *fg_magic);

/* From nmg_misc.c */
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
NMG_EXPORT extern void nmg_find_zero_length_edges(const struct model *m, struct bu_list *vlfree);
NMG_EXPORT extern struct face *nmg_find_top_face_in_dir(const struct shell *s,
							int dir, long *flags);
NMG_EXPORT extern struct face *nmg_find_top_face(const struct shell *s,
						 int *dir,
						 long *flags);
NMG_EXPORT extern int nmg_find_outer_and_void_shells(struct nmgregion *r,
						     struct bu_ptbl ***shells,
						     struct bu_list *vlfree,
						     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_mark_edges_real(const uint32_t *magic_p, struct bu_list *vlfree);
NMG_EXPORT extern void nmg_tabulate_face_g_verts(struct bu_ptbl *tab,
						 const struct face_g_plane *fg);
NMG_EXPORT extern void nmg_isect_shell_self(struct shell *s,
					    struct bu_list *vlfree,
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
					  plane_t pl, struct bu_list *vlfree);
NMG_EXPORT extern int nmg_calc_face_g(struct faceuse *fu, struct bu_list *vlfree);
NMG_EXPORT extern fastf_t nmg_faceuse_area(const struct faceuse *fu);
NMG_EXPORT extern fastf_t nmg_shell_area(const struct shell *s);
NMG_EXPORT extern fastf_t nmg_region_area(const struct nmgregion *r);
NMG_EXPORT extern fastf_t nmg_model_area(const struct model *m);
/* Some stray rt_ plane functions here */
NMG_EXPORT extern void nmg_purge_unwanted_intersection_points(struct bu_ptbl *vert_list,
							      fastf_t *mag,
							      const struct faceuse *fu,
							      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_in_or_ref(struct vertexuse *vu,
				    struct bu_ptbl *b);
NMG_EXPORT extern void nmg_rebound(struct model *m,
				   const struct bn_tol *tol);
NMG_EXPORT extern void nmg_count_shell_kids(const struct model *m,
					    size_t *total_wires,
					    size_t *total_faces,
					    size_t *total_points);
NMG_EXPORT extern void nmg_close_shell(struct shell *s, struct bu_list *vlfree,
				       const struct bn_tol *tol);
NMG_EXPORT extern struct shell *nmg_dup_shell(struct shell *s,
					      long ***copy_tbl,
					      struct bu_list *vlfree,
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
NMG_EXPORT extern struct model *nmg_mk_model_from_region(struct nmgregion *r,
							 int reindex, struct bu_list *vlfree);
NMG_EXPORT extern void nmg_fix_normals(struct shell *s_orig,
				       struct bu_list *vlfree,
				       const struct bn_tol *tol);
NMG_EXPORT extern int nmg_break_long_edges(struct shell *s,
					   const struct bn_tol *tol);
NMG_EXPORT extern struct faceuse *nmg_mk_new_face_from_loop(struct loopuse *lu);
NMG_EXPORT extern int nmg_split_loops_into_faces(uint32_t *magic_p, struct bu_list *vlfree,
						 const struct bn_tol *tol);
NMG_EXPORT extern int nmg_decompose_shell(struct shell *s, struct bu_list *vlfree,
					  const struct bn_tol *tol);
NMG_EXPORT extern int nmg_unbreak_region_edges(uint32_t *magic_p, struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlist_to_eu(struct bu_list *vlist,
				       struct shell *s);
NMG_EXPORT extern int nmg_mv_shell_to_region(struct shell *s,
					     struct nmgregion *r);
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
					      struct bu_list *vlfree,
					      const struct bn_tol *tol);
NMG_EXPORT extern void nmg_kill_cracks_at_vertex(const struct vertex *vp);
NMG_EXPORT extern int nmg_complex_vertex_solve(struct vertex *new_v,
					       const struct bu_ptbl *faces,
					       const int free_edges,
					       const int approximate,
					       struct bu_list *vlfree,
					       const struct bn_tol *tol);
NMG_EXPORT extern int nmg_bad_face_normals(const struct shell *s,
					   const struct bn_tol *tol);
NMG_EXPORT extern int nmg_faces_are_radial(const struct faceuse *fu1,
					   const struct faceuse *fu2);
NMG_EXPORT extern int nmg_move_edge_thru_pnt(struct edgeuse *mv_eu,
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
					      struct bu_list *vlfree,
					      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_in_vert(struct vertex *new_v,
				  const int approximate,
				  struct bu_list *vlfree,
				  const struct bn_tol *tol);
NMG_EXPORT extern void nmg_mirror_model(struct model *m, struct bu_list *vlfree);
NMG_EXPORT extern int nmg_kill_cracks(struct shell *s);
NMG_EXPORT extern int nmg_kill_zero_length_edgeuses(struct model *m);
NMG_EXPORT extern void nmg_make_faces_within_tol(struct shell *s,
						 struct bu_list *vlfree,
						 const struct bn_tol *tol);
NMG_EXPORT extern void nmg_intersect_loops_self(struct shell *s,
						const struct bn_tol *tol);
NMG_EXPORT extern struct edge_g_cnurb *nmg_join_cnurbs(struct bu_list *crv_head);
NMG_EXPORT extern struct edge_g_cnurb *nmg_arc2d_to_cnurb(point_t i_center,
							  point_t i_start,
							  point_t i_end,
							  int point_type,
							  const struct bn_tol *tol);
NMG_EXPORT extern int nmg_break_edge_at_verts(struct edge *e,
					      struct bu_ptbl *verts,
					      const struct bn_tol *tol);
NMG_EXPORT extern fastf_t nmg_loop_plane_area(const struct loopuse *lu,
					      plane_t pl);
NMG_EXPORT extern int nmg_break_edges(uint32_t *magic_p, struct bu_list *vlfree,
				      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_lu_is_convex(struct loopuse *lu,
				       struct bu_list *vlfree,
				       const struct bn_tol *tol);
NMG_EXPORT extern int nmg_simplify_shell_edges(struct shell *s,
					       const struct bn_tol *tol);
NMG_EXPORT extern int nmg_edge_collapse(struct model *m,
					const struct bn_tol *tol,
					const fastf_t tol_coll,
					const fastf_t min_angle,
					struct bu_list *vlfree);

/* From nmg_copy.c */
NMG_EXPORT extern struct model *nmg_clone_model(const struct model *original);

/* From nmg_tri.c */
NMG_EXPORT extern void nmg_triangulate_shell(struct shell *s,
					     struct bu_list *vlfree,
					     const struct bn_tol  *tol);


NMG_EXPORT extern void nmg_triangulate_model(struct model *m,
					     struct bu_list *vlfree,
					     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_triangulate_fu(struct faceuse *fu,
					 struct bu_list *vlfree,
					 const struct bn_tol *tol);
NMG_EXPORT extern void nmg_dump_model(struct model *m);

/* nmg_manif.c */
NMG_EXPORT extern int nmg_dangling_face(const struct faceuse *fu,
					const char *manifolds);
/* static paint_face */
/* static set_edge_sub_manifold */
/* static set_loop_sub_manifold */
/* static set_face_sub_manifold */
NMG_EXPORT extern char *nmg_shell_manifolds(struct shell *sp,
					    char *tbl);
NMG_EXPORT extern char *nmg_manifolds(struct model *m);

/* From nmg_fuse.c */
NMG_EXPORT extern int nmg_is_common_bigloop(const struct face *f1,
					    const struct face *f2);
NMG_EXPORT extern void nmg_region_v_unique(struct nmgregion *r1, struct bu_list *vlfree,
					   const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ptbl_vfuse(struct bu_ptbl *t,
				     const struct bn_tol *tol);
NMG_EXPORT extern int	nmg_region_both_vfuse(struct bu_ptbl *t1,
					      struct bu_ptbl *t2,
					      const struct bn_tol *tol);
/* nmg_two_region_vertex_fuse replaced with nmg_vertex_fuse */
NMG_EXPORT extern int nmg_vertex_fuse(const uint32_t *magic_p,struct bu_list *vlfree,
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
NMG_EXPORT extern int nmg_edge_fuse(const uint32_t *magic_p,struct bu_list *vlfree,
				    const struct bn_tol *tol);
NMG_EXPORT extern int nmg_edge_g_fuse(const uint32_t *magic_p,struct bu_list *vlfree,
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
NMG_EXPORT extern int nmg_model_face_fuse(struct model *m,struct bu_list *vlfree,
					  const struct bn_tol *tol);
NMG_EXPORT extern int nmg_break_all_es_on_v(uint32_t *magic_p,
					    struct vertex *v,struct bu_list *vlfree,
					    const struct bn_tol *tol);
NMG_EXPORT extern int nmg_break_e_on_v(const uint32_t *magic_p,struct bu_list *vlfree,
				       const struct bn_tol *tol);
/* DEPRECATED: use nmg_break_e_on_v */
NMG_EXPORT extern int nmg_model_break_e_on_v(const uint32_t *magic_p,
					     struct bu_list *vlfree,
					     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_model_fuse(struct model *m,
				     struct bu_list *vlfree,
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
					    struct bu_list *vlfree,
					    const struct bn_tol	*tol);
NMG_EXPORT extern struct nmg_radial	*nmg_find_radial_eu(const struct bu_list *hd,
							    const struct edgeuse *eu);
NMG_EXPORT extern const struct edgeuse *nmg_find_next_use_of_2e_in_lu(const struct edgeuse	*eu,
								      const struct edge	*e1,
								      const struct edge	*e2);
NMG_EXPORT extern void nmg_radial_mark_cracks(struct bu_list	*hd,
					      const struct edge	*e1,
					      const struct edge	*e2,
					      struct bu_list *vlfree,
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
					      struct bu_list *vlfree,
					      const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_s_radial_check(struct shell		*s,
					  struct bu_list *vlfree,
					  const struct bn_tol	*tol);
NMG_EXPORT extern void nmg_r_radial_check(const struct nmgregion	*r,
					  struct bu_list *vlfree,
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
					 void *			state,
					 struct bu_list *vlfree);
NMG_EXPORT extern void nmg_visit_shell(struct shell			*s,
				       const struct nmg_visit_handlers	*htab,
				       void *			state,
				       struct bu_list *vlfree);
NMG_EXPORT extern void nmg_visit_region(struct nmgregion		*r,
					const struct nmg_visit_handlers	*htab,
					void *			state,
					struct bu_list *vlfree);
NMG_EXPORT extern void nmg_visit_model(struct model			*model,
				       const struct nmg_visit_handlers	*htab,
				       void *			state,
				       struct bu_list *vlfree);
NMG_EXPORT extern void nmg_visit(const uint32_t		*magicp,
				 const struct nmg_visit_handlers	*htab,
				 void *			state,
				 struct bu_list *vlfree);


/* nmg_class.c */
NMG_EXPORT extern int nmg_classify_pnt_loop(const point_t pt,
					   const struct loopuse *lu,
					   struct bu_list *vlfree,
					   const struct bn_tol *tol);

NMG_EXPORT extern int nmg_classify_s_vs_s(struct shell *s,
					  struct shell *s2,
					  struct bu_list *vlfree,
					  const struct bn_tol *tol);

NMG_EXPORT extern int nmg_classify_lu_lu(const struct loopuse *lu1,
					 const struct loopuse *lu2,
					 struct bu_list *vlfree,
					 const struct bn_tol *tol);

NMG_EXPORT extern int nmg_class_pnt_f(const point_t pt,
				     const struct faceuse *fu,
				     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_class_pnt_s(const point_t pt,
				     const struct shell *s,
				     const int in_or_out_only,
				     struct bu_list *vlfree,
				     const struct bn_tol *tol);

/* From nmg_pt_fu.c */
NMG_EXPORT extern int nmg_eu_is_part_of_crack(const struct edgeuse *eu);

NMG_EXPORT extern int nmg_class_pnt_lu_except(point_t             pt,
					     const struct loopuse        *lu,
					     const struct edge           *e_p,
					     struct bu_list *vlfree,
					     const struct bn_tol *tol);

NMG_EXPORT extern int nmg_class_pnt_fu_except(const point_t pt,
					     const struct faceuse *fu,
					     const struct loopuse *ignore_lu,
					     void (*eu_func)(struct edgeuse *, point_t, const char *, struct bu_list *),
					     void (*vu_func)(struct vertexuse *, point_t, const char *),
					     const char *priv,
					     const int call_on_hits,
					     const int in_or_out_only,
					     struct bu_list *vlfree,
					     const struct bn_tol *tol);

/* From nmg_plot.c */
NMG_EXPORT extern void nmg_pl_shell(FILE *fp,
				    const struct shell *s,
				    int fancy,
				    struct bu_list *vlfree);

NMG_EXPORT extern void nmg_vu_to_vlist(struct bu_list *vhead,
				       const struct vertexuse    *vu,
				       struct bu_list *vlfree);
NMG_EXPORT extern void nmg_eu_to_vlist(struct bu_list *vhead,
				       const struct bu_list      *eu_hd,
				       struct bu_list *vlfree);
NMG_EXPORT extern void nmg_lu_to_vlist(struct bu_list *vhead,
				       const struct loopuse      *lu,
				       int                       poly_markers,
				       const vectp_t             norm,
				       struct bu_list *vlfree);
NMG_EXPORT extern void nmg_snurb_fu_to_vlist(struct bu_list              *vhead,
					     const struct faceuse        *fu,
					     int                 poly_markers,
					     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_s_to_vlist(struct bu_list             *vhead,
				      const struct shell *s,
				      int                        poly_markers,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_r_to_vlist(struct bu_list             *vhead,
				      const struct nmgregion     *r,
				      int                        poly_markers,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_m_to_vlist(struct bu_list     *vhead,
				      struct model       *m,
				      int                poly_markers,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_offset_eu_vert(point_t                        base,
					  const struct edgeuse   *eu,
					  const vect_t           face_normal,
					  int                    tip);
/* plot */
NMG_EXPORT extern void nmg_pl_v(FILE     *fp,
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
				 int blue,
				 struct bu_list *vlfree);
NMG_EXPORT extern void nmg_pl_fu(FILE *fp,
				 const struct faceuse *fu,
				 long *b,
				 int red,
				 int green,
				 int blue,
				 struct bu_list *vlfree);
NMG_EXPORT extern void nmg_pl_s(FILE *fp,
				const struct shell *s,
				struct bu_list *vlfree);
NMG_EXPORT extern void nmg_pl_r(FILE *fp,
				const struct nmgregion *r,
				struct bu_list *vlfree);
NMG_EXPORT extern void nmg_pl_m(FILE *fp,
				const struct model *m,
				struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_v(struct bv_vlblock *vbp,
				     const struct vertex *v,
				     long *tab,
				     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_e(struct bv_vlblock *vbp,
				     const struct edge *e,
				     long *tab,
				     int red,
				     int green,
				     int blue,
				     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_eu(struct bv_vlblock *vbp,
				      const struct edgeuse *eu,
				      long *tab,
				      int red,
				      int green,
				      int blue,
				      int fancy,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_euleft(struct bu_list                 *vh,
					  const struct edgeuse           *eu,
					  const point_t                  center,
					  const mat_t                    mat,
					  const vect_t                   xvec,
					  const vect_t                   yvec,
					  double                         len,
					  struct bu_list 	        *vlfree,
					  const struct bn_tol            *tol);
NMG_EXPORT extern void nmg_vlblock_around_eu(struct bv_vlblock           *vbp,
					     const struct edgeuse        *arg_eu,
					     long                        *tab,
					     int                 fancy,
					     struct bu_list *vlfree,
					     const struct bn_tol *tol);
NMG_EXPORT extern void nmg_vlblock_lu(struct bv_vlblock *vbp,
				      const struct loopuse *lu,
				      long *tab,
				      int red,
				      int green,
				      int blue,
				      int fancy,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_fu(struct bv_vlblock *vbp,
				      const struct faceuse *fu,
				      long *tab, int fancy, struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_s(struct bv_vlblock *vbp,
				     const struct shell *s,
				     int fancy,
				     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_r(struct bv_vlblock *vbp,
				     const struct nmgregion *r,
				     int fancy,
				     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_m(struct bv_vlblock *vbp,
				     const struct model *m,
				     int fancy,
				     struct bu_list *vlfree);

/* visualization helper routines */
NMG_EXPORT extern void nmg_pl_edges_in_2_shells(struct bv_vlblock        *vbp,
						long                     *b,
						const struct edgeuse     *eu,
						int                      fancy,
						struct bu_list *vlfree,
						const struct bn_tol      *tol);
NMG_EXPORT extern void nmg_pl_isect(const char           *filename,
				    const struct shell   *s,
				    struct bu_list *vlfree,
				    const struct bn_tol  *tol);
NMG_EXPORT extern void nmg_pl_comb_fu(int num1,
				      int num2,
				      const struct faceuse *fu1,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_pl_2fu(const char *str,
				  const struct faceuse *fu1,
				  const struct faceuse *fu2,
				  int show_mates,
				  struct bu_list *vlfree);
/* graphical display of classifier results */
NMG_EXPORT extern void nmg_show_broken_classifier_stuff(uint32_t *p,
							char     **classlist,
							int      all_new,
							int      fancy,
							const char       *a_string,
							struct bu_list *vlfree);
NMG_EXPORT extern void nmg_face_plot(const struct faceuse *fu, struct bu_list *vlfree);
NMG_EXPORT extern void nmg_2face_plot(const struct faceuse *fu1,
				      const struct faceuse *fu2,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_face_lu_plot(const struct loopuse *lu,
					const struct vertexuse *vu1,
					const struct vertexuse *vu2,
					struct bu_list *vlfree);
NMG_EXPORT extern void nmg_plot_lu_ray(const struct loopuse              *lu,
				       const struct vertexuse            *vu1,
				       const struct vertexuse            *vu2,
				       const vect_t                      left,
				       struct bu_list *vlfree);
NMG_EXPORT extern void nmg_plot_ray_face(const char *fname,
					 point_t pt,
					 const vect_t dir,
					 const struct faceuse *fu,
					 struct bu_list *vlfree);
NMG_EXPORT extern void nmg_plot_lu_around_eu(const char          *prefix,
					     const struct edgeuse        *eu,
					     struct bu_list *vlfree,
					     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_snurb_to_vlist(struct bu_list          *vhead,
					 const struct face_g_snurb       *fg,
					 int                     n_interior,
					 struct bu_list *vlfree);
NMG_EXPORT extern void nmg_cnurb_to_vlist(struct bu_list *vhead,
					  const struct edgeuse *eu,
					  int n_interior,
					  int cmd,
					  struct bu_list *vlfree);

/**
 * edge use distance tolerance
 */
NMG_EXPORT extern double nmg_eue_dist;


/* from nmg_mesh.c */
NMG_EXPORT extern int nmg_mesh_two_faces(struct faceuse *fu1,
					 struct faceuse *fu2,
					 const struct bn_tol     *tol);
NMG_EXPORT extern void nmg_radial_join_eu(struct edgeuse *eu1,
					  struct edgeuse *eu2,
					  const struct bn_tol *tol);
NMG_EXPORT extern void nmg_mesh_faces(struct faceuse *fu1,
				      struct faceuse *fu2,
				      struct bu_list *vlfree,
				      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_mesh_face_shell(struct faceuse *fu1,
					  struct shell *s,
					  const struct bn_tol *tol);
NMG_EXPORT extern int nmg_mesh_shell_shell(struct shell *s1,
					   struct shell *s2,
					   struct bu_list *vlfree,
					   const struct bn_tol *tol);
NMG_EXPORT extern double nmg_measure_fu_angle(const struct edgeuse *eu,
					      const vect_t xvec,
					      const vect_t yvec,
					      const vect_t zvec);

/* from nmg_bool.c */
NMG_EXPORT extern struct nmgregion *nmg_do_bool(struct nmgregion *s1,
						struct nmgregion *s2,
						const int oper, struct bu_list *vlfree, const struct bn_tol *tol);
NMG_EXPORT extern int nmg_two_region_vertex_fuse(struct nmgregion *r1,
						 struct nmgregion *r2,
						 const struct bn_tol *tol);


/* from nmg_class.c */
NMG_EXPORT extern void nmg_class_shells(struct shell *sA,
					struct shell *sB,
					char **classlist,
					struct bu_list *vlfree,
					const struct bn_tol *tol);

/* from nmg_fcut.c */
/* static void ptbl_vsort */
NMG_EXPORT extern int nmg_ck_vu_ptbl(struct bu_ptbl      *p,
				     struct faceuse      *fu);
NMG_EXPORT extern double nmg_vu_angle_measure(struct vertexuse   *vu,
					      vect_t x_dir,
					      vect_t y_dir,
					      int assessment,
					      int in);
NMG_EXPORT extern int nmg_wedge_class(int        ass,    /* assessment of two edges forming wedge */
				      double     a,
				      double     b);
NMG_EXPORT extern void nmg_sanitize_fu(struct faceuse    *fu);
NMG_EXPORT extern void nmg_unlist_v(struct bu_ptbl       *b,
				    fastf_t *mag,
				    struct vertex        *v);
NMG_EXPORT extern struct edge_g_lseg *nmg_face_cutjoin(struct bu_ptbl *b1,
						       struct bu_ptbl *b2,
						       fastf_t *mag1,
						       fastf_t *mag2,
						       struct faceuse *fu1,
						       struct faceuse *fu2,
						       point_t pt,
						       vect_t dir,
						       struct edge_g_lseg *eg,
						       struct bu_list *vlfree,
						       const struct bn_tol *tol);
NMG_EXPORT extern void nmg_fcut_face_2d(struct bu_ptbl *vu_list,
					fastf_t *mag,
					struct faceuse *fu1,
					struct faceuse *fu2,
					struct bu_list *vlfree,
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
NMG_EXPORT extern void nmg_evaluate_boolean(struct shell *sA,
					    struct shell *sB,
					    int          op,
					    char         **classlist,
					    struct bu_list *vlfree,
					    const struct bn_tol  *tol);

/* The following functions cannot be publicly declared because struct
 * nmg_bool_state is private to nmg_eval.c
 */
/* nmg_eval_shell */
/* nmg_eval_action */
/* nmg_eval_plot */

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
NMG_EXPORT extern void nmg_veu(const struct bu_list      *hp,
			       const uint32_t *up_magic_p);
NMG_EXPORT extern void nmg_vlg(const struct loop_g *lg);
NMG_EXPORT extern void nmg_vloop(const struct loop *l,
				 const struct loopuse *lup);
NMG_EXPORT extern void nmg_vlu(const struct bu_list      *hp,
			       const uint32_t *up);
NMG_EXPORT extern void nmg_vfg(const struct face_g_plane *fg);
NMG_EXPORT extern void nmg_vface(const struct face *f,
				 const struct faceuse *fup);
NMG_EXPORT extern void nmg_vfu(const struct bu_list      *hp,
			       const struct shell *s);
NMG_EXPORT extern void nmg_vsshell(const struct shell *s,
				   const struct nmgregion *r);
NMG_EXPORT extern void nmg_vshell(const struct bu_list *hp,
				  const struct nmgregion *r);
NMG_EXPORT extern void nmg_vregion(const struct bu_list *hp,
				   const struct model *m);
NMG_EXPORT extern void nmg_vmodel(const struct model *m);

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
NMG_EXPORT extern size_t nmg_ck_geometry(const struct model *m,
					 struct bu_list *vlfree,
					 const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ck_face_worthless_edges(const struct faceuse *fu);
NMG_EXPORT extern void nmg_ck_lueu(const struct loopuse *lu, const char *s);
NMG_EXPORT extern int nmg_check_radial(const struct edgeuse *eu, const struct bn_tol *tol);
NMG_EXPORT extern int nmg_eu_2s_orient_bad(const struct edgeuse  *eu,
					   const struct shell    *s1,
					   const struct shell    *s2,
					   const struct bn_tol   *tol);
NMG_EXPORT extern int nmg_ck_closed_surf(const struct shell *s,
					 const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ck_closed_region(const struct nmgregion *r,
					   const struct bn_tol *tol);
NMG_EXPORT extern void nmg_ck_v_in_2fus(const struct vertex *vp,
					const struct faceuse *fu1,
					const struct faceuse *fu2,
					const struct bn_tol *tol);
NMG_EXPORT extern void nmg_ck_vs_in_region(const struct nmgregion *r,
					   struct bu_list *vlfree,
					   const struct bn_tol *tol);


/* From nmg_inter.c */
NMG_EXPORT extern struct vertexuse *nmg_make_dualvu(struct vertex *v,
						    struct faceuse *fu,
						    const struct bn_tol *tol);
NMG_EXPORT extern struct vertexuse *nmg_enlist_vu(struct nmg_inter_struct        *is,
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
NMG_EXPORT extern void nmg_break_eg_on_v(const struct edge_g_lseg        *eg,
					 struct vertex           *v,
					 const struct bn_tol     *tol);
NMG_EXPORT extern int nmg_isect_2colinear_edge2p(struct edgeuse  *eu1,
						 struct edgeuse  *eu2,
						 struct faceuse          *fu,
						 struct nmg_inter_struct *is,
						 struct bu_ptbl          *l1,
						 struct bu_ptbl          *l2);
NMG_EXPORT extern int nmg_isect_edge2p_edge2p(struct nmg_inter_struct    *is,
					      struct edgeuse             *eu1,
					      struct edgeuse             *eu2,
					      struct faceuse             *fu1,
					      struct faceuse             *fu2);
NMG_EXPORT extern int nmg_isect_construct_nice_ray(struct nmg_inter_struct       *is,
						   struct faceuse                *fu2);
NMG_EXPORT extern void nmg_enlist_one_vu(struct nmg_inter_struct *is,
					 const struct vertexuse  *vu,
					 fastf_t                 dist);
NMG_EXPORT extern int    nmg_isect_line2_edge2p(struct nmg_inter_struct  *is,
						struct bu_ptbl           *list,
						struct edgeuse           *eu1,
						struct faceuse           *fu1,
						struct faceuse           *fu2);
NMG_EXPORT extern void nmg_isect_line2_vertex2(struct nmg_inter_struct   *is,
					       struct vertexuse  *vu1,
					       struct faceuse            *fu1);
NMG_EXPORT extern int nmg_isect_two_ptbls(struct nmg_inter_struct        *is,
					  const struct bu_ptbl   *t1,
					  const struct bu_ptbl   *t2);
NMG_EXPORT extern struct edge_g_lseg     *nmg_find_eg_on_line(const uint32_t *magic_p,
							      const point_t              pt,
							      const vect_t               dir,
							      struct bu_list *vlfree,
							      const struct bn_tol        *tol);
NMG_EXPORT extern int nmg_k0eu(struct vertex     *v);
NMG_EXPORT extern struct vertex *nmg_repair_v_near_v(struct vertex               *hit_v,
						     struct vertex               *v,
						     const struct edge_g_lseg    *eg1,
						     const struct edge_g_lseg    *eg2,
						     int                 bomb,
						     const struct bn_tol *tol);
NMG_EXPORT extern struct vertex *nmg_search_v_eg(const struct edgeuse    *eu,
						 int                     second,
						 const struct edge_g_lseg        *eg1,
						 const struct edge_g_lseg        *eg2,
						 struct vertex           *hit_v,
						 const struct bn_tol     *tol);
NMG_EXPORT extern struct vertex *nmg_common_v_2eg(struct edge_g_lseg     *eg1,
						  struct edge_g_lseg     *eg2,
						  const struct bn_tol    *tol);
NMG_EXPORT extern int nmg_is_vertex_on_inter(struct vertex *v,
					     struct faceuse *fu1,
					     struct faceuse *fu2,
					     struct nmg_inter_struct *is,
					     struct bu_list *vlfree);
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
				       struct bu_ptbl            *verts,
				       struct edgeuse            *eu,
				       struct faceuse          *fu,
				       struct bu_list *vlfree);
NMG_EXPORT extern void nmg_isect_fu_jra(struct nmg_inter_struct  *is,
					struct faceuse           *fu1,
					struct faceuse           *fu2,
					struct bu_ptbl           *eu1_list,
					struct bu_ptbl           *eu2_list,
					struct bu_list *vlfree);
NMG_EXPORT extern void nmg_isect_line2_face2pNEW(struct nmg_inter_struct *is,
						 struct faceuse *fu1, struct faceuse *fu2,
						 struct bu_ptbl *eu1_list,
						 struct bu_ptbl *eu2_list,
						 struct bu_list *vlfree);
NMG_EXPORT extern int    nmg_is_eu_on_line3(const struct edgeuse *eu,
					    const point_t                pt,
					    const vect_t         dir,
					    const struct bn_tol  *tol);
NMG_EXPORT extern struct edge_g_lseg     *nmg_find_eg_between_2fg(const struct faceuse   *ofu1,
								  const struct faceuse   *fu2,
								  struct bu_list *vlfree,
								  const struct bn_tol    *tol);
NMG_EXPORT extern struct edgeuse *nmg_does_fu_use_eg(const struct faceuse        *fu1,
						     const uint32_t      *eg);
NMG_EXPORT extern int rt_line_on_plane(const point_t     pt,
				       const vect_t      dir,
				       const plane_t     plane,
				       const struct bn_tol       *tol);
NMG_EXPORT extern void nmg_cut_lu_into_coplanar_and_non(struct loopuse *lu,
							plane_t pl,
							struct nmg_inter_struct *is,
							struct bu_list *vlfree);
NMG_EXPORT extern void nmg_check_radial_angles(char *str,
					       struct shell *s,
					       struct bu_list *vlfree,
					       const struct bn_tol *tol);
NMG_EXPORT extern int nmg_faces_can_be_intersected(struct nmg_inter_struct *bs,
						   const struct faceuse *fu1,
						   const struct faceuse *fu2,
						   struct bu_list *vlfree,
						   const struct bn_tol *tol);
NMG_EXPORT extern void nmg_isect_two_generic_faces(struct faceuse                *fu1,
						   struct faceuse                *fu2,
						   struct bu_list *vlfree,
						   const struct bn_tol   *tol);
NMG_EXPORT extern void nmg_crackshells(struct shell *s1,
				       struct shell *s2,
				       struct bu_list *vlfree,
				       const struct bn_tol *tol);
NMG_EXPORT extern int nmg_fu_touchingloops(const struct faceuse *fu);


/* From nmg_index.c */
NMG_EXPORT extern int nmg_index_of_struct(const uint32_t *p);
NMG_EXPORT extern void nmg_m_set_high_bit(struct model *m);
NMG_EXPORT extern void nmg_m_reindex(struct model *m, long newindex);
NMG_EXPORT extern void nmg_vls_struct_counts(struct bu_vls *str,
					     const struct nmg_struct_counts *ctr);
NMG_EXPORT extern void nmg_pr_struct_counts(const struct nmg_struct_counts *ctr,
					    const char *str);
NMG_EXPORT extern uint32_t **nmg_m_struct_count(struct nmg_struct_counts *ctr,
						const struct model *m);
NMG_EXPORT extern void nmg_pr_m_struct_counts(const struct model *m,
					      const char         *str);
NMG_EXPORT extern void nmg_merge_models(struct model *m1,
					struct model *m2);
NMG_EXPORT extern long nmg_find_max_index(const struct model *m);

/* From nmg_rt_isect.c */
NMG_EXPORT extern const char * nmg_rt_inout_str(int code);

NMG_EXPORT extern void nmg_rt_print_hitlist(struct bu_list *hd);

NMG_EXPORT extern void nmg_rt_print_hitmiss(struct nmg_hitmiss *a_hit);

NMG_EXPORT extern int nmg_class_ray_vs_shell(struct nmg_ray *rp,
					     const struct shell *s,
					     const int in_or_out_only,
					     struct bu_list *vlfree,
					     const struct bn_tol *tol);

NMG_EXPORT extern void nmg_isect_ray_model(struct nmg_ray_data *rd, struct bu_list *vlfree);


/***********************************************************************************/
/*                        NURBS specific bits of NMG                               */
/***********************************************************************************/

/* TODO - this is another one of those data concepts common to librt and libnmg */
struct nmg_curvature {
    vect_t      crv_pdir;       /**< @brief Principle direction */
    fastf_t     crv_c1;         /**< @brief curvature in principle dir */
    fastf_t     crv_c2;         /**< @brief curvature in other direction */
};

/* nurb_basis.c */
NMG_EXPORT extern fastf_t nmg_nurb_basis_eval(struct knot_vector *knts, int interval,
					      int order, fastf_t mu);

/* nurb_bezier.c */
NMG_EXPORT extern int nmg_nurb_bezier(struct bu_list *bezier_hd, const struct face_g_snurb * srf);
NMG_EXPORT extern int nmg_bez_check(const struct face_g_snurb * srf);
NMG_EXPORT extern int nurb_crv_is_bezier(const struct edge_g_cnurb *crv);
NMG_EXPORT extern void nurb_c_to_bezier(struct bu_list *clist, struct edge_g_cnurb *crv);

/* nurb_bound.c */
NMG_EXPORT extern int nmg_nurb_s_bound(struct face_g_snurb *srf, point_t bmin, point_t bmax);
NMG_EXPORT extern int nmg_nurb_c_bound(struct edge_g_cnurb *crv, point_t bmin, point_t bmax);
NMG_EXPORT extern int nmg_nurb_s_check(struct face_g_snurb *srf);
NMG_EXPORT extern int nmg_nurb_c_check(struct edge_g_cnurb *crv);

/* nurb_copy.c */
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_scopy(const struct face_g_snurb *srf);
NMG_EXPORT extern struct edge_g_cnurb *nmg_nurb_crv_copy(const struct edge_g_cnurb * crv);

/* nurb_diff.c */
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_s_diff(const struct face_g_snurb *srf, int dir);
NMG_EXPORT extern struct edge_g_cnurb *nmg_nurb_c_diff(const struct edge_g_cnurb *crv);
NMG_EXPORT extern void nmg_nurb_mesh_diff(int order, const fastf_t *o_pts,
					  fastf_t *n_pts,
					  const fastf_t *knots, int o_stride, int n_stride,
					  int o_size, int pt_type);

/* nurb_eval.c */
NMG_EXPORT extern void nmg_nurb_s_eval(const struct face_g_snurb *srf, fastf_t u, fastf_t v, fastf_t * final_value);
NMG_EXPORT extern void nmg_nurb_c_eval(const struct edge_g_cnurb *crv, fastf_t param, fastf_t * final_value);
NMG_EXPORT extern fastf_t *nmg_nurb_eval_crv(fastf_t *crv, int order,
					     fastf_t param,
					     const struct knot_vector *k_vec, int k_index, int coords);
NMG_EXPORT extern void nmg_nurb_pr_crv(fastf_t *crv, int c_size, int coords);

/* nurb_flat.c */
NMG_EXPORT extern int nmg_nurb_s_flat(struct face_g_snurb *srf, fastf_t epsilon);
NMG_EXPORT extern fastf_t nmg_nurb_crv_flat(fastf_t *crv, int	size, int pt_type);

/* nurb_knot.c */
NMG_EXPORT extern void nmg_nurb_kvknot(struct knot_vector *new_knots, int order,
				       fastf_t lower, fastf_t upper, int num);
NMG_EXPORT extern void nmg_nurb_kvmult(struct knot_vector *new_kv,
				       const struct knot_vector *kv,
				       int num, fastf_t val);
NMG_EXPORT extern void nmg_nurb_kvgen(struct knot_vector *kv,
				      fastf_t lower, fastf_t upper, int num);
NMG_EXPORT extern void nmg_nurb_kvmerge(struct knot_vector *new_knots,
					const struct knot_vector *kv1,
					const struct knot_vector *kv2);
NMG_EXPORT extern int nmg_nurb_kvcheck(fastf_t val, const struct knot_vector *kv);
NMG_EXPORT extern void nmg_nurb_kvextract(struct knot_vector *new_kv,
					  const struct knot_vector *kv,
					  int lower, int upper);
NMG_EXPORT extern void nmg_nurb_kvcopy(struct knot_vector *new_kv,
				       const struct knot_vector *old_kv);
NMG_EXPORT extern void nmg_nurb_kvnorm(struct knot_vector *kv);
NMG_EXPORT extern int nmg_nurb_knot_index(const struct knot_vector *kv, fastf_t k_value, int order);
NMG_EXPORT extern void nmg_nurb_gen_knot_vector(struct knot_vector *new_knots,
						int order, fastf_t lower, fastf_t upper);

/* nurb_norm.c */
NMG_EXPORT extern void nmg_nurb_s_norm(struct face_g_snurb *srf, fastf_t u, fastf_t v, fastf_t * norm);

/* nurb_c2.c */
NMG_EXPORT extern void nmg_nurb_curvature(struct nmg_curvature *cvp,
					  const struct face_g_snurb *srf, fastf_t u, fastf_t v);

/* nurb_plot.c */
NMG_EXPORT extern void nmg_nurb_plot_snurb(FILE *fp, const struct face_g_snurb *srf);
NMG_EXPORT extern void nmg_nurb_plot_cnurb(FILE *fp, const struct edge_g_cnurb *crv);
NMG_EXPORT extern void nmg_nurb_s_plot(const struct face_g_snurb *srf);

/* nurb_interp.c */
NMG_EXPORT extern void nmg_nurb_cinterp(struct edge_g_cnurb *crv, int order,
					const fastf_t *data, int n);
NMG_EXPORT extern void nmg_nurb_sinterp(struct face_g_snurb *srf, int order,
					const fastf_t *data, int ymax, int xmax);

/* nurb_poly.c */
NMG_EXPORT extern struct nmg_nurb_poly *nmg_nurb_to_poly(struct face_g_snurb *srf);
NMG_EXPORT extern struct nmg_nurb_poly *nmg_nurb_mk_poly(fastf_t *v1, fastf_t *v2, fastf_t *v3,
							 fastf_t uv1[2], fastf_t uv2[2], fastf_t uv3[2]);

/* nurb_ray.c */
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_project_srf(const struct face_g_snurb *srf,
							    plane_t plane1, plane_t plane2);
NMG_EXPORT extern void nmg_nurb_clip_srf(const struct face_g_snurb *srf,
					 int dir, fastf_t *min, fastf_t *max);
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_region_from_srf(const struct face_g_snurb *srf,
								int dir, fastf_t param1, fastf_t param2);
NMG_EXPORT extern struct nmg_nurb_uv_hit *nmg_nurb_intersect(const struct face_g_snurb * srf,
							     plane_t plane1, plane_t plane2, double uv_tol, struct bu_list *plist);

/* nurb_refine.c */
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_s_refine(const struct face_g_snurb *srf,
							 int dir, struct knot_vector *kv);
NMG_EXPORT extern struct edge_g_cnurb *nmg_nurb_c_refine(const struct edge_g_cnurb * crv,
							 struct knot_vector *kv);

/* nurb_solve.c */
NMG_EXPORT extern void nmg_nurb_solve(fastf_t *mat_1, fastf_t *mat_2,
				      fastf_t *solution, int dim, int coords);
NMG_EXPORT extern void nmg_nurb_doolittle(fastf_t *mat_1, fastf_t *mat_2,
					  int row, int coords);
NMG_EXPORT extern void nmg_nurb_forw_solve(const fastf_t *lu, const fastf_t *b,
					   fastf_t *y, int n);
NMG_EXPORT extern void nmg_nurb_back_solve(const fastf_t *lu, const fastf_t *y,
					   fastf_t *x, int n);
NMG_EXPORT extern void nmg_nurb_p_mat(const fastf_t * mat, int dim);

/* nurb_split.c */
NMG_EXPORT extern void nmg_nurb_s_split(struct bu_list *split_hd, const struct face_g_snurb *srf,
					int dir);
NMG_EXPORT extern void nmg_nurb_c_split(struct bu_list *split_hd, const struct edge_g_cnurb *crv);

/* nurb_trim.c */
NMG_EXPORT extern int nmg_uv_in_lu(const fastf_t u, const fastf_t v, const struct loopuse *lu);

/* nurb_util.c */
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_new_snurb(int u_order, int v_order,
							  int n_u_knots, int n_v_knots,
							  int n_rows, int n_cols, int pt_type);
NMG_EXPORT extern struct edge_g_cnurb *nmg_nurb_new_cnurb(int order, int n_knots,
							  int n_pts, int pt_type);
NMG_EXPORT extern void nmg_nurb_free_snurb(struct face_g_snurb *srf);
NMG_EXPORT extern void nmg_nurb_free_cnurb(struct edge_g_cnurb * crv);
NMG_EXPORT extern void nmg_nurb_c_print(const struct edge_g_cnurb *crv);
NMG_EXPORT extern void nmg_nurb_s_print(char *c, const struct face_g_snurb *srf);
NMG_EXPORT extern void nmg_nurb_pr_kv(const struct knot_vector *kv);
NMG_EXPORT extern void nmg_nurb_pr_mesh(const struct face_g_snurb *m);
NMG_EXPORT extern void nmg_nurb_print_pnt_type(int c);
NMG_EXPORT extern void nmg_nurb_clean_cnurb(struct edge_g_cnurb *crv);

/* nurb_xsplit.c */
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_s_xsplit(struct face_g_snurb *srf,
							 fastf_t param, int dir);
NMG_EXPORT extern struct edge_g_cnurb *nmg_nurb_c_xsplit(struct edge_g_cnurb *crv, fastf_t param);

/* oslo_calc.c */
NMG_EXPORT extern struct oslo_mat *nmg_nurb_calc_oslo(int order,
						      const struct knot_vector *tau_kv,
						      struct knot_vector *t_kv);
NMG_EXPORT extern void nmg_nurb_pr_oslo(struct oslo_mat *om);
NMG_EXPORT extern void nmg_nurb_free_oslo(struct oslo_mat *om);

/* oslo_map.c */
NMG_EXPORT extern void nmg_nurb_map_oslo(struct oslo_mat *oslo,
					 fastf_t *old_pts, fastf_t *new_pts,
					 int o_stride, int n_stride,
					 int lower, int upper, int pt_type);

/* nurb_tess.c */
NMG_EXPORT extern fastf_t rt_cnurb_par_edge(const struct edge_g_cnurb *crv, fastf_t epsilon);

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
