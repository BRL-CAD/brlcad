/*                       T O P O L O G Y . H
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

/*----------------------------------------------------------------------*/
/** @addtogroup nmg_topology
 *
 * This is the interconnected hierarchical data scheme proposed by Weiler and
 * documented by Muuss and Butler in: Combinatorial Solid Geometry, Boundary
 * Representations, and Non-Manifold Geometry, State of the Art in Computer
 * Graphics: Visualization and Modeling D. F. Rogers, R. A. Earnshaw editors,
 * Springer-Verlag, New York, 1991, pages 185-223:
 * https://ftp.arl.army.mil/~mike/papers/90nmg/joined.html
 *
 * Because of the interconnectedness of these containers, they are defined
 * together rather than in separate headers - all of the "use" containers
 * need to know about other use container types, as seen in Figure 4 from the
 * Muuss/Butler paper:
 *
   @verbatim
   Model
     |
   Region
     |
   Shell
   ||||
   |||*--> Faceuse <--------> Face
   |||          |
   ||*---> Loopuse <--------> Loop
   ||      |    |
   |*------|--> Edgeuse <---> Edge
   |       |    |
   |       *--> Vertexuse <-> Vertex
   *------------^
   @endverbatim
 *
 * Each element has a direct connection to its parent and child types.  The
 * elements are defined thusly:
 *
 * - Vertex:  a unique topological point
 * - Edge:    a line or curve terminated by one or two Vertices
 * - Loop:    a single Vertex, or a circuit of one or more Edges
 * - Face:    one or more Loops defining a surface.  Exterior Loops include an area, interior Loops exclude an area
 * - Shell:   a single Vertex, or a collection of Faces, Loops and Edgesa
 * - Region:  a collection of shells
 * - Model:   a collection of regions
 *
 * The "use" notation refers to a conceptual separation between an entity and a
 * reference to that entity.  For example, a face has two sides - each of those
 * sides is individually referenced by a "use" of the underlying face data
 * structure, with the faceuse adding additional information to identify the
 * specific side associated with that particular application of the underlying
 * face data.
 *
 */
/** @{ */
/** @file nmg/topology.h */

#ifndef NMG_TOPOLOGY_H
#define NMG_TOPOLOGY_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"

__BEGIN_DECLS

/**
 * @brief
 * NMG topological vertex - the simplest element of the topology system
 *
 * A vertex stores knowledge of all the places in the model topology where it
 * is used, via a list of "vertexuse" containers stored in the vu_hd list.
 * Each vertex has its own unique associated geometric point, vg_p;
 *
 * Neither vertex_g nor vertex structures are the primary topological unit used
 * in describing more complex structures - it is the "use" of a vertex (i.e.
 * the vertexuse structure) that manifests an active vertex point in the model.
 */
