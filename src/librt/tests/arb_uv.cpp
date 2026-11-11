/*              A R B _ U V . C P P
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
#include "common.h"

#include <array>
#include <cmath>
#include <condition_variable>
#include <mutex>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/parallel.h"
#include "raytrace.h"
#include "wdb.h"

namespace {
#ifdef PARALLEL
constexpr size_t WorkerCount = 8;
#else
constexpr size_t WorkerCount = 1;
#endif
constexpr size_t QueriesPerWorker = 128;
constexpr double UVTolerance = 1.0e-9;

struct Queries {
    struct application application;
    struct soltab *solid;
    struct hit hit;
    std::mutex mutex;
    std::condition_variable ready;
    size_t arrived = 0;
    std::array<bool, WorkerCount> passed = {};
};

void
query_uv(int, void *data)
{
    auto &queries = *static_cast<Queries *>(data);
    size_t worker;
    {
	std::unique_lock<std::mutex> lock(queries.mutex);
	worker = queries.arrived++;
	if (queries.arrived == WorkerCount)
	    queries.ready.notify_all();
	else
	    queries.ready.wait(lock, [&queries] { return queries.arrived == WorkerCount; });
    }
    auto application = queries.application;
    auto hit = queries.hit;
    bool ok = true;
    for (size_t i = 0; i < QueriesPerWorker; ++i) {
	// Normal queries must remain safe during another worker's first UV call.
	application.a_ray.r_pt[X] = i % 2 ? 5.0 : -5.0;
	queries.solid->st_meth->ft_norm(&hit, queries.solid, &application.a_ray);
	struct uvcoord uv = {NAN, NAN, NAN, NAN};
	const int status = rt_obj_uv(&application, queries.solid, &hit, &uv);
	const double expected_v = i % 2 ? 0.25 : 0.75;
	ok = status == 0 && std::isfinite(uv.uv_u) && std::isfinite(uv.uv_v) &&
	    std::isfinite(uv.uv_du) && std::isfinite(uv.uv_dv) &&
	    NEAR_EQUAL(uv.uv_u, 0.5, UVTolerance) &&
	    NEAR_EQUAL(uv.uv_v, expected_v, UVTolerance) && ok;
    }
    queries.passed[worker] = ok;
}
} // namespace

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1) return 1;
    struct db_i *db = db_open_inmem();
    if (!db) return 1;
    struct rt_wdb *writer = wdb_dbopen(db, RT_WDB_TYPE_DB_INMEM);
    const point_t minimum = {-10, -10, -10}, maximum = {10, 10, 10};
    bool ok = mk_rpp(writer, "box.s", minimum, maximum) == 0;
    struct rt_i *rtip = rt_i_create(db);
    ok = ok && rt_gettree(rtip, "box.s") == 0;
    if (ok) rt_prep_parallel(rtip, 1);
    struct resource resource = RT_RESOURCE_INIT_ZERO;
    rt_init_resource(&resource, 0, rtip);
    Queries queries;
    RT_APPLICATION_INIT(&queries.application);
    queries.application.a_rt_i = rtip;
    queries.application.a_resource = &resource;
    queries.application.a_ray.magic = RT_RAY_MAGIC;
    VSET(queries.application.a_ray.r_pt, 0, 0, 20);
    VSET(queries.application.a_ray.r_dir, 0, 0, -1);
    queries.solid = rt_find_solid(rtip, "box.s");
    struct seg head;
    BU_LIST_INIT(&head.l);
    ok = ok && queries.solid && queries.solid->st_meth->ft_shot(
	queries.solid, &queries.application.a_ray, &queries.application, &head) > 0;
    if (ok && !BU_LIST_IS_EMPTY(&head.l)) {
	queries.hit = BU_LIST_FIRST(seg, &head.l)->seg_in;
	// No mapping has been requested: all workers start with a cold cache.
	bu_parallel(query_uv, WorkerCount, &queries);
	for (bool passed : queries.passed) ok = passed && ok;
    } else {
	ok = false;
    }
    struct seg *segment;
    while (BU_LIST_WHILE(segment, seg, &head.l)) {
	BU_LIST_DEQUEUE(&segment->l);
	RT_FREE_SEG(segment, (&resource));
    }
    rt_i_destroy(rtip);
    rt_clean_resource_basic(nullptr, &resource);
    wdb_close(writer);
    if (!ok) bu_log("Concurrent ARB UV queries failed\n");
    return ok ? 0 : 1;
}
