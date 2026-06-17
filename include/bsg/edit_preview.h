/*                  E D I T _ P R E V I E W . H
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */
/** @addtogroup libbsg
 *
 * @brief Typed edit-preview callback contract.
 */
/** @{ */
/* @file bsg/edit_preview.h */

#ifndef BSG_EDIT_PREVIEW_H
#define BSG_EDIT_PREVIEW_H

#include "common.h"
#include <stdint.h>
#include "vmath.h"
#include "bsg/defines.h"

__BEGIN_DECLS

struct bsg_view;

typedef uint64_t (*bsg_edit_preview_revision_cb_t)(void *preview_ctx);
typedef int (*bsg_edit_preview_update_cb_t)(void *preview_ctx, struct bsg_view *v);
typedef int (*bsg_edit_preview_bounds_cb_t)(void *preview_ctx, point_t *bmin, point_t *bmax);
typedef int (*bsg_edit_preview_pick_cb_t)(void *preview_ctx, struct bsg_view *v, int x, int y, void *pick_out);
typedef int (*bsg_edit_preview_snap_cb_t)(void *preview_ctx, struct bsg_view *v, const point_t sample_pt, point_t out_pt);
typedef void (*bsg_edit_preview_free_cb_t)(void *preview_ctx);

typedef enum bsg_edit_preview_stale_reason {
    BSG_EDIT_PREVIEW_STALE_NONE = 0,
    BSG_EDIT_PREVIEW_STALE_SOURCE_CHANGED,
    BSG_EDIT_PREVIEW_STALE_VIEW_INPUT_CHANGED,
    BSG_EDIT_PREVIEW_STALE_SETTINGS_CHANGED,
    BSG_EDIT_PREVIEW_STALE_FORCED,
    BSG_EDIT_PREVIEW_STALE_UPDATE_FAILED
} bsg_edit_preview_stale_reason;

struct bsg_edit_preview_ops {
    void *preview_ctx;
    int owns_preview_ctx;
    bsg_edit_preview_revision_cb_t revision_cb;
    bsg_edit_preview_update_cb_t update_cb;
    bsg_edit_preview_bounds_cb_t bounds_cb;
    bsg_edit_preview_pick_cb_t pick_cb;
    bsg_edit_preview_snap_cb_t snap_cb;
    bsg_edit_preview_free_cb_t free_cb;
};

#define BSG_EDIT_PREVIEW_OPS_INIT { NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL }

BSG_EXPORT extern const char *
bsg_edit_preview_stale_reason_name(bsg_edit_preview_stale_reason reason);

__END_DECLS

#endif /* BSG_EDIT_PREVIEW_H */

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
