/*                       V E R T E X . H
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
 * NMG vertex definitions
 */
/** @{ */
/** @file nmg/vertex.h */

#ifndef NMG_VERTEX_H
#define NMG_VERTEX_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_VERTEX(_p)             NMG_CKMAG(_p, NMG_VERTEX_MAGIC, "vertex")
#define NMG_CK_VERTEX_G(_p)           NMG_CKMAG(_p, NMG_VERTEX_G_MAGIC, "vertex_g")
#define NMG_CK_VERTEXUSE(_p)          NMG_CKMAG(_p, NMG_VERTEXUSE_MAGIC, "vertexuse")
#define NMG_CK_VERTEXUSE_A_PLANE(_p)  NMG_CKMAG(_p, NMG_VERTEXUSE_A_PLANE_MAGIC, "vertexuse_a_plane")

#define GET_VERTEX(p, m)            {NMG_GETSTRUCT(p, vertex); NMG_INCR_INDEX(p, m);}
#define GET_VERTEX_G(p, m)          {NMG_GETSTRUCT(p, vertex_g); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE(p, m)         {NMG_GETSTRUCT(p, vertexuse); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE_A_PLANE(p, m) {NMG_GETSTRUCT(p, vertexuse_a_plane); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE_A_CNURB(p, m) {NMG_GETSTRUCT(p, vertexuse_a_cnurb); NMG_INCR_INDEX(p, m);}
#define FREE_VERTEX(p)            NMG_FREESTRUCT(p, vertex)
#define FREE_VERTEX_G(p)          NMG_FREESTRUCT(p, vertex_g)
#define FREE_VERTEXUSE(p)         NMG_FREESTRUCT(p, vertexuse)
#define FREE_VERTEXUSE_A_PLANE(p) NMG_FREESTRUCT(p, vertexuse_a_plane)
#define FREE_VERTEXUSE_A_CNURB(p) NMG_FREESTRUCT(p, vertexuse_a_cnurb)



#define PREEXIST 1
#define NEWEXIST 2


#define VU_PREEXISTS(_bs, _vu) { chkidxlist((_bs), (_vu)); \
        (_bs)->vertlist[(_vu)->index] = PREEXIST; }

#define VU_NEW(_bs, _vu) { chkidxlist((_bs), (_vu)); \
        (_bs)->vertlist[(_vu)->index] = NEWEXIST; }


NMG_EXPORT extern void nmg_vertex_gv(struct vertex *v,
                                     const point_t pt);
NMG_EXPORT extern void nmg_vertex_g(struct vertex *v,
                                    fastf_t x,
                                    fastf_t y,
                                    fastf_t z);
NMG_EXPORT extern void nmg_vertexuse_nv(struct vertexuse *vu,
                                        const vect_t norm);

NMG_EXPORT extern void nmg_movevu(struct vertexuse *vu,
                                  struct vertex *v);

NMG_EXPORT extern int nmg_kvu(struct vertexuse *vu);

NMG_EXPORT extern void nmg_jv(struct vertex *v1,
                              struct vertex *v2);
NMG_EXPORT extern void nmg_vertex_tabulate(struct bu_ptbl *tab,
                                           const uint32_t *magic_p,
                                           struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vertexuse_normal_tabulate(struct bu_ptbl *tab,
                                                     const uint32_t *magic_p,
                                                     struct bu_list *vlfree);

NMG_EXPORT extern int nmg_in_or_ref(struct vertexuse *vu,
                                    struct bu_ptbl *b);
NMG_EXPORT extern int nmg_simple_vertex_solve(struct vertex *new_v,
                                              const struct bu_ptbl *faces,
                                              const struct bn_tol *tol);
NMG_EXPORT extern int nmg_in_vert(struct vertex *new_v,
                                  const int approximate,
                                  struct bu_list *vlfree,
                                  const struct bn_tol *tol);

NMG_EXPORT extern int nmg_ptbl_vfuse(struct bu_ptbl *t,
                                     const struct bn_tol *tol);

NMG_EXPORT extern int nmg_vertex_fuse(const uint32_t *magic_p,struct bu_list *vlfree,
                                      const struct bn_tol *tol);

NMG_EXPORT extern void nmg_unlist_v(struct bu_ptbl       *b,
                                    fastf_t *mag,
                                    struct vertex        *v);

__END_DECLS

#endif  /* NMG_VERTEX_H */
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
