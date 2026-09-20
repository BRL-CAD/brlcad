/*                S A M P L I N G _ T E S T . C
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

#include <float.h>
#include <stdio.h>

#include "analyze/gqa.h"
#include "analyze/sampling.h"
#include "bu/app.h"
#include "vmath.h"


int
main(int UNUSED(argc), char *argv[])
{
    static const double estimates[ANALYZE_SAMPLING_MIN_REPLICATES] = {
	1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    struct analyze_sampling_interval interval;
    static const double ratio_numerators[ANALYZE_SAMPLING_MIN_REPLICATES] = {
	1.0, 4.0, 9.0, 16.0, 25.0, 36.0, 49.0, 64.0, 81.0, 100.0};
    static const double zero_denominators[ANALYZE_SAMPLING_MIN_REPLICATES] = {0.0};

    bu_setprogname(argv[0]);

    if (analyze_sampling_interval95(&interval, estimates,
	    ANALYZE_SAMPLING_MIN_REPLICATES) != ANALYZE_OK ||
	!NEAR_EQUAL(interval.estimate, 5.5, SMALL_FASTF) ||
	!(interval.half_width > 0.0) || !(interval.half_width < DBL_MAX)) {
	fprintf(stderr, "valid sampling interval was rejected\n");
	return 1;
    }
    if (analyze_sampling_ratio_interval95(&interval, ratio_numerators,
	    estimates, ANALYZE_SAMPLING_MIN_REPLICATES) != ANALYZE_OK ||
	!NEAR_EQUAL(interval.estimate, 7.0, SMALL_FASTF) ||
	!(interval.half_width > 0.0) || !(interval.half_width < DBL_MAX)) {
	fprintf(stderr, "valid ratio sampling interval was rejected\n");
	return 1;
    }
    if (analyze_sampling_ratio_interval95(&interval, ratio_numerators,
	    zero_denominators, ANALYZE_SAMPLING_MIN_REPLICATES) != ANALYZE_ERROR) {
	fprintf(stderr, "zero-denominator ratio interval was accepted\n");
	return 1;
    }
    if (analyze_sampling_interval95(&interval, estimates,
	    ANALYZE_SAMPLING_MIN_REPLICATES - 1) != ANALYZE_ERROR) {
	fprintf(stderr, "undersized replicate set was accepted\n");
	return 1;
    }

    {
	const char *arguments[] = {
	    "gqa", "--analyze", "--uncertainty", "--rays", "1600",
	    "model.g", "object"};
	if (analyze_gqa_model_argument(7, arguments) != 5) {
	    fprintf(stderr, "analysis model argument was not located\n");
	    return 1;
	}
    }
    {
	const char *arguments[] = {
	    "gqa", "--analyze", "--rays", "model.g"};
	if (analyze_gqa_model_argument(4, arguments) >= 0) {
	    fprintf(stderr, "missing analysis option value was accepted\n");
	    return 1;
	}
    }

    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
