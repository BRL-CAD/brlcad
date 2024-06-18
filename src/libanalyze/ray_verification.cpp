/*            R A Y _ V E R I F I C A T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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
/** @file ray_verification.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include <string.h> /* for memset */

#include "vmath.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bn/mat.h"
#include "bu/time.h"
#include "raytrace.h"
#include "analyze.h"
#include "./analyze_private.h"

static int
_hit_noop(struct application *UNUSED(ap),
	struct partition *PartHeadp,
	struct seg *UNUSED(segs))
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    return 0;
}

static int
_miss_noop(struct application *UNUSED(ap))
{
    return 0;
}

static int
_overlap_noop(struct application *UNUSED(ap),
	struct partition *UNUSED(pp),
	struct region *UNUSED(reg1),
	struct region *UNUSED(reg2),
	struct partition *UNUSED(hp))
{
    return 0;
}

class part {
    public:
	std::string region;
	double in_dist;
	point_t in;
	vect_t innorm;
	double out_dist;
	point_t out;
	vect_t outnorm;
};

class shot_results {
    public:
	bool usable = true;
	std::vector<part> parts;
};

class rv_worker_data {
    public:
	rv_worker_data(struct rt_i *l, struct rt_i *r, struct resource *lr, struct resource *rr);
	~rv_worker_data();

	void shoot(ind);

	struct application l_ap;
	struct application r_ap;

	shot_results l_parts;
	shot_results r_parts;

	const std::unordered_set<int> differing_shotlines;
};

rv_worker_data::rv_worker_data(struct rt_i *l, struct rt_i *r, struct resource *lr, struct resource *rr)
{
    RT_APPLICATION_INIT(&l_ap);
    l_ap.a_onehit = 0;
    l_ap.a_rt_i = l;             /* application uses this instance */
    l_ap.a_hit = _hit_noop;         /* where to go on a hit */
    l_ap.a_miss = _miss_noop;       /* where to go on a miss */
    l_ap.a_overlap = _overlap_noop; /* where to go if an overlap is found */
    l_ap.a_onehit = 0;              /* whether to stop the raytrace on the first hit */
    l_ap.a_resource = lr;
    l_ap.a_uptr = (void *)&l_parts;

    RT_APPLICATION_INIT(&r_ap);
    r_ap.a_onehit = 0;
    r_ap.a_rt_i = r;             /* application uses this instance */
    r_ap.a_hit = _hit_noop;         /* where to go on a hit */
    r_ap.a_miss = _miss_noop;       /* where to go on a miss */
    r_ap.a_overlap = _overlap_noop; /* where to go if an overlap is found */
    r_ap.a_onehit = 0;              /* whether to stop the raytrace on the first hit */
    r_ap.a_resource = rr;
    r_ap.a_uptr = (void *)&r_parts;

}

rv_worker_data::~rv_worker_data()
{
}

rv_worker_data::shoot(int ind)
{

}

/* 0 = all differences <= max_len, 1 = difference > max_len */
int
analyze_ray_verify(
	struct bu_vls *msg, const char *lfile,
	struct db_i *ldbip, const char *lobj,
	struct db_i *rdbip, const char *robj,
	fastf_t max_len
	)
{
    if (!ldbip || !lobj || !rdbip || !robj)
	return 0;

    struct directory *ldp = db_lookup(ldbip, lobj, LOOKUP_QUIET);
    struct directory *rdp = db_lookup(rdbip, robj, LOOKUP_QUIET);
    if (!ldp || !rdp)
	return 0;

    struct rt_i *l_rtip = rt_new_rti(ldbip);
    rt_gettree(l_rtip, lobj);
    rt_prep(l_rtip);

    struct rt_i *r_rtip = rt_new_rti(rdbip);
    rt_gettree(r_rtip, robj);
    rt_prep(r_rtip);

    size_t ncpus = bu_avail_cpus();
    struct resource *l_resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "left resources");
    struct resource *r_resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "right resources");

    // TODO - will need to filter out any shots that lint would reject due to bad triangles
    // or unexpected hit/miss...

    // TODO - will need to be able to skip partitions where the in/out normals indicate grazing - there
    // could potentially be long partitions along grazing hits lost to facetization that don't indicate
    // an error.
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

