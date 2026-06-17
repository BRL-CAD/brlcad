/*                   D R A W _ C T X . H
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
 * Draw-tree context stored on the draw-tree root.
 *
 * bsg_draw_ctx is a thin wrapper that lets code reach the draw-revision
 * counter without carrying a struct ged * pointer.  The owner (libged)
 * sets draw_rev to point at its own storage (ged_drawable::gd_draw_rev)
 * at root-creation time.  Freeing helpers in the draw-set layer walk the
 * parent chain to the root and bump *draw_rev through this pointer,
 * removing the last struct ged * dependency from the BSG freeing helpers.
 */
/** @{ */
/* @file bsg/draw_ctx.h */

#ifndef BSG_DRAW_CTX_H
#define BSG_DRAW_CTX_H

#include "common.h"
#include <stdint.h>
#include "bsg/defines.h"
#include "bsg/scene_builder.h"

/**
 * Per-root draw-tree context.  Stored on the draw root by the owner that
 * creates the root.  Accessed (read-only pointer) by
 * libbsg helpers that need to bump the revision counter or free objects
 * without access to the owning application's private state.
 *
 * Fields:
 *   draw_rev  - pointer to the owner's structural revision counter
 *               (set by libged to &ged_drawable::gd_draw_rev at root
 *               creation time).
 *   fso       - opaque pointer to the draw-tree's private scene-node recycle
 *               pool (set by libged via bsg_set_fsos at root creation time).
 *               If NULL, libbsg falls back to each node's own recycle pool.
 *   owner_data - opaque pointer reserved for the owning layer.  libbsg does
 *               not inspect it.
 */
struct bsg_draw_ctx {
    uint64_t          *draw_rev;  /**< @brief pointer to the owner's revision counter */
    void             *fso;       /**< @brief private free-object pool handle */
    void             *owner_data; /**< @brief opaque owner data */
};

__BEGIN_DECLS

BSG_EXPORT extern void
bsg_scene_set_draw_ctx(bsg_scene_ref ref, struct bsg_draw_ctx *ctx);

BSG_EXPORT extern struct bsg_draw_ctx *
bsg_scene_draw_ctx(bsg_scene_ref ref);

__END_DECLS

#endif /* BSG_DRAW_CTX_H */

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
