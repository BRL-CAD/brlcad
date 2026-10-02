/*                        R O U T E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file libged/pipe/route.c
 * Geometry validation and conservative grid routing for PIPE runs.
 */

#include "common.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "bu/malloc.h"

#include "route.h"

#define PIPE_ROUTE_INITIAL_POINT_CAPACITY 8
#define PIPE_ROUTE_DIRECTION_COUNT 6
#define PIPE_ROUTE_GRID_MARGIN_MULTIPLIER 2.0
#define PIPE_ROUTE_CONNECTOR_MIN_BEND_MULTIPLIER 4.0
#define PIPE_ROUTE_CONNECTOR_GRID_MULTIPLIER 4.0
#define PIPE_ROUTE_MINIMUM_GRID_BEND_MULTIPLIER 2.0
#define PIPE_ROUTE_NUMERIC_EPSILON 1.0e-9

struct pipe_route_grid {
    point_t origin;
    size_t dimensions[3];
    size_t node_count;
    fastf_t step;
};

struct pipe_route_heap_entry {
    size_t node;
    fastf_t priority;
};

struct pipe_route_heap {
    struct pipe_route_heap_entry *entries;
    size_t count;
    size_t capacity;
};

static fastf_t
pipe_route_clamp_unit(fastf_t value)
{
    if (value < -1.0)
	return -1.0;
    if (value > 1.0)
	return 1.0;
    return value;
}

static int
pipe_route_reserve(struct pipe_route_path *path, size_t count)
{
    size_t capacity = path->capacity;

    if (count <= capacity)
	return PIPE_ROUTE_OK;
    if (count > SIZE_MAX / sizeof(point_t))
	return PIPE_ROUTE_INVALID;
    if (!capacity)
	capacity = PIPE_ROUTE_INITIAL_POINT_CAPACITY;
    while (capacity < count) {
	if (capacity > SIZE_MAX / 2)
	    return PIPE_ROUTE_INVALID;
	capacity *= 2;
    }
    path->points = (point_t *)bu_realloc(path->points, capacity * sizeof(point_t), "pipe route points");
    path->capacity = capacity;
    return PIPE_ROUTE_OK;
}

void
pipe_route_path_init(struct pipe_route_path *path)
{
    path->points = NULL;
    path->count = 0;
    path->capacity = 0;
}

void
pipe_route_path_free(struct pipe_route_path *path)
{
    if (path->points)
	bu_free(path->points, "pipe route points");
    pipe_route_path_init(path);
}

int
pipe_route_path_append(struct pipe_route_path *path, const point_t point)
{
    if (path->count == SIZE_MAX || pipe_route_reserve(path, path->count + 1) != PIPE_ROUTE_OK)
	return PIPE_ROUTE_INVALID;

    VMOVE(path->points[path->count], point);
    path->count++;
    return PIPE_ROUTE_OK;
}

int
pipe_route_path_copy(struct pipe_route_path *destination, const struct pipe_route_path *source)
{
    size_t point_index;

    if (destination == source)
	return PIPE_ROUTE_OK;

    pipe_route_path_free(destination);
    if (pipe_route_reserve(destination, source->count) != PIPE_ROUTE_OK)
	return PIPE_ROUTE_INVALID;

    for (point_index = 0; point_index < source->count; point_index++)
	VMOVE(destination->points[point_index], source->points[point_index]);
    destination->count = source->count;
    return PIPE_ROUTE_OK;
}

static fastf_t
pipe_route_distance(const point_t first, const point_t second)
{
    vect_t direction;

    VSUB2(direction, second, first);
    return MAGNITUDE(direction);
}

static int
pipe_route_point_is_finite(const point_t point)
{
    return isfinite(point[X]) && isfinite(point[Y]) && isfinite(point[Z]);
}

static fastf_t
pipe_route_envelope_radius(const struct pipe_route_options *options)
{
    fastf_t harness_offset = 0.0;

    if (options->profile == PIPE_ROUTE_PROFILE_HARNESS && options->wire_count > 1)
	harness_offset = ((fastf_t)(options->wire_count - 1) * options->wire_spacing) * 0.5;

    return options->bend_radius + options->clearance + options->outer_diameter * 0.5 + harness_offset;
}

static fastf_t
pipe_route_self_separation(const struct pipe_route_options *options)
{
    fastf_t harness_width = 0.0;

    if (options->profile == PIPE_ROUTE_PROFILE_HARNESS && options->wire_count > 1)
	harness_width = (fastf_t)(options->wire_count - 1) * options->wire_spacing;

    return options->outer_diameter + options->clearance + harness_width;
}

static int
pipe_route_point_inside_box(const point_t point, const struct pipe_route_obstacle *obstacle, fastf_t expansion)
{
    size_t axis;

    for (axis = X; axis <= Z; axis++) {
	if (point[axis] < obstacle->minimum[axis] - expansion ||
	    point[axis] > obstacle->maximum[axis] + expansion)
	    return 0;
    }

    return 1;
}

