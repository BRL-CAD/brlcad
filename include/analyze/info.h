/*                           I N F O . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file analyze/info.h */

#ifndef ANALYZE_INFO_H
#define ANALYZE_INFO_H

#include "common.h"
#include "raytrace.h"
#include <string.h>

#include "analyze/defines.h"

__BEGIN_DECLS

/* ======================================================================
 * Public analysis-type flags for perform_raytracing() and analyze_run().
 *
 * These values are the canonical public identifiers for the analysis
 * flags previously scattered across check_private.h and api.c.  Callers
 * that invoke perform_raytracing() directly should use ANALYZE_* names;
 * the legacy ANALYSIS_* aliases inside the library remain for internal
 * compatibility but should not appear in new code.
 * ====================================================================== */
#define ANALYZE_VOLUME       0x0001  /**< volume computation */
#define ANALYZE_CENTROIDS    0x0002  /**< centroid computation */
#define ANALYZE_SURF_AREA    0x0004  /**< surface area computation */
#define ANALYZE_MASS         0x0008  /**< mass / weight computation */
#define ANALYZE_OVERLAPS     0x0010  /**< overlap detection */
#define ANALYZE_MOMENTS      0x0020  /**< moments of inertia */
#define ANALYZE_BOX          0x0040  /**< bounding box (reserved; computed separately) */
#define ANALYZE_GAP          0x0080  /**< gap detection */
#define ANALYZE_EXP_AIR      0x0100  /**< exposed air detection */
#define ANALYZE_ADJ_AIR      0x0200  /**< adjacent air detection */
#define ANALYZE_FIRST_AIR    0x0400  /**< first-air detection */
#define ANALYZE_LAST_AIR     0x0800  /**< last-air detection */
#define ANALYZE_UNCONF_AIR   0x1000  /**< unconfined air detection */
#define ANALYZE_ALL          0x1FFF  /**< all raytracing-based analyses */

/* ======================================================================
 * Sampler type constants for struct analyze_config.
 * ====================================================================== */
/** Original three-axis aligned rectangular grid (legacy default). */
#define ANALYZE_SAMPLER_TRIPLE_GRID  0
/** Rectangular grid along an arbitrary direction (bias-reduction). */
#define ANALYZE_SAMPLER_ROTATED      1
/** Isotropic random sampling via Cauchy-Crofton formulae. */
#define ANALYZE_SAMPLER_CROFTON      2
/** Single-plane grid derived from an explicit eye / view specification. */
#define ANALYZE_SAMPLER_VIEW_PLANE   3

/* ======================================================================
 * Session-based analysis API (new, forward-looking design).
 *
 * The preferred usage pattern is:
 *
 *   struct analyze_config cfg = ANALYZE_CONFIG_INIT_ZERO;
 *   cfg.grid_spacing = 5.0;
 *   cfg.ncpu = 0;   // 0 = all CPUs
 *   ...
 *   struct analyze_results *res = analyze_run(sess, dbip, names, n, ANALYZE_VOLUME | ANALYZE_MASS);
 *   printf("volume = %g mm^3\n", res->total_volume);
 *   analyze_results_free(res);
 *
 * This interface is being introduced alongside the existing
 * perform_raytracing() / setter API to allow gradual migration without
 * breaking existing callers.
 * ====================================================================== */

/* ======================================================================
 * Per-event render hook types.
 *
 * These optional callbacks are placed in struct analyze_config and are
 * invoked by analyze_run() once per detected event during the ray pass.
 * They allow front-end callers (check, gqa, …) to write plot files, draw
 * overlays, or produce real-time output without coupling libanalyze to any
 * specific output mechanism.
 *
 * All geometric arguments are in BRL-CAD model-space millimetres.
 * Implementations must not call analyze_results_free() or re-enter
 * analyze_run() from within a hook because the analysis state is still live.
 * Thread safety (e.g. for plot-file writes) is the responsibility of the
 * hook implementation.
 * ====================================================================== */

