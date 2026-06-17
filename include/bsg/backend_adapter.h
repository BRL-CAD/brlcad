/*                B A C K E N D _ A D A P T E R . H
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
 * Backend adapter interface — the contract between BSG render batches and a
 * concrete drawing backend.
 *
 * A backend (libdm GL, swrast, qtgl, or a future Obol renderer) fills in
 * a `bsg_backend_adapter` and attaches it to a `bsg_render_request` with
 * bsg_render_request_set_adapter().  The renderer first resolves a
 * `bsg_render_batch`; dispatch then calls the adapter callbacks once per
 * resolved `bsg_render_item`.
 *
 * The adapter owns all backend-side state; libbsg never reads or frees it.
 * `dmp` is the same opaque display-manager handle passed to
 * bsg_render_request_create() and is forwarded to each callback.
 *
 * Null function pointers are silently skipped; a backend only needs to
 * implement the callbacks it uses.
 *
 * Typical lifetime per frame:
 *   1. Caller attaches an initialised bsg_backend_adapter* to the request.
 *   2. bsg_render_request_collect calls begin_request(), allowing the backend
 *      to add backend-specific batch state before item collection.
 *   3. bsg_render_batch_dispatch calls prepare() → draw() per item.
 *   4. bsg_render_batch_dispatch calls end_request().
 *   5. Backend-owned caches reclaim resources not seen by the owning frame.
 */
/** @{ */
/* @file bsg/backend_adapter.h */

#ifndef BSG_BACKEND_ADAPTER_H
#define BSG_BACKEND_ADAPTER_H

#include "common.h"
#include "bsg/defines.h"
#include "bsg/render_item.h"

__BEGIN_DECLS

struct bsg_render_request;

/* -----------------------------------------------------------------------
 * Capability flags returned by bsg_backend_adapter::capabilities()
 * ----------------------------------------------------------------------- */

/** Backend supports per-object transparency (alpha blending). */
#define BSG_ADAPTER_CAP_TRANSPARENCY  0x01u

/** Backend can render wireframe display modes. */
#define BSG_ADAPTER_CAP_WIREFRAME     0x02u

/** Backend can render shaded (BoT/mesh) display modes. */
#define BSG_ADAPTER_CAP_SHADED        0x04u

/** Backend can render HUD / faceplate overlay items. */
#define BSG_ADAPTER_CAP_HUD           0x08u

/** Backend supports GPU-side back-to-front sort for transparent items. */
#define BSG_ADAPTER_CAP_SORTED_ALPHA  0x10u

/** Backend supports BRep/NURBS surface payload records. */
#define BSG_ADAPTER_CAP_BREP          0x20u


/* -----------------------------------------------------------------------
 * Invalidation reason mask
 *
 * Passed to the adapter invalidate() callback so that the backend can
 * selectively re-upload only the resources that actually changed instead
 * of blindly discarding the entire cached representation.
 *
 * Multiple reasons may be OR'd together in a single invalidate() call.
 * ----------------------------------------------------------------------- */

/**
 * The payload geometry changed (e.g. vlist rebuilt, mesh re-tessellated).
 * Backend should re-upload vertex/index data.
 */
#define BSG_INVALIDATE_PAYLOAD     0x01u

/**
 * The node's material or appearance changed (color, transparency, line
 * style/width, display mode, highlight state).
 * Backend should update per-object uniform buffers / material state.
 */
#define BSG_INVALIDATE_APPEARANCE  0x02u

/**
 * The accumulated transform path changed (a parent transform matrix was
 * edited).  Backend may need to rebuild the model matrix for the object.
 */
#define BSG_INVALIDATE_TRANSFORM   0x04u

/**
 * The render mode changed (draw mode switched between wireframe/shaded,
 * hidden-line toggled, etc.).
 * Backend should re-select or recompile the render pipeline state.
 */
#define BSG_INVALIDATE_RENDER_MODE 0x08u

/**
 * All backend state for the node is stale; equivalent to
 * BSG_INVALIDATE_PAYLOAD | BSG_INVALIDATE_APPEARANCE |
 * BSG_INVALIDATE_TRANSFORM | BSG_INVALIDATE_RENDER_MODE.
 */
#define BSG_INVALIDATE_ALL         0x0Fu


/* -----------------------------------------------------------------------
 * Adapter struct
 * ----------------------------------------------------------------------- */

/**
 * Backend adapter — a vtable of callbacks that a concrete drawing backend
 * implements to consume BSG render items.
 *
 * All callback pointers may be NULL; missing callbacks are silently skipped.
 * The `dmp` argument to each callback is the display-manager handle from the
 * owning `bsg_render_request`.
 */
struct bsg_backend_adapter {
    int (*begin_request)(void *dmp, struct bsg_render_request *req);

    /**
     * Prepare backend resources for @p item before the first draw call.
     *
     * Called by `bsg_render_batch_dispatch` for each item that is about to be
     * drawn.  Backends use this to allocate GPU buffers, compile shaders, or
     * upload mesh data.  May be NULL when no per-item preparation is needed.
     *
     * Returns non-zero on success, 0 on failure (the item will still be
     * passed to draw() — the backend decides how to handle a prepare failure).
     */
    int (*prepare)(void *dmp, const struct bsg_render_item *item);

    /**
     * Issue the draw call for @p item.
     *
     * Called after prepare() (when set) for each item in phase order.
     * The draw call must not modify @p item.  May be NULL (drawing
     * is silently skipped for this backend).
     */
    void (*draw)(void *dmp, const struct bsg_render_item *item);

    /**
     * Mark cached backend resources for @p item as stale.
     *
     * Called when a node's geometry or appearance has changed and the backend
     * needs to re-upload data on the next draw.  @p reason_mask is a bitmask
     * of BSG_INVALIDATE_* flags describing what changed; backends should only
     * re-upload or recompile the resources indicated by the set flags.
     * Pass BSG_INVALIDATE_ALL when the full reason is unknown.  May be NULL.
     */
    void (*invalidate)(void *dmp, const struct bsg_render_item *item,
		       unsigned int reason_mask);

    /**
     * Release all backend resources associated with @p item.
     *
     * Called when a source is removed from the scene.  The adapter must use
     * the render-item source identity rather than graph-node access.  May be
     * NULL.
     */
    void (*free)(void *dmp, const struct bsg_render_item *item);

    /**
     * Return the set of `BSG_ADAPTER_CAP_*` flags this backend supports.
     *
     * May be NULL (treated as returning 0 — no declared capabilities).
     * `bsg_render_request_collect` records capabilities in item context;
     * callers may use them to choose between adapters or skip unsupported
     * render passes.
     */
    unsigned int (*capabilities)(void *dmp);

    void (*end_request)(void *dmp, const struct bsg_render_request *req);
};

__END_DECLS

#endif /* BSG_BACKEND_ADAPTER_H */

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
