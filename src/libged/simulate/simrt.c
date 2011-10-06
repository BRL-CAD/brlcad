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
get_overlap(struct rigid_body *rbA, struct rigid_body *rbB, vect_t overlap_min, vect_t overlap_max)
{
	bu_log("Calculating overlap between BB of %s(%f, %f, %f):(%f,%f,%f) \
			and %s(%f, %f, %f):(%f,%f,%f)",
			rbA->rb_namep,
			V3ARGS(rbA->btbb_min), V3ARGS(rbA->btbb_max),
			rbB->rb_namep,
			V3ARGS(rbB->btbb_min), V3ARGS(rbB->btbb_max)
		  );

	VMOVE(overlap_max, rbA->btbb_max);
	VMIN(overlap_max, rbB->btbb_max);

	VMOVE(overlap_min, rbA->btbb_min);
	VMAX(overlap_min, rbB->btbb_min);

	bu_log("Overlap volume between %s & %s is (%f, %f, %f):(%f,%f,%f)",
			rbA->rb_namep,
			rbB->rb_namep,
			V3ARGS(overlap_min),
			V3ARGS(overlap_max)
			);


	return GED_OK;
}


int
generate_manifolds(struct ged *gedp, struct simulation_params *sim_params)
{
	struct sim_manifold *current_manifold;
	struct rigid_body *rb;
	vect_t overlap_min, overlap_max;
	char *prefix_overlap = "overlap_";
	struct bu_vls overlap_name = BU_VLS_INIT_ZERO;

	/* Add all sim objects to raytrace instance */


	/* Check all rigid bodies for overlaps using their manifold lists */
	for (rb = sim_params->head_node; rb != NULL; rb = rb->next) {

		for (current_manifold = rb->head_manifold; current_manifold != NULL;
		 current_manifold = current_manifold->next) {

			/* Get the overlap region */
			get_overlap(current_manifold->rbA, current_manifold->rbB,
					overlap_min,
					overlap_max);

			/* Prepare the overlap prim name */
			bu_vls_sprintf(&overlap_name, "%s_%s_%s",
					prefix_overlap,
					current_manifold->rbA->rb_namep,
					current_manifold->rbB->rb_namep);

			/* Make the overlap volume RPP */
			make_rpp(gedp, overlap_min, overlap_max, bu_vls_addr(&overlap_name));

			/* Add the region to the result of the sim so it will be drawn too */
			add_to_comb(gedp, sim_params->sim_comb_name, bu_vls_addr(&overlap_name));

			/* Shoot rays right here as the pair of rigid_body ptrs are known, ignore volumes already shot */

			/* Note down this volume as already covered, so no need to shoot rays through it again */


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