/** Fired for every detected overlap segment (before per-pair deduplication;
 *  may fire many times for the same region pair). */
typedef void (*analyze_overlap_render_fn)(
    const char *r1, const char *r2,
    double depth, point_t ihit, point_t ohit,
    void *data);

/** Fired for every detected gap between solid objects.
 *  gap_start is the exit of the last solid; gap_end is the entry of the next. */
typedef void (*analyze_gap_render_fn)(
    const char *r1, const char *r2,
    double dist, point_t gap_start, point_t gap_end,
    void *data);

/** Fired for every detected adjacent-air boundary.
 *  in_pt is entry into the air region; out_pt is 1/4 across it. */
typedef void (*analyze_adj_air_render_fn)(
    const char *r1, const char *r2,
    point_t in_pt, point_t out_pt,
    void *data);

/** Fired for every detected exposed-air partition.
 *  in_pt / out_pt are the inhit / outhit of the air segment. */
typedef void (*analyze_exp_air_render_fn)(
    const char *region_name,
    point_t in_pt, point_t out_pt,
    void *data);

/** Fired for every detected unconfined-air segment.
 *  in_pt / out_pt are the entry and exit of the unconfined segment. */
typedef void (*analyze_unconf_air_render_fn)(
    const char *r1, const char *r2,
    point_t in_pt, point_t out_pt,
    void *data);


/**
 * Configuration for a geometry analysis session.
 *
 * Zero-initialise with ANALYZE_CONFIG_INIT_ZERO and set only the fields
 * that differ from the defaults; the library fills in sensible values for
 * any field left at zero / NULL.
 */
struct analyze_config {
    /* ---- Sampling ---- */
    int    sampler;           /**< ANALYZE_SAMPLER_* constant (default: TRIPLE_GRID) */
    int    num_views;         /**< views per refinement pass (default: 3) */
    double azimuth_deg;       /**< azimuth for ROTATED / VIEW_PLANE sampler (degrees) */
    double elevation_deg;     /**< elevation for ROTATED / VIEW_PLANE sampler (degrees) */
    double grid_spacing;      /**< initial ray spacing in mm; 0 = auto-compute */
    double grid_spacing_min;  /**< minimum ray spacing (refinement limit) in mm */
    double aspect;            /**< cell width:height ratio (default 1.0) */
    size_t n_crofton_rays;    /**< ray count for CROFTON sampler; 0 = library default */

    /* ---- Grid size override ---- */
    int    grid_width;        /**< fixed grid width in cells; 0 = derive from spacing */
    int    grid_height;       /**< fixed grid height in cells; 0 = same as grid_width */

    /* ---- Sampling detail ---- */
    int    quiet_missed;            /**< suppress "not hit" messages when non-zero */
    double samples_per_model_axis;  /**< minimum samples per model-space axis; 0 = disabled */

    /* ---- VIEW_PLANE sampler parameters ---- */
    double  view_size;  /**< view size in model units; 0 = sampler not configured */
    point_t view_eye;   /**< eye position for VIEW_PLANE sampler */
    quat_t  view_quat;  /**< view orientation quaternion for VIEW_PLANE sampler */

    /* ---- Convergence tolerances ---- */
    double overlap_tol;       /**< minimum overlap depth to report, in mm */
    double volume_tol;        /**< volume convergence tolerance in mm^3; -1 = disabled */
    double mass_tol;          /**< mass convergence tolerance in g; -1 = disabled */
    double surf_area_tol;     /**< surface-area convergence tolerance in mm^2; -1 = disabled */

    /* ---- Material densities ---- */
    const char *density_file; /**< path to density file; NULL = _DENSITIES from database */

    /* ---- Execution ---- */
    int    use_air;           /**< include air-coded regions (default 1) */
    int    ncpu;              /**< parallel CPUs; 0 = use all available */
    size_t required_hits;     /**< minimum ray hits per region to trust result (default 1) */

