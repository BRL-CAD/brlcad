/*                      C O N T O U R . H
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
/** @addtogroup libanalyze
 * @{ */
/** @file analyze/contour.h
 *
 * Ray-driven manifold dual contouring of evaluated BRL-CAD geometry.
 */

#ifndef ANALYZE_CONTOUR_H
#define ANALYZE_CONTOUR_H

#include "common.h"

#include <stddef.h>

#include "vmath.h"
#include "analyze/defines.h"

__BEGIN_DECLS

#define ANALYZE_MDC_DEFAULT_MAX_RAYS 0
#define ANALYZE_MDC_DEFAULT_MINIMUM_FREE_MEM 150000000
#define ANALYZE_MDC_DEFAULT_MIN_DEPTH 2
#define ANALYZE_MDC_DEFAULT_MAX_DEPTH 10
#define ANALYZE_MDC_MAX_DEPTH 20
#define ANALYZE_MDC_DEFAULT_MAX_TIME 600

struct db_i;
/** Resource and accuracy controls for analyze_mdc(). */
struct analyze_mdc_params {
    /** Requested maximum finest-cell edge length.  Zero selects an
     * implementation default based on the model bounds. */
    fastf_t feature_size;

    /** Optional user ceiling for the automatically derived ray budget.
     * Zero applies only geometry, depth, time, and memory limits. */
    size_t max_rays;

    /** Abort before available memory drops below this many bytes. */
    size_t minimum_free_mem;

    /** Minimum and maximum power-of-two grid depths. */
    int min_depth;
    int max_depth;

    /** Maximum wall-clock processing time in seconds.  Zero is unlimited. */
    int max_time;

    /** Emit progress diagnostics when non-zero. */
    int verbosity;
};

#define ANALYZE_MDC_PARAMS_DEFAULT { \
    0.0, \
    ANALYZE_MDC_DEFAULT_MAX_RAYS, \
    ANALYZE_MDC_DEFAULT_MINIMUM_FREE_MEM, \
    ANALYZE_MDC_DEFAULT_MIN_DEPTH, \
    ANALYZE_MDC_DEFAULT_MAX_DEPTH, \
    ANALYZE_MDC_DEFAULT_MAX_TIME, \
    0 \
}

enum analyze_mdc_status {
    ANALYZE_MDC_OK = 0,
    ANALYZE_MDC_INVALID_INPUT = 1,
    ANALYZE_MDC_PREP_FAILED = 2,
    ANALYZE_MDC_NO_SURFACE = 3,
    ANALYZE_MDC_RAY_LIMIT = 4,
    ANALYZE_MDC_TIMEOUT = 5,
    ANALYZE_MDC_MEMORY_LIMIT = 6,
    ANALYZE_MDC_AMBIGUOUS = 7,
    ANALYZE_MDC_NOT_MANIFOLD = 8
};

/**
 * Evaluate @p object with librt and construct an indexed triangle mesh.
 *
 * On success, the caller owns @p faces and @p vertices and must release them
 * with bu_free().  Failure leaves all outputs empty.
 */
ANALYZE_EXPORT extern enum analyze_mdc_status
analyze_mdc(int **faces, size_t *num_faces, point_t **vertices,
	    size_t *num_vertices, const char *object, struct db_i *dbip,
	    const struct analyze_mdc_params *params);

__END_DECLS

#endif /* ANALYZE_CONTOUR_H */

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
