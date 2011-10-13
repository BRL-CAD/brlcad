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
#define TOL 0.04


/*
 * This structure is a single node of a circularly linked list
 * of overlap regions: similar to the one in nirt/usrfrmt.h
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
    struct overlap *forw;
    struct overlap *backw;
};


/*
 * This structure is a single node of a circularly linked list
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
    struct hit_reg *forw;
    struct hit_reg *backw;
};


struct rayshot_results{
    /* Results of shooting rays towards -ve x-axis : xr means x rays */
    fastf_t xr_min_x_x, xr_min_x_y, xr_min_x_z;  /* the min X found while shooting x rays & rltd y,z*/
    fastf_t xr_max_x_x, xr_max_x_y, xr_max_x_z;  /* the max X found while shooting x rays & rltd y,z*/
    fastf_t xr_min_y, xr_min_y_z; /* the min y where overlap was found & the z co-ord for it*/
    fastf_t xr_max_y, xr_max_y_z; /* the max y where overlap was still found */
    fastf_t xr_min_z, xr_min_z_y; /* the min z where overlap was found & the y co-ord for it*/
    fastf_t xr_max_z, xr_max_z_y; /* the max z where overlap was still found */

    /* Results of shooting rays down y axis */


    /* Results of shooting rays down z axis */

};


/**
 * Creates the contact pairs from the raytracing results
 *
 */
int
create_contact_pairs(struct sim_manifold *mf);


/**
 * Shoots rays within the AABB overlap regions only, to allow more rays to be shot
 * in a grid of finer granularity and to increase performance.
 */
int
generate_manifolds(struct ged *gedp, struct simulation_params *sim_params);


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
shoot_ray(struct rt_i *rtip, point_t pt, point_t dir);


/**
 * Shoots a grid of rays down x axis
 */
int
shoot_x_rays(
	     struct ged *gedp,
	     struct sim_manifold *current_manifold,
	     struct simulation_params *sim_params,
	     struct rt_i *rtip,
	     vect_t overlap_min, vect_t overlap_max);


/**
 * Initializes the simulation scene for raytracing
 */
int
init_raytrace(struct simulation_params *sim_params, struct rt_i *rtip);


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
