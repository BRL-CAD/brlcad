/*                         S I M U L A T E . C
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
/** @file libged/simulate/simulate.c
 *
 * The simulate command.
 *
 * Routines related to performing physics on passed objects only
 *
 * 
 */

#include "common.h"

#ifdef HAVE_BULLET

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "bu.h"
#include "raytrace.h"
#include "../ged_private.h"
#include "simulate.h"


/* The C++ simulation function */
extern int run_simulation(struct simulation_params *sim_params);


/**
 * How to use simulate.Blissfully simple interface, more options will be added soon
 */
static void
print_usage(struct bu_vls *str)
{
    bu_vls_printf(str, "Usage: simulate <steps>\n\n");
    bu_vls_printf(str, "Currently this command adds all regions in the model database to a \n\
    	simulation having only gravity as a force. The objects should fall towards the ground plane XY.\n");
    bu_vls_printf(str, "The positions of the regions are set after <steps> number of simulation steps.\n");
    bu_vls_printf(str, "-f <n> <x> <y> <z>\t- Specifies frequency of update(eg 1/60 Hz)(WIP)\n");
    bu_vls_printf(str, "-t <x> <y> <z>\t\t  - Specifies time for which to run(alternative to -n)(WIP)\n");
    return;
}


/**
 * Prints a 16 by 16 transform matrix for debugging
 *
 */
void print_matrix(char *rb_namep, mat_t t)
{
	int i, j;
	char buffer[500];

	sprintf(buffer, "------------Transformation matrix(%s)--------------\n",
			rb_namep);

	for (i=0 ; i<4 ; i++) {
		for (j=0 ; j<4 ; j++) {
			sprintf(buffer, "%st[%d]: %f\t", buffer, (i*4 + j), t[i*4 + j] );
		}
		sprintf(buffer, "%s\n", buffer);
	}

	sprintf(buffer, "%s-------------------------------------------------------\n", buffer);
	bu_log(buffer);
}


/**
 * Prints a struct rigid_body for debugging, more members will be printed later
 */
void print_rigid_body(struct rigid_body *rb)
{
    bu_log("print_rigid_body : \"%s\", state = %d\n", rb->rb_namep, rb->state);
}


void print_manifold_list(struct rigid_body *rb)
{
    struct sim_manifold *current_manifold;
    int i;

    bu_log("print_manifold_list: %s\n", rb->rb_namep);

	for (current_manifold = rb->first_manifold; current_manifold != NULL;
			current_manifold = current_manifold->next) {
		for (i=0; i<current_manifold->num_contacts; i++) {
			bu_log("contact %d of %d, (%f, %f, %f):(%f, %f, %f), n(%f, %f, %f)\n",
					i+1, current_manifold->num_contacts,
					current_manifold->rb_contacts[i].ptA[0],
					current_manifold->rb_contacts[i].ptA[1],
					current_manifold->rb_contacts[i].ptA[2],
					current_manifold->rb_contacts[i].ptB[0],
					current_manifold->rb_contacts[i].ptB[1],
					current_manifold->rb_contacts[i].ptB[2],
					current_manifold->rb_contacts[i].normalWorldOnB[0],
					current_manifold->rb_contacts[i].normalWorldOnB[1],
					current_manifold->rb_contacts[i].normalWorldOnB[2]);
		}
	}
}


/**
 * Deletes a prim/comb if it exists
 * TODO : lower to librt
 */
int kill(struct ged *gedp, char *name)
{
    char *cmd_args[5];

    /* Check if the duplicate already exists, and kill it if so */
    if (db_lookup(gedp->ged_wdbp->dbip, name, LOOKUP_QUIET) != RT_DIR_NULL) {
        bu_log("kill: WARNING \"%s\" exists, deleting it\n", name);
        cmd_args[0] = "kill";
        cmd_args[1] = name;
        cmd_args[2] = (char *)0;

        if(ged_kill(gedp, 2, (const char **)cmd_args) != GED_OK){
            bu_log("kill: ERROR Could not delete existing \"%s\"\n", name);
            return GED_ERROR;
        }
    }

    return GED_OK;
}


