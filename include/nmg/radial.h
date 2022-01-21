/*                       R A D I A L . H
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
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
/** @addtogroup libnmg
 * @brief
 * NMG radial definitions
 */
/** @{ */
/** @file nmg/radial.h */

#ifndef NMG_RADIAL_H
#define NMG_RADIAL_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/model.h"

__BEGIN_DECLS

struct nmg_radial {
    struct bu_list      l;
    struct edgeuse      *eu;
    struct faceuse      *fu;            /**< @brief  Derived from eu */
    struct shell        *s;             /**< @brief  Derived from eu */
    int                 existing_flag;  /**< @brief  !0 if this eu exists on dest edge */
    int                 is_crack;       /**< @brief  This eu is part of a crack. */
    int                 is_outie;       /**< @brief  This crack is an "outie" */
    int                 needs_flip;     /**< @brief  Insert eumate, not eu */
    fastf_t             ang;            /**< @brief  angle, in radians.  0 to 2pi */
};

NMG_EXPORT extern void nmg_radial_sorted_list_insert(struct bu_list *hd,
                                                     struct nmg_radial *rad);
NMG_EXPORT extern void nmg_radial_verify_pointers(const struct bu_list *hd,
                                                  const struct bn_tol *tol);
NMG_EXPORT extern void nmg_radial_verify_monotone(const struct bu_list  *hd,
                                                  const struct bn_tol   *tol);
NMG_EXPORT extern void nmg_insure_radial_list_is_increasing(struct bu_list      *hd,
                                                            fastf_t amin, fastf_t amax);
NMG_EXPORT extern void nmg_radial_build_list(struct bu_list             *hd,
                                             struct bu_ptbl             *shell_tbl,
                                             int                        existing,
                                             struct edgeuse             *eu,
                                             const vect_t               xvec,
                                             const vect_t               yvec,
                                             const vect_t               zvec,
                                             const struct bn_tol        *tol);
NMG_EXPORT extern void nmg_radial_merge_lists(struct bu_list            *dest,
                                              struct bu_list            *src,
                                              const struct bn_tol       *tol);
NMG_EXPORT extern int    nmg_is_crack_outie(const struct edgeuse        *eu,
                                            struct bu_list *vlfree,
                                            const struct bn_tol *tol);
NMG_EXPORT extern struct nmg_radial     *nmg_find_radial_eu(const struct bu_list *hd,
                                                            const struct edgeuse *eu);
NMG_EXPORT extern const struct edgeuse *nmg_find_next_use_of_2e_in_lu(const struct edgeuse      *eu,
                                                                      const struct edge *e1,
                                                                      const struct edge *e2);
NMG_EXPORT extern void nmg_radial_mark_cracks(struct bu_list    *hd,
                                              const struct edge *e1,
                                              const struct edge *e2,
                                              struct bu_list *vlfree,
                                              const struct bn_tol       *tol);
NMG_EXPORT extern struct nmg_radial *nmg_radial_find_an_original(const struct bu_list   *hd,
                                                                 const struct shell     *s,
                                                                 const struct bn_tol    *tol);
NMG_EXPORT extern int nmg_radial_mark_flips(struct bu_list              *hd,
                                            const struct shell  *s,
                                            const struct bn_tol *tol);
NMG_EXPORT extern int nmg_radial_check_parity(const struct bu_list      *hd,
                                              const struct bu_ptbl      *shells,
                                              const struct bn_tol       *tol);
NMG_EXPORT extern void nmg_radial_implement_decisions(struct bu_list            *hd,
                                                      const struct bn_tol       *tol,
                                                      struct edgeuse            *eu1,
                                                      vect_t xvec,
                                                      vect_t yvec,
                                                      vect_t zvec);
NMG_EXPORT extern void nmg_pr_radial(const char *title,
                                     const struct nmg_radial    *rad);
NMG_EXPORT extern void nmg_pr_radial_list(const struct bu_list *hd,
                                          const struct bn_tol *tol);
NMG_EXPORT extern void nmg_do_radial_flips(struct bu_list *hd);
NMG_EXPORT extern void nmg_do_radial_join(struct bu_list *hd,
                                          struct edgeuse *eu1ref,
                                          vect_t xvec, vect_t yvec, vect_t zvec,
                                          const struct bn_tol *tol);
NMG_EXPORT extern void nmg_radial_join_eu_NEW(struct edgeuse *eu1,
                                              struct edgeuse *eu2,
                                              const struct bn_tol *tol);
NMG_EXPORT extern void nmg_radial_exchange_marked(struct bu_list                *hd,
                                                  const struct bn_tol   *tol);
NMG_EXPORT extern void nmg_s_radial_harmonize(struct shell              *s,
                                              struct bu_list *vlfree,
                                              const struct bn_tol       *tol);
NMG_EXPORT extern void nmg_s_radial_check(struct shell          *s,
                                          struct bu_list *vlfree,
                                          const struct bn_tol   *tol);
NMG_EXPORT extern void nmg_r_radial_check(const struct nmgregion        *r,
                                          struct bu_list *vlfree,
                                          const struct bn_tol   *tol);


__END_DECLS

#endif  /* NMG_RADIAL_H */
/** @} */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
