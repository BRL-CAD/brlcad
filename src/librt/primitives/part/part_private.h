/*                     P A R T _ P R I V A T E . H
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

#ifndef PART_PRIVATE_H
#define PART_PRIVATE_H

#include "common.h"

#include <math.h>

#include "bu/defines.h"
#include "rt/geom.h"
#include "vmath.h"

/* Preserve the import classification thresholds for edited particles. */
#define PART_SPHERE_HEIGHT_RATIO 0.001
#define PART_CYLINDER_RADIUS_RATIO 0.001

static inline int
part_update_type(struct rt_part_internal *part)
{
    if (!part || !isfinite(part->part_vrad) ||
	!isfinite(part->part_hrad) ||
	part->part_vrad < 0.0 || part->part_hrad < 0.0)
	return BRLCAD_ERROR;

    fastf_t maxrad = (part->part_vrad > part->part_hrad) ?
	part->part_vrad : part->part_hrad;
    fastf_t minrad = (part->part_vrad < part->part_hrad) ?
	part->part_vrad : part->part_hrad;
    fastf_t height = MAGNITUDE(part->part_H);
    if (maxrad <= 0.0 || !isfinite(height))
	return BRLCAD_ERROR;

    if (height / maxrad < PART_SPHERE_HEIGHT_RATIO) {
	part->part_vrad = part->part_hrad = maxrad;
	VSETALL(part->part_H, 0);
	part->part_type = RT_PARTICLE_TYPE_SPHERE;
	return BRLCAD_OK;
    }

    if ((maxrad - minrad) / maxrad < PART_CYLINDER_RADIUS_RATIO) {
	part->part_vrad = part->part_hrad = maxrad;
	part->part_type = RT_PARTICLE_TYPE_CYLINDER;
	return BRLCAD_OK;
    }

    part->part_type = RT_PARTICLE_TYPE_CONE;
    return BRLCAD_OK;
}

#endif /* PART_PRIVATE_H */
