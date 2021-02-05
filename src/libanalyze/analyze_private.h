/*               A N A L Y Z E _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2015-2020 United States Government as represented by
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
/** @file libanalyze/analyze_private.h
 *
 */
#ifndef ANALYZE_PRIVATE_H
#define ANALYZE_PRIVATE_H

#include "common.h"
#include "bu/ptbl.h"

#include "raytrace.h"
#include "analyze.h"

__BEGIN_DECLS

struct minimal_partitions {
    fastf_t *hits;
    int hit_cnt;
    fastf_t *gaps;
    int gap_cnt;
    struct xray ray;
    int index;
    int valid;
    fastf_t missing_in;
    fastf_t missing_out;
};


extern void analyze_gen_worker(int cpu, void *ptr);

/* Returns count of rays in rays array */
ANALYZE_EXPORT extern int analyze_get_bbox_rays(fastf_t **rays, point_t min, point_t max, struct bn_tol *tol);
ANALYZE_EXPORT extern int analyze_get_scaled_bbox_rays(fastf_t **rays, point_t min, point_t max, fastf_t ratio);

ANALYZE_EXPORT extern int analyze_get_solid_partitions(struct bu_ptbl *results, struct rt_gen_worker_vars *pstate, fastf_t *rays, size_t ray_cnt,
	struct db_i *dbip, const char *obj, struct bn_tol *tol, size_t ncpus, int filter);

typedef struct xray * (*getray_t)(void *ptr);
typedef int *         (*getflag_t)(void *ptr);

extern void analyze_seg_filter(struct bu_ptbl *segs, getray_t gray, getflag_t gflag, struct rt_i *rtip, struct resource *resp, fastf_t tol, int ncpus);

/* summary data structure for objects specified on command line */
struct per_obj_data {
    char *o_name;
    double *o_len;
    double *o_lenDensity;
    double *o_volume;
    double *o_mass;
    double *o_area;
    double *o_surf_area;
    fastf_t *o_lenTorque; /* torque vector for each view */
    fastf_t *o_moi;       /* one vector per view for collecting the partial moments of inertia calculation */
    fastf_t *o_poi;       /* one vector per view for collecting the partial products of inertia calculation */
};

/**
 * this is the data we track for each region
 */
struct per_region_data {
    unsigned long hits;
    char *r_name;
    double *r_lenDensity; /* for per-region per-view mass computation */
    double *r_len;        /* for per-region, per-view computation */
    double *r_mass;
    double *r_volume;
    double *r_area;
    double *r_surf_area;
    struct per_obj_data *optr;
};

/* Some defines for re-using the values from the application structure
 * for other purposes
 */
#define A_LENDEN a_color[0]
#define A_LEN a_color[1]
#define A_STATE a_uptr

struct current_state {
    int curr_view; 	/* the "view" number we are shooting */
    int u_axis;    	/* these 3 are in the range 0..2 inclusive and indicate which axis (X, Y, or Z) */
    int v_axis;    	/* is being used for the U, V, or invariant vector direction */
    int i_axis;

    int sem_worker;

    /* sem_worker protects this */
    int v;         	/* indicates how many "grid_size" steps in the v direction have been taken */

    int sem_stats;

    /* sem_stats protects this */
    double *m_lenDensity;
    double *m_len;
    unsigned long *shots;

    /* Plot file I/O protection */
    int sem_plot;

    vect_t u_dir;  	/* direction of U vector for "current view" */
    vect_t v_dir;  	/* direction of V vector for "current view" */
    long steps[3]; 	/* this is per-dimension, not per-view */
    vect_t span;   	/* How much space does the geometry span in each of X, Y, Z directions */
    vect_t area;   	/* area of the view for view with invariant at index */

    int num_objects;	/* number of objects specified on command line */
    int num_regions;	/* number of regions */

    struct per_obj_data *objs;
    struct per_region_data *reg_tbl;

    struct analyze_densities *densities;

    /* the parameters */
    int num_views;
    double overlap_tolerance;
    double volume_tolerance;
    double mass_tolerance;
    double sa_tolerance;
    double azimuth_deg, elevation_deg;
    double gridSpacing, gridSpacingLimit, gridRatio, aspect;
    size_t grid_width, grid_height;
    double samples_per_model_axis;
    int ncpu;
    size_t required_number_hits;
    int use_air;
    int use_single_grid;
    int grid_size_flag; 	/* flag that identifies when the grid-size is mentioned */
    int use_view_information;
    int quiet_missed_report;
    int default_den;
    int analysis_flags;
    int aborted;
    char *densityFileName;

    FILE *plot_volume;

    struct bu_vls *log_str;

    int verbose;
    struct bu_vls *verbose_str;

    int debug;
    struct bu_vls *debug_str;

    fastf_t *m_lenTorque; /* torque vector for each view */
    fastf_t *m_moi;       /* one vector per view for collecting the partial moments of inertia calculation */
    fastf_t *m_poi;       /* one vector per view for collecting the partial products of inertia calculation */

    /* single gird variables */
    mat_t Viewrotscale;
    fastf_t viewsize;
    mat_t model2view;
    point_t eye_model;
    struct rectangular_grid *grid;

    struct rt_i *rtip;
    struct resource *resp;

    struct region_pair *overlapList;
    overlap_callback_t overlaps_callback;
    void* overlaps_callback_data;

    exp_air_callback_t exp_air_callback;
    void* exp_air_callback_data;

    gaps_callback_t gaps_callback;
    void* gaps_callback_data;

    adj_air_callback_t adj_air_callback;
    void* adj_air_callback_data;

    first_air_callback_t first_air_callback;
    void* first_air_callback_data;

    last_air_callback_t last_air_callback;
    void* last_air_callback_data;

    unconf_air_callback_t unconf_air_callback;
    void* unconf_air_callback_data;
};

__END_DECLS

#endif /* ANALYZE_PRIVATE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