static int
pipe_route_segment_intersects_box(const point_t start,
	const point_t end,
	const struct pipe_route_obstacle *obstacle,
	fastf_t expansion)
{
    fastf_t entry = 0.0;
    fastf_t exit = 1.0;
    size_t axis;

    for (axis = X; axis <= Z; axis++) {
	fastf_t direction = end[axis] - start[axis];
	fastf_t minimum = obstacle->minimum[axis] - expansion;
	fastf_t maximum = obstacle->maximum[axis] + expansion;

	if (fabs(direction) <= PIPE_ROUTE_NUMERIC_EPSILON) {
	    if (start[axis] < minimum || start[axis] > maximum)
		return 0;
	    continue;
	}

	{
	    fastf_t first_parameter = (minimum - start[axis]) / direction;
	    fastf_t second_parameter = (maximum - start[axis]) / direction;
	    fastf_t axis_entry = first_parameter < second_parameter ? first_parameter : second_parameter;
	    fastf_t axis_exit = first_parameter < second_parameter ? second_parameter : first_parameter;

	    if (axis_entry > entry)
		entry = axis_entry;
	    if (axis_exit < exit)
		exit = axis_exit;
	    if (entry > exit + PIPE_ROUTE_NUMERIC_EPSILON)
		return 0;
	}
    }

    return exit >= -PIPE_ROUTE_NUMERIC_EPSILON && entry <= 1.0 + PIPE_ROUTE_NUMERIC_EPSILON;
}

static int
pipe_route_segment_is_clear(const point_t start,
	const point_t end,
	const struct pipe_route_obstacle *obstacles,
	size_t obstacle_count,
	fastf_t expansion)
{
    size_t obstacle_index;

    for (obstacle_index = 0; obstacle_index < obstacle_count; obstacle_index++) {
	if (pipe_route_segment_intersects_box(start, end, &obstacles[obstacle_index], expansion))
	    return 0;
    }

    return 1;
}

int
pipe_route_path_intersects_obstacle(const struct pipe_route_path *route,
	const struct pipe_route_obstacle *obstacle,
	fastf_t expansion)
{
    size_t segment_index;

    for (segment_index = 0; segment_index + 1 < route->count; segment_index++) {
	if (pipe_route_segment_intersects_box(route->points[segment_index], route->points[segment_index + 1], obstacle, expansion))
	    return 1;
    }

    return 0;
}

static fastf_t
pipe_route_turn_angle(const point_t previous, const point_t current, const point_t next, int *is_collinear)
{
    vect_t incoming;
    vect_t outgoing;
    vect_t normal;
    fastf_t incoming_length;
    fastf_t outgoing_length;

    VSUB2(incoming, current, previous);
    VSUB2(outgoing, next, current);
    incoming_length = MAGNITUDE(incoming);
    outgoing_length = MAGNITUDE(outgoing);
    if (incoming_length <= PIPE_ROUTE_NUMERIC_EPSILON || outgoing_length <= PIPE_ROUTE_NUMERIC_EPSILON)
	return -1.0;

    VSCALE(incoming, incoming, 1.0 / incoming_length);
    VSCALE(outgoing, outgoing, 1.0 / outgoing_length);
    VCROSS(normal, incoming, outgoing);
    *is_collinear = MAGNITUDE(normal) <= PIPE_ROUTE_NUMERIC_EPSILON;
    return acos(pipe_route_clamp_unit(VDOT(incoming, outgoing)));
}

static void
pipe_route_simplify(struct pipe_route_path *route)
{
    size_t input_index;
    size_t output_count = 0;

    for (input_index = 0; input_index < route->count; input_index++) {
	if (output_count && pipe_route_distance(route->points[output_count - 1], route->points[input_index]) <= PIPE_ROUTE_NUMERIC_EPSILON)
	    continue;

	VMOVE(route->points[output_count], route->points[input_index]);
	output_count++;
	while (output_count >= 3) {
	    vect_t first_direction;
	    vect_t second_direction;
	    vect_t normal;

	    VSUB2(first_direction, route->points[output_count - 2], route->points[output_count - 3]);
	    VSUB2(second_direction, route->points[output_count - 1], route->points[output_count - 2]);
	    VCROSS(normal, first_direction, second_direction);
	    if (MAGNITUDE(normal) > PIPE_ROUTE_NUMERIC_EPSILON || VDOT(first_direction, second_direction) <= 0.0)
		break;

	    VMOVE(route->points[output_count - 2], route->points[output_count - 1]);
	    output_count--;
	}
    }

    route->count = output_count;
}

static fastf_t
pipe_route_point_segment_distance(const point_t point, const point_t start, const point_t end)
{
    vect_t direction;
    vect_t point_offset;
    point_t closest;
    fastf_t length_squared;
    fastf_t parameter;

    VSUB2(direction, end, start);
    VSUB2(point_offset, point, start);
    length_squared = MAGSQ(direction);
    if (length_squared <= PIPE_ROUTE_NUMERIC_EPSILON)
	return pipe_route_distance(point, start);

    parameter = VDOT(point_offset, direction) / length_squared;
    if (parameter < 0.0)
	parameter = 0.0;
    if (parameter > 1.0)
	parameter = 1.0;
    VJOIN1(closest, start, parameter, direction);
    return pipe_route_distance(point, closest);
}

