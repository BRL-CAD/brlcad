/*                        S N A P . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2025 United States Government as represented by
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
/** @addtogroup bv_snap
 *
 * @brief Logic for snapping sample points to view elements and grids.
 *
 */
/** @{ */
/** @file bv/snap.h */

#ifndef BV_SNAP_H
#define BV_SNAP_H

#include "common.h"
#include "vmath.h"
#include "bv/defines.h"

__BEGIN_DECLS

/* Logic for snapping points to their closes view lines. */

/* Snap sample 2D point to lines active in the view.  If populated,
 * v->gv_s->gv_snap_objs contains a subset of bv_scene_obj pointers indicating
 * which view objects to consider for snapping.  If nonzero,
 * v->gv_s->gv_snap_flags also tells the routine which categories of objects to
 * consider - objs objects will also be evaluated against the flags before
 * being used. */
BV_EXPORT extern int bv_snap_lines_2d(struct bview *v, const struct bu_ptbl *sobjs, fastf_t *fx, fastf_t *fy);

BV_EXPORT extern void bv_view_center_linesnap(struct bview *v, const struct bu_ptbl *sobjs);

BV_EXPORT extern int bv_snap_lines_3d(point_t *out_pt, struct bview *v, const struct bu_ptbl *sobjs, point_t *p);
BV_EXPORT extern int bv_snap_grid_2d(struct bview *v, fastf_t *fx, fastf_t *fy);

__END_DECLS

#endif /* BV_SNAP_H */

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
