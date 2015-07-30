/*               A N A L Y Z E _ P R I V A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
/** @file libanalyze/analyze_private.h
 *
 */
#include "common.h"
#include "bu/ptbl.h"

#include "raytrace.h"
#include "analyze.h"

#ifndef ANALYZE_PRIVATE_H
#define ANALYZE_PRIVATE_H

struct minimal_partitions {
    fastf_t *hits;
    int hit_cnt;
    fastf_t *gaps;
    int gap_cnt;
    struct xray ray;
    int index;
    int valid;
};


void analyze_gen_worker(int cpu, void *ptr);

/* Returns count of rays in rays array */
int analyze_get_bbox_rays(fastf_t **rays, point_t min, point_t max, struct bn_tol *tol);

int
analyze_get_solid_partitions(struct bu_ptbl *results, struct rt_gen_worker_vars *pstate, const fastf_t *rays, int ray_cnt,
	        struct db_i *dbip, const char *obj, struct bn_tol *tol, int ncpus, int filter);


typedef struct xray * (*getray_t)(void *ptr);
typedef int *         (*getflag_t)(void *ptr);
void analyze_seg_filter(struct bu_ptbl *segs, getray_t gray, getflag_t gflag, struct rt_i *rtip, struct resource *resp, fastf_t tol, int ncpus);

#endif /* ANALYZE_PRIVATE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
