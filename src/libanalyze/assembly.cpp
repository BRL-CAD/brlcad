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
/** @file libanalyze/assembly.cpp */

#include "common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "analyze/assembly.h"
#include "bn.h"
#include "bu/str.h"


namespace {


constexpr size_t MATRIX_ELEMENTS = 16;
constexpr size_t MAX_JOINT_DOFS = 3;
constexpr fastf_t PI = 3.141592653589793238462643383279502884;
constexpr fastf_t TWO_PI = 2.0 * PI;
constexpr fastf_t RIGID_TOLERANCE = 1.0e-8;
constexpr fastf_t DEFAULT_POSITION_TOLERANCE = 1.0e-6;
constexpr fastf_t DEFAULT_ANGULAR_TOLERANCE = 1.0e-6;
constexpr fastf_t DEFAULT_DAMPING = 1.0e-4;
constexpr fastf_t DEFAULT_ORIENTATION_WEIGHT = 1.0;
constexpr fastf_t DEFAULT_LINEAR_STEP = 1.0e-3;
constexpr fastf_t DEFAULT_ANGULAR_STEP = 1.0e-4;
constexpr size_t DEFAULT_MAX_ITERATIONS = 64;
constexpr fastf_t LINEAR_SOLVE_TOLERANCE = 1.0e-14;

using Matrix = std::array<fastf_t, MATRIX_ELEMENTS>;
using Vector3 = std::array<fastf_t, 3>;
using Vector6 = std::array<fastf_t, 6>;

struct Body {
    std::string name;
    Matrix pose;
    bool fixed;
};

struct Joint {
    std::string name;
    int type;
    size_t body_a;
    size_t body_b;
    Matrix frame_a;
    Matrix frame_b;
    std::array<fastf_t, MAX_JOINT_DOFS> values{};
    std::array<fastf_t, MAX_JOINT_DOFS> lower{};
    std::array<fastf_t, MAX_JOINT_DOFS> upper{};
    std::array<bool, MAX_JOINT_DOFS> limited{};
    fastf_t screw_pitch = 0.0;
    fastf_t tangent_distance = 0.0;
};

static Matrix
identity_matrix()
{
    return {{
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    }};
}

static Matrix
from_mat(const fastf_t *matrix)
{
    Matrix result{};
    std::copy(matrix, matrix + MATRIX_ELEMENTS, result.begin());
    return result;
}

static void
to_mat(const Matrix &matrix, fastf_t *result)
{
    std::copy(matrix.begin(), matrix.end(), result);
}

static Matrix
multiply(const Matrix &left, const Matrix &right)
{
    Matrix result{};
    for (size_t row = 0; row < 4; ++row) {
	for (size_t column = 0; column < 4; ++column) {
	    for (size_t index = 0; index < 4; ++index)
		result[row * 4 + column] += left[row * 4 + index] * right[index * 4 + column];
	}
    }
    return result;
}

static bool
inverse(const Matrix &matrix, Matrix *result)
{
    mat_t input;
    mat_t output;
    to_mat(matrix, input);
    if (!bn_mat_inverse(output, input))
	return false;
    *result = from_mat(output);
    return true;
}

static Matrix
translation(fastf_t x, fastf_t y, fastf_t z)
{
    Matrix result = identity_matrix();
    result[3] = x;
    result[7] = y;
    result[11] = z;
    return result;
}

static Matrix
rotation_x(fastf_t angle)
{
    Matrix result = identity_matrix();
    const fastf_t sine = std::sin(angle);
    const fastf_t cosine = std::cos(angle);
    result[5] = cosine;
    result[6] = -sine;
    result[9] = sine;
    result[10] = cosine;
    return result;
}

static Matrix
rotation_y(fastf_t angle)
{
    Matrix result = identity_matrix();
    const fastf_t sine = std::sin(angle);
    const fastf_t cosine = std::cos(angle);
    result[0] = cosine;
    result[2] = sine;
    result[8] = -sine;
    result[10] = cosine;
    return result;
}

static Matrix
rotation_z(fastf_t angle)
{
    Matrix result = identity_matrix();
    const fastf_t sine = std::sin(angle);
    const fastf_t cosine = std::cos(angle);
    result[0] = cosine;
    result[1] = -sine;
    result[4] = sine;
    result[5] = cosine;
    return result;
}

static Vector3
translation_of(const Matrix &matrix)
{
    return {{matrix[3], matrix[7], matrix[11]}};
}

static Vector3
subtract(const Vector3 &left, const Vector3 &right)
{
    return {{left[0] - right[0], left[1] - right[1], left[2] - right[2]}};
}

static fastf_t
dot(const Vector3 &left, const Vector3 &right)
{
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

static Vector3
cross(const Vector3 &left, const Vector3 &right)
{
    return {{
	left[1] * right[2] - left[2] * right[1],
	left[2] * right[0] - left[0] * right[2],
	left[0] * right[1] - left[1] * right[0]
    }};
}

static fastf_t
length(const Vector3 &vector)
{
    return std::sqrt(dot(vector, vector));
}

static bool
normalize(Vector3 *vector)
{
    const fastf_t magnitude = length(*vector);
    if (magnitude <= RIGID_TOLERANCE)
	return false;
    for (fastf_t &value : *vector)
	value /= magnitude;
    return true;
}

static fastf_t
clamp(fastf_t value, fastf_t lower, fastf_t upper)
{
    return std::max(lower, std::min(value, upper));
}

static bool
is_rigid(const Matrix &matrix)
{
    for (fastf_t value : matrix) {
	if (!std::isfinite(value))
	    return false;
    }
    if (std::fabs(matrix[12]) > RIGID_TOLERANCE ||
	std::fabs(matrix[13]) > RIGID_TOLERANCE ||
	std::fabs(matrix[14]) > RIGID_TOLERANCE ||
	std::fabs(matrix[15] - 1.0) > RIGID_TOLERANCE)
	return false;

    const Vector3 x = {{matrix[0], matrix[4], matrix[8]}};
    const Vector3 y = {{matrix[1], matrix[5], matrix[9]}};
    const Vector3 z = {{matrix[2], matrix[6], matrix[10]}};
    return std::fabs(length(x) - 1.0) <= RIGID_TOLERANCE &&
	std::fabs(length(y) - 1.0) <= RIGID_TOLERANCE &&
	std::fabs(length(z) - 1.0) <= RIGID_TOLERANCE &&
	std::fabs(dot(x, y)) <= RIGID_TOLERANCE &&
	std::fabs(dot(x, z)) <= RIGID_TOLERANCE &&
	std::fabs(dot(y, z)) <= RIGID_TOLERANCE &&
	std::fabs(dot(cross(x, y), z) - 1.0) <= RIGID_TOLERANCE;
}

} /* namespace */

struct analyze_assembly {
    std::vector<Body> bodies;
    std::vector<Joint> joints;
    analyze_assembly_collision_callback_t collision_callback = NULL;
    void *collision_callback_data = NULL;
};

namespace {

struct Connection {
    size_t joint_index;
    bool from_a;
};

struct ForwardState {
    std::vector<bool> visited;
    std::vector<size_t> parent_body;
    std::vector<size_t> parent_joint;
};

struct ActiveDof {
    size_t joint_index;
    size_t dof;
};

static bool
valid_type(int type)
{
    return type >= ANALYZE_ASSEMBLY_FIXED && type <= ANALYZE_ASSEMBLY_TANGENT;
}

static size_t
dof_count(int type)
{
    switch (type) {
	case ANALYZE_ASSEMBLY_FIXED:
	    return 0;
	case ANALYZE_ASSEMBLY_REVOLUTE:
	case ANALYZE_ASSEMBLY_PRISMATIC:
	case ANALYZE_ASSEMBLY_SCREW:
	    return 1;
	case ANALYZE_ASSEMBLY_COAXIAL:
	    return 2;
	case ANALYZE_ASSEMBLY_SPHERICAL:
	case ANALYZE_ASSEMBLY_PLANAR:
	case ANALYZE_ASSEMBLY_TANGENT:
	    return 3;
	default:
	    return 0;
    }
}

static bool
angular_dof(const Joint &joint, size_t dof)
{
    switch (joint.type) {
	case ANALYZE_ASSEMBLY_REVOLUTE:
	case ANALYZE_ASSEMBLY_SPHERICAL:
	case ANALYZE_ASSEMBLY_SCREW:
	    return true;
	case ANALYZE_ASSEMBLY_COAXIAL:
	    return dof == 1;
	case ANALYZE_ASSEMBLY_PLANAR:
	case ANALYZE_ASSEMBLY_TANGENT:
	    return dof == 2;
	default:
	    return false;
    }
}

static bool
valid_body(const analyze_assembly *assembly, size_t body_index)
{
    return assembly && body_index < assembly->bodies.size();
}

static bool
valid_joint(const analyze_assembly *assembly, size_t joint_index)
{
    return assembly && joint_index < assembly->joints.size();
}

static fastf_t
limited_value(const Joint &joint, size_t dof, fastf_t value)
{
    return joint.limited[dof] ? clamp(value, joint.lower[dof], joint.upper[dof]) : value;
}

static bool
motion(const Joint &joint, Matrix *result)
{
    switch (joint.type) {
	case ANALYZE_ASSEMBLY_FIXED:
	    *result = identity_matrix();
	    return true;
	case ANALYZE_ASSEMBLY_REVOLUTE:
	    *result = rotation_z(joint.values[0]);
	    return true;
	case ANALYZE_ASSEMBLY_PRISMATIC:
	    *result = translation(0.0, 0.0, joint.values[0]);
	    return true;
	case ANALYZE_ASSEMBLY_COAXIAL:
	    *result = multiply(translation(0.0, 0.0, joint.values[0]), rotation_z(joint.values[1]));
	    return true;
	case ANALYZE_ASSEMBLY_SPHERICAL:
	    *result = multiply(rotation_z(joint.values[2]),
		multiply(rotation_y(joint.values[1]), rotation_x(joint.values[0])));
	    return true;
	case ANALYZE_ASSEMBLY_SCREW:
	    if (std::fabs(joint.screw_pitch) <= RIGID_TOLERANCE)
		return false;
	    *result = multiply(translation(0.0, 0.0, joint.screw_pitch * joint.values[0] / TWO_PI),
		rotation_z(joint.values[0]));
	    return true;
	case ANALYZE_ASSEMBLY_PLANAR:
	    *result = multiply(translation(joint.values[0], joint.values[1], 0.0), rotation_z(joint.values[2]));
	    return true;
	case ANALYZE_ASSEMBLY_TANGENT:
	    *result = multiply(
		multiply(translation(joint.values[0], joint.values[1], joint.tangent_distance),
		    rotation_z(joint.values[2])),
		rotation_x(PI));
	    return true;
	default:
	    return false;
    }
}

static bool
desired_b(const Body &body_a, const Joint &joint, Matrix *result)
{
    Matrix inverse_frame_b;
    Matrix joint_motion;
    if (!inverse(joint.frame_b, &inverse_frame_b) || !motion(joint, &joint_motion))
	return false;
    *result = multiply(multiply(multiply(body_a.pose, joint.frame_a), joint_motion), inverse_frame_b);
    return true;
}

static bool
desired_a(const Body &body_b, const Joint &joint, Matrix *result)
{
    Matrix inverse_frame_a;
    Matrix inverse_motion;
    Matrix joint_motion;
    if (!inverse(joint.frame_a, &inverse_frame_a) || !motion(joint, &joint_motion) ||
	!inverse(joint_motion, &inverse_motion))
	return false;
    *result = multiply(multiply(multiply(body_b.pose, joint.frame_b), inverse_motion), inverse_frame_a);
    return true;
}

static Matrix
rotation_transpose(const Matrix &matrix)
{
    Matrix result = identity_matrix();
    result[0] = matrix[0];
    result[1] = matrix[4];
    result[2] = matrix[8];
    result[4] = matrix[1];
    result[5] = matrix[5];
    result[6] = matrix[9];
    result[8] = matrix[2];
    result[9] = matrix[6];
    result[10] = matrix[10];
    return result;
}

static Vector3
rotation_error(const Matrix &current, const Matrix &target, fastf_t *angle)
{
    const Matrix difference = multiply(target, rotation_transpose(current));
    const fastf_t value = std::acos(clamp((difference[0] + difference[5] + difference[10] - 1.0) * 0.5,
	-1.0, 1.0));
    Vector3 axis = {{
	difference[9] - difference[6],
	difference[2] - difference[8],
	difference[4] - difference[1]
    }};
    if (angle)
	*angle = value;
    if (value <= RIGID_TOLERANCE)
	return {{0.0, 0.0, 0.0}};
    if (!normalize(&axis))
	axis = {{1.0, 0.0, 0.0}};
    return {{axis[0] * value, axis[1] * value, axis[2] * value}};
}

static void
record_error(const Body &body_a, const Body &body_b, const Joint &joint,
	     analyze_assembly_solve_result *result)
{
    Matrix expected;
    if (!desired_b(body_a, joint, &expected)) {
	result->max_position_error = std::numeric_limits<fastf_t>::infinity();
	result->max_angular_error = std::numeric_limits<fastf_t>::infinity();
	return;
    }
    result->max_position_error = std::max(result->max_position_error,
	length(subtract(translation_of(expected), translation_of(body_b.pose))));
    fastf_t angular_error = 0.0;
    (void)rotation_error(body_b.pose, expected, &angular_error);
    result->max_angular_error = std::max(result->max_angular_error, angular_error);
}

static size_t
root_body(const analyze_assembly *assembly, size_t requested_root)
{
    if (requested_root != ANALYZE_ASSEMBLY_BODY_NONE)
	return requested_root;
    for (size_t index = 0; index < assembly->bodies.size(); ++index) {
	if (assembly->bodies[index].fixed)
	    return index;
    }
    return assembly->bodies.empty() ? ANALYZE_ASSEMBLY_BODY_NONE : 0;
}

static int
detect_collisions(analyze_assembly *assembly, const std::vector<bool> &visited,
		  analyze_assembly_solve_result *result)
{
    if (!assembly->collision_callback)
	return ANALYZE_ASSEMBLY_OK;
    for (size_t first = 0; first < assembly->bodies.size(); ++first) {
	if (!visited[first])
	    continue;
	for (size_t second = first + 1; second < assembly->bodies.size(); ++second) {
	    if (visited[second] && assembly->collision_callback(assembly->collision_callback_data,
		first, second, assembly->bodies[first].pose.data(), assembly->bodies[second].pose.data()))
		result->collision_count++;
	}
    }
    return result->collision_count ? ANALYZE_ASSEMBLY_COLLISION : ANALYZE_ASSEMBLY_OK;
}

static int
forward(analyze_assembly *assembly, size_t requested_root, bool check_collisions,
	analyze_assembly_solve_result *result, ForwardState *state)
{
    const size_t root = root_body(assembly, requested_root);
    if (!valid_body(assembly, root))
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;

    std::vector<std::vector<Connection> > links(assembly->bodies.size());
    std::vector<bool> processed(assembly->joints.size(), false);
    state->visited.assign(assembly->bodies.size(), false);
    state->parent_body.assign(assembly->bodies.size(), ANALYZE_ASSEMBLY_BODY_NONE);
    state->parent_joint.assign(assembly->bodies.size(), ANALYZE_ASSEMBLY_BODY_NONE);
    for (size_t index = 0; index < assembly->joints.size(); ++index) {
	const Joint &joint = assembly->joints[index];
	links[joint.body_a].push_back({index, true});
	links[joint.body_b].push_back({index, false});
    }

    std::deque<size_t> pending;
    state->visited[root] = true;
    pending.push_back(root);
    while (!pending.empty()) {
	const size_t current = pending.front();
	pending.pop_front();
	for (const Connection &link : links[current]) {
	    const Joint &joint = assembly->joints[link.joint_index];
	    const size_t other = link.from_a ? joint.body_b : joint.body_a;
	    Matrix pose;
	    const bool valid_pose = link.from_a ? desired_b(assembly->bodies[current], joint, &pose) :
		desired_a(assembly->bodies[current], joint, &pose);
	    if (!valid_pose)
		return ANALYZE_ASSEMBLY_ERROR;
	    if (!state->visited[other]) {
		if (assembly->bodies[other].fixed) {
		    record_error(assembly->bodies[joint.body_a], assembly->bodies[joint.body_b], joint, result);
		    result->cycle_count++;
		} else {
		    assembly->bodies[other].pose = pose;
		}
		state->visited[other] = true;
		state->parent_body[other] = current;
		state->parent_joint[other] = link.joint_index;
		pending.push_back(other);
	    } else if (!processed[link.joint_index]) {
		record_error(assembly->bodies[joint.body_a], assembly->bodies[joint.body_b], joint, result);
		result->cycle_count++;
	    }
	    processed[link.joint_index] = true;
	}
    }
    for (bool visited : state->visited) {
	if (visited)
	    result->reachable_body_count++;
    }
    result->disconnected_body_count = assembly->bodies.size() - result->reachable_body_count;
    if (check_collisions) {
	const int collision_result = detect_collisions(assembly, state->visited, result);
	if (collision_result != ANALYZE_ASSEMBLY_OK)
	    return collision_result;
    }
    return result->cycle_count ? ANALYZE_ASSEMBLY_CYCLE : ANALYZE_ASSEMBLY_OK;
}

static Vector6
target_error(const Matrix &current, const Matrix &target, fastf_t orientation_weight,
	     fastf_t *position_error, fastf_t *angular_error)
{
    const Vector3 translation_error = subtract(translation_of(target), translation_of(current));
    const Vector3 angular_vector = rotation_error(current, target, angular_error);
    if (position_error)
	*position_error = length(translation_error);
    return {{
	translation_error[0],
	translation_error[1],
	translation_error[2],
	angular_vector[0] * orientation_weight,
	angular_vector[1] * orientation_weight,
	angular_vector[2] * orientation_weight
    }};
}

static bool
solve_linear(std::vector<std::vector<fastf_t> > matrix, std::vector<fastf_t> values,
	     std::vector<fastf_t> *solution)
{
    for (size_t column = 0; column < values.size(); ++column) {
	size_t pivot = column;
	for (size_t row = column + 1; row < values.size(); ++row) {
	    if (std::fabs(matrix[row][column]) > std::fabs(matrix[pivot][column]))
		pivot = row;
	}
	if (std::fabs(matrix[pivot][column]) <= LINEAR_SOLVE_TOLERANCE)
	    return false;
	if (pivot != column) {
	    std::swap(matrix[pivot], matrix[column]);
	    std::swap(values[pivot], values[column]);
	}
	const fastf_t divisor = matrix[column][column];
	for (size_t index = column; index < values.size(); ++index)
	    matrix[column][index] /= divisor;
	values[column] /= divisor;
	for (size_t row = 0; row < values.size(); ++row) {
	    if (row == column)
		continue;
	    const fastf_t scale = matrix[row][column];
	    for (size_t index = column; index < values.size(); ++index)
		matrix[row][index] -= scale * matrix[column][index];
	    values[row] -= scale * values[column];
	}
    }
    *solution = values;
    return true;
}

static std::vector<ActiveDof>
path_dofs(const analyze_assembly *assembly, const ForwardState &state,
	  size_t root, size_t target)
{
    std::vector<size_t> joints;
    std::vector<ActiveDof> result;
    for (size_t current = target; current != root; current = state.parent_body[current]) {
	if (current == ANALYZE_ASSEMBLY_BODY_NONE || state.parent_joint[current] == ANALYZE_ASSEMBLY_BODY_NONE)
	    return std::vector<ActiveDof>();
	joints.push_back(state.parent_joint[current]);
    }
    std::reverse(joints.begin(), joints.end());
    for (size_t joint_index : joints) {
	for (size_t dof = 0; dof < dof_count(assembly->joints[joint_index].type); ++dof)
	    result.push_back({joint_index, dof});
    }
    return result;
}

} /* namespace */

extern "C" struct analyze_assembly *
analyze_assembly_create(void)
{
    return new analyze_assembly();
}

extern "C" void
analyze_assembly_destroy(struct analyze_assembly *assembly)
{
    delete assembly;
}

extern "C" void
analyze_assembly_ik_options_init(struct analyze_assembly_ik_options *options)
{
    if (!options)
	return;
    options->max_iterations = DEFAULT_MAX_ITERATIONS;
    options->position_tolerance = DEFAULT_POSITION_TOLERANCE;
    options->angular_tolerance = DEFAULT_ANGULAR_TOLERANCE;
    options->damping = DEFAULT_DAMPING;
    options->orientation_weight = DEFAULT_ORIENTATION_WEIGHT;
    options->linear_step = DEFAULT_LINEAR_STEP;
    options->angular_step = DEFAULT_ANGULAR_STEP;
    options->check_collisions = 0;
}

extern "C" void
analyze_assembly_solve_result_init(struct analyze_assembly_solve_result *result)
{
    if (result)
	std::memset(result, 0, sizeof(*result));
}

extern "C" const char *
analyze_assembly_result_message(int result)
{
    switch (result) {
	case ANALYZE_ASSEMBLY_OK: return "ok";
	case ANALYZE_ASSEMBLY_ERROR: return "assembly solve failed";
	case ANALYZE_ASSEMBLY_INVALID_ARGUMENT: return "invalid assembly argument";
	case ANALYZE_ASSEMBLY_NOT_FOUND: return "assembly body or joint was not found";
	case ANALYZE_ASSEMBLY_LIMIT: return "joint value violates its limit";
	case ANALYZE_ASSEMBLY_DISCONNECTED: return "target body is disconnected from the root";
	case ANALYZE_ASSEMBLY_CYCLE: return "assembly contains a kinematic cycle";
	case ANALYZE_ASSEMBLY_COLLISION: return "assembly motion produced a collision";
	case ANALYZE_ASSEMBLY_NO_CONVERGENCE: return "inverse kinematics did not converge";
	default: return "unknown assembly result";
    }
}

extern "C" const char *
analyze_assembly_joint_type_name(int type)
{
    static const char *names[] = {
	"fixed", "revolute", "prismatic", "coaxial", "spherical", "screw", "planar", "tangent"
    };
    return valid_type(type) ? names[type] : NULL;
}

extern "C" int
analyze_assembly_joint_type_from_name(const char *name, int *type)
{
    if (!name || !type)
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    static const struct { const char *name; int type; } names[] = {
	{"fixed", ANALYZE_ASSEMBLY_FIXED},
	{"coincident", ANALYZE_ASSEMBLY_FIXED},
	{"mate", ANALYZE_ASSEMBLY_FIXED},
	{"revolute", ANALYZE_ASSEMBLY_REVOLUTE},
	{"hinge", ANALYZE_ASSEMBLY_REVOLUTE},
	{"prismatic", ANALYZE_ASSEMBLY_PRISMATIC},
	{"slider", ANALYZE_ASSEMBLY_PRISMATIC},
	{"coaxial", ANALYZE_ASSEMBLY_COAXIAL},
	{"cylindrical", ANALYZE_ASSEMBLY_COAXIAL},
	{"spherical", ANALYZE_ASSEMBLY_SPHERICAL},
	{"ball", ANALYZE_ASSEMBLY_SPHERICAL},
	{"screw", ANALYZE_ASSEMBLY_SCREW},
	{"planar", ANALYZE_ASSEMBLY_PLANAR},
	{"tangent", ANALYZE_ASSEMBLY_TANGENT}
    };
    for (const auto &entry : names) {
	if (BU_STR_EQUAL(name, entry.name)) {
	    *type = entry.type;
	    return ANALYZE_ASSEMBLY_OK;
	}
    }
    return ANALYZE_ASSEMBLY_NOT_FOUND;
}

extern "C" size_t
analyze_assembly_joint_dof_count(int type)
{
    return dof_count(type);
}

extern "C" int
analyze_assembly_add_body(struct analyze_assembly *assembly, const char *name,
			  const mat_t pose, int fixed, size_t *body_index)
{
    if (!assembly || !name || !name[0] || !pose || !body_index)
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    const Matrix body_pose = from_mat(pose);
    if (!is_rigid(body_pose))
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    assembly->bodies.push_back({name, body_pose, fixed != 0});
    *body_index = assembly->bodies.size() - 1;
    return ANALYZE_ASSEMBLY_OK;
}

extern "C" int
analyze_assembly_set_body_transform(struct analyze_assembly *assembly, size_t body_index,
				     const mat_t pose)
{
    if (!valid_body(assembly, body_index) || !pose)
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    const Matrix body_pose = from_mat(pose);
    if (!is_rigid(body_pose))
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    assembly->bodies[body_index].pose = body_pose;
    return ANALYZE_ASSEMBLY_OK;
}

extern "C" int
analyze_assembly_get_body_transform(const struct analyze_assembly *assembly, size_t body_index,
				     mat_t pose)
{
    if (!valid_body(assembly, body_index) || !pose)
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    to_mat(assembly->bodies[body_index].pose, pose);
    return ANALYZE_ASSEMBLY_OK;
}

extern "C" int
analyze_assembly_add_joint(struct analyze_assembly *assembly, const char *name, int type,
			   size_t body_a, size_t body_b, const mat_t frame_a,
			   const mat_t frame_b, size_t *joint_index)
{
    if (!assembly || !name || !name[0] || !joint_index || !valid_type(type) ||
	!valid_body(assembly, body_a) || !valid_body(assembly, body_b) || body_a == body_b ||
	!frame_a || !frame_b)
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    const Matrix local_frame_a = from_mat(frame_a);
    const Matrix local_frame_b = from_mat(frame_b);
    if (!is_rigid(local_frame_a) || !is_rigid(local_frame_b))
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    Joint joint;
    joint.name = name;
    joint.type = type;
    joint.body_a = body_a;
    joint.body_b = body_b;
    joint.frame_a = local_frame_a;
    joint.frame_b = local_frame_b;
    assembly->joints.push_back(joint);
    *joint_index = assembly->joints.size() - 1;
    return ANALYZE_ASSEMBLY_OK;
}

extern "C" int
analyze_assembly_set_joint_value(struct analyze_assembly *assembly, size_t joint_index,
				 size_t dof, fastf_t value)
{
    if (!valid_joint(assembly, joint_index) || !std::isfinite(value) ||
	dof >= dof_count(assembly->joints[joint_index].type))
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    Joint &joint = assembly->joints[joint_index];
    if (joint.limited[dof] && (value < joint.lower[dof] || value > joint.upper[dof]))
	return ANALYZE_ASSEMBLY_LIMIT;
    joint.values[dof] = value;
    return ANALYZE_ASSEMBLY_OK;
}

extern "C" int
analyze_assembly_get_joint_value(const struct analyze_assembly *assembly, size_t joint_index,
				 size_t dof, fastf_t *value)
{
    if (!valid_joint(assembly, joint_index) || !value ||
	dof >= dof_count(assembly->joints[joint_index].type))
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    *value = assembly->joints[joint_index].values[dof];
    return ANALYZE_ASSEMBLY_OK;
}

extern "C" int
analyze_assembly_set_joint_limit(struct analyze_assembly *assembly, size_t joint_index,
				 size_t dof, fastf_t lower, fastf_t upper)
{
    if (!valid_joint(assembly, joint_index) || !std::isfinite(lower) || !std::isfinite(upper) ||
	dof >= dof_count(assembly->joints[joint_index].type) || lower > upper)
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    Joint &joint = assembly->joints[joint_index];
    joint.lower[dof] = lower;
    joint.upper[dof] = upper;
    joint.limited[dof] = true;
    joint.values[dof] = limited_value(joint, dof, joint.values[dof]);
    return ANALYZE_ASSEMBLY_OK;
}

extern "C" int
analyze_assembly_set_screw_pitch(struct analyze_assembly *assembly, size_t joint_index, fastf_t pitch)
{
    if (!valid_joint(assembly, joint_index) || !std::isfinite(pitch) ||
	assembly->joints[joint_index].type != ANALYZE_ASSEMBLY_SCREW ||
	std::fabs(pitch) <= RIGID_TOLERANCE)
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    assembly->joints[joint_index].screw_pitch = pitch;
    return ANALYZE_ASSEMBLY_OK;
}

extern "C" int
analyze_assembly_set_tangent_distance(struct analyze_assembly *assembly, size_t joint_index,
				      fastf_t distance)
{
    if (!valid_joint(assembly, joint_index) || !std::isfinite(distance) ||
	assembly->joints[joint_index].type != ANALYZE_ASSEMBLY_TANGENT)
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    assembly->joints[joint_index].tangent_distance = distance;
    return ANALYZE_ASSEMBLY_OK;
}

extern "C" int
analyze_assembly_set_collision_callback(struct analyze_assembly *assembly,
					analyze_assembly_collision_callback_t callback,
					void *callback_data)
{
    if (!assembly)
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    assembly->collision_callback = callback;
    assembly->collision_callback_data = callback_data;
    return ANALYZE_ASSEMBLY_OK;
}

extern "C" int
analyze_assembly_solve_forward(struct analyze_assembly *assembly, size_t root,
			       struct analyze_assembly_solve_result *result)
{
    if (!assembly || assembly->bodies.empty())
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    struct analyze_assembly_solve_result local_result;
    analyze_assembly_solve_result_init(&local_result);
    ForwardState state;
    const int status = forward(assembly, root, true, &local_result, &state);
    if (result)
	*result = local_result;
    return status;
}

extern "C" int
analyze_assembly_evaluate(const struct analyze_assembly *assembly,
			  struct analyze_assembly_solve_result *result)
{
    if (!assembly)
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    struct analyze_assembly_solve_result local_result;
    analyze_assembly_solve_result_init(&local_result);
    local_result.reachable_body_count = assembly->bodies.size();
    for (const Joint &joint : assembly->joints)
	record_error(assembly->bodies[joint.body_a], assembly->bodies[joint.body_b], joint, &local_result);
    if (result)
	*result = local_result;
    return ANALYZE_ASSEMBLY_OK;
}

extern "C" int
analyze_assembly_solve_inverse(struct analyze_assembly *assembly, size_t requested_root,
			       size_t target_body, const mat_t target_pose,
			       const struct analyze_assembly_ik_options *input_options,
			       struct analyze_assembly_solve_result *result)
{
    if (!assembly || assembly->bodies.empty() || !valid_body(assembly, target_body) || !target_pose)
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    const Matrix target = from_mat(target_pose);
    if (!is_rigid(target))
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;

