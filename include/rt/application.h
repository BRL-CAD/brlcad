/*                   A P P L I C A T I O N . H
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
/** @addtogroup rt_application
 * @brief
 * This structure is the only parameter to rt_shootray() and holds
 * information about how the ray-casting should be performed.
 */
/** @{ */
/** @file rt/application.h */

#ifndef RT_APPLICATION_H
#define RT_APPLICATION_H

#include "common.h"
#include "vmath.h"
#include "bu/magic.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bn/tabdata.h"
#include "rt/defines.h"
#include "rt/ray_partition.h"
#include "rt/region.h"
#include "rt/resource.h"
#include "rt/seg.h"
#include "rt/xray.h"

__BEGIN_DECLS

struct rt_i; /* forward declaration */

/**
 * This structure is the only parameter to rt_shootray().  The entire
 * structure should be zeroed (e.g. by memset) before it is used the
 * first time.
 *
 * When calling rt_shootray(), these fields are mandatory:
 *
 * Field       | Description
 * ----------- | ---------------------------------------------------
 * a_ray.r_pt  | Starting point of ray to be fired
 * a_ray.r_dir | UNIT VECTOR with direction to fire in (dir cosines)
 * a_hit()     | Routine to call when something is hit
 * a_miss()    | Routine to call when ray misses everything
 * a_rt_i      | The current struct rt_i instance, which must be set to the value returned by rt_dirbuild().
 *
 * In addition, these fields are used by the library.  If they are set
 * to zero, default behavior will be used.
 *
 * Field            | Description
 * ---------------- | ---------------------------------------------------
 * a_resource       | Pointer to CPU-specific resources.  Multi-CPU only.
 * a_overlap()      | DEPRECATED, set a_multioverlap() instead.
 *                    If non-null, this routine will be called to
 *                    handle overlap conditions.  See librt/bool.c
 *                    for calling sequence.
 *                    Return of 0 eliminates partition with overlap entirely
 *                    Return of !0 retains one partition in output
 * a_multioverlap() | Called when two or more regions overlap in a partition.
 *                    Default behavior used if pointer not set.
 *                    See librt/bool.c for calling sequence.
 * a_level          | Printed by librt on errors, but otherwise not used.
 * a_x              | Printed by librt on errors, but otherwise not used.
 * a_y              | Printed by librt on errors, but otherwise not used.
 * a_purpose        | Printed by librt on errors, but otherwise not used.
 * a_rbeam          | Used to compute beam coverage on geometry,
 * a_diverge        | for spline subdivision & many UV mappings.
 *
 *  Note that rt_shootray() returns the (int) return of the
 *  a_hit()/a_miss() function called, as well as placing it in
 *  a_return.  A future "multiple rays at a time" interface will only
 *  provide a_return.
 *
 *  Note that the organization of this structure, and the details of
 *  the non-mandatory elements are subject to change in every release.
 *  Therefore, rather than using compile-time structure
 *  initialization, you should create a zeroed-out structure, and then
 *  assign the intended values at runtime.  A zeroed structure can be
 *  obtained at compile time with "static struct application
 *  zero_ap;", or at run time by using memset(), bu_calloc(), or
 *  BU_ALLOC().
 */
