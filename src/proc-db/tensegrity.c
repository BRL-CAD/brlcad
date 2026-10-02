/*                    T E N S E G R I T Y . C
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
/** @file proc-db/tensegrity.c
 *
 * Generate a Snelson-inspired tensegrity tower.  Each stage has a pair
 * of triangular cable rings with three compression struts and three
 * opposite-handed tension cables between them.  Adjacent stages share
 * a ring and alternate their twist, creating a collision-free, stable
 * looking stack from a compact set of parameters.
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "raytrace.h"
#include "wdb.h"


#define TRIANGLE_VERTEX_COUNT 3

#define DEFAULT_STAGE_COUNT 4
#define MIN_STAGE_COUNT 1
#define MAX_STAGE_COUNT 12

#define DEFAULT_RADIUS 260.0
#define DEFAULT_STAGE_HEIGHT 270.0
#define DEFAULT_TWIST_DEGREES 58.0
#define MIN_TWIST_DEGREES 20.0
#define MAX_TWIST_DEGREES 100.0

#define STRUT_RADIUS_RATIO 0.045
#define CABLE_RADIUS_RATIO 0.010
#define NODE_RADIUS_RATIO 0.060

#define GROUND_HALF_SPAN_RATIO 2.00
#define GROUND_DEPTH_RATIO 0.080
#define LIGHT_X_RATIO 2.50
#define LIGHT_Y_RATIO -2.25
#define LIGHT_Z_RATIO 1.50
#define LIGHT_RADIUS_RATIO 0.025

#define FULL_TURN_RADIANS (2.0 * M_PI)
#define DEGREES_TO_RADIANS (M_PI / 180.0)
#define MIN_MEMBER_LENGTH 1.0e-6
#define SELF_TEST_TOLERANCE 1.0e-9
#define PARAMETER_ERROR_SIZE 160
#define MEMBER_NAME_SIZE 64

#define COMPRESSION_REGION_NAME "compression.r"
#define TENSION_REGION_NAME "tension.r"
#define NODE_REGION_NAME "nodes.r"
#define TOWER_COMBINATION_NAME "tower.c"
#define GROUND_REGION_NAME "ground.r"
#define LIGHT_REGION_NAME "light.r"
#define SCENE_NAME "all"

#define COMPRESSION_SHADER "di=0.35"
#define TENSION_SHADER "di=0.30"
#define NODE_SHADER "di=0.35"
#define GROUND_SHADER "di=0.75"
#define LIGHT_SHADER "inten=1.0"

#define COMPRESSION_RED 222
#define COMPRESSION_GREEN 73
#define COMPRESSION_BLUE 53
#define TENSION_RED 43
#define TENSION_GREEN 91
#define TENSION_BLUE 183
#define NODE_RED 234
#define NODE_GREEN 179
#define NODE_BLUE 57
#define GROUND_RED 45
#define GROUND_GREEN 50
#define GROUND_BLUE 62
#define LIGHT_RED 255
#define LIGHT_GREEN 255
#define LIGHT_BLUE 255

struct tensegrity_parameters {
    int stage_count;
    double radius;
    double stage_height;
    double twist_degrees;
};


static void
print_usage(const char *program_name)
{
    bu_log("Usage: %s output.g [--stages count] [--radius mm] [--height mm] [--twist degrees]\n", program_name);
    bu_log("       %s --self-test\n", program_name);
}


static struct tensegrity_parameters
default_parameters(void)
{
    struct tensegrity_parameters parameters;

    parameters.stage_count = DEFAULT_STAGE_COUNT;
    parameters.radius = DEFAULT_RADIUS;
    parameters.stage_height = DEFAULT_STAGE_HEIGHT;
    parameters.twist_degrees = DEFAULT_TWIST_DEGREES;

    return parameters;
}


static int
parse_integer(const char *text, int *value)
{
    char *end = NULL;
    long parsed_value;

    if (!text || !text[0])
	return 0;

    errno = 0;
    parsed_value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
	parsed_value < INT_MIN || parsed_value > INT_MAX)
	return 0;

    *value = (int)parsed_value;
    return 1;
}


static int
parse_number(const char *text, double *value)
{
    char *end = NULL;
    double parsed_value;

    if (!text || !text[0])
	return 0;

    errno = 0;
    parsed_value = strtod(text, &end);
    if (errno == ERANGE || end == text || *end != '\0' || !isfinite(parsed_value))
	return 0;

    *value = parsed_value;
    return 1;
}


static int
validate_parameters(const struct tensegrity_parameters *parameters,
		    char *message,
		    size_t message_size)
{
    if (parameters->stage_count < MIN_STAGE_COUNT ||
	parameters->stage_count > MAX_STAGE_COUNT) {
	snprintf(message, message_size,
		 "stage count must be between %d and %d",
		 MIN_STAGE_COUNT, MAX_STAGE_COUNT);
	return 0;
    }

    if (parameters->radius <= 0.0) {
	snprintf(message, message_size, "radius must be positive");
	return 0;
    }

    if (parameters->stage_height <= 0.0) {
	snprintf(message, message_size, "height must be positive");
	return 0;
    }

    if (parameters->twist_degrees < MIN_TWIST_DEGREES ||
	parameters->twist_degrees > MAX_TWIST_DEGREES) {
	snprintf(message, message_size,
		 "twist must be between %.0f and %.0f degrees",
		 MIN_TWIST_DEGREES, MAX_TWIST_DEGREES);
	return 0;
    }

    return 1;
}


/* Alternating the ring orientation gives consecutive stages opposite chirality. */
static double
ring_twist_radians(const struct tensegrity_parameters *parameters, int ring_index)
{
    if (ring_index % 2 == 0)
	return 0.0;

    return parameters->twist_degrees * DEGREES_TO_RADIANS;
}


