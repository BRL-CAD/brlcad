/*                      A N I M A T I O N _ L I F E T I M E . C P P
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

#include <initializer_list>
#include <limits>
#include <memory>
#include <string>

#include "bu/app.h"
#include "bu/log.h"
#include "raytrace.h"
#include "vmath.h"
#include "wdb.h"

using RaytracePtr = std::unique_ptr<struct rt_i, decltype(&rt_i_destroy)>;

namespace {
constexpr double Translation = 30.0;
constexpr double MatrixTolerance = 1.0e-9;

struct Transform {
    double x = (std::numeric_limits<double>::quiet_NaN)();
    size_t count = 0;
};

int
collect_transform(struct db_tree_state *state, const struct db_full_path *UNUSED(path),
    const struct rt_comb_internal *UNUSED(combination), void *data)
{
    auto &transform = *static_cast<Transform *>(data);
    transform.x = state->ts_mat[MDX];
    ++transform.count;
    return -1;
}

bool
verify_translation(struct db_i *db, double expected)
{
    struct db_tree_state state;
    RT_DBTS_INIT(&state);
    state.ts_dbip = db;
    const char *object = "group.g";
    Transform transform;
    const bool valid = db_walk_tree(db, 1, &object, 1, &state,
	collect_transform, nullptr, nullptr, &transform) >= 0 &&
	transform.count == 1 && NEAR_EQUAL(transform.x, expected, MatrixTolerance);
    if (!valid)
	bu_log("Animation transform: expected %g, got %g in %zu regions\n",
	    expected, transform.x, transform.count);
    return valid;
}

bool
prepare_geometry(struct rt_i *rtip)
{
    if (rt_gettree(rtip, "group.g") < 0)
	return false;
    rt_prep_parallel(rtip, 1);
    return rtip->stats.nregions == 1 && rtip->stats.nsolids == 1;
}

bool
verify_lifetime(struct db_i *db, const char *path)
{
    RaytracePtr owner(rt_i_create(db), rt_i_destroy);
    const std::string distance = std::to_string(Translation);
    const char *animation[] = {"anim", path, "matrix", "rmul", "translate", distance.c_str(), "0", "0"};
    db_free_anim(db);
    if (!owner || db_parse_anim(db, sizeof(animation) / sizeof(animation[0]), animation) < 0 ||
	!prepare_geometry(owner.get()) || !verify_translation(db, Translation))
	return false;

    rt_clean_geometry(owner.get());
    if (!BU_LIST_IS_EMPTY(&owner->HeadRegion) || owner->stats.nsolids != 0 ||
	owner->stats.nregions != 0 || !owner->needprep || !verify_translation(db, Translation)) {
	bu_log("Geometry invalidation did not retain articulation and clear preparation\n");
	return false;
    }
    {
	RaytracePtr borrower(rt_i_create(db), rt_i_destroy);
	if (!borrower || !prepare_geometry(borrower.get()))
	    return false;
    }
    if (!verify_translation(db, Translation)) {
	bu_log("Destroying a shared raytracing instance removed database animation\n");
	return false;
    }
    rt_clean(owner.get());
    return verify_translation(db, 0.0);
}
} // namespace

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    struct db_i *db = db_open_inmem_ex(DB_OPEN_INMEM_COMB_INSTANCES);
    if (!db)
	return 1;
    std::unique_ptr<struct rt_wdb, decltype(&wdb_close)> writer(
	wdb_dbopen(db, RT_WDB_TYPE_DB_INMEM), wdb_close);
    if (!writer) {
	db_close(db);
	return 1;
    }
    const point_t center = VINIT_ZERO;
    if (mk_sph(writer.get(), "sphere.s", center, 1.0) < 0 ||
	mk_region1(writer.get(), "sphere.r", "sphere.s", nullptr, nullptr, nullptr) < 0)
	return 1;
    struct wmember members;
    BU_LIST_INIT(&members.l);
    if (!mk_addmember("sphere.r", &members.l, nullptr, WMOP_UNION) ||
	mk_lcomb(writer.get(), "group.g", &members, 0, nullptr, nullptr, nullptr, 0) < 0)
	return 1;
    bool valid = true;
    // Node animation applies to the member arc; rooted animation applies to
    // the selected top-level combination.
    for (const char *path : {"sphere.r", "/group.g"}) {
	if (!verify_lifetime(db, path)) {
	    bu_log("Animation lifetime failed for %s\n", path);
	    valid = false;
	}
    }
    return valid ? 0 : 1;
}
