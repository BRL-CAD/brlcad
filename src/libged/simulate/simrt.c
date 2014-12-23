/*                         S I M R T . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
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

#ifdef HAVE_BULLET

/* Private Header */
#include "simrt.h"


/*
 * Global lists filled up while raytracing : remove these as in the forward
 * progression of a ray, the y needs to be increased gradually, no need to
 * record other info
 */
#define MAX_OVERLAPS 4
#define MAX_HITS 4
#define MAX_AIRGAPS 4

/* Rays will be at least this far apart when shot through the overlap regions
 * This is also the contact threshold for Bullet (0.04 cm if units are in meters)
 * Overlaps regions smaller than this will have only a single plane of rays slicing the
 * region in half, generating manifolds in a plane.
 */
#define TOL 0.04


/*
 * Maximum normals allowed to be detected by ray shots
 */
#define MAX_NORMALS 10


/*
 * This structure is a single node of an array
 * of overlap regions: similar to the one in nirt/usrfmt.h
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
    vect_t in_normal, out_normal;
    struct soltab *insol, *outsol;
    struct curvature incur, outcur;
};


/*
 * This structure is a single node of an array
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
};


/**
 * This structure contains the results of analyzing an overlap volume(among 2
 * regions), through shooting rays
 */
struct rayshot_results {

    /* Was an overlap detected ? in that case no point in using air gap
     * It may happen that an object is not sufficiently close to the object it
     * intends to strike, i.e. the gap between them is not <= TOL, so the air gap
     * info can't be used to create contact pairs. In the next iteration the object
     * moves so far that it has overlapped(i.e. penetrated) the target,
     * so air gap still can't be used, thus the below flag detects any overlap and
     * enables the logic for creating contact pairs using overlap info.
     *
     * Also rt_result.overlap_found is set to FALSE, before even a single
     * ray is shot and its value is valid across all the different ray shots,
     * so if an overlap has been detected in a ray, all subsequent air gap processing is
     * skipped.
     */
    int overlap_found;

    /* The vector sum of the normals over the surface in the overlap region for A & B*/
    vect_t resultant_normal_A;
    vect_t resultant_normal_B;

    /* List of normals added to a resultant so far, used to prevent adding a normal again */
    vect_t normals[MAX_NORMALS];
    int num_normals;

    /* The following members are used while shooting rays parallel to the resultant normal */


};


int num_hits = 0;
int num_overlaps = 0;
int num_airgaps = 0;
struct overlap overlap_list[MAX_OVERLAPS];
struct hit_reg hit_list[MAX_HITS];
struct hit_reg airgap_list[MAX_AIRGAPS];

struct rayshot_results rt_result;


static void
print_overlap_node(int i)
{
    bu_log("--------- Index %d -------\n", overlap_list[i].index);


    bu_log("insol :%s -->> outsol :%s \n", overlap_list[i].insol->st_name,
	   overlap_list[i].outsol->st_name);


    bu_log("Entering at (%f, %f, %f) at distance of %f\n",
	   V3ARGS(overlap_list[i].in_point), overlap_list[i].in_dist);
    bu_log("Exiting  at (%f, %f, %f) at distance of %f\n",
	   V3ARGS(overlap_list[i].out_point), overlap_list[i].out_dist);

    bu_log("in_normal: %f, %f, %f  -->> out_normal: %f, %f, %f\n",
	   V3ARGS(overlap_list[i].in_normal),
	   V3ARGS(overlap_list[i].out_normal));

    bu_log("incurve pdir : (%f, %f, %f), curv in dir c1: %f, curv opp dir c2: %f\n",
	   V3ARGS(overlap_list[i].incur.crv_pdir), overlap_list[i].incur.crv_c1,
	   overlap_list[i].incur.crv_c2);

    bu_log("outcurve pdir : (%f, %f, %f), curv in dir c1: %f, curv opp dir c2: %f\n",
	   V3ARGS(overlap_list[i].outcur.crv_pdir), overlap_list[i].outcur.crv_c1,
	   overlap_list[i].outcur.crv_c2);
}