/**
 * Deletes and duplicates the prim/comb passed in dp as new_name
 * TODO : lower to librt
 */
int kill_copy(struct ged *gedp, struct directory *dp, char* new_name)
{
    char *cmd_args[5];
    int rv;

    if( kill(gedp, new_name) != GED_OK){
		bu_log("kill_copy: ERROR Could not delete existing \"%s\"\n", new_name);
		return GED_ERROR;
	}

    /* Copy the passed prim/comb */
    cmd_args[0] = "copy";
    cmd_args[1] = dp->d_namep;
    cmd_args[2] = new_name;
    cmd_args[3] = (char *)0;
    rv = ged_copy(gedp, 3, (const char **)cmd_args);
    if (rv != GED_OK){
        bu_log("kill_copy: ERROR Could not copy \"%s\" to \"%s\"\n", dp->d_namep,
                new_name);
        return GED_ERROR;
    }

    return GED_OK;
}


/**
 * Adds a prim/comb to an existing comb or creates it if not existing
 * TODO : lower to librt
 */
int add_to_comb(struct ged *gedp, char *target, char *add)
{
	char *cmd_args[5];
	int rv;

	cmd_args[0] = "comb";
	cmd_args[1] = target;
	cmd_args[2] = "u";
	cmd_args[3] = add;
	cmd_args[4] = (char *)0;
	rv = ged_comb(gedp, 4, (const char **)cmd_args);
	if (rv != GED_OK){
		bu_log("add_to_comb: ERROR Could not add \"%s\" to the combination \"%s\"\n",
				target, add);
		return GED_ERROR;
	}

	return GED_OK;
}


int add_physics_attribs(struct rigid_body *current_node)
{

	VSETALL(current_node->linear_velocity, 0.0f);
	VSETALL(current_node->angular_velocity, 0.0f);

	current_node->num_manifolds = 0;
	current_node->first_manifold = NULL;

	return GED_OK;
}


/**
 * Add the list of regions in the model to the rigid bodies list in
 * simulation parameters. This function will duplicate the existing regions
 * prefixing "sim_" to the new region and putting them all under a new
 * comb "sim.c". It will skip over any existing regions with "sim_" in the name
 */
