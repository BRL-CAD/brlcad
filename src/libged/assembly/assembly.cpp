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
/** @file libged/assembly/assembly.cpp */

#include "common.h"

#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "analyze/assembly.h"
#include "bn.h"
#include "ged.h"
#include "raytrace.h"

#include "../include/plugin.h"


namespace {


constexpr size_t MATRIX_ELEMENTS = 16;
constexpr size_t MAX_JOINT_DOFS = 3;
constexpr fastf_t PI = 3.141592653589793238462643383279502884;
constexpr fastf_t DEGREES_PER_RADIAN = 180.0 / PI;
constexpr fastf_t RADIANS_PER_DEGREE = PI / 180.0;
constexpr fastf_t FRAME_TOLERANCE = 1.0e-10;
static const char *ASSEMBLY_PAYLOAD_PREFIX = "brlcad-assembly-v1;";

using Matrix = std::array<fastf_t, MATRIX_ELEMENTS>;

struct JointLimit {
    bool enabled = false;
    fastf_t lower = 0.0;
    fastf_t upper = 0.0;
};

struct Definition {
    std::string name;
    int type = ANALYZE_ASSEMBLY_FIXED;
    std::string body_a;
    std::string body_b;
    Matrix frame_a{};
    Matrix frame_b{};
    std::array<fastf_t, MAX_JOINT_DOFS> values{};
    std::array<JointLimit, MAX_JOINT_DOFS> limits{};
    fastf_t screw_pitch = 0.0;
    fastf_t tangent_distance = 0.0;
};

struct RuntimeBody {
    std::string path;
    size_t solver_index = 0;
    Matrix initial_pose{};
};

struct RuntimeAssembly {
    struct analyze_assembly *solver = NULL;
    std::vector<RuntimeBody> bodies;
    std::vector<size_t> joint_indices;

