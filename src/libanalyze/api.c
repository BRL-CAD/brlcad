/*                           A P I . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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

/* BRL-CAD includes */
#include "common.h"
#include "analyze.h"
#include "raytrace.h"
#include "vmath.h"
#include "rt/geom.h"
#include "bu/parallel.h"

/* System includes */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <limits.h>

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

struct current_state {
    int curr_view; 	/* the "view" number we are shooting */
    int u_axis;    	/* these 3 are in the range 0..2 inclusive and indicate which axis (X, Y, or Z) */
    int v_axis;    	/* is being used for the U, V, or invariant vector direction */
    int i_axis;

    /* ANALYZE_SEM_WORKER protects this */
    int v;         	/* indicates how many "grid_size" steps in the v direction have been taken */

    /* ANALYZE_SEM_STATS protects this */
    double *m_lenDensity;
    double *m_len;
    double *m_volume;
    double *m_weight;
    double *m_surf_area;
    unsigned long *shots;
    int first;     	/* this is the first time we've computed a set of views */

    vect_t u_dir;  	/* direction of U vector for "current view" */
    vect_t v_dir;  	/* direction of V vector for "current view" */
    struct rt_i *rtip;
    long steps[3]; 	/* this is per-dimension, not per-view */
    vect_t span;   	/* How much space does the geometry span in each of X, Y, Z directions */
    vect_t area;   	/* area of the view for view with invariant at index */
    double overlap_tolerance;
    double volume_tolerance;
    double weight_tolerance;
    double sa_tolerance;
    int num_objects; 	/* number of objects specified on command line */
    int num_views;

    fastf_t *m_lenTorque; /* torque vector for each view */

    struct resource *resp;
    struct raytracing_context *context;
};

/* summary data structure for objects specified on command line */
struct per_obj_data {
    char *o_name;
    double *o_len;
    double *o_lenDensity;
    double *o_volume;
    double *o_weight;
    double *o_surf_area;
    fastf_t *o_lenTorque; /* torque vector for each view */
};

struct per_obj_data *obj_tbl;

/**
 * this is the data we track for each region
 */
struct per_region_data {
    unsigned long hits;
    double *r_lenDensity; /* for per-region per-view weight computation */
    double *r_len;        /* for per-region, per-view computation */
    double *r_weight;
    double *r_volume;
    double *r_surf_area;
    struct per_obj_data *optr;
};

static struct per_region_data *reg_tbl;

static int debug = 0;
static int default_den = 0;
static int analysis_flags;
static char *densityFileName;
static double gridSpacing;
static double gridSpacingLimit;
static int ncpu;
static double Samples_per_model_axis = 2.0;
static int aborted = 0;
static int verbose = 0;

/* Some defines for re-using the values from the application structure
 * for other purposes
 */
#define A_LENDEN a_color[0]
#define A_LEN a_color[1]
#define A_STATE a_uptr

struct cvt_tab {
    double val;
    char name[32];
};

/* this table keeps track of the "current" or "user selected units and
 * the associated conversion values
 */