static fastf_t
pipe_route_segment_distance(const point_t first_start,
	const point_t first_end,
	const point_t second_start,
	const point_t second_end)
{
    vect_t first_direction;
    vect_t second_direction;
    vect_t start_offset;
    fastf_t first_length_squared;
    fastf_t second_length_squared;
    fastf_t direction_dot;
    fastf_t first_offset_dot;
    fastf_t second_offset_dot;
    fastf_t denominator;
    fastf_t first_numerator;
    fastf_t second_numerator;
    fastf_t first_denominator;
    fastf_t second_denominator;
    fastf_t first_parameter;
    fastf_t second_parameter;
    point_t first_closest;
    point_t second_closest;

    VSUB2(first_direction, first_end, first_start);
    VSUB2(second_direction, second_end, second_start);
    VSUB2(start_offset, first_start, second_start);
    first_length_squared = MAGSQ(first_direction);
    second_length_squared = MAGSQ(second_direction);
    if (first_length_squared <= PIPE_ROUTE_NUMERIC_EPSILON)
	return pipe_route_point_segment_distance(first_start, second_start, second_end);
    if (second_length_squared <= PIPE_ROUTE_NUMERIC_EPSILON)
	return pipe_route_point_segment_distance(second_start, first_start, first_end);

    direction_dot = VDOT(first_direction, second_direction);
    first_offset_dot = VDOT(first_direction, start_offset);
    second_offset_dot = VDOT(second_direction, start_offset);
    denominator = first_length_squared * second_length_squared - direction_dot * direction_dot;
    first_denominator = denominator;
    second_denominator = denominator;

    if (denominator <= PIPE_ROUTE_NUMERIC_EPSILON) {
	first_numerator = 0.0;
	first_denominator = 1.0;
	second_numerator = second_offset_dot;
	second_denominator = second_length_squared;
    } else {
	first_numerator = direction_dot * second_offset_dot - second_length_squared * first_offset_dot;
	second_numerator = first_length_squared * second_offset_dot - direction_dot * first_offset_dot;
	if (first_numerator < 0.0) {
	    first_numerator = 0.0;
	    second_numerator = second_offset_dot;
	    second_denominator = second_length_squared;
	} else if (first_numerator > first_denominator) {
	    first_numerator = first_denominator;
	    second_numerator = second_offset_dot + direction_dot;
	    second_denominator = second_length_squared;
	}
    }

    if (second_numerator < 0.0) {
	second_numerator = 0.0;
	if (-first_offset_dot < 0.0) {
	    first_numerator = 0.0;
	    first_denominator = first_length_squared;
	} else if (-first_offset_dot > first_length_squared) {
	    first_numerator = first_length_squared;
	    first_denominator = first_length_squared;
	} else {
	    first_numerator = -first_offset_dot;
	    first_denominator = first_length_squared;
	}
    } else if (second_numerator > second_denominator) {
	second_numerator = second_denominator;
	if (-first_offset_dot + direction_dot < 0.0) {
	    first_numerator = 0.0;
	    first_denominator = first_length_squared;
	} else if (-first_offset_dot + direction_dot > first_length_squared) {
	    first_numerator = first_length_squared;
	    first_denominator = first_length_squared;
	} else {
	    first_numerator = -first_offset_dot + direction_dot;
	    first_denominator = first_length_squared;
	}
    }

    first_parameter = fabs(first_numerator) <= PIPE_ROUTE_NUMERIC_EPSILON ? 0.0 : first_numerator / first_denominator;
    second_parameter = fabs(second_numerator) <= PIPE_ROUTE_NUMERIC_EPSILON ? 0.0 : second_numerator / second_denominator;
    VJOIN1(first_closest, first_start, first_parameter, first_direction);
    VJOIN1(second_closest, second_start, second_parameter, second_direction);
    return pipe_route_distance(first_closest, second_closest);
}

static int
pipe_route_validate_parameters(const struct pipe_route_options *options, struct bu_vls *message)
{
    if (options->profile < PIPE_ROUTE_PROFILE_RIGID || options->profile > PIPE_ROUTE_PROFILE_HARNESS) {
	bu_vls_printf(message, "route profile is invalid");
	return PIPE_ROUTE_INVALID;
    }
    if (!isfinite(options->outer_diameter) || !isfinite(options->inner_diameter) ||
	!isfinite(options->bend_radius) || !isfinite(options->clearance) ||
	!isfinite(options->grid_size) || !isfinite(options->wire_spacing)) {
	bu_vls_printf(message, "route dimensions must be finite");
	return PIPE_ROUTE_INVALID;
    }
    if (options->outer_diameter <= PIPE_ROUTE_NUMERIC_EPSILON) {
	bu_vls_printf(message, "outer diameter must be greater than zero");
	return PIPE_ROUTE_INVALID;
    }
    if (options->inner_diameter < 0.0 || options->inner_diameter >= options->outer_diameter) {
	bu_vls_printf(message, "inner diameter must be non-negative and smaller than outer diameter");
	return PIPE_ROUTE_INVALID;
    }
    if (options->bend_radius < options->outer_diameter * 0.5) {
	bu_vls_printf(message, "bend radius must be at least half the outer diameter");
	return PIPE_ROUTE_INVALID;
    }
    if (options->clearance < 0.0 || options->grid_size <= PIPE_ROUTE_NUMERIC_EPSILON) {
	bu_vls_printf(message, "clearance must be non-negative and grid size must be greater than zero");
	return PIPE_ROUTE_INVALID;
    }
    if (options->max_nodes == 0 || options->max_nodes > (size_t)INT_MAX) {
	bu_vls_printf(message, "max-nodes must be between 1 and %d", INT_MAX);
	return PIPE_ROUTE_INVALID;
    }
    if (options->profile == PIPE_ROUTE_PROFILE_HARNESS &&
	(options->wire_count == 0 || (options->wire_count > 1 && options->wire_spacing + PIPE_ROUTE_NUMERIC_EPSILON < options->outer_diameter))) {
	bu_vls_printf(message, "a harness needs one or more wires spaced at least one wire diameter apart");
	return PIPE_ROUTE_INVALID;
    }
    return PIPE_ROUTE_OK;
}

int
pipe_route_validate(const struct pipe_route_path *route,
	const struct pipe_route_options *options,
	const struct pipe_route_obstacle *obstacles,
	size_t obstacle_count,
	int check_obstacles,
	struct pipe_route_metrics *metrics,
	struct bu_vls *message)
{
    struct pipe_route_metrics route_metrics = {0.0, 0.0, 0.0, 0};
    fastf_t previous_tangent_length = 0.0;
    size_t point_index;