    /* ---- Output ---- */
    int    verbose;           /**< emit progress messages if non-zero */
    struct bu_vls *log_str;   /**< destination for log output; NULL = standard error */

    /* ---- Runtime limits ---- */
    long   timeout_ms;        /**< wall-clock timeout in milliseconds; 0 = no limit.
                                * When the limit is reached the loop terminates and
                                * returns whatever results have been accumulated so far. */
    double required_digits;   /**< required significant decimal digits of numerical
                                * stability for convergence; 0 = disabled (use only
                                * absolute tolerances).  For each analysis quantity Q
                                * the criterion is: log10(Q_avg / Q_spread) >= required_digits.
                                * Typical values: 2 (1% accuracy), 3 (0.1%), 4 (0.01%). */

    /* ---- Per-pass volume plot file ---- */
    FILE *volume_plot_file;   /**< optional plot3 file for volume visualisation; NULL = none */

    /* ---- Per-event render hooks (presentation-layer callbacks) ---- */
    analyze_overlap_render_fn    overlap_render;       /**< fired per overlap segment */
    void                        *overlap_render_data;
    analyze_gap_render_fn        gap_render;           /**< fired per gap */
    void                        *gap_render_data;
    analyze_adj_air_render_fn    adj_air_render;       /**< fired per adjacent-air event */
    void                        *adj_air_render_data;
    analyze_exp_air_render_fn    exp_air_render;       /**< fired per exposed-air partition */
    void                        *exp_air_render_data;
    analyze_unconf_air_render_fn unconf_air_render;    /**< fired per unconfined-air segment */
    void                        *unconf_air_render_data;
};


/**
 * Return an analyze_config with library-default values.
 *
 * All fields are first zero-initialised; then the non-zero defaults are
 * applied.  Use as:
 *
 *   struct analyze_config cfg = ANALYZE_CONFIG_INIT_ZERO;
 *
 * This is a static inline function (not a designated-initialiser macro) so
 * it compiles correctly in both C99+ and C++03/11/14/17 translation units.
 */
static inline struct analyze_config
analyze_config_defaults(void) __attribute__((unused));
static inline struct analyze_config
analyze_config_defaults(void)
{
    struct analyze_config _c;
    memset(&_c, 0, sizeof(_c));
    _c.sampler       = ANALYZE_SAMPLER_TRIPLE_GRID;
    _c.num_views     = 3;
    _c.aspect        = 1.0;
    _c.volume_tol    = -1.0;
    _c.mass_tol      = -1.0;
    _c.surf_area_tol = -1.0;
    _c.use_air       = 1;
    _c.required_hits = 1;
    return _c;
}
/** Convenience macro: obtain a default-initialised analyze_config. */
#define ANALYZE_CONFIG_INIT_ZERO analyze_config_defaults()

/**
 * Per-region result record returned inside struct analyze_results.
 */
struct analyze_region_result {
    const char *name;     /**< region name (string owned by the rt_i) */
    double volume;        /**< estimated volume in mm^3 */
    double mass;          /**< estimated mass in grams */
    double surf_area;     /**< estimated surface area in mm^2 */
    unsigned long hits;   /**< number of ray-hits recorded */
    point_t centroid;     /**< estimated centroid in model coordinates */
};


/**
 * Per-input-object result record returned inside struct analyze_results.
 *
 * An "object" here is one of the top-level names passed to analyze_run().
 * It may expand to many regions; these fields are the aggregated values
 * across all regions that belong to the object.
 *
 * Fields not requested via the flags argument of analyze_run() are left at
 * their zero-initialised default values.
 */
