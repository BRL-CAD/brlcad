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

#ifndef ANALYZE_PRIVATE_H
#define ANALYZE_PRIVATE_H

struct minimal_partitions {
    fastf_t *solids;
    fastf_t *gaps;
    int cnt;
    struct xray ray;
    int valid;
};


typedef int (*hitfunc_t)(struct application *, struct partition *, struct seg *);
typedef int (*missfunc_t)(struct application *);
typedef int (*overlapfunc_t)(struct application *, struct partition *, struct region *, struct region *, struct partition *);
typedef int (*nextstart_t)(int *);

struct rt_gen_worker_vars {
    struct rt_i *rtip;
    struct resource *resp;
    int rays_cnt;
    const fastf_t *rays;
    hitfunc_t fhit;
    missfunc_t fmiss;
    overlapfunc_t foverlap;
    int step;       /* number of rays to be fired by this worker before calling back */
    int *ind_src;   /* source of starting index */
    nextstart_t nstart; /* function to call to get next starting index */
    void *ptr; /* application specific info */
};

void analyze_gen_worker(int cpu, void *ptr);

/* Returns count of rays in rays array */
int analyze_get_bbox_rays(fastf_t **rays, point_t min, point_t max, struct bn_tol *tol);



typedef struct xray * (*getray_t)(void *ptr);
typedef int *         (*getflag_t)(void *ptr);
void analyze_seg_filter(struct bu_ptbl *segs, getray_t gray, getflag_t gflag, struct rt_i *rtip, struct resource *resp, fastf_t tol);

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