#define LINE 0
#define VOL 1
#define WGT 2
static const struct cvt_tab units[3][3] = {
    {{1.0, "mm"}},	/* linear */
    {{1.0, "cu mm"}},	/* volume */
    {{1.0, "grams"}}	/* weight */
};

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
    point_t pt;
    double dist;       /* the thickness of the partition */
    double val;
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

	/* computing the weight of the objects */
	if (analysis_flags & ANALYSIS_WEIGHT) {
	    if (debug) {
		bu_semaphore_acquire(ANALYZE_SEM_WORKER);
		bu_log("Hit %s doing weight\n", pp->pt_regionp->reg_name);
		bu_semaphore_release(ANALYZE_SEM_WORKER);
	    }

	    /* make sure mater index is within range of densities */
	    if (pp->pt_regionp->reg_gmater >= state->context->num_densities) {
		if(default_den == 0) {
		    bu_semaphore_acquire(ANALYZE_SEM_WORKER);
		    bu_log("Density index %d on region %s is outside of range of table [1..%d]\nSet GIFTmater on region or add entry to density table\n",
			    pp->pt_regionp->reg_gmater,
			    pp->pt_regionp->reg_name,
			    state->context->num_densities); /* XXX this should do something else */
		    bu_semaphore_release(ANALYZE_SEM_WORKER);
		    return ANALYZE_ERROR;
		}
	    }

	    /* make sure the density index has been set */
	    bu_semaphore_acquire(ANALYZE_SEM_WORKER);
	    if(default_den || state->context->densities[pp->pt_regionp->reg_gmater].magic != DENSITY_MAGIC) {
		BU_GET(de, struct density_entry);		/* Aluminium 7xxx series as default material */
		de->magic = DENSITY_MAGIC;
		de->grams_per_cu_mm = 2.74;
		de->name = "Aluminium, 7079-T6";
	    } else {
		de = state->context->densities + pp->pt_regionp->reg_gmater;
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

	}

	/* note that this region has been seen */
	((struct per_region_data *)pp->pt_regionp->reg_udata)->hits++;
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
 * Returns
 * 0 on success
 * !0 on failure
 */
HIDDEN int
densities_from_file(struct raytracing_context *context, char *name)
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

    context->densities = (struct density_entry *)bu_calloc(128, sizeof(struct density_entry), "density entries");
    context->num_densities = 128;

    /* a mapped file would make more sense here */
    buf = (char *)bu_malloc(sb.st_size+1, "density buffer");
    sret = fread(buf, sb.st_size, 1, fp);
    if (sret != 1)
	perror("fread");
    ret = parse_densities_buffer(buf, (unsigned long)sb.st_size, context->densities, NULL, &context->num_densities);
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
densities_from_database(struct raytracing_context *context, struct rt_i *rtip)
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

    context->densities = (struct density_entry *)bu_calloc(128, sizeof(struct density_entry), "density entries");
    context->num_densities = 128;

    /* Acquire one extra byte to accommodate parse_densities_buffer()
     * (i.e. it wants to write an EOS in buf[bu->count]).
     */
    buf = (char *)bu_malloc(bu->count+1, "density buffer");
    memcpy(buf, bu->u.int8, bu->count);
    ret = parse_densities_buffer(buf, bu->count, context->densities, NULL, &context->num_densities);
    bu_free((void *)buf, "density buffer");

    return ret;
}

/**
 * This routine must be prepared to run in parallel
 */