    if (pipe_route_validate_parameters(options, message) != PIPE_ROUTE_OK)
	return PIPE_ROUTE_INVALID;
    if (route->count < 2) {
	bu_vls_printf(message, "a route needs at least two distinct points");
	return PIPE_ROUTE_INVALID;
    }

    for (point_index = 0; point_index + 1 < route->count; point_index++) {
	if (!pipe_route_point_is_finite(route->points[point_index]) ||
	    !pipe_route_point_is_finite(route->points[point_index + 1])) {
	    bu_vls_printf(message, "route contains a non-finite coordinate near segment %zu", point_index);
	    return PIPE_ROUTE_INVALID;
	}
	fastf_t segment_length = pipe_route_distance(route->points[point_index], route->points[point_index + 1]);
	if (segment_length <= PIPE_ROUTE_NUMERIC_EPSILON) {
	    bu_vls_printf(message, "route contains coincident points at indices %zu and %zu", point_index, point_index + 1);
	    return PIPE_ROUTE_INVALID;
	}
	route_metrics.centerline_length += segment_length;
    }

    for (point_index = 1; point_index + 1 < route->count; point_index++) {
	int is_collinear = 0;
	fastf_t turn_angle = pipe_route_turn_angle(route->points[point_index - 1], route->points[point_index], route->points[point_index + 1], &is_collinear);
	fastf_t tangent_length = 0.0;
	fastf_t previous_segment_length;

	if (turn_angle < 0.0) {
	    bu_vls_printf(message, "route contains a degenerate bend at point %zu", point_index);
	    return PIPE_ROUTE_INVALID;
	}
	if (is_collinear) {
	    if (turn_angle > PIPE_ROUTE_NUMERIC_EPSILON) {
		bu_vls_printf(message, "route reverses direction at point %zu", point_index);
		return PIPE_ROUTE_INVALID;
	    }
	} else {
	    if (turn_angle >= M_PI - PIPE_ROUTE_NUMERIC_EPSILON) {
		bu_vls_printf(message, "route reverses direction at point %zu", point_index);
		return PIPE_ROUTE_INVALID;
	    }
	    tangent_length = options->bend_radius * tan(turn_angle * 0.5);
	    if (!isfinite(tangent_length)) {
		bu_vls_printf(message, "bend at point %zu cannot be represented", point_index);
		return PIPE_ROUTE_INVALID;
	    }
	    route_metrics.centerline_length += options->bend_radius * turn_angle - 2.0 * tangent_length;
	    route_metrics.bend_allowance += options->bend_radius * turn_angle;
	    route_metrics.elbow_count++;
	}

	previous_segment_length = pipe_route_distance(route->points[point_index - 1], route->points[point_index]);
	if (previous_tangent_length + tangent_length >= previous_segment_length - PIPE_ROUTE_NUMERIC_EPSILON) {
	    bu_vls_printf(message, "bend-radius constraints consume the segment before point %zu", point_index);
	    return PIPE_ROUTE_INVALID;
	}
	previous_tangent_length = tangent_length;
    }

    if (previous_tangent_length >= pipe_route_distance(route->points[route->count - 2], route->points[route->count - 1]) - PIPE_ROUTE_NUMERIC_EPSILON) {
	bu_vls_printf(message, "bend-radius constraints consume the final route segment");
	return PIPE_ROUTE_INVALID;
    }
    route_metrics.minimum_bend_radius = options->bend_radius;

    if (check_obstacles) {
	fastf_t expansion = pipe_route_envelope_radius(options);
	size_t obstacle_index;

	for (obstacle_index = 0; obstacle_index < obstacle_count; obstacle_index++) {
	    if (pipe_route_path_intersects_obstacle(route, &obstacles[obstacle_index], expansion)) {
		bu_vls_printf(message, "route intersects keep-out envelope for %s", obstacles[obstacle_index].name);
		return PIPE_ROUTE_NO_PATH;
	    }
	}
    }

    {
	fastf_t required_separation = pipe_route_self_separation(options);
	size_t first_segment;

	for (first_segment = 0; first_segment + 1 < route->count; first_segment++) {
	    size_t second_segment;
	    for (second_segment = first_segment + 2; second_segment + 1 < route->count; second_segment++) {
		fastf_t distance = pipe_route_segment_distance(route->points[first_segment], route->points[first_segment + 1], route->points[second_segment], route->points[second_segment + 1]);
		if (distance + PIPE_ROUTE_NUMERIC_EPSILON < required_separation) {
		    bu_vls_printf(message, "route overlaps itself near segments %zu and %zu", first_segment, second_segment);
		    return PIPE_ROUTE_INVALID;
		}
	    }
	}
    }

    if (metrics)
	*metrics = route_metrics;
    return PIPE_ROUTE_OK;
}

static int
pipe_route_point_on_segment(const point_t point, const point_t start, const point_t end)
{
    vect_t direction;
    vect_t point_offset;
    vect_t cross;
    fastf_t direction_length_squared;
    fastf_t parameter;

    VSUB2(direction, end, start);
    VSUB2(point_offset, point, start);
    direction_length_squared = MAGSQ(direction);
    if (direction_length_squared <= PIPE_ROUTE_NUMERIC_EPSILON)
	return pipe_route_distance(point, start) <= PIPE_ROUTE_NUMERIC_EPSILON;

    VCROSS(cross, direction, point_offset);
    if (MAGNITUDE(cross) > PIPE_ROUTE_NUMERIC_EPSILON * MAGNITUDE(direction))
	return 0;

    parameter = VDOT(point_offset, direction) / direction_length_squared;
    return parameter >= -PIPE_ROUTE_NUMERIC_EPSILON && parameter <= 1.0 + PIPE_ROUTE_NUMERIC_EPSILON;
}

