/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include "bu/app.h"
#include "bu/log.h"
#include "raytrace.h"

namespace {

using Intervals = std::vector<std::pair<double, double>>;
constexpr int GRID_SAMPLES = 9;
/* Native exporters print decimal parameters.  Allow rounding at their
 * documented model resolution, without hiding a missing or misplaced part. */
constexpr double DISTANCE_TOLERANCE_MM = 0.0005;

int
hit(struct application *ap, struct partition *head, struct seg *)
{
    auto &intervals = *static_cast<Intervals *>(ap->a_uptr);
    for (struct partition *part = head->pt_forw; part != head; part = part->pt_forw) {
	const double start = part->pt_inhit->hit_dist;
	const double end = part->pt_outhit->hit_dist;
	if (!std::isfinite(start) || !std::isfinite(end) || end < start)
	    bu_exit(1, "Ray comparison received an invalid hit interval\n");
	/* Compare occupied space, not how the two databases partition regions. */
	if (!intervals.empty() && start <= intervals.back().second + DISTANCE_TOLERANCE_MM)
	    intervals.back().second = std::max(intervals.back().second, end);
	else
	    intervals.emplace_back(start, end);
    }
    return 1;
}

int
miss(struct application *)
{
    return 0;
}

struct Model {
    struct rt_i *rtip = nullptr;
    struct resource resource = RT_RESOURCE_INIT_ZERO;
    bool has_brep = false;

    Model(const char *path, const char *root)
    {
	rtip = rt_dirbuild(path, nullptr, 0);
	if (!rtip || rt_gettree(rtip, root))
	    bu_exit(1, "Cannot load test object %s\n", root);
	rt_prep_parallel(rtip, 1);
	rt_init_resource(&resource, 0, rtip);
	struct soltab *solid;
	RT_VISIT_ALL_SOLTABS_START(solid, rtip) {
	    has_brep = has_brep || solid->st_id == ID_BREP;
	} RT_VISIT_ALL_SOLTABS_END;
    }

    ~Model() { rt_i_destroy(rtip); }
    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;

    Intervals shoot(const point_t origin, int axis)
    {
	Intervals intervals;
	struct application ap;
	RT_APPLICATION_INIT(&ap);
	ap.a_rt_i = rtip;
	ap.a_resource = &resource;
	ap.a_hit = hit;
	ap.a_miss = miss;
	ap.a_uptr = &intervals;
	VMOVE(ap.a_ray.r_pt, origin);
	VSETALL(ap.a_ray.r_dir, 0.0);
	ap.a_ray.r_dir[axis] = 1.0;
	rt_shootray(&ap);
	return intervals;
    }
};

}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 5)
	bu_exit(1, "Usage: %s source.g source-object imported.g imported-object\n", argv[0]);
    Model source(argv[1], argv[2]);
    Model imported(argv[3], argv[4]);
    /* BRep preparation may bound untrimmed support surfaces.  Those bounds
     * need not match native primitive bounds even when the occupied space
     * does.  For native-to-native round trips bounds must match as well. */
    if (!source.has_brep && !imported.has_brep &&
	(!VNEAR_EQUAL(source.rtip->mdl_min, imported.rtip->mdl_min, DISTANCE_TOLERANCE_MM) ||
	!VNEAR_EQUAL(source.rtip->mdl_max, imported.rtip->mdl_max, DISTANCE_TOLERANCE_MM)))
	bu_exit(1, "Round-trip bounding box changed: (%g %g %g)-(%g %g %g) -> (%g %g %g)-(%g %g %g)\n",
	    V3ARGS(source.rtip->mdl_min), V3ARGS(source.rtip->mdl_max),
	    V3ARGS(imported.rtip->mdl_min), V3ARGS(imported.rtip->mdl_max));
    point_t lower, upper;
    VMOVE(lower, source.rtip->mdl_min);
    VMOVE(upper, source.rtip->mdl_max);
    VMIN(lower, imported.rtip->mdl_min);
    VMAX(upper, imported.rtip->mdl_max);
    size_t hits = 0;
    for (int axis = 0; axis < 3; ++axis) {
	const int u = (axis + 1) % 3;
	const int v = (axis + 2) % 3;
	for (int row = 0; row < GRID_SAMPLES; ++row) {
	    for (int col = 0; col < GRID_SAMPLES; ++col) {
		point_t origin;
		VMOVE(origin, lower);
		/* Cell centers avoid systematic sampling of edges and tangencies. */
		origin[u] += (upper[u] - origin[u]) * (row + 0.5) / GRID_SAMPLES;
		origin[v] += (upper[v] - origin[v]) * (col + 0.5) / GRID_SAMPLES;
		origin[axis] -= upper[axis] - lower[axis] + 1.0;
		const Intervals expected = source.shoot(origin, axis);
		const Intervals actual = imported.shoot(origin, axis);
		hits += !expected.empty();
		if (expected.size() != actual.size()) {
		    for (const auto &interval : expected)
			bu_log("source: %.17g %.17g\n", interval.first, interval.second);
		    for (const auto &interval : actual)
			bu_log("imported: %.17g %.17g\n", interval.first, interval.second);
		    bu_exit(1, "Round-trip interval count changed at axis %d, cell %d/%d: %zu -> %zu\n",
			axis, row, col, expected.size(), actual.size());
		}
		for (size_t i = 0; i < expected.size(); ++i)
		    if (std::fabs(expected[i].first - actual[i].first) > DISTANCE_TOLERANCE_MM ||
			std::fabs(expected[i].second - actual[i].second) > DISTANCE_TOLERANCE_MM)
			bu_exit(1, "Round-trip ray endpoints changed at axis %d, cell %d/%d: %g/%g -> %g/%g\n",
			    axis, row, col, expected[i].first, expected[i].second, actual[i].first, actual[i].second);
	    }
	}
    }
    if (!hits)
	bu_exit(1, "Round-trip ray comparison did not hit any geometry\n");
    bu_log("Matched %d rays (%zu hits)%s\n", 3 * GRID_SAMPLES * GRID_SAMPLES, hits,
	!source.has_brep && !imported.has_brep ? " and native bounds" : "");
    return 0;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 */
