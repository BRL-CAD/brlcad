/*                           A P I . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2018 United States Government as represented by
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
#define ANALYSIS_WEIGHT 8
#define ANALYSIS_GAP 16
#define ANALYSIS_EXP_AIR 32 /* exposed air */
#define ANALYSIS_BOX 64
#define ANALYSIS_INTERFACES 128
#define ANALYSIS_ADJ_AIR 256
#define ANALYSIS_OVERLAPS 512
#define ANALYSIS_MOMENTS 1024
#define ANALYSIS_PLOT_OVERLAPS 2048

static int debug = 0;
static int default_den = 0;
static int analysis_flags;
static double Samples_per_model_axis = 2.0;
static int aborted = 0;
static int verbose = 0;


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
    int air_first = 1; /* are we in an air before a solid */
    double val;
    double last_out_dist = -1.0;
    double gap_dist;
    struct current_state *state = (struct current_state *)ap->A_STATE;

    if (!segs) /* unexpected */
	return 0;

    if (PartHeadp->pt_forw == PartHeadp) return 1;


    /* examine each partition until we get back to the head */
    for (pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	struct density_entry *de;

	/* inhit info */
	dist = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);

	if (debug) {
	    bu_semaphore_acquire(ANALYZE_SEM_WORKER);
	    bu_log("%s %g->%g\n", pp->pt_regionp->reg_name,
		    pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist);
	    bu_semaphore_release(ANALYZE_SEM_WORKER);
	}

	if (analysis_flags & ANALYSIS_EXP_AIR) {

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

	/* computing the weight of the objects */
	if (analysis_flags & ANALYSIS_WEIGHT) {
	    if (debug) {
		bu_semaphore_acquire(ANALYZE_SEM_WORKER);
		bu_log("Hit %s doing weight\n", pp->pt_regionp->reg_name);
		bu_semaphore_release(ANALYZE_SEM_WORKER);
	    }

	    /* make sure mater index is within range of densities */
	    if (pp->pt_regionp->reg_gmater >= state->num_densities) {
		if(default_den == 0) {
		    bu_semaphore_acquire(ANALYZE_SEM_WORKER);
		    bu_log("Density index %d on region %s is outside of range of table [1..%d]\nSet GIFTmater on region or add entry to density table\n",
			    pp->pt_regionp->reg_gmater,
			    pp->pt_regionp->reg_name,
			    state->num_densities); /* XXX this should do something else */
		    bu_semaphore_release(ANALYZE_SEM_WORKER);
		    return ANALYZE_ERROR;
		}
	    }

	    /* make sure the density index has been set */
	    bu_semaphore_acquire(ANALYZE_SEM_WORKER);
	    if(default_den || state->densities[pp->pt_regionp->reg_gmater].magic != DENSITY_MAGIC) {
		BU_GET(de, struct density_entry);		/* Aluminium 7xxx series as default material */
		de->magic = DENSITY_MAGIC;
		de->grams_per_cu_mm = 2.74;
		de->name = "Aluminium, 7079-T6";
	    } else {
		de = state->densities + pp->pt_regionp->reg_gmater;
	    }
	    bu_semaphore_release(ANALYZE_SEM_WORKER);
	    if (de->magic == DENSITY_MAGIC) {
		struct per_region_data *prd;
		vect_t cmass;
		vect_t lenTorque;
		int los;

		/* factor in the density of this object weight
		 * computation, factoring in the LOS percentage
		 * material of the object
		 */
		los = pp->pt_regionp->reg_los;

		if (los < 1) {
		    bu_semaphore_acquire(ANALYZE_SEM_WORKER);
		    bu_log("bad LOS (%d) on %s\n", los, pp->pt_regionp->reg_name);
		    bu_semaphore_release(ANALYZE_SEM_WORKER);
		}

		/* accumulate the total weight values */
		val = de->grams_per_cu_mm * dist * (pp->pt_regionp->reg_los * 0.01);
		ap->A_LENDEN += val;

		prd = ((struct per_region_data *)pp->pt_regionp->reg_udata);
		/* accumulate the per-region per-view weight values */
		bu_semaphore_acquire(ANALYZE_SEM_STATS);
		prd->r_lenDensity[state->i_axis] += val;

		/* accumulate the per-object per-view weight values */
		prd->optr->o_lenDensity[state->i_axis] += val;

		if (analysis_flags & ANALYSIS_CENTROIDS) {
		    /* calculate the center of mass for this partition */
		    VJOIN1(cmass, pt, dist*0.5, ap->a_ray.r_dir);

		    /* calculate the lenTorque for this partition (i.e. centerOfMass * lenDensity) */
		    VSCALE(lenTorque, cmass, val);

		    /* accumulate per-object per-view torque values */
		    VADD2(&prd->optr->o_lenTorque[state->i_axis*3], &prd->optr->o_lenTorque[state->i_axis*3], lenTorque);

		    /* accumulate the total lenTorque */
		    VADD2(&state->m_lenTorque[state->i_axis*3], &state->m_lenTorque[state->i_axis*3], lenTorque);

		}
		bu_semaphore_release(ANALYZE_SEM_STATS);

	    } else {
		bu_semaphore_acquire(ANALYZE_SEM_WORKER);
		bu_log("Density index %d from region %s is not set.\nAdd entry to density table\n",
			pp->pt_regionp->reg_gmater, pp->pt_regionp->reg_name);
		bu_semaphore_release(ANALYZE_SEM_WORKER);

		aborted = 1;
		return ANALYZE_ERROR;
	    }
	}

	/* compute the surface area of the object */
	if(analysis_flags & ANALYSIS_SURF_AREA) {
	    struct per_region_data *prd = ((struct per_region_data *)pp->pt_regionp->reg_udata);
	    fastf_t Lx = state->span[0] / state->steps[0];
	    fastf_t Ly = state->span[1] / state->steps[1];
	    fastf_t Lz = state->span[2] / state->steps[2];
	    fastf_t cell_area;
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
		bu_semaphore_acquire(ANALYZE_SEM_STATS);

		/* add to region surface area */
		prd->r_surf_area[state->curr_view] += (cell_area * 2);

		/* add to object surface area */
		prd->optr->o_surf_area[state->curr_view] += (cell_area * 2);

		bu_semaphore_release(ANALYZE_SEM_STATS);
	    }
	}

	/* compute the volume of the object */
	if (analysis_flags & ANALYSIS_VOLUME) {
	    struct per_region_data *prd = ((struct per_region_data *)pp->pt_regionp->reg_udata);
	    ap->A_LEN += dist; /* add to total volume */
	    {
		bu_semaphore_acquire(ANALYZE_SEM_STATS);

		/* add to region volume */
		prd->r_len[state->curr_view] += dist;

		/* add to object volume */
		prd->optr->o_len[state->curr_view] += dist;

		bu_semaphore_release(ANALYZE_SEM_STATS);
	    }
	    if (debug) {
		bu_semaphore_acquire(ANALYZE_SEM_WORKER);
		bu_log("\t\tvol hit %s oDist:%g objVol:%g %s\n",
			pp->pt_regionp->reg_name, dist, prd->optr->o_len[state->curr_view], prd->optr->o_name);
		bu_semaphore_release(ANALYZE_SEM_WORKER);
	    }
	    if (state->plot_volume) {
		VJOIN1(opt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

		bu_semaphore_acquire(BU_SEM_SYSCALL);
		if (ap->a_user & 1) {
		    pl_color(state->plot_volume, 128, 255, 192);  /* pale green */
		} else {
		    pl_color(state->plot_volume, 128, 192, 255);  /* cyan */
		}

		pdv_3line(state->plot_volume, pt, opt);
		bu_semaphore_release(BU_SEM_SYSCALL);
	    }
	}

	/* note that this region has been seen */
	((struct per_region_data *)pp->pt_regionp->reg_udata)->hits++;
	last_out_dist = pp->pt_outhit->hit_dist;
	VJOIN1(last_out_point, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
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
    vect_t ohit;
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
    VJOIN1(ohit, rp->r_pt, ohitp->hit_dist, rp->r_dir);

    state->overlaps_callback(reg1, reg2, depth, ihit, ohit, state->overlaps_callback_data);

    /* XXX We should somehow flag the volume/weight calculations as invalid */

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
    struct stat sb;

    FILE *fp = (FILE *)NULL;
    char *buf = NULL;
    int ret = 0;
    size_t sret = 0;

    fp = fopen(name, "rb");
    if (fp == (FILE *)NULL) {
	bu_log("Could not open file - %s\n", name);
	return ANALYZE_ERROR;
    }

    if (stat(name, &sb)) {
	bu_log("Could not read file - %s\n", name);
	fclose(fp);
	return ANALYZE_ERROR;
    }

    state->densities = (struct density_entry *)bu_calloc(128, sizeof(struct density_entry), "density entries");
    state->num_densities = 128;

    /* a mapped file would make more sense here */
    buf = (char *)bu_malloc(sb.st_size+1, "density buffer");
    sret = fread(buf, sb.st_size, 1, fp);
    if (sret != 1)
	perror("fread");
    ret = parse_densities_buffer(buf, (unsigned long)sb.st_size, state->densities, NULL, &state->num_densities);
    bu_free(buf, "density buffer");
    fclose(fp);

    return ret;
}


/**
 * Returns
 * 0 on success
 * !0 on failure
 */
HIDDEN int
densities_from_database(struct current_state *state, struct rt_i *rtip)
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_binunif_internal *bu;
    int ret;
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

    state->densities = (struct density_entry *)bu_calloc(128, sizeof(struct density_entry), "density entries");
    state->num_densities = 128;

    /* Acquire one extra byte to accommodate parse_densities_buffer()
     * (i.e. it wants to write an EOS in buf[bu->count]).
     */
    buf = (char *)bu_malloc(bu->count+1, "density buffer");
    memcpy(buf, bu->u.int8, bu->count);
    ret = parse_densities_buffer(buf, bu->count, state->densities, NULL, &state->num_densities);
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
weight_volume_surf_area_terminate_check(struct current_state *state)
{
    /* Both weight and volume computations rely on this routine to
     * compute values that are printed in summaries.  Hence, both
     * checks must always be done before this routine exits.  So we
     * store the status (can we terminate processing?) in this
     * variable and act on it once both volume and weight computations
     * are done.
     */
    int can_terminate = 1;

    double low, hi, val, delta;

    if (analysis_flags & ANALYSIS_WEIGHT) {
	/* for each object, compute the weight for all views */
	int obj;

	for (obj = 0; obj < state->num_objects; obj++) {
	    int view;
	    double tmp;

	    if (verbose)
		bu_log("object %d of %d\n", obj, state->num_objects);

	    /* compute weight of object for given view */
	    low = INFINITY;
	    hi = -INFINITY;
	    tmp = 0.0;
	    for (view = 0; view < state->num_views; view++) {
		val = state->objs[obj].o_weight[view] =
		    state->objs[obj].o_lenDensity[view] * (state->area[view] / state->shots[view]);
		if(verbose)
		    bu_log("Value : %g\n", val);
		V_MIN(low, val);
		V_MAX(hi, val);
		tmp += val;
	    }
	    delta = hi - low;

	    if (verbose)
		bu_log(
			"\t%s running avg weight %g %s hi=(%g) low=(%g)\n",
			state->objs[obj].o_name,
			(tmp / state->num_views) / units[WGT]->val,
			units[WGT]->name,
			hi / units[WGT]->val,
			low / units[WGT]->val);

	    if (delta > state->weight_tolerance) {
		/* this object differs too much in each view, so we
		 * need to refine the grid. signal that we cannot
		 * terminate.
		 */
		can_terminate = 0;
		if (verbose)
		    bu_log("\t%s differs too much in weight per view.\n",
			    state->objs[obj].o_name);
	    }
	}
	if (can_terminate) {
	    if (verbose)
		bu_log("all objects within tolerance on weight calculation\n");
	}
    }

    if (analysis_flags & ANALYSIS_VOLUME) {
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

	    if (verbose)
		bu_log(
			"\t%s running avg volume %g %s hi=(%g) low=(%g)\n",
			state->objs[obj].o_name,
			(tmp / state->num_views) / units[VOL]->val, units[VOL]->name,
			hi / units[VOL]->val,
			low / units[VOL]->val);

	    if (delta > state->volume_tolerance) {
		/* this object differs too much in each view, so we
		 * need to refine the grid.
		 */
		can_terminate = 0;
		if (verbose)
		    bu_log("\tvolume tol not met on %s.  Refine grid\n",
			    state->objs[obj].o_name);
		break;
	    }
	}
    }

    if(analysis_flags & ANALYSIS_SURF_AREA) {
	int obj;

	for(obj = 0; obj < state->num_objects; obj++) {
	    int view;
	    double tmp;

	    low = INFINITY;
	    hi = -INFINITY;
	    tmp = 0.0;
	    for(view = 0; view < state->num_views; view++) {
		val = state->objs[obj].o_surf_area[view];
		V_MIN(low, val);
		V_MAX(hi, val);
		tmp += val;
	    }
	    delta = hi - low;

	    if(delta > state->sa_tolerance) {
		can_terminate = 0;
		break;
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

    /* this computation is done first, because there are side effects
     * that must be obtained whether we terminate or not
     */
    wv_status = weight_volume_surf_area_terminate_check(state);

    /* if we've reached the grid limit, we're done, no matter what */
    if (state->gridSpacing < state->gridSpacingLimit) {
	bu_log("NOTE: Stopped, grid spacing refined to %g (below lower limit %g).\n",
		state->gridSpacing, state->gridSpacingLimit);
	return 0;
    }
    if (analysis_flags & (ANALYSIS_WEIGHT|ANALYSIS_VOLUME)) {
	if (wv_status == 0) {
	    if (verbose)
		bu_log("%s: Volume/Weight tolerance met. Terminate\n", CPP_FILELINE);
	    return 0; /* terminate */
	}
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

    if (aborted)
	return;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = (struct rt_i *)state->rtip;	/* application uses this instance */
    ap.a_hit = analyze_hit;    /* where to go on a hit */
    ap.a_miss = analyze_miss;  /* where to go on a miss */
    ap.a_resource = &state->resp[cpu];
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.A_LENDEN = 0.0; /* really the cumulative length*density for weight computation*/
    ap.A_LEN = 0.0;    /* really the cumulative length for volume computation */
    ap.A_STATE = ptr; /* really copying the state ptr to the a_uptr */

    if (analysis_flags & ANALYSIS_OVERLAPS) {
	ap.a_overlap = analyze_overlap;
    }

    shot_cnt = 0;
    while (1) {
	bu_semaphore_acquire(ANALYZE_SEM_WORKER);
	if (rectangular_grid_generator(&ap.a_ray, state->grid) == 1){
	    bu_semaphore_release(ANALYZE_SEM_WORKER);
	    break;
	}
	ap.a_user = (int)(state->grid->current_point / (state->grid->x_points));
	bu_semaphore_release(ANALYZE_SEM_WORKER);
	(void)rt_shootray(&ap);
	if (aborted)
	    return;
	shot_cnt++;
    }

    /* There's nothing else left to work on in this view.  It's time
     * to add the values we have accumulated to the totals for the
     * view and return.  When all threads have been through here,
     * we'll have returned to serial computation.
     */
    bu_semaphore_acquire(ANALYZE_SEM_STATS);
    state->shots[state->curr_view] += shot_cnt;
    state->m_lenDensity[state->curr_view] += ap.A_LENDEN; /* add our length*density value */
    state->m_len[state->curr_view] += ap.A_LEN; /* add our volume value */
    bu_semaphore_release(ANALYZE_SEM_STATS);
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
    if (analysis_flags & ANALYSIS_WEIGHT) {
	if (state->densityFileName) {
	    if(debug)
		bu_log("Density from file\n");
	    if (densities_from_file(state, state->densityFileName) != ANALYZE_OK) {
		bu_log("Couldn't load density table from file. Using default density.\n");
		default_den = 1;
	    }
	} else {
	    if(debug)
		bu_log("Density from db\n");
	    if (densities_from_database(state, rtip) != ANALYZE_OK) {
		bu_log("Couldn't load density table from database. Using default density.\n");
		default_den = 1;
	    }
	}
    }
    /* refine the grid spacing if the user has set a lower bound on
     * the number of rays per model axis
     */
    for (axis = 0; axis < 3; axis++) {
	if (state->span[axis] < newGridSpacing * Samples_per_model_axis) {
	    /* along this axis, the gridSpacing is larger than the
	     * model span.  We need to refine.
	     */
	    newGridSpacing = state->span[axis] / Samples_per_model_axis;
	}
    }

    if (!ZERO(newGridSpacing - state->gridSpacing)) {
	bu_log("Initial grid spacing %g %s does not allow %g samples per axis.\n",
		state->gridSpacing / units[LINE]->val, units[LINE]->name, Samples_per_model_axis - 1);

	bu_log("Adjusted initial grid spacing to %g %s to get %g samples per model axis.\n",
		newGridSpacing / units[LINE]->val, units[LINE]->name, Samples_per_model_axis);

	state->gridSpacing = newGridSpacing;
    }

    /* if the vol/weight tolerances are not set, pick something */
    if (analysis_flags & ANALYSIS_VOLUME) {
	if (state->volume_tolerance < 0.0) {
	    /* using 1/1000th the volume as a default tolerance, no particular reason */
	    state->volume_tolerance = state->span[X] * state->span[Y] * state->span[Z] * 0.001;
	    bu_log("Using estimated volume tolerance %g %s\n",
		    state->volume_tolerance / units[VOL]->val, units[VOL]->name);
	} else
	    bu_log("Using volume tolerance %g %s\n",
		    state->volume_tolerance / units[VOL]->val, units[VOL]->name);
	/*if (plot_files)
	    if ((plot_volume=fopen(name, "wb")) == (FILE *)NULL) {
		bu_log("Cannot open plot file %s\n", name);
	    }*/
    }
    if(analysis_flags & ANALYSIS_SURF_AREA) {
	if(state->sa_tolerance < 0.0) {
	    state->sa_tolerance = INFINITY;
	    V_MIN(state->sa_tolerance, state->span[0] * state->span[1]);
	    V_MIN(state->sa_tolerance, state->span[1] * state->span[2]);
	    V_MIN(state->sa_tolerance, state->span[2] * state->span[0]);
	    state->sa_tolerance *= 0.01;
	}
    }
    if (analysis_flags & ANALYSIS_WEIGHT) {
	if (state->weight_tolerance < 0.0) {
	    double max_den = 2.74;	/* Aluminium 7xxx series as default material */
	    int i;
	    for (i = 0; i < state->num_densities; i++) {
		if (state->densities[i].grams_per_cu_mm > max_den)
		    max_den = state->densities[i].grams_per_cu_mm;
	    }
	    state->weight_tolerance = state->span[X] * state->span[Y] * state->span[Z] * 0.1 * max_den;
	    bu_log("Setting weight tolerance to %g %s\n",
		    state->weight_tolerance / units[WGT]->val,
		    units[WGT]->name);
	} else {
	    bu_log("Weight tolerance   %g\n", state->weight_tolerance);
	}
    }

    if (analysis_flags & ANALYSIS_OVERLAPS) {
	if (!ZERO(state->overlap_tolerance))
	    bu_log("overlap tolerance to %g\n", state->overlap_tolerance);
	if (state->overlaps_callback == NULL) {
	    bu_log("\nOverlaps callback function not registered!");
	    return ANALYZE_ERROR;
	}
    }

    if (analysis_flags & ANALYSIS_EXP_AIR) {
	if (state->exp_air_callback == NULL) {
	    bu_log("\nexp_air callback function not registered!");
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

    bu_log("%s Didn't find object named \"%s\" in %d entries\n", CPP_FILELINE, name, state->num_objects);

    return ANALYZE_ERROR;
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
    state->m_len = (double *)bu_calloc(state->num_views, sizeof(double), "volume");
    state->m_volume = (double *)bu_calloc(state->num_views, sizeof(double), "volume");
    state->m_weight = (double *)bu_calloc(state->num_views, sizeof(double), "volume");
    state->m_surf_area = (double *)bu_calloc(state->num_views, sizeof(double), "surface area");
    state->shots = (unsigned long *)bu_calloc(state->num_views, sizeof(unsigned long), "volume");
    state->m_lenTorque = (fastf_t *)bu_calloc(state->num_views, sizeof(vect_t), "lenTorque");

    state->objs = (struct per_obj_data *)bu_calloc(state->num_objects, sizeof(struct per_obj_data), "state report tables");

    /* build data structures for the list of objects the user
     * specified on the command line
     */
    state->objs = (struct per_obj_data *)bu_calloc(state->num_objects, sizeof(struct per_obj_data), "report tables");
    for (i = 0; i < state->num_objects; i++) {
	state->objs[i].o_name = (char *)av[i];
	state->objs[i].o_len = (double *)bu_calloc(state->num_views, sizeof(double), "o_len");
	state->objs[i].o_lenDensity = (double *)bu_calloc(state->num_views, sizeof(double), "o_lenDensity");
	state->objs[i].o_volume = (double *)bu_calloc(state->num_views, sizeof(double), "o_volume");
	state->objs[i].o_weight = (double *)bu_calloc(state->num_views, sizeof(double), "o_weight");
	state->objs[i].o_surf_area = (double *)bu_calloc(state->num_views, sizeof(double), "o_surf_area");
	state->objs[i].o_lenTorque = (fastf_t *)bu_calloc(state->num_views, sizeof(vect_t), "lenTorque");
    }

    /* build objects for each region */
    state->reg_tbl = (struct per_region_data *)bu_calloc(rtip->nregions, sizeof(struct per_region_data), "per_region_data");


    for (i = 0, BU_LIST_FOR (regp, region, &(rtip->HeadRegion)), i++) {
	regp->reg_udata = &state->reg_tbl[i];

	state->reg_tbl[i].r_lenDensity = (double *)bu_calloc(state->num_views, sizeof(double), "r_lenDensity");
	state->reg_tbl[i].r_len = (double *)bu_calloc(state->num_views, sizeof(double), "r_len");
	state->reg_tbl[i].r_volume = (double *)bu_calloc(state->num_views, sizeof(double), "len");
	state->reg_tbl[i].r_weight = (double *)bu_calloc(state->num_views, sizeof(double), "len");
	state->reg_tbl[i].r_surf_area = (double *)bu_calloc(state->num_views, sizeof(double), "surface area");

	state->reg_tbl[i].optr = &state->objs[ find_cmd_obj(state, state->objs, &regp->reg_name[1]) ];
    }
}


HIDDEN int
analyze_single_grid_setup(fastf_t gridSpacing,
			  fastf_t viewsize,
			  point_t eye_model,
			  mat_t Viewrotscale,
			  mat_t model2view,
			  struct rectangular_grid *grid)
{
    mat_t toEye;
    vect_t temp;
    vect_t dx_unit;	 /* view delta-X as unit-len vect */
    vect_t dy_unit;	 /* view delta-Y as unit-len vect */
    mat_t view2model;

    if (viewsize <= 0.0) {
	bu_log("viewsize <= 0");
	return ANALYZE_ERROR;
    }
    /* model2view takes us to eye_model location & orientation */
    MAT_IDN(toEye);
    MAT_DELTAS_VEC_NEG(toEye, eye_model);
    Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
    bn_mat_mul(model2view, Viewrotscale, toEye);
    bn_mat_inv(view2model, model2view);

    grid->grid_spacing = gridSpacing;
    grid->x_points = viewsize/gridSpacing;
    grid->total_points = grid->x_points*grid->x_points;
    bu_log("Processing with grid spacing %g mm %ld x %ld\n", grid->grid_spacing, grid->x_points, grid->x_points);
    grid->current_point=0;

    /* Create basis vectors dx and dy for emanation plane (grid) */
    VSET(temp, 1, 0, 0);
    MAT3X3VEC(dx_unit, view2model, temp);	/* rotate only */
    VSCALE(grid->dx_grid, dx_unit, gridSpacing);

    VSET(temp, 0, 1, 0);
    MAT3X3VEC(dy_unit, view2model, temp);	/* rotate only */
    VSCALE(grid->dy_grid, dy_unit, gridSpacing);

    /* all rays go this direction */
    VSET(temp, 0, 0, -1);
    MAT4X3VEC(grid->ray_direction, view2model, temp);
    VUNITIZE(grid->ray_direction);

    VSET(temp, -1, -1, 0);	/* eye plane */
    MAT4X3PNT(grid->start_coord, view2model, temp);
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

    grid->x_points = grid->steps[state->u_axis] - 1;
    grid->total_points = grid->x_points * (grid->steps[state->v_axis] - 1);
}


HIDDEN void
shoot_rays(struct current_state *state)
{
    /* compute */
    do {
	if (state->use_single_grid) {
	    analyze_single_grid_setup(state->gridSpacing, state->viewsize, state->eye_model, state->Viewrotscale, state->model2view, state->grid);
	    bu_parallel(analyze_worker, state->ncpu, (void *)state);
	} else {
	    int view;
	    double inv_spacing = 1.0/state->gridSpacing;
	    VSCALE(state->grid->steps, state->span, inv_spacing);
	    bu_log("Processing with grid spacing %g mm %ld x %ld x %ld\n",
		    state->gridSpacing,
		    state->grid->steps[0]-1,
		    state->grid->steps[1]-1,
		    state->grid->steps[2]-1);
	    for (view = 0; view < state->num_views; view++) {
		analyze_triple_grid_setup(view, state);
		bu_parallel(analyze_worker, state->ncpu, (void *)state);
		if (aborted)
		    break;
	    }
	}
	state->grid->refine_flag = 1;
	state->gridSpacing *= 0.5;

    } while (check_terminate(state));

    rt_free_rti(state->rtip);
}


HIDDEN int
analyze_setup_ae(double azim,
		 double elev,
		 struct rt_i *rtip,
		 mat_t Viewrotscale,
		 fastf_t *viewsize,
		 mat_t model2view,
		 point_t eye_model)
{
    vect_t temp;
    vect_t diag;
    mat_t toEye;
    mat_t view2model;
    fastf_t eye_backoff = (fastf_t)M_SQRT2;

    if (rtip == NULL) {
	bu_log("analov_do_ae: ERROR: rtip is null");
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
	bu_log("do_ae: infinite model bounds? setting a unit minimum\n");
	VSETALL(rtip->mdl_min, -1);
    }
    if (rtip->mdl_max[X] <= -INFINITY) {
	bu_log("do_ae: infinite model bounds? setting a unit maximum\n");
	VSETALL(rtip->mdl_max, 1);
    }

    MAT_IDN(Viewrotscale);
    bn_mat_angles(Viewrotscale, 270.0+elev, 0.0, 270.0-azim);

    /* Look at the center of the model */
    MAT_IDN(toEye);
    toEye[MDX] = -((rtip->mdl_max[X]+rtip->mdl_min[X])/2.0);
    toEye[MDY] = -((rtip->mdl_max[Y]+rtip->mdl_min[Y])/2.0);
    toEye[MDZ] = -((rtip->mdl_max[Z]+rtip->mdl_min[Z])/2.0);

    VSUB2(diag, rtip->mdl_max, rtip->mdl_min);
    *viewsize = MAGNITUDE(diag);

    /* sanity check: make sure viewsize still isn't zero in case
     * bounding box is empty, otherwise bn_mat_int() will bomb.
     */
    if (*viewsize < 0 || ZERO(*viewsize)) {
	*viewsize = 2.0; /* arbitrary so Viewrotscale is normal */
    }

    Viewrotscale[15] = 0.5*(*viewsize);	/* Viewscale */
    bn_mat_mul(model2view, Viewrotscale, toEye);
    bn_mat_inv(view2model, model2view);
    VSET(temp, 0, 0, eye_backoff);
    MAT4X3PNT(eye_model, view2model, temp);
    return ANALYZE_OK;
}


int
perform_raytracing(struct current_state *state, struct db_i *dbip, char *names[], int num_objects, int flags)
{
    int i;
    struct rt_i *rtip;
    struct resource resp[MAX_PSW];
    struct rectangular_grid grid;

    /* TODO : Add a bu_vls struct / use bu_log() wherever
     * possible.
     */

    state->grid = &grid;
    grid.single_grid = 0;

    analysis_flags = flags;

    /* Get raytracing instance */
    rtip = rt_new_rti(dbip);
    memset(resp, 0, sizeof(resp));
    for(i = 0; i < MAX_PSW; i++) {
	rt_init_resource(&resp[i], i, rtip);
    }

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

    if (state->use_single_grid) {
	state->num_views = 1;
	grid.single_grid = 1;
	if (analyze_setup_ae(state->azimuth_deg, state->elevation_deg, rtip, state->Viewrotscale, &state->viewsize, state->model2view, state->eye_model)) {
	    rt_free_rti(rtip);
	    rtip = NULL;
	    return ANALYZE_ERROR;
	}
    }

    /* we now have to subdivide space */
    VSUB2(state->span, rtip->mdl_max, rtip->mdl_min);
    state->area[0] = state->span[1] * state->span[2];
    state->area[1] = state->span[2] * state->span[0];
    state->area[2] = state->span[0] * state->span[1];

    if (analysis_flags & ANALYSIS_BOX) {
	bu_log("Bounding Box: %g %g %g  %g %g %g\n",
		V3ARGS(rtip->mdl_min), V3ARGS(rtip->mdl_max));

	bu_log("Area: (%g, %g, %g)\n", state->area[X], state->area[Y], state->area[Z]);
    }
    if (verbose) bu_log("ncpu: %d\n", state->ncpu);

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

	bu_log("Trying estimated initial grid spacing: %g %s\n",
		state->gridSpacing / units[LINE]->val, units[LINE]->name);
    } else {
	bu_log("Trying initial grid spacing: %g %s\n",
		state->gridSpacing / units[LINE]->val, units[LINE]->name);
    }

    bu_log("Using grid spacing lower limit: %g %s\n",
	    state->gridSpacingLimit / units[LINE]->val, units[LINE]->name);

    if (options_set(state) != ANALYZE_OK) {
	bu_log("Couldn't set up the options correctly!\n");
	return ANALYZE_ERROR;
    }

    /* initialize some stuff */
    state->rtip = rtip;
    state->resp = resp;
    allocate_region_data(state, names);
    grid.refine_flag = 0;
    shoot_rays(state);
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
