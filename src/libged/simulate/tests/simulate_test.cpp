#include "common.h"

#include "simulation.hpp"
#include "utility.hpp"

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
    const simulate::AutoPtr<db_i, db_close> db(db_create_inmem());

    if (!db.ptr)
	bu_bomb("db_create_inmem() failed");

    {
	const point_t center = {10.0, -3.0, 7.0};

	if (mk_sph(db.ptr->dbi_wdbp, "sphere.s", center, 1.0))
	    bu_bomb("mk_sph() failed");
    }

    {
	const point_t min = { -1.0, -4.0, -3.0};
	const point_t max = {6.0, 3.0, 5.0};

	if (mk_rpp(db.ptr->dbi_wdbp, "base.s", min, max))
	    bu_bomb("mk_rcc() failed");

	if (db5_update_attribute("base.s", "simulate::mass", "0.0", db.ptr))
	    bu_bomb("db5_update_attribute() failed");
    }

    mat_t base_matrix = {
	-5.521101248230e-01, -3.139768433222e-01, 7.723942982217e-01, 2.187053795275e+01,
	-2.610004893800e-01, 9.449108513725e-01, 1.975404452307e-01, 1.139654545789e+01,
	-7.918668793499e-01, -9.253120995622e-02, -6.036429578589e-01, 1.538490559814e+01,
	0.0, 0.0, 0.0, 2.171448921007e+00
    };

    {
	mat_t sphere1_matrix = {
	    8.107419012686e-01, -5.807087377357e-01, 7.399277968041e-02, -1.037746328886e+01,
	    -2.644176206353e-01, -2.504944693283e-01, 9.313086721026e-01, -3.751995030519e+00,
	    -5.222843013388e-01, -7.746159582357e-01, -3.566359850347e-01, 6.822434777292e+00,
	    0.0, 0.0, 0.0, 2.187865118746e-01
	};

	mat_t sphere2_matrix = {
	    8.107419012686e-01, -5.807087377357e-01, 7.399277968041e-02, 1.315409627756e+00,
	    -2.644176206353e-01, -2.504944693283e-01, 9.313086721026e-01, -3.208803314406e-01,
	    -5.222843013388e-01, -7.746159582357e-01, -3.566359850347e-01, 1.381761676557e+01,
	    0.0, 0.0, 0.0, 1.0
	};

	wmember members;
	BU_LIST_INIT(&members.l);
	mk_addmember("sphere.s", &members.l, sphere1_matrix, WMOP_UNION);
	mk_addmember("sphere.s", &members.l, sphere2_matrix, WMOP_UNION);

	if (mk_comb(db.ptr->dbi_wdbp, "falling.c", &members.l, false, NULL, NULL, NULL,
		    0, 0, 0, 0, false, false, false))
	    bu_bomb("mk_comb() failed");

	if (db5_update_attribute("falling.c", "simulate::type", "region", db.ptr))
	    bu_bomb("db5_update_attribute() failed");

	BU_LIST_INIT(&members.l);
	mk_addmember("base.s", &members.l, base_matrix, WMOP_UNION);
	mk_addmember("falling.c", &members.l, NULL, WMOP_UNION);

	if (mk_comb(db.ptr->dbi_wdbp, "scene.c", &members.l, false, NULL, NULL, NULL, 0,
		    0, 0, 0, false, false, false))
	    bu_bomb("mk_comb() failed");

	if (db5_update_attribute("scene.c", "simulate::gravity", "<0.0, 0.0, -9.8>",
				 db.ptr))
	    bu_bomb("db5_update_attribute() failed");
    }

    db_full_path path;
    db_full_path_init(&path);
    const simulate::AutoPtr<db_full_path, db_free_full_path> autofree_path(&path);

    if (db_string_to_path(&path, db.ptr, "scene.c"))
	bu_bomb("db_string_to_path() failed");

    try {
	simulate::Simulation(*db.ptr, path).step(3.0,
		simulate::Simulation::debug_none);
    } catch (const simulate::InvalidSimulationError &exception) {
	bu_log("simulation failed: '%s'\n", exception.what());
	return false;
    }

    {
	const mat_t expected_falling_matrix = {
	    -8.601763732025e-01, -3.622751595212e-02, -5.087041542726e-01, 1.267898491013e+01,
	    -2.648488540913e-02, 9.993015440712e-01, -2.638166412216e-02, 2.841764865291e-01,
	    5.093044817210e-01, -9.219920561365e-03, -8.605341136100e-01, -1.939468699647e+01,
	    0.0, 0.0, 0.0, 1.0
	};

	return matrix_equal(*db.ptr, "/scene.c/base.s", base_matrix)
	       && matrix_equal(*db.ptr, "/scene.c/falling.c", expected_falling_matrix);
    }
}


