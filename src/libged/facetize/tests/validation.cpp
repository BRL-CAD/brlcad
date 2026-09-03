/*             F A C E T I Z E _ V A L I D A T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file facetize/tests/validation.cpp
 *
 * Exercise finite and unbounded CSG reference sampling.
 */

#include "common.h"

#include <cmath>
#include <cstdio>

#include "bu/log.h"
#include "raytrace.h"
#include "wdb.h"

#include "../validation.h"

static int failures = 0;

static int
count_log(void *data, void *UNUSED(message))
{
    if (data)
	(*static_cast<int *>(data))++;
    return 0;
}

static void
expect(bool condition, const char *message)
{
    if (condition)
	return;

    std::fprintf(stderr, "FAIL: %s\n", message);
    failures++;
}

int
main()
{
    struct db_i *dbip = db_create_inmem();
    expect(dbip != NULL, "create in-memory database");
    if (!dbip)
	return 1;

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    expect(wdbp != NULL, "open in-memory database for writing");
    if (!wdbp) {
	db_close(dbip);
	return 1;
    }

    point_t center = VINIT_ZERO;
    vect_t normal = {1.0, 0.0, 0.0};
    expect(mk_sph(wdbp, "sphere.s", center, 10.0) == 0,
	    "create bounded test solid");
    expect(mk_half(wdbp, "half.s", normal, 0.0) == 0,
	    "create unbounded test solid");

    struct bu_list members;
    BU_LIST_INIT(&members);
    expect(mk_addmember("sphere.s", &members, NULL, WMOP_UNION) != NULL,
	    "add bounded member");
    expect(mk_addmember("half.s", &members, NULL, WMOP_INTERSECT) != NULL,
	    "add halfspace clipping member");
    expect(mk_comb(wdbp, "clipped.r", &members, 1, NULL, NULL, NULL,
	    0, 0, 0, 0, 0, 0, 0) == 0, "create clipped test region");

    double surface_area = -1.0;
    double volume = -1.0;
    int log_count = 0;
    struct bu_hook_list saved_hooks = BU_HOOK_LIST_INIT_ZERO;
    bu_log_hook_save_all(&saved_hooks);
    bu_log_hook_delete_all();
    bu_log_add_hook(count_log, &log_count);
    long crossings = facetize_csg_metrics(dbip, "half.s", &surface_area,
	    &volume);
    int bound_log_count = log_count;
    bu_log("facetize validation hook restoration probe\n");
    bu_log_hook_delete_all();
    bu_log_hook_restore_all(&saved_hooks);
    bu_hook_delete_all(&saved_hooks);
    expect(crossings < 0,
	    "unbounded halfspace is not assigned Crofton metrics");
    expect(bound_log_count == 0,
	    "expected bounding failure diagnostics are suppressed");
    expect(log_count == 1,
	    "bounding failure log suppression restores existing hooks");

    crossings = facetize_csg_metrics(dbip, "clipped.r", &surface_area,
	    &volume);
    expect(crossings > 0,
	    "bounded geometry clipped by a halfspace is sampled");
    expect(std::isfinite(surface_area) && surface_area > 0.0,
	    "clipped geometry has finite positive surface area");
    expect(std::isfinite(volume) && volume > 0.0,
	    "clipped geometry has finite positive volume");

    db_close(dbip);
    return failures ? 1 : 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
