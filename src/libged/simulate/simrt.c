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


#define MAX_OVERLAPS 4

/*
 * Maximum normals allowed to be detected by ray shots
 */
#define MAX_NORMALS 10


HIDDEN int
check_tree_funcleaf(
    struct db_i *dbip,
    struct rt_comb_internal *comb,
    union tree *comb_tree,
    int (*leaf_func)(),
    void *user_ptr1)
{
    int rv = 0;

    RT_CK_DBI(dbip);

    if (!comb_tree)
	return 0;

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

	    if (!rv)
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


HIDDEN int
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
	return 1;
    else
	return 0;
}


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
};


int num_overlaps = 0;
struct overlap overlap_list[MAX_OVERLAPS];
struct rayshot_results rt_result;


/**
 * Gets the exact overlap volume between 2 AABBs
 */
static int
get_overlap(struct rigid_body *rbA, struct rigid_body *rbB, vect_t overlap_min,
	    vect_t overlap_max)
{
    VMOVE(overlap_max, rbA->btbb_max);
    VMIN(overlap_max, rbB->btbb_max);

    VMOVE(overlap_min, rbA->btbb_min);
    VMAX(overlap_min, rbB->btbb_min);
    return GED_OK;
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


	/* Entry solid */
	overlap_list[i].insol = pp1->pt_inseg->seg_stp;

	/* Exit solid */
	overlap_list[i].outsol = pp1->pt_outseg->seg_stp;

	overlap_list[i].index = i;
	num_overlaps++;
    }

    (void)rt_defoverlap(ap, pp1, reg1, reg2, pp2);
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

	if (!rv)
	    continue;

	/* Check if the out solid belongs to rbB */
	comb = (struct rt_comb_internal *)mf->rbB->intern.idb_ptr;
	rv = check_tree_funcleaf(sim_params->gedp->ged_wdbp->dbip,
				 comb,
				 comb->tree,
				 find_solid,
				 (void *)(overlap_list[i].outsol->st_name));

	if (!rv)
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
	    continue;
	}

	j = mf->num_contacts;
	VMOVE(mf->contacts[j].ptB, overlap_list[i].out_point);
	VMOVE(mf->contacts[j].normalWorldOnB, rt_result.resultant_normal_B);
	mf->contacts[j].depth = -depth;
	mf->num_contacts++;

	bu_log("traverse_normalray_lists : Penetration depth set to %f\n",
	       mf->contacts[j].depth);


    } /* end-for overlap */


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
	VUNITIZE(entry->ray.r_dir);
	{
	    struct application ap;

	    /* initialization of the application structure */
	    RT_APPLICATION_INIT(&ap);
	    ap.a_multioverlap = if_multioverlap;
	    ap.a_logoverlap = rt_silent_logoverlap;
	    ap.a_rt_i = sim_params->rtip;         /* rt_i pointer */

	    /* Set the ray start point and direction rt_shootray() uses these
	     * two to determine what ray to fire.
	     * It's worth nothing that librt assumes units of millimeters.
	     * All geometry is stored as millimeters regardless of the units
	     * set during editing.  There are libbu routines for performing
	     * unit conversions if desired.
	     */
	    VMOVE(ap.a_ray.r_pt, entry->ray.r_pt);
	    VMOVE(ap.a_ray.r_dir, entry->ray.r_dir);

	    (void)rt_shootray(&ap);
	}


	traverse_normalray_lists(current_manifold, sim_params,
				 overlap_min, overlap_max);

	num_overlaps = 0; /* clean up the list */

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

    rt_mf = &(rbB->rt_manifold);

    /* Setup the manifold participant pointers */
    rt_mf->rbA = rbA;
    rt_mf->rbB = rbB;

    /* Get the overlap region */
    get_overlap(rbA, rbB, overlap_min, overlap_max);

    /* Initialize the rayshot results structure, has to be done for each manifold */
    VSETALL(rt_result.resultant_normal_A, SMALL_FASTF);
    VSETALL(rt_result.resultant_normal_B, SMALL_FASTF);
    rt_result.overlap_found = 0;


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
