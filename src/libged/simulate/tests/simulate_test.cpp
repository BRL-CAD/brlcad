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
	    -0.8601763732025367, -0.036227515952117456, -0.50870415427255744, 12.678984910132268,
	    -0.026484885409131961, 0.99930154407117733, -0.026381664122163178, 0.28417648652906946,
	    0.50930448172096932, -0.0092199205613647701, -0.8605341136099528, -19.394686996467197,
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
	    0.9109535781920024, -0.0059109808515701431, -0.41246572538591258, 20.613749620650484,
	    -0.41239798502422625, 0.010028961881171757, -0.91095202773134265, 45.735841753689698,
	    0.0095237115924034603, 0.99993671739409096, 0.0066972536478696974, 1.6200142104360542,
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
	const point_t center = {0.0, 0.0, 10.0};

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
	    0.86933164646925964, 0.4453003990884265, 0.21440785283046893, -2.1362251762822551,
	    -0.4083346425446982, 0.89154023532250259, -0.19600827116279843, 1.9722702501354501,
	    -0.27843499801926619, 0.08284621363134631, 0.95687430852209765, -3.5969458076103624,
	    0.0, 0.0, 0.0, 1.0
	};

	return matrix_equal(*db.ptr, "/scene.c/falling.c/falling_solid.c",
			    expected_falling_matrix);
    }
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
