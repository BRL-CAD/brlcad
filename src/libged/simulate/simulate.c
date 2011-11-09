/*                         S I M U L A T E . C
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
 * Routines related to performing physics on passed objects only
 *
 */

#include "common.h"

#ifdef HAVE_BULLET

/* System Headers */
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "bio.h"

/* Public Headers */
#include "vmath.h"
#include "db.h"
#include "bu.h"
#include "raytrace.h"

/* Private Headers */
#include "../ged_private.h"
#include "simulate.h"
#include "simutils.h"

/*
 * This structure is a single node of a circularly linked list
 * of overlap regions: similar to the one in nirt/usrfrmt.h
 */
struct overlap1 {
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
    struct overlap *forw;
    struct overlap *backw;
};



/* The C++ simulation function */
extern int run_simulation(struct simulation_params *sim_params);


/**
 * Adds physics specific attributes, will be used to add some more properties later
 */
int
add_physics_attribs(struct rigid_body *current_node)
{

    VSETALL(current_node->linear_velocity, 0.0f);
    VSETALL(current_node->angular_velocity, 0.0f);

    current_node->num_bt_manifolds = 0;

    return GED_OK;
}


/**
 * Add the list of regions in the model to the rigid bodies list in
 * simulation parameters. This function will duplicate the existing
 * regions prefixing "sim_" to the new region and putting them all
 * under a new comb "sim.c". It will skip over any existing regions
 * with "sim_" in the name
 */
int
add_regions(struct ged *gedp, struct simulation_params *sim_params)
{
    struct directory *dp, *ndp;
    char *prefix = "sim_";
    int i;
    struct rigid_body *prev_node = NULL, *current_node;
    struct bu_vls dp_name_vls = BU_VLS_INIT_ZERO;

    /* Kill the existing sim comb */
    kill(gedp, sim_params->sim_comb_name);
    sim_params->num_bodies = 0;

    /* Walk the directory list duplicating all regions only, skip some regions */
    for (i = 0; i < RT_DBNHASH; i++)
	for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if ((dp->d_flags & RT_DIR_HIDDEN) ||  /* check for hidden comb/prim */
		!(dp->d_flags & RT_DIR_REGION)     /* check if region */
		)
		continue;

	    if (strstr(dp->d_namep, prefix)) {
		bu_vls_printf(gedp->ged_result_str, "add_regions: Skipping \"%s\" due to \"%s\" in name\n",
			      dp->d_namep, prefix);
		continue;
	    }

	    if (BU_STR_EMPTY(dp->d_namep)) {
		bu_vls_printf(gedp->ged_result_str, "add_regions: Skipping \"%s\" due to empty name\n",
			      dp->d_namep);
		continue;
	    }

	    /* Duplicate the region */
	    bu_vls_sprintf(&dp_name_vls, "%s%s", prefix, dp->d_namep);

	    kill_copy(gedp, dp, bu_vls_addr(&dp_name_vls));
	    bu_vls_printf(gedp->ged_result_str, "add_regions: Copied \"%s\" to \"%s\"\n", dp->d_namep,
			  bu_vls_addr(&dp_name_vls));

	    /* Get the directory pointer for the object just added */
	    if ((ndp=db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&dp_name_vls), LOOKUP_QUIET)) == RT_DIR_NULL) {
	    	bu_vls_printf(gedp->ged_result_str, "add_regions: db_lookup(%s) failed", bu_vls_addr(&dp_name_vls));
	    	return GED_ERROR;
	    }


	    /* Add to simulation list */
	    current_node = (struct rigid_body *)bu_malloc(sizeof(struct rigid_body), "rigid_body: current_node");
	    current_node->index = sim_params->num_bodies;
	    current_node->rb_namep = bu_strdup(bu_vls_addr(&dp_name_vls));
	    current_node->dp = ndp;
	    current_node->next = NULL;

	    /* Save the internal format as well */
		if (!rt_db_lookup_internal(sim_params->gedp->ged_wdbp->dbip, ndp->d_namep, &ndp,
				&(current_node->intern), LOOKUP_NOISY, &rt_uniresource)) {
			bu_exit(1, "add_regions: ERROR rt_db_lookup_internal(%s) failed to get the internal form",
					ndp->d_namep);
			return GED_ERROR;
		}

	    /* Add physics attribs : one shot get from user */
	    add_physics_attribs(current_node);

	    /* Setup the linked list */
	    if (prev_node == NULL) {
	    	/* first node */
	    	prev_node = current_node;
	    	sim_params->head_node = current_node;
	    } else {
	    	/* past 1st node now */
	    	prev_node->next = current_node;
	    	prev_node = prev_node->next;
	    }

	    /* Add the new region to the simulation result */
	    add_to_comb(gedp, sim_params->sim_comb_name, bu_vls_addr(&dp_name_vls));

	    sim_params->num_bodies++;
	}

    bu_vls_free(&dp_name_vls);

    if(sim_params->num_bodies == 0){
	bu_vls_printf(gedp->ged_result_str, "add_regions: ERROR No objects were added\n");
	return GED_ERROR;
    }
  

    /* Show list of objects to be added to the sim : keep for debugging as of now */
    /* bu_log("add_regions: The following %d regions will participate in the sim : \n", sim_params->num_bodies);
       for (current_node = sim_params->head_node; current_node != NULL; current_node = current_node->next) {
       print_rigid_body(current_node);
       }*/

    return GED_OK;

}