static void
ring_vertex(point_t point,
	    const struct tensegrity_parameters *parameters,
	    int ring_index,
	    int vertex_index)
{
    double angle_step = FULL_TURN_RADIANS / (double)TRIANGLE_VERTEX_COUNT;
    double angle = ring_twist_radians(parameters, ring_index) +
	angle_step * (double)vertex_index;

    VSET(point,
	 parameters->radius * cos(angle),
	 parameters->radius * sin(angle),
	 parameters->stage_height * (double)ring_index);
}


static int
add_member(struct wmember *members, const char *name)
{
    if (mk_addmember(name, &members->l, NULL, WMOP_UNION) == WMEMBER_NULL) {
	bu_log("tensegrity: failed to add member '%s'\n", name);
	return 0;
    }

    return 1;
}


static int
add_cylinder_member(struct rt_wdb *database,
		    struct wmember *members,
		    const char *name,
		    const point_t start,
		    const point_t end,
		    double radius)
{
    vect_t height;

    VSUB2(height, end, start);
    if (MAGNITUDE(height) <= MIN_MEMBER_LENGTH) {
	bu_log("tensegrity: refusing zero-length member '%s'\n", name);
	return 0;
    }

    if (mk_rcc(database, name, start, height, radius) != 0) {
	bu_log("tensegrity: failed to create cylinder '%s'\n", name);
	return 0;
    }

    return add_member(members, name);
}


static int
add_node_member(struct rt_wdb *database,
		struct wmember *members,
		const char *name,
		const point_t center,
		double radius)
{
    if (mk_sph(database, name, center, radius) != 0) {
	bu_log("tensegrity: failed to create node '%s'\n", name);
	return 0;
    }

    return add_member(members, name);
}


static int
add_stage_members(struct rt_wdb *database,
		  struct wmember *compression_members,
		  struct wmember *tension_members,
		  const struct tensegrity_parameters *parameters,
		  int stage_index,
		  double strut_radius,
		  double cable_radius)
{
    int vertex_index;

    for (vertex_index = 0; vertex_index < TRIANGLE_VERTEX_COUNT; vertex_index++) {
	point_t lower_vertex;
	point_t upper_vertex;
	point_t diagonal_upper_vertex;
	char strut_name[MEMBER_NAME_SIZE];
	char cable_name[MEMBER_NAME_SIZE];
	int previous_vertex = (vertex_index + TRIANGLE_VERTEX_COUNT - 1) %
	    TRIANGLE_VERTEX_COUNT;

	ring_vertex(lower_vertex, parameters, stage_index, vertex_index);
	ring_vertex(upper_vertex, parameters, stage_index + 1, vertex_index);
	ring_vertex(diagonal_upper_vertex, parameters, stage_index + 1, previous_vertex);

	snprintf(strut_name, sizeof(strut_name), "strut.%02d.%d.s",
		 stage_index, vertex_index);
	if (!add_cylinder_member(database, compression_members, strut_name,
			 lower_vertex, upper_vertex, strut_radius))
	    return 0;

	/* The diagonal takes the opposite winding from the strut.  It stays
	 * outside the core instead of crossing the other tension cables.
	 */
	snprintf(cable_name, sizeof(cable_name), "diagonal.%02d.%d.s",
		 stage_index, vertex_index);
	if (!add_cylinder_member(database, tension_members, cable_name,
			 lower_vertex, diagonal_upper_vertex, cable_radius))
	    return 0;
    }

    return 1;
}


