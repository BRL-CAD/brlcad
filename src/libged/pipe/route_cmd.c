/*                    R O U T E _ C M D . C
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
/** @file libged/pipe/route_cmd.c
 *
 * GED command adapter for PIPE routing.
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "rt/calc.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"

#include "../ged_private.h"
#include "route.h"


#define PIPE_ROUTE_DEFAULT_OUTER_DIAMETER 10.0
#define PIPE_ROUTE_DEFAULT_BEND_MULTIPLIER 1.5
#define PIPE_ROUTE_DEFAULT_CLEARANCE 0.0
#define PIPE_ROUTE_DEFAULT_GRID_BEND_MULTIPLIER 4.0
#define PIPE_ROUTE_DEFAULT_WIRE_SPACING_MULTIPLIER 1.25
#define PIPE_ROUTE_DEFAULT_MAX_NODES 250000
#define PIPE_ROUTE_INITIAL_REQUEST_CAPACITY 4
#define PIPE_ROUTE_CUTTER_SUFFIX ".clearance.s"


enum pipe_route_fallback {
    PIPE_ROUTE_FALLBACK_ERROR,
    PIPE_ROUTE_FALLBACK_DIRECT,
    PIPE_ROUTE_FALLBACK_SUBTRACT
};

struct pipe_route_tee {
    point_t attachment;
    point_t endpoint;
};

struct pipe_route_request {
    const char *output_name;
    const char *guide_name;
    enum pipe_route_fallback fallback;
    struct pipe_route_options options;
    struct pipe_route_path guide;
    struct pipe_route_tee *tees;
    size_t tee_count;
    size_t tee_capacity;
    const char **avoid_names;
    size_t avoid_count;
    size_t avoid_capacity;
    fastf_t wall_thickness;
    int bend_set;
    int grid_set;
    int spacing_set;
    int wires_set;
    int inner_set;
    int wall_set;
};


static void
pipe_route_usage(struct ged *gedp)
{
    bu_vls_printf(gedp->ged_result_str,
	"Usage: pipe route [options] name {x y z} {x y z} [guide-point ...]\n"
	"       pipe route [options] --guide source_pipe name\n"
	"Options: --profile rigid|hose|harness --diameter d --inner-diameter d --wall t\n"
	"         --bend r --clearance c --grid s --avoid object --tee {x y z} {x y z}\n"
	"         --wires n --spacing s --max-nodes n --fallback error|direct|subtract\n");
}


static void
pipe_route_request_init(struct pipe_route_request *request)
{
    memset(request, 0, sizeof(*request));
    request->fallback = PIPE_ROUTE_FALLBACK_ERROR;
    request->options.profile = PIPE_ROUTE_PROFILE_RIGID;
    request->options.outer_diameter = PIPE_ROUTE_DEFAULT_OUTER_DIAMETER;
    request->options.clearance = PIPE_ROUTE_DEFAULT_CLEARANCE;
    request->options.wire_count = 1;
    request->options.max_nodes = PIPE_ROUTE_DEFAULT_MAX_NODES;
    pipe_route_path_init(&request->guide);
}


static void
pipe_route_request_free(struct pipe_route_request *request)
{
    if (request->tees)
	bu_free(request->tees, "pipe route tees");
    if (request->avoid_names)
	bu_free(request->avoid_names, "pipe route avoid names");
    pipe_route_path_free(&request->guide);
    pipe_route_request_init(request);
}


static int
pipe_route_request_add_tee(struct pipe_route_request *request, const point_t attachment, const point_t endpoint)
{
    if (request->tee_count == request->tee_capacity) {
	size_t capacity = request->tee_capacity ? request->tee_capacity * 2 : PIPE_ROUTE_INITIAL_REQUEST_CAPACITY;
	if (capacity < request->tee_capacity || capacity > SIZE_MAX / sizeof(struct pipe_route_tee))
	    return PIPE_ROUTE_INVALID;
	request->tees = (struct pipe_route_tee *)bu_realloc(request->tees, capacity * sizeof(struct pipe_route_tee), "pipe route tees");
	request->tee_capacity = capacity;
    }

    VMOVE(request->tees[request->tee_count].attachment, attachment);
    VMOVE(request->tees[request->tee_count].endpoint, endpoint);
    request->tee_count++;
    return PIPE_ROUTE_OK;
}


static int
pipe_route_request_add_avoid(struct pipe_route_request *request, const char *name)
{
    if (request->avoid_count == request->avoid_capacity) {
	size_t capacity = request->avoid_capacity ? request->avoid_capacity * 2 : PIPE_ROUTE_INITIAL_REQUEST_CAPACITY;
	if (capacity < request->avoid_capacity || capacity > SIZE_MAX / sizeof(const char *))
	    return PIPE_ROUTE_INVALID;
	request->avoid_names = (const char **)bu_realloc(request->avoid_names, capacity * sizeof(const char *), "pipe route avoid names");
	request->avoid_capacity = capacity;
    }

    request->avoid_names[request->avoid_count++] = name;
    return PIPE_ROUTE_OK;
}


static int
pipe_route_parse_scalar(struct ged *gedp, const char *text, fastf_t *value)
{
    struct bu_vls parse_message = BU_VLS_INIT_ZERO;
    const char *values[] = {text};
    int result;

    result = bu_opt_fastf_t(&parse_message, 1, values, value);
    bu_vls_free(&parse_message);
    if (result != 1 || !isfinite(*value)) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: expected a finite number, got %s", text);
	return PIPE_ROUTE_INVALID;
    }

    return PIPE_ROUTE_OK;
}


static int
pipe_route_parse_count(struct ged *gedp, const char *text, size_t *value)
{
    char *end = NULL;
    unsigned long long parsed;

    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno || !end || *end || parsed == 0 || parsed > SIZE_MAX) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: expected a positive integer, got %s", text);
	return PIPE_ROUTE_INVALID;
    }

    *value = (size_t)parsed;
    return PIPE_ROUTE_OK;
}


static int
pipe_route_parse_point(struct ged *gedp, const char *text, point_t point)
{
    struct bu_vls parse_message = BU_VLS_INIT_ZERO;
    const char *values[] = {text};
    vect_t local_point;
    int result;

    result = bu_opt_vect_t(&parse_message, 1, values, &local_point);
    bu_vls_free(&parse_message);
    if (result != 1 || !isfinite(local_point[X]) || !isfinite(local_point[Y]) || !isfinite(local_point[Z])) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: expected a point {x y z}, got %s", text);
	return PIPE_ROUTE_INVALID;
    }

    VSCALE(point, local_point, gedp->dbip->dbi_local2base);
    return PIPE_ROUTE_OK;
}

static int
pipe_route_parse_profile(struct ged *gedp, const char *text, enum pipe_route_profile *profile)
{
    if (BU_STR_EQUAL(text, "rigid")) {
	*profile = PIPE_ROUTE_PROFILE_RIGID;
    } else if (BU_STR_EQUAL(text, "hose")) {
	*profile = PIPE_ROUTE_PROFILE_HOSE;
    } else if (BU_STR_EQUAL(text, "harness")) {
	*profile = PIPE_ROUTE_PROFILE_HARNESS;
    } else {
	bu_vls_printf(gedp->ged_result_str, "pipe route: unknown profile %s", text);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

static int
pipe_route_parse_fallback(struct ged *gedp, const char *text, enum pipe_route_fallback *fallback)
{
    if (BU_STR_EQUAL(text, "error")) {
	*fallback = PIPE_ROUTE_FALLBACK_ERROR;
    } else if (BU_STR_EQUAL(text, "direct")) {
	*fallback = PIPE_ROUTE_FALLBACK_DIRECT;
    } else if (BU_STR_EQUAL(text, "subtract")) {
	*fallback = PIPE_ROUTE_FALLBACK_SUBTRACT;
    } else {
	bu_vls_printf(gedp->ged_result_str, "pipe route: unknown fallback %s", text);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

static int
pipe_route_next_option_value(struct ged *gedp, int argc, const char *argv[], int *argument_index, const char **value)
{
    if (*argument_index + 1 >= argc) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: %s needs a value", argv[*argument_index]);
	return BRLCAD_ERROR;
    }

    *value = argv[++*argument_index];
    return BRLCAD_OK;
}

static int
pipe_route_parse_args(struct ged *gedp, int argc, const char *argv[], struct pipe_route_request *request)
{
    int argument_index;

    for (argument_index = 1; argument_index < argc; argument_index++) {
	const char *argument = argv[argument_index];
	const char *value;

	if (BU_STR_EQUAL(argument, "--help") || BU_STR_EQUAL(argument, "-h")) {
	    pipe_route_usage(gedp);
	    return GED_HELP;
	}
	if (BU_STR_EQUAL(argument, "--profile")) {
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_parse_profile(gedp, value, &request->options.profile) != BRLCAD_OK)
		return BRLCAD_ERROR;
	    continue;
	}
	if (BU_STR_EQUAL(argument, "--diameter")) {
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_parse_scalar(gedp, value, &request->options.outer_diameter) != PIPE_ROUTE_OK)
		return BRLCAD_ERROR;
	    continue;
	}
	if (BU_STR_EQUAL(argument, "--inner-diameter")) {
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_parse_scalar(gedp, value, &request->options.inner_diameter) != PIPE_ROUTE_OK)
		return BRLCAD_ERROR;
	    request->inner_set = 1;
	    continue;
	}
	if (BU_STR_EQUAL(argument, "--wall")) {
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_parse_scalar(gedp, value, &request->wall_thickness) != PIPE_ROUTE_OK)
		return BRLCAD_ERROR;
	    request->wall_set = 1;
	    continue;
	}
	if (BU_STR_EQUAL(argument, "--bend")) {
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_parse_scalar(gedp, value, &request->options.bend_radius) != PIPE_ROUTE_OK)
		return BRLCAD_ERROR;
	    request->bend_set = 1;
	    continue;
	}
	if (BU_STR_EQUAL(argument, "--clearance")) {
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_parse_scalar(gedp, value, &request->options.clearance) != PIPE_ROUTE_OK)
		return BRLCAD_ERROR;
	    continue;
	}
	if (BU_STR_EQUAL(argument, "--grid")) {
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_parse_scalar(gedp, value, &request->options.grid_size) != PIPE_ROUTE_OK)
		return BRLCAD_ERROR;
	    request->grid_set = 1;
	    continue;
	}
	if (BU_STR_EQUAL(argument, "--avoid")) {
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_request_add_avoid(request, value) != PIPE_ROUTE_OK)
		return BRLCAD_ERROR;
	    continue;
	}
	if (BU_STR_EQUAL(argument, "--guide")) {
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK)
		return BRLCAD_ERROR;
	    request->guide_name = value;
	    continue;
	}
	if (BU_STR_EQUAL(argument, "--wires")) {
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_parse_count(gedp, value, &request->options.wire_count) != PIPE_ROUTE_OK)
		return BRLCAD_ERROR;
	    request->wires_set = 1;
	    continue;
	}
	if (BU_STR_EQUAL(argument, "--spacing")) {
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_parse_scalar(gedp, value, &request->options.wire_spacing) != PIPE_ROUTE_OK)
		return BRLCAD_ERROR;
	    request->spacing_set = 1;
	    continue;
	}
	if (BU_STR_EQUAL(argument, "--max-nodes")) {
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_parse_count(gedp, value, &request->options.max_nodes) != PIPE_ROUTE_OK)
		return BRLCAD_ERROR;
	    continue;
	}
	if (BU_STR_EQUAL(argument, "--fallback")) {
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_parse_fallback(gedp, value, &request->fallback) != BRLCAD_OK)
		return BRLCAD_ERROR;
	    continue;
	}
	if (BU_STR_EQUAL(argument, "--tee")) {
	    point_t attachment;
	    point_t endpoint;

	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_parse_point(gedp, value, attachment) != PIPE_ROUTE_OK)
		return BRLCAD_ERROR;
	    if (pipe_route_next_option_value(gedp, argc, argv, &argument_index, &value) != BRLCAD_OK ||
		pipe_route_parse_point(gedp, value, endpoint) != PIPE_ROUTE_OK)
		return BRLCAD_ERROR;
	    if (pipe_route_request_add_tee(request, attachment, endpoint) != PIPE_ROUTE_OK) {
		bu_vls_printf(gedp->ged_result_str, "pipe route: too many tee branches");
		return BRLCAD_ERROR;
	    }
	    continue;
	}
	if (argument[0] == '-' && argument[1] == '-') {
	    bu_vls_printf(gedp->ged_result_str, "pipe route: unknown option %s", argument);
	    return BRLCAD_ERROR;
	}

	if (!request->output_name) {
	    request->output_name = argument;
	} else {
	    point_t point;
	    if (pipe_route_parse_point(gedp, argument, point) != PIPE_ROUTE_OK)
		return BRLCAD_ERROR;
	    if (pipe_route_path_append(&request->guide, point) != PIPE_ROUTE_OK) {
		bu_vls_printf(gedp->ged_result_str, "pipe route: unable to allocate a guide point");
		return BRLCAD_ERROR;
	    }
	}
    }

    if (!request->output_name || (request->guide_name && request->guide.count)) {
	pipe_route_usage(gedp);
	return BRLCAD_ERROR;
    }
    if (!request->guide_name && request->guide.count < 2) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: specify at least two guide points");
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
pipe_route_finalize_options(struct ged *gedp, struct pipe_route_request *request)
{
    fastf_t local2base = gedp->dbip->dbi_local2base;
    struct pipe_route_options *options = &request->options;

    if (request->inner_set && request->wall_set) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: --inner-diameter and --wall cannot be combined");
	return BRLCAD_ERROR;
    }
    if (options->outer_diameter <= 0.0 || options->clearance < 0.0 || (request->wall_set && request->wall_thickness < 0.0)) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: diameters, wall thickness, and clearance must be non-negative with a positive outer diameter");
	return BRLCAD_ERROR;
    }
    if (request->wall_set)
	options->inner_diameter = options->outer_diameter - 2.0 * request->wall_thickness;
    if (!request->bend_set)
	options->bend_radius = PIPE_ROUTE_DEFAULT_BEND_MULTIPLIER * options->outer_diameter;
    if (!request->grid_set)
	options->grid_size = PIPE_ROUTE_DEFAULT_GRID_BEND_MULTIPLIER * options->bend_radius;
    if (!request->spacing_set)
	options->wire_spacing = PIPE_ROUTE_DEFAULT_WIRE_SPACING_MULTIPLIER * options->outer_diameter;
	if (options->profile != PIPE_ROUTE_PROFILE_HARNESS && (request->wires_set || request->spacing_set)) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: --wires and --spacing are only valid with --profile harness");
	return BRLCAD_ERROR;
	}
	if (options->profile == PIPE_ROUTE_PROFILE_HARNESS && (request->inner_set || request->wall_set)) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: harness wires are solid; --inner-diameter and --wall are not supported");
	return BRLCAD_ERROR;
    }
    if (options->profile == PIPE_ROUTE_PROFILE_HARNESS && request->tee_count) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: tee branches are not supported for flattened harnesses");
	return BRLCAD_ERROR;
    }

    options->outer_diameter *= local2base;
    options->inner_diameter *= local2base;
    options->bend_radius *= local2base;
    options->clearance *= local2base;
    options->grid_size *= local2base;
    options->wire_spacing *= local2base;
    return BRLCAD_OK;
}

static int
pipe_route_load_guide(struct ged *gedp, const char *name, struct pipe_route_path *guide)
{
    struct rt_db_internal internal;
    struct rt_pipe_internal *pipe_internal;
    struct wdb_pipe_pnt *pipe_point;
    struct rt_wdb *wdbp;
    mat_t matrix;

    RT_DB_INTERNAL_INIT(&internal);
    wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_import_from_path2(gedp->ged_result_str, &internal, name, wdbp, matrix) & BRLCAD_ERROR)
	return BRLCAD_ERROR;
    if (internal.idb_major_type != DB5_MAJORTYPE_BRLCAD || internal.idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: guide %s is not a PIPE primitive", name);
	rt_db_free_internal(&internal);
	return BRLCAD_ERROR;
    }

    pipe_internal = (struct rt_pipe_internal *)internal.idb_ptr;
    for (BU_LIST_FOR(pipe_point, wdb_pipe_pnt, &pipe_internal->pipe_segs_head)) {
	if (pipe_route_path_append(guide, pipe_point->pp_coord) != PIPE_ROUTE_OK) {
	    bu_vls_printf(gedp->ged_result_str, "pipe route: out of memory while reading guide %s", name);
	    rt_db_free_internal(&internal);
	    return BRLCAD_ERROR;
	}
    }
    rt_db_free_internal(&internal);

    if (guide->count < 2) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: guide %s needs at least two PIPE points", name);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static fastf_t
pipe_route_command_distance(const point_t first, const point_t second)
{
    vect_t direction;

    VSUB2(direction, second, first);
    return MAGNITUDE(direction);
}

static fastf_t
pipe_route_attachment_distance(const point_t point, const point_t start, const point_t end)
{
    vect_t direction;
    vect_t point_offset;
    point_t closest;
    fastf_t length_squared;
    fastf_t parameter;

    VSUB2(direction, end, start);
    VSUB2(point_offset, point, start);
    length_squared = MAGSQ(direction);
    if (length_squared <= SMALL_FASTF)
	return pipe_route_command_distance(point, start);

    parameter = VDOT(point_offset, direction) / length_squared;
    if (parameter < 0.0)
	parameter = 0.0;
    if (parameter > 1.0)
	parameter = 1.0;
    VJOIN1(closest, start, parameter, direction);
    return pipe_route_command_distance(point, closest);
}

static int
pipe_route_insert_attachment(struct ged *gedp, struct pipe_route_path *guide, const point_t attachment)
{
    size_t point_index;
    size_t insertion_index = 1;
    fastf_t nearest_distance = INFINITY;

    for (point_index = 0; point_index < guide->count; point_index++) {
	if (pipe_route_command_distance(guide->points[point_index], attachment) <= SMALL_FASTF)
	    return BRLCAD_OK;
    }
    for (point_index = 0; point_index + 1 < guide->count; point_index++) {
	fastf_t distance = pipe_route_attachment_distance(attachment, guide->points[point_index], guide->points[point_index + 1]);
	if (distance < nearest_distance) {
	    nearest_distance = distance;
	    insertion_index = point_index + 1;
	}
    }

    if (pipe_route_path_append(guide, guide->points[guide->count - 1]) != PIPE_ROUTE_OK) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: out of memory while inserting tee attachment");
	return BRLCAD_ERROR;
    }
    for (point_index = guide->count - 1; point_index > insertion_index; point_index--)
	VMOVE(guide->points[point_index], guide->points[point_index - 1]);
    VMOVE(guide->points[insertion_index], attachment);
    return BRLCAD_OK;
}

static int
pipe_route_insert_tee_attachments(struct ged *gedp, struct pipe_route_request *request)
{
    size_t tee_index;

    for (tee_index = 0; tee_index < request->tee_count; tee_index++) {
	if (pipe_route_insert_attachment(gedp, &request->guide, request->tees[tee_index].attachment) != BRLCAD_OK)
	    return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
pipe_route_load_obstacles(struct ged *gedp,
	const struct pipe_route_request *request,
	struct pipe_route_obstacle **obstacles)
{
    struct pipe_route_obstacle *loaded;
    size_t obstacle_index;

    *obstacles = NULL;
    if (!request->avoid_count)
	return BRLCAD_OK;

    loaded = (struct pipe_route_obstacle *)bu_calloc(request->avoid_count, sizeof(struct pipe_route_obstacle), "pipe route obstacles");
    for (obstacle_index = 0; obstacle_index < request->avoid_count; obstacle_index++) {
	struct bu_vls bounds_message = BU_VLS_INIT_ZERO;
	const char *names[] = {request->avoid_names[obstacle_index]};

	if (rt_obj_bounds(&bounds_message, gedp->dbip, 1, names, 0, loaded[obstacle_index].minimum, loaded[obstacle_index].maximum) != BRLCAD_OK) {
	    bu_vls_printf(gedp->ged_result_str, "pipe route: unable to bound keep-out %s: %s", request->avoid_names[obstacle_index], bu_vls_cstr(&bounds_message));
	    bu_vls_free(&bounds_message);
	    bu_free(loaded, "pipe route obstacles");
	    return BRLCAD_ERROR;
	}
	bu_vls_free(&bounds_message);
	loaded[obstacle_index].name = request->avoid_names[obstacle_index];
    }

    *obstacles = loaded;
    return BRLCAD_OK;
}

static int
pipe_route_name_unused(struct ged *gedp, const char *name)
{
    if (!name || !name[0]) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: output name must not be empty");
	return BRLCAD_ERROR;
    }
    if (db_lookup(gedp->dbip, name, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: output object %s already exists", name);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
pipe_route_write_pipe(struct ged *gedp,
	struct rt_wdb *wdbp,
	const char *name,
	const struct pipe_route_path *route,
	const struct pipe_route_options *options)
{
    struct bu_list head;
    size_t point_index;

    mk_pipe_init(&head);
    for (point_index = 0; point_index < route->count; point_index++)
	mk_add_pipe_pnt(&head, route->points[point_index], options->outer_diameter, options->inner_diameter, options->bend_radius);

    if (rt_pipe_ck(&head)) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: generated route %s violates PIPE constraints", name);
	mk_pipe_free(&head);
	return BRLCAD_ERROR;
    }
    if (mk_pipe(wdbp, name, &head)) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: failed to create %s", name);
	mk_pipe_free(&head);
	return BRLCAD_ERROR;
    }
    mk_pipe_free(&head);
    return BRLCAD_OK;
}

static int
pipe_route_write_group(struct ged *gedp,
	struct rt_wdb *wdbp,
	const char *name,
	const char *const *members,
	size_t member_count)
{
    struct wmember group;
    size_t member_index;

    BU_LIST_INIT(&group.l);
    for (member_index = 0; member_index < member_count; member_index++)
	(void)mk_addmember(members[member_index], &group.l, NULL, WMOP_UNION);
    if (mk_lcomb(wdbp, name, &group, 0, NULL, NULL, NULL, 0)) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: failed to create group %s", name);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static void
pipe_route_format_indexed_name(struct bu_vls *name, const char *base, const char *kind, size_t index)
{
    bu_vls_sprintf(name, "%s.%s%02zu.s", base, kind, index + 1);
}

static int
pipe_route_write_harness(struct ged *gedp,
	struct rt_wdb *wdbp,
	const struct pipe_route_request *request,
	const struct pipe_route_path *centerline)
{
    struct pipe_route_options wire_options = request->options;
    struct bu_vls *wire_names;
    const char **members;
    struct pipe_route_path wire;
    size_t wire_index;
    size_t wire_name_count = 0;
    int status = BRLCAD_ERROR;

    if (pipe_route_name_unused(gedp, request->output_name) != BRLCAD_OK)
	return BRLCAD_ERROR;

    wire_names = (struct bu_vls *)bu_calloc(request->options.wire_count, sizeof(struct bu_vls), "pipe route wire names");
    members = (const char **)bu_calloc(request->options.wire_count, sizeof(const char *), "pipe route harness members");
    for (wire_index = 0; wire_index < request->options.wire_count; wire_index++) {
	bu_vls_init(&wire_names[wire_index]);
	wire_name_count++;
	pipe_route_format_indexed_name(&wire_names[wire_index], request->output_name, "wire", wire_index);
	if (pipe_route_name_unused(gedp, bu_vls_cstr(&wire_names[wire_index])) != BRLCAD_OK)
	    goto cleanup;
	members[wire_index] = bu_vls_cstr(&wire_names[wire_index]);
    }

    wire_options.profile = PIPE_ROUTE_PROFILE_RIGID;
    wire_options.wire_count = 1;
    wire_options.wire_spacing = 0.0;
    pipe_route_path_init(&wire);
    for (wire_index = 0; wire_index < request->options.wire_count; wire_index++) {
	if (pipe_route_make_flat_wire_path(&wire, centerline, wire_index, request->options.wire_count, request->options.wire_spacing, gedp->ged_result_str) != PIPE_ROUTE_OK ||
	    pipe_route_validate(&wire, &wire_options, NULL, 0, 0, NULL, gedp->ged_result_str) != PIPE_ROUTE_OK ||
	    pipe_route_write_pipe(gedp, wdbp, members[wire_index], &wire, &wire_options) != BRLCAD_OK) {
	    pipe_route_path_free(&wire);
	    goto cleanup;
	}
	pipe_route_path_free(&wire);
	pipe_route_path_init(&wire);
    }
    pipe_route_path_free(&wire);

    status = pipe_route_write_group(gedp, wdbp, request->output_name, members, request->options.wire_count);

cleanup:
	for (wire_index = 0; wire_index < wire_name_count; wire_index++)
	bu_vls_free(&wire_names[wire_index]);
    bu_free(wire_names, "pipe route wire names");
    bu_free(members, "pipe route harness members");
    return status;
}

static int
pipe_route_write_tee_group(struct ged *gedp,
	struct rt_wdb *wdbp,
	const struct pipe_route_request *request,
	const struct pipe_route_path *main_route,
	const struct pipe_route_path *branch_routes)
{
    struct bu_vls main_name = BU_VLS_INIT_ZERO;
    struct bu_vls *branch_names;
    const char **members;
    size_t tee_index;
    size_t branch_name_count = 0;
    int status = BRLCAD_ERROR;

    if (pipe_route_name_unused(gedp, request->output_name) != BRLCAD_OK)
	return BRLCAD_ERROR;

    bu_vls_sprintf(&main_name, "%s.main.s", request->output_name);
    if (pipe_route_name_unused(gedp, bu_vls_cstr(&main_name)) != BRLCAD_OK)
	goto cleanup;

    branch_names = (struct bu_vls *)bu_calloc(request->tee_count, sizeof(struct bu_vls), "pipe route tee names");
    members = (const char **)bu_calloc(request->tee_count + 1, sizeof(const char *), "pipe route tee members");
    members[0] = bu_vls_cstr(&main_name);
    for (tee_index = 0; tee_index < request->tee_count; tee_index++) {
	bu_vls_init(&branch_names[tee_index]);
	branch_name_count++;
	pipe_route_format_indexed_name(&branch_names[tee_index], request->output_name, "tee", tee_index);
	if (pipe_route_name_unused(gedp, bu_vls_cstr(&branch_names[tee_index])) != BRLCAD_OK)
	    goto branch_cleanup;
	members[tee_index + 1] = bu_vls_cstr(&branch_names[tee_index]);
    }

    if (pipe_route_write_pipe(gedp, wdbp, members[0], main_route, &request->options) != BRLCAD_OK)
	goto branch_cleanup;
    for (tee_index = 0; tee_index < request->tee_count; tee_index++) {
	if (pipe_route_write_pipe(gedp, wdbp, members[tee_index + 1], &branch_routes[tee_index], &request->options) != BRLCAD_OK)
	    goto branch_cleanup;
    }
    status = pipe_route_write_group(gedp, wdbp, request->output_name, members, request->tee_count + 1);

branch_cleanup:
	for (tee_index = 0; tee_index < branch_name_count; tee_index++)
	bu_vls_free(&branch_names[tee_index]);
    bu_free(branch_names, "pipe route tee names");
    bu_free(members, "pipe route tee members");
cleanup:
    bu_vls_free(&main_name);
    return status;
}

static int
pipe_route_write_subtractions(struct ged *gedp,
	struct rt_wdb *wdbp,
	const struct pipe_route_request *request,
	const struct pipe_route_path *route,
	const struct pipe_route_obstacle *obstacles,
	size_t obstacle_count,
	size_t *cut_count)
{
    struct pipe_route_options cutter_options = request->options;
    struct bu_vls cutter_name = BU_VLS_INIT_ZERO;
    struct bu_vls *cut_names = NULL;
    size_t obstacle_index;
    size_t cut_name_count = 0;
    int have_cuts = 0;

    *cut_count = 0;
    cutter_options.outer_diameter += 2.0 * cutter_options.clearance;
    cutter_options.inner_diameter = 0.0;
    if (pipe_route_validate(route, &cutter_options, NULL, 0, 0, NULL, gedp->ged_result_str) != PIPE_ROUTE_OK) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: the requested clearance cutter is not PIPE-valid");
	return BRLCAD_ERROR;
    }

    for (obstacle_index = 0; obstacle_index < obstacle_count; obstacle_index++) {
	if (pipe_route_path_intersects_obstacle(route, &obstacles[obstacle_index], cutter_options.outer_diameter * 0.5)) {
	    have_cuts = 1;
	    (*cut_count)++;
	}
    }
    if (!have_cuts)
	return BRLCAD_OK;

    bu_vls_sprintf(&cutter_name, "%s%s", request->output_name, PIPE_ROUTE_CUTTER_SUFFIX);
    if (pipe_route_name_unused(gedp, bu_vls_cstr(&cutter_name)) != BRLCAD_OK)
	goto failed;

    cut_names = (struct bu_vls *)bu_calloc(*cut_count, sizeof(struct bu_vls), "pipe route cut names");
    {
	size_t cut_index = 0;
	for (obstacle_index = 0; obstacle_index < obstacle_count; obstacle_index++) {
	    if (!pipe_route_path_intersects_obstacle(route, &obstacles[obstacle_index], cutter_options.outer_diameter * 0.5))
		continue;
	    bu_vls_init(&cut_names[cut_index]);
	    cut_name_count++;
	    bu_vls_sprintf(&cut_names[cut_index], "%s.cut%02zu.c", request->output_name, cut_index + 1);
	    if (pipe_route_name_unused(gedp, bu_vls_cstr(&cut_names[cut_index])) != BRLCAD_OK)
		goto failed;
	    cut_index++;
	}
    }

    if (pipe_route_write_pipe(gedp, wdbp, bu_vls_cstr(&cutter_name), route, &cutter_options) != BRLCAD_OK)
	goto failed;

    {
	size_t cut_index = 0;
	for (obstacle_index = 0; obstacle_index < obstacle_count; obstacle_index++) {
	    struct wmember cut_group;

	    if (!pipe_route_path_intersects_obstacle(route, &obstacles[obstacle_index], cutter_options.outer_diameter * 0.5))
		continue;
	    BU_LIST_INIT(&cut_group.l);
	    (void)mk_addmember(obstacles[obstacle_index].name, &cut_group.l, NULL, WMOP_UNION);
	    (void)mk_addmember(bu_vls_cstr(&cutter_name), &cut_group.l, NULL, WMOP_SUBTRACT);
	    if (mk_lcomb(wdbp, bu_vls_cstr(&cut_names[cut_index]), &cut_group, 0, NULL, NULL, NULL, 0)) {
		bu_vls_printf(gedp->ged_result_str, "pipe route: failed to create clearance replacement %s", bu_vls_cstr(&cut_names[cut_index]));
		goto failed;
	    }
	    cut_index++;
	}
    }

	for (obstacle_index = 0; obstacle_index < cut_name_count; obstacle_index++)
	bu_vls_free(&cut_names[obstacle_index]);
    bu_free(cut_names, "pipe route cut names");
    bu_vls_free(&cutter_name);
    return BRLCAD_OK;

failed:
    if (cut_names) {
	for (obstacle_index = 0; obstacle_index < cut_name_count; obstacle_index++)
	    bu_vls_free(&cut_names[obstacle_index]);
	bu_free(cut_names, "pipe route cut names");
    }
    bu_vls_free(&cutter_name);
    return BRLCAD_ERROR;
}

static const char *
pipe_route_profile_name(enum pipe_route_profile profile)
{
    switch (profile) {
	case PIPE_ROUTE_PROFILE_HOSE:
	    return "hose";
	case PIPE_ROUTE_PROFILE_HARNESS:
	    return "harness";
	case PIPE_ROUTE_PROFILE_RIGID:
	default:
	    return "rigid";
    }
}

static void
pipe_route_report(struct ged *gedp,
	const struct pipe_route_request *request,
	const struct pipe_route_metrics *metrics,
	int fallback_used,
	size_t cut_count)
{
    fastf_t base2local = gedp->dbip->dbi_base2local;

    bu_vls_trunc(gedp->ged_result_str, 0);
    bu_vls_printf(gedp->ged_result_str,
	"created: %s\nprofile: %s\ncenterline_length: %.12g\nminimum_bend_radius: %.12g\nbend_allowance: %.12g\nelbows: %zu\n",
	request->output_name,
	pipe_route_profile_name(request->options.profile),
	metrics->centerline_length * base2local,
	metrics->minimum_bend_radius * base2local,
	metrics->bend_allowance * base2local,
	metrics->elbow_count);
    if (request->options.profile == PIPE_ROUTE_PROFILE_HARNESS)
	bu_vls_printf(gedp->ged_result_str, "wires: %zu\n", request->options.wire_count);
    if (request->tee_count)
	bu_vls_printf(gedp->ged_result_str, "tees: %zu\n", request->tee_count);
    if (fallback_used)
	bu_vls_printf(gedp->ged_result_str, "fallback: %s\n", request->fallback == PIPE_ROUTE_FALLBACK_SUBTRACT ? "subtract" : "direct");
    if (cut_count)
	bu_vls_printf(gedp->ged_result_str, "clearance_replacements: %zu\n", cut_count);
}

static int
pipe_route_check_subtraction_names(struct ged *gedp, const struct pipe_route_request *request)
{
    struct bu_vls name = BU_VLS_INIT_ZERO;
    size_t cut_index;
    int status = BRLCAD_OK;

    bu_vls_sprintf(&name, "%s%s", request->output_name, PIPE_ROUTE_CUTTER_SUFFIX);
    if (pipe_route_name_unused(gedp, bu_vls_cstr(&name)) != BRLCAD_OK) {
	status = BRLCAD_ERROR;
	goto cleanup;
    }
    for (cut_index = 0; cut_index < request->avoid_count; cut_index++) {
	bu_vls_sprintf(&name, "%s.cut%02zu.c", request->output_name, cut_index + 1);
	if (pipe_route_name_unused(gedp, bu_vls_cstr(&name)) != BRLCAD_OK) {
	    status = BRLCAD_ERROR;
	    goto cleanup;
	}
    }

cleanup:
    bu_vls_free(&name);
    return status;
}

int
ged_pipe_route_core(struct ged *gedp, int argc, const char *argv[])
{
    struct pipe_route_request request;
    struct pipe_route_obstacle *obstacles = NULL;
    struct pipe_route_path main_route;
    struct pipe_route_path *branch_routes = NULL;
    struct pipe_route_metrics metrics;
    struct rt_wdb *wdbp = NULL;
    int route_status;
    int return_code = BRLCAD_ERROR;
    int fallback_used = 0;
    size_t cut_count = 0;
    size_t tee_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);
    pipe_route_request_init(&request);
    pipe_route_path_init(&main_route);

    return_code = pipe_route_parse_args(gedp, argc, argv, &request);
    if (return_code != BRLCAD_OK)
	goto cleanup;
    if (pipe_route_finalize_options(gedp, &request) != BRLCAD_OK)
	goto cleanup;
    if (request.guide_name && pipe_route_load_guide(gedp, request.guide_name, &request.guide) != BRLCAD_OK)
	goto cleanup;
    if (pipe_route_insert_tee_attachments(gedp, &request) != BRLCAD_OK)
	goto cleanup;
    if (pipe_route_load_obstacles(gedp, &request, &obstacles) != BRLCAD_OK)
	goto cleanup;

    bu_vls_trunc(gedp->ged_result_str, 0);
    route_status = pipe_route_solve(&main_route, &request.guide, &request.options, obstacles, request.avoid_count, &metrics, gedp->ged_result_str);
    if (route_status == PIPE_ROUTE_NO_PATH && request.fallback != PIPE_ROUTE_FALLBACK_ERROR) {
	if (request.fallback == PIPE_ROUTE_FALLBACK_SUBTRACT &&
	    (request.options.profile == PIPE_ROUTE_PROFILE_HARNESS || request.tee_count)) {
	    bu_vls_trunc(gedp->ged_result_str, 0);
	    bu_vls_printf(gedp->ged_result_str, "pipe route: --fallback subtract is available for one rigid pipe or hose run only");
	    goto cleanup;
	}
	bu_vls_trunc(gedp->ged_result_str, 0);
	route_status = pipe_route_make_direct(&main_route, &request.guide, &request.options, &metrics, gedp->ged_result_str);
	if (route_status == PIPE_ROUTE_OK)
	    fallback_used = 1;
    }
    if (route_status != PIPE_ROUTE_OK) {
	if (route_status == PIPE_ROUTE_NO_PATH && request.fallback == PIPE_ROUTE_FALLBACK_ERROR)
	    bu_vls_printf(gedp->ged_result_str, "\nUse --fallback direct to create an unchecked guide run or --fallback subtract to create clearance replacements.");
	goto cleanup;
    }

    if (request.tee_count) {
	branch_routes = (struct pipe_route_path *)bu_calloc(request.tee_count, sizeof(struct pipe_route_path), "pipe route tee paths");
	for (tee_index = 0; tee_index < request.tee_count; tee_index++) {
	    struct pipe_route_path branch_guide;

	    pipe_route_path_init(&branch_routes[tee_index]);
	    pipe_route_path_init(&branch_guide);
	    if (pipe_route_path_append(&branch_guide, request.tees[tee_index].attachment) != PIPE_ROUTE_OK ||
		pipe_route_path_append(&branch_guide, request.tees[tee_index].endpoint) != PIPE_ROUTE_OK) {
		pipe_route_path_free(&branch_guide);
		bu_vls_printf(gedp->ged_result_str, "pipe route: out of memory while creating tee branch");
		goto cleanup;
	    }

	    bu_vls_trunc(gedp->ged_result_str, 0);
	    route_status = pipe_route_solve(&branch_routes[tee_index], &branch_guide, &request.options, obstacles, request.avoid_count, NULL, gedp->ged_result_str);
	    if (route_status == PIPE_ROUTE_NO_PATH && request.fallback == PIPE_ROUTE_FALLBACK_DIRECT) {
		bu_vls_trunc(gedp->ged_result_str, 0);
		route_status = pipe_route_make_direct(&branch_routes[tee_index], &branch_guide, &request.options, NULL, gedp->ged_result_str);
		if (route_status == PIPE_ROUTE_OK)
		    fallback_used = 1;
	    }
	    pipe_route_path_free(&branch_guide);
	    if (route_status != PIPE_ROUTE_OK ||
		pipe_route_paths_clear(&main_route, &branch_routes[tee_index], request.tees[tee_index].attachment, &request.options, gedp->ged_result_str) != PIPE_ROUTE_OK)
		goto cleanup;
	}
    }

    if (fallback_used && request.fallback == PIPE_ROUTE_FALLBACK_SUBTRACT &&
	pipe_route_check_subtraction_names(gedp, &request) != BRLCAD_OK)
	goto cleanup;

    wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!wdbp) {
	bu_vls_printf(gedp->ged_result_str, "pipe route: unable to open the database for writing");
	goto cleanup;
    }

    if (request.options.profile == PIPE_ROUTE_PROFILE_HARNESS) {
	if (pipe_route_write_harness(gedp, wdbp, &request, &main_route) != BRLCAD_OK)
	    goto cleanup;
    } else if (request.tee_count) {
	if (pipe_route_write_tee_group(gedp, wdbp, &request, &main_route, branch_routes) != BRLCAD_OK)
	    goto cleanup;
    } else {
	if (pipe_route_name_unused(gedp, request.output_name) != BRLCAD_OK ||
	    pipe_route_write_pipe(gedp, wdbp, request.output_name, &main_route, &request.options) != BRLCAD_OK)
	    goto cleanup;
	if (fallback_used && request.fallback == PIPE_ROUTE_FALLBACK_SUBTRACT &&
	    pipe_route_write_subtractions(gedp, wdbp, &request, &main_route, obstacles, request.avoid_count, &cut_count) != BRLCAD_OK)
	    goto cleanup;
    }

    pipe_route_report(gedp, &request, &metrics, fallback_used, cut_count);
    return_code = BRLCAD_OK;

cleanup:
    if (branch_routes) {
	for (tee_index = 0; tee_index < request.tee_count; tee_index++)
	    pipe_route_path_free(&branch_routes[tee_index]);
	bu_free(branch_routes, "pipe route tee paths");
    }
    if (obstacles)
	bu_free(obstacles, "pipe route obstacles");
    pipe_route_path_free(&main_route);
    pipe_route_request_free(&request);
    return return_code;
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
