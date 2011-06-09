/*                    O V E R L A P S . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "ged.h" /* need GED_SEM_LIST - may have to re-think how to handle SEMS here. */
#include "analyze.h"
#include "bu.h"


struct region_pair *
add_unique_pair(struct region_pair *list, /* list to add into */
		struct region *r1,        /* first region involved */
		struct region *r2,        /* second region involved */
		double dist,              /* distance/thickness metric value */
		point_t pt)               /* location where this takes place */
{
    struct region_pair *rp, *rpair;

    /* look for it in our list */
    bu_semaphore_acquire(GED_SEM_LIST);
    for (BU_LIST_FOR (rp, region_pair, &list->l)) {

	if ((r1 == rp->r.r1 && r2 == rp->r2) || (r1 == rp->r2 && r2 == rp->r.r1)) {
	    /* we already have an entry for this region pair, we
	     * increase the counter, check the depth and update
	     * thickness maximum and entry point if need be and
	     * return.
	     */
	    rp->count++;

	    if (dist > rp->max_dist) {
		rp->max_dist = dist;
		VMOVE(rp->coord, pt);
	    }
	    rpair = rp;
	    goto found;
	}
    }
    /* didn't find it in the list.  Add it */
    rpair = bu_malloc(sizeof(struct region_pair), "region_pair");
    rpair->r.r1 = r1;
    rpair->r2 = r2;
    rpair->count = 1;
    rpair->max_dist = dist;
    VMOVE(rpair->coord, pt);
    list->max_dist ++; /* really a count */

    /* insert in the list at the "nice" place */
    for (BU_LIST_FOR (rp, region_pair, &list->l)) {
	if (bu_strcmp(rp->r.r1->reg_name, r1->reg_name) <= 0)
	    break;
    }
    BU_LIST_INSERT(&rp->l, &rpair->l);
 found:
    bu_semaphore_release(GED_SEM_LIST);
    return rpair;
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