int add_regions(struct ged *gedp, struct simulation_params *sim_params)
{
    struct directory *dp, *ndp;
    char *prefixed_name;
    char *prefix = "sim_";
    size_t  prefix_len, prefixed_name_len;
    int i;
    struct rigid_body *prev_node = NULL, *current_node;

    /* Kill the existing sim comb */
    kill(gedp, sim_params->sim_comb_name);
    sim_params->num_bodies = 0;

    /* Walk the directory list duplicating all regions only, skip some regions */
    for (i = 0; i < RT_DBNHASH; i++)
        for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
            if ( (dp->d_flags & RT_DIR_HIDDEN) ||  /* check for hidden comb/prim */
                !(dp->d_flags & RT_DIR_REGION)     /* check if region */
            )
                continue;

            if (strstr(dp->d_namep, prefix)){
            	bu_vls_printf(gedp->ged_result_str, "add_regions: Skipping \"%s\" due to \"%s\" in name\n",
                        dp->d_namep, prefix);
                continue;
            }

            /* Duplicate the region */
            prefix_len = strlen(prefix);
            prefixed_name_len = strlen(prefix)+strlen(dp->d_namep)+1;
            prefixed_name = (char *)bu_malloc(prefixed_name_len, "Adding sim_ prefix");
            bu_strlcpy(prefixed_name, prefix, prefix_len + 1);
            bu_strlcat(prefixed_name + prefix_len, dp->d_namep, prefixed_name_len - prefix_len);

            kill_copy(gedp, dp, prefixed_name);
            bu_vls_printf(gedp->ged_result_str, "add_regions: Copied \"%s\" to \"%s\"\n", dp->d_namep, prefixed_name);

            /* Get the directory pointer for the object just added */
            if ((ndp=db_lookup(gedp->ged_wdbp->dbip, prefixed_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
            	bu_vls_printf(gedp->ged_result_str, "add_regions: db_lookup(%s) failed", prefixed_name);
                return GED_ERROR;
            }

            /* Add to simulation list */
            current_node = (struct rigid_body *)bu_malloc(sizeof(struct rigid_body), "rigid_body: current_node");
            current_node->index = sim_params->num_bodies;
            current_node->rb_namep = bu_strdup(prefixed_name);
            current_node->dp = ndp;
            current_node->next = NULL;

            /* Add physics attribs : one shot get from user */
            add_physics_attribs(current_node);

            /* Setup the linked list */
            if(prev_node == NULL){ /* first node */
                prev_node = current_node;
                sim_params->head_node = current_node;
            }
            else{                   /* past 1st node now */
                prev_node->next = current_node;
                prev_node = prev_node->next;
            }

            /* Add the new region to the simulation result */
            add_to_comb(gedp, sim_params->sim_comb_name, prefixed_name);

            sim_params->num_bodies++;
        }




    /* Show list of objects to be added to the sim : keep for debugging as of now */
    /*    bu_log("add_regions: The following %d regions will participate in the sim : \n", sim_params->num_bodies);
    for (current_node = sim_params->head_node; current_node != NULL; current_node = current_node->next) {
        print_rigid_body(current_node);
    }*/

    return GED_OK;

}


int get_bb(struct ged *gedp, struct simulation_params *sim_params)
{
	struct rigid_body *current_node;
	point_t rpp_min, rpp_max;

	/* Free memory in rigid_body list */
	for (current_node = sim_params->head_node; current_node != NULL; current_node = current_node->next) {

		/* Get its BB */
		if(rt_bound_internal(gedp->ged_wdbp->dbip, current_node->dp, rpp_min, rpp_max) == 0)
			bu_log("get_bb: Got the BB for \"%s\" as \
					min {%f %f %f} max {%f %f %f}\n", current_node->dp->d_namep,
					rpp_min[0], rpp_min[1], rpp_min[2],
					rpp_max[0], rpp_max[1], rpp_max[2]);
		else{
			bu_log("get_bb: ERROR Could not get the BB\n");
			return GED_ERROR;
		}

		VMOVE(current_node->bb_min, rpp_min);
		VMOVE(current_node->bb_max, rpp_max);

		/* Get BB length, width, height */
		current_node->bb_dims[0] = current_node->bb_max[0] - current_node->bb_min[0];
		current_node->bb_dims[1] = current_node->bb_max[1] - current_node->bb_min[1];
		current_node->bb_dims[2] = current_node->bb_max[2] - current_node->bb_min[2];

		bu_log("get_bb: Dimensions of this BB : %f %f %f\n",
				  current_node->bb_dims[0], current_node->bb_dims[1], current_node->bb_dims[2]);

		/* Get BB position in 3D space */
		current_node->bb_center[0] = current_node->bb_min[0] + current_node->bb_dims[0]/2;
		current_node->bb_center[1] = current_node->bb_min[1] + current_node->bb_dims[1]/2;
		current_node->bb_center[2] = current_node->bb_min[2] + current_node->bb_dims[2]/2;

		MAT_IDN(current_node->m);
		current_node->m[12] = current_node->bb_center[0];
		current_node->m[13] = current_node->bb_center[1];
		current_node->m[14] = current_node->bb_center[2];

		MAT_COPY(current_node->m_prev, current_node->m);
	}

	return GED_OK;
}

/**
 * This function draws the bounding box around a comb as reported by
 * Bullet
 * TODO : this should be used with a debugging flag
 * TODO : this function will soon be lowered to librt
 *
 */
int insertAABB(struct ged *gedp, struct simulation_params *sim_params,
								 struct rigid_body *current_node)
{
	char* cmd_args[28];
	char buffer[20];
	int rv;
	char *prefixed_name, *prefixed_reg_name;
	char *prefix = "bb_";
	char *prefix_reg = "bb_reg_";
	size_t  prefix_len, prefixed_name_len;
	point_t v;

	/* Prepare prefixed bounding box primitive name */
	prefix_len = strlen(prefix);
	prefixed_name_len = strlen(prefix)+strlen(current_node->rb_namep)+1;
	prefixed_name = (char *)bu_malloc(prefixed_name_len, "Adding bb_ prefix");
	bu_strlcpy(prefixed_name, prefix, prefix_len + 1);
	bu_strlcat(prefixed_name + prefix_len, current_node->rb_namep,
			   prefixed_name_len - prefix_len);

	/* Prepare prefixed bounding box region name */
	prefix_len = strlen(prefix_reg);
	prefixed_name_len = strlen(prefix_reg) + strlen(current_node->rb_namep) + 1;
	prefixed_reg_name = (char *)bu_malloc(prefixed_name_len, "Adding bb_reg_ prefix");
	bu_strlcpy(prefixed_reg_name, prefix_reg, prefix_len + 1);
	bu_strlcat(prefixed_reg_name + prefix_len, current_node->rb_namep,
			   prefixed_name_len - prefix_len);

	/* Delete existing bb prim and region */
	rv = kill(gedp, prefixed_name);
	if (rv != GED_OK){
		bu_log("insertAABB: ERROR Could not delete existing bounding box arb8 : %s \
				so NOT attempting to add new bounding box\n", prefixed_name);
		return GED_ERROR;
	}

	rv = kill(gedp, prefixed_reg_name);
	if (rv != GED_OK){
		bu_log("insertAABB: ERROR Could not delete existing bounding box region : %s \
				so NOT attempting to add new region\n", prefixed_reg_name);
		return GED_ERROR;
	}

	/* Setup the simulation result group union-ing the new objects */
	cmd_args[0] = "in";
	cmd_args[1] = bu_strdup(prefixed_name);
	cmd_args[2] = "arb8";

	/* Front face vertices */
	/* v1 */
	v[0] = current_node->btbb_center[0] + current_node->btbb_dims[0]/2;
	v[1] = current_node->btbb_center[1] + current_node->btbb_dims[1]/2;
	v[2] = current_node->btbb_center[2] - current_node->btbb_dims[2]/2;
	sprintf(buffer, "%f", v[0]); cmd_args[3] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[1]); cmd_args[4] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[2]); cmd_args[5] = bu_strdup(buffer);

	/* v2 */
	v[0] = current_node->btbb_center[0] + current_node->btbb_dims[0]/2;
	v[1] = current_node->btbb_center[1] + current_node->btbb_dims[1]/2;
	v[2] = current_node->btbb_center[2] + current_node->btbb_dims[2]/2;
	sprintf(buffer, "%f", v[0]); cmd_args[6] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[1]); cmd_args[7] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[2]); cmd_args[8] = bu_strdup(buffer);

	/* v3 */
	v[0] = current_node->btbb_center[0] + current_node->btbb_dims[0]/2;
	v[1] = current_node->btbb_center[1] - current_node->btbb_dims[1]/2;
	v[2] = current_node->btbb_center[2] + current_node->btbb_dims[2]/2;
	sprintf(buffer, "%f", v[0]); cmd_args[9]  = bu_strdup(buffer);
	sprintf(buffer, "%f", v[1]); cmd_args[10] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[2]); cmd_args[11] = bu_strdup(buffer);

	/* v4 */
	v[0] = current_node->btbb_center[0] + current_node->btbb_dims[0]/2;
	v[1] = current_node->btbb_center[1] - current_node->btbb_dims[1]/2;
	v[2] = current_node->btbb_center[2] - current_node->btbb_dims[2]/2;
	sprintf(buffer, "%f", v[0]); cmd_args[12] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[1]); cmd_args[13] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[2]); cmd_args[14] = bu_strdup(buffer);

	/* Back face vertices */
	/* v5 */
	v[0] = current_node->btbb_center[0] - current_node->btbb_dims[0]/2;
	v[1] = current_node->btbb_center[1] + current_node->btbb_dims[1]/2;
	v[2] = current_node->btbb_center[2] - current_node->btbb_dims[2]/2;
	sprintf(buffer, "%f", v[0]); cmd_args[15] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[1]); cmd_args[16] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[2]); cmd_args[17] = bu_strdup(buffer);

	/* v6 */
	v[0] = current_node->btbb_center[0] - current_node->btbb_dims[0]/2;
	v[1] = current_node->btbb_center[1] + current_node->btbb_dims[1]/2;
	v[2] = current_node->btbb_center[2] + current_node->btbb_dims[2]/2;
	sprintf(buffer, "%f", v[0]); cmd_args[18] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[1]); cmd_args[19] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[2]); cmd_args[20] = bu_strdup(buffer);

	/* v7 */
	v[0] = current_node->btbb_center[0] - current_node->btbb_dims[0]/2;
	v[1] = current_node->btbb_center[1] - current_node->btbb_dims[1]/2;
	v[2] = current_node->btbb_center[2] + current_node->btbb_dims[2]/2;
	sprintf(buffer, "%f", v[0]); cmd_args[21] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[1]); cmd_args[22] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[2]); cmd_args[23] = bu_strdup(buffer);

	/* v8 */
	v[0] = current_node->btbb_center[0] - current_node->btbb_dims[0]/2;
	v[1] = current_node->btbb_center[1] - current_node->btbb_dims[1]/2;
	v[2] = current_node->btbb_center[2] - current_node->btbb_dims[2]/2;
	sprintf(buffer, "%f", v[0]); cmd_args[24] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[1]); cmd_args[25] = bu_strdup(buffer);
	sprintf(buffer, "%f", v[2]); cmd_args[26] = bu_strdup(buffer);

	/* Finally make the bb primitive, phew ! */
	cmd_args[27] = (char *)0;
	rv = ged_in(gedp, 27, (const char **)cmd_args);
	if (rv != GED_OK){
		bu_log("insertAABB: WARNING Could not draw bounding box for \"%s\"\n",
				current_node->rb_namep);
	}

	/* Make the region for the bb primitive */
	add_to_comb(gedp, prefixed_reg_name, prefixed_name);

	/* Adjust the material for region to be almost transparent */
	cmd_args[0] = "mater";
	cmd_args[1] = bu_strdup(prefixed_reg_name);
	cmd_args[2] = "plastic tr 0.9";
	cmd_args[3] = "210";
	cmd_args[4] = "0";
	cmd_args[5] = "100";
	cmd_args[6] = "0";
	cmd_args[7] = (char *)0;
	rv = ged_mater(gedp, 7, (const char **)cmd_args);
	if (rv != GED_OK){
		bu_log("insertAABB: WARNING Could not adjust the material for \"%s\"\n",
				prefixed_reg_name);
	}

	/* Add the region to the result of the sim so it will be drawn too */
	add_to_comb(gedp, sim_params->sim_comb_name, prefixed_reg_name);

	return GED_OK;

}


