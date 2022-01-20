/*                       S H E L L . H
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
 * NMG shell definitions
 */
/** @{ */
/** @file nmg/shell.h */

#ifndef NMG_SHELL_H
#define NMG_SHELL_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_SHELL(_p)              NMG_CKMAG(_p, NMG_SHELL_MAGIC, "shell")
#define NMG_CK_SHELL_A(_p)            NMG_CKMAG(_p, NMG_SHELL_A_MAGIC, "shell_a")

#define GET_SHELL(p, m)             {NMG_GETSTRUCT(p, shell); NMG_INCR_INDEX(p, m);}
#define GET_SHELL_A(p, m)           {NMG_GETSTRUCT(p, shell_a); NMG_INCR_INDEX(p, m);}

#define FREE_SHELL(p)             NMG_FREESTRUCT(p, shell)
#define FREE_SHELL_A(p)           NMG_FREESTRUCT(p, shell_a)

NMG_EXPORT extern void nmg_shell_a(struct shell *s,
                                   const struct bn_tol *tol);

/* From file nmg_mk.c */
/*      MAKE routines */
NMG_EXPORT extern struct loopuse *nmg_ml(struct shell *s);
NMG_EXPORT extern int nmg_keu_zl(struct shell *s,
                                 const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ks(struct shell *s);

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

NMG_EXPORT extern int nmg_shell_is_empty(const struct shell *s);
NMG_EXPORT extern struct shell *nmg_find_s_of_lu(const struct loopuse *lu);
NMG_EXPORT extern struct shell *nmg_find_s_of_eu(const struct edgeuse *eu);
NMG_EXPORT extern struct shell *nmg_find_s_of_vu(const struct vertexuse *vu);


NMG_EXPORT extern struct shell *nmg_extrude_cleanup(struct shell *is, const int is_void, struct bu_list *vlfree, const struct bn_tol *tol);
NMG_EXPORT extern void nmg_hollow_shell(struct shell *s, const fastf_t thick, const int approximate, struct bu_list *vlfree, const struct bn_tol *tol);
NMG_EXPORT extern struct shell *nmg_extrude_shell(struct shell *s, const fastf_t dist, const int normal_ward, const int approximate, struct bu_list *vlfree, const struct bn_tol *tol);

NMG_EXPORT extern void nmg_close_shell(struct shell *s, struct bu_list *vlfree,
                                       const struct bn_tol *tol);

NMG_EXPORT extern struct shell *nmg_dup_shell(struct shell *s,
                                              long ***copy_tbl,
                                              struct bu_list *vlfree,
                                              const struct bn_tol *tol);
NMG_EXPORT extern void nmg_glue_face_in_shell(const struct faceuse *fu,
                                              struct shell *s,
                                              const struct bn_tol *tol);
NMG_EXPORT extern int nmg_open_shells_connect(struct shell *dst,
                                              struct shell *src,
                                              const long **copy_tbl,
                                              struct bu_list *vlfree,
                                              const struct bn_tol *tol);
NMG_EXPORT extern int nmg_simplify_shell_edges(struct shell *s,
                                               const struct bn_tol *tol);
NMG_EXPORT extern char *nmg_shell_manifolds(struct shell *sp,
                                            char *tbl);

NMG_EXPORT extern void nmg_mv_vu_between_shells(struct shell *dest,
                                                struct shell *src,
                                                struct vertexuse *vu);
NMG_EXPORT extern struct shell *nmg_find_shell(const uint32_t *magic_p);

NMG_EXPORT extern struct vertexuse *nmg_find_v_in_shell(const struct vertex *v,
                                                        const struct shell *s,
                                                        int edges_only);
NMG_EXPORT extern struct vertex *nmg_find_pnt_in_shell(const struct shell *s,
                                                      const point_t pt,
                                                      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_is_vertex_a_selfloop_in_shell(const struct vertex *v,
                                                        const struct shell *s);
NMG_EXPORT extern struct shell *nmg_extrude_cleanup(struct shell *is, const int is_void, struct bu_list *vlfree, const struct bn_tol *tol);
NMG_EXPORT extern void nmg_hollow_shell(struct shell *s, const fastf_t thick, const int approximate, struct bu_list *vlfree, const struct bn_tol *tol);
NMG_EXPORT extern struct shell *nmg_extrude_shell(struct shell *s, const fastf_t dist, const int normal_ward, const int approximate, struct bu_list *vlfree, const struct bn_tol *tol);

NMG_EXPORT extern struct face *nmg_find_top_face_in_dir(const struct shell *s,
                                                        int dir, long *flags);
NMG_EXPORT extern struct face *nmg_find_top_face(const struct shell *s,
                                                 int *dir,
                                                 long *flags);
NMG_EXPORT extern int nmg_find_outer_and_void_shells(struct nmgregion *r,
                                                     struct bu_ptbl ***shells,
                                                     struct bu_list *vlfree,
                                                     const struct bn_tol *tol);
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
NMG_EXPORT extern fastf_t nmg_shell_area(const struct shell *s);
NMG_EXPORT extern int nmg_shell_is_void(const struct shell *s);
NMG_EXPORT extern void nmg_connect_same_fu_orients(struct shell *s);
NMG_EXPORT extern void nmg_fix_decomposed_shell_normals(struct shell *s,
                                                        const struct bn_tol *tol);
NMG_EXPORT extern void nmg_fix_normals(struct shell *s_orig,
                                       struct bu_list *vlfree,
                                       const struct bn_tol *tol);
NMG_EXPORT extern int nmg_break_long_edges(struct shell *s,
                                           const struct bn_tol *tol);
NMG_EXPORT extern int nmg_decompose_shell(struct shell *s, struct bu_list *vlfree,
                                          const struct bn_tol *tol);
NMG_EXPORT extern void nmg_vlist_to_eu(struct bu_list *vlist,
                                       struct shell *s);
NMG_EXPORT extern int nmg_mv_shell_to_region(struct shell *s,
                                             struct nmgregion *r);
NMG_EXPORT extern int nmg_bad_face_normals(const struct shell *s,
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
NMG_EXPORT extern int nmg_kill_cracks(struct shell *s);
NMG_EXPORT extern void nmg_make_faces_within_tol(struct shell *s,
                                                 struct bu_list *vlfree,
                                                 const struct bn_tol *tol);
NMG_EXPORT extern void nmg_intersect_loops_self(struct shell *s,
                                                const struct bn_tol *tol);
NMG_EXPORT extern void nmg_triangulate_shell(struct shell *s,
                                             struct bu_list *vlfree,
                                             const struct bn_tol  *tol);
NMG_EXPORT extern int nmg_classify_s_vs_s(struct shell *s,
                                          struct shell *s2,
                                          struct bu_list *vlfree,
                                          const struct bn_tol *tol);
NMG_EXPORT extern int nmg_class_pnt_s(const point_t pt,
                                     const struct shell *s,
                                     const int in_or_out_only,
                                     struct bu_list *vlfree,
                                     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_mesh_face_shell(struct faceuse *fu1,
                                          struct shell *s,
                                          const struct bn_tol *tol);
NMG_EXPORT extern int nmg_mesh_shell_shell(struct shell *s1,
                                           struct shell *s2,
                                           struct bu_list *vlfree,
                                           const struct bn_tol *tol);
NMG_EXPORT extern void nmg_class_shells(struct shell *sA,
                                        struct shell *sB,
                                        char **classlist,
                                        struct bu_list *vlfree,
                                        const struct bn_tol *tol);
NMG_EXPORT extern void nmg_evaluate_boolean(struct shell *sA,
                                            struct shell *sB,
                                            int          op,
                                            char         **classlist,
                                            struct bu_list *vlfree,
                                            const struct bn_tol  *tol);

__END_DECLS

#endif  /* NMG_SHELL_H */
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