static int
pipe_route_t_junction_pair(const point_t first_start,
	const point_t first_end,
	const point_t second_start,
	const point_t second_end,
	const point_t shared_point)
{
    vect_t first_direction;
    vect_t second_direction;
    vect_t cross;

    if (!pipe_route_point_on_segment(shared_point, first_start, first_end) ||
	!pipe_route_point_on_segment(shared_point, second_start, second_end))
	return 0;

    VSUB2(first_direction, first_end, first_start);
    VSUB2(second_direction, second_end, second_start);
    VCROSS(cross, first_direction, second_direction);
    return MAGNITUDE(cross) > PIPE_ROUTE_NUMERIC_EPSILON;
}

int
pipe_route_paths_clear(const struct pipe_route_path *first,
	const struct pipe_route_path *second,
	const point_t shared_point,
	const struct pipe_route_options *options,
	struct bu_vls *message)
{
    fastf_t required_separation = pipe_route_self_separation(options);
    size_t first_segment;

    for (first_segment = 0; first_segment + 1 < first->count; first_segment++) {
	size_t second_segment;
	for (second_segment = 0; second_segment + 1 < second->count; second_segment++) {
	    fastf_t distance = pipe_route_segment_distance(first->points[first_segment], first->points[first_segment + 1], second->points[second_segment], second->points[second_segment + 1]);
	    if (distance + PIPE_ROUTE_NUMERIC_EPSILON >= required_separation)
		continue;
	    if (pipe_route_t_junction_pair(first->points[first_segment], first->points[first_segment + 1], second->points[second_segment], second->points[second_segment + 1], shared_point))
		continue;

	    bu_vls_printf(message, "tee branch overlaps the primary route near segments %zu and %zu", first_segment, second_segment);
	    return PIPE_ROUTE_INVALID;
	}
    }

    return PIPE_ROUTE_OK;
}

static int
pipe_route_grid_initialize(struct pipe_route_grid *grid,
	const point_t start,
	const point_t end,
	const struct pipe_route_options *options,
	const struct pipe_route_obstacle *obstacles,
	size_t obstacle_count,
	struct bu_vls *message)
{
    point_t minimum;
    point_t maximum;
    fastf_t margin;
    size_t obstacle_index;
    size_t axis;

    if (options->grid_size < PIPE_ROUTE_MINIMUM_GRID_BEND_MULTIPLIER * options->bend_radius) {
	bu_vls_printf(message, "grid size must be at least twice the bend radius");
	return PIPE_ROUTE_INVALID;
    }

    VMOVE(minimum, start);
    VMOVE(maximum, start);
    VMIN(minimum, end);
    VMAX(maximum, end);
    for (obstacle_index = 0; obstacle_index < obstacle_count; obstacle_index++) {
	VMIN(minimum, obstacles[obstacle_index].minimum);
	VMAX(maximum, obstacles[obstacle_index].maximum);
    }

    margin = PIPE_ROUTE_GRID_MARGIN_MULTIPLIER * options->grid_size + pipe_route_envelope_radius(options);
    for (axis = X; axis <= Z; axis++) {
	double span;
	double cell_count;

	minimum[axis] -= margin;
	maximum[axis] += margin;
	span = maximum[axis] - minimum[axis];
	cell_count = ceil(span / options->grid_size) + 1.0;
	if (cell_count < 2.0)
	    cell_count = 2.0;
	if (cell_count > (double)SIZE_MAX) {
	    bu_vls_printf(message, "routing grid is too large");
	    return PIPE_ROUTE_NO_PATH;
	}
	grid->dimensions[axis] = (size_t)cell_count;
    }

    if (grid->dimensions[X] > options->max_nodes / grid->dimensions[Y] ||
	grid->dimensions[X] * grid->dimensions[Y] > options->max_nodes / grid->dimensions[Z]) {
	bu_vls_printf(message, "routing grid exceeds --max-nodes (%zu)", options->max_nodes);
	return PIPE_ROUTE_NO_PATH;
    }

    VMOVE(grid->origin, minimum);
    grid->node_count = grid->dimensions[X] * grid->dimensions[Y] * grid->dimensions[Z];
    grid->step = options->grid_size;
    return PIPE_ROUTE_OK;
}

static void
pipe_route_grid_point(const struct pipe_route_grid *grid, size_t node, point_t point)
{
    size_t xy_count = grid->dimensions[X] * grid->dimensions[Y];
    size_t z_index = node / xy_count;
    size_t remainder = node % xy_count;
    size_t y_index = remainder / grid->dimensions[X];
    size_t x_index = remainder % grid->dimensions[X];

    point[X] = grid->origin[X] + (fastf_t)x_index * grid->step;
    point[Y] = grid->origin[Y] + (fastf_t)y_index * grid->step;
    point[Z] = grid->origin[Z] + (fastf_t)z_index * grid->step;
}