/**
 * This function colors the passed comb. It's for showing the current
 * state of the object inside the physics engine.
 * TODO : this should be used with a debugging flag
 *
 */
int apply_color(struct ged *gedp, char* rb_namep, unsigned char r,
											      unsigned char g,
											      unsigned char b )
{
	struct directory *dp = NULL;
	struct rt_comb_internal *comb = NULL;
	struct rt_db_internal intern;
	struct bu_attribute_value_set avs;

	/* Look up directory pointer for the passed comb name */
	GED_DB_LOOKUP(gedp, dp, rb_namep, LOOKUP_NOISY, GED_ERROR);
	GED_CHECK_COMB(gedp, dp, GED_ERROR);
	GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);

	/* Get a comb from the internal format */
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	/* Set the color related members */
	comb->rgb[0] = r;
	comb->rgb[1] = g;
	comb->rgb[2] = b;
	comb->rgb_valid = 1;
	comb->inherit = 0;

	/* Get the current attribute set of the comb from the db */
	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR Cannot get attributes for object %s\n", dp->d_namep);
		bu_avs_free(&avs);
		return GED_ERROR;
	}

	/* Sync the changed attributes with the old ones */
	db5_standardize_avs(&avs);
	db5_sync_comb_to_attr(&avs, comb);
	db5_standardize_avs(&avs);

	/* Put back in db to allow drawing */
	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
	if (db5_update_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
		bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR failed to update attributes\n");
		bu_avs_free(&avs);
		return GED_ERROR;
	}

	bu_avs_free(&avs);
	return GED_OK;
}


