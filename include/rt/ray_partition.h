/*               R A Y _ P A R T I T I O N . H
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
/** @addtogroup rt_partition
 * @brief Partitions of a ray.
 *
 * The partition structure contains information about intervals of the ray which pass
 * through geometry.  A partition consists of one or more segments, which are
 * woven together via rt_boolweave to form the final "solid" partitions that describe
 * solid segments of geometry along a given ray.
 *
 */
/** @{ */
/** @file rt/ray_partition.h */

#ifndef RT_RAY_PARTITION_H
#define RT_RAY_PARTITION_H

#include "common.h"
#include "vmath.h"
#include "bu/ptbl.h"
#include "rt/defines.h"
#include "rt/seg.h"

__BEGIN_DECLS

/**
 * Partitions of a ray.  Passed from rt_shootray() into user's a_hit()
 * function.
 *
 * Not changed to a bu_list for backwards compatibility, but you can
 * iterate the whole list by writing:
 *
 * for (BU_LIST_FOR(pp, partition, (struct bu_list *)PartHeadp))
 */

struct partition {
    /* This can be thought of and operated on as a struct bu_list */
    uint32_t            pt_magic;       /**< @brief sanity check */
    struct partition *  pt_forw;        /**< @brief forwards link */
    struct partition *  pt_back;        /**< @brief backwards link */
    struct seg *        pt_inseg;       /**< @brief IN seg ptr (gives stp) */
    struct hit *        pt_inhit;       /**< @brief IN hit pointer */
    struct seg *        pt_outseg;      /**< @brief OUT seg pointer */
    struct hit *        pt_outhit;      /**< @brief OUT hit ptr */
    struct region *     pt_regionp;     /**< @brief ptr to containing region */
    char                pt_inflip;      /**< @brief flip inhit->hit_normal */
    char                pt_outflip;     /**< @brief flip outhit->hit_normal */
    struct region **    pt_overlap_reg; /**< @brief NULL-terminated array of overlapping regions.  NULL if no overlap. */
    struct bu_ptbl      pt_seglist;     /**< @brief all segs in this partition */
};
#define PT_NULL         ((struct partition *)0)

#define RT_CHECK_PT(_p) RT_CK_PT(_p)    /**< @brief compat */
#define RT_CK_PT(_p) BU_CKMAG(_p, PT_MAGIC, "struct partition")
#define RT_CK_PARTITION(_p) BU_CKMAG(_p, PT_MAGIC, "struct partition")
#define RT_CK_PT_HD(_p) BU_CKMAG(_p, PT_HD_MAGIC, "struct partition list head")

/* Macros for copying only the essential "middle" part of a partition struct. */
#define RT_PT_MIDDLE_START      pt_inseg                /**< @brief 1st elem to copy */
#define RT_PT_MIDDLE_END        pt_seglist.l.magic      /**< @brief copy up to this elem (non-inclusive) */
#define RT_PT_MIDDLE_LEN(p) \
    (((char *)&(p)->RT_PT_MIDDLE_END) - ((char *)&(p)->RT_PT_MIDDLE_START))

#define RT_DUP_PT(ip, new, old, res) { \
	GET_PT(ip, new, res); \
	memcpy((char *)(&(new)->RT_PT_MIDDLE_START), (char *)(&(old)->RT_PT_MIDDLE_START), RT_PT_MIDDLE_LEN(old)); \
	(new)->pt_overlap_reg = NULL; \
	bu_ptbl_cat(&(new)->pt_seglist, &(old)->pt_seglist);  }

/** Clear out the pointers, empty the hit list */
#define GET_PT_INIT(ip, p, res) {\
	GET_PT(ip, p, res); \
	memset(((char *) &(p)->RT_PT_MIDDLE_START), 0, RT_PT_MIDDLE_LEN(p)); }

#define GET_PT(ip, p, res) { \
	if (BU_LIST_NON_EMPTY_P(p, partition, &res->re_parthead)) { \
	    BU_LIST_DEQUEUE((struct bu_list *)(p)); \
	    bu_ptbl_reset(&(p)->pt_seglist); \
	} else { \
	    BU_ALLOC((p), struct partition); \
	    (p)->pt_magic = PT_MAGIC; \
	    bu_ptbl_init(&(p)->pt_seglist, 42, "pt_seglist ptbl"); \
	    (res)->re_partlen++; \
	} \
	res->re_partget++; }

#define FREE_PT(p, res) { \
	BU_LIST_APPEND(&(res->re_parthead), (struct bu_list *)(p)); \
	if ((p)->pt_overlap_reg) { \
	    bu_free((void *)((p)->pt_overlap_reg), "pt_overlap_reg");\
	    (p)->pt_overlap_reg = NULL; \
	} \
	res->re_partfree++; }

#define RT_FREE_PT_LIST(_headp, _res) { \
	register struct partition *_pp, *_zap; \
	for (_pp = (_headp)->pt_forw; _pp != (_headp);) { \
	    _zap = _pp; \
	    _pp = _pp->pt_forw; \
	    BU_LIST_DEQUEUE((struct bu_list *)(_zap)); \
	    FREE_PT(_zap, _res); \
	} \
	(_headp)->pt_forw = (_headp)->pt_back = (_headp); \
    }

/** Insert "new" partition in front of "old" partition.  Note order change */
#define INSERT_PT(_new, _old) BU_LIST_INSERT((struct bu_list *)_old, (struct bu_list *)_new)

/** Append "new" partition after "old" partition.  Note arg order change */
#define APPEND_PT(_new, _old) BU_LIST_APPEND((struct bu_list *)_old, (struct bu_list *)_new)

/** Dequeue "cur" partition from doubly-linked list */
#define DEQUEUE_PT(_cur) BU_LIST_DEQUEUE((struct bu_list *)_cur)

/**
 * The partition_list structure - bu_list based structure for holding
 * ray bundles.
 */
struct partition_list {
    struct bu_list l;
    struct application  *ap;
    struct partition PartHeadp;
    struct seg segHeadp;
    void *              userptr;
};


/**
 * Partition bundle.  Passed from rt_shootrays() into user's
 * bundle_hit() function.
 */
struct partition_bundle {
    int hits;
    int misses;
    struct partition_list *list;
    struct application  *ap;
};


/**
 * Return the length of a partition linked list.
 */
RT_EXPORT extern int rt_partition_len(const struct partition *partheadp);


__END_DECLS

#endif /* RT_RAY_PARTITION_H */
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
