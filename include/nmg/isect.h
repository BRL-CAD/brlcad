/*                        I S E C T . H
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
/** @addtogroup nmg_isect
 */
/** @{ */
/** @file nmg/isect.h */

#ifndef NMG_ISECT_H
#define NMG_ISECT_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "bn/tol.h"
#include "nmg/defines.h"

__BEGIN_DECLS

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


__END_DECLS

#endif  /* NMG_ISECT_H */
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