/**
 * Cleanup the hit list and overlap list: private to simrt
 */

static int
cleanup_lists(void)
{
    num_hits     = 0;
    num_overlaps = 0;
    num_airgaps  = 0;

    return GED_OK;
}


/**
 * Gets the exact overlap volume between 2 AABBs
 */
static int
get_overlap(struct rigid_body *rbA, struct rigid_body *rbB, vect_t overlap_min,
	    vect_t overlap_max)
{
    bu_log("Calculating overlap between BB of %s(%f, %f, %f):(%f, %f, %f) \
			and %s(%f, %f, %f):(%f, %f, %f)",
	   rbA->rb_namep,
	   V3ARGS(rbA->btbb_min), V3ARGS(rbA->btbb_max),
	   rbB->rb_namep,
	   V3ARGS(rbB->btbb_min), V3ARGS(rbB->btbb_max)
	  );

    VMOVE(overlap_max, rbA->btbb_max);
    VMIN(overlap_max, rbB->btbb_max);

    VMOVE(overlap_min, rbA->btbb_min);
    VMAX(overlap_min, rbB->btbb_min);

    bu_log("Overlap volume between %s & %s is (%f, %f, %f):(%f, %f, %f)\n",
	   rbA->rb_namep,
	   rbB->rb_namep,
	   V3ARGS(overlap_min),
	   V3ARGS(overlap_max)
	  );


    return GED_OK;
}


/**
 * Handles hits, records then in a global list
 * TODO : Stop the ray after it's left the overlap region which is being currently
 * queried.
 */
static int
if_hit(struct application *ap, struct partition *part_headp,
       struct seg *UNUSED(segs))
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

    int i = 0, j = 0;

    /* iterate over each partition until we get back to the head.
     * each partition corresponds to a specific homogeneous region of
     * material.
     */
    for (pp = part_headp->pt_forw; pp != part_headp; pp = pp->pt_forw) {

	if (num_hits < MAX_HITS) {

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

	    if (pp->pt_regionp->reg_aircode != 0)
		bu_log("AIR REGION FOUND !!\n");


	    /* Insert partition */
	    hit_list[i].pp = pp;

	    /* Insert solid data into list node */
	    if (pp->pt_regionp->reg_name[0] == '/')
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
	    /* rt_pr_hit("  In", hitp);
	       VPRINT("  Ipoint", in_pt);
	       VPRINT("  Inormal", inormal);*/


	    if (pp->pt_overlap_reg) {
		struct region *pp_reg;
		int rj = -1;

		bu_log("    Claiming regions:\n");

		while ((pp_reg = pp->pt_overlap_reg[++rj]))
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
	    /* VPRINT("PDir", cur.crv_pdir);
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
	    /* rt_pr_hit("  Out", hitp);
	       VPRINT("  Opoint", out_pt);
	       VPRINT("  Onormal", onormal);*/

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
	     * points WILL BE different(otherwise the hit region would not be reported as different),
	     * though they may belong to the same comb(A or B) but
	     * we do not check that here. We just record the points and prims in the airgap_list[].
	     */
	    if ((num_hits > 1) &&
		(!VEQUAL(hit_list[i].in_point, hit_list[i - 1].out_point))
	       ) {
		/* There has been at least 1 out_point recorded and the
		 * in_point of current hit reg. does not match out_point of previous hit reg.
		 */

		if (num_airgaps < MAX_AIRGAPS) {
		    j = num_airgaps;

		    VMOVE(airgap_list[j].in_point,  hit_list[i].in_point);
		    VMOVE(airgap_list[j].out_point, hit_list[i - 1].out_point); /* Note: i-1 */
		    VSUB2(v, airgap_list[j].out_point, airgap_list[j].in_point);
		    airgap_list[j].out_dist = MAGNITUDE(v);

		    airgap_list[j].in_stp   = hit_list[i].in_stp;
		    airgap_list[j].out_stp  = hit_list[i - 1].out_stp; /* Note: i-1 */

		    airgap_list[j].index = j;

		    bu_log("\nRecorded AIR GAP in %s(%f, %f, %f), out %s(%f, %f, %f), gap size %f mm\n",
			   airgap_list[j].in_stp->st_name,
			   V3ARGS(airgap_list[j].in_point),
			   airgap_list[j].out_stp->st_name,
			   V3ARGS(airgap_list[j].out_point),
			   airgap_list[j].out_dist
			  );

		    num_airgaps++;
		} else
		    bu_log("if_hit: WARNING Skipping AIR region as maximum AIR regions reached\n");

	    }


	} else {
	    bu_log("if_hit: WARNING Skipping hit region as maximum hits reached\n");
	}
    }

    return HIT;
}


