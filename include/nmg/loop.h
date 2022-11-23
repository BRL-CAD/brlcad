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
/** @addtogroup nmg_loop */
/** @{ */
/** @file nmg/loop.h */

#ifndef NMG_LOOP_H
#define NMG_LOOP_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_LOOP(_p)               NMG_CKMAG(_p, NMG_LOOP_MAGIC, "loop")
#define NMG_CK_LOOP_A(_p)             NMG_CKMAG(_p, NMG_LOOP_A_MAGIC, "loop_a")
#define NMG_CK_LOOPUSE(_p)            NMG_CKMAG(_p, NMG_LOOPUSE_MAGIC, "loopuse")

#define GET_LOOP(p, m)              {NMG_GETSTRUCT(p, loop); NMG_INCR_INDEX(p, m);}
#define GET_LOOP_A(p, m)            {NMG_GETSTRUCT(p, loop_a); NMG_INCR_INDEX(p, m);}
#define GET_LOOPUSE(p, m)           {NMG_GETSTRUCT(p, loopuse); NMG_INCR_INDEX(p, m);}
#define GET_LOOPUSE_A(p, m)         {NMG_GETSTRUCT(p, loopuse_a); NMG_INCR_INDEX(p, m);}

#define FREE_LOOP(p)              NMG_FREESTRUCT(p, loop)
#define FREE_LOOP_A(p)            NMG_FREESTRUCT(p, loop_a)
#define FREE_LOOPUSE(p)           NMG_FREESTRUCT(p, loopuse)
#define FREE_LOOPUSE_A(p)         NMG_FREESTRUCT(p, loopuse_a)

/**
 * @brief Build the bounding box for a loop.
 *
 * The bounding box is guaranteed never to have zero thickness.
 *
 * XXX This really isn't loop geometry, this is a loop attribute.  This routine
 * really should be called nmg_loop_bb(), unless it gets something more to do.
 */
NMG_EXPORT extern void nmg_loop_a(struct loop *l,
                                  const struct bn_tol *tol);

/**
 * @brief Demote a loopuse of edgeuses to a bunch of wire edges in the shell.
 *
 * @retval 0 If all is well (edges moved to shell, loopuse deleted).
 * @retval 1 If parent is empty, and is thus "illegal".  Still successful.
 */
NMG_EXPORT extern int nmg_demote_lu(struct loopuse *lu);

/**
 * @brief Make a new loop (with specified orientation) and vertex, in a shell
 * or face.
 * XXX - vertex or vertexuse? or both? ctj
 *
 * If the vertex 'v' is NULL, the shell's lone vertex is used, or a new vertex
 * is created.
 *
 * "magic" must point to the magic number of a faceuse or shell.
 *
 * If the shell has a lone vertex in it, that lone vertex *will* be used.  If a
 * non-NULL 'v' is provided, the lone vertex and 'v' will be fused together.
 * XXX Why is this good?
 *
 * If a convenient shell does not exist, use s=nmg_msv() to make the shell and
 * vertex, then call lu=nmg_mlv(s, s->vu_p->v_p, OT_SAME), followed by
 * nmg_kvu(s->vu_p).
 *
 * Implicit returns -
 * The new vertexuse can be had by:
 * BU_LIST_FIRST(vertexuse, &lu->down_hd);
 *
 * In case the returned loopuse isn't retained, the new loopuse was
 * inserted at the +head+ of the appropriate list, e.g.:
 * lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
 * or
 * lu = BU_LIST_FIRST(loopuse, &s->lu_hd);
 *
 * N.B.  This function is made more complex than warranted by using the "hack"
 * of stealing a vertexuse structure from the shell if at all possible.  A
 * future enhancement to this function would be to remove the vertexuse steal
 * and have the caller pass in the vertex from the shell followed by a call to
 * nmg_kvu(s->vu_p).  The v==NULL convention is used only in nmg_mod.c.
 */
NMG_EXPORT extern struct loopuse *nmg_mlv(uint32_t *magic,
                                          struct vertex *v,
                                          int orientation);

/**
 * @brief Kill loopuse, loopuse mate, and loop.
 *
 * if the loop contains any edgeuses or vertexuses they are killed
 * before the loop is deleted.
 *
 * We support the concept of killing a loop with no children to
 * support the routine "nmg_demote_lu"
 *
 * @retval 0 If all is well
 * @retval 1 If parent is empty, and is thus "illegal"
 */
NMG_EXPORT extern int nmg_klu(struct loopuse *lu1);

/**
 * @brief Join two loops together which share a common edge, such that both
 * occurrences of the common edge are deleted.
 *
 * This routine always leaves "lu" intact, and kills the loop radial to "eu"
 * (after stealing all its edges).
 *
 * Either both loops must be of the same orientation, or then first
 * loop must be OT_SAME, and the second loop must be OT_OPPOSITE.
 * Joining OT_SAME & OT_OPPOSITE always gives an OT_SAME result.
 * Above statement is not true!!!! I have added nmg_lu_reorient() -JRA
 * Since "lu" must survive, it must be the OT_SAME one.
 */
NMG_EXPORT extern void nmg_jl(struct loopuse *lu,
                              struct edgeuse *eu);

/**
 * @brief Intended to join an interior and exterior loop together, by
 * building a bridge between the two indicated vertices.
 *
 * This routine can be used to join two exterior loops which do not
 * overlap, and it can also be used to join an exterior loop with a
 * loop of opposite orientation that lies entirely within it.  This
 * restriction is important, but not checked for.
 *
 * If the two vertexuses reference distinct vertices, then two new
 * edges are built to bridge the loops together.  If the two
 * vertexuses share the same vertex, then it is even easier.
 *
 * Returns the replacement for vu2.
 */