/**
 * This function takes the transforms present in the current node and applies them
 * in 3 steps : translate to origin, apply the rotation, then translate to final
 * position with respect to origin(as obtained from physics)
 */
int apply_transforms(struct ged *gedp, struct simulation_params *sim_params)
{
	struct rt_db_internal intern;
	struct rigid_body *current_node;
	mat_t t , m;
	/*int rv;*/

	for (current_node = sim_params->head_node; current_node != NULL; current_node = current_node->next) {

		/*if(strcmp(current_node->rb_namep, sim_params->ground_plane_name) == 0)
			continue;*/

		/* Get the internal representation of the object */
		GED_DB_GET_INTERNAL(gedp, &intern, current_node->dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

		bu_log("Got this matrix for current iteration :");
		print_matrix(current_node->dp->d_namep, current_node->m);

		bu_log("Previous iteration matrix:");
		print_matrix(current_node->dp->d_namep, current_node->m_prev);

		/* Translate to origin without any rotation, before applying rotation */
		MAT_IDN(m);
		m[12] = - (current_node->m_prev[12]);
		m[13] = - (current_node->m_prev[13]);
		m[14] = - (current_node->m_prev[14]);
		MAT_TRANSPOSE(t, m);
		if (rt_matrix_transform(&intern, t, &intern, 0, gedp->ged_wdbp->dbip, &rt_uniresource) < 0){
			bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR rt_matrix_transform(%s) failed while \
					translating to origin!\n",
					current_node->dp->d_namep);
			return GED_ERROR;
		}

		bu_log("Translating back : %f, %f, %f", m[12], m[13], m[14]);
		print_matrix(current_node->dp->d_namep, t);

		/* Apply inverse rotation with no translation to undo previous iteration's rotation */
		MAT_COPY(m, current_node->m_prev);
		m[12] = 0;
		m[13] = 0;
		m[14] = 0;
		MAT_COPY(t, m);
		if (rt_matrix_transform(&intern, t, &intern, 0, gedp->ged_wdbp->dbip, &rt_uniresource) < 0){
			bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR rt_matrix_transform(%s) failed while \
					applying rotation\n",
					current_node->dp->d_namep);
			return GED_ERROR;
		}

		bu_log("Rotating back :");
		print_matrix(current_node->dp->d_namep, t);

		/*---------------------- Now apply current transformation -------------------------*/

		/* Apply rotation with no translation*/
		MAT_COPY(m, current_node->m);
		m[12] = 0;
		m[13] = 0;
		m[14] = 0;
		MAT_TRANSPOSE(t, m);
		if (rt_matrix_transform(&intern, t, &intern, 0, gedp->ged_wdbp->dbip, &rt_uniresource) < 0){
			bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR rt_matrix_transform(%s) failed while \
					applying rotation\n",
					current_node->dp->d_namep);
			return GED_ERROR;
		}

		bu_log("Rotating forward :");
		print_matrix(current_node->dp->d_namep, t);


		/* Translate again without any rotation, to apply final position */
		MAT_IDN(m);
		m[12] = current_node->m[12];
		m[13] = current_node->m[13];
		m[14] = current_node->m[14];
		MAT_TRANSPOSE(t, m);

		bu_log("Translating forward by %f, %f, %f", m[12], m[13], m[14]);
		print_matrix(current_node->dp->d_namep, t);

		if (rt_matrix_transform(&intern, t, &intern, 0, gedp->ged_wdbp->dbip, &rt_uniresource) < 0){
			bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR rt_matrix_transform(%s) failed while \
					translating to final position\n",
					current_node->dp->d_namep);
			return GED_ERROR;
		}



		/* Write the modified solid to the db so it can be redrawn at the new position & orientation by Mged */
		if (rt_db_put_internal(current_node->dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
			bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR Database write error for '%s', aborting\n",
					current_node->dp->d_namep);
			return GED_ERROR;
		}

		/*current_node->bb_center[0] = m[12];
		current_node->bb_center[1] = m[13];
		current_node->bb_center[2] = m[14];*/

		/* Store this world transformation to undo it before next world transformation */
		MAT_COPY(current_node->m_prev, current_node->m);

		insertAABB(gedp, sim_params, current_node);

		print_manifold_list(current_node);


	}

    return GED_OK;
}

