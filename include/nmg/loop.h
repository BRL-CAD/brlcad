/*                       L O O P . H
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
 * NMG loop definitions
 */
/** @{ */
/** @file nmg/loop.h */

#ifndef NMG_LOOP_H
#define NMG_LOOP_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"

__BEGIN_DECLS

#define NMG_CK_LOOP(_p)               NMG_CKMAG(_p, NMG_LOOP_MAGIC, "loop")
#define NMG_CK_LOOP_G(_p)             NMG_CKMAG(_p, NMG_LOOP_G_MAGIC, "loop_g")
#define NMG_CK_LOOPUSE(_p)            NMG_CKMAG(_p, NMG_LOOPUSE_MAGIC, "loopuse")

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
    struct loopuse *lu_p;       /**< @brief Ptr to one use of this loop */
    struct loop_g *lg_p;        /**< @brief Geometry */
    long index;                 /**< @brief struct # in this model */
};

struct loop_g {
    uint32_t magic;
    point_t min_pt;             /**< @brief minimums of bounding box */
    point_t max_pt;             /**< @brief maximums of bounding box */
    long index;                 /**< @brief struct # in this model */
};

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

#define GET_LOOP(p, m)              {NMG_GETSTRUCT(p, loop); NMG_INCR_INDEX(p, m);}
#define GET_LOOP_G(p, m)            {NMG_GETSTRUCT(p, loop_g); NMG_INCR_INDEX(p, m);}
#define GET_LOOPUSE(p, m)           {NMG_GETSTRUCT(p, loopuse); NMG_INCR_INDEX(p, m);}
#define GET_LOOPUSE_A(p, m)         {NMG_GETSTRUCT(p, loopuse_a); NMG_INCR_INDEX(p, m);}

#define FREE_LOOP(p)              NMG_FREESTRUCT(p, loop)
#define FREE_LOOP_G(p)            NMG_FREESTRUCT(p, loop_g)
#define FREE_LOOPUSE(p)           NMG_FREESTRUCT(p, loopuse)
#define FREE_LOOPUSE_A(p)         NMG_FREESTRUCT(p, loopuse_a)

/* From file nmg_mk.c */
/*      MAKE routines */
NMG_EXPORT extern struct loopuse *nmg_mlv(uint32_t *magic,
                                          struct vertex *v,
                                          int orientation);
NMG_EXPORT extern int nmg_klu(struct loopuse *lu1);


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

NMG_EXPORT extern struct loopuse*nmg_find_lu_of_vu(const struct vertexuse *vu);
NMG_EXPORT extern int nmg_loop_is_a_crack(const struct loopuse *lu);
NMG_EXPORT extern int    nmg_loop_is_ccw(const struct loopuse *lu,
                                         const vect_t norm,
                                         const struct bn_tol *tol);
NMG_EXPORT extern const struct vertexuse *nmg_loop_touches_self(const struct loopuse *lu);
NMG_EXPORT extern int nmg_2lu_identical(const struct edgeuse *eu1,
                                        const struct edgeuse *eu2);


__END_DECLS

#endif  /* NMG_LOOP_H */
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
