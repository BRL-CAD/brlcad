/*                         S I M U L A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/simulate/simulate.h
 *
 * The simulate command.
 *
 * Declares structures for passing simulation parameters and
 * hold info regarding rigid bodies
 *
 */

#ifndef SIMULATE_H_
#define SIMULATE_H_

/* Contains information about a single rigid body constructed from a BRL-CAD region.
 * This structure is the node of a linked list containing the geometry to be added to the sim
 * Only the bb is currently present, but physical properties like elasticity, custom forces
 * will be added later: TODO
 */
struct rigid_body {
    int index;
    char *rb_namep;                 /**< @brief pointer to name string */
    point_t bb_min, bb_max;         /**< @brief body bb bounds */
    point_t bb_center, bb_dims;     /**< @brief bb center and dimensions */
    mat_t m;                        /**< @brief transformation matrix from Bullet */
    struct directory *dp;           /**< @brief directory pointer to the related region */
    struct rigid_body *next;        /**< @brief link to next body */
};

/* Contains the simulation parameters, such as number of rigid bodies,
 * the head node of the linked list containing the bodies and time/steps for
 * which the simulation will be run.
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
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