    ~RuntimeAssembly()
    {
	analyze_assembly_destroy(solver);
    }
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

static void
copy_to_mat(const Matrix &source, mat_t destination)
{
    for (size_t index = 0; index < MATRIX_ELEMENTS; ++index)
	destination[index] = source[index];
}

static Matrix
copy_from_mat(const mat_t source)
{
    Matrix destination{};
    for (size_t index = 0; index < MATRIX_ELEMENTS; ++index)
	destination[index] = source[index];
    return destination;
}

static bool
parse_number(const std::string &text, fastf_t *value)
{
    if (!value || text.empty())
	return false;
    char *end = NULL;
    errno = 0;
    const double parsed = std::strtod(text.c_str(), &end);
    if (errno || end == text.c_str() || *end || !std::isfinite(parsed))
	return false;
    *value = parsed;
    return true;
}

static bool
parse_integer(const std::string &text, int *value)
{
    if (!value || text.empty())
	return false;
    char *end = NULL;
    errno = 0;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (errno || end == text.c_str() || *end || parsed < std::numeric_limits<int>::min() ||
	parsed > std::numeric_limits<int>::max())
	return false;
    *value = static_cast<int>(parsed);
    return true;
}

static std::string
format_number(fastf_t value)
{
    std::ostringstream stream;
    stream << std::setprecision(std::numeric_limits<fastf_t>::max_digits10) << value;
    return stream.str();
}

static std::string
encode_numbers(const fastf_t *values, size_t count)
{
    std::ostringstream stream;
    for (size_t index = 0; index < count; ++index) {
	if (index)
	    stream << ',';
	stream << std::setprecision(std::numeric_limits<fastf_t>::max_digits10) << values[index];
    }
    return stream.str();
}

static bool
decode_numbers(const std::string &text, fastf_t *values, size_t count)
{
    size_t start = 0;
    for (size_t index = 0; index < count; ++index) {
	const size_t end = text.find(',', start);
	if ((index + 1 < count && end == std::string::npos) ||
	    (index + 1 == count && end != std::string::npos) ||
	    !parse_number(text.substr(start, end == std::string::npos ? std::string::npos : end - start), &values[index]))
	    return false;
	start = end + 1;
    }
    return true;
}

static char
hex_digit(unsigned char value)
{
    return value < 10 ? static_cast<char>('0' + value) : static_cast<char>('a' + value - 10);
}

static int
hex_value(char value)
{
    if (value >= '0' && value <= '9')
	return value - '0';
    if (value >= 'a' && value <= 'f')
	return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
	return value - 'A' + 10;
    return -1;
}

static std::string
hex_encode(const std::string &value)
{
    std::string encoded;
    encoded.reserve(value.size() * 2);
    for (unsigned char character : value) {
	encoded.push_back(hex_digit(character >> 4));
	encoded.push_back(hex_digit(character & 0x0f));
    }
    return encoded;
}

static bool
hex_decode(const std::string &value, std::string *decoded)
{
    if (!decoded || value.empty() || value.size() % 2)
	return false;
    decoded->clear();
    decoded->reserve(value.size() / 2);
    for (size_t index = 0; index < value.size(); index += 2) {
	const int high = hex_value(value[index]);
	const int low = hex_value(value[index + 1]);
	if (high < 0 || low < 0)
	    return false;
	decoded->push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

static bool
angular_dof(int type, size_t dof)
{
    switch (type) {
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

static std::string
serialize_definition(const Definition &definition)
{
    std::array<fastf_t, MAX_JOINT_DOFS> enabled{};
    std::array<fastf_t, MAX_JOINT_DOFS> lower{};
    std::array<fastf_t, MAX_JOINT_DOFS> upper{};
    for (size_t index = 0; index < MAX_JOINT_DOFS; ++index) {
	enabled[index] = definition.limits[index].enabled ? 1.0 : 0.0;
	lower[index] = definition.limits[index].lower;
	upper[index] = definition.limits[index].upper;
    }
    return std::string(ASSEMBLY_PAYLOAD_PREFIX) +
	"type=" + std::to_string(definition.type) +
	";a=" + hex_encode(definition.body_a) +
	";b=" + hex_encode(definition.body_b) +
	";fa=" + encode_numbers(definition.frame_a.data(), definition.frame_a.size()) +
	";fb=" + encode_numbers(definition.frame_b.data(), definition.frame_b.size()) +
	";v=" + encode_numbers(definition.values.data(), definition.values.size()) +
	";le=" + encode_numbers(enabled.data(), enabled.size()) +
	";ll=" + encode_numbers(lower.data(), lower.size()) +
	";lu=" + encode_numbers(upper.data(), upper.size()) +
	";p=" + format_number(definition.screw_pitch) +
	";d=" + format_number(definition.tangent_distance);
}

static bool
deserialize_definition(const std::string &payload, Definition *definition)
{
    if (!definition || payload.rfind(ASSEMBLY_PAYLOAD_PREFIX, 0) != 0)
	return false;
    std::map<std::string, std::string> fields;
    size_t start = std::strlen(ASSEMBLY_PAYLOAD_PREFIX);
    while (start < payload.size()) {
	const size_t end = payload.find(';', start);
	const std::string field = payload.substr(start, end == std::string::npos ? std::string::npos : end - start);
	const size_t equal = field.find('=');
	if (equal == std::string::npos || equal == 0 || fields.count(field.substr(0, equal)))
	    return false;
	fields[field.substr(0, equal)] = field.substr(equal + 1);
	if (end == std::string::npos)
	    break;
	start = end + 1;
    }
    static const char *required[] = {"type", "a", "b", "fa", "fb", "v", "le", "ll", "lu", "p", "d"};
    for (const char *name : required) {
	if (!fields.count(name))
	    return false;
    }
    if (!parse_integer(fields["type"], &definition->type) ||
	!analyze_assembly_joint_type_name(definition->type) ||
	!hex_decode(fields["a"], &definition->body_a) || !hex_decode(fields["b"], &definition->body_b) ||
	!decode_numbers(fields["fa"], definition->frame_a.data(), definition->frame_a.size()) ||
	!decode_numbers(fields["fb"], definition->frame_b.data(), definition->frame_b.size()) ||
	!decode_numbers(fields["v"], definition->values.data(), definition->values.size()) ||
	!parse_number(fields["p"], &definition->screw_pitch) ||
	!parse_number(fields["d"], &definition->tangent_distance))
	return false;
    std::array<fastf_t, MAX_JOINT_DOFS> enabled{};
    std::array<fastf_t, MAX_JOINT_DOFS> lower{};
    std::array<fastf_t, MAX_JOINT_DOFS> upper{};
    if (!decode_numbers(fields["le"], enabled.data(), enabled.size()) ||
	!decode_numbers(fields["ll"], lower.data(), lower.size()) ||
	!decode_numbers(fields["lu"], upper.data(), upper.size()))
	return false;
    for (size_t index = 0; index < MAX_JOINT_DOFS; ++index) {
	if (enabled[index] != 0.0 && enabled[index] != 1.0)
	    return false;
	definition->limits[index].enabled = enabled[index] != 0.0;
	definition->limits[index].lower = lower[index];
	definition->limits[index].upper = upper[index];
	if (definition->limits[index].enabled && lower[index] > upper[index])
	    return false;
    }
    return !definition->body_a.empty() && !definition->body_b.empty();
}

