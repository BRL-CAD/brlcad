/*                       S I M U T I L S . H
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
 * The header for the utility functions for the simulate command.
 *
 *
 */

#ifndef SIMUTILS_H_
#define SIMUTILS_H_

#if defined __cplusplus

    /* If the functions in this header have C linkage, this
     * will specify linkage for all C++ language compilers */
    extern "C" {
#endif

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

/**
 * How to use simulate.Blissfully simple interface, more options will be added soon
 */
void
print_usage(struct bu_vls *str);


/**
 * Prints a 16 by 16 transform matrix for debugging
 *
 */
void
print_matrix(char *rb_namep, mat_t t);


/**
 * Prints a struct rigid_body for debugging, more members will be printed later
 */
void
print_rigid_body(struct rigid_body *rb);


/**
 * Prints the list of contacts in each manifold of a rigid body
 */
void
print_manifold_list(struct rigid_body *rb);


/**
 * Prints the args of a command to be executed using libged
 */
void
print_command(char* cmd_args[], int num_args);


/**
 * Deletes a prim/comb if it exists.
 *
 * TODO: lower to librt
 */
int
kill(struct ged *gedp, char *name);


/**
 * Deletes and duplicates the prim/comb passed in dp as new_name.
 *
 * TODO : lower to librt
 */
int
kill_copy(struct ged *gedp, struct directory *dp, char* new_name);


/**
 * Adds a prim/comb to an existing comb or creates it if not existing.
 *
 * TODO: lower to librt
 */
int
add_to_comb(struct ged *gedp, char *target, char *add);


/**
 * Draw a line from,to and color it
 */
int
line(struct ged *gedp, char* name, point_t from, point_t to,
     unsigned char r,
     unsigned char g,
     unsigned char b);


/**
 * Draws an arrow from, to using the BOT primitive & TRC
 * Used to draw manifold normals
 * TODO: find a simpler way
 */
int
arrow(struct ged *gedp, char* name, point_t from, point_t to);


/**
 * Applies a material to passed comb using libged
 * TODO: lower to librt
 */
int
apply_material(struct ged *gedp,
	       char* comb,
	       char* material,
	       unsigned char r,
	       unsigned char g,
	       unsigned char b);


/**
 * This function colors the passed comb. It's for showing the current
 * state of the object inside the physics engine.
 *
 * TODO : this should be used with a debugging flag
 */
int
apply_color(struct ged *gedp,
	    char* name,
	    unsigned char r,
	    unsigned char g,
	    unsigned char b);


/**
 * This function draws the bounding box around a comb as reported by
 * Bullet.
 *
 * TODO: this should be used with a debugging flag
 * TODO: this function will soon be lowered to librt
 */
int
insert_AABB(struct ged *gedp,
	    struct simulation_params *sim_params,
	    struct rigid_body *current_node);


/**
 * This function inserts a manifold comb as reported by Bullet.
 *
 * TODO: this should be used with a debugging flag
 * TODO: this function should be lowered to librt
 */
int
insert_manifolds(struct ged *gedp, struct simulation_params *sim_params, struct rigid_body *rb);


/**
 * This function creates and inserts a RPP
 * Used to show AABB overlap volume
 *
 * TODO: this function should be lowered to librt
 */
int
make_rpp(struct ged *gedp, vect_t max, vect_t min, char* name);


#if defined __cplusplus
    }   /* matches the linkage specification at the beginning. */
#endif


#endif /* SIMUTILS_H_ */

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
