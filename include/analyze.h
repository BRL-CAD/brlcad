/*                       A N A L Y Z E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2012 United States Government as represented by
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
/** @file analyze.h
 *
 * Functions provided by the LIBANALYZE geometry analysis library.
 *
 */

#ifndef __ANALYZE_H__
#define __ANALYZE_H__

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
    const char *regionName;
    fastf_t regionDistance;
    struct voxelRegion *nextRegion;
};

/**
 * This structure stores the information about voxels provided by a single raytrace.
 */

struct rayInfo {
    fastf_t sizeVoxel[3];
    fastf_t *fillDistances;
    struct voxelRegion *regionList;
};

/**
 * This structure stores the user defined parameters for the voxelization
 */

struct paramVoxelize {
    fastf_t sizeVoxel[3];
    int levelOfDetail;
    fastf_t threshold;
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
 * rt_shootray() was told to call this on a hit.
 *
 * This callback routine utilizes the application structure which
 * describes the current state of the raytrace.
 *
 * This callback routine is provided a circular linked list of
 * partitions, each one describing one in and out segment of one
 * region for each region encountered.
 *
 * The 'segs' segment list is unused in this example.
 */
int
hit(struct application *ap, struct partition *PartHeadp, struct seg*UNUSED(segs));

/**
 * Function to print values to Screen
 */
void
printToScreen(char inout, int x, int y, int z, const char *a, fastf_t fill);

/**
 * Function to print values to File
 */
void
printToFile(char inout, int x, int y, int z, const char *a, fastf_t fill);

/**
 * Function to assign a new region to a voxel.
 */
void
setRegionName(struct bu_vls **vp, const char **nameSource, const char **nameDestination);

/**
 * voxelize function takes raytrace instance and user parameters as inputs
 */
void
voxelize(struct rt_i *rtip, struct paramVoxelize *userParameters, void (*printFunction)(char ch, int x, int y, int z, const char *regionName, fastf_t percentageFill));


__END_DECLS

#endif /* __ANALYZE_H__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
