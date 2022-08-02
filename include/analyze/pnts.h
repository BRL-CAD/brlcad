/*                       P N T S . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
 *
 * Using ray intersection, sample the database object obj and return a pnts
 * primitive.
 *
 */
/** @{ */
/** @file include/analyze.h */

#ifndef ANALYZE_PNTS_H
#define ANALYZE_PNTS_H

#include "common.h"
#include "raytrace.h"

#include "analyze/defines.h"

__BEGIN_DECLS

/**
 * Using ray intersection, sample the database object obj and return a pnts
 * primitive.
 *
 * For the grid sampling method, the tolerance sets the number of rays fired.
 * max_time and max_pnts do *not* impact the GRID sampling logic.
 *
 * The max_pnts limit will cap the number of reported points for the
 * pseudorandom sampling methods, on a per method basis - i.e., the function
 * will return up to max_pnts for each non-grid sampling method that is
 * enabled.  If unset, the maximum pnt count return is 500,000 per method
 * (except for GRID).
 *
 * Likewise, max_time will limit the run time of each pseudorandom method, with
 * the total limit for all methods being method_cnt_enabled * max_time.
 *
 * Return codes:
 *
 * -1 - error
 *  0 - success
 *
 */
#define ANALYZE_OBJ_TO_PNTS_SURF  0x1 /**< @brief save only the first and last hit point on a ray */
#define ANALYZE_OBJ_TO_PNTS_GRID  0x2 /**< @brief sample using an XYZ grid based on the bounding box (default if no method flags are specified) */
#define ANALYZE_OBJ_TO_PNTS_RAND  0x4 /**< @brief sample using Marsaglia sampling on the bounding sphere with pseudo random numbers */
#define ANALYZE_OBJ_TO_PNTS_SOBOL 0x8 /**< @brief sample using Marsaglia sampling on the bounding sphere with Sobol' low-discrepancy-sequence generation */
ANALYZE_EXPORT int analyze_obj_to_pnts(struct rt_pnts_internal *rpnts, double *avg_thickness, struct db_i *dbip,
				       const char *obj, struct bn_tol *tol, int flags, int max_pnts, int max_time, int verbosity);

__END_DECLS

#endif /* ANALYZE_PNTS_H */

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