struct analyze_object_result {
    const char *name;               /**< strdup'd input object name */
    double      volume;             /**< estimated volume in mm^3 */
    double      mass;               /**< estimated mass in grams */
    double      surf_area;          /**< estimated surface area in mm^2 */
    point_t     centroid;           /**< model-space centroid */
    mat_t       moments_of_inertia; /**< 4x4 inertia tensor */
    point_t     bbox_min;           /**< axis-aligned bounding-box minimum */
    point_t     bbox_max;           /**< axis-aligned bounding-box maximum */
};


/**
 * A detected overlap (or gap / air boundary) between two regions.
 * Used in the bu_ptbl lists inside struct analyze_results.
 */
struct analyze_overlap_record {
    const char *region1;     /**< first region name */
    const char *region2;     /**< second region name (may be NULL for single-region events) */
    unsigned long count;     /**< number of overlapping ray segments recorded */
    double max_dist;         /**< maximum depth (mm) of the worst instance */
    point_t coord;           /**< representative coordinate of the deepest overlap */
    double estimated_volume; /**< estimated overlap volume (mm^3): sum of depth*cell_area over all hits */
};


/**
 * Container for all analysis results returned by analyze_run().
 *
 * Allocate via analyze_run(); release with analyze_results_free().
 * Fields not requested via the flags argument of analyze_run() are
 * left at their zero-initialised values.
 */
struct analyze_results {
    /* ---- Global totals ---- */
    double  total_volume;        /**< mm^3 */
    double  total_mass;          /**< grams */
    double  total_surf_area;     /**< mm^2 */
    point_t centroid;            /**< model-space centroid */
    mat_t   moments_of_inertia;  /**< 4x4 inertia tensor (total for all objects) */
    point_t bbox_min;            /**< axis-aligned bounding box minimum */
    point_t bbox_max;            /**< axis-aligned bounding box maximum */
    double  final_grid_spacing;  /**< last grid spacing used before convergence (mm) */
    int     sampler_type;        /**< ANALYZE_SAMPLER_* value used for this run */
    int     is_stochastic;       /**< non-zero if the run used stochastic sampling */

    /* ---- Per-input-object details ---- */
    struct analyze_object_result *objects; /**< array of n_objects entries */
    size_t n_objects;

    /* ---- Per-region details ---- */
    struct analyze_region_result *regions; /**< array of n_regions entries */
    size_t n_regions;

    /* ---- Air boundary lists ---- */
    struct bu_ptbl first_air;   /**< first-air boundary records */
    struct bu_ptbl last_air;    /**< last-air boundary records */

    /* ---- Issue lists (each entry is struct analyze_overlap_record *) ---- */
    struct bu_ptbl overlaps;   /**< geometric overlaps */
    struct bu_ptbl gaps;       /**< gaps between solids */
    struct bu_ptbl adj_air;    /**< adjacent differing-air regions */
    struct bu_ptbl exp_air;    /**< exposed air (air before / after all solids) */
    struct bu_ptbl unconf_air; /**< unconfined air */
};


/**
 * Compute only the axis-aligned bounding box of the named objects.
 *
 * This is cheaper than a full analysis because no rays are shot; the
 * function only performs tree preparation (rt_gettrees + rt_prep).
 *
 * @param dbip         open database instance
 * @param names        NULL-terminated array of object name strings
 * @param num_objects  number of entries in @p names
 * @param bbox_min     output: model-space bounding box minimum (3 doubles)
 * @param bbox_max     output: model-space bounding box maximum (3 doubles)
 * @return 0 on success, -1 on error
 */
ANALYZE_EXPORT extern int
analyze_bbox(struct db_i *dbip, char *names[], int num_objects,
	     point_t bbox_min, point_t bbox_max);


/**
 * Free all memory owned by an analyze_results struct.
 * The struct itself is also freed; the pointer must not be used afterwards.
 *
 * This function frees the strdup'd region1 / region2 strings in every
 * analyze_overlap_record.  Callers that construct analyze_overlap_record
 * entries manually (i.e. not via analyze_run()) must ensure those string
 * fields are heap-allocated.
 */
