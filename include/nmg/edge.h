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
/** @addtogroup nmg_edge */
/** @{ */
/** @file nmg/edge.h */

#ifndef NMG_EDGE_H
#define NMG_EDGE_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_EDGE(_p)               NMG_CKMAG(_p, NMG_EDGE_MAGIC, "edge")
#define NMG_CK_EDGE_G_LSEG(_p)        NMG_CKMAG(_p, NMG_EDGE_G_LSEG_MAGIC, "edge_g_lseg")
#define NMG_CK_EDGEUSE(_p)            NMG_CKMAG(_p, NMG_EDGEUSE_MAGIC, "edgeuse")

#define GET_EDGE(p, m)              {NMG_GETSTRUCT(p, edge); NMG_INCR_INDEX(p, m);}
#define GET_EDGE_G_LSEG(p, m)       {NMG_GETSTRUCT(p, edge_g_lseg); NMG_INCR_INDEX(p, m);}
#define GET_EDGE_G_CNURB(p, m)      {NMG_GETSTRUCT(p, edge_g_cnurb); NMG_INCR_INDEX(p, m);}
#define GET_EDGEUSE(p, m)           {NMG_GETSTRUCT(p, edgeuse); NMG_INCR_INDEX(p, m);}

#define FREE_EDGE(p)              NMG_FREESTRUCT(p, edge)
#define FREE_EDGE_G_LSEG(p)       NMG_FREESTRUCT(p, edge_g_lseg)
#define FREE_EDGE_G_CNURB(p)      NMG_FREESTRUCT(p, edge_g_cnurb)
#define FREE_EDGEUSE(p)           NMG_FREESTRUCT(p, edgeuse)

/**
 * @brief Compute the equation of the line formed by the endpoints of the
 * edge.
 */
NMG_EXPORT extern void nmg_edge_g(struct edgeuse *eu);

/**
 * @brief Associate edgeuse 'eu' with the edge geometry structure identified by
 * 'magic_p', where magic_p is the pointer to the magic entry of an edge_g_lseg.
 *
 * Example magic_p keys:  eu->g.magic_p  &eg->l.magic
 *
 * If the edgeuse is already associated with some geometry, that geometry is
 * first released.  Note that, to start with, the two edgeuses may be using
 * different original geometries.
 *
 * Also do the edgeuse mate.
 *
 * @retval 0 If the old edge geometry (eu->g.magic_p) has other uses.
 * @retval 1 If the old edge geometry has been destroyed. Caller beware!
 */
NMG_EXPORT extern int nmg_use_edge_g(struct edgeuse *eu,
                                     uint32_t *magic_p);

/**
 * @brief Demote a wire edge into a pair of self-loop vertices
 *
 * @retval 0 If all is well
 * @retval 1 If shell is empty, and is thus "illegal".
 */
NMG_EXPORT extern int nmg_demote_eu(struct edgeuse *eu);

/**
 * @brief Move a pair of edgeuses onto a single edge (glue edgeuse).
 *
 * The edgeuse eusrc and its mate are moved to the edge used by eudst.  eusrc
 * is made to be immediately radial to eudst.  If eusrc does not share the same
 * vertices as eudst, we bomb.
 *
 * The edgeuse geometry pointers are not changed by this operation.
 *
 * This routine was formerly called nmg_moveeu().
 */
NMG_EXPORT extern void nmg_je(struct edgeuse *eudst,
                              struct edgeuse *eusrc);

/**
 * @brief If edgeuse is part of a shared edge (more than one pair of edgeuses
 * on the edge), it and its mate are "unglued" from the edge, and associated
 * with a new edge structure.
 *
 * Primarily a support routine for nmg_eusplit()
 *
 * If the original edge had edge geometry, that is *not* duplicated here,
 * because it is not easy (or appropriate) for nmg_eusplit() to know whether
 * the new vertex lies on the previous edge geometry or not.  Hence having the
 * nmg_ebreak() interface, which preserves the ege geometry across a split, and
 * nmg_esplit() which does not.
 */
NMG_EXPORT extern void nmg_unglueedge(struct edgeuse *eu);

/**
 * @brief Join two edge geometries.
 *
 * For all edges in the model which refer to 'src_eg', change them to
 * refer to 'dest_eg'.  The source is destroyed.
 *
 * It is the responsibility of the caller to make certain that the
 * 'dest_eg' is the best one for these edges.  Outrageously wrong
 * requests will cause this routine to abort.
 *
 * This algorithm does not make sense if dest_eg is an edge_g_cnurb;
 * those only make sense in the parameter space of their associated
 * face.
 */
NMG_EXPORT extern void nmg_jeg(struct edge_g_lseg *dest_eg,
                               struct edge_g_lseg *src_eg);