/**
 * Frees the list of manifolds as generated by Bullet: this list is used
 * to compare with rt generated manifold for accuracy
 *
 */
int free_manifold_lists(struct simulation_params *sim_params)
{
	/* Free memory in manifold list */
    struct sim_manifold *current_manifold, *next_manifold;
    struct rigid_body *current_node;

    for (current_node = sim_params->head_node; current_node != NULL;
    		current_node = current_node->next) {

		for (current_manifold = current_node->first_manifold; current_manifold != NULL; ) {

			next_manifold = current_manifold->next;
			bu_free(current_manifold, "simulate : current_manifold");
			current_manifold = next_manifold;
			current_node->num_manifolds--;
		}

		current_node->num_manifolds = 0;
		current_node->first_manifold = NULL;
	}

    return GED_OK;
}


/**
 * The libged physics simulation function :
 * Check flags, adds regions to simulation parameters, runs the simulation
 * applies the transforms, frees memory
 */
int
ged_simulate(struct ged *gedp, int argc, const char *argv[])
{
    int rv, i;
    struct simulation_params sim_params;
    static const char *sim_comb_name = "sim.c";
    static const char *ground_plane_name = "sim_gp.r";
    struct rigid_body *current_node, *next_node;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* Initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Must be wanting help */
    if (argc == 1) {
        print_usage(gedp->ged_result_str);
        return GED_HELP;
    }

    if (argc < 2) {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s <steps>", argv[0]);
        return GED_ERROR;
    }
    

    /* Make a list containing the bb and existing transforms of all the objects in the model
     * which will participate in the simulation
     */
    sim_params.duration  = atoi(argv[1]);
    sim_params.result_str = gedp->ged_result_str;
    sim_params.sim_comb_name = bu_strdup(sim_comb_name);
    sim_params.ground_plane_name = bu_strdup(ground_plane_name);

	rv = add_regions(gedp, &sim_params);
	if (rv != GED_OK){
		bu_vls_printf(gedp->ged_result_str, "%s: ERROR while adding objects and sim attributes\n", argv[0]);
		return GED_ERROR;
	}

	rv = get_bb(gedp, &sim_params);
	if (rv != GED_OK){
		bu_vls_printf(gedp->ged_result_str, "%s: ERROR while getting bounding boxes\n", argv[0]);
		return GED_ERROR;
	}

    for (i=0 ; i < sim_params.duration ; i++) {

    	bu_log("%s: ------------------------- Iteration %d -----------------------\n", argv[0], i+1);



    	/* Generate manifolds using rt */
    	//generate_manifolds(sim_params);

    	/* Run the physics simulation  */
		rv = run_simulation(&sim_params);
		if (rv != GED_OK){
			bu_vls_printf(gedp->ged_result_str, "%s: ERROR while running the simulation\n", argv[0]);
			return GED_ERROR;
		}

		/* Apply transforms on the participating objects, also shades objects */
		rv = apply_transforms(gedp, &sim_params);
		if (rv != GED_OK){
			bu_vls_printf(gedp->ged_result_str, "%s: ERROR while applying transforms\n", argv[0]);
			return GED_ERROR;
		}

		free_manifold_lists(&sim_params);

    }


    /* Free memory in rigid_body list */
	for (current_node = sim_params.head_node; current_node != NULL; ) {
		next_node = current_node->next;
		bu_free(current_node, "simulate : current_node");
		current_node = next_node;
		sim_params.num_bodies--;
	}


    bu_vls_printf(gedp->ged_result_str, "%s: The simulation result is in group : %s\n", argv[0], sim_comb_name);
                                                     
    /* Draw the result : inserting it in argv[1] will cause it to be automatically drawn in the cmd_wrapper */
    argv[1] = sim_comb_name;
    argv[2] = (char *)0;

    return GED_OK;
}


#else

#include "../ged_private.h"

/**
 * Dummy simulation function in case no physics library found
 */
int
ged_simulate(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* Initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_vls_printf(gedp->ged_result_str, "%s : ERROR This command is disabled due to the absence of a physics library",
            argv[0]);
    return GED_ERROR;
}

#endif






/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
