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
 * Global lists filled up while raytracing : remove these as in the forward
 * progression of a ray, the y needs to be increased gradually, no need to
 * record other info
 */
struct overlap overlap_list;
struct hit_reg hit_list;
struct rayshot_results rt_result;


void
print_rayshot_results(void)
{

    bu_log("print_rayshot_results: -------\n");

    bu_log("X bounds xr_min_x_x:%f, xr_min_x_y:%f, xr_min_x_z:%f \n",
	   rt_result.xr_min_x_x,
	   rt_result.xr_min_x_y,
	   rt_result.xr_min_x_z);
    bu_log("X bounds xr_max_x_x:%f, xr_max_x_y:%f, xr_max_x_z:%f \n",
	   rt_result.xr_max_x_x,
	   rt_result.xr_max_x_y,
	   rt_result.xr_max_x_z);
    bu_log("Y bounds xr_min_y:%f, xr_min_y_z:%f\n",
	   rt_result.xr_min_y,
	   rt_result.xr_min_y_z);
    bu_log("Y bounds xr_max_y:%f, xr_max_y_z:%f\n",
	   rt_result.xr_max_y,
	   rt_result.xr_max_y_z);
    bu_log("Z bounds xr_min_z:%f, xr_min_z_y:%f\n",
	   rt_result.xr_min_z,
	   rt_result.xr_min_z_y);
    bu_log("Z bounds xr_max_z:%f, xr_max_z_y:%f\n",
	   rt_result.xr_max_z,
	   rt_result.xr_max_z_y);


}


int
cleanup_lists(void)
{
    struct overlap *ovp, *ovp_free;
    struct hit_reg *hrp, *hrp_free;

    /* Free all nodes of overlap circularly linked list */
    if (overlap_list.forw != &overlap_list) {
	ovp = overlap_list.forw;
	while (ovp != &overlap_list) {
	    ovp_free = ovp;
	    ovp = ovp->forw;
	    bu_free(ovp_free, "cleanup_lists:free overlap_list");

	}
    }

    /* Free all nodes of hit region circularly linked list */
    if (hit_list.forw != &hit_list) {
	hrp = hit_list.forw;
	while (hrp != &hit_list) {
	    hrp_free = hrp;
	    hrp = hrp->forw;
	    bu_free(hrp_free, "cleanup_lists:hit_list");
	}
    }

    overlap_list.forw = overlap_list.backw = &overlap_list;
    hit_list.forw = hit_list.backw = &hit_list;

    overlap_list.index = 0;
    hit_list.index = 0;

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

    /* iterate over each partition until we get back to the head.
     * each partition corresponds to a specific homogeneous region of
     * material.
     */
    for (pp=part_headp->pt_forw; pp != part_headp; pp = pp->pt_forw) {

	new_hit_regp = (struct hit_reg *) bu_malloc(sizeof(struct hit_reg), "new_hit_regp");
	if(new_hit_regp){

	    /* print the name of the region we hit as well as the name of
	     * the primitives encountered on entry and exit.
	     */
	    /*	bu_log("\n--- Hit region %s (in %s, out %s)\n",
		pp->pt_regionp->reg_name,
		pp->pt_inseg->seg_stp->st_name,
		pp->pt_outseg->seg_stp->st_name );*/

	    /* Insert solid data into list node */
	    if(pp->pt_regionp->reg_name[0] == '/')
		new_hit_regp->reg_name = (pp->pt_regionp->reg_name) + 1;
	    else
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
	    /*	rt_pr_hit("  In", hitp);
		VPRINT(   "  Ipoint", in_pt);
		VPRINT(   "  Inormal", inormal);*/


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
	    /*	VPRINT("PDir", cur.crv_pdir);
		bu_log(" c1=%g\n", cur.crv_c1);
		bu_log(" c2=%g\n", cur.crv_c2);*/

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
	    /*	rt_pr_hit("  Out", hitp);
		VPRINT(   "  Opoint", out_pt);
		VPRINT(   "  Onormal", onormal);*/

	    /* Insert the data about the input point into the list */
	    VMOVE(new_hit_regp->out_point, out_pt);
	    VMOVE(new_hit_regp->out_normal, onormal);
	    new_hit_regp->out_dist = hitp->hit_dist;

	    /* Insert the new hit region into the list of hit regions */
	    new_hit_regp->forw = hit_list.forw;
	    new_hit_regp->backw = &hit_list;
	    new_hit_regp->forw->backw = new_hit_regp;
	    hit_list.forw = new_hit_regp;
	    new_hit_regp->index = (new_hit_regp->forw->index) + 1;

	}
    }

    return HIT;
}


