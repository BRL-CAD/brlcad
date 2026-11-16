/*              I N S T A N C E _ P A T H S . C P P
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
#include <set>
#include <string>

#include "bu/app.h"
#include "bu/log.h"
#include "raytrace.h"
#include "wdb.h"

static int
collect_path(struct db_tree_state *, const struct db_full_path *path,
    const struct rt_comb_internal *, void *data)
{
    auto *paths = static_cast<std::set<std::string> *>(data);
    struct bu_vls name = BU_VLS_INIT_ZERO;
    db_path_to_vls(&name, path);
    paths->insert(bu_vls_cstr(&name));
    bu_vls_free(&name);
    return -1;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    struct db_i *db = db_open_inmem_ex(DB_OPEN_INMEM_COMB_INSTANCES);
    if (!db)
	return 1;
    struct rt_wdb *writer = wdb_dbopen(db, RT_WDB_TYPE_DB_INMEM);
    point_t center = {0, 0, 0};
    struct wmember members;
    bool ok = mk_sph(writer, "sphere.s", center, 1) == 0;
    BU_LIST_INIT(&members.l);
    ok = ok && mk_addmember("sphere.s", &members.l, NULL, WMOP_UNION) &&
	mk_lcomb(writer, "sphere.r", &members, 1, NULL, NULL, NULL, 0) == 0;
    for (const char *parent : {"left.g", "right.g"}) {
	BU_LIST_INIT(&members.l);
	ok = ok && mk_addmember("sphere.r", &members.l, NULL, WMOP_UNION) &&
	    mk_lcomb(writer, parent, &members, 0, NULL, NULL, NULL, 0) == 0;
    }
    BU_LIST_INIT(&members.l);
    ok = ok && mk_addmember("sphere.r", &members.l, NULL, WMOP_UNION) &&
	mk_addmember("sphere.r", &members.l, NULL, WMOP_UNION) &&
	mk_lcomb(writer, "arcs.g", &members, 0, NULL, NULL, NULL, 0) == 0;
    BU_LIST_INIT(&members.l);
    for (const char *child : {"left.g", "right.g", "arcs.g"})
	ok = ok && mk_addmember(child, &members.l, NULL, WMOP_UNION);
    ok = ok && mk_lcomb(writer, "all.g", &members, 0, NULL, NULL, NULL, 0) == 0;

    struct db_tree_state state;
    RT_DBTS_INIT(&state);
    state.ts_dbip = db;
    const char *object = "all.g";
    std::set<std::string> paths;
    if (ok)
	db_walk_tree(db, 1, &object, 1, &state, collect_path, NULL, NULL, &paths);
    const std::set<std::string> expected = {
	"/all.g/left.g/sphere.r", "/all.g/right.g/sphere.r",
	"/all.g/arcs.g/sphere.r", "/all.g/arcs.g/sphere.r@1"
    };
    if (paths != expected) {
	bu_log("Unexpected instance paths:\n");
	for (const auto &path : paths)
	    bu_log("  %s\n", path.c_str());
	ok = false;
    }
    wdb_close(writer);
    return ok ? 0 : 1;
}
