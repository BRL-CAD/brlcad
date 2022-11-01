/*                           A P I . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "raytrace.h"
#include "vmath.h"
#include "bu/parallel.h"

#include "analyze.h"
#include "./analyze_private.h"

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
HIDDEN int
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

	    {
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

	/* compute the volume of the object */
	if (state->analysis_flags & ANALYSIS_VOLUME) {
	    struct per_region_data *prd = ((struct per_region_data *)pp->pt_regionp->reg_udata);
	    ap->A_LEN += dist; /* add to total volume */
	    {
		bu_semaphore_acquire(state->sem_worker);

		/* add to region volume */
		prd->r_len[state->curr_view] += dist;

		/* add to object volume */
		prd->optr->o_len[state->curr_view] += dist;

		bu_semaphore_release(state->sem_worker);
	    }
	    if (state->debug) {
		bu_semaphore_acquire(BU_SEM_GENERAL);
		bu_vls_printf(state->debug_str, "\t\tvol hit %s oDist:%g objVol:%g %s\n",
			      pp->pt_regionp->reg_name, dist, prd->optr->o_len[state->curr_view], prd->optr->o_name);
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
		bu_semaphore_acquire(state->sem_plot);
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
HIDDEN int
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
HIDDEN int
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
HIDDEN int
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
HIDDEN int
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

    if (rt_db_get_internal(&intern, dp, rtip->rti_dbip, NULL, &rt_uniresource) < 0) {
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
HIDDEN int
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
		/* divide by 4 to prepare for next iteration of grid refinment */
		state->objs[obj].o_area[view] /= 4;
		V_MIN(low, val);
		V_MAX(hi, val);
		tmp += val;
	    }
	    delta = hi - low;

	    if (state->verbose)
		bu_vls_printf(state->verbose_str, "\t%s running avg surface area %g mm^2 hi=(%g) low=(%g)\n", state->objs[obj].o_name, (tmp / state->num_views), hi, low);

	    if (delta > state->sa_tolerance) {
		/* this object differs too much in each view, so we
		 * need to refine the grid.
		 */
		can_terminate = 0;
		if (state->verbose)
		    bu_vls_printf(state->verbose_str, "\tsurface area tol not met on %s.  Refine grid\n", state->objs[obj].o_name);
		break;
	    }
	}

	/* for each region, compute the surface area for all views */
	for (i = 0, BU_LIST_FOR (regp, region, &(state->rtip->HeadRegion)), i++) {
	    int view;
	    for (view = 0; view < state->num_views; view++) {
		state->reg_tbl[i].r_surf_area[view] = state->reg_tbl[i].r_area[view];
		/* divide by 4 to prepare for next iteration of grid refinment */
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
HIDDEN int
check_terminate(struct current_state *state)
{
    int wv_status;
    int view, obj;

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
HIDDEN void
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
 * Do some computations prior to raytracing based upon options the
 * user has specified
 *
 * Returns:
 * 0 continue, ready to go
 * !0 error encountered, terminate processing
 */
HIDDEN int
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

HIDDEN int
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
HIDDEN void
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

    if (rtip->nregions == 0) {
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
    state->reg_tbl = (struct per_region_data *)bu_calloc(rtip->nregions, sizeof(struct per_region_data), "per_region_data");


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


HIDDEN int
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


HIDDEN void
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


HIDDEN int
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

    if (rtip->nsolids <= 0) {
	bu_log("ERROR: no primitives active\n");
	return ANALYZE_ERROR;
    }

    if (rtip->nregions <= 0) {
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


HIDDEN void
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
		state->curr_view = view;
		bu_parallel(analyze_worker, state->ncpu, (void *)state);
	    }
	} else if (state->use_single_grid) {
	    state->num_views = 1;
	    analyze_single_grid_setup(state);
	    bu_parallel(analyze_worker, state->ncpu, (void *)state);
	} else {
	    int view;
	    bu_log("Processing with grid spacing %g mm %ld x %ld x %ld\n",
		    state->gridSpacing,
		    state->steps[0]-1,
		    state->steps[1]-1,
		    state->steps[2]-1);
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


int
perform_raytracing(struct current_state *state, struct db_i *dbip, char *names[], int num_objects, int flags)
{
    int i;
    struct rt_i *rtip;

    struct region *regp;
    struct resource resp[MAX_PSW];
    struct rectangular_grid grid;
    struct region_pair overlapList;

    /* local copy for overlaps list to check later for hits */
    BU_LIST_INIT(&overlapList.l);
    state->overlapList = &overlapList;

    state->grid = &grid;
    grid.single_grid = 0;

    state->analysis_flags = flags;

    /* Get raytracing instance */
    rtip = rt_new_rti(dbip);
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
	    rt_free_rti(rtip);
	    rtip = NULL;
	    return ANALYZE_ERROR;
	}
    }

    rt_prep_parallel(rtip, state->ncpu);

    /* setup azimuth and elevation angles in case of single grid */
    if (state->use_single_grid && !state->use_view_information) {
	if (analyze_setup_ae(state)) {
	    rt_free_rti(rtip);
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
	return ANALYZE_ERROR;
    }

    /* initialize some stuff */
    state->sem_worker = bu_semaphore_register("analyze_sem_worker");
    state->sem_stats = bu_semaphore_register("analyze_sem_stats");
    state->sem_plot = bu_semaphore_register("analyze_sem_plot");
    allocate_region_data(state, names);
    grid.refine_flag = 0;
    shoot_rays(state);

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
	bu_free(state->densities, "densities");
	state->densities = NULL;
    }

    rt_free_rti(rtip);
    return ANALYZE_OK;
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
