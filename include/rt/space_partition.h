/*                 S P A C E _ P A R T I T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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
/** @file space_partition.h
 *
 */

#ifndef RT_SPACE_PARTITION_H
#define RT_SPACE_PARTITION_H

#include "common.h"
#include "vmath.h"
#include "rt/soltab.h"

__BEGIN_DECLS

struct rt_piecelist;

/**
 * Structures for space subdivision.
 *
 * cut_type is an integer for efficiency of access in rt_shootray() on
 * non-word addressing machines.
 *
 * If a solid has 'pieces', it will be listed either in bn_list
 * (initially), or in bn_piecelist, but not both.
 */
struct cutnode {
    int         cn_type;
    int         cn_axis;                /**< @brief 0, 1, 2 = cut along X, Y, Z */
    fastf_t             cn_point;       /**< @brief cut through axis==point */
    union cutter *      cn_l;           /**< @brief val < point */
    union cutter *      cn_r;           /**< @brief val >= point */
};

struct boxnode {
    int         bn_type;
    fastf_t             bn_min[3];
    fastf_t             bn_max[3];
    struct soltab **bn_list;            /**< @brief bn_list[bn_len] */
    size_t              bn_len;         /**< @brief # of solids in list */
    size_t              bn_maxlen;      /**< @brief # of ptrs allocated to list */
    struct rt_piecelist *bn_piecelist;  /**< @brief [] solids with pieces */
    size_t              bn_piecelen;    /**< @brief # of piecelists used */
    size_t              bn_maxpiecelen; /**< @brief # of piecelists allocated */
};

struct nu_axis {
    fastf_t     nu_spos;        /**< @brief cell start position */
    fastf_t     nu_epos;        /**< @brief cell end position */
    fastf_t     nu_width;       /**< @brief voxel size (end - start) */
};

struct nugridnode {
    int nu_type;
    struct nu_axis *nu_axis[3];
    int nu_cells_per_axis[3];   /**< @brief number of slabs */
    int nu_stepsize[3];         /**< @brief number of cells to jump for one step in each axis */
    union cutter *nu_grid;      /**< @brief 3-D array of boxnodes */
};

#define CUT_CUTNODE     1
#define CUT_BOXNODE     2
#define CUT_NUGRIDNODE  3
#define CUT_MAXIMUM     3
union cutter {
    int cut_type;
    union cutter *cut_forw;     /**< @brief Freelist forward link */
    struct cutnode cn;
    struct boxnode bn;
    struct nugridnode nugn;
};


#define CUTTER_NULL     ((union cutter *)0)


__END_DECLS

#endif /* RT_SPACE_PARTITION_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
