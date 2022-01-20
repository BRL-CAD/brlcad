/*                       R E G I O N . H
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
 * NMG region definitions
 */
/** @{ */
/** @file nmg/region.h */

#ifndef NMG_REGION_H
#define NMG_REGION_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_REGION(_p)             NMG_CKMAG(_p, NMG_REGION_MAGIC, "region")
#define NMG_CK_REGION_A(_p)           NMG_CKMAG(_p, NMG_REGION_A_MAGIC, "region_a")

#define GET_REGION(p, m)            {NMG_GETSTRUCT(p, nmgregion); NMG_INCR_INDEX(p, m);}
#define GET_REGION_A(p, m)          {NMG_GETSTRUCT(p, nmgregion_a); NMG_INCR_INDEX(p, m);}

#define FREE_REGION(p)            NMG_FREESTRUCT(p, nmgregion)
#define FREE_REGION_A(p)          NMG_FREESTRUCT(p, nmgregion_a)

NMG_EXPORT extern void nmg_region_a(struct nmgregion *r,
                                    const struct bn_tol *tol);

NMG_EXPORT extern void nmg_merge_regions(struct nmgregion *r1,
                                         struct nmgregion *r2,
                                         const struct bn_tol *tol);

/* From file nmg_mk.c */
/*      MAKE routines */
NMG_EXPORT extern struct nmgregion *nmg_mrsv(struct model *m);
NMG_EXPORT extern struct shell *nmg_msv(struct nmgregion *r_p);
NMG_EXPORT extern int nmg_kr(struct nmgregion *r);

NMG_EXPORT extern fastf_t nmg_region_area(const struct nmgregion *r);
NMG_EXPORT extern int nmg_unbreak_region_edges(uint32_t *magic_p, struct bu_list *vlfree);
NMG_EXPORT extern void nmg_region_v_unique(struct nmgregion *r1, struct bu_list *vlfree,
                                           const struct bn_tol *tol);
NMG_EXPORT extern int   nmg_region_both_vfuse(struct bu_ptbl *t1,
                                              struct bu_ptbl *t2,
                                              const struct bn_tol *tol);
NMG_EXPORT extern struct nmgregion *nmg_do_bool(struct nmgregion *s1,
                                                struct nmgregion *s2,
                                                const int oper, struct bu_list *vlfree, const struct bn_tol *tol);
NMG_EXPORT extern int nmg_two_region_vertex_fuse(struct nmgregion *r1,
                                                 struct nmgregion *r2,
                                                 const struct bn_tol *tol);

__END_DECLS

#endif  /* NMG_REGION_H */
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
