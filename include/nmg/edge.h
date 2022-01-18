/*                       E D G E . H
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
/** @addtogroup nmg
 * @brief
 * NMG edge definitions
 */
/** @{ */
/** @file nmg/edge.h */

#ifndef NMG_EDGE_H
#define NMG_EDGE_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"

__BEGIN_DECLS

#define NMG_CK_EDGE(_p)               NMG_CKMAG(_p, NMG_EDGE_MAGIC, "edge")
#define NMG_CK_EDGE_G_LSEG(_p)        NMG_CKMAG(_p, NMG_EDGE_G_LSEG_MAGIC, "edge_g_lseg")
#define NMG_CK_EDGEUSE(_p)            NMG_CKMAG(_p, NMG_EDGEUSE_MAGIC, "edgeuse")

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
    struct edgeuse *eu_p;       /**< @brief Ptr to one use of this edge */
    long is_real;               /**< @brief artifact or modeled edge (from tessellator) */
    long index;                 /**< @brief struct # in this model */
};

/**
 * IMPORTANT: First two items in edge_g_lseg and edge_g_cnurb must be
 * identical structure, so pointers are puns for both.  eu_hd2 list
 * must be in same place for both.
 */
struct edge_g_lseg {
    struct bu_list l;           /**< @brief NOTICE:  l.forw & l.back *not* stored in database.  For alignment only. */
    struct bu_list eu_hd2;      /**< @brief heads l2 list of edgeuses on this line */
    point_t e_pt;               /**< @brief parametric equation of the line */
    vect_t e_dir;
    long index;                 /**< @brief struct # in this model */
};

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

#define GET_EDGE(p, m)              {NMG_GETSTRUCT(p, edge); NMG_INCR_INDEX(p, m);}
#define GET_EDGE_G_LSEG(p, m)       {NMG_GETSTRUCT(p, edge_g_lseg); NMG_INCR_INDEX(p, m);}
#define GET_EDGE_G_CNURB(p, m)      {NMG_GETSTRUCT(p, edge_g_cnurb); NMG_INCR_INDEX(p, m);}
#define GET_EDGEUSE(p, m)           {NMG_GETSTRUCT(p, edgeuse); NMG_INCR_INDEX(p, m);}

#define FREE_EDGE(p)              NMG_FREESTRUCT(p, edge)
#define FREE_EDGE_G_LSEG(p)       NMG_FREESTRUCT(p, edge_g_lseg)
#define FREE_EDGE_G_CNURB(p)      NMG_FREESTRUCT(p, edge_g_cnurb)
#define FREE_EDGEUSE(p)           NMG_FREESTRUCT(p, edgeuse)

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


__END_DECLS

#endif  /* NMG_EDGE_H */
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
