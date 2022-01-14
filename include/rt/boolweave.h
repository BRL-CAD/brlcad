/*                      B O O L W E A V E . H
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
/** @addtogroup rt_boolweave
 * @brief Boolean weaving of raytracing segments
 */
/** @{ */
/** @file rt/boolweave.h */

#ifndef RT_BOOLWEAVE_H
#define RT_BOOLWEAVE_H

#include "common.h"
#include "vmath.h"
#include "bu/bitv.h"
#include "bu/ptbl.h"
#include "rt/application.h"
#include "rt/resource.h"
#include "rt/seg.h"
#include "rt/ray_partition.h"

__BEGIN_DECLS

/*****************************************************************
 *                                                               *
 *  Internal routines in the RT library.                         *
 *  These routines are *not* intended for Applications to use.   *
 *  The interface to these routines may change significantly     *
 *  from release to release of this software.                    *
 *                                                               *
 *****************************************************************/

/**
 * @brief
 * Weave segs into partitions
 *
 * Weave a chain of segments into an existing set of partitions.  The
 * edge of each partition is an inhit or outhit of some solid (seg).
 *
 * NOTE: When the final partitions are completed, it is the user's
 * responsibility to honor the inflip and outflip flags.  They can not
 * be flipped here because an outflip=1 edge and an inflip=0 edge
 * following it may in fact be the same edge.  This could be dealt
 * with by giving the partition struct a COPY of the inhit and outhit
 * rather than a pointer, but that's more cycles than the neatness is
 * worth.
 *
 * Inputs -
 * Pointer to first segment in seg chain.
 * Pointer to head of circular doubly-linked list of
 * partitions of the original ray.
 *
 * Outputs -
 * Partitions, queued on doubly-linked list specified.
 *
 * Notes -
 * It is the responsibility of the CALLER to free the seg chain, as
 * well as the partition list that we return.
 */
RT_EXPORT extern void rt_boolweave(struct seg *out_hd,
				   struct seg *in_hd,
				   struct partition *PartHeadp,
				   struct application *ap);

/**
 * @brief
 * Eval booleans over partitions
 *
 * Consider each partition on the sorted & woven input partition list.
 * If the partition ends before this box's start, discard it
 * immediately.  If the partition begins beyond this box's end,
 * return.
 *
 * Next, evaluate the boolean expression tree for all regions that
 * have some presence in the partition.
 *
 * If 0 regions result, continue with next partition.
 *
 * If 1 region results, a valid hit has occurred, so transfer the
 * partition from the Input list to the Final list.
 *
 * If 2 or more regions claim the partition, then an overlap exists.
 *
 * If the overlap handler gives a non-zero return, then the
 * overlapping partition is kept, with the region ID being the first
 * one encountered.
 *
 * Otherwise, the partition is eliminated from further consideration.
 *
 * All partitions in the indicated range of the ray are evaluated.
 * All partitions which really exist (booleval is true) are appended
 * to the Final partition list.  All partitions on the Final partition
 * list have completely valid entry and exit information, except for
 * the last partition's exit information when a_onehit!=0 and a_onehit
 * is odd.
 *
 * The flag a_onehit is interpreted as follows:
 *
 * If a_onehit = 0, then the ray is traced to +infinity, and all hit
 * points in the final partition list are valid.
 *
 * If a_onehit != 0, the ray is traced through a_onehit hit points.
 * (Recall that each partition has 2 hit points, entry and exit).
 * Thus, if a_onehit is odd, the value of pt_outhit.hit_dist in the
 * last partition may be incorrect; this should not matter because the
 * application specifically said it only wanted pt_inhit there.  This
 * is most commonly seen when a_onehit = 1, which is useful for
 * lighting models.  Not having to correctly determine the exit point
 * can result in a significant savings of computer time.
 *
 * If a_onehit is negative, it indicates the number of non-air hits
 * needed.
 *
 * Returns -
 * 0 If more partitions need to be done
 * 1 Requested number of hits are available in FinalHdp
 *
 * The caller must free whatever is in both partition chains.
 *
 * NOTES for code improvements -
 *
 * With a_onehit != 0, it is difficult to stop at the 'enddist' value
 * (or the a_ray_length value), and always get correct results.  Need
 * to take into account some additional factors:
 *
 * 1) A region shouldn't be evaluated until all its solids have been
 * intersected, to prevent the "CERN" problem of out points being
 * wrong because subtracted solids aren't intersected yet.
 *
 * Maybe "all" solids don't have to be intersected, but some strong
 * statements are needed along these lines.
 *
 * A region is definitely ready to be evaluated IF all its solids
 * have been intersected.
 *
 * 2) A partition shouldn't be evaluated until all the regions within
 * it are ready to be evaluated.
 */
RT_EXPORT extern int rt_boolfinal(struct partition *InputHdp,
				  struct partition *FinalHdp,
				  fastf_t startdist,
				  fastf_t enddist,
				  struct bu_ptbl *regionbits,
				  struct application *ap,
				  const struct bu_bitv *solidbits);

/**
 * Increase the size of re_boolstack to double the previous size.
 * Depend on bu_realloc() to copy the previous data to the new area
 * when the size is increased.
 *
 * Return the new pointer for what was previously the last element.
 */
RT_EXPORT extern void rt_bool_growstack(struct resource *res);

__END_DECLS

#endif /* RT_BOOLWEAVE_H */

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
