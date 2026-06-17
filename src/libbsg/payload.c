/*                    P A Y L O A D . C
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
/** @file libbsg/payload.c
 *
 * Payload type accessors.
 */

#include "common.h"

#include "bsg/defines.h"
#include "bsg/payload.h"
#include "bsg/scene_object.h"
#include "bsg_private.h"
#include "payload_typed_private.h"
#include "node_private.h"
#include "payload_private.h"


void
bsg_node_set_payload_type(bsg_node *node, unsigned long long payload_flags)
{
    if (!node)
	return;

    struct bsg_node_internal *ni = bsg_node_internal_ensure(node);
    unsigned long long flags = payload_flags & BSG_PAYLOAD_MASK;

    ni->payload_flags = flags;
    ni->payload_flags_valid = 1;
}


unsigned long long
bsg_node_get_payload_type(const bsg_node *node)
{
    if (!node)
	return 0;

    if (node->i && node->i->payload_flags_valid)
	return node->i->payload_flags & BSG_PAYLOAD_MASK;
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