int
if_miss(struct application *UNUSED(ap))
{
    bu_log("MISS");
    return MISS;
}


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
	new_ovlp->index = (new_ovlp->forw->index) + 1;

	if(new_ovlp->reg1->reg_name[0] == '/')
	    new_ovlp->reg1->reg_name++;

	if(new_ovlp->reg2->reg_name[0] == '/')
	    new_ovlp->reg2->reg_name++;
    }

    bu_log("if_overlap: Entering at (%f,%f,%f) at distance of %f",
	   V3ARGS(new_ovlp->in_point), new_ovlp->in_dist);
    bu_log("if_overlap: Exiting  at (%f,%f,%f) at distance of %f",
	   V3ARGS(new_ovlp->out_point), new_ovlp->out_dist);


    return rt_defoverlap (ap, pp, reg1, reg2, InputHdp);
}


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
     * two to determine what ray to fire.
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

    overlap_list.index = 0;
    hit_list.index = 0;

    return GED_OK;
}


/**
 * Traverse the hit list and overlap list, drawing the ray segments
 */
int
traverse_lists(struct ged *gedp, struct simulation_params *sim_params,
	       point_t pt, point_t dir)
{
    struct overlap *ovp;
    /*struct hit_reg *hrp;*/
    struct bu_vls reg_vls = BU_VLS_INIT_ZERO;

    /* quellage */
    bu_log("traverse_lists : From : (%f,%f,%f), towards(%f,%f,%f)", V3ARGS(pt),  V3ARGS(dir));

    /* Draw all the overlap regions : lines are added for overlap segments
     * to help visual debugging
     */
    if (overlap_list.forw != &overlap_list) {

	/* Initialize the result structure : todo move to separate function*/
	rt_result.xr_min_y = MAX_FASTF;
	rt_result.xr_max_y = -MAX_FASTF;
	rt_result.xr_min_x_x  = MAX_FASTF;
	rt_result.xr_max_x_x  = -MAX_FASTF;

	ovp = overlap_list.forw;
	while (ovp != &overlap_list) {

	    bu_vls_sprintf(&reg_vls, "ray_overlap_%s_%s_%d_%f_%f_%f_%f_%f_%f",
			   ovp->reg1->reg_name,
			   ovp->reg2->reg_name,
			   ovp->index,
			   V3ARGS(pt), V3ARGS(dir));
	    line(gedp, bu_vls_addr(&reg_vls),
		 ovp->in_point,
		 ovp->out_point,
		 0, 210, 0);

	    add_to_comb(gedp, sim_params->sim_comb_name, bu_vls_addr(&reg_vls));


	    /* Fill up the result structure */

	    /* The min x in the direction of the ray where it hit an overlap */
	    if(ovp->in_point[X] < rt_result.xr_min_x_x){
		rt_result.xr_min_x_x = ovp->in_point[X];
		rt_result.xr_min_x_y = ovp->in_point[Y];
		rt_result.xr_min_x_z = ovp->in_point[Z];
	    }

	    /* The max x in the direction of the ray where it hit an overlap,
	     * could be on a different ray from above
	     */
	    if(ovp->out_point[X] > rt_result.xr_max_x_x){
		rt_result.xr_max_x_x = ovp->out_point[X];
		rt_result.xr_max_x_y = ovp->out_point[Y];
		rt_result.xr_max_x_z = ovp->out_point[Z];
	    }

	    /* The min y where the x rays encountered overlap */
	    if(ovp->in_point[Y] < rt_result.xr_min_y){
		rt_result.xr_min_y   = ovp->in_point[Y];
		rt_result.xr_min_y_z = ovp->in_point[Z];
	    }

	    /* The max y for the same */
	    if(ovp->in_point[Y] > rt_result.xr_max_y){
		rt_result.xr_max_y   = ovp->in_point[Y];
		rt_result.xr_max_y_z = ovp->in_point[Z];
	    }

	    /* The min z where the x rays encountered overlap */
	    if(ovp->in_point[Y] < rt_result.xr_min_z){
		/* Not need currently, may be removed when z-rays are shot */
	    }

	    /* The max z for the same */
	    if(ovp->in_point[Y] > rt_result.xr_max_z){
		/* Not need currently, may be removed when z-rays are shot */
	    }


	    ovp = ovp->forw;


	}
    }

    /* Draw all the hit regions : not really needed to be visualized */
    /*	if (hit_list.forw != &hit_list) {

	hrp = hit_list.forw;

	while (hrp != &hit_list) {
	bu_vls_sprintf(&reg_vls, "ray_hit_%s_%d_%f_%f_%f_%f_%f_%f",
	hrp->reg_name,
	hrp->index,
	V3ARGS(pt), V3ARGS(dir));
	line(gedp, bu_vls_addr(&reg_vls),
	hrp->in_point,
	hrp->out_point,
	0, 210, 0);

	add_to_comb(gedp, sim_params->sim_comb_name, bu_vls_addr(&reg_vls));
	hrp = hrp->forw;
	}
	}*/

    bu_vls_free(&reg_vls);

    return GED_OK;
}