static int
pipe_route_grid_neighbor(const struct pipe_route_grid *grid, size_t node, size_t direction, size_t *neighbor)
{
    static const int axis_direction[PIPE_ROUTE_DIRECTION_COUNT][2] = {
	{X, -1}, {X, 1}, {Y, -1}, {Y, 1}, {Z, -1}, {Z, 1}
    };
    size_t xy_count = grid->dimensions[X] * grid->dimensions[Y];
    size_t coordinates[3];
    size_t remainder;
    int axis;
    int delta;

    coordinates[Z] = node / xy_count;
    remainder = node % xy_count;
    coordinates[Y] = remainder / grid->dimensions[X];
    coordinates[X] = remainder % grid->dimensions[X];
    axis = axis_direction[direction][0];
    delta = axis_direction[direction][1];
    if ((delta < 0 && coordinates[axis] == 0) ||
	(delta > 0 && coordinates[axis] + 1 >= grid->dimensions[axis]))
	return 0;

    coordinates[axis] = (size_t)((int)coordinates[axis] + delta);
    *neighbor = coordinates[Z] * xy_count + coordinates[Y] * grid->dimensions[X] + coordinates[X];
    return 1;
}

static void
pipe_route_heap_init(struct pipe_route_heap *heap)
{
    heap->entries = NULL;
    heap->count = 0;
    heap->capacity = 0;
}

static void
pipe_route_heap_free(struct pipe_route_heap *heap)
{
    if (heap->entries)
	bu_free(heap->entries, "pipe route priority queue");
    pipe_route_heap_init(heap);
}

static int
pipe_route_heap_push(struct pipe_route_heap *heap, size_t node, fastf_t priority)
{
    size_t entry_index;

    if (heap->count == heap->capacity) {
	size_t capacity = heap->capacity ? heap->capacity * 2 : PIPE_ROUTE_INITIAL_POINT_CAPACITY;
	if (capacity < heap->capacity || capacity > SIZE_MAX / sizeof(struct pipe_route_heap_entry))
	    return PIPE_ROUTE_INVALID;
	heap->entries = (struct pipe_route_heap_entry *)bu_realloc(heap->entries, capacity * sizeof(struct pipe_route_heap_entry), "pipe route priority queue");
	heap->capacity = capacity;
    }

    entry_index = heap->count++;
    while (entry_index) {
	size_t parent_index = (entry_index - 1) / 2;
	if (heap->entries[parent_index].priority <= priority)
	    break;
	heap->entries[entry_index] = heap->entries[parent_index];
	entry_index = parent_index;
    }
    heap->entries[entry_index].node = node;
    heap->entries[entry_index].priority = priority;
    return PIPE_ROUTE_OK;
}

static int
pipe_route_heap_pop(struct pipe_route_heap *heap, struct pipe_route_heap_entry *entry)
{
    struct pipe_route_heap_entry last;
    size_t entry_index = 0;

    if (!heap->count)
	return 0;

    *entry = heap->entries[0];
    heap->count--;
    if (!heap->count)
	return 1;

    last = heap->entries[heap->count];
    while (1) {
	size_t left_index = entry_index * 2 + 1;
	size_t right_index = left_index + 1;
	size_t child_index;

	if (left_index >= heap->count)
	    break;
	child_index = left_index;
	if (right_index < heap->count && heap->entries[right_index].priority < heap->entries[left_index].priority)
	    child_index = right_index;
	if (heap->entries[child_index].priority >= last.priority)
	    break;
	heap->entries[entry_index] = heap->entries[child_index];
	entry_index = child_index;
    }
    heap->entries[entry_index] = last;
    return 1;
}

static int
pipe_route_reconstruct(struct pipe_route_path *route,
	const struct pipe_route_grid *grid,
	const int *previous,
	size_t destination,
	const point_t start,
	const point_t end)
{
    size_t chain_count = 1;
    size_t current = destination;
    size_t *nodes;
    size_t node_index;

    while (previous[current] >= 0) {
	chain_count++;
	current = (size_t)previous[current];
	if (chain_count > grid->node_count)
	    return PIPE_ROUTE_INVALID;
    }
    if (previous[current] != -2)
	return PIPE_ROUTE_INVALID;

    nodes = (size_t *)bu_malloc(chain_count * sizeof(size_t), "pipe route predecessor chain");
    current = destination;
    for (node_index = chain_count; node_index > 0; node_index--) {
	nodes[node_index - 1] = current;
	if (previous[current] < 0)
	    break;
	current = (size_t)previous[current];
    }

    pipe_route_path_free(route);
    pipe_route_path_init(route);
    if (pipe_route_path_append(route, start) != PIPE_ROUTE_OK)
	goto failed;
    for (node_index = 0; node_index < chain_count; node_index++) {
	point_t point;

	pipe_route_grid_point(grid, nodes[node_index], point);
	if (pipe_route_path_append(route, point) != PIPE_ROUTE_OK)
	    goto failed;
    }
    if (pipe_route_path_append(route, end) != PIPE_ROUTE_OK)
	goto failed;

    bu_free(nodes, "pipe route predecessor chain");
    pipe_route_simplify(route);
    return PIPE_ROUTE_OK;

failed:
    bu_free(nodes, "pipe route predecessor chain");
    pipe_route_path_free(route);
    return PIPE_ROUTE_INVALID;
}

static int
pipe_route_plan_segment(struct pipe_route_path *route,
	const point_t start,
	const point_t end,
	const struct pipe_route_options *options,
	const struct pipe_route_obstacle *obstacles,
	size_t obstacle_count,
	struct bu_vls *message)
{
    struct pipe_route_grid grid;
    unsigned char *blocked = NULL;
    fastf_t *cost = NULL;
    int *previous = NULL;
    struct pipe_route_heap heap;
    fastf_t expansion = pipe_route_envelope_radius(options);
    fastf_t connector_minimum = PIPE_ROUTE_CONNECTOR_MIN_BEND_MULTIPLIER * options->bend_radius;
    fastf_t connector_maximum = PIPE_ROUTE_CONNECTOR_GRID_MULTIPLIER * options->grid_size + connector_minimum;
    size_t node;
    int status;

