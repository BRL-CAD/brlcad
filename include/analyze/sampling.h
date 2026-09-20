/*                     S A M P L I N G . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @addtogroup libanalyze */
/** @{ */
/** @file analyze/sampling.h */

#ifndef ANALYZE_SAMPLING_H
#define ANALYZE_SAMPLING_H

#include "common.h"
#include "analyze/defines.h"

__BEGIN_DECLS

/** Minimum independent replications accepted by the interval estimator. */
#define ANALYZE_SAMPLING_MIN_REPLICATES 10u

struct analyze_sampling_interval {
    double estimate;   /**< arithmetic mean of the replicate estimates */
    double half_width; /**< approximate two-sided 95% half-width */
};

/**
 * Estimate a pointwise two-sided 95% sampling interval from independent
 * randomized replicate estimates.  At least ANALYZE_SAMPLING_MIN_REPLICATES
 * finite values are required.  The implementation uses the conservative
 * 95% Student-t critical value for nine degrees of freedom for every accepted
 * replicate count.
 *
 * This API is suitable for ordinary Monte Carlo or independently randomized
 * QMC replications.  Consecutive points from one deterministic or randomized
 * QMC sequence are not independent replications.
 *
 * @return ANALYZE_OK on success; ANALYZE_ERROR for invalid input.
 */
ANALYZE_EXPORT extern int analyze_sampling_interval95(
    struct analyze_sampling_interval *interval,
    const double *replicate_estimates,
    size_t replicate_count);

/**
 * Estimate a pointwise two-sided 95% interval for a ratio of replicate means.
 * Numerator and denominator values must be paired independent replications.
 * The point estimate is mean(numerator) / mean(denominator), and the standard
 * error uses their paired first-order delta-method residuals.  This retains
 * numerator/denominator covariance and is appropriate for derived quantities
 * such as a mass centroid.
 *
 * @return ANALYZE_OK on success; ANALYZE_ERROR for invalid input or a zero
 * denominator mean.
 */
ANALYZE_EXPORT extern int analyze_sampling_ratio_interval95(
    struct analyze_sampling_interval *interval,
    const double *replicate_numerators,
    const double *replicate_denominators,
    size_t replicate_count);

__END_DECLS

#endif /* ANALYZE_SAMPLING_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
