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

/*
 * Global lists filled up while raytracing
 */
struct overlap overlap_list;
struct hit_reg hit_list;


int
cleanup_lists(void)
{
	struct overlap *ovp;
	struct hit_reg *hrp;

	/* Free all nodes of overlap circularly linked list */
	if (overlap_list.forw != &overlap_list) {
		ovp = overlap_list.forw;
		while (ovp != &overlap_list) {
			bu_free(ovp, "overlap_list");
			ovp = ovp->forw;
		}
	}

	/* Free all nodes of hit region circularly linked list */
	if (hit_list.forw != &hit_list) {
		hrp = hit_list.forw;
		while (hrp != &hit_list) {
			bu_free(hrp, "hit_list");
			hrp = hrp->forw;
		}
	}

	return GED_OK;
}



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

/*
 * Handles hits, records then in a global list
 * TODO : Stop the ray after it's left the overlap region which is being currently
 * queried.
 */
int
if_hit(struct application *ap, struct partition *part_headp, struct seg *UNUSED(segs))
{

	struct hit_reg *new_hit_regp;

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

	new_hit_regp = (struct hit_reg *) bu_malloc(sizeof(struct hit_reg), "new_ovlp");
	if(new_hit_regp){

		/* iterate over each partition until we get back to the head.
		 * each partition corresponds to a specific homogeneous region of
		 * material.
		 */

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

			/* Insert solid data into list node */
			new_hit_regp->reg_name = pp->pt_regionp->reg_name;
			new_hit_regp->in_stp   = pp->pt_inseg->seg_stp;
			new_hit_regp->out_stp  = pp->pt_outseg->seg_stp;

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

			/* Insert the data about the input point into the list */
			VMOVE(new_hit_regp->in_point, in_pt);
			VMOVE(new_hit_regp->in_normal, inormal);
			new_hit_regp->in_dist = hitp->hit_dist;
			new_hit_regp->cur = cur;

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

			/* Insert the data about the input point into the list */
			VMOVE(new_hit_regp->out_point, out_pt);
			VMOVE(new_hit_regp->out_normal, onormal);
			new_hit_regp->out_dist = hitp->hit_dist;

			/* Insert the new hit region into the list of hit regions */
			new_hit_regp->forw = hit_list.forw;
			new_hit_regp->backw = &hit_list;
			new_hit_regp->forw->backw = new_hit_regp;
			hit_list.forw = new_hit_regp;

		}
	}

    return HIT;
}


/**
 *
 * Handles misses while shooting manifold rays,
 * not interested in misses.
 */
int
if_miss(struct application *UNUSED(ap))
{
	bu_log("MISS");
    return MISS;
}


/**
 *
 * Handles overlaps while shooting manifold rays,
 * records the overlap regions in a global list
 *
 */
int
if_overlap(struct application *ap, struct partition *pp, struct region *reg1,
		struct region *reg2, struct partition *InputHdp)
{
	struct overlap *new_ovlp;

	bu_log("if_overlap: OVERLAP between %s and %s", reg1->reg_name, reg2->reg_name);

	new_ovlp = (struct overlap *) bu_malloc(sizeof(struct overlap), "new_ovlp");
	if(new_ovlp){
		new_ovlp->ap = ap;
		new_ovlp->pp = pp;
		new_ovlp->reg1 = reg1;
		new_ovlp->reg2 = reg2;
		new_ovlp->in_dist = pp->pt_inhit->hit_dist;
		new_ovlp->out_dist = pp->pt_outhit->hit_dist;
		VJOIN1(new_ovlp->in_point, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
		   ap->a_ray.r_dir);
		VJOIN1(new_ovlp->out_point, ap->a_ray.r_pt, pp->pt_outhit->hit_dist,
		   ap->a_ray.r_dir);

		/* Insert the new overlap into the list of overlaps */
		new_ovlp->forw = overlap_list.forw;
		new_ovlp->backw = &overlap_list;
		new_ovlp->forw->backw = new_ovlp;
		overlap_list.forw = new_ovlp;

	}

