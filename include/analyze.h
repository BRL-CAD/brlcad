/*                       A N A L Y Z E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2019 United States Government as represented by
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
/** @addtogroup libanalyze
 *
 * Functions provided by the LIBANALYZE geometry analysis library.
 *
 */
/** @{ */
/** @file include/analyze.h */

#ifndef ANALYZE_H
#define ANALYZE_H

#include "common.h"
#include "bu/opt.h"
#include "bu/vls.h"
#include "raytrace.h"

#include "analyze/defines.h"

__BEGIN_DECLS

/*
 *     Grid specific structures
 */

/**
 * This structure acts a function pointer table for grid generating functions
 */

struct grid_generator_functions {
    int (*next_ray)(struct xray *rayp, void *grid_context);
    double (*grid_spacing)(void *grid_context);
};

/**
 * This structure is the context passed to the grid generating functions for
 * the rectangular grid type.
 */

struct rectangular_grid {
    int view;
    int max_views;
    int single_grid;
    point_t mdl_origin;
    long steps[3];
    int refine_flag;

    vect_t ray_direction;
    point_t start_coord;
    vect_t dx_grid;
    vect_t dy_grid;
    size_t x_points;
    fastf_t grid_spacing;
    size_t current_point;
    size_t total_points;
};

/*
 *     Overlap specific structures
 */

struct region_pair {
    struct bu_list l;
    union {
	char *name;
	struct region *r1;
    } r;
    struct region *r2;
    unsigned long count;
    double max_dist;
    vect_t coord;
};

/*
 *      Voxel specific structures
 */

/**
 * This structure is for lists that store region names for each voxel
 */

struct voxelRegion {
    char *regionName;
    fastf_t regionDistance;
    struct voxelRegion *nextRegion;
};

/**
 * This structure stores the information about voxels provided by a single raytrace.
 */

struct rayInfo {
    fastf_t sizeVoxel;
    fastf_t *fillDistances;
    struct voxelRegion *regionList;
};

#include "analyze/density.h"

/**
 *     region_pair for gqa
 */
ANALYZE_EXPORT extern struct region_pair *add_unique_pair(struct region_pair *list,
							  struct region *r1,
							  struct region *r2,
							  double dist, point_t pt);

/**
 * grid generator for rectangular grid type
 */
ANALYZE_EXPORT extern int rectangular_grid_generator(struct xray *rayp, void *grid_context);

/**
 * grid generator for rectangular triple grid type
 */
ANALYZE_EXPORT extern int rectangular_triple_grid_generator(struct xray *rayp, void *grid_context);

/**
 * function to get the grid spacing of rectangular grid
 */
ANALYZE_EXPORT extern double rectangular_grid_spacing(void *grid_context);


/**
 * voxelize function takes raytrace instance and user parameters as inputs
 */
ANALYZE_EXPORT extern void
voxelize(struct rt_i *rtip, fastf_t voxelSize[3], int levelOfDetail, void (*create_boxes)(void *callBackData, int x, int y, int z, const char *regionName, fastf_t percentageFill), void *callBackData);


struct analyze_raydiff_results {
    struct bu_ptbl *left;
    struct bu_ptbl *right;
    struct bu_ptbl *both;
};

struct diff_seg {
    point_t in_pt;
    point_t out_pt;
    struct xray ray;
    int valid;
};

struct per_obj_data;
struct per_region_data;

ANALYZE_EXPORT int
analyze_raydiff(struct analyze_raydiff_results **results, struct db_i *dbip,
		const char *left, const char *right, struct bn_tol *tol, int solidcheck);

ANALYZE_EXPORT void
analyze_raydiff_results_free(struct analyze_raydiff_results *results);

ANALYZE_EXPORT int
analyze_obj_inside(struct db_i *dbip, const char *outside, const char *inside, fastf_t tol);

typedef int (*hitfunc_t)(struct application *, struct partition *, struct seg *);
typedef int (*missfunc_t)(struct application *);
typedef int (*overlapfunc_t)(struct application *, struct partition *, struct region *, struct region *, struct partition *);

