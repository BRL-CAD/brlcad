/*                       F A C E . H
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
 * NMG face definitions
 */
/** @{ */
/** @file nmg/face.h */

#ifndef NMG_FACE_H
#define NMG_FACE_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_FACE(_p)               NMG_CKMAG(_p, NMG_FACE_MAGIC, "face")
#define NMG_CK_FACE_G_PLANE(_p)       NMG_CKMAG(_p, NMG_FACE_G_PLANE_MAGIC, "face_g_plane")
#define NMG_CK_FACE_G_SNURB(_p)       NMG_CKMAG(_p, NMG_FACE_G_SNURB_MAGIC, "face_g_snurb")
#define NMG_CK_FACE_G_EITHER(_p)      NMG_CK2MAG(_p, NMG_FACE_G_PLANE_MAGIC, NMG_FACE_G_SNURB_MAGIC, "face_g_plane|face_g_snurb")
#define NMG_CK_FACEUSE(_p)            NMG_CKMAG(_p, NMG_FACEUSE_MAGIC, "faceuse")

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

#define GET_FACE(p, m)              {NMG_GETSTRUCT(p, face); NMG_INCR_INDEX(p, m);}
#define GET_FACE_G_PLANE(p, m)      {NMG_GETSTRUCT(p, face_g_plane); NMG_INCR_INDEX(p, m);}
#define GET_FACE_G_SNURB(p, m)      {NMG_GETSTRUCT(p, face_g_snurb); NMG_INCR_INDEX(p, m);}
#define GET_FACEUSE(p, m)           {NMG_GETSTRUCT(p, faceuse); NMG_INCR_INDEX(p, m);}

#define FREE_FACE(p)              NMG_FREESTRUCT(p, face)
#define FREE_FACE_G_PLANE(p)      NMG_FREESTRUCT(p, face_g_plane)
#define FREE_FACE_G_SNURB(p)      NMG_FREESTRUCT(p, face_g_snurb)
#define FREE_FACEUSE(p)           NMG_FREESTRUCT(p, faceuse)

NMG_EXPORT extern void nmg_face_g(struct faceuse *fu,
                                  const plane_t p);
NMG_EXPORT extern void nmg_face_new_g(struct faceuse *fu,
                                      const plane_t pl);
NMG_EXPORT extern void nmg_face_bb(struct face *f,
                                   const struct bn_tol *tol);

NMG_EXPORT extern void nmg_jfg(struct face *f1,
                               struct face *f2);

NMG_EXPORT extern struct faceuse *nmg_find_fu_of_eu(const struct edgeuse *eu);
NMG_EXPORT extern struct faceuse *nmg_find_fu_of_lu(const struct loopuse *lu);
NMG_EXPORT extern struct faceuse *nmg_find_fu_of_vu(const struct vertexuse *vu);
NMG_EXPORT extern struct faceuse *nmg_find_fu_with_fg_in_s(const struct shell *s1,
                                                           const struct faceuse *fu2);
NMG_EXPORT extern double nmg_measure_fu_angle(const struct edgeuse *eu,
                                              const vect_t xvec,
                                              const vect_t yvec,
                                              const vect_t zvec);

NMG_EXPORT extern struct faceuse *nmg_mf(struct loopuse *lu1);
NMG_EXPORT extern int nmg_kfu(struct faceuse *fu1);

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


NMG_EXPORT extern struct vertexuse *nmg_find_v_in_face(const struct vertex *,
                                                       const struct faceuse *);

NMG_EXPORT extern struct vertexuse *nmg_find_pnt_in_face(const struct faceuse *fu,
                                                        const point_t pt,
                                                        const struct bn_tol *tol);
NMG_EXPORT extern struct vertexuse *nmg_is_vertex_in_face(const struct vertex *v,
                                                          const struct face *f);
NMG_EXPORT extern int nmg_is_vertex_in_facelist(const struct vertex *v,
                                                const struct bu_list *hd);

NMG_EXPORT extern int nmg_is_edge_in_facelist(const struct edge *e,
                                              const struct bu_list *hd);
NMG_EXPORT extern int nmg_is_loop_in_facelist(const struct loop *l,
                                              const struct bu_list *fu_hd);
