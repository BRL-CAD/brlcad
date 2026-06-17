/*                      R E N D E R . H
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
 * BSG render-request — retained render-item collection and dispatch.
 *
 * A `bsg_render_request` bundles together:
 *   - a target view (bsg_view*)
 *   - a retained scene stream selected from the view and request flags
 *   - a display-manager handle (void*) forwarded to payload realization
 *   - a set of BSG_RENDER_FLAG_* control flags
 *
 * bsg_render_request_collect() resolves the retained stream into a
 * bsg_render_batch: an immutable, phase-ordered sequence of bsg_render_item
 * records.  Backends consume that batch with bsg_render_batch_dispatch().
 * bsg_render_request_execute() is a compatibility wrapper over those two
 * operations.
 */
/** @{ */
/* @file bsg/render.h */

#ifndef BSG_RENDER_H
#define BSG_RENDER_H

#include "common.h"
#include "bu/ptbl.h"
#include "bsg/defines.h"
#include "bsg/render_item.h"
#include "bsg/backend_adapter.h"
#include "bsg/render_settings.h"

__BEGIN_DECLS

/** Only dispatch shapes effectively visible in the request view. */
#define BSG_RENDER_FLAG_VISIBLE_ONLY      0x01

/** Realize typed payloads for every qualifying shape node before dispatch. */
#define BSG_RENDER_FLAG_PAYLOAD_PREPARE   0x02

/** Alias for payload realization before backend dispatch. */
#define BSG_RENDER_FLAG_PAYLOAD_DISPATCH  BSG_RENDER_FLAG_PAYLOAD_PREPARE

/** Queue overlay items for dispatch after all solids. */
#define BSG_RENDER_FLAG_OVERLAY_LAST      0x04

/** Sort translucent shapes back-to-front before dispatching (placeholder). */
#define BSG_RENDER_FLAG_SORTED_ALPHA      0x08

/**
 * Execute the HUD pass: collect the view HUD stream rather than the model
 * draw stream, ordered by HUD metadata.  Call bsg_hud_sync() before
 * collecting a HUD batch.
 */
#define BSG_RENDER_FLAG_HUD_PASS          0x10

/**
 * Compatibility flag: collect render items into the output item table set by
 * bsg_render_request_set_output_items() when using bsg_render_request_execute().
 *
 * When this flag is set the compatibility executor populates the output table
 * (which must be an initialised bu_ptbl allocated and owned by the caller)
 * with pointers to heap-allocated bsg_render_item objects — one per
 * qualifying shape.  Items are appended in phase order (OPAQUE →
 * TRANSPARENT → OVERLAY → HUD) rather than tree-traversal order.  The caller
 * is responsible for calling bsg_render_item_free() on each item and
 * bu_ptbl_free() on the table.
 *
 * When this flag is clear (the default) items are allocated internally,
 * dispatched to the adapter, and then freed
 * before bsg_render_request_execute() returns.
 */
#define BSG_RENDER_FLAG_COLLECT_ITEMS     0x20

struct bsg_render_request;
struct bsg_render_batch;

/**
 * Allocate and return a render request with default flags
 * (BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE).
 * @p view and @p dmp are borrowed references — the request
 * does not take ownership of them.
 * Returns NULL on allocation failure.
 */
BSG_EXPORT extern struct bsg_render_request *
bsg_render_request_create(struct bsg_view *view,
			  void            *dmp);

/**
 * Release a render request previously allocated by
 * bsg_render_request_create().
 * The referenced view/dmp are NOT freed.
 * No-op if @p req is NULL.
 */
BSG_EXPORT extern void
bsg_render_request_destroy(struct bsg_render_request *req);

BSG_EXPORT extern struct bsg_view *
bsg_render_request_view(const struct bsg_render_request *req);

BSG_EXPORT extern void *
bsg_render_request_dmp(const struct bsg_render_request *req);

BSG_EXPORT extern unsigned int
bsg_render_request_flags(const struct bsg_render_request *req);

BSG_EXPORT extern void
bsg_render_request_set_flags(struct bsg_render_request *req, unsigned int flags);

BSG_EXPORT extern void
bsg_render_request_add_flags(struct bsg_render_request *req, unsigned int flags);

BSG_EXPORT extern void
bsg_render_request_set_adapter(struct bsg_render_request *req,
			       struct bsg_backend_adapter *adapter);

BSG_EXPORT extern void
bsg_render_request_set_output_items(struct bsg_render_request *req,
				    struct bu_ptbl *items);

BSG_EXPORT extern struct bsg_render_settings *
bsg_render_request_settings(struct bsg_render_request *req);

BSG_EXPORT extern const struct bsg_render_settings *
bsg_render_request_settings_const(const struct bsg_render_request *req);

BSG_EXPORT extern void
bsg_render_request_set_background(struct bsg_render_request *req,
				  const unsigned char *background1,
				  const unsigned char *background2);

BSG_EXPORT extern void
bsg_render_request_set_geometry_default_color(struct bsg_render_request *req,
					      const unsigned char *color);

BSG_EXPORT extern void
bsg_render_request_set_clip(struct bsg_render_request *req,
			    const vect_t *clipmin,
			    const vect_t *clipmax,
			    int zclip);

BSG_EXPORT extern void
bsg_render_request_set_lighting(struct bsg_render_request *req, int lighting);

BSG_EXPORT extern struct bsg_render_batch *
bsg_render_batch_create(void);

BSG_EXPORT extern void
bsg_render_batch_clear(struct bsg_render_batch *batch);

BSG_EXPORT extern void
bsg_render_batch_destroy(struct bsg_render_batch *batch);

BSG_EXPORT extern size_t
bsg_render_batch_count(const struct bsg_render_batch *batch);

BSG_EXPORT extern const struct bsg_render_item *
bsg_render_batch_get(const struct bsg_render_batch *batch, size_t idx);

BSG_EXPORT extern size_t
bsg_render_batch_move_items(struct bsg_render_batch *batch, struct bu_ptbl *dest);

/**
 * Resolve @p req's retained view stream into @p batch.
 *
 * The batch is cleared before collection.  Items are appended in render phase
 * order and owned by the batch until bsg_render_batch_clear() or
 * bsg_render_batch_destroy().
 */
BSG_EXPORT extern int
bsg_render_request_collect(struct bsg_render_request *req,
			   struct bsg_render_batch *batch);

/**
 * Dispatch an already-collected render batch through the request adapter.
 *
 * This is the backend-facing renderer entry point.  It reads only
 * bsg_render_item records in @p batch and does not traverse the scene graph.
 */
BSG_EXPORT extern int
bsg_render_batch_dispatch(struct bsg_render_request *req,
			  const struct bsg_render_batch *batch);

/**
 * Compatibility wrapper:
 *  1. Resolve the retained view stream and collect shape items with resolved transform and
 *     appearance.
 *  2. Sort and dispatch in phase order:
 *     OPAQUE -> TRANSPARENT -> OVERLAY -> HUD.
 *  3. For each item: call adapter prepare/draw callbacks when an adapter is
 *     attached.
 * Returns the number of shapes dispatched, or -1 if @p req is NULL.
 */
BSG_EXPORT extern int
bsg_render_request_execute(struct bsg_render_request *req);

__END_DECLS

#endif /* BSG_RENDER_H */

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