struct rt_gen_worker_vars {
    struct rt_i *rtip;
    struct resource *resp;
    int rays_cnt;
    fastf_t *rays;
    hitfunc_t fhit;
    missfunc_t fmiss;
    overlapfunc_t foverlap;
    int step;       /* number of rays to be fired by this worker before calling back */
    int *ind_src;   /* source of starting index */
    int curr_ind;   /* current ray index */
    void *ptr; /* application specific info */
};

ANALYZE_EXPORT int
analyze_find_subtracted(struct bu_ptbl *results, struct rt_wdb *wdbp,
			const char *pbrep, struct rt_gen_worker_vars *pbrep_rtvars,
			const char *curr_comb, struct bu_ptbl *candidates, void *curr_union_data, size_t ncpus);

ANALYZE_EXPORT void
analyze_heal_bot(struct rt_bot_internal *bot, double zipper_tol);

#include "analyze/nirt.h"

/**
 * Using ray intersection, sample the database object obj and return a pnts
 * primitive.
 *
 * For the grid sampling method, the tolerance sets the number of rays fired.
 * max_time and max_pnts do *not* impact the GRID sampling logic.
 *
 * The max_pnts limit will cap the number of reported points for the
 * pseudorandom sampling methods, on a per method basis - i.e., the function
 * will return up to max_pnts for each non-grid sampling method that is
 * enabled.  If unset, the maximum pnt count return is 500,000 per method
 * (except for GRID).
 *
 * Likewise, max_time will limit the run time of each pseudorandom method, with
 * the total limit for all methods being method_cnt_enabled * max_time.
 *
 * Return codes:
 *
 * -1 - error
 *  0 - success
 *
 */
#define ANALYZE_OBJ_TO_PNTS_SURF  0x1 /**< @brief save only the first and last hit point on a ray */
#define ANALYZE_OBJ_TO_PNTS_GRID  0x2 /**< @brief sample using an XYZ grid based on the bounding box (default if no method flags are specified) */
#define ANALYZE_OBJ_TO_PNTS_RAND  0x4 /**< @brief sample using Marsaglia sampling on the bounding sphere with pseudo random numbers */
#define ANALYZE_OBJ_TO_PNTS_SOBOL 0x8 /**< @brief sample using Marsaglia sampling on the bounding sphere with Sobol' low-discrepancy-sequence generation */
ANALYZE_EXPORT int analyze_obj_to_pnts(struct rt_pnts_internal *rpnts, double *avg_thickness, struct db_i *dbip,
				       const char *obj, struct bn_tol *tol, int flags, int max_pnts, int max_time, int verbosity);

struct analyze_polygonize_params {
    int max_time;
    long int minimum_free_mem;
    int verbosity;
};
#define ANALYZE_POLYGONIZE_PARAMS_DEFAULT { 30, 150000000, 0 }
ANALYZE_EXPORT extern int
analyze_polygonize(int **faces, int *num_faces, point_t **vertices, int *num_vertices, fastf_t size, point_t p_s, const char *obj, struct db_i *dbip, struct analyze_polygonize_params *p);


struct current_state;
typedef void (*overlap_callback_t)(const struct xray* ray, const struct partition *pp, const struct region *reg1, const struct region *reg2, double depth, void* callback_data);
typedef void (*exp_air_callback_t)(const struct partition *pp, point_t last_out_point, point_t pt, point_t opt, void* callback_data);
typedef void (*gaps_callback_t)(const struct xray* ray, const struct partition *pp, double gap_dist, point_t pt, void* callback_data);
typedef void (*adj_air_callback_t)(const struct xray* ray, const struct partition *pp, point_t pt, void* callback_data);
typedef void (*first_air_callback_t)(const struct xray* ray, const struct partition *pp, void* callback_data);
typedef void (*last_air_callback_t)(const struct xray* ray, const struct partition *pp, void* callback_data);
typedef void (*unconf_air_callback_t)(const struct xray* ray, const struct partition *in_part, const struct partition *out_part, void* callback_data);

/**
 * returns the volume of the specified object (name)
 */
