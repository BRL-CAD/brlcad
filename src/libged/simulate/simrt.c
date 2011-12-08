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

#include "common.h"

/* Private Header */
#include "simrt.h"


#define USE_VELOCITY_FOR_NORMAL 1

/*
 * Global lists filled up while raytracing : remove these as in the forward
 * progression of a ray, the y needs to be increased gradually, no need to
 * record other info
 */
#define MAX_OVERLAPS 4
#define MAX_HITS 4
#define MAX_AIRGAPS 4

int num_hits = 0;
int num_overlaps = 0;
int num_airgaps = 0;
struct overlap overlap_list[MAX_OVERLAPS];
struct hit_reg hit_list[MAX_HITS];
struct hit_reg airgap_list[MAX_AIRGAPS];

struct rayshot_results rt_result;



void
print_overlap_node(int i)
{
	bu_log("--------- Index %d -------\n", overlap_list[i].index);



    bu_log("insol :%s -->> outsol :%s \n", overlap_list[i].insol->st_name,
    									   overlap_list[i].outsol->st_name);


    bu_log("Entering at (%f,%f,%f) at distance of %f",
	   V3ARGS(overlap_list[i].in_point), overlap_list[i].in_dist);
	bu_log("Exiting  at (%f,%f,%f) at distance of %f",
	   V3ARGS(overlap_list[i].out_point), overlap_list[i].out_dist);

	bu_log("in_normal: %f,%f,%f  -->> out_normal: %f,%f,%f",
	    	       V3ARGS(overlap_list[i].in_normal),
	    	       V3ARGS(overlap_list[i].out_normal) );

	bu_log("incurve pdir : (%f,%f,%f), curv in dir c1: %f, curv opp dir c2: %f",
	   V3ARGS(overlap_list[i].incur.crv_pdir), overlap_list[i].incur.crv_c1,
	   	   	   	   	   	   	   	   	   	   	   overlap_list[i].incur.crv_c2);

	bu_log("outcurve pdir : (%f,%f,%f), curv in dir c1: %f, curv opp dir c2: %f",
		   V3ARGS(overlap_list[i].outcur.crv_pdir), overlap_list[i].outcur.crv_c1,
		   	   	   	   	   	   	   	   	   	   	    overlap_list[i].outcur.crv_c2);


}