/**
 * Handles misses while shooting manifold rays,
 * not interested in misses.
 */
static int
if_miss(struct application *UNUSED(ap))
{
    bu_log("MISS\n");
    return MISS;
}


static void
if_multioverlap(struct application *ap, struct partition *pp1,
		struct bu_ptbl *pptbl,
		struct partition *pp2)
{
    int i = 0;
    struct region *reg1, *reg2;

    reg1 = (struct region *)BU_PTBL_GET(pptbl, 0);
    reg2 = (struct region *)BU_PTBL_GET(pptbl, 1);

    bu_log("if_overlap: OVERLAP between %s and %s\n", reg1->reg_name,
	   reg2->reg_name);


    if (num_overlaps < MAX_OVERLAPS) {
	i = num_overlaps;
	overlap_list[i].ap = ap;
	overlap_list[i].pp = pp1;
	overlap_list[i].reg1 = reg1;
	overlap_list[i].reg2 = reg2;
	overlap_list[i].in_dist = pp1->pt_inhit->hit_dist;
	overlap_list[i].out_dist = pp1->pt_outhit->hit_dist;
	VJOIN1(overlap_list[i].in_point, ap->a_ray.r_pt, pp1->pt_inhit->hit_dist,
	       ap->a_ray.r_dir);
	VJOIN1(overlap_list[i].out_point, ap->a_ray.r_pt, pp1->pt_outhit->hit_dist,
	       ap->a_ray.r_dir);


	/* compute the normal vector at the exit point, flipping the
	 * normal if necessary.
	 */
	RT_HIT_NORMAL(overlap_list[i].in_normal, pp1->pt_inhit,
		      pp1->pt_inseg->seg_stp, &(ap->a_ray), pp1->pt_inflip);


	/* compute the normal vector at the exit point, flipping the
	 * normal if necessary.
	 */
	RT_HIT_NORMAL(overlap_list[i].out_normal, pp1->pt_outhit,
		      pp1->pt_outseg->seg_stp, &(ap->a_ray), pp1->pt_outflip);


	/* Entry solid */
	overlap_list[i].insol = pp1->pt_inseg->seg_stp;

	/* Exit solid */
	overlap_list[i].outsol = pp1->pt_outseg->seg_stp;

	/* Entry curvature */
	RT_CURVATURE(&overlap_list[i].incur, pp1->pt_inhit, pp1->pt_inflip,
		     pp1->pt_inseg->seg_stp);

	/* Exit curvature */
	RT_CURVATURE(&overlap_list[i].outcur, pp1->pt_outhit, pp1->pt_outflip,
		     pp1->pt_outseg->seg_stp);

	overlap_list[i].index = i;
	num_overlaps++;

	bu_log("Added overlap node : \n");

	print_overlap_node(i);
    } else {
	bu_log("if_overlap: WARNING Skipping overlap region as maximum overlaps reached\n");
    }

    (void)rt_defoverlap(ap, pp1, reg1, reg2, pp2);
}


/**
 * Shoots a ray at the simulation geometry and fills up the hit &
 * overlap global list
 */