	bu_log("if_overlap: Entering at (%f,%f,%f) at distance of %f",
			V3ARGS(new_ovlp->in_point), new_ovlp->in_dist);
	bu_log("if_overlap: Exiting  at (%f,%f,%f) at distance of %f",
			V3ARGS(new_ovlp->out_point), new_ovlp->out_dist);


    return rt_defoverlap (ap, pp, reg1, reg2, InputHdp);
}

/*
 * Shoots a ray at the simulation geometry and fills up the hit &
 * overlap global list
 */
int
shoot_ray(struct rt_i *rtip, point_t pt, point_t dir)
{
	struct application ap;

    /* Initialize the table of resource structures */
    /* rt_init_resource(&res_tab, 0, rtip); */

	/* initialization of the application structure */
	RT_APPLICATION_INIT(&ap);
	ap.a_hit = if_hit;        /* branch to if_hit routine */
	ap.a_miss = if_miss;      /* branch to if_miss routine */
	ap.a_overlap = if_overlap;/* branch to if_overlap routine */
	/*ap.a_logoverlap = rt_silent_logoverlap;*/
	ap.a_onehit = 0;          /* continue through shotline after hit */
	ap.a_purpose = "Manifold ray";
	ap.a_rt_i = rtip;         /* rt_i pointer */
	ap.a_zero1 = 0;           /* sanity check, sayth raytrace.h */
	ap.a_zero2 = 0;           /* sanity check, sayth raytrace.h */

	/* Set the ray start point and direction rt_shootray() uses these
	 * two to determine what ray to fire.  In this case we simply
	 * shoot down the z axis toward the origin from 10 meters away.
	 *
	 * It's worth nothing that librt assumes units of millimeters.
	 * All geometry is stored as millimeters regardless of the units
	 * set during editing.  There are libbu routines for performing
	 * unit conversions if desired.
	 */
	VMOVE(ap.a_ray.r_pt, pt);
	VMOVE(ap.a_ray.r_dir, dir);

	/* Simple debug printing */
	VPRINT("Pnt", ap.a_ray.r_pt);
	VPRINT("Dir", ap.a_ray.r_dir);

	/* Shoot the ray. */
	(void)rt_shootray(&ap);

	return GED_OK;
}


/*
 * Initialize the simulation scene for raytracing
 */
int
init_raytrace(struct simulation_params *sim_params, struct rt_i *rtip)
{
	struct rigid_body *rb;

	/* Add all sim objects to raytrace instance */

	/* Add all the sim objects to the rt_i */
	for (rb = sim_params->head_node; rb != NULL; rb = rb->next) {
		if (rt_gettree(rtip, rb->rb_namep) < 0)
			bu_log("generate_manifolds: Failed to load geometry for [%s]\n", rb->rb_namep);
		else
			bu_log("generate_manifolds: Added [%s] to raytracer\n", rb->rb_namep);
	}

	/* This next call causes some values to be pre-computed, sets up space
	 * partitioning, computes bounding volumes, etc.
	 */
	rt_prep_parallel(rtip, 1);

	bu_log("Simulation objects bounding box (%f, %f, %f):(%f,%f,%f)",
			V3ARGS(rtip->mdl_min), V3ARGS(rtip->mdl_max));

	overlap_list.forw = overlap_list.backw = &overlap_list;
	hit_list.forw = hit_list.backw = &hit_list;

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

	/* Raytrace related stuff */
	struct rt_i *rtip;
	point_t r_pt, r_dir;

	/* Make a new rt_i instance from the existing db_i structure */
	if ((rtip=rt_new_rti(gedp->ged_wdbp->dbip)) == RTI_NULL) {
		bu_log("generate_manifolds: rt_new_rti failed while getting new rt instance\n");
		return GED_ERROR;
	}
	rtip->useair = 1;

	init_raytrace(sim_params, rtip);

	VSET(r_pt, 1.0, 1.0, 0.0);
	VSET(r_dir, -1.0, 0.0, 0.0);

	shoot_ray(rtip, r_pt, r_dir);


	/* Traverse the hit list and overlap list */
	/* line(gedp, "ray_test", in_pt, out_pt, 0, 210, 0); */

	/* Cleanup the overlap and hit lists and free memory */
	cleanup_lists();


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