int
cleanup_lists(void)
{
    num_hits     = 0;
    num_overlaps = 0;
    num_airgaps  = 0;

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

    /* used for calculating the air gap distance */
    vect_t v;

    int i = 0;

    /* iterate over each partition until we get back to the head.
     * each partition corresponds to a specific homogeneous region of
     * material.
     */
    for (pp=part_headp->pt_forw; pp != part_headp; pp = pp->pt_forw) {

	if(num_hits < MAX_HITS){

	    i = num_hits;

	    /* print the name of the region we hit as well as the name of
	     * the primitives encountered on entry and exit.
	     */
	    bu_log("\n--- Hit region %s (in %s, out %s), RegionID=%d, AIRCode=%d, MaterialCode=%d\n",
	    			pp->pt_regionp->reg_name,
	    			pp->pt_inseg->seg_stp->st_name,
	    			pp->pt_outseg->seg_stp->st_name,
	    			pp->pt_regionp->reg_regionid,
	    			pp->pt_regionp->reg_aircode,
	    			pp->pt_regionp->reg_gmater);

	    if(pp->pt_regionp->reg_aircode != 0)
	    	 bu_log("AIR REGION FOUND !!");


	    /* Insert partition */
	    hit_list[i].pp = pp;

	    /* Insert solid data into list node */
	    if(pp->pt_regionp->reg_name[0] == '/')
	    	hit_list[i].reg_name = (pp->pt_regionp->reg_name) + 1;
	    else
	    	hit_list[i].reg_name = pp->pt_regionp->reg_name;

	    hit_list[i].in_stp   = pp->pt_inseg->seg_stp;
	    hit_list[i].out_stp  = pp->pt_outseg->seg_stp;

	    /* entry hit pointer, so we type less */
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
			VMOVE(hit_list[i].in_point, in_pt);
			VMOVE(hit_list[i].in_normal, inormal);
			hit_list[i].in_dist = hitp->hit_dist;
			hit_list[i].cur = cur;

			/* exit pointer, so we type less */
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
			VMOVE(hit_list[i].out_point, out_pt);
			VMOVE(hit_list[i].out_normal, onormal);
			hit_list[i].out_dist = hitp->hit_dist;

			/* Insert the new hit region into the list of hit regions */
			hit_list[i].index = i;

			num_hits++;


			/* Record air gap
			 * Finding an air region involves, comparing the out_point of prev hit reg.
			 * with in_point of current hit reg.
			 * After every ray shot, the lists are cleared, so no need to chk
			 * ray dir and pos of subsequent hit regions for equality(they will be same).
			 * If the current hit region in_point != out_point(of prev hit region),
			 * then the ray must have traveled through air in the interim. The prim of the
			 * points WILL BE different(otherwise the hit region would not be reported as different ),
			 * though they may belong to the same comb(A or B) but
			 * we do not check that here. We just record the points and prims in the airgap_list[].
			 */
			if( (num_hits > 1) &&
				(!VEQUAL(hit_list[i].in_point, hit_list[i-1].out_point))
			){
				/* There has been at least 1 out_point recorded and the
				 * in_point of current hit reg. does not match out_point of previous hit reg.
				 */

				if(num_airgaps < MAX_AIRGAPS){
					i = num_airgaps;

					VMOVE(airgap_list[i].in_point,  hit_list[i].in_point);
					VMOVE(airgap_list[i].out_point, hit_list[i-1].out_point); /* Note: i-1 */
					VSUB2(v, airgap_list[i].out_point, airgap_list[i].in_point);
					airgap_list[i].out_dist = MAGNITUDE(v);

					airgap_list[i].in_stp   = hit_list[i].in_stp;
					airgap_list[i].out_stp  = hit_list[i-1].out_stp; /* Note: i-1 */

					airgap_list[i].index = i;

					bu_log("\nRecorded AIR GAP in %s(%f,%f,%f), out %s(%f,%f,%f), gap size %f mm\n",
							 airgap_list[i].in_stp->st_name,
							 V3ARGS(airgap_list[i].in_point),
							 airgap_list[i].out_stp->st_name,
						     V3ARGS(airgap_list[i].out_point),
						     airgap_list[i].out_dist
					 );

					num_airgaps++;
				}
				else
					bu_log("if_hit: WARNING Skipping AIR region as maximum AIR regions reached");

			}




		}
		else{
			bu_log("if_hit: WARNING Skipping hit region as maximum hits reached");
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
    int i = 0;

    bu_log("if_overlap: OVERLAP between %s and %s", reg1->reg_name, reg2->reg_name);


    if(num_overlaps < MAX_OVERLAPS){
		i = num_overlaps;
		overlap_list[i].ap = ap;
		overlap_list[i].pp = pp;
		overlap_list[i].reg1 = reg1;
		overlap_list[i].reg2 = reg2;
		overlap_list[i].in_dist = pp->pt_inhit->hit_dist;
		overlap_list[i].out_dist = pp->pt_outhit->hit_dist;
		VJOIN1(overlap_list[i].in_point, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
			   ap->a_ray.r_dir);
		VJOIN1(overlap_list[i].out_point, ap->a_ray.r_pt, pp->pt_outhit->hit_dist,
			   ap->a_ray.r_dir);


		/* compute the normal vector at the exit point, flipping the
		 * normal if necessary.
		 */
		RT_HIT_NORMAL(overlap_list[i].in_normal, pp->pt_inhit,
				  pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip);


		/* compute the normal vector at the exit point, flipping the
		 * normal if necessary.
		 */
		RT_HIT_NORMAL(overlap_list[i].out_normal, pp->pt_outhit,
				  pp->pt_outseg->seg_stp, &(ap->a_ray), pp->pt_outflip);


		/* Entry solid */
		overlap_list[i].insol = pp->pt_inseg->seg_stp;

		/* Exit solid */
		overlap_list[i].outsol = pp->pt_outseg->seg_stp;

		/* Entry curvature */
		RT_CURVATURE(&overlap_list[i].incur, pp->pt_inhit, pp->pt_inflip, pp->pt_inseg->seg_stp);

		/* Exit curvature */
		RT_CURVATURE(&overlap_list[i].outcur, pp->pt_outhit, pp->pt_outflip, pp->pt_outseg->seg_stp);

		overlap_list[i].index = i;
		num_overlaps++;

		bu_log("Added overlap node : ");

		print_overlap_node(i);
    }
	else{
		bu_log("if_overlap: WARNING Skipping overlap region as maximum overlaps reached");
	}

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


int
init_rayshot_results(void)
{
    VSETALL(rt_result.resultant_normal_A, SMALL_FASTF);
    VSETALL(rt_result.resultant_normal_B, SMALL_FASTF);

    rt_result.num_normals = 0;

    rt_result.overlap_found = 0;

    return GED_OK;
}


void
clear_bad_chars(struct bu_vls *vp)
{
    unsigned int i = 0;
    for(i = vp->vls_offset; i<(vp->vls_offset + vp->vls_len); i++){
	if( vp->vls_str[i] == '-' ||
	    vp->vls_str[i] == '/')
	    vp->vls_str[i] = 'R';

    }
}


int
exists_normal(vect_t n)
{
    int i;

    for(i=0; i<rt_result.num_normals; i++){
		if(VEQUAL(rt_result.normals[i], n))
			return 1;
    }

    return 0;
}



int
add_normal(vect_t n)
{
    if(rt_result.num_normals < MAX_NORMALS){
    	VMOVE(rt_result.normals[rt_result.num_normals], n);
    	rt_result.num_normals++;
    	return 1;
    }
    else{
    	bu_log("add_normal: WARNING Number of normals have been exceeded, only %d allowed, (%f,%f,%f) not added",
    			MAX_NORMALS, V3ARGS(n));
    }

    return 0;
}


int
traverse_xray_lists(
		struct sim_manifold *current_manifold,
		struct simulation_params *sim_params,
		point_t pt, point_t dir)
{
    int i, rv;

    /*struct hit_reg *hrp;*/
    struct bu_vls reg_vls = BU_VLS_INIT_ZERO;
    struct rt_comb_internal *comb =(struct rt_comb_internal *)NULL;

    /* Draw all the overlap regions : lines are added for overlap segments
     * to help visual debugging
     */
    for(i=0; i<num_overlaps; i++){

		bu_vls_sprintf(&reg_vls, "ray_overlap_%s_%s_%d_%f_%f_%f_%f_%f_%f",
				   overlap_list[i].reg1->reg_name,
				   overlap_list[i].reg2->reg_name,
				   overlap_list[i].index,
				   V3ARGS(pt), V3ARGS(dir));

		clear_bad_chars(&reg_vls);

		line(sim_params->gedp, bu_vls_addr(&reg_vls),
			 overlap_list[i].in_point,
			 overlap_list[i].out_point,
			 0, 210, 0);

		/*bu_log("traverse_xray_lists: %s", bu_vls_addr(&reg_vls));*/

		add_to_comb(sim_params->gedp, sim_params->sim_comb_name, bu_vls_addr(&reg_vls));


		/* Fill up the result structure */

		/* Only check with the comb of rigid body B */
		comb = (struct rt_comb_internal *)(current_manifold->rbB->intern.idb_ptr);

		/* Check if the in solid belongs to rbB and also if the normal has been summed before(do not sum if so) */
		rv = check_tree_funcleaf(sim_params->gedp->ged_wdbp->dbip,
								 comb,
								 comb->tree,
								 find_solid,
								 (genptr_t)(overlap_list[i].insol->st_name));
		if(rv == FOUND && !exists_normal(overlap_list[i].in_normal) ){
			/* It does, so sum the in_normal */
			bu_log("traverse_xray_lists: %s is present in %s", overlap_list[i].insol->st_name,
															   current_manifold->rbB->rb_namep);

			bu_log("traverse_xray_lists: resultant_normal_B is (%f,%f,%f)", V3ARGS(rt_result.resultant_normal_B));
			VADD2(rt_result.resultant_normal_B, rt_result.resultant_normal_B, overlap_list[i].in_normal);
			bu_log("traverse_xray_lists: resultant_normal_B is now (%f,%f,%f) after adding (%f,%f,%f)",
					V3ARGS(rt_result.resultant_normal_B), V3ARGS(overlap_list[i].in_normal));
			add_normal(overlap_list[i].in_normal);

		}

		/* Check if the out solid belongs to rbB and also if the normal has been summed before(do not sum if so) */
		rv = check_tree_funcleaf(sim_params->gedp->ged_wdbp->dbip,
								 comb,
								 comb->tree,
						 	 	 find_solid,
						 	 	 (genptr_t)(overlap_list[i].outsol->st_name));
		if(rv == FOUND && !exists_normal(overlap_list[i].out_normal) ){
			/* It does, so sum the in_normal */
			bu_log("traverse_xray_lists: %s is present in %s", overlap_list[i].outsol->st_name,
															   current_manifold->rbB->rb_namep);

			bu_log("traverse_xray_lists: resultant_normal_B is (%f,%f,%f)", V3ARGS(rt_result.resultant_normal_B));
			VADD2(rt_result.resultant_normal_B, rt_result.resultant_normal_B, overlap_list[i].out_normal);
			bu_log("traverse_xray_lists: resultant_normal_B is now (%f,%f,%f) after adding (%f,%f,%f)",
					V3ARGS(rt_result.resultant_normal_B), V3ARGS(overlap_list[i].out_normal));
			add_normal(overlap_list[i].out_normal);

		}
	}

    bu_vls_free(&reg_vls);

    return GED_OK;
}


int
traverse_yray_lists(
		struct sim_manifold *current_manifold,
		struct simulation_params *sim_params,
		point_t pt, point_t dir)
{
    int i, rv;

    /*struct hit_reg *hrp;*/
    struct bu_vls reg_vls = BU_VLS_INIT_ZERO;
    struct rt_comb_internal *comb =(struct rt_comb_internal *)NULL;



    /* Draw all the overlap regions : lines are added for overlap segments
     * to help visual debugging
     */
    for(i=0; i<num_overlaps; i++){

		bu_vls_sprintf(&reg_vls, "ray_overlap_%s_%s_%d_%f_%f_%f_%f_%f_%f",
				   overlap_list[i].reg1->reg_name,
				   overlap_list[i].reg2->reg_name,
				   overlap_list[i].index,
				   V3ARGS(pt), V3ARGS(dir));

		clear_bad_chars(&reg_vls);

		line(sim_params->gedp, bu_vls_addr(&reg_vls),
			 overlap_list[i].in_point,
			 overlap_list[i].out_point,
			 0, 210, 0);

		bu_log("traverse_yray_lists: %s", bu_vls_addr(&reg_vls));

		add_to_comb(sim_params->gedp, sim_params->sim_comb_name, bu_vls_addr(&reg_vls));


		/* Fill up the result structure */

		/* Only check with the comb of rigid body B */
		comb = (struct rt_comb_internal *)(current_manifold->rbB->intern.idb_ptr);

		/* Check if the in solid belongs to rbB */
		rv = check_tree_funcleaf(sim_params->gedp->ged_wdbp->dbip,
								 comb,
								 comb->tree,
								 find_solid,
								 (genptr_t)(overlap_list[i].insol->st_name));
		if(rv == FOUND && !exists_normal(overlap_list[i].in_normal) ){
			/* It does, so sum the in_normal */
			bu_log("traverse_yray_lists: %s is present in %s", overlap_list[i].insol->st_name,
															   current_manifold->rbB->rb_namep);

			bu_log("traverse_yray_lists: resultant_normal_B is (%f,%f,%f)", V3ARGS(rt_result.resultant_normal_B));
			VADD2(rt_result.resultant_normal_B, rt_result.resultant_normal_B, overlap_list[i].in_normal);
			bu_log("traverse_yray_lists: resultant_normal_B is now (%f,%f,%f) after adding (%f,%f,%f)",
					V3ARGS(rt_result.resultant_normal_B), V3ARGS(overlap_list[i].in_normal));
			add_normal(overlap_list[i].in_normal);

		}

		/* Check if the out solid belongs to rbB */
		rv = check_tree_funcleaf(sim_params->gedp->ged_wdbp->dbip,
								 comb,
								 comb->tree,
						 	 	 find_solid,
						 	 	 (genptr_t)(overlap_list[i].outsol->st_name));
		if(rv == FOUND && !exists_normal(overlap_list[i].out_normal) ){
			/* It does, so sum the out_normal */
			bu_log("traverse_yray_lists: %s is present in %s", overlap_list[i].outsol->st_name,
															   current_manifold->rbB->rb_namep);

			bu_log("traverse_yray_lists: resultant_normal_B is (%f,%f,%f)", V3ARGS(rt_result.resultant_normal_B));
			VADD2(rt_result.resultant_normal_B, rt_result.resultant_normal_B, overlap_list[i].out_normal);
			bu_log("traverse_yray_lists: resultant_normal_B is now (%f,%f,%f) after adding (%f,%f,%f)",
					V3ARGS(rt_result.resultant_normal_B), V3ARGS(overlap_list[i].out_normal));
			add_normal(overlap_list[i].out_normal);
		}
	}


    bu_vls_free(&reg_vls);

    return GED_OK;
}


int
traverse_zray_lists(
		struct sim_manifold *current_manifold,
		struct simulation_params *sim_params,
		point_t pt, point_t dir)
{
    int i, rv;

    /*struct hit_reg *hrp;*/
    struct bu_vls reg_vls = BU_VLS_INIT_ZERO;
    struct rt_comb_internal *comb =(struct rt_comb_internal *)NULL;


    /* Draw all the overlap regions : lines are added for overlap segments
     * to help visual debugging
     */
    for(i=0; i<num_overlaps; i++){

		bu_vls_sprintf(&reg_vls, "ray_overlap_%s_%s_%d_%f_%f_%f_%f_%f_%f",
				   overlap_list[i].reg1->reg_name,
				   overlap_list[i].reg2->reg_name,
				   overlap_list[i].index,
				   V3ARGS(pt), V3ARGS(dir));

		clear_bad_chars(&reg_vls);

		line(sim_params->gedp, bu_vls_addr(&reg_vls),
			 overlap_list[i].in_point,
			 overlap_list[i].out_point,
			 0, 210, 0);

		bu_log("traverse_zray_lists: %s", bu_vls_addr(&reg_vls));

		add_to_comb(sim_params->gedp, sim_params->sim_comb_name, bu_vls_addr(&reg_vls));


		/* Fill up the result structure */

		/* Only check with the comb of rigid body B */
		comb = (struct rt_comb_internal *)(current_manifold->rbB->intern.idb_ptr);

		/* Check if the in solid belongs to rbB */
		rv = check_tree_funcleaf(sim_params->gedp->ged_wdbp->dbip,
								 comb,
								 comb->tree,
								 find_solid,
								 (genptr_t)(overlap_list[i].insol->st_name));
		if(rv == FOUND && !exists_normal(overlap_list[i].in_normal) ){
			/* It does, so sum the in_normal */
			bu_log("traverse_zray_lists: %s is present in %s", overlap_list[i].insol->st_name,
															   current_manifold->rbB->rb_namep);

			bu_log("traverse_zray_lists: resultant_normal_B is (%f,%f,%f)", V3ARGS(rt_result.resultant_normal_B));
			VADD2(rt_result.resultant_normal_B, rt_result.resultant_normal_B, overlap_list[i].in_normal);
			bu_log("traverse_zray_lists: resultant_normal_B is now (%f,%f,%f) after adding (%f,%f,%f)",
					V3ARGS(rt_result.resultant_normal_B), V3ARGS(overlap_list[i].in_normal));
			add_normal(overlap_list[i].in_normal);

		}

		/* Check if the out solid belongs to rbB */
		rv = check_tree_funcleaf(sim_params->gedp->ged_wdbp->dbip,
								 comb,
								 comb->tree,
						 	 	 find_solid,
						 	 	 (genptr_t)(overlap_list[i].outsol->st_name));
		if(rv == FOUND && !exists_normal(overlap_list[i].out_normal) ){
			/* It does, so sum the out_normal */
			bu_log("traverse_zray_lists: %s is present in %s", overlap_list[i].outsol->st_name,
															   current_manifold->rbB->rb_namep);

			bu_log("traverse_zray_lists: resultant_normal_B is (%f,%f,%f)", V3ARGS(rt_result.resultant_normal_B));
			VADD2(rt_result.resultant_normal_B, rt_result.resultant_normal_B, overlap_list[i].out_normal);
			bu_log("traverse_zray_lists: resultant_normal_B is now (%f,%f,%f) after adding (%f,%f,%f)",
					V3ARGS(rt_result.resultant_normal_B), V3ARGS(overlap_list[i].out_normal));
			add_normal(overlap_list[i].out_normal);

		}
	}


    bu_vls_free(&reg_vls);

    return GED_OK;
}


int
traverse_normalray_lists(
		struct sim_manifold *mf,
		struct simulation_params *sim_params,
		point_t pt,
		point_t dir,
		vect_t overlap_min,
		vect_t overlap_max)
{
    int i, j, rv;

    /*struct hit_reg *hrp;*/
    struct bu_vls reg_vls = BU_VLS_INIT_ZERO;
    fastf_t depth;
    struct rt_comb_internal *comb =(struct rt_comb_internal *)NULL;


    /* Draw all the overlap regions : lines are added for overlap segments
     * to help visual debugging
     */
    for(i=0; i<num_overlaps; i++){

		bu_vls_sprintf(&reg_vls, "ray_ovrlp_%s_%s_%d_%f_%f_%f_%f_%f_%f",
				   overlap_list[i].reg1->reg_name,
				   overlap_list[i].reg2->reg_name,
				   overlap_list[i].index,
				   V3ARGS(pt), V3ARGS(dir));

		clear_bad_chars(&reg_vls);

		line(sim_params->gedp, bu_vls_addr(&reg_vls),
			 overlap_list[i].in_point,
			 overlap_list[i].out_point,
			 0, 210, 0);

		/* bu_log("traverse_normalray_lists: %s", bu_vls_addr(&reg_vls)); */

		add_to_comb(sim_params->gedp, sim_params->sim_comb_name, bu_vls_addr(&reg_vls));


		/* Check of both points are within the overlap RPP */
		if(!V3PT_IN_RPP(overlap_list[i].in_point,  overlap_min, overlap_max) ||
		   !V3PT_IN_RPP(overlap_list[i].out_point, overlap_min, overlap_max))
			continue;


		/* Must check if insol belongs to rbA and outsol to rbB  IN THIS ORDER, else results may
		 * be incorrect if geometry of either has overlap defects within.
		 */

		/* Check if the in solid belongs to rbA */
		comb = (struct rt_comb_internal *)mf->rbA->intern.idb_ptr;
		rv = check_tree_funcleaf(sim_params->gedp->ged_wdbp->dbip,
				 	 	 		 comb,
				 	 	 		 comb->tree,
				 	 	 		 find_solid,
				 	 	 		 (genptr_t)(overlap_list[i].insol->st_name));

		if(rv == NOT_FOUND)
			continue;

		/* Check if the out solid belongs to rbB */
		comb = (struct rt_comb_internal *)mf->rbB->intern.idb_ptr;
		rv = check_tree_funcleaf(sim_params->gedp->ged_wdbp->dbip,
						 	 	 comb,
						 	 	 comb->tree,
						 	 	 find_solid,
						 	 	 (genptr_t)(overlap_list[i].outsol->st_name));

		if(rv == NOT_FOUND)
			continue;

		/* Overlap confirmed, set overlap_found here, because only here its known
		 * that at least one of detected overlaps was between A & B  */
		rt_result.overlap_found = 1;

		depth = DIST_PT_PT(overlap_list[i].in_point, overlap_list[i].out_point);

		bu_log("traverse_normalray_lists: Contact point %d for B:%s at (%f,%f,%f) , depth %f \
				n=(%f,%f,%f), at solid %s", mf->num_contacts + 1,
											mf->rbB->rb_namep,
											V3ARGS(overlap_list[i].out_point),
											-depth,
											V3ARGS(rt_result.resultant_normal_B),
											overlap_list[i].insol->st_name);


		/* Add the points to the contacts list of the 1st manifold, without maximizing area here
		 * Bullet should handle contact point minimization and area maximization
		 */
		if(mf->num_contacts == MAX_CONTACTS_PER_MANIFOLD){
			bu_log("Contact point skipped as %d points are the maximum allowed per manifold", mf->num_contacts);
			continue;
		}

		j = mf->num_contacts;
		VMOVE(mf->contacts[j].ptB, overlap_list[i].out_point);
		VMOVE(mf->contacts[j].normalWorldOnB, rt_result.resultant_normal_B);
		mf->contacts[j].depth = -depth;
		mf->num_contacts++;

		bu_log("traverse_normalray_lists : ptB set to (%f,%f,%f)", V3ARGS((mf->contacts[j].ptB)));
		bu_log("traverse_normalray_lists : normalWorldOnB set to (%f,%f,%f)", V3ARGS(mf->contacts[j].normalWorldOnB));
		bu_log("traverse_normalray_lists : Penetration depth set to %f", mf->contacts[j].depth);


	} /* end-for overlap */



    /* Investigate hit regions, only if no overlap was found for the ENTIRE bunch of
     * rays being shot, thus rt_result.overlap_found is set to FALSE, before a single
     * ray is fired and its value is valid across all the different ray shots
     * Draw all the hit regions : lines are added for hit segments
	 * to help visual debugging
	 */
    if(!rt_result.overlap_found){

    	bu_log("traverse_normalray_lists : No overlap found yet, checking hit regions");


    	/* Draw the hit regions */
		for(i=0; i<num_hits; i++){

			bu_vls_sprintf(&reg_vls, "ray_hit_%s_%d_%d_%d_%f_%f_%f_%f_%f_%f_%d",
					   hit_list[i].reg_name,
					   hit_list[i].pp->pt_regionp->reg_regionid,
					   hit_list[i].pp->pt_regionp->reg_aircode,
					   hit_list[i].pp->pt_regionp->reg_gmater,
					   V3ARGS(pt), V3ARGS(dir),
					   hit_list[i].index);

			clear_bad_chars(&reg_vls);

			line(sim_params->gedp, bu_vls_addr(&reg_vls),
					hit_list[i].in_point,
					hit_list[i].out_point,
					0, 210, 0);


			add_to_comb(sim_params->gedp, sim_params->sim_comb_name, bu_vls_addr(&reg_vls));

		} /* end-for hits */


		/* Draw the AIR regions */
		for(i=0; i<num_airgaps; i++){

			bu_vls_sprintf(&reg_vls, "ray_airgap_%s_%f_%f_%f_%s_%f_%f_%f_%d",
					 airgap_list[i].in_stp->st_name,
					 V3ARGS(airgap_list[i].in_point),
					 airgap_list[i].out_stp->st_name,
					 V3ARGS(airgap_list[i].out_point),
					 airgap_list[i].index);

			clear_bad_chars(&reg_vls);

			line(sim_params->gedp, bu_vls_addr(&reg_vls),
					airgap_list[i].in_point,
					airgap_list[i].out_point,
					0, 210, 0);


			add_to_comb(sim_params->gedp, sim_params->sim_comb_name, bu_vls_addr(&reg_vls));



		} /* end-for hits */
    }


    bu_vls_free(&reg_vls);

    return GED_OK;
}


int
shoot_x_rays(struct sim_manifold *current_manifold,
	     struct simulation_params *sim_params,
	     vect_t overlap_min,
	     vect_t overlap_max)
{
    point_t r_pt, r_dir;
    fastf_t startz, starty, y, z, incr_y, incr_z;
    vect_t diff;

    /* Set direction as straight down X-axis */
    VSET(r_dir, 1.0, 0.0, 0.0);


    bu_log("Querying overlap between A: %s & B: %s",
	   current_manifold->rbA->rb_namep,
	   current_manifold->rbB->rb_namep);

    /* Determine the width along z axis */
    VSUB2(diff, overlap_max, overlap_min);

    /* If it's thinner than TOLerance, reduce TOL, so that only 2 boundary rays shot
	 */
    incr_z = TOL;
    if(diff[Z] < TOL){
    	incr_z = diff[Z]*0.5;
    }

    incr_y = TOL;
    if(diff[Y] < TOL){
    	incr_y = diff[Y]*0.5;
    }

    startz = overlap_min[Z] - incr_z;

	/* Shoot rays vertically and across the yz plane */
	for(z=startz; z<overlap_max[Z]; ){

		z += incr_z;
		if(z > overlap_max[Z])
			z = overlap_max[Z];

		starty = overlap_min[Y] - incr_y;

		for(y=starty; y<overlap_max[Y]; ){

			y += incr_y;
			if(y > overlap_max[Y])
				y = overlap_max[Y];

			/* Shooting towards higher x, so start from min x outside of overlap box */
			VSET(r_pt, overlap_min[X], y, z);

			bu_log("*****shoot_x_rays : From : (%f,%f,%f) , dir:(%f,%f,%f)*******",
			   V3ARGS(r_pt),  V3ARGS(r_dir));

			num_overlaps = 0;
			shoot_ray(sim_params->rtip, r_pt, r_dir);

			/* Traverse the hit list and overlap list, drawing the ray segments
			 * for the current ray
			 */
			traverse_yray_lists(current_manifold, sim_params, r_pt, r_dir);


			/* Cleanup the overlap and hit lists and free memory */
			cleanup_lists();

			/*bu_log("Last y ray fired from y = %f, overlap_max[Y]=%f", y, overlap_max[Y]);*/

		}

		/*bu_log("Last z ray fired from z = %f, overlap_max[Z]=%f", z, overlap_max[Z]);*/

    }


    return GED_OK;
}


int
shoot_y_rays(struct sim_manifold *current_manifold,
	     struct simulation_params *sim_params,
	     vect_t overlap_min,
	     vect_t overlap_max)
{
    point_t r_pt, r_dir;
    fastf_t startz, startx, x, z, incr_x, incr_z;
    vect_t diff;

    /* Set direction as straight down Y-axis */
    VSET(r_dir, 0.0, 1.0, 0.0);


    bu_log("Querying overlap between A:%s & B:%s",
	   current_manifold->rbA->rb_namep,
	   current_manifold->rbB->rb_namep);

    /* Determine the width along z axis */
    VSUB2(diff, overlap_max, overlap_min);

    /* If it's thinner than TOLerance, reduce TOL, so that only 2 boundary rays shot
	 */
    incr_z = TOL;
    if(diff[Z] < TOL){
    	incr_z = diff[Z]*0.5;
    }

    incr_x = TOL;
    if(diff[X] < TOL){
    	incr_x = diff[X]*0.5;
    }

    startz = overlap_min[Z] - incr_z;

	/* Shoot rays vertically and across the xz plane */
	for(z=startz; z<overlap_max[Z]; ){

		z += incr_z;
		if(z > overlap_max[Z])
			z = overlap_max[Z];

		startx = overlap_min[X] - incr_x;

		for(x=startx; x<overlap_max[X]; ){

			x += incr_x;
			if(x > overlap_max[X])
				x = overlap_max[X];

			/* Shooting towards higher y, so start from min y outside of overlap box */
			VSET(r_pt, x, overlap_min[Y], z);

			bu_log("*****shoot_y_rays : From : (%f,%f,%f) , dir:(%f,%f,%f)*******",
			   V3ARGS(r_pt),  V3ARGS(r_dir));

			num_overlaps = 0;
			shoot_ray(sim_params->rtip, r_pt, r_dir);

			/* Traverse the hit list and overlap list, drawing the ray segments
			 * for the current ray
			 */
			traverse_yray_lists(current_manifold, sim_params, r_pt, r_dir);

			/* Cleanup the overlap and hit lists and free memory */
			cleanup_lists();

			/*bu_log("Last x ray fired from x = %f, overlap_max[X]=%f", x, overlap_max[X]);*/

		}

		/*bu_log("Last z ray fired from z = %f, overlap_max[Z]=%f", z, overlap_max[Z]);*/

    }


    return GED_OK;
}


int
shoot_z_rays(struct sim_manifold *current_manifold,
	     struct simulation_params *sim_params,
	     vect_t overlap_min,
	     vect_t overlap_max)
{
    point_t r_pt, r_dir;
    fastf_t starty, startx, x, y, incr_x, incr_y;
    vect_t diff;

    /* Set direction as straight down Z-axis */
    VSET(r_dir, 0.0, 0.0, 1.0);


    bu_log("Querying overlap between A:%s & B:%s",
	   current_manifold->rbA->rb_namep,
	   current_manifold->rbB->rb_namep);

    /* Determine the width along z axis */
    VSUB2(diff, overlap_max, overlap_min);

    /* If it's thinner than TOLerance, reduce TOL, so that only 2 boundary rays shot
	 */
    incr_y = TOL;
    if(diff[Y] < TOL){
    	incr_y = diff[Y]*0.5;
    }

    incr_x = TOL;
    if(diff[X] < TOL){
    	incr_x = diff[X]*0.5;
    }

    starty = overlap_min[Y] - incr_y;

	/* Shoot rays vertically and across the xy plane */
	for(y=starty; y<overlap_max[Y]; ){

		y += incr_y;
		if(y > overlap_max[Y])
			y = overlap_max[Y];

		startx = overlap_min[X] - incr_x;

		for(x=startx; x<overlap_max[X]; ){

			x += incr_x;
			if(x > overlap_max[X])
				x = overlap_max[X];

			/* Shooting towards higher z, so start from min z outside of overlap box */
			VSET(r_pt, x, y, overlap_min[Z]);

			bu_log("*****shoot_z_rays : From : (%f,%f,%f) , dir:(%f,%f,%f)*******",
			   V3ARGS(r_pt),  V3ARGS(r_dir));

			num_overlaps = 0;
			shoot_ray(sim_params->rtip, r_pt, r_dir);

			/* Traverse the hit list and overlap list, drawing the ray segments
			 * for the current ray
			 */
			traverse_zray_lists(current_manifold, sim_params, r_pt, r_dir);

			/* Cleanup the overlap and hit lists and free memory */
			cleanup_lists();

			/*bu_log("Last x ray fired from x = %f, overlap_max[X]=%f", x, overlap_max[X]);*/

		}

		/*bu_log("Last y ray fired from y = %f, overlap_max[Y]=%f", y, overlap_max[Y]);*/

    }


    return GED_OK;
}


int
shoot_normal_rays(struct sim_manifold *current_manifold,
	     	 	 struct simulation_params *sim_params,
	     	 	 vect_t overlap_min,
	     	 	 vect_t overlap_max)
{
	vect_t diff, up_vec, ref_axis;
	point_t overlap_center;
	fastf_t d, r;
	struct xrays *xrayp, *entry ;
	struct xray center_ray;

	/* Setup center ray */
	center_ray.index = 0;
	VMOVE(center_ray.r_dir, rt_result.resultant_normal_B);

	/* The radius of the circular bunch of rays to shoot to get depth and points on B is
	 * the half of the diagonal of the overlap RPP , so that no matter how the normal is
	 * oriented, rays are shot to cover the entire volume
	 */
	VSUB2(diff, overlap_max, overlap_min);
	d = MAGNITUDE(diff);
	r = d/2;

	/* Get overlap volume position in 3D space */
	VCOMB2(overlap_center, 1, overlap_min, 0.5, overlap_max);

	/* Step back from the overlap_center, along the normal by r
	 * to ensure rays start from outside overlap region
	 */
	VSCALE(diff, rt_result.resultant_normal_B, -r);
	VADD2(center_ray.r_pt, overlap_center, diff);

	/* Generate the up vector, try cross with x-axis */
	VSET(ref_axis, 1, 0, 0);
	VCROSS(up_vec, rt_result.resultant_normal_B, ref_axis);

	/* Parallel to x-axis ? then try y-axis */
	if(VZERO(up_vec)){
		VSET(ref_axis, 0, 1, 0);
		VCROSS(up_vec, rt_result.resultant_normal_B, ref_axis);
	}

	bu_log("shoot_normal_rays: center_ray pt(%f,%f,%f) dir(%f,%f,%f), up_vec(%f,%f,%f), r=%f",
			V3ARGS(center_ray.r_pt), V3ARGS(center_ray.r_dir), V3ARGS(up_vec), r);

	/* Initialize the BU_LIST in preparation for rt_gen_circular_grid() */
	BU_GET(xrayp, struct xrays);
	BU_LIST_INIT(&(xrayp->l));
	VMOVE(xrayp->ray.r_pt, center_ray.r_pt);
	VMOVE(xrayp->ray.r_dir, center_ray.r_dir);
	xrayp->ray.index = 0;
	xrayp->ray.magic = RT_RAY_MAGIC;

	rt_gen_circular_grid(xrayp, &center_ray, r, up_vec, 0.1);

	/* Lets check the rays */
	while (BU_LIST_WHILE(entry, xrays, &(xrayp->l))) {
	   bu_log("shoot_normal_rays: *****ray pt(%f,%f,%f) dir(%f,%f,%f) ******",
				V3ARGS(entry->ray.r_pt), V3ARGS(entry->ray.r_dir));


	   VUNITIZE(entry->ray.r_dir);
	   shoot_ray(sim_params->rtip, entry->ray.r_pt, entry->ray.r_dir);

	   traverse_normalray_lists(current_manifold, sim_params, entry->ray.r_pt, entry->ray.r_dir,
			   overlap_min, overlap_max);

	   cleanup_lists();

	   BU_LIST_DEQUEUE(&(entry->l));
	   bu_free(entry, "free xrays entry");
	}


	bu_free(xrayp, "free xrays list head");

	return GED_OK;
}


int
create_contact_pairs(
		 struct sim_manifold *mf,
		 struct simulation_params *sim_params,
		 vect_t overlap_min,
		 vect_t overlap_max)
{

#ifdef USE_VELOCITY_FOR_NORMAL
	 vect_t v;
#endif


	mf->num_contacts = 0;


	bu_log("create_contact pairs : between A : %s(%f,%f,%f) &  B : %s(%f,%f,%f)",
	   mf->rbA->rb_namep, V3ARGS(mf->rbA->btbb_center),
	   mf->rbB->rb_namep, V3ARGS(mf->rbB->btbb_center));


#ifdef USE_VELOCITY_FOR_NORMAL
	/* Calculate the normal of the contact points as the resultant of -A & B velocity
     * NOTE: Currently the sum of normals along overlapping surface , approach is not used
     */
	VMOVE(v, mf->rbA->linear_velocity);
	VUNITIZE(v);
	VREVERSE(rt_result.resultant_normal_B, v);

	VMOVE(v, mf->rbB->linear_velocity);
	VUNITIZE(v);
	VADD2(rt_result.resultant_normal_B, rt_result.resultant_normal_B, v);
	VUNITIZE(rt_result.resultant_normal_B);
#endif


	bu_log("create_contact pairs : Final normal from B to A : (%f,%f,%f)",
			V3ARGS(rt_result.resultant_normal_B));

    /* Begin making contacts */

	/* Shoot rays along the normal to get points on B and the depth along this direction */
	shoot_normal_rays(mf, sim_params, overlap_min, overlap_max);

    return GED_OK;
}


int
generate_manifolds(struct simulation_params *sim_params,
				   struct rigid_body *rbA,
				   struct rigid_body *rbB)
{
    /* The manifold between rbA & rbB will be stored in B */
	struct sim_manifold *rt_mf;

    vect_t overlap_min, overlap_max;
    char *prefix_overlap = "overlap_";
    struct bu_vls overlap_name = BU_VLS_INIT_ZERO;
    rt_mf = &(rbB->rt_manifold);

    /* Setup the manifold participant pointers */
    rt_mf->rbA = rbA;
    rt_mf->rbB = rbB;

	/* Get the overlap region */
	get_overlap(rbA, rbB, overlap_min, overlap_max);

	/* Prepare the overlap prim name */
	bu_vls_sprintf(&overlap_name, "%s%s_%s",
		   prefix_overlap,
		   rbA->rb_namep,
		   rbB->rb_namep);

	/* Make the overlap volume RPP */
	make_rpp(sim_params->gedp, overlap_min, overlap_max, bu_vls_addr(&overlap_name));

	/* Add the region to the result of the sim so it will be drawn too */
	add_to_comb(sim_params->gedp, sim_params->sim_comb_name, bu_vls_addr(&overlap_name));

    /* Clear ray visualization primitives */
    kill(sim_params->gedp, "ray_*");

	/* Initialize the rayshot results structure, has to be done for each manifold  */
	init_rayshot_results();


#ifndef USE_VELOCITY_FOR_NORMAL
	/* Shoot rays right here as the pair of rigid_body ptrs are known,
	 * TODO: ignore volumes already shot
	 */
	shoot_x_rays(rt_mf, sim_params, overlap_min, overlap_max);
	shoot_y_rays(rt_mf, sim_params, overlap_min, overlap_max);
	shoot_z_rays(rt_mf, sim_params, overlap_min, overlap_max);
#endif


	/* Create the contact pairs and normals : Currently just 1 manifold is allowed per pair of objects*/
	create_contact_pairs(rt_mf, sim_params, overlap_min, overlap_max);

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
