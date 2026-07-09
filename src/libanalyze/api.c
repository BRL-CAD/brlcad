/*                           A P I . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
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
/** @file api.c
 *
 * Libanalyze API raytracing core and helper functions
 *
 */

#include "common.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "raytrace.h"
#include "vmath.h"
#include "bu/parallel.h"
#include "bu/time.h"

#include "analyze.h"
#include "./analyze_private.h"
#include "./engine.h"

#define ANALYSIS_VOLUME 1
#define ANALYSIS_CENTROIDS 2
#define ANALYSIS_SURF_AREA 4
#define ANALYSIS_MASS 8
#define ANALYSIS_OVERLAPS 16
#define ANALYSIS_MOMENTS 32
#define ANALYSIS_BOX 64
#define ANALYSIS_GAP 128
#define ANALYSIS_EXP_AIR 256 /* exposed air */
#define ANALYSIS_ADJ_AIR 512
#define ANALYSIS_FIRST_AIR 1024
#define ANALYSIS_LAST_AIR 2048
#define ANALYSIS_UNCONF_AIR 4096

/*
 * Minimum fractional volume reduction required for the overlap AABB pre-filter
 * union-bbox fallback to be considered worthwhile.  The candidate union bbox
 * must be smaller than (1 - OV_PREFILTER_MIN_REDUCTION) of the full model
 * volume before we restrict the grid; otherwise the full model bbox is used.
 *
 * With the default of 0.9 the grid is only restricted when the union of
 * candidate intersection AABBs covers less than 90% of the model volume.
 * This constant is used only in the dense-cluster fallback path; the normal
 * path uses per-cluster focused sampling which is always more targeted.
 */
#define OV_PREFILTER_MIN_REDUCTION 0.9

/*
 * Maximum number of AABB-overlap clusters for which the per-cluster focused
 * sampling strategy is applied.  Each cluster gets one independent grid pass
 * restricted to just its members' intersection-volume union.
 *
 * Clusters are the connected components of the region AABB-intersection graph,
 * so this count is always ≤ the number of candidate pairs.  For dense models
 * many pairs collapse into a small number of large clusters; the threshold is
 * deliberately higher than the old per-pair limit.
 *
 * When the number of clusters exceeds this threshold, fall back to the
 * union-bbox strategy: restrict rtip->mdl_min/max to the union of all cluster
 * intersection volumes and run the normal shoot_rays() refinement loop.
 */
#define OV_CLUSTER_MAX 256

/*
 * returns a random angle between 0 and 360 degrees
 * used for when doing surface area analysis to shoot grids at
 * random azimuth and elevation angles.
 */
#define RAND_ANGLE ((rand()/(fastf_t)RAND_MAX) * 360)

/**
 * rt_shootray() was told to call this on a hit.  It passes the
 * application structure which describes the state of the world (see
 * raytrace.h), and a circular linked list of partitions, each one
 * describing one in and out segment of one region.
 *
 * this routine must be prepared to run in parallel
 */