struct vertex {
    uint32_t magic;
    struct bu_list vu_hd;       /**< @brief heads list of vu's of this vertex */
    struct vertex_g *vg_p;      /**< @brief geometry */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * NMG topological vertex usage
 */
struct vertexuse {
    struct bu_list l;           /**< @brief list of all vu's on a vertex */
    union {
        struct shell *s_p;      /**< @brief no fu's or eu's on shell */
        struct loopuse *lu_p;   /**< @brief loopuse contains single vertex */
        struct edgeuse *eu_p;   /**< @brief eu causing this vu */
        uint32_t *magic_p; /**< @brief for those times when we're not sure */
    } up;
    struct vertex *v_p;         /**< @brief vertex definition and attributes */
    union {
        uint32_t *magic_p;
        struct vertexuse_a_plane *plane_p;
        struct vertexuse_a_cnurb *cnurb_p;
    } a;                        /**< @brief Attributes */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * NMG topological edge
 *
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
 * An edge occupies the range eu->param to eu->eumate_p->param in its
 * geometry's parameter space.  (cnurbs only)
 */
struct edge {
    uint32_t magic;
    struct edgeuse *eu_p;       /**< @brief Ptr to one use of this edge */
    long is_real;               /**< @brief artifact or modeled edge (from tessellator) */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * NMG topological edge usage
 */
struct edgeuse {
    struct bu_list l;           /**< @brief cw/ccw edges in loop or wire edges in shell */
    struct bu_list l2;          /**< @brief member of edge_g's eu_hd2 list */
    union {
        struct loopuse *lu_p;
        struct shell *s_p;
        uint32_t *magic_p;      /**< @brief for those times when we're not sure */
    } up;
    struct edgeuse *eumate_p;   /**< @brief eu on other face or other end of wire*/
    struct edgeuse *radial_p;   /**< @brief eu on radially adj. fu (null if wire)*/
    struct edge *e_p;           /**< @brief edge definition and attributes */
    int orientation;            /**< @brief compared to geom (null if wire) */
    struct vertexuse *vu_p;     /**< @brief first vu of eu in this orient */
    union {
        uint32_t *magic_p;
        struct edge_g_lseg *lseg_p;
        struct edge_g_cnurb *cnurb_p;
    } g;                        /**< @brief geometry */
    /* (u, v, w) param[] of vu is found in vu_p->vua_p->param */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * NMG topological loop
 */
struct loop {
    uint32_t magic;
    struct loopuse *lu_p;       /**< @brief Ptr to one use of this loop */
    struct loop_a *la_p;        /**< @brief Geometry */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * NMG topological loop usage
 */
struct loopuse {
    struct bu_list l;           /**< @brief lu's, in fu's lu_hd, or shell's lu_hd */
    union {
        struct faceuse *fu_p;   /**< @brief owning face-use */
        struct shell *s_p;
        uint32_t *magic_p;
    } up;
    struct loopuse *lumate_p;   /**< @brief loopuse on other side of face */
    int orientation;            /**< @brief OT_SAME=outside loop */
    struct loop *l_p;           /**< @brief loop definition and attributes */
    struct bu_list down_hd;     /**< @brief eu list or vu pointer */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * NMG topological face
 */
struct face {
    struct bu_list l;           /**< @brief faces in face_g's f_hd list */
    struct faceuse *fu_p;       /**< @brief Ptr up to one use of this face */
    union {
        uint32_t *magic_p;
        struct face_g_plane *plane_p;
        struct face_g_snurb *snurb_p;
    } g;                        /**< @brief geometry */
    int flip;                   /**< @brief !0 ==> flip normal of fg */
    /* These might be better stored in a face_a (not faceuse_a!) */
    /* These are not stored on disk */
    point_t min_pt;             /**< @brief minimums of bounding box */
    point_t max_pt;             /**< @brief maximums of bounding box */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * NMG topological face usage
 */
struct faceuse {
    struct bu_list l;           /**< @brief fu's, in shell's fu_hd list */
    struct shell *s_p;          /**< @brief owning shell */
    struct faceuse *fumate_p;   /**< @brief opposite side of face */
    int orientation;            /**< @brief rel to face geom defn */
    int outside;                /**< @brief RESERVED for future:  See Lee Butler */
    struct face *f_p;           /**< @brief face definition and attributes */
    struct bu_list lu_hd;       /**< @brief list of loops in face-use */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * NMG topological shell
 *
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
    struct bu_list l;           /**< @brief shells, in region's s_hd list */
    struct nmgregion *r_p;      /**< @brief owning region */
    struct shell_a *sa_p;       /**< @brief attribs */

