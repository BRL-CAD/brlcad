/*                      T E S T _ N O I S E . C
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

#include "test_util.h"


static int
test_noise_extended(void)
{
    int failures = 0;
    const char *test = "noise_extended";
    point_t pt = {0.125, -2.75, 99.5};
    point_t vec1 = VINIT_ZERO;
    point_t vec2 = VINIT_ZERO;
    double p1;
    double p2;
    double turb;
    double fbm;
    double mf;
    double ridged;

    bn_noise_init();

    p1 = bn_noise_perlin(pt);
    p2 = bn_noise_perlin(pt);
    if (!isfinite(p1) || !scalar_close(p1, p2, 0.0)) {
	report_failure(test, "bn_noise_perlin was not deterministic and finite");
	failures++;
    }

    bn_noise_vec(pt, vec1);
    bn_noise_vec(pt, vec2);
    if (!finite_vec(vec1) || !vect_close(vec1, vec2, DBL_EPSILON * 8.0)) {
	report_failure(test, "bn_noise_vec was not deterministic and finite");
	failures++;
    }

    turb = bn_noise_turb(pt, 1.0, 2.1753974, 4.0);
    fbm = bn_noise_fbm(pt, 1.0, 2.1753974, 4.0);
    mf = bn_noise_mf(pt, 1.0, 2.1753974, 4.0, 1.0);
    ridged = bn_noise_ridged(pt, 1.0, 2.1753974, 4.0, 1.0);

    if (!isfinite(turb) || !isfinite(fbm) || !isfinite(mf) || !isfinite(ridged)) {
	report_failure(test, "one or more advanced noise APIs returned non-finite results");
	failures++;
    }
    if (turb < 0.0) {
	report_failure(test, "bn_noise_turb should not return a negative turbulence magnitude");
	failures++;
    }

    return failures;
}


int
main(int argc, char *argv[])
{
    return bn_api_single(argc, argv, "noise", test_noise_extended);
}