static int
add_ring_members(struct rt_wdb *database,
		 struct wmember *tension_members,
		 const struct tensegrity_parameters *parameters,
		 double cable_radius)
{
    int ring_index;

    for (ring_index = 0; ring_index <= parameters->stage_count; ring_index++) {
	int vertex_index;

	for (vertex_index = 0; vertex_index < TRIANGLE_VERTEX_COUNT; vertex_index++) {
	    point_t start;
	    point_t end;
	    char cable_name[MEMBER_NAME_SIZE];
	    int next_vertex = (vertex_index + 1) % TRIANGLE_VERTEX_COUNT;

	    ring_vertex(start, parameters, ring_index, vertex_index);
	    ring_vertex(end, parameters, ring_index, next_vertex);
	    snprintf(cable_name, sizeof(cable_name), "ring.%02d.%d.s",
		     ring_index, vertex_index);
	    if (!add_cylinder_member(database, tension_members, cable_name,
			     start, end, cable_radius))
		return 0;
	}
    }

    return 1;
}


static int
add_node_members(struct rt_wdb *database,
		 struct wmember *node_members,
		 const struct tensegrity_parameters *parameters,
		 double node_radius)
{
    int ring_index;

    for (ring_index = 0; ring_index <= parameters->stage_count; ring_index++) {
	int vertex_index;

	for (vertex_index = 0; vertex_index < TRIANGLE_VERTEX_COUNT; vertex_index++) {
	    point_t center;
	    char node_name[MEMBER_NAME_SIZE];

	    ring_vertex(center, parameters, ring_index, vertex_index);
	    snprintf(node_name, sizeof(node_name), "node.%02d.%d.s",
		     ring_index, vertex_index);
	    if (!add_node_member(database, node_members, node_name, center, node_radius))
		return 0;
	}
    }

    return 1;
}


