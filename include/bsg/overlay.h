/*                    O V E R L A Y . H
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
 * Pure-BSG overlay ownership helpers.  Installed callers operate on overlay
 * ownership/source-path records; node group creation, replacement, metadata
 * slots, and node queries are private libbsg storage details.
 */
/** @{ */
/* @file bsg/overlay.h */

#ifndef BSG_OVERLAY_H
#define BSG_OVERLAY_H

#include "common.h"
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "bsg/defines.h"
#include "bsg/hud.h"
#include "bsg/scene_builder.h"

__BEGIN_DECLS

struct bsg_view;          /* forward-declare to avoid circular includes */

typedef enum {
    BSG_OVERLAY_ORDER_MODEL = 0,
    BSG_OVERLAY_ORDER_SCREEN,
    BSG_OVERLAY_ORDER_XRAY,
    BSG_OVERLAY_ORDER_POST_TRANSPARENT
} bsg_overlay_order;

struct bsg_overlay_info {
    const void *owner;
    bsg_overlay_role role;
    bsg_overlay_class overlay_class;
    bsg_overlay_lifecycle lifecycle;
    bsg_overlay_order ordering;
    int sort_order;
    struct bu_vls source_path;
};


BSG_EXPORT extern size_t
bsg_overlay_clear_owned(struct bsg_view *v, const void *owner);

BSG_EXPORT extern int
bsg_overlay_append_scene(struct bsg_view *v, bsg_scene_ref overlay);

BSG_EXPORT extern bsg_scene_ref
bsg_overlay_find_scene(struct bsg_view *v, const char *name);

BSG_EXPORT extern void
bsg_overlay_erase_name(struct bsg_view *v, const char *name);

BSG_EXPORT extern int
bsg_overlay_register_scene_owner(bsg_scene_ref overlay,
				 const void *owner,
				 bsg_overlay_role role,
				 bsg_overlay_class overlay_class,
				 bsg_overlay_lifecycle lifecycle,
				 bsg_overlay_order ordering,
				 const char *source_path,
				 int sort_order);


__END_DECLS

#endif /* BSG_OVERLAY_H */

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