/**
 * @brief Make wire edge.
 *
 * Make a new edge between a pair of vertices in a shell.
 *
 * A new vertex will be made for any NULL vertex pointer parameters.
 * If we need to make a new vertex and the shell still has its
 * vertexuse we re-use that vertex rather than freeing and
 * re-allocating.
 *
 * If both vertices were specified, and the shell also had a vertexuse
 * pointer, the vertexuse in the shell is killed.  XXX Why?
 *
 * The explicit result is an edgeuse in shell "s" whose vertexuse refers to
 * vertex v1.  The edgeuse mate's vertexuse refers to vertex v2
 *
 * Additional possible side effects:
 * -# If the shell had a lone vertex in vu_p, it is destroyed, even if both
 *    vertices were specified.
 * -# The returned edgeuse is the first item on the shell's eu_hd list,
 *    followed immediately by the mate.
 */
NMG_EXPORT extern struct edgeuse *nmg_me(struct vertex *v1,
                                         struct vertex *v2,
                                         struct shell *s);
#define nmg_mev(_v, _u) nmg_me((_v), (struct vertex *)NULL, (_u))

/**
 * @brief Make an edge on vertexuse.
 *
 * The new edge runs from and to that vertex.
 *
 * If the vertexuse was the shell's sole vertexuse, then the new edge
 * is a wire edge in the shell's eu_hd list.
 *
 * If the vertexuse was part of a loop-of-a-single-vertex, either as a
 * loop in a face or as a wire-loop in the shell, the loop becomes a
 * regular loop with one edge that runs from and to the original
 * vertex.
 */
NMG_EXPORT extern struct edgeuse *nmg_meonvu(struct vertexuse *vu);

NMG_EXPORT extern int nmg_keg(struct edgeuse *eu);
NMG_EXPORT extern int nmg_keu(struct edgeuse *eu);

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

NMG_EXPORT extern int nmg_is_vertex_in_edgelist(const struct vertex *v,
                                                const struct bu_list *hd);
NMG_EXPORT extern int nmg_is_edge_in_edgelist(const struct edge *e,
                                              const struct bu_list *hd);
NMG_EXPORT extern void nmg_edgeuse_tabulate(struct bu_ptbl *tab,
                                            const uint32_t *magic_p,
                                            struct bu_list *vlfree);
NMG_EXPORT extern void nmg_edge_tabulate(struct bu_ptbl *tab,
                                         const uint32_t *magic_p,
                                         struct bu_list *vlfree);
NMG_EXPORT extern void nmg_edge_g_tabulate(struct bu_ptbl *tab,
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

NMG_EXPORT extern int nmg_mark_edges_real(const uint32_t *magic_p, struct bu_list *vlfree);
NMG_EXPORT extern struct edgeuse *nmg_pop_eu(struct bu_ptbl *stack);
NMG_EXPORT extern int nmg_move_edge_thru_pnt(struct edgeuse *mv_eu,
                                             const point_t pt,
                                             const struct bn_tol *tol);
NMG_EXPORT extern int nmg_break_edge_at_verts(struct edge *e,
                                              struct bu_ptbl *verts,
                                              const struct bn_tol *tol);
NMG_EXPORT extern int nmg_break_edges(uint32_t *magic_p, struct bu_list *vlfree,
                                      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_edge_fuse(const uint32_t *magic_p,struct bu_list *vlfree,
                                    const struct bn_tol *tol);
NMG_EXPORT extern int nmg_edge_g_fuse(const uint32_t *magic_p,struct bu_list *vlfree,
                                      const struct bn_tol       *tol);
NMG_EXPORT extern struct edge_g_lseg    *nmg_pick_best_edge_g(struct edgeuse *eu1,
                                                              struct edgeuse *eu2,
                                                              const struct bn_tol *tol);
NMG_EXPORT extern int nmg_eu_is_part_of_crack(const struct edgeuse *eu);
NMG_EXPORT extern void nmg_radial_join_eu(struct edgeuse *eu1,
                                          struct edgeuse *eu2,
                                          const struct bn_tol *tol);
NMG_EXPORT extern int nmg_insert_vu_if_on_edge(struct vertexuse *vu1,
                                               struct vertexuse *vu2,
                                               struct edgeuse *new_eu,
                                               struct bn_tol *tol);

NMG_EXPORT extern int nmg_break_all_es_on_v(uint32_t *magic_p,
                                            struct vertex *v,struct bu_list *vlfree,
                                            const struct bn_tol *tol);
NMG_EXPORT extern int nmg_break_e_on_v(const uint32_t *magic_p,struct bu_list *vlfree,
                                       const struct bn_tol *tol);

NMG_EXPORT extern double nmg_vu_angle_measure(struct vertexuse   *vu,
                                              vect_t x_dir,
                                              vect_t y_dir,
                                              int assessment,
                                              int in);
NMG_EXPORT extern int nmg_wedge_class(int        ass,    /* assessment of two edges forming wedge */
                                      double     a,
                                      double     b);

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
