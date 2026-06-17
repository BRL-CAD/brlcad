/*                  D R A W _ S O U R C E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @addtogroup libbsg
 *
 * @brief
 * Draw-source accessors for graph-associated view, draw matrices, bounds,
 * and source-realization metadata.
 *
 * Drawing producers use these accessors to describe their intent in BSG
 * scene-graph terms instead of writing private view and draw-node fields.
 */
/** @{ */
/* @file bsg/draw_source.h */

#ifndef BSG_DRAW_SOURCE_H
#define BSG_DRAW_SOURCE_H

#include "common.h"
#include "bu/list.h"
#include "vmath.h"
#include "bsg/defines.h"
#include "bsg/scene_builder.h"

__BEGIN_DECLS

/* -----------------------------------------------------------------------
 * Associated view
 *
 * A scene element may retain its creating/editing bsg_view.  In a multi-view
 * scenario the view association may be updated as the element is edited from
 * different views.
 * ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * Draw matrix
 *
 * The draw matrix is the 4x4 matrix used for internal lookup and mesh LoD
 * drawing.  It is distinct from the authored scene transform.
 * ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * Bounding sphere
 *
 * The bounding sphere stores the model-space radius and center maintained by
 * bsg_scene_node_update_bounds().
 * ----------------------------------------------------------------------- */

BSG_EXPORT extern void
bsg_scene_set_draw_mat(bsg_scene_ref ref, const mat_t mat);

BSG_EXPORT extern void
bsg_scene_draw_mat(bsg_scene_ref ref, mat_t mat);

BSG_EXPORT extern fastf_t
bsg_scene_draw_size(bsg_scene_ref ref);

BSG_EXPORT extern void
bsg_scene_set_draw_size(bsg_scene_ref ref, fastf_t size);

BSG_EXPORT extern void
bsg_scene_draw_center(bsg_scene_ref ref, vect_t center);

BSG_EXPORT extern void
bsg_scene_set_draw_center(bsg_scene_ref ref, const vect_t center);

__END_DECLS

#endif /* BSG_DRAW_SOURCE_H */

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