ANALYZE_EXPORT extern fastf_t
analyze_volume(struct current_state *context, const char *name);

/**
 * returns the volume of all the specified objects while ray-tracing
 */
ANALYZE_EXPORT extern fastf_t
analyze_total_volume(struct current_state *context);

/**
 * stores the region name, volume, high and low ranges of volume
 * for the specifed index of region in region table.
 */
ANALYZE_EXPORT extern void
analyze_volume_region(struct current_state *context, int index, char** reg_name, double *volume, double *high, double *low);

/**
 * returns the mass of the specified object (name)
 */
ANALYZE_EXPORT extern fastf_t
analyze_mass(struct current_state *context, const char *name);

/**
 * returns the mass of all the specified objects while ray-tracing
 */
ANALYZE_EXPORT extern fastf_t
analyze_total_mass(struct current_state *context);

/**
 * stores the region name, mass, high and low ranges of mass
 * for the specifed index of region in region table.
 */
ANALYZE_EXPORT extern void
analyze_mass_region(struct current_state *context, int index, char** reg_name, double *mass, double *high, double *low);

/**
 * returns the centroid of the specified object (name)
 */
ANALYZE_EXPORT extern void
analyze_centroid(struct current_state *context, const char *name, point_t value);

/**
 * returns the centroid of all the specified objects while ray-tracing
 */
ANALYZE_EXPORT extern void
analyze_total_centroid(struct current_state *context, point_t value);

/**
 * returns the moments and products of inertia of the specified object (name)
 */
ANALYZE_EXPORT extern void
analyze_moments(struct current_state *context, const char *name, mat_t value);

/**
 * returns the moments and products of all the specified objects while ray-tracing
 */
ANALYZE_EXPORT extern void
analyze_moments_total(struct current_state *context, mat_t moments);

/**
 * returns the surface area of the specified object (name)
 */
ANALYZE_EXPORT extern fastf_t
analyze_surf_area(struct current_state *context, const char *name);

/**
 * returns the surface area of all the specified objects while ray-tracing
 */
ANALYZE_EXPORT extern fastf_t
analyze_total_surf_area(struct current_state *state);

/**
 * stores the region name, surf_area, high and low ranges of surf_area
 * for the specifed index of region in region table.
 */
ANALYZE_EXPORT extern void
analyze_surf_area_region(struct current_state *state, int i, char **name, double *surf_area, double *high, double *low);

/**
 * performs raytracing based on the current state
 */
ANALYZE_EXPORT extern int
perform_raytracing(struct current_state *context, struct db_i *dbip, char *names[], int num_objects, int flags);

/**
 * functions to initialize and clear current_state struct
 */
ANALYZE_EXPORT extern 
struct current_state * analyze_current_state_init();

ANALYZE_EXPORT extern void
analyze_free_current_state(struct current_state *context);

/*
 * Below are the setter functions for the parameters of check API
 */

/**
 * sets the azimuth and elevation for single grid to shoot rays
 */
ANALYZE_EXPORT extern void
analyze_set_azimuth(struct current_state *context , fastf_t azimuth);

ANALYZE_EXPORT extern void
analyze_set_elevation(struct current_state *context , fastf_t elevation);

/**
 * sets the grid_spacing and grid spacing limit for shooting the rays
 */
ANALYZE_EXPORT extern void
analyze_set_grid_spacing(struct current_state *context , fastf_t gridSpacing, fastf_t gridSpacingLimit);

/**
 * returns the grid_spacing when the raytracing stopped -- used for printing summaries
 */
ANALYZE_EXPORT extern fastf_t
analyze_get_grid_spacing(struct current_state *context);

/**
 * sets the cell_width by cell_height ratio (default is 1)
 */
ANALYZE_EXPORT extern void
analyze_set_grid_ratio(struct current_state *context, fastf_t gridRatio);

/**
 * sets the grid width and grid height values
 */
ANALYZE_EXPORT extern void
analyze_set_grid_size(struct current_state *state, fastf_t width, fastf_t height);

/**
 * sets the width by height ratio (default is 1)
 */
