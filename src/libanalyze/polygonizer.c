/*                  P O L Y G O N I Z E R . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file polygonizer.c
 *
 * Compatibility interface for the ray-driven manifold dual contourer.
 */

#include "common.h"

#include <limits.h>
#include <stdint.h>

#include "analyze/contour.h"
#include "analyze/polygonize.h"
#include "bu/malloc.h"


static int
polygonize_status(enum analyze_mdc_status status)
{
    if (status == ANALYZE_MDC_TIMEOUT)
	return 2;
    if (status == ANALYZE_MDC_MEMORY_LIMIT)
	return 3;
    return -1;
}


int
analyze_polygonize(int **faces, int *num_faces, point_t **vertices,
	int *num_vertices, fastf_t size, point_t seed, const char *object,
	struct db_i *dbip, struct analyze_polygonize_params *parameters)
{
    const struct analyze_polygonize_params defaults =
	ANALYZE_POLYGONIZE_PARAMS_DEFAULT;
    const struct analyze_polygonize_params *legacy =
	parameters ? parameters : &defaults;
    struct analyze_mdc_params mdc = ANALYZE_MDC_PARAMS_DEFAULT;
    size_t face_count = 0;
    size_t vertex_count = 0;
    int64_t time_limit = 0;
    enum analyze_mdc_status status;

    (void)seed;

    if (faces)
	*faces = NULL;
    if (num_faces)
	*num_faces = 0;
    if (vertices)
	*vertices = NULL;
    if (num_vertices)
	*num_vertices = 0;
    if (!faces || !num_faces || !vertices || !num_vertices)
	return -1;

    if (legacy->max_time > 0) {
	time_limit = (int64_t)legacy->max_time - legacy->time_offset;
	if (time_limit <= 0)
	    return 2;
    }
    if (legacy->max_cycle_time > 0 &&
	    (time_limit == 0 || legacy->max_cycle_time < time_limit))
	time_limit = legacy->max_cycle_time;
    if (time_limit > INT_MAX)
	time_limit = INT_MAX;

    mdc.feature_size = size;
    mdc.minimum_free_mem = legacy->minimum_free_mem;
    mdc.max_time = (int)time_limit;
    mdc.verbosity = legacy->verbosity;

    status = analyze_mdc(faces, &face_count, vertices, &vertex_count,
	    object, dbip, &mdc);
    if (status != ANALYZE_MDC_OK)
	return polygonize_status(status);

    if (face_count > INT_MAX || vertex_count > INT_MAX) {
	bu_free(*faces, "polygonize compatibility faces");
	bu_free(*vertices, "polygonize compatibility vertices");
	*faces = NULL;
	*vertices = NULL;
	return -1;
    }

    *num_faces = (int)face_count;
    *num_vertices = (int)vertex_count;
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