static int
write_scene(struct rt_wdb *database, const struct tensegrity_parameters *parameters)
{
    struct wmember compression_members;
    struct wmember tension_members;
    struct wmember node_members;
    struct wmember tower_members;
    struct wmember ground_members;
    struct wmember light_members;
    struct wmember scene_members;
    unsigned char compression_rgb[3] = {
	COMPRESSION_RED, COMPRESSION_GREEN, COMPRESSION_BLUE
    };
    unsigned char tension_rgb[3] = {
	TENSION_RED, TENSION_GREEN, TENSION_BLUE
    };
    unsigned char node_rgb[3] = {
	NODE_RED, NODE_GREEN, NODE_BLUE
    };
    unsigned char ground_rgb[3] = {
	GROUND_RED, GROUND_GREEN, GROUND_BLUE
    };
    unsigned char light_rgb[3] = {
	LIGHT_RED, LIGHT_GREEN, LIGHT_BLUE
    };
    double strut_radius = parameters->radius * STRUT_RADIUS_RATIO;
    double cable_radius = parameters->radius * CABLE_RADIUS_RATIO;
    double node_radius = parameters->radius * NODE_RADIUS_RATIO;
    double ground_half_span = parameters->radius * GROUND_HALF_SPAN_RATIO;
    double ground_depth = parameters->radius * GROUND_DEPTH_RATIO;
    double tower_height = parameters->stage_height * (double)parameters->stage_count;
    point_t ground_min;
    point_t ground_max;
    point_t light_center;
    int stage_index;

    BU_LIST_INIT(&compression_members.l);
    BU_LIST_INIT(&tension_members.l);
    BU_LIST_INIT(&node_members.l);

    for (stage_index = 0; stage_index < parameters->stage_count; stage_index++) {
	if (!add_stage_members(database, &compression_members, &tension_members,
		       parameters, stage_index, strut_radius, cable_radius))
	    return 0;
    }

    if (!add_ring_members(database, &tension_members, parameters, cable_radius) ||
	!add_node_members(database, &node_members, parameters, node_radius))
	return 0;

    if (mk_lcomb(database, COMPRESSION_REGION_NAME, &compression_members, 1,
		 "plastic", COMPRESSION_SHADER, compression_rgb, 0) != 0 ||
	mk_lcomb(database, TENSION_REGION_NAME, &tension_members, 1,
		 "plastic", TENSION_SHADER, tension_rgb, 0) != 0 ||
	mk_lcomb(database, NODE_REGION_NAME, &node_members, 1,
		 "plastic", NODE_SHADER, node_rgb, 0) != 0) {
	bu_log("tensegrity: failed to create structure regions\n");
	return 0;
    }

    BU_LIST_INIT(&tower_members.l);
    if (!add_member(&tower_members, COMPRESSION_REGION_NAME) ||
	!add_member(&tower_members, TENSION_REGION_NAME) ||
	!add_member(&tower_members, NODE_REGION_NAME) ||
	mk_lcomb(database, TOWER_COMBINATION_NAME, &tower_members, 0,
		 NULL, NULL, NULL, 0) != 0) {
	bu_log("tensegrity: failed to create tower combination\n");
	return 0;
    }

    VSET(ground_min, -ground_half_span, -ground_half_span,
	 -node_radius - ground_depth);
    VSET(ground_max, ground_half_span, ground_half_span, -node_radius);
    if (mk_rpp(database, "ground.s", ground_min, ground_max) != 0) {
	bu_log("tensegrity: failed to create ground slab\n");
	return 0;
    }

    BU_LIST_INIT(&ground_members.l);
    if (!add_member(&ground_members, "ground.s") ||
	mk_lcomb(database, GROUND_REGION_NAME, &ground_members, 1,
		 "plastic", GROUND_SHADER, ground_rgb, 0) != 0) {
	bu_log("tensegrity: failed to create ground region\n");
	return 0;
    }

    VSET(light_center,
	 parameters->radius * LIGHT_X_RATIO,
	 parameters->radius * LIGHT_Y_RATIO,
	 tower_height + parameters->radius * LIGHT_Z_RATIO);
    if (mk_sph(database, "light.s", light_center,
	       parameters->radius * LIGHT_RADIUS_RATIO) != 0) {
	bu_log("tensegrity: failed to create light source\n");
	return 0;
    }

    BU_LIST_INIT(&light_members.l);
    if (!add_member(&light_members, "light.s") ||
	mk_lcomb(database, LIGHT_REGION_NAME, &light_members, 1,
		 "light", LIGHT_SHADER, light_rgb, 0) != 0) {
	bu_log("tensegrity: failed to create light region\n");
	return 0;
    }

    BU_LIST_INIT(&scene_members.l);
    if (!add_member(&scene_members, TOWER_COMBINATION_NAME) ||
	!add_member(&scene_members, GROUND_REGION_NAME) ||
	!add_member(&scene_members, LIGHT_REGION_NAME) ||
	mk_lcomb(database, SCENE_NAME, &scene_members, 0,
		 NULL, NULL, NULL, 0) != 0) {
	bu_log("tensegrity: failed to create scene combination\n");
	return 0;
    }

    bu_log("tensegrity: wrote %d stages, %d struts, %d cables, and %d nodes\n",
	   parameters->stage_count,
	   parameters->stage_count * TRIANGLE_VERTEX_COUNT,
	   (parameters->stage_count * TRIANGLE_VERTEX_COUNT * 2) +
	       ((parameters->stage_count + 1) * TRIANGLE_VERTEX_COUNT),
	   (parameters->stage_count + 1) * TRIANGLE_VERTEX_COUNT);

    return 1;
}


static int
nearly_equal(double left, double right)
{
    return fabs(left - right) <= SELF_TEST_TOLERANCE;
}