HIDDEN int
next_row(struct current_state *state)
{
    int v;
    /* look for more work */
    bu_semaphore_acquire(ANALYZE_SEM_WORKER);

    if (state->v < state->steps[state->v_axis])
	v = state->v++;	/* get a row to work on */
    else
	v = 0; /* signal end of work */

    bu_semaphore_release(ANALYZE_SEM_WORKER);

    return v;
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
		val = obj_tbl[obj].o_weight[view] =
		    obj_tbl[obj].o_lenDensity[view] * (state->area[view] / state->shots[view]);
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
			obj_tbl[obj].o_name,
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
			    obj_tbl[obj].o_name);
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
		val = obj_tbl[obj].o_volume[view] =
		    obj_tbl[obj].o_len[view] * (state->area[view] / state->shots[view]);
		V_MIN(low, val);
		V_MAX(hi, val);
		tmp += val;
	    }
	    delta = hi - low;

	    if (verbose)
		bu_log(
			"\t%s running avg volume %g %s hi=(%g) low=(%g)\n",
			obj_tbl[obj].o_name,
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
			    obj_tbl[obj].o_name);
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
		val = obj_tbl[obj].o_surf_area[view];
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
    if (gridSpacing < gridSpacingLimit) {
	bu_log("NOTE: Stopped, grid spacing refined to %g (below lower limit %g).\n",
		gridSpacing, gridSpacingLimit);
	return 0;
    }
    if (wv_status == 0) {
	if (verbose)
	    bu_log("%s: Volume/Weight tolerance met. Terminate\n", CPP_FILELINE);
	return 0; /* terminate */
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
    int u, v;
    double v_coord;
    struct current_state *state = (struct current_state *)ptr;
    unsigned long shot_cnt;

    if (aborted)
	return;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = (struct rt_i *)state->rtip;	/* application uses this instance */
    ap.a_hit = analyze_hit;    /* where to go on a hit */
    ap.a_miss = analyze_miss;  /* where to go on a miss */
    ap.a_resource = &state->resp[cpu];
    ap.A_LENDEN = 0.0; /* really the cumulative length*density for weight computation*/
    ap.A_LEN = 0.0;    /* really the cumulative length for volume computation */

    /* gross hack */
    ap.a_ray.r_dir[state->u_axis] = ap.a_ray.r_dir[state->v_axis] = 0.0;
    ap.a_ray.r_dir[state->i_axis] = 1.0;

    ap.A_STATE = ptr; /* really copying the state ptr to the a_uptr */

    u = -1;

    v = next_row(state);

    shot_cnt = 0;
    while (v) {

	v_coord = v * gridSpacing;
	if (debug) {
	    bu_semaphore_acquire(ANALYZE_SEM_WORKER);
	    bu_log("  v = %d v_coord=%g\n", v, v_coord);
	    bu_semaphore_release(ANALYZE_SEM_WORKER);
	}

	if ((v&1) || state->first) {
	    /* shoot all the rays in this row.  This is either the
	     * first time a view has been computed or it is an odd
	     * numbered row in a grid refinement
	     */
	    for (u=1; u < state->steps[state->u_axis]; u++) {
		ap.a_ray.r_pt[state->u_axis] = ap.a_rt_i->mdl_min[state->u_axis] + u*gridSpacing;
		ap.a_ray.r_pt[state->v_axis] = ap.a_rt_i->mdl_min[state->v_axis] + v*gridSpacing;
		ap.a_ray.r_pt[state->i_axis] = ap.a_rt_i->mdl_min[state->i_axis];

		if (debug) {
		    bu_semaphore_acquire(ANALYZE_SEM_WORKER);
		    bu_log("%5g %5g %5g -> %g %g %g\n", V3ARGS(ap.a_ray.r_pt),
			    V3ARGS(ap.a_ray.r_dir));
		    bu_semaphore_release(ANALYZE_SEM_WORKER);
		}
		ap.a_user = v;
		(void)rt_shootray(&ap);

		if (aborted)
		    return;

		shot_cnt++;
	    }
	} else {
	    /* shoot only the rays we need to on this row.  Some of
	     * them have been computed in a previous iteration.
	     */
	    for (u=1; u < state->steps[state->u_axis]; u+=2) {
		ap.a_ray.r_pt[state->u_axis] = ap.a_rt_i->mdl_min[state->u_axis] + u*gridSpacing;
		ap.a_ray.r_pt[state->v_axis] = ap.a_rt_i->mdl_min[state->v_axis] + v*gridSpacing;
		ap.a_ray.r_pt[state->i_axis] = ap.a_rt_i->mdl_min[state->i_axis];

		if (debug) {
		    bu_semaphore_acquire(ANALYZE_SEM_WORKER);
		    bu_log("%5g %5g %5g -> %g %g %g\n", V3ARGS(ap.a_ray.r_pt),
			    V3ARGS(ap.a_ray.r_dir));
		    bu_semaphore_release(ANALYZE_SEM_WORKER);
		}
		ap.a_user = v;
		(void)rt_shootray(&ap);

		if (aborted)
		    return;

		shot_cnt++;

		if (debug)
		    if (u+1 < state->steps[state->u_axis]) {
			bu_semaphore_acquire(ANALYZE_SEM_WORKER);
			bu_log("  ---\n");
			bu_semaphore_release(ANALYZE_SEM_WORKER);
		    }
	    }
	}

	/* iterate */
	v = next_row(state);
    }

    if (debug && (u == -1)) {
	bu_semaphore_acquire(ANALYZE_SEM_WORKER);
	bu_log("Didn't shoot any rays\n");
	bu_semaphore_release(ANALYZE_SEM_WORKER);
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
    double newGridSpacing = gridSpacing;
    int axis;

    /* figure out where the density values are coming from and get
     * them.
     */
    if (analysis_flags & ANALYSIS_WEIGHT) {
	if (densityFileName) {
	    if(debug)
		bu_log("Density from file\n");
	    if (densities_from_file(state->context, densityFileName) != ANALYZE_OK) {
		bu_log("Couldn't load density table from file. Using default density.\n");
		default_den = 1;
	    }
	} else {
	    if(debug)
		bu_log("Density from db\n");
	    if (densities_from_database(state->context, rtip) != ANALYZE_OK) {
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

    if (!ZERO(newGridSpacing - gridSpacing)) {
	bu_log("Initial grid spacing %g %s does not allow %g samples per axis.\n",
		gridSpacing / units[LINE]->val, units[LINE]->name, Samples_per_model_axis - 1);

	bu_log("Adjusted initial grid spacing to %g %s to get %g samples per model axis.\n",
		newGridSpacing / units[LINE]->val, units[LINE]->name, Samples_per_model_axis);

	gridSpacing = newGridSpacing;
    }

    /* if the vol/weight tolerances are not set, pick something */
    if (analysis_flags & ANALYSIS_VOLUME) {
	/*char *name = "volume.plot3";*/
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
	    for (i = 0; i < state->context->num_densities; i++) {
		if (state->context->densities[i].grams_per_cu_mm > max_den)
		    max_den = state->context->densities[i].grams_per_cu_mm;
	    }
	    state->weight_tolerance = state->span[X] * state->span[Y] * state->span[Z] * 0.1 * max_den;
	    bu_log("Setting weight tolerance to %g %s\n",
		    state->weight_tolerance / units[WGT]->val,
		    units[WGT]->name);
	} else {
	    bu_log("Weight tolerance   %g\n", state->weight_tolerance);
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
allocate_region_data(struct current_state *state, struct raytracing_context *context, const char *av[])
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

    context->shots = (unsigned long *)bu_calloc(state->num_views, sizeof(unsigned long), "volume");
    context->objs = (struct per_obj_data *)bu_calloc(state->num_objects, sizeof(struct per_obj_data), "state report tables");

    /* build data structures for the list of objects the user
     * specified on the command line
     */
    obj_tbl = (struct per_obj_data *)bu_calloc(state->num_objects, sizeof(struct per_obj_data), "report tables");
    for (i = 0; i < state->num_objects; i++) {
	obj_tbl[i].o_name = (char *)av[i];
	obj_tbl[i].o_len = (double *)bu_calloc(state->num_views, sizeof(double), "o_len");
	obj_tbl[i].o_lenDensity = (double *)bu_calloc(state->num_views, sizeof(double), "o_lenDensity");
	obj_tbl[i].o_volume = (double *)bu_calloc(state->num_views, sizeof(double), "o_volume");
	obj_tbl[i].o_weight = (double *)bu_calloc(state->num_views, sizeof(double), "o_weight");
	obj_tbl[i].o_surf_area = (double *)bu_calloc(state->num_views, sizeof(double), "o_surf_area");
	obj_tbl[i].o_lenTorque = (fastf_t *)bu_calloc(state->num_views, sizeof(vect_t), "lenTorque");
    }

    /* build objects for each region */
    reg_tbl = (struct per_region_data *)bu_calloc(rtip->nregions, sizeof(struct per_region_data), "per_region_data");


    for (i = 0, BU_LIST_FOR (regp, region, &(rtip->HeadRegion)), i++) {
	regp->reg_udata = &reg_tbl[i];

	reg_tbl[i].r_lenDensity = (double *)bu_calloc(state->num_views, sizeof(double), "r_lenDensity");
	reg_tbl[i].r_len = (double *)bu_calloc(state->num_views, sizeof(double), "r_len");
	reg_tbl[i].r_volume = (double *)bu_calloc(state->num_views, sizeof(double), "len");
	reg_tbl[i].r_weight = (double *)bu_calloc(state->num_views, sizeof(double), "len");
	reg_tbl[i].r_surf_area = (double *)bu_calloc(state->num_views, sizeof(double), "surface area");

	reg_tbl[i].optr = &obj_tbl[ find_cmd_obj(state, obj_tbl, &regp->reg_name[1]) ];
    }
}

HIDDEN struct raytracing_context *
perform_raytracing(struct current_state *state, struct raytracing_context *context)
{
    /* compute */
    do {
	double inv_spacing = 1.0/gridSpacing;
	int view;

	VSCALE(state->steps, state->span, inv_spacing);

	bu_log("Processing with grid spacing %g %s %ld x %ld x %ld\n",
		gridSpacing / units[LINE]->val,
		units[LINE]->name,
		state->steps[0]-1,
		state->steps[1]-1,
		state->steps[2]-1);


	for (view = 0; view < state->num_views; view++) {

	    if(verbose)
		bu_log("  view %d\n", view);

	    /* gross hack.  By assuming we have <= 3 views, we can let
	     * the view # indicate a coordinate axis.  Note this is
	     * used as an index into state.area[]
	     */
	    state->i_axis = state->curr_view = view;
	    state->u_axis = (state->curr_view+1) % 3;
	    state->v_axis = (state->curr_view+2) % 3;

	    state->u_dir[state->u_axis] = 1;
	    state->u_dir[state->v_axis] = 0;
	    state->u_dir[state->i_axis] = 0;

	    state->v_dir[state->u_axis] = 0;
	    state->v_dir[state->v_axis] = 1;
	    state->v_dir[state->i_axis] = 0;
	    state->v = 1;

	    bu_parallel(analyze_worker, ncpu, (void *)state);

	    if (aborted)
		break;
	}
	state->first = 0;
	gridSpacing *= 0.5;

    } while (check_terminate(state));

    if(!aborted) {
	context->objs = obj_tbl;
	context->shots = state->shots;
    }
    else {
	analyze_raytracing_context_clear(context);
    }

    rt_free_rti(state->rtip);

    return context;
}

int
analyze_raytracing_context_init(struct raytracing_context *context, struct db_i *dbip,
	const char *names[], int *flags)
{
    int i;
    struct current_state state;
    struct rt_i *rtip;
    struct resource resp[MAX_PSW];
    char *densityfile = NULL;
    FILE *densityfp = (FILE *)0;
    const char *curdir = getenv("PWD");
    const char *homedir = getenv("HOME");

    /* TODO : Add a bu_vls struct / use bu_log() wherever
     * possible.
     */

#define maxm(a, b) (a>b?a:b)
#define DENSITY_FILE ".density"

    context->densities = NULL;
    context->num_densities = 0;
    state.context = context;

    analysis_flags = (*flags);
    i = maxm(strlen(curdir), strlen(homedir)) + strlen(DENSITY_FILE) + 2;
    /* densityfile is global to this file and will be used later (and then freed) */
    densityfile = (char *)bu_calloc((unsigned int)i, sizeof(char), "densityfile");

    snprintf(densityfile, i, "%s/%s", curdir, DENSITY_FILE);

    if ((densityfp = fopen(densityfile, "r")) == (FILE *)0) {
	snprintf(densityfile, i, "%s/%s", homedir, DENSITY_FILE);
    }

    /* Get raytracing instance */
    rtip = rt_new_rti(dbip);
    memset(resp, 0, sizeof(resp));
    for(i = 0; i < MAX_PSW; i++) {
	rt_init_resource(&resp[i], i, rtip);
    }

    /* Prep for raytracing */
    state.num_objects = 0;
    state.num_views = 3;
    densityFileName = densityfile;
    state.volume_tolerance = -1;
    state.weight_tolerance = -1;
    state.sa_tolerance = -1;
    for(i = 0; ; i++) {
	if(names[i])
	    state.num_objects++;
	else
	    break;
    }
    for(i = 0; i < state.num_objects; i++) {
	if(rt_gettree(rtip, names[i]) < 0) {
	    bu_log("Loading geometry for [%s] FAILED", names[i]);
	    return ANALYZE_ERROR;
	}
    }
    rt_prep_parallel(rtip, ncpu);

    /* we now have to subdivide space */
    VSUB2(state.span, rtip->mdl_max, rtip->mdl_min);
    state.area[0] = state.span[1] * state.span[2];
    state.area[1] = state.span[2] * state.span[0];
    state.area[2] = state.span[0] * state.span[1];
    VMOVE(context->span, state.span);
    VMOVE(context->area, state.area);
    context->num_objects = state.num_objects;
    context->num_views = state.num_views;

    gridSpacing = 50.0;
    gridSpacingLimit = 0.001;

    if (analysis_flags & ANALYSIS_BOX) {
	bu_log("Bounding Box: %g %g %g  %g %g %g\n",
		V3ARGS(rtip->mdl_min), V3ARGS(rtip->mdl_max));

	bu_log("Area: (%g, %g, %g)\n", state.area[X], state.area[Y], state.area[Z]);
    }
    if (verbose) bu_log("ncpu: %d\n", ncpu);

    /* if the user did not specify the initial grid spacing limit, we
     * need to compute a reasonable one for them.
     */
    if (ZERO(gridSpacing)) {
	double min_span = MAX_FASTF;
	VPRINT("span", state.span);

	V_MIN(min_span, state.span[X]);
	V_MIN(min_span, state.span[Y]);
	V_MIN(min_span, state.span[Z]);

	gridSpacing = gridSpacingLimit;
	do {
	    gridSpacing *= 2.0;
	} while (gridSpacing < min_span);

	gridSpacing *= 0.25;
	if (gridSpacing < gridSpacingLimit) gridSpacing = gridSpacingLimit;

	bu_log("Trying estimated initial grid spacing: %g %s\n",
		gridSpacing / units[LINE]->val, units[LINE]->name);
    } else {
	bu_log("Trying initial grid spacing: %g %s\n",
		gridSpacing / units[LINE]->val, units[LINE]->name);
    }

    bu_log("Using grid spacing lower limit: %g %s\n",
	    gridSpacingLimit / units[LINE]->val, units[LINE]->name);

    if (options_set(&state) != ANALYZE_OK) {
	bu_log("Couldn't set up the options correctly!\n");
	return ANALYZE_ERROR;
    }

    /* initialize some stuff */
    state.rtip = rtip;
    state.resp = resp;
    allocate_region_data(&state, context, names);

    context = perform_raytracing(&state, context);
    (*flags) = analysis_flags;
    return (!context) ? ANALYZE_ERROR : ANALYZE_OK;
}

void
analyze_raytracing_context_clear(struct raytracing_context *context)
{
    int i;

    if (context->densities != NULL) {
	for (i = 0; i < context->num_densities; ++i) {
	    if (context->densities[i].name != 0)
		bu_free(context->densities[i].name, "density name");
	}

	bu_free(context->densities, "densities");
	context->densities = NULL;
	context->num_densities = 0;
    }

    bu_free(context->shots, "m_shots");

    for (i = 0; i < context->num_objects; i++) {
	bu_free(obj_tbl[i].o_len, "o_len");
	bu_free(obj_tbl[i].o_lenDensity, "o_lenDensity");
	bu_free(obj_tbl[i].o_volume, "o_volume");
	bu_free(obj_tbl[i].o_weight, "o_weight");
	bu_free(obj_tbl[i].o_lenTorque, "o_lenTorque");
    }
    bu_free(obj_tbl, "object table");
    context->objs = obj_tbl = NULL;
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
