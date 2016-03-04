/*                       A N A L Y Z E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
#include "ged.h"
#include "raytrace.h"

__BEGIN_DECLS

#ifndef ANALYZE_EXPORT
#  if defined(ANALYZE_DLL_EXPORTS) && defined(ANALYZE_DLL_IMPORTS)
#    error "Only ANALYZE_DLL_EXPORTS or ANALYZE_DLL_IMPORTS can be defined, not both."
#  elif defined(ANALYZE_DLL_EXPORTS)
#    define ANALYZE_EXPORT __declspec(dllexport)
#  elif defined(ANALYZE_DLL_IMPORTS)
#    define ANALYZE_EXPORT __declspec(dllimport)
#  else
#    define ANALYZE_EXPORT
#  endif
#endif

/* Libanalyze return codes */
#define ANALYZE_OK 0x0000
#define ANALYZE_ERROR 0x0001 /**< something went wrong, function not completed */
#define ANALYZE_SEM_WORKER GED_SEM_LAST
#define ANALYZE_SEM_STATS ANALYZE_SEM_WORKER+1
#define ANALYZE_SEM_LIST ANALYZE_SEM_STATS+1
#define ANALYZE_SEM_LAST ANALYZE_SEM_LIST+1

/*
 *     Density specific structures
 */

#define DENSITY_MAGIC 0xaf0127

/* the entries in the density table */
struct density_entry {
    uint32_t magic;
    double grams_per_cu_mm;
    char *name;
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

/**
 *     Routine to parse a .density file
 */
ANALYZE_EXPORT extern int parse_densities_buffer(char *buf,
						 size_t len,
						 struct density_entry *densities,
						 struct bu_vls *result_str,
						 int *num_densities);

/**
 *     region_pair for gqa
 */
ANALYZE_EXPORT extern struct region_pair *add_unique_pair(struct region_pair *list,
							  struct region *r1,
							  struct region *r2,
							  double dist, point_t pt);


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

struct raytracing_context {
    int num_objects;
    int num_views;

    unsigned long *shots;

    vect_t span;    /* How much space does the geometry span in each of X, Y, Z directions */
    vect_t area;    /* area of the view for view with invariant at index */

    struct per_obj_data *objs;

    struct density_entry *densities;
    int num_densities;
};

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

ANALYZE_EXPORT extern fastf_t
analyze_volume(struct raytracing_context *context, const char *name);

ANALYZE_EXPORT extern point_t *
analyze_centroid(struct raytracing_context *context, const char *name);

ANALYZE_EXPORT extern fastf_t
analyze_surf_area(struct raytracing_context *context, const char *name);

ANALYZE_EXPORT extern int
analyze_raytracing_context_init(struct raytracing_context *context, struct db_i *dbip,
    const char **names, int *flags);

ANALYZE_EXPORT extern void
analyze_raytracing_context_clear(struct raytracing_context *context);

struct rt_gen_worker_vars {
    struct rt_i *rtip;
    struct resource *resp;
    int rays_cnt;
    const fastf_t *rays;
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
	const char *curr_comb, struct bu_ptbl *candidates, void *curr_union_data, int ncpus);


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