/**
 * Gets the bounding box of all the combs which will participate in the simulation
 */
int
get_bb(struct ged *gedp, struct simulation_params *sim_params)
{
    struct rigid_body *current_node;
    point_t rpp_min, rpp_max;

    /* Free memory in rigid_body list */
    for (current_node = sim_params->head_node; current_node != NULL; current_node = current_node->next) {

	/* Get its BB */
	if (rt_bound_internal(gedp->ged_wdbp->dbip, current_node->dp, rpp_min, rpp_max) == 0) {
	    bu_log("get_bb: Got the BB for \"%s\" as \
					min {%f %f %f} max {%f %f %f}\n", current_node->dp->d_namep,
		   V3ARGS(rpp_min),
		   V3ARGS(rpp_max));
	} else {
	    bu_log("get_bb: ERROR Could not get the BB\n");
	    return GED_ERROR;
	}

	VMOVE(current_node->bb_min, rpp_min);
	VMOVE(current_node->bb_max, rpp_max);

	/* Get BB length, width, height */
	VSUB2(current_node->bb_dims, current_node->bb_max, current_node->bb_min);

	bu_log("get_bb: Dimensions of this BB : %f %f %f\n", V3ARGS(current_node->bb_dims));

	/* Get BB position in 3D space */
	VSCALE(current_node->bb_center, current_node->bb_dims, 0.5);
	VADD2(current_node->bb_center, current_node->bb_center, current_node->bb_min)

	    MAT_IDN(current_node->m);
	current_node->m[12] = current_node->bb_center[0];
	current_node->m[13] = current_node->bb_center[1];
	current_node->m[14] = current_node->bb_center[2];

	MAT_COPY(current_node->m_prev, current_node->m);
    }

    return GED_OK;
}


/**
 * This function takes the transforms present in the current node and
 * applies them in 3 steps : translate to origin, apply the rotation,
 * then translate to final position with respect to origin(as obtained
 * from physics)
 */
