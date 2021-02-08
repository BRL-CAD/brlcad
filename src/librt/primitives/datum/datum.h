/*                         D A T U M . H
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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
/** @file datum.h
 *
 * Intersect a ray with an 'datum' primitive object.
 *
 */

#ifndef LIBRT_PRIMITIVES_DATUM_DATUM_H
#define LIBRT_PRIMITIVES_DATUM_DATUM_H

#include "common.h"

#include "rt/geom.h"


/* ray tracing form of solid, including precomputed terms */
struct datum_specific {
    struct rt_datum_internal *datum;
    size_t count;
};

#endif /* LIBRT_PRIMITIVES_DATUM_DATUM_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
