/*                    P O L Y G O N I Z E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
 *
 */
/** @{ */
/** @file analyze/polygonize.h */

#ifndef ANALYZE_POLYGONIZE_H
#define ANALYZE_POLYGONIZE_H

#include "common.h"
#include "raytrace.h"

#include "analyze/defines.h"

__BEGIN_DECLS
struct analyze_polygonize_params {
    int max_time;
    long int minimum_free_mem;
    int verbosity;
};
#define ANALYZE_POLYGONIZE_PARAMS_DEFAULT { 30, 150000000, 0 }
ANALYZE_EXPORT extern int
analyze_polygonize(int **faces, int *num_faces, point_t **vertices, int *num_vertices, fastf_t size, point_t p_s, const char *obj, struct db_i *dbip, struct analyze_polygonize_params *p);

__END_DECLS

#endif /* ANALYZE_POLYGONIZE_H */

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
