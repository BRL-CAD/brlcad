/*                D E C I M A T E _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef LIBRT_PRIMITIVES_BOT_DECIMATE_PRIVATE_H
#define LIBRT_PRIMITIVES_BOT_DECIMATE_PRIVATE_H

#include "common.h"

#include "rt/geom.h"

__BEGIN_DECLS

/* Return 1 when every candidate vertex is within max_distance of source, 0
 * when one is outside the limit, and -1 for malformed input. */
int rt_bot_decimation_is_within_distance(
    size_t *offending_vertex,
    const struct rt_bot_internal *source,
    const struct rt_bot_internal *candidate,
    fastf_t max_distance);

__END_DECLS

#endif /* LIBRT_PRIMITIVES_BOT_DECIMATE_PRIVATE_H */
