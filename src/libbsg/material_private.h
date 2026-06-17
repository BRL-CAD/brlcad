/*                 M A T E R I A L _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file material_private.h
 *
 * Source-private raw-node material accessors.
 */

#ifndef BSG_MATERIAL_PRIVATE_H
#define BSG_MATERIAL_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "bsg/material.h"

__BEGIN_DECLS

BSG_EXPORT extern void bsg_material_set_rgb(bsg_node *node,
	unsigned char r, unsigned char g, unsigned char b);
BSG_EXPORT extern void bsg_material_get_rgb(const bsg_node *node,
	unsigned char *r, unsigned char *g, unsigned char *b);
BSG_EXPORT extern void bsg_material_set_revision(bsg_node *node, uint32_t revision);
BSG_EXPORT extern uint32_t bsg_material_revision(const bsg_node *node);

__END_DECLS

#endif /* BSG_MATERIAL_PRIVATE_H */
