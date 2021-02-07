/*                       P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
/** @file private.h
 *
 * The structs and defines in this file should be moved into
 * src/librt/librt_private.h
 */

#ifndef RT_PRIVATE_H
#define RT_PRIVATE_H

#include "common.h"
#include "vmath.h"
#include "rt/application.h"
#include "rt/resource.h"
#include "rt/space_partition.h" /* cutter */
#include "rt/xray.h"

__BEGIN_DECLS

/**
 * Used by rpc.c, ehy.c, epa.c, eto.c and rhc.c to contain
 * forward-linked lists of points.
 */
struct rt_pnt_node {
    point_t p;                  /**< @brief  a point */
    struct rt_pnt_node *next;    /**< @brief  ptr to next pt */
};


/**
 * Internal to shoot.c and bundle.c
 */
struct rt_shootray_status {
    fastf_t             dist_corr;      /**< @brief  correction distance */
    fastf_t             odist_corr;
    fastf_t             box_start;
    fastf_t             obox_start;
    fastf_t             box_end;
    fastf_t             obox_end;
    fastf_t             model_start;
    fastf_t             model_end;
    struct xray         newray;         /**< @brief  closer ray start */
    struct application *ap;
    struct resource *   resp;
    vect_t              inv_dir;      /**< @brief  inverses of ap->a_ray.r_dir */
    vect_t              abs_inv_dir;  /**< @brief  absolute values of inv_dir */
    int                 rstep[3];     /**< @brief  -/0/+ dir of ray in axis */
    const union cutter *lastcut, *lastcell;
    const union cutter *curcut;
    point_t             curmin, curmax;
    int                 igrid[3];     /**< @brief  integer cell coordinates */
    vect_t              tv;           /**< @brief  next t intercept values */
    int                 out_axis;     /**< @brief  axis ray will leave through */
    struct rt_shootray_status *old_status;
    int                 box_num;        /**< @brief  which cell along ray */
};


__END_DECLS

#endif /* RT_PRIVATE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