NMG_EXPORT extern struct vertexuse *nmg_join_2loops(struct vertexuse *vu1,
                                                    struct vertexuse *vu2);

/**
 * @brief vu1 is in a regular loop, vu2 is in a loop of a single vertex A
 * jaunt is taken from vu1 to vu2 and back to vu1, and the old loop at
 * vu2 is destroyed.
 *
 * Return is the new vu that replaces vu2.
 */
NMG_EXPORT extern struct vertexuse *nmg_join_singvu_loop(struct vertexuse *vu1,
                                                         struct vertexuse *vu2);

/**
 * @brief Both vertices are part of single vertex loops.  Converts loop on
 * vu1 into a real loop that connects them together, with a single
 * edge (two edgeuses).  Loop on vu2 is killed.
 *
 * Returns replacement vu for vu2.
 * Does not change the orientation.
 */
NMG_EXPORT extern struct vertexuse *nmg_join_2singvu_loops(struct vertexuse *vu1,
                                                           struct vertexuse *vu2);

/**
 * @brief Divide a loop of edges between two vertexuses.
 *
 * Make a new loop between the two vertexes, and split it and the loop of the
 * vertexuses at the same time.
 *
 * @verbatim
                BEFORE                                  AFTER


       Va    eu1  vu1           Vb             Va   eu1  vu1             Vb
        * <---------* <---------*               * <--------*  * <--------*
        |                                       |             |
        |                       ^               |          ^  |          ^
        |        Original       |               | Original |  |    New   |
        |          Loopuse      |               | Loopuse  |  |  Loopuse |
        V                       |               V          |  V /        |
                                |                          |   /         |
        *----------> *--------> *               *--------> *  *--------> *
       Vd            vu2 eu2    Vc             Vd             vu2  eu2   Vc
   @endverbatim
 *
 * Returns the new loopuse pointer.  The new loopuse will contain "vu2" and the
 * edgeuse associated with "vu2" as the FIRST edgeuse on the list of edgeuses.
 * The edgeuse for the new edge (connecting the vertices indicated by vu1 and
 * vu2) will be the LAST edgeuse on the new loopuse's list of edgeuses.
 *
 * It is the caller's responsibility to re-bound the loops.
 *
 * Both old and new loopuse will have orientation OT_UNSPEC.  It is the callers
 * responsibility to determine what the orientations should be.  This can be
 * conveniently done with nmg_lu_reorient().
 *
 * Here is a simple example of how the new loopuse might have a different
 * orientation than the original one:
 *
 * @verbatim
                F<----------------E
                |                 ^
                |                 |
                |      C--------->D
                |      ^          .
                |      |          .
                |      |          .
                |      B<---------A
                |                 ^
                v                 |
                G---------------->H
   @endverbatim
 *
 * When nmg_cut_loop(A, D) is called, the new loop ABCD is clockwise, even
 * though the original loop was counter-clockwise.  There is no way to
 * determine this without referring to the face normal and vertex geometry,
 * which being a topology routine this routine shouldn't do.
 *
 * @retval NULL on error
 * @retval lu on success, loopuse of new loop.
 */
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

NMG_EXPORT extern struct vertexuse *nmg_find_pnt_in_lu(const struct loopuse *lu,
                                                      const point_t pt,
                                                      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_is_vertex_in_looplist(const struct vertex *v,
                                                const struct bu_list *hd,
                                                int singletons);

NMG_EXPORT extern int nmg_is_edge_in_looplist(const struct edge *e,
                                              const struct bu_list *hd);

NMG_EXPORT extern struct vertexuse *nmg_find_vertex_in_lu(const struct vertex *v, const struct loopuse *lu);
NMG_EXPORT extern void nmg_fix_overlapping_loops(struct shell *s, struct bu_list *vlfree, const struct bn_tol *tol);
NMG_EXPORT extern void nmg_break_crossed_loops(struct shell *is, const struct bn_tol *tol);

NMG_EXPORT extern void nmg_loop_plane_newell(const struct loopuse *lu,
                                             plane_t pl);
NMG_EXPORT extern fastf_t nmg_loop_plane_area(const struct loopuse *lu,
                                              plane_t pl);
NMG_EXPORT extern fastf_t nmg_loop_plane_area2(const struct loopuse *lu,
                                               plane_t pl,
                                               const struct bn_tol *tol);
NMG_EXPORT extern fastf_t nmg_loop_plane_area(const struct loopuse *lu,
                                              plane_t pl);
NMG_EXPORT extern int nmg_lu_is_convex(struct loopuse *lu,
                                       struct bu_list *vlfree,
                                       const struct bn_tol *tol);
NMG_EXPORT extern int nmg_classify_pnt_loop(const point_t pt,
                                           const struct loopuse *lu,
                                           struct bu_list *vlfree,
                                           const struct bn_tol *tol);

NMG_EXPORT extern int nmg_classify_lu_lu(const struct loopuse *lu1,
                                         const struct loopuse *lu2,
                                         struct bu_list *vlfree,
                                         const struct bn_tol *tol);
NMG_EXPORT extern int nmg_class_pnt_lu_except(point_t             pt,
                                             const struct loopuse        *lu,
                                             const struct edge           *e_p,
                                             struct bu_list *vlfree,
                                             const struct bn_tol *tol);
NMG_EXPORT extern void nmg_ck_lu_orientation(struct loopuse *lu,
                                             const struct bn_tol *tolp);


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
