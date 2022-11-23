/*                        A R B 8 . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @addtogroup rt_arb */
/** @{ */
/** @file rt/primitives/arb8.h */

#ifndef RT_PRIMITIVES_ARB8_H
#define RT_PRIMITIVES_ARB8_H

#include "common.h"
#include "vmath.h"
#include "bn/tol.h"
#include "rt/defines.h"
#include "rt/db_instance.h"
#include "rt/db_internal.h"
#include "rt/wdb.h"

__BEGIN_DECLS

/* arb8.c */
RT_EXPORT extern int rt_arb_get_cgtype(
    int *cgtype,
    struct rt_arb_internal *arb,
    const struct bn_tol *tol,
    int *uvec,  /* array of indexes to unique points in arb->pt[] */
    int *svec); /* array of indexes to like points in arb->pt[] */
RT_EXPORT extern int rt_arb_std_type(const struct rt_db_internal *ip,
				     const struct bn_tol *tol);
RT_EXPORT extern void rt_arb_centroid(point_t                       *cent,
				      const struct rt_db_internal   *ip);
RT_EXPORT extern int rt_arb_calc_points(struct rt_arb_internal *arb, int cgtype, const plane_t planes[6], const struct bn_tol *tol);            /* needs wdb.h for arg list */
RT_EXPORT extern int rt_arb_check_points(struct rt_arb_internal *arb,
					 int cgtype,
					 const struct bn_tol *tol);
RT_EXPORT extern int rt_arb_3face_intersect(point_t                     point,
					    const plane_t               planes[6],
					    int                 type,           /* 4..8 */
					    int                 loc);
RT_EXPORT extern int rt_arb_calc_planes(struct bu_vls           *error_msg_ret,
					struct rt_arb_internal  *arb,
					int                     type,
					plane_t                 planes[6],
					const struct bn_tol     *tol);
RT_EXPORT extern int rt_arb_move_edge(struct bu_vls             *error_msg_ret,
				      struct rt_arb_internal    *arb,
				      vect_t                    thru,
				      int                       bp1,
				      int                       bp2,
				      int                       end1,
				      int                       end2,
				      const vect_t              dir,
				      plane_t                   planes[6],
				      const struct bn_tol       *tol);
RT_EXPORT extern int rt_arb_edit(struct bu_vls          *error_msg_ret,
				 struct rt_arb_internal *arb,
				 int                    arb_type,
				 int                    edit_type,
				 vect_t                 pos_model,
				 plane_t                        planes[6],
				 const struct bn_tol    *tol);
RT_EXPORT extern int rt_arb_find_e_nearest_pt2(int *edge, int *vert1, int *vert2, const struct rt_db_internal *ip, const point_t pt2, const mat_t mat, fastf_t ptol);


__END_DECLS

/** @} */
#endif /* RT_PRIMITIVES_ARB8_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
