/*               S I M U L A T E _ T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017-2021 United States Government as represented by
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
 * Brief description
 *
 */

#include "common.h"

#include "../utility.hpp"

#include "bu/app.h"
#include "ged/analyze.h"
#include "wdb.h"


namespace
{


HIDDEN bool
matrix_equal(const db_i &db, const std::string &path,
	     const fastf_t * const other_matrix)
{
    RT_CK_DBI(&db);

    if (!other_matrix)
	bu_bomb("missing argument");

    db_full_path full_path;
    db_full_path_init(&full_path);
    const simulate::AutoPtr<db_full_path, db_free_full_path> autofree_full_path(
	&full_path);

    if (db_string_to_path(&full_path, &db, path.c_str()))
	bu_bomb("db_string_to_path() failed");

    if (full_path.fp_len < 2)
	bu_bomb("invalid path");

    mat_t matrix = MAT_INIT_IDN;

    if (1 != db_path_to_mat(const_cast<db_i *>(&db), &full_path, matrix,
			    full_path.fp_len, &rt_uniresource))
	bu_bomb("db_path_to_mat() failed");

    bn_tol tol = BN_TOL_INIT_ZERO;
    rt_tol_default(&tol);
    return bn_mat_is_equal(matrix, other_matrix, &tol);
}


HIDDEN bool
test_basic()
{
    ged ged_instance;
    const simulate::AutoPtr<ged, ged_free> autofree_ged_instance(&ged_instance);
    ged_init(&ged_instance);
    ged_instance.ged_wdbp = db_create_inmem()->dbi_wdbp;

    {
	const point_t center = {10.0e3, -3.0e3, 7.0e3};

	if (mk_sph(ged_instance.ged_wdbp, "sphere.s", center, 1.0e3))
	    bu_bomb("mk_sph() failed");
    }

    {
	const point_t min = { -1.0e3, -4.0e3, -3.0e3};
	const point_t max = {6.0e3, 3.0e3, 5.0e3};

	if (mk_rpp(ged_instance.ged_wdbp, "base.s", min, max))
	    bu_bomb("mk_rcc() failed");

	if (db5_update_attribute("base.s", "simulate::mass", "0.0",
				 ged_instance.ged_wdbp->dbip))
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

	if (mk_comb(ged_instance.ged_wdbp, "falling.c", &members.l, false, NULL, NULL,
		    NULL, 0, 0, 0, 0, false, false, false))
	    bu_bomb("mk_comb() failed");

	if (db5_update_attribute("falling.c", "simulate::type", "region",
				 ged_instance.ged_wdbp->dbip))
	    bu_bomb("db5_update_attribute() failed");

	BU_LIST_INIT(&members.l);
	mk_addmember("base.s", &members.l, base_matrix, WMOP_UNION);
	mk_addmember("falling.c", &members.l, NULL, WMOP_UNION);

	if (mk_comb(ged_instance.ged_wdbp, "scene.c", &members.l, false, NULL, NULL,
		    NULL, 0, 0, 0, 0, false, false, false))
	    bu_bomb("mk_comb() failed");

	if (db5_update_attribute("scene.c", "simulate::gravity", "<0.0, 0.0, -9.8>",
				 ged_instance.ged_wdbp->dbip))
	    bu_bomb("db5_update_attribute() failed");
    }

    const char *argv[] = {"simulate", "scene.c", "3.0"};

    if (GED_OK != ged_simulate(&ged_instance, sizeof(argv) / sizeof(argv[0]), argv))
	bu_bomb("ged_simulate() failed");

    {
	const mat_t expected_falling_matrix = {
	    -0.83488252261392082, -0.18829868121554275, -0.51721756972120769, 13130.932412480919,
	    -0.095216705450072589, 0.97490557247749365, -0.20122876743611806, 1916.7183325763783,
	    0.5421291614254794, -0.11875441313835919, -0.83186236599743135, -19513.130960162609,
	    0.0, 0.0, 0.0, 1.0
	};

	return matrix_equal(*ged_instance.ged_wdbp->dbip, "/scene.c/base.s",
			    base_matrix)
	       && matrix_equal(*ged_instance.ged_wdbp->dbip, "/scene.c/falling.c",
			       expected_falling_matrix);
    }
}


HIDDEN bool
test_matrices()
{
    ged ged_instance;
    const simulate::AutoPtr<ged, ged_free> autofree_ged_instance(&ged_instance);
    ged_init(&ged_instance);
    ged_instance.ged_wdbp = db_create_inmem()->dbi_wdbp;

    {
	const point_t center = {0.0e3, 0.0e3, 10.0e3};

	if (mk_sph(ged_instance.ged_wdbp, "falling.s", center, 1.0e3))
	    bu_bomb("mk_sph() failed");
    }

    {
	const point_t center = {0.0e3, 0.0e3, 0.0e3};

	if (mk_sph(ged_instance.ged_wdbp, "ground.s", center, 5.0e3))
	    bu_bomb("mk_sph() failed");
    }

    if (db5_update_attribute("ground.s", "simulate::mass", "0.0",
			     ged_instance.ged_wdbp->dbip))
	bu_bomb("db5_update_attribute() failed");

    {
	wmember members;
	BU_LIST_INIT(&members.l);
	mk_addmember("falling.s", &members.l, NULL, WMOP_UNION);

	if (mk_comb(ged_instance.ged_wdbp, "falling_solid.c", &members.l, false, NULL,
		    NULL, NULL, 0, 0, 0, 0, false, false, false))
	    bu_bomb("mk_comb() failed");

	if (db5_update_attribute("falling_solid.c", "simulate::type", "region",
				 ged_instance.ged_wdbp->dbip))
	    bu_bomb("db5_update_attribute() failed");

	BU_LIST_INIT(&members.l);
	mk_addmember("falling_solid.c", &members.l, NULL, WMOP_UNION);

	if (mk_comb(ged_instance.ged_wdbp, "falling.c", &members.l, false, NULL, NULL,
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

	if (mk_comb(ged_instance.ged_wdbp, "scene.c", &members.l, false, NULL, NULL,
		    NULL, 0, 0, 0, 0, false, false, false))
	    bu_bomb("mk_comb() failed");
    }

    const char *argv[] = {"simulate", "scene.c", "3.0"};

    if (GED_OK != ged_simulate(&ged_instance, sizeof(argv) / sizeof(argv[0]), argv))
	bu_bomb("ged_simulate() failed");

    const mat_t expected_falling_matrix = {
	0.76003024487701787, 0.52035495172539881, 0.38933959776974092, -3919.7253431376898,
	-0.46888883480839944, 0.85388110210507029, -0.22589892464991282, 2270.7736692668104,
	-0.44999717758147378, -0.010867182559630151, 0.89296351072752234, -2958.9319288135939,
	0.0, 0.0, 0.0, 1.0
    };

    return matrix_equal(*ged_instance.ged_wdbp->dbip,
			"/scene.c/falling.c/falling_solid.c", expected_falling_matrix);
}


HIDDEN bool
test_tutorial()
{
    ged ged_instance;
    const simulate::AutoPtr<ged, ged_free> autofree_ged_instance(&ged_instance);
    ged_init(&ged_instance);
    ged_instance.ged_wdbp = db_create_inmem()->dbi_wdbp;

    {
	const point_t cube_min = { -1.0e3, -1.0e3, -1.0e3}, cube_max = {1.0e3, 1.0e3, 1.0e3};

	if (mk_rpp(ged_instance.ged_wdbp, "cube.s", cube_min, cube_max))
	    bu_bomb("mk_rpp failed");

	const point_t ground_min = { -15.0e3, -15.0e3, -1.0e3}, ground_max = {15.0e3, 15.0e3, 1.0e3};

	if (mk_rpp(ged_instance.ged_wdbp, "ground.s", ground_min, ground_max))
	    bu_bomb("mk_rpp failed");
    }

    {
	mat_t cube_matrix = MAT_INIT_IDN;
	cube_matrix[MDZ] = 50.0e3;

	wmember members;
	BU_LIST_INIT(&members.l);
	mk_addmember("cube.s", &members.l, cube_matrix, WMOP_UNION);

	if (mk_comb(ged_instance.ged_wdbp, "cube.r", &members.l, true, NULL, NULL, NULL,
		    0, 0, 0, 0, false, false, false))
	    bu_bomb("mk_comb() failed");
    }

    if (db5_update_attribute("cube.r", "simulate::angular_velocity",
			     "<2.0, -1.0, 3.0>", ged_instance.ged_wdbp->dbip))
	bu_bomb("db5_update_attribute() failed");

    if (db5_update_attribute("cube.r", "simulate::type", "region",
			     ged_instance.ged_wdbp->dbip))
	bu_bomb("db5_update_attribute() failed");

    if (mk_comb1(ged_instance.ged_wdbp, "ground.r", "ground.s", true))
	bu_bomb("mk_comb1() failed");

    if (db5_update_attribute("ground.r", "simulate::type", "region",
			     ged_instance.ged_wdbp->dbip))
	bu_bomb("db5_update_attribute() failed");

    if (db5_update_attribute("ground.r", "simulate::mass", "0.0",
			     ged_instance.ged_wdbp->dbip))
	bu_bomb("db5_update_attribute() failed");

    {
	wmember members;
	BU_LIST_INIT(&members.l);
	mk_addmember("cube.r", &members.l, NULL, WMOP_UNION);
	mk_addmember("ground.r", &members.l, NULL, WMOP_UNION);

	if (mk_comb(ged_instance.ged_wdbp, "scene.c", &members.l, false, NULL, NULL,
		    NULL, 0, 0, 0, 0, false, false, false))
	    bu_bomb("mk_comb() failed");
    }

    const char *argv[] = {"simulate", "scene.c", "10.0"};

    if (GED_OK != ged_simulate(&ged_instance, sizeof(argv) / sizeof(argv[0]), argv))
	bu_bomb("ged_simulate() failed");

    {
	const mat_t expected_cube_matrix = {
	    0.9564669310705749, 0.29183374977387605, -0.0019578268555551577, 422.35388061063617,
	    -0.29182600647974366, 0.95646460793920496, 0.0035588551600282793, -429.04376101571802,
	    0.0029106985819245387, -0.0028329015592209695, 0.99999127805300314, -48009.523032656609,
	    0.0, 0.0, 0.0, 1.0
	};

	return matrix_equal(*ged_instance.ged_wdbp->dbip, "/scene.c/cube.r",
			    expected_cube_matrix);
    }
}


HIDDEN bool
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
