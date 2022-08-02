/*                       G L O B A L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @file rt/global.h
 *
 */

#ifndef RT_GLOBAL_H
#define RT_GLOBAL_H

#include "common.h"

#include "rt/wdb.h"
#include "vmath.h"

__BEGIN_DECLS


/**
 * Definitions for librt.a which are global to the library regardless
 * of how many different models are being worked on
 */
struct rt_g {
    /* DEPRECATED:  rtg_parallel is not used by LIBRT any longer (and will be removed) */
    int8_t              rtg_parallel;   /**< @brief  !0 = trying to use multi CPUs */
    struct bu_list      rtg_vlfree;     /**< @brief  head of bv_vlist freelist */
    struct rt_wdb       rtg_headwdb;    /**< @brief  head of database object list */
};
#define RT_G_INIT_ZERO { 0, BU_LIST_INIT_ZERO, RT_WDB_INIT_ZERO }

/**
 * global ray-trace geometry state
 */
RT_EXPORT extern struct rt_g RTG;

/**
 * Applications that are going to use RT_ADD_VLIST and RT_GET_VLIST
 * are required to execute this macro once, first:
 *
 * BU_LIST_INIT(&RTG.rtg_vlfree);
 *
 * Note that RT_GET_VLIST and RT_FREE_VLIST are non-PARALLEL.
 */
#define RT_GET_VLIST(p) BV_GET_VLIST(&RTG.rtg_vlfree, p)

/** Place an entire chain of bv_vlist structs on the freelist */
#define RT_FREE_VLIST(hd) BV_FREE_VLIST(&RTG.rtg_vlfree, hd)

#define RT_ADD_VLIST(hd, pnt, draw) BV_ADD_VLIST(&RTG.rtg_vlfree, hd, pnt, draw)

/** set the transformation matrix to display matrix */
#define RT_VLIST_SET_DISP_MAT(_dest_hd, _ref_pt) BV_VLIST_SET_DISP_MAT(&RTG.rtg_vlfree, _dest_hd, _ref_pt)

/** set the transformation matrix to model matrix */
#define RT_VLIST_SET_MODEL_MAT(_dest_hd) BV_VLIST_SET_MODEL_MAT(&RTG.rtg_vlfree, _dest_hd)

/** Set a point size to apply to the vlist elements that follow. */
#define RT_VLIST_SET_POINT_SIZE(hd, size) BV_VLIST_SET_POINT_SIZE(&RTG.rtg_vlfree, hd, size)

/** Set a line width to apply to the vlist elements that follow. */
#define RT_VLIST_SET_LINE_WIDTH(hd, width) BV_VLIST_SET_LINE_WIDTH(&RTG.rtg_vlfree, hd, width)


__END_DECLS

#endif /* RT_GLOBAL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
