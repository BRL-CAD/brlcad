/*                       S I M U L A T E . H
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
 * The simulate command.
 *
 * Declares structures for passing simulation parameters and
 * hold info regarding rigid bodies
 *
 */

#ifndef SIMULATE_H_
#define SIMULATE_H_

/* interface headers */
#include "vmath.h"


//Copy of the rigid body state tags from btCollisionObject.h
#define ACTIVE_TAG 1
#define ISLAND_SLEEPING 2
#define WANTS_DEACTIVATION 3
#define DISABLE_DEACTIVATION 4
#define DISABLE_SIMULATION 5

#define MAX_CONTACTS_PER_MANIFOLD 4

struct sim_contact {
    vect_t ptA;
    vect_t ptB;
    vect_t normalWorldOnB;
    fastf_t depth;
};


struct sim_manifold {
    struct rigid_body *rbA, *rbB;
    int num_bt_contacts;
    struct sim_contact bt_contacts[MAX_CONTACTS_PER_MANIFOLD];

    int num_rt_contacts;
    struct sim_contact rt_contacts[MAX_CONTACTS_PER_MANIFOLD];

    struct sim_manifold *next;
};


/* Contains information about a single rigid body constructed from a
 * BRL-CAD region.  This structure is the node of a linked list
 * containing the geometry to be added to the sim.
 *
 * TODO: Only the bb is currently present, but physical properties
 * like elasticity, custom forces will be added later.
 */
struct rigid_body {
    int index;
    char *rb_namep;                 /**< @brief pointer to name string */
    point_t bb_min;                 /**< @brief body min bb bounds, only calculated 1st time */
    point_t bb_max;                 /**< @brief body max bb bounds, only calculated 1st time */
    point_t bb_center;              /**< @brief bb center */
    point_t bb_dims;                /**< @brief bb dimensions */
    point_t btbb_min;               /**< @brief Bullet body min bb bounds, updated after each iter. */
    point_t btbb_max;               /**< @brief Bullet body max bb bounds, updated after each iter. */
    point_t btbb_center;            /**< @brief Bullet bb center */
    point_t btbb_dims;              /**< @brief Bullet bb dimensions */
    mat_t m;                        /**< @brief transformation matrix from Bullet */
    mat_t m_prev;                   /**< @brief previous transformation matrix from Bullet */
    int state;                      /**< @brief rigid body state from Bullet */
    struct directory *dp;           /**< @brief directory pointer to the related region */
    struct rigid_body *next;        /**< @brief link to next body */

    /* Can be set by libged or Bullet(checked and inserted into sim) */
    vect_t linear_velocity;         /**< @brief linear velocity components */
    vect_t angular_velocity;        /**< @brief angular velocity components */

    /* Manifold generated, where this body is B, only body B has manifold info */
    int num_manifolds;			 	   /**< @brief number of manifolds for this body */
    struct sim_manifold *head_manifold;/**< @brief head of the manifolds linked list */
};


/* Contains the simulation parameters, such as number of rigid bodies,
 * the head node of the linked list containing the bodies and
 * time/steps for which the simulation will be run.
 */
struct simulation_params {
    int duration;                  /**< @brief contains either the number of steps or the time */
    int num_bodies;                /**< @brief number of rigid bodies participating in the sim */
    struct bu_vls *result_str;     /**< @brief handle to the libged object to access geometry info */
    char *sim_comb_name;           /**< @brief name of the group which contains all sim regions*/
    char *ground_plane_name;       /**< @brief name of the ground plane region */
    struct rigid_body *head_node;  /**< @brief link to first rigid body node */
};


#endif /* SIMULATE_H_ */


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