static int
shoot_ray(struct rt_i *rtip, point_t pt, point_t dir)
{
    struct application ap;

    /* initialization of the application structure */
    RT_APPLICATION_INIT(&ap);
    ap.a_hit = if_hit;        /* branch to if_hit routine */
    ap.a_miss = if_miss;      /* branch to if_miss routine */
    ap.a_multioverlap = if_multioverlap;
    ap.a_logoverlap = rt_silent_logoverlap;
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
    bu_log("Rt:\nPnt: (%f, %f, %f)\n", V3ARGS(ap.a_ray.r_pt));
    bu_log("Dir: (%f, %f, %f)\n", V3ARGS(ap.a_ray.r_dir));

    /* Shoot the ray. */
    (void)rt_shootray(&ap);

    return GED_OK;
}


/**
 * Initializes the rayshot results structure, called before analyzing
 * each manifold through rays shot in x, y & z directions
 */
static int
init_rayshot_results(void)
{
    VSETALL(rt_result.resultant_normal_A, SMALL_FASTF);
    VSETALL(rt_result.resultant_normal_B, SMALL_FASTF);

    rt_result.num_normals = 0;

    rt_result.overlap_found = 0;

    return GED_OK;
}


#ifdef DEBUG_DRAW_LINES
static void
clear_bad_chars(struct bu_vls *vp)
{
    size_t i;
    char *str = bu_vls_addr(vp);
    size_t len = bu_vls_strlen(vp);

    for (i = 0; i < len; i++) {
	if (str[i] == '-' || str[i] == '/') {
	    str[i] = 'R';
	}
    }
}
#endif

#define FOUND 1

int
check_tree_funcleaf(
    struct db_i *dbip,
    struct rt_comb_internal *comb,
    union tree *comb_tree,
    int (*leaf_func)(),
    void *user_ptr1)
{
    int rv = NOT_FOUND;

    RT_CK_DBI(dbip);

    if (!comb_tree)
	return NOT_FOUND;

    RT_CK_TREE(comb_tree);

    switch (comb_tree->tr_op) {
	case OP_DB_LEAF:
	    rv = (*leaf_func)(dbip, comb, comb_tree, user_ptr1);
	    break;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    rv = check_tree_funcleaf(dbip, comb, comb_tree->tr_b.tb_left, leaf_func,
				     user_ptr1);

	    if (rv == NOT_FOUND)
		rv = check_tree_funcleaf(dbip, comb, comb_tree->tr_b.tb_right, leaf_func,
					 user_ptr1);

	    break;

	default:
	    bu_log("check_tree_funcleaf: bad op %d\n", comb_tree->tr_op);
	    bu_bomb("check_tree_funcleaf: bad op\n");
	    break;
    }

    return rv;
}


int
find_solid(struct db_i *dbip,
	   struct rt_comb_internal *comb,
	   union tree *comb_leaf,
	   void *object)
{
    char *obj_name;

    if (dbip) RT_CK_DBI(dbip);

    if (comb) RT_CK_COMB(comb);

    RT_CK_TREE(comb_leaf);

    obj_name = (char *)object;

    if (BU_STR_EQUAL(comb_leaf->tr_l.tl_name, obj_name))
	return FOUND;
    else
	return NOT_FOUND;
}






/**
 * Traverse the hit list and overlap list, drawing the ray segments
 * for normal rays
 */
