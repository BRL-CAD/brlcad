/*                       V E R T E X . H
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
 * NMG vertex definitions
 */
/** @{ */
/** @file nmg/vertex.h */

#ifndef NMG_VERTEX_H
#define NMG_VERTEX_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"

__BEGIN_DECLS

#define NMG_CK_VERTEX(_p)             NMG_CKMAG(_p, NMG_VERTEX_MAGIC, "vertex")
#define NMG_CK_VERTEX_G(_p)           NMG_CKMAG(_p, NMG_VERTEX_G_MAGIC, "vertex_g")
#define NMG_CK_VERTEXUSE(_p)          NMG_CKMAG(_p, NMG_VERTEXUSE_MAGIC, "vertexuse")
#define NMG_CK_VERTEXUSE_A_PLANE(_p)  NMG_CKMAG(_p, NMG_VERTEXUSE_A_PLANE_MAGIC, "vertexuse_a_plane")

/**
 * The vertex and vertexuse structures are connected in a way
 * different from the superior kinds of topology elements.  The vertex
 * structure heads a linked list that all vertexuse's that use the
 * vertex are linked onto.
 */
struct vertex {
    uint32_t magic;
    struct bu_list vu_hd;       /**< @brief heads list of vu's of this vertex */
    struct vertex_g *vg_p;      /**< @brief geometry */
    long index;                 /**< @brief struct # in this model */
};

struct vertex_g {
    uint32_t magic;
    point_t coord;              /**< @brief coordinates of vertex in space */
    long index;                 /**< @brief struct # in this model */
};

struct vertexuse_a_plane;
struct vertexuse_a_cnurb;
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

#define GET_VERTEX(p, m)            {NMG_GETSTRUCT(p, vertex); NMG_INCR_INDEX(p, m);}
#define GET_VERTEX_G(p, m)          {NMG_GETSTRUCT(p, vertex_g); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE(p, m)         {NMG_GETSTRUCT(p, vertexuse); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE_A_PLANE(p, m) {NMG_GETSTRUCT(p, vertexuse_a_plane); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE_A_CNURB(p, m) {NMG_GETSTRUCT(p, vertexuse_a_cnurb); NMG_INCR_INDEX(p, m);}
#define FREE_VERTEX(p)            NMG_FREESTRUCT(p, vertex)
#define FREE_VERTEX_G(p)          NMG_FREESTRUCT(p, vertex_g)
#define FREE_VERTEXUSE(p)         NMG_FREESTRUCT(p, vertexuse)
#define FREE_VERTEXUSE_A_PLANE(p) NMG_FREESTRUCT(p, vertexuse_a_plane)
#define FREE_VERTEXUSE_A_CNURB(p) NMG_FREESTRUCT(p, vertexuse_a_cnurb)

#define PREEXIST 1
#define NEWEXIST 2


#define VU_PREEXISTS(_bs, _vu) { chkidxlist((_bs), (_vu)); \
        (_bs)->vertlist[(_vu)->index] = PREEXIST; }

#define VU_NEW(_bs, _vu) { chkidxlist((_bs), (_vu)); \
        (_bs)->vertlist[(_vu)->index] = NEWEXIST; }


__END_DECLS

#endif  /* NMG_VERTEX_H */
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
