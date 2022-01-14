/*                          S O L T A B . H
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
/** @addtogroup rt_soltab
 * @brief The LIBRT Solids Table
 */
/** @{ */
/** @file soltab.h */

#ifndef RT_SOLTAB_H
#define RT_SOLTAB_H

#include "common.h"
#include "vmath.h"
#include "bu/magic.h"
#include "bu/list.h"
#include "bu/ptbl.h"
#include "rt/defines.h"
#include "rt/db_fullpath.h"

__BEGIN_DECLS

/**
 * Macros to operate on Right Rectangular Parallelepipeds (RPPs).
 * TODO: move to vmath.h
 */
struct bound_rpp {
    point_t min;
    point_t max;
};

struct rt_functab;  /* forward declaration */
struct directory;   /* forward declaration */

/**
 * Internal information used to keep track of solids in the model.
 * Leaf name and Xform matrix are unique identifier.  Note that all
 * objects store dimensional values in millimeters (mm).
 */
struct soltab {
    struct bu_list              l;              /**< @brief links, headed by rti_headsolid */
    struct bu_list              l2;             /**< @brief links, headed by st_dp->d_use_hd */
    const struct rt_functab *   st_meth;        /**< @brief pointer to per-solid methods */
    struct rt_i *               st_rtip;        /**< @brief "up" pointer to rt_i */
    long                        st_uses;        /**< @brief Usage count, for instanced solids */
    int                         st_id;          /**< @brief Solid ident */
    point_t                     st_center;      /**< @brief Centroid of solid */
    fastf_t                     st_aradius;     /**< @brief Radius of APPROXIMATING sphere */
    fastf_t                     st_bradius;     /**< @brief Radius of BOUNDING sphere */
    void *                      st_specific;    /**< @brief -> ID-specific (private) struct */
    const struct directory *    st_dp;          /**< @brief Directory entry of solid */
    point_t                     st_min;         /**< @brief min X, Y, Z of bounding RPP */
    point_t                     st_max;         /**< @brief max X, Y, Z of bounding RPP */
    long                        st_bit;         /**< @brief solids bit vector index (const) */
    struct bu_ptbl              st_regions;     /**< @brief ptrs to regions using this solid (const) */
    matp_t                      st_matp;        /**< @brief solid coords to model space, NULL=identity */
    struct db_full_path         st_path;        /**< @brief path from region to leaf */
    /* Experimental stuff for accelerating "pieces" of solids */
    long                        st_npieces;     /**< @brief #  pieces used by this solid */
    long                        st_piecestate_num; /**< @brief re_pieces[] subscript */
    struct bound_rpp *          st_piece_rpps;  /**< @brief bounding RPP of each piece of this solid */
};
#define st_name         st_dp->d_namep
#define RT_SOLTAB_NULL  ((struct soltab *)0)
#define SOLTAB_NULL     RT_SOLTAB_NULL          /**< @brief backwards compat */

#define RT_CHECK_SOLTAB(_p) BU_CKMAG(_p, RT_SOLTAB_MAGIC, "struct soltab")
#define RT_CK_SOLTAB(_p) BU_CKMAG(_p, RT_SOLTAB_MAGIC, "struct soltab")

/**
 * Decrement use count on soltab structure.  If no longer needed,
 * release associated storage, and free the structure.
 *
 * This routine semaphore protects against other copies of itself
 * running in parallel, and against other routines (such as
 * _rt_find_identical_solid()) which might also be modifying the
 * linked list heads.
 *
 * Called by -
 * db_free_tree()
 * rt_clean()
 * rt_gettrees()
 * rt_kill_deal_solid_refs()
 */
RT_EXPORT extern void rt_free_soltab(struct soltab *stp);

/* Print a soltab */
RT_EXPORT extern void rt_pr_soltab(const struct soltab *stp);


__END_DECLS

#endif /* RT_SOLTAB_H */
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