static int
run_self_test(void)
{
    struct tensegrity_parameters parameters = default_parameters();
    struct tensegrity_parameters invalid_parameters;
    char validation_message[PARAMETER_ERROR_SIZE] = {0};
    point_t base_vertex;
    point_t rotated_vertex;
    double base_radius;
    double rotated_radius;
    double rotated_angle;

    if (!validate_parameters(&parameters, validation_message,
		     sizeof(validation_message))) {
	bu_log("tensegrity self-test: default parameters rejected: %s\n",
	       validation_message);
	return 1;
    }

    ring_vertex(base_vertex, &parameters, 0, 0);
    ring_vertex(rotated_vertex, &parameters, 1, 0);
    base_radius = sqrt(base_vertex[X] * base_vertex[X] +
		       base_vertex[Y] * base_vertex[Y]);
    rotated_radius = sqrt(rotated_vertex[X] * rotated_vertex[X] +
			  rotated_vertex[Y] * rotated_vertex[Y]);
    rotated_angle = atan2(rotated_vertex[Y], rotated_vertex[X]) /
	DEGREES_TO_RADIANS;

    if (!nearly_equal(base_radius, parameters.radius) ||
	!nearly_equal(rotated_radius, parameters.radius) ||
	!nearly_equal(base_vertex[Z], 0.0) ||
	!nearly_equal(rotated_vertex[Z], parameters.stage_height) ||
	!nearly_equal(rotated_angle, parameters.twist_degrees)) {
	bu_log("tensegrity self-test: ring placement regression\n");
	return 1;
    }

    invalid_parameters = parameters;
    invalid_parameters.stage_count = MIN_STAGE_COUNT - 1;
    if (validate_parameters(&invalid_parameters, validation_message,
		    sizeof(validation_message))) {
	bu_log("tensegrity self-test: invalid stage count accepted\n");
	return 1;
    }

    invalid_parameters = parameters;
    invalid_parameters.twist_degrees = MIN_TWIST_DEGREES - 1.0;
    if (validate_parameters(&invalid_parameters, validation_message,
		    sizeof(validation_message))) {
	bu_log("tensegrity self-test: invalid twist accepted\n");
	return 1;
    }

    bu_log("tensegrity: self-test passed\n");
    return 0;
}


int
main(int argc, char *argv[])
{
    struct tensegrity_parameters parameters = default_parameters();
    struct rt_wdb *database;
    char validation_message[PARAMETER_ERROR_SIZE] = {0};
    const char *output_file;
    int argument_index;

    bu_setprogname(argv[0]);

    if (argc == 2 && BU_STR_EQUAL(argv[1], "--self-test"))
	return run_self_test();

    if (argc < 2 || BU_STR_EQUAL(argv[1], "--help")) {
	print_usage(argv[0]);
	return (argc < 2) ? 1 : 0;
    }

    output_file = argv[1];
    for (argument_index = 2; argument_index < argc; argument_index++) {
	const char *option = argv[argument_index];

	if (BU_STR_EQUAL(option, "--stages")) {
	    if (++argument_index >= argc ||
		!parse_integer(argv[argument_index], &parameters.stage_count))
		bu_exit(1, "tensegrity: --stages requires an integer\n");
	} else if (BU_STR_EQUAL(option, "--radius")) {
	    if (++argument_index >= argc ||
		!parse_number(argv[argument_index], &parameters.radius))
		bu_exit(1, "tensegrity: --radius requires a number\n");
	} else if (BU_STR_EQUAL(option, "--height")) {
	    if (++argument_index >= argc ||
		!parse_number(argv[argument_index], &parameters.stage_height))
		bu_exit(1, "tensegrity: --height requires a number\n");
	} else if (BU_STR_EQUAL(option, "--twist")) {
	    if (++argument_index >= argc ||
		!parse_number(argv[argument_index], &parameters.twist_degrees))
		bu_exit(1, "tensegrity: --twist requires a number\n");
	} else {
	    bu_exit(1, "tensegrity: unknown option '%s'\n", option);
	}
    }

    if (!validate_parameters(&parameters, validation_message,
		     sizeof(validation_message)))
	bu_exit(1, "tensegrity: %s\n", validation_message);

    database = wdb_fopen(output_file);
    if (!database) {
	perror(output_file);
	return 2;
    }

    mk_id_units(database, "Procedural Tensegrity Tower", "mm");
    if (!write_scene(database, &parameters)) {
	db_close(database->dbip);
	return 3;
    }

    db_close(database->dbip);
    bu_log("tensegrity: wrote %s; render '%s' (or frame '%s' and '%s').\n",
	   output_file, SCENE_NAME, TOWER_COMBINATION_NAME, GROUND_REGION_NAME);

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
