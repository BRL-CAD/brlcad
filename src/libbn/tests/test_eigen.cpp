/*                    T E S T _ E I G E N . C P P
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

#include <Eigen/Geometry>

#include "test_util.h"


static int
test_eigen_map(void)
{
    int failures = 0;
    const char *test = "eigen_map";
    mat_t bn_mat = MAT_INIT_ZERO;
    mat_t expected = {
	1.0, 2.0, 3.0, 4.0,
	5.0, 6.0, 7.0, 8.0,
	9.0, 10.0, 11.0, 12.0,
	13.0, 14.0, 15.0, 16.0
    };
    hvect_t vin = {1.0, 2.0, 3.0, 1.0};
    hvect_t vexpected = {18.0, 46.0, 74.0, 102.0};
    hvect_t vout = HINIT_ZERO;
    vect_t bn_vec = VINIT_ZERO;
    vect_t cmp_vec = {3.0, 2.0, 5.0};
    Eigen::Map<Eigen::Matrix<fastf_t, 4, 4, Eigen::RowMajor>> emat(bn_mat);
    Eigen::Map<Eigen::Matrix<fastf_t, 3, 1>> evec(bn_vec);

    emat = Eigen::Matrix<fastf_t, 4, 4>::Identity();
    if (!bn_mat_is_identity(bn_mat)) {
	report_failure(test, "Eigen identity assignment did not update libbn matrix storage");
	failures++;
    }

    emat <<
	1.0, 2.0, 3.0, 4.0,
	5.0, 6.0, 7.0, 8.0,
	9.0, 10.0, 11.0, 12.0,
	13.0, 14.0, 15.0, 16.0;
    if (!mat_close(bn_mat, expected, 0.0)) {
	report_failure(test, "row-major Eigen matrix map did not preserve libbn matrix ordering");
	failures++;
    }

    bn_matXvec(vout, bn_mat, vin);
    if (!hvect_close(vout, vexpected, 1.0e-12)) {
	report_failure(test, "Eigen-authored matrix did not behave correctly with bn_matXvec");
	failures++;
    }

    evec << 3.0, 2.0, 5.0;
    if (!vect_close(bn_vec, cmp_vec, 0.0)) {
	report_failure(test, "Eigen vector assignment did not update libbn vector storage");
	failures++;
    }

    VUNITIZE(cmp_vec);
    evec.normalize();
    if (!vect_close(bn_vec, cmp_vec, SMALL_FASTF)) {
	report_failure(test, "Eigen vector normalization diverged from libbn normalization");
	failures++;
    }

    return failures;
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_single(argc, argv, "map", test_eigen_map);
}
