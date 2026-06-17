/*               S I M U L A T E _ T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017-2026 United States Government as represented by
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
/** @file simulate_test.cpp
 *
 * Intended to exercise the libged simulate command
 *
 */

#include "common.h"

#include <cstring>
#include <string>

#include "../utility.hpp"

#include "bu/app.h"
#include "analyze.h"
#include "ged/commands.h"
#include "ged/event_txn.h"
#include "wdb.h"

namespace
{

    struct SimulateEventObserver {
	SimulateEventObserver() : calls(0), saw_scene_change(false),
				  saw_falling_state_attr(false) {}

	size_t calls;
	bool saw_scene_change;
	bool saw_falling_state_attr;
	std::string affected_names;
	std::string event_summary;
    };


    static bool
	name_equal(const char *a, const char *b)
	{
	    return a && b && std::strcmp(a, b) == 0;
	}


    static void
	simulate_event_cb(struct ged *UNUSED(gedp),
		const struct ged_event *events,
		size_t event_count,
		const struct ged_event_txn_result *result,
		void *client_data)
	{
	    SimulateEventObserver *observer =
		static_cast<SimulateEventObserver *>(client_data);
	    if (!observer)
		return;

	    observer->calls++;
	    if (result)
		observer->affected_names = bu_vls_cstr(&result->affected_names);

	    for (size_t i = 0; i < event_count; ++i) {
		if (!observer->event_summary.empty())
		    observer->event_summary += "; ";
		observer->event_summary += std::to_string((int)events[i].kind);
		observer->event_summary += ":";
		observer->event_summary += events[i].name ? events[i].name : "(null)";

		if ((events[i].kind == GED_EVENT_OBJECT_MODIFIED ||
			events[i].kind == GED_EVENT_COMB_TREE_CHANGED) &&
			name_equal(events[i].name, "scene.c"))
		    observer->saw_scene_change = true;

		if (events[i].kind == GED_EVENT_ATTRIBUTE_CHANGED &&
			name_equal(events[i].name, "falling.c"))
		    observer->saw_falling_state_attr = true;
	    }
	}


    static bool
	path_matrix(const db_i &db, const std::string &path, mat_t matrix)
	{
	    RT_CK_DBI(&db);

	    if (!matrix)
		bu_bomb("missing argument");

	    db_full_path full_path;
	    db_full_path_init(&full_path);
	    const simulate::AutoPtr<db_full_path, db_free_full_path> autofree_full_path(&full_path);

	    if (db_string_to_path(&full_path, &db, path.c_str()))
		return false;

	    if (full_path.fp_len < 2)
		return false;

	    if (1 != db_path_to_mat(const_cast<db_i *>(&db), &full_path, matrix, full_path.fp_len))
		return false;

	    return true;
	}


    static bool
	matrix_equal(const db_i &db, const std::string &path,
		const fastf_t * const other_matrix)
	{
	    if (!other_matrix)
		bu_bomb("missing argument");

	    mat_t matrix = MAT_INIT_IDN;
	    if (!path_matrix(db, path, matrix))
		return false;

	    bn_tol tol = BN_TOL_INIT_ZERO;
	    rt_tol_default(&tol);
	    return bn_mat_is_equal(matrix, other_matrix, &tol);
	}


    static bool
	matrix_changed(const db_i &db, const std::string &path,
		const fastf_t * const original_matrix)
	{
	    return !matrix_equal(db, path, original_matrix);
	}


    static bool
	has_attribute(const db_i &db, const char *name, const char *key)
	{
	    if (!name || !key)
		return false;

	    struct directory *dp =
		db_lookup(const_cast<db_i *>(&db), name, LOOKUP_QUIET);
	    if (dp == RT_DIR_NULL)
		return false;

	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);
	    if (db5_get_attributes(const_cast<db_i *>(&db), &avs, dp) != 0) {
		bu_avs_free(&avs);
		return false;
	    }

