/*                  I D E N T I T Y _ P R I V A T E . H
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
/** @file identity_private.h
 *
 * Private node identity and revision accessors.
 */

#ifndef BSG_IDENTITY_PRIVATE_H
#define BSG_IDENTITY_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "bsg/defines.h"

__BEGIN_DECLS

BSG_EXPORT extern void
bsg_node_identity_set_name(bsg_node *node, const char *name);

BSG_EXPORT extern const char *
bsg_node_identity_name(const bsg_node *node);

extern uint64_t
bsg_node_revision_get(const bsg_node *node);

extern void
bsg_node_revision_set(bsg_node *node, uint64_t revision);

extern uint64_t
bsg_node_revision_bump(bsg_node *node);

__END_DECLS

#endif /* BSG_IDENTITY_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
