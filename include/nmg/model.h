/*                        M O D E L . H
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
 * NMG model definitions
 */
/** @{ */
/** @file nmg/model.h */

#ifndef NMG_MODEL_H
#define NMG_MODEL_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_MODEL(_p)              NMG_CKMAG(_p, NMG_MODEL_MAGIC, "model")

#define GET_MODEL(p, m)             {NMG_GETSTRUCT(p, model); NMG_INCR_INDEX(p, m);}
#define FREE_MODEL(p)             NMG_FREESTRUCT(p, model)

NMG_EXPORT extern struct model *nmg_mm(void);
NMG_EXPORT extern struct model *nmg_mmr(void);
NMG_EXPORT extern struct nmgregion *nmg_mrsv(struct model *m);
NMG_EXPORT extern void nmg_km(struct model *m);

NMG_EXPORT extern void nmg_count_shell_kids(const struct model *m,
                                            size_t *total_wires,
                                            size_t *total_faces,
                                            size_t *total_points);
NMG_EXPORT extern struct model *nmg_find_model(const uint32_t *magic_p);
NMG_EXPORT extern void nmg_model_bb(point_t min_pt,
                                    point_t max_pt,
                                    const struct model *m);
NMG_EXPORT extern struct vertex *nmg_find_pnt_in_model(const struct model *m,
                                                      const point_t pt,
                                                      const struct bn_tol *tol);
NMG_EXPORT extern void nmg_find_zero_length_edges(const struct model *m, struct bu_list *vlfree);
NMG_EXPORT extern fastf_t nmg_model_area(const struct model *m);
NMG_EXPORT extern void nmg_rebound(struct model *m,
                                   const struct bn_tol *tol);
NMG_EXPORT extern struct model *nmg_mk_model_from_region(struct nmgregion *r,
                                                         int reindex, struct bu_list *vlfree);
NMG_EXPORT extern void nmg_mirror_model(struct model *m, struct bu_list *vlfree);
NMG_EXPORT extern int nmg_kill_zero_length_edgeuses(struct model *m);
NMG_EXPORT extern int nmg_edge_collapse(struct model *m,
                                        const struct bn_tol *tol,
                                        const fastf_t tol_coll,
                                        const fastf_t min_angle,
                                        struct bu_list *vlfree);
NMG_EXPORT extern struct model *nmg_clone_model(const struct model *original);

NMG_EXPORT extern void nmg_triangulate_model(struct model *m,
                                             struct bu_list *vlfree,
                                             const struct bn_tol *tol);
NMG_EXPORT extern void nmg_dump_model(struct model *m);
NMG_EXPORT extern char *nmg_manifolds(struct model *m);
NMG_EXPORT extern int nmg_model_face_fuse(struct model *m,struct bu_list *vlfree,
                                          const struct bn_tol *tol);
/* DEPRECATED: use nmg_break_e_on_v */
NMG_EXPORT extern int nmg_model_break_e_on_v(const uint32_t *magic_p,
                                             struct bu_list *vlfree,
                                             const struct bn_tol *tol);
NMG_EXPORT extern int nmg_model_fuse(struct model *m,
                                     struct bu_list *vlfree,
                                     const struct bn_tol *tol);
NMG_EXPORT extern void nmg_m_set_high_bit(struct model *m);
NMG_EXPORT extern void nmg_m_reindex(struct model *m, long newindex);
NMG_EXPORT extern void nmg_merge_models(struct model *m1,
                                        struct model *m2);
NMG_EXPORT extern long nmg_find_max_index(const struct model *m);

NMG_EXPORT extern const char *nmg_class_name(int class_no);

__END_DECLS

#endif  /* NMG_MODEL_H */
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