ANALYZE_EXPORT extern void
analyze_results_free(struct analyze_results *res);


/**
 * Run a geometry analysis session.
 *
 * This is the preferred high-level entry point.  It selects the appropriate
 * sampler backend from cfg->sampler (ANALYZE_SAMPLER_TRIPLE_GRID or
 * ANALYZE_SAMPLER_CROFTON), registers result-capture callbacks internally,
 * and returns a fully populated struct analyze_results.
 *
 * Configuration notes:
 *  - Pass NULL for @p cfg to use library defaults for all settings.
 *  - Set cfg->n_crofton_rays to control the sample count for the Crofton
 *    sampler; 0 means the library default (currently 100 000).
 *  - cfg->density_file is only consulted when ANALYZE_MASS is in @p flags.
 *
 * @param cfg       Analysis configuration (NULL = library defaults).
 * @param dbip      Open database instance.
 * @param names     Array of object names to analyse.
 * @param num_names Number of entries in @p names.
 * @param flags     Bitwise OR of ANALYZE_* flags selecting which metrics to
 *                  compute.  Issue-detection flags (ANALYZE_OVERLAPS etc.)
 *                  populate the corresponding bu_ptbl lists in the result.
 * @return          Heap-allocated result struct (caller frees with
 *                  analyze_results_free()), or NULL on error.
 */
ANALYZE_EXPORT extern struct analyze_results *
analyze_run(const struct analyze_config *cfg, struct db_i *dbip,
	    char *names[], int num_names, int flags);


/*
 *     Overlap specific structures
 */

struct region_pair {
    struct bu_list l;
    union {
	const char *name;
	struct region *r1;
    } r;
    struct region *r2;
    unsigned long count;
    double max_dist;
    vect_t coord;
};

/**
 *     region_pair for gqa
 */
ANALYZE_EXPORT extern struct region_pair *add_unique_pair(struct region_pair *list,
							  struct region *r1,
							  struct region *r2,
							  double dist, point_t pt);

/**
 * Region AABB overlap pair as returned by analyze_overlapping_region_pairs().
 *
 * r1 and r2 are a pair of regions whose bounding boxes intersect.
 * isect_min / isect_max is the intersection AABB of their bounding boxes.
 * Actual geometric overlaps, if any, must lie inside this volume.
 */
struct analyze_region_overlap_pair {
    struct region *r1;
    struct region *r2;
    point_t isect_min;
    point_t isect_max;
};

/**
 * A cluster of mutually-reachable (via AABB overlap) regions.
 *
 * Clusters are the connected components of the AABB-intersection graph:
 * nodes are regions, edges are AABB-intersecting pairs.  Two regions belong
 * to the same cluster if and only if there is a path of AABB-intersecting
 * pairs connecting them (transitive closure, same algorithm as gdiff's
 * cluster_content()).
 *
 * isect_union_min / isect_union_max is the union of all pairwise intersection
 * AABBs within this cluster.  Rays restricted to this volume are guaranteed
 * to cover every candidate geometric overlap in the cluster.
 *
 * The regions bu_ptbl holds struct region* pointers; it is owned by the
 * cluster and freed by analyze_free_overlap_clusters().
 */
struct overlap_cluster {
    struct bu_ptbl regions;      /**< unique region pointers (struct region *) */
    point_t isect_union_min;     /**< union of all pairwise intersection AABBs */
    point_t isect_union_max;
};

/**
 * Use an R-Tree to find all region pairs whose conservative axis-aligned
 * bounding boxes intersect.
 *
 * This is a cheap pre-filter for overlap analysis.  The returned pairs are
 * candidates only: their actual geometry may or may not overlap.  A subsequent
 * ray-sampling pass restricted to each pair's isect_min/isect_max volume will
 * conclusively confirm or deny the geometric overlap.
 *
 * @param rtip         a prepared rt_i (rt_prep_parallel() already called)
 * @param result_pairs bu_ptbl to receive struct analyze_region_overlap_pair*
 *                     pointers; must be uninitialised on entry (will be
 *                     initialised by this function)
 * @return number of candidate pairs found (>= 0), or -1 on error
 */
