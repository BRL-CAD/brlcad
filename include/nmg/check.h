/*                        C H E C K . H
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
/** @addtogroup nmg_check
 */
/** @{ */
/** @file nmg/check.h */

#ifndef NMG_CHECK_H
#define NMG_CHECK_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "bn/tol.h"
#include "nmg/defines.h"
#include "nmg/model.h"

__BEGIN_DECLS

/* From nmg_ck.c */
NMG_EXPORT extern void nmg_vvg(const struct vertex_g *vg);
NMG_EXPORT extern void nmg_vvertex(const struct vertex *v,
                                   const struct vertexuse *vup);
NMG_EXPORT extern void nmg_vvua(const uint32_t *vua);
NMG_EXPORT extern void nmg_vvu(const struct vertexuse *vu,
                               const uint32_t *up_magic_p);
NMG_EXPORT extern void nmg_veg(const uint32_t *eg);
NMG_EXPORT extern void nmg_vedge(const struct edge *e,
                                 const struct edgeuse *eup);
NMG_EXPORT extern void nmg_veu(const struct bu_list      *hp,
                               const uint32_t *up_magic_p);
NMG_EXPORT extern void nmg_vlg(const struct loop_a *lg);
NMG_EXPORT extern void nmg_vloop(const struct loop *l,
                                 const struct loopuse *lup);
NMG_EXPORT extern void nmg_vlu(const struct bu_list      *hp,
                               const uint32_t *up);
NMG_EXPORT extern void nmg_vfg(const struct face_g_plane *fg);
NMG_EXPORT extern void nmg_vface(const struct face *f,
                                 const struct faceuse *fup);
NMG_EXPORT extern void nmg_vfu(const struct bu_list      *hp,
                               const struct shell *s);
NMG_EXPORT extern void nmg_vsshell(const struct shell *s,
                                   const struct nmgregion *r);
NMG_EXPORT extern void nmg_vshell(const struct bu_list *hp,
                                  const struct nmgregion *r);
NMG_EXPORT extern void nmg_vregion(const struct bu_list *hp,
                                   const struct model *m);
NMG_EXPORT extern void nmg_vmodel(const struct model *m);

/* checking routines */
NMG_EXPORT extern void nmg_ck_e(const struct edgeuse *eu,
                                const struct edge *e,
                                const char *str);
NMG_EXPORT extern void nmg_ck_vu(const uint32_t *parent,
                                 const struct vertexuse *vu,
                                 const char *str);
NMG_EXPORT extern void nmg_ck_eu(const uint32_t *parent,
                                 const struct edgeuse *eu,
                                 const char *str);
NMG_EXPORT extern void nmg_ck_lg(const struct loop *l,
                                 const struct loop_a *lg,
                                 const char *str);
NMG_EXPORT extern void nmg_ck_l(const struct loopuse *lu,
                                const struct loop *l,
                                const char *str);
NMG_EXPORT extern void nmg_ck_lu(const uint32_t *parent,
                                 const struct loopuse *lu,
                                 const char *str);
NMG_EXPORT extern void nmg_ck_fg(const struct face *f,
                                 const struct face_g_plane *fg,
                                 const char *str);
NMG_EXPORT extern void nmg_ck_f(const struct faceuse *fu,
                                const struct face *f,
                                const char *str);
NMG_EXPORT extern void nmg_ck_fu(const struct shell *s,
                                 const struct faceuse *fu,
                                 const char *str);
NMG_EXPORT extern int nmg_ck_eg_verts(const struct edge_g_lseg *eg,
                                      const struct bn_tol *tol);
NMG_EXPORT extern size_t nmg_ck_geometry(const struct model *m,
                                         struct bu_list *vlfree,
                                         const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ck_face_worthless_edges(const struct faceuse *fu);
NMG_EXPORT extern void nmg_ck_lueu(const struct loopuse *lu, const char *s);
NMG_EXPORT extern int nmg_check_radial(const struct edgeuse *eu, const struct bn_tol *tol);
NMG_EXPORT extern int nmg_eu_2s_orient_bad(const struct edgeuse  *eu,
                                           const struct shell    *s1,
                                           const struct shell    *s2,
                                           const struct bn_tol   *tol);
NMG_EXPORT extern int nmg_ck_closed_surf(const struct shell *s,
                                         const struct bn_tol *tol);
NMG_EXPORT extern int nmg_ck_closed_region(const struct nmgregion *r,
                                           const struct bn_tol *tol);
NMG_EXPORT extern void nmg_ck_v_in_2fus(const struct vertex *vp,
                                        const struct faceuse *fu1,
                                        const struct faceuse *fu2,
                                        const struct bn_tol *tol);
NMG_EXPORT extern void nmg_ck_vs_in_region(const struct nmgregion *r,
                                           struct bu_list *vlfree,
                                           const struct bn_tol *tol);


__END_DECLS

#endif  /* NMG_CHECK_H */
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