HIDDEN bool
test_tutorial()
{
    const simulate::AutoPtr<db_i, db_close> db(db_create_inmem());

    if (!db.ptr)
	bu_bomb("db_create_inmem() failed");

    {
	const point_t cube_min = { -1.0, -1.0, -1.0}, cube_max = {1.0, 1.0, 1.0};

	if (mk_rpp(db.ptr->dbi_wdbp, "cube.s", cube_min, cube_max))
	    bu_bomb("mk_rpp failed");

	const point_t ground_min = { -15.0, -15.0, -1.0}, ground_max = {15.0, 15.0, 1.0};

	if (mk_rpp(db.ptr->dbi_wdbp, "ground.s", ground_min, ground_max))
	    bu_bomb("mk_rpp failed");
    }

    {
	mat_t cube_matrix = MAT_INIT_IDN;
	cube_matrix[MDZ] = 50.0;

	wmember members;
	BU_LIST_INIT(&members.l);
	mk_addmember("cube.s", &members.l, cube_matrix, WMOP_UNION);

	if (mk_comb(db.ptr->dbi_wdbp, "cube.r", &members.l, true, NULL, NULL, NULL, 0,
		    0, 0, 0, false, false, false))
	    bu_bomb("mk_comb() failed");
    }

    if (db5_update_attribute("cube.r", "simulate::angular_velocity",
			     "<2.0, -1.0, 3.0>", db.ptr))
	bu_bomb("db5_update_attribute() failed");

    if (db5_update_attribute("cube.r", "simulate::type", "region", db.ptr))
	bu_bomb("db5_update_attribute() failed");

    if (mk_comb1(db.ptr->dbi_wdbp, "ground.r", "ground.s", true))
	bu_bomb("mk_comb1() failed");

    if (db5_update_attribute("ground.r", "simulate::type", "region", db.ptr))
	bu_bomb("db5_update_attribute() failed");

    if (db5_update_attribute("ground.r", "simulate::mass", "0.0", db.ptr))
	bu_bomb("db5_update_attribute() failed");

    {
	wmember members;
	BU_LIST_INIT(&members.l);
	mk_addmember("cube.r", &members.l, NULL, WMOP_UNION);
	mk_addmember("ground.r", &members.l, NULL, WMOP_UNION);

	if (mk_comb(db.ptr->dbi_wdbp, "scene.c", &members.l, false, NULL, NULL, NULL, 0,
		    0, 0, 0, false, false, false))
	    bu_bomb("mk_comb() failed");
    }

    db_full_path path;
    db_full_path_init(&path);
    const simulate::AutoPtr<db_full_path, db_free_full_path> autofree_path(&path);

    if (db_string_to_path(&path, db.ptr, "scene.c"))
	bu_bomb("db_string_to_path() failed");

    try {
	simulate::Simulation(*db.ptr, path).step(10.0,
		simulate::Simulation::debug_none);
    } catch (const simulate::InvalidSimulationError &exception) {
	bu_log("simulation failed: '%s'\n", exception.what());
	return false;
    }

    {
	const mat_t expected_cube_matrix = {
	    9.109535781920e-01, -5.910980851570e-03, -4.124657253859e-01, 2.061374962065e+01,
	    -4.123979850242e-01, 1.002896188117e-02, -9.109520277313e-01, 4.573584175369e+01,
	    9.523711592403e-03, 9.999367173941e-01, 6.697253647870e-03, 1.620014210436e+00,
	    0.0, 0.0, 0.0, 1.0
	};

	return matrix_equal(*db.ptr, "/scene.c/cube.r", expected_cube_matrix);
    }
}