    memset(&grid, 0, sizeof(grid));
    pipe_route_heap_init(&heap);
    status = pipe_route_grid_initialize(&grid, start, end, options, obstacles, obstacle_count, message);
    if (status != PIPE_ROUTE_OK)
	return status;

    blocked = (unsigned char *)bu_calloc(grid.node_count, sizeof(unsigned char), "pipe route blocked grid cells");
    cost = (fastf_t *)bu_malloc(grid.node_count * sizeof(fastf_t), "pipe route costs");
    previous = (int *)bu_malloc(grid.node_count * sizeof(int), "pipe route predecessors");
    for (node = 0; node < grid.node_count; node++) {
	point_t point;
	size_t obstacle_index;

	pipe_route_grid_point(&grid, node, point);
	for (obstacle_index = 0; obstacle_index < obstacle_count; obstacle_index++) {
	    if (pipe_route_point_inside_box(point, &obstacles[obstacle_index], expansion)) {
		blocked[node] = 1;
		break;
	    }
	}
	cost[node] = INFINITY;
	previous[node] = -1;
    }

    for (node = 0; node < grid.node_count; node++) {
	point_t point;
	fastf_t source_distance;

	if (blocked[node])
	    continue;
	pipe_route_grid_point(&grid, node, point);
	source_distance = pipe_route_distance(start, point);
	if (source_distance + PIPE_ROUTE_NUMERIC_EPSILON < connector_minimum || source_distance > connector_maximum)
	    continue;
	if (!pipe_route_segment_is_clear(start, point, obstacles, obstacle_count, expansion))
	    continue;

	cost[node] = source_distance;
	previous[node] = -2;
	if (pipe_route_heap_push(&heap, node, source_distance + pipe_route_distance(point, end)) != PIPE_ROUTE_OK) {
	    bu_vls_printf(message, "out of memory while building route search");
	    status = PIPE_ROUTE_INVALID;
	    goto cleanup;
	}
    }

    if (!heap.count) {
	bu_vls_printf(message, "no bend-compatible entry into the routing grid");
	status = PIPE_ROUTE_NO_PATH;
	goto cleanup;
    }

    while (heap.count) {
	struct pipe_route_heap_entry entry;
	point_t current_point;
	fastf_t expected_priority;
	fastf_t destination_distance;
	size_t direction;

	(void)pipe_route_heap_pop(&heap, &entry);
	pipe_route_grid_point(&grid, entry.node, current_point);
	expected_priority = cost[entry.node] + pipe_route_distance(current_point, end);
	if (entry.priority > expected_priority + PIPE_ROUTE_NUMERIC_EPSILON)
	    continue;

	destination_distance = pipe_route_distance(current_point, end);
	if (destination_distance + PIPE_ROUTE_NUMERIC_EPSILON >= connector_minimum &&
	    destination_distance <= connector_maximum &&
	    pipe_route_segment_is_clear(current_point, end, obstacles, obstacle_count, expansion)) {
	    status = pipe_route_reconstruct(route, &grid, previous, entry.node, start, end);
	    goto cleanup;
	}

	for (direction = 0; direction < PIPE_ROUTE_DIRECTION_COUNT; direction++) {
	    size_t neighbor;
	    point_t neighbor_point;
	    fastf_t tentative_cost;

	    if (!pipe_route_grid_neighbor(&grid, entry.node, direction, &neighbor) || blocked[neighbor])
		continue;
	    pipe_route_grid_point(&grid, neighbor, neighbor_point);
	    if (!pipe_route_segment_is_clear(current_point, neighbor_point, obstacles, obstacle_count, expansion))
		continue;

	    tentative_cost = cost[entry.node] + grid.step;
	    if (tentative_cost + PIPE_ROUTE_NUMERIC_EPSILON >= cost[neighbor])
		continue;

	    cost[neighbor] = tentative_cost;
	    previous[neighbor] = (int)entry.node;
	    if (pipe_route_heap_push(&heap, neighbor, tentative_cost + pipe_route_distance(neighbor_point, end)) != PIPE_ROUTE_OK) {
		bu_vls_printf(message, "out of memory while building route search");
		status = PIPE_ROUTE_INVALID;
		goto cleanup;
	    }
	}
    }

    bu_vls_printf(message, "no collision-free path exists within the routing grid");
    status = PIPE_ROUTE_NO_PATH;

cleanup:
    if (blocked)
	bu_free(blocked, "pipe route blocked grid cells");
    if (cost)
	bu_free(cost, "pipe route costs");
    if (previous)
	bu_free(previous, "pipe route predecessors");
    pipe_route_heap_free(&heap);
    return status;
}

int
pipe_route_solve(struct pipe_route_path *route,
	const struct pipe_route_path *guide,
	const struct pipe_route_options *options,
	const struct pipe_route_obstacle *obstacles,
	size_t obstacle_count,
	struct pipe_route_metrics *metrics,
	struct bu_vls *message)
{
    size_t point_index;
    int status;

    if (pipe_route_validate_parameters(options, message) != PIPE_ROUTE_OK)
	return PIPE_ROUTE_INVALID;
    if (guide->count < 2) {
	bu_vls_printf(message, "a route needs at least two guide points");
	return PIPE_ROUTE_INVALID;
    }

    pipe_route_path_free(route);
    pipe_route_path_init(route);
    if (pipe_route_path_append(route, guide->points[0]) != PIPE_ROUTE_OK) {
	bu_vls_printf(message, "out of memory while creating route");
	return PIPE_ROUTE_INVALID;
    }