int
apply_transforms(struct ged *gedp, struct simulation_params *sim_params)
{
    struct rt_db_internal intern;
    struct rigid_body *current_node;
    mat_t t , m;
    /*int rv;*/

    for (current_node = sim_params->head_node; current_node != NULL; current_node = current_node->next) {

		/*if (strcmp(current_node->rb_namep, sim_params->ground_plane_name) == 0)
		  continue;*/

		/* Get the internal representation of the object */
		GED_DB_GET_INTERNAL(gedp, &intern, current_node->dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

		/*bu_log("Got this matrix for current iteration :");
		  print_matrix(current_node->dp->d_namep, current_node->m); */

		/*bu_log("Previous iteration matrix:");
		  print_matrix(current_node->dp->d_namep, current_node->m_prev); */

		/* Translate to origin without any rotation, before applying rotation */
		MAT_IDN(m);
		m[12] = - (current_node->m_prev[12]);
		m[13] = - (current_node->m_prev[13]);
		m[14] = - (current_node->m_prev[14]);
		MAT_TRANSPOSE(t, m);
		if (rt_matrix_transform(&intern, t, &intern, 0, gedp->ged_wdbp->dbip, &rt_uniresource) < 0) {
			bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR rt_matrix_transform(%s) failed while \
				translating to origin!\n",
				  current_node->dp->d_namep);
			return GED_ERROR;
		}

		/*bu_log("Translating back : %f, %f, %f", m[12], m[13], m[14]);
		  print_matrix(current_node->dp->d_namep, t); */

		/* Apply inverse rotation with no translation to undo previous iteration's rotation */
		MAT_COPY(m, current_node->m_prev);
		m[12] = 0;
		m[13] = 0;
		m[14] = 0;
		MAT_COPY(t, m);
		if (rt_matrix_transform(&intern, t, &intern, 0, gedp->ged_wdbp->dbip, &rt_uniresource) < 0) {
			bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR rt_matrix_transform(%s) failed while \
				applying rotation\n",
				  current_node->dp->d_namep);
			return GED_ERROR;
		}

		/*bu_log("Rotating back :");
		  print_matrix(current_node->dp->d_namep, t);*/

		/*---------------------- Now apply current transformation -------------------------*/

		/* Apply rotation with no translation*/
		MAT_COPY(m, current_node->m);
		m[12] = 0;
		m[13] = 0;
		m[14] = 0;
		MAT_TRANSPOSE(t, m);
		if (rt_matrix_transform(&intern, t, &intern, 0, gedp->ged_wdbp->dbip, &rt_uniresource) < 0) {
			bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR rt_matrix_transform(%s) failed while \
				applying rotation\n",
				  current_node->dp->d_namep);
			return GED_ERROR;
		}

		/*bu_log("Rotating forward :");
		  print_matrix(current_node->dp->d_namep, t);*/


		/* Translate again without any rotation, to apply final position */
		MAT_IDN(m);
		m[12] = current_node->m[12];
		m[13] = current_node->m[13];
		m[14] = current_node->m[14];
		MAT_TRANSPOSE(t, m);

		/*bu_log("Translating forward by %f, %f, %f", m[12], m[13], m[14]);
		  print_matrix(current_node->dp->d_namep, t);*/

		if (rt_matrix_transform(&intern, t, &intern, 0, gedp->ged_wdbp->dbip, &rt_uniresource) < 0) {
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

		/* Store this world transformation to undo it before next world transformation */
		MAT_COPY(current_node->m_prev, current_node->m);

		insert_AABB(gedp, sim_params, current_node);

		print_manifold_list(current_node);

		insert_manifolds(gedp, sim_params, current_node);

		current_node->num_bt_manifolds = 0;

    }

    return GED_OK;
}


/**
 * Will recreate the simulation comb, to clear the AABB and manifold regions
 * of previous iteration
 */
int recreate_sim_comb(struct ged *gedp, struct simulation_params *sim_params)
{
    struct rigid_body *current_node;

    if (kill(gedp, sim_params->sim_comb_name) != GED_OK) {
	bu_log("kill_copy: ERROR Could not delete existing \"%s\"\n", sim_params->sim_comb_name);
	return GED_ERROR;
    }

    for (current_node = sim_params->head_node; current_node != NULL;
	 current_node = current_node->next) {
	add_to_comb(gedp, sim_params->sim_comb_name, current_node->rb_namep);
    }

    return GED_OK;
}


/**
 * Initializes the simulation scene for raytracing
 */
