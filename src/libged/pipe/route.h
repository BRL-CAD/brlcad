/*                        R O U T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/pipe/route.h
 *
 * Internal routing helpers for the PIPE command.
 */

#ifndef LIBGED_PIPE_ROUTE_H
#define LIBGED_PIPE_ROUTE_H

#include "common.h"

#include <stddef.h>

#include "bu/vls.h"
#include "vmath.h"

struct ged;

enum pipe_route_profile {
    PIPE_ROUTE_PROFILE_RIGID,
    PIPE_ROUTE_PROFILE_HOSE,
    PIPE_ROUTE_PROFILE_HARNESS
};

enum pipe_route_status {
    PIPE_ROUTE_OK = 0,
    PIPE_ROUTE_NO_PATH = 1,
    PIPE_ROUTE_INVALID = 2
};

struct pipe_route_path {
    point_t *points;
    size_t count;
    size_t capacity;
};

struct pipe_route_obstacle {
    const char *name;
    point_t minimum;
    point_t maximum;
};

struct pipe_route_options {
    enum pipe_route_profile profile;
    fastf_t outer_diameter;
    fastf_t inner_diameter;
    fastf_t bend_radius;
    fastf_t clearance;
    fastf_t grid_size;
    fastf_t wire_spacing;
    size_t wire_count;
    size_t max_nodes;
};

struct pipe_route_metrics {
    fastf_t centerline_length;
    fastf_t bend_allowance;
    fastf_t minimum_bend_radius;
    size_t elbow_count;
};

void pipe_route_path_init(struct pipe_route_path *path);
void pipe_route_path_free(struct pipe_route_path *path);
int pipe_route_path_append(struct pipe_route_path *path, const point_t point);
int pipe_route_path_copy(struct pipe_route_path *destination, const struct pipe_route_path *source);

int pipe_route_solve(struct pipe_route_path *route,
	const struct pipe_route_path *guide,
	const struct pipe_route_options *options,
	const struct pipe_route_obstacle *obstacles,
	size_t obstacle_count,
	struct pipe_route_metrics *metrics,
	struct bu_vls *message);

int pipe_route_make_direct(struct pipe_route_path *route,
	const struct pipe_route_path *guide,
	const struct pipe_route_options *options,
	struct pipe_route_metrics *metrics,
	struct bu_vls *message);

int pipe_route_validate(const struct pipe_route_path *route,
	const struct pipe_route_options *options,
	const struct pipe_route_obstacle *obstacles,
	size_t obstacle_count,
	int check_obstacles,
	struct pipe_route_metrics *metrics,
	struct bu_vls *message);

int pipe_route_path_intersects_obstacle(const struct pipe_route_path *route,
	const struct pipe_route_obstacle *obstacle,
	fastf_t expansion);

int pipe_route_paths_clear(const struct pipe_route_path *first,
	const struct pipe_route_path *second,
	const point_t shared_point,
	const struct pipe_route_options *options,
	struct bu_vls *message);

int pipe_route_make_flat_wire_path(struct pipe_route_path *wire,
	const struct pipe_route_path *centerline,
	size_t wire_index,
	size_t wire_count,
	fastf_t spacing,
	struct bu_vls *message);

int ged_pipe_route_core(struct ged *gedp, int argc, const char *argv[]);

#endif /* LIBGED_PIPE_ROUTE_H */
