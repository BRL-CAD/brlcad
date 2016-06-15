/*                       S I M R T . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
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

#ifndef LIBGED_SIMULATE_SIMRT_H
#define LIBGED_SIMULATE_SIMRT_H

#if defined __cplusplus

/* If the functions in this header have C linkage, this
 * will specify linkage for all C++ language compilers */
extern "C" {
#endif

    /*#include "common.h"*/

    /* System Headers */
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

    /* Public Headers */
#include "vmath.h"
#include "db.h"

    /* Private Headers */
#include "../ged_private.h"
#include "simulate.h"
#include "simutils.h"


    /**
     * Shoots rays within the AABB overlap regions only, to allow more rays to be shot
     * in a grid of finer granularity and to increase performance. The bodies to be targeted
     * are got from the list of manifolds returned by Bullet which carries out AABB
     * intersection checks. These manifolds are stored in the corresponding rigid_body
     * structures of each body participating in the simulation. The manifolds are then used
     * to generate manifolds based on raytracing and stored in a separate list for the body B
     * of that particular manifold. The list is freed in the next iteration in this function
     * as well, to prevent memory leaks, before a new set manifolds are created.
     */

    int
    generate_manifolds(struct simulation_params *sim_params,
		       struct rigid_body *rbA,
		       struct rigid_body *rbB);

#if defined __cplusplus
}   /* matches the linkage specification at the beginning. */
#endif


#endif /* LIBGED_SIMULATE_SIMRT_H */

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
