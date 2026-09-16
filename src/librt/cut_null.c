/*                     C U T _ N U L L . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @addtogroup ray */
/** @{ */
/** @file librt/cut_null.c
 *
 * Null spatial partitioning method.
 *
 * This method intentionally does not subdivide model space.  It keeps a
 * single model-sized CUT_BOXNODE containing the prepared primitive lists
 * assembled by rt_cut_it().  Reusing the existing boxnode leaf keeps the
 * ray-shooting hot path, duplicate suppression, piece handling, dynamic
 * insert/remove support, and infinite-solid handling identical to the
 * normal traversal path while making the spatial partitioning work itself
 * effectively a no-op.
 */
/** @} */

#include "common.h"

#include "raytrace.h"
#include "cut_private.h"


void
rt_cut_null_build(struct rt_i *rtip, const union cutter *root, int UNUSED(ncpu))
{
    RT_CK_RTI(rtip);
    BU_ASSERT(root->cut_type == CUT_BOXNODE);

    rtip->i->rti_CutHead = *root;	/* union copy; dynamic lists move with the root */
    rtip->i->rti_cutlen = rt_ct_piececount(&rtip->i->rti_CutHead);
    rtip->i->rti_cutdepth = 0;

    if (RT_G_DEBUG&RT_DEBUG_CUT) {
	bu_log("Null Space Partitioning: one model-sized cell containing %zu primitive entries\n",
	       rtip->i->rti_cutlen);
    }
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