static int
traverse_normalray_lists(
    struct sim_manifold *mf,
    struct simulation_params *sim_params,
    vect_t overlap_min,
    vect_t overlap_max)
{
    int i, j, rv;

    /*struct hit_reg *hrp;*/
    struct bu_vls reg_vls = BU_VLS_INIT_ZERO;
    fastf_t depth;
    struct rt_comb_internal *comb = (struct rt_comb_internal *)NULL;


    /* Draw all the overlap regions : lines are added for overlap segments
     * to help visual debugging
     */
    for (i = 0; i < num_overlaps; i++) {

	/* Check of both points are within the overlap RPP */
	if (!V3PT_IN_RPP(overlap_list[i].in_point,  overlap_min, overlap_max) ||
	    !V3PT_IN_RPP(overlap_list[i].out_point, overlap_min, overlap_max))
	    continue;


	/* Must check if insol belongs to rbA and outsol to rbB IN THIS ORDER, else results may
	 * be incorrect if geometry of either has overlap defects within.
	 */

	/* Check if the in solid belongs to rbA */
	comb = (struct rt_comb_internal *)mf->rbA->intern.idb_ptr;
	rv = check_tree_funcleaf(sim_params->gedp->ged_wdbp->dbip,
				 comb,
				 comb->tree,
				 find_solid,
				 (void *)(overlap_list[i].insol->st_name));

	if (rv == NOT_FOUND)
	    continue;

	/* Check if the out solid belongs to rbB */
	comb = (struct rt_comb_internal *)mf->rbB->intern.idb_ptr;
	rv = check_tree_funcleaf(sim_params->gedp->ged_wdbp->dbip,
				 comb,
				 comb->tree,
				 find_solid,
				 (void *)(overlap_list[i].outsol->st_name));

	if (rv == NOT_FOUND)
	    continue;

	/* Overlap confirmed, set overlap_found here, because only here it's known
	 * that at least one of detected overlaps was between A & B */
	rt_result.overlap_found = 1;

	depth = DIST_PT_PT(overlap_list[i].in_point, overlap_list[i].out_point);

	bu_log("traverse_normalray_lists: Contact point %d for B:%s at (%f, %f, %f) , depth %f \
		n=(%f, %f, %f), at solid %s", mf->num_contacts + 1,
	       mf->rbB->rb_namep,
	       V3ARGS(overlap_list[i].out_point),
	       -depth,
	       V3ARGS(rt_result.resultant_normal_B),
	       overlap_list[i].insol->st_name);


	/* Add the points to the contacts list of the 1st manifold, without maximizing area here
	 * Bullet should handle contact point minimization and area maximization
	 */
	if (mf->num_contacts == MAX_CONTACTS_PER_MANIFOLD) {
	    bu_log("Contact point skipped as %d points are the maximum allowed per manifold\n",
		   mf->num_contacts);
	    continue;
	}

	j = mf->num_contacts;
	VMOVE(mf->contacts[j].ptB, overlap_list[i].out_point);
	VMOVE(mf->contacts[j].normalWorldOnB, rt_result.resultant_normal_B);
	mf->contacts[j].depth = -depth;
	mf->num_contacts++;

	bu_log("traverse_normalray_lists : ptB set to (%f, %f, %f)\n",
	       V3ARGS((mf->contacts[j].ptB)));
	bu_log("traverse_normalray_lists : normalWorldOnB set to (%f, %f, %f)\n",
	       V3ARGS(mf->contacts[j].normalWorldOnB));
	bu_log("traverse_normalray_lists : Penetration depth set to %f\n",
	       mf->contacts[j].depth);


    } /* end-for overlap */


    /* Investigate hit regions, only if no overlap was found for the ENTIRE bunch of
     * rays being shot, thus rt_result.overlap_found is set to FALSE, before a single
     * ray is fired and its value is valid across all the different ray shots
     * Draw all the hit regions : lines are added for hit segments
     * to help visual debugging
     */
    if (!rt_result.overlap_found) {

	bu_log("traverse_normalray_lists : No overlap found yet, checking hit regions\n");
    }

    bu_vls_free(&reg_vls);

    return GED_OK;
}




/*
 * Shoots a circular bunch of rays from B towards A along resultant_normal_B
 *
 */
