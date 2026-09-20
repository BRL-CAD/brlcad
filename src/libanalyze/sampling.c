/*                     S A M P L I N G . C
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

#include <float.h>
#include <math.h>

#include "analyze/sampling.h"

/*
 * Owen recommends at least ten independent randomized-QMC replications and
 * an ordinary Student-t interval across their estimates.  See A. B. Owen,
 * "Error estimation for quasi-Monte Carlo" (2025), section 5,
 * https://arxiv.org/abs/2501.00150v3.
 *
 * Using t(0.975, 9) for all R >= 10 is conservative relative to the smaller
 * critical value for R - 1 degrees of freedom and avoids making the public
 * API depend on a special-functions package.
 */
#define ANALYZE_STUDENT_T_95_DF9 2.2621571627409915

int
analyze_sampling_interval95(struct analyze_sampling_interval *interval,
    const double *replicate_estimates, size_t replicate_count)
{
    if (!interval || !replicate_estimates ||
        replicate_count < ANALYZE_SAMPLING_MIN_REPLICATES)
	return ANALYZE_ERROR;

    double sum = 0.0;
    for (size_t i = 0; i < replicate_count; i++) {
	if (!isfinite(replicate_estimates[i])) return ANALYZE_ERROR;
	sum += replicate_estimates[i];
    }

    interval->estimate = sum / (double)replicate_count;
    double squared_deviation = 0.0;
    for (size_t i = 0; i < replicate_count; i++) {
	const double deviation = replicate_estimates[i] - interval->estimate;
	squared_deviation += deviation * deviation;
    }
    const double variance = squared_deviation / (double)(replicate_count - 1);
    interval->half_width = ANALYZE_STUDENT_T_95_DF9 *
	sqrt(variance / (double)replicate_count);

    return isfinite(interval->estimate) && isfinite(interval->half_width) ?
	ANALYZE_OK : ANALYZE_ERROR;
}

int
analyze_sampling_ratio_interval95(struct analyze_sampling_interval *interval,
    const double *replicate_numerators,
    const double *replicate_denominators, size_t replicate_count)
{
    if (!interval || !replicate_numerators || !replicate_denominators ||
        replicate_count < ANALYZE_SAMPLING_MIN_REPLICATES)
	return ANALYZE_ERROR;

    double numerator_sum = 0.0;
    double denominator_sum = 0.0;
    for (size_t i = 0; i < replicate_count; i++) {
	if (!isfinite(replicate_numerators[i]) ||
	    !isfinite(replicate_denominators[i]))
	    return ANALYZE_ERROR;
	numerator_sum += replicate_numerators[i];
	denominator_sum += replicate_denominators[i];
    }
    const double mean_numerator = numerator_sum / (double)replicate_count;
    const double mean_denominator = denominator_sum / (double)replicate_count;
    if (!isfinite(mean_numerator) || !isfinite(mean_denominator) ||
	fabs(mean_denominator) <= DBL_MIN)
	return ANALYZE_ERROR;

    interval->estimate = mean_numerator / mean_denominator;
    double squared_residual = 0.0;
    for (size_t i = 0; i < replicate_count; i++) {
	/* Linearization of Xbar/Ybar retains the paired X,Y covariance. */
	const double residual = (replicate_numerators[i] -
	    interval->estimate * replicate_denominators[i]) / mean_denominator;
	squared_residual += residual * residual;
    }
    const double variance = squared_residual /
	(double)(replicate_count - 1);
    interval->half_width = ANALYZE_STUDENT_T_95_DF9 *
	sqrt(variance / (double)replicate_count);

    return isfinite(interval->estimate) && isfinite(interval->half_width) ?
	ANALYZE_OK : ANALYZE_ERROR;
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
