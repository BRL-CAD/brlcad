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
    simulate::AutoPtr<db_full_path, db_free_full_path> autofree_full_path(
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
    simulate::AutoPtr<db_i, db_close> db(db_create_inmem());

    if (!db.ptr)
	bu_bomb("db_create_inmem() failed");

    {
	const point_t center = {10.0, -3.0, 7.0};

	if (mk_sph(db.ptr->dbi_wdbp, "sphere.s", center, 1.0))
	    bu_bomb("mk_sph() failed");
    }

    {
	const point_t min = {-1.0, -4.0, -3.0};
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

	if (mk_comb(db.ptr->dbi_wdbp, "falling.r", &members.l, true, NULL, NULL, NULL,
		    0, 0, 0, 0, false, true, false))
	    bu_bomb("mk_comb() failed");

	BU_LIST_INIT(&members.l);
	mk_addmember("base.s", &members.l, base_matrix, WMOP_UNION);
	mk_addmember("falling.r", &members.l, NULL, WMOP_UNION);

	if (mk_comb(db.ptr->dbi_wdbp, "scene.c", &members.l, false, NULL, NULL, NULL, 0,
		    0, 0, 0, false, true, false))
	    bu_bomb("mk_comb() failed");
    }

    simulate::Simulation(*db.ptr, "scene.c").step(3.0);

    {
	const mat_t expected_falling_matrix = {
	    -8.601763732025e-01, -3.622751595212e-02, -5.087041542726e-01, 1.267898491013e+01,
	    -2.648488540913e-02, 9.993015440712e-01, -2.638166412216e-02, 2.841764865291e-01,
	    5.093044817210e-01, -9.219920561365e-03, -8.605341136100e-01, -1.939468699647e+01,
	    0.0, 0.0, 0.0, 1.0
	};

	return matrix_equal(*db.ptr, "/scene.c/base.s", base_matrix)
	       && matrix_equal(*db.ptr, "/scene.c/falling.r", expected_falling_matrix);
    }
}


HIDDEN bool
simulate_test()
{
    return test_basic();
}


}


int
main()
{
    return simulate_test() ? EXIT_SUCCESS : EXIT_FAILURE;
}