    struct analyze_assembly_ik_options options;
    analyze_assembly_ik_options_init(&options);
    if (input_options)
	options = *input_options;
    if (!options.max_iterations || options.position_tolerance < 0.0 ||
	options.angular_tolerance < 0.0 || options.damping < 0.0 ||
	options.orientation_weight <= 0.0 || options.linear_step <= 0.0 || options.angular_step <= 0.0)
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;

    const size_t root = root_body(assembly, requested_root);
    if (!valid_body(assembly, root))
	return ANALYZE_ASSEMBLY_INVALID_ARGUMENT;
    struct analyze_assembly_solve_result local_result;
    analyze_assembly_solve_result_init(&local_result);
    ForwardState state;
    int status = forward(assembly, root, false, &local_result, &state);
    if (status != ANALYZE_ASSEMBLY_OK) {
	if (result)
	    *result = local_result;
	return status;
    }
    if (!state.visited[target_body]) {
	if (result)
	    *result = local_result;
	return ANALYZE_ASSEMBLY_DISCONNECTED;
    }
    const std::vector<ActiveDof> active = path_dofs(assembly, state, root, target_body);

    for (size_t iteration = 0; iteration < options.max_iterations; ++iteration) {
	analyze_assembly_solve_result_init(&local_result);
	status = forward(assembly, root, false, &local_result, &state);
	if (status != ANALYZE_ASSEMBLY_OK)
	    break;
	fastf_t position_error = 0.0;
	fastf_t angular_error = 0.0;
	const Vector6 error = target_error(assembly->bodies[target_body].pose, target,
		options.orientation_weight, &position_error, &angular_error);
	local_result.max_position_error = position_error;
	local_result.max_angular_error = angular_error;
	local_result.iteration_count = iteration;
	if (position_error <= options.position_tolerance && angular_error <= options.angular_tolerance) {
	    if (options.check_collisions)
		status = detect_collisions(assembly, state.visited, &local_result);
	    if (result)
		*result = local_result;
	    return status;
	}
	if (active.empty()) {
	    status = ANALYZE_ASSEMBLY_NO_CONVERGENCE;
	    break;
	}

	std::vector<Vector6> jacobian(active.size());
	for (size_t column = 0; column < active.size(); ++column) {
	    const ActiveDof active_dof = active[column];
	    Joint &joint = assembly->joints[active_dof.joint_index];
	    const fastf_t original = joint.values[active_dof.dof];
	    const fastf_t step = angular_dof(joint, active_dof.dof) ? options.angular_step : options.linear_step;
	    const fastf_t perturbed = limited_value(joint, active_dof.dof, original + step);
	    if (perturbed == original)
		continue;
	    joint.values[active_dof.dof] = perturbed;
	    struct analyze_assembly_solve_result perturb_result;
	    analyze_assembly_solve_result_init(&perturb_result);
	    ForwardState perturb_state;
	    const int perturb_status = forward(assembly, root, false, &perturb_result, &perturb_state);
	    if (perturb_status != ANALYZE_ASSEMBLY_OK) {
		joint.values[active_dof.dof] = original;
		return perturb_status;
	    }
	    const Vector6 perturbed_error = target_error(assembly->bodies[target_body].pose, target,
		options.orientation_weight, NULL, NULL);
	    for (size_t row = 0; row < error.size(); ++row)
		jacobian[column][row] = (perturbed_error[row] - error[row]) / (perturbed - original);
	    joint.values[active_dof.dof] = original;
	}

	std::vector<std::vector<fastf_t> > normal(active.size(), std::vector<fastf_t>(active.size(), 0.0));
	std::vector<fastf_t> rhs(active.size(), 0.0);
	for (size_t row = 0; row < active.size(); ++row) {
	    for (size_t column = 0; column < active.size(); ++column) {
		for (size_t dimension = 0; dimension < error.size(); ++dimension)
		    normal[row][column] += jacobian[row][dimension] * jacobian[column][dimension];
	    }
	    normal[row][row] += options.damping * options.damping;
	    for (size_t dimension = 0; dimension < error.size(); ++dimension)
		rhs[row] -= jacobian[row][dimension] * error[dimension];
	}
	std::vector<fastf_t> update;
	if (!solve_linear(normal, rhs, &update)) {
	    status = ANALYZE_ASSEMBLY_NO_CONVERGENCE;
	    break;
	}
	bool changed = false;
	for (size_t index = 0; index < active.size(); ++index) {
	    const ActiveDof active_dof = active[index];
	    Joint &joint = assembly->joints[active_dof.joint_index];
	    const fastf_t updated = limited_value(joint, active_dof.dof,
		joint.values[active_dof.dof] + update[index]);
	    changed = changed || updated != joint.values[active_dof.dof];
	    joint.values[active_dof.dof] = updated;
	}
	if (!changed) {
	    status = ANALYZE_ASSEMBLY_NO_CONVERGENCE;
	    break;
	}
    }

    analyze_assembly_solve_result_init(&local_result);
    ForwardState final_state;
    const int final_status = forward(assembly, root, options.check_collisions != 0, &local_result, &final_state);
    fastf_t position_error = 0.0;
    fastf_t angular_error = 0.0;
    (void)target_error(assembly->bodies[target_body].pose, target, options.orientation_weight,
	&position_error, &angular_error);
    local_result.max_position_error = position_error;
    local_result.max_angular_error = angular_error;
    local_result.iteration_count = options.max_iterations;
    if (result)
	*result = local_result;
    if (final_status != ANALYZE_ASSEMBLY_OK)
	return final_status;
    return status == ANALYZE_ASSEMBLY_OK ? ANALYZE_ASSEMBLY_NO_CONVERGENCE : status;
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
