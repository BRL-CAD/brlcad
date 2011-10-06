/*                         S I M R T . C
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
 * Implements the raytrace based manifold generator.
 *
 * Used for returning the manifold points to physics.
 *
 */

/* Private Header */
#include "simrt.h"


int
get_overlap(struct rigid_body *rbA, struct rigid_body *rbB)
{
	bu_log("Calculating overlap between BB of A %s(%f, %f, %f):(%f,%f,%f) \
			and B %s(%f, %f, %f):(%f,%f,%f)",
			rbA->rb_namep,
			V3ARGS(rbA->btbb_min), V3ARGS(rbA->btbb_max),
			rbB->rb_namep,
			V3ARGS(rbB->btbb_min), V3ARGS(rbB->btbb_max)
		  );
	return GED_OK;
}


int
generate_manifolds(struct simulation_params *sim_params)
{
	/* Shoot lasers */

	struct sim_manifold *current_manifold;
	struct rigid_body *rb;
	/*vect_t overlap_max, overlap_min;*/


	for (rb = sim_params->head_node; rb != NULL; rb = rb->next) {

		for (current_manifold = rb->bt_manifold; current_manifold != NULL;
		 current_manifold = current_manifold->next) {


			get_overlap(current_manifold->rbA, current_manifold->rbB);

			/* make_rpp(gedp, overlap_max, overlap_min, overlap_name); */


		}


	}


	return GED_OK;
}









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
