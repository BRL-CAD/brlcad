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

NMG_EXPORT extern void nmg_loop_a(struct loop *l,
                                  const struct bn_tol *tol);
NMG_EXPORT extern int nmg_demote_lu(struct loopuse *lu);

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
