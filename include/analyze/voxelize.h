/*                       V O X E L I Z E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
 */
/** @{ */
/** @file analyze/voxelize.h */

#ifndef ANALYZE_VOXELIZE_H
#define ANALYZE_VOXELIZE_H

#include "common.h"
#include "raytrace.h"
#include "analyze/defines.h"

__BEGIN_DECLS

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
 * voxelize function takes raytrace instance and user parameters as inputs
 */
ANALYZE_EXPORT extern void
voxelize(struct rt_i *rtip, fastf_t voxelSize[3], int levelOfDetail, void (*create_boxes)(void *callBackData, int x, int y, int z, const char *regionName, fastf_t percentageFill), void *callBackData);

__END_DECLS

#endif /* ANALYZE_VOXELIZE_H */

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