NMG_EXPORT extern void nmg_face_tabulate(struct bu_ptbl *tab,
                                         const uint32_t *magic_p,
                                         struct bu_list *vlfree);

NMG_EXPORT extern void nmg_translate_face(struct faceuse *fu, const vect_t Vec, struct bu_list *vlfree);
NMG_EXPORT extern int nmg_extrude_face(struct faceuse *fu, const vect_t Vec, struct bu_list *vlfree, const struct bn_tol *tol);

NMG_EXPORT extern void nmg_tabulate_face_g_verts(struct bu_ptbl *tab,
                                                 const struct face_g_plane *fg);
NMG_EXPORT extern int nmg_move_lu_between_fus(struct faceuse *dest,
                                              struct faceuse *src,
                                              struct loopuse *lu);
NMG_EXPORT extern int nmg_calc_face_plane(struct faceuse *fu_in,
                                          plane_t pl, struct bu_list *vlfree);
NMG_EXPORT extern int nmg_calc_face_g(struct faceuse *fu, struct bu_list *vlfree);
NMG_EXPORT extern fastf_t nmg_faceuse_area(const struct faceuse *fu);
NMG_EXPORT extern void nmg_purge_unwanted_intersection_points(struct bu_ptbl *vert_list,
                                                              fastf_t *mag,
                                                              const struct faceuse *fu,
                                                              const struct bn_tol *tol);
NMG_EXPORT extern void nmg_reverse_radials(struct faceuse *fu,
                                           const struct bn_tol *tol);
NMG_EXPORT extern void nmg_reverse_face_and_radials(struct faceuse *fu,
                                                    const struct bn_tol *tol);
NMG_EXPORT extern void nmg_propagate_normals(struct faceuse *fu_in,
                                             long *flags,
                                             const struct bn_tol *tol);
NMG_EXPORT extern struct faceuse *nmg_mk_new_face_from_loop(struct loopuse *lu);
NMG_EXPORT extern int nmg_split_loops_into_faces(uint32_t *magic_p, struct bu_list *vlfree,
                                                 const struct bn_tol *tol);
NMG_EXPORT extern int nmg_find_isect_faces(const struct vertex *new_v,
                                           struct bu_ptbl *faces,
                                           int *free_edges,
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
NMG_EXPORT extern int nmg_faces_are_radial(const struct faceuse *fu1,
                                           const struct faceuse *fu2);
NMG_EXPORT extern int nmg_triangulate_fu(struct faceuse *fu,
                                         struct bu_list *vlfree,
                                         const struct bn_tol *tol);
NMG_EXPORT extern int nmg_class_pnt_f(const point_t pt,
                                     const struct faceuse *fu,
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
NMG_EXPORT extern int nmg_mesh_two_faces(struct faceuse *fu1,
                                         struct faceuse *fu2,
                                         const struct bn_tol     *tol);
NMG_EXPORT extern void nmg_mesh_faces(struct faceuse *fu1,
                                      struct faceuse *fu2,
                                      struct bu_list *vlfree,
                                      const struct bn_tol *tol);
NMG_EXPORT extern double nmg_measure_fu_angle(const struct edgeuse *eu,
                                              const vect_t xvec,
                                              const vect_t yvec,
                                              const vect_t zvec);

NMG_EXPORT extern int nmg_ck_vu_ptbl(struct bu_ptbl      *p,
                                     struct faceuse      *fu);

NMG_EXPORT extern void nmg_sanitize_fu(struct faceuse    *fu);

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

NMG_EXPORT extern int nmg_ck_vert_on_fus(const struct vertex *v,
                                         const struct bn_tol *tol);

NMG_EXPORT extern int nmg_dangling_face(const struct faceuse *fu,
                                        const char *manifolds);

NMG_EXPORT extern int nmg_is_common_bigloop(const struct face *f1,
                                            const struct face *f2);

NMG_EXPORT extern int nmg_ck_fu_verts(struct faceuse *fu1,
                                      struct face *f2,
                                      const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ck_fg_verts(struct faceuse *fu1,
                                      struct face *f2,
                                      const struct bn_tol *tol);
NMG_EXPORT extern int   nmg_two_face_fuse(struct face   *f1,
                                          struct face *f2,
                                          const struct bn_tol *tol);


__END_DECLS

#endif  /* NMG_FACE_H */
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
