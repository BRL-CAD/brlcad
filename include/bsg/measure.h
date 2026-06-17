/*                     M E A S U R E . H
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
/** @file bsg/measure.h */

#ifndef BSG_MEASURE_H
#define BSG_MEASURE_H

#include "common.h"
#include "vmath.h"
#include "bsg/defines.h"

__BEGIN_DECLS

struct bsg_measure_result {
    fastf_t mr_distance;
    fastf_t mr_projection;
    fastf_t mr_normal_alignment;
    int mr_valid;
};

BSG_EXPORT extern int
bsg_measure_candidates(struct bsg_view *v, point_t a, point_t b,
		       struct bsg_measure_result *out);

__END_DECLS

#endif /* BSG_MEASURE_H */
