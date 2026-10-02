/*                  A S S E M B L Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libanalyze/tests/assembly.cpp */

#include "common.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "analyze/assembly.h"
#include "bu/app.h"


namespace {


constexpr fastf_t PI = 3.141592653589793238462643383279502884;
constexpr fastf_t TOLERANCE = 1.0e-8;

static bool
near(fastf_t left, fastf_t right)
{
    return std::fabs(left - right) <= TOLERANCE;
}

static bool
same_matrix(const mat_t left, const mat_t right)
{
    for (size_t index = 0; index < 16; ++index) {
	if (!near(left[index], right[index]))
	    return false;
    }
    return true;
}

static struct analyze_assembly *
make_pair(int type, size_t *joint_index)
{
    mat_t identity = MAT_INIT_IDN;
    struct analyze_assembly *assembly = analyze_assembly_create();
    size_t base = 0;
    size_t link = 0;
    if (!assembly ||
	analyze_assembly_add_body(assembly, "base", identity, 1, &base) ||
	analyze_assembly_add_body(assembly, "link", identity, 0, &link) ||
	analyze_assembly_add_joint(assembly, "joint", type, base, link, identity, identity, joint_index)) {
	analyze_assembly_destroy(assembly);
	return NULL;
    }
    return assembly;
}

static bool
motion_case(int type, const std::vector<fastf_t> &values, fastf_t pitch,
	    fastf_t tangent_distance, const mat_t expected)
{
    size_t joint = 0;
    mat_t actual;
    struct analyze_assembly_solve_result result;
    struct analyze_assembly *assembly = make_pair(type, &joint);
    if (!assembly)
	return false;
    bool success = true;
    if (type == ANALYZE_ASSEMBLY_SCREW)
	success = analyze_assembly_set_screw_pitch(assembly, joint, pitch) == ANALYZE_ASSEMBLY_OK;
    if (success && type == ANALYZE_ASSEMBLY_TANGENT)
	success = analyze_assembly_set_tangent_distance(assembly, joint, tangent_distance) == ANALYZE_ASSEMBLY_OK;
    for (size_t index = 0; success && index < values.size(); ++index)
	success = analyze_assembly_set_joint_value(assembly, joint, index, values[index]) == ANALYZE_ASSEMBLY_OK;
    success = success && analyze_assembly_solve_forward(assembly, 0, &result) == ANALYZE_ASSEMBLY_OK &&
	analyze_assembly_get_body_transform(assembly, 1, actual) == ANALYZE_ASSEMBLY_OK &&
	same_matrix(actual, expected);
    analyze_assembly_destroy(assembly);
    return success;
}

static bool
test_joint_families()
{
    const mat_t identity = MAT_INIT_IDN;
    const mat_t revolute = {
	0.0, -1.0, 0.0, 0.0,
	1.0, 0.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0
    };
    const mat_t prismatic = {
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 12.5,
	0.0, 0.0, 0.0, 1.0
    };
    const mat_t coaxial = {
	0.0, -1.0, 0.0, 0.0,
	1.0, 0.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 4.0,
	0.0, 0.0, 0.0, 1.0
    };
    const mat_t spherical = {
	0.0, 0.0, 1.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	-1.0, 0.0, 0.0, 0.0,
	0.0, 0.0, 0.0, 1.0
    };
    const mat_t screw = {
	-1.0, 0.0, 0.0, 0.0,
	0.0, -1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 2.0,
	0.0, 0.0, 0.0, 1.0
    };
    const mat_t planar = {
	0.0, -1.0, 0.0, 3.0,
	1.0, 0.0, 0.0, 4.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0
    };
    const mat_t tangent = {
	0.0, 1.0, 0.0, 3.0,
	1.0, 0.0, 0.0, 4.0,
	0.0, 0.0, -1.0, 2.0,
	0.0, 0.0, 0.0, 1.0
    };
    return motion_case(ANALYZE_ASSEMBLY_FIXED, {}, 0.0, 0.0, identity) &&
	motion_case(ANALYZE_ASSEMBLY_REVOLUTE, {PI * 0.5}, 0.0, 0.0, revolute) &&
	motion_case(ANALYZE_ASSEMBLY_PRISMATIC, {12.5}, 0.0, 0.0, prismatic) &&
	motion_case(ANALYZE_ASSEMBLY_COAXIAL, {4.0, PI * 0.5}, 0.0, 0.0, coaxial) &&
	motion_case(ANALYZE_ASSEMBLY_SPHERICAL, {0.0, PI * 0.5, 0.0}, 0.0, 0.0, spherical) &&
	motion_case(ANALYZE_ASSEMBLY_SCREW, {PI}, 4.0, 0.0, screw) &&
	motion_case(ANALYZE_ASSEMBLY_PLANAR, {3.0, 4.0, PI * 0.5}, 0.0, 0.0, planar) &&
	motion_case(ANALYZE_ASSEMBLY_TANGENT, {3.0, 4.0, PI * 0.5}, 0.0, 2.0, tangent);
}

static bool
test_inverse_and_limits()
{
    size_t joint = 0;
    mat_t target = MAT_INIT_IDN;
    struct analyze_assembly_ik_options options;
    struct analyze_assembly_solve_result result;
    fastf_t value = 0.0;
    struct analyze_assembly *assembly = make_pair(ANALYZE_ASSEMBLY_PRISMATIC, &joint);
    if (!assembly)
	return false;
    target[11] = 7.5;
    analyze_assembly_ik_options_init(&options);
    options.position_tolerance = TOLERANCE;
    const bool success = analyze_assembly_set_joint_limit(assembly, joint, 0, -10.0, 10.0) == ANALYZE_ASSEMBLY_OK &&
	analyze_assembly_set_joint_value(assembly, joint, 0, 11.0) == ANALYZE_ASSEMBLY_LIMIT &&
	analyze_assembly_solve_inverse(assembly, 0, 1, target, &options, &result) == ANALYZE_ASSEMBLY_OK &&
	analyze_assembly_get_joint_value(assembly, joint, 0, &value) == ANALYZE_ASSEMBLY_OK && near(value, 7.5);
    analyze_assembly_destroy(assembly);
    return success;
}

static int
always_collide(void *, size_t, size_t, const mat_t, const mat_t)
{
    return 1;
}

static bool
test_collision_and_cycle()
{
    mat_t identity = MAT_INIT_IDN;
    size_t first = 0;
    size_t second = 0;
    size_t base = 0;
    size_t link = 0;
    struct analyze_assembly_solve_result result;
    struct analyze_assembly *assembly = make_pair(ANALYZE_ASSEMBLY_FIXED, &first);
    if (!assembly)
	return false;
    if (analyze_assembly_set_collision_callback(assembly, always_collide, NULL) ||
	analyze_assembly_solve_forward(assembly, 0, &result) != ANALYZE_ASSEMBLY_COLLISION ||
	result.collision_count != 1) {
	analyze_assembly_destroy(assembly);
	return false;
    }
    analyze_assembly_destroy(assembly);
    assembly = analyze_assembly_create();
    const bool success = assembly &&
	analyze_assembly_add_body(assembly, "base", identity, 1, &base) == ANALYZE_ASSEMBLY_OK &&
	analyze_assembly_add_body(assembly, "link", identity, 0, &link) == ANALYZE_ASSEMBLY_OK &&
	analyze_assembly_add_joint(assembly, "first", ANALYZE_ASSEMBLY_FIXED, base, link, identity, identity, &first) == ANALYZE_ASSEMBLY_OK &&
	analyze_assembly_add_joint(assembly, "second", ANALYZE_ASSEMBLY_FIXED, base, link, identity, identity, &second) == ANALYZE_ASSEMBLY_OK &&
	analyze_assembly_solve_forward(assembly, 0, &result) == ANALYZE_ASSEMBLY_CYCLE && result.cycle_count == 1;
    analyze_assembly_destroy(assembly);
    return success;
}

static bool
test_aliases()
{
    int type = -1;
    return analyze_assembly_joint_type_from_name("hinge", &type) == ANALYZE_ASSEMBLY_OK &&
	type == ANALYZE_ASSEMBLY_REVOLUTE &&
	analyze_assembly_joint_type_from_name("cylindrical", &type) == ANALYZE_ASSEMBLY_OK &&
	type == ANALYZE_ASSEMBLY_COAXIAL &&
	analyze_assembly_joint_type_from_name("coincident", &type) == ANALYZE_ASSEMBLY_OK &&
	type == ANALYZE_ASSEMBLY_FIXED;
}

} /* namespace */

int
main(int UNUSED(argc), char *argv[])
{
    bu_setprogname(argv[0]);

    const struct { const char *name; bool (*function)(); } tests[] = {
	{"joint families", test_joint_families},
	{"inverse and limits", test_inverse_and_limits},
	{"collision and cycle", test_collision_and_cycle},
	{"type aliases", test_aliases}
    };
    int failures = 0;
    for (const auto &test : tests) {
	if (!test.function()) {
	    std::fprintf(stderr, "assembly test failed: %s\n", test.name);
	    ++failures;
	}
    }
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
