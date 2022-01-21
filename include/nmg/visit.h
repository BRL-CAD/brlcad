/*                        V I S I T . H
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
 * NMG visit definitions
 */
/** @{ */
/** @file nmg/visit.h */

#ifndef NMG_VISIT_H
#define NMG_VISIT_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/model.h"

__BEGIN_DECLS

/**
 * Function table, for use with nmg_visit().
 *
 * Intended to have same generally the organization as
 * nmg_struct_counts.  The handler's args are long* to allow generic
 * handlers to be written, in which case the magic number at long*
 * specifies the object type.
 *
 * The "vis_" prefix means the handler is visited only once.  The
 * "bef_" and "aft_" prefixes are called (respectively) before and
 * after recursing into subsidiary structures.  The 3rd arg is 0 for a
 * "bef_" call, and 1 for an "aft_" call, to allow generic handlers to
 * be written, if desired.
 */
struct nmg_visit_handlers {
    void (*bef_model)(uint32_t *, void *, int);
    void (*aft_model)(uint32_t *, void *, int);

    void (*bef_region)(uint32_t *, void *, int);
    void (*aft_region)(uint32_t *, void *, int);

    void (*vis_region_a)(uint32_t *, void *, int);

    void (*bef_shell)(uint32_t *, void *, int);
    void (*aft_shell)(uint32_t *, void *, int);

    void (*vis_shell_a)(uint32_t *, void *, int);

    void (*bef_faceuse)(uint32_t *, void *, int);
    void (*aft_faceuse)(uint32_t *, void *, int, struct bu_list *);

    void (*vis_face)(uint32_t *, void *, int);
    void (*vis_face_g)(uint32_t *, void *, int);

    void (*bef_loopuse)(uint32_t *, void *, int);
    void (*aft_loopuse)(uint32_t *, void *, int);

    void (*vis_loop)(uint32_t *, void *, int);
    void (*vis_loop_a)(uint32_t *, void *, int);

    void (*bef_edgeuse)(uint32_t *, void *, int);
    void (*aft_edgeuse)(uint32_t *, void *, int);

    void (*vis_edge)(uint32_t *, void *, int);
    void (*vis_edge_g)(uint32_t *, void *, int);

    void (*bef_vertexuse)(uint32_t *, void *, int);
    void (*aft_vertexuse)(uint32_t *, void *, int);

    void (*vis_vertexuse_a)(uint32_t *, void *, int);
    void (*vis_vertex)(uint32_t *, void *, int);
    void (*vis_vertex_g)(uint32_t *, void *, int);
};

/* nmg_visit.c */
NMG_EXPORT extern void nmg_visit_vertex(struct vertex                   *v,
                                        const struct nmg_visit_handlers *htab,
                                        void *                  state);
NMG_EXPORT extern void nmg_visit_vertexuse(struct vertexuse             *vu,
                                           const struct nmg_visit_handlers      *htab,
                                           void *                       state);
NMG_EXPORT extern void nmg_visit_edge(struct edge                       *e,
                                      const struct nmg_visit_handlers   *htab,
                                      void *                    state);
NMG_EXPORT extern void nmg_visit_edgeuse(struct edgeuse                 *eu,
                                         const struct nmg_visit_handlers        *htab,
                                         void *                 state);
NMG_EXPORT extern void nmg_visit_loop(struct loop                       *l,
                                      const struct nmg_visit_handlers   *htab,
                                      void *                    state);
NMG_EXPORT extern void nmg_visit_loopuse(struct loopuse                 *lu,
                                         const struct nmg_visit_handlers        *htab,
                                         void *                 state);
NMG_EXPORT extern void nmg_visit_face(struct face                       *f,
                                      const struct nmg_visit_handlers   *htab,
                                      void *                    state);
NMG_EXPORT extern void nmg_visit_faceuse(struct faceuse                 *fu,
                                         const struct nmg_visit_handlers        *htab,
                                         void *                 state,
                                         struct bu_list *vlfree);
NMG_EXPORT extern void nmg_visit_shell(struct shell                     *s,
                                       const struct nmg_visit_handlers  *htab,
                                       void *                   state,
                                       struct bu_list *vlfree);
NMG_EXPORT extern void nmg_visit_region(struct nmgregion                *r,
                                        const struct nmg_visit_handlers *htab,
                                        void *                  state,
                                        struct bu_list *vlfree);
NMG_EXPORT extern void nmg_visit_model(struct model                     *model,
                                       const struct nmg_visit_handlers  *htab,
                                       void *                   state,
                                       struct bu_list *vlfree);
NMG_EXPORT extern void nmg_visit(const uint32_t         *magicp,
                                 const struct nmg_visit_handlers        *htab,
                                 void *                 state,
                                 struct bu_list *vlfree);


__END_DECLS

#endif  /* NMG_VISIT_H */
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