	    const char *value = bu_avs_get(&avs, key);
	    bool found = value != NULL;
	    bu_avs_free(&avs);
	    return found;
	}


    static bool
	test_basic()
	{
	    ged ged_instance;
	    const simulate::AutoPtr<ged, ged_free> autofree_ged_instance(&ged_instance);
	    ged_init(&ged_instance);
	    ged_instance.dbip = db_create_inmem();
	    if (!ged_event_librt_callbacks_enable(&ged_instance))
		bu_bomb("ged_event_librt_callbacks_enable() failed");
	    struct rt_wdb *wdbp = wdb_dbopen(ged_instance.dbip, RT_WDB_TYPE_DB_INMEM);

	    {
		const point_t center = {10.0e3, -3.0e3, 7.0e3};

		if (mk_sph(wdbp, "sphere.s", center, 1.0e3))
		    bu_bomb("mk_sph() failed");
	    }

	    {
		const point_t min = { -1.0e3, -4.0e3, -3.0e3};
		const point_t max = {6.0e3, 3.0e3, 5.0e3};

		if (mk_rpp(wdbp, "base.s", min, max))
		    bu_bomb("mk_rcc() failed");

		if (db5_update_attribute("base.s", "simulate::mass", "0.0", ged_instance.dbip))
		    bu_bomb("db5_update_attribute() failed");
	    }

	    mat_t base_matrix = {
		-5.521101248230e-01, -3.139768433222e-01, 7.723942982217e-01, 2.187053795275e+04,
		-2.610004893800e-01, 9.449108513725e-01, 1.975404452307e-01, 1.139654545789e+04,
		-7.918668793499e-01, -9.253120995622e-02, -6.036429578589e-01, 1.538490559814e+04,
		0.0, 0.0, 0.0, 2.171448921007e+00
	    };

	    {
		mat_t sphere1_matrix = {
		    8.107419012686e-01, -5.807087377357e-01, 7.399277968041e-02, -1.037746328886e+04,
		    -2.644176206353e-01, -2.504944693283e-01, 9.313086721026e-01, -3.751995030519e+03,
		    -5.222843013388e-01, -7.746159582357e-01, -3.566359850347e-01, 6.822434777292e+03,
		    0.0, 0.0, 0.0, 2.187865118746e-01
		};

		mat_t sphere2_matrix = {
		    8.107419012686e-01, -5.807087377357e-01, 7.399277968041e-02, 1.315409627756e+03,
		    -2.644176206353e-01, -2.504944693283e-01, 9.313086721026e-01, -3.208803314406e-04,
		    -5.222843013388e-01, -7.746159582357e-01, -3.566359850347e-01, 1.381761676557e+04,
		    0.0, 0.0, 0.0, 1.0
		};

		wmember members;
		BU_LIST_INIT(&members.l);
		mk_addmember("sphere.s", &members.l, sphere1_matrix, WMOP_UNION);
		mk_addmember("sphere.s", &members.l, sphere2_matrix, WMOP_UNION);

		if (mk_comb(wdbp, "falling.c", &members.l, false, NULL, NULL,
			    NULL, 0, 0, 0, 0, false, false, false))
		    bu_bomb("mk_comb() failed");

		if (db5_update_attribute("falling.c", "simulate::type", "region",
			    ged_instance.dbip))
		    bu_bomb("db5_update_attribute() failed");

		BU_LIST_INIT(&members.l);
		mk_addmember("base.s", &members.l, base_matrix, WMOP_UNION);
		mk_addmember("falling.c", &members.l, NULL, WMOP_UNION);

		if (mk_comb(wdbp, "scene.c", &members.l, false, NULL, NULL,
			    NULL, 0, 0, 0, 0, false, false, false))
		    bu_bomb("mk_comb() failed");

	    if (db5_update_attribute("scene.c", "simulate::gravity", "<0.0, 0.0, -9.8>",
			ged_instance.dbip))
		bu_bomb("db5_update_attribute() failed");
	    }

	    mat_t falling_before = MAT_INIT_IDN;
	    if (!path_matrix(*ged_instance.dbip, "/scene.c/falling.c",
			falling_before))
		bu_bomb("initial falling matrix lookup failed");

	    if (!ged_event_txn_available(&ged_instance))
		bu_bomb("GedEventTxn unavailable");

	    SimulateEventObserver observer;
	    ged_event_observer_token observer_token =
		ged_event_observer_add(&ged_instance,
			GED_EVENT_OBSERVER_POST_RECONCILE,
			simulate_event_cb, &observer);
	    if (!observer_token)
		bu_bomb("ged_event_observer_add() failed");

	    const char *argv[] = {"simulate", "scene.c", "3.0"};

	    if (BRLCAD_OK != ged_exec(&ged_instance, sizeof(argv) / sizeof(argv[0]), argv))
		bu_bomb("ged_simulate() failed");

	    if (1 != ged_event_observer_remove(&ged_instance, observer_token))
		bu_bomb("ged_event_observer_remove() failed");

	    if (observer.calls != 1) {
		bu_log("simulate event observer saw %zu transactions, expected 1\n",
			observer.calls);
		return false;
	    }
	    if (!observer.saw_scene_change) {
		bu_log("simulate event observer did not see scene.c geometry update; events: %s\n",
			observer.event_summary.c_str());
		return false;
	    }
	    if (!observer.saw_falling_state_attr) {
		bu_log("simulate event observer did not see falling.c saved-state attr update\n");
		return false;
	    }
	    if (observer.affected_names.find("falling.c") == std::string::npos) {
		bu_log("simulate event observer affected names missing falling.c: %s\n",
			observer.affected_names.c_str());
		return false;
	    }
	    if (observer.affected_names.find("scene.c") == std::string::npos) {
		bu_log("simulate event observer affected names missing scene.c: %s\n",
			observer.affected_names.c_str());
		return false;
	    }

	    return matrix_equal(*ged_instance.dbip, "/scene.c/base.s",
		    base_matrix)
		&& matrix_changed(*ged_instance.dbip, "/scene.c/falling.c",
			falling_before)
		&& has_attribute(*ged_instance.dbip, "falling.c",
			"simulate::state_linear_velocity")
		&& has_attribute(*ged_instance.dbip, "falling.c",
			"simulate::state_angular_velocity");
	}


    static bool
	test_matrices()
	{
	    ged ged_instance;
	    const simulate::AutoPtr<ged, ged_free> autofree_ged_instance(&ged_instance);
	    ged_init(&ged_instance);
	    ged_instance.dbip = db_create_inmem();
	    struct rt_wdb *wdbp = wdb_dbopen(ged_instance.dbip, RT_WDB_TYPE_DB_INMEM);

	    {
		const point_t center = {0.0e3, 0.0e3, 10.0e3};

		if (mk_sph(wdbp, "falling.s", center, 1.0e3))
		    bu_bomb("mk_sph() failed");
	    }

	    {
		const point_t center = {0.0e3, 0.0e3, 0.0e3};

		if (mk_sph(wdbp, "ground.s", center, 5.0e3))
		    bu_bomb("mk_sph() failed");
	    }

	    if (db5_update_attribute("ground.s", "simulate::mass", "0.0", ged_instance.dbip))
		bu_bomb("db5_update_attribute() failed");

	    {
		wmember members;
		BU_LIST_INIT(&members.l);
		mk_addmember("falling.s", &members.l, NULL, WMOP_UNION);

		if (mk_comb(wdbp, "falling_solid.c", &members.l, false, NULL,
			    NULL, NULL, 0, 0, 0, 0, false, false, false))
		    bu_bomb("mk_comb() failed");

		if (db5_update_attribute("falling_solid.c", "simulate::type", "region",
			    ged_instance.dbip))
		    bu_bomb("db5_update_attribute() failed");

		BU_LIST_INIT(&members.l);
		mk_addmember("falling_solid.c", &members.l, NULL, WMOP_UNION);

		if (mk_comb(wdbp, "falling.c", &members.l, false, NULL, NULL,
			    NULL, 0, 0, 0, 0, false, false, false))
		    bu_bomb("mk_comb() failed");

		mat_t falling_matrix = {
		    1.879681598222e-01, 6.371933019316e-01, 7.474307104116e-01, -7.474307104116e+03,
		    1.777804646915e-01, 7.263520916684e-01, -6.639327867359e-01, 6.639327867359e+03,
		    -9.659513845256e-01, 2.576768031901e-01, 2.325054473825e-02, 9.767494552618e+03,
		    0.0, 0.0, 0.0, 1.0
		};

		BU_LIST_INIT(&members.l);
		mk_addmember("falling.c", &members.l, falling_matrix, WMOP_UNION);
		mk_addmember("ground.s", &members.l, NULL, WMOP_UNION);

		if (mk_comb(wdbp, "scene.c", &members.l, false, NULL, NULL,
			    NULL, 0, 0, 0, 0, false, false, false))
		    bu_bomb("mk_comb() failed");
	    }

	    mat_t falling_before = MAT_INIT_IDN;
	    if (!path_matrix(*ged_instance.dbip,
		    "/scene.c/falling.c/falling_solid.c", falling_before))
		bu_bomb("initial nested falling matrix lookup failed");

	    const char *argv[] = {"simulate", "scene.c", "3.0"};

	    if (BRLCAD_OK != ged_exec(&ged_instance, sizeof(argv) / sizeof(argv[0]), argv))
		bu_bomb("ged_simulate() failed");

	    return matrix_changed(*ged_instance.dbip,
		    "/scene.c/falling.c/falling_solid.c", falling_before);
	}


    static bool
	test_tutorial()
	{
	    ged ged_instance;
	    const simulate::AutoPtr<ged, ged_free> autofree_ged_instance(&ged_instance);
	    ged_init(&ged_instance);
	    ged_instance.dbip = db_create_inmem();
	    struct rt_wdb *wdbp = wdb_dbopen(ged_instance.dbip, RT_WDB_TYPE_DB_INMEM);

	    {
		const point_t cube_min = { -1.0e3, -1.0e3, -1.0e3}, cube_max = {1.0e3, 1.0e3, 1.0e3};

		if (mk_rpp(wdbp, "cube.s", cube_min, cube_max))
		    bu_bomb("mk_rpp failed");

		const point_t ground_min = { -15.0e3, -15.0e3, -1.0e3}, ground_max = {15.0e3, 15.0e3, 1.0e3};

		if (mk_rpp(wdbp, "ground.s", ground_min, ground_max))
		    bu_bomb("mk_rpp failed");
	    }

	    {
		mat_t cube_matrix = MAT_INIT_IDN;
		cube_matrix[MDZ] = 50.0e3;

		wmember members;
		BU_LIST_INIT(&members.l);
		mk_addmember("cube.s", &members.l, cube_matrix, WMOP_UNION);

		if (mk_comb(wdbp, "cube.r", &members.l, true, NULL, NULL, NULL,
			    0, 0, 0, 0, false, false, false))
		    bu_bomb("mk_comb() failed");
	    }

	    if (db5_update_attribute("cube.r", "simulate::angular_velocity",
			"<2.0, -1.0, 3.0>", ged_instance.dbip))
		bu_bomb("db5_update_attribute() failed");

	    if (db5_update_attribute("cube.r", "simulate::type", "region",
			ged_instance.dbip))
		bu_bomb("db5_update_attribute() failed");

	    if (mk_comb1(wdbp, "ground.r", "ground.s", true))
		bu_bomb("mk_comb1() failed");

	    if (db5_update_attribute("ground.r", "simulate::type", "region",
			ged_instance.dbip))
		bu_bomb("db5_update_attribute() failed");

	    if (db5_update_attribute("ground.r", "simulate::mass", "0.0",
			ged_instance.dbip))
		bu_bomb("db5_update_attribute() failed");

	    {
		wmember members;
		BU_LIST_INIT(&members.l);
		mk_addmember("cube.r", &members.l, NULL, WMOP_UNION);
		mk_addmember("ground.r", &members.l, NULL, WMOP_UNION);

		if (mk_comb(wdbp, "scene.c", &members.l, false, NULL, NULL,
			    NULL, 0, 0, 0, 0, false, false, false))
		    bu_bomb("mk_comb() failed");
	    }

	    mat_t cube_before = MAT_INIT_IDN;
	    if (!path_matrix(*ged_instance.dbip, "/scene.c/cube.r",
		    cube_before))
		bu_bomb("initial cube matrix lookup failed");

	    const char *argv[] = {"simulate", "scene.c", "10.0"};

	    if (BRLCAD_OK != ged_exec(&ged_instance, sizeof(argv) / sizeof(argv[0]), argv))
		bu_bomb("ged_simulate() failed");

	    return matrix_changed(*ged_instance.dbip, "/scene.c/cube.r",
		    cube_before);
	}


    static bool
	simulate_test()
	{
	    return test_basic() && test_matrices() && test_tutorial();
	}


}


int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    return simulate_test() ? EXIT_SUCCESS : EXIT_FAILURE;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
