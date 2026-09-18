/*                       P I P E _ S H O T L I N E . C
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
/** @file librt/tests/pipe_shotline.c
 *
 * Regression tests for pipe shotline invariance.  Moving a ray start
 * along the same pre-entry line must not change the physical pipe
 * intervals reported by ray tracing.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "raytrace.h"
#include "wdb.h"


#define MAX_INTERVALS 8
#define POINT_TOL 1.0e-5


struct interval_set {
    int count;
    point_t in[MAX_INTERVALS];
    point_t out[MAX_INTERVALS];
};


struct fixture_case {
    const char *name;
    point_t ray_pt;
    vect_t ray_dir;
};


static int
collect_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    struct interval_set *iset = (struct interval_set *)ap->a_uptr;
    struct partition *pp;

    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	if (iset->count >= MAX_INTERVALS) {
	    bu_log("[pipe_shotline] too many intervals\n");
	    return 1;
	}

	VJOIN1(iset->in[iset->count], ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	VJOIN1(iset->out[iset->count], ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
	iset->count++;
    }

    return iset->count > 0;
}


static int
collect_miss(struct application *UNUSED(ap))
{
    return 0;
}


static void
make_ray(struct xray *ray, const point_t pt, const vect_t dir)
{
    memset(ray, 0, sizeof(struct xray));
    ray->magic = RT_RAY_MAGIC;
    VMOVE(ray->r_pt, pt);
    VMOVE(ray->r_dir, dir);
    VUNITIZE(ray->r_dir);
    ray->r_min = 0.0;
    ray->r_max = INFINITY;
}


static int
shoot_intervals(struct rt_i *rtip, const struct xray *ray, struct interval_set *iset)
{
    struct application ap;

    memset(iset, 0, sizeof(struct interval_set));
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_hit = collect_hit;
    ap.a_miss = collect_miss;
    ap.a_uptr = iset;
    ap.a_ray = *ray;

    return rt_shootray(&ap);
}


static int
compare_intervals(const char *label, const struct interval_set *expected, const struct interval_set *got)
{
    int i;

    if (expected->count != got->count) {
	bu_log("[pipe_shotline] %s interval count mismatch: expected=%d got=%d\n",
	       label, expected->count, got->count);
	return -1;
    }

    for (i = 0; i < expected->count; i++) {
	if (DIST_PNT_PNT(expected->in[i], got->in[i]) > POINT_TOL ||
	    DIST_PNT_PNT(expected->out[i], got->out[i]) > POINT_TOL) {
	    bu_log("[pipe_shotline] %s interval %d endpoint mismatch\n", label, i);
	    bu_log("  expected in=(%g %g %g) out=(%g %g %g)\n",
		   V3ARGS(expected->in[i]), V3ARGS(expected->out[i]));
	    bu_log("       got in=(%g %g %g) out=(%g %g %g)\n",
		   V3ARGS(got->in[i]), V3ARGS(got->out[i]));
	    return -1;
	}
    }

    return 0;
}


static int
add_pipe(struct rt_wdb *wdbp, const char *name, int fixture)
{
    struct bu_list head;
    int ret;

    mk_pipe_init(&head);

    if (fixture == 0) {
	point_t p0 = {0.0, 0.0, 0.0};
	point_t p1 = {0.0, 0.0, 16.0};
	mk_add_pipe_pnt(&head, p0, 4.0, 0.0, 5.0);
	mk_add_pipe_pnt(&head, p1, 4.0, 0.0, 5.0);
    } else if (fixture == 1) {
	point_t p0 = {0.0, 0.0, 0.0};
	point_t p1 = {0.0, 0.0, 16.0};
	mk_add_pipe_pnt(&head, p0, 6.0, 2.0, 5.0);
	mk_add_pipe_pnt(&head, p1, 6.0, 2.0, 5.0);
    } else if (fixture == 2) {
	point_t p0 = {0.0, 0.0, 0.0};
	point_t p1 = {0.0, 0.0, 16.0};
	mk_add_pipe_pnt(&head, p0, 6.0, 0.0, 5.0);
	mk_add_pipe_pnt(&head, p1, 3.0, 0.0, 5.0);
    } else {
	point_t p0 = {-10.0, 0.0, 0.0};
	point_t p1 = {0.0, 0.0, 0.0};
	point_t p2 = {0.0, 10.0, 0.0};
	mk_add_pipe_pnt(&head, p0, 3.0, 0.0, 4.0);
	mk_add_pipe_pnt(&head, p1, 3.0, 0.0, 4.0);
	mk_add_pipe_pnt(&head, p2, 3.0, 0.0, 4.0);
    }

    ret = mk_pipe(wdbp, name, &head);
    mk_pipe_free(&head);
    return ret;
}


static struct rt_i *
prep_object(struct db_i *dbip, const char *name)
{
    struct rt_i *rtip = rt_i_create(dbip);

    if (!rtip)
	return RTI_NULL;

    if (rt_gettree(rtip, name) != 0) {
	rt_i_destroy(rtip);
	return RTI_NULL;
    }

    rt_prep_parallel(rtip, 1);
    return rtip;
}


static int
run_origin_invariance(struct db_i *dbip, const struct fixture_case *fc)
{
    static const fastf_t shifts[] = {8.0, 16.0, 24.0};
    struct rt_i *rtip;
    struct xray base_ray;
    struct interval_set expected;
    int failures = 0;
    size_t i;

    rtip = prep_object(dbip, fc->name);
    if (!rtip) {
	bu_log("[pipe_shotline] failed to prep %s\n", fc->name);
	return -1;
    }

    make_ray(&base_ray, fc->ray_pt, fc->ray_dir);
    shoot_intervals(rtip, &base_ray, &expected);
    if (expected.count == 0) {
	bu_log("[pipe_shotline] %s base ray missed\n", fc->name);
	rt_i_destroy(rtip);
	return -1;
    }

    for (i = 0; i < sizeof(shifts) / sizeof(shifts[0]); i++) {
	struct xray shifted_ray;
	struct interval_set got;
	point_t shifted_pt;
	char label[128];

	VJOIN1(shifted_pt, fc->ray_pt, -shifts[i], fc->ray_dir);
	make_ray(&shifted_ray, shifted_pt, fc->ray_dir);
	shoot_intervals(rtip, &shifted_ray, &got);

	snprintf(label, sizeof(label), "%s/shift_%g", fc->name, shifts[i]);
	if (compare_intervals(label, &expected, &got) < 0)
	    failures++;
    }

    rt_i_destroy(rtip);
    return failures ? -1 : 0;
}


static int
build_db(struct db_i **dbip_out, struct rt_wdb **wdbp_out)
{
    struct db_i *dbip = db_open_inmem();
    struct rt_wdb *wdbp;
    int i;

    if (!dbip)
	return -1;

    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	db_close(dbip);
	return -1;
    }

    for (i = 0; i < 4; i++) {
	char name[32];

	snprintf(name, sizeof(name), "pipe_%d.s", i);
	if (add_pipe(wdbp, name, i) < 0) {
	    wdb_close(wdbp);
	    return -1;
	}
    }

    db_update_nref(dbip);
    *dbip_out = dbip;
    *wdbp_out = wdbp;
    return 0;
}


int
main(int argc, char *argv[])
{
    static const struct fixture_case fixtures[] = {
	{"pipe_0.s", {30.0, 0.0, 8.0}, {-1.0, 0.0, 0.0}},
	{"pipe_1.s", {30.0, 0.0, 8.0}, {-1.0, 0.0, 0.0}},
	{"pipe_2.s", {30.0, 0.0, 8.0}, {-1.0, 0.0, 0.0}},
	{"pipe_3.s", {-5.0, 0.0, 20.0}, {0.0, 0.0, -1.0}}
    };
    struct db_i *dbip = DBI_NULL;
    struct rt_wdb *wdbp = RT_WDB_NULL;
    int failures = 0;
    size_t i;

    bu_setprogname(argv[0]);
    (void)argc;

    if (build_db(&dbip, &wdbp) < 0)
	bu_exit(1, "[pipe_shotline] failed to build test database\n");

    for (i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++) {
	if (run_origin_invariance(dbip, &fixtures[i]) < 0)
	    failures++;
    }

    wdb_close(wdbp);
    return failures ? 1 : 0;
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
