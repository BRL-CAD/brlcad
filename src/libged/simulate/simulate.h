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
/** @file libged/simulate.c
 *
 * The simulate command.
 *
 * Routines related to performing physics on passed objects only
 *
 *
 */

#ifndef SIMULATE_H_
#define SIMULATE_H_

/* Contains information about a single rigid body constructed from BRL-CAD geometry.
 * This structure is the node of a linked list containing the geometry to be added to the sim
 * Only the bb is currently present, but physical properties like elasticity, custom forces
 * will be added later: TODO
 */
struct rigid_body {
    char * rb_namep;		     /**< @brief pointer to name string */
    point_t bbmin, bbmax;		 /**< @brief body bb bounds */
    mat_t t;					 /**< @brief current transformation matrix */
    struct rigid_body *next;     /**< @brief link to next body */
};

/* Contains the simulation parameters, such as number of rigid bodies,
 * the head node of the linked list containing the bodies and time/steps for
 * which the simulation will be run.
 */
struct simulation_params {
    int duration;				  /**< @brief contains either the number of steps or the time */
    int num_bodies;				  /**< @brief number of rigid bodies participating in the sim */
    struct ged *gedp;			  /**< @brief handle to the libged object to access geometry info */
    struct rigid_body *head_node; /**< @brief link to first rigid body node */
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
