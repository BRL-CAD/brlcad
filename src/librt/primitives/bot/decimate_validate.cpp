/*             D E C I M A T E _ V A L I D A T E . C P P
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "common.h"

#include <cmath>

#include "bg/tri_pt.h"
#include "RTree.h"

#include "./decimate_private.h"

namespace {

struct distance_search {
    const struct rt_bot_internal *source;
    const fastf_t *candidate_vertex;
    fastf_t distance_limit;
    bool within_limit;
};


static bool
check_triangle_distance(const size_t &face, void *context)
{
    distance_search *search = static_cast<distance_search *>(context);
    const int *vertices = &search->source->faces[face * 3];
    double distance = bg_tri_closest_pt(NULL, search->candidate_vertex,
	&search->source->vertices[vertices[0] * 3],
	&search->source->vertices[vertices[1] * 3],
	&search->source->vertices[vertices[2] * 3]);
    if (distance <= search->distance_limit) {
	search->within_limit = true;
	return false;
    }
    return true;
}


static bool
valid_vertex(const fastf_t *vertex)
{
    return std::isfinite(vertex[X]) && std::isfinite(vertex[Y]) &&
	std::isfinite(vertex[Z]);
}

} // namespace


extern "C" int
rt_bot_decimation_is_within_distance(size_t *offending_vertex,
	const struct rt_bot_internal *source,
	const struct rt_bot_internal *candidate, fastf_t max_distance)
{
    if (offending_vertex)
	*offending_vertex = 0;
    if (!source || !candidate || source->magic != RT_BOT_INTERNAL_MAGIC ||
	candidate->magic != RT_BOT_INTERNAL_MAGIC || max_distance < 0.0 ||
	!std::isfinite(max_distance) ||
	(source->num_faces && (!source->faces || !source->vertices ||
	    !source->num_vertices)) ||
	(candidate->num_vertices && !candidate->vertices))
	return -1;
    if (!candidate->num_vertices)
	return 1;
    if (!source->num_faces)
	return 0;

    RTree<size_t, fastf_t, 3> face_boxes;
    for (size_t face = 0; face < source->num_faces; ++face) {
	const int *vertices = &source->faces[face * 3];
	for (size_t corner = 0; corner < 3; ++corner) {
	    int vertex = vertices[corner];
	    if (vertex < 0 || (size_t)vertex >= source->num_vertices ||
		!valid_vertex(&source->vertices[vertex * 3]))
		return -1;
	}

	point_t minimum;
	point_t maximum;
	VMOVE(minimum, &source->vertices[vertices[0] * 3]);
	VMOVE(maximum, &source->vertices[vertices[0] * 3]);
	VMINMAX(minimum, maximum, &source->vertices[vertices[1] * 3]);
	VMINMAX(minimum, maximum, &source->vertices[vertices[2] * 3]);
	face_boxes.Insert(minimum, maximum, face);
    }

    const fastf_t comparison_limit = max_distance + SMALL_FASTF;
    for (size_t vertex = 0; vertex < candidate->num_vertices; ++vertex) {
	const fastf_t *point = &candidate->vertices[vertex * 3];
	if (!valid_vertex(point))
	    return -1;

	point_t minimum;
	point_t maximum;
	for (size_t axis = 0; axis < 3; ++axis) {
	    minimum[axis] = point[axis] - comparison_limit;
	    maximum[axis] = point[axis] + comparison_limit;
	}
	distance_search search = {source, point, comparison_limit, false};
	face_boxes.Search(minimum, maximum, check_triangle_distance, &search);
	if (!search.within_limit) {
	    if (offending_vertex)
		*offending_vertex = vertex;
	    return 0;
	}
    }

    return 1;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
