/*                          A N A L Y Z E . H
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
/** @addtogroup ged_analyze
 *
 * Geometry EDiting Library Database Analysis Related Functions.
 *
 */
/** @{ */
/** @file ged/analyze.h */

#ifndef GED_ANALYZE_H
#define GED_ANALYZE_H

#include "common.h"
#include "ged/defines.h"

__BEGIN_DECLS


/**
 * Returns lots of information about the specified object(s)
 */
GED_EXPORT extern int ged_analyze(struct ged *gedp, int argc, const char *argv[]);


/**
 * Report the size of the bounding box (rpp) around the specified object
 */
GED_EXPORT extern int ged_bb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Analyzes the geometry
 */
GED_EXPORT extern int ged_check(struct ged *gedp, int argc, const char *argv[]);

/**
 * Calculate a geometry diff
 */
GED_EXPORT extern int ged_gdiff(struct ged *gedp, int argc, const char *argv[]);

/**
 * General quantitative analyzer
 */
GED_EXPORT extern int ged_gqa(struct ged *gedp, int argc, const char *argv[]);

/**
 * Trace a single ray from the current view.
 */
GED_EXPORT extern int ged_nirt(struct ged *gedp, int argc, const char *argv[]);
GED_EXPORT extern int ged_vnirt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set query_ray attributes
 */
GED_EXPORT extern int ged_qray(struct ged *gedp, int argc, const char *argv[]);

GED_EXPORT extern void ged_init_qray(struct ged_drawable *gdp);
GED_EXPORT extern void ged_free_qray(struct ged_drawable *gdp);

/**
 * Check for overlaps in the current view.
 */
GED_EXPORT extern int ged_rtcheck(struct ged *gedp, int argc, const char *argv[]);

/**
 * Performs simulations.
 */
GED_EXPORT extern int ged_simulate(struct ged *gedp, int argc, const char *argv[]);

GED_EXPORT extern int ged_solids_on_ray(struct ged *gedp, int argc, const char *argv[]);



__END_DECLS

#endif /* GED_ANALYZE_H */

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