ANALYZE_EXPORT extern void
analyze_set_aspect(struct current_state *context, fastf_t aspect);

/**
 * used to specify the minimum samples per model axis
 */
ANALYZE_EXPORT extern void
analyze_set_samples_per_model_axis(struct current_state *context, fastf_t samples_per_model_axis);

/**
 * sets the tolerance values for overlaps, volume, mass and surface area for the analysis
 */
ANALYZE_EXPORT extern void
analyze_set_overlap_tolerance(struct current_state *context , fastf_t overlap_tolerance);

ANALYZE_EXPORT extern void
analyze_set_volume_tolerance(struct current_state *context , fastf_t volume_tolerance);

ANALYZE_EXPORT extern void
analyze_set_mass_tolerance(struct current_state *context , fastf_t mass_tolerance);

ANALYZE_EXPORT extern void
analyze_set_surf_area_tolerance(struct current_state *context, fastf_t sa_tolerance);

/**
 * sets the number of cpus to be used for raytracing
 */
ANALYZE_EXPORT extern void
analyze_set_ncpu(struct current_state *context , int ncpu);

/**
 * sets the required number of hits per object when raytracing
 */
ANALYZE_EXPORT extern void
analyze_set_required_number_hits(struct current_state *context , size_t required_number_hits);

/**
 * sets a flag which quiets the missed reports
 */
ANALYZE_EXPORT extern void
analyze_set_quiet_missed_report(struct current_state *context);

/**
 * sets the use_air flag for raytracing
 */
ANALYZE_EXPORT extern void
analyze_set_use_air(struct current_state *context , int use_air);

/**
 * set the number of views when shooting triple grids of rays
 */
ANALYZE_EXPORT extern void
analyze_set_num_views(struct current_state *context , int num_views);

/**
 * set the name of the density file
 */
ANALYZE_EXPORT extern void
analyze_set_densityfile(struct current_state *context , char *densityFileName);

/**
 * registers the plotfile used to store the plot information for volume
 */
ANALYZE_EXPORT extern void
analyze_set_volume_plotfile(struct current_state *context , FILE* plotvolume);

/**
 * used to set debug flag and get debug information into the bu_vls pointer
 */
ANALYZE_EXPORT extern void
analyze_enable_debug(struct current_state *context, struct bu_vls *vls);

/**
 * used to set verbose flag and get verbose information into the bu_vls pointer
 */
ANALYZE_EXPORT extern void
analyze_enable_verbose(struct current_state *context, struct bu_vls *vls);

/**
 * used to get the value of number of regions
 */
ANALYZE_EXPORT extern int
analyze_get_num_regions(struct current_state *context);

/**
 * used to prepare single grid (eye position) by expliciting mentioning viewsize,
 * eye model and orientation
 */
ANALYZE_EXPORT extern void
analyze_set_view_information(struct current_state *context, double viewsize, point_t *eye_model, quat_t *orientation);

/**
 * registers the callback functions defined by the user to be called when raytracing
 */
ANALYZE_EXPORT extern void
analyze_register_overlaps_callback(struct current_state *context, overlap_callback_t callback_function, void* callback_data);

ANALYZE_EXPORT extern void
analyze_register_exp_air_callback(struct current_state *context, exp_air_callback_t callback_function, void* callback_data);

ANALYZE_EXPORT extern void
analyze_register_gaps_callback(struct current_state *context, gaps_callback_t callback_function, void* callback_data);

ANALYZE_EXPORT extern void
analyze_register_adj_air_callback(struct current_state *context, adj_air_callback_t callback_function, void* callback_data);

ANALYZE_EXPORT extern void
analyze_register_first_air_callback(struct current_state *context, first_air_callback_t callback_function, void* callback_data);

ANALYZE_EXPORT extern void
analyze_register_last_air_callback(struct current_state *context, last_air_callback_t callback_function, void* callback_data);

ANALYZE_EXPORT extern void
analyze_register_unconf_air_callback(struct current_state *context, unconf_air_callback_t callback_function, void* callback_data);

__END_DECLS

#endif /* ANALYZE_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
