/*                    S T E P B U D G E T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEPBUDGET_H
#define CONV_STEP_STEPBUDGET_H

#include "common.h"

#include <cstdint>

namespace brlcad {
namespace step {

/** Result of the bounded startup geometry benchmark. */
struct BudgetCalibration {
    bool valid = false;
    uint64_t scalar_queries = 0;
    uint64_t scalar_microseconds = 0;
    uint64_t parallel_cpu_microseconds = 0;
    unsigned int parallel_workers = 0;
    double scalar_queries_per_second = 0.0;
    double parallel_queries_per_second = 0.0;
    double parallel_cpu_queries_per_second = 0.0;
    double scale = 1.0;
};

/** Exercise planar, spherical-NURBS, and toroidal-NURBS preparation and
 * closest-point machinery used by exact STEP pullback.  The benchmark is
 * deterministic and bounded to a short startup interval; max_workers is
 * capped internally. */
BudgetCalibration CalibrateImportBudgets(unsigned int max_workers);

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPBUDGET_H */
