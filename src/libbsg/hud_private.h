/*                         H U D _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file libbsg/hud_private.h
 *
 * Private raw-node HUD helpers.  Public callers use HUD records and render
 * products rather than node pointers.
 */

#ifndef LIBBSG_HUD_PRIVATE_H
#define LIBBSG_HUD_PRIVATE_H

#include "common.h"
#include "bsg/hud.h"
#include "node_private.h"

__BEGIN_DECLS

BSG_EXPORT extern bsg_node *bsg_hud_root_create(struct bsg_view *v);
BSG_EXPORT extern bsg_node *bsg_hud_root_get(struct bsg_view *v);
BSG_EXPORT extern struct bsg_hud_node_meta *bsg_hud_node_get_meta(bsg_node *node);
BSG_EXPORT extern const struct bsg_hud_payload *bsg_hud_node_get_payload(bsg_node *node);

__END_DECLS

#endif /* LIBBSG_HUD_PRIVATE_H */