    struct bu_list fu_hd;       /**< @brief list of face uses in shell */
    struct bu_list lu_hd;       /**< @brief wire loopuses (edge groups) */
    struct bu_list eu_hd;       /**< @brief wire list (shell has wires) */
    struct vertexuse *vu_p;     /**< @brief internal ptr to single vertexuse */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * NMG topological region
 */
struct nmgregion {
    struct bu_list l;           /**< @brief regions, in model's r_hd list */
    struct model *m_p;          /**< @brief owning model */
    struct nmgregion_a *ra_p;   /**< @brief attributes */
    struct bu_list s_hd;        /**< @brief list of shells in region */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * NMG topological model
 */
struct model {
    uint32_t magic;
    struct bu_list r_hd;        /**< @brief list of regions */
    char *manifolds;            /**< @brief structure 1-3manifold table */
    long index;                 /**< @brief struct # in this model */
    long maxindex;              /**< @brief # of structs so far */
};

/*****************************************************************************/
/*****************************************************************************/
/* Geometric data containers - referenced by the topological containers to
 * describe geometric information in 3D space. */

/**
 * @brief
 * Point in 3D space.
 *
 * Note that this container is responsible ONLY for geometric information, and
 * has no knowledge of topological relationships.
 */
struct vertex_g {
    uint32_t magic;
    point_t coord;              /**< @brief coordinates of vertex in space */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * Line in 3D space.
 *
 * An edge_g_lseg structure represents a line in 3-space.  All edges on that
 * line should share the same edge_g.
 *
 * IMPORTANT: First two items in edge_g_lseg and edge_g_cnurb (or any other
 * segment type added) must be identical structures, so pointers are puns for
 * both.  eu_hd2 list must be in same place for both.
 */
struct edge_g_lseg {
    struct bu_list l;           /**< @brief NOTICE:  l.forw & l.back *not* stored in database.  For alignment only. */
    struct bu_list eu_hd2;      /**< @brief heads l2 list of edgeuses on this line */
    point_t e_pt;               /**< @brief parametric equation of the line */
    vect_t e_dir;
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * Planar face geometry
 *
 * Note: there will always be exactly two faceuse's using a face.  To
 * find them, go up fu_p for one, then across fumate_p to other.
 */
struct face_g_plane {
    uint32_t magic;
    struct bu_list f_hd;        /**< @brief list of faces sharing this surface */
    plane_t N;                  /**< @brief Plane equation (incl normal) */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * Definition of a knot vector.
 *
 * Not found independently, but used in the cnurb and snurb
 * structures.  (Exactly the same as the definition in nurb.h)
 */
struct knot_vector {
    uint32_t magic;
    int k_size;         /**< @brief knot vector size */
    fastf_t * knots;    /**< @brief pointer to knot vector */
};

/**
 * @brief
 * Edge NURBS curve geometry.
 *
 * The ctl_points on this curve are (u, v) values on the face's
 * surface.  As a storage and performance efficiency measure, if order
 * <= 0, then the cnurb is a straight line segment in parameter space,
 * and the k.knots and ctl_points pointers will be NULL.  In this
 * case, the vertexuse_a_cnurb's at both ends of the edgeuse define
 * the path through parameter space.
 *
 * IMPORTANT: First two items in edge_g_lseg and edge_g_cnurb (or any other
 * segment type added) must be identical structures, so pointers are puns for
 * both.  eu_hd2 list must be in same place for both.
 */
struct edge_g_cnurb {
    struct bu_list l;           /**< @brief NOTICE: l.forw & l.back are NOT stored in database.  For bspline primitive   internal use only. */
    struct bu_list eu_hd2;      /**< @brief heads l2 list of edgeuses on this curve */
    int order;                  /**< @brief Curve Order */
    struct knot_vector k;       /**< @brief curve knot vector */
    /* curve control polygon */
    int c_size;                 /**< @brief number of ctl points */
    int pt_type;                /**< @brief curve point type */
    fastf_t *ctl_points;        /**< @brief array [c_size] */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * Face NURBS surface geometry.
 */
struct face_g_snurb {
    /* NOTICE: l.forw & l.back *not* stored in database.  They are for
     * bspline primitive internal use only.
     */
    struct bu_list l;
    struct bu_list f_hd;        /**< @brief list of faces sharing this surface */
    int order[2];               /**< @brief surface order [0] = u, [1] = v */
    struct knot_vector u;       /**< @brief surface knot vectors */
    struct knot_vector v;       /**< @brief surface knot vectors */
    /* surface control points */
    int s_size[2];              /**< @brief mesh size, u, v */
    int pt_type;                /**< @brief surface point type */
    fastf_t *ctl_points;        /**< @brief array [size[0]*size[1]] */
    /* START OF ITEMS VALID IN-MEMORY ONLY -- NOT STORED ON DISK */
    int dir;                    /**< @brief direction of last refinement */
    point_t min_pt;             /**< @brief min corner of bounding box */
    point_t max_pt;             /**< @brief max corner of bounding box */
    /* END OF ITEMS VALID IN-MEMORY ONLY -- NOT STORED ON DISK */
    long index;                 /**< @brief struct # in this model */
};
/*****************************************************************************/

/*****************************************************************************/
/* Attribute data containers - storing additional information about topological
 * elements over and above their basic geometry definitions.
 *
 * It questionable to me (CY) whether the added complexity of these structures
 * being separate entities is warranted, instead of just including these fields
 * directly in the struct definitions.  Presumably this was originally done to
 * allow applications that didn't need to store these data to avoid the space
 * overhead, but hardware constraints in 2022 are a bit different than in the
 * 1980s... Unless there's another motivation for this separation I've not
 * found yet, we should just fold these into their parent structs to simplify
 * the data management logic.
 *
 * Unfortunately, there is a complication - the serialization logic in librt
 * that writes NMGs out to disk has awareness of these data types.  To remove
 * these cleanly would also mean changing the on-disk storage format of the
 * NMG primitives, or doing some VERY careful analysis to determine if we can
 * change the in-memory containers while still making it possible to retain
 * the existing on-disk serialization.
 */

/**
 * @brief
 * Vertexuse normal
 */
struct vertexuse_a_plane {
    uint32_t magic;
    vect_t N;                   /**< @brief (opt) surface Normal at vertexuse */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * Vertexuse NURBS parameters
 */
struct vertexuse_a_cnurb {
    uint32_t magic;
    fastf_t param[3];           /**< @brief (u, v, w) of vu on eu's cnurb */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * Loop bounding box
 */
struct loop_a {
    uint32_t magic;
    point_t min_pt;             /**< @brief minimums of bounding box */
    point_t max_pt;             /**< @brief maximums of bounding box */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * Shell bounding box
 */
struct shell_a {
    uint32_t magic;
    point_t min_pt;             /**< @brief minimums of bounding box */
    point_t max_pt;             /**< @brief maximums of bounding box */
    long index;                 /**< @brief struct # in this model */
};

/**
 * @brief
 * Region bounding box
 */
struct nmgregion_a {
    uint32_t magic;
    point_t min_pt;             /**< @brief minimums of bounding box */
    point_t max_pt;             /**< @brief maximums of bounding box */
    long index;                 /**< @brief struct # in this model */
};
/*****************************************************************************/



__END_DECLS

#endif  /* NMG_TOPOLOGY_H */
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