int
init_raytrace(struct simulation_params *sim_params)
{
    struct rigid_body *rb;

    /* Add all sim objects to raytrace instance */

    /* Add all the sim objects to the rt_i */
    for (rb = sim_params->head_node; rb != NULL; rb = rb->next) {
		if (rt_gettree(sim_params->rtip, rb->rb_namep) < 0)
			bu_log("init_raytrace: Failed to load geometry for [%s]\n",
			   rb->rb_namep);
		else
			bu_log("init_raytrace: Added [%s] to raytracer\n", rb->rb_namep);
    }

    /* This next call causes some values to be pre-computed, sets up space
     * partitioning, computes bounding volumes, etc.
     */
    rt_prep_parallel(sim_params->rtip, 1);

    bu_log("init_raytrace: Simulation objects bounding box (%f, %f, %f):(%f,%f,%f)",
	   V3ARGS(sim_params->rtip->mdl_min), V3ARGS(sim_params->rtip->mdl_max));


    return GED_OK;
}


int
if_hit1(struct application *ap, struct partition *part_headp, struct seg *UNUSED(segs))
{

    /* iterating over partitions, this will keep track of the current
     * partition we're working on.
     */
    struct partition *pp;

    /* will serve as a pointer for the entry and exit hitpoints */
    struct hit *hitp;

    /* will serve as a pointer to the solid primitive we hit */
    struct soltab *stp;

    /* will contain surface curvature information at the entry */
    struct curvature cur;

    /* will contain our hit point coordinate */
    point_t in_pt, out_pt;

    /* will contain normal vector where ray enters geometry */
    vect_t inormal;

    /* will contain normal vector where ray exits geometry */
    vect_t onormal;

    int i = 0;

    /* iterate over each partition until we get back to the head.
     * each partition corresponds to a specific homogeneous region of
     * material.
     */
    for (pp=part_headp->pt_forw; pp != part_headp; pp = pp->pt_forw) {



		/* print the name of the region we hit as well as the name of
		 * the primitives encountered on entry and exit.
		 */
		bu_log("\n--- Hit region %s (in %s, out %s)\n",
		pp->pt_regionp->reg_name,
		pp->pt_inseg->seg_stp->st_name,
		pp->pt_outseg->seg_stp->st_name );


		/* entry hit point, so we type less */
		hitp = pp->pt_inhit;

		/* construct the actual (entry) hit-point from the ray and the
		 * distance to the intersection point (i.e., the 't' value).
		 */
		VJOIN1(in_pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);

		/* primitive we encountered on entry */
		stp = pp->pt_inseg->seg_stp;

		/* compute the normal vector at the entry point, flipping the
		 * normal if necessary.
		 */
		RT_HIT_NORMAL(inormal, hitp, stp, &(ap->a_ray), pp->pt_inflip);

		/* print the entry hit point info */
		rt_pr_hit("  In", hitp);
		VPRINT(   "  Ipoint", in_pt);
		VPRINT(   "  Inormal", inormal);


		if (pp->pt_overlap_reg) {
			struct region *pp_reg;
			int j = -1;

			bu_log("    Claiming regions:\n");
			while ((pp_reg = pp->pt_overlap_reg[++j]))
				bu_log("        %s\n", pp_reg->reg_name);
		}

		/* This next macro fills in the curvature information which
		 * consists of a principle direction vector, and the inverse
		 * radii of curvature along that direction and perpendicular
		 * to it.  Positive curvature bends toward the outward
		 * pointing normal.
		 */
		RT_CURVATURE(&cur, hitp, pp->pt_inflip, stp);

		/* print the entry curvature information */
		VPRINT("PDir", cur.crv_pdir);
		bu_log(" c1=%g\n", cur.crv_c1);
		bu_log(" c2=%g\n", cur.crv_c2);


		/* exit point, so we type less */
		hitp = pp->pt_outhit;

		/* construct the actual (exit) hit-point from the ray and the
		 * distance to the intersection point (i.e., the 't' value).
		 */
		VJOIN1(out_pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);

		/* primitive we exited from */
		stp = pp->pt_outseg->seg_stp;

		/* compute the normal vector at the exit point, flipping the
		 * normal if necessary.
		 */
		RT_HIT_NORMAL(onormal, hitp, stp, &(ap->a_ray), pp->pt_outflip);

		/* print the exit hit point info */
		rt_pr_hit("  Out", hitp);
		VPRINT(   "  Opoint", out_pt);
		VPRINT(   "  Onormal", onormal);
    }

    return HIT;
}