static int
analyze_hit(struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    /* see raytrace.h for all of these guys */
    struct partition *pp;
    point_t pt, opt, last_out_point;
    double dist;       /* the thickness of the partition */
    int last_air = 0;  /* what was the aircode of the last item */
    int air_first = 1; /* are we in an air before a solid */
    double val;
    double last_out_dist = -1.0;
    double gap_dist;
    struct current_state *state = (struct current_state *)ap->A_STATE;

    if (!segs) /* unexpected */
	return 0;

    if (PartHeadp->pt_forw == PartHeadp)
	return 1;


    /* examine each partition until we get back to the head */
    for (pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {

	fastf_t grams_per_cu_mm;
	long int material_id = pp->pt_regionp->reg_gmater;

	if (state->default_den) {
	    /* Aluminium 7xxx series as default material */
	    fastf_t default_density = 2.74; /* Aluminium, 7079-T6 */
	    material_id = -1;
	    grams_per_cu_mm = default_density;
	} else {
	    grams_per_cu_mm = analyze_densities_density(state->densities, material_id);
	}


	/* inhit info */
	dist = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	VJOIN1(opt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

	if (state->debug) {
	    bu_semaphore_acquire(BU_SEM_GENERAL);
	    bu_vls_printf(state->debug_str, "%s %g->%g\n", pp->pt_regionp->reg_name,
			  pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist);
	    bu_semaphore_release(BU_SEM_GENERAL);
	}

	if (state->analysis_flags & ANALYSIS_EXP_AIR) {

	    gap_dist = (pp->pt_inhit->hit_dist - last_out_dist);

	    /* if air is first on the ray, or we're moving from void/gap to air
	     * then this is exposed air
	     */
	    if (pp->pt_regionp->reg_aircode &&
		(air_first || gap_dist > state->overlap_tolerance)) {
		state->exp_air_callback(pp, last_out_point, pt, opt, state->exp_air_callback_data);
	    } else {
		air_first = 0;
	    }
	}

	/* looking for voids in the model */
	if (state->analysis_flags & ANALYSIS_GAP) {
	    if (pp->pt_back != PartHeadp) {
		/* if this entry point is further than the previous
		 * exit point then we have a void
		 */
		gap_dist = pp->pt_inhit->hit_dist - last_out_dist;

		if (gap_dist > state->overlap_tolerance) {
		    state->gaps_callback(&ap->a_ray, pp, gap_dist, pt, state->gaps_callback_data);
		}
	    }
	}

	/* computing the mass of the objects */
	if (state->analysis_flags & ANALYSIS_MASS) {
	    if (state->debug) {
		bu_semaphore_acquire(BU_SEM_GENERAL);
		bu_vls_printf(state->debug_str, "Hit %s doing mass\n", pp->pt_regionp->reg_name);
		bu_semaphore_release(BU_SEM_GENERAL);
	    }

	    /* make sure mater index is within range of densities */
	    if (material_id < 0 && state->default_den == 0) {
		bu_semaphore_acquire(BU_SEM_GENERAL);
		bu_vls_printf(state->log_str, "Density index %d on region %s is not in density table.\nSet GIFTmater on region or add entry to density table\n",
			pp->pt_regionp->reg_gmater,
			pp->pt_regionp->reg_name); /* XXX this should do something else */
		bu_semaphore_release(BU_SEM_GENERAL);
		return ANALYZE_ERROR;
	    }

	    {
		struct per_region_data *prd;
		vect_t cmass;
		vect_t lenTorque;
		fastf_t Lx = state->span[0]/state->steps[0];
		fastf_t Ly = state->span[1]/state->steps[1];
		fastf_t Lz = state->span[2]/state->steps[2];
		fastf_t Lx_sq;
		fastf_t Ly_sq;
		fastf_t Lz_sq;
		fastf_t cell_area;
		int los;

		switch (state->i_axis) {
		    case 0:
			Lx_sq = dist*pp->pt_regionp->reg_los*0.01;
			Lx_sq *= Lx_sq;
			Ly_sq = Ly*Ly;
			Lz_sq = Lz*Lz;
			cell_area = Ly_sq;
			break;
		    case 1:
			Lx_sq = Lx*Lx;
			Ly_sq = dist*pp->pt_regionp->reg_los*0.01;
			Ly_sq *= Ly_sq;
			Lz_sq = Lz*Lz;
			cell_area = Lx_sq;
			break;
		    case 2:
		    default:
			Lx_sq = Lx*Lx;
			Ly_sq = Ly*Ly;
			Lz_sq = dist*pp->pt_regionp->reg_los*0.01;
			Lz_sq *= Lz_sq;
			cell_area = Lx_sq;
			break;
		}

		/* factor in the density of this object mass
		 * computation, factoring in the LOS percentage
		 * material of the object
		 */
		los = pp->pt_regionp->reg_los;

		if (los < 1) {
		    bu_semaphore_acquire(BU_SEM_GENERAL);
		    bu_vls_printf(state->log_str, "bad LOS (%d) on %s\n", los, pp->pt_regionp->reg_name);
		    bu_semaphore_release(BU_SEM_GENERAL);
		}

		/* accumulate the total mass values */
		val = grams_per_cu_mm * dist * (pp->pt_regionp->reg_los * 0.01);
		ap->A_LENDEN += val;

		prd = ((struct per_region_data *)pp->pt_regionp->reg_udata);
		/* accumulate the per-region per-view mass values */
		bu_semaphore_acquire(state->sem_stats);
		prd->r_lenDensity[state->i_axis] += val;

		/* accumulate the per-object per-view mass values */
		prd->optr->o_lenDensity[state->i_axis] += val;

		if (state->analysis_flags & ANALYSIS_CENTROIDS) {
		    /* calculate the center of mass for this partition */
		    VJOIN1(cmass, pt, dist*0.5, ap->a_ray.r_dir);

		    /* calculate the lenTorque for this partition (i.e. centerOfMass * lenDensity) */
		    VSCALE(lenTorque, cmass, val);

		    /* accumulate per-object per-view torque values */
		    VADD2(&prd->optr->o_lenTorque[state->i_axis*3], &prd->optr->o_lenTorque[state->i_axis*3], lenTorque);

		    /* accumulate the total lenTorque */
		    VADD2(&state->m_lenTorque[state->i_axis*3], &state->m_lenTorque[state->i_axis*3], lenTorque);

		    if (state->analysis_flags & ANALYSIS_MOMENTS) {
			vectp_t moi;
			vectp_t poi;
			fastf_t dx_sq = cmass[X]*cmass[X];
			fastf_t dy_sq = cmass[Y]*cmass[Y];
			fastf_t dz_sq = cmass[Z]*cmass[Z];
			fastf_t mass = val * cell_area;
			static const fastf_t ONE_TWELFTH = 1.0 / 12.0;

			/* Collect moments and products of inertia for the current object */
			moi = &prd->optr->o_moi[state->i_axis*3];
			moi[X] += ONE_TWELFTH*mass*(Ly_sq + Lz_sq) + mass*(dy_sq + dz_sq);
			moi[Y] += ONE_TWELFTH*mass*(Lx_sq + Lz_sq) + mass*(dx_sq + dz_sq);
			moi[Z] += ONE_TWELFTH*mass*(Lx_sq + Ly_sq) + mass*(dx_sq + dy_sq);
			poi = &prd->optr->o_poi[state->i_axis*3];
			poi[X] -= mass*cmass[X]*cmass[Y];
			poi[Y] -= mass*cmass[X]*cmass[Z];
			poi[Z] -= mass*cmass[Y]*cmass[Z];

			/* Collect moments and products of inertia for all objects */
			moi = &state->m_moi[state->i_axis*3];
			moi[X] += ONE_TWELFTH*mass*(Ly_sq + Lz_sq) + mass*(dy_sq + dz_sq);
			moi[Y] += ONE_TWELFTH*mass*(Lx_sq + Lz_sq) + mass*(dx_sq + dz_sq);
			moi[Z] += ONE_TWELFTH*mass*(Lx_sq + Ly_sq) + mass*(dx_sq + dy_sq);
			poi = &state->m_poi[state->i_axis*3];
			poi[X] -= mass*cmass[X]*cmass[Y];
			poi[Y] -= mass*cmass[X]*cmass[Z];
			poi[Z] -= mass*cmass[Y]*cmass[Z];
		    }
		}
		bu_semaphore_release(state->sem_stats);

	    }
	}

	/* compute the surface area of the object */
	if(state->analysis_flags & ANALYSIS_SURF_AREA) {
	    struct per_region_data *prd = ((struct per_region_data *)pp->pt_regionp->reg_udata);

	    if (state->sampler == ANALYZE_SAMPLER_CROFTON) {
		/* Crofton path: count surface boundary crossings.
		 * Each solid partition contributes 2 crossings (entry + exit).
		 * The Crofton SA formula is applied in shoot_rays_crofton() after
		 * all rays have been fired:
		 *   SA = (4 * π * R²) * total_crossings / (2 * N)
		 */
		bu_semaphore_acquire(state->sem_crofton);
		state->crofton_crossings += 2;
		prd->crofton_crossings += 2;
		bu_semaphore_release(state->sem_crofton);
	    } else {
		/* Grid-based path: project cell area through the incidence angle. */
		fastf_t Lx = state->span[0] / state->steps[0];
		fastf_t Ly = state->span[1] / state->steps[1];
		fastf_t Lz = state->span[2] / state->steps[2];
		fastf_t cell_area;
		vect_t inormal = VINIT_ZERO;
		vect_t onormal = VINIT_ZERO;
		double icos, ocos;
		switch(state->i_axis) {
		    case 0:
			cell_area = Ly*Ly;
			break;
		    case 1:
			cell_area = Lz*Lz;
			break;
		    case 2:
		    default:
			cell_area = Lx*Lx;
		}

		bu_semaphore_acquire(state->sem_worker);
		/* factor in the normal vector to find how 'skew' the surface is */
		RT_HIT_NORMAL(inormal, pp->pt_inhit, pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip);
		VREVERSE(inormal, inormal);
		RT_HIT_NORMAL(onormal, pp->pt_outhit, pp->pt_outseg->seg_stp, &(ap->a_ray), pp->pt_outflip);

		/* find the cosine angle between the normal vector and ray_direction */
		icos = VDOT(inormal, ap->a_ray.r_dir)/(MAGSQ(inormal)*MAGSQ(ap->a_ray.r_dir));
		ocos = VDOT(onormal, ap->a_ray.r_dir)/(MAGSQ(onormal)*MAGSQ(ap->a_ray.r_dir));

		/* add to region surface area */
		prd->r_area[state->curr_view] += (cell_area/icos);
		prd->r_area[state->curr_view] += (cell_area/ocos);

		/* add to object surface area */
		prd->optr->o_area[state->curr_view] += (cell_area/icos);
		prd->optr->o_area[state->curr_view] += (cell_area/ocos);

		bu_semaphore_release(state->sem_worker);
	    }
	}

	/* compute the volume of the object.
	 *
	 * When state->background_mv is set we always accumulate chord
	 * lengths into A_LEN / r_len / o_len even if ANALYSIS_VOLUME is
	 * not in analysis_flags.  This is essentially free (one fp-add per
	 * partition) and lets check_terminate() use volume convergence as
	 * a proxy for "have we sampled densely enough?" when running
	 * validation-only analyses (overlaps, air checks, etc.).
	 */
	if ((state->analysis_flags & ANALYSIS_VOLUME) || state->background_mv) {
	    struct per_region_data *prd = ((struct per_region_data *)pp->pt_regionp->reg_udata);
	    ap->A_LEN += dist; /* accumulate total chord length */
	    {
		bu_semaphore_acquire(state->sem_worker);
		/* add to region volume */
		prd->r_len[state->curr_view] += dist;
		/* add to object volume (optr may be NULL for unmapped regions) */
		if (prd->optr)
		    prd->optr->o_len[state->curr_view] += dist;
		bu_semaphore_release(state->sem_worker);
	    }
	    /* optional debug/plot output only for explicitly requested volume */
	    if (state->analysis_flags & ANALYSIS_VOLUME) {
		if (state->debug) {
		    bu_semaphore_acquire(BU_SEM_GENERAL);
		    bu_vls_printf(state->debug_str, "\t\tvol hit %s oDist:%g objVol:%g %s\n",
				  pp->pt_regionp->reg_name, dist, prd->optr ? prd->optr->o_len[state->curr_view] : 0.0, prd->optr ? prd->optr->o_name : "?");
		    bu_semaphore_release(BU_SEM_GENERAL);
		}
		if (state->plot_volume) {
		    bu_semaphore_acquire(state->sem_plot);
		    if (ap->a_user & 1) {
			pl_color(state->plot_volume, 128, 255, 192);  /* pale green */
		    } else {
			pl_color(state->plot_volume, 128, 192, 255);  /* cyan */
		    }
		    pdv_3line(state->plot_volume, pt, opt);
		    bu_semaphore_release(state->sem_plot);
		}
	    }
	}

	/* look for two adjacent air regions */
	if (state->analysis_flags & ANALYSIS_ADJ_AIR) {
	    if (last_air && pp->pt_regionp->reg_aircode &&
		pp->pt_regionp->reg_aircode != last_air) {
		state->adj_air_callback(&ap->a_ray, pp, pt, state->adj_air_callback_data);
	    }
	}

	if (pp->pt_regionp->reg_aircode) {
	    /* look for air first on shotlines */
	    if (pp->pt_back == PartHeadp) {
		if (state->analysis_flags & ANALYSIS_FIRST_AIR)
		    state->first_air_callback(&ap->a_ray, pp, state->first_air_callback_data);
	    } else {
		/* else add to unconfined air regions list */
		if (state->analysis_flags & ANALYSIS_UNCONF_AIR) {
		    state->unconf_air_callback(&ap->a_ray, pp, pp->pt_back, state->unconf_air_callback_data);
		}
	    }
	    /* look for air last on shotlines */
	    if (pp->pt_forw == PartHeadp) {
		if (state->analysis_flags & ANALYSIS_LAST_AIR)
		    state->last_air_callback(&ap->a_ray, pp, state->last_air_callback_data);
	    } else {
		/* else add to unconfined air regions list */
		if (state->analysis_flags & ANALYSIS_UNCONF_AIR) {
		    state->unconf_air_callback(&ap->a_ray, pp->pt_forw, pp, state->unconf_air_callback_data);
		}
	    }
	}

	/* note that this region has been seen */
	((struct per_region_data *)pp->pt_regionp->reg_udata)->hits++;

	last_air = pp->pt_regionp->reg_aircode;
	last_out_dist = pp->pt_outhit->hit_dist;
	VJOIN1(last_out_point, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
    }

    if (state->analysis_flags & ANALYSIS_EXP_AIR && last_air) {
	/* the last thing we hit was air.  Make a note of that */
	pp = PartHeadp->pt_back;
	state->exp_air_callback(pp, last_out_point, pt, opt, state->exp_air_callback_data);
    }

    /* This value is returned by rt_shootray a hit usually returns 1,
     * miss 0.
     */
    return 1;
}


/**
 * rt_shootray() was told to call this on a miss.
 *
 * This routine must be prepared to run in parallel
 */
static int
analyze_miss(struct application *ap)
{
    RT_CK_APPLICATION(ap);

    return 0;
}


/**
 * Write end points of partition to the standard output.  If this
 * routine return !0, this partition will be dropped from the boolean
 * evaluation.
 *
 * Returns:
 * 0 to eliminate partition with overlap entirely
 * 1 to retain partition in output list, claimed by reg1
 * 2 to retain partition in output list, claimed by reg2
 *
 * This routine must be prepared to run in parallel
 */
static int
analyze_overlap(struct application *ap,
		struct partition *pp,
		struct region *reg1,
		struct region *reg2,
		struct partition *hp)
{

    struct xray *rp = &ap->a_ray;
    struct current_state *state = (struct current_state *)ap->A_STATE;
    struct hit *ihitp = pp->pt_inhit;
    struct hit *ohitp = pp->pt_outhit;
    vect_t ihit;
    double depth;

    if (!hp) /* unexpected */
	return 0;

    /* if one of the regions is air, let it loose */
    if (reg1->reg_aircode && ! reg2->reg_aircode)
	return 2;
    if (reg2->reg_aircode && ! reg1->reg_aircode)
	return 1;

    depth = ohitp->hit_dist - ihitp->hit_dist;

    if (depth < state->overlap_tolerance)
	/* too small to matter, pick one or none */
	return 1;

    VJOIN1(ihit, rp->r_pt, ihitp->hit_dist, rp->r_dir);

    if (state->analysis_flags & ANALYSIS_OVERLAPS) {
	bu_semaphore_acquire(state->sem_worker);
	add_unique_pair(state->overlapList, reg1, reg2, depth, ihit);
	bu_semaphore_release(state->sem_worker);
	state->overlaps_callback(&ap->a_ray, pp, reg1, reg2, depth, state->overlaps_callback_data);
    }  else {
	bu_semaphore_acquire(state->sem_worker);
	bu_vls_printf(state->log_str, "overlap %s %s\n", reg1->reg_name, reg2->reg_name);
	bu_semaphore_release(state->sem_worker);
    }

    /* XXX We should somehow flag the volume/mass calculations as invalid */

    /* since we have no basis to pick one over the other, just pick */
    return 1;	/* No further consideration to this partition */
}


/**
 * Returns
 * 0 on success
 * !0 on failure
 */
static int
densities_from_file(struct current_state *state, char *name)
{
    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    struct bu_mapped_file *dfile = NULL;
    char *buf = NULL;
    int ret = 0;
    int ecnt = 0;

    if (!bu_file_exists(name, NULL)) {
	bu_log("Could not find density file - %s\n", name);
	return ANALYZE_ERROR;
    }

    dfile = bu_open_mapped_file(name, "densities file");
    if (!dfile) {
	bu_log("Could not open density file - %s\n", name);
	return ANALYZE_ERROR;
    }

    buf = (char *)(dfile->buf);

    (void)analyze_densities_create(&(state->densities));

    ret = analyze_densities_load(state->densities, buf, &msgs, &ecnt);

    if (ecnt && bu_vls_strlen(&msgs)) {
	bu_log("Problem reading densities file:\n%s\n", bu_vls_cstr(&msgs));
    }
    bu_vls_free(&msgs);

    bu_close_mapped_file(dfile);

    return (ret <= 0) ? -1 : 0;
}


/**
 * Returns
 * 0 on success
 * !0 on failure
 */
static int
densities_from_database(struct current_state *state, struct rt_i *rtip)
{
    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_binunif_internal *bu;
    int ret;
    int ecnt = 0;
    char *buf;

    dp = db_lookup(rtip->rti_dbip, "_DENSITIES", LOOKUP_QUIET);
    if (dp == (struct directory *)NULL) {
	bu_log("No \"_DENSITIES\" density table object in database.");
	return ANALYZE_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, rtip->rti_dbip, NULL) < 0) {
	bu_log("could not import %s\n", dp->d_namep);
	return ANALYZE_ERROR;
    }

    if ((intern.idb_major_type & DB5_MAJORTYPE_BINARY_MASK) == 0)
	return ANALYZE_ERROR;

    bu = (struct rt_binunif_internal *)intern.idb_ptr;

    RT_CHECK_BINUNIF (bu);

    (void)analyze_densities_create(&(state->densities));

    buf = (char *)bu_calloc(bu->count+1, sizeof(char), "density buffer");
    memcpy(buf, bu->u.int8, bu->count);

    ret = analyze_densities_load(state->densities, buf, &msgs, &ecnt);

    if (ecnt && bu_vls_strlen(&msgs)) {
	bu_log("Problem reading densities file:\n%s\n", bu_vls_cstr(&msgs));
    }
    bu_vls_free(&msgs);

    bu_free((void *)buf, "density buffer");

    return ret;
}


/**
 * These checks are unique because they must both be completed.  Early
 * termination before they are done is not an option.  The results
 * computed here are used later.
 *
 * Returns:
 * 0 terminate
 * 1 continue processing
 */
static int
mass_volume_surf_area_terminate_check(struct current_state *state)
{
    /* Both mass and volume computations rely on this routine to
     * compute values that are printed in summaries.  Hence, both
     * checks must always be done before this routine exits.  So we
     * store the status (can we terminate processing?) in this
     * variable and act on it once both volume and mass computations
     * are done.
     */
    int can_terminate = 1;

    double low, hi, val, delta;

    if (state->analysis_flags & ANALYSIS_MASS) {
	/* for each object, compute the mass for all views */
	int obj;

	for (obj = 0; obj < state->num_objects; obj++) {
	    int view;
	    double tmp;

	    if (state->verbose)
		bu_vls_printf(state->verbose_str, "object %d of %d\n", obj, state->num_objects);

	    /* compute mass of object for given view */
	    low = INFINITY;
	    hi = -INFINITY;
	    tmp = 0.0;
	    for (view = 0; view < state->num_views; view++) {
		val = state->objs[obj].o_mass[view] =
		    state->objs[obj].o_lenDensity[view] * (state->area[view] / state->shots[view]);
		if (state->verbose)
		    bu_vls_printf(state->verbose_str, "Value : %g\n", val);
		V_MIN(low, val);
		V_MAX(hi, val);
		tmp += val;
	    }
	    delta = hi - low;

	    if (state->verbose)
		bu_vls_printf(state->verbose_str, "\t%s running avg mass %g gram hi=(%g) low=(%g)\n", state->objs[obj].o_name, (tmp / state->num_views), hi, low );

	    if (delta > state->mass_tolerance) {
		/* Absolute tolerance not met; check significant-digits criterion */
		int digits_ok = 0;
		if (state->required_digits > 0.0) {
		    double avg = tmp / state->num_views;
		    if (delta <= 0.0)
			digits_ok = 1;
		    else if (avg > 0.0)
			digits_ok = (log10(avg / delta) >= state->required_digits);
		}
		if (!digits_ok) {
		    /* this object differs too much in each view, so we
		     * need to refine the grid. signal that we cannot
		     * terminate.
		     */
		    can_terminate = 0;
		    if (state->verbose)
			bu_vls_printf(state->verbose_str, "\t%s differs too much in mass per view.\n",
				state->objs[obj].o_name);
		}
	    }
	}
	if (can_terminate) {
	    if (state->verbose)
		bu_vls_printf(state->verbose_str, "all objects within tolerance on mass calculation\n");
	}
    }

    if (state->analysis_flags & ANALYSIS_VOLUME) {
	/* find the range of values for object volumes */
	int obj;

	/* for each object, compute the volume for all views */
	for (obj = 0; obj < state->num_objects; obj++) {
	    int view;
	    double tmp;

	    /* compute volume of object for given view */
	    low = INFINITY;
	    hi = -INFINITY;
	    tmp = 0.0;
	    for (view = 0; view < state->num_views; view++) {
		val = state->objs[obj].o_volume[view] =
		    state->objs[obj].o_len[view] * (state->area[view] / state->shots[view]);
		V_MIN(low, val);
		V_MAX(hi, val);
		tmp += val;
	    }
	    delta = hi - low;

	    if (state->verbose)
		bu_vls_printf(state->verbose_str, "\t%s running avg volume %g cu mm hi=(%g) low=(%g)\n", state->objs[obj].o_name, (tmp / state->num_views), hi, low);

	    if (delta > state->volume_tolerance) {
		/* Absolute tolerance not met; check significant-digits criterion */
		int digits_ok = 0;
		if (state->required_digits > 0.0) {
		    double avg = tmp / state->num_views;
		    if (delta <= 0.0)
			digits_ok = 1;
		    else if (avg > 0.0)
			digits_ok = (log10(avg / delta) >= state->required_digits);
		}
		if (!digits_ok) {
		    /* this object differs too much in each view, so we
		     * need to refine the grid.
		     */
		    can_terminate = 0;
		    if (state->verbose)
			bu_vls_printf(state->verbose_str, "\tvolume tol not met on %s.  Refine grid\n", state->objs[obj].o_name);
		    break;
		}
	    }
	}
    }

    if (state->analysis_flags & ANALYSIS_SURF_AREA) {
	/* find the range of values for surface area */
	int obj, i;
	struct region *regp;

	/* for each object, compute the surface area for all views */
	for (obj = 0; obj < state->num_objects; obj++) {
	    int view;
	    double tmp;

	    low = INFINITY;
	    hi = -INFINITY;
	    tmp = 0.0;
	    for (view = 0; view < state->num_views; view++) {
		val = state->objs[obj].o_surf_area[view] = state->objs[obj].o_area[view];
		/* divide by 4 to prepare for next iteration of grid refinement */
		state->objs[obj].o_area[view] /= 4;
		V_MIN(low, val);
		V_MAX(hi, val);
		tmp += val;
	    }
	    delta = hi - low;

	    if (state->verbose)
		bu_vls_printf(state->verbose_str, "\t%s running avg surface area %g mm^2 hi=(%g) low=(%g)\n", state->objs[obj].o_name, (tmp / state->num_views), hi, low);

	    if (delta > state->sa_tolerance) {
		/* Absolute tolerance not met; check significant-digits criterion */
		int digits_ok = 0;
		if (state->required_digits > 0.0) {
		    double avg = tmp / state->num_views;
		    if (delta <= 0.0)
			digits_ok = 1;
		    else if (avg > 0.0)
			digits_ok = (log10(avg / delta) >= state->required_digits);
		}
		if (!digits_ok) {
		    /* this object differs too much in each view, so we
		     * need to refine the grid.
		     */
		    can_terminate = 0;
		    if (state->verbose)
			bu_vls_printf(state->verbose_str, "\tsurface area tol not met on %s.  Refine grid\n", state->objs[obj].o_name);
		    break;
		}
	    }
	}

	/* for each region, compute the surface area for all views */
	for (i = 0, BU_LIST_FOR (regp, region, &(state->rtip->HeadRegion)), i++) {
	    int view;
	    for (view = 0; view < state->num_views; view++) {
		state->reg_tbl[i].r_surf_area[view] = state->reg_tbl[i].r_area[view];
		/* divide by 4 to prepare for next iteration of grid refinement */
		state->reg_tbl[i].r_area[view] /= 4;
	    }
	}
    }

    if (can_terminate) {
	return 0; /* signal we don't want to go onward */
    }
    return 1;
}

/**
 * Check to see if we are done processing due to some user specified
 * limit being achieved.
 *
 * Returns:
 * 0 Terminate
 * 1 Continue processing
 */
static int
check_terminate(struct current_state *state)
{
    int wv_status;
    int view, obj;

    /* --- Wall-clock timeout check ---
     * This is tested before the (potentially expensive) convergence check
     * so that the loop exits promptly when the budget is exhausted.
     */
    if (state->timeout_ms > 0) {
	int64_t elapsed_us = bu_gettime() - state->start_time_us;
	long elapsed_ms = (long)(elapsed_us / 1000);
	if (elapsed_ms >= state->timeout_ms) {
	    bu_log("NOTE: Stopped, timeout of %ld ms reached (elapsed %ld ms).\n",
		   state->timeout_ms, elapsed_ms);
	    return 0;
	}
    }

    /* this computation is done first, because there are side effects
     * that must be obtained whether we terminate or not
     */
    wv_status = mass_volume_surf_area_terminate_check(state);

    /* if we've reached the grid limit, we're done, no matter what */
    if (state->gridSpacing < state->gridSpacingLimit) {
	bu_log("NOTE: Stopped, grid spacing refined to %g (below lower limit %g).\n",
		state->gridSpacing, state->gridSpacingLimit);
	return 0;
    }
    if (state->analysis_flags & (ANALYSIS_MASS|ANALYSIS_VOLUME|ANALYSIS_SURF_AREA)) {
	if (wv_status == 0) {
	    if (state->verbose)
		bu_vls_printf(state->verbose_str, "%s: Volume/mass tolerance met. Terminate\n", CPP_FILELINE);
	    return 0; /* terminate */
	}
    }

    /* --- Background volume convergence proxy ---
     *
     * When the caller only requested validation analyses (overlaps, air
     * checks, etc.) without any quantitative analysis (V/M/SA), we use
     * the background chord-length accumulation as a proxy for sampling
     * adequacy.  Once the implied volume estimates agree across views to
     * within 1% (or the required_digits criterion), we consider the
     * sample density sufficient for the validation pass as well.
     *
     * Rationale: the probability of discovering a new overlap or air
     * boundary on the next grid-refinement pass falls off as the grid
     * spacing decreases, just as measurement variance falls off.  Using
     * volume convergence as a halting signal is a conservative proxy
     * that avoids an unbounded refinement loop when no explicit V/M/SA
     * tolerance was supplied.
     */
    if (state->background_mv &&
	!(state->analysis_flags & (ANALYSIS_MASS|ANALYSIS_VOLUME|ANALYSIS_SURF_AREA))) {

	/* 1% relative spread as the default background proxy threshold */
	static const double BG_REL_TOL = 0.01;
	int bg_converged = 1;

	for (obj = 0; obj < state->num_objects; obj++) {
	    double low = INFINITY, hi = -INFINITY, tmp = 0.0;
	    for (view = 0; view < state->num_views; view++) {
		double val;
		if (state->shots[view] == 0) continue;
		val = state->objs[obj].o_len[view] *
		    (state->area[view] / (double)state->shots[view]);
		if (val < low) low = val;
		if (val > hi)  hi  = val;
		tmp += val;
	    }
	    if (state->num_views < 1) continue;

	    double avg   = tmp / state->num_views;
	    double delta = hi - low;

	    /* Absolute relative test */
	    if (avg > 0.0 && delta > avg * BG_REL_TOL) {
		/* Not yet converged by relative tolerance; check digits */
		int digits_ok = 0;
		if (state->required_digits > 0.0 && delta > 0.0)
		    digits_ok = (log10(avg / delta) >= state->required_digits);
		if (!digits_ok) {
		    bg_converged = 0;
		    break;
		}
	    }
	}

	if (bg_converged) {
	    if (state->verbose)
		bu_vls_printf(state->verbose_str,
			      "Background volume proxy converged; sampling adequate for validation.\n");
	    return 0;
	}
    }
    for (view=0; view < state->num_views; view++) {
	for (obj = 0; obj < state->num_objects; obj++) {
	    VSCALE(&state->objs[obj].o_moi[view*3], &state->objs[obj].o_moi[view*3], 0.25);
	    VSCALE(&state->objs[obj].o_poi[view*3], &state->objs[obj].o_poi[view*3], 0.25);
	}

	VSCALE(&state->m_moi[view*3], &state->m_moi[view*3], 0.25);
	VSCALE(&state->m_poi[view*3], &state->m_poi[view*3], 0.25);
    }
    return 1;
}

/**
 * This routine must be prepared to run in parallel
 */
static void
analyze_worker(int cpu, void *ptr)
{
    struct application ap;
    struct current_state *state = (struct current_state *)ptr;
    unsigned long shot_cnt;

    if (state->aborted)
	return;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = (struct rt_i *)state->rtip;	/* application uses this instance */
    ap.a_hit = analyze_hit;    /* where to go on a hit */
    ap.a_miss = analyze_miss;  /* where to go on a miss */
    ap.a_resource = &state->resp[cpu];
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.A_LENDEN = 0.0; /* really the cumulative length*density for mass computation*/
    ap.A_LEN = 0.0;    /* really the cumulative length for volume computation */
    ap.A_STATE = ptr; /* really copying the state ptr to the a_uptr */
    ap.a_overlap = analyze_overlap;

    shot_cnt = 0;
    while (1) {
	bu_semaphore_acquire(state->sem_worker);
	if (rectangular_grid_generator(&ap.a_ray, state->grid) == 1){
	    bu_semaphore_release(state->sem_worker);
	    break;
	}
	ap.a_user = (int)(state->grid->current_point / (state->grid->x_points));
	bu_semaphore_release(state->sem_worker);
	(void)rt_shootray(&ap);
	if (state->aborted)
	    return;
	shot_cnt++;
    }

    /* There's nothing else left to work on in this view.  It's time
     * to add the values we have accumulated to the totals for the
     * view and return.  When all threads have been through here,
     * we'll have returned to serial computation.
     */
    bu_semaphore_acquire(state->sem_stats);
    state->shots[state->curr_view] += shot_cnt;
    state->m_lenDensity[state->curr_view] += ap.A_LENDEN; /* add our length*density value */
    state->m_len[state->curr_view] += ap.A_LEN; /* add our volume value */
    bu_semaphore_release(state->sem_stats);
}


/**
 * Worker function for the Crofton isotropic random sampler.
 *
 * Mirrors analyze_worker() but uses crofton_grid_generator() to obtain rays,
 * accumulating results into view index 0 (the Crofton pass uses num_views=1).
 *
 * Thread safety: the generator call is serialised under sem_worker (the same
 * mechanism used by analyze_worker for the rectangular grid), so the PRNG
 * state in crofton_g is always updated atomically.
 */
static void
analyze_worker_crofton(int cpu, void *ptr)
{
    struct application ap;
    struct current_state *state = (struct current_state *)ptr;
    unsigned long shot_cnt;

    if (state->aborted)
	return;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = (struct rt_i *)state->rtip;
    ap.a_hit = analyze_hit;
    ap.a_miss = analyze_miss;
    ap.a_resource = &state->resp[cpu];
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.A_LENDEN = 0.0;
    ap.A_LEN = 0.0;
    ap.A_STATE = ptr;
    ap.a_overlap = analyze_overlap;
    ap.a_user = 0; /* no view-row index needed for Crofton */

    shot_cnt = 0;
    while (1) {
	struct xray ray;
	bu_semaphore_acquire(state->sem_worker);
	if (crofton_grid_generator(&ray, &state->crofton_g) == 1) {
	    bu_semaphore_release(state->sem_worker);
	    break;
	}
	VMOVE(ap.a_ray.r_pt,  ray.r_pt);
	VMOVE(ap.a_ray.r_dir, ray.r_dir);
	bu_semaphore_release(state->sem_worker);

	(void)rt_shootray(&ap);
	if (state->aborted)
	    return;
	shot_cnt++;
    }

    bu_semaphore_acquire(state->sem_stats);
    state->shots[0] += shot_cnt;           /* Crofton uses view index 0 */
    state->m_lenDensity[0] += ap.A_LENDEN;
    state->m_len[0] += ap.A_LEN;
    bu_semaphore_release(state->sem_stats);
}


/**
 * shoot_rays_crofton - fire n_rays isotropic Crofton rays in parallel.
 *
 * After all rays have been fired the function applies the Crofton formulas to
 * fill the per-region surface-area fields (when ANALYSIS_SURF_AREA is set) so
 * that the existing analyze_surf_area*() getter functions return correct values.
 *
 * The existing volume / mass / centroid getters already return correct values
 * for the Crofton case once state->area[0] and state->shots[0] are overridden
 * to π*R² and n_rays respectively (see setup block below).
 *
 * @param state  fully initialised current_state (rtip ready, allocate_region_data done)
 * @param n_rays number of isotropic random rays to fire
 */
static void
shoot_rays_crofton(struct current_state *state, size_t n_rays)
{
    int i;

    /* Initialise the Crofton generator from the model bounding box. */
    crofton_grid_setup(&state->crofton_g,
		       state->rtip->mdl_min,
		       state->rtip->mdl_max,
		       n_rays,
		       0 /* time-based seed */);
    state->crofton_radius = state->crofton_g.radius;

    /* Reset per-model Crofton accumulators. */
    state->crofton_crossings = 0;
    state->crofton_n_rays    = 0;

    /* Zero per-region crossing counts. */
    for (i = 0; i < state->num_regions; i++)
	state->reg_tbl[i].crofton_crossings = 0;

    /* Override area and num_views so that the existing getter functions
     * (analyze_total_volume, analyze_total_mass, analyze_centroid, …) give
     * correct Crofton estimates once shots[0] is filled in by the workers:
     *
     *   volume = m_len[0]        * (area[0] / shots[0])
     *          = chord_sum        * (π*R²    / n_rays)     ← Crofton formula ✓
     *   mass   = m_lenDensity[0] * (area[0] / shots[0])   ← same ✓
     */
    state->num_views = 1;
    state->curr_view = 0;
    state->i_axis    = 0;
    state->u_axis    = 1;
    state->v_axis    = 2;
    state->area[0]   = M_PI * state->crofton_radius * state->crofton_radius;
    state->area[1]   = state->area[0];
    state->area[2]   = state->area[0];
    /* steps[] are referenced by the grid SA formula only, but set them to
     * something sensible so we don't divide by zero if the path is hit. */
    VSETALL(state->steps, 1);

    /* Fire all rays. */
    state->current_cell_area = state->area[0] / (double)n_rays;
    bu_parallel(analyze_worker_crofton, state->ncpu, (void *)state);

    /* After workers finish, shots[0] holds the actual ray count. */
    state->crofton_n_rays = state->shots[0];

    /* Apply the Cauchy-Crofton surface-area formula per-region and aggregate
     * to the per-object surface-area arrays.
     *
     * SA_region = (4 * π * R²) * region_crossings / (2 * N)
     *
     * This is the correct formula because each Crofton ray is an isotropic
     * line through the bounding sphere of radius R.  The total crossing count
     * for the model equals the sum of per-region counts (boundaries not
     * shared between distinct regions each contribute to exactly one region).
     */
    if ((state->analysis_flags & ANALYSIS_SURF_AREA) && state->crofton_n_rays > 0) {
	for (i = 0; i < state->num_regions; i++) {
	    double r_sa = crofton_surface_area(
		    state->reg_tbl[i].crofton_crossings,
		    state->crofton_n_rays,
		    state->crofton_radius);
	    /* Store in both r_surf_area and r_area so the getter functions that
	     * read r_surf_area[0] (set by mass_volume_surf_area_terminate_check)
	     * and r_area[0] (used as input there) both see the correct value. */
	    state->reg_tbl[i].r_surf_area[0] = r_sa;
	    state->reg_tbl[i].r_area[0]      = r_sa;

	    if (state->reg_tbl[i].optr) {
		state->reg_tbl[i].optr->o_surf_area[0] += r_sa;
		state->reg_tbl[i].optr->o_area[0]      += r_sa;
	    }
	}
    }

    if (state->verbose)
	bu_vls_printf(state->verbose_str,
		      "Crofton: fired %zu rays, %zu boundary crossings.\n",
		      state->crofton_n_rays, state->crofton_crossings);
}

/**
 * Worker function for the rotated-grid sampler (ANALYZE_SAMPLER_ROTATED).
 *
 * Mirrors analyze_worker() but consults state->rot_grid[state->curr_view]
 * instead of the rectangular grid, allowing an arbitrary ray direction.
 */
static void
analyze_worker_rotated(int cpu, void *ptr)
{
    struct application ap;
    struct current_state *state = (struct current_state *)ptr;
    unsigned long shot_cnt;
    int view;

    if (state->aborted)
	return;

    view = state->curr_view;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i     = (struct rt_i *)state->rtip;
    ap.a_hit      = analyze_hit;
    ap.a_miss     = analyze_miss;
    ap.a_resource = &state->resp[cpu];
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.A_LENDEN   = 0.0;
    ap.A_LEN      = 0.0;
    ap.A_STATE    = ptr;
    ap.a_overlap  = analyze_overlap;
    ap.a_user     = view;

    shot_cnt = 0;
    while (1) {
	struct xray ray;
	bu_semaphore_acquire(state->sem_worker);
	if (rotated_grid_generator(&ray, &state->rot_grid[view]) == 1) {
	    bu_semaphore_release(state->sem_worker);
	    break;
	}
	VMOVE(ap.a_ray.r_pt,  ray.r_pt);
	VMOVE(ap.a_ray.r_dir, ray.r_dir);
	bu_semaphore_release(state->sem_worker);

	(void)rt_shootray(&ap);
	if (state->aborted)
	    return;
	shot_cnt++;
    }

    bu_semaphore_acquire(state->sem_stats);
    state->shots[state->curr_view] += shot_cnt;
    state->m_lenDensity[state->curr_view] += ap.A_LENDEN;
    state->m_len[state->curr_view]        += ap.A_LEN;
    bu_semaphore_release(state->sem_stats);
}


/**
 * shoot_rays_rotated - convergence loop using the rotated-grid sampler.
 *
 * Mirrors shoot_rays() but calls rotated_grid_setup_ae() / rotated_grid_setup()
 * to orient each view along an arbitrary direction (state->azimuth_deg /
 * state->elevation_deg) and fires analyze_worker_rotated() workers.
 *
 * The view-0 direction is derived from the user az/el; views 1 and 2 are
 * orthogonal directions computed with bn_vec_perp() and VCROSS so that the
 * three views are mutually perpendicular (same algorithm as gqa's rotated
 * mode).
 */
static void
shoot_rays_rotated(struct current_state *state)
{
    struct rt_i *rtip = (struct rt_i *)state->rtip;
    int view;

    do {
	double inv_spacing = 1.0 / state->gridSpacing;
	VSCALE(state->steps, state->span, inv_spacing);

	/* Build the three rotated grids. */
	rotated_grid_setup_ae(&state->rot_grid[0],
			      rtip->mdl_min, rtip->mdl_max,
			      state->azimuth_deg, state->elevation_deg,
			      state->gridSpacing);

	if (state->num_views > 1) {
	    vect_t v1_dir;
	    bn_vec_perp(v1_dir, state->rot_grid[0].ray_dir);
	    VUNITIZE(v1_dir);
	    rotated_grid_setup(&state->rot_grid[1],
			      rtip->mdl_min, rtip->mdl_max,
			      v1_dir, state->gridSpacing);
	}

	if (state->num_views > 2) {
	    vect_t v2_dir;
	    VCROSS(v2_dir, state->rot_grid[0].ray_dir, state->rot_grid[1].ray_dir);
	    VUNITIZE(v2_dir);
	    rotated_grid_setup(&state->rot_grid[2],
			      rtip->mdl_min, rtip->mdl_max,
			      v2_dir, state->gridSpacing);
	}

	bu_log("Processing with rotated grid spacing %g mm\n", state->gridSpacing);

	for (view = 0; view < state->num_views; view++) {
	    if (state->verbose)
		bu_vls_printf(state->verbose_str, "  view %d\n", view);
	    state->curr_view = state->i_axis = view;
	    state->u_axis    = (view + 1) % 3;
	    state->v_axis    = (view + 2) % 3;
	    state->rot_grid[view].current     = 0;
	    state->rot_grid[view].refine_flag = state->grid->refine_flag;
	    /* The rotated grid's projection plane is NOT axis-aligned, so its
	     * effective area (u_count × v_count × spacing²) differs from the
	     * bbox-face area stored in state->area[] at startup.  Update
	     * area[view] to the actual grid projection area so that the volume
	     * formula m_len * (area / shots) yields the correct cell_area. */
	    state->area[view] = (double)state->rot_grid[view].total
			       * state->gridSpacing * state->gridSpacing;
	    state->current_cell_area = state->gridSpacing * state->gridSpacing;
	    bu_parallel(analyze_worker_rotated, state->ncpu, (void *)state);
	    if (state->aborted)
		return;
	}

	state->grid->refine_flag = 1;
	state->gridSpacing *= 0.5;

    } while (check_terminate(state));

    if (state->verbose)
	bu_vls_printf(state->verbose_str, "Computation Done\n");
}


/**
 * Do some computations prior to raytracing based upon options the
 * user has specified
 *
 * Returns:
 * 0 continue, ready to go
 * !0 error encountered, terminate processing
 */
static int
options_set(struct current_state *state)
{
    struct rt_i *rtip = state->rtip;
    double newGridSpacing = state->gridSpacing;
    int axis;

    /* figure out where the density values are coming from and get
     * them.
     */
    if (state->analysis_flags & ANALYSIS_MASS) {
	if (state->densityFileName) {
	    if(state->debug)
		bu_vls_printf(state->debug_str, "Density from file\n");
	    if (densities_from_file(state, state->densityFileName) != ANALYZE_OK) {
		bu_log("Couldn't load density table from file. Using default density.\n");
		state->default_den = 1;
	    }
	} else {
	    if(state->debug)
		bu_vls_printf(state->debug_str, "Density from db\n");
	    if (densities_from_database(state, rtip) != ANALYZE_OK) {
		bu_log("Couldn't load density table from database. Using default density.\n");
		state->default_den = 1;
	    }
	}
    }
    /* refine the grid spacing if the user has set a lower bound on
     * the number of rays per model axis
     */
    for (axis = 0; axis < 3; axis++) {
	if (state->span[axis] < newGridSpacing * state->samples_per_model_axis) {
	    /* along this axis, the gridSpacing is larger than the
	     * model span.  We need to refine.
	     */
	    newGridSpacing = state->span[axis] / state->samples_per_model_axis;
	}
    }

    if (!ZERO(newGridSpacing - state->gridSpacing)) {
	bu_log("Initial grid spacing %g mm does not allow %g samples per axis.\n", state->gridSpacing, state->samples_per_model_axis - 1);
	bu_log("Adjusted initial grid spacing to %g mm to get %g samples per model axis.\n", newGridSpacing, state->samples_per_model_axis);
	state->gridSpacing = newGridSpacing;
    }

    /* if the vol/mass/surf_area tolerances are not set, pick something */
    if (state->analysis_flags & ANALYSIS_VOLUME) {
	if (state->volume_tolerance < 0.0) {
	    /* using 1/1000th the volume as a default tolerance, no particular reason */
	    state->volume_tolerance = state->span[X] * state->span[Y] * state->span[Z] * 0.001;
	    bu_log("Using estimated volume tolerance %g cu mm\n", state->volume_tolerance);
	} else
	    bu_log("Using volume tolerance %g cu mm\n", state->volume_tolerance);
    }
    if(state->analysis_flags & ANALYSIS_SURF_AREA) {
	if(state->sa_tolerance < 0.0) {
	    state->sa_tolerance = INFINITY;
	    V_MIN(state->sa_tolerance, state->span[0] * state->span[1]);
	    V_MIN(state->sa_tolerance, state->span[1] * state->span[2]);
	    V_MIN(state->sa_tolerance, state->span[2] * state->span[0]);
	    state->sa_tolerance *= 0.01;
	    bu_log("Using estimated surface area tolerance %g mm^2\n", state->sa_tolerance);
	} else
	    bu_log("Using surface area tolerance %g mm^2\n", state->sa_tolerance);
    }
    if (state->analysis_flags & ANALYSIS_MASS) {
	if (state->mass_tolerance < 0.0) {
	    double max_den = 0.0;
	    long int curr_id = -1;
	    while ((curr_id = analyze_densities_next(state->densities, curr_id)) != -1) {
		if (analyze_densities_density(state->densities, curr_id) > max_den)
		    max_den = analyze_densities_density(state->densities, curr_id);
	    }
	    state->mass_tolerance = state->span[X] * state->span[Y] * state->span[Z] * 0.1 * max_den;
	    bu_log("Setting mass tolerance to %g gram\n", state->mass_tolerance);
	} else {
	    bu_log("mass tolerance   %g gram\n", state->mass_tolerance);
	}
    }

    if (state->analysis_flags & ANALYSIS_OVERLAPS) {
	if (!ZERO(state->overlap_tolerance))
	    bu_log("overlap tolerance to %g\n", state->overlap_tolerance);
	if (state->overlaps_callback == NULL) {
	    bu_log("\nOverlaps callback function not registered!");
	    return ANALYZE_ERROR;
	}
    }

    if ((state->analysis_flags & (ANALYSIS_ADJ_AIR|ANALYSIS_EXP_AIR|ANALYSIS_FIRST_AIR|ANALYSIS_LAST_AIR|ANALYSIS_UNCONF_AIR)) && ! state->use_air) {
	bu_log("\nError:  Air regions discarded but air analysis requested!\nSet use_air non-zero or eliminate air analysis\n");
	return ANALYZE_ERROR;
    }

    if (state->analysis_flags & ANALYSIS_EXP_AIR) {
	if (state->exp_air_callback == NULL) {
	    bu_log("\nexp_air callback function not registered!");
	    return ANALYZE_ERROR;
	}
    }

    if (state->analysis_flags & ANALYSIS_GAP) {
	if (state->gaps_callback == NULL) {
	    bu_log("\ngaps callback function not registered!");
	    return ANALYZE_ERROR;
	}
    }

    if (state->analysis_flags & ANALYSIS_ADJ_AIR) {
	if (state->adj_air_callback == NULL) {
	    bu_log("\nadj_air callback function not registered!");
	    return ANALYZE_ERROR;
	}
    }

    if (state->analysis_flags & ANALYSIS_FIRST_AIR) {
	if (state->first_air_callback == NULL) {
	    bu_log("\nfirst_air callback function not registered!");
	    return ANALYZE_ERROR;
	}
    }

    if (state->analysis_flags & ANALYSIS_LAST_AIR) {
	if (state->last_air_callback == NULL) {
	    bu_log("\nlast_air callback function not registered!");
	    return ANALYZE_ERROR;
	}
    }

    if (state->analysis_flags & ANALYSIS_UNCONF_AIR) {
	if (state->unconf_air_callback == NULL) {
	    bu_log("\nunconf_air callback function not registered!");
	    return ANALYZE_ERROR;
	}
    }
    return ANALYZE_OK;
}

static int
find_cmd_obj(struct current_state *state, struct per_obj_data *obj_rpt, const char *name)
{
    int i;
    char *str = bu_strdup(name);
    char *p;

    p=strchr(str, '/');
    if (p) {
	*p = '\0';
    }

    for (i = 0; i < state->num_objects; i++) {
	if (BU_STR_EQUAL(obj_rpt[i].o_name, str)) {
	    bu_free(str, "");
	    return i;
	}
    }

    if(state->debug)
	bu_vls_printf(state->debug_str, "%s Didn't find object named \"%s\" in %d entries\n", CPP_FILELINE, name, state->num_objects);

    return -1;
}

/**
 * Allocate data structures for tracking statistics on a per-view
 * basis for each of the view, object and region levels.
 */
static void
allocate_region_data(struct current_state *state, char *av[])
{
    struct region *regp;
    struct rt_i *rtip = state->rtip;
    int i;
    int index;
    if (state->num_objects < 1) {
	/* what?? */
	bu_log("WARNING: No objects remaining.\n");
	return;
    }

    if (state->num_views == 0) {
	/* crap. */
	bu_log("WARNING: No views specified.\n");
	return;
    }

    if (rtip->stats.nregions == 0) {
	/* dammit! */
	bu_log("WARNING: No regions remaining.\n");
	return;
    }

    state->m_lenDensity = (double *)bu_calloc(state->num_views, sizeof(double), "densityLen");
    state->m_len = (double *)bu_calloc(state->num_views, sizeof(double), "len");
    state->shots = (unsigned long *)bu_calloc(state->num_views, sizeof(unsigned long), "shots");
    state->m_lenTorque = (fastf_t *)bu_calloc(state->num_views, sizeof(vect_t), "lenTorque");
    state->m_moi = (fastf_t *)bu_calloc(state->num_views, sizeof(vect_t), "moments of inertia");
    state->m_poi = (fastf_t *)bu_calloc(state->num_views, sizeof(vect_t), "products of inertia");

    /* build data structures for the list of objects the user
     * specified on the command line
     */
    state->objs = (struct per_obj_data *)bu_calloc(state->num_objects, sizeof(struct per_obj_data), "report tables");
    for (i = 0; i < state->num_objects; i++) {
	state->objs[i].o_name = (char *)av[i];
	state->objs[i].o_len = (double *)bu_calloc(state->num_views, sizeof(double), "o_len");
	state->objs[i].o_lenDensity = (double *)bu_calloc(state->num_views, sizeof(double), "o_lenDensity");
	state->objs[i].o_volume = (double *)bu_calloc(state->num_views, sizeof(double), "o_volume");
	state->objs[i].o_mass = (double *)bu_calloc(state->num_views, sizeof(double), "o_mass");
	state->objs[i].o_area = (double *)bu_calloc(state->num_views, sizeof(double), "o_area");
	state->objs[i].o_surf_area = (double *)bu_calloc(state->num_views, sizeof(double), "o_surf_area");
	state->objs[i].o_lenTorque = (fastf_t *)bu_calloc(state->num_views, sizeof(vect_t), "lenTorque");
	state->objs[i].o_moi = (fastf_t *)bu_calloc(state->num_views, sizeof(vect_t), "moments of inertia");
	state->objs[i].o_poi = (fastf_t *)bu_calloc(state->num_views, sizeof(vect_t), "products of inertia");
    }

    /* build objects for each region */
    state->reg_tbl = (struct per_region_data *)bu_calloc(rtip->stats.nregions, sizeof(struct per_region_data), "per_region_data");


    for (i = 0, BU_LIST_FOR (regp, region, &(rtip->HeadRegion)), i++) {
	regp->reg_udata = &state->reg_tbl[i];
	state->reg_tbl[i].r_name = (char *)bu_malloc(strlen(regp->reg_name)+1, "r_name");
	bu_strlcpy(state->reg_tbl[i].r_name, regp->reg_name, strlen(regp->reg_name)+1);
	state->reg_tbl[i].r_lenDensity = (double *)bu_calloc(state->num_views, sizeof(double), "r_lenDensity");
	state->reg_tbl[i].r_len = (double *)bu_calloc(state->num_views, sizeof(double), "r_len");
	state->reg_tbl[i].r_volume = (double *)bu_calloc(state->num_views, sizeof(double), "len");
	state->reg_tbl[i].r_mass = (double *)bu_calloc(state->num_views, sizeof(double), "len");
	state->reg_tbl[i].r_area = (double *)bu_calloc(state->num_views, sizeof(double), "area");
	state->reg_tbl[i].r_surf_area = (double *)bu_calloc(state->num_views, sizeof(double), "surface area");

	index = find_cmd_obj(state, state->objs, &regp->reg_name[1]);
	if (index == -1) {
	    state->reg_tbl[i].optr = NULL;
	} else {
	    state->reg_tbl[i].optr = &state->objs[index];
	}
    }
    state->num_regions = i;
}


static int
analyze_single_grid_setup(struct current_state *state)
{
    mat_t toEye;
    vect_t temp;
    vect_t dx_unit;	 /* view delta-X as unit-len vect */
    vect_t dy_unit;	 /* view delta-Y as unit-len vect */
    mat_t view2model;
    double cell_width, cell_height;
    size_t width, height;

    state->curr_view = 0;
    state->i_axis = 0;
    state->u_axis = 1;
    state->v_axis = 2;
    state->grid->single_grid = 1;

    if (state->viewsize <= 0.0) {
	bu_log("viewsize <= 0");
	return ANALYZE_ERROR;
    }

    /* model2view takes us to eye_model location & orientation */
    MAT_IDN(toEye);
    MAT_DELTAS_VEC_NEG(toEye, state->eye_model);
    state->Viewrotscale[15] = 0.5*(state->viewsize);	/* Viewscale */
    bn_mat_mul(state->model2view, state->Viewrotscale, toEye);
    bn_mat_inv(view2model, state->model2view);

    state->area[0] = state->viewsize * state->viewsize;

    cell_width = state->gridSpacing;
    cell_height = cell_width / state->gridRatio;
    width = state->viewsize/cell_width + 0.99;
    height = state->viewsize/(cell_height * state->aspect) + 0.99;

    state->grid->grid_spacing = cell_width;
    state->grid->x_points = width;
    state->grid->total_points = width*height;
    bu_log("Processing with grid: (%g, %g) mm, (%zu, %zu) pixels\n", cell_width, cell_height, width, height);
    state->grid->current_point=0;

    /* Create basis vectors dx and dy for emanation plane (grid) */
    VSET(temp, 1, 0, 0);
    MAT3X3VEC(dx_unit, view2model, temp);	/* rotate only */
    VSCALE(state->grid->dx_grid, dx_unit, cell_width);

    VSET(temp, 0, 1, 0);
    MAT3X3VEC(dy_unit, view2model, temp);	/* rotate only */
    VSCALE(state->grid->dy_grid, dy_unit, cell_height);

    /* all rays go this direction */
    VSET(temp, 0, 0, -1);
    MAT4X3VEC(state->grid->ray_direction, view2model, temp);
    VUNITIZE(state->grid->ray_direction);

    VSET(temp, -1, -1/state->aspect, 0);	/* eye plane */
    MAT4X3PNT(state->grid->start_coord, view2model, temp);
    return ANALYZE_OK;
}


static void
analyze_triple_grid_setup(int view, struct current_state *state)
{
    struct rectangular_grid *grid = (struct rectangular_grid *)state->grid;
    grid->grid_spacing = state->gridSpacing;
    grid->current_point = 0;
    state->curr_view = view;
    state->i_axis = state->curr_view;
    state->u_axis = (state->curr_view+1) % 3;
    state->v_axis = (state->curr_view+2) % 3;

    state->u_dir[state->u_axis] = 1;
    state->u_dir[state->v_axis] = 0;
    state->u_dir[state->i_axis] = 0;

    state->v_dir[state->u_axis] = 0;
    state->v_dir[state->v_axis] = 1;
    state->v_dir[state->i_axis] = 0;

    grid->ray_direction[state->u_axis] = 0;
    grid->ray_direction[state->v_axis] = 0;
    grid->ray_direction[state->i_axis] = 1;

    VMOVE(grid->start_coord, state->rtip->mdl_min);
    grid->start_coord[state->u_axis] += grid->grid_spacing;
    grid->start_coord[state->v_axis] += grid->grid_spacing;

    VSCALE(grid->dx_grid, state->u_dir, grid->grid_spacing);
    VSCALE(grid->dy_grid, state->v_dir, grid->grid_spacing);

    grid->x_points = state->steps[state->u_axis] - 1;
    grid->total_points = grid->x_points * (state->steps[state->v_axis] - 1);
}


static int
analyze_setup_ae(struct current_state *state)
{
    vect_t temp;
    vect_t diag;
    mat_t toEye;
    mat_t view2model;
    fastf_t eye_backoff = (fastf_t)M_SQRT2;
    struct rt_i *rtip = (struct rt_i *) state->rtip;

    if (rtip == NULL) {
	bu_log("analyze_setup_ae: ERROR: rtip is null");
	return ANALYZE_ERROR;
    }

    if (rtip->stats.nsolids <= 0) {
	bu_log("ERROR: no primitives active\n");
	return ANALYZE_ERROR;
    }

    if (rtip->stats.nregions <= 0) {
	bu_log("ERROR: no regions active\n");
	return ANALYZE_ERROR;
    }

    if (rtip->mdl_max[X] >= INFINITY) {
	bu_log("analyze_setup_ae: infinite model bounds? setting a unit minimum\n");
	VSETALL(rtip->mdl_min, -1);
    }
    if (rtip->mdl_max[X] <= -INFINITY) {
	bu_log("analyze_setup_ae: infinite model bounds? setting a unit maximum\n");
	VSETALL(rtip->mdl_max, 1);
    }
    MAT_IDN(state->Viewrotscale);
    bn_mat_angles(state->Viewrotscale, 270.0 + state->elevation_deg, 0.0, 270.0 - state->azimuth_deg);

    /* Look at the center of the model */
    MAT_IDN(toEye);
    toEye[MDX] = -((rtip->mdl_max[X]+rtip->mdl_min[X])/2.0);
    toEye[MDY] = -((rtip->mdl_max[Y]+rtip->mdl_min[Y])/2.0);
    toEye[MDZ] = -((rtip->mdl_max[Z]+rtip->mdl_min[Z])/2.0);

    if ((state->viewsize) <= 0) {
	VSUB2(diag, rtip->mdl_max, rtip->mdl_min);
	state->viewsize = MAGNITUDE(diag);
	if (state->aspect > 1) {
	    /* don't clip any of the image when autoscaling */
	    state->viewsize *= state->aspect;
	}
    }

    /* sanity check: make sure viewsize still isn't zero in case
     * bounding box is empty, otherwise bn_mat_int() will bomb.
     */
    if (state->viewsize < 0 || ZERO(state->viewsize)) {
	state->viewsize = 2.0; /* arbitrary so Viewrotscale is normal */
    }

    state->Viewrotscale[15] = 0.5*(state->viewsize);	/* Viewscale */
    bn_mat_mul(state->model2view, state->Viewrotscale, toEye);
    bn_mat_inv(view2model, state->model2view);
    VSET(temp, 0, 0, eye_backoff);
    MAT4X3PNT(state->eye_model, view2model, temp);
    return ANALYZE_OK;
}


static void
shoot_rays(struct current_state *state)
{
    /* compute */
    double inv_spacing;
    do {
	inv_spacing = 1.0/state->gridSpacing;
	VSCALE(state->steps, state->span, inv_spacing);
	if (state->analysis_flags & ANALYSIS_SURF_AREA) {
	    int view;
	    for (view = 0; view < state->num_views; view++) {
		if (state->verbose)
		    bu_vls_printf(state->verbose_str, "  view %d\n", view);
		/* fire rays at random azimuth and elevation angles */
		state->azimuth_deg = RAND_ANGLE;
		state->elevation_deg = RAND_ANGLE;
		analyze_setup_ae(state);
		analyze_single_grid_setup(state);
		/* analyze_single_grid_setup() always writes the effective
		 * projection area (viewsize²) into area[0].  When view > 0,
		 * the volume formula later reads area[view], which would still
		 * hold the stale bbox-face value set at startup.  Copy area[0]
		 * to area[view] so every view uses the correct projection area. */
		state->area[view] = state->area[0];
		state->curr_view = view;
		state->current_cell_area = state->gridSpacing * state->gridSpacing;
		bu_parallel(analyze_worker, state->ncpu, (void *)state);
	    }
	} else if (state->use_single_grid) {
	    state->num_views = 1;
	    analyze_single_grid_setup(state);
	    state->current_cell_area = state->gridSpacing * state->gridSpacing;
	    bu_parallel(analyze_worker, state->ncpu, (void *)state);
	} else {
	    int view;
	    bu_log("Processing with grid spacing %g mm %ld x %ld x %ld\n",
		    state->gridSpacing,
		    state->steps[0]-1,
		    state->steps[1]-1,
		    state->steps[2]-1);
	    state->current_cell_area = state->gridSpacing * state->gridSpacing;
	    for (view = 0; view < state->num_views; view++) {
		if (state->verbose)
		    bu_vls_printf(state->verbose_str, "  view %d\n", view);
		analyze_triple_grid_setup(view, state);
		bu_parallel(analyze_worker, state->ncpu, (void *)state);
		if (state->aborted)
		    break;
	    }
	}
	state->grid->refine_flag = 1;
	state->gridSpacing *= 0.5;

    } while (check_terminate(state));
    if (state->verbose)
	bu_vls_printf(state->verbose_str, "Computation Done\n");
}


/**
 * shoot_rays_clustered - Focused overlap ray sampling over AABB clusters.
 *
 * Each cluster is a connected component of the region AABB-intersection graph
 * (computed by analyze_cluster_overlapping_pairs()).  For each cluster this
 * function:
 *
 *   1. Restricts rtip->mdl_min/max to the cluster's pre-computed pairwise
 *      intersection-AABB union (isect_union_min / isect_union_max).
 *   2. Computes a grid spacing appropriate for that volume (target: 50 cells
 *      across the shortest axis, floored at gridSpacingLimit).
 *   3. Runs one triple-grid pass of rays through the restricted volume.
 *   4. Restores rtip->mdl_min/max and all derived state before the next cluster.
 *
 * Grouping pairs into clusters before shooting has two benefits over the older
 * per-pair approach:
 *   - N mutually-overlapping regions form one cluster → one pass instead of
 *     O(N²) passes.
 *   - The ray grid for each cluster still covers only the candidates relevant
 *     to that cluster, keeping ray counts small for sparse models.
 *
 * The existing overlap callback (analyze_overlap) fires normally for any
 * geometric overlap found within each sub-pass.
 *
 * Timeout is checked between clusters so callers can bound total runtime.
 */
static void
shoot_rays_clustered(struct current_state *state, struct bu_ptbl *clusters)
{
    size_t k;
    size_t nclusters = BU_PTBL_LEN(clusters);

    /* Save the full-model state we temporarily replace per cluster. */
    point_t saved_mdl_min, saved_mdl_max;
    vect_t  saved_span;
    double  saved_area[3];
    double  saved_gridSpacing;

    VMOVE(saved_mdl_min, state->rtip->mdl_min);
    VMOVE(saved_mdl_max, state->rtip->mdl_max);
    VMOVE(saved_span, state->span);
    saved_area[0] = state->area[0];
    saved_area[1] = state->area[1];
    saved_area[2] = state->area[2];
    saved_gridSpacing = state->gridSpacing;

    state->grid->refine_flag = 1; /* indicate this is a focused pass */

    for (k = 0; k < nclusters; k++) {
	struct overlap_cluster *cl;
	vect_t isect_span;
	double min_span, cl_spacing, inv_spacing;
	int view;
	size_t nreg;

	if (state->aborted)
	    break;

	/* Respect wall-clock timeout between clusters. */
	if (state->timeout_ms > 0) {
	    long elapsed_ms = (long)((bu_gettime() - state->start_time_us) / 1000);
	    if (elapsed_ms >= state->timeout_ms) {
		bu_log("NOTE: Clustered overlap scan: timeout of %ld ms reached "
		       "after %zu of %zu clusters.\n",
		       state->timeout_ms, k, nclusters);
		break;
	    }
	}

	cl = (struct overlap_cluster *)BU_PTBL_GET(clusters, k);
	nreg = BU_PTBL_LEN(&cl->regions);

	/* Guard against degenerate intersection volumes. */
	VSUB2(isect_span, cl->isect_union_max, cl->isect_union_min);
	if (isect_span[X] <= 0.0 || isect_span[Y] <= 0.0 || isect_span[Z] <= 0.0)
	    continue;

	/* Restrict the sampling grid to this cluster's intersection volume. */
	VMOVE(state->rtip->mdl_min, cl->isect_union_min);
	VMOVE(state->rtip->mdl_max, cl->isect_union_max);
	VSUB2(state->span, cl->isect_union_max, cl->isect_union_min);
	state->area[0] = state->span[1] * state->span[2];
	state->area[1] = state->span[2] * state->span[0];
	state->area[2] = state->span[0] * state->span[1];

	/* Grid spacing: 50 cells across the shortest axis, >= gridSpacingLimit. */
	min_span = state->span[X];
	V_MIN(min_span, state->span[Y]);
	V_MIN(min_span, state->span[Z]);
	cl_spacing = min_span / 50.0;
	if (cl_spacing < state->gridSpacingLimit)
	    cl_spacing = state->gridSpacingLimit;

	state->gridSpacing = cl_spacing;
	inv_spacing = 1.0 / cl_spacing;
	VSCALE(state->steps, state->span, inv_spacing);

	bu_log("  Cluster %zu/%zu: %zu region(s), grid spacing %g mm, "
	       "%ld x %ld x %ld cells\n",
	       k + 1, nclusters, nreg, cl_spacing,
	       (long)state->steps[0],
	       (long)state->steps[1],
	       (long)state->steps[2]);

	/* Set cell area for overlap volume estimation. */
	state->current_cell_area = cl_spacing * cl_spacing;

	/* Run one triple-grid pass over this cluster's intersection volume. */
	for (view = 0; view < state->num_views; view++) {
	    if (state->verbose)
		bu_vls_printf(state->verbose_str, "    view %d\n", view);
	    analyze_triple_grid_setup(view, state);
	    bu_parallel(analyze_worker, state->ncpu, (void *)state);
	    if (state->aborted)
		break;
	}
    }

    /* Restore full-model state. */
    VMOVE(state->rtip->mdl_min, saved_mdl_min);
    VMOVE(state->rtip->mdl_max, saved_mdl_max);
    VMOVE(state->span, saved_span);
    state->area[0] = saved_area[0];
    state->area[1] = saved_area[1];
    state->area[2] = saved_area[2];
    state->gridSpacing = saved_gridSpacing;

    if (state->verbose)
	bu_vls_printf(state->verbose_str,
		      "Clustered overlap scan done (%zu cluster(s)).\n", nclusters);
}


/**
 * shoot_rays_rotated_clustered - cluster-focused overlap detection using the rotated-grid sampler.
 *
 * Mirrors shoot_rays_clustered() but fires rays through each cluster's
 * intersection volume using the rotated-grid sampler (ANALYZE_SAMPLER_ROTATED)
 * rather than the axis-aligned triple-grid.  The primary ray direction is
 * taken from state->azimuth_deg / state->elevation_deg; two orthogonal
 * companion views are derived with bn_vec_perp() and VCROSS.
 *
 * Using the rotated-grid sampler with cluster-focused sampling avoids the
 * axis-aligned ray artifact that causes touching / kissing surfaces to be
 * reported as overlaps by the triple-grid sampler.
 */
static void
shoot_rays_rotated_clustered(struct current_state *state, struct bu_ptbl *clusters)
{
    struct rt_i *rtip = (struct rt_i *)state->rtip;
    size_t k;
    size_t nclusters = BU_PTBL_LEN(clusters);

    /* Save the full-model state we temporarily replace per cluster. */
    point_t saved_mdl_min, saved_mdl_max;
    vect_t  saved_span;
    double  saved_area[3];
    double  saved_gridSpacing;

    VMOVE(saved_mdl_min, rtip->mdl_min);
    VMOVE(saved_mdl_max, rtip->mdl_max);
    VMOVE(saved_span, state->span);
    saved_area[0] = state->area[0];
    saved_area[1] = state->area[1];
    saved_area[2] = state->area[2];
    saved_gridSpacing = state->gridSpacing;

    state->grid->refine_flag = 1; /* indicate this is a focused pass */

    for (k = 0; k < nclusters; k++) {
	struct overlap_cluster *cl;
	vect_t isect_span;
	double min_span, cl_spacing;
	int view;
	size_t nreg;

	if (state->aborted)
	    break;

	/* Respect wall-clock timeout between clusters. */
	if (state->timeout_ms > 0) {
	    long elapsed_ms = (long)((bu_gettime() - state->start_time_us) / 1000);
	    if (elapsed_ms >= state->timeout_ms) {
		bu_log("NOTE: Clustered rotated-grid overlap scan: timeout of %ld ms reached "
		       "after %zu of %zu clusters.\n",
		       state->timeout_ms, k, nclusters);
		break;
	    }
	}

	cl = (struct overlap_cluster *)BU_PTBL_GET(clusters, k);
	nreg = BU_PTBL_LEN(&cl->regions);

	/* Guard against degenerate intersection volumes. */
	VSUB2(isect_span, cl->isect_union_max, cl->isect_union_min);
	if (isect_span[X] <= 0.0 || isect_span[Y] <= 0.0 || isect_span[Z] <= 0.0)
	    continue;

	/* Restrict the sampling grid to this cluster's intersection volume. */
	VMOVE(rtip->mdl_min, cl->isect_union_min);
	VMOVE(rtip->mdl_max, cl->isect_union_max);
	VSUB2(state->span, cl->isect_union_max, cl->isect_union_min);
	state->area[0] = state->span[1] * state->span[2];
	state->area[1] = state->span[2] * state->span[0];
	state->area[2] = state->span[0] * state->span[1];

	/* Grid spacing: 50 cells across the shortest axis, >= gridSpacingLimit. */
	min_span = state->span[X];
	V_MIN(min_span, state->span[Y]);
	V_MIN(min_span, state->span[Z]);
	cl_spacing = min_span / 50.0;
	if (cl_spacing < state->gridSpacingLimit)
	    cl_spacing = state->gridSpacingLimit;

	state->gridSpacing = cl_spacing;
	VSCALE(state->steps, state->span, 1.0 / cl_spacing);

	bu_log("  Cluster %zu/%zu: %zu region(s), rotated-grid spacing %g mm, "
	       "%ld x %ld x %ld cells\n",
	       k + 1, nclusters, nreg, cl_spacing,
	       (long)state->steps[0],
	       (long)state->steps[1],
	       (long)state->steps[2]);

	/* Set cell area for overlap volume estimation (spacing² per cell). */
	state->current_cell_area = cl_spacing * cl_spacing;

	/* Build three mutually perpendicular rotated grids for this cluster bbox. */
	rotated_grid_setup_ae(&state->rot_grid[0],
			      rtip->mdl_min, rtip->mdl_max,
			      state->azimuth_deg, state->elevation_deg,
			      cl_spacing);

	if (state->num_views > 1) {
	    vect_t v1_dir;
	    bn_vec_perp(v1_dir, state->rot_grid[0].ray_dir);
	    VUNITIZE(v1_dir);
	    rotated_grid_setup(&state->rot_grid[1],
			      rtip->mdl_min, rtip->mdl_max,
			      v1_dir, cl_spacing);
	}

	if (state->num_views > 2) {
	    vect_t v2_dir;
	    VCROSS(v2_dir, state->rot_grid[0].ray_dir, state->rot_grid[1].ray_dir);
	    VUNITIZE(v2_dir);
	    rotated_grid_setup(&state->rot_grid[2],
			      rtip->mdl_min, rtip->mdl_max,
			      v2_dir, cl_spacing);
	}

	/* Fire one rotated-grid pass per view for this cluster. */
	for (view = 0; view < state->num_views; view++) {
	    if (state->verbose)
		bu_vls_printf(state->verbose_str, "    view %d\n", view);
	    state->curr_view = state->i_axis = view;
	    state->u_axis    = (view + 1) % 3;
	    state->v_axis    = (view + 2) % 3;
	    state->rot_grid[view].current     = 0;
	    state->rot_grid[view].refine_flag = state->grid->refine_flag;
	    state->area[view] = (double)state->rot_grid[view].total
			       * cl_spacing * cl_spacing;
	    bu_parallel(analyze_worker_rotated, state->ncpu, (void *)state);
	    if (state->aborted)
		break;
	}
    }

    /* Restore full-model state. */
    VMOVE(rtip->mdl_min, saved_mdl_min);
    VMOVE(rtip->mdl_max, saved_mdl_max);
    VMOVE(state->span, saved_span);
    state->area[0] = saved_area[0];
    state->area[1] = saved_area[1];
    state->area[2] = saved_area[2];
    state->gridSpacing = saved_gridSpacing;

    if (state->verbose)
	bu_vls_printf(state->verbose_str,
		      "Clustered rotated-grid overlap scan done (%zu cluster(s)).\n", nclusters);
}


int
perform_raytracing(struct current_state *state, struct db_i *dbip, char *names[], int num_objects, int flags)
{
    int i;
    struct rt_i *rtip;

    struct region *regp;
    struct resource resp[MAX_PSW];
    struct rectangular_grid grid;
    struct region_pair overlapList;

    /* Per-cluster focused overlap sampling state (set in the prefilter block
     * below; used in place of shoot_rays() when cluster-focused mode is active). */
    struct bu_ptbl *pairwise_candidates_p = NULL;
    int use_pairwise = 0;

    /* local copy for overlaps list to check later for hits */
    BU_LIST_INIT(&overlapList.l);
    state->overlapList = &overlapList;

    state->grid = &grid;
    grid.single_grid = 0;

    state->analysis_flags = flags;

    /* Get raytracing instance */
    rtip = rt_i_create(dbip);
    rtip->useair = state->use_air;

    memset(resp, 0, sizeof(resp));
    for(i = 0; i < MAX_PSW; i++) {
	rt_init_resource(&resp[i], i, rtip);
    }

    state->rtip = rtip;
    state->resp = resp;

    /* Prep for raytracing */
    state->num_objects = num_objects;
    for(i = 0; i < state->num_objects; i++) {
	if(rt_gettree(rtip, names[i]) < 0) {
	    bu_log("Loading geometry for [%s] FAILED", names[i]);
	    rt_i_destroy(rtip);
	    rtip = NULL;
	    return ANALYZE_ERROR;
	}
    }

    rt_prep_parallel(rtip, state->ncpu);

    /* Record start time for timeout enforcement in check_terminate() */
    state->start_time_us = bu_gettime();

    /* --- AABB overlap pre-filter with cluster-based focused sampling ---
     *
     * When the caller only wants overlap detection (no volume / mass /
     * surface-area / gap / air checks), we avoid shooting rays over the
     * entire model bounding box.  Instead:
     *
     *   1. Find all AABB-intersecting region pairs (R-Tree query).
     *   2. Cluster the pairs into connected components of the AABB-
     *      intersection graph (transitive-closure BFS, same algorithm
     *      as gdiff's cluster_content()).  Dense sub-regions produce
     *      large but few clusters; sparse models produce many small ones.
     *   3. For each cluster, shoot a focused grid restricted to the union
     *      of the cluster's pairwise intersection AABBs.
     *
     * This is strictly better than either the per-pair approach (which
     * shoots O(pairs) passes) or the union-all approach (which shoots one
     * large grid).  For N mutually-overlapping regions we get one pass
     * instead of O(N²).
     *
     * Fall-back: when the number of clusters exceeds OV_CLUSTER_MAX (a
     * very dense model), restrict rtip->mdl_min/max to the union of all
     * cluster intersection volumes and run the normal shoot_rays() loop.
     */
    if ((flags & ANALYSIS_OVERLAPS) &&
	!(flags & (ANALYSIS_VOLUME | ANALYSIS_MASS | ANALYSIS_SURF_AREA |
		   ANALYSIS_GAP | ANALYSIS_EXP_AIR | ANALYSIS_ADJ_AIR |
		   ANALYSIS_FIRST_AIR | ANALYSIS_LAST_AIR | ANALYSIS_UNCONF_AIR))) {

	struct bu_ptbl raw_pairs;
	int n_pairs = analyze_overlapping_region_pairs(rtip, &raw_pairs);

	if (n_pairs > 0) {
	    /* Cluster the raw pairs into connected components */
	    pairwise_candidates_p =
		(struct bu_ptbl *)bu_malloc(sizeof(struct bu_ptbl), "overlap clusters");
	    int n_clusters = analyze_cluster_overlapping_pairs(&raw_pairs, pairwise_candidates_p);
	    analyze_free_region_overlap_pairs(&raw_pairs);

	    if (n_clusters > 0 && n_clusters <= OV_CLUSTER_MAX) {
		/* Per-cluster focused sampling path */
		bu_log("Overlap prefilter: %d candidate pair(s) → %d cluster(s) — "
		       "using cluster-focused sampling.\n",
		       n_pairs, n_clusters);
		use_pairwise = 1;

	    } else if (n_clusters > OV_CLUSTER_MAX) {
		/* Dense-cluster fallback: union-bbox strategy */
		size_t k;
		point_t union_min, union_max;
		VSETALL(union_min,  INFINITY);
		VSETALL(union_max, -INFINITY);

		for (k = 0; k < BU_PTBL_LEN(pairwise_candidates_p); k++) {
		    struct overlap_cluster *cl =
			(struct overlap_cluster *)BU_PTBL_GET(pairwise_candidates_p, k);
		    VMIN(union_min, cl->isect_union_min);
		    VMAX(union_max, cl->isect_union_max);
		}
		analyze_free_overlap_clusters(pairwise_candidates_p);
		bu_free(pairwise_candidates_p, "overlap clusters");
		pairwise_candidates_p = NULL;

		{
		    vect_t full_span, cand_span;
		    double full_vol, cand_vol;
		    VSUB2(full_span, rtip->mdl_max, rtip->mdl_min);
		    VSUB2(cand_span, union_max, union_min);
		    full_vol = full_span[X] * full_span[Y] * full_span[Z];
		    cand_vol = cand_span[X] * cand_span[Y] * cand_span[Z];

		    if (full_vol > 0.0 && cand_vol < full_vol * OV_PREFILTER_MIN_REDUCTION) {
			bu_log("Overlap prefilter: %d cluster(s) (> %d threshold — "
			       "very dense model), restricting grid to "
			       "%.1f%% of model volume.\n",
			       n_clusters, OV_CLUSTER_MAX,
			       100.0 * cand_vol / full_vol);
			VMOVE(rtip->mdl_min, union_min);
			VMOVE(rtip->mdl_max, union_max);
		    } else {
			bu_log("Overlap prefilter: %d cluster(s) (> %d threshold — "
			       "very dense model, union bbox not substantially "
			       "smaller; using full grid).\n",
			       n_clusters, OV_CLUSTER_MAX);
		    }
		}

	    } else {
		/* n_clusters <= 0: clustering failed; fall through to full grid */
		bu_log("Overlap prefilter: clustering failed, using full model bbox.\n");
		if (pairwise_candidates_p) {
		    analyze_free_overlap_clusters(pairwise_candidates_p);
		    bu_free(pairwise_candidates_p, "overlap clusters");
		    pairwise_candidates_p = NULL;
		}
	    }

	} else if (n_pairs == 0) {
	    bu_log("Overlap prefilter: no AABB-intersecting region pairs — "
		   "geometric overlaps are not possible.\n");
	    bu_ptbl_free(&raw_pairs);
	} else {
	    /* n_pairs < 0: AABB query error; proceed with full bbox */
	    bu_log("Overlap prefilter: AABB query failed, using full model bbox.\n");
	}
    }

    /* setup azimuth and elevation angles in case of single grid */
    if (state->use_single_grid && !state->use_view_information) {
	if (analyze_setup_ae(state)) {
	    if (pairwise_candidates_p) {
		analyze_free_overlap_clusters(pairwise_candidates_p);
		bu_free(pairwise_candidates_p, "overlap clusters");
	    }
	    rt_i_destroy(rtip);
	    rtip = NULL;
	    return ANALYZE_ERROR;
	}
    }

    /* set the grid spacing from viewsize if the width and height are mentioned */
    if (state->grid_size_flag && state->use_single_grid) {
	double cell_height = 0.0;
	double cell_width = 0.0;
	cell_width = state->viewsize / state->grid_width;
	cell_height = state->viewsize / (state->grid_height * state->aspect);
	state->gridSpacing = cell_width;
	state->gridSpacingLimit = cell_width;
	state->gridRatio = cell_width/cell_height;
    }

    /* we now have to subdivide space */
    VSUB2(state->span, rtip->mdl_max, rtip->mdl_min);
    state->area[0] = state->span[1] * state->span[2];
    state->area[1] = state->span[2] * state->span[0];
    state->area[2] = state->span[0] * state->span[1];

    if (state->analysis_flags & ANALYSIS_BOX) {
	bu_log("Bounding Box: %g %g %g  %g %g %g\n",
		V3ARGS(rtip->mdl_min), V3ARGS(rtip->mdl_max));

	bu_log("Area: (%g, %g, %g)\n", state->area[X], state->area[Y], state->area[Z]);
    }
    if (state->verbose) bu_vls_printf(state->verbose_str, "ncpu: %d\n", state->ncpu);

    /* if the user did not specify the initial grid spacing limit, we
     * need to compute a reasonable one for them.
     */
    if (ZERO(state->gridSpacing)) {
	double min_span = MAX_FASTF;
	VPRINT("span", state->span);

	V_MIN(min_span, state->span[X]);
	V_MIN(min_span, state->span[Y]);
	V_MIN(min_span, state->span[Z]);

	state->gridSpacing = state->gridSpacingLimit;
	do {
	    state->gridSpacing *= 2.0;
	} while (state->gridSpacing < min_span);

	state->gridSpacing *= 0.25;
	if (state->gridSpacing < state->gridSpacingLimit) state->gridSpacing = state->gridSpacingLimit;

	bu_log("Trying estimated initial grid spacing: %g mm\n", state->gridSpacing);
    } else {
	bu_log("Trying initial grid spacing: %g mm\n", state->gridSpacing);
    }

    bu_log("Using grid spacing lower limit: %g mm\n", state->gridSpacingLimit);

    if (options_set(state) != ANALYZE_OK) {
	bu_log("Couldn't set up the options correctly!\n");
	if (pairwise_candidates_p) {
	    analyze_free_overlap_clusters(pairwise_candidates_p);
	    bu_free(pairwise_candidates_p, "overlap clusters");
	}
	return ANALYZE_ERROR;
    }

    /* initialize some stuff */
    state->sem_worker = bu_semaphore_register("analyze_sem_worker");
    state->sem_stats = bu_semaphore_register("analyze_sem_stats");
    state->sem_plot = bu_semaphore_register("analyze_sem_plot");
    allocate_region_data(state, names);
    grid.refine_flag = 0;

    /* Cluster-focused sampling replaces the normal shoot_rays() loop for
     * overlap-only analyses.  shoot_rays_clustered() processes each
     * connected component of the AABB-intersection graph in isolation,
     * restricting rays to just that cluster's intersection-volume union.
     * For all other analyses (or when the cluster count exceeded
     * OV_CLUSTER_MAX), the normal refinement loop in shoot_rays() is used.
     *
     * The Crofton sampler bypasses both the cluster and grid paths: it fires
     * a fixed number of isotropic random rays in a single parallel pass.
     */
    if (state->sampler == ANALYZE_SAMPLER_CROFTON) {
	/* Default ray count if the caller left crofton_n_rays at zero. */
	size_t n_crofton = (state->crofton_n_rays > 0)
	    ? state->crofton_n_rays
	    : (size_t)100000;

	state->sem_crofton = bu_semaphore_register("analyze_sem_crofton");

	if (pairwise_candidates_p) {
	    analyze_free_overlap_clusters(pairwise_candidates_p);
	    bu_free(pairwise_candidates_p, "overlap clusters");
	    pairwise_candidates_p = NULL;
	}

	bu_log("Crofton: firing %zu isotropic random rays.\n", n_crofton);
	if (state->verbose)
	    bu_vls_printf(state->verbose_str,
			 "Crofton: firing %zu isotropic random rays.\n", n_crofton);
	shoot_rays_crofton(state, n_crofton);

	/* Fill in per-object mass/volume so the getter functions that read
	 * o_mass[view] / o_volume[view] return correct values.  (These are
	 * normally filled by mass_volume_surf_area_terminate_check() inside
	 * check_terminate() which the Crofton path skips.) */
	if (state->analysis_flags & (ANALYSIS_MASS | ANALYSIS_VOLUME)) {
	    int obj, view;
	    for (obj = 0; obj < state->num_objects; obj++) {
		for (view = 0; view < state->num_views; view++) {
		    double cell_area = (state->shots[view] > 0)
			? state->area[view] / (double)state->shots[view]
			: 0.0;
		    state->objs[obj].o_volume[view] =
			state->objs[obj].o_len[view] * cell_area;
		    state->objs[obj].o_mass[view] =
			state->objs[obj].o_lenDensity[view] * cell_area;
		}
	    }
	}
    } else if (state->sampler == ANALYZE_SAMPLER_ROTATED) {
	/* Rotated-grid sampler: fires rays along a user-specified az/el
	 * direction with two orthogonal companions.  When cluster-focused
	 * sampling is active (use_pairwise), use shoot_rays_rotated_clustered()
	 * to restrict each pass to the cluster's intersection volume — the same
	 * optimization applied to the triple-grid sampler.  Otherwise fall back
	 * to the whole-model shoot_rays_rotated() convergence loop. */
	if (use_pairwise && pairwise_candidates_p) {
	    shoot_rays_rotated_clustered(state, pairwise_candidates_p);
	    analyze_free_overlap_clusters(pairwise_candidates_p);
	    bu_free(pairwise_candidates_p, "overlap clusters");
	    pairwise_candidates_p = NULL;
	} else {
	    if (pairwise_candidates_p) {
		analyze_free_overlap_clusters(pairwise_candidates_p);
		bu_free(pairwise_candidates_p, "overlap clusters");
		pairwise_candidates_p = NULL;
	    }
	    shoot_rays_rotated(state);
	}
    } else if (use_pairwise && pairwise_candidates_p) {
	shoot_rays_clustered(state, pairwise_candidates_p);
	analyze_free_overlap_clusters(pairwise_candidates_p);
	bu_free(pairwise_candidates_p, "overlap clusters");
	pairwise_candidates_p = NULL;
    } else {
	shoot_rays(state);
    }

    /* print any logs in main thread */
    bu_log("%s", bu_vls_strgrab(state->log_str));

    /* check for any non-hits */
    for (BU_LIST_FOR (regp, region, &(state->rtip->HeadRegion))) {
	size_t hits;
	struct region_pair *rp;
	int is_overlap_only_hit;

	RT_CK_REGION(regp);
	hits = (size_t)((struct per_region_data *)regp->reg_udata)->hits;
	if (hits < state->required_number_hits) {
	    if (hits == 0 && !state->quiet_missed_report) {
		is_overlap_only_hit = 0;
		if (state->analysis_flags & ANALYSIS_OVERLAPS) {
		    /* If the region is in the overlap list, it has
		     * been hit even though the hit count is zero.
		     * Do not report zero hit regions if they are in
		     * the overlap list.
		     */
		    for (BU_LIST_FOR (rp, region_pair, &(overlapList.l))) {
			if (rp->r.r1->reg_name == regp->reg_name) {
			    is_overlap_only_hit = 1;
			    break;
			} else if (rp->r2) {
			    if (rp->r2->reg_name == regp->reg_name) {
				is_overlap_only_hit = 1;
				break;
			    }
			}
		    }
		}
		if (!is_overlap_only_hit) {
		    bu_log("%s was not hit\n", regp->reg_name);
		}
	    } else if (hits) {
		bu_log("%s hit only %zu times (< %zu)\n", regp->reg_name, hits, state->required_number_hits);
	    }
	}
    }

    /* Free dynamically allocated memory */
    bu_vls_free(state->log_str);
    bu_list_free(&overlapList.l);

    if (state->densities != NULL) {
	analyze_densities_destroy(state->densities);
	state->densities = NULL;
    }

    rt_i_destroy(rtip);
    return ANALYZE_OK;
}


int
analyze_bbox(struct db_i *dbip, char *names[], int num_objects,
	     point_t bbox_min, point_t bbox_max)
{
    int i;
    struct rt_i *rtip;

    if (!dbip || !names || num_objects <= 0 || !bbox_min || !bbox_max)
	return -1;

    rtip = rt_i_create(dbip);
    if (!rtip)
	return -1;

    for (i = 0; i < num_objects; i++) {
	if (rt_gettree(rtip, names[i]) < 0) {
	    bu_log("analyze_bbox: failed to load '%s'\n", names[i]);
	    rt_i_destroy(rtip);
	    return -1;
	}
    }

    /* Prepare the space partition (needed to compute mdl_min/mdl_max).
     * Use a single CPU; we are not shooting rays. */
    rt_prep_parallel(rtip, 1);

    VMOVE(bbox_min, rtip->mdl_min);
    VMOVE(bbox_max, rtip->mdl_max);

    rt_i_destroy(rtip);
    return 0;
}


struct analyze_results *
analyze_run(const struct analyze_config *cfg, struct db_i *dbip,
	    char *names[], int num_names, int flags)
{
    if (!dbip || !names || num_names <= 0)
	return NULL;
    return analyze_engine_run(cfg, dbip, names, num_names, flags);
}



void
analyze_results_free(struct analyze_results *res)
{
    size_t i;

    if (!res)
	return;

    if (res->objects) {
	/* Object names in this array are strdup'd by analyze_run(). */
	for (i = 0; i < res->n_objects; i++) {
	    if (res->objects[i].name)
		bu_free((void *)res->objects[i].name, "object name");
	}
	bu_free(res->objects, "analyze_results objects");
	res->objects = NULL;
    }

    if (res->regions) {
	/* Region names in this array are strdup'd by analyze_run(). */
	for (i = 0; i < res->n_regions; i++) {
	    if (res->regions[i].name)
		bu_free((void *)res->regions[i].name, "region name");
	}
	bu_free(res->regions, "analyze_results regions");
	res->regions = NULL;
    }

    /* Free overlap / issue record lists.  region1 and region2 strings are
     * always strdup'd by the capture callbacks in analyze_run(). */
#define AR_FREE_PTBL(tbl) do { \
    for (i = 0; i < BU_PTBL_LEN(&res->tbl); i++) { \
	struct analyze_overlap_record *_r = \
	    (struct analyze_overlap_record *)BU_PTBL_GET(&res->tbl, i); \
	bu_free((void *)_r->region1, "ar region1"); \
	if (_r->region2) bu_free((void *)_r->region2, "ar region2"); \
	bu_free(_r, "analyze_overlap_record"); \
    } \
    bu_ptbl_free(&res->tbl); \
} while (0)

    AR_FREE_PTBL(overlaps);
    AR_FREE_PTBL(gaps);
    AR_FREE_PTBL(adj_air);
    AR_FREE_PTBL(exp_air);
    AR_FREE_PTBL(first_air);
    AR_FREE_PTBL(last_air);
    AR_FREE_PTBL(unconf_air);
#undef AR_FREE_PTBL

    bu_free(res, "analyze_results");
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
