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
#define TOL 0.04

/*
 * Maximum normals allowed to be detected by ray shots
 */
#define MAX_NORMALS 10


/*
 * This structure is a single node of an array
 * of overlap regions: similar to the one in nirt/usrfmt.h
 */
struct overlap {
    int index;
    struct application *ap;
    struct partition *pp;
    struct region *reg1;
    struct region *reg2;
    fastf_t in_dist;
    fastf_t out_dist;
    point_t in_point;
    point_t out_point;
    vect_t in_normal, out_normal;
    struct soltab *insol, *outsol;
    struct curvature incur, outcur;
};


/*
 * This structure is a single node of an array
 * of hit regions, similar to struct hit from raytrace.h
 */
struct hit_reg {
    int index;
    struct application *ap;
    struct partition *pp;
    const char *reg_name;
    struct soltab *in_stp;
    struct soltab *out_stp;
    fastf_t in_dist;
    fastf_t out_dist;
    point_t in_point;
    point_t out_point;
    vect_t in_normal;
    vect_t out_normal;
    struct curvature cur;
    int	hit_surfno;			/**< @brief solid-specific surface indicator */
};


/**
 * This structure contains the results of analyzing an overlap volume(among 2
 * regions), through shooting rays
 */
struct rayshot_results{

	/* Was an overlap detected ? in that case no point in using air gap
	 * It may happen that an object is not sufficiently close to the object it
	 * intends to strike, i.e. the gap between them is not <= TOL, so the air gap
	 * info can't be used to create contact pairs. In the next iteration the object
	 * moves so far that it has overlapped(i.e. penetrated) the target,
	 * so air gap still can't be used, thus the below flag detects any overlap and
	 * enables the logic for creating contact pairs using overlap info.
	 *
	 * Also rt_result.overlap_found is set to FALSE, before even a single
     * ray is shot and its value is valid across all the different ray shots,
     * so if an overlap has been detected in a ray, all subsequent air gap processing is
     * skipped.
	 */
	int overlap_found;

	/* The vector sum of the normals over the surface in the overlap region for A & B*/
	vect_t resultant_normal_A;
	vect_t resultant_normal_B;

	/* List of normals added to a resultant so far, used to prevent adding a normal again */
	vect_t normals[MAX_NORMALS];
	int num_normals;

	/* The following members are used while shooting rays parallel to the resultant normal */


};


/**
 * Creates the contact pairs from the raytracing results.
 * This is the core logic of the simulation and the manifold points
 * have to satisfy certain constraints (max area within overlap region etc.)
 * to have a successful simulation. The normals and penetration depth is also
 * generated here for each point in the contact pairs. There can be upto 4
 * contact pairs.
 */
int
create_contact_pairs(struct sim_manifold *mf,
					 struct simulation_params *sim_params,
					 vect_t overlap_min, vect_t overlap_max);


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


/**
 * Cleanup the hit list and overlap list: private to simrt
 */
int
cleanup_lists(void);


/**
 * Gets the exact overlap volume between 2 AABBs
 */
int
get_overlap(struct rigid_body *rbA, struct rigid_body *rbB, vect_t overlap_min,
	    vect_t overlap_max);


/**
 * Handles hits, records then in a global list
 * TODO : Stop the ray after it's left the overlap region which is being currently
 * queried.
 */
int
if_hit(struct application *ap, struct partition *part_headp, struct seg *UNUSED(segs));


/**
 * Handles misses while shooting manifold rays,
 * not interested in misses.
 */
int
if_miss(struct application *UNUSED(ap));


/**
 * Handles overlaps while shooting manifold rays,
 * records the overlap regions in a global list
 *
 */
int
if_overlap(struct application *ap, struct partition *pp, struct region *reg1,
	   struct region *reg2, struct partition *InputHdp);


/**
 * Shoots a ray at the simulation geometry and fills up the hit &
 * overlap global list
 */
int
shoot_ray(struct rt_i *rtip,
		  point_t pt,
		  point_t dir);


/**
 * Shoots a grid of rays down x axis
 */
int
shoot_x_rays(struct sim_manifold *current_manifold,
		 struct simulation_params *sim_params,
		 vect_t overlap_min,
		 vect_t overlap_max);


/**
 * Shoots a grid of rays down y axis
 */
int
shoot_y_rays(struct sim_manifold *current_manifold,
	     struct simulation_params *sim_params,
	     vect_t overlap_min,
	     vect_t overlap_max);


/**
 * Shoots a grid of rays down z axis
 */
int
shoot_z_rays(struct sim_manifold *current_manifold,
	     struct simulation_params *sim_params,
	     vect_t overlap_min,
	     vect_t overlap_max);


/*
 * Shoots a circular bunch of rays from B towards A along resultant_normal_B
 *
 */
int
shoot_normal_rays(struct sim_manifold *current_manifold,
			 struct simulation_params *sim_params,
			 vect_t overlap_min,
			 vect_t overlap_max);


/**
 * Traverse the hit list and overlap list, drawing the ray segments
 * for x-rays
 */
int
traverse_xray_lists(struct sim_manifold *current_manifold,
					struct simulation_params *sim_params,
					point_t pt, point_t dir);


/**
 * Traverse the hit list and overlap list, drawing the ray segments
 * for y-rays
 */
int
traverse_yray_lists(
		struct sim_manifold *current_manifold,
		struct simulation_params *sim_params,
		point_t pt, point_t dir);


/**
 * Traverse the hit list and overlap list, drawing the ray segments
 * for z-rays
 */
int
traverse_zray_lists(
		struct sim_manifold *current_manifold,
		struct simulation_params *sim_params,
		point_t pt, point_t dir);


/**
 * Traverse the hit list and overlap list, drawing the ray segments
 * for normal rays
 */
int
traverse_normalray_lists(
		struct sim_manifold *current_manifold,
		struct simulation_params *sim_params,
		point_t pt,
		point_t dir,
		vect_t overlap_min,
		vect_t overlap_max);


/**
 * Initializes the rayshot results structure, called before analyzing
 * each manifold through rays shot in x, y & z directions
 */
int
init_rayshot_results(void);

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
