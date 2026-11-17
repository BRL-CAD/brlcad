/*              P R E S E T _ P L A N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "vmath.h"

#include "preset_plan.h"

namespace {

constexpr double full_rotation_degrees = 360.0;
constexpr double axis_reference_threshold = 0.9;
constexpr double cut_margin_fraction = 0.05;
constexpr double minimum_cut_margin = 1.0e-9;
constexpr double vector_magnitude_tolerance = 1.0e-12;

static void
validate_settings(const PresetSettings &settings)
{
    if (settings.orientation.size() != 4 || settings.eye.size() != 3)
	throw std::runtime_error("animation preset requires a quaternion orientation and three-component eye point");
    if (settings.preset == "cut" && settings.direction.size() != 3)
	throw std::runtime_error("cut direction must contain three components");
    if ((settings.preset == "orbit" || settings.preset == "turntable") && settings.axis.size() != 3)
	throw std::runtime_error(settings.preset + " axis must contain three components");
}

static std::vector<double>
rotate_vector(const std::vector<double> &vector, const std::vector<double> &axis, double degrees)
{
    double radians = degrees * DEG2RAD;
    double cosine = std::cos(radians);
    double sine = std::sin(radians);
    double dot = axis[0]*vector[0] + axis[1]*vector[1] + axis[2]*vector[2];
    std::vector<double> cross = {
	axis[1]*vector[2] - axis[2]*vector[1],
	axis[2]*vector[0] - axis[0]*vector[2],
	axis[0]*vector[1] - axis[1]*vector[0]
    };
    return {
	vector[0]*cosine + cross[0]*sine + axis[0]*dot*(1.0-cosine),
	vector[1]*cosine + cross[1]*sine + axis[1]*dot*(1.0-cosine),
	vector[2]*cosine + cross[2]*sine + axis[2]*dot*(1.0-cosine)
    };
}

static std::vector<double>
perpendicular_reference(const std::vector<double> &axis)
{
    std::vector<double> reference = std::fabs(axis[2]) < axis_reference_threshold ?
	std::vector<double>{0.0, 0.0, 1.0} : std::vector<double>{1.0, 0.0, 0.0};
    return {
	axis[1]*reference[2] - axis[2]*reference[1],
	axis[2]*reference[0] - axis[0]*reference[2],
	axis[0]*reference[1] - axis[1]*reference[0]
    };
}

static std::vector<std::string>
turntable_paths(const PresetSettings &settings)
{
    if (!settings.object.empty()) return {settings.object};
    std::set<std::string> roots;
    for (std::string object : settings.objects) {
	while (!object.empty() && object[0] == '/') object.erase(object.begin());
	size_t separator = object.find('/');
	if (separator != std::string::npos) object.erase(separator);
	if (!object.empty()) roots.insert('/' + object);
    }
    if (roots.empty()) throw std::runtime_error("turntable animation requires at least one rendered object");
    return {roots.begin(), roots.end()};
}

static AnimationPlan
initialize_plan(const PresetSettings &settings, int &frame_count, bool &cyclic)
{
    cyclic = (settings.preset == "orbit" || settings.preset == "turntable") &&
	std::fabs(settings.angle) >= full_rotation_degrees;
    if (settings.cyclic_override >= 0) cyclic = settings.cyclic_override != 0;
    frame_count = settings.frame_count;
    if (settings.preset == "cut" && settings.cut_steps >= 2) frame_count = settings.cut_steps;
    if (frame_count < 2)
	frame_count = (int)std::llround(settings.duration * settings.fps) + (cyclic ? 0 : 1);
    if (!(settings.duration > 0.0) || settings.fps <= 0 || frame_count < 2)
	throw std::runtime_error("animation preset requires positive duration and fps and at least two frames");

    AnimationPlan plan;
    plan.duration = settings.duration;
    plan.fps = settings.fps;
    plan.cyclic = cyclic;
    plan.plays = settings.plays < 0 ? (cyclic ? 0 : 1) : settings.plays;
    plan.frames.reserve((size_t)frame_count);
    return plan;
}

static AnimationPlan
plan_cut(const PresetSettings &settings, const PresetSceneFacts &facts,
    AnimationPlan plan, int frame_count)
{
    double span = facts.sweep.second - facts.sweep.first;
    if (!(span > 0.0)) throw std::runtime_error("rendered objects have degenerate cutting-plane bounds");
    if (settings.width <= 0 || settings.height <= 0)
	throw std::runtime_error("cut animation requires positive image dimensions");
    double epsilon = std::max({std::fabs(span) / (double)std::max(settings.width, settings.height),
	std::fabs(span) * cut_margin_fraction, minimum_cut_margin});
    double first = facts.sweep.first - epsilon;
    double last = facts.sweep.second - epsilon;
    for (int frame_number = 0; frame_number < frame_count; ++frame_number) {
	double fraction = (double)frame_number / (double)(frame_count - 1);
	double distance = first + fraction * (last - first);
	AnimationFrame frame = make_animation_frame(settings.duration*fraction, settings.viewsize,
	    settings.orientation, settings.eye, settings.perspective);
	std::ostringstream plane;
	plane.precision(15);
	plane << settings.direction[0]*distance << ',' << settings.direction[1]*distance << ','
	    << settings.direction[2]*distance << ',' << settings.direction[0] << ','
	    << settings.direction[1] << ',' << settings.direction[2];
	frame.cut_plane = plane.str();
	plan.frames.push_back(std::move(frame));
    }
    return plan;
}

static AnimationPlan
plan_orbit(const PresetSettings &settings, const PresetSceneFacts &facts,
    AnimationPlan plan, int frame_count, bool cyclic)
{
    std::vector<double> axis = settings.axis;
    normalize_animation_vector(axis, "orbit axis");
    const std::vector<double> &center = facts.center;
    if (center.size() != 3) throw std::runtime_error("orbit planning requires a scene center");
    std::vector<double> radial = {
	settings.eye[0]-center[0], settings.eye[1]-center[1], settings.eye[2]-center[2]
    };
    double radius = std::sqrt(radial[0]*radial[0] + radial[1]*radial[1] + radial[2]*radial[2]);
    if (radius < vector_magnitude_tolerance) {
	radial = perpendicular_reference(axis);
	normalize_animation_vector(radial, "orbit radial direction");
	radius = settings.viewsize;
	for (double &component : radial) component *= radius;
    }
    if (settings.has_radius) {
	if (!(settings.radius > 0.0)) throw std::runtime_error("orbit radius must be positive");
	radius = settings.radius;
	normalize_animation_vector(radial, "orbit radial direction");
	for (double &component : radial) component *= radius;
    }
    if (settings.has_elevation) {
	double elevation = settings.elevation * DEG2RAD;
	double along = radial[0]*axis[0] + radial[1]*axis[1] + radial[2]*axis[2];
	std::vector<double> plane = {
	    radial[0]-along*axis[0], radial[1]-along*axis[1], radial[2]-along*axis[2]
	};
	double plane_magnitude = std::sqrt(plane[0]*plane[0] + plane[1]*plane[1] + plane[2]*plane[2]);
	if (plane_magnitude < vector_magnitude_tolerance) plane = perpendicular_reference(axis);
	normalize_animation_vector(plane, "orbit plane");
	radial = {
	    plane[0]*radius*std::cos(elevation)+axis[0]*radius*std::sin(elevation),
	    plane[1]*radius*std::cos(elevation)+axis[1]*radius*std::sin(elevation),
	    plane[2]*radius*std::cos(elevation)+axis[2]*radius*std::sin(elevation)
	};
    }
    for (int frame_number = 0; frame_number < frame_count; ++frame_number) {
	double fraction = (double)frame_number / (double)(cyclic ? frame_count : frame_count-1);
	std::vector<double> rotated = rotate_vector(radial, axis, fraction*settings.angle);
	std::vector<double> frame_eye = {
	    center[0]+rotated[0], center[1]+rotated[1], center[2]+rotated[2]
	};
	std::vector<double> direction = {
	    center[0]-frame_eye[0], center[1]-frame_eye[1], center[2]-frame_eye[2]
	};
	normalize_animation_vector(direction, "orbit viewing direction");
	std::vector<double> camera_up = axis;
	std::vector<double> right = {
	    direction[1]*axis[2]-direction[2]*axis[1],
	    direction[2]*axis[0]-direction[0]*axis[2],
	    direction[0]*axis[1]-direction[1]*axis[0]
	};
	if (std::sqrt(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]) <= SMALL_FASTF)
	    camera_up.clear();
	AnimationFrame frame = make_animation_frame(settings.duration*fraction, settings.viewsize,
	    animation_look_at_quaternion(direction, camera_up), frame_eye, settings.perspective);
	plan.frames.push_back(std::move(frame));
    }
    return plan;
}

static AnimationPlan
plan_turntable(const PresetSettings &settings, const PresetSceneFacts &facts,
    AnimationPlan plan, int frame_count, bool cyclic)
{
    std::vector<double> axis = settings.axis;
    normalize_animation_vector(axis, "turntable axis");
    const std::vector<double> &center = facts.center;
    if (center.size() != 3) throw std::runtime_error("turntable planning requires a scene center");

    if (!settings.object.empty() && settings.object.find_first_of(";\r\n\t ") != std::string::npos)
	throw std::runtime_error("turntable object path may not contain whitespace or command separators");
    std::vector<std::string> objects = turntable_paths(settings);
    for (std::string &object : objects)
	if (object[0] != '/') object.insert(object.begin(), '/');
    for (int frame_number = 0; frame_number < frame_count; ++frame_number) {
	double fraction = (double)frame_number / (double)(cyclic ? frame_count : frame_count-1);
	double radians = fraction * settings.angle * DEG2RAD;
	double cosine = std::cos(radians);
	double sine = std::sin(radians);
	double remainder = 1.0-cosine;
	double x = axis[0], y = axis[1], z = axis[2];
	double r00=cosine+x*x*remainder, r01=x*y*remainder-z*sine, r02=x*z*remainder+y*sine;
	double r10=y*x*remainder+z*sine, r11=cosine+y*y*remainder, r12=y*z*remainder-x*sine;
	double r20=z*x*remainder-y*sine, r21=z*y*remainder+x*sine, r22=cosine+z*z*remainder;
	double tx=center[0]-(r00*center[0]+r01*center[1]+r02*center[2]);
	double ty=center[1]-(r10*center[0]+r11*center[1]+r12*center[2]);
	double tz=center[2]-(r20*center[0]+r21*center[1]+r22*center[2]);
	AnimationFrame frame = make_animation_frame(settings.duration*fraction, settings.viewsize,
	    settings.orientation, settings.eye, settings.perspective);
	for (const std::string &object : objects) {
	    DatabaseOperation operation;
	    operation.type = DatabaseOperationType::Transform;
	    operation.path = object;
	    operation.action = "lmul";
	    operation.values = {r00, r01, r02, tx, r10, r11, r12, ty, r20, r21, r22, tz, 0, 0, 0, 1};
	    frame.operations.push_back(std::move(operation));
	}
	plan.frames.push_back(std::move(frame));
    }
    return plan;
}

}

AnimationPlan
plan_animation_preset(const PresetSettings &settings, const PresetSceneFacts &facts)
{
    validate_settings(settings);
    int frame_count = 0;
    bool cyclic = false;
    AnimationPlan plan = initialize_plan(settings, frame_count, cyclic);
    if (settings.preset == "cut") return plan_cut(settings, facts, std::move(plan), frame_count);
    if (settings.preset == "orbit") return plan_orbit(settings, facts, std::move(plan), frame_count, cyclic);
    if (settings.preset == "turntable") return plan_turntable(settings, facts, std::move(plan), frame_count, cyclic);
    throw std::runtime_error("unknown animation preset '" + settings.preset + "'");
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
