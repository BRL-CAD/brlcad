/*                       S I M R T . H
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/*
 * The header for the raytrace based manifold generator
 * for the simulate command.
 *
 *
 */

#ifndef SIMRT_H_
#define SIMRT_H_

/* System Headers */
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

/* Public Headers */
#include "vmath.h"
#include "db.h"
#include "bu.h"

/* Private Headers */
#include "../ged_private.h"
#include "simulate.h"
#include "simutils.h"


/* Rays will be at least this far apart when shot through the overlap regions
 * This is also the contact threshold for Bullet (0.04 cm if units are in meters)
 * Overlaps regions smaller than this will have only a single plane of rays slicing the
 * region in half, generating manifolds in a plane.
 */
#define GRID_GRANULARITY 0.04


/**
 * Shoots rays within the AABB overlap regions only to allow more rays to be shot
 * in a grid of finer granularity and to increase performance.
 */
int
generate_manifolds(struct simulation_params *sim_params);










#endif /* SIMRT_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
