/*                    P A Y L O A D _ P R I V A T E . H
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
/** @file payload_private.h
 *
 * Private node-owned payload storage helpers.  Public producers should build
 * typed payload records and commit them through feature, HUD, overlay, or GED
 * draw-source endpoints.
 */

#ifndef BSG_PAYLOAD_PRIVATE_H
#define BSG_PAYLOAD_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "bsg/payload.h"
#include "payload_typed_private.h"

__BEGIN_DECLS

BSG_EXPORT extern void
bsg_node_set_payload_type(bsg_node *node, unsigned long long payload_flags);

BSG_EXPORT extern unsigned long long
bsg_node_get_payload_type(const bsg_node *node);

__END_DECLS

#endif /* BSG_PAYLOAD_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