int
if_miss1(struct application *UNUSED(ap))
{
    bu_log("MISS");
    return MISS;
}


int
if_overlap1(struct application *ap, struct partition *pp, struct region *reg1,
	   struct region *reg2, struct partition *InputHdp)
{
    int i = 0;

    struct overlap1 ovlp;

    bu_log("if_overlap: OVERLAP between %s and %s", reg1->reg_name, reg2->reg_name);

   	bu_log("\n--- Overlap region (in %s, out %s)\n",			
			pp->pt_inseg->seg_stp->st_name,
			pp->pt_outseg->seg_stp->st_name );


	/* compute the normal vector at the exit point, flipping the
	 * normal if necessary.
	 */
	RT_HIT_NORMAL(ovlp.in_normal, pp->pt_inhit,
			  pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip);


	/* compute the normal vector at the exit point, flipping the
	 * normal if necessary.
	 */
	RT_HIT_NORMAL(ovlp.out_normal, pp->pt_outhit,
			  pp->pt_outseg->seg_stp, &(ap->a_ray), pp->pt_outflip);


	/* Entry solid */
	ovlp.insol = pp->pt_inseg->seg_stp;

	/* Exit solid */
	ovlp.outsol = pp->pt_outseg->seg_stp;

	/* Entry curvature */
	RT_CURVATURE(&ovlp.incur, pp->pt_inhit, pp->pt_inflip, pp->pt_inseg->seg_stp);

	/* Exit curvature */
	RT_CURVATURE(&ovlp.outcur, pp->pt_outhit, pp->pt_outflip, pp->pt_outseg->seg_stp);

	VJOIN1(ovlp.in_point, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
		   ap->a_ray.r_dir);
	VJOIN1(ovlp.out_point, ap->a_ray.r_pt, pp->pt_outhit->hit_dist,
		   ap->a_ray.r_dir);


	bu_log("Entering at (%f,%f,%f) at distance of %f",
	   V3ARGS(ovlp.in_point), pp->pt_inhit->hit_dist);
	bu_log("Exiting  at (%f,%f,%f) at distance of %f",
	   V3ARGS(ovlp.out_point), pp->pt_outhit->hit_dist);

	bu_log("insol :%s -->> outsol :%s \n", ovlp.insol->st_name,
			ovlp.outsol->st_name);


    return rt_defoverlap (ap, pp, reg1, reg2, InputHdp);
}


int
shoot_ray1(struct rt_i *rtip, point_t pt, point_t dir)
{
    struct application ap;

    /* Initialize the table of resource structures */
    /* rt_init_resource(&res_tab, 0, rtip); */

    /* initialization of the application structure */
    RT_APPLICATION_INIT(&ap);
    ap.a_hit = if_hit1;        /* branch to if_hit routine */
    ap.a_miss = if_miss1;      /* branch to if_miss routine */
    ap.a_overlap = if_overlap1;/* branch to if_overlap routine */
    /*ap.a_logoverlap = rt_silent_logoverlap;*/
    ap.a_onehit = 0;          /* continue through shotline after hit */
    ap.a_purpose = "Sim Manifold ray";
    ap.a_rt_i = rtip;         /* rt_i pointer */
    ap.a_zero1 = 0;           /* sanity check, sayth raytrace.h */
    ap.a_zero2 = 0;           /* sanity check, sayth raytrace.h */

    /* Set the ray start point and direction rt_shootray() uses these
     * two to determine what ray to fire.
     * It's worth nothing that librt assumes units of millimeters.
     * All geometry is stored as millimeters regardless of the units
     * set during editing.  There are libbu routines for performing
     * unit conversions if desired.
     */
    VMOVE(ap.a_ray.r_pt, pt);
    VMOVE(ap.a_ray.r_dir, dir);

    /* Simple debug printing */
    /*bu_log("Pnt (%f,%f,%f)", V3ARGS(ap.a_ray.r_pt));
    VPRINT("Dir", ap.a_ray.r_dir);*/

    /* Shoot the ray. */
    (void)rt_shootray(&ap);

    return GED_OK;
}



