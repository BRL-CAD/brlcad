/*                    O V E R L A Y _ P R I V A T E . H
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
/** @file overlay_private.h
 *
 * Private overlay node ownership and metadata helpers.
 */

#ifndef BSG_OVERLAY_PRIVATE_H
#define BSG_OVERLAY_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "bu/ptbl.h"
#include "bsg/overlay.h"

__BEGIN_DECLS

extern bsg_node *
bsg_find_overlay_group(bsg_node *draw_root);

extern bsg_node *
bsg_ensure_overlay_group(bsg_node *draw_root, struct bsg_view *v);

extern void
bsg_erase_overlay_by_name(bsg_node *draw_root, const char *name);

extern int
bsg_overlay_register_owner(bsg_node *overlay_node,
			   const void *owner,
			   bsg_overlay_role role,
			   bsg_overlay_class overlay_class,
			   bsg_overlay_lifecycle lifecycle,
			   bsg_overlay_order ordering,
			   const char *source_path,
			   int sort_order);

extern bsg_node *
bsg_overlay_replace(struct bsg_view *v, const void *owner, bsg_node *overlay_node);

extern size_t
bsg_overlay_query_by_role(bsg_node *root, bsg_overlay_role role, struct bu_ptbl *out);

extern size_t
bsg_overlay_auto_remove(bsg_node *root, const char *source_path);

extern const struct bsg_overlay_info *
bsg_overlay_info_get(const bsg_node *overlay_node);

extern void
bsg_overlay_info_clear(bsg_node *overlay_node);

__END_DECLS

#endif /* BSG_OVERLAY_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