static int
shoot_normal_rays(struct sim_manifold *current_manifold,
		  struct simulation_params *sim_params,
		  vect_t overlap_min,
		  vect_t overlap_max)
{
    vect_t diff, up_vec, ref_axis;
    point_t overlap_center;
    fastf_t d, r;
    struct xrays *xrayp;
    struct xrays *entry;
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
    r = d / 2;

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
    if (VZERO(up_vec)) {
	VSET(ref_axis, 0, 1, 0);
	VCROSS(up_vec, rt_result.resultant_normal_B, ref_axis);
    }

    bu_log("shoot_normal_rays: center_ray pt(%f, %f, %f) dir(%f, %f, %f), up_vec(%f, %f, %f), r=%f\n",
	   V3ARGS(center_ray.r_pt), V3ARGS(center_ray.r_dir), V3ARGS(up_vec), r);

    /* Initialize the BU_LIST in preparation for rt_gen_circular_grid() */
    BU_ALLOC(xrayp, struct xrays);
    BU_LIST_INIT(&(xrayp->l));
    VMOVE(xrayp->ray.r_pt, center_ray.r_pt);
    VMOVE(xrayp->ray.r_dir, center_ray.r_dir);
    xrayp->ray.index = 0;
    xrayp->ray.magic = RT_RAY_MAGIC;

    rt_gen_circular_grid(xrayp, &center_ray, r, up_vec, 0.1);

    /* Lets check the rays */
    while (BU_LIST_WHILE(entry, xrays, &(xrayp->l))) {
	bu_log("shoot_normal_rays: *****ray pt(%f, %f, %f) dir(%f, %f, %f) ******\n",
	       V3ARGS(entry->ray.r_pt), V3ARGS(entry->ray.r_dir));

	VUNITIZE(entry->ray.r_dir);
	shoot_ray(sim_params->rtip, entry->ray.r_pt, entry->ray.r_dir);

	traverse_normalray_lists(current_manifold, sim_params,
				 overlap_min, overlap_max);

	cleanup_lists();

	BU_LIST_DEQUEUE(&(entry->l));
	bu_free(entry, "free xrays entry");
    }


    bu_free(xrayp, "free xrays list head");

    return GED_OK;
}


/**
 * Creates the contact pairs from the raytracing results.
 * This is the core logic of the simulation and the manifold points
 * have to satisfy certain constraints (max area within overlap region etc.)
 * to have a successful simulation. The normals and penetration depth is also
 * generated here for each point in the contact pairs. There can be upto 4
 * contact pairs.
 */


static int
create_contact_pairs(
    struct sim_manifold *mf,
    struct simulation_params *sim_params,
    vect_t overlap_min,
    vect_t overlap_max)
{
    /* Calculate the normal of the contact points as the resultant of -A & B velocity
     * NOTE: Currently the sum of normals along overlapping surface , approach is not used
     */
    vect_t v;
    VMOVE(v, mf->rbA->linear_velocity);
    VUNITIZE(v);
    VREVERSE(rt_result.resultant_normal_B, v);

    VMOVE(v, mf->rbB->linear_velocity);
    VUNITIZE(v);
    VADD2(rt_result.resultant_normal_B, rt_result.resultant_normal_B, v);
    VUNITIZE(rt_result.resultant_normal_B);

    mf->num_contacts = 0;

    bu_log("create_contact_pairs : between A : %s(%f, %f, %f) &  B : %s(%f, %f, %f)\n",
	   mf->rbA->rb_namep, V3ARGS(mf->rbA->btbb_center),
	   mf->rbB->rb_namep, V3ARGS(mf->rbB->btbb_center));

    bu_log("create_contact_pairs : Final normal from B to A : (%f, %f, %f)\n",
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

    /* Add the region to the result of the sim so it will be drawn too */
    /* FIXME disabled -- jon
     * add_to_comb(sim_params->gedp, sim_params->sim_comb_name,
    	bu_vls_addr(&overlap_name));*/

    /* Initialize the rayshot results structure, has to be done for each manifold */
    init_rayshot_results();

    /* Create the contact pairs and normals : Currently just 1 manifold is allowed per pair of objects*/
    create_contact_pairs(rt_mf, sim_params, overlap_min, overlap_max);

    return GED_OK;
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