struct application {
    uint32_t a_magic;
    /* THESE ELEMENTS ARE MANDATORY */
    struct xray         a_ray;          /**< @brief  Actual ray to be shot */
    int                 (*a_hit)(struct application *, struct partition *, struct seg *);       /**< @brief  called when shot hits model */
    int                 (*a_miss)(struct application *);        /**< @brief  called when shot misses */
    int                 a_onehit;       /**< @brief  flag to stop on first hit */
    fastf_t             a_ray_length;   /**< @brief  distance from ray start to end intersections */
    struct rt_i *       a_rt_i;         /**< @brief  this librt instance */
    int                 a_zero1;        /**< @brief  must be zero (sanity check) */
    /* THESE ELEMENTS ARE USED BY THE LIBRARY, BUT MAY BE LEFT ZERO */
    struct resource *   a_resource;     /**< @brief  dynamic memory resources */
    int                 (*a_overlap)(struct application *, struct partition *, struct region *, struct region *, struct partition *);   /**< @brief  DEPRECATED */
    void                (*a_multioverlap)(struct application *, struct partition *, struct bu_ptbl *, struct partition *);      /**< @brief  called to resolve overlaps */
    void                (*a_logoverlap)(struct application *, const struct partition *, const struct bu_ptbl *, const struct partition *);      /**< @brief  called to log overlaps */
    int                 a_level;        /**< @brief  recursion level (for printing) */
    int                 a_x;            /**< @brief  Screen X of ray, if applicable */
    int                 a_y;            /**< @brief  Screen Y of ray, if applicable */
    const char *        a_purpose;      /**< @brief  Debug string:  purpose of ray */
    fastf_t             a_rbeam;        /**< @brief  initial beam radius (mm) */
    fastf_t             a_diverge;      /**< @brief  slope of beam divergence/mm */
    int                 a_return;       /**< @brief  Return of a_hit()/a_miss() */
    int                 a_no_booleans;  /**< @brief  1= partitions==segs, no booleans */
    char **             attrs;          /**< @brief  null terminated list of attributes
					 * This list should be the same as passed to
					 * rt_gettrees_and_attrs() */
    int                 a_bot_reverse_normal_disabled;  /**< @brief  1= no bot normals get reversed in BOT_UNORIENTED_NORM */
    /* THESE ELEMENTS ARE USED BY THE PROGRAM "rt" AND MAY BE USED BY */
    /* THE LIBRARY AT SOME FUTURE DATE */
    /* AT THIS TIME THEY MAY BE LEFT ZERO */
    struct pixel_ext *  a_pixelext;     /**< @brief  locations of pixel corners */
    /* THESE ELEMENTS ARE WRITTEN BY THE LIBRARY, AND MAY BE READ IN a_hit() */
    struct seg *        a_finished_segs_hdp;
    struct partition *  a_Final_Part_hdp;
    vect_t              a_inv_dir;      /**< @brief  filled in by rt_shootray(), inverse of ray direction cosines */
    /* THE FOLLOWING ELEMENTS ARE MAINLINE & APPLICATION SPECIFIC. */
    /* THEY SHOULD NEVER BE USED BY THE LIBRARY. */
    int                 a_user;         /**< @brief  application-specific value */
    void *              a_uptr;         /**< @brief  application-specific pointer */
    struct bn_tabdata * a_spectrum;     /**< @brief  application-specific bn_tabdata pointer */
    fastf_t             a_color[3];     /**< @brief  application-specific color */
    fastf_t             a_dist;         /**< @brief  application-specific distance */
    vect_t              a_uvec;         /**< @brief  application-specific vector */
    vect_t              a_vvec;         /**< @brief  application-specific vector */
    fastf_t             a_refrac_index; /**< @brief  current index of refraction */
    fastf_t             a_cumlen;       /**< @brief  cumulative length of ray */
    int                 a_flag;         /**< @brief  application-specific flag */
    int                 a_zero2;        /**< @brief  must be zero (sanity check) */
};

/**
 * This structure is the only parameter to rt_shootrays().  The entire
 * structure should be zeroed (e.g. by memset) before it is used the
 * first time.
 *
 * When calling rt_shootrays(), these fields are mandatory:
 *
 *      - b_ap  Members in this single ray application structure should
 *              be set in a similar fashion as when used with
 *              rt_shootray() with the exception of a_hit() and
 *              a_miss(). Default implementations of these routines
 *              are provided that simple update hit/miss counters and
 *              attach the hit partitions and segments to the
 *              partition_bundle structure. Users can still override
 *              this default functionality but have to make sure to
 *              move the partition and segment list to the new
 *              partition_bundle structure.
 *      - b_hit() Routine to call when something is hit by the ray bundle.
 *      - b_miss() Routine to call when ray bundle misses everything.
 *
 * Note that rt_shootrays() returns the (int) return of the
 * b_hit()/b_miss() function called, as well as placing it in
 * b_return.
 *
 * An integer field b_user and a void *field b_uptr are provided in
 * the structure for custom user data.
 */
struct application_bundle
{
    uint32_t b_magic;
    /* THESE ELEMENTS ARE MANDATORY */
    struct xrays b_rays; /**< @brief  Actual bundle of rays to be shot */
    struct application b_ap; /**< @brief  application setting to be applied to each ray */
    int (*b_hit)(struct application_bundle *, struct partition_bundle *); /**< @brief  called when bundle hits model */
    int (*b_miss)(struct application_bundle *); /**< @brief  called when entire bundle misses */
    int b_user; /**< @brief  application_bundle-specific value */
    void *b_uptr; /**< @brief  application_bundle-specific pointer */
    int b_return;
};


#define RT_APPLICATION_NULL ((struct application *)0)
#define RT_AFN_NULL     ((int (*)(struct application *, struct partition *, struct region *, struct region *, struct partition *))NULL)
#define RT_CK_AP(_p) BU_CKMAG(_p, RT_AP_MAGIC, "struct application")
#define RT_CK_APPLICATION(_p) BU_CKMAG(_p, RT_AP_MAGIC, "struct application")
#define RT_APPLICATION_INIT(_p) { \
	memset((char *)(_p), 0, sizeof(struct application)); \
	(_p)->a_magic = RT_AP_MAGIC; \
    }


#ifdef NO_BOMBING_MACROS
#  define RT_AP_CHECK(_ap) (void)(_ap)
#else
#  define RT_AP_CHECK(_ap)      \
    {if ((_ap)->a_zero1||(_ap)->a_zero2) \
	    bu_bomb("corrupt application struct"); }
#endif

__END_DECLS

#endif /* RT_APPLICATION_H */

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
