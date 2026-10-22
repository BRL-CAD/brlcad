/*                       C A N O N I C A L I Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <limits>

#include "bu/app.h"
#include "rt/func.h"

#include "../canonicalize_private.h"


namespace {

void
make_ell(struct rt_db_internal *intern, const point_t center,
	 const vect_t a, const vect_t b, const vect_t c)
{
    RT_DB_INTERNAL_INIT(intern);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_minor_type = ID_ELL;
    intern->idb_meth = &OBJ[ID_ELL];
    BU_ALLOC(intern->idb_ptr, struct rt_ell_internal);
    auto *ell = static_cast<struct rt_ell_internal *>(intern->idb_ptr);
    ell->magic = RT_ELL_INTERNAL_MAGIC;
    VMOVE(ell->v, center);
    VMOVE(ell->a, a);
    VMOVE(ell->b, b);
    VMOVE(ell->c, c);
}


int
test_matrix_comparison(const struct bn_tol *tol)
{
    mat_t a = MAT_INIT_IDN;
    mat_t b = MAT_INIT_IDN;
    const fastf_t epsilon = std::numeric_limits<fastf_t>::epsilon();

    b[0] += 16.0 * epsilon;
    if (!_ged_matrices_numerically_equal(a, b, tol))
	return 1;

    b[0] = a[0] + 0.5 * tol->perp;
    if (_ged_matrices_numerically_equal(a, b, tol))
	return 1;

    b[0] = a[0];
    b[3] = 0.5 * tol->dist;
    return _ged_matrices_numerically_equal(a, b, tol) ? 1 : 0;
}


int
test_geometry_comparison(const struct bn_tol *tol)
{
    const point_t center = VINIT_ZERO;
    const vect_t x_axis = {2.0, 0.0, 0.0};
    const vect_t y_axis = {0.0, 3.0, 0.0};
    const vect_t z_axis = {0.0, 0.0, 4.0};
    struct rt_db_internal a;
    struct rt_db_internal equivalent;

    make_ell(&a, center, x_axis, y_axis, z_axis);
    make_ell(&equivalent, center, y_axis, x_axis, z_axis);
    int failed = _ged_canonical_geometry_equal(&a, &equivalent, tol) ||
	!_ged_primitive_geometry_equal(&a, &equivalent, tol);
    rt_db_free_internal(&equivalent);
    rt_db_free_internal(&a);
    return failed;
}


int
test_transform(const struct bn_tol *tol)
{
    const point_t center = VINIT_ZERO;
    const vect_t x_axis = {2.0, 0.0, 0.0};
    const vect_t y_axis = {0.0, 3.0, 0.0};
    const vect_t z_axis = {0.0, 0.0, 4.0};
    struct rt_db_internal input;
    struct rt_db_internal transformed;
    mat_t matrix = MAT_INIT_IDN;

    make_ell(&input, center, x_axis, y_axis, z_axis);
    matrix[0] = 0.0;
    matrix[1] = -1.0;
    matrix[4] = 1.0;
    matrix[5] = 0.0;
    MAT_DELTAS(matrix, 11.0, -7.0, 5.0);
    RT_DB_INTERNAL_INIT(&transformed);
    if (_ged_transform_primitive(&transformed, matrix, &input) != BRLCAD_OK) {
	rt_db_free_internal(&input);
	return 1;
    }

    int failed = !_ged_transformed_geometry_equal(&input, matrix,
	&transformed, bn_mat_identity, tol);
    rt_db_free_internal(&transformed);
    rt_db_free_internal(&input);
    return failed;
}

} /* namespace */


int
main(int UNUSED(argc), const char *argv[])
{
    const struct bn_tol tol = BN_TOL_INIT_TOL;

    bu_setprogname(argv[0]);
    int failures = test_matrix_comparison(&tol) +
	test_geometry_comparison(&tol) + test_transform(&tol);
    if (failures)
	bu_log("libged canonical geometry tests failed: %d\n", failures);
    return failures ? 1 : 0;
}