/**
 * The libged physics simulation function.
 *
 * Check flags, adds regions to simulation parameters, runs the
 * simulation applies the transforms, frees memory
 */
int
ged_simulate(struct ged *gedp, int argc, const char *argv[])
{
    int rv, i;
    struct simulation_params sim_params;
    static const char *sim_comb_name = "sim.c";
    static const char *ground_plane_name = "sim_gp.r";
    struct rigid_body *current_node, *next_node;
    point_t r_pt, r_dir;

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
    sim_params.gedp = gedp;

    rv = add_regions(gedp, &sim_params);
    if (rv != GED_OK) {
		bu_vls_printf(gedp->ged_result_str, "%s: ERROR while adding objects and sim attributes\n", argv[0]);
		return GED_ERROR;
    }

    rv = get_bb(gedp, &sim_params);
    if (rv != GED_OK) {
		bu_vls_printf(gedp->ged_result_str, "%s: ERROR while getting bounding boxes\n", argv[0]);
		return GED_ERROR;
    }

    for (i=0 ; i < sim_params.duration ; i++) {

		bu_log("%s: ------------------------- Iteration %d -----------------------\n", argv[0], i+1);


		/* Make a new rt_i instance from the existing db_i structure */
		if ((sim_params.rtip=rt_new_rti(gedp->ged_wdbp->dbip)) == RTI_NULL) {
			bu_log("run_simulation: rt_new_rti failed while getting new rt instance\n");
			return 1;
		}
		sim_params.rtip->useair = 1;

		/* Initialize the raytrace world */
		init_raytrace(&sim_params);

		VSET(r_dir, 0.000000,0.000000,-0.6667);
		VUNITIZE(r_dir);
		VSET(r_pt, 0.5, 0.3, 1.0);
		shoot_ray1(sim_params.rtip, r_pt, r_dir);



		/* Recreate sim.c to clear AABBs and manifold regions from previous iteration */
		recreate_sim_comb(gedp, &sim_params);

		/* Run the physics simulation */
		sim_params.gedp = gedp;
		sim_params.iter = i;
		rv = run_simulation(&sim_params);
		if (rv != GED_OK) {
			bu_vls_printf(gedp->ged_result_str, "%s: ERROR while running the simulation\n", argv[0]);
			return GED_ERROR;
		}

		/* Apply transforms on the participating objects, also shades objects */
		rv = apply_transforms(gedp, &sim_params);
		if (rv != GED_OK) {
			bu_vls_printf(gedp->ged_result_str, "%s: ERROR while applying transforms\n", argv[0]);
			return GED_ERROR;
		}

		/* free the raytrace instance */
		rt_free_rti(sim_params.rtip);

    }


    /* Free memory in rigid_body list */
    for (current_node = sim_params.head_node; current_node != NULL;) {
		next_node = current_node->next;
		rt_db_free_internal(&(current_node->intern));
		bu_free(current_node->rb_namep, "simulate : free y");
		bu_free(current_node, "simulate : free current_node");
		current_node = next_node;
		sim_params.num_bodies--;
    }


    bu_vls_printf(gedp->ged_result_str, "%s: The simulation result is in group : %s\n", argv[0], sim_comb_name);

    bu_free(sim_params.sim_comb_name, "simulate : free sim.c");
    bu_free(sim_params.ground_plane_name, "simulate : free sim_gp.r");

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
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