HIDDEN bool
test_matrices()
{
    const simulate::AutoPtr<db_i, db_close> db(db_create_inmem());

    if (!db.ptr)
	bu_bomb("db_create_inmem() failed");

    {
	const point_t center = {00.0, 0.0, 10.0};

	if (mk_sph(db.ptr->dbi_wdbp, "falling.s", center, 1.0))
	    bu_bomb("mk_sph() failed");
    }

    {
	const point_t center = {0.0, 0.0, 0.0};

	if (mk_sph(db.ptr->dbi_wdbp, "ground.s", center, 5.0))
	    bu_bomb("mk_sph() failed");
    }

    if (db5_update_attribute("ground.s", "simulate::mass", "0.0", db.ptr))
	bu_bomb("db5_update_attribute() failed");

    {
	wmember members;
	BU_LIST_INIT(&members.l);
	mk_addmember("falling.s", &members.l, NULL, WMOP_UNION);

	if (mk_comb(db.ptr->dbi_wdbp, "falling_solid.c", &members.l, false, NULL, NULL,
		    NULL,
		    0, 0, 0, 0, false, false, false))
	    bu_bomb("mk_comb() failed");

	if (db5_update_attribute("falling_solid.c", "simulate::type", "region", db.ptr))
	    bu_bomb("db5_update_attribute() failed");

	BU_LIST_INIT(&members.l);
	mk_addmember("falling_solid.c", &members.l, NULL, WMOP_UNION);

	if (mk_comb(db.ptr->dbi_wdbp, "falling.c", &members.l, false, NULL, NULL, NULL,
		    0, 0, 0, 0, false, false, false))
	    bu_bomb("mk_comb() failed");

	mat_t falling_matrix = {
	    1.879681598222e-01, 6.371933019316e-01, 7.474307104116e-01, -7.474307104116e+00,
	    1.777804646915e-01, 7.263520916684e-01, -6.639327867359e-01, 6.639327867359e+00,
	    -9.659513845256e-01, 2.576768031901e-01, 2.325054473825e-02, 9.767494552618e+00,
	    0.0, 0.0, 0.0, 1.0
	};

	BU_LIST_INIT(&members.l);
	mk_addmember("falling.c", &members.l, falling_matrix, WMOP_UNION);
	mk_addmember("ground.s", &members.l, NULL, WMOP_UNION);

	if (mk_comb(db.ptr->dbi_wdbp, "scene.c", &members.l, false, NULL, NULL, NULL, 0,
		    0, 0, 0, false, false, false))
	    bu_bomb("mk_comb() failed");
    }

    db_full_path path;
    db_full_path_init(&path);
    const simulate::AutoPtr<db_full_path, db_free_full_path> autofree_path(&path);

    if (db_string_to_path(&path, db.ptr, "scene.c"))
	bu_bomb("db_string_to_path() failed");

    try {
	simulate::Simulation(*db.ptr, path).step(10.0,
		simulate::Simulation::debug_none);
    } catch (const simulate::InvalidSimulationError &exception) {
	bu_log("simulation failed: '%s'\n", exception.what());
	return false;
    }

    {
	const mat_t expected_falling_matrix = {
	    8.693316464693e-01, 4.453003990884e-01, 2.144078528305e-01, -2.136225176282e+00,
	    -4.083346425447e-01, 8.915402353225e-01, -1.960082711628e-01, 1.972270250135e+00,
	    -2.784349980193e-01, 8.284621363135e-02, 9.568743085221e-01, -3.596945807610e+00,
	    0.0, 0.0, 0.0, 1.0
	};

	return matrix_equal(*db.ptr, "/scene.c/falling.c/falling_solid.c",
			    expected_falling_matrix);
    }

    return true;
}


HIDDEN bool
simulate_test()
{
    return test_basic() && test_tutorial() && test_matrices();
}


}


int
main()
{
    return simulate_test() ? EXIT_SUCCESS : EXIT_FAILURE;
}