    for (point_index = 0; point_index + 1 < guide->count; point_index++) {
	point_t start;
	point_t end;
	struct pipe_route_path detour;

	VMOVE(start, guide->points[point_index]);
	VMOVE(end, guide->points[point_index + 1]);
	if (pipe_route_distance(start, end) <= PIPE_ROUTE_NUMERIC_EPSILON) {
	    bu_vls_printf(message, "guide points %zu and %zu are coincident", point_index, point_index + 1);
	    pipe_route_path_free(route);
	    return PIPE_ROUTE_INVALID;
	}

	if (pipe_route_segment_is_clear(start, end, obstacles, obstacle_count, pipe_route_envelope_radius(options))) {
	    if (pipe_route_path_append(route, end) != PIPE_ROUTE_OK) {
		bu_vls_printf(message, "out of memory while creating route");
		pipe_route_path_free(route);
		return PIPE_ROUTE_INVALID;
	    }
	    continue;
	}

	pipe_route_path_init(&detour);
	status = pipe_route_plan_segment(&detour, start, end, options, obstacles, obstacle_count, message);
	if (status != PIPE_ROUTE_OK) {
	    pipe_route_path_free(&detour);
	    pipe_route_path_free(route);
	    return status;
	}

	{
	    size_t detour_index;
	    for (detour_index = 1; detour_index < detour.count; detour_index++) {
		if (pipe_route_path_append(route, detour.points[detour_index]) != PIPE_ROUTE_OK) {
		    bu_vls_printf(message, "out of memory while creating route");
		    pipe_route_path_free(&detour);
		    pipe_route_path_free(route);
		    return PIPE_ROUTE_INVALID;
		}
	    }
	}
	pipe_route_path_free(&detour);
    }

    pipe_route_simplify(route);
    status = pipe_route_validate(route, options, obstacles, obstacle_count, 1, metrics, message);
    if (status != PIPE_ROUTE_OK)
	pipe_route_path_free(route);
    return status;
}

int
pipe_route_make_direct(struct pipe_route_path *route,
	const struct pipe_route_path *guide,
	const struct pipe_route_options *options,
	struct pipe_route_metrics *metrics,
	struct bu_vls *message)
{
    int status;

    if (pipe_route_path_copy(route, guide) != PIPE_ROUTE_OK) {
	bu_vls_printf(message, "out of memory while creating route");
	return PIPE_ROUTE_INVALID;
    }

    pipe_route_simplify(route);
    status = pipe_route_validate(route, options, NULL, 0, 0, metrics, message);
    if (status != PIPE_ROUTE_OK)
	pipe_route_path_free(route);
    return status;
}

static void
pipe_route_initial_normal(vect_t normal, const vect_t tangent)
{
    vect_t reference = VINIT_ZERO;

    VSET(reference, 0.0, 0.0, 1.0);
    if (fabs(VDOT(reference, tangent)) > 0.9)
	VSET(reference, 0.0, 1.0, 0.0);

    VJOIN1(normal, reference, -VDOT(reference, tangent), tangent);
    VUNITIZE(normal);
}

int
pipe_route_make_flat_wire_path(struct pipe_route_path *wire,
	const struct pipe_route_path *centerline,
	size_t wire_index,
	size_t wire_count,
	fastf_t spacing,
	struct bu_vls *message)
{
    vect_t previous_normal = VINIT_ZERO;
    fastf_t offset;
    size_t point_index;

    if (centerline->count < 2 || wire_count == 0 || wire_index >= wire_count) {
	bu_vls_printf(message, "invalid harness wire request");
	return PIPE_ROUTE_INVALID;
    }

    offset = ((fastf_t)wire_index - ((fastf_t)wire_count - 1.0) * 0.5) * spacing;
    pipe_route_path_free(wire);
    pipe_route_path_init(wire);

    for (point_index = 0; point_index < centerline->count; point_index++) {
	vect_t tangent;
	vect_t normal;
	point_t point;

	if (point_index == 0) {
	    VSUB2(tangent, centerline->points[1], centerline->points[0]);
	} else if (point_index + 1 == centerline->count) {
	    VSUB2(tangent, centerline->points[point_index], centerline->points[point_index - 1]);
	} else {
	    VSUB2(tangent, centerline->points[point_index + 1], centerline->points[point_index - 1]);
	}
	if (MAGNITUDE(tangent) <= PIPE_ROUTE_NUMERIC_EPSILON) {
	    bu_vls_printf(message, "cannot flatten a harness through a degenerate route point");
	    pipe_route_path_free(wire);
	    return PIPE_ROUTE_INVALID;
	}
	VUNITIZE(tangent);

	if (point_index == 0) {
	    pipe_route_initial_normal(normal, tangent);
	} else {
	    VJOIN1(normal, previous_normal, -VDOT(previous_normal, tangent), tangent);
	    if (MAGNITUDE(normal) <= PIPE_ROUTE_NUMERIC_EPSILON)
		pipe_route_initial_normal(normal, tangent);
	    else
		VUNITIZE(normal);
	}

	VJOIN1(point, centerline->points[point_index], offset, normal);
	if (pipe_route_path_append(wire, point) != PIPE_ROUTE_OK) {
	    bu_vls_printf(message, "out of memory while flattening harness");
	    pipe_route_path_free(wire);
	    return PIPE_ROUTE_INVALID;
	}
	VMOVE(previous_normal, normal);
    }

    return PIPE_ROUTE_OK;
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
