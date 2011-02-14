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


#if 0

/* Playing with some ideas on overlap storage, analysis, etc. */

struct overlap_segment {
    struct bu_list *nextseg;
    vect_t ihit;
    vect_t ohit;
}

struct overlap_instance {
    struct region *reg1; /* first region encountered */
    struct region *reg2; /* second region encountered */
    double maxdepth;    /* maximum overlap depth for this overlap instance */
    int hitcount;        /* number of ray segments that have intersected
			    this overlap instance */
    struct bu_list segments; /* List of overlap segments associated with this 
				pair of regions. */
}

struct overlap_set {
    struct bu_hash_tbl *overlaps;
    int count; /* total number of overlap instances */
    struct bu_list sorted_by_maxdepth;
    struct bu_list sorted_by_hitcount;
}

void overlap_key(struct region *reg1, struct region *reg2, struct bu_vls *key, int order) {
    bu_vls_trunc(key, 0);
    if (order > 0) {
        bu_vls_printf(key, "%s%s", reg1->reg_name, reg2->reg_name);
    } else {
	bu_vls_printf(key, "%s%s", reg2->reg_name, reg1->reg_name);
    }
}

int add_segment(struct xray *rp, struct hit *ihitp, struct hit *outp, struct region *reg1, 
	struct region *reg2, fastf_t overlap_tolerance, struct overlap_set *overlaps, 
	struct bu_vls *str) {
    vect_t ihit, ohit;
    double depth;
    unsigned long index;
    struct bu_hash_entry *prev, *entry;
    struct overlap_instance *overlap;
    int status;
    int order = bu_strcmp(reg1->reg_name, reg2->reg_name);

    /* If we somehow got the same region name in both regions, ignore the segment */
    if (!order) return 0; 
    
    /* Translate ray hit information into segment length */
    VJOIN1( ihit, rp->r_pt, ihitp->hit_dist, rp->r_dir );
    VJOIN1( ohit, rp->r_pt, ohitp->hit_dist, rp->r_dir );
    depth = ohitp->hit_dist - ihitp->hit_dist;
    
    /* If depth is so small as to be within floating point tolerance, don't report it */
    if (ZERO(depth))
	return 0;
    
    /* Filter based on overlap tolerance, if any */
    if ((overlap_tolerance > 0) && (depth < overlap_tolerance))
	return 0;
    
    /* Check for segment in existing overlap set, if any have already been found.
     * This part of the code needs to be prepared to run in parallel.*/
    bu_semaphore_acquire( BU_SEM_SYSCALL );
    /* Use the two region names to generate a key for the hash table */
    overlap_key(reg1, reg2, str, order);
    /* See if it already exists in the table */
    entry = bu_find_hash_entry(overlaps->overlaps, (unsigned char *)bu_vls_addr(str), bu_vls_strlen((const)str), &prev, &index);
    while (entry != NULL) {
       overlap = (struct overlap_instance *)bu_get_hash_value(entry);	
       if (reg1 == overlap->reg1 && reg2 == overlap->reg2) || (reg1 == overlap->reg2 && reg2 == overlap->reg1) {
	   /* Found pre-existing instance of this overlap - do numerical updates and return*/
	   overlap->hitcount++;
	   if (overlap->maxdepth < depth) overlap->maxdepth = depth;
	   /* append segment to list - figure this out */
	   bu_semaphore_release( BU_SEM_SYSCALL );
	   return 1;
       } 	   
    }
    /* If we're here we have a new overlap, need to create a new entry */
    overlaps->count++; 
    overlap = bu_malloc(sizeof(struct overlap_instance), "New overlap instance");
    BU_GETSTRUCT(overlap->segments, overlap_segment);
    BU_LIST_INIT(&(overlap->segments->l));
    if (order > 0) {
    	overlap->reg1 = reg1;
	overlap->reg2 = reg2;
    } else {
	overlap->reg1 = reg2;
	overlap->reg2 = reg1;
    }
    overlap->maxdepth = depth;
    overlap->hitcount = 1;
    /* Having create it, add it to the hash table */
    entry = bu_hash_add_entry(overlaps, (unsigned char *)bu_vls_addr(str), bu_vls_strlen((const)str), &status);
    bu_set_hash_value(entry, (unsigned char *)overlap);
    bu_semaphore_release( BU_SEM_SYSCALL );
    return 2;
}
#endif	
    
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