int
shoot_x_rays(
	     struct ged *gedp,
	     struct sim_manifold *current_manifold,
	     struct simulation_params *sim_params,
	     struct rt_i *rtip,
	     vect_t overlap_min, vect_t overlap_max)
{
    point_t r_pt, r_dir;
    fastf_t startz, starty, y, z;
    vect_t diff;

    /* Set direction as straight down X-axis */
    VSET(r_dir, 1.0, 0.0, 0.0);

    startz = overlap_min[Z];
    starty = overlap_min[Y];

    bu_log("Querying overlap between %s & %s",
	   current_manifold->rbA->rb_namep,
	   current_manifold->rbB->rb_namep);

    /* Determine the width along z axis */
    VSUB2(diff, overlap_max, overlap_min);

    /* Is it thinner than TOLerance ? */
    if(diff[Z] < TOL){

	/* Yep , so shoot rays only in a single plane in the middle of the overlap region*/
	startz = overlap_min[Z] + diff[Z]*0.5;

	/* The overlap region is too thin for generating 4 contacts points, 2 will do */
	for(z=startz; z<=overlap_max[Z]; z += TOL){
	    for(y=starty; y<=overlap_max[Y]; y += TOL){

		/* Shooting towards lower x, so start from max x outside of overlap box */
		VSET(r_pt, overlap_min[X], y, z);

		bu_log("*****shoot_x_rays : From : (%f,%f,%f) >>> towards(%f,%f,%f)*******",
		       V3ARGS(r_pt),  V3ARGS(r_dir));

		shoot_ray(rtip, r_pt, r_dir);

		/* Traverse the hit list and overlap list, drawing the ray segments
		 * for the current ray
		 */
		traverse_lists(gedp, sim_params, r_pt, r_dir);

		print_rayshot_results();

		/* Cleanup the overlap and hit lists and free memory */
		cleanup_lists();

	    }
	}
    }


    return GED_OK;
}


int
create_contact_pairs(struct sim_manifold *mf)
{
    /* Prepare the overlap prim name */
    bu_log("create_contact_pairs : between %s & %s",
	   mf->rbA->rb_namep, mf->rbB->rb_namep);


    /* Determine if an arb4 needs to be generated using x/y/z diff. */


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

    /* Make a new rt_i instance from the existing db_i structure */
    if ((rtip=rt_new_rti(gedp->ged_wdbp->dbip)) == RTI_NULL) {
	bu_log("generate_manifolds: rt_new_rti failed while getting new rt instance\n");
	return GED_ERROR;
    }
    rtip->useair = 1;

    init_raytrace(sim_params, rtip);


    /* Check all rigid bodies for overlaps using their manifold lists */
    for (rb = sim_params->head_node; rb != NULL; rb = rb->next) {

	for (current_manifold = rb->head_manifold; current_manifold != NULL;
	     current_manifold = current_manifold->next) {

	    /* Get the overlap region */
	    get_overlap(current_manifold->rbA, current_manifold->rbB,
			overlap_min,
			overlap_max);

	    /* Prepare the overlap prim name */
	    bu_vls_sprintf(&overlap_name, "%s%s_%s",
			   prefix_overlap,
			   current_manifold->rbA->rb_namep,
			   current_manifold->rbB->rb_namep);

	    /* Make the overlap volume RPP */
	    make_rpp(gedp, overlap_min, overlap_max, bu_vls_addr(&overlap_name));

	    /* Add the region to the result of the sim so it will be drawn too */
	    add_to_comb(gedp, sim_params->sim_comb_name, bu_vls_addr(&overlap_name));

	    /* Shoot rays right here as the pair of rigid_body ptrs are known,
	     * todo: ignore volumes already shot
	     */
	    shoot_x_rays(gedp, current_manifold, sim_params, rtip, overlap_min, overlap_max);


	    /* shoot_y_rays(); */


	    /* shoot_z_rays(); */

	    /* Note down this volume as already covered, so no need to shoot rays through it again */

	    /* Create the contact pairs and normals */
	    create_contact_pairs(current_manifold);


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