ANALYZE_EXPORT extern int
analyze_overlapping_region_pairs(struct rt_i *rtip, struct bu_ptbl *result_pairs);

/**
 * Free memory allocated by analyze_overlapping_region_pairs().
 * Calls bu_ptbl_free(pairs) internally.
 */
ANALYZE_EXPORT extern void
analyze_free_region_overlap_pairs(struct bu_ptbl *pairs);

/**
 * Group AABB-intersecting region pairs into clusters via transitive closure.
 *
 * The clusters are the connected components of the pairwise AABB-intersection
 * graph (nodes = regions, edges = AABB-intersecting pairs).  This is the same
 * BFS-over-adjacency-list algorithm used by gdiff's cluster_content().
 *
 * Clustering reduces the number of focused ray-sampling passes from O(pairs)
 * to O(clusters), which can be dramatically smaller when many regions mutually
 * overlap in a dense sub-region of the model.
 *
 * @param pairs    bu_ptbl of struct analyze_region_overlap_pair* (input, owned
 *                 by caller; this function does not free it)
 * @param clusters bu_ptbl to receive struct overlap_cluster* pointers;
 *                 must be uninitialised on entry
 * @return number of clusters formed (>= 0), or -1 on error
 */
ANALYZE_EXPORT extern int
analyze_cluster_overlapping_pairs(struct bu_ptbl *pairs, struct bu_ptbl *clusters);

/**
 * Free memory allocated by analyze_cluster_overlapping_pairs().
 * Calls bu_ptbl_free(clusters) internally.
 */
ANALYZE_EXPORT extern void
analyze_free_overlap_clusters(struct bu_ptbl *clusters);


ANALYZE_EXPORT int
analyze_obj_inside(struct db_i *dbip, const char *outside, const char *inside, fastf_t tol);


ANALYZE_EXPORT int
analyze_find_subtracted(struct bu_ptbl *results, struct rt_wdb *wdbp,
			const char *pbrep, struct rt_gen_worker_vars *pbrep_rtvars,
			const char *curr_comb, struct bu_ptbl *candidates, void *curr_union_data, size_t ncpus);


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
 * for the specified index of region in region table.
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
 * for the specified index of region in region table.
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
 * for the specified index of region in region table.
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
struct current_state * analyze_current_state_init(void);

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

/**
 * Set a wall-clock timeout for the analysis.
 *
 * When the elapsed time since perform_raytracing() was called reaches
 * @p timeout_ms milliseconds the grid-refinement loop terminates and
 * returns whatever partially-converged results have been accumulated so
 * far.  A value of 0 (the default) disables the timeout.
 */
ANALYZE_EXPORT extern void
analyze_set_timeout(struct current_state *context, long timeout_ms);

/**
 * Set a significant-digit convergence criterion.
 *
 * When @p required_digits is positive, the grid-refinement loop stops
 * as soon as every analysis quantity Q satisfies:
 *
 *   log10( Q_average / Q_spread ) >= required_digits
 *
 * where Q_spread is the max−min spread across the three (or more) views
 * in the current pass.  This is an alternative to supplying absolute
 * tolerance values via analyze_set_volume_tolerance() etc.
 *
 * Typical values: 2.0 (≈1% accuracy), 3.0 (0.1%), 4.0 (0.01%).
 * A value of 0.0 (the default) disables this criterion and falls back
 * to absolute tolerances.
 */
ANALYZE_EXPORT extern void
analyze_set_required_digits(struct current_state *context, double required_digits);

__END_DECLS

#endif /* ANALYZE_INFO_H */

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
