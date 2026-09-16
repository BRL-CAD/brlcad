/*                       D A T U M . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file rt/primitives/datum.h */

#ifndef RT_PRIMITIVES_DATUM_H
#define RT_PRIMITIVES_DATUM_H

#include "common.h"
#include "bu/vls.h"
#include "rt/defines.h"
#include "rt/geom.h"

__BEGIN_DECLS

/** Resolve RT_DATUM_AUTO using the historical pnt/dir/w convention. */
RT_EXPORT extern rt_datum_type rt_datum_resolved_type(
    const struct rt_datum_internal *datum);

/** Validate a NULL-terminated datum chain.  Returns zero when valid.  If
 * messages is non-NULL, validation failures are appended to it. */
RT_EXPORT extern int rt_datum_validate(
    const struct rt_datum_internal *datum,
    struct bu_vls *messages);

__END_DECLS

#endif /* RT_PRIMITIVES_DATUM_H */

